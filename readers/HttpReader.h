#ifndef KSYNC_HTTPREADER_H
#define KSYNC_HTTPREADER_H

#include "Reader.h"

class HttpReader : public Reader {
public:
  explicit HttpReader(const std::string &url);

  ~HttpReader() override;

  [[nodiscard]] size_t size() const override;

  size_t read(void *buffer, size_t offset, size_t size) const override;

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl;
};

#endif  // KSYNC_HTTPREADER_H
