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

TEST_CASE("inet diag parser decodes IPv6 UDP socket") {
 nlmsghdr h{}; inet_diag_msg m{}; h.nlmsg_len=NLMSG_LENGTH(sizeof(m));h.nlmsg_type=SOCK_DIAG_BY_FAMILY;h.nlmsg_seq=5;m.idiag_family=AF_INET6;m.id.idiag_sport=htons(53);::inet_pton(AF_INET6,"::1",m.id.idiag_src);
 std::vector<std::byte>d(h.nlmsg_len);std::memcpy(d.data(),&h,sizeof(h));std::memcpy(d.data()+NLMSG_HDRLEN,&m,sizeof(m));
 auto r=lx::linux::netlink::parseInetDiagDatagram(d,5,lx::TransportProtocol::Udp); REQUIRE(r); REQUIRE(r.value().sockets[0].local.address=="::1"); REQUIRE(r.value().sockets[0].state=="unconnected");
}

TEST_CASE("inet diag parser handles DONE and validates sequence") {
 nlmsghdr h{};h.nlmsg_len=NLMSG_HDRLEN;h.nlmsg_type=NLMSG_DONE;h.nlmsg_seq=12;std::vector<std::byte>d(h.nlmsg_len);std::memcpy(d.data(),&h,sizeof(h));
 auto done=lx::linux::netlink::parseInetDiagDatagram(d,12,lx::TransportProtocol::Tcp);REQUIRE(done);REQUIRE(done.value().done);
 REQUIRE_FALSE(lx::linux::netlink::parseInetDiagDatagram(d,13,lx::TransportProtocol::Tcp));
 h.nlmsg_flags=NLM_F_DUMP_INTR;std::memcpy(d.data(),&h,sizeof(h));REQUIRE_FALSE(lx::linux::netlink::parseInetDiagDatagram(d,12,lx::TransportProtocol::Tcp));
}

TEST_CASE("inet diag parser handles kernel errors and invalid lengths") {
 nlmsghdr h{};nlmsgerr e{};h.nlmsg_len=NLMSG_LENGTH(sizeof(e));h.nlmsg_type=NLMSG_ERROR;h.nlmsg_seq=2;e.error=-EPERM;std::vector<std::byte>d(h.nlmsg_len);std::memcpy(d.data(),&h,sizeof(h));std::memcpy(d.data()+NLMSG_HDRLEN,&e,sizeof(e));
 REQUIRE_FALSE(lx::linux::netlink::parseInetDiagDatagram(d,2,lx::TransportProtocol::Tcp));
 h.nlmsg_len=UINT32_MAX;std::memcpy(d.data(),&h,sizeof(h));REQUIRE_FALSE(lx::linux::netlink::parseInetDiagDatagram(d,2,lx::TransportProtocol::Tcp));
}
