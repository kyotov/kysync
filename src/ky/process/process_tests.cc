#include <gtest/gtest.h>
#include <ky/process.h>
#include <ky/temp_path.h>
#include <kysync/test_locations.h>

#include "test_file_chatter.h"

namespace ky {

using namespace std::chrono_literals;

TEST(KyProcess, Test1) {  // NOLINT
  auto tmp = TempPath(std::filesystem::temp_directory_path(), true);

  auto parent_chat = TestFileChatter(
      "test<->parent",
      tmp.GetPath() / "parent_output",
      tmp.GetPath() / "parent_input");

  auto child_chat = TestFileChatter(
      "test<->child",
      tmp.GetPath() / "child_output",
      tmp.GetPath() / "child_input");

  // start the parent chat bot, which internally starts the child chat bot
  auto pw = ProcessWatcher();
  pw.Execute(
      {kTestChatBotExecutable.string(),
       "--name",
       "parent",
       "--input-path",
       (tmp.GetPath() / "parent_input").string(),
       "--output-path",
       (tmp.GetPath() / "parent_output").string(),
       "--spawn-a-child",
       "true"});

  // make sure that the child is alive
  child_chat.Write("ping");
  ASSERT_EQ(child_chat.Read(), "pong");

  // make sure that the parent is alive
  parent_chat.Write("ping");
  ASSERT_EQ(parent_chat.Read(), "pong");

  // ask the parent to stop itself
  parent_chat.Write("stop");

  // make sure the parent is gone
  parent_chat.Write("ping");
  ASSERT_EQ(parent_chat.Read(), "");

  // make sure the child is also gone (as a result of the parent exiting)
  child_chat.Write("ping");
  ASSERT_EQ(child_chat.Read(), "");
}

}  // namespace ky
