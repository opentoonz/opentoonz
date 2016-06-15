

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "toonz.h"
#include "tmsg.h"
#include "file.h"
#include "img.h"
#include "tmachine.h"
#include "../infoRegionP.h"
#include "ImageP/filetzr.h"

#if !defined(TNZ_LITTLE_ENDIAN)
  TNZ_LITTLE_ENDIAN undefined!!
#endif

#if TNZ_LITTLE_ENDIAN
#define TZR_MAGIC ('T' | 'Z' << 8 | 'R' << 16 | 'A' << 24)
#else
#define TZR_MAGIC ('T' << 24 | 'Z' << 16 | 'R' << 8 | 'A')
#endif

#ifdef NOTES

numbers are always big endian (most significant byte first)
floats and doubles are IEEE, sign and exponent first (1+8+23 and 1+11+52)

4 bytes: 'T' 'Z' 'R' 'A' (0x545a5241 big endian or 0x41525a54 little endian)
1 byte: type of file (0 is not valid)
1 byte: ST1 (subtype, must be 0 if not used)
1 byte: ST2 (")
1 byte: ST3 (")
(types form a tree, numbers should increase chronologically)

type 1: fast
  ST1 1: lineart CM (1 ink, 2 colors (transparent and opaque), 4 bits ramp)
    ST2 1: RLE compression (see image section)

    3 bytes: image lx
    3 bytes: image ly
    3 bytes: image offset from start of file
    tagged fields (fill up the space before the image):
      1 byte field tag
      if (tag >= 128)
        2 bytes field length(=n)
	n bytes field value
      else
	1 byte field length(=n)
	n bytes field value
    image:
      rle encoded 1 line at a time (to allow for subsampling)
      rle encoding of 1 line:
        2 or 3 bytes: length in bytes of encoded line
	(2 if image lx <= 8*1024, 3 otherwise)
        1 nibble at a time, most significative nibble first:
        if (0x0 <= nibble[0] <= 0xE)
	  nibble[0] is the tone value (0 is black)
        if (nibble[0] == 0xF)
          if (0x0 <= nibble[1] <= 0x7)
            nibble[1] is the count of nibbles which follow (may be zero)
            nibble[2...] number of white pixels in a run - 1, big endian
          if (nibble[1] == 0x8)
            toggle between transparent (0) and opaque (1) (starts with 0)
          if (nibble[1] == 0xF)
            if (nibble[2] == 0xE)
              end of encoding
            if (nibble[2] == 0xF)
              nibble[3] is last pixel tone value, end of encoding
        if line ends at odd nibble, a 0x0 nibble is appended


32:

        1 nibble at a time, most significative nibble first:
	
        if (0x0 <= nibble[1]|nibble[0] <= 0xFE)
	  nibble[1]|nibble[0] is the tone value (0 is black)
        if (nibble[1]|nibble[0] == 0xFF)
          if (0x0 <= nibble[2] <= 0x7)
            nibble[2] is the count of nibbles which follow (may be zero)
            nibble[3...] number of white pixels in a run - 1, big endian
          if (nibble[2] == 0x8)
            toggle between transparent (0) and opaque (1) (starts with 0)
          if (nibble[2] == 0xF)
            if (nibble[3] == 0xE)
              end of encoding
            if (nibble[3] == 0xF)
              nibble[4]|nibble[5] is last pixel tone value, end of encoding
        if line ends at odd nibble, a 0x0 nibble is appended

#endif

typedef enum
  {
  TAG_NONE = 0,

  TAG_DPI      = 1, /* size 16: double[2] (x_dpi, y_dpi) */
      TAG_HPOS = 2, /* size  8: double */

      TAG_HOW_MANY = 256
  }
  TAG_TYPE;

#define TAGS_BUFLEN 65536

#define READ_BYTES_OR_ERROR(buf, n, file)                                      \
  {                                                                            \
    if (fread((buf), 1, (n), (file)) < (n)) goto error;                        \
  }

#define BYTES_TO_FLOAT(b, f)                                                   \
  {                                                                            \
    UCHAR *fr = (b);                                                           \
    UCHAR *to = (UCHAR *)&(f);                                                 \
    if (TNZ_LITTLE_ENDIAN) {                                                   \
      to[0] = fr[3];                                                           \
      to[1] = fr[2];                                                           \
      to[2] = fr[1];                                                           \
      to[3] = fr[0];                                                           \
    } else {                                                                   \
      to[0] = fr[0];                                                           \
      to[1] = fr[1];                                                           \
      to[2] = fr[2];                                                           \
      to[3] = fr[3];                                                           \
    }                                                                          \
  }

#define FLOAT_TO_BYTES(f, b)                                                   \
  {                                                                            \
    UCHAR *fr = (UCHAR *)&(f);                                                 \
    UCHAR *to = (b);                                                           \
    if (TNZ_LITTLE_ENDIAN) {                                                   \
      to[0] = fr[3];                                                           \
      to[1] = fr[2];                                                           \
      to[2] = fr[1];                                                           \
      to[3] = fr[0];                                                           \
    } else {                                                                   \
      to[0] = fr[0];                                                           \
      to[1] = fr[1];                                                           \
      to[2] = fr[2];                                                           \
      to[3] = fr[3];                                                           \
    }                                                                          \
  }

#define BYTES_TO_DOUBLE(b, d)                                                  \
  {                                                                            \
    UCHAR *fr = (b);                                                           \
    UCHAR *to = (UCHAR *)&(d);                                                 \
    if (TNZ_LITTLE_ENDIAN) {                                                   \
      to[0] = fr[7];                                                           \
      to[1] = fr[6];                                                           \
      to[2] = fr[5];                                                           \
      to[3] = fr[4];                                                           \
      to[4] = fr[3];                                                           \
      to[5] = fr[2];                                                           \
      to[6] = fr[1];                                                           \
      to[7] = fr[0];                                                           \
    } else {                                                                   \
      to[0] = fr[0];                                                           \
      to[1] = fr[1];                                                           \
      to[2] = fr[2];                                                           \
      to[3] = fr[3];                                                           \
      to[4] = fr[4];                                                           \
      to[5] = fr[5];                                                           \
      to[6] = fr[6];                                                           \
      to[7] = fr[7];                                                           \
    }                                                                          \
  }

#define DOUBLE_TO_BYTES(d, b)                                                  \
  {                                                                            \
    UCHAR *fr = (UCHAR *)&(d);                                                 \
    UCHAR *to = (b);                                                           \
    if (TNZ_LITTLE_ENDIAN) {                                                   \
      to[0] = fr[7];                                                           \
      to[1] = fr[6];                                                           \
      to[2] = fr[5];                                                           \
      to[3] = fr[4];                                                           \
      to[4] = fr[3];                                                           \
      to[5] = fr[2];                                                           \
      to[6] = fr[1];                                                           \
      to[7] = fr[0];                                                           \
    } else {                                                                   \
      to[0] = fr[0];                                                           \
      to[1] = fr[1];                                                           \
      to[2] = fr[2];                                                           \
      to[3] = fr[3];                                                           \
      to[4] = fr[4];                                                           \
      to[5] = fr[5];                                                           \
      to[6] = fr[6];                                                           \
      to[7] = fr[7];                                                           \
    }                                                                          \
  }

static void *Tzr_buffer;
static int   Tzr_buffer_bytes;

static int Next_img_read_tzr_cmapped = FALSE;
static int Read_cmapped = FALSE;
#define SET_READ_CMAPPED                                                       \
  {                                                                            \
    Read_cmapped              = Next_img_read_tzr_cmapped;                     \
    Next_img_read_tzr_cmapped = FALSE;                                         \
  }

/*---------------------------------------------------------------------------*/

int tzr_safe_bytes_for_1_1_1_pixels (int n_pix)
{
  /* un colore con cambio colore, un colore con cambio colore */
  /* = 2 + 2 + 2 + 2 nibble = 4 bytes per 2 pixel */

  return 2 * n_pix + 2;
}

/*---------------------------------------------------------------------------*/

int tzr_safe_bytes_for_2_1_1_pixels (int n_pix)
{
  /* un colore con cambio colore, un colore con cambio colore */
  /* = 2 + 4 + 2 + 4 nibble = 6 bytes per 2 pixel */

  return 4 * n_pix + 2;
}

  /*---------------------------------------------------------------------------*/

#define GET_INVAL                                                              \
  {                                                                            \
    inval = *in++;                                                             \
    remain--;                                                                  \
  }
#define PUT_OUTVAL                                                             \
  { *out++ = (UCHAR)outval; }

/*---------------------------------------------------------------------------*/

int tzr_encode_cm16_1_1_1 (USHORT *buf_in, int buf_in_len,
                           UCHAR  *buf_out)
{
  int count, prevremain, remain;
  UINT inval, outval;
  UINT lastval, lastcol__;
  UINT colmask, maxtone;
  UINT incol__, tone;
  USHORT *in, save;
  UCHAR *out;

  maxtone = 0xF;
  colmask = 0x07F0;

  remain    = buf_in_len;
  lastval   = maxtone;
  lastcol__ = 0;

  in  = buf_in;
  out = buf_out;

  if (buf_in_len <= 1) {
    if (buf_in_len < 0) return 0;
    if (buf_in_len == 0) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = 0xE0;
      PUT_OUTVAL
    } else {
      GET_INVAL
      if (inval & colmask) {
        outval = 0xF8;
        PUT_OUTVAL
      }
      outval = 0xFF;
      PUT_OUTVAL
      outval = 0xF0 | inval & 0xF;
      PUT_OUTVAL
    }
    return out - buf_out;
  }

  save                   = buf_in[buf_in_len - 1];
  buf_in[buf_in_len - 1] = buf_in[buf_in_len - 2] ^ colmask;

  GET_INVAL

check_colpen_on_out0:
  incol__              = inval & colmask;
  if (incol__) incol__ = 0x10;
  if (incol__ == lastcol__) goto check_tone_on_out0;
  lastcol__ = incol__;
  if (!remain) goto end_toonz1_encoding_on_out0;

  outval = (0xF << 4) | 0x8;
  PUT_OUTVAL

check_tone_on_out0:
  tone = inval & maxtone;
  if (tone == maxtone) {
    lastval    = inval;
    prevremain = remain;
    do
      GET_INVAL
    while (inval == lastval);
    count = prevremain - remain - 1;
    if (count <= 0xF)
      if (count == 0) {
        outval = (0xF << 4) | 0x0;
        PUT_OUTVAL
        goto check_colpen_on_out0;
      } else {
        outval = (0xF << 4) | 0x1;
        PUT_OUTVAL
        outval = count << 4;
        goto check_colpen_on_out1;
      }
    else if (count <= 0xFF) {
      outval = (0xF << 4) | 0x2;
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFF) {
      outval = (0xF << 4) | 0x3;
      PUT_OUTVAL
      outval = count >> 4;
      PUT_OUTVAL
      outval = count << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFF) {
      outval = (0xF << 4) | 0x4;
      PUT_OUTVAL
      outval = count >> 8;
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFFF) {
      outval = (0xF << 4) | 0x5;
      PUT_OUTVAL
      outval = count >> 12;
      PUT_OUTVAL
      outval = count >> 4;
      PUT_OUTVAL
      outval = count << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFFFF) {
      outval = (0xF << 4) | 0x6;
      PUT_OUTVAL
      outval = count >> 16;
      PUT_OUTVAL
      outval = count >> 8;
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFFFFF) {
      outval = (0xF << 4) | 0x7;
      PUT_OUTVAL
      outval = count >> 20;
      PUT_OUTVAL
      outval = count >> 12;
      PUT_OUTVAL
      outval = count >> 4;
      PUT_OUTVAL
      outval = count << 4;
      goto check_colpen_on_out1;
    } else {
      buf_in[buf_in_len - 1] = save;
      return 0;
    }
  } else {
    outval = tone << 4;
    GET_INVAL
    /*goto check_colpen_on_out1;*/
  }

check_colpen_on_out1:
  incol__              = inval & colmask;
  if (incol__) incol__ = 0x10;
  if (incol__ == lastcol__) goto check_tone_on_out1;
  lastcol__ = incol__;
  if (!remain) goto end_toonz1_encoding_on_out1;

  outval |= 0xF;
  PUT_OUTVAL
  outval = 0x8 << 4;

check_tone_on_out1:
  tone = inval & maxtone;
  if (tone == maxtone) {
    lastval    = inval;
    prevremain = remain;
    do
      GET_INVAL
    while (inval == lastval);
    count = prevremain - remain - 1;
    outval |= 0xF;
    PUT_OUTVAL
    if (count <= 0xF)
      if (count == 0) {
        outval = 0x0 << 4;
        goto check_colpen_on_out1;
      } else {
        outval = (0x1 << 4) | count;
        PUT_OUTVAL
        goto check_colpen_on_out0;
      }
    else if (count <= 0xFF) {
      outval = (0x2 << 4) | (count >> 4);
      PUT_OUTVAL
      outval = count << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFF) {
      outval = (0x3 << 4) | (count >> 8);
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFF) {
      outval = (0x4 << 4) | (count >> 12);
      PUT_OUTVAL
      outval = count >> 4;
      PUT_OUTVAL
      outval = count << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFFF) {
      outval = (0x5 << 4) | (count >> 16);
      PUT_OUTVAL
      outval = count >> 8;
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFFFF) {
      outval = (0x6 << 4) | (count >> 20);
      PUT_OUTVAL
      outval = count >> 12;
      PUT_OUTVAL
      outval = count >> 4;
      PUT_OUTVAL
      outval = count << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFFFFF) {
      outval = (0x7 << 4) | (count >> 24);
      PUT_OUTVAL
      outval = count >> 16;
      PUT_OUTVAL
      outval = count >> 8;
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else {
      buf_in[buf_in_len - 1] = save;
      return 0;
    }
  } else {
    outval |= tone;
    PUT_OUTVAL
    GET_INVAL
    goto check_colpen_on_out0;
  }

end_toonz1_encoding_on_out0:
  if ((save & colmask) != (in[-2] & colmask)) {
    outval = 0xF8;
    PUT_OUTVAL
  }
  outval = 0xFF;
  PUT_OUTVAL
  outval = 0xF0 | save & 0xF;
  PUT_OUTVAL
  buf_in[buf_in_len - 1] = save;
  return out - buf_out;

end_toonz1_encoding_on_out1:
  if ((save & colmask) != (in[-2] & colmask)) {
    outval |= 0xF;
    PUT_OUTVAL
    outval = 0x80;
  }
  outval |= 0xF;
  PUT_OUTVAL
  outval = 0xFF;
  PUT_OUTVAL
  outval = (save & 0xF) << 4;
  PUT_OUTVAL
  buf_in[buf_in_len - 1] = save;
  return out - buf_out;
}

/*---------------------------------------------------------------------------*/

int tzr_encode_cm24_2_1_1 (ULONG *buf_in, int buf_in_len,
                           UCHAR  *buf_out)
{
  int count, prevremain, remain;
  UINT inval, outval;
  UINT lastval, lastcol__;
  UINT colmask, maxtone;
  UINT incol__, tone;
  ULONG *in, save;
  UCHAR *out;

  maxtone = 0xFF;
  colmask = 0x0FF00;

  remain    = buf_in_len;
  lastval   = maxtone;
  lastcol__ = 0;

  in  = buf_in;
  out = buf_out;

  if (buf_in_len <= 1) {
    if (buf_in_len < 0) return 0;
    if (buf_in_len == 0) {
      outval = 0xFFFF;
      PUT_OUTVAL
      outval = 0xFE;
      PUT_OUTVAL
    } else {
      GET_INVAL
      if (inval & colmask) {
        outval = 0xF8;
        PUT_OUTVAL
      }
      outval = 0xFF;
      PUT_OUTVAL
      if ((inval & 0xF) == 0xF) inval--;
      outval = 0xF0 | inval & 0xF;
      PUT_OUTVAL
      outval = (inval & 0xF0) >> 4;
    }
    return out - buf_out;
  }

  save                   = buf_in[buf_in_len - 1];
  buf_in[buf_in_len - 1] = buf_in[buf_in_len - 2] ^ colmask;

  GET_INVAL

check_colpen_on_out0:
  incol__              = inval & colmask;
  if (incol__) incol__ = 0x100;
  if (incol__ == lastcol__) goto check_tone_on_out0;
  lastcol__ = incol__;
  if (!remain) goto end_toonz1_encoding_and_save_on_out0;

  outval = 0xFF;
  PUT_OUTVAL
  outval = 0x8 << 4;
  goto check_tone_on_out1;

check_tone_on_out0:
  tone = inval & maxtone;
  if (tone == maxtone) {
    lastval    = inval;
    prevremain = remain;
    do
      GET_INVAL
    while (inval == lastval);
    count = prevremain - remain - 1;
    if (count <= 0xF)
      if (count == 0) {
        outval = 0xFF;
        PUT_OUTVAL
        outval = 0x0 << 4;
        goto check_colpen_on_out1;
      } else {
        outval = 0xFF;
        PUT_OUTVAL
        outval = (0x1 << 4) | count;
        PUT_OUTVAL
        goto check_colpen_on_out0;
      }
    else if (count <= 0xFF) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = (0x2 << 4) | (count >> 4);
      PUT_OUTVAL
      outval = (count & 0xF) << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFF) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = (0x3 << 4) | (count >> 8);
      PUT_OUTVAL
      outval = count & 0xFF;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFF) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = (0x4 << 4) | (count >> 12);
      PUT_OUTVAL
      outval = (count & 0xFF0) >> 4;
      PUT_OUTVAL
      outval = (count & 0xF) << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFFF) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = (0x5 << 4) | (count >> 16);
      PUT_OUTVAL
      outval = (count & 0xFF0) >> 4;
      PUT_OUTVAL
      outval = (count & 0xFF000) >> 12;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFFFF) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = (0x6 << 4) | (count >> 20);
      PUT_OUTVAL
      outval = (count & 0xFF0) >> 4;
      PUT_OUTVAL
      outval = (count & 0xFF000) >> 12;
      PUT_OUTVAL
      outval = (count & 0xF00000) >> 16;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFFFFF) {
      outval = 0xFF;
      PUT_OUTVAL
      outval = (0x7 << 4) | (count >> 24);
      PUT_OUTVAL
      outval = (count & 0xFF0) >> 4;
      PUT_OUTVAL
      outval = (count & 0xFF000) >> 12;
      PUT_OUTVAL
      outval = (count & 0xFF00000) >> 20;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else {
      buf_in[buf_in_len - 1] = save;
      return 0;
    }
  } else {
    /*if ((tone & 0xF)==0xF) tone--;*/
    outval = (((tone & 0xF) << 4) | ((tone & 0xF0)) >> 4);
    PUT_OUTVAL
    GET_INVAL
    goto check_colpen_on_out0;
  }

check_colpen_on_out1:
  incol__              = inval & colmask;
  if (incol__) incol__ = 0x100;
  if (incol__ == lastcol__) goto check_tone_on_out1;
  lastcol__ = incol__;
  if (!remain) goto end_toonz1_encoding_and_save_on_out1;

  outval |= 0xF;
  PUT_OUTVAL
  outval = (0xF << 4) | 0x8;
  PUT_OUTVAL
  goto check_tone_on_out0;

check_tone_on_out1:
  tone = inval & maxtone;
  if (tone == maxtone) {
    lastval    = inval;
    prevremain = remain;
    do
      GET_INVAL
    while (inval == lastval);
    count = prevremain - remain - 1;
    outval |= 0xF;
    PUT_OUTVAL
    outval = 0xF << 4;
    if (count <= 0xF)
      if (count == 0) {
        outval |= 0x0;
        PUT_OUTVAL
        goto check_colpen_on_out0;
      } else {
        outval |= 0x1;
        PUT_OUTVAL
        outval = count << 4;
        goto check_colpen_on_out1;
      }
    else if (count <= 0xFF) {
      outval |= 0x2;
      PUT_OUTVAL
      outval = count;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFF) {
      outval |= 0x3;
      PUT_OUTVAL
      outval = count >> 4;
      PUT_OUTVAL
      outval = (count & 0XF) << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFF) {
      outval |= 0x4;
      PUT_OUTVAL
      outval = count >> 8;
      PUT_OUTVAL
      outval = count & 0XFF;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFFF) {
      outval |= 0x5;
      PUT_OUTVAL
      outval = count >> 12;
      PUT_OUTVAL
      outval = (count & 0XFF0) >> 4;
      PUT_OUTVAL
      outval = (count & 0XF) << 4;
      goto check_colpen_on_out1;
    } else if (count <= 0xFFFFFF) {
      outval |= 0x6;
      PUT_OUTVAL
      outval = count >> 16;
      PUT_OUTVAL
      outval = (count & 0XFF00) >> 8;
      PUT_OUTVAL
      outval = count & 0XFF;
      PUT_OUTVAL
      goto check_colpen_on_out0;
    } else if (count <= 0xFFFFFFF) {
      outval |= 0x7;
      PUT_OUTVAL
      outval = count >> 20;
      PUT_OUTVAL
      outval = (count & 0XFF000) >> 12;
      PUT_OUTVAL
      outval = (count & 0XFF0) >> 4;
      PUT_OUTVAL
      outval = (count & 0XF) << 4;
      goto check_colpen_on_out1;
    } else {
      buf_in[buf_in_len - 1] = save;
      return 0;
    }
  } else {
    /*if ((tone & 0xF)==0xF) tone--;*/
    outval |= tone & 0xF;
    PUT_OUTVAL
    outval = (tone & 0xF0);
    /*PUT_OUTVAL*/
    GET_INVAL
    goto check_colpen_on_out1;
  }

end_toonz1_encoding_and_save_on_out0:
  if ((save & colmask) != (in[-2] & colmask)) {
    outval = 0xFF;
    PUT_OUTVAL
    outval = 0x8 << 4;
    goto end_toonz1_encoding_on_out1;
  }

end_toonz1_encoding_on_out0:

  outval = 0xFF;
  PUT_OUTVAL
  outval = 0xFF;
  PUT_OUTVAL
  outval = (save & 0xF) << 4;
  outval |= (save & 0xF0) >> 4;
  PUT_OUTVAL
  buf_in[buf_in_len - 1] = save;
  return out - buf_out;

end_toonz1_encoding_and_save_on_out1:
  if ((save & colmask) != (in[-2] & colmask)) {
    outval |= 0xF;
    PUT_OUTVAL
    outval = (0xF << 4) | 0x80;
    PUT_OUTVAL
    goto end_toonz1_encoding_on_out0;
  }
end_toonz1_encoding_on_out1:

  outval |= 0xF;
  PUT_OUTVAL
  outval = 0xFF;
  PUT_OUTVAL
  outval = (0xF << 4) | (save & 0xF);
  PUT_OUTVAL
  outval = (save & 0xF0);
  PUT_OUTVAL
  buf_in[buf_in_len - 1] = save;
  return out - buf_out;

}

/*---------------------------------------------------------------------------*/

static TBOOL write_tzr_1_1_1 (char *filename, FILE *file, IMAGE *image)
{
  int lx, ly, wrap, safe_bytes, enc, enclen_bytes, bytes, y;
  USHORT *buffer, *line;
  UCHAR *tzr_buf;

  lx           = image->pixmap.xsize;
  ly           = image->pixmap.ysize;
  wrap         = lx;
  enclen_bytes = 2 + (lx > 8 * 1024);
  safe_bytes   = enclen_bytes + tzr_safe_bytes_for_1_1_1_pixels(lx);
  tzr_buf      = Tzr_buffer;
  if (safe_bytes > Tzr_buffer_bytes) {
    TREALLOC(tzr_buf, safe_bytes)
    Tzr_buffer = tzr_buf;
    if (Tzr_buffer)
      Tzr_buffer_bytes = safe_bytes;
    else {
      Tzr_buffer_bytes = 0;
      return FALSE;
    }
  }
  buffer = image->pixmap.buffer;
  for (line = buffer, y = 0; y < ly; line += wrap, y++) {
    enc = tzr_encode_cm16_1_1_1(line, lx, tzr_buf + enclen_bytes);
    if (enclen_bytes == 2) {
      tzr_buf[0] = (enc >> 8) & 0xFF;
      tzr_buf[1] = enc & 0xFF;
    } else {
      tzr_buf[0] = (enc >> 16) & 0xFF;
      tzr_buf[1] = (enc >> 8) & 0xFF;
      tzr_buf[2] = enc & 0xFF;
    }
    bytes = enclen_bytes + enc;
    if (fwrite(tzr_buf, 1, bytes, file) != bytes) return FALSE;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static TBOOL write_tzr_2_1_1 (char *filename, FILE *file, IMAGE *image)
{
  int lx, ly, wrap, safe_bytes, enc, enclen_bytes, bytes, y;
  ULONG *buffer, *line;
  UCHAR *tzr_buf;

  lx           = image->pixmap.xsize;
  ly           = image->pixmap.ysize;
  wrap         = lx;
  enclen_bytes = 2 + (lx > 8 * 1024);
  safe_bytes   = enclen_bytes + tzr_safe_bytes_for_2_1_1_pixels(lx);
  tzr_buf      = Tzr_buffer;
  if (safe_bytes > Tzr_buffer_bytes) {
    TREALLOC(tzr_buf, safe_bytes)
    Tzr_buffer = tzr_buf;
    if (Tzr_buffer)
      Tzr_buffer_bytes = safe_bytes;
    else {
      Tzr_buffer_bytes = 0;
      return FALSE;
    }
  }
  buffer = (ULONG *)image->pixmap.buffer;
  for (line = buffer, y = 0; y < ly; line += wrap, y++) {
    int i;
    enc = tzr_encode_cm24_2_1_1(line, lx, tzr_buf + enclen_bytes);
    /*for (i=0; i<enc; i+=4)
printf("*** %x %x %x %x\n", tzr_buf[enclen_bytes+i],
                    tzr_buf[enclen_bytes+i+1],
                    tzr_buf[enclen_bytes+i+2],
                    tzr_buf[enclen_bytes+i+3]); */
    if (enclen_bytes == 2) {
      tzr_buf[0] = (enc >> 8) & 0xFF;
      tzr_buf[1] = enc & 0xFF;
    } else {
      tzr_buf[0] = (enc >> 16) & 0xFF;
      tzr_buf[1] = (enc >> 8) & 0xFF;
      tzr_buf[2] = enc & 0xFF;
    }
    bytes = enclen_bytes + enc;
    if (fwrite(tzr_buf, 1, bytes, file) != bytes) return FALSE;
  }
  return TRUE;
}

/*---------------------------------------------------------------------------*/

static TBOOL write_tzr_header (char *filename, FILE *file, IMAGE *image,
                               UINT tzr_type)
{
  UCHAR buf[100 + TAGS_BUFLEN];
  ULONG *buf32;
  int lx, ly, img_offs = 0, img_offs_offs = 0, pos = 0;
  double x_dpi, y_dpi, h_pos;

  switch (tzr_type) {
    CASE 0x01010100 : __OR 0x02010100 : lx = image->pixmap.xsize;
    ly                                     = image->pixmap.ysize;
    buf32                                  = (ULONG *)buf;
    *buf32                                 = TZR_MAGIC;
    buf[4]                                 = (tzr_type >> 24) & 0xFF;
    buf[5]                                 = (tzr_type >> 16) & 0xFF;
    buf[6]                                 = (tzr_type >> 8) & 0xFF;
    buf[7]                                 = tzr_type & 0xFF;
    buf[8]                                 = (lx >> 16) & 0xFF;
    buf[9]                                 = (lx >> 8) & 0xFF;
    buf[10]                                = lx & 0xFF;
    buf[11]                                = (ly >> 16) & 0xFF;
    buf[12]                                = (ly >> 8) & 0xFF;
    buf[13]                                = ly & 0xFF;
    img_offs_offs                          = 14;
    pos                                    = img_offs_offs + 3;

  DEFAULT:
    assert(FALSE);
  }
  if (image->pixmap.x_dpi && image->pixmap.y_dpi) {
    buf[pos]     = TAG_DPI;
    buf[pos + 1] = 8 + 8;
    x_dpi        = image->pixmap.x_dpi;
    y_dpi        = image->pixmap.y_dpi;
    DOUBLE_TO_BYTES(x_dpi, buf + pos + 2)
    DOUBLE_TO_BYTES(y_dpi, buf + pos + 10)
    pos += 1 + 1 + 8 + 8;
  }
  if (image->pixmap.h_pos) {
    buf[pos]     = TAG_HPOS;
    buf[pos + 1] = 8;
    h_pos        = image->pixmap.h_pos;
    DOUBLE_TO_BYTES(h_pos, buf + pos + 2)
    pos += 1 + 1 + 8;
  }
  img_offs               = pos;
  buf[img_offs_offs]     = (img_offs >> 16) & 0xFF;
  buf[img_offs_offs + 1] = (img_offs >> 8) & 0xFF;
  buf[img_offs_offs + 2] = img_offs & 0xFF;
  if (fwrite(buf, 1, img_offs, file) != img_offs) return FALSE;

  return TRUE;
}

/*---------------------------------------------------------------------------*/

TBOOL img_write_tzr (char *filename, IMAGE *image)
{
  FILE *file;
  UCHAR buf[256];
  UINT tzr_type;

  file = fopen(filename, "wb");
  if (!file) return FALSE;
  switch (image->type) {
    CASE CMAPPED : tzr_type   = 0x01010100;
    CASE CMAPPED24 : tzr_type = 0x02010100;
  DEFAULT:
    tmsg_error("bad image type for .tzr files");
    goto error;
  }
  if (!write_tzr_header(filename, file, image, tzr_type)) goto error;
  switch (tzr_type) {
    CASE 0x01010100 : if (!write_tzr_1_1_1(filename, file, image)) goto error;
    CASE 0x02010100 : if (!write_tzr_2_1_1(filename, file, image)) goto error;
  DEFAULT:
    goto error;
  }
  fclose(file);
  return TRUE;

error:
  if (file) fclose(file);
  return FALSE;
}

/*---------------------------------------------------------------------------*/

int tzr_decode_1_1_1_cm16 (UCHAR  *buf_in, int *buf_in_len,
                           USHORT *buf_out)
{
  UCHAR *in;
  USHORT *out;
  int count;
  UINT inval, in0, in1;
  UINT outval, maxtone, outval_maxtone;

#define GET_IN0 (inval = *in++, in1 = inval & 0xF, in0 = inval >> 4)

  maxtone        = 0xF;
  outval         = 0;
  outval_maxtone = outval | maxtone;
  count          = 0;

  in  = buf_in;
  out = buf_out;

  goto start_from_in0;

count_out_and_start_from_in0:
  outval_maxtone         = outval | maxtone;
  *out++                 = outval_maxtone;
  while (count--) *out++ = outval_maxtone;

start_from_in0:
  if (GET_IN0 == 0xF) {
    switch (in1) {
      CASE 0x0 : *out++ = outval | maxtone;
      goto start_from_in0;

      CASE 0x1 : count = GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x2 : count = *in++;
      goto count_out_and_start_from_in0;

      CASE 0x3 : count = *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x4 : count = *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x5 : count = *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x6 : count = *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x7 : count = *in++ << 20;
      count += *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x8 : outval ^= 0x10;
      goto start_from_in0;

      CASE 0xF : switch (GET_IN0) {
        CASE 0xE : CASE 0xF : *out++ = outval | in1;
      DEFAULT:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

    DEFAULT:
      goto rle_decoding_error;
    }
  } else {
    *out++ = outval | in0;
    goto start_from_in1;
  }

count_out_and_start_from_in1:
  outval_maxtone         = outval | maxtone;
  *out++                 = outval_maxtone;
  while (count--) *out++ = outval_maxtone;

start_from_in1:
  if (in1 == 0xF) {
    switch (GET_IN0) {
      CASE 0x0 : *out++ = outval | maxtone;
      goto start_from_in1;

      CASE 0x1 : count = in1;
      goto count_out_and_start_from_in0;

      CASE 0x2 : count = in1 << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x3 : count = in1 << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x4 : count = in1 << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x5 : count = in1 << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x6 : count = in1 << 20;
      count += *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x7 : count = in1 << 24;
      count += *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x8 : outval ^= 0x10;
      goto start_from_in1;

      CASE 0xF : switch (in1) {
        CASE 0xE : CASE 0xF : *out++ = outval | *in++ >> 4;
      DEFAULT:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

    DEFAULT:
      return 0;
    }
  } else {
    *out++ = outval | in1;
    goto start_from_in0;
  }

end_rle_decoding:
  if (buf_in_len) *buf_in_len = in - buf_in;
  return out - buf_out;

rle_decoding_error:
  if (buf_in_len) *buf_in_len = 0;
  return 0;
}

/*---------------------------------------------------------------------------*/

int tzr_decode_2_1_1_cm24 (UCHAR  *buf_in, int *buf_in_len,
                           ULONG *buf_out)
{
  UCHAR *in;
  ULONG *out;
  int count;
  UINT inval, in0, in1, aux;
  UINT outval, maxtone, outval_maxtone;

#define GET_IN0 (inval = *in++, in1 = inval & 0xF, in0 = inval >> 4)

  maxtone        = 0xFF;
  outval         = 0;
  outval_maxtone = outval | maxtone;
  count          = 0;

  in  = buf_in;
  out = buf_out;

  goto start_from_in0;

count_out_and_start_from_in0:
  outval_maxtone         = outval | maxtone;
  *out++                 = outval_maxtone;
  while (count--) *out++ = outval_maxtone;
start_from_in0:
  GET_IN0;
  if (inval == 0xFF) {
    GET_IN0;
    switch (in0) {
      CASE 0x0 : *out++ = outval | maxtone;
      goto start_from_in1;

      CASE 0x1 : count = in1;
      goto count_out_and_start_from_in0;

      CASE 0x2 : count = in1 << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x3 : count = in1 << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x4 : count = in1 << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x5 : count = in1 << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x6 : count = in1 << 20;
      count += *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x7 : count = in1 << 24;
      count += *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x8 : outval ^= 0x100;
      goto start_from_in1;

      CASE 0xF : switch (in1) {
        CASE 0xE : CASE 0xF : GET_IN0;
        *out++ = outval | ((in1 << 4) | in0);
      DEFAULT:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

    DEFAULT:
      goto rle_decoding_error;
    }
  } else {
    *out++ = outval | ((in1 << 4) | in0);
    goto start_from_in0;
  }

count_out_and_start_from_in1:
  outval_maxtone         = outval | maxtone;
  *out++                 = outval_maxtone;
  while (count--) *out++ = outval_maxtone;

start_from_in1:
  aux = in1;
  GET_IN0;
  if (in0 == 0xF && aux == 0xF) {
    switch (in1) {
      CASE 0x0 : *out++ = outval | maxtone;
      goto start_from_in0;

      CASE 0x1 : GET_IN0;
      count = in0;
      goto count_out_and_start_from_in1;

      CASE 0x2 : count = *in++;
      goto count_out_and_start_from_in0;

      CASE 0x3 : count = *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x4 : count = *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x5 : count = *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x6 : count = *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      CASE 0x7 : count = *in++ << 20;
      count += *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      CASE 0x8 : outval ^= 0x100;
      goto start_from_in0;

      CASE 0xF : switch (GET_IN0) {
        UINT aux;
        CASE 0xE : CASE 0xF : aux = in1;
        GET_IN0;

        *out++ = outval | (aux | (in0 << 4));
      DEFAULT:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

    DEFAULT:
      return 0;
    }
  } else {
    *out++ = outval | ((in0 << 4) | aux);
    goto start_from_in1;
  }

end_rle_decoding:
  if (buf_in_len) *buf_in_len = in - buf_in;
  return out - buf_out;

rle_decoding_error:
  if (buf_in_len) *buf_in_len = 0;
  return 0;
}

/*---------------------------------------------------------------------------*/

static TBOOL read_tzr_1_1_1 (char *filename, FILE *file, IMAGE *image,
                             int img_offs, TBOOL cmapped)
{
  int lx, ly, wrap, y, x;
  USHORT *buf_cm, *line_cm;
  LPIXEL *buf_rgbm, *line_rgbm;
  int safe_bytes, enclen_bytes, enclen, dec, tzr_dec;
  UCHAR *tzr_buf;
  UINT tone, color;
  LPIXEL val;

  if (fseek(file, img_offs, SEEK_SET)) {
    tmsg_error("inconsistent data in %s (seek failed)", filename);
    return FALSE;
  }
  lx           = image->pixmap.xsize;
  ly           = image->pixmap.ysize;
  wrap         = lx;
  enclen_bytes = 2 + (lx > 8 * 1024);
  safe_bytes   = enclen_bytes + tzr_safe_bytes_for_1_1_1_pixels(lx);
  tzr_buf      = Tzr_buffer;
  if (safe_bytes > Tzr_buffer_bytes) {
    TREALLOC(tzr_buf, safe_bytes)
    Tzr_buffer = tzr_buf;
    if (Tzr_buffer)
      Tzr_buffer_bytes = safe_bytes;
    else {
      Tzr_buffer_bytes = 0;
      return FALSE;
    }
  }
  if (cmapped) {
    buf_cm = image->pixmap.buffer;
    for (line_cm = buf_cm, y = 0; y < ly; line_cm += wrap, y++) {
      READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
      if (enclen_bytes == 2)
        enclen = tzr_buf[0] << 8 | tzr_buf[1];
      else
        enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
      READ_BYTES_OR_ERROR(tzr_buf, enclen, file)
      dec = tzr_decode_1_1_1_cm16(tzr_buf, &tzr_dec, line_cm);
      assert(dec == lx);
      assert(tzr_dec == enclen);
    }
  } else {
    buf_rgbm = (LPIXEL *)image->pixmap.buffer;
    for (line_rgbm = buf_rgbm, y = 0; y < ly; line_rgbm += wrap, y++) {
      READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
      if (enclen_bytes == 2)
        enclen = tzr_buf[0] << 8 | tzr_buf[1];
      else
        enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
      READ_BYTES_OR_ERROR(tzr_buf, enclen, file)
      line_cm = (USHORT *)line_rgbm;
      dec     = tzr_decode_1_1_1_cm16(tzr_buf, &tzr_dec, line_cm);
      assert(dec == lx);
      assert(tzr_dec == enclen);
      for (x = lx - 1; x >= 0; x--) {
        tone  = line_cm[x] & 0xF;
        color = line_cm[x] & 0x10;
        if (color) {
          val.r = val.g = val.b = tone * 8;
          val.m                 = 255;
        } else {
          val.r = val.g = val.b = 0;
          val.m                 = ~(tone * 17);
        }
        line_rgbm[x] = val;
      }
    }
  }
  return TRUE;

error:
  return FALSE;
}

/*---------------------------------------------------------------------------*/
void vaffa(void)
{}

static TBOOL read_tzr_2_1_1 (char *filename, FILE *file, IMAGE *image,
                             int img_offs, TBOOL cmapped)
{
  int lx, ly, wrap, y, x;
  ULONG *buf_cm, *line_cm;
  LPIXEL *buf_rgbm, *line_rgbm;
  int safe_bytes, enclen_bytes, enclen, dec, tzr_dec;
  UCHAR *tzr_buf;
  UINT tone, color;
  LPIXEL val;

  if (fseek(file, img_offs, SEEK_SET)) {
    tmsg_error("inconsistent data in %s (seek failed)", filename);
    return FALSE;
  }
  lx           = image->pixmap.xsize;
  ly           = image->pixmap.ysize;
  wrap         = lx;
  enclen_bytes = 2 + (lx > 8 * 1024);
  safe_bytes   = enclen_bytes + tzr_safe_bytes_for_2_1_1_pixels(lx);
  tzr_buf      = Tzr_buffer;
  if (safe_bytes > Tzr_buffer_bytes) {
    TREALLOC(tzr_buf, safe_bytes)
    Tzr_buffer = tzr_buf;
    if (Tzr_buffer)
      Tzr_buffer_bytes = safe_bytes;
    else {
      Tzr_buffer_bytes = 0;
      return FALSE;
    }
  }
  if (cmapped) {
    int i;
    buf_cm = (ULONG *)image->pixmap.buffer;
    for (line_cm = buf_cm, y = 0; y < ly; line_cm += wrap, y++) {
      READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
      if (enclen_bytes == 2)
        enclen = tzr_buf[0] << 8 | tzr_buf[1];
      else
        enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
      READ_BYTES_OR_ERROR(tzr_buf, enclen, file)
      dec = tzr_decode_2_1_1_cm24(tzr_buf, &tzr_dec, line_cm);
      assert(dec == lx);
      assert(tzr_dec == enclen);
    }
  } else {
    buf_rgbm = (LPIXEL *)image->pixmap.buffer;
    for (line_rgbm = buf_rgbm, y = 0; y < ly; line_rgbm += wrap, y++) {
      READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
      if (enclen_bytes == 2)
        enclen = tzr_buf[0] << 8 | tzr_buf[1];
      else
        enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
      READ_BYTES_OR_ERROR(tzr_buf, enclen, file)
      line_cm = (ULONG *)line_rgbm;
      dec     = tzr_decode_2_1_1_cm24(tzr_buf, &tzr_dec, line_cm);
      assert(dec == lx);
      assert(tzr_dec == enclen);
      for (x = lx - 1; x >= 0; x--) {
        tone  = line_cm[x] & 0xFF;
        color = line_cm[x] & 0x100;
        if (color) {
          val.r = val.g = val.b = tone >> 1; /* ???? tone * 8;*/
          val.m                 = 255;
        } else {
          val.r = val.g = val.b = 0;
          val.m                 = ~(tone);
        }
        line_rgbm[x] = val;
      }
    }
  }
  return TRUE;

error:
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static TBOOL read_region_tzr_1_1_1 (char *filename, FILE *file, IMAGE *image,
                                    int img_offs, TBOOL cmapped,
				    INFO_REGION *region)
{
  int reg_lx, reg_ly, full_lx, full_ly, wrap, y, x, skip, step, k, tmp_x;
  USHORT *buf_cm, *line_cm, *tmp_buf;
  LPIXEL *buf_rgbm, *line_rgbm;
  int safe_bytes, decline_bytes, enclen_bytes, enclen, dec, tzr_dec;
  UCHAR *tzr_buf;
  UINT tone, color;
  LPIXEL val;

  if (fseek(file, img_offs, SEEK_SET)) {
    tmsg_error("inconsistent data in %s (seek failed)", filename);
    return FALSE;
  }
  full_lx       = region->lx_in;
  wrap          = region->xsize;
  step          = region->step;
  enclen_bytes  = 2 + (full_lx > 8 * 1024);
  safe_bytes    = enclen_bytes + tzr_safe_bytes_for_1_1_1_pixels(full_lx);
  decline_bytes = full_lx * sizeof(USHORT);
  tzr_buf       = Tzr_buffer;
  if (safe_bytes + decline_bytes > Tzr_buffer_bytes) {
    TREALLOC(tzr_buf, safe_bytes + decline_bytes)
    Tzr_buffer = tzr_buf;
    if (Tzr_buffer)
      Tzr_buffer_bytes = safe_bytes + decline_bytes;
    else {
      Tzr_buffer_bytes = 0;
      return FALSE;
    }
  }
  tmp_buf = (USHORT *)(tzr_buf + safe_bytes);
  for (y = 0; y < region->startScanRow; y++) {
    READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
    if (enclen_bytes == 2)
      enclen = tzr_buf[0] << 8 | tzr_buf[1];
    else
      enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
    if (fseek(file, enclen, SEEK_CUR)) goto error;
  }
  buf_cm    = image->pixmap.buffer;
  line_cm   = buf_cm + region->x_offset + region->y_offset * wrap;
  buf_rgbm  = (LPIXEL *)image->pixmap.buffer;
  line_rgbm = buf_rgbm + region->x_offset + region->y_offset * wrap;
  for (y = 0; y < region->scanNrow; y++, line_cm += wrap, line_rgbm += wrap) {
    READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
    if (enclen_bytes == 2)
      enclen = tzr_buf[0] << 8 | tzr_buf[1];
    else
      enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
    READ_BYTES_OR_ERROR(tzr_buf, enclen, file)
    dec = tzr_decode_1_1_1_cm16(tzr_buf, &tzr_dec, tmp_buf);
    assert(dec == full_lx);
    assert(tzr_dec == enclen);
    if (cmapped)
      for (tmp_x = region->x1, k = 0; k < region->scanNcol;
           k++, tmp_x += step) {
        line_cm[k] = tmp_buf[tmp_x];
      }
    else {
      for (tmp_x = region->x1, k = 0; k < region->scanNcol;
           k++, tmp_x += step) {
        tone  = tmp_buf[tmp_x] & 0xF;
        color = tmp_buf[tmp_x] & 0x10;
        if (color) {
          val.r = val.g = val.b = tone * 8;
          val.m                 = 255;
        } else {
          val.r = val.g = val.b = 0;
          val.m                 = ~(tone * 17);
        }
        line_rgbm[k] = val;
      }
    }
    if (y != region->scanNrow - 1)
      for (skip = 0; skip < step - 1; skip++) {
        READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
        if (enclen_bytes == 2)
          enclen = tzr_buf[0] << 8 | tzr_buf[1];
        else
          enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
        if (fseek(file, enclen, SEEK_CUR)) goto error;
      }
  }
  return TRUE;

error:
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static TBOOL read_region_tzr_2_1_1 (char *filename, FILE *file, IMAGE *image,
                                    int img_offs, TBOOL cmapped,
				    INFO_REGION *region)
{
  int reg_lx, reg_ly, full_lx, full_ly, wrap, y, x, skip, step, k, tmp_x;
  ULONG *buf_cm, *line_cm, *tmp_buf;
  LPIXEL *buf_rgbm, *line_rgbm;
  int safe_bytes, decline_bytes, enclen_bytes, enclen, dec, tzr_dec;
  UCHAR *tzr_buf;
  UINT tone, color;
  LPIXEL val;

  if (fseek(file, img_offs, SEEK_SET)) {
    tmsg_error("inconsistent data in %s (seek failed)", filename);
    return FALSE;
  }
  full_lx       = region->lx_in;
  wrap          = region->xsize;
  step          = region->step;
  enclen_bytes  = 2 + (full_lx > 8 * 1024);
  safe_bytes    = enclen_bytes + tzr_safe_bytes_for_2_1_1_pixels(full_lx);
  decline_bytes = full_lx * sizeof(ULONG);
  tzr_buf       = Tzr_buffer;
  if (safe_bytes + decline_bytes > Tzr_buffer_bytes) {
    TREALLOC(tzr_buf, safe_bytes + decline_bytes)
    Tzr_buffer = tzr_buf;
    if (Tzr_buffer)
      Tzr_buffer_bytes = safe_bytes + decline_bytes;
    else {
      Tzr_buffer_bytes = 0;
      return FALSE;
    }
  }
  tmp_buf = (ULONG *)(tzr_buf + safe_bytes);
  for (y = 0; y < region->startScanRow; y++) {
    READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
    if (enclen_bytes == 2)
      enclen = tzr_buf[0] << 8 | tzr_buf[1];
    else
      enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
    if (fseek(file, enclen, SEEK_CUR)) goto error;
  }
  buf_cm    = (ULONG *)image->pixmap.buffer;
  line_cm   = buf_cm + region->x_offset + region->y_offset * wrap;
  buf_rgbm  = (LPIXEL *)image->pixmap.buffer;
  line_rgbm = buf_rgbm + region->x_offset + region->y_offset * wrap;
  for (y = 0; y < region->scanNrow; y++, line_cm += wrap, line_rgbm += wrap) {
    READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
    if (enclen_bytes == 2)
      enclen = tzr_buf[0] << 8 | tzr_buf[1];
    else
      enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
    READ_BYTES_OR_ERROR(tzr_buf, enclen, file)
    dec = tzr_decode_2_1_1_cm24(tzr_buf, &tzr_dec, tmp_buf);
    assert(dec == full_lx);
    assert(tzr_dec == enclen);
    if (cmapped)
      for (tmp_x = region->x1, k = 0; k < region->scanNcol;
           k++, tmp_x += step) {
        line_cm[k] = tmp_buf[tmp_x];
      }
    else {
      for (tmp_x = region->x1, k = 0; k < region->scanNcol;
           k++, tmp_x += step) {
        tone  = tmp_buf[tmp_x] & 0xFF;
        color = tmp_buf[tmp_x] & 0x100;
        if (color) {
          val.r = val.g = val.b = tone >> 1 /* ???? tone * 8  */;
          val.m                 = 255;
        } else {
          val.r = val.g = val.b = 0;
          val.m                 = ~(tone);
        }
        line_rgbm[k] = val;
      }
    }
    if (y != region->scanNrow - 1)
      for (skip = 0; skip < step - 1; skip++) {
        READ_BYTES_OR_ERROR(tzr_buf, enclen_bytes, file)
        if (enclen_bytes == 2)
          enclen = tzr_buf[0] << 8 | tzr_buf[1];
        else
          enclen = tzr_buf[0] << 16 | tzr_buf[1] << 8 | tzr_buf[2];
        if (fseek(file, enclen, SEEK_CUR)) goto error;
      }
  }
  return TRUE;

error:
  return FALSE;
}

/*---------------------------------------------------------------------------*/

static TBOOL read_tzr_header (char *filename, FILE *file, IMAGE *image,
                              UINT *tzr_type, int *p_img_offs, TBOOL cmapped,
			      TBOOL read_tags)
{
  UCHAR buf[TAGS_BUFLEN];
  int lx, ly, img_offs = 0;
  int tags_len, file_pos, pos;
  double x_dpi, y_dpi, h_pos;

  *tzr_type = 0;
  READ_BYTES_OR_ERROR(buf, 8, file)
  if (*(ULONG *)buf != TZR_MAGIC) {
    tmsg_error("bad magic number in %s", filename);
    goto error;
  }
  switch (buf[4 + 0]) {
    CASE 1 : switch (buf[4 + 1]) {
      CASE 1 : switch (buf[4 + 2]) {
        CASE 1 : *tzr_type = 0x01010100;
      DEFAULT:
        tmsg_error("unsupported file type (%d %d %d) in %s", buf[4 + 0],
                   buf[4 + 1], buf[4 + 2], filename);
        goto error;
      }
    }
    CASE 2 : switch (buf[4 + 1]) {
      CASE 1 : switch (buf[4 + 2]) {
        CASE 1 : *tzr_type = 0x02010100;
      DEFAULT:
        tmsg_error("unsupported file type (%d %d %d) in %s", buf[4 + 0],
                   buf[4 + 1], buf[4 + 2], filename);
        goto error;
      }
    DEFAULT:
      tmsg_error("unsupported file type (%d %d) in %s", buf[4 + 0], buf[4 + 1],
                 filename);
      goto error;
    }
  DEFAULT:
    tmsg_error("unsupported file type (%d) in %s", buf[4 + 0], filename);
    goto error;
  }
  switch (*tzr_type) {
    CASE 0x01010100 : __OR 0x02010100 : READ_BYTES_OR_ERROR(buf, 9, file) lx =
                                            buf[0] << 16 | buf[1] << 8 | buf[2];
    ly       = buf[3] << 16 | buf[4] << 8 | buf[5];
    img_offs = buf[6] << 16 | buf[7] << 8 | buf[8];
    if (*tzr_type == 0x01010100) {
      if (cmapped) {
        image->type      = CMAPPED;
        image->cmap.info = Tcm_new_default_info;
      } else
        image->type = RGB;
    } else {
      if (cmapped) {
        image->type      = CMAPPED24;
        image->cmap.info = Tcm_24_default_info;
      } else
        image->type = RGB;
    }
    image->pixmap.xsize   = lx;
    image->pixmap.ysize   = ly;
    image->pixmap.xSBsize = lx;
    image->pixmap.ySBsize = ly;

  DEFAULT:
    assert(0);
  }
  if (read_tags) {
    file_pos = ftell(file);
    tags_len = img_offs - file_pos;
    if (tags_len > TAGS_BUFLEN) {
      tmsg_error("tags section of %s too long", filename);
      goto error;
    }
    if (tags_len) READ_BYTES_OR_ERROR(buf, tags_len, file)
    pos = 0;
    while (pos < tags_len) {
      switch (buf[pos]) {
        CASE TAG_DPI : if (buf[pos + 1] != 8 + 8) goto unknown_tag;
        BYTES_TO_DOUBLE(buf + pos + 2, x_dpi)
        BYTES_TO_DOUBLE(buf + pos + 10, y_dpi)
        image->pixmap.x_dpi = x_dpi;
        image->pixmap.y_dpi = y_dpi;
        pos += 1 + 1 + 8 + 8;

        CASE TAG_HPOS : if (buf[pos + 1] != 8) goto unknown_tag;
        BYTES_TO_DOUBLE(buf + pos + 2, h_pos)
        image->pixmap.h_pos = h_pos;
        pos += 1 + 1 + 8;

      DEFAULT:
      unknown_tag:
        if (buf[pos] >= 128)
          pos += 1 + 2 + (buf[pos + 1] << 8 | buf[pos + 2]);
        else
          pos += 1 + 1 + buf[pos + 1];
      }
    }
  }
  *p_img_offs = img_offs;
  return TRUE;

error:
  return FALSE;
}

/*===========================================================================*/

void next_img_read_tzr_cmapped (void)
{
  Next_img_read_tzr_cmapped = TRUE;
}

/*---------------------------------------------------------------------------*/

IMAGE *img_read_tzr (char *filename)
{
  FILE *file;
  IMAGE *image;
  UINT tzr_type;
  int lx, ly, img_offs;

  SET_READ_CMAPPED

  file = fopen(filename, "rb");
  if (!file) return NIL;
  image = new_img();
  if (!image) goto error;
  if (!read_tzr_header(filename, file, image, &tzr_type, &img_offs,
                       Read_cmapped, TRUE))
    goto error;
  lx = image->pixmap.xsize;
  ly = image->pixmap.ysize;
  if (!allocate_pixmap(image, lx, ly)) goto error;
  switch (tzr_type) {
    CASE 0x01010100 : if (!read_tzr_1_1_1(filename, file, image, img_offs,
                                          Read_cmapped)) goto error;

    CASE 0x02010100 : if (!read_tzr_2_1_1(filename, file, image, img_offs,
                                          Read_cmapped)) goto error;

  DEFAULT:
    goto error;
  }
  fclose(file);
  return image;

error:
  if (image) free_img(image);
  if (file) fclose(file);
  return NIL;
}

/*---------------------------------------------------------------------------*/

IMAGE *img_read_region_tzr (char *filename, int x0, int y0, 
                                            int x1, int y1, int step)
{
  FILE *file;
  IMAGE *image;
  UINT tzr_type;
  int full_lx, full_ly, reg_lx, reg_ly, img_offs;
  INFO_REGION region;
  int i, pixels;

  SET_READ_CMAPPED

  file = fopen(filename, "rb");
  if (!file) return NIL;
  image = new_img();
  if (!image) goto error;
  if (!read_tzr_header(filename, file, image, &tzr_type, &img_offs,
                       Read_cmapped, TRUE))
    goto error;
  full_lx          = image->pixmap.xsize;
  full_ly          = image->pixmap.ysize;
  if (x0 == -1) x0 = full_lx - 1;
  if (y0 == -1) y0 = full_ly - 1;
  reg_lx           = (x1 - x0) / step + 1;
  reg_ly           = (y1 - y0) / step + 1;
  if (!allocate_pixmap(image, reg_lx, reg_ly)) goto error;

  /* per adesso si fa un clear brutale */
  pixels = reg_lx * reg_ly;
  switch (image->type) {
    CASE RGB
        : for (i = 0; i < pixels; i++)((ULONG *)(image->pixmap.buffer))[i] = 0;
    CASE CMAPPED : for (i = 0; i < pixels;
                        i++)((USHORT *)(image->pixmap.buffer))[i] = 0xF;
    CASE CMAPPED24 : for (i = 0; i < pixels;
                          i++)((ULONG *)(image->pixmap.buffer))[i] = 0xFF;
  DEFAULT:
    assert(0);
  }

  getInfoRegion(&region, x0, y0, x1, y1, step, full_lx, full_ly);

  switch (tzr_type) {
    CASE 0x01010100
        : if (!read_region_tzr_1_1_1(filename, file, image, img_offs,
                                     Read_cmapped, &region)) goto error;

    CASE 0x02010100
        : if (!read_region_tzr_2_1_1(filename, file, image, img_offs,
                                     Read_cmapped, &region)) goto error;

  DEFAULT:
    goto error;
  }
  fclose(file);
  return image;

error:
  if (image) free_img(image);
  if (file) fclose(file);
  return NIL;
}

/*---------------------------------------------------------------------------*/

IMAGE *img_read_tzr_info (char *filename)
{
  FILE *file;
  IMAGE *image;
  int img_offs;
  UINT tzr_type;

  file = fopen(filename, "rb");
  if (!file) return NIL;
  image = new_img();
  if (!image) goto error;
  if (!read_tzr_header(filename, file, image, &tzr_type, &img_offs,
                       Read_cmapped, TRUE))
    goto error;
  fclose(file);
  return image;

error:
  if (image) free_img(image);
  if (file) fclose(file);
  return NIL;
}
