#ifndef KSYNC_STRONG_CHECKSUM_BUILDER_H
#define KSYNC_STRONG_CHECKSUM_BUILDER_H

#include <memory>

#include "../utilities/utilities.h"
#include "strong_checksum.h"

namespace kysync {

class StrongChecksumBuilder final {
  PIMPL;
  NO_COPY_OR_MOVE(StrongChecksumBuilder);

public:
  StrongChecksumBuilder();

  ~StrongChecksumBuilder();

  void Update(const void* buffer, size_t size);

  StrongChecksum Digest();
};

}  // namespace kysync

#endif  // KSYNC_STRONG_CHECKSUM_BUILDER_H
