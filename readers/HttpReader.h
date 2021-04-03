#ifndef KSYNC_HTTPREADER_H
#define KSYNC_HTTPREADER_H

#include "Reader.h"

class HttpReader : public Reader {
public:
  explicit HttpReader(const std::string &url);

  ~HttpReader() override;

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // KSYNC_HTTPREADER_H
