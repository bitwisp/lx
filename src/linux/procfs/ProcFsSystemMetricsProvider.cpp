#include "lx/linux/procfs/ProcFsSystemMetricsProvider.h"

#include "lx/linux/procfs/SystemMetricsParser.h"

#include <cerrno>
#include <fstream>
#include <iterator>
#include <limits.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace lx::linux::procfs {
namespace {

Result<std::string> readFile(const std::filesystem::path& path)
{
    errno = 0;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        const auto number = errno == 0 ? EIO : errno;
        return Result<std::string>::failure(
            {ErrorCode::IoError, "Unable to read " + path.string() + ": " +
                 std::system_category().message(number), number,
             "procfs-metrics", "sample"});
    }
    std::string contents{std::istreambuf_iterator<char>{input},
                         std::istreambuf_iterator<char>{}};
    return Result<std::string>::success(std::move(contents));
}

} // namespace

ProcFsSystemMetricsProvider::ProcFsSystemMetricsProvider(std::filesystem::path root)
    : root_(std::move(root))
{
}

Result<SystemMetricsSample> ProcFsSystemMetricsProvider::sample() const
{
    auto stat = readFile(root_ / "stat");
    auto memory = readFile(root_ / "meminfo");
    auto uptime = readFile(root_ / "uptime");
    if (!stat) return Result<SystemMetricsSample>::failure(stat.error());
    if (!memory) return Result<SystemMetricsSample>::failure(memory.error());
    if (!uptime) return Result<SystemMetricsSample>::failure(uptime.error());

    auto cpu = parseCpuTimes(stat.value());
    auto mem = parseMemoryInfo(memory.value());
    auto up = parseUptime(uptime.value());
    if (!cpu) return Result<SystemMetricsSample>::failure(cpu.error());
    if (!mem) return Result<SystemMetricsSample>::failure(mem.error());
    if (!up) return Result<SystemMetricsSample>::failure(up.error());

    char hostname[HOST_NAME_MAX + 1]{};
    if (::gethostname(hostname, sizeof(hostname)) != 0) {
        const auto number = errno;
        return Result<SystemMetricsSample>::failure(
            {ErrorCode::IoError, "Unable to read hostname: " +
                 std::system_category().message(number), number,
             "procfs-metrics", "sample"});
    }
    hostname[HOST_NAME_MAX] = '\0';

    SystemMetricsSample sample;
    sample.cpu = cpu.value();
    sample.memoryTotalBytes = mem.value().totalBytes;
    sample.memoryAvailableBytes = mem.value().availableBytes;
    sample.uptime = up.value();
    sample.logicalCpuCount = countLogicalCpus(stat.value());
    sample.hostname = hostname;
    return Result<SystemMetricsSample>::success(std::move(sample));
}

} // namespace lx::linux::procfs
