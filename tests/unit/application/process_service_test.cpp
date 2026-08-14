#include "lx/application/ProcessService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class FakeProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(const pid_t pid) const override
    {
        lx::ProcessInfo info;
        info.pid = pid;
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(info), {}});
    }
};

} // namespace

TEST_CASE("ProcessService delegates valid process inspection")
{
    const FakeProcessProvider provider;
    const lx::application::ProcessService service{provider};

    const auto result = service.inspect(42);

    REQUIRE(result);
    REQUIRE(result.value().value.pid == 42);
}

TEST_CASE("ProcessService rejects non-positive PIDs")
{
    const FakeProcessProvider provider;
    const lx::application::ProcessService service{provider};

    const auto result = service.inspect(0);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == lx::ErrorCode::InvalidArgument);
}
