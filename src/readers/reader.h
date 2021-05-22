#ifndef KSYNC_READER_H
#define KSYNC_READER_H

#include <memory>
#include <vector>

#include "../metrics/metric_container.h"
#include "../metrics/metric_visitor.h"
#include "../utilities/utilities.h"
#include "batch_retrieval_info.h"

namespace kysync {

class Reader : public MetricContainer {
public:
  [[nodiscard]] virtual size_t GetSize() const = 0;

  virtual size_t Read(void *buffer, size_t offset, size_t size);

  virtual size_t Read(
      void *buffer,
      std::vector<BatchRetrivalInfo> &batch_retrieval_infos);

  void Accept(MetricVisitor &visitor) override;

  static std::unique_ptr<Reader> Create(const std::string &uri);

private:
  Metric total_reads_;
  Metric total_bytes_read_;
};

}  // namespace kysync

#endif  // KSYNC_READER_H
