#pragma once

#include <iosfwd>

namespace lx::application {
class DoctorService;
class ProcessService;
class PortService;
class ServiceService;
class LogService;
}

namespace lx::cli {

class CliApp final {
public:
    CliApp(const application::DoctorService& doctorService,
           const application::ProcessService& processService,
           const application::PortService& portService,
           const application::ServiceService& serviceService,
           const application::LogService& logService) noexcept;

    int run(int argc, char** argv, std::istream& input,
            std::ostream& output, std::ostream& error) const;

private:
    const application::DoctorService& doctorService_;
    const application::ProcessService& processService_;
    const application::PortService& portService_;
    const application::ServiceService& serviceService_;
    const application::LogService& logService_;
};

} // namespace lx::cli
