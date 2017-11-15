

#include "trop.h"
#include "tpixel.h"
#include "tpixelutils.h"

#ifdef _WIN32
#include <emmintrin.h>  // per SSE2
#endif

namespace {
inline double luminance(TPixel32 *pix) {
  return 0.2126 * pix->r + 0.7152 * pix->g + 0.0722 * pix->b;
}
inline double luminance(TPixel64 *pix) {
  return 0.2126 * pix->r + 0.7152 * pix->g + 0.0722 * pix->b;
}
}  // namespace

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_BEGIN_LOOP(UpType, up, DownType, down, OutType, out)    \
  {                                                                            \
    int upWrap   = up->getWrap();                                              \
    int downWrap = down->getWrap();                                            \
    int outWrap  = out->getWrap();                                             \
                                                                               \
    up->lock();                                                                \
    down->lock();                                                              \
    out->lock();                                                               \
    UpType *upPix = 0, *upRow   = up->pixels();                                \
    DownType *downPix, *downRow = down->pixels();                              \
    OutType *outPix, *outRow    = out->pixels();                               \
    UpType *endPix;                                                            \
    int upLx        = up->getLx();                                             \
    UpType *lastPix = upRow + upWrap * (up->getLy() - 1) + upLx;               \
    while (upPix < lastPix) {                                                  \
      upPix   = upRow;                                                         \
      downPix = downRow;                                                       \
      outPix  = outRow;                                                        \
      endPix  = upPix + upLx;                                                  \
      while (upPix < endPix) {
//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_END_LOOP(up, down, out)                                 \
  ++upPix;                                                                     \
  ++downPix;                                                                   \
  ++outPix;                                                                    \
  }                                                                            \
  upRow += upWrap;                                                             \
  downRow += downWrap;                                                         \
  outRow += outWrap;                                                           \
  }                                                                            \
  up->unlock();                                                                \
  down->unlock();                                                              \
  out->unlock();                                                               \
  }

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_32_BEGIN_LOOP                                           \
  assert(up32 &&down32 &&out32);                                               \
  FOR_EACH_PIXEL_BEGIN_LOOP(TPixelRGBM32, up32, TPixelRGBM32, down32,          \
                            TPixelRGBM32, out32)

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_32_END_LOOP                                             \
  assert(up32 &&down32 &&out32);                                               \
  FOR_EACH_PIXEL_END_LOOP(up32, down32, out32)

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_64_BEGIN_LOOP                                           \
  assert(up64 &&down64 &&out64);                                               \
  FOR_EACH_PIXEL_BEGIN_LOOP(TPixelRGBM64, up64, TPixelRGBM64, down64,          \
                            TPixelRGBM64, out64)

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_64_END_LOOP                                             \
  assert(up64 &&down64 &&out64);                                               \
  FOR_EACH_PIXEL_END_LOOP(up64, down64, out64)

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_8_BEGIN_LOOP                                            \
  assert(up8 &&down8 &&out8);                                                  \
  FOR_EACH_PIXEL_BEGIN_LOOP(TPixelGR8, up8, TPixelGR8, down8, TPixelGR8, out8)

//-----------------------------------------------------------------------------

#define FOR_EACH_PIXEL_8_END_LOOP                                              \
  assert(up8 &&down8 &&out8);                                                  \
  FOR_EACH_PIXEL_END_LOOP(up32, down32, out32)

//-----------------------------------------------------------------------------

void TRop::add(const TRasterP &rup, const TRasterP &rdown, const TRasterP &rout,
               double v) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    TINT32 r, g, b, m;
    if (upPix->m == 0)
      *outPix = *downPix;
    else {
      r         = downPix->r + tround(upPix->r * v);
      g         = downPix->g + tround(upPix->g * v);
      b         = downPix->b + tround(upPix->b * v);
      m         = downPix->m + tround(upPix->m * v);
      outPix->r = (UCHAR)tcrop<TINT32>(r, (TINT32)0, (TINT32)255);
      outPix->g = (UCHAR)tcrop<TINT32>(g, (TINT32)0, (TINT32)255);
      outPix->b = (UCHAR)tcrop<TINT32>(b, (TINT32)0, (TINT32)255);
      outPix->m = (UCHAR)tcrop<TINT32>(m, (TINT32)0, (TINT32)255);
    }

    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      TINT32 r, g, b, m;
      r = downPix->r + tround(upPix->r * v);
      g = downPix->g + tround(upPix->g * v);
      b = downPix->b + tround(upPix->b * v);
      m = downPix->m + tround(upPix->m * v);

      outPix->r = (USHORT)tcrop<TINT32>(r, 0, 0xffff);
      outPix->g = (USHORT)tcrop<TINT32>(g, 0, 0xffff);
      outPix->b = (USHORT)tcrop<TINT32>(b, 0, 0xffff);
      outPix->m = (USHORT)tcrop<TINT32>(m, 0, 0xffff);

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP

        USHORT value = troundp(upPix->value * v) + downPix->value;

        outPix->value = (UCHAR)tcrop<USHORT>(value, 0, 255);

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::add invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::add(const TRasterP &rup, const TRasterP &rdown,
               const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    USHORT r, g, b, m;
    r = downPix->r + upPix->r;
    g = downPix->g + upPix->g;
    b = downPix->b + upPix->b;
    m = downPix->m + upPix->m;

    outPix->r = (UCHAR)tcrop<USHORT>(r, 0, 255);
    outPix->g = (UCHAR)tcrop<USHORT>(g, 0, 255);
    outPix->b = (UCHAR)tcrop<USHORT>(b, 0, 255);
    outPix->m = (UCHAR)tcrop<USHORT>(m, 0, 255);

    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      TINT32 r, g, b, m;
      r = downPix->r + upPix->r;
      g = downPix->g + upPix->g;
      b = downPix->b + upPix->b;
      m = downPix->m + upPix->m;

      outPix->r = (USHORT)tcrop<TINT32>(r, 0, 0xffff);
      outPix->g = (USHORT)tcrop<TINT32>(g, 0, 0xffff);
      outPix->b = (USHORT)tcrop<TINT32>(b, 0, 0xffff);
      outPix->m = (USHORT)tcrop<TINT32>(m, 0, 0xffff);

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP

        USHORT value = upPix->value + downPix->value;

        outPix->value = (UCHAR)tcrop<USHORT>(value, 0, 255);

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::add invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::colordodge(const TRasterP &rup, const TRasterP &rdown,
                      const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    USHORT r, g, b, m;

    r = (USHORT)((downPix->r << 8) / (256.0 - upPix->r));
    g = (USHORT)((downPix->g << 8) / (256.0 - upPix->g));
    b = (USHORT)((downPix->b << 8) / (256.0 - upPix->b));

    m = downPix->m + upPix->m;

    outPix->r = (UCHAR)tcrop<USHORT>(r, 0, 255);
    outPix->g = (UCHAR)tcrop<USHORT>(g, 0, 255);
    outPix->b = (UCHAR)tcrop<USHORT>(b, 0, 255);
    outPix->m = (UCHAR)tcrop<USHORT>(m, 0, 255);

    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      TINT32 r, g, b, m;
      r = (TINT32)(65536.0 * (downPix->r / (65536.0 - upPix->r)));

      g = (TINT32)(65536.0 * (downPix->g / (65536.0 - upPix->g)));

      b = (TINT32)(65536.0 * (downPix->b / (65536.0 - upPix->b)));

      m = downPix->m + upPix->m;

      outPix->r = (USHORT)tcrop<TINT32>(r, 0, 0xffff);
      outPix->g = (USHORT)tcrop<TINT32>(g, 0, 0xffff);
      outPix->b = (USHORT)tcrop<TINT32>(b, 0, 0xffff);
      outPix->m = (USHORT)tcrop<TINT32>(m, 0, 0xffff);

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP
        USHORT value;
        if (downPix->value)
          value = (USHORT)((downPix->value << 8) / (255.0 - upPix->value));

        outPix->value = (UCHAR)tcrop<USHORT>(value, 0, 255);

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::color dodge invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::colorburn(const TRasterP &rup, const TRasterP &rdown,
                     const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP
    //  downPix->r=0;
    //  downPix->g=255;
    //  downPix->b=0;
    double r, g, b;
    if (upPix->m) {
      if (downPix->r == 0 || downPix->r == 255)
        r = downPix->r;
      else if (upPix->r)
        r = 255 - (((255 - downPix->r) << 8) / (double)upPix->r);
      else
        r = 0;
      if (downPix->g == 0 || downPix->g == 255)
        g = downPix->g;
      else if (upPix->g)
        g = 255 - (((255 - downPix->g) << 8) / (double)upPix->g);
      else
        g = 0;
      if (downPix->b == 0 || downPix->b == 255)
        b = downPix->b;
      else if (upPix->b)
        b = 255 - (((255 - downPix->b) << 8) / (double)upPix->b);
      else
        b = 0;

      if (upPix->m != 255) {
        TPixel32 tmpPix;
        tmpPix.r = (UCHAR)tcrop<double>(r, .0, 255.0);
        tmpPix.g = (UCHAR)tcrop<double>(g, .0, 255.0);
        tmpPix.b = (UCHAR)tcrop<double>(b, .0, 255.0);
        tmpPix.m = upPix->m;
        ;
        overPix<TPixel32, UCHAR>(*outPix, *downPix, tmpPix);
      } else {
        outPix->r = (UCHAR)tcrop<double>(r, .0, 255.0);
        outPix->g = (UCHAR)tcrop<double>(g, .0, 255.0);
        outPix->b = (UCHAR)tcrop<double>(b, .0, 255.0);
        outPix->m = downPix->m;
      }
    } else {
      outPix = downPix;
    }
    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP
      double r, g, b;
      if (upPix->m) {
        if (downPix->r == 0 || downPix->r == 65535)
          r = downPix->r;
        else if (upPix->r)
          r = 65535 - 65536 * ((65535.0 - downPix->r) / (double)upPix->r);
        else
          r = 0;
        if (downPix->g == 0 || downPix->g == 65535)
          g = downPix->g;
        else if (upPix->g)
          g = 65535 - 65536 * ((65535.0 - downPix->g) / (double)upPix->g);
        else
          g = 0;
        if (downPix->b == 0 || downPix->b == 65535)
          b = downPix->b;
        else if (upPix->b)
          b = 65535 - 65536 * ((65535.0 - downPix->b) / (double)upPix->b);
        else
          b = 0;

        if (upPix->m != 65535) {
          TPixel64 tmpPix;
          tmpPix.r = (USHORT)tcrop<double>(r, .0, 65535.0);
          tmpPix.g = (USHORT)tcrop<double>(g, .0, 65535.0);
          tmpPix.b = (USHORT)tcrop<double>(b, .0, 65535.0);
          tmpPix.m = upPix->m;
          overPix<TPixel64, USHORT>(*outPix, *downPix, tmpPix);
        } else {
          outPix->r = (USHORT)tcrop<double>(r, .0, 65535.0);
          outPix->g = (USHORT)tcrop<double>(g, .0, 65535.0);
          outPix->b = (USHORT)tcrop<double>(b, .0, 65535.0);
          outPix->m = downPix->m;
        }
      } else {
        outPix = downPix;
      }
      FOR_EACH_PIXEL_64_END_LOOP
    } else
      throw TRopException("TRop::color burn invalid raster combination");
  }
}

//-----------------------------------------------------------------------------

void TRop::screen(const TRasterP &rup, const TRasterP &rdown,
                  const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    double r, g, b;
    r = 256 - ((256 - upPix->r) * (256 - downPix->r) >> 8);
    g = 256 - ((256 - upPix->g) * (256 - downPix->g) >> 8);
    b = 256 - ((256 - upPix->b) * (256 - downPix->b) >> 8);

    if (upPix->m != 255) {
      double m;
      m = 256 - ((256 - upPix->m) * (256 - downPix->m) >> 8);
      TPixel32 tmpPix;
      tmpPix.r = (UCHAR)tcrop<double>(r, 0, 255);
      tmpPix.g = (UCHAR)tcrop<double>(g, 0, 255);
      tmpPix.b = (UCHAR)tcrop<double>(b, 0, 255);
      tmpPix.m = (UCHAR)tcrop<double>(m, 0, 255);
      overPix<TPixel32, UCHAR>(*outPix, *downPix, tmpPix);
    } else {
      outPix->r = (UCHAR)tcrop<double>(r, 0, 255);
      outPix->g = (UCHAR)tcrop<double>(g, 0, 255);
      outPix->b = (UCHAR)tcrop<double>(b, 0, 255);
      outPix->m = upPix->m;
    }
    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      double r, g, b;
      r = 65536 - (65536 - upPix->r) * ((65536 - downPix->r) / 65536.0);
      g = 65536 - (65536 - upPix->g) * ((65536 - downPix->g) / 65536.0);
      b = 65536 - (65536 - upPix->b) * ((65536 - downPix->b) / 65536.0);

      if (upPix->m != 65535) {
        double m;
        m = 65536 - (65536 - upPix->m) * ((65536 - downPix->m) / 65536.0);
        TPixel64 tmpPix;
        tmpPix.r = (USHORT)tcrop<double>(r, 0, 65535);
        tmpPix.g = (USHORT)tcrop<double>(g, 0, 65535);
        tmpPix.b = (USHORT)tcrop<double>(b, 0, 65535);
        tmpPix.m = (USHORT)tcrop<double>(m, 0, 65535);
        overPix<TPixel64, USHORT>(*outPix, *downPix, tmpPix);
      } else {
        outPix->r = (USHORT)tcrop<double>(r, 0, 65535);
        outPix->g = (USHORT)tcrop<double>(g, 0, 65535);
        outPix->b = (USHORT)tcrop<double>(b, 0, 65535);
        outPix->m = upPix->m;
      }

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP
        USHORT value;
        if (downPix->value)
          value = (USHORT)((downPix->value << 8) / (255.0 - upPix->value));

        outPix->value = (UCHAR)tcrop<USHORT>(value, 0, 255);

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::color dodge invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::sub(const TRasterP &rup, const TRasterP &rdown, const TRasterP &rout,
               bool matte) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  if (matte) {
    if (up32 && down32 && out32) {
      FOR_EACH_PIXEL_32_BEGIN_LOOP

      SHORT r = downPix->r - upPix->r;
      SHORT g = downPix->g - upPix->g;
      SHORT b = downPix->b - upPix->b;
      SHORT m = downPix->m - upPix->m;

      outPix->r = (UCHAR)tcrop<SHORT>(r, 0, 255);
      outPix->g = (UCHAR)tcrop<SHORT>(g, 0, 255);
      outPix->b = (UCHAR)tcrop<SHORT>(b, 0, 255);
      outPix->m = (UCHAR)tcrop<SHORT>(m, 0, 255);

      FOR_EACH_PIXEL_32_END_LOOP
    } else {
      TRaster64P up64   = rup;
      TRaster64P down64 = rdown;
      TRaster64P out64  = rout;

      if (up64 && down64 && out64) {
        FOR_EACH_PIXEL_64_BEGIN_LOOP

        TINT32 r  = downPix->r - upPix->r;
        TINT32 g  = downPix->g - upPix->g;
        TINT32 b  = downPix->b - upPix->b;
        TINT32 m  = downPix->m - upPix->m;
        outPix->r = (USHORT)tcrop<TINT32>(r, 0, 0xffff);
        outPix->g = (USHORT)tcrop<TINT32>(g, 0, 0xffff);
        outPix->b = (USHORT)tcrop<TINT32>(b, 0, 0xffff);
        outPix->m = (USHORT)tcrop<TINT32>(m, 0, 0xffff);

        FOR_EACH_PIXEL_64_END_LOOP
      } else {
        TRasterGR8P up8   = rup;
        TRasterGR8P down8 = rdown;
        TRasterGR8P out8  = rout;

        if (up8 && down8 && out8) {
          FOR_EACH_PIXEL_8_BEGIN_LOOP

          SHORT value   = upPix->value - downPix->value;
          outPix->value = (UCHAR)tcrop<SHORT>(value, 0, 255);

          FOR_EACH_PIXEL_8_END_LOOP
        } else
          throw TRopException("TRop::sub invalid raster combination");
      }
    }
  } else {
    if (up32 && down32 && out32) {
      FOR_EACH_PIXEL_32_BEGIN_LOOP

      SHORT r = downPix->r - upPix->r;
      SHORT g = downPix->g - upPix->g;
      SHORT b = downPix->b - upPix->b;
      SHORT m = downPix->m;  // - upPix->m;

      outPix->r = (UCHAR)tcrop<SHORT>(r, 0, 255);
      outPix->g = (UCHAR)tcrop<SHORT>(g, 0, 255);
      outPix->b = (UCHAR)tcrop<SHORT>(b, 0, 255);
      outPix->m = (UCHAR)tcrop<SHORT>(m, 0, 255);

      FOR_EACH_PIXEL_32_END_LOOP
    } else {
      TRaster64P up64   = rup;
      TRaster64P down64 = rdown;
      TRaster64P out64  = rout;

      if (up64 && down64 && out64) {
        FOR_EACH_PIXEL_64_BEGIN_LOOP

        TINT32 r  = downPix->r - upPix->r;
        TINT32 g  = downPix->g - upPix->g;
        TINT32 b  = downPix->b - upPix->b;
        TINT32 m  = downPix->m;  // - upPix->m;
        outPix->r = (USHORT)tcrop<TINT32>(r, 0, 0xffff);
        outPix->g = (USHORT)tcrop<TINT32>(g, 0, 0xffff);
        outPix->b = (USHORT)tcrop<TINT32>(b, 0, 0xffff);
        outPix->m = (USHORT)tcrop<TINT32>(m, 0, 0xffff);

        FOR_EACH_PIXEL_64_END_LOOP
      } else {
        TRasterGR8P up8   = rup;
        TRasterGR8P down8 = rdown;
        TRasterGR8P out8  = rout;

        if (up8 && down8 && out8) {
          FOR_EACH_PIXEL_8_BEGIN_LOOP

          SHORT value   = upPix->value - downPix->value;
          outPix->value = (UCHAR)tcrop<SHORT>(value, 0, 255);

          FOR_EACH_PIXEL_8_END_LOOP
        } else
          throw TRopException("TRop::sub invalid raster combination");
      }
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::mult(const TRasterP &rup, const TRasterP &rdown,
                const TRasterP &rout, int v, bool matte) {
  /*
Let U be 'up', D be 'down' and M be 'multiplied' (the result), and suppose for
the moment
that pixels are both NOT premultiplied and normalized to [0, 1].

The additional value v is used to add to RGB components of the U pixel:

U'_rgb = (U_rgb + v / 255)

The matte component is either U_m D_m in case (matte == true), or D_m in case
(matte == false).
Please, observe that in the case (matte == false) that choice makes the product
NOT COMMUTATIVE,
but I think that's justified - it's simply the most obvious use case.

In case (matte == true), each channel is multiplied independently, and that's
it.

The matter is more complicated when (matte == false). The problem in this case
is dealing with rgb
components when U and D both have some transparency.  When U is fully
transparent, we expect the
result to be D, and vice-versa, which is non-trivial.

We REQUIRE that (let's denote r only here):

M_r = M_r_u = U_r (1 - D_m) + U_r D_r D_m,   when  U_m == 1    (When U is fully
opaque, M_r is a D_m-linear
                                                                combination of
U_r and U_r D_r)
M_r = M_r_d = D_r (1 - U_m) + U_r D_r U_m,   when  D_m == 1    (Vice-versa, when
it's D that is fully opaque)

Finally, we're building a weighted sum, by U_m and D_m of the two above:

M_r = (M_r_u * U_m + M_r_d * D_m) / (U_m + D_m) =
    = (...) =
    = [ U_r U_m (1 - D_m) + D_r D_m (1 - U_m) + 2 U_r U_m  D_r D_m ] / (U_m +
D_m)
*/

  // 32-bit images case
  TRaster32P up32 = rup, down32 = rdown, out32 = rout;

  if (up32 && down32 && out32) {
    static const float maxChannelF = float(TPixel32::maxChannelValue);
    static const UCHAR maxChannelC = UCHAR(TPixel32::maxChannelValue);

    float vf = v;

    if (matte) {
      float dnMf, upMf_norm, outMf;

      FOR_EACH_PIXEL_32_BEGIN_LOOP  // Awful... should be explicit...

          dnMf  = downPix->m;
      upMf_norm = upPix->m / maxChannelF;

      outMf = downPix->m * upMf_norm;

      outPix->r =
          tcrop((upPix->r / upMf_norm + vf) * (downPix->r / dnMf), 0.0f, outMf);
      outPix->g =
          tcrop((upPix->g / upMf_norm + vf) * (downPix->g / dnMf), 0.0f, outMf);
      outPix->b =
          tcrop((upPix->b / upMf_norm + vf) * (downPix->b / dnMf), 0.0f, outMf);
      outPix->m = outMf;

      // NOTE:  + 0.5f in the crop arguments could take care of rounding...

      FOR_EACH_PIXEL_32_END_LOOP
    } else {
      float umf_norm, dmf_norm, umdmf_norm, outMf;
      float mSumf, uf, df, ufdf, normalizer;

      FOR_EACH_PIXEL_32_BEGIN_LOOP

      mSumf = upPix->m + float(downPix->m);
      if (mSumf > 0.0f) {
        umf_norm = upPix->m / maxChannelF, dmf_norm = downPix->m / maxChannelF;
        outMf = upPix->m +
                (1.0f - umf_norm) *
                    downPix->m;  // umf_norm should be ensured in [0.0, 1.0].
        // Convex combination should be in the conversion range.
        normalizer = outMf / (maxChannelF * mSumf);
        umdmf_norm = umf_norm * dmf_norm;

        uf = upPix->r + vf * umdmf_norm, df = downPix->r, ufdf = uf * df;
        outPix->r = tcrop((uf * (maxChannelC - downPix->m) +
                           df * (maxChannelC - upPix->m) + ufdf + ufdf) *
                              normalizer,
                          0.0f, outMf);

        uf = upPix->g + vf * umdmf_norm, df = downPix->g, ufdf = uf * df;
        outPix->g = tcrop((uf * (maxChannelC - downPix->m) +
                           df * (maxChannelC - upPix->m) + ufdf + ufdf) *
                              normalizer,
                          0.0f, outMf);

        uf = upPix->b + vf * umdmf_norm, df = downPix->b, ufdf = uf * df;
        outPix->b = tcrop((uf * (maxChannelC - downPix->m) +
                           df * (maxChannelC - upPix->m) + ufdf + ufdf) *
                              normalizer,
                          0.0f, outMf);

        outPix->m = outMf;
      } else
        *outPix = TPixel32::Transparent;

      FOR_EACH_PIXEL_32_END_LOOP
    }

    return;
  }

  // 64-bit images case
  TRaster64P up64 = rup, down64 = rdown, out64 = rout;

  if (up64 && down64 && out64) {
    static const double maxChannelF = double(TPixel64::maxChannelValue);
    static const USHORT maxChannelC = USHORT(TPixel64::maxChannelValue);

    double vf =
        v * (TPixel64::maxChannelValue / double(TPixel32::maxChannelValue));

    if (matte) {
      double dnMf, upMf_norm, outMf;

      FOR_EACH_PIXEL_64_BEGIN_LOOP

      dnMf      = downPix->m;
      upMf_norm = upPix->m / maxChannelF;

      outMf = downPix->m * upMf_norm;

      outPix->r =
          tcrop((upPix->r / upMf_norm + vf) * (downPix->r / dnMf), 0.0, outMf);
      outPix->g =
          tcrop((upPix->g / upMf_norm + vf) * (downPix->g / dnMf), 0.0, outMf);
      outPix->b =
          tcrop((upPix->b / upMf_norm + vf) * (downPix->b / dnMf), 0.0, outMf);
      outPix->m = outMf;

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      double umf_norm, dmf_norm, umdmf_norm, outMf;
      double mSumf, uf, df, ufdf, normalizer;

      FOR_EACH_PIXEL_64_BEGIN_LOOP

      mSumf = upPix->m + double(downPix->m);
      if (mSumf > 0.0) {
        umf_norm = upPix->m / maxChannelF, dmf_norm = downPix->m / maxChannelF;
        outMf = upPix->m + (1.0 - umf_norm) * downPix->m;

        normalizer = outMf / (maxChannelF * mSumf);
        umdmf_norm = umf_norm * dmf_norm;

        uf = upPix->r + vf * umdmf_norm, df = downPix->r, ufdf = uf * df;
        outPix->r = tcrop((uf * (maxChannelC - downPix->m) +
                           df * (maxChannelC - upPix->m) + ufdf + ufdf) *
                              normalizer,
                          0.0, outMf);

        uf = upPix->g + vf * umdmf_norm, df = downPix->g, ufdf = uf * df;
        outPix->g = tcrop((uf * (maxChannelC - downPix->m) +
                           df * (maxChannelC - upPix->m) + ufdf + ufdf) *
                              normalizer,
                          0.0, outMf);

        uf = upPix->b + vf * umdmf_norm, df = downPix->b, ufdf = uf * df;
        outPix->b = tcrop((uf * (maxChannelC - downPix->m) +
                           df * (maxChannelC - upPix->m) + ufdf + ufdf) *
                              normalizer,
                          0.0, outMf);

        outPix->m = outMf;
      } else
        *outPix = TPixel64::Transparent;

      FOR_EACH_PIXEL_64_END_LOOP
    }

    return;
  }

  // According to the specifics, throw an exception. I think it's not
  // appropriate, though.
  throw TRopException("TRop::mult invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::ropin(const TRasterP &source, const TRasterP &matte,
                 const TRasterP &rout) {
  TRaster32P source32 = source;
  TRaster32P matte32  = matte;
  TRaster32P out32    = rout;
  TRaster64P source64 = source;
  TRaster64P matte64  = matte;
  TRaster64P out64    = rout;

  if (source32 && matte32 && out32) {
    FOR_EACH_PIXEL_BEGIN_LOOP(TPixelRGBM32, source32, TPixelRGBM32, matte32,
                              TPixelRGBM32, out32)

    if (downPix->m == 0)
      outPix->r = outPix->g = outPix->b = outPix->m = 0;
    else if (downPix->m == 255)
      *outPix = *upPix;
    else {
      /*
__m128i zeros = _mm_setzero_si128();

__m128i upPix_packed_i= _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD*)upPix),
zeros);
__m128  upPix_packed  = _mm_cvtepi32_ps(_mm_unpacklo_epi16(upPix_packed_i,
zeros));

float fac = downPix->m / 255.0;
__m128 fac_packed = _mm_load1_ps(&fac);

upPix_packed = _mm_mul_ps(upPix_packed, fac_packed);

__m128i outPix_packed_i = _mm_cvtps_epi32(upPix_packed);
outPix_packed_i = _mm_packs_epi32(outPix_packed_i, zeros);
outPix_packed_i = _mm_packus_epi16(outPix_packed_i, zeros);
*(DWORD*)(outPix) = _mm_cvtsi128_si32(outPix_packed_i);
*/

      const int MAGICFAC = (257U * 256U + 1U);
      UINT fac           = MAGICFAC * downPix->m;

      outPix->r = (UINT)(upPix->r * fac + (1U << 23)) >> 24;
      outPix->g = (UINT)(upPix->g * fac + (1U << 23)) >> 24;
      outPix->b = (UINT)(upPix->b * fac + (1U << 23)) >> 24;
      outPix->m = (UINT)(upPix->m * fac + (1U << 23)) >> 24;
    }

    FOR_EACH_PIXEL_END_LOOP(source32, matte32, out32)
  } else if (source64 && matte64 && out64)

  {
    FOR_EACH_PIXEL_BEGIN_LOOP(TPixelRGBM64, source64, TPixelRGBM64, matte64,
                              TPixelRGBM64, out64)

    if (downPix->m == 0)
      outPix->r = outPix->g = outPix->b = outPix->m = 0;
    else if (downPix->m == 65535)
      *outPix = *upPix;
    else {
      /*
__m128i zeros = _mm_setzero_si128();

__m128i upPix_packed_i= _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(DWORD*)upPix),
zeros);
__m128  upPix_packed  = _mm_cvtepi32_ps(_mm_unpacklo_epi16(upPix_packed_i,
zeros));

float fac = downPix->m / 255.0;
__m128 fac_packed = _mm_load1_ps(&fac);

upPix_packed = _mm_mul_ps(upPix_packed, fac_packed);

__m128i outPix_packed_i = _mm_cvtps_epi32(upPix_packed);
outPix_packed_i = _mm_packs_epi32(outPix_packed_i, zeros);
outPix_packed_i = _mm_packus_epi16(outPix_packed_i, zeros);
*(DWORD*)(outPix) = _mm_cvtsi128_si32(outPix_packed_i);
*/
      double fac = downPix->m / 65535.0;

      outPix->r = (USHORT)(upPix->r * fac);
      outPix->g = (USHORT)(upPix->g * fac);
      outPix->b = (USHORT)(upPix->b * fac);
      outPix->m = (USHORT)(upPix->m * fac);
    }

    FOR_EACH_PIXEL_END_LOOP(source64, matte64, out64)
  } else
    throw TRopException("TRop::in invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::ropout(const TRasterP &source, const TRasterP &matte,
                  const TRasterP &rout) {
  TRaster32P source32 = source;
  TRaster32P matte32  = matte;
  TRaster32P out32    = rout;
  TRaster64P source64 = source;
  TRaster64P matte64  = matte;
  TRaster64P out64    = rout;

  if (source32 && matte32 && out32) {
    FOR_EACH_PIXEL_BEGIN_LOOP(TPixelRGBM32, source32, TPixelRGBM32, matte32,
                              TPixelRGBM32, out32)

    if (downPix->m == 255)
      outPix->r = outPix->g = outPix->b = outPix->m = 0;
    else if (downPix->m == 0)
      *outPix = *upPix;
    else {
      const int MAGICFAC = (257U * 256U + 1U);
      UINT fac           = MAGICFAC * (255 - downPix->m);

      outPix->r = (UINT)(upPix->r * fac + (1U << 23)) >> 24;
      outPix->g = (UINT)(upPix->g * fac + (1U << 23)) >> 24;
      outPix->b = (UINT)(upPix->b * fac + (1U << 23)) >> 24;
      outPix->m = (UINT)(upPix->m * fac + (1U << 23)) >> 24;
    }

    FOR_EACH_PIXEL_END_LOOP(source32, matte32, out32)
  } else if (source64 && matte64 && out64) {
    FOR_EACH_PIXEL_BEGIN_LOOP(TPixelRGBM64, source64, TPixelRGBM64, matte64,
                              TPixelRGBM64, out64)

    if (downPix->m == 65535)
      outPix->r = outPix->g = outPix->b = outPix->m = 0;
    else if (downPix->m == 0)
      *outPix = *upPix;
    else {
      double fac = (65535 - downPix->m) / 65535.0;

      outPix->r = (USHORT)(upPix->r * fac);
      outPix->g = (USHORT)(upPix->g * fac);
      outPix->b = (USHORT)(upPix->b * fac);
      outPix->m = (USHORT)(upPix->m * fac);
    }

    FOR_EACH_PIXEL_END_LOOP(source64, matte64, out64)
  } else
    throw TRopException("TRop::out invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::atop(const TRasterP &rup, const TRasterP &rdown,
                const TRasterP &rout) {
  // calcola rup ATOP rdown

  // da ottimizzare...
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  TRaster64P up64   = rup;
  TRaster64P down64 = rdown;
  TRaster64P out64  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    TPixel32 tmpPix(0, 0, 0, 0);
    if (downPix->m != 0) {
      const int MAGICFAC = (257U * 256U + 1U);
      UINT fac           = MAGICFAC * downPix->m;

      tmpPix.r = (UINT)(upPix->r * fac + (1U << 23)) >> 24;
      tmpPix.g = (UINT)(upPix->g * fac + (1U << 23)) >> 24;
      tmpPix.b = (UINT)(upPix->b * fac + (1U << 23)) >> 24;
      tmpPix.m = (UINT)(upPix->m * fac + (1U << 23)) >> 24;
    }

    overPix<TPixel32, UCHAR>(*outPix, *downPix, tmpPix);

    FOR_EACH_PIXEL_32_END_LOOP
  } else if (up64 && down64 && out64) {
    FOR_EACH_PIXEL_64_BEGIN_LOOP

    TPixel64 tmpPix(0, 0, 0, 0);
    if (downPix->m != 0) {
      double fac = downPix->m / 65535.0;

      tmpPix.r = (USHORT)(upPix->r * fac);
      tmpPix.g = (USHORT)(upPix->g * fac);
      tmpPix.b = (USHORT)(upPix->b * fac);
      tmpPix.m = (USHORT)(upPix->m * fac);
    }

    overPix<TPixel64, USHORT>(*outPix, *downPix, tmpPix);

    FOR_EACH_PIXEL_64_END_LOOP
  } else
    throw TRopException("TRop::atop invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::txor(const TRasterP &rup, const TRasterP &rdown,
                const TRasterP &rout) {
  // da ottimizzare...
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  TRaster64P up64   = rup;
  TRaster64P down64 = rdown;
  TRaster64P out64  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    TUINT32 notUpM   = 255 - upPix->m;
    TUINT32 notDownM = 255 - downPix->m;

    TUINT32 r = notUpM + upPix->r * (notDownM);
    TUINT32 g = notUpM + upPix->g * (notDownM);
    TUINT32 b = notUpM + upPix->b * (notDownM);

    outPix->r = (UCHAR)tcrop<TUINT32>(0, 255, r);
    outPix->g = (UCHAR)tcrop<TUINT32>(0, 255, g);
    outPix->b = (UCHAR)tcrop<TUINT32>(0, 255, b);

    FOR_EACH_PIXEL_32_END_LOOP
  } else if (up64 && down64 && out64) {
    FOR_EACH_PIXEL_64_BEGIN_LOOP

    TUINT32 notUpM   = 65535 - upPix->m;
    TUINT32 notDownM = 65535 - downPix->m;

    TUINT32 r = notUpM + upPix->r * (notDownM);
    TUINT32 g = notUpM + upPix->g * (notDownM);
    TUINT32 b = notUpM + upPix->b * (notDownM);

    outPix->r = (USHORT)tcrop<TUINT32>(0, 65535, r);
    outPix->g = (USHORT)tcrop<TUINT32>(0, 65535, g);
    outPix->b = (USHORT)tcrop<TUINT32>(0, 65535, b);

    FOR_EACH_PIXEL_64_END_LOOP
  } else
    throw TRopException("TRop::xor invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::crossDissolve(const TRasterP &rup, const TRasterP &rdown,
                         const TRasterP &rout, UCHAR v) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  TRaster64P up64   = rup;
  TRaster64P down64 = rdown;
  TRaster64P out64  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    outPix->r = (upPix->r * v + downPix->r * (255 - v)) / 255;
    outPix->g = (upPix->g * v + downPix->g * (255 - v)) / 255;
    outPix->b = (upPix->b * v + downPix->b * (255 - v)) / 255;
    outPix->m = (upPix->m * v + downPix->m * (255 - v)) / 255;

    FOR_EACH_PIXEL_32_END_LOOP
  } else if (up64 && down64 && out64) {
    USHORT vv = v * 257;

    FOR_EACH_PIXEL_64_BEGIN_LOOP

    outPix->r = (upPix->r * vv + downPix->r * (65535 - vv)) / 65535;
    outPix->g = (upPix->g * vv + downPix->g * (65535 - vv)) / 65535;
    outPix->b = (upPix->b * vv + downPix->b * (65535 - vv)) / 65535;
    outPix->m = (upPix->m * vv + downPix->m * (65535 - vv)) / 65535;

    FOR_EACH_PIXEL_64_END_LOOP
  } else
    throw TRopException("TRop::crossDissolve invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::darken(const TRasterP &rup, const TRasterP &rdown,
                  const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  TRaster64P up64   = rup;
  TRaster64P down64 = rdown;
  TRaster64P out64  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    double value0 = luminance(upPix);
    double value1 = luminance(downPix);

    if (value0 < value1)
      *outPix = *upPix;
    else
      *outPix = *downPix;

    FOR_EACH_PIXEL_32_END_LOOP
  } else if (up64 && down64 && out64) {
    FOR_EACH_PIXEL_64_BEGIN_LOOP

    double value0 = luminance(upPix);
    double value1 = luminance(downPix);

    if (value0 < value1)
      *outPix = *upPix;
    else
      *outPix = *downPix;

    FOR_EACH_PIXEL_64_END_LOOP
  } else
    throw TRopException("TRop::darken invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::lighten(const TRasterP &rup, const TRasterP &rdown,
                   const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;
  TRaster64P up64   = rup;
  TRaster64P down64 = rdown;
  TRaster64P out64  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    double value0 = luminance(upPix);
    double value1 = luminance(downPix);

    if (value0 > value1) {
      TINT32 r, g, b, m;
      if (upPix->m == 0)
        *outPix = *downPix;
      else {
        r         = downPix->r + upPix->r;
        g         = downPix->g + upPix->g;
        b         = downPix->b + upPix->b;
        m         = downPix->m + upPix->m;
        outPix->r = (UCHAR)tcrop<TINT32>(r, (TINT32)0, (TINT32)255);
        outPix->g = (UCHAR)tcrop<TINT32>(g, (TINT32)0, (TINT32)255);
        outPix->b = (UCHAR)tcrop<TINT32>(b, (TINT32)0, (TINT32)255);
        outPix->m = (UCHAR)tcrop<TINT32>(m, (TINT32)0, (TINT32)255);
      }
    } else {
      *outPix = *downPix;
    }

    FOR_EACH_PIXEL_32_END_LOOP
  } else if (up64 && down64 && out64) {
    FOR_EACH_PIXEL_64_BEGIN_LOOP

    double value0 = luminance(upPix);
    double value1 = luminance(downPix);

    if (value0 > value1) {
      TINT32 r, g, b, m;
      if (upPix->m == 0)
        *outPix = *downPix;
      else {
        r         = downPix->r + upPix->r;
        g         = downPix->g + upPix->g;
        b         = downPix->b + upPix->b;
        m         = downPix->m + upPix->m;
        outPix->r = (USHORT)tcrop<TINT32>(r, (TINT32)0, (TINT32)65535);
        outPix->g = (USHORT)tcrop<TINT32>(g, (TINT32)0, (TINT32)65535);
        outPix->b = (USHORT)tcrop<TINT32>(b, (TINT32)0, (TINT32)65535);
        outPix->m = (USHORT)tcrop<TINT32>(m, (TINT32)0, (TINT32)65535);
      }
    } else {
      *outPix = *downPix;
    }

    FOR_EACH_PIXEL_64_END_LOOP
  } else
    throw TRopException("TRop::lighten invalid raster combination");
}

//-----------------------------------------------------------------------------

void TRop::ropmin(const TRasterP &rup, const TRasterP &rdown,
                  const TRasterP &rout, bool matte) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;

  if (up32 && down32 && out32) {
    if (matte) {
      FOR_EACH_PIXEL_32_BEGIN_LOOP

      outPix->r = upPix->r < downPix->r ? upPix->r : downPix->r;
      outPix->g = upPix->g < downPix->g ? upPix->g : downPix->g;
      outPix->b = upPix->b < downPix->b ? upPix->b : downPix->b;
      outPix->m = upPix->m < downPix->m ? upPix->m : downPix->m;

      FOR_EACH_PIXEL_32_END_LOOP
    } else {
      FOR_EACH_PIXEL_32_BEGIN_LOOP
      if (upPix->m >= 255) {
        outPix->r = upPix->r < downPix->r ? upPix->r : downPix->r;
        outPix->g = upPix->g < downPix->g ? upPix->g : downPix->g;
        outPix->b = upPix->b < downPix->b ? upPix->b : downPix->b;
        outPix->m = upPix->m < downPix->m ? upPix->m : downPix->m;

      } else if (upPix->m) {
        TPixel32 tmp;
        tmp.r = upPix->r < downPix->r ? upPix->r : downPix->r;
        tmp.g = upPix->g < downPix->g ? upPix->g : downPix->g;
        tmp.b = upPix->b < downPix->b ? upPix->b : downPix->b;
        // tmp.m = upPix->m < downPix->m ? upPix->m : downPix->m;
        outPix->r = upPix->m * (tmp.r - downPix->r) / 255.0 + downPix->r;
        outPix->g = upPix->m * (tmp.g - downPix->g) / 255.0 + downPix->g;
        outPix->b = upPix->m * (tmp.b - downPix->b) / 255.0 + downPix->b;
        outPix->m = upPix->m * (tmp.m - downPix->m) / 255.0 + downPix->m;
      } else
        *outPix = *downPix;
      FOR_EACH_PIXEL_32_END_LOOP
    }
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      if (matte) {
        FOR_EACH_PIXEL_64_BEGIN_LOOP

        outPix->r = upPix->r < downPix->r ? upPix->r : downPix->r;
        outPix->g = upPix->g < downPix->g ? upPix->g : downPix->g;
        outPix->b = upPix->b < downPix->b ? upPix->b : downPix->b;
        outPix->m = upPix->m < downPix->m ? upPix->m : downPix->m;

        FOR_EACH_PIXEL_64_END_LOOP
      } else {
        FOR_EACH_PIXEL_32_BEGIN_LOOP
        if (upPix->m >= 65535) {
          outPix->r = upPix->r < downPix->r ? upPix->r : downPix->r;
          outPix->g = upPix->g < downPix->g ? upPix->g : downPix->g;
          outPix->b = upPix->b < downPix->b ? upPix->b : downPix->b;
          outPix->m = upPix->m < downPix->m ? upPix->m : downPix->m;

        } else if (upPix->m) {
          TPixel32 tmp;
          tmp.r = upPix->r < downPix->r ? upPix->r : downPix->r;
          tmp.g = upPix->g < downPix->g ? upPix->g : downPix->g;
          tmp.b = upPix->b < downPix->b ? upPix->b : downPix->b;
          // tmp.m = upPix->m < downPix->m ? upPix->m : downPix->m;
          outPix->r = upPix->m * (tmp.r - downPix->r) / 65535.0 + downPix->r;
          outPix->g = upPix->m * (tmp.g - downPix->g) / 65535.0 + downPix->g;
          outPix->b = upPix->m * (tmp.b - downPix->b) / 65535.0 + downPix->b;
          outPix->m = upPix->m * (tmp.m - downPix->m) / 65535.0 + downPix->m;
        } else
          *outPix = *downPix;
        FOR_EACH_PIXEL_32_END_LOOP
      }
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP

        outPix->value =
            upPix->value < downPix->value ? upPix->value : downPix->value;

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::min invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::ropmax(const TRasterP &rup, const TRasterP &rdown,
                  const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP

    outPix->r = upPix->r > downPix->r ? upPix->r : downPix->r;
    outPix->g = upPix->g > downPix->g ? upPix->g : downPix->g;
    outPix->b = upPix->b > downPix->b ? upPix->b : downPix->b;
    outPix->m = upPix->m > downPix->m ? upPix->m : downPix->m;

    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      outPix->r = upPix->r > downPix->r ? upPix->r : downPix->r;
      outPix->g = upPix->g > downPix->g ? upPix->g : downPix->g;
      outPix->b = upPix->b > downPix->b ? upPix->b : downPix->b;
      outPix->m = upPix->m > downPix->m ? upPix->m : downPix->m;

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP

        outPix->value =
            upPix->value > downPix->value ? upPix->value : downPix->value;

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::max invalid raster combination");
    }
  }
}
//-----------------------------------------------------------------------------

void TRop::linearburn(const TRasterP &rup, const TRasterP &rdown,
                      const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP
    if (upPix->m) {
      TPixel32 app;
      if (downPix->m) {
        TINT32 r, g, b, m;
        TPixel32 tmpPix;
        tmpPix = depremultiply(*downPix);
        r      = tmpPix.r + upPix->r - 255;
        g      = tmpPix.g + upPix->g - 255;
        b      = tmpPix.b + upPix->b - 255;
        m      = tmpPix.m + upPix->m - 255;

        app.r = (UCHAR)tcrop<TINT32>(r, (TINT32)0, (TINT32)255);
        app.g = (UCHAR)tcrop<TINT32>(g, (TINT32)0, (TINT32)255);
        app.b = (UCHAR)tcrop<TINT32>(b, (TINT32)0, (TINT32)255);
        // app.m = (UCHAR)tcrop<TINT32> (m, (TINT32)0, (TINT32)255);
        app.m = upPix->m;
      } else
        app = *upPix;
      overPix<TPixel32, UCHAR>(*outPix, *downPix, app);
    }
    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      if (upPix->m) {
        TPixel64 app;
        if (downPix->m) {
          TINT32 r, g, b, m;
          TPixel64 tmpPix;
          tmpPix = depremultiply(*downPix);
          r      = tmpPix.r + upPix->r - 65535;
          g      = tmpPix.g + upPix->g - 65535;
          b      = tmpPix.b + upPix->b - 65535;
          m      = tmpPix.m + upPix->m - 65535;

          app.r = (USHORT)tcrop<TINT32>(r, 0, 0xffff);
          app.g = (USHORT)tcrop<TINT32>(g, 0, 0xffff);
          app.b = (USHORT)tcrop<TINT32>(b, 0, 0xffff);
          // app.m = (UCHAR)tcrop<TINT32> (m, (TINT32)0, (TINT32)255);
          app.m = upPix->m;
        } else
          app = *upPix;
        overPix<TPixel64, USHORT>(*outPix, *downPix, app);
      }

      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP
        USHORT value = troundp(upPix->value + downPix->value - 255);

        outPix->value = (UCHAR)tcrop<USHORT>(value, 0, 255);

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::max invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::overlay(const TRasterP &rup, const TRasterP &rdown,
                   const TRasterP &rout) {
  TRaster32P up32   = rup;
  TRaster32P down32 = rdown;
  TRaster32P out32  = rout;

  if (up32 && down32 && out32) {
    FOR_EACH_PIXEL_32_BEGIN_LOOP
    if (upPix->m) {
      TPixel32 app;
      TPixel32 tmpPix, tmp2Pix;
      if (downPix->m) {
        tmpPix  = *downPix;
        tmp2Pix = depremultiply(*upPix);
        if (tmpPix.r < 128)
          app.r = troundp(2 * tmp2Pix.r * (tmpPix.r / 255.0));
        else {
          SHORT r =
              255 *
              (1 - 2 * (1.0 - tmpPix.r / 255.0) * (1.0 - tmp2Pix.r / 255.0));
          app.r = (UCHAR)tcrop<SHORT>(r, 0, 255);
        }
        if (tmpPix.g < 128)
          app.g = troundp(2 * tmp2Pix.g * (tmpPix.g / 255.0));
        else {
          SHORT g =
              255 *
              (1 - 2 * (1.0 - tmpPix.g / 255.0) * (1.0 - tmp2Pix.g / 255.0));
          app.g = (UCHAR)tcrop<SHORT>(g, 0, 255);
        }
        if (tmpPix.b < 128)
          app.b = troundp(2 * tmp2Pix.b * (tmpPix.b / 255.0));
        else {
          SHORT b =
              255 *
              (1 - 2 * (1.0 - tmpPix.b / 255.0) * (1.0 - tmp2Pix.b / 255.0));
          app.b = (UCHAR)tcrop<SHORT>(b, 0, 255);
        }
        app.m = tmp2Pix.m;
        app   = premultiply(app);
      } else
        app = *upPix;
      overPix<TPixel32, UCHAR>(*outPix, *downPix, app);
    }
    FOR_EACH_PIXEL_32_END_LOOP
  } else {
    TRaster64P up64   = rup;
    TRaster64P down64 = rdown;
    TRaster64P out64  = rout;

    if (up64 && down64 && out64) {
      FOR_EACH_PIXEL_64_BEGIN_LOOP

      if (upPix->m) {
        TPixel64 app;
        TPixel64 tmpPix, tmp2Pix;
        if (downPix->m) {
          tmpPix  = *downPix;
          tmp2Pix = depremultiply(*upPix);
          if (tmpPix.r < 32768)
            app.r = troundp(2 * tmp2Pix.r * (tmpPix.r / 65535.0));
          else {
            SHORT r = 65535 * (1 -
                               2 * (1.0 - tmpPix.r / 65535.0) *
                                   (1.0 - tmp2Pix.r / 65535.0));
            app.r = (USHORT)tcrop<TINT32>(r, 0, 65535);
          }
          if (tmpPix.g < 32768)
            app.g = troundp(2 * tmp2Pix.g * (tmpPix.g / 65535.0));
          else {
            SHORT g = 65535 * (1 -
                               2 * (1.0 - tmpPix.g / 65535.0) *
                                   (1.0 - tmp2Pix.g / 65535.0));
            app.g = (USHORT)tcrop<TINT32>(g, 0, 65535);
          }
          if (tmpPix.b < 32768)
            app.b = troundp(2 * tmp2Pix.b * (tmpPix.b / 65535.0));
          else {
            SHORT b = 65535 * (1 -
                               2 * (1.0 - tmpPix.b / 65535.0) *
                                   (1.0 - tmp2Pix.b / 65535.0));
            app.b = (USHORT)tcrop<TINT32>(b, 0, 65535);
          }
          app.m = tmp2Pix.m;
          app   = premultiply(app);
        } else
          app = *upPix;
        overPix<TPixel64, USHORT>(*outPix, *downPix, app);
      }
      FOR_EACH_PIXEL_64_END_LOOP
    } else {
      TRasterGR8P up8   = rup;
      TRasterGR8P down8 = rdown;
      TRasterGR8P out8  = rout;

      if (up8 && down8 && out8) {
        FOR_EACH_PIXEL_8_BEGIN_LOOP

        USHORT value;
        if (downPix->value < 128)
          value = troundp(2 * upPix->value * (downPix->value / 255.0));
        else
          value = 255 * (1 -
                         2 * (1.0 - downPix->value / 255.0) *
                             (1.0 - upPix->value / 255.0));
        outPix->value = (UCHAR)tcrop<USHORT>(value, 0, 255);

        FOR_EACH_PIXEL_8_END_LOOP
      } else
        throw TRopException("TRop::max invalid raster combination");
    }
  }
}

//-----------------------------------------------------------------------------

void TRop::premultiply(const TRasterP &ras) {
  ras->lock();
  TRaster32P ras32 = ras;
  if (ras32) {
    TPixel32 *endPix, *upPix = 0, *upRow = ras32->pixels();
    TPixel32 *lastPix =
        upRow + ras32->getWrap() * (ras32->getLy() - 1) + ras32->getLx();

    while (upPix < lastPix) {
      upPix  = upRow;
      endPix = upPix + ras32->getLx();
      while (upPix < endPix) {
        premult(*upPix);
        ++upPix;
      }
      upRow += ras32->getWrap();
    }
  } else {
    TRaster64P ras64 = ras;
    if (ras64) {
      TPixel64 *endPix, *upPix = 0, *upRow = ras64->pixels();
      TPixel64 *lastPix =
          upRow + ras64->getWrap() * (ras64->getLy() - 1) + ras64->getLx();

      while (upPix < lastPix) {
        upPix  = upRow;
        endPix = upPix + ras64->getLx();
        while (upPix < endPix) {
          premult(*upPix);
          ++upPix;
        }
        upRow += ras64->getWrap();
      }
    } else {
      ras->unlock();
      throw TException("TRop::premultiply invalid raster type");
    }
  }
  ras->unlock();
}

//-----------------------------------------------------------------------------

void TRop::depremultiply(const TRasterP &ras) {
  ras->lock();
  TRaster32P ras32 = ras;
  if (ras32) {
    TPixel32 *endPix, *upPix = 0, *upRow = ras32->pixels();
    TPixel32 *lastPix =
        upRow + ras32->getWrap() * (ras32->getLy() - 1) + ras32->getLx();

    while (upPix < lastPix) {
      upPix  = upRow;
      endPix = upPix + ras32->getLx();
      while (upPix < endPix) {
        depremult(*upPix);
        ++upPix;
      }
      upRow += ras32->getWrap();
    }
  } else {
    TRaster64P ras64 = ras;
    if (ras64) {
      TPixel64 *endPix, *upPix = 0, *upRow = ras64->pixels();
      TPixel64 *lastPix =
          upRow + ras64->getWrap() * (ras64->getLy() - 1) + ras64->getLx();

      while (upPix < lastPix) {
        upPix  = upRow;
        endPix = upPix + ras64->getLx();
        while (upPix < endPix) {
          depremult(*upPix);
          ++upPix;
        }
        upRow += ras64->getWrap();
      }
    } else {
      ras->unlock();
      throw TException("TRop::depremultiply invalid raster type");
    }
  }
  ras->unlock();
}

//-----------------------------------------------------------------------------

void TRop::whiteTransp(const TRasterP &ras) {
  ras->lock();
  TRaster32P ras32 = ras;
  if (ras32) {
    TPixel32 *endPix, *upPix = 0, *upRow = ras32->pixels();
    TPixel32 *lastPix =
        upRow + ras32->getWrap() * (ras32->getLy() - 1) + ras32->getLx();

    while (upPix < lastPix) {
      upPix  = upRow;
      endPix = upPix + ras32->getLx();
      while (upPix < endPix) {
        if (*upPix == TPixel::White) *upPix = TPixel::Transparent;
        ++upPix;
      }
      upRow += ras32->getWrap();
    }
  } else {
    TRaster64P ras64 = ras;
    if (ras64) {
      TPixel64 *endPix, *upPix = 0, *upRow = ras64->pixels();
      TPixel64 *lastPix =
          upRow + ras64->getWrap() * (ras64->getLy() - 1) + ras64->getLx();

      while (upPix < lastPix) {
        upPix  = upRow;
        endPix = upPix + ras64->getLx();
        while (upPix < endPix) {
          if (*upPix == TPixel64::White) *upPix = TPixel64::Transparent;
          ++upPix;
        }
        upRow += ras64->getWrap();
      }
    } else {
      ras->unlock();
      throw TException("TRop::premultiply invalid raster type");
    }
  }
  ras->unlock();
}

//-----------------------------------------------------------------------------

void TRop::applyColorScale(const TRasterP &ras, const TPixel32 &colorScale) {
  ras->lock();

  TRop::depremultiply(ras);

  TRaster32P ras32 = ras;
  int maxCh        = TPixel32::maxChannelValue;
  if (ras32) {
    TPixel32 *endPix, *upPix = 0, *upRow = ras32->pixels();
    TPixel32 *lastPix =
        upRow + ras32->getWrap() * (ras32->getLy() - 1) + ras32->getLx();

    while (upPix < lastPix) {
      upPix  = upRow;
      endPix = upPix + ras32->getLx();
      while (upPix < endPix) {
        int maxCh = TPixel32::maxChannelValue;
        int r  = maxCh - (maxCh - (*upPix).r) * (maxCh - colorScale.r) / maxCh;
        int g  = maxCh - (maxCh - (*upPix).g) * (maxCh - colorScale.g) / maxCh;
        int b  = maxCh - (maxCh - (*upPix).b) * (maxCh - colorScale.b) / maxCh;
        int m  = (*upPix).m * colorScale.m / maxCh;
        *upPix = TPixel32(r, g, b, m);
        ++upPix;
      }
      upRow += ras32->getWrap();
    }
  } else {
    TRaster64P ras64 = ras;
    if (ras64) {
      TPixel64 *endPix, *upPix = 0, *upRow = ras64->pixels();
      TPixel64 *lastPix =
          upRow + ras64->getWrap() * (ras64->getLy() - 1) + ras64->getLx();

      int maxCh64 = TPixel64::maxChannelValue;
      while (upPix < lastPix) {
        upPix  = upRow;
        endPix = upPix + ras64->getLx();
        while (upPix < endPix) {
          int r =
              maxCh64 - (maxCh64 - (*upPix).r) * (maxCh - colorScale.r) / maxCh;
          int g =
              maxCh64 - (maxCh64 - (*upPix).g) * (maxCh - colorScale.g) / maxCh;
          int b =
              maxCh64 - (maxCh64 - (*upPix).b) * (maxCh - colorScale.b) / maxCh;
          int m  = (*upPix).m * colorScale.m / maxCh;
          *upPix = TPixel64(r, g, b, m);
          ++upPix;
        }
        upRow += ras64->getWrap();
      }
    } else {
      ras->unlock();
      throw TException("TRop::premultiply invalid raster type");
    }
  }
  TRop::premultiply(ras);
  ras->unlock();
}

//-----------------------------------------------------------------------------

template <typename Chan>
const double *premultiplyTable() {
  static double *table = 0;
  if (!table) {
    int maxChannelValue = (std::numeric_limits<Chan>::max)();
    int chanValuesCount = maxChannelValue + 1;
    double maxD         = maxChannelValue;

    table = new double[chanValuesCount];

    for (int i = 0; i < chanValuesCount; ++i) table[i] = i / maxD;
  }

  return table;
}

template DVAPI const double *premultiplyTable<UCHAR>();
template DVAPI const double *premultiplyTable<USHORT>();

//-----------------------------------------------------------------------------

template <typename Chan>
const double *depremultiplyTable() {
  static double *table = 0;
  if (!table) {
    int maxChannelValue = (std::numeric_limits<Chan>::max)();
    int chanValuesCount = maxChannelValue + 1;
    double maxD         = maxChannelValue;

    table = new double[chanValuesCount];

    table[0] = 0.0;
    for (int i = 1; i < chanValuesCount; ++i) table[i] = maxD / i;
  }

  return table;
}

template DVAPI const double *depremultiplyTable<UCHAR>();
template DVAPI const double *depremultiplyTable<USHORT>();

//-----------------------------------------------------------------------------

#undef FOR_EACH_PIXEL_BEGIN_LOOP
#undef FOR_EACH_PIXEL_32_BEGIN_LOOP
#undef FOR_EACH_PIXEL_64_BEGIN_LOOP
#undef FOR_EACH_PIXEL_8_BEGIN_LOOP
#undef FOR_EACH_PIXEL_END_LOOP
#undef FOR_EACH_PIXEL_32_END_LOOP
#undef FOR_EACH_PIXEL_64_END_LOOP
#undef FOR_EACH_PIXEL_8_END_LOOP
