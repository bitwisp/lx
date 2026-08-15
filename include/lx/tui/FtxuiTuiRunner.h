#pragma once

#include "lx/cli/ITuiRunner.h"

namespace lx::application {
class DashboardService;
}

namespace lx::tui {

class FtxuiTuiRunner final : public cli::ITuiRunner {
public:
    explicit FtxuiTuiRunner(application::DashboardService& dashboard);
    [[nodiscard]] Result<void> run() override;

private:
    application::DashboardService& dashboard_;
};

} // namespace lx::tui
