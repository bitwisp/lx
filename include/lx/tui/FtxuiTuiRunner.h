#pragma once

#include "lx/cli/ITuiRunner.h"

namespace lx::application {
class DashboardService;
class InspectService;
class FindService;
class LogService;
class ProcessService;
class ServiceService;
}

namespace lx::tui {

class FtxuiTuiRunner final : public cli::ITuiRunner {
public:
    FtxuiTuiRunner(application::DashboardService& dashboard,
                   const application::InspectService& inspect,
                   const application::FindService& find,
                   const application::LogService& logs,
                   const application::ProcessService& processes,
                   const application::ServiceService& services);
    [[nodiscard]] Result<void> run() override;

private:
    application::DashboardService& dashboard_;
    const application::InspectService& inspect_;
    const application::FindService& find_;
    const application::LogService& logs_;
    const application::ProcessService& processes_;
    const application::ServiceService& services_;
};

} // namespace lx::tui
