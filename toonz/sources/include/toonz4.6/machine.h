#pragma once

#ifndef MACHINE_DEFINED
#define MACHINE_DEFINED

/*
#ifdef WIN32
#define SCANNER_DEVNAME "scanner0"
#define DDR_DEVNAME     "ddr0"
#else
#define SCANNER_DEVNAME "/dev/scanner"
#define DDR_DEVNAME     "/dev/ddr"
#endif


#ifdef __sgi
#define TNZ_LITTLE_ENDIAN 0
#else
#define TNZ_LITTLE_ENDIAN 1
#endif
*/

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

/*
#ifdef WIN32
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#else
#define TNZ_MACHINE_CHANNEL_ORDER_MBGR 1
#endif
*/

#endif /*MACHINE_DEFINED*/
