#pragma once

#include "lx/application/DashboardService.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace lx::application {

class DashboardWorker final {
public:
    using Refreshed = std::function<void()>;

    explicit DashboardWorker(DashboardService& service);
    ~DashboardWorker();
    DashboardWorker(const DashboardWorker&) = delete;
    DashboardWorker& operator=(const DashboardWorker&) = delete;

    void start(Refreshed refreshed = {});
    void stop() noexcept;
    [[nodiscard]] std::shared_ptr<const DashboardSnapshot> snapshot() const;

private:
    void work(Refreshed refreshed) noexcept;

    DashboardService& service_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::shared_ptr<const DashboardSnapshot> snapshot_;
    std::thread thread_;
    bool stopping_ = false;
};

} // namespace lx::application
