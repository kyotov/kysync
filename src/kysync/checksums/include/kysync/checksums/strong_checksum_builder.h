#ifndef KSYNC_STRONG_CHECKSUM_BUILDER_H
#define KSYNC_STRONG_CHECKSUM_BUILDER_H

#include <memory>

#include <kysync/checksums/strong_checksum.h>

struct XXH3_state_s;

namespace kysync {

class StrongChecksumBuilder final {
  XXH3_state_s* state_;

public:
  StrongChecksumBuilder();

  void Update(const void* buffer, std::streamsize size);

  StrongChecksum Digest();
};

}  // namespace kysync

#endif  // KSYNC_STRONG_CHECKSUM_BUILDER_H
