#pragma once

#include "lx/contracts/IDashboardProvider.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace lx::application {

class DashboardWorker final {
public:
    using Refreshed = std::function<void()>;

    explicit DashboardWorker(contracts::IDashboardProvider& service);
    ~DashboardWorker();
    DashboardWorker(const DashboardWorker&) = delete;
    DashboardWorker& operator=(const DashboardWorker&) = delete;

    void start(Refreshed refreshed = {});
    void stop() noexcept;
    [[nodiscard]] std::shared_ptr<const DashboardSnapshot> snapshot() const;

private:
    void work(Refreshed refreshed) noexcept;

    contracts::IDashboardProvider& service_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::shared_ptr<const DashboardSnapshot> snapshot_;
    std::thread thread_;
    bool stopping_ = false;
};

} // namespace lx::application
