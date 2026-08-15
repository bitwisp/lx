#pragma once

#include "lx/contracts/IProcessProvider.h"
#include "lx/contracts/ISocketProvider.h"
#include "lx/domain/DoctorReport.h"

namespace lx::application {

class DoctorService final {
public:
    DoctorService(const contracts::IProcessProvider& processProvider, const contracts::ISocketProvider& socketProvider) noexcept;
    [[nodiscard]] DoctorReport inspect() const;

private:
    const contracts::IProcessProvider& processProvider_;
    const contracts::ISocketProvider& socketProvider_;
};

} // namespace lx::application
