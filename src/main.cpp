#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
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
    const lx::application::DoctorService doctorService{processProvider, socketProvider};
    return lx::cli::CliApp{doctorService, processService, portService}.run(
        argc, argv, std::cin, std::cout, std::cerr);
}
