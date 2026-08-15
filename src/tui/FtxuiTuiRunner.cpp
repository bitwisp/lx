#include "lx/tui/FtxuiTuiRunner.h"

#include "lx/application/DashboardWorker.h"
#include "lx/application/DashboardService.h"
#include "lx/application/FindService.h"
#include "lx/application/InspectService.h"
#include "lx/application/LogService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ServiceService.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <fmt/format.h>

#include <exception>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
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

std::string selectedResource(const DashboardSnapshot& snapshot,
                             const int tab, const int selected)
{
    if (selected < 0 || selected >= itemCount(snapshot, tab)) return {};
    const auto index = static_cast<std::size_t>(selected);
    if (tab == 0) return "service:" + snapshot.services[index].unitName;
    if (tab == 1) return "port:" +
                         std::to_string(snapshot.ports[index].socket.local.port);
    return "pid:" + std::to_string(snapshot.processes[index].pid);
}

std::vector<std::string> lines(const std::string& value)
{
    std::vector<std::string> result;
    std::istringstream input{value};
    std::string line;
    while (std::getline(input, line)) result.push_back(std::move(line));
    return result;
}

class TaskWorker final {
public:
    using Task = std::function<std::string()>;
    using Updated = std::function<void()>;
    explicit TaskWorker(Updated updated)
        : updated_(std::move(updated)), thread_(&TaskWorker::work, this) {}
    ~TaskWorker()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        wake_.notify_all();
        thread_.join();
    }
    bool submit(Task task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pending_ || busy_) return false;
        pending_ = std::move(task);
        result_ = "Loading...";
        wake_.notify_one();
        return true;
    }
    std::string result() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return result_;
    }
    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result_.clear();
    }
private:
    void work() noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (true) {
            wake_.wait(lock, [this] { return stopping_ || pending_.has_value(); });
            if (stopping_) break;
            auto task = std::move(*pending_);
            pending_.reset();
            busy_ = true;
            lock.unlock();
            std::string value;
            try { value = task(); }
            catch (const std::exception& error) { value = error.what(); }
            catch (...) { value = "Unknown background task failure"; }
            lock.lock();
            result_ = std::move(value);
            busy_ = false;
            lock.unlock();
            if (updated_) updated_();
            lock.lock();
        }
    }
    Updated updated_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<Task> pending_;
    std::string result_;
    std::thread thread_;
    bool busy_ = false;
    bool stopping_ = false;
};

std::string inspectText(const Result<ResourceGraph>& result)
{
    if (!result) return "Inspect failed: " + result.error().message;
    const auto& graph = result.value();
    std::ostringstream out;
    out << "Inspect result\n"
        << "Processes: " << graph.processes.size() << '\n'
        << "Ports: " << graph.ports.size() << '\n'
        << "Services: " << graph.services.size() << '\n'
        << "Recent logs: " << graph.recentLogs.size();
    for (const auto& process : graph.processes) {
        out << "\nPID " << process.pid << "  " << process.name;
    }
    for (const auto& service : graph.services) {
        out << "\n" << service.unitName << "  " << service.activeState;
    }
    return out.str();
}

std::string findText(const Result<FindResult>& result)
{
    if (!result) return "Find failed: " + result.error().message;
    const auto& found = result.value();
    std::ostringstream out;
    out << "Find results\nServices: " << found.services.size()
        << "  Processes: " << found.processes.size()
        << "  Ports: " << found.ports.size();
    for (const auto& service : found.services) out << "\nservice  " << service.unitName;
    for (const auto& process : found.processes) out << "\nprocess  " << process.pid << " " << process.name;
    for (const auto& port : found.ports) out << "\nport     " << port.socket.local.port;
    for (const auto& executable : found.executables) out << "\nexe      " << executable;
    return out.str();
}

} // namespace

FtxuiTuiRunner::FtxuiTuiRunner(application::DashboardService& dashboard,
                               const application::InspectService& inspect,
                               const application::FindService& find,
                               const application::LogService& logs,
                               const application::ProcessService& processes,
                               const application::ServiceService& services)
    : dashboard_(dashboard), inspect_(inspect), find_(find), logs_(logs),
      processes_(processes), services_(services)
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
        bool searching = false;
        bool help = false;
        enum class PendingAction { None, StopService, RestartService,
                                   TerminateProcess, KillProcess };
        PendingAction pendingAction = PendingAction::None;
        int confirmationStep = 0;
        std::string actionTarget;
        pid_t actionPid = -1;
        std::string searchQuery;
        auto searchInput = Input(&searchQuery, "name, PID, port, path...");
        TaskWorker tasks{[&screen] { screen.PostEvent(Event::Custom); }};

        auto content = Renderer(searchInput, [&] {
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
                const auto taskResult = tasks.result();
                if (pendingAction != PendingAction::None) {
                    const auto action = pendingAction == PendingAction::StopService
                                            ? "Stop service"
                                        : pendingAction == PendingAction::RestartService
                                            ? "Restart service"
                                        : pendingAction == PendingAction::TerminateProcess
                                            ? "Send SIGTERM to process"
                                            : "Send SIGKILL to process";
                    const auto warning = pendingAction == PendingAction::KillProcess &&
                                                 confirmationStep == 2
                                             ? "FINAL CONFIRMATION: this cannot be undone"
                                             : "Confirm the selected action";
                    rows.push_back(vbox({text(action) | bold,
                                         text(actionTarget),
                                         separator(),
                                         text(warning) | color(Color::Yellow),
                                         text("Press y to continue or n/Esc to cancel")}) |
                                   border | center | flex);
                } else if (help) {
                    rows.push_back(vbox({text("Keyboard help") | bold,
                        text("Tab/Left/Right  switch panel"),
                        text("Up/Down         select resource"),
                        text("Enter           inspect selected resource"),
                        text("/               find resources"),
                        text("l               recent logs for selected resource"),
                        text("Esc             close view / quit"),
                        text("q               quit")}) | border | flex);
                } else if (!taskResult.empty()) {
                    Elements resultLines;
                    for (const auto& line : lines(taskResult)) resultLines.push_back(text(line));
                    rows.push_back(vbox(std::move(resultLines)) | border | flex);
                } else {
                    rows.push_back(resourcePanel(*snapshot, tab, selected[tab]));
                }
                if (!snapshot->warnings.empty()) {
                    rows.push_back(separator());
                    rows.push_back(text("Warning: " + snapshot->warnings.front().message) |
                                   color(Color::Yellow));
                }
            }
            if (searching) {
                rows.push_back(hbox({text(" Find: "), searchInput->Render()}) |
                               border);
            }
            rows.push_back(separator());
            rows.push_back(text(" Tab/arrows navigate  Enter inspect  / find  l logs  s/r service  k/K process  F1 help  q quit ") | dim);
            return vbox(std::move(rows)) | border;
        });
        auto root = CatchEvent(content, [&](const Event& event) {
            if (searching && event == Event::Return) {
                if (!searchQuery.empty()) {
                    const auto query = searchQuery;
                    tasks.submit([this, query] { return findText(find_.find(query)); });
                }
                searching = false;
                return true;
            }
            if (pendingAction != PendingAction::None &&
                (event == Event::Character('n') || event == Event::Escape)) {
                pendingAction = PendingAction::None;
                confirmationStep = 0;
                return true;
            }
            if (pendingAction != PendingAction::None &&
                event == Event::Character('y')) {
                if (pendingAction == PendingAction::KillProcess &&
                    confirmationStep == 1) {
                    confirmationStep = 2;
                    return true;
                }
                const auto action = pendingAction;
                const auto unit = actionTarget;
                const auto pid = actionPid;
                pendingAction = PendingAction::None;
                confirmationStep = 0;
                tasks.submit([this, action, unit, pid] {
                    if (action == PendingAction::StopService ||
                        action == PendingAction::RestartService) {
                        const auto result = action == PendingAction::StopService
                                                ? services_.stop(unit)
                                                : services_.restart(unit);
                        return result ? std::string{"Service action submitted for "} + unit
                                      : std::string{"Service action failed: "} +
                                            result.error().message;
                    }
                    const auto result = action == PendingAction::TerminateProcess
                                            ? processes_.stop(pid)
                                            : processes_.kill(pid);
                    return result ? std::string{"Signal sent to PID "} + std::to_string(pid)
                                  : std::string{"Signal failed: "} + result.error().message;
                });
                return true;
            }
            if (pendingAction != PendingAction::None) return true;
            if (event == Event::Escape && (searching || help || !tasks.result().empty())) {
                searching = false;
                help = false;
                tasks.clear();
                return true;
            }
            if (!searching && (event == Event::Character('q') || event == Event::Escape)) {
                screen.ExitLoopClosure()();
                return true;
            }
            if (!searching && event == Event::F1) {
                help = !help;
                tasks.clear();
                return true;
            }
            if (!searching && event == Event::Character('/')) {
                searching = true;
                searchQuery.clear();
                help = false;
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
            if (!searching && snapshot && tab == 0 &&
                (event == Event::Character('s') ||
                 event == Event::Character('r')) &&
                selected[tab] < itemCount(*snapshot, tab)) {
                const auto& service = snapshot->services[
                    static_cast<std::size_t>(selected[tab])];
                pendingAction = event == Event::Character('s')
                                    ? PendingAction::StopService
                                    : PendingAction::RestartService;
                actionTarget = service.unitName;
                confirmationStep = 1;
                tasks.clear();
                return true;
            }
            if (!searching && snapshot && tab == 2 &&
                (event == Event::Character('k') ||
                 event == Event::Character('K')) &&
                selected[tab] < itemCount(*snapshot, tab)) {
                const auto& process = snapshot->processes[
                    static_cast<std::size_t>(selected[tab])];
                pendingAction = event == Event::Character('k')
                                    ? PendingAction::TerminateProcess
                                    : PendingAction::KillProcess;
                actionPid = process.pid;
                actionTarget = fmt::format("PID {} ({})", process.pid, process.name);
                confirmationStep = 1;
                tasks.clear();
                return true;
            }
            if (!searching && snapshot && event == Event::Return) {
                const auto resource = selectedResource(*snapshot, tab, selected[tab]);
                if (!resource.empty()) {
                    tasks.submit([this, resource] {
                        return inspectText(inspect_.inspect(resource));
                    });
                }
                return true;
            }
            if (!searching && snapshot && event == Event::Character('l')) {
                LogQuery query;
                query.limit = 20;
                if (tab == 0 && selected[tab] < itemCount(*snapshot, tab)) {
                    query.unit = snapshot->services[static_cast<std::size_t>(selected[tab])].unitName;
                } else if (tab == 2 && selected[tab] < itemCount(*snapshot, tab)) {
                    query.pid = snapshot->processes[static_cast<std::size_t>(selected[tab])].pid;
                } else {
                    tasks.submit([] { return std::string{"Select a service or process to view logs"}; });
                    return true;
                }
                tasks.submit([this, query] {
                    const auto result = logs_.read(query);
                    if (!result) return std::string{"Logs failed: "} + result.error().message;
                    std::ostringstream out;
                    out << "Recent logs (" << result.value().value.size() << ")";
                    for (const auto& entry : result.value().value) out << "\n" << entry.message;
                    return out.str();
                });
                return true;
            }
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
            if (!searching && event.is_character()) return true;
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
