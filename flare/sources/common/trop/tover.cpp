

#include "quickputP.h"
#include "tpixelutils.h"
#include "trastercm.h"
#include "tsystem.h"
#include "tropcm.h"
#include "tpalette.h"

#if defined(_WIN32) && defined(x64)
#define USE_SSE2
#endif

#ifdef USE_SSE2
#include <emmintrin.h>  // per SSE2
#endif

//-----------------------------------------------------------------------------
namespace {

inline bool transp(const TPixel32 &p) { return p.m == 0; }
inline bool transp(const TPixel64 &p) { return p.m == 0; }

//-----------------------------------------------------------------------------

inline bool opaque(const TPixel32 &p) { return p.m == 0xff; }
inline bool opaque(const TPixel64 &p) { return p.m == 0xffff; }

//-----------------------------------------------------------------------------

#define MODO2
#define VELOCE
template <class T>
void do_overT3(TRasterPT<T> rout, const TRasterPT<T> &rdn,
               const TRasterPT<T> &rup) {
  for (int y = 0; y < rout->getLy(); y++) {
#ifdef MODO1
    const T *dn_pix = rdn->pixels(y);
    const T *up_pix = rup->pixels(y);
    T *out_pix      = rout->pixels(y);

#else
#ifdef MODO2
    const T *dn_pix = ((T *)rdn->getRawData()) + y * rdn->getWrap();
    const T *up_pix = ((T *)rup->getRawData()) + y * rup->getWrap();

    T *out_pix = ((T *)rout->getRawData()) + y * rout->getWrap();
#endif
#endif

    const T *dn_limit = dn_pix + rdn->getLx();
    for (; dn_pix < dn_limit; dn_pix++, up_pix++, out_pix++) {
#ifdef VELOCE
      if (transp(*up_pix))
        *out_pix = *dn_pix;
      else if (opaque(*up_pix))
        *out_pix = *up_pix;
      else {
        *out_pix = overPix(*dn_pix, *up_pix);
      }
#else

      T topval = *up_pix;
      if (transp(topval))
        *out_pix = *dn_pix;
      else if (opaque(topval))
        *out_pix = topval;
      else {
        *out_pix = overPix(*dn_pix, topval);
      }
#endif
    }
  }
}

//-----------------------------------------------------------------------------

template <typename PixTypeOut, typename PixTypeDn, typename PixTypeUp>
void do_over(TRasterPT<PixTypeOut> rout, const TRasterPT<PixTypeDn> &rdn,
             const TRasterPT<PixTypeUp> &rup, const TRasterGR8P rmask) {
  for (int y = 0; y < rout->getLy(); y++) {
    const PixTypeDn *dn_pix =
        ((PixTypeDn *)rdn->getRawData()) + y * rdn->getWrap();
    const PixTypeUp *up_pix =
        ((PixTypeUp *)rup->getRawData()) + y * rup->getWrap();

    PixTypeOut *out_pix =
        ((PixTypeOut *)rout->getRawData()) + y * rout->getWrap();
    TPixelGR8 *mask_pix =
        ((TPixelGR8 *)rmask->getRawData()) + y * rmask->getWrap();

    const PixTypeDn *dn_limit = dn_pix + rout->getLx();
    for (; dn_pix < dn_limit; dn_pix++, up_pix++, out_pix++, mask_pix++) {
      if (mask_pix->value == 0x00)
        *out_pix = *dn_pix;
      else if (mask_pix->value == 0xff)
        *out_pix = *up_pix;
      else {
        PixTypeUp p(*up_pix);
        p.m      = mask_pix->value;
        *out_pix = overPix(*dn_pix, p);  // hei!
      }
    }
  }
}

//-----------------------------------------------------------------------------

void do_over(TRasterCM32P rout, const TRasterCM32P &rup) {
  assert(rout->getSize() == rup->getSize());
  for (int y = 0; y < rout->getLy(); y++) {
    TPixelCM32 *out_pix       = rout->pixels(y);
    TPixelCM32 *const out_end = out_pix + rout->getLx();
    const TPixelCM32 *up_pix  = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (!up_pix->isPureInk() && up_pix->getPaint() != 0)  // BackgroundStyle)
        *out_pix = *up_pix;
      else if (!up_pix->isPurePaint()) {
        TUINT32 *outl = (TUINT32 *)out_pix, *upl = (TUINT32 *)up_pix;

        *outl = ((*upl) & (TPixelCM32::getInkMask())) |
                ((*outl) & (TPixelCM32::getPaintMask())) |
                std::min(up_pix->getTone(), out_pix->getTone());
      }
    }
  }
}

//-----------------------------------------------------------------------------

template <class T, class Q>
void do_overT2(TRasterPT<T> rout, const TRasterPT<T> &rup) {
  UINT max    = T::maxChannelValue;
  double maxD = max;

  assert(rout->getSize() == rup->getSize());
  for (int y = 0; y < rout->getLy(); y++) {
    T *out_pix       = rout->pixels(y);
    T *const out_end = out_pix + rout->getLx();
    const T *up_pix  = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (up_pix->m == max)
        *out_pix = *up_pix;
      else if (up_pix->m > 0) {
        TUINT32 r, g, b;
        r = up_pix->r + (out_pix->r * (max - up_pix->m)) / maxD;
        g = up_pix->g + (out_pix->g * (max - up_pix->m)) / maxD;
        b = up_pix->b + (out_pix->b * (max - up_pix->m)) / maxD;

        out_pix->r = (r < max) ? (Q)r : (Q)max;
        out_pix->g = (g < max) ? (Q)g : (Q)max;
        out_pix->b = (b < max) ? (Q)b : (Q)max;
        out_pix->m = up_pix->m + (out_pix->m * (max - up_pix->m)) / maxD;
      }
    }
  }
}

//-----------------------------------------------------------------------------

#ifdef USE_SSE2

void do_over_SSE2(TRaster32P rout, const TRaster32P &rup) {
  __m128i zeros = _mm_setzero_si128();
  __m128i out_pix_packed_i, up_pix_packed_i;
  __m128 out_pix_packed, up_pix_packed;

  float maxChannelValue    = 255.0;
  float maxChannelValueInv = 1.0f / maxChannelValue;

  __m128 maxChanneValue_packed = _mm_load1_ps(&maxChannelValue);

  assert(rout->getSize() == rup->getSize());
  for (int y = 0; y < rout->getLy(); y++) {
    TPixel32 *out_pix       = rout->pixels(y);
    TPixel32 *const out_end = out_pix + rout->getLx();
    const TPixel32 *up_pix  = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (up_pix->m == 0xff)
        *out_pix = *up_pix;
      else if (up_pix->m > 0) {
        float factor         = (255.0f - up_pix->m) / 255.0f;
        __m128 factor_packed = _mm_load1_ps(&factor);

        // carica up_pix e out_pix in due registri a 128 bit
        up_pix_packed_i =
            _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)up_pix), zeros);
        up_pix_packed =
            _mm_cvtepi32_ps(_mm_unpacklo_epi16(up_pix_packed_i, zeros));

        out_pix_packed_i =
            _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD *)out_pix), zeros);
        out_pix_packed =
            _mm_cvtepi32_ps(_mm_unpacklo_epi16(out_pix_packed_i, zeros));

        out_pix_packed = _mm_add_ps(up_pix_packed,
                                    _mm_mul_ps(out_pix_packed, factor_packed));
        out_pix_packed = _mm_min_ps(maxChanneValue_packed, out_pix_packed);

        out_pix_packed_i    = _mm_cvtps_epi32(out_pix_packed);
        out_pix_packed_i    = _mm_packs_epi32(out_pix_packed_i, zeros);
        out_pix_packed_i    = _mm_packus_epi16(out_pix_packed_i, zeros);
        *(DWORD *)(out_pix) = _mm_cvtsi128_si32(out_pix_packed_i);
      }
    }
  }
}

#endif

//-----------------------------------------------------------------------------

void do_over(TRaster32P rout, const TRasterGR8P &rup) {
  assert(rout->getSize() == rup->getSize());
  for (int y = rout->getLy(); --y >= 0;) {
    TPixel32 *out_pix       = rout->pixels(y);
    TPixel32 *const out_end = out_pix + rout->getLx();
    const TPixelGR8 *up_pix = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      int v      = up_pix->value;
      out_pix->r = out_pix->r * v / 255;
      out_pix->g = out_pix->r;
      out_pix->b = out_pix->r;
    }
  }
}

//-----------------------------------------------------------------------------

void do_over(TRasterGR8P rout, const TRaster32P &rup) {
  assert(rout->getSize() == rup->getSize());
  for (int y = rout->getLy(); --y >= 0;) {
    TPixelGR8 *out_pix       = rout->pixels(y);
    TPixelGR8 *const out_end = out_pix + rout->getLx();
    const TPixel32 *up_pix   = rup->pixels(y);
    TPixel32 *temp_pix       = new TPixel32();
    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      temp_pix->r        = out_pix->value;
      temp_pix->g        = out_pix->value;
      temp_pix->b        = out_pix->value;
      temp_pix->m        = 0xff;
      TPixel32 out32_pix = overPix(*temp_pix, *up_pix);
      *out_pix           = out_pix->from(out32_pix);
    }
    delete temp_pix;
  }
}

//-----------------------------------------------------------------------------

void do_over(TRasterFP rout, const TRasterFP &rup) {
  assert(rout->getSize() == rup->getSize());
  for (int y = 0; y < rout->getLy(); y++) {
    TPixelF *out_pix       = rout->pixels(y);
    TPixelF *const out_end = out_pix + rout->getLx();
    const TPixelF *up_pix  = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      if (up_pix->m >= 1.f)
        *out_pix = *up_pix;
      else if (up_pix->m > 0.f) {
        out_pix->r = up_pix->r + out_pix->r * (1.f - up_pix->m);
        out_pix->g = up_pix->g + out_pix->g * (1.f - up_pix->m);
        out_pix->b = up_pix->b + out_pix->b * (1.f - up_pix->m);
        out_pix->m = up_pix->m + out_pix->m * (1.f - up_pix->m);
      }
    }
  }
}

}  // namespace

//-----------------------------------------------------------------------------

static void do_over(TRaster32P rout, const TRasterGR8P &rup,
                    const TPixel32 &color) {
  assert(rout->getSize() == rup->getSize());
  for (int y = rout->getLy(); --y >= 0;) {
    TPixel32 *out_pix       = rout->pixels(y);
    TPixel32 *const out_end = out_pix + rout->getLx();
    const TPixelGR8 *up_pix = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      double v = up_pix->value / 255.0;
      TPixel32 up(troundp(v * color.r), troundp(v * color.g),
                  troundp(v * color.b), troundp(v * color.m));
      *out_pix = overPix(*out_pix, up);
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::over(TRaster32P rout, const TRasterGR8P &rup,
                const TPixel32 &color) {
  rout->lock();
  do_over(rout, rup, color);
  rout->unlock();
}

//-----------------------------------------------------------------------------

void TRop::over(const TRasterP &rout, const TRasterP &rdn,
                const TRasterP &rup) {
  TRect rect = rout->getBounds() * rdn->getBounds() * rup->getBounds();
  if (rect.isEmpty()) return;

  TRasterP cRout = rout->extract(rect);
  TRasterP cRdn  = rdn->extract(rect);
  TRasterP cRup  = rup->extract(rect);
  rout->lock();
  rdn->lock();
  rup->lock();
  TRaster32P rout32 = cRout, rdn32 = cRdn, rup32 = cRup;
  TRaster64P rout64 = cRout, rdn64 = cRdn, rup64 = cRup;
  if (rout32 && rdn32 && rup32)
    do_overT3<TPixel32>(rout32, rdn32, rup32);
  else if (rout64 && rdn64 && rup64)
    do_overT3<TPixel64>(rout64, rdn64, rup64);
  else {
    rout->unlock();
    rdn->unlock();
    rup->unlock();
    throw TRopException("unsupported pixel type");
  }

  rout->unlock();
  rdn->unlock();
  rup->unlock();
}

//-----------------------------------------------------------------------------

void TRop::over(const TRasterP &rout, const TRasterP &rup, const TPoint &pos) {
  TRect outRect(rout->getBounds());
  TRect upRect(rup->getBounds() + pos);
  TRect intersection = outRect * upRect;
  if (intersection.isEmpty()) return;

  TRasterP cRout = rout->extract(intersection);
  TRect r        = intersection - pos;
  TRasterP cRup  = rup->extract(r);

  TRaster32P rout32 = cRout, rup32 = cRup;
  TRaster64P rout64 = cRout, rup64 = cRup;
  TRasterFP routF = cRout, rupF = cRup;

  TRasterGR8P rout8 = cRout, rup8 = cRup;

  TRasterCM32P routCM32 = cRout, rupCM32 = cRup;

  rout->lock();
  rup->lock();

  // TRaster64P rout64 = rout, rin64 = rin;
  if (rout32 && rup32) {
#ifdef USE_SSE2
    if (TSystem::getCPUExtensions() & TSystem::CpuSupportsSse2)
      do_over_SSE2(rout32, rup32);
    else
#endif
      do_overT2<TPixel32, UCHAR>(rout32, rup32);
  } else if (rout64) {
    if (!rup64) {
      TRaster64P raux(cRup->getSize());
      TRop::convert(raux, cRup);
      rup64 = raux;
    }
    do_overT2<TPixel64, USHORT>(rout64, rup64);
  } else if (rout32 && rup8)
    do_over(rout32, rup8);
  else if (rout8 && rup32)
    do_over(rout8, rup32);
  else if (rout8 && rup8)
    TRop::copy(rout8, rup8);
  else if (routCM32 && rupCM32)
    do_over(routCM32, rupCM32);
  else if (routF && rupF)
    do_over(routF, rupF);
  else {
    rout->unlock();
    rup->unlock();
    throw TRopException("unsupported pixel type");
  }

  rout->unlock();
  rup->unlock();
}

//-----------------------------------------------------------------------------

static void addBackground32(TRaster32P ras, const TPixel32 &col) {
  ras->lock();
  int nrows = ras->getLy();
  while (nrows-- > 0) {
    TPixel32 *pix    = ras->pixels(nrows);
    TPixel32 *endPix = pix + ras->getLx();
    while (pix < endPix) {
      *pix = overPix(col, *pix);
      pix++;
    }
  }
  ras->unlock();
}

//-----------------------------------------------------------------------------

void TRop::addBackground(TRasterP ras, const TPixel32 &col) {
  TRaster32P ras32 = ras;
  if (ras32)
    addBackground32(ras32, col);
  else
    throw TRopException("unsupported pixel type");
}

//===================================================================

// Usata tinylinetest
static void my_do_over(TRaster32P rout, const TRasterGR8P &rup) {
  assert(rout->getSize() == rup->getSize());
  for (int y = rout->getLy(); --y >= 0;) {
    TPixel32 *out_pix       = rout->pixels(y);
    TPixel32 *const out_end = out_pix + rout->getLx();
    const TPixelGR8 *up_pix = rup->pixels(y);

    for (; out_pix < out_end; ++out_pix, ++up_pix) {
      int v      = up_pix->value;
      out_pix->r = out_pix->r * v / 255;
      out_pix->g = out_pix->r;
      out_pix->b = out_pix->r;
    }
  }
}

//===================================================================

void TRop::over(const TRasterP &out, const TRasterP &up, const TAffine &aff,
                ResampleFilterType filterType) {
  out->lock();
  up->lock();

  if (filterType == ClosestPixel || filterType == Bilinear)
    ::quickPut(out, up, aff, filterType);
  else {
    TRect rasterBounds = up->getBounds();
    TRectD dbounds(rasterBounds.x0, rasterBounds.y0, rasterBounds.x1 + 1,
                   rasterBounds.y1 + 1);
    dbounds = aff * dbounds;
    TRect bounds(tfloor(dbounds.x0), tfloor(dbounds.y0), tceil(dbounds.x1) - 1,
                 tceil(dbounds.y1) - 1);
    TRasterP tmp = up->create(bounds.getLx(), bounds.getLy());
    resample(tmp, up, TTranslation(-bounds.x0, -bounds.y0) * aff, filterType);
    over(out, tmp, bounds.getP00());
  }
  out->unlock();
  up->unlock();
}

void TRop::over(const TRasterP &out, const TRasterP &up, const TPoint &pos,
                const TAffine &aff, ResampleFilterType filterType) {
  if (aff.isIdentity())
    // simple over with offset
    TRop::over(out, up, pos);
  else {
    TRect rasterBounds = up->getBounds();
    TRectD dbounds(rasterBounds.x0, rasterBounds.y0, rasterBounds.x1,
                   rasterBounds.y1);
    dbounds = aff * dbounds;
    TRect bounds(tfloor(dbounds.x0), tfloor(dbounds.y0), tceil(dbounds.x1),
                 tceil(dbounds.y1));
    TRasterP tmp = up->create(bounds.getLx(), bounds.getLy());
    resample(tmp, up, TTranslation(-dbounds.getP00()) * aff, filterType);
    TRop::over(out, tmp, pos);
  }
}

void TRop::over(TRasterP rout, const TRasterCM32P &rup, TPalette *palette,
                const TPoint &point, const TAffine &aff) {
  TRaster32P app(rup->getSize());
  TRop::convert(app, rup, palette);
  TRop::over(rout, app, point, aff);
}

//===================================================================

void TRop::quickPut(const TRasterP &out, const TRasterP &up, const TAffine &aff,
                    const TPixel32 &colorScale, bool doPremultiply,
                    bool whiteTransp, bool firstColumn,
                    bool doRasterDarkenBlendedView) {
  ::quickPut(out, up, aff, ClosestPixel, colorScale, doPremultiply, whiteTransp,
             firstColumn, doRasterDarkenBlendedView);
}

//===================================================================

void TRop::over(const TRasterP &out, const TRasterP &dn, const TRasterP &up,
                const TRasterGR8P &mask) {
  out->lock();
  up->lock();
  dn->lock();

  TRaster32P out32 = out;
  TRaster32P dn32  = dn;
  TRaster32P up32  = up;

  if (out32 && dn32 && up32)
    do_over<TPixel32, TPixel32, TPixel32>(out32, dn32, up32, mask);
  else {
    TRaster64P out64 = out;
    TRaster64P dn64  = dn;
    TRaster64P up64  = up;
    if (out64 && dn64 && up64)
      do_over<TPixel64, TPixel64, TPixel64>(out64, dn64, up64, mask);
    else
      throw TRopException("unsupported pixel type");
  }
  out->unlock();
  up->unlock();
  dn->unlock();
}
