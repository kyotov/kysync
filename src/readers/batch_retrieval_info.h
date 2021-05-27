#ifndef KSYNC_SRC_READERS_BATCH_RETRIEVAL_INFO_H
#define KSYNC_SRC_READERS_BATCH_RETRIEVAL_INFO_H

#include <ios>

namespace kysync {

struct BatchRetrivalInfo {
  int block_index;
  std::streamoff source_begin_offset;
  std::streamsize size_to_read;
  std::streamoff offset_to_write_to;
};

}  // namespace kysync

#endif  // KSYNC_SRC_READERS_BATCH_RETRIEVAL_INFO_H
