#ifndef KSYNC_GENDATA_H
#define KSYNC_GENDATA_H

#include <filesystem>
#include <utility>

#include "../../commands/Command.h"
#include "../../utilities/utilities.h"

namespace kysync {

class GenData final : public Command {
  PIMPL;
  NO_COPY_OR_MOVE(GenData);

public:
  GenData(
      const std::filesystem::path &output_path,
      uint64_t data_size,
      uint64_t seed_data_size,
      uint64_t fragment_size,
      uint32_t similarity);

  ~GenData() noexcept final;

  int Run() override;
};

}  // namespace kysync

#endif  // KSYNC_GENDATA_H
