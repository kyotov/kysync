#include "strong_checksum_builder.h"

#include "xxhash.h"

namespace kysync {

StrongChecksumBuilder::StrongChecksumBuilder() : state_(XXH3_createState()) {
  XXH3_128bits_reset(state_);
}

void StrongChecksumBuilder::Update(const void* buffer, size_t size) {
  XXH3_128bits_update(state_, buffer, size);
}

StrongChecksum StrongChecksumBuilder::Digest() {
  auto digest = XXH3_128bits_digest(state_);

  return {.kHi = digest.high64, .kLo = digest.low64};
}

}  // namespace kysync
