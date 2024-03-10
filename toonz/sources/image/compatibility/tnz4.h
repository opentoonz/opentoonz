#pragma once

#ifndef TNZ4_H
#define TNZ4_H

#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>

#if defined(MACOSX)
#include <sys/malloc.h>
#endif

#define UCHAR unsigned char
#define USHORT unsigned short

#include "tmachine.h"
#include "tnztypes.h"

#include "tfile_io.h"

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

#define NIL 0

#define FALSE 0
#define TRUE 1
#define TBOOL int

    typedef struct LPIXEL {
#ifdef _WIN32
  unsigned char b, g, r, m;
#elif defined(__sgi)
  unsigned char m, b, g, r;
#elif defined(LINUX) || defined(FREEBSD) || defined(HAIKU)
  unsigned char r, g, b, m;
#elif defined(MACOSX)
  unsigned char m, r, g, b;
#else
#error Not	yet implemented
#endif
} LPIXEL;

typedef struct SPIXEL {
#ifdef _WIN32
  unsigned short b, g, r, m;
#elif defined(__sgi)
  unsigned short m, b, g, r;
#elif defined(LINUX) || defined(FREEBSD) || defined(HAIKU)
  unsigned short r, g, b, m;
#elif defined(MACOSX)
  unsigned char m, r, g, b;
#else
#error Not	yet implemented
#endif
} SPIXEL;

#define UINT TINT32

typedef LPIXEL GL_COLOR;

typedef struct IMAGE {
  int xsize, ysize;
  int xSBsize, ySBsize;
  int type;
  UCHAR *buffer;
  double x_dpi, y_dpi;
} IMAGE;

#define TOONZRGB (1234)
#define TOONZRGB64 (4567)

#define TMALLOC(ptr, elem) (ptr) = (void *)malloc((elem) * sizeof(*(ptr)));
#define TCALLOC(ptr, elem) (ptr) = (void *)calloc((elem), sizeof(*(ptr)));
#define TREALLOC(ptr, elem)                                                    \
  {                                                                            \
    if (ptr)                                                                   \
      (ptr) = (void *)realloc((ptr), (elem) * sizeof(*(ptr)));                 \
    else                                                                       \
      (ptr) = (void *)malloc((elem) * sizeof(*(ptr)));                         \
  }
#define TFREE(ptr)                                                             \
  {                                                                            \
    if (ptr) {                                                                 \
      free(ptr);                                                               \
      ptr = NIL;                                                               \
    }                                                                          \
  }

#define NOT_LESS_THAN(MIN, X)                                                  \
  {                                                                            \
    if ((X) < (MIN)) (X) = (MIN);                                              \
  }
#define NOT_MORE_THAN(MAX, X)                                                  \
  {                                                                            \
    if ((X) > (MAX)) (X) = (MAX);                                              \
  }

#define CROP(X, MIN, MAX) (X < MIN ? MIN : (X > MAX ? MAX : X))

#ifdef _WIN32
#define LPIXEL_TO_BGRM(X) (X)
#else
#if TNZ_LITTLE_ENDIAN
#define LPIXEL_TO_BGRM(X) (((TUINT32)(X) >> 8) | ((TUINT32)(X) << 24))
/*
    #define BGRM_TO_LPIXEL(X)  (((TUINT32)(X)<<8) | ((TUINT32)(X)>>24))
*/
#else
#define LPIXEL_TO_BGRM(X) (((TUINT32)(X) << 8) | ((TUINT32)(X) >> 24))
/*
    #define BGRM_TO_LPIXEL(X)  (((TUINT32)(X)>>8) | ((TUINT32)(X)<<24))
*/
#endif
#endif

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

short swap_short(short s);

TINT32 swap_TINT32(TINT32 s);

USHORT swap_ushort(USHORT x);

/*ULONG swap_uTLong (ULONG x);*/

/*---------------------------------------------------------------------------*/

/*! These functions are implemented in .cpp files, and allocate any buffer
using new operator, so it's safe return img->buffer to .cpp units */

IMAGE *new_img();

void free_img(IMAGE *);

int allocate_pixmap(IMAGE *img, int w, int h);

#endif
