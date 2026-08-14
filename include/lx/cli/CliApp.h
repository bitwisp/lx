#pragma once

#include <iosfwd>

namespace lx::application {
class DoctorService;
class ProcessService;
}

namespace lx::cli {

class CliApp final {
public:
    CliApp(const application::DoctorService& doctorService,
           const application::ProcessService& processService) noexcept;

    int run(int argc, char** argv, std::ostream& output, std::ostream& error) const;

private:
    const application::DoctorService& doctorService_;
    const application::ProcessService& processService_;
};

} // namespace lx::cli
