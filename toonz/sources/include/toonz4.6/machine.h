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
*/

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

/*
#ifdef WIN32
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#else
#define TNZ_MACHINE_CHANNEL_ORDER_MBGR 1
#endif
*/

#endif /*MACHINE_DEFINED*/
