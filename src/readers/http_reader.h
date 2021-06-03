#ifndef KSYNC_HTTP_READER_H
#define KSYNC_HTTP_READER_H

#include <cstdio>

#include "reader.h"

#if defined(_MSC_VER) && defined(_WIN64)
using ssize_t = int64_t;
#endif

namespace httplib {
class Client;
}  // namespace httplib

namespace kysync {

class HttpReader final : public Reader {
  std::string host_;
  std::string path_;
  std::unique_ptr<httplib::Client> client_;

  // FIXME: httplib::Ranges
  std::streamsize Read(
      void *buffer,
      std::vector<std::pair<ssize_t, ssize_t>> ranges);

public:
  explicit HttpReader(const std::string &url);

  ~HttpReader() override;

  [[nodiscard]] std::streamsize GetSize() const override;

  std::streamsize
  Read(void *buffer, std::streamoff offset, std::streamsize size) override;

  std::streamsize Read(
      void *buffer,
      std::vector<BatchRetrivalInfo> &batched_retrieval_infos) override;
};

}  // namespace kysync

#endif  // KSYNC_HTTP_READER_H
