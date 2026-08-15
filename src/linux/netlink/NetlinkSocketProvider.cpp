#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include "lx/linux/netlink/InetDiagCodec.h"
#include "lx/linux/netlink/NetlinkSocket.h"
#include <algorithm>
#include <limits>
namespace lx::linux::netlink { namespace {
Warning warning(const Error&e){return {e.code,e.message,e.systemError,e.component,e.operation};}
Result<std::vector<SocketInfo>> one(NetlinkSocket& socket, AddressFamily family, TransportProtocol protocol, std::uint32_t sequence) {
 auto request=buildInetDiagRequest(family,protocol,sequence,protocol==TransportProtocol::Tcp?(1u<<10):std::numeric_limits<std::uint32_t>::max());
 auto sent=socket.send(request);if(!sent)return Result<std::vector<SocketInfo>>::failure(sent.error());std::vector<SocketInfo> values;
 for(;;){auto data=socket.receive();if(!data)return Result<std::vector<SocketInfo>>::failure(data.error());auto batch=parseInetDiagDatagram(data.value(),sequence,protocol);if(!batch)return Result<std::vector<SocketInfo>>::failure(batch.error());values.insert(values.end(),std::make_move_iterator(batch.value().sockets.begin()),std::make_move_iterator(batch.value().sockets.end()));if(batch.value().done)break;}
 return Result<std::vector<SocketInfo>>::success(std::move(values));
}
}
Result<Observation<std::vector<SocketInfo>>> NetlinkSocketProvider::query(const SocketQuery& query) const {
 auto opened=NetlinkSocket::open();if(!opened)return Result<Observation<std::vector<SocketInfo>>>::failure(opened.error());auto socket=std::move(opened).value();Observation<std::vector<SocketInfo>> observed{{},{}};std::optional<Error> last;std::uint32_t seq=1;std::size_t attempts=0;std::size_t failures=0;
 for(auto protocol:{TransportProtocol::Tcp,TransportProtocol::Udp}){if(query.protocol&&*query.protocol!=protocol)continue;for(auto family:{AddressFamily::IPv4,AddressFamily::IPv6}){++attempts;auto result=one(socket,family,protocol,seq++);if(!result){++failures;last=result.error();observed.warnings.push_back(warning(result.error()));continue;}for(auto& item:result.value()){if(protocol==TransportProtocol::Udp&&query.listeningOnly&&item.remote)continue;if(query.localPort&&item.local.port!=*query.localPort)continue;observed.value.push_back(std::move(item));}}}
 if(last&&failures==attempts)return Result<Observation<std::vector<SocketInfo>>>::failure(std::move(*last));
 std::sort(observed.value.begin(),observed.value.end(),[](const auto&a,const auto&b){return std::tie(a.local.port,a.protocol,a.family,a.local.address)<std::tie(b.local.port,b.protocol,b.family,b.local.address);});return Result<Observation<std::vector<SocketInfo>>>::success(std::move(observed));
}
} // namespace lx::linux::netlink
