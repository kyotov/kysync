#ifndef KSYNC_BATCH_RETRIEVAL_INFO_H
#define KSYNC_BATCH_RETRIEVAL_INFO_H

struct BatchRetrivalInfo {
  const size_t block_index;
  const size_t source_begin_offset;
  const size_t size_to_read;
  const size_t offset_to_write_to;
};

#endif  // KSYNC_BATCH_RETRIEVAL_INFO_H
