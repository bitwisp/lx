#include "lx/application/DashboardWorker.h"

#include <chrono>
#include <utility>

namespace lx::application {

DashboardWorker::DashboardWorker(contracts::IDashboardProvider& service)
    : service_(service)
{
}

DashboardWorker::~DashboardWorker()
{
    stop();
}

void DashboardWorker::start(Refreshed refreshed)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (thread_.joinable()) return;
    stopping_ = false;
    thread_ = std::thread(&DashboardWorker::work, this, std::move(refreshed));
}

void DashboardWorker::stop() noexcept
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

std::shared_ptr<const DashboardSnapshot> DashboardWorker::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void DashboardWorker::work(Refreshed refreshed) noexcept
{
    try {
        while (true) {
            auto next = std::make_shared<const DashboardSnapshot>(service_.refresh());
            {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_ = std::move(next);
            }
            if (refreshed) refreshed();

            std::unique_lock<std::mutex> lock(mutex_);
            if (wake_.wait_for(lock, std::chrono::milliseconds{1500},
                               [this] { return stopping_; })) {
                break;
            }
        }
    } catch (...) {
        // Keep exceptions inside the worker; the previous immutable snapshot
        // remains safe for the presentation thread.
    }
}

} // namespace lx::application
