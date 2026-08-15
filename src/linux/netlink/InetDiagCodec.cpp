#include "lx/linux/netlink/InetDiagCodec.h"
#include <cstring>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <netinet/in.h>
namespace lx::linux::netlink {
std::vector<std::byte> buildInetDiagRequest(AddressFamily family, TransportProtocol protocol, std::uint32_t sequence, std::uint32_t states) {
    struct Request { nlmsghdr header; inet_diag_req_v2 request; } value{};
    value.header.nlmsg_len=sizeof(value); value.header.nlmsg_type=SOCK_DIAG_BY_FAMILY;
    value.header.nlmsg_flags=NLM_F_REQUEST|NLM_F_DUMP; value.header.nlmsg_seq=sequence;
    value.request.sdiag_family=family==AddressFamily::IPv4?AF_INET:AF_INET6;
    value.request.sdiag_protocol=protocol==TransportProtocol::Tcp?IPPROTO_TCP:IPPROTO_UDP;
    value.request.idiag_states=states;
    value.request.id.idiag_cookie[0]=INET_DIAG_NOCOOKIE; value.request.id.idiag_cookie[1]=INET_DIAG_NOCOOKIE;
    std::vector<std::byte> bytes(sizeof(value)); std::memcpy(bytes.data(),&value,sizeof(value)); return bytes;
}
} // namespace lx::linux::netlink
