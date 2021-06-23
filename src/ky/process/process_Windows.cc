#include <glog/logging.h>
#include <ky/process.h>
#include <windows.h>

#include "include/ky/process.h"

namespace ky {

struct ProcessWatcher::State {
  HANDLE job_handle;
};

/***
 * Windows Version.
 * Adapted from https://devblogs.microsoft.com/oldnewthing/20131209-00/?p=2433
 */
ProcessWatcher::ProcessWatcher() : state_(std::make_unique<State>()) {
  state_->job_handle = CreateJobObject(nullptr, nullptr);

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {
      .BasicLimitInformation = {
          .LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE}};

  SetInformationJobObject(
      state_->job_handle,
      JobObjectExtendedLimitInformation,
      &info,
      sizeof(info));
}

ProcessWatcher::~ProcessWatcher() = default;

void ProcessWatcher::Execute(const std::vector<std::string> &argv) {
  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi;

  auto mutable_command_line = std::string();

  for (const auto &arg : argv) {
    mutable_command_line += arg + " ";
  }

  auto result = CreateProcess(
      nullptr,
      mutable_command_line.data(),
      nullptr,
      nullptr,
      FALSE,
      CREATE_SUSPENDED,
      nullptr,
      nullptr,
      &si,
      &pi);
  CHECK(result);

  result = AssignProcessToJobObject(state_->job_handle, pi.hProcess);
  if (result) {
    result = ResumeThread(pi.hThread) != static_cast<DWORD>(-1);
  } else {
    TerminateProcess(pi.hProcess, 0);
  }
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CHECK(result);
}

}  // namespace ky