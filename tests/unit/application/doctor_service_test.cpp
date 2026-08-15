#include "lx/application/DoctorService.h"

#include <catch2/catch_test_macros.hpp>

namespace {

class DoctorProcessProvider final : public lx::contracts::IProcessProvider {
public:
    lx::Result<lx::Observation<lx::ProcessInfo>> get(const pid_t pid) const override
    {
        lx::ProcessInfo info;
        info.pid = pid;
        return lx::Result<lx::Observation<lx::ProcessInfo>>::success(
            {std::move(info), {}});
    }
};
class DoctorSocketProvider final : public lx::contracts::ISocketProvider {
public:
    lx::Result<lx::Observation<std::vector<lx::SocketInfo>>> query(const lx::SocketQuery&) const override
    { return lx::Result<lx::Observation<std::vector<lx::SocketInfo>>>::success({{}, {}}); }
};

} // namespace

TEST_CASE("DoctorService reports only implemented foundation as available")
{
    const DoctorProcessProvider provider;
    const DoctorSocketProvider sockets;
    const auto report = lx::application::DoctorService{provider, sockets}.inspect();

    REQUIRE(report.checks.size() == 5);
    REQUIRE(report.checks.front().name == "Project foundation");
    REQUIRE(report.checks.front().status == lx::CapabilityStatus::Available);

    REQUIRE(report.checks[1].status == lx::CapabilityStatus::Available);
    REQUIRE(report.checks[2].status == lx::CapabilityStatus::Available);
    for (auto check = report.checks.begin() + 3; check != report.checks.end();
         ++check) {
        REQUIRE(check->status == lx::CapabilityStatus::NotImplemented);
        REQUIRE_FALSE(check->detail.empty());
    }
}
