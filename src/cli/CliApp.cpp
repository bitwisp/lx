#include "lx/cli/CliApp.h"

#include "lx/Version.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <ostream>
#include <istream>
#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace lx::cli {

namespace {

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

void printReleasePlan(const PortReleasePlan& plan, std::ostream& output)
{
    output << fmt::format("Port {} is owned by:\n\n", plan.localPort);
    output << "PID      PROCESS\n";
    for (const auto pid : plan.ownerPids) {
        output << fmt::format("{:<8} {}\n", pid, processName(plan, pid));
    }
    output << '\n';
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
    if (!observed.warnings.empty()) {
        output << "\nWarnings\n";
        for (const auto& warning : observed.warnings) output << "  " << warning.message << '\n';
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
               const application::PortService& portService) noexcept
    : doctorService_(doctorService), processService_(processService), portService_(portService)
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
    auto* processPidOption = process->add_option("pid", processPid, "Process ID")->check(
        CLI::Range(1, std::numeric_limits<pid_t>::max()));
    process->add_flag("--raw-command", rawCommand,
                      "Display command arguments without best-effort redaction");
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

    try {
        app.parse(argc, argv);
        if (*doctor) {
            printDoctorReport(doctorService_.inspect(), output);
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
            if (processPidOption->count() == 0) {
                error << "Process PID or action is required\n";
                return 2;
            }
            const auto result = processService_.inspect(processPid);
            if (!result) {
                error << result.error().message << '\n';
                return exitCode(result.error().code);
            }
            printProcess(result.value(), rawCommand, output);
        } else if (*freePort) {
            auto plan = portService_.prepareRelease(
                static_cast<std::uint16_t>(freePortNumber));
            if (!plan) {
                error << plan.error().message << '\n';
                return exitCode(plan.error().code);
            }
            printReleasePlan(plan.value().value, output);
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
            output << "PROTO  ADDRESS                                  PORT   STATE        UID     INODE       PROCESS                  PID\n";
            for (const auto& portInfo : result.value().value) {
                const auto& socket = portInfo.socket;
                output << fmt::format(
                    "{:<6} {:<40} {:<6} {:<12} {:<7} {:<11} {:<24} {}\n",
                    socket.protocol == TransportProtocol::Tcp ? "TCP" : "UDP",
                    socket.local.address, socket.local.port, socket.state,
                    socket.uid, socket.inode, ownerNames(portInfo),
                    ownerPids(portInfo));
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
