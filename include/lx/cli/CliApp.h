#pragma once

#include <iosfwd>

namespace lx::cli {

class CliApp final {
public:
    int run(int argc, char** argv, std::ostream& output, std::ostream& error) const;
};

} // namespace lx::cli

