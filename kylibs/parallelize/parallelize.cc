#include <glog/logging.h>
#include <ky/parallelize.h>

#include <future>

namespace ky::parallelize {

void Parallelize(
    std::streamsize data_size,
    std::streamsize block_size,
    std::streamsize overlap_size,
    int threads,
    const std::function<
        void(int /*id*/, std::streamoff /*beg*/, std::streamoff /*end*/)> &f) {
  auto blocks = (data_size + block_size - 1) / block_size;
  auto chunk = (blocks + threads - 1) / threads;

  if (chunk < 2) {
    LOG(INFO) << "size too small... not using parallelization";
    threads = 1;
    chunk = blocks;
  }

  VLOG(1)  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
      << "parallelize size=" << data_size  //
      << " block=" << block_size           //
      << " threads=" << threads;

  std::vector<std::future<void>> fs;
  for (int id = 0; id < threads; id++) {
    auto beg = id * chunk * block_size;
    auto end = (id + 1) * chunk * block_size + overlap_size;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    VLOG(2) << "thread=" << id << " [" << beg << ", " << end << ")";

    fs.push_back(std::async(f, id, beg, std::min(end, data_size)));
  }

  // FIXME: research if this is needed at all... maybe the threads are jthreads
  //        and therefore we don't need this loop. once upon a time i think i
  //        tripped on it and needed it... consider removing it.
  // NOTE: https://www.cplusplus.com/reference/future/async/ states:
  // When launch::async is selected, the future returned is linked to the end of
  // the thread created, even if its shared state is never accessed: in this
  // case, its destructor synchronizes with the return of fn. Therefore, the
  // return value shall not be disregarded for asynchronous behavior, even when
  // fn returns void.
  // Based on this, there is reason to expect the future destructor to perform a
  // join until the corresponding async function completes.
  auto done = false;
  for (auto &ff : fs) {
    CHECK(ff.valid()) << "Future in invalid state";
    ff.wait();
  }
}

}  // namespace ky::parallelize
