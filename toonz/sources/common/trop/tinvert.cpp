

#include "trop.h"
#include "tpixelgr.h"

namespace {
template <typename PixType>
inline void do_invert(TRasterPT<PixType> ras) {
  int wrap         = ras->getWrap();
  int lx           = ras->getLx();
  PixType *rowIn   = ras->pixels();
  PixType *lastPix = rowIn + wrap * ras->getLy();
  PixType *pixIn   = 0;
  PixType *endPix  = 0;

  while (pixIn < lastPix) {
    pixIn  = rowIn;
    endPix = pixIn + lx;
    while (pixIn < endPix) {
      pixIn->r = pixIn->m - pixIn->r;
      pixIn->g = pixIn->m - pixIn->g;
      pixIn->b = pixIn->m - pixIn->b; /*pixIn->m = pixIn->m;*/

      ++pixIn;
    }
    rowIn += wrap;
  }
}
//------------------------------------------------------------------------------

template <typename PixType>
inline void do_invert(TRasterPT<PixType> ras, bool invRed, bool invGreen,
                      bool invBlue, bool invMatte) {
  int wrap         = ras->getWrap();
  int lx           = ras->getLx();
  PixType *rowIn   = ras->pixels();
  PixType *lastPix = rowIn + wrap * ras->getLy();
  PixType *pixIn   = 0;
  PixType *endPix  = 0;

  while (pixIn < lastPix) {
    pixIn  = rowIn;
    endPix = pixIn + lx;
    while (pixIn < endPix) {
      if (invRed) pixIn->r   = pixIn->m - pixIn->r;
      if (invGreen) pixIn->g = pixIn->m - pixIn->g;
      if (invBlue) pixIn->b  = pixIn->m - pixIn->b;
      if (invMatte) pixIn->m = ~pixIn->m;
      ++pixIn;
    }
    rowIn += wrap;
  }
}

//------------------------------------------------------------------------------

template <>
inline void do_invert<TPixelGR8>(TRasterPT<TPixelGR8> ras) {
  int wrap           = ras->getWrap();
  int lx             = ras->getLx();
  TPixelGR8 *rowIn   = ras->pixels();
  TPixelGR8 *lastPix = rowIn + wrap * ras->getLy();
  TPixelGR8 *pixIn   = 0;
  TPixelGR8 *endPix  = 0;

  while (pixIn < lastPix) {
    pixIn  = rowIn;
    endPix = pixIn + lx;
    while (pixIn < endPix) {
      pixIn->value = 255 - pixIn->value;

      ++pixIn;
    }
    rowIn += wrap;
  }
}
}

//------------------------------------------------------------------------------

void TRop::invert(TRasterP ras, bool invRed, bool invGreen, bool invBlue,
                  bool invMatte) {
  if (!invRed && !invGreen && !invBlue) return;
  bool flag = invRed && invGreen && invBlue && !invMatte;

  TRaster32P ras32 = ras;
  ras->lock();
  if (ras32)
    if (flag)
      do_invert<TPixel32>(ras32);
    else
      do_invert<TPixel32>(ras, invRed, invGreen, invBlue, invMatte);
  else {
    TRaster64P ras64 = ras;
    if (ras64)
      if (flag)
        do_invert<TPixel64>(ras64);
      else
        do_invert<TPixel64>(ras64, invRed, invGreen, invBlue, invMatte);
    else {
      TRasterGR8P ras8 = ras;
      if (ras8)
        do_invert<TPixelGR8>(ras8);
      else {
        ras->unlock();
        throw TRopException("unsupported pixel type");
      }
    }
  }
  ras->unlock();
}

//------------------------------------------------------------------------------
