#include "lx/linux/procfs/ProcFsReader.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

class TempProc final {
public:
    TempProc()
        : root_(std::filesystem::temp_directory_path() /
                ("lx-procfs-reader-" + std::to_string(::getpid())))
    {
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_ / "42");
    }

    ~TempProc() { std::filesystem::remove_all(root_); }

    const std::filesystem::path& root() const { return root_; }

private:
    std::filesystem::path root_;
};

} // namespace

TEST_CASE("ProcFsReader reads binary process files from an injected root")
{
    TempProc fixture;
    {
        std::ofstream output(fixture.root() / "42" / "cmdline", std::ios::binary);
        const std::string data{"app\0--flag\0", 11};
        output.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    const lx::linux::procfs::ProcFsReader reader{fixture.root()};
    const auto result = reader.readFile(42, "cmdline");

    REQUIRE(result);
    REQUIRE(result.value().size() == 11);
    REQUIRE(result.value()[3] == '\0');
}

TEST_CASE("ProcFsReader reads process symbolic links")
{
    TempProc fixture;
    std::filesystem::create_symlink("/usr/bin/demo", fixture.root() / "42" / "exe");

    const lx::linux::procfs::ProcFsReader reader{fixture.root()};
    const auto result = reader.readLink(42, "exe");

    REQUIRE(result);
    REQUIRE(result.value() == "/usr/bin/demo");
}

TEST_CASE("ProcFsReader maps a disappeared process to not found")
{
    TempProc fixture;
    const lx::linux::procfs::ProcFsReader reader{fixture.root()};

    const auto result = reader.readFile(999, "status");

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::NotFound);
    REQUIRE(result.error().systemError != 0);
    REQUIRE(result.error().message.find("Unable to read status") !=
            std::string::npos);
}
