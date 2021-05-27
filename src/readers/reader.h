#ifndef KSYNC_READER_H
#define KSYNC_READER_H

#include <memory>
#include <vector>

#include "../metrics/metric_container.h"
#include "../metrics/metric_visitor.h"
#include "batch_retrieval_info.h"

namespace kysync {

class Reader : public MetricContainer {
  Metric total_reads_{};
  Metric total_bytes_read_{};

public:
  virtual ~Reader() = default;

  [[nodiscard]] virtual std::streamsize GetSize() const = 0;

  virtual std::streamsize
  Read(void *buffer, std::streamoff offset, std::streamsize size);

  virtual std::streamsize Read(
      void *buffer,
      std::vector<BatchRetrivalInfo> &batch_retrieval_infos);

  void Accept(MetricVisitor &visitor) override;

  static std::unique_ptr<Reader> Create(const std::string &uri);
};

}  // namespace kysync

#endif  // KSYNC_READER_H
