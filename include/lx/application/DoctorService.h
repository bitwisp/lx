#pragma once

#include "lx/contracts/IProcessProvider.h"
#include "lx/domain/DoctorReport.h"

namespace lx::application {

class DoctorService final {
public:
    explicit DoctorService(const contracts::IProcessProvider& processProvider) noexcept;
    [[nodiscard]] DoctorReport inspect() const;

private:
    const contracts::IProcessProvider& processProvider_;
};

} // namespace lx::application
