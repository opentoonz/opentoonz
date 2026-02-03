

#include "tiffiop.h"
#include <assert.h>
#include "toonztags.h"
#include "tnztypes.h"

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef USHORT
#define USHORT unsigned short
#endif

#ifndef UINT
#define UINT TUINT32
#endif

extern "C" {
static int tif_toonz1_decode_cm24(UCHAR* buf_in, int* buf_in_len,
                                  TUINT32* buf_out);

static int tif_toonz1_decode_cm16(UCHAR* buf_in, int* buf_in_len,
                                  USHORT* buf_out, int tone_bits,
                                  int color_offs, int color_bits,
                                  int pencil_offs, int pencil_bits,
                                  USHORT offset_mask);
static int Toonz1Decode(TIFF* tif, tidataval_t* buffer, tsize_t bytes,
                        tsample_t s);

static int TIFFInitToonz1(TIFF* tif, int) {
  tif->tif_decoderow   = Toonz1Decode;
  tif->tif_decodestrip = 0;
  tif->tif_decodetile  = 0;
  tif->tif_encoderow   = 0;  // Toonz1Encode;
  tif->tif_encodestrip = 0;
  tif->tif_encodetile  = 0;
  return 1;
}
//-------------------- DECODE

static int Toonz1Decode(TIFF* tif, tidataval_t* buffer, tsize_t bytes,
                        tsample_t s) {
  int enc, dec;
  short bitspersample;
  // USHORT *palette;
  int tone_bits, color_offs, color_bits, pencil_offs, pencil_bits;
  USHORT offset_mask;

  if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample)) assert(0);
  enc = dec = 0;
  switch (bitspersample) {
  case 8:
    assert(!"Not Implemented");
    /*
dec = tif_toonz1_decode_extra ((UCHAR *)tif->tif_rawcp, &enc,
                       (UCHAR *)buffer);
*/
    break;

  case 16: {
    USHORT* palette;
    USHORT paletteCount;
    if (TIFFGetField(tif, TIFFTAG_TOONZPALETTE, &paletteCount, &palette)) {
      tone_bits   = palette[4];
      color_offs  = palette[5];
      color_bits  = palette[6];
      pencil_offs = palette[7];
      pencil_bits = palette[8];
      offset_mask = palette[9];
    } else {
      tone_bits   = 4;
      color_offs  = 4;
      color_bits  = 7;
      pencil_offs = 11;
      pencil_bits = 5;
      offset_mask = 0;
    }
    dec = tif_toonz1_decode_cm16((UCHAR*)tif->tif_rawcp, &enc, (USHORT*)buffer,
                                 tone_bits, color_offs, color_bits, pencil_offs,
                                 pencil_bits, offset_mask);
  } break;

  case 32:
    dec =
        tif_toonz1_decode_cm24((UCHAR*)tif->tif_rawcp, &enc, (TUINT32*)buffer);
    break;

  default:
    assert(0);
  }
  assert(enc);
  assert(dec * bitspersample == bytes * 8);
  tif->tif_rawcc += enc;
  tif->tif_rawcp += enc;

  return 1;
}

//-------------------- DECODE CM16 ----------------------------

static int tif_toonz1_decode_cm16(UCHAR* buf_in, int* buf_in_len,
                                  USHORT* buf_out, int tone_bits,
                                  int color_offs, int color_bits,
                                  int pencil_offs, int pencil_bits,
                                  USHORT offset_mask)

{
  UCHAR* in;
  USHORT* out;
  int count, col_offs, pen_offs;
  UINT inval, in0, in1, tmp, not_colmask, not_penmask;
  UINT outval, maxtone, outval_maxtone;

#define GET_IN0 (inval = *in++, in1 = inval & 0xF, in0 = inval >> 4)

  maxtone     = (1U << tone_bits) - 1U;
  col_offs    = color_offs;
  pen_offs    = pencil_offs;
  not_colmask = ~(((1U << color_bits) - 1U) << color_offs);
  not_penmask = ~(((1U << pencil_bits) - 1U) << pencil_offs);
  assert(maxtone <= 0xF);
  outval         = offset_mask;
  outval_maxtone = outval | maxtone;

  in  = buf_in;
  out = buf_out;

  goto start_from_in0;

count_out_and_start_from_in0:
  outval_maxtone = outval | maxtone;
  *out++         = (USHORT)outval_maxtone;
  while (count--) *out++ = (USHORT)outval_maxtone;

start_from_in0:
  if (GET_IN0 == 0xF) {
    switch (in1) {
    case 0x0:
      *out++ = (USHORT)(outval | maxtone);
      goto start_from_in0;
      break;

    case 0x1:
      count = GET_IN0;
      goto count_out_and_start_from_in1;
      break;

    case 0x2:
      count = *in++;
      goto count_out_and_start_from_in0;
      break;

    case 0x3:
      count = *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;
      break;

    case 0x4:
      count = *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;
      break;

    case 0x5:
      count = *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      break;

    case 0x6:
      count = *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      break;

    case 0x7:
      count = *in++ << 20;
      count += *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      break;

    case 0x8:
      outval &= not_colmask;
      goto start_from_in0;

      break;

    case 0x9:
      outval &= not_penmask;
      goto start_from_in0;

      break;

    case 0xA:
      outval = outval & not_colmask | GET_IN0 << col_offs;
      goto start_from_in1;

      break;

    case 0xB:
      outval = outval & not_penmask | GET_IN0 << pen_offs;
      goto start_from_in1;

      break;

    case 0xC:
      outval = outval & not_colmask | *in++ << col_offs;
      goto start_from_in0;

      break;

    case 0xD:
      outval = outval & not_penmask | *in++ << pen_offs;
      goto start_from_in0;

      break;

    case 0xE:
      *out++ = (USHORT)(outval | maxtone);
      goto end_rle_decoding;

      break;

    case 0xF:
      switch (GET_IN0) {
      case 0:
        break;
      case 1:
        tmp    = in1 << 12 | *in++ << 4;
        *out++ = (USHORT)(tmp | GET_IN0);
        break;
      default:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

      break;
    default:
      goto rle_decoding_error;
    }
  } else {
    *out++ = (USHORT)(outval | in0);
    goto start_from_in1;
  }

count_out_and_start_from_in1:
  outval_maxtone = outval | maxtone;
  *out++         = (USHORT)(outval_maxtone);
  while (count--) *out++ = (USHORT)(outval_maxtone);

start_from_in1:
  if (in1 == 0xF) {
    switch (GET_IN0) {
    case 0x0:
      *out++ = (USHORT)(outval | maxtone);
      goto start_from_in1;

      break;

    case 0x1:
      count = in1;
      goto count_out_and_start_from_in0;

      break;

    case 0x2:
      count = in1 << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      break;

    case 0x3:
      count = in1 << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      break;

    case 0x4:
      count = in1 << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      break;

    case 0x5:
      count = in1 << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      break;

    case 0x6:
      count = in1 << 20;
      count += *in++ << 12;
      count += *in++ << 4;
      count += GET_IN0;
      goto count_out_and_start_from_in1;

      break;

    case 0x7:
      count = in1 << 24;
      count += *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out_and_start_from_in0;

      break;

    case 0x8:
      outval &= not_colmask;
      goto start_from_in1;

      break;

    case 0x9:
      outval &= not_penmask;
      goto start_from_in1;

      break;

    case 0xA:
      outval = outval & not_colmask | in1 << col_offs;
      goto start_from_in0;

      break;

    case 0xB:
      outval = outval & not_penmask | in1 << pen_offs;
      goto start_from_in0;

      break;

    case 0xC:
      tmp    = in1 << 4;
      outval = outval & not_colmask | (tmp | GET_IN0) << col_offs;
      goto start_from_in1;

      break;

    case 0xD:
      tmp    = in1 << 4;
      outval = outval & not_penmask | (tmp | GET_IN0) << pen_offs;
      goto start_from_in1;

      break;

    case 0xE:
      *out++ = (USHORT)(outval | maxtone);
      goto end_rle_decoding;

      break;

    case 0xF:
      switch (in1) {
      case 0:
        break;
      case 1:
        tmp    = *in++ << 8;
        *out++ = (USHORT)(tmp | *in++);
        break;
      default:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

      break;
    default:
      return 0;
    }
  } else {
    *out++ = (USHORT)(outval | in1);
    goto start_from_in0;
  }

end_rle_decoding:
  if (buf_in_len) *buf_in_len = (int)(in - buf_in);
  return (int)(out - buf_out);

rle_decoding_error:
  if (buf_in_len) *buf_in_len = 0;
  return 0;
}

//-------------------- DECODE CM24 ----------------------------

static int tif_toonz1_decode_cm24(UCHAR* buf_in, int* buf_in_len,
                                  TUINT32* buf_out) {
  UCHAR* in;
  TUINT32* out;
  int count;
  TUINT32 inval, tmp;
  TUINT32 outval, outval_maxtone;
  const int col_offs        = 8;
  const int pen_offs        = 16;
  const int xub_offs        = 24;
  const TUINT32 not_colmask = 0xffff00ff;
  const TUINT32 not_penmask = 0xff00ffff;
  const TUINT32 not_xubmask = 0x00ffffff;
  const TUINT32 maxtone     = 0x000000ff;

  outval         = 0;
  outval_maxtone = outval | maxtone;

  in  = buf_in;
  out = buf_out;

  for (;;) {
    inval = *in++;
    switch (inval) {
    case 0xF6:
      outval = outval & not_xubmask | *in++ << xub_offs;

      break;

    case 0xF7:
      count = *in++;
      goto count_out;

      break;

    case 0xF8:
      count = *in++ << 8;
      count += *in++;
      goto count_out;

      break;

    case 0xF9:
      count = *in++ << 16;
      count += *in++ << 8;
      count += *in++;
      goto count_out;

      break;

    case 0xFA:
      outval &= not_colmask;

      break;

    case 0xFB:
      outval &= not_penmask;

      break;

    case 0xFC:
      outval = outval & not_colmask | *in++ << col_offs;

      break;

    case 0xFD:
      outval = outval & not_penmask | *in++ << pen_offs;

      break;

    case 0xFE:
      switch (*in++) {
      case 0:
        break;
      case 1:
        *out++ = outval | maxtone;
        break;

      case 2:
        tmp    = *in++;
        tmp    = tmp << 8 | *in++;
        tmp    = tmp << 8 | *in++;
        *out++ = tmp << 8 | *in++;
        break;

      default:
        goto rle_decoding_error;
      }
      goto end_rle_decoding;

      break;

    case 0xFF:
      *out++ = outval | *in++;

      break;

    default:
      *out++ = outval | inval;
    }
    continue;

  count_out:
    outval_maxtone = outval | maxtone;
    *out++         = outval_maxtone;
    while (count--) *out++ = outval_maxtone;
  }

end_rle_decoding:
  if (buf_in_len) *buf_in_len = (int)(in - buf_in);
  return (int)(out - buf_out);

rle_decoding_error:
  if (buf_in_len) *buf_in_len = 0;
  return 0;
}
}

//=============================================================================

namespace {

class ToonzRleCodecRegisterer {
  static TIFFCodec* m_codec;

public:
  ToonzRleCodecRegisterer() {
    uint16 scheme    = 32881;
    const char* name = "TOONZ4RLE";
    m_codec          = TIFFRegisterCODEC(scheme, name, &TIFFInitToonz1);
  }
  ~ToonzRleCodecRegisterer() {
    TIFFUnRegisterCODEC(m_codec);
    m_codec = 0;
  }
};

TIFFCodec* ToonzRleCodecRegisterer::m_codec = 0;

ToonzRleCodecRegisterer registerer;
}  // namespace

//=============================================================================

#if 0

/*
Queste stanno qui piu' che altro per memoria...
*/

extern "C" {

  /*===========================================================================*/

  /*---------------------------------------------------------------------------*/

#define GET_INVAL                                                              \
  {                                                                            \
    inval = *in++;                                                             \
    remain--;                                                                  \
  }
#define PUT_OUTVAL                                                             \
  { *out++ = (UCHAR)outval; }

/*---------------------------------------------------------------------------*/

  int tif_toonz1_safe_bytes_for_pixels(int n_pix, int pixbytes)
  {
    if (n_pix < 0)
      return 0;
    switch (pixbytes)
    {
    case 1: return 2 * n_pix + 1;
    case 2: return 5 * n_pix + 2;   /* == 2.5 * n_bytes + 2 */
    case 4: return 8 * n_pix + 6;   /* == 2   * n_bytes + 6 */
    }
    return 0;
  }

  /*---------------------------------------------------------------------------*/

  static int tif_toonz1_encode_cm16(USHORT* buf_in, int buf_in_len,
    UCHAR* buf_out,
    int tone_bits,
    int color_offs, int color_bits,
    int pencil_offs, int pencil_bits,
    USHORT offset_mask)
  {
    int  count, prevremain, remain;
    UINT inval, outval;
    UINT lastcol__, lastpen__, lastval, lastcolpen;
    int  col_offs, pen_offs;
    UINT colpenmask, colmask, penmask, maxtone;
    UINT incolpen, incol__, incol, inpen__, inpen, tone;
    USHORT* in, save;
    UCHAR* out;

    maxtone = (1U << tone_bits) - 1U;
    col_offs = color_offs;
    pen_offs = pencil_offs;
    colmask = ((1U << color_bits) - 1U) << color_offs;
    penmask = ((1U << pencil_bits) - 1U) << pencil_offs;
    colpenmask = colmask | penmask;

    remain = buf_in_len;
    lastcol__ = 0;
    lastpen__ = 0;
    lastval = offset_mask | maxtone;
    lastcolpen = 0;

    in = buf_in;
    out = buf_out;

    if (buf_in_len <= 1)
    {
      if (buf_in_len < 0)
        return 0;
      if (buf_in_len == 0)
      {
        outval = 0xFF;
        PUT_OUTVAL
          outval = 0x00;
        PUT_OUTVAL
      }
      else
      {
        GET_INVAL
          if (inval == (offset_mask | maxtone))
          {
            outval = 0xFE;
            PUT_OUTVAL
          }
          else
          {
            outval = 0xFF;
            PUT_OUTVAL
              outval = 0x10 | inval >> 12;
            PUT_OUTVAL
              outval = inval >> 4;
            PUT_OUTVAL
              outval = inval << 4;
            PUT_OUTVAL
          }
      }
      return out - buf_out;
    }

    save = buf_in[buf_in_len - 1];
    buf_in[buf_in_len - 1] = buf_in[buf_in_len - 2] ^ colpenmask;

    GET_INVAL

      check_colpen_on_out0 :
    incolpen = inval & colpenmask;
    if (incolpen == lastcolpen)
      goto check_tone_on_out0;
    lastcolpen = incolpen;
    if (!remain)
      goto end_toonz1_encoding_on_out0;

    /*check_col_on_out0:*/
    incol__ = inval & colmask;
    if (incol__ != lastcol__)
    {
      lastcol__ = incol__;
      if (incol__ == 0)
      {
        outval = (0xF << 4) | 0x8;
        PUT_OUTVAL
          goto check_pen_on_out0;
      }
      else
      {
        incol = incol__ >> col_offs;
        if (incol <= 0xF)
        {
          outval = (0xF << 4) | 0xA;
          PUT_OUTVAL
            outval = incol << 4;
          goto check_pen_on_out1;
        }
        else
        {
          outval = (0xF << 4) | 0xC;
          PUT_OUTVAL
            outval = incol;
          PUT_OUTVAL
            goto check_pen_on_out0;
        }
      }
    }
  check_pen_on_out0:
    inpen__ = inval & penmask;
    if (inpen__ != lastpen__)
    {
      lastpen__ = inpen__;
      if (inpen__ == 0)
      {
        outval = (0xF << 4) | 0x9;
        PUT_OUTVAL
          goto check_tone_on_out0;
      }
      else
      {
        inpen = inpen__ >> pen_offs;
        if (inpen <= 0xF)
        {
          outval = (0xF << 4) | 0xB;
          PUT_OUTVAL
            outval = inpen << 4;
          goto check_tone_on_out1;
        }
        else
        {
          outval = (0xF << 4) | 0xD;
          PUT_OUTVAL
            outval = inpen;
          PUT_OUTVAL
            goto check_tone_on_out0;
        }
      }
    }
  check_tone_on_out0:
    tone = inval & maxtone;
    if (tone == maxtone)
    {
      lastval = inval;
      prevremain = remain;
      do
        GET_INVAL
        while (inval == lastval);
      count = prevremain - remain - 1;
      if (count <= 0xF)
        if (count == 0)
        {
          outval = (0xF << 4) | 0x0;
          PUT_OUTVAL
            goto check_colpen_on_out0;
        }
        else
        {
          outval = (0xF << 4) | 0x1;
          PUT_OUTVAL
            outval = count << 4;
          goto check_colpen_on_out1;
        }
      else if (count <= 0xFF)
      {
        outval = (0xF << 4) | 0x2;
        PUT_OUTVAL
          outval = count;
        PUT_OUTVAL
          goto check_colpen_on_out0;
      }
      else if (count <= 0xFFF)
      {
        outval = (0xF << 4) | 0x3;
        PUT_OUTVAL
          outval = count >> 4;
        PUT_OUTVAL
          outval = count << 4;
        goto check_colpen_on_out1;
      }
      else if (count <= 0xFFFF)
      {
        outval = (0xF << 4) | 0x4;
        PUT_OUTVAL
          outval = count >> 8;
        PUT_OUTVAL
          outval = count;
        PUT_OUTVAL
          goto check_colpen_on_out0;
      }
      else if (count <= 0xFFFFF)
      {
        outval = (0xF << 4) | 0x5;
        PUT_OUTVAL
          outval = count >> 12;
        PUT_OUTVAL
          outval = count >> 4;
        PUT_OUTVAL
          outval = count << 4;
        goto check_colpen_on_out1;
      }
      else if (count <= 0xFFFFFF)
      {
        outval = (0xF << 4) | 0x6;
        PUT_OUTVAL
          outval = count >> 16;
        PUT_OUTVAL
          outval = count >> 8;
        PUT_OUTVAL
          outval = count;
        PUT_OUTVAL
          goto check_colpen_on_out0;
      }
      else if (count <= 0xFFFFFFF)
      {
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
      }
      else
      {
        buf_in[buf_in_len - 1] = save;
        return 0;
      }
    }
    else
    {
      outval = tone << 4;
      GET_INVAL
        /*goto check_colpen_on_out1;*/
    }

  check_colpen_on_out1:
    incolpen = inval & colpenmask;
    if (incolpen == lastcolpen)
      goto check_tone_on_out1;
    lastcolpen = incolpen;
    if (!remain)
      goto end_toonz1_encoding_on_out1;

    /*check_col_on_out1:*/
    incol__ = inval & colmask;
    if (incol__ != lastcol__)
    {
      lastcol__ = incol__;
      outval |= 0xF;
      PUT_OUTVAL
        if (incol__ == 0)
        {
          outval = 0x8 << 4;
          goto check_pen_on_out1;
        }
        else
        {
          incol = incol__ >> col_offs;
          if (incol <= 0xF)
          {
            outval = (0xA << 4) | incol;
            PUT_OUTVAL
              goto check_pen_on_out0;
          }
          else
          {
            outval = (0xC << 4) | (incol >> 4);
            PUT_OUTVAL
              outval = incol << 4;
            goto check_pen_on_out1;
          }
        }
    }
  check_pen_on_out1:
    inpen__ = inval & penmask;
    if (inpen__ != lastpen__)
    {
      lastpen__ = inpen__;
      outval |= 0xF;
      PUT_OUTVAL
        if (inpen__ == 0)
        {
          outval = 0x9 << 4;
          goto check_tone_on_out1;
        }
        else
        {
          inpen = inpen__ >> pen_offs;
          if (inpen <= 0xF)
          {
            outval = (0xB << 4) | inpen;
            PUT_OUTVAL
              goto check_tone_on_out0;
          }
          else
          {
            outval = (0xD << 4) | (inpen >> 4);
            PUT_OUTVAL
              outval = inpen << 4;
            goto check_tone_on_out1;
          }
        }
    }
  check_tone_on_out1:
    tone = inval & maxtone;
    if (tone == maxtone)
    {
      lastval = inval;
      prevremain = remain;
      do
        GET_INVAL
        while (inval == lastval);
      count = prevremain - remain - 1;
      outval |= 0xF;
      PUT_OUTVAL
        if (count <= 0xF)
          if (count == 0)
          {
            outval = 0x0 << 4;
            goto check_colpen_on_out1;
          }
          else
          {
            outval = (0x1 << 4) | count;
            PUT_OUTVAL
              goto check_colpen_on_out0;
          }
        else if (count <= 0xFF)
        {
          outval = (0x2 << 4) | (count >> 4);
          PUT_OUTVAL
            outval = count << 4;
          goto check_colpen_on_out1;
        }
        else if (count <= 0xFFF)
        {
          outval = (0x3 << 4) | (count >> 8);
          PUT_OUTVAL
            outval = count;
          PUT_OUTVAL
            goto check_colpen_on_out0;
        }
        else if (count <= 0xFFFF)
        {
          outval = (0x4 << 4) | (count >> 12);
          PUT_OUTVAL
            outval = count >> 4;
          PUT_OUTVAL
            outval = count << 4;
          goto check_colpen_on_out1;
        }
        else if (count <= 0xFFFFF)
        {
          outval = (0x5 << 4) | (count >> 16);
          PUT_OUTVAL
            outval = count >> 8;
          PUT_OUTVAL
            outval = count;
          PUT_OUTVAL
            goto check_colpen_on_out0;
        }
        else if (count <= 0xFFFFFF)
        {
          outval = (0x6 << 4) | (count >> 20);
          PUT_OUTVAL
            outval = count >> 12;
          PUT_OUTVAL
            outval = count >> 4;
          PUT_OUTVAL
            outval = count << 4;
          goto check_colpen_on_out1;
        }
        else if (count <= 0xFFFFFFF)
        {
          outval = (0x7 << 4) | (count >> 24);
          PUT_OUTVAL
            outval = count >> 16;
          PUT_OUTVAL
            outval = count >> 8;
          PUT_OUTVAL
            outval = count;
          PUT_OUTVAL
            goto check_colpen_on_out0;
        }
        else
        {
          buf_in[buf_in_len - 1] = save;
          return 0;
        }
    }
    else
    {
      outval |= tone;
      PUT_OUTVAL
        GET_INVAL
        goto check_colpen_on_out0;
    }

  end_toonz1_encoding_on_out0:
    if (save == (in[-2] | maxtone))
    {
      outval = 0xFE;
      PUT_OUTVAL
    }
    else
    {
      outval = 0xFF;
      PUT_OUTVAL
        outval = 0x10 | save >> 12;
      PUT_OUTVAL
        outval = save >> 4;
      PUT_OUTVAL
        outval = save << 4;
      PUT_OUTVAL
    }
    buf_in[buf_in_len - 1] = save;
    return out - buf_out;

  end_toonz1_encoding_on_out1:
    if (save == (in[-2] | maxtone))
    {
      outval |= 0xF;
      PUT_OUTVAL
        outval = 0xE0;
      PUT_OUTVAL
    }
    else
    {
      outval |= 0xF;
      PUT_OUTVAL
        outval = 0xF1;
      PUT_OUTVAL
        outval = save >> 8;
      PUT_OUTVAL
        outval = save;
      PUT_OUTVAL
    }
    buf_in[buf_in_len - 1] = save;
    return out - buf_out;
  }

  /*---------------------------------------------------------------------------*/

  static int tif_toonz1_encode_cm24(TUINT32* buf_in, int buf_in_len,
    UCHAR* buf_out)
  {
    int   count, prevremain, remain;
    TUINT32 inval, outval;
    TUINT32 lastcol__, lastpen__, lastxub__, lastval, lastenot;
    TUINT32 inenot, incol__, incol, inpen__, inpen, inxub__, inxub, tone;
    TUINT32* in, save;
    UCHAR* out;
    const int col_offs = 8;
    const int pen_offs = 16;
    const int xub_offs = 24;
    const TUINT32 enotmask = 0xffffff00;
    const TUINT32 colmask = 0x0000ff00;
    const TUINT32 penmask = 0x00ff0000;
    const TUINT32 xubmask = 0xff000000;
    const TUINT32 maxtone = 0x000000ff;

    /* enot == ~tone , xub == extra upper byte */

    remain = buf_in_len;
    lastcol__ = 0;
    lastpen__ = 0;
    lastxub__ = 0;
    lastval = maxtone;
    lastenot = 0;

    in = buf_in;
    out = buf_out;

    if (buf_in_len <= 1)
    {
      if (buf_in_len < 0)
        return 0;
      if (buf_in_len == 0)
      {
        outval = 0xFE;
        PUT_OUTVAL
          outval = 0x00;
        PUT_OUTVAL
      }
      else
      {
        GET_INVAL
          if (inval == maxtone)
          {
            outval = 0xFE;
            PUT_OUTVAL
              outval = 0x01;
            PUT_OUTVAL
          }
          else
          {
            outval = 0xFE;
            PUT_OUTVAL
              outval = 0x02;
            PUT_OUTVAL
              outval = inval >> 24;
            PUT_OUTVAL
              outval = inval >> 16;
            PUT_OUTVAL
              outval = inval >> 8;
            PUT_OUTVAL
              outval = inval;
            PUT_OUTVAL
          }
      }
      return out - buf_out;
    }

    save = buf_in[buf_in_len - 1];
    buf_in[buf_in_len - 1] = buf_in[buf_in_len - 2] ^ enotmask;

    GET_INVAL

      for (;;)
      {
        inenot = inval & enotmask;
        if (inenot != lastenot)
        {
          lastenot = inenot;
          if (!remain)
            goto end_toonz1_encoding;

          incol__ = inval & colmask;
          if (incol__ != lastcol__)
          {
            lastcol__ = incol__;
            if (incol__ == 0)
            {
              outval = 0xFA;
              PUT_OUTVAL
            }
            else
            {
              incol = incol__ >> col_offs;
              outval = 0xFC;
              PUT_OUTVAL
                outval = incol;
              PUT_OUTVAL
            }
          }
          inpen__ = inval & penmask;
          if (inpen__ != lastpen__)
          {
            lastpen__ = inpen__;
            if (inpen__ == 0)
            {
              outval = 0xFB;
              PUT_OUTVAL
            }
            else
            {
              inpen = inpen__ >> pen_offs;
              outval = 0xFD;
              PUT_OUTVAL
                outval = inpen;
              PUT_OUTVAL
            }
          }
          inxub__ = inval & xubmask;
          if (inxub__ != lastxub__)
          {
            lastxub__ = inxub__;
            inxub = inxub__ >> xub_offs;
            outval = 0xF6;
            PUT_OUTVAL
              outval = inxub;
            PUT_OUTVAL
          }
        }
        tone = inval & maxtone;
        if (tone == maxtone)
        {
          lastval = inval;
          prevremain = remain;
          do
            GET_INVAL
            while (inval == lastval);
          count = prevremain - remain - 1;
          if (count <= 0xFF)
          {
            outval = 0xF7;
            PUT_OUTVAL
              outval = count;
            PUT_OUTVAL
          }
          else if (count <= 0xFFFF)
          {
            outval = 0xF8;
            PUT_OUTVAL
              outval = count >> 8;
            PUT_OUTVAL
              outval = count;
            PUT_OUTVAL
          }
          else
          {
            while (count > 0xFFFFFF)
            {
              outval = 0xF9;
              PUT_OUTVAL
                outval = 0xFF;
              PUT_OUTVAL
                outval = 0xFF;
              PUT_OUTVAL
                outval = 0xFF;
              count -= 0xFFFFFF + 1;
            }
            outval = 0xF9;
            PUT_OUTVAL
              outval = count >> 16;
            PUT_OUTVAL
              outval = count >> 8;
            PUT_OUTVAL
              outval = count;
            PUT_OUTVAL
          }
        }
        else
        {
          if (tone < 0xF6)
          {
            outval = tone;
            PUT_OUTVAL
          }
          else
          {
            outval = 0xFF;
            PUT_OUTVAL
              outval = tone;
            PUT_OUTVAL
          }
          GET_INVAL
        }
      }

  end_toonz1_encoding:
    if (save == (in[-2] | maxtone))
    {
      outval = 0xFE;
      PUT_OUTVAL
        outval = 0x01;
      PUT_OUTVAL
    }
    else
    {
      outval = 0xFE;
      PUT_OUTVAL
        outval = 0x02;
      PUT_OUTVAL
        outval = save >> 24;
      PUT_OUTVAL
        outval = save >> 16;
      PUT_OUTVAL
        outval = save >> 8;
      PUT_OUTVAL
        outval = save;
      PUT_OUTVAL
    }
    buf_in[buf_in_len - 1] = save;
    return out - buf_out;
  }

  /*---------------------------------------------------------------------------*/

  static int tif_toonz1_encode_extra(UCHAR* buf_in, int buf_in_len,
    UCHAR* buf_out)
  {
    UCHAR* in, inval, lastval, save;
    UCHAR* out;
    int count, remain;

    remain = buf_in_len;
    in = buf_in;
    out = buf_out;
    if (buf_in_len < 2)
    {
      if (!buf_in_len)
      {
        *out = 0;
        return 1;
      }
      else
      {
        *out++ = 1;
        *out++ = *in;
        *out = 0;
        return 3;
      }
    }
    save = buf_in[buf_in_len - 1];
    buf_in[buf_in_len - 1] = buf_in[buf_in_len - 2] ^ 0xff;

    lastval = *buf_in;
    count = 0;
    for (;;)
    {
      GET_INVAL
        if (inval == lastval)
          count++;
        else
        {
          while (count >> 15)
          {
            *out++ = 0xff;
            *out++ = 0xff;
            *out++ = lastval;
            count -= 0x7fff;
          }
          if (count >> 7)
          {
            *out++ = 0x80 | count & 0x7f;
            *out++ = count >> 7;
          }
          else
            *out++ = count;
          *out++ = lastval;
          lastval = inval;
          count = 1;
          if (!remain)
            break;
        }
    }
    *out++ = 1;
    *out++ = save;
    *out++ = 0;

    buf_in[buf_in_len - 1] = save;
    return out - buf_out;
  }


  static int tif_toonz1_decode_extra(UCHAR* buf_in, int* buf_in_len,
    UCHAR* buf_out)
  {
    UCHAR* in, val;
    UCHAR* out;
    int count;

    in = buf_in;
    out = buf_out;
    for (;;)
    {
      count = (SCHAR)*in++;
      if (count > 0)
      {
        val = *in++;
        while (count--)
          *out++ = val;
      }
      else if (count < 0)
      {
        count &= 0x7f;
        count |= *in++ << 7;
        val = *in++;
        while (count--)
          *out++ = val;
      }
      else
        break;
    }
    if (buf_in_len)
      *buf_in_len = in - buf_in;
    return out - buf_out;
  }

  /*---------------------------------------------------------------------------*/



  /*---------------------------------------------------------------------------*/

  static int
    DECLARE4(Toonz1Encode, TIFF*, tif, u_char*, buffer, u_long, bytes, u_int, s)
  {
    int enc;
    short bitspersample;
    USHORT* palette;
    int tone_bits, color_offs, color_bits, pencil_offs, pencil_bits;
    USHORT offset_mask;

    if (!TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample))
      assert(0);
    enc = 0;
    switch (bitspersample)
    {
    case 8:
      enc = tif_toonz1_encode_extra((UCHAR*)buffer, (int)bytes,
        (UCHAR*)tif->tif_rawcp);
      break;

    case 16:
      if (TIFFGetField(tif, TIFFTAG_TOONZPALETTE, &palette))
      {
        tone_bits = palette[4];
        color_offs = palette[5];  color_bits = palette[6];
        pencil_offs = palette[7];  pencil_bits = palette[8];
        offset_mask = palette[9];
      }
      else
      {
        tone_bits = 4;
        color_offs = 4;  color_bits = 7;
        pencil_offs = 11;  pencil_bits = 5;
        offset_mask = 0;
      }
      enc = tif_toonz1_encode_cm16((USHORT*)buffer, (int)(bytes >> 1),
        (UCHAR*)tif->tif_rawcp,
        tone_bits,
        color_offs, color_bits,
        pencil_offs, pencil_bits, offset_mask);
      break;

    case 32:
      enc = tif_toonz1_encode_cm24((TUINT32*)buffer, (int)(bytes >> 2),
        (UCHAR*)tif->tif_rawcp);
      break;

    default:
      assert(0);
    }
    assert(enc);
    tif->tif_rawcc += enc;
    tif->tif_rawcp += enc;
    if (tif->tif_rawcc >= tif->tif_rawdatasize)
      if (!TIFFFlushData1(tif))
        return -1;

    return 1;
  }



}
#endif