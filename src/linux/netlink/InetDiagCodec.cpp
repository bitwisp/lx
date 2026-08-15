#include "lx/linux/netlink/InetDiagCodec.h"
#include <cstring>
#include <arpa/inet.h>
#include <algorithm>
#include <system_error>
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

namespace {
Result<InetDiagBatch> parseError(const std::string& message, int number=0) { return Result<InetDiagBatch>::failure({ErrorCode::ProtocolError,message,number,"inet-diag-codec","parse"}); }
std::string tcpState(unsigned state) { static const char* names[]={"unknown","established","syn-sent","syn-recv","fin-wait-1","fin-wait-2","time-wait","close","close-wait","last-ack","listen","closing","new-syn-recv"}; return state<13?names[state]:"unknown"; }
bool nonzero(const std::uint32_t* words) { return std::any_of(words,words+4,[](auto v){return v!=0;}); }
}
Result<InetDiagBatch> parseInetDiagDatagram(const std::vector<std::byte>& data, std::uint32_t sequence, TransportProtocol protocol) {
    InetDiagBatch batch; std::size_t offset=0;
    while(offset<data.size()) {
        if(data.size()-offset<sizeof(nlmsghdr)) return parseError("Truncated Netlink header");
        nlmsghdr h{}; std::memcpy(&h,data.data()+offset,sizeof(h));
        if(h.nlmsg_len<sizeof(nlmsghdr)||h.nlmsg_len>data.size()-offset) return parseError("Invalid Netlink message length");
        if(h.nlmsg_seq!=sequence) return parseError("Unexpected Netlink sequence");
        const auto* payload=data.data()+offset+NLMSG_HDRLEN; const auto payloadSize=h.nlmsg_len-NLMSG_HDRLEN;
        if(h.nlmsg_type==NLMSG_DONE) { if(h.nlmsg_flags&NLM_F_DUMP_INTR) return parseError("Netlink dump was interrupted"); batch.done=true; }
        else if(h.nlmsg_type==NLMSG_ERROR) {
            if(payloadSize<sizeof(nlmsgerr)) return parseError("Truncated Netlink error");
            nlmsgerr e{}; std::memcpy(&e,payload,sizeof(e));
            if(e.error!=0) return Result<InetDiagBatch>::failure({ErrorCode::ProtocolError,std::system_category().message(-e.error),-e.error,"inet-diag-codec","kernel response"});
        } else if(h.nlmsg_type==NLMSG_OVERRUN) return parseError("Netlink response overrun");
        else {
            if(payloadSize<sizeof(inet_diag_msg)) return parseError("Truncated INET_DIAG message");
            inet_diag_msg m{}; std::memcpy(&m,payload,sizeof(m));
            SocketInfo socket; socket.protocol=protocol; char address[INET6_ADDRSTRLEN]{};
            const void* src=m.id.idiag_src; const void* dst=m.id.idiag_dst;
            if(m.idiag_family==AF_INET) socket.family=AddressFamily::IPv4;
            else if(m.idiag_family==AF_INET6) socket.family=AddressFamily::IPv6;
            else return parseError("Unsupported INET_DIAG address family");
            int af=socket.family==AddressFamily::IPv4?AF_INET:AF_INET6;
            if(::inet_ntop(af,src,address,sizeof(address))==nullptr) return parseError("Unable to format local address");
            socket.local={address,ntohs(m.id.idiag_sport)};
            auto remotePort=ntohs(m.id.idiag_dport); if(remotePort!=0||nonzero(m.id.idiag_dst)) { if(::inet_ntop(af,dst,address,sizeof(address))==nullptr)return parseError("Unable to format remote address"); socket.remote=Endpoint{address,remotePort}; }
            socket.state=protocol==TransportProtocol::Tcp?tcpState(m.idiag_state):(socket.remote?"connected":"unconnected"); socket.uid=m.idiag_uid; socket.inode=m.idiag_inode; batch.sockets.push_back(std::move(socket));
        }
        const auto aligned=NLMSG_ALIGN(h.nlmsg_len); if(aligned>data.size()-offset) { if(h.nlmsg_len!=data.size()-offset)return parseError("Truncated aligned Netlink message"); offset=data.size(); } else offset+=aligned;
    }
    return Result<InetDiagBatch>::success(std::move(batch));
}
} // namespace lx::linux::netlink
