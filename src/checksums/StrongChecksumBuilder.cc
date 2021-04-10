#include "StrongChecksumBuilder.h"

#include "xxhash.h"

namespace kysync {

class StrongChecksumBuilder::Impl {
  XXH3_state_t* state_;

public:
  Impl() : state_(XXH3_createState()) { XXH3_128bits_reset(state_); }

  void Update(const void* buffer, size_t size) const {
    XXH3_128bits_update(state_, buffer, size);
  }

  [[nodiscard]] XXH128_hash_t Digest() const {
    return XXH3_128bits_digest(state_);
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

  return {.kHi = digest.high64, .kLo = digest.low64};
}

}  // namespace kysync
