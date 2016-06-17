#pragma once

#ifndef __FILEQUANTELP_H__
#define __FILEQUANTELP_H__

#include "filequantel.h"

#define QNT_PAL_FILE_SIZE 829440
#define QNT_PAL_W_FILE_SIZE 855360
#define QTL_NTSC_FILE_SIZE 699840
#define QTL_NTSC_W_FILE_SIZE 728640

#define QUANTEL_XSIZE 720
#define QNT_PAL_XSIZE QUANTEL_XSIZE
#define QNT_PAL_YSIZE 576
#define QTL_NTSC_XSIZE QUANTEL_XSIZE
#define QTL_NTSC_YSIZE 486
#define BYTESPERROW (QUANTEL_XSIZE * sizeof(short))

#define CHECK_END_OF_FILE(car)                                                 \
  {                                                                            \
    if ((car) == EOF) {                                                        \
      printf("read error: EOF encountered\n");                                 \
      exit = 1;                                                                \
      break;                                                                   \
    }                                                                          \
  }

#define QUANTEL_GET_YUV_LINE(file_ptr, buf, xsize)                             \
  {                                                                            \
    int pixel;                                                                 \
    TINT32 r, g, b, y1, y2, u, v;                                              \
                                                                               \
    for (pixel = ((xsize) >> 1); pixel--;) {                                   \
      u = fgetc(file_ptr);                                                     \
      CHECK_END_OF_FILE(u)                                                     \
      u -= 128;                                                                \
      y1 = fgetc(file_ptr);                                                    \
      CHECK_END_OF_FILE(y1)                                                    \
      y1 -= 16;                                                                \
                                                                               \
      if (y1 < 0) y1 = 0;                                                      \
                                                                               \
      v = fgetc(file_ptr);                                                     \
      CHECK_END_OF_FILE(v)                                                     \
      v -= 128;                                                                \
      y2 = fgetc(file_ptr);                                                    \
      CHECK_END_OF_FILE(y2)                                                    \
      y2 -= 16;                                                                \
                                                                               \
      if (y2 < 0) y2 = 0;                                                      \
                                                                               \
      r                   = 76310 * y1 + 104635 * v;                           \
      if (r > 0xFFFFFF) r = 0xFFFFFF;                                          \
      if (r <= 0xFFFF) r  = 0;                                                 \
                                                                               \
      g                   = 76310 * y1 + -25690 * u + -53294 * v;              \
      if (g > 0xFFFFFF) g = 0xFFFFFF;                                          \
      if (g <= 0xFFFF) g  = 0;                                                 \
                                                                               \
      b                   = 76310 * y1 + 132278 * u;                           \
      if (b > 0xFFFFFF) b = 0xFFFFFF;                                          \
      if (b <= 0xFFFF) b  = 0;                                                 \
                                                                               \
      buf->r = (UCHAR)(r >> 16);                                               \
      buf->g = (UCHAR)(g >> 16);                                               \
      buf->b = (UCHAR)(b >> 16);                                               \
      buf->m = (UCHAR)255;                                                     \
      buf++;                                                                   \
                                                                               \
      r                   = 76310 * y2 + 104635 * v;                           \
      if (r > 0xFFFFFF) r = 0xFFFFFF;                                          \
      if (r <= 0xFFFF) r  = 0;                                                 \
                                                                               \
      g                   = 76310 * y2 + -25690 * u + -53294 * v;              \
      if (g > 0xFFFFFF) g = 0xFFFFFF;                                          \
      if (g <= 0xFFFF) g  = 0;                                                 \
                                                                               \
      b                   = 76310 * y2 + 132278 * u;                           \
      if (b > 0xFFFFFF) b = 0xFFFFFF;                                          \
      if (b <= 0xFFFF) b  = 0;                                                 \
                                                                               \
      buf->r = (UCHAR)(r >> 16);                                               \
      buf->g = (UCHAR)(g >> 16);                                               \
      buf->b = (UCHAR)(b >> 16);                                               \
      buf->m = (UCHAR)255;                                                     \
      buf++;                                                                   \
    }                                                                          \
  }

#define QUANTEL_FILL_LINE_OF_BLACK(rbuf, gbuf, bbuf, size)                     \
  {                                                                            \
    memset(rbuf, 0, (size_t)(size * sizeof(USHORT)));                          \
    memset(gbuf, 0, (size_t)(size * sizeof(USHORT)));                          \
    memset(bbuf, 0, (size_t)(size * sizeof(USHORT)));                          \
  }

#define QUANTEL_FILL_LINE_OF_RGB(xmarg, xsize, rbuf, gbuf, bbuf, RGBbuf)       \
  {                                                                            \
    int i;                                                                     \
    QUANTEL_FILL_LINE_OF_BLACK(rbuf, gbuf, bbuf, xmarg)                        \
    for (i = xmarg; i < xsize + xmarg; i++) {                                  \
      rbuf[i] = (USHORT)RGBbuf->r;                                             \
      gbuf[i] = (USHORT)RGBbuf->g;                                             \
      bbuf[i] = (USHORT)RGBbuf->b;                                             \
      RGBbuf++;                                                                \
    }                                                                          \
    QUANTEL_FILL_LINE_OF_BLACK(rbuf + i, gbuf + i, bbuf + i, xmarg)            \
  }

#define QUANTEL_FILL_LINE_OF_RGB2(xmarg, rbuf, gbuf, bbuf, RGBbuf)             \
  {                                                                            \
    int i;                                                                     \
    QUANTEL_FILL_LINE_OF_BLACK(rbuf, gbuf, bbuf, xmarg)                        \
    for (i = xmarg; i < (QUANTEL_XSIZE - xmarg); i++) {                        \
      rbuf[i] = (USHORT)RGBbuf->r;                                             \
      gbuf[i] = (USHORT)RGBbuf->g;                                             \
      bbuf[i] = (USHORT)RGBbuf->b;                                             \
      RGBbuf++;                                                                \
    }                                                                          \
    QUANTEL_FILL_LINE_OF_BLACK(rbuf + i, gbuf + i, bbuf + i, xmarg)            \
  }

#define QUANTEL_LIMIT(r, x)                                                    \
  {                                                                            \
    r                      = x;                                                \
    if (r > 0x00ffffff) r  = 0x00ffffff;                                       \
    if (r <= 0x00000000) r = 0;                                                \
  }

#endif /* __FILEQUANTELP_H__ */
