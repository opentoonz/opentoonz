#pragma once

#ifndef igs_resource_sleep_h
#define igs_resource_sleep_h

#include "igs_os_type.h"  // DWORD

#ifndef IGS_RESOURCE_IFX_EXPORT
#define IGS_RESOURCE_IFX_EXPORT
#endif

namespace igs {
namespace resource {
/* 1second = 1,000milli_seconds(ミリ秒) */
IGS_RESOURCE_IFX_EXPORT void sleep_m(const DWORD milli_seconds);
}
}

#if !defined _WIN32
#include <ctime>  // time_t, ::nanosleep()
namespace igs {
namespace resource {
/* unixではこちらがメイン,sleep_m()は内部でsleep_sn()を呼んでいる
1second=1,000milli_seconds=1,000,000micro_seconds=1,000,000,000nano_seconds
tv_sec(seconds)が負か、tv_nsec(nano_seconds)が0〜999,999,999範囲外、はエラー
        */
void sleep_sn(const time_t seconds, const long nano_seconds);
}
}
#endif /* !_WIN32 */

#endif /* !igs_resource_sleep_h */
