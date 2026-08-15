#include "lx/cli/CliApp.h"

#include "lx/Version.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <ostream>
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
    std::ostream& output,
    std::ostream& error) const
{
    CLI::App app{"Resource-oriented Linux inspection and management tool", "lx"};
    app.set_version_flag("--version", fmt::format("LX {}", lx::version));
    const auto* doctor = app.add_subcommand("doctor", "Check LX capabilities");
    auto* process = app.add_subcommand("process", "Inspect a process");
    pid_t processPid = -1;
    bool rawCommand = false;
    process->add_option("pid", processPid, "Process ID")->required()->check(
        CLI::Range(1, std::numeric_limits<pid_t>::max()));
    process->add_flag("--raw-command", rawCommand,
                      "Display command arguments without best-effort redaction");
    auto* port = app.add_subcommand("port", "List listening ports");
    std::uint32_t portNumber = 0;
    auto* portOption = port->add_option("port", portNumber, "Local port")->check(CLI::Range(1, 65535));

    try {
        app.parse(argc, argv);
        if (*doctor) {
            printDoctorReport(doctorService_.inspect(), output);
        } else if (*process) {
            const auto result = processService_.inspect(processPid);
            if (!result) {
                error << result.error().message << '\n';
                return exitCode(result.error().code);
            }
            printProcess(result.value(), rawCommand, output);
        } else if (*port) {
            SocketQuery query;
            if (portOption->count() != 0) query.localPort = static_cast<std::uint16_t>(portNumber);
            const auto result = portService_.query(query);
            if (!result) { error << result.error().message << '\n'; return exitCode(result.error().code); }
            if (result.value().value.empty() && query.localPort) { error << "No socket found for port " << portNumber << '\n'; return 3; }
            output << "PROTO  ADDRESS                                  PORT   STATE        UID     INODE       PROCESS\n";
            for (const auto& socket : result.value().value) output << fmt::format("{:<6} {:<40} {:<6} {:<12} {:<7} {:<11} unresolved\n", socket.protocol==TransportProtocol::Tcp?"TCP":"UDP",socket.local.address,socket.local.port,socket.state,socket.uid,socket.inode);
            for (const auto& warning : result.value().warnings) error << "Warning: " << warning.message << '\n';
        }
        return 0;
    } catch (const CLI::ParseError& parseError) {
        const auto cliExitCode = app.exit(parseError, output, error);
        return cliExitCode == 0 ? 0 : 2;
    }
}

} // namespace lx::cli
