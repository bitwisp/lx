#include "lx/linux/netlink/NetlinkSocket.h"
#include <cerrno>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>
namespace lx::linux::netlink { namespace {
Error error(const char* op) { const auto code=(errno==EACCES||errno==EPERM)?ErrorCode::PermissionDenied:ErrorCode::IoError; return {code, std::string("Unable to ")+op+": "+std::system_category().message(errno), errno, "netlink-socket", op}; }
} 
Result<NetlinkSocket> NetlinkSocket::open() {
    int fd = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_SOCK_DIAG);
    if (fd < 0) return Result<NetlinkSocket>::failure(error("open sock-diag socket"));
    return Result<NetlinkSocket>::success(NetlinkSocket(fd));
}
NetlinkSocket::~NetlinkSocket(){ if(fd_>=0) ::close(fd_); }
NetlinkSocket::NetlinkSocket(NetlinkSocket&& o) noexcept : fd_(std::exchange(o.fd_,-1)) {}
NetlinkSocket& NetlinkSocket::operator=(NetlinkSocket&& o) noexcept { if(this!=&o){ if(fd_>=0)::close(fd_); fd_=std::exchange(o.fd_,-1);} return *this; }
Result<void> NetlinkSocket::send(const std::vector<std::byte>& request) const {
    sockaddr_nl address{}; address.nl_family=AF_NETLINK;
    ssize_t sent; do { sent=::sendto(fd_,request.data(),request.size(),0,reinterpret_cast<sockaddr*>(&address),sizeof(address)); } while(sent<0&&errno==EINTR);
    if(sent<0 || static_cast<std::size_t>(sent)!=request.size()) return Result<void>::failure(error("send sock-diag request"));
    return Result<void>::success();
}
Result<std::vector<std::byte>> NetlinkSocket::receive() const {
    std::vector<std::byte> data(65536); ssize_t count;
    do { count=::recv(fd_,data.data(),data.size(),0); } while(count<0&&errno==EINTR);
    if(count<0) return Result<std::vector<std::byte>>::failure(error("receive sock-diag response"));
    data.resize(static_cast<std::size_t>(count)); return Result<std::vector<std::byte>>::success(std::move(data));
}
} // namespace lx::linux::netlink
