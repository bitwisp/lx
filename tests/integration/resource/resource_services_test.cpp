#include "lx/application/FindService.h"
#include "lx/application/InspectService.h"
#include "lx/application/LogService.h"
#include "lx/application/PortService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/ResourceResolver.h"
#include "lx/application/ServiceService.h"
#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include "lx/linux/process/LinuxSignalProvider.h"
#include "lx/linux/procfs/ProcFsProcessProvider.h"
#include "lx/linux/procfs/SocketInodeResolver.h"
#if LX_HAS_SYSTEMD
#include "lx/linux/systemd/SystemdJournalProvider.h"
#include "lx/linux/systemd/SystemdServiceProvider.h"
#else
#include "lx/linux/systemd/UnavailableJournalProvider.h"
#include "lx/linux/systemd/UnavailableServiceProvider.h"
#endif

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <unistd.h>

TEST_CASE("real resource services inspect and find the running test process")
{
    const lx::linux::procfs::ProcFsProcessProvider processProvider;
    const lx::linux::process::LinuxSignalProvider signalProvider;
    const lx::linux::netlink::NetlinkSocketProvider socketProvider;
    const lx::linux::procfs::SocketInodeResolver ownerResolver;
#if LX_HAS_SYSTEMD
    const lx::linux::SystemdServiceProvider serviceProvider;
    const lx::linux::SystemdJournalProvider journalProvider;
#else
    const lx::linux::UnavailableServiceProvider serviceProvider{"unavailable"};
    const lx::linux::UnavailableJournalProvider journalProvider{"unavailable"};
#endif
    const lx::application::ServiceService services{serviceProvider};
    const lx::application::ProcessService processes{
        processProvider, signalProvider, ::getpid(), &services};
    const lx::application::PortService ports{
        socketProvider, ownerResolver, processes, &services};
    const lx::application::LogService logs{journalProvider};
    const lx::application::ResourceResolver resolver{ports, processes, services};
    const lx::application::InspectService inspect{
        resolver, ports, processes, services, logs};
    const lx::application::FindService find{ports, processes, services};

    const auto graph = inspect.inspect("pid:" + std::to_string(::getpid()));
    REQUIRE(graph);
    REQUIRE(graph.value().processes.size() == 1);
    CHECK(graph.value().processes.front().pid == ::getpid());

    const auto found = find.find(graph.value().processes.front().name);
    REQUIRE(found);
    CHECK(std::any_of(
        found.value().processes.begin(), found.value().processes.end(),
        [](const lx::ProcessInfo& process) { return process.pid == ::getpid(); }));
}
