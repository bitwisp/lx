#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"
#include "lx/application/ProcessService.h"
#include "lx/application/PortService.h"
#include "lx/application/ServiceService.h"
#include "lx/application/LogService.h"
#include "lx/application/InspectService.h"
#include "lx/application/ResourceResolver.h"
#include "lx/application/FindService.h"
#include "lx/application/MetricsService.h"
#include "lx/application/StatusService.h"
#include "lx/application/DashboardService.h"
#if LX_HAS_SYSTEMD
#include "lx/linux/systemd/SystemdJournalProvider.h"
#include "lx/linux/systemd/SystemdServiceProvider.h"
#else
#include "lx/linux/systemd/UnavailableJournalProvider.h"
#include "lx/linux/systemd/UnavailableServiceProvider.h"
#endif
#include "lx/linux/procfs/ProcFsProcessProvider.h"
#include "lx/linux/procfs/ProcFsSystemMetricsProvider.h"
#include "lx/linux/procfs/SocketInodeResolver.h"
#include "lx/linux/netlink/NetlinkSocketProvider.h"
#include "lx/linux/process/LinuxSignalProvider.h"

#include <iostream>
#include <unistd.h>
#if LX_HAS_TUI
#include "lx/tui/FtxuiTuiRunner.h"
#endif

int main(const int argc, char** argv)
{
    const lx::linux::procfs::ProcFsProcessProvider processProvider;
    const lx::linux::procfs::ProcFsSystemMetricsProvider systemMetricsProvider;
    const lx::application::MetricsService metricsService{
        systemMetricsProvider, processProvider};
    const lx::application::StatusService statusService{metricsService};
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
    const lx::application::FindService findService{
        portService, processService, serviceService};
#if LX_HAS_TUI
    lx::application::DashboardService dashboardService{
        metricsService, processService, portService, serviceService};
    lx::tui::FtxuiTuiRunner tuiRunner{dashboardService};
    if (argc == 1 && ::isatty(STDIN_FILENO) && ::isatty(STDOUT_FILENO)) {
        const auto result = tuiRunner.run();
        if (!result) {
            std::cerr << result.error().message << '\n';
            return 5;
        }
        return 0;
    }
#endif
    return lx::cli::CliApp{doctorService, processService, portService,
                           serviceService, logService, inspectService,
                           findService, &statusService,
#if LX_HAS_TUI
                           &tuiRunner
#else
                           nullptr
#endif
                           }.run(
        argc, argv, std::cin, std::cout, std::cerr);
}
