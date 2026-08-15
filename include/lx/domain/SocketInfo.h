#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>
namespace lx {
enum class TransportProtocol { Tcp, Udp };
enum class AddressFamily { IPv4, IPv6 };
struct Endpoint { std::string address; std::uint16_t port = 0; };
struct SocketInfo {
    TransportProtocol protocol = TransportProtocol::Tcp;
    AddressFamily family = AddressFamily::IPv4;
    Endpoint local;
    std::optional<Endpoint> remote;
    std::string state;
    std::uint32_t uid = 0;
    std::uint64_t inode = 0;
    std::vector<pid_t> ownerPids;
};
struct SocketQuery {
    std::optional<std::uint16_t> localPort;
    std::optional<TransportProtocol> protocol;
    bool listeningOnly = true;
};
} // namespace lx
