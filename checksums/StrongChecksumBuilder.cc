#include "StrongChecksumBuilder.h"

#include "xxhash.h"

struct StrongChecksumBuilder::Impl {
  XXH3_state_t* state;

  Impl() : state(XXH3_createState()) { XXH3_128bits_reset(state); }

  void update(const void* buffer, size_t size) const {
    XXH3_128bits_update(state, buffer, size);
  }

  [[nodiscard]] XXH128_hash_t digest() const {
    return XXH3_128bits_digest(state);
  }
};

StrongChecksumBuilder::StrongChecksumBuilder()
    : pImpl(std::make_unique<Impl>()) {}

StrongChecksumBuilder::~StrongChecksumBuilder() = default;

void StrongChecksumBuilder::update(const void* buffer, size_t size) {
  pImpl->update(buffer, size);
}

StrongChecksum StrongChecksumBuilder::digest() {
  auto digest = pImpl->digest();

  return {.hi = digest.high64, .lo = digest.low64};
}
