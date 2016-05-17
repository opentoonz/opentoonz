#pragma once

#ifndef T_MACHINE_INCLUDED
#define T_MACHINE_INCLUDED

#if defined(WIN32) || defined(i386)
#define TNZ_LITTLE_ENDIAN 1
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#elif defined(__sgi)
#define TNZ_LITTLE_ENDIAN 0
#define TNZ_MACHINE_CHANNEL_ORDER_MBGR 1
#elif defined(LINUX)
#define TNZ_LITTLE_ENDIAN 1
#define TNZ_MACHINE_CHANNEL_ORDER_RGBM 1
#elif defined(MACOSX)
#define TNZ_LITTLE_ENDIAN 0
#define TNZ_MACHINE_CHANNEL_ORDER_MRGB 1
#else
@UNKNOW PLATFORM @
#endif

#ifndef WIN32
#ifdef MACOSX
#define _finite isfinite
#else
// verificare che su sgi sia isfinite
#define _finite isfinite
#endif
#endif

#endif
