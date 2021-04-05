#ifndef KSYNC_PREPARECOMMAND_H
#define KSYNC_PREPARECOMMAND_H

#include <filesystem>
#include <memory>

#include "../readers/Reader.h"
#include "Command.h"

namespace kysync {

namespace fs = std::filesystem;

class KySyncTest;

class PrepareCommand final : public Command {
  friend class KySyncTest;
  PIMPL;
  NO_COPY_OR_MOVE(PrepareCommand);

public:
  PrepareCommand(
      const fs::path &input_filename,
      const fs::path &output_ksync_filename,
      const fs::path &output_compressed_filename,
      size_t block_size);

  ~PrepareCommand() override;

  // TODO: can this be const??
  int Run() override;

  void Accept(MetricVisitor &visitor) override;
};

}  // namespace kysync

#endif  // KSYNC_PREPARECOMMAND_H
