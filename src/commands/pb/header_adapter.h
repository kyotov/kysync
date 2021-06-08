#ifndef KSYNC_SRC_COMMANDS_PB_HEADER_ADAPTER_H
#define KSYNC_SRC_COMMANDS_PB_HEADER_ADAPTER_H

#include <ostream>
#include <vector>

namespace kysync {

/***
 * NOTE: clang-tidy 13 + MSVC does not like protobuf.
 * See the error below and note it is an error not warning.
 * Errors cannot be suppressed in clang-tidy.

 kydeps\install\protobuf_33fd97f288586ea391c2328b683d5ca093443e06\include\google/protobuf/repeated_field.h:743:27:
error: constexpr variable 'kRepHeaderSize' must be initialized by a constant
expression [clang-diagnostic-error] static constexpr size_t kRepHeaderSize =
offsetof(Rep, elements);

 kydeps\install\protobuf_33fd97f288586ea391c2328b683d5ca093443e06\include\google/protobuf/repeated_field.h:743:44:
note: cast that performs the conversions of a reinterpret_cast is not allowed in
a constant expression static constexpr size_t kRepHeaderSize = offsetof(Rep,
elements);

 C:\Program Files (x86)\Windows Kits\10\include\10.0.19041.0\ucrt\stddef.h:47:32:
note: expanded from macro 'offsetof' #define offsetof(s,m)
((::size_t)&reinterpret_cast<char const volatile&>((((s*)0)->m)))

 * I did not want to disable clang-tidy on all of Prepare and Sync cc files.
 * So I refactored the protobuf code within this class.
 * This class is compiled in the protobuf library which is clang-tidy-less.
 */

class HeaderAdapter {
public:
  static std::streamsize WriteHeader(
      std::ostream &output,
      int version,
      std::streamsize data_size,
      std::streamsize block_size,
      std::string hash);

  static std::streamsize ReadHeader(
      const std::vector<uint8_t> &buffer,
      int &version,
      std::streamsize &data_size,
      std::streamsize &block_size,
      std::string &hash);
};

}  // namespace kysync

#endif  // KSYNC_SRC_COMMANDS_PB_HEADER_ADAPTER_H
