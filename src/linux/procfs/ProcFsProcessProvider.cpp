#include "lx/linux/procfs/ProcFsProcessProvider.h"

#include "lx/linux/procfs/CmdlineParser.h"
#include "lx/linux/procfs/StatParser.h"
#include "lx/linux/procfs/StatusParser.h"

#include <string>
#include <utility>

namespace lx::linux::procfs {
namespace {

Warning warningFrom(const Error& error)
{
    return {error.code, error.message, error.systemError, error.component,
            error.operation};
}

std::string stateName(const char state)
{
    switch (state) {
    case 'R': return "running";
    case 'S': return "sleeping";
    case 'D': return "disk sleep";
    case 'Z': return "zombie";
    case 'T': return "stopped";
    case 't': return "tracing stop";
    case 'X': case 'x': return "dead";
    case 'I': return "idle";
    default: return std::string(1, state);
    }
}

} // namespace

ProcFsProcessProvider::ProcFsProcessProvider(std::filesystem::path root)
    : reader_(std::move(root))
{
}

Result<Observation<ProcessInfo>> ProcFsProcessProvider::get(const pid_t pid) const
{
    const auto statText = reader_.readFile(pid, "stat");
    if (!statText) return Result<Observation<ProcessInfo>>::failure(statText.error());
    const auto stat = parseStat(statText.value());
    if (!stat) return Result<Observation<ProcessInfo>>::failure(stat.error());
    if (stat.value().pid != pid) {
        return Result<Observation<ProcessInfo>>::failure({
            ErrorCode::ParseError, "Process stat PID does not match request", 0,
            "procfs-process-provider", "get"});
    }
    const auto statusText = reader_.readFile(pid, "status");
    if (!statusText) return Result<Observation<ProcessInfo>>::failure(statusText.error());
    const auto status = parseStatus(statusText.value());
    if (!status) return Result<Observation<ProcessInfo>>::failure(status.error());

    ProcessInfo info;
    info.pid = pid;
    info.ppid = stat.value().ppid;
    info.name = stat.value().name;
    info.state = stateName(stat.value().state);
    info.uid = status.value().uid;
    info.gid = status.value().gid;
    info.threads = status.value().threads;
    info.rssBytes = status.value().rssBytes;
    Observation<ProcessInfo> observation{std::move(info), {}};

    const auto user = userResolver_.nameForUid(observation.value.uid);
    if (user) observation.value.user = user.value();
    else {
        observation.value.user = std::to_string(observation.value.uid);
        observation.warnings.push_back(warningFrom(user.error()));
    }
    const auto cmdline = reader_.readFile(pid, "cmdline");
    if (cmdline) observation.value.argv = parseCmdline(cmdline.value());
    else observation.warnings.push_back(warningFrom(cmdline.error()));
    const auto executable = reader_.readLink(pid, "exe");
    if (executable) observation.value.executable = executable.value();
    else observation.warnings.push_back(warningFrom(executable.error()));
    const auto cwd = reader_.readLink(pid, "cwd");
    if (cwd) observation.value.cwd = cwd.value();
    else observation.warnings.push_back(warningFrom(cwd.error()));

    return Result<Observation<ProcessInfo>>::success(std::move(observation));
}

} // namespace lx::linux::procfs

