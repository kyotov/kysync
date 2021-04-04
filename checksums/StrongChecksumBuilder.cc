#include "StrongChecksumBuilder.h"

#include "xxhash.h"

struct StrongChecksumBuilder::Impl {
  XXH3_state_t* state;

  Impl() : state(XXH3_createState()) { XXH3_128bits_reset(state); }

  void Update(const void* buffer, size_t size) const {
    XXH3_128bits_update(state, buffer, size);
  }

  [[nodiscard]] XXH128_hash_t Digest() const {
    return XXH3_128bits_digest(state);
  }
};

StrongChecksumBuilder::StrongChecksumBuilder()
    : impl_(std::make_unique<Impl>()) {}

StrongChecksumBuilder::~StrongChecksumBuilder() = default;

void StrongChecksumBuilder::Update(const void* buffer, size_t size) {
  impl_->Update(buffer, size);
}

StrongChecksum StrongChecksumBuilder::Digest() {
  auto digest = impl_->Digest();

  return {.hi_ = digest.high64, .lo_ = digest.low64};
}
