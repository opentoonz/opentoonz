

#include "trop.h"

// TnzCore includes
#include "tpixelgr.h"
#include "trandom.h"
#include "tpixelutils.h"

//******************************************************************
//    Conversion functions
//******************************************************************

static void do_convert(const TRaster64P &dst, const TRaster32P &src) {
  assert(dst->getSize() == src->getSize());
  int lx = src->getLx();
  for (int y = 0; y < src->getLy(); y++) {
    TPixel64 *outPix   = dst->pixels(y);
    TPixel32 *inPix    = src->pixels(y);
    TPixel32 *inEndPix = inPix + lx;
    for (; inPix < inEndPix; ++outPix, ++inPix) {
      outPix->r = ushortFromByte(inPix->r);
      outPix->g = ushortFromByte(inPix->g);
      outPix->b = ushortFromByte(inPix->b);
      outPix->m = ushortFromByte(inPix->m);
    }
  }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRasterGR8P &dst, const TRaster32P &src) {
  assert(dst->getSize() == src->getSize());

  int lx = src->getLx();

  for (int y = 0; y < src->getLy(); ++y) {
    TPixelGR8 *outPix = dst->pixels(y);
    TPixel32 *inPix = src->pixels(y), *inEndPix = inPix + lx;

    for (; inPix < inEndPix; ++outPix, ++inPix)
      *outPix = TPixelGR8::from(overPix(TPixel32::White, *inPix));
  }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRasterGR16P &dst, const TRaster32P &src) {
  assert(dst->getSize() == src->getSize());

  int lx = src->getLx();

  for (int y = 0; y < src->getLy(); ++y) {
    TPixelGR16 *outPix = dst->pixels(y);
    TPixel32 *inPix = src->pixels(y), *inEndPix = inPix + lx;

    for (; inPix < inEndPix; ++outPix, ++inPix)
      outPix->value =
          257 * (TPixelGR8::from(overPix(TPixel32::White, *inPix))).value;
  }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRasterGR16P &dst, const TRaster64P &src) {
  assert(dst->getSize() == src->getSize());
  int lx = src->getLx();
  for (int y = 0; y < src->getLy(); y++) {
    TPixelGR16 *outPix = dst->pixels(y);
    TPixel64 *inPix    = src->pixels(y);
    TPixel64 *inEndPix = inPix + lx;
    while (inPix < inEndPix) {
      outPix->value = (inPix->r + 2 * inPix->g + inPix->b) >> 2;
      outPix++;
      inPix++;
    }
  }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRaster32P &dst, const TRasterGR8P &src) {
  assert(dst->getSize() == src->getSize());
  int lx = src->getLx();
  for (int y = 0; y < src->getLy(); y++) {
    TPixel32 *outPix    = dst->pixels(y);
    TPixelGR8 *inPix    = src->pixels(y);
    TPixelGR8 *inEndPix = inPix + lx;
    while (inPix < inEndPix) {
      outPix->r = inPix->value;
      outPix->g = inPix->value;
      outPix->b = inPix->value;
      outPix->m = 0xff;
      outPix++;
      inPix++;
    }
  }
}

//-----------------------------------------------------------------------------

#define USHORT2BYTE_MAGICFAC (256U * 255U + 1U)

inline UCHAR ditherUcharFromUshort(USHORT in, UINT rndNum) {
  return ((((in * USHORT2BYTE_MAGICFAC) - ((in * USHORT2BYTE_MAGICFAC) >> 24)) +
           rndNum) >>
          24);
}

inline void ditherRgbmFromRgbm64(TPixel32 &out, const TPixel64 &in,
                                 TRandom &rnd) {
  UINT randomRound;
  randomRound = rnd.getUInt() & ((1U << 24) - 1);

  out.r = ditherUcharFromUshort(in.r, randomRound);
  out.g = ditherUcharFromUshort(in.g, randomRound);
  out.b = ditherUcharFromUshort(in.b, randomRound);
  out.m = ditherUcharFromUshort(in.m, randomRound);
}

//-----------------------------------------------------------------------------

inline void ditherConvert(TRaster64P inRas, TRaster32P outRas) {
  int inWrap  = inRas->getWrap();
  int outWrap = outRas->getWrap();

  TPixel64 *inPix = 0, *inRow = inRas->pixels();
  TPixel32 *outPix, *outRow   = outRas->pixels();
  TPixel64 *endPix;
  int inLx          = inRas->getLx();
  TPixel64 *lastPix = inRow + inWrap * (inRas->getLy() - 1) + inLx;

  TRandom rnd(130266);

  while (inPix < lastPix) {
    inPix  = inRow;
    outPix = outRow;
    endPix = inPix + inLx;
    while (inPix < endPix) {
      ditherRgbmFromRgbm64(*outPix, *inPix, rnd);
      inPix++;
      outPix++;
    }
    inRow += inWrap;
    outRow += outWrap;
  }
}

//******************************************************************
//    Obsolete conversion functions
//******************************************************************

static void do_convert(const TRasterCM32P &dst, const TRasterGR8P &src) {
  assert(dst->getSize() == src->getSize());
  TPixelCM32 bg = TPixelCM32(0, 0, TPixelCM32::getMaxTone());

  int lx = src->getLx();
  for (int y = 0; y < src->getLy(); y++) {
    TPixelCM32 *outPix  = dst->pixels(y);
    TPixelGR8 *inPix    = src->pixels(y);
    TPixelGR8 *inEndPix = inPix + lx;
    while (inPix < inEndPix) {
      *outPix = (inPix->value == 255) ? bg : TPixelCM32(1, 0, inPix->value);
      outPix++;
      inPix++;
    }
  }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRasterCM32P &dst, const TRaster32P &src) {
  assert(dst->getSize() == src->getSize());
  TPixelCM32 bg = TPixelCM32(0, 0, TPixelCM32::getMaxTone());

  int lx = src->getLx();

  bool isOverlay = false;

  for (int y = 0; y < src->getLy(); y++)  // if it is an overlay, I use the
                                          // matte value for inks, otherwise I
                                          // use the brightness.
  {
    TPixel32 *inPix    = src->pixels(y);
    TPixel32 *inEndPix = inPix + lx;
    while (inPix < inEndPix) {
      if (inPix->m != 255) {
        isOverlay = true;
        break;
      }
      inPix++;
    }
    if (isOverlay) break;
  }

  if (isOverlay)
    for (int y = 0; y < src->getLy(); y++) {
      TPixelCM32 *outPix = dst->pixels(y);
      TPixel32 *inPix    = src->pixels(y);
      TPixel32 *inEndPix = inPix + lx;
      while (inPix < inEndPix) {
        *outPix = (inPix->m == 0) ? bg : TPixelCM32(1, 0, 255 - inPix->m);
        outPix++;
        inPix++;
      }
    }
  else
    for (int y = 0; y < src->getLy(); y++) {
      TPixelCM32 *outPix = dst->pixels(y);
      TPixel32 *inPix    = src->pixels(y);
      TPixel32 *inEndPix = inPix + lx;
      while (inPix < inEndPix) {
        UCHAR val = TPixelGR8::from(*inPix).value;
        *outPix   = (val == 255) ? bg : TPixelCM32(1, 0, val);
        outPix++;
        inPix++;
      }
    }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRasterYUV422P &dst, const TRaster32P &src) {
  assert(src->getLx() & 0);
  long y1, y2, u, v, u1, u2, v1, v2;
  TPixel32 *pix     = (TPixel32 *)src->pixels();
  TPixel32 *lastPix = &(src->pixels(src->getLy() - 1)[src->getLx() - 1]);

  UCHAR *out = dst->getRawData();

  while (pix < lastPix) {
    /* first pixel gives Y and 0.5 of chroma */

    y1 = 16829 * pix->r + 33039 * pix->g + 6416 * pix->b;
    u1 = -4831 * pix->r + -9488 * pix->g + 14319 * pix->b;
    v1 = 14322 * pix->r + -11992 * pix->g + -2330 * pix->b;

    /* second pixel gives Y and 0.5 of chroma */
    ++pix;

    y2 = 16829 * pix->r + 33039 * pix->g + 6416 * pix->b;
    u2 = -4831 * pix->r + -9488 * pix->g + 14319 * pix->b;
    v2 = 14322 * pix->r + -11992 * pix->g + -2330 * pix->b;

    /* average the chroma */
    u = u1 + u2;
    v = v1 + v2;

    /* round the chroma */
    u1 = (u + 0x008000) >> 16;
    v1 = (v + 0x008000) >> 16;

    /* limit the chroma */
    if (u1 < -112) u1 = -112;
    if (u1 > 111) u1  = 111;
    if (v1 < -112) v1 = -112;
    if (v1 > 111) v1  = 111;

    /* limit the lum */
    if (y1 > 0x00dbffff) y1 = 0x00dbffff;
    if (y2 > 0x00dbffff) y2 = 0x00dbffff;

    /* save the results */
    *out++ = (UCHAR)(u1 + 128);
    *out++ = (UCHAR)((y1 >> 16) + 16);
    *out++ = (UCHAR)(v1 + 128);
    *out++ = (UCHAR)((y2 >> 16) + 16);
    ++pix;
  }
}

//-----------------------------------------------------------------------------

static void do_convert(const TRaster32P &dst, const TRasterYUV422P &src) {
  int long r, g, b, y1, y2, u, v;
  TPixel32 *buf     = dst->pixels();
  const UCHAR *in   = src->getRawData();
  const UCHAR *last = in + src->getRowSize() * src->getLy() - 1;

  while (in < last) {
    u = *in;
    u -= 128;
    in++;
    y1 = *in;
    y1 -= 16;
    in++;
    v = *in;
    v -= 128;
    in++;
    y2 = *in;
    y2 -= 16;
    in++;

    r                   = 76310 * y1 + 104635 * v;
    if (r > 0xFFFFFF) r = 0xFFFFFF;
    if (r <= 0xFFFF) r  = 0;

    g                   = 76310 * y1 + -25690 * u + -53294 * v;
    if (g > 0xFFFFFF) g = 0xFFFFFF;
    if (g <= 0xFFFF) g  = 0;

    b                   = 76310 * y1 + 132278 * u;
    if (b > 0xFFFFFF) b = 0xFFFFFF;
    if (b <= 0xFFFF) b  = 0;

    buf->r = (UCHAR)(r >> 16);
    buf->g = (UCHAR)(g >> 16);
    buf->b = (UCHAR)(b >> 16);
    buf->m = (UCHAR)255;
    buf++;

    r                   = 76310 * y2 + 104635 * v;
    if (r > 0xFFFFFF) r = 0xFFFFFF;
    if (r <= 0xFFFF) r  = 0;

    g                   = 76310 * y2 + -25690 * u + -53294 * v;
    if (g > 0xFFFFFF) g = 0xFFFFFF;
    if (g <= 0xFFFF) g  = 0;

    b                   = 76310 * y2 + 132278 * u;
    if (b > 0xFFFFFF) b = 0xFFFFFF;
    if (b <= 0xFFFF) b  = 0;

    buf->r = (UCHAR)(r >> 16);
    buf->g = (UCHAR)(g >> 16);
    buf->b = (UCHAR)(b >> 16);
    buf->m = (UCHAR)255;
    buf++;
  }
}

//******************************************************************
//    Main conversion function
//******************************************************************

void TRop::convert(TRasterP dst, const TRasterP &src) {
  if (dst->getSize() != src->getSize())
    throw TRopException("convert: size mismatch");

  TRaster32P dst32   = dst;
  TRasterGR8P dst8   = dst;
  TRasterGR16P dst16 = dst;
  TRaster64P dst64   = dst;
  TRasterCM32P dstCm = dst;

  TRaster32P src32      = src;
  TRasterGR8P src8      = src;
  TRaster64P src64      = src;
  TRasterYUV422P srcYUV = src;
  TRasterYUV422P dstYUV = dst;

  src->lock();
  dst->lock();

  if (dst64 && src32)
    do_convert(dst64, src32);
  else if (dst8 && src32)
    do_convert(dst8, src32);
  else if (dst16 && src32)
    do_convert(dst16, src32);
  else if (dst32 && src64)
    ditherConvert(src64, dst32);
  else if (dst16 && src64)
    do_convert(dst16, src64);
  else if (dst32 && src8)
    do_convert(dst32, src8);
  else if (dstYUV && src32)
    do_convert(dstYUV, src32);  // Obsolete conversions
  else if (dst32 && srcYUV)
    do_convert(dst32, srcYUV);  //
  else if (dstCm && src32)
    do_convert(dstCm, src32);  //
  else if (dstCm && src8)
    do_convert(dstCm, src8);  //
  else {
    dst->unlock();
    src->unlock();

    throw TRopException("unsupported pixel type");
  }

  dst->unlock();
  src->unlock();
}
