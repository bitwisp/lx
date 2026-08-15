#include "lx/linux/systemd/JournalFieldParser.h"

#include <charconv>
#include <limits>
#include <utility>

namespace lx::linux {
namespace {

Warning parseWarning(const std::string& message, const std::string& operation)
{
    return {ErrorCode::ParseError, message, 0, "sd-journal", operation};
}

} // namespace

Result<std::string> journalFieldValue(
    const std::string_view field, const void* data, const std::size_t length)
{
    if (data == nullptr) {
        return Result<std::string>::failure(
            {ErrorCode::ProtocolError, "Journal field has no data", 0,
             "sd-journal", "read field"});
    }
    const auto* bytes = static_cast<const char*>(data);
    if (length <= field.size() ||
        std::string_view{bytes, field.size()} != field ||
        bytes[field.size()] != '=') {
        return Result<std::string>::failure(
            {ErrorCode::ProtocolError,
             "Journal field does not match the requested name", 0,
             "sd-journal", "read field"});
    }
    const auto valueOffset = field.size() + 1;
    return Result<std::string>::success(
        std::string{bytes + valueOffset, length - valueOffset});
}

Observation<JournalEntry> journalEntryFromFields(JournalFields fields)
{
    Observation<JournalEntry> observed;
    observed.value.timestamp = std::chrono::system_clock::time_point{
        std::chrono::microseconds{fields.timestampUsec}};
    observed.value.cursor = std::move(fields.cursor);
    observed.value.systemdUnit = std::move(fields.systemdUnit);
    observed.value.command = std::move(fields.command);
    observed.value.message = fields.message ? std::move(*fields.message) : "";

    if (fields.pid) {
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(
            fields.pid->data(), fields.pid->data() + fields.pid->size(), value);
        if (parsed.ec == std::errc{} &&
            parsed.ptr == fields.pid->data() + fields.pid->size() && value > 0 &&
            value <= static_cast<std::uint64_t>(
                         std::numeric_limits<pid_t>::max())) {
            observed.value.pid = static_cast<pid_t>(value);
        } else {
            observed.warnings.push_back(
                parseWarning("Journal entry contains an invalid _PID", "parse PID"));
        }
    }

    if (fields.priority) {
        unsigned int value = 0;
        const auto parsed = std::from_chars(
            fields.priority->data(),
            fields.priority->data() + fields.priority->size(), value);
        if (parsed.ec == std::errc{} &&
            parsed.ptr == fields.priority->data() + fields.priority->size() &&
            value <= 7) {
            observed.value.priority = static_cast<std::uint8_t>(value);
        } else {
            observed.warnings.push_back(parseWarning(
                "Journal entry contains an invalid PRIORITY", "parse priority"));
        }
    }
    return observed;
}

} // namespace lx::linux
