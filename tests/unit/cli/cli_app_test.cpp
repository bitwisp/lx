#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(const pid_t pid) const override
    {
        if (pid == 404) return lx::Result<lx::Observation<lx::ProcessInfo>>::failure(
            {lx::ErrorCode::NotFound, "Process not found", 2, "fake", "get"});
        lx::ProcessInfo info;
        info.pid = pid; info.ppid = 1; info.name = "demo"; info.state = "sleeping";
        info.uid = 1000; info.user = "tester"; info.threads = 2;
        info.rssBytes = 2 * 1024 * 1024;
        info.argv = {"demo", "--token", "secret", "--port=80"};
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success({std::move(info), {}});
    }
};

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
    const FakeProcessProvider provider;
    const lx::application::DoctorService doctorService{provider};
    const lx::application::ProcessService processService{provider};
    const auto exitCode = lx::cli::CliApp{doctorService, processService}.run(
        static_cast<int>(argv.size()), argv.data(), output, error);
    return {exitCode, output.str(), error.str()};
}

TEST_CASE("CLI prints process details with secrets redacted")
{
    const auto result = runCli({"lx", "process", "42"});
    REQUIRE(result.exitCode == 0);
    REQUIRE(result.output.find("Process 42") != std::string::npos);
    REQUIRE(result.output.find("--token <redacted>") != std::string::npos);
    REQUIRE(result.output.find("secret") == std::string::npos);
}

TEST_CASE("CLI raw process command is explicit")
{
    const auto result = runCli({"lx", "process", "42", "--raw-command"});
    REQUIRE(result.exitCode == 0);
    REQUIRE(result.output.find("--token secret") != std::string::npos);
}

TEST_CASE("CLI validates process PID and maps not found")
{
    REQUIRE(runCli({"lx", "process", "0"}).exitCode == 2);
    const auto missing = runCli({"lx", "process", "404"});
    REQUIRE(missing.exitCode == 3);
    REQUIRE(missing.output.empty());
    REQUIRE(missing.error.find("Process not found") != std::string::npos);
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
    REQUIRE(result.output.find("Process API          OK") !=
            std::string::npos);
    const auto systemdPosition = result.output.find("systemd");
    REQUIRE(systemdPosition != std::string::npos);
    REQUIRE(result.output.substr(systemdPosition, 64).find("not implemented") !=
            std::string::npos);
    REQUIRE(result.error.empty());
}
