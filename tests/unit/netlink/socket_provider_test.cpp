#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include <catch2/catch_test_macros.hpp>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
TEST_CASE("socket provider finds a self-contained TCP listener") {
 int fd=::socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0); if(fd<0) SKIP("AF_INET sockets unavailable in this environment");
 sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);REQUIRE(::bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0);REQUIRE(::listen(fd,1)==0);socklen_t n=sizeof(a);REQUIRE(::getsockname(fd,reinterpret_cast<sockaddr*>(&a),&n)==0);lx::SocketQuery q;q.localPort=ntohs(a.sin_port);q.protocol=lx::TransportProtocol::Tcp;auto r=lx::linux::netlink::NetlinkSocketProvider{}.query(q);::close(fd);REQUIRE(r);REQUIRE_FALSE(r.value().value.empty());REQUIRE(r.value().value[0].state=="listen");
}

TEST_CASE("socket provider finds a self-contained UDP binding") {
 int fd=::socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0); if(fd<0) SKIP("AF_INET sockets unavailable in this environment");
 sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);REQUIRE(::bind(fd,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0);socklen_t n=sizeof(a);REQUIRE(::getsockname(fd,reinterpret_cast<sockaddr*>(&a),&n)==0);lx::SocketQuery q;q.localPort=ntohs(a.sin_port);q.protocol=lx::TransportProtocol::Udp;auto r=lx::linux::netlink::NetlinkSocketProvider{}.query(q);::close(fd);REQUIRE(r);REQUIRE_FALSE(r.value().value.empty());REQUIRE(r.value().value[0].state=="unconnected");
}
