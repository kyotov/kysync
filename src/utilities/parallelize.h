#ifndef KSYNC_PARALLELIZE_H
#define KSYNC_PARALLELIZE_H

#include <functional>

int Parallelize(
    size_t data_size,
    size_t block_size,
    size_t overlap_size,
    int threads,
    std::function<void(int /*id*/, size_t /*beg*/, size_t /*end*/)> f);

#endif  // KSYNC_PARALLELIZE_H
