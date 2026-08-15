#pragma once

#include "lx/domain/Error.h"
#include "lx/domain/Warning.h"
#include "lx/domain/Observation.h"
#include "lx/domain/ProcessInfo.h"
#include "lx/domain/PortInfo.h"
#include "lx/domain/ServiceInfo.h"
#include "lx/domain/JournalEntry.h"
#include "lx/domain/DoctorReport.h"
#include "lx/domain/FindResult.h"
#include "lx/domain/ResourceGraph.h"

#include <string>
#include <vector>

namespace lx::cli {

class JsonSerializer final {
public:
    static constexpr int schemaVersion = 1;

    [[nodiscard]] static std::string error(
        const std::string& command, const std::string& operation,
        const Error& value);

    [[nodiscard]] static std::string emptySuccess(
        const std::string& command, const std::string& operation,
        const std::vector<Warning>& warnings = {});
    [[nodiscard]] static std::string process(
        const Observation<ProcessInfo>& value, bool rawCommand = false);
    [[nodiscard]] static std::string processes(
        const Observation<std::vector<ProcessInfo>>& value);
    [[nodiscard]] static std::string ports(
        const Observation<std::vector<PortInfo>>& value,
        const std::string& operation = "list");
    [[nodiscard]] static std::string service(
        const Observation<ServiceInfo>& value);
    [[nodiscard]] static std::string services(
        const Observation<std::vector<ServiceInfo>>& value);
    [[nodiscard]] static std::string logs(
        const Observation<std::vector<JournalEntry>>& value);
    [[nodiscard]] static std::string logEvent(
        const Observation<JournalEntry>& value);
    [[nodiscard]] static std::string doctor(const DoctorReport& value);
    [[nodiscard]] static std::string inspect(const ResourceGraph& value);
    [[nodiscard]] static std::string find(const FindResult& value);
};

} // namespace lx::cli
