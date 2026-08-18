#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/contracts/IProcessProvider.h"
#include "lx/contracts/ISignalProvider.h"
#include "lx/contracts/ISocketOwnerResolver.h"
#include "lx/contracts/ISocketProvider.h"
#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include "lx/linux/procfs/ProcFsProcessProvider.h"
#include "lx/linux/procfs/SocketInodeResolver.h"

#include <benchmark/benchmark.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {

class ProcFixture final {
public:
    explicit ProcFixture(const std::size_t count)
        : root_(std::filesystem::temp_directory_path() /
                ("lx-benchmark-" + std::to_string(::getpid()) + "-" +
                 std::to_string(count)))
    {
        std::filesystem::remove_all(root_);
        for (std::size_t index = 1; index <= count; ++index) {
            const auto pid = static_cast<pid_t>(1000 + index);
            const auto directory = root_ / std::to_string(pid);
            std::filesystem::create_directories(directory / "fd");
            std::ofstream(directory / "stat")
                << pid << " (benchmark) S 1 2 3 4\n";
            std::ofstream(directory / "status")
                << "Uid:\t" << ::getuid() << "\nGid:\t" << ::getgid()
                << "\nThreads:\t1\nVmRSS:\t128 kB\n";
            std::ofstream command(directory / "cmdline", std::ios::binary);
            const std::string argv{"benchmark\0--worker\0", 19};
            command.write(argv.data(), static_cast<std::streamsize>(argv.size()));
            std::filesystem::create_symlink(
                "socket:[" + std::to_string(100000 + index) + "]",
                directory / "fd" / "3");
        }
    }

    ~ProcFixture()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

void listSockets(benchmark::State& state)
{
    const lx::linux::netlink::NetlinkSocketProvider provider;
    for (auto _ : state) {
        auto result = provider.query({});
        benchmark::DoNotOptimize(result);
        if (!result) state.SkipWithError(result.error().message.c_str());
    }
}

void resolveSocketInodes(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    const ProcFixture fixture{count};
    const lx::linux::procfs::SocketInodeResolver resolver{fixture.root()};
    std::vector<std::uint64_t> inodes;
    inodes.reserve(count);
    for (std::size_t index = 1; index <= count; ++index) {
        inodes.push_back(100000 + index);
    }
    for (auto _ : state) {
        auto result = resolver.resolve(inodes);
        benchmark::DoNotOptimize(result);
        if (!result) state.SkipWithError(result.error().message.c_str());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(count));
}

void listProcesses(benchmark::State& state)
{
    const auto count = static_cast<std::size_t>(state.range(0));
    const ProcFixture fixture{count};
    const lx::linux::procfs::ProcFsProcessProvider provider{fixture.root()};
    for (auto _ : state) {
        auto result = provider.list();
        benchmark::DoNotOptimize(result);
        if (!result) state.SkipWithError(result.error().message.c_str());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(count));
}

class SocketProvider final : public lx::contracts::ISocketProvider {
public:
    explicit SocketProvider(const std::size_t count)
    {
        sockets_.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            lx::SocketInfo socket;
            socket.local = {"127.0.0.1", 8080};
            socket.state = "LISTEN";
            socket.inode = 200000 + index;
            sockets_.push_back(std::move(socket));
        }
    }

    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(
        const lx::SocketQuery&) const override
    {
        return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success(
            {sockets_, {}});
    }

private:
    std::vector<lx::SocketInfo> sockets_;
};

class OwnerResolver final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>& inodes) const override
    {
        lx::SocketOwnership owners;
        for (const auto inode : inodes) owners[inode] = {42};
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {std::move(owners), {}});
    }
};

class ProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(pid_t pid) const override
    {
        lx::ProcessInfo process;
        process.pid = pid;
        process.name = "benchmark";
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(process), {}});
    }

    lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>> list()
        const override
    {
        return lx::Result<lx::Observation<std::vector<lx::ProcessInfo>>>::success(
            {{}, {}});
    }
};

class SignalProvider final : public lx::contracts::ISignalProvider {
public:
    lx::Result<lx::SignalDelivery> send(
        pid_t, lx::ProcessSignal) const override
    {
        return lx::Result<lx::SignalDelivery>::failure({});
    }
    lx::Result<bool> waitForExit(
        pid_t, std::chrono::milliseconds) const override
    {
        return lx::Result<bool>::success(false);
    }
    lx::SignalCapabilities capabilities() const override { return {}; }
};

void inspectPort(benchmark::State& state)
{
    const SocketProvider sockets{static_cast<std::size_t>(state.range(0))};
    const OwnerResolver owners;
    const ProcessProvider processes;
    const SignalProvider signals;
    const lx::application::ProcessService processService{
        processes, signals, ::getpid()};
    const lx::application::PortService portService{
        sockets, owners, processService};
    lx::SocketQuery query;
    query.localPort = 8080;
    for (auto _ : state) {
        auto result = portService.inspect(query);
        benchmark::DoNotOptimize(result);
        if (!result) state.SkipWithError(result.error().message.c_str());
    }
}

BENCHMARK(listSockets)->Name("BM_ListSockets");
BENCHMARK(resolveSocketInodes)->Name("BM_ResolveSocketInodes")->Arg(100)->Arg(1000);
BENCHMARK(listProcesses)->Name("BM_ListProcesses")->Arg(100)->Arg(1000);
BENCHMARK(inspectPort)->Name("BM_InspectPort")->Arg(100)->Arg(1000);

} // namespace
