#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
#include "lx/application/ServiceService.h"
#include "lx/application/LogService.h"
#include "lx/application/InspectService.h"
#include "lx/application/ResourceResolver.h"
#if LX_HAS_SYSTEMD
#include "lx/linux/systemd/SystemdJournalProvider.h"
#include "lx/linux/systemd/SystemdServiceProvider.h"
#else
#include "lx/linux/systemd/UnavailableJournalProvider.h"
#include "lx/linux/systemd/UnavailableServiceProvider.h"
#endif
#include "lx/linux/procfs/ProcFsProcessProvider.h"
#include "lx/linux/procfs/SocketInodeResolver.h"
#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include "lx/linux/process/LinuxSignalProvider.h"

#include <iostream>
#include <unistd.h>

int main(const int argc, char** argv)
{
    const lx::linux::procfs::ProcFsProcessProvider processProvider;
    const lx::linux::process::LinuxSignalProvider signalProvider;
#if LX_HAS_SYSTEMD
    const lx::linux::SystemdServiceProvider serviceProvider;
    const lx::linux::SystemdJournalProvider journalProvider;
#else
    const lx::linux::UnavailableServiceProvider serviceProvider{
        "LX was built without libsystemd support"};
    const lx::linux::UnavailableJournalProvider journalProvider{
        "LX was built without libsystemd support"};
#endif
    const lx::application::ServiceService serviceService{serviceProvider};
    const lx::application::LogService logService{journalProvider};
    const lx::application::ProcessService processService{
        processProvider, signalProvider, ::getpid(), &serviceService};
    const lx::linux::netlink::NetlinkSocketProvider socketProvider;
    const lx::linux::procfs::SocketInodeResolver socketOwnerResolver;
    const lx::application::PortService portService{
        socketProvider, socketOwnerResolver, processService, &serviceService};
    const lx::application::DoctorService doctorService{
        processProvider, socketProvider, signalProvider, serviceProvider,
        journalProvider};
    const lx::application::ResourceResolver resourceResolver{
        portService, processService, serviceService};
    const lx::application::InspectService inspectService{
        resourceResolver, portService, processService, serviceService,
        logService};
    return lx::cli::CliApp{doctorService, processService, portService,
                           serviceService, logService, inspectService}.run(
        argc, argv, std::cin, std::cout, std::cerr);
}
