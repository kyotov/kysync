#ifndef KSYNC_SRC_KY_PARALLELIZE_PARALLELIZE_H
#define KSYNC_SRC_KY_PARALLELIZE_PARALLELIZE_H

#include <functional>

namespace ky::parallelize {

int Parallelize(
    std::streamsize data_size,
    std::streamsize block_size,
    std::streamsize overlap_size,
    int threads,
    std::function<
        void(int /*id*/, std::streamoff /*beg*/, std::streamoff /*end*/)> f);

}

#endif  // KSYNC_SRC_KY_PARALLELIZE_PARALLELIZE_H
