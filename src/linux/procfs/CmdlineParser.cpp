#include "lx/linux/procfs/CmdlineParser.h"

namespace lx::linux::procfs {

std::vector<std::string> parseCmdline(const std::string_view contents)
{
    std::vector<std::string> arguments;
    std::size_t begin = 0;
    while (begin < contents.size()) {
        const auto end = contents.find('\0', begin);
        if (end == std::string_view::npos) {
            arguments.emplace_back(contents.substr(begin));
            break;
        }
        arguments.emplace_back(contents.substr(begin, end - begin));
        begin = end + 1;
    }
    return arguments;
}

} // namespace lx::linux::procfs

