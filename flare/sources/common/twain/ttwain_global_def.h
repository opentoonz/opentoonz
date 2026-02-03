#pragma once

/*max@home*/
#ifndef __GLOBAL_DEF_H__
#define __GLOBAL_DEF_H__
#ifdef _WIN32
#define GLOBAL_LOCK(P) GlobalLock(P)
#define GLOBAL_ALLOC(T, S) GlobalAlloc(T, S)
#define GLOBAL_FREE(P) GlobalFree(P)
#define GLOBAL_UNLOCK(P) GlobalUnlock(P)
#else
#ifdef __APPLE__
/*
#define GLOBAL_LOCK(P)      *(P)
//#define GLOBAL_ALLOC(T, S)  NewHandle(S)
//#define GLOBAL_FREE(P)      DisposeHandle( (char**)P)
#define GLOBAL_ALLOC(T, S)  (TW_HANDLE)NewPtr(S)
#define GLOBAL_FREE(S)      DisposePtr(S)
#define GLOBAL_UNLOCK(P)    {}
*/

#define GLOBAL_ALLOC(T, S) NewHandle(S)
#define GLOBAL_FREE(P) DisposeHandle(P)
//#define GLOBAL_ALLOC(T, S)  (TW_HANDLE)NewPtr(S)
//#define GLOBAL_FREE(S)      DisposePtr((char*)S)

#ifdef __cplusplus
extern "C" {
#endif

TW_HANDLE GLOBAL_LOCK(TW_HANDLE S);

#ifdef __cplusplus
}
#endif

#define GLOBAL_UNLOCK(P) HUnlock((TW_HANDLE)P)

#else /* UNIX */

#include <stdlib.h>
/* just some hack to get it built */
typedef void *TW_HANDLE;
#define GLOBAL_LOCK(P) (P)
#define GLOBAL_ALLOC(T, S) malloc(S)
#define GLOBAL_FREE(P) free(P)
#define GLOBAL_UNLOCK(P)

#endif
#endif
#endif /*__GLOBAL_DEF_H__*/

#ifdef __cplusplus
extern "C" {
#endif

void TTWAIN_ErrorBox(const char *msg);

#ifdef __cplusplus
}
#endif
