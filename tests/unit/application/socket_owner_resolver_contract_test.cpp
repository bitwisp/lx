#include "lx/contracts/ISocketOwnerResolver.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class SharedOwnerResolver final : public lx::contracts::ISocketOwnerResolver {
public:
    lx::Result<lx::Observation<lx::SocketOwnership>> resolve(
        const std::vector<std::uint64_t>& inodes) const override
    {
        lx::SocketOwnership ownership;
        if (!inodes.empty()) {
            ownership.emplace(inodes.front(), std::vector<pid_t>{10, 20});
        }
        return lx::Result<lx::Observation<lx::SocketOwnership>>::success(
            {std::move(ownership), {}});
    }
};

} // namespace

TEST_CASE("socket owner resolver contract permits multiple owners")
{
    const SharedOwnerResolver resolver;
    const auto result = resolver.resolve({42});

    REQUIRE(result);
    REQUIRE(result.value().value.at(42) == std::vector<pid_t>{10, 20});
}
