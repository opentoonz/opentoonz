#pragma once

/* SDef.h: Sasa definitions.
*/

//#include "tmsg.h"

#ifndef _SDEF_H_
#define _SDEF_H_

// typedef   signed char  SCHAR;
typedef unsigned char UCHAR;
/*typedef unsigned short USHORT;
typedef unsigned int   UINT;
typedef unsigned long  ULONG;
*/

typedef struct _UC_PIXEL { unsigned char b, g, r, m; } UC_PIXEL;
typedef struct _US_PIXEL { unsigned short b, g, r, m; } US_PIXEL;
typedef struct _I_PIXEL { int b, g, r, m; } I_PIXEL;

#define ASSIGN_PIXEL(d, s)                                                     \
  {                                                                            \
    (d)->r = (s)->r;                                                           \
    (d)->g = (s)->g;                                                           \
    (d)->b = (s)->b;                                                           \
    (d)->m = (s)->m;                                                           \
  }
#define NULL_PIXEL(d)                                                          \
  { (d)->r = (d)->g = (d)->b = (d)->m = 0; }
#define COLOR_PIXEL(d, r, g, b, m)                                             \
  {                                                                            \
    (d)->r = (r);                                                              \
    (d)->g = (g);                                                              \
    (d)->b = (b);                                                              \
    (d)->m = (m);                                                              \
  }

typedef struct { int x0, y0, x1, y1; } SRECT;
typedef struct { double x0, y0, x1, y1; } SDRECT;
typedef struct { int x0, y0, x1, y1; } SLINE;
typedef struct { double x0, y0, x1, y1; } SDLINE;
typedef struct { int x, y; } SPOINT;
typedef struct { double x, y; } SDPOINT;
typedef struct { int x, y, w; } SXYW;
typedef struct {
  int x, y;
  double w;
} SXYDW;

typedef struct {
  int x, y;
  double d;
} SXYD;

#define I_ROUND(x)                                                             \
  ((int)(((int)(-0.9F) == 0 && (x) < 0.0F) ? ((x)-0.5F) : ((x) + 0.5F)))
#define I_ROUNDP(x) ((int)((x) + 0.5F))
#define UC_ROUND(x) ((unsigned char)((x) + 0.5F))
#define US_ROUND(x) ((unsigned short)((x) + 0.5F))

#define I_CUT_0_255(x) ((x) < 0 ? 0 : ((x) > 255 ? 255 : (x)))
#define I_CUT_0_65535(x) ((x) < 0 ? 0 : ((x) > 65535 ? 65535 : (x)))
#define D_CUT_0_255(x) ((x) < 0.0 ? 0.0 : ((x) > 255.0 ? 255.0 : (x)))
#define D_CUT_0_65535(x) ((x) < 0.0 ? 0.0 : ((x) > 65535.0 ? 65535.0 : (x)))
#define D_CUT_0_1(x) ((x) < 0.0 ? 0.0 : ((x) > 1.0 ? 1.0 : (x)))
#define D_CUT(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

#define UC_MAX 255
#define US_MAX 65535

#define PI 3.1415926535
#define UNDEFINED (-99999.9)
/*
#define FLOOR(x) ((int)(x) > (x) ? (int)(x)-1 : (int)(x))
#define CEIL(x)  ((int)(x) < (x) ? (int)(x)+1 : (int)(x))
#define DEG2RAD(x) ((x)*PI/180.0)
*/
#define DEG2RAD(x) ((x)*0.01745329252)
/*#define RAD2DEG(x) ((x)*180.0/PI )
*/
#define RAD2DEG(x) ((x)*57.29577951472)

#define TOONZ_INTERNAL_MSG

#ifdef TOONZ_INTERNAL_MSG
#define smsg_info tmsg_info
#define smsg_warning tmsg_warning
#define smsg_error tmsg_error
#else
#define smsg_info printf
#define smsg_warning printf
#define smsg_error printf
#endif

#endif
