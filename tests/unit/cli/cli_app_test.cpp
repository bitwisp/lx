#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CliResult {
    int exitCode;
    std::string output;
    std::string error;
};

CliResult runCli(std::vector<std::string> arguments)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }

    std::ostringstream output;
    std::ostringstream error;
    const lx::application::DoctorService doctorService;
    const auto exitCode = lx::cli::CliApp{doctorService}.run(
        static_cast<int>(argv.size()), argv.data(), output, error);
    return {exitCode, output.str(), error.str()};
}

} // namespace

TEST_CASE("CLI prints help to standard output")
{
    const auto result = runCli({"lx", "--help"});

    REQUIRE(result.exitCode == 0);
    REQUIRE(result.output.find("Resource-oriented Linux") != std::string::npos);
    REQUIRE(result.output.find("--version") != std::string::npos);
    REQUIRE(result.error.empty());
}

TEST_CASE("CLI prints the development version")
{
    const auto result = runCli({"lx", "--version"});

    REQUIRE(result.exitCode == 0);
    REQUIRE(result.output == "LX 0.1.0-dev\n");
    REQUIRE(result.error.empty());
}

TEST_CASE("CLI maps invalid arguments to the public exit code")
{
    const auto result = runCli({"lx", "--unknown"});

    REQUIRE(result.exitCode == 2);
    REQUIRE(result.output.empty());
    REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("CLI doctor clearly distinguishes pending capabilities")
{
    const auto result = runCli({"lx", "doctor"});

    REQUIRE(result.exitCode == 0);
    REQUIRE(result.output.find("Project foundation   OK") != std::string::npos);
    REQUIRE(result.output.find("Process API          not implemented") !=
            std::string::npos);
    const auto systemdPosition = result.output.find("systemd");
    REQUIRE(systemdPosition != std::string::npos);
    REQUIRE(result.output.substr(systemdPosition, 64).find("not implemented") !=
            std::string::npos);
    REQUIRE(result.error.empty());
}
