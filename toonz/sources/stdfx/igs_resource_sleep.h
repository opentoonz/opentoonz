#pragma once

#if defined _WIN32  // win32 or win64
#include "igs_resource_sleep_win.h"
#else
#include "igs_resource_sleep_unix.h"
#endif
