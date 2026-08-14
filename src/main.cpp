#include "lx/cli/CliApp.h"

#include <iostream>

int main(const int argc, char** argv)
{
    return lx::cli::CliApp{}.run(argc, argv, std::cout, std::cerr);
}
