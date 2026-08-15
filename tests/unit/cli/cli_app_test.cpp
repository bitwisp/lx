#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
#include "lx/application/ServiceService.h"
#include "lx/application/LogService.h"
#include "lx/application/InspectService.h"
#include "lx/application/ResourceResolver.h"
#include "lx/application/FindService.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <csignal>
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
    lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>> list() const override
    {
        lx::ProcessInfo info;
        info.pid = 42;
        info.name = "demo";
        info.user = "alice";
        info.state = "sleeping";
        info.threads = 2;
        info.systemdUnit = "demo.service";
        return lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>>::success(
            {{std::move(info)}, {}});
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

class FakeServiceProvider final : public lx::contracts::IServiceProvider {
public:
    explicit FakeServiceProvider(int& releaseStage)
        : releaseStage_(releaseStage)
    {
    }
    lx::Result<void> probe() const override
    {
        return lx::Result<void>::success();
    }
    lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>> list()
        const override
    {
        lx::ServiceInfo service;
        service.unitName = "demo.service";
        service.description = "Demo service";
        service.loadState = "loaded";
        service.activeState = "active";
        service.subState = "running";
        service.unitFileState = "enabled";
        service.mainPid = 10;
        lx::ServiceInfo longService;
        longService.unitName =
            "systemd-udev-load-credentials-very-long.service";
        longService.loadState = "loaded";
        longService.activeState = "inactive";
        longService.subState = "dead";
        longService.unitFileState = "static";
        return lx::Result<lx::Observation<std::vector<lx::ServiceInfo>>>::success(
            {{std::move(service), std::move(longService)}, {}});
    }
    lx::Result<lx::Observation<lx::ServiceInfo>> get(
        const std::string& unit) const override
    {
        lx::ServiceInfo service;
        service.unitName = unit;
        service.description = "Demo service";
        service.loadState = "loaded";
        service.activeState = "active";
        service.subState = "running";
        service.unitFileState = "enabled";
        service.mainPid = 10;
        return lx::Result<lx::Observation<lx::ServiceInfo>>::success(
            {std::move(service), {}});
    }
    lx::Result<std::optional<std::string>> unitByPid(pid_t) const override
    {
        return lx::Result<std::optional<std::string>>::success(
            std::string{"demo.service"});
    }
    lx::Result<void> start(const std::string&) const override
    {
        return lx::Result<void>::success();
    }
    lx::Result<void> stop(const std::string&) const override
    {
        releaseStage_ = 2;
        return lx::Result<void>::success();
    }
    lx::Result<void> restart(const std::string&) const override
    {
        return lx::Result<void>::success();
    }

private:
    int& releaseStage_;
};

class FakeJournalProvider final : public lx::contracts::IJournalProvider {
public:
    lx::Result<void> probe() const override { return lx::Result<void>::success(); }

    lx::Result<lx::Observation<std::vector<lx::JournalEntry>>> query(
        const lx::LogQuery& query) const override
    {
        lastQuery = query;
        lx::JournalEntry entry;
        entry.timestamp = std::chrono::system_clock::time_point{
            std::chrono::seconds{1}};
        entry.systemdUnit = query.unit;
        entry.pid = query.pid.value_or(42);
        entry.command = "demo";
        entry.message = "first\nsecond\x1b";
        entry.priority = 6;
        return lx::Result<lx::Observation<std::vector<lx::JournalEntry>>>::success(
            {{std::move(entry)}, {}});
    }

    lx::Result<void> follow(
        const lx::LogQuery& query, const lx::contracts::JournalEntrySink& sink,
        const lx::contracts::JournalStopRequested& stopRequested) const override
    {
        lastQuery = query;
        lx::JournalEntry entry;
        entry.systemdUnit = query.unit;
        entry.message = "followed";
        auto accepted = sink({std::move(entry), {}});
        if (!accepted) return accepted;
        std::raise(SIGINT);
        if (stopRequested()) {
            return lx::Result<void>::failure(
                {lx::ErrorCode::Interrupted, "interrupted", 0, "fake",
                 "follow"});
        }
        return lx::Result<void>::success();
    }

    mutable lx::LogQuery lastQuery;
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
    const FakeServiceProvider serviceProvider{releaseStage};
    const lx::application::ServiceService serviceService{serviceProvider};
    const FakeJournalProvider journalProvider;
    const lx::application::LogService logService{journalProvider};
    const lx::application::ProcessService processService{
        provider, signalProvider, 999, &serviceService};
    const FakeSocketProvider socketProvider{releaseStage};
    const FakeSocketOwnerResolver socketOwnerResolver;
    const lx::application::DoctorService doctorService{
        provider, socketProvider, signalProvider, serviceProvider,
        journalProvider};
    const lx::application::PortService portService{
        socketProvider, socketOwnerResolver, processService, &serviceService};
    const lx::application::ResourceResolver resolver{
        portService, processService, serviceService};
    const lx::application::InspectService inspectService{
        resolver, portService, processService, serviceService, logService};
    const lx::application::FindService findService{
        portService, processService, serviceService};
    const auto exitCode = lx::cli::CliApp{doctorService, processService,
                                         portService, serviceService,
                                         logService, inspectService,
                                         findService}.run(
        static_cast<int>(argv.size()), argv.data(), input, output, error);
    return {exitCode, output.str(), error.str()};
}

TEST_CASE("CLI lists and filters ports") {
    const auto all=runCli({"lx","port"}); REQUIRE(all.exitCode==0); REQUIRE(all.output.find("127.0.0.1")!=std::string::npos); REQUIRE(all.output.find("demo,worker")!=std::string::npos); REQUIRE(all.output.find("10,20")!=std::string::npos); REQUIRE(all.output.find("demo.service")!=std::string::npos); REQUIRE(all.output.find("unresolved")!=std::string::npos);
    const auto unavailable=runCli({"lx","port","9090"}); REQUIRE(unavailable.output.find("<unavailable>")!=std::string::npos); REQUIRE(unavailable.output.find("404")!=std::string::npos);
    REQUIRE(runCli({"lx","port","9999"}).exitCode==3);
}

TEST_CASE("CLI inspects typed resources and reports ambiguity")
{
    const auto port = runCli({"lx", "inspect", "port:8080"});
    REQUIRE(port.exitCode == 0);
    CHECK(port.output.find("Port 8080") != std::string::npos);
    CHECK(port.output.find("Processes") != std::string::npos);
    CHECK(port.output.find("Services") != std::string::npos);
    CHECK(port.output.find("Recent logs") != std::string::npos);

    const auto ambiguous = runCli({"lx", "inspect", "8080"});
    REQUIRE(ambiguous.exitCode == 7);
    CHECK(ambiguous.error.find("port:8080") != std::string::npos);
}

TEST_CASE("CLI finds related observable resources")
{
    const auto result = runCli({"lx", "find", "demo"});
    REQUIRE(result.exitCode == 0);
    CHECK(result.output.find("SERVICES") != std::string::npos);
    CHECK(result.output.find("PROCESSES") != std::string::npos);
    CHECK(result.output.find("PORTS") != std::string::npos);

    const auto missing = runCli({"lx", "find", "does-not-exist"});
    CHECK(missing.exitCode == 3);
}

TEST_CASE("CLI validates port range") { REQUIRE(runCli({"lx","port","0"}).exitCode==2); REQUIRE(runCli({"lx","port","65536"}).exitCode==2); }

TEST_CASE("CLI lists and inspects services")
{
    const auto listed = runCli({"lx", "service"});
    REQUIRE(listed.exitCode == 0);
    REQUIRE(listed.output.find("demo.service") != std::string::npos);
    REQUIRE(listed.output.find("running") != std::string::npos);
    REQUIRE(listed.output.find(
                "systemd-udev-load-credentials-very-long.service") ==
            std::string::npos);
    const auto longLineStart = listed.output.find("systemd-udev");
    const auto longLineEnd = listed.output.find('\n', longLineStart);
    const auto longLine = listed.output.substr(
        longLineStart, longLineEnd - longLineStart);
    REQUIRE(longLine.find("...") != std::string::npos);
    REQUIRE(longLine.find("inactive") == 41);

    const auto detail = runCli({"lx", "svc", "demo"});
    REQUIRE(detail.exitCode == 0);
    REQUIRE(detail.output.find("demo.service") != std::string::npos);
    REQUIRE(detail.output.find("Demo service") != std::string::npos);
}

TEST_CASE("CLI controls services with conservative confirmation")
{
    const auto started = runCli({"lx", "service", "demo", "start"});
    REQUIRE(started.exitCode == 0);
    REQUIRE(started.output.find("start submitted") != std::string::npos);

    const auto cancelled = runCli({"lx", "service", "demo", "stop"});
    REQUIRE(cancelled.exitCode == 0);
    REQUIRE(cancelled.output.find("Cancelled") != std::string::npos);

    const auto restarted = runCli(
        {"lx", "service", "demo", "restart", "--yes"});
    REQUIRE(restarted.exitCode == 0);
    REQUIRE(restarted.output.find("restart submitted") != std::string::npos);
}

TEST_CASE("CLI queries journal by service or PID")
{
    const auto byService = runCli(
        {"lx", "log", "demo", "--lines", "10", "--since", "10m"});
    REQUIRE(byService.exitCode == 0);
    REQUIRE(byService.output.find("demo.service") != std::string::npos);
    REQUIRE(byService.output.find("first\\nsecond\\x1b") !=
            std::string::npos);

    const auto byPid = runCli({"lx", "log", "--pid", "42"});
    REQUIRE(byPid.exitCode == 0);
    REQUIRE(byPid.output.find("[42]") != std::string::npos);
}

TEST_CASE("CLI validates journal query arguments")
{
    REQUIRE(runCli({"lx", "log"}).exitCode == 2);
    REQUIRE(runCli({"lx", "log", "demo", "--lines", "0"}).exitCode == 2);
    REQUIRE(runCli({"lx", "log", "demo", "--since", "last week"})
                .exitCode == 2);
}

TEST_CASE("CLI follows journal entries until SIGINT")
{
    const auto result = runCli({"lx", "log", "demo", "--follow"});
    REQUIRE(result.exitCode == 130);
    REQUIRE(result.output.find("followed") != std::string::npos);
    REQUIRE(result.error.empty());
}

TEST_CASE("CLI confirms graceful and forced port release")
{
    const auto cancelled = runCli({"lx", "port", "free", "8080"}, "no\n");
    REQUIRE(cancelled.exitCode == 0);
    REQUIRE(cancelled.output.find("Cancelled.") != std::string::npos);

    const auto stopped = runCli(
        {"lx", "port", "free", "8080"}, "\n");
    REQUIRE(stopped.exitCode == 0);
    REQUIRE(stopped.output.find("stopping demo.service") != std::string::npos);

    const auto assumed = runCli(
        {"lx", "port", "free", "8080", "--yes"});
    REQUIRE(assumed.exitCode == 0);
    REQUIRE(assumed.output.find("stopping demo.service") != std::string::npos);
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

TEST_CASE("CLI lists processes and accepts list filters")
{
    const auto listed = runCli({"lx", "process"});
    REQUIRE(listed.exitCode == 0);
    REQUIRE(listed.output.find("PID") != std::string::npos);
    REQUIRE(listed.output.find("demo.service") != std::string::npos);

    REQUIRE(runCli({"lx", "process", "--name", "demo"}).exitCode == 0);
    REQUIRE(runCli({"lx", "process", "42", "--user", "alice"}).exitCode == 2);
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
    REQUIRE(runCli({"lx", "process"}).exitCode == 0);
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
    REQUIRE(result.output.substr(systemdPosition, 64).find("OK") !=
            std::string::npos);
    const auto journalPosition = result.output.find("Journal");
    REQUIRE(journalPosition != std::string::npos);
    REQUIRE(result.output.substr(journalPosition, 64).find("OK") !=
            std::string::npos);
    REQUIRE(result.error.empty());
}
