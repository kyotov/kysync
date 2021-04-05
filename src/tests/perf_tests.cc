#include <gtest/gtest.h>

#include "../Monitor.h"
#include "../commands/PrepareCommand.h"
#include "../commands/SyncCommand.h"
#include "TempPath.h"
#include "commands/GenDataCommand.h"

namespace kysync {

TEST(Performance, 1G_same) {  // NOLINT
  auto tmp = TempPath(true);

  auto gen_data = GenDataCommand(  //
      tmp.GetPath(),
      1'000'000,
      -1ULL,
      123'456,
      90);
  EXPECT_EQ(Monitor(gen_data).Run(), 0);

  auto prepare = PrepareCommand(  //
      tmp.GetPath() / "data.bin",
      "",
      "",
      16'384);
  EXPECT_EQ(Monitor(prepare).Run(), 0);

  auto sync = SyncCommand(  //
      "file://" + (tmp.GetPath() / "data.bin").string(),
      "file://" + (tmp.GetPath() / "data.bin.kysync").string(),
      "file://" + (tmp.GetPath() / "data.bin").string(),
      tmp.GetPath() / "output.bin",
      true,
      32);
  EXPECT_EQ(Monitor(sync).Run(), 0);
}

}  // namespace kysync