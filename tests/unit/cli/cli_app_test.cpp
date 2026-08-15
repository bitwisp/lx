#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"

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
        info.pid = pid; info.ppid = 1; info.name = pid == 20 ? "worker" : "demo"; info.state = "sleeping";
        info.uid = 1000; info.user = "tester"; info.threads = 2;
        info.rssBytes = 2 * 1024 * 1024;
        info.argv = {"demo", "--token", "secret", "--port=80"};
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success({std::move(info), {}});
    }
};

class FakeSocketProvider final : public lx::contracts::ISocketProvider {
public:
    explicit FakeSocketProvider(const int& releaseStage)
        : releaseStage_(releaseStage) {}

    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(const lx::SocketQuery& query) const override {
        std::vector<lx::SocketInfo> values;
        if (releaseStage_ < 2 && (!query.localPort || *query.localPort == 8080)) { lx::SocketInfo socket; socket.local={"127.0.0.1",8080};socket.state="listen";socket.uid=1000;socket.inode=42;values.push_back(socket); }
        if (!query.localPort || *query.localPort == 9090) { lx::SocketInfo socket; socket.local={"0.0.0.0",9090};socket.state="listen";socket.uid=1000;socket.inode=99;values.push_back(socket); }
        if (!query.localPort || *query.localPort == 9091) { lx::SocketInfo socket; socket.local={"0.0.0.0",9091};socket.state="listen";socket.uid=1000;socket.inode=100;values.push_back(socket); }
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success({std::move(values),{}});
    }

private:
    const int& releaseStage_;
};

class FakeSocketOwnerResolver final
    : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>&) const override
    {
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {{{42, {10, 20}}, {99, {404}}}, {}});
    }
};

class FakeSignalProvider final : public lx::contracts::ISignalProvider {
public:
    explicit FakeSignalProvider(int& releaseStage)
        : releaseStage_(releaseStage) {}

    lx::Result<lx::SignalDelivery> send(
        const pid_t pid, const lx::ProcessSignal signal) const override
    {
        releaseStage_ = signal == lx::ProcessSignal::Terminate ? 1 : 2;
        return lx::Result<lx::SignalDelivery>::success(
            {pid, signal, lx::SignalMechanism::PidFd});
    }
    lx::Result<bool> waitForExit(
        const pid_t, const std::chrono::milliseconds) const override
    {
        return lx::Result<bool>::success(true);
    }
    lx::SignalCapabilities capabilities() const override { return {true, true}; }

private:
    int& releaseStage_;
};

struct CliResult {
    int exitCode;
    std::string output;
    std::string error;
};

CliResult runCli(std::vector<std::string> arguments,
                 const std::string& inputText = {})
{
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (auto& argument : arguments) {
        argv.push_back(argument.data());
    }

    std::ostringstream output;
    std::ostringstream error;
    std::istringstream input{inputText};
    int releaseStage = 0;
    const FakeProcessProvider provider;
    const FakeSignalProvider signalProvider{releaseStage};
    const lx::application::ProcessService processService{
        provider, signalProvider, 999};
    const FakeSocketProvider socketProvider{releaseStage};
    const FakeSocketOwnerResolver socketOwnerResolver;
    const lx::application::DoctorService doctorService{
        provider, socketProvider, signalProvider};
    const lx::application::PortService portService{
        socketProvider, socketOwnerResolver, processService};
    const auto exitCode = lx::cli::CliApp{doctorService, processService, portService}.run(
        static_cast<int>(argv.size()), argv.data(), input, output, error);
    return {exitCode, output.str(), error.str()};
}

TEST_CASE("CLI lists and filters ports") {
    const auto all=runCli({"lx","port"}); REQUIRE(all.exitCode==0); REQUIRE(all.output.find("127.0.0.1")!=std::string::npos); REQUIRE(all.output.find("demo,worker")!=std::string::npos); REQUIRE(all.output.find("10,20")!=std::string::npos); REQUIRE(all.output.find("unresolved")!=std::string::npos);
    const auto unavailable=runCli({"lx","port","9090"}); REQUIRE(unavailable.output.find("<unavailable>")!=std::string::npos); REQUIRE(unavailable.output.find("404")!=std::string::npos);
    REQUIRE(runCli({"lx","port","9999"}).exitCode==3);
}

TEST_CASE("CLI validates port range") { REQUIRE(runCli({"lx","port","0"}).exitCode==2); REQUIRE(runCli({"lx","port","65536"}).exitCode==2); }

TEST_CASE("CLI confirms graceful and forced port release")
{
    const auto cancelled = runCli({"lx", "port", "free", "8080"}, "\n");
    REQUIRE(cancelled.exitCode == 0);
    REQUIRE(cancelled.output.find("Port is still occupied") != std::string::npos);
    REQUIRE(cancelled.output.find("Cancelled.") != std::string::npos);

    const auto forced = runCli(
        {"lx", "port", "free", "8080"}, "\nyes\n");
    REQUIRE(forced.exitCode == 0);
    REQUIRE(forced.output.find("released with SIGKILL") != std::string::npos);

    const auto assumed = runCli(
        {"lx", "port", "free", "8080", "--yes"});
    REQUIRE(assumed.exitCode == 0);
    REQUIRE(assumed.output.find("released with SIGKILL") != std::string::npos);
}

TEST_CASE("CLI refuses to free ports with unresolved owners")
{
    const auto result = runCli({"lx", "port", "free", "9091", "--yes"});
    REQUIRE(result.exitCode == 4);
    REQUIRE(result.error.find("unresolved") != std::string::npos);
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

TEST_CASE("CLI stops and confirms killing processes")
{
    const auto stopped = runCli({"lx", "process", "stop", "42"});
    REQUIRE(stopped.exitCode == 0);
    REQUIRE(stopped.output.find("SIGTERM") != std::string::npos);
    REQUIRE(stopped.output.find("pidfd") != std::string::npos);

    const auto cancelled = runCli({"lx", "process", "kill", "42"});
    REQUIRE(cancelled.exitCode == 0);
    REQUIRE(cancelled.output.find("Cancelled.") != std::string::npos);

    const auto confirmed = runCli(
        {"lx", "process", "kill", "42"}, "yes\n");
    REQUIRE(confirmed.exitCode == 0);
    REQUIRE(confirmed.output.find("SIGKILL") != std::string::npos);

    const auto assumed = runCli(
        {"lx", "process", "kill", "42", "--yes"});
    REQUIRE(assumed.exitCode == 0);
    REQUIRE(assumed.output.find("SIGKILL") != std::string::npos);
}

TEST_CASE("CLI rejects protected process signal targets")
{
    REQUIRE(runCli({"lx", "process", "stop", "1"}).exitCode == 7);
    REQUIRE(runCli({"lx", "process", "kill", "999", "--yes"}).exitCode == 7);
    REQUIRE(runCli({"lx", "process"}).exitCode == 2);
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
