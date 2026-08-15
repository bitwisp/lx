#include "lx/application/PortService.h"
#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include "lx/linux/procfs/ProcFsProcessProvider.h"
#include "lx/linux/procfs/SocketInodeResolver.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

class SocketHandle final {
public:
    explicit SocketHandle(const int fd) noexcept : fd_(fd) {}
    ~SocketHandle() { if (fd_ >= 0) ::close(fd_); }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    [[nodiscard]] int get() const noexcept { return fd_; }

private:
    int fd_;
};

} // namespace

TEST_CASE("port inspection maps a live TCP listener to the test process")
{
    const SocketHandle listener{
        ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)};
    if (listener.get() < 0) {
        SKIP("AF_INET sockets unavailable in this environment");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(::bind(listener.get(), reinterpret_cast<sockaddr*>(&address),
                   sizeof(address)) == 0);
    REQUIRE(::listen(listener.get(), 1) == 0);
    socklen_t length = sizeof(address);
    REQUIRE(::getsockname(listener.get(),
                          reinterpret_cast<sockaddr*>(&address),
                          &length) == 0);

    const lx::linux::netlink::NetlinkSocketProvider sockets;
    const lx::linux::procfs::SocketInodeResolver owners;
    const lx::linux::procfs::ProcFsProcessProvider processes;
    const lx::application::PortService service{sockets, owners, processes};
    lx::SocketQuery query;
    query.localPort = ntohs(address.sin_port);
    query.protocol = lx::TransportProtocol::Tcp;

    const auto result = service.inspect(query);
    if (!result && result.error().code == lx::ErrorCode::PermissionDenied) {
        SKIP("INET_DIAG unavailable in this environment");
    }
    REQUIRE(result);

    const auto processId = ::getpid();
    const auto port = std::find_if(
        result.value().value.begin(), result.value().value.end(),
        [processId](const lx::PortInfo& value) {
            return std::find(value.socket.ownerPids.begin(),
                             value.socket.ownerPids.end(),
                             processId) != value.socket.ownerPids.end();
        });
    REQUIRE(port != result.value().value.end());
    REQUIRE(port->socket.inode != 0);
    REQUIRE(std::any_of(
        port->owners.begin(), port->owners.end(),
        [processId](const lx::ProcessInfo& process) {
            return process.pid == processId && !process.name.empty();
        }));
}
