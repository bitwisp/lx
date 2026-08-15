#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
#include "lx/linux/procfs/ProcFsProcessProvider.h"
#include "lx/linux/netlink/NetlinkSocketProvider.h"

#include <iostream>

int main(const int argc, char** argv)
{
    const lx::linux::procfs::ProcFsProcessProvider processProvider;
    const lx::application::ProcessService processService{processProvider};
    const lx::linux::netlink::NetlinkSocketProvider socketProvider;
    const lx::application::PortService portService{socketProvider};
    const lx::application::DoctorService doctorService{processProvider};
    return lx::cli::CliApp{doctorService, processService, portService}.run(
        argc, argv, std::cout, std::cerr);
}
