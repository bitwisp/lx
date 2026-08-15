#pragma once
#include "lx/domain/Result.h"
#include <cstddef>
#include <vector>
namespace lx::linux::netlink {
class NetlinkSocket final {
public:
    static Result<NetlinkSocket> open();
    ~NetlinkSocket();
    NetlinkSocket(NetlinkSocket&& other) noexcept;
    NetlinkSocket& operator=(NetlinkSocket&& other) noexcept;
    NetlinkSocket(const NetlinkSocket&) = delete;
    NetlinkSocket& operator=(const NetlinkSocket&) = delete;
    Result<void> send(const std::vector<std::byte>& request) const;
    Result<std::vector<std::byte>> receive() const;
private:
    explicit NetlinkSocket(int fd) noexcept : fd_(fd) {}
    int fd_ = -1;
};
} // namespace lx::linux::netlink
