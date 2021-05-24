#ifndef KSYNC_HTTP_READER_H
#define KSYNC_HTTP_READER_H

#include "reader.h"

namespace httplib {
  class Client;
}

namespace kysync {

class HttpReader final : public Reader {
  std::string host_;
  std::string path_;
  std::unique_ptr<httplib::Client> client_;

  // FIXME: httplib::Ranges
  size_t Read(void *buffer, std::vector<std::pair<int64_t, int64_t>> ranges);

public:
  explicit HttpReader(const std::string &url);

  ~HttpReader();

  [[nodiscard]] size_t GetSize() const override;

  size_t Read(void *buffer, size_t offset, size_t size) override;

  size_t Read(
      void *buffer,
      std::vector<BatchRetrivalInfo> &batched_retrieval_infos) override;
};

}  // namespace kysync

#endif  // KSYNC_HTTP_READER_H
