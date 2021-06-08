#include "header_adapter.h"

#include <glog/logging.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <src/commands/pb/header.pb.h>

namespace kysync {

std::streamsize HeaderAdapter::WriteHeader(
    std::ostream &output,
    int version,
    std::streamsize data_size,
    std::streamsize block_size,
    std::string hash) {
  auto header = Header();
  header.set_version(version);
  header.set_size(data_size);
  header.set_block_size(block_size);
  header.set_hash(hash);

  google::protobuf::util::SerializeDelimitedToOstream(header, &output);

  return header.ByteSizeLong();
}

std::streamsize HeaderAdapter::ReadHeader(
    const std::vector<uint8_t> &buffer,
    int &version,
    std::streamsize &data_size,
    std::streamsize &block_size,
    std::string &hash) {
  auto header = Header();
  auto cs =
      google::protobuf::io::CodedInputStream(buffer.data(), buffer.size());
  google::protobuf::util::ParseDelimitedFromCodedStream(&header, &cs, nullptr);

  LOG(INFO) << header.DebugString();

  version = header.version();
  data_size = header.size();
  block_size = header.block_size();
  hash = header.hash();

  return cs.CurrentPosition();
}

}  // namespace kysync