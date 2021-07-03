#ifndef KSYNC_READER_H
#define KSYNC_READER_H

#include <ky/metrics/metrics.h>
#include <kysync/readers/batch_retrieval_info.h>

#include <functional>
#include <memory>
#include <vector>

namespace kysync {

using RetrievalCallback = std::function<void(
    const char * /*read_buffer*/,
    const BatchRetrivalInfo & /*retrieval_info*/)>;

class Reader : public ky::metrics::MetricContainer {
  ky::metrics::Metric total_reads_{};
  ky::metrics::Metric total_bytes_read_{};

public:
  virtual ~Reader() = default;

  [[nodiscard]] virtual std::streamsize GetSize() const = 0;

  virtual std::streamsize
  Read(void *buffer, std::streamoff offset, std::streamsize size);

  virtual std::streamsize Read(
      void *buffer,
      std::vector<BatchRetrivalInfo> &batch_retrieval_infos,
      const RetrievalCallback &retrieval_callback);

  void Accept(ky::metrics::MetricVisitor &visitor) override;

  static std::unique_ptr<Reader> Create(const std::string &uri);
};

}  // namespace kysync

#endif  // KSYNC_READER_H
