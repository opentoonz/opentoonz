#pragma once

#ifndef _PIXEL_H_
#define _PIXEL_H_

#include "machine.h"
//#include "toonz.h"

#include "tnztypes.h"

#ifdef TNZ_MACHINE_CHANNEL_ORDER_BGRM

typedef struct _LPIXEL { unsigned char b, g, r, m; } LPIXEL;
typedef struct _SPIXEL { unsigned short b, g, r, m; } SPIXEL;

#if TNZ_LITTLE_ENDIAN
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0x00ffffff
#define LPIXEL_M_MASK 0xff000000
#define LPIXEL_R_MASK 0x00ff0000
#define LPIXEL_G_MASK 0x0000ff00
#define LPIXEL_B_MASK 0x000000ff
#define LPIXEL_M_SHIFT 24
#define LPIXEL_R_SHIFT 16
#define LPIXEL_G_SHIFT 8
#define LPIXEL_B_SHIFT 0
#else
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0xffffff00
#define LPIXEL_M_MASK 0x000000ff
#define LPIXEL_R_MASK 0x0000ff00
#define LPIXEL_G_MASK 0x00ff0000
#define LPIXEL_B_MASK 0xff000000
#define LPIXEL_M_SHIFT 0
#define LPIXEL_R_SHIFT 8
#define LPIXEL_G_SHIFT 16
#define LPIXEL_B_SHIFT 24
#endif

#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)

typedef struct { unsigned char m, b, g, r; } LPIXEL;
typedef struct { unsigned short m, b, g, r; } SPIXEL;

#if TNZ_LITTLE_ENDIAN
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0xffffff00
#define LPIXEL_M_MASK 0x000000ff
#define LPIXEL_R_MASK 0xff000000
#define LPIXEL_G_MASK 0x00ff0000
#define LPIXEL_B_MASK 0x0000ff00
#define LPIXEL_M_SHIFT 0
#define LPIXEL_R_SHIFT 24
#define LPIXEL_G_SHIFT 16
#define LPIXEL_B_SHIFT 8
#else
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0x00ffffff
#define LPIXEL_M_MASK 0xff000000
#define LPIXEL_R_MASK 0x000000ff
#define LPIXEL_G_MASK 0x0000ff00
#define LPIXEL_B_MASK 0x00ff0000
#define LPIXEL_M_SHIFT 24
#define LPIXEL_R_SHIFT 0
#define LPIXEL_G_SHIFT 8
#define LPIXEL_B_SHIFT 16
#endif

#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)

typedef struct _LPIXEL { unsigned char r, g, b, m; } LPIXEL;
typedef struct _SPIXEL { unsigned short r, g, b, m; } SPIXEL;

#if TNZ_LITTLE_ENDIAN
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0x00ffffff
#define LPIXEL_M_MASK 0xff000000
#define LPIXEL_B_MASK 0x00ff0000
#define LPIXEL_G_MASK 0x0000ff00
#define LPIXEL_R_MASK 0x000000ff
#define LPIXEL_M_SHIFT 24
#define LPIXEL_B_SHIFT 16
#define LPIXEL_G_SHIFT 8
#define LPIXEL_R_SHIFT 0
#else
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0xffffff00
#define LPIXEL_M_MASK 0x000000ff
#define LPIXEL_R_MASK 0x0000ff00
#define LPIXEL_G_MASK 0x00ff0000
#define LPIXEL_B_MASK 0xff000000
#define LPIXEL_M_SHIFT 0
#define LPIXEL_R_SHIFT 8
#define LPIXEL_G_SHIFT 16
#define LPIXEL_B_SHIFT 24
#endif
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)

typedef struct _LPIXEL { unsigned char m, r, g, b; } LPIXEL;
typedef struct _SPIXEL { unsigned short m, r, g, b; } SPIXEL;

#if TNZ_LITTLE_ENDIAN
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0xffffff00
#define LPIXEL_B_MASK 0xff000000
#define LPIXEL_G_MASK 0x00ff0000
#define LPIXEL_R_MASK 0x0000ff00
#define LPIXEL_M_MASK 0x000000ff
#define LPIXEL_B_SHIFT 24
#define LPIXEL_G_SHIFT 16
#define LPIXEL_R_SHIFT 8
#define LPIXEL_M_SHIFT 0
#else
#define LPIXEL_RGBM_MASK 0xffffffff
#define LPIXEL_RGB_MASK 0x00ffffff
#define LPIXEL_B_MASK 0x000000ff
#define LPIXEL_G_MASK 0x0000ff00
#define LPIXEL_R_MASK 0x00ff0000
#define LPIXEL_M_MASK 0xff000000
#define LPIXEL_B_SHIFT 0
#define LPIXEL_G_SHIFT 8
#define LPIXEL_R_SHIFT 16
#define LPIXEL_M_SHIFT 24
#endif
#else
!@ #
#endif

/*---------------------------------------------------------------------------*/

typedef enum {
  PIX_NONE,
  PIX_BW,       /* 1 bit, B=0, W=1, first pixel in bit 7 */
  PIX_WB,       /* "    , W=0, B=1, "                    */
  PIX_GR8,      /* grey tones, 8 bits */
  PIX_CM8,      /* color-mapped, 8 bits */
  PIX_CM16,     /* color-mapped, 16 bits */
  PIX_RGB16,    /* RGB 5+6+5 bits, red most significant */
  PIX_XRGB1555, /* RGB 5+5+5 bits, 1 bit unused most significant(Apple format)
                   */
  PIX_RGB,      /* 1 byte per color channel, red first */
  PIX_RGB_,     /* LPIXEL, matte channel ignored, for display */
  PIX_RGBM,     /* LPIXEL, matte channel considered */
  PIX_MBW16,    /* 16 images of 1 bit per pixel, B=0, W=1 */
  PIX_CM8S4,    /* cmapped,  8 bits, standard SGI  16-color colormap */
  PIX_CM8S8,    /* cmapped,  8 bits, standard SGI 256-color colormap */
  PIX_CM16S4,   /* cmapped, 16 bits, standard SGI  16-color colormap */
  PIX_CM16S8,   /* cmapped, 16 bits, standard SGI 256-color colormap */
  PIX_CM16S12,  /* cmapped, 16 bits, standard SGI+Toonz 4096-color colormap */
  PIX_CM24, /* color-mapped, 8+8+8 bits (pen, col, tone) + 8 msb bits extra*/
  PIX_HOW_MANY
} PIX_TYPE;

/*---------------------------------------------------------------------------*/

/* conversion macros
 */

#define BYTE_FROM_USHORT_MAGICFAC (256U * 255U + 1U)
#define PIX_USHORT_FROM_BYTE(X) ((X) | ((X) << 8))
#define PIX_BYTE_FROM_USHORT(X)                                                \
  ((((X)*BYTE_FROM_USHORT_MAGICFAC) + (1 << 23)) >> 24)

#define PIX_DITHER_BYTE_FROM_USHORT(X, R)                                      \
  (((((X)*BYTE_FROM_USHORT_MAGICFAC) -                                         \
     (((X)*BYTE_FROM_USHORT_MAGICFAC) >> 24)) +                                \
    (R)) >>                                                                    \
   24)

/*---------------------------------------------------------------------------*/

#define PIX_RGB16_FROM_BYTES(R, G, B)                                          \
  (((R) << 8) & 0xf800 | ((G) << 3) & 0x7e0 | ((B) >> 3))
#define PIX_RGB16_FROM_RGBX(L) (PIX_RGB16_FROM_BYTES((L).r, (L).g, (L).b))

#define PIX_R_BYTE_FROM_RGB16(X) ((((X)&0xf800) >> 8) | (((X)&0xf800) >> 13))
#define PIX_G_BYTE_FROM_RGB16(X) ((((X)&0x07e0) >> 3) | (((X)&0x07e0) >> 9))
#define PIX_B_BYTE_FROM_RGB16(X) ((((X)&0x001f) << 3) | (((X)&0x001f) >> 2))

#define PIX_FAST_R_BYTE_FROM_RGB16(X) (((X)&0xf800) >> 8)
#define PIX_FAST_G_BYTE_FROM_RGB16(X) (((X)&0x07e0) >> 3)
#define PIX_FAST_B_BYTE_FROM_RGB16(X) (((X)&0x001f) << 3)

#define PIX_RGB16_TO_RGB_(X, L)                                                \
  {                                                                            \
    (L).r = PIX_R_BYTE_FROM_RGB16(X);                                          \
    (L).g = PIX_G_BYTE_FROM_RGB16(X);                                          \
    (L).b = PIX_B_BYTE_FROM_RGB16(X);                                          \
  }
#define PIX_RGB16_TO_RGBM(X, L)                                                \
  {                                                                            \
    (L).r = PIX_R_BYTE_FROM_RGB16(X);                                          \
    (L).g = PIX_G_BYTE_FROM_RGB16(X);                                          \
    (L).b = PIX_B_BYTE_FROM_RGB16(X);                                          \
    (L).m = 0xff;                                                              \
  }

#define PIX_FAST_RGB16_TO_RGB_(X, L)                                           \
  {                                                                            \
    (L).r = PIX_FAST_R_BYTE_FROM_RGB16(X);                                     \
    (L).g = PIX_FAST_G_BYTE_FROM_RGB16(X);                                     \
    (L).b = PIX_FAST_B_BYTE_FROM_RGB16(X);                                     \
  }
#define PIX_FAST_RGB16_TO_RGBM(X, L)                                           \
  {                                                                            \
    (L).r = PIX_FAST_R_BYTE_FROM_RGB16(X);                                     \
    (L).g = PIX_FAST_G_BYTE_FROM_RGB16(X);                                     \
    (L).b = PIX_FAST_B_BYTE_FROM_RGB16(X);                                     \
    (L).m = 0xff;                                                              \
  }

/*---------------------------------------------------------------------------*/

#define PIX_XRGB1555_FROM_BYTES(R, G, B)                                       \
  (((R) << 7) & 0x7c00 | ((G) << 2) & 0x3e0 | ((B) >> 3))
#define PIX_XRGB1555_FROM_RGBX(L) (PIX_XRGB1555_FROM_BYTES((L).r, (L).g, (L).b))

#define PIX_R_BYTE_FROM_XRGB1555(X) ((((X)&0x7c00) >> 7) | (((X)&0x7c00) >> 12))
#define PIX_G_BYTE_FROM_XRGB1555(X) ((((X)&0x03e0) >> 2) | (((X)&0x03e0) >> 7))
#define PIX_B_BYTE_FROM_XRGB1555(X) ((((X)&0x001f) << 3) | (((X)&0x001f) >> 2))

#define PIX_FAST_R_BYTE_FROM_XRGB1555(X) (((X)&0x7c00) >> 7)
#define PIX_FAST_G_BYTE_FROM_XRGB1555(X) (((X)&0x03e0) >> 2)
#define PIX_FAST_B_BYTE_FROM_XRGB1555(X) (((X)&0x001f) << 3)

#define PIX_XRGB1555_TO_RGB_(X, L)                                             \
  {                                                                            \
    (L).r = PIX_R_BYTE_FROM_XRGB1555(X);                                       \
    (L).g = PIX_G_BYTE_FROM_XRGB1555(X);                                       \
    (L).b = PIX_B_BYTE_FROM_XRGB1555(X);                                       \
  }
#define PIX_XRGB1555_TO_RGBM(X, L)                                             \
  {                                                                            \
    (L).r = PIX_R_BYTE_FROM_XRGB1555(X);                                       \
    (L).g = PIX_G_BYTE_FROM_XRGB1555(X);                                       \
    (L).b = PIX_B_BYTE_FROM_XRGB1555(X);                                       \
    (L).m = 0xff;                                                              \
  }

#define PIX_FAST_XRGB1555_TO_RGB_(X, L)                                        \
  {                                                                            \
    (L).r = PIX_FAST_R_BYTE_FROM_XRGB1555(X);                                  \
    (L).g = PIX_FAST_G_BYTE_FROM_XRGB1555(X);                                  \
    (L).b = PIX_FAST_B_BYTE_FROM_XRGB1555(X);                                  \
  }
#define PIX_FAST_XRGB1555_TO_RGBM(X, L)                                        \
  {                                                                            \
    (L).r = PIX_FAST_R_BYTE_FROM_XRGB1555(X);                                  \
    (L).g = PIX_FAST_G_BYTE_FROM_XRGB1555(X);                                  \
    (L).b = PIX_FAST_B_BYTE_FROM_XRGB1555(X);                                  \
    (L).m = 0xff;                                                              \
  }

/*---------------------------------------------------------------------------*/

#define PIX_RGBM_TO_RGBM64(X, L)                                               \
  {                                                                            \
    (L).r = PIX_USHORT_FROM_BYTE((X).r);                                       \
    (L).g = PIX_USHORT_FROM_BYTE((X).g);                                       \
    (L).b = PIX_USHORT_FROM_BYTE((X).b);                                       \
    (L).m = PIX_USHORT_FROM_BYTE((X).m);                                       \
  }
#define PIX_RGBM64_TO_RGBM(X, L)                                               \
  {                                                                            \
    (L).r = PIX_BYTE_FROM_USHORT((X).r);                                       \
    (L).g = PIX_BYTE_FROM_USHORT((X).g);                                       \
    (L).b = PIX_BYTE_FROM_USHORT((X).b);                                       \
    (L).m = PIX_BYTE_FROM_USHORT((X).m);                                       \
  }

/*****************  WARNING !!!! *******
This conversion uses random generated numbers;
Before you use it for the first time, execute
the macro PIX_INIT_DITHER_RGBM64_TO_RGBM
*****************************************/

#define PIX_INIT_DITHER_RGBM64_TO_RGBM tnz_random_seed(180461);

#define PIX_DITHER_RGBM64_TO_RGBM(X, L)                                        \
  {                                                                            \
    UINT random_round;                                                         \
    random_round = tnz_random_uint() & ((1U << 24) - 1);                       \
                                                                               \
    (L).r = PIX_DITHER_BYTE_FROM_USHORT((X).r, random_round);                  \
    (L).g = PIX_DITHER_BYTE_FROM_USHORT((X).g, random_round);                  \
    (L).b = PIX_DITHER_BYTE_FROM_USHORT((X).b, random_round);                  \
    (L).m = PIX_DITHER_BYTE_FROM_USHORT((X).m, random_round);                  \
  }

/***************************************/

#define PIX_RGB_TO_GREY(X)                                                     \
  (((UINT)((X).r) * 19594 + (UINT)((X).g) * 38472 + (UINT)((X).b) * 7470 +     \
    (UINT)(1 << 15)) >>                                                        \
   16)

#define PIX_CM32_MAP_TO_RGBM(PIX, MAP, RES)                                    \
  {                                                                            \
    TUINT32 pix = (PIX);                                                       \
    int t       = pix & 0xff;                                                  \
    if (t == 255) {                                                            \
      int p = (pix >> 8) & 0xfff;                                              \
      (RES) = (MAP)[p];                                                        \
    } else if (t == 0) {                                                       \
      int i = (pix >> 20) & 0xfff;                                             \
      (RES) = (MAP)[i];                                                        \
    } else {                                                                   \
      LPIXEL ink   = (MAP)[(pix >> 20) & 0xfff];                               \
      LPIXEL paint = (MAP)[(pix >> 8) & 0xfff];                                \
      (RES).r      = (int)(((255 - t) * ink.r + t * paint.r) / 255);           \
      (RES).g      = (int)(((255 - t) * ink.g + t * paint.g) / 255);           \
      (RES).b      = (int)(((255 - t) * ink.b + t * paint.b) / 255);           \
      (RES).m      = (int)(((255 - t) * ink.m + t * paint.m) / 255);           \
    }                                                                          \
  }

/*---------------------------------------------------------------------------*/

#endif
