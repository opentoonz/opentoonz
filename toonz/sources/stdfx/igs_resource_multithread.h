#pragma once

#ifndef igs_resource_multithread_h
#define igs_resource_multithread_h

#include <vector>
namespace igs {
namespace resource {
class thread_execute_interface {
public:
  virtual void run(void) = 0;
  virtual ~thread_execute_interface() {} /* 仮想デストラクタの空定義 */
};

class multithread {
public:
  void add(void *thread_execute_instance);
  void run(void); /* 指定が一個の場合はスレッド実行せずただ実行 */
  void clear(void);

private:
  std::vector<void *> thre_exec_;
};
}
}

#endif /* !igs_resource_multithread_h */
