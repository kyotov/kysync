#ifndef KSYNC_PARALLELIZE_H
#define KSYNC_PARALLELIZE_H

#include <functional>
#include <ios>

int Parallelize(
    std::streamsize data_size,
    std::streamsize block_size,
    std::streamsize overlap_size,
    int threads,
    std::function<
        void(int /*id*/, std::streamoff /*beg*/, std::streamoff /*end*/)> f);

#endif  // KSYNC_PARALLELIZE_H
