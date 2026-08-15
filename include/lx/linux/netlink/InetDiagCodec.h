#pragma once
#include "lx/domain/Result.h"
#include "lx/domain/SocketInfo.h"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace lx::linux::netlink {
std::vector<std::byte> buildInetDiagRequest(AddressFamily family, TransportProtocol protocol, std::uint32_t sequence, std::uint32_t states);
struct InetDiagBatch { std::vector<SocketInfo> sockets; bool done=false; };
Result<InetDiagBatch> parseInetDiagDatagram(const std::vector<std::byte>& data, std::uint32_t sequence, TransportProtocol protocol);
} // namespace lx::linux::netlink
