#if defined _WIN32  // win32 or win64
#include "igs_resource_sleep_win.cpp"
#else
#include "igs_resource_sleep_unix.cpp"
#endif
