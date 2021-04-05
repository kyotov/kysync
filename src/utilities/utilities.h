#ifndef KSYNC_UTILITIES_H
#define KSYNC_UTILITIES_H

#define NO_COPY_OR_MOVE(X)         \
private:                           \
  X(const X &) = delete;           \
  X(X &&) = delete;                \
  X operator=(const X &) = delete; \
  X operator=(X &&) = delete

#define PIMPL  \
private:       \
  struct Impl; \
  std::unique_ptr<Impl> impl_

#endif  // KSYNC_UTILITIES_H
