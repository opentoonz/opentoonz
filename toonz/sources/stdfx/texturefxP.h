#pragma once

#include "tpixelutils.h"

namespace {

enum { SUBSTITUTE, PATTERNTYPE, ADD, SUBTRACT, MULTIPLY, LIGHTEN, DARKEN };

typedef void (*func32)(TPixel32 &pixmask, const TPixel32 &pixtext, double v);
typedef void (*func64)(TPixel64 &pixmask, const TPixel64 &pixtext, double v);

//=======================================================================================

//  Pixel operations

//-----------------------------------------------------------------------------

//! Make \b pixout completely opaque if \b pixcomp is not fully transparent.
//! This helps
//! in avoiding antialias merge problems when overing palette filtered masks
//! with their
//! inverses.
template <class T>
inline void makeOpaque(T &pixout, const T &pixcomp, double v) {
  if (pixcomp.m > 0) {
    double k = T::maxChannelValue / (double)pixout.m;
    pixout.r = troundp(k * pixout.r);
    pixout.g = troundp(k * pixout.g);
    pixout.b = troundp(k * pixout.b);
    pixout.m = T::maxChannelValue;
  }
}

//---------------------------------------------------------------------------------------

//! Copies \b pixin into \b pixout - while matte components are multiplied.
template <class T>
inline void substitute(T &pixout, const T &pixin, double v) {
  double k = pixout.m / (double)T::maxChannelValue;
  pixout.r = k * pixin.r;
  pixout.g = k * pixin.g;
  pixout.b = k * pixin.b;
  pixout.m = k * pixin.m;
}

//---------------------------------------------------------------------------------------

//! Decrease \b pixout's rgb colors proportionally to \b pixin's value. The
//! matte channel
//! is kept unaltered.
inline void pattern32(TPixel32 &pixout, const TPixel32 &pixin, double v) {
  double val = TPixelGR8::from(pixin).value / 255.0;
  pixout.r   = troundp(val * pixout.r);
  pixout.g   = troundp(val * pixout.g);
  pixout.b   = troundp(val * pixout.b);
}

inline void pattern64(TPixel64 &pixout, const TPixel64 &pixin, double v) {
  double val = TPixelGR16::from(pixin).value / 65535.0;
  pixout.r   = troundp(val * pixout.r);
  pixout.g   = troundp(val * pixout.g);
  pixout.b   = troundp(val * pixout.b);
}

//---------------------------------------------------------------------------------------

template <class T>
void textureAdd(T &pixout, const T &pixin, double v) {
  if (pixin.m > 0) {
    TINT32 pixoutm = pixout.m;

    double k = T::maxChannelValue / (double)pixoutm;
    pixout.r = pixout.r * k;
    pixout.g = pixout.g * k;
    pixout.b = pixout.b * k;
    pixout.m = T::maxChannelValue;

    add(pixout, pixin, v);

    k        = pixoutm / (double)T::maxChannelValue;
    pixout.r = pixout.r * k;
    pixout.g = pixout.g * k;
    pixout.b = pixout.b * k;
    pixout.m = pixoutm;
  }
}

//---------------------------------------------------------------------------------------

template <class T>
void textureSub(T &pixout, const T &pixin, double v) {
  if (pixin.m > 0) {
    TINT32 pixoutm = pixout.m;

    double k = T::maxChannelValue / (double)pixoutm;
    pixout.r = pixout.r * k;
    pixout.g = pixout.g * k;
    pixout.b = pixout.b * k;
    pixout.m = T::maxChannelValue;

    sub(pixout, pixin, v);

    k        = pixoutm / (double)T::maxChannelValue;
    pixout.r = pixout.r * k;
    pixout.g = pixout.g * k;
    pixout.b = pixout.b * k;
    pixout.m = pixoutm;
  }
}

//---------------------------------------------------------------------------------------

template <class T>
void textureMult(T &pixout, const T &pixin, double v) {
  TINT32 pixoutm = pixout.m;

  double k = T::maxChannelValue / (double)pixoutm;
  pixout.r = pixout.r * k;
  pixout.g = pixout.g * k;
  pixout.b = pixout.b * k;
  pixout.m = T::maxChannelValue;

  mult(pixout, pixin, v);

  k        = pixoutm / (double)T::maxChannelValue;
  pixout.r = pixout.r * k;
  pixout.g = pixout.g * k;
  pixout.b = pixout.b * k;
  pixout.m = pixoutm;
}

//---------------------------------------------------------------------------------------

template <class T>
void textureLighten(T &pixout, const T &pixin, double v) {
  TINT32 pixoutm = pixout.m;

  double k = T::maxChannelValue / (double)pixoutm;
  pixout.r = pixout.r * k;
  pixout.g = pixout.g * k;
  pixout.b = pixout.b * k;
  pixout.m = T::maxChannelValue;

  lighten(pixout, pixin, v);

  k        = pixoutm / (double)T::maxChannelValue;
  pixout.r = pixout.r * k;
  pixout.g = pixout.g * k;
  pixout.b = pixout.b * k;
  pixout.m = pixoutm;
}

//---------------------------------------------------------------------------------------

template <class T>
void textureDarken(T &pixout, const T &pixin, double v) {
  TINT32 pixoutm = pixout.m;

  double k = T::maxChannelValue / (double)pixoutm;
  pixout.r = pixout.r * k;
  pixout.g = pixout.g * k;
  pixout.b = pixout.b * k;
  pixout.m = T::maxChannelValue;

  darken(pixout, pixin, v);

  k        = pixoutm / (double)T::maxChannelValue;
  pixout.r = pixout.r * k;
  pixout.g = pixout.g * k;
  pixout.b = pixout.b * k;
  pixout.m = pixoutm;
}

//=======================================================================================

void myOver32(const TRaster32P &rasOut, const TRasterP &rasUp, func32 func,
              double v) {
  assert(rasOut->getSize() == rasUp->getSize());
  TRaster32P rasUp32 = rasUp;
  assert(rasUp32);
  for (int y = rasOut->getLy(); --y >= 0;) {
    TPixel32 *out_pix       = rasOut->pixels(y);
    TPixel32 *const out_end = out_pix + rasOut->getLx();
    const TPixel32 *up_pix  = rasUp32->pixels(y);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (out_pix->m > 0) (*func)(*out_pix, *up_pix, v);
    }
  }
}

void myOver64(const TRaster64P &rasOut, const TRasterP &rasUp, func64 func,
              double v) {
  assert(rasOut->getSize() == rasUp->getSize());
  TRaster64P rasUp64 = rasUp;
  assert(rasUp64);
  for (int y = rasOut->getLy(); --y >= 0;) {
    TPixel64 *out_pix       = rasOut->pixels(y);
    TPixel64 *const out_end = out_pix + rasOut->getLx();
    const TPixel64 *up_pix  = rasUp64->pixels(y);
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (out_pix->m > 0) (*func)(*out_pix, *up_pix, v);
    }
  }
}
}
