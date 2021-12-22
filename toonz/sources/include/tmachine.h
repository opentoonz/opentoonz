#pragma once

#ifndef T_MACHINE_INCLUDED
#define T_MACHINE_INCLUDED

#if defined(_WIN32) || defined(i386)
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#elif defined(__sgi)
#define TNZ_MACHINE_CHANNEL_ORDER_MBGR 1
#elif defined(LINUX) || defined(FREEBSD)
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#elif defined(MACOSX)
#define TNZ_MACHINE_CHANNEL_ORDER_MRGB 1
#else
@UNKNOW PLATFORM @
#endif

#if !defined(TNZ_LITTLE_ENDIAN)
#error "TNZ_LITTLE_ENDIAN not defined!"
#endif

#endif
