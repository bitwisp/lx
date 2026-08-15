#include "lx/application/DashboardWorker.h"

#include <catch2/catch_test_macros.hpp>

#include <condition_variable>
#include <mutex>

namespace {

class DashboardProvider final : public lx::contracts::IDashboardProvider {
public:
    lx::DashboardSnapshot refresh() override
    {
        lx::DashboardSnapshot value;
        value.host.hostname = "worker-test";
        ++refreshes;
        return value;
    }
    int refreshes = 0;
};

} // namespace

TEST_CASE("dashboard worker publishes immutable snapshots and stops promptly")
{
    DashboardProvider provider;
    lx::application::DashboardWorker worker{provider};
    std::mutex mutex;
    std::condition_variable updated;
    bool ready = false;
    worker.start([&] {
        {
            std::lock_guard<std::mutex> lock(mutex);
            ready = true;
        }
        updated.notify_one();
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        REQUIRE(updated.wait_for(lock, std::chrono::seconds{2}, [&] { return ready; }));
    }
    const auto snapshot = worker.snapshot();
    REQUIRE(snapshot);
    CHECK(snapshot->host.hostname == "worker-test");
    worker.stop();
    CHECK(provider.refreshes >= 1);
}
