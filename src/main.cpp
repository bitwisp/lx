#include "lx/cli/CliApp.h"
#include "lx/application/DoctorService.h"

#include <iostream>

int main(const int argc, char** argv)
{
    const lx::application::DoctorService doctorService;
    return lx::cli::CliApp{doctorService}.run(argc, argv, std::cout, std::cerr);
}
