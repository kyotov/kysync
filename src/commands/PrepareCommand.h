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
public:
  PrepareCommand(
      const fs::path &input_filename,
      const fs::path &output_ksync_filename,
      const fs::path &output_compressed_filename,
      size_t block_size);

  PrepareCommand(PrepareCommand &&) noexcept;

  ~PrepareCommand() override;

  // TODO: can this be const??
  int Run() override;

  void Accept(MetricVisitor &visitor) override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  friend class KySyncTest;
};

}  // namespace kysync

#endif  // KSYNC_PREPARECOMMAND_H
