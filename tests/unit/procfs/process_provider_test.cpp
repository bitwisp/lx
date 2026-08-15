#include "lx/linux/procfs/ProcFsProcessProvider.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {
class ProcessFixture final {
public:
    ProcessFixture()
        : root_(std::filesystem::temp_directory_path() /
                ("lx-process-provider-" + std::to_string(::getpid())))
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / "42");
        std::ofstream(root_ / "42" / "stat") << "42 (demo worker) S 1 2 3 4\n";
        std::ofstream(root_ / "42" / "status")
            << "Uid:\t" << ::getuid() << "\nGid:\t" << ::getgid()
            << "\nThreads:\t3\nVmRSS:\t2048 kB\n";
        std::ofstream command(root_ / "42" / "cmdline", std::ios::binary);
        const std::string data{"demo\0--token\0secret\0", 20};
        command.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    ~ProcessFixture() { std::filesystem::remove_all(root_); }
    const std::filesystem::path& root() const { return root_; }
private:
    std::filesystem::path root_;
};
} // namespace

TEST_CASE("process provider assembles procfs process details")
{
    ProcessFixture fixture;
    std::filesystem::create_symlink("/usr/bin/demo", fixture.root() / "42" / "exe");
    std::filesystem::create_symlink("/srv/demo", fixture.root() / "42" / "cwd");
    const lx::linux::procfs::ProcFsProcessProvider provider{fixture.root()};
    const auto result = provider.get(42);
    REQUIRE(result);
    const auto& process = result.value().value;
    REQUIRE(process.name == "demo worker");
    REQUIRE(process.state == "sleeping");
    REQUIRE(process.ppid == 1);
    REQUIRE(process.threads == 3);
    REQUIRE(process.rssBytes == 2048 * 1024);
    REQUIRE(process.executable == "/usr/bin/demo");
    REQUIRE(process.cwd == "/srv/demo");
    REQUIRE(result.value().warnings.empty());
}

TEST_CASE("process provider returns partial details for missing optional files")
{
    ProcessFixture fixture;
    const lx::linux::procfs::ProcFsProcessProvider provider{fixture.root()};
    const auto result = provider.get(42);
    REQUIRE(result);
    REQUIRE_FALSE(result.value().value.executable);
    REQUIRE_FALSE(result.value().value.cwd);
    REQUIRE(result.value().warnings.size() == 2);
}

TEST_CASE("process provider reads the running test process")
{
    const lx::linux::procfs::ProcFsProcessProvider provider;
    const auto result = provider.get(::getpid());
    REQUIRE(result);
    REQUIRE(result.value().value.pid == ::getpid());
    REQUIRE_FALSE(result.value().value.name.empty());
    REQUIRE(result.value().value.uid == ::getuid());
}

TEST_CASE("process provider lists observable processes and preserves warnings")
{
    ProcessFixture fixture;
    std::filesystem::create_directories(fixture.root() / "77");
    std::ofstream(fixture.root() / "77" / "stat") << "invalid";
    const lx::linux::procfs::ProcFsProcessProvider provider{fixture.root()};

    const auto result = provider.list();

    REQUIRE(result);
    REQUIRE(result.value().value.size() == 1);
    REQUIRE(result.value().value.front().pid == 42);
    REQUIRE_FALSE(result.value().warnings.empty());
}
