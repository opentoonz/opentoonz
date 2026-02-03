#include <stdexcept> /* std::domain_error */
#include "igs_resource_thread.h"
#include "igs_resource_msg_from_err.h"

const HANDLE igs::resource::thread_run(unsigned(__stdcall *function)(void *),
                                       void *func_arg, const int priority) {
  // unsigned thread_addr=0;
  HANDLE thread_id = reinterpret_cast<HANDLE>(
      ::_beginthreadex(NULL, 0, function, func_arg, 0, NULL));
  if (0 == thread_id) {
    throw std::domain_error(
        igs_resource_msg_from_err(TEXT("_beginthreadex(-)"), errno));
  }
  /*
  vc2005 MSDN より
  BOOL SetThreadPriority(int nPriority);
  に与える値を優先度の高いものから順に並べます。
  THREAD_PRIORITY_TIME_CRITICAL		プロセスにより15 or 31
  THREAD_PRIORITY_HIGHEST			標準より2ポイント高い
  THREAD_PRIORITY_ABOVE_NORMAL		標準より1ポイント高い
  THREAD_PRIORITY_NORMAL		-1	標準
  THREAD_PRIORITY_BELOW_NORMAL		標準より1ポイント低い
  THREAD_PRIORITY_LOWEST			標準より2ポイント低い
  THREAD_PRIORITY_IDLE			プロセスにより1 or 16
  */
  if (0 == ::SetThreadPriority(thread_id, priority)) {
    throw std::domain_error(igs_resource_msg_from_err(
        TEXT("SetThreadPriority(-)"), ::GetLastError()));
  }
  return thread_id;
}

/*
以下の関数でthreadの終了を見るよりも、
thread内で終了時に終了スイッチを入れる方法で感知する。
linuxでのやり方にあわせる。2011-03-30
*/
const bool igs::resource::thread_was_done(const HANDLE thread_id) {
  DWORD exit_code = 0;
  if (0 == ::GetExitCodeThread(thread_id, &exit_code)) {
    throw std::domain_error(
        igs_resource_msg_from_err("GetExitCodeThread(-)", ::GetLastError()));
  }
  if (exit_code == STILL_ACTIVE) {
    return false;
  }
  return true;
}
void igs::resource::thread_join(const HANDLE thread_id) {
  thread_wait(thread_id);
  thread_close(thread_id);
}
void igs::resource::thread_wait(const HANDLE thread_id) {
  /* _endthreadex(-)はスレッドハンドルを閉じない??? */
  if (WAIT_FAILED == ::WaitForSingleObject(thread_id, INFINITE)) {
    throw std::domain_error(igs_resource_msg_from_err(
        TEXT("WaitForSingleObject(-)"), ::GetLastError()));
  }
}
void igs::resource::thread_close(const HANDLE thread_id) {
  if (0 == ::CloseHandle(thread_id)) {
    throw std::domain_error(
        igs_resource_msg_from_err(TEXT("CloseHandle(-)"), ::GetLastError()));
  }
}
