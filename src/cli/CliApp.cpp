#include "lx/cli/CliApp.h"

#include "lx/Version.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
#include "lx/application/ServiceService.h"
#include "lx/application/LogService.h"
#include "lx/application/InspectService.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <ostream>
#include <istream>
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <limits>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace lx::cli {

namespace {

class InterruptGuard final {
public:
    InterruptGuard() noexcept = default;
    ~InterruptGuard()
    {
        if (installed_) sigaction(SIGINT, &previous_, nullptr);
    }

    InterruptGuard(const InterruptGuard&) = delete;
    InterruptGuard& operator=(const InterruptGuard&) = delete;

    bool install() noexcept
    {
        requested_ = 0;
        struct sigaction action {};
        action.sa_handler = &InterruptGuard::handle;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;
        if (sigaction(SIGINT, &action, &previous_) != 0) return false;
        installed_ = true;
        return true;
    }

    [[nodiscard]] bool requested() const noexcept
    {
        return requested_ != 0;
    }

private:
    static void handle(int) noexcept { requested_ = 1; }

    static volatile std::sig_atomic_t requested_;
    struct sigaction previous_ {};
    bool installed_ = false;
};

volatile std::sig_atomic_t InterruptGuard::requested_ = 0;

std::string_view statusText(const CapabilityStatus status)
{
    switch (status) {
    case CapabilityStatus::Available:
        return "OK";
    case CapabilityStatus::Unavailable:
        return "unavailable";
    case CapabilityStatus::NotImplemented:
        return "not implemented";
    }
    return "unknown";
}

void printDoctorReport(const DoctorReport& report, std::ostream& output)
{
    output << "LX Doctor\n\n";
    for (const auto& check : report.checks) {
        output << fmt::format("{:<20} {}\n", check.name, statusText(check.status));
        output << fmt::format("  {}\n", check.detail);
    }
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool confirm(std::istream& input, std::ostream& output,
             const std::string& prompt, const bool defaultYes)
{
    output << prompt << std::flush;
    std::string answer;
    if (!std::getline(input, answer)) return false;
    answer.erase(answer.begin(), std::find_if(
        answer.begin(), answer.end(),
        [](const unsigned char c) { return !std::isspace(c); }));
    answer.erase(std::find_if(
        answer.rbegin(), answer.rend(),
        [](const unsigned char c) { return !std::isspace(c); }).base(),
        answer.end());
    if (answer.empty()) return defaultYes;
    const auto normalized = lower(std::move(answer));
    return normalized == "y" || normalized == "yes";
}

std::string_view mechanismName(const SignalMechanism mechanism)
{
    return mechanism == SignalMechanism::PidFd ? "pidfd" : "kill(2)";
}

bool isSecretKey(const std::string& key)
{
    const auto normalized = lower(key);
    return normalized == "password" || normalized == "passwd" ||
           normalized == "token" || normalized == "api-key" ||
           normalized == "apikey" || normalized == "secret" ||
           normalized == "database_url";
}

std::vector<std::string> displayedArguments(
    const std::vector<std::string>& arguments, const bool raw)
{
    if (raw) return arguments;
    auto result = arguments;
    bool redactNext = false;
    for (auto& argument : result) {
        if (redactNext) {
            argument = "<redacted>";
            redactNext = false;
            continue;
        }
        const auto equal = argument.find('=');
        auto key = equal == std::string::npos ? argument : argument.substr(0, equal);
        while (!key.empty() && key.front() == '-') key.erase(key.begin());
        if (isSecretKey(key)) {
            if (equal == std::string::npos) redactNext = true;
            else argument = argument.substr(0, equal + 1) + "<redacted>";
        }
    }
    return result;
}

std::string joinArguments(const std::vector<std::string>& arguments)
{
    std::ostringstream joined;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index != 0) joined << ' ';
        joined << arguments[index];
    }
    return joined.str();
}

std::string ownerNames(const PortInfo& port)
{
    if (port.socket.ownerPids.empty()) return "unresolved";

    std::ostringstream joined;
    for (const auto pid : port.socket.ownerPids) {
        const auto owner = std::find_if(
            port.owners.begin(), port.owners.end(),
            [pid](const ProcessInfo& process) { return process.pid == pid; });
        if (joined.tellp() != 0) joined << ',';
        joined << (owner == port.owners.end() ? "<unavailable>" : owner->name);
    }
    return joined.str();
}

std::string ownerPids(const PortInfo& port)
{
    if (port.socket.ownerPids.empty()) return "-";

    std::ostringstream joined;
    for (std::size_t index = 0; index < port.socket.ownerPids.size(); ++index) {
        if (index != 0) joined << ',';
        joined << port.socket.ownerPids[index];
    }
    return joined.str();
}

std::string ownerServices(const PortInfo& port)
{
    std::vector<std::string> units;
    for (const auto& owner : port.owners) {
        if (owner.systemdUnit &&
            std::find(units.begin(), units.end(), *owner.systemdUnit) ==
                units.end()) {
            units.push_back(*owner.systemdUnit);
        }
    }
    if (units.empty()) return "-";
    std::ostringstream joined;
    for (std::size_t index = 0; index < units.size(); ++index) {
        if (index != 0) joined << ',';
        joined << units[index];
    }
    return joined.str();
}

std::string processName(const PortReleasePlan& plan, const pid_t pid)
{
    for (const auto& port : plan.ports) {
        const auto owner = std::find_if(
            port.owners.begin(), port.owners.end(),
            [pid](const ProcessInfo& process) { return process.pid == pid; });
        if (owner != port.owners.end()) return owner->name;
    }
    return "<unavailable>";
}

std::string shortened(const std::string& value, std::size_t width);

void printReleasePlan(const PortReleasePlan& plan, std::ostream& output)
{
    output << fmt::format("Port {} is owned by:\n\n", plan.localPort);
    output << "PID      PROCESS\n";
    for (const auto pid : plan.ownerPids) {
        output << fmt::format("{:<8} {}\n", pid, processName(plan, pid));
    }
    output << '\n';
    if (plan.recommendedUnit) {
        output << fmt::format(
            "All owners belong to {}.\nRecommended action: stop the service instead of signaling its processes.\n\n",
            *plan.recommendedUnit);
    }
}

void printProcess(const Observation<ProcessInfo>& observed, const bool raw,
                  std::ostream& output)
{
    const auto& process = observed.value;
    output << fmt::format("Process {}\n\n", process.pid);
    output << fmt::format("Name        {}\nState       {}\nPPID        {}\n", process.name,
                          process.state, process.ppid);
    output << fmt::format("User        {} ({})\nThreads     {}\nRSS         {:.1f} MiB\n",
                          process.user, process.uid, process.threads,
                          static_cast<double>(process.rssBytes) / (1024.0 * 1024.0));
    output << fmt::format("Executable  {}\nCWD         {}\n",
                          process.executable.value_or("<unavailable>"),
                          process.cwd.value_or("<unavailable>"));
    output << "\nCommand\n  "
           << joinArguments(displayedArguments(process.argv, raw)) << '\n';
    output << "\nService\n  "
           << process.systemdUnit.value_or("<none>") << '\n';
    if (!observed.warnings.empty()) {
        output << "\nWarnings\n";
        for (const auto& warning : observed.warnings) output << "  " << warning.message << '\n';
    }
}

void printProcesses(const Observation<std::vector<ProcessInfo>>& observed,
                    std::ostream& output, std::ostream& error)
{
    output << "PID      USER             STATE          RSS MiB   THREADS  NAME                     SERVICE\n";
    for (const auto& process : observed.value) {
        output << fmt::format(
            "{:<8} {:<16} {:<14} {:>9.1f} {:>8}  {:<24} {}\n",
            process.pid, shortened(process.user, 16),
            shortened(process.state, 14),
            static_cast<double>(process.rssBytes) / (1024.0 * 1024.0),
            process.threads, shortened(process.name, 24),
            process.systemdUnit.value_or("-"));
    }
    for (const auto& warning : observed.warnings) {
        error << "Warning: " << warning.message << '\n';
    }
}

std::string timestampText(const std::uint64_t timestampUsec)
{
    if (timestampUsec == 0) return "-";
    const auto seconds = static_cast<std::time_t>(timestampUsec / 1000000U);
    std::tm local{};
    if (localtime_r(&seconds, &local) == nullptr) return "-";
    std::ostringstream formatted;
    formatted << std::put_time(&local, "%F %T");
    return formatted.str();
}

std::string tableCell(const std::string_view value, const std::size_t width)
{
    std::string cell;
    if (value.size() <= width) {
        cell = value;
    } else if (width > 3) {
        cell = std::string{value.substr(0, width - 3)} + "...";
    } else {
        cell = std::string{value.substr(0, width)};
    }
    cell.append(width - cell.size(), ' ');
    return cell;
}

std::string shortened(const std::string& value, const std::size_t width)
{
    if (value.size() <= width) return value;
    return width > 3 ? value.substr(0, width - 3) + "..."
                     : value.substr(0, width);
}

std::string safeLogText(const std::string& value,
                        const std::size_t limit = 4096)
{
    std::string result;
    result.reserve(std::min(value.size(), limit));
    for (const unsigned char character : value) {
        if (result.size() >= limit) {
            result += "...";
            break;
        }
        switch (character) {
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20 || character == 0x7f) {
                result += fmt::format("\\x{:02x}", character);
            } else {
                result.push_back(static_cast<char>(character));
            }
        }
    }
    return result;
}

std::string_view priorityText(const std::optional<std::uint8_t> priority)
{
    if (!priority) return "unknown";
    constexpr std::string_view names[] = {
        "emerg", "alert", "crit", "err", "warning", "notice", "info",
        "debug"};
    return *priority < 8 ? names[*priority] : "unknown";
}

std::string logTimestamp(const std::chrono::system_clock::time_point timestamp)
{
    const auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::tm local{};
    if (localtime_r(&time, &local) == nullptr) return "-";
    std::ostringstream formatted;
    formatted << std::put_time(&local, "%F %T");
    return formatted.str();
}

void printJournalEntry(const Observation<JournalEntry>& observed,
                       std::ostream& output, std::ostream& error)
{
    const auto& entry = observed.value;
    std::string source = entry.systemdUnit.value_or("-");
    if (entry.pid) source += '[' + std::to_string(*entry.pid) + ']';
    if (entry.command) source += ' ' + shortened(*entry.command, 24);
    output << fmt::format("{} {:<7} {}: {}\n", logTimestamp(entry.timestamp),
                          priorityText(entry.priority), shortened(source, 56),
                          safeLogText(entry.message));
    for (const auto& warning : observed.warnings) {
        error << "Warning: " << warning.message << '\n';
    }
}

void printJournalEntries(
    const Observation<std::vector<JournalEntry>>& observed,
    std::ostream& output, std::ostream& error)
{
    for (const auto& entry : observed.value) {
        printJournalEntry({entry, {}}, output, error);
    }
    for (const auto& warning : observed.warnings) {
        error << "Warning: " << warning.message << '\n';
    }
}

void printServices(const Observation<std::vector<ServiceInfo>>& observed,
                   std::ostream& output, std::ostream& error)
{
    constexpr std::size_t serviceWidth = 40;
    constexpr std::size_t activeWidth = 12;
    constexpr std::size_t subWidth = 16;
    constexpr std::size_t enabledWidth = 16;
    output << tableCell("SERVICE", serviceWidth) << ' '
           << tableCell("ACTIVE", activeWidth) << ' '
           << tableCell("SUB", subWidth) << ' '
           << tableCell("ENABLED", enabledWidth) << " MAIN PID\n";
    for (const auto& service : observed.value) {
        output << tableCell(service.unitName, serviceWidth) << ' '
               << tableCell(service.activeState, activeWidth) << ' '
               << tableCell(service.subState, subWidth) << ' '
               << tableCell(service.unitFileState.empty()
                                ? "-"
                                : service.unitFileState,
                            enabledWidth)
               << ' '
               << (service.mainPid ? std::to_string(*service.mainPid) : "-")
               << '\n';
    }
    for (const auto& warning : observed.warnings) {
        error << "Warning: " << warning.message << '\n';
    }
}

void printService(const Observation<ServiceInfo>& observed,
                  std::ostream& output, std::ostream& error)
{
    const auto& service = observed.value;
    output << service.unitName << "\n\n";
    output << fmt::format(
        "Description  {}\nLoad         {}\nActive       {}\nSub          {}\nEnabled      {}\nMain PID     {}\nSince        {}\n",
        service.description, service.loadState, service.activeState,
        service.subState,
        service.unitFileState.empty() ? "-" : service.unitFileState,
        service.mainPid ? std::to_string(*service.mainPid) : "-",
        timestampText(service.activeEnterTimestampUsec));
    for (const auto& warning : observed.warnings) {
        error << "Warning: " << warning.message << '\n';
    }
}

void printResourceGraph(const ResourceGraph& graph, std::ostream& output,
                        std::ostream& error)
{
    std::visit([&output](const auto& root) {
        using Root = std::decay_t<decltype(root)>;
        if constexpr (std::is_same<Root, PortTarget>::value) {
            output << fmt::format("Port {}\n", root.port);
        } else if constexpr (std::is_same<Root, ProcessTarget>::value) {
            output << fmt::format("Process {}\n", root.pid);
        } else {
            output << root.unit << '\n';
        }
    }, graph.root);

    if (!graph.ports.empty()) {
        output << "\nPorts\n";
        for (const auto& port : graph.ports) {
            output << fmt::format(
                "  {} {}:{} {:<10} PID {}\n",
                port.socket.protocol == TransportProtocol::Tcp ? "TCP" : "UDP",
                port.socket.local.address, port.socket.local.port,
                port.socket.state, ownerPids(port));
        }
    }
    if (!graph.processes.empty()) {
        output << "\nProcesses\n";
        for (const auto& process : graph.processes) {
            output << fmt::format(
                "  {:<8} {:<24} {:<14} {}\n", process.pid,
                shortened(process.name, 24), shortened(process.state, 14),
                process.user);
        }
    }
    if (!graph.services.empty()) {
        output << "\nServices\n";
        for (const auto& service : graph.services) {
            output << fmt::format("  {:<40} {} / {}\n",
                                  shortened(service.unitName, 40),
                                  service.activeState, service.subState);
        }
    }
    if (!graph.recentLogs.empty()) {
        output << "\nRecent logs\n";
        for (const auto& entry : graph.recentLogs) {
            printJournalEntry({entry, {}}, output, error);
        }
    }
    for (const auto& warning : graph.warnings) {
        error << "Warning: " << warning.message << '\n';
    }
}

int exitCode(const ErrorCode code)
{
    switch (code) {
    case ErrorCode::InvalidArgument: return 2;
    case ErrorCode::NotFound: return 3;
    case ErrorCode::PermissionDenied: return 4;
    case ErrorCode::Unsupported: case ErrorCode::Unavailable: return 6;
    case ErrorCode::Conflict: return 7;
    case ErrorCode::Timeout: return 8;
    case ErrorCode::Interrupted: return 130;
    default: return 5;
    }
}

} // namespace

CliApp::CliApp(const application::DoctorService& doctorService,
               const application::ProcessService& processService,
               const application::PortService& portService,
               const application::ServiceService& serviceService,
               const application::LogService& logService,
               const application::InspectService& inspectService) noexcept
    : doctorService_(doctorService), processService_(processService),
      portService_(portService), serviceService_(serviceService),
      logService_(logService), inspectService_(inspectService)
{
}

int CliApp::run(
    const int argc,
    char** argv,
    std::istream& input,
    std::ostream& output,
    std::ostream& error) const
{
    CLI::App app{"Resource-oriented Linux inspection and management tool", "lx"};
    app.set_version_flag("--version", fmt::format("LX {}", lx::version));
    const auto* doctor = app.add_subcommand("doctor", "Check LX capabilities");
    auto* process = app.add_subcommand("process", "Inspect a process");
    pid_t processPid = -1;
    bool rawCommand = false;
    std::string processNameFilter;
    std::string processUserFilter;
    std::string processServiceFilter;
    auto* processPidOption = process->add_option("pid", processPid, "Process ID")->check(
        CLI::Range(1, std::numeric_limits<pid_t>::max()));
    process->add_flag("--raw-command", rawCommand,
                      "Display command arguments without best-effort redaction");
    auto* processNameOption = process->add_option(
        "--name", processNameFilter, "Filter by exact process name");
    auto* processUserOption = process->add_option(
        "--user", processUserFilter, "Filter by user name or UID");
    auto* processServiceOption = process->add_option(
        "--service", processServiceFilter, "Filter by systemd service");
    auto* stopProcess = process->add_subcommand("stop", "Send SIGTERM to a process");
    pid_t stopPid = -1;
    stopProcess->add_option("pid", stopPid, "Process ID")->required()->check(
        CLI::Range(1, std::numeric_limits<pid_t>::max()));
    auto* killProcess = process->add_subcommand("kill", "Send SIGKILL to a process");
    pid_t killPid = -1;
    bool confirmKill = false;
    killProcess->add_option("pid", killPid, "Process ID")->required()->check(
        CLI::Range(1, std::numeric_limits<pid_t>::max()));
    killProcess->add_flag("--yes", confirmKill, "Skip the SIGKILL confirmation");
    auto* port = app.add_subcommand("port", "List listening ports");
    std::uint32_t portNumber = 0;
    auto* portOption = port->add_option("port", portNumber, "Local port")->check(CLI::Range(1, 65535));
    auto* freePort = port->add_subcommand("free", "Release a listening port");
    std::uint32_t freePortNumber = 0;
    bool confirmRelease = false;
    freePort->add_option("port", freePortNumber, "Local port")
        ->required()->check(CLI::Range(1, 65535));
    freePort->add_flag("--yes", confirmRelease,
                       "Skip SIGTERM and SIGKILL confirmations");
    auto* service = app.add_subcommand("service", "List or manage services");
    service->alias("svc");
    std::string serviceUnit;
    std::string serviceAction;
    bool confirmServiceAction = false;
    auto* serviceUnitOption = service->add_option("unit", serviceUnit,
                                                  "Service unit name");
    service->add_option("action", serviceAction,
                        "Lifecycle action: start, stop, or restart")
        ->check(CLI::IsMember({"start", "stop", "restart"}));
    service->add_flag("--yes", confirmServiceAction,
                      "Skip stop and restart confirmation");
    auto* log = app.add_subcommand("log", "Query journal entries");
    std::string logUnit;
    pid_t logPid = -1;
    std::uint32_t logLines = 50;
    std::string logSince;
    auto* logUnitOption = log->add_option("unit", logUnit,
                                          "Service unit name");
    auto* logPidOption = log->add_option("--pid", logPid, "Process ID")
                             ->check(CLI::Range(
                                 1, std::numeric_limits<pid_t>::max()));
    log->add_option("--lines", logLines, "Maximum journal entries")
        ->check(CLI::Range(1, 10000));
    auto* logSinceOption = log->add_option(
        "--since", logSince,
        "Start time: YYYY-MM-DD HH:MM:SS, 30s, 10m, 2h, or 3d");
    bool followLog = false;
    log->add_flag("--follow", followLog,
                  "Show recent entries and follow new journal entries");
    auto* inspect = app.add_subcommand(
        "inspect", "Inspect a port, PID, service, or untyped resource");
    std::string inspectExpression;
    inspect->add_option("resource", inspectExpression, "Resource expression")
        ->required();

    try {
        app.parse(argc, argv);
        if (*doctor) {
            printDoctorReport(doctorService_.inspect(), output);
        } else if (*inspect) {
            const auto result = inspectService_.inspect(inspectExpression);
            if (!result) {
                error << result.error().message << '\n';
                return exitCode(result.error().code);
            }
            printResourceGraph(result.value(), output, error);
        } else if (*stopProcess) {
            const auto result = processService_.stop(stopPid);
            if (!result) {
                error << result.error().message << '\n';
                return exitCode(result.error().code);
            }
            output << fmt::format("Sent SIGTERM to PID {} via {}\n", stopPid,
                                  mechanismName(result.value().mechanism));
        } else if (*killProcess) {
            const auto valid = processService_.validateSignalTarget(killPid);
            if (!valid) {
                error << valid.error().message << '\n';
                return exitCode(valid.error().code);
            }
            const auto target = processService_.inspect(killPid);
            if (!target) {
                error << target.error().message << '\n';
                return exitCode(target.error().code);
            }
            if (!confirmKill && !confirm(
                    input, output,
                    fmt::format("Force termination of PID {} ({}) with SIGKILL? [y/N] ",
                                killPid, target.value().value.name),
                    false)) {
                output << "Cancelled.\n";
                return 0;
            }
            const auto result = processService_.kill(killPid);
            if (!result) {
                error << result.error().message << '\n';
                return exitCode(result.error().code);
            }
            output << fmt::format("Sent SIGKILL to PID {} via {}\n", killPid,
                                  mechanismName(result.value().mechanism));
        } else if (*process) {
            const bool hasFilters = processNameOption->count() != 0 ||
                                    processUserOption->count() != 0 ||
                                    processServiceOption->count() != 0;
            if (processPidOption->count() != 0 && hasFilters) {
                error << "Process PID cannot be combined with list filters\n";
                return 2;
            }
            if (processPidOption->count() == 0) {
                if (rawCommand) {
                    error << "--raw-command requires a process PID\n";
                    return 2;
                }
                ProcessQuery query;
                if (processNameOption->count() != 0) query.name = processNameFilter;
                if (processUserOption->count() != 0) query.user = processUserFilter;
                if (processServiceOption->count() != 0) {
                    query.systemdUnit = processServiceFilter;
                }
                const auto result = processService_.list(std::move(query));
                if (!result) {
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
                printProcesses(result.value(), output, error);
            } else {
                const auto result = processService_.inspect(processPid);
                if (!result) {
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
                printProcess(result.value(), rawCommand, output);
            }
        } else if (*service) {
            if (serviceUnitOption->count() == 0) {
                const auto result = serviceService_.list();
                if (!result) {
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
                printServices(result.value(), output, error);
            } else if (serviceAction.empty()) {
                const auto result = serviceService_.inspect(serviceUnit);
                if (!result) {
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
                printService(result.value(), output, error);
            } else {
                if ((serviceAction == "stop" || serviceAction == "restart") &&
                    !confirmServiceAction &&
                    !confirm(input, output,
                             fmt::format("{} {}? [y/N] ",
                                         serviceAction == "stop" ? "Stop"
                                                                   : "Restart",
                                         serviceUnit),
                             false)) {
                    output << "Cancelled.\n";
                    return 0;
                }
                Result<void> result = serviceAction == "start"
                                          ? serviceService_.start(serviceUnit)
                                      : serviceAction == "stop"
                                          ? serviceService_.stop(serviceUnit)
                                          : serviceService_.restart(serviceUnit);
                if (!result) {
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
                output << fmt::format("{} submitted for {}\n",
                                      serviceAction, serviceUnit);
            }
        } else if (*log) {
            LogQuery query;
            if (logUnitOption->count() != 0) query.unit = logUnit;
            if (logPidOption->count() != 0) query.pid = logPid;
            query.limit = logLines;
            if (logSinceOption->count() != 0) {
                auto since = application::LogService::parseSince(logSince);
                if (!since) {
                    error << since.error().message << '\n';
                    return exitCode(since.error().code);
                }
                query.since = since.value();
            }
            if (followLog) {
                InterruptGuard interrupt;
                if (!interrupt.install()) {
                    error << "Unable to install SIGINT handler: "
                          << std::strerror(errno) << '\n';
                    return 5;
                }
                const auto result = logService_.follow(
                    std::move(query),
                    [&output, &error](
                        const Observation<JournalEntry>& entry) {
                        printJournalEntry(entry, output, error);
                        output.flush();
                        return Result<void>::success();
                    },
                    [&interrupt] { return interrupt.requested(); });
                if (!result) {
                    if (result.error().code == ErrorCode::Interrupted) {
                        return 130;
                    }
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
            } else {
                const auto result = logService_.read(std::move(query));
                if (!result) {
                    error << result.error().message << '\n';
                    return exitCode(result.error().code);
                }
                printJournalEntries(result.value(), output, error);
            }
        } else if (*freePort) {
            auto plan = portService_.prepareRelease(
                static_cast<std::uint16_t>(freePortNumber));
            if (!plan) {
                error << plan.error().message << '\n';
                return exitCode(plan.error().code);
            }
            printReleasePlan(plan.value().value, output);
            if (plan.value().value.recommendedUnit) {
                if (!confirmRelease &&
                    !confirm(input, output,
                             fmt::format("Stop {}? [Y/n] ",
                                         *plan.value().value.recommendedUnit),
                             true)) {
                    output << "Cancelled.\n";
                    return 0;
                }
                auto stopped = portService_.stopManagedService(
                    plan.value().value);
                if (!stopped) {
                    error << stopped.error().message << '\n';
                    return exitCode(stopped.error().code);
                }
                for (const auto& warning : stopped.value().warnings) {
                    error << "Warning: " << warning.message << '\n';
                }
                output << fmt::format("Port {} released by stopping {}.\n",
                                      freePortNumber,
                                      *plan.value().value.recommendedUnit);
                return 0;
            }
            if (!confirmRelease && !confirm(
                    input, output, "Send SIGTERM to all owners? [Y/n] ", true)) {
                output << "Cancelled.\n";
                return 0;
            }

            auto terminated = portService_.terminate(plan.value().value);
            if (!terminated) {
                error << terminated.error().message << '\n';
                return exitCode(terminated.error().code);
            }
            for (const auto& warning : terminated.value().warnings) {
                error << "Warning: " << warning.message << '\n';
            }
            if (terminated.value().value.released) {
                output << fmt::format("Port {} released with SIGTERM.\n",
                                      freePortNumber);
                return 0;
            }

            output << "Port is still occupied.\n";
            if (!confirmRelease && !confirm(
                    input, output,
                    "Force remaining owners with SIGKILL? [y/N] ", false)) {
                output << "Cancelled.\n";
                return 0;
            }
            auto forced = portService_.force(
                *terminated.value().value.remaining);
            if (!forced) {
                error << forced.error().message << '\n';
                return exitCode(forced.error().code);
            }
            for (const auto& warning : forced.value().warnings) {
                error << "Warning: " << warning.message << '\n';
            }
            output << fmt::format("Port {} released with SIGKILL.\n",
                                  freePortNumber);
        } else if (*port) {
            SocketQuery query;
            if (portOption->count() != 0) query.localPort = static_cast<std::uint16_t>(portNumber);
            const auto result = portService_.inspect(query);
            if (!result) { error << result.error().message << '\n'; return exitCode(result.error().code); }
            if (result.value().value.empty() && query.localPort) { error << "No socket found for port " << portNumber << '\n'; return 3; }
            output << "PROTO  ADDRESS                                  PORT   STATE        UID     INODE       PROCESS          PID          SERVICE\n";
            for (const auto& portInfo : result.value().value) {
                const auto& socket = portInfo.socket;
                output << fmt::format(
                    "{:<6} {:<40} {:<6} {:<12} {:<7} {:<11} {:<16} {:<12} {}\n",
                    socket.protocol == TransportProtocol::Tcp ? "TCP" : "UDP",
                    socket.local.address, socket.local.port, socket.state,
                    socket.uid, socket.inode, ownerNames(portInfo),
                    ownerPids(portInfo), ownerServices(portInfo));
            }
            for (const auto& warning : result.value().warnings) error << "Warning: " << warning.message << '\n';
        }
        return 0;
    } catch (const CLI::ParseError& parseError) {
        const auto cliExitCode = app.exit(parseError, output, error);
        return cliExitCode == 0 ? 0 : 2;
    }
}

} // namespace lx::cli
