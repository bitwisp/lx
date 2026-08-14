#include "lx/cli/CliApp.h"

#include "lx/Version.h"
#include "lx/application/DoctorService.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <ostream>

namespace lx::cli {

namespace {

std::string_view statusText(const CapabilityStatus status)
{
    switch (status) {
    case CapabilityStatus::Available:
        return "OK";
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

} // namespace

CliApp::CliApp(const application::DoctorService& doctorService) noexcept
    : doctorService_(doctorService)
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

    try {
        app.parse(argc, argv);
        if (*doctor) {
            printDoctorReport(doctorService_.inspect(), output);
        }
        return 0;
    } catch (const CLI::ParseError& parseError) {
        const auto cliExitCode = app.exit(parseError, output, error);
        return cliExitCode == 0 ? 0 : 2;
    }
}

} // namespace lx::cli
