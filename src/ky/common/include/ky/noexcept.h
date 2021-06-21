#ifndef KSYNC_SRC_KY_COMMON_NOEXCEPT_H
#define KSYNC_SRC_KY_COMMON_NOEXCEPT_H

#include <functional>

namespace ky {

void NoExcept(const std::function<void()> &function);

}

#endif  // KSYNC_SRC_KY_COMMON_NOEXCEPT_H
