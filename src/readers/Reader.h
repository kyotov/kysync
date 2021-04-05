#ifndef KSYNC_READER_H
#define KSYNC_READER_H

#include <memory>

#include "../metrics/MetricContainer.h"
#include "../metrics/MetricVisitor.h"
#include "../utilities/utilities.h"

namespace kysync {

class Reader : public MetricContainer {
  PIMPL;
  NO_COPY_OR_MOVE(Reader);

public:
  Reader();

  virtual ~Reader();

  [[nodiscard]] virtual size_t GetSize() const = 0;

  virtual size_t Read(void *buffer, size_t offset, size_t size) const;

  void Accept(MetricVisitor &visitor) override;

  static std::unique_ptr<Reader> Create(const std::string &uri);
};

}  // namespace kysync

#endif  // KSYNC_READER_H
