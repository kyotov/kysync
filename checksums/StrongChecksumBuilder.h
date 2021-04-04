#ifndef KSYNC_STRONGCHECKSUMBUILDER_H
#define KSYNC_STRONGCHECKSUMBUILDER_H

#include <memory>

#include "StrongChecksum.h"

namespace kysync {

class StrongChecksumBuilder {
public:
  StrongChecksumBuilder();

  ~StrongChecksumBuilder();

  void Update(const void* buffer, size_t size);

  StrongChecksum Digest();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace kysync

#endif  // KSYNC_STRONGCHECKSUMBUILDER_H
