#include "lx/linux/netlink/InetDiagCodec.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <netinet/in.h>
TEST_CASE("inet diag request encodes family protocol states and sequence") {
 auto data=lx::linux::netlink::buildInetDiagRequest(lx::AddressFamily::IPv6,lx::TransportProtocol::Udp,77,0x1234);
 nlmsghdr h{}; inet_diag_req_v2 r{}; std::memcpy(&h,data.data(),sizeof(h)); std::memcpy(&r,data.data()+NLMSG_HDRLEN,sizeof(r));
 REQUIRE(h.nlmsg_type==SOCK_DIAG_BY_FAMILY); REQUIRE((h.nlmsg_flags&NLM_F_DUMP)!=0); REQUIRE(h.nlmsg_seq==77);
 REQUIRE(r.sdiag_family==AF_INET6); REQUIRE(r.sdiag_protocol==IPPROTO_UDP); REQUIRE(r.idiag_states==0x1234);
}
