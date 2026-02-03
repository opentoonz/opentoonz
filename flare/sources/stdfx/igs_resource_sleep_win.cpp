#include "igs_resource_sleep.h"

/* 1second = 1,000milli_seconds(ミリ秒) */
void igs::resource::sleep_m(const DWORD milli_seconds) {
  ::Sleep(milli_seconds);
}
