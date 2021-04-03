#ifndef KSYNC_READER_H
#define KSYNC_READER_H

#include <memory>

#include "../metrics/MetricContainer.h"
#include "../metrics/MetricVisitor.h"

class Reader : public MetricContainer {
public:
  Reader();

  virtual ~Reader();

  [[nodiscard]] virtual size_t GetSize() const = 0;

  virtual size_t Read(void *buffer, size_t offset, size_t size) const;

  void Accept(MetricVisitor &visitor) const override;

  static std::unique_ptr<Reader> Create(const std::string &uri);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif  // KSYNC_READER_H
