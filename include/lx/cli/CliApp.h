#pragma once

#include <iosfwd>

namespace lx::application {
class DoctorService;
class ProcessService;
class PortService;
}

namespace lx::cli {

class CliApp final {
public:
    CliApp(const application::DoctorService& doctorService,
           const application::ProcessService& processService,
           const application::PortService& portService) noexcept;

    int run(int argc, char** argv, std::ostream& output, std::ostream& error) const;

private:
    const application::DoctorService& doctorService_;
    const application::ProcessService& processService_;
    const application::PortService& portService_;
};

} // namespace lx::cli
