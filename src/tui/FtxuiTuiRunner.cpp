#include "lx/tui/FtxuiTuiRunner.h"

#include "lx/application/DashboardWorker.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fmt/format.h>

#include <exception>
#include <string>

namespace lx::tui {

FtxuiTuiRunner::FtxuiTuiRunner(application::DashboardService& dashboard)
    : dashboard_(dashboard)
{
}

Result<void> FtxuiTuiRunner::run()
{
    try {
        using namespace ftxui;
        auto screen = ScreenInteractive::Fullscreen();
        application::DashboardWorker worker{dashboard_};
        int tab = 0;
        const char* tabs[] = {"Services", "Ports", "Processes"};

        auto content = Renderer([&] {
            const auto snapshot = worker.snapshot();
            Elements rows;
            rows.push_back(text(" LX resource dashboard ") | bold | center);
            if (!snapshot) {
                rows.push_back(separator());
                rows.push_back(text("Loading resources...") | center | flex);
            } else {
                const auto cpu = snapshot->host.cpuPercent
                                     ? fmt::format("{:.1f}%", *snapshot->host.cpuPercent)
                                     : "-";
                rows.push_back(text(fmt::format(
                    " {}   CPU {}   Memory {:.1f}/{:.1f} MiB ",
                    snapshot->host.hostname, cpu,
                    static_cast<double>(snapshot->host.memoryUsedBytes) /
                        (1024.0 * 1024.0),
                    static_cast<double>(snapshot->host.memoryTotalBytes) /
                        (1024.0 * 1024.0))));
                rows.push_back(separator());
                rows.push_back(text(fmt::format(
                    "[{}]  Services {}  Ports {}  Processes {}",
                    tabs[tab], snapshot->services.size(), snapshot->ports.size(),
                    snapshot->processes.size())) | bold);
                rows.push_back(text("Dashboard panels are loading") | center | flex);
                if (!snapshot->warnings.empty()) {
                    rows.push_back(separator());
                    rows.push_back(text("Warning: " + snapshot->warnings.front().message) |
                                   color(Color::Yellow));
                }
            }
            rows.push_back(separator());
            rows.push_back(text(" Tab switch panel   q quit ") | dim);
            return vbox(std::move(rows)) | border;
        });
        auto root = CatchEvent(content, [&](const Event& event) {
            if (event == Event::Character('q') || event == Event::Escape) {
                screen.ExitLoopClosure()();
                return true;
            }
            if (event == Event::Tab) {
                tab = (tab + 1) % 3;
                return true;
            }
            return false;
        });

        worker.start([&screen] { screen.PostEvent(Event::Custom); });
        screen.Loop(root);
        worker.stop();
        return Result<void>::success();
    } catch (const std::exception& exception) {
        return Result<void>::failure(
            {ErrorCode::OperationFailed,
             std::string{"Unable to run terminal UI: "} + exception.what(), 0,
             "tui", "run"});
    }
}

} // namespace lx::tui
