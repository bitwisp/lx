#pragma once

#include <iosfwd>

namespace lx::application {
class DoctorService;
}

namespace lx::cli {

class CliApp final {
public:
    explicit CliApp(const application::DoctorService& doctorService) noexcept;

    int run(int argc, char** argv, std::ostream& output, std::ostream& error) const;

private:
    const application::DoctorService& doctorService_;
};

} // namespace lx::cli
