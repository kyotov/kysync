#ifndef KSYNC_READER_H
#define KSYNC_READER_H

#include <memory>
#include <vector>

#include "../metrics/metric_container.h"
#include "../metrics/metric_visitor.h"
#include "../utilities/utilities.h"
#include "batched_retrieval_info.h"

namespace kysync {

class Reader : public MetricContainer {
  PIMPL;

public:
  Reader();

  virtual ~Reader();

  [[nodiscard]] virtual size_t GetSize() const = 0;

  virtual size_t Read(void *buffer, size_t offset, size_t size) const;
  virtual size_t Read(
      void *buffer,
      std::vector<BatchedRetrivalInfo> &batched_retrievals_info) const = 0;

  void Accept(MetricVisitor &visitor) override;

  static std::unique_ptr<Reader> Create(const std::string &uri);
};

}  // namespace kysync

#endif  // KSYNC_READER_H
