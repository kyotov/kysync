#include <glog/logging.h>
#include <ky/noexcept.h>

namespace ky {

void NoExcept(const std::function<void()> &function) {
  try {
    function();
  } catch (std::exception &e) {
    LOG(FATAL) << e.what();
    exit(1);  // NOLINT(concurrency-mt-unsafe)
  } catch (...) {
    LOG(FATAL) << "unknown exception";
    exit(2);  // NOLINT(concurrency-mt-unsafe)
  }
}

}  // namespace ky