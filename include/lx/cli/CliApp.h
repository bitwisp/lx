#pragma once

#include <iosfwd>

namespace lx::application {
class DoctorService;
class ProcessService;
class PortService;
class ServiceService;
class LogService;
class InspectService;
class FindService;
class StatusService;
}

namespace lx::cli {

class ITuiRunner;

class CliApp final {
public:
    CliApp(const application::DoctorService& doctorService,
           const application::ProcessService& processService,
           const application::PortService& portService,
           const application::ServiceService& serviceService,
           const application::LogService& logService,
           const application::InspectService& inspectService,
           const application::FindService& findService,
           const application::StatusService* statusService = nullptr,
           ITuiRunner* tuiRunner = nullptr) noexcept;

    int run(int argc, char** argv, std::istream& input,
            std::ostream& output, std::ostream& error) const;

private:
    const application::DoctorService& doctorService_;
    const application::ProcessService& processService_;
    const application::PortService& portService_;
    const application::ServiceService& serviceService_;
    const application::LogService& logService_;
    const application::InspectService& inspectService_;
    const application::FindService& findService_;
    const application::StatusService* statusService_;
    ITuiRunner* tuiRunner_;
};

} // namespace lx::cli
