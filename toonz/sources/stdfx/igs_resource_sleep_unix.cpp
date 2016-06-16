#include <stdexcept>  // std::domain_error()
#include <cerrno>     // errno
#include "igs_resource_msg_from_err.h"
#include "igs_resource_sleep.h"
/*
1second=1,000milli_seconds=1,000,000micro_seconds=1,000,000,000nano_seconds
tv_sec(seconds)が負か、tv_nsec(nano_seconds)が0〜999,999,999範囲外、はエラー
*/
void igs::resource::sleep_sn(const time_t seconds, const long nano_seconds) {
  struct timespec req;
  req.tv_sec  = seconds;
  req.tv_nsec = nano_seconds;
  struct timespec rem;
  rem.tv_sec  = 0;
  rem.tv_nsec = 0;
  if (::nanosleep(&req, &rem) < 0) {
    throw std::domain_error(
        igs_resource_msg_from_err(TEXT("nanosleep(-)"), errno));
  }
}
/*
windows ::Sleep()の仕様にあわせるためだけの関数
1second = 1,000milli_seconds(ミリ秒)
*/
void igs::resource::sleep_m(const DWORD milli_seconds) {
  const time_t seconds    = milli_seconds / 1000;
  const long nano_seconds = (milli_seconds % 1000) * 1000000;
  // std::cout << "milli_seconds=" << milli_seconds << std::endl;
  // std::cout << "      seconds=" << seconds << std::endl;
  // std::cout << " nano_seconds=" << nano_seconds << std::endl;
  igs::resource::sleep_sn(seconds, nano_seconds);
}
