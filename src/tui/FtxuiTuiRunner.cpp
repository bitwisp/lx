#include "lx/tui/FtxuiTuiRunner.h"

#include "lx/application/DashboardWorker.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fmt/format.h>

#include <exception>
#include <algorithm>
#include <string>
#include <vector>

namespace lx::tui {
namespace {

std::string clipped(const std::string& value, const std::size_t width)
{
    if (value.size() <= width) return value;
    return width > 3 ? value.substr(0, width - 3) + "..."
                     : value.substr(0, width);
}

ftxui::Element resourcePanel(const DashboardSnapshot& snapshot,
                             const int tab, const int selected)
{
    using namespace ftxui;
    Elements rows;
    std::vector<std::string> values;
    if (tab == 0) {
        rows.push_back(text(" SERVICE                                  ACTIVE       SUB             PID"));
        for (const auto& service : snapshot.services) {
            values.push_back(fmt::format(" {:<40} {:<12} {:<15} {}",
                clipped(service.unitName, 40), clipped(service.activeState, 12),
                clipped(service.subState, 15),
                service.mainPid ? std::to_string(*service.mainPid) : "-"));
        }
    } else if (tab == 1) {
        rows.push_back(text(" PROTO  ADDRESS                                  PORT   PID       PROCESS"));
        for (const auto& port : snapshot.ports) {
            const auto& socket = port.socket;
            const auto pid = socket.ownerPids.empty()
                                 ? "-" : std::to_string(socket.ownerPids.front());
            const auto owner = port.owners.empty() ? "-" : port.owners.front().name;
            values.push_back(fmt::format(" {:<6} {:<40} {:<6} {:<9} {}",
                socket.protocol == TransportProtocol::Tcp ? "TCP" : "UDP",
                clipped(socket.local.address, 40), socket.local.port, pid,
                clipped(owner, 24)));
        }
    } else {
        rows.push_back(text(" PID      CPU %   RSS MiB   USER             NAME                     SERVICE"));
        for (const auto& process : snapshot.processes) {
            values.push_back(fmt::format(" {:<8} {:>6} {:>9.1f} {:<16} {:<24} {}",
                process.pid,
                process.cpuPercent ? fmt::format("{:.1f}", *process.cpuPercent) : "-",
                static_cast<double>(process.rssBytes) / (1024.0 * 1024.0),
                clipped(process.user, 16), clipped(process.name, 24),
                process.systemdUnit.value_or("-")));
        }
    }
    rows.push_back(separator());
    const auto begin = std::max(0, selected - 10);
    const auto end = std::min<int>(static_cast<int>(values.size()), begin + 22);
    for (int index = begin; index < end; ++index) {
        auto row = text(values[static_cast<std::size_t>(index)]);
        if (index == selected) row = row | inverted;
        rows.push_back(std::move(row));
    }
    if (values.empty()) rows.push_back(text(" No resources available") | dim);
    return vbox(std::move(rows)) | frame | flex;
}

int itemCount(const DashboardSnapshot& snapshot, const int tab)
{
    if (tab == 0) return static_cast<int>(snapshot.services.size());
    if (tab == 1) return static_cast<int>(snapshot.ports.size());
    return static_cast<int>(snapshot.processes.size());
}

} // namespace

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
        int selected[] = {0, 0, 0};
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
                Elements tabElements;
                for (int index = 0; index < 3; ++index) {
                    auto label = text(fmt::format(" {} ({}) ", tabs[index],
                        index == 0 ? snapshot->services.size()
                        : index == 1 ? snapshot->ports.size()
                                     : snapshot->processes.size()));
                    tabElements.push_back(index == tab ? label | inverted | bold
                                                       : label);
                }
                rows.push_back(hbox(std::move(tabElements)));
                rows.push_back(resourcePanel(*snapshot, tab, selected[tab]));
                if (!snapshot->warnings.empty()) {
                    rows.push_back(separator());
                    rows.push_back(text("Warning: " + snapshot->warnings.front().message) |
                                   color(Color::Yellow));
                }
            }
            rows.push_back(separator());
            rows.push_back(text(" Tab switch panel   arrows select   Enter inspect   / find   l logs   F1 help   q quit ") | dim);
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
            if (event == Event::ArrowLeft) {
                tab = (tab + 2) % 3;
                return true;
            }
            if (event == Event::ArrowRight) {
                tab = (tab + 1) % 3;
                return true;
            }
            const auto snapshot = worker.snapshot();
            if (snapshot && event == Event::ArrowUp) {
                selected[tab] = std::max(0, selected[tab] - 1);
                return true;
            }
            if (snapshot && event == Event::ArrowDown) {
                selected[tab] = std::min(
                    std::max(0, itemCount(*snapshot, tab) - 1),
                    selected[tab] + 1);
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
