#include "lx/linux/netlink/InetDiagCodec.h"
#include <catch2/catch_test_macros.hpp>
#include <arpa/inet.h>
#include <cstring>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
namespace { std::vector<std::byte> ipv4() { nlmsghdr h{}; inet_diag_msg m{}; h.nlmsg_len=NLMSG_LENGTH(sizeof(m)); h.nlmsg_type=SOCK_DIAG_BY_FAMILY; h.nlmsg_seq=9; m.idiag_family=AF_INET; m.idiag_state=10; m.id.idiag_sport=htons(8080); ::inet_pton(AF_INET,"127.0.0.1",m.id.idiag_src); m.idiag_uid=1000;m.idiag_inode=42; std::vector<std::byte>d(h.nlmsg_len);std::memcpy(d.data(),&h,sizeof(h));std::memcpy(d.data()+NLMSG_HDRLEN,&m,sizeof(m));return d;} }
TEST_CASE("inet diag parser decodes IPv4 socket") { auto r=lx::linux::netlink::parseInetDiagDatagram(ipv4(),9,lx::TransportProtocol::Tcp); REQUIRE(r); REQUIRE(r.value().sockets.size()==1); auto&s=r.value().sockets[0]; REQUIRE(s.local.address=="127.0.0.1"); REQUIRE(s.local.port==8080); REQUIRE(s.state=="listen"); REQUIRE(s.inode==42); }
TEST_CASE("inet diag parser rejects truncated messages") { REQUIRE_FALSE(lx::linux::netlink::parseInetDiagDatagram(std::vector<std::byte>(3),9,lx::TransportProtocol::Tcp)); }
