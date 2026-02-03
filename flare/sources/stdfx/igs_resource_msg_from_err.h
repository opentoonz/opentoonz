#pragma once

#if defined _WIN32  // win32 or win64
#include "igs_resource_msg_from_err_win.h"
#else
#include "igs_resource_msg_from_err_unix.h"
#endif
