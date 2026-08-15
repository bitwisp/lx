#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <variant>

namespace lx {

struct PortTarget { std::uint16_t port = 0; };
struct ProcessTarget { pid_t pid = -1; };
struct ServiceTarget { std::string unit; };

using ResourceTarget = std::variant<PortTarget, ProcessTarget, ServiceTarget>;

} // namespace lx
