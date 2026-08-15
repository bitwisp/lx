#include "lx/linux/procfs/SystemMetricsParser.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace lx::linux::procfs {
namespace {

template <typename T>
Result<T> failure(const std::string& message)
{
    return Result<T>::failure(
        {ErrorCode::ParseError, message, 0, "procfs-metrics", "parse"});
}

bool parseUnsigned(const std::string_view text, std::uint64_t& value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool checkedAdd(std::uint64_t& sum, const std::uint64_t value)
{
    if (value > std::numeric_limits<std::uint64_t>::max() - sum) {
        return false;
    }
    sum += value;
    return true;
}

} // namespace

Result<CpuTimes> parseCpuTimes(const std::string_view contents)
{
    const auto end = contents.find('\n');
    std::istringstream line{std::string(contents.substr(0, end))};
    std::string label;
    line >> label;
    if (label != "cpu") {
        return failure<CpuTimes>("Missing aggregate CPU row in /proc/stat");
    }

    CpuTimes result;
    std::string token;
    std::size_t index = 0;
    while (line >> token) {
        std::uint64_t value = 0;
        if (!parseUnsigned(token, value) || !checkedAdd(result.totalTicks, value)) {
            return failure<CpuTimes>("Invalid CPU counter in /proc/stat");
        }
        if ((index == 3 || index == 4) && !checkedAdd(result.idleTicks, value)) {
            return failure<CpuTimes>("CPU idle counter overflow in /proc/stat");
        }
        ++index;
    }
    if (index < 4) {
        return failure<CpuTimes>("Incomplete aggregate CPU row in /proc/stat");
    }
    return Result<CpuTimes>::success(result);
}

Result<MemoryRecord> parseMemoryInfo(const std::string_view contents)
{
    MemoryRecord result;
    bool hasTotal = false;
    bool hasAvailable = false;
    std::istringstream input{std::string(contents)};
    std::string key;
    std::string valueText;
    std::string unit;
    while (input >> key >> valueText >> unit) {
        if (key != "MemTotal:" && key != "MemAvailable:") {
            std::string ignored;
            std::getline(input, ignored);
            continue;
        }
        std::uint64_t kibibytes = 0;
        if (unit != "kB" || !parseUnsigned(valueText, kibibytes) ||
            kibibytes > std::numeric_limits<std::uint64_t>::max() / 1024U) {
            return failure<MemoryRecord>("Invalid memory value in /proc/meminfo");
        }
        const auto bytes = kibibytes * 1024U;
        if (key == "MemTotal:") {
            result.totalBytes = bytes;
            hasTotal = true;
        } else {
            result.availableBytes = bytes;
            hasAvailable = true;
        }
    }
    if (!hasTotal || !hasAvailable || result.availableBytes > result.totalBytes) {
        return failure<MemoryRecord>("Missing or inconsistent memory fields");
    }
    return Result<MemoryRecord>::success(result);
}

Result<std::chrono::milliseconds> parseUptime(const std::string_view contents)
{
    double seconds = 0.0;
    const auto firstEnd = contents.find_first_of(" \t\r\n");
    const auto token = contents.substr(0, firstEnd);
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), seconds);
    if (token.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != token.data() + token.size() || !std::isfinite(seconds) ||
        seconds < 0.0 || seconds > static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1000.0) {
        return failure<std::chrono::milliseconds>("Invalid uptime in /proc/uptime");
    }
    return Result<std::chrono::milliseconds>::success(
        std::chrono::milliseconds{static_cast<std::int64_t>(seconds * 1000.0)});
}

std::uint32_t countLogicalCpus(const std::string_view contents)
{
    std::uint32_t count = 0;
    std::size_t begin = 0;
    while (begin < contents.size()) {
        const auto end = contents.find('\n', begin);
        auto line = contents.substr(begin, end == std::string_view::npos
                                               ? contents.size() - begin
                                               : end - begin);
        if (line.size() > 3 && line.substr(0, 3) == "cpu" &&
            line[3] >= '0' && line[3] <= '9') {
            ++count;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return count == 0 ? 1 : count;
}

} // namespace lx::linux::procfs
