#include "parallelize.h"

#include <glog/logging.h>

#include <future>

int Parallelize(
    size_t data_size,
    size_t block_size,
    size_t overlap_size,
    int threads,
    std::function<void(int /*id*/, size_t /*beg*/, size_t /*end*/)> f) {
  auto blocks = (data_size + block_size - 1) / block_size;
  auto chunk = (blocks + threads - 1) / threads;

  if (chunk < 2) {
    LOG(INFO) << "size too small... not using parallelization";
    threads = 1;
    chunk = blocks;
  }

  VLOG(1)                                  //
      << "parallelize size=" << data_size  //
      << " block=" << block_size           //
      << " threads=" << threads;

  std::vector<std::future<void>> fs;

  for (int id = 0; id < threads; id++) {
    auto beg = id * chunk * block_size;
    auto end = (id + 1) * chunk * block_size + overlap_size;
    VLOG(2) << "thread=" << id << " [" << beg << ", " << end << ")";

    fs.push_back(std::async(f, id, beg, std::min(end, data_size)));
  }

  std::chrono::milliseconds chill(100);
  auto done = false;
  while (!done) {
    done = true;
    for (auto &ff : fs) {
      if (ff.wait_for(chill) != std::future_status::ready) {
        done = false;
      }
    }
  }

  return threads;
}