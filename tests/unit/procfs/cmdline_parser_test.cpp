#include "lx/linux/procfs/CmdlineParser.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("cmdline parser splits NUL-delimited arguments")
{
    const std::string input{"demo\0--port\08080\0", 17};
    const auto arguments = lx::linux::procfs::parseCmdline(input);
    REQUIRE(arguments == std::vector<std::string>{"demo", "--port", "8080"});
}

TEST_CASE("cmdline parser preserves empty arguments")
{
    const std::string input{"demo\0\0tail\0", 11};
    const auto arguments = lx::linux::procfs::parseCmdline(input);
    REQUIRE(arguments == std::vector<std::string>{"demo", "", "tail"});
}

TEST_CASE("cmdline parser accepts empty and unterminated data")
{
    REQUIRE(lx::linux::procfs::parseCmdline("").empty());
    REQUIRE(lx::linux::procfs::parseCmdline("demo --flag") ==
            std::vector<std::string>{"demo --flag"});
}
