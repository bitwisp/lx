#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
#include "lx/application/ServiceService.h"
#if LX_HAS_SYSTEMD
#include "lx/linux/systemd/SystemdServiceProvider.h"
#else
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
    const lx::application::ProcessService processService{
        processProvider, signalProvider, ::getpid()};
    const lx::linux::netlink::NetlinkSocketProvider socketProvider;
    const lx::linux::procfs::SocketInodeResolver socketOwnerResolver;
    const lx::application::PortService portService{
        socketProvider, socketOwnerResolver, processService};
#if LX_HAS_SYSTEMD
    const lx::linux::SystemdServiceProvider serviceProvider;
#else
    const lx::linux::UnavailableServiceProvider serviceProvider{
        "LX was built without libsystemd support"};
#endif
    const lx::application::ServiceService serviceService{serviceProvider};
    const lx::application::DoctorService doctorService{
        processProvider, socketProvider, signalProvider};
    return lx::cli::CliApp{doctorService, processService, portService,
                           serviceService}.run(
        argc, argv, std::cin, std::cout, std::cerr);
}
