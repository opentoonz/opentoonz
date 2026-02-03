#include "igs_resource_thread.h"
#include "igs_resource_multithread.h"

namespace {
#if defined _WIN32  // vc compile_type
unsigned __stdcall
#else
void *
#endif
    function_(void *param) {
  igs::resource::thread_execute_interface *pp =
      static_cast<igs::resource::thread_execute_interface *>(param);
  pp->run();
  return 0;
};
}

void igs::resource::multithread::add(void *thread_execute_instance) {
  this->thre_exec_.push_back(thread_execute_instance);
}

void igs::resource::multithread::run(void) {
  if (1 == this->thre_exec_.size()) {
    /* 指定が一個の場合はスレッド実行せず、ただ実行 */
    igs::resource::thread_execute_interface *pp =
        static_cast<igs::resource::thread_execute_interface *>(
            this->thre_exec_.at(0));
    pp->run();
    return;
  }

// pthread_t = unsigned long int(rhel4)
// HANDLE = unsigned long(vc6.0) = void *(vc2005)
#if defined _WIN32  // vc compile_type
  std::vector<HANDLE> id;
#else
  std::vector<pthread_t> id;
#endif
  {
    std::vector<void *>::iterator it;
    for (it = this->thre_exec_.begin(); it != this->thre_exec_.end(); ++it) {
      id.push_back(igs::resource::thread_run(function_, *it));
    }
  }
  {
#if defined _WIN32  // vc compile_type
    std::vector<HANDLE>::iterator it;
#else
    std::vector<pthread_t>::iterator it;
#endif
    for (it = id.begin(); it != id.end(); ++it) {
      igs::resource::thread_join(*it);
    }
  }

  id.clear();
}
void igs::resource::multithread::clear(void) { this->thre_exec_.clear(); }
