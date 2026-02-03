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

#endif /* !igs_resource_sleep_h */
