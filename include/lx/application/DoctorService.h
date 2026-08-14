#pragma once

#include "lx/domain/DoctorReport.h"

namespace lx::application {

class DoctorService final {
public:
    [[nodiscard]] DoctorReport inspect() const;
};

} // namespace lx::application

