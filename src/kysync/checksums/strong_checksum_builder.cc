#include <kysync/checksums/strong_checksum_builder.h>

#include <xxhash.h>

namespace kysync {

StrongChecksumBuilder::StrongChecksumBuilder() : state_(XXH3_createState()) {
  XXH3_128bits_reset(state_);
}

void StrongChecksumBuilder::Update(const void* buffer, std::streamsize size) {
  XXH3_128bits_update(state_, buffer, size);
}

StrongChecksum StrongChecksumBuilder::Digest() {
  auto digest = XXH3_128bits_digest(state_);

  return {digest.high64, digest.low64};
}

}  // namespace kysync
