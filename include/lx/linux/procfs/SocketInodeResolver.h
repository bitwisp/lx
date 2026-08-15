#pragma once

#include "lx/contracts/ISocketOwnerResolver.h"

#include <filesystem>

namespace lx::linux::procfs {

class SocketInodeResolver final : public contracts::ISocketOwnerResolver {
public:
    explicit SocketInodeResolver(std::filesystem::path root = "/proc");

    [[nodiscard]] Result<Observation<SocketOwnership>> resolve(
        const std::vector<std::uint64_t>& inodes) const override;

private:
    std::filesystem::path root_;
};

} // namespace lx::linux::procfs
