#include "lx/cli/CliApp.h"

#include "lx/Version.h"

#include <CLI/CLI.hpp>
#include <fmt/format.h>

#include <ostream>

namespace lx::cli {

int CliApp::run(
    const int argc,
    char** argv,
    std::ostream& output,
    std::ostream& error) const
{
    CLI::App app{"Resource-oriented Linux inspection and management tool", "lx"};
    app.set_version_flag("--version", fmt::format("LX {}", lx::version));

    try {
        app.parse(argc, argv);
        return 0;
    } catch (const CLI::ParseError& parseError) {
        const auto cliExitCode = app.exit(parseError, output, error);
        return cliExitCode == 0 ? 0 : 2;
    }
}

} // namespace lx::cli

