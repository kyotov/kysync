#ifndef KSYNC_HTTPREADER_H
#define KSYNC_HTTPREADER_H

#include "Reader.h"

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

#endif  // KSYNC_HTTPREADER_H
