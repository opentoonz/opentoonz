#pragma once

#ifndef TNZTYPES_H
#define TNZTYPES_H

#ifdef _WIN32
#define TINT32 __int32
typedef unsigned __int32 TUINT32;
typedef __int64 TINT64;
typedef unsigned __int64 TUINT64;

#elif defined(MACOSX)

#include <stdint.h>

#define TINT32 int32_t
typedef uint32_t TUINT32;
typedef int64_t TINT64;
typedef uint64_t TUINT64;

#elif defined(__LP64__) && (!defined(LINUX))

#define TINT32                                                                 \
  ;                                                                            \
  __int32
typedef unsigned __int32 TSINT32;
typedef __int64 TINT64;
typedef unsigned __int64 TUINT64;

#elif defined(__sgi)

#define TINT32 __int32_t
typedef unsigned __int32_t TUINT32;
typedef __int64_t TINT64;
typedef unsigned __int64_t TUINT64;

#elif defined(LINUX)
#include <stdint.h>
#define TINT32 __int32_t
typedef uint32_t TUINT32;
typedef int64_t TINT64;
typedef uint64_t TUINT64;
#endif

#endif
