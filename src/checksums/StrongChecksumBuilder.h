#ifndef KSYNC_STRONGCHECKSUMBUILDER_H
#define KSYNC_STRONGCHECKSUMBUILDER_H

#include <memory>

#include "StrongChecksum.h"
#include "../utilities/utilities.h"

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

#endif  // KSYNC_STRONGCHECKSUMBUILDER_H
