#ifndef KSYNC_HTTP_READER_H
#define KSYNC_HTTP_READER_H

#include "reader.h"

namespace kysync {

class HttpReader : public Reader {
  PIMPL;
  NO_COPY_OR_MOVE(HttpReader);

public:
  explicit HttpReader(const std::string &url);

  ~HttpReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;
};

}  // namespace kysync

#endif  // KSYNC_HTTP_READER_H
