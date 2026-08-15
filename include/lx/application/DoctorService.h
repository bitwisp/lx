#pragma once

#include "lx/contracts/IProcessProvider.h"
#include "lx/contracts/ISignalProvider.h"
#include "lx/contracts/ISocketProvider.h"
#include "lx/contracts/IServiceProvider.h"
#include "lx/domain/DoctorReport.h"

namespace lx::application {

class DoctorService final {
public:
    DoctorService(const contracts::IProcessProvider& processProvider,
                  const contracts::ISocketProvider& socketProvider,
                  const contracts::ISignalProvider& signalProvider,
                  const contracts::IServiceProvider& serviceProvider) noexcept;
    [[nodiscard]] DoctorReport inspect() const;

private:
    const contracts::IProcessProvider& processProvider_;
    const contracts::ISocketProvider& socketProvider_;
    const contracts::ISignalProvider& signalProvider_;
    const contracts::IServiceProvider& serviceProvider_;
};

} // namespace lx::application
