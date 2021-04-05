#ifndef KSYNC_STREAMS_H
#define KSYNC_STREAMS_H

#include <limits>
#include <ostream>
#include <vector>

namespace kysync {

size_t StreamWrite(std::ostream &stream, const void *data, size_t size);

template <typename T>
size_t StreamWrite(
    std::ostream &stream,
    std::vector<T> data,
    size_t max_size_to_write = std::numeric_limits<size_t>::max());

}  // namespace kysync

#endif  // KSYNC_STREAMS_H
