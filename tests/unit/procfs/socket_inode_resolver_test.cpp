#include "lx/linux/procfs/SocketInodeResolver.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>

namespace {

class ProcFixture final {
public:
    ProcFixture()
        : root_(std::filesystem::temp_directory_path() /
                ("lx-proc-fixture-" + std::to_string(::getpid())))
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);
    }

    ~ProcFixture()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void addDescriptor(const pid_t pid, const std::string& descriptor,
                       const std::string& target) const
    {
        const auto directory = root_ / std::to_string(pid) / "fd";
        std::filesystem::create_directories(directory);
        std::filesystem::create_symlink(target, directory / descriptor);
    }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("socket inode resolver scans procfs once for all owners")
{
    const ProcFixture fixture;
    fixture.addDescriptor(20, "3", "socket:[42]");
    fixture.addDescriptor(10, "4", "socket:[42]");
    fixture.addDescriptor(10, "5", "socket:[42]");
    fixture.addDescriptor(10, "6", "socket:[84]");
    fixture.addDescriptor(30, "7", "pipe:[42]");
    std::filesystem::create_directories(fixture.root() / "self" / "fd");

    const lx::linux::procfs::SocketInodeResolver resolver{fixture.root()};
    const auto result = resolver.resolve({42, 84, 42, 999});

    REQUIRE(result);
    REQUIRE(result.value().warnings.empty());
    REQUIRE(result.value().value.at(42) == std::vector<pid_t>{10, 20});
    REQUIRE(result.value().value.at(84) == std::vector<pid_t>{10});
    REQUIRE(result.value().value.find(999) == result.value().value.end());
}

TEST_CASE("socket inode resolver handles empty targets and missing procfs")
{
    const ProcFixture fixture;
    const lx::linux::procfs::SocketInodeResolver resolver{fixture.root()};

    const auto empty = resolver.resolve({0});
    REQUIRE(empty);
    REQUIRE(empty.value().value.empty());

    const lx::linux::procfs::SocketInodeResolver missing{
        fixture.root() / "missing"};
    const auto failure = missing.resolve({42});
    REQUIRE_FALSE(failure);
    REQUIRE(failure.error().code == lx::ErrorCode::NotFound);
}

TEST_CASE("socket inode resolver reports inaccessible process descriptors")
{
    if (::geteuid() == 0) {
        SKIP("root bypasses procfs fixture permissions");
    }

    const ProcFixture fixture;
    const auto descriptors = fixture.root() / "10" / "fd";
    std::filesystem::create_directories(descriptors);
    std::filesystem::permissions(descriptors, std::filesystem::perms::none);

    const lx::linux::procfs::SocketInodeResolver resolver{fixture.root()};
    const auto result = resolver.resolve({42});

    std::filesystem::permissions(
        descriptors, std::filesystem::perms::owner_all);
    REQUIRE(result);
    REQUIRE(result.value().warnings.size() == 1);
    REQUIRE(result.value().warnings.front().code ==
            lx::ErrorCode::PermissionDenied);
}
