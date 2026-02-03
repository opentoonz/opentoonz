#if defined _WIN32  // win32 or win64
#include "igs_resource_thread_win.cpp"
#else
#include "igs_resource_thread_unix.cpp"
#endif
/*
rem ------ WindowsXp sp3(32bits) vc2008sp1  :7,13 w! tes_w32_thread.bat
set SRC=igs_resource_thread
set TGT=tes_w32_thread
set INC=../../l01log/src
cl /W4 /MD /EHa /O2 /wd4819 /DDEBUG_THREAD_WIN /I%INC% %SRC%.cxx /Fe%TGT%
mt -manifest %TGT%.exe.manifest -outputresource:%TGT%.exe;1
del %SRC%.obj %TGT%.exe.manifest
rem ------ Windows7 (64bits) vc2008sp1  :14,20 w! tes_w64_thread.bat
set SRC=igs_resource_thread
set TGT=tes_w64_thread
set INC=../../l01log/src
cl /W4 /MD /EHa /O2 /wd4819 /DDEBUG_THREAD_WIN /I%INC% %SRC%.cxx /Fe%TGT%
mt -manifest %TGT%.exe.manifest -outputresource:%TGT%.exe;1
del %SRC%.obj %TGT%.exe.manifest
# ------ rhel4 (32bits)  :21,22 w! tes_u32_thread.csh
g++ -Wall -O2 -g -DDEBUG_THREAD_UNIX -I../../l01log/src igs_resource_thread.cxx
-lpthread -o tes_u32_thread
*/
