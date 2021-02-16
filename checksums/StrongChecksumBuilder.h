#ifndef KSYNC_STRONGCHECKSUMBUILDER_H
#define KSYNC_STRONGCHECKSUMBUILDER_H

#include "StrongChecksum.h"

class StrongChecksumBuilder {
public:
  StrongChecksumBuilder();

  ~StrongChecksumBuilder();

  void update(const void* buffer, size_t size);

  StrongChecksum digest();

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_STRONGCHECKSUMBUILDER_H
