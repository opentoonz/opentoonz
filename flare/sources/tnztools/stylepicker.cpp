

#include "tools/stylepicker.h"
#include "tcolorstyles.h"
#include "tstroke.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "toonz/dpiscale.h"
#include "tpixelutils.h"
#include "tregion.h"
#include "toonzqt/gutil.h"

#include <QRect>

//---------------------------------------------------------

StylePicker::StylePicker(const QWidget *parent, const TImageP &image)
    : m_widget(parent), m_image(image), m_palette(image->getPalette()) {}

//---------------------------------------------------------

StylePicker::StylePicker(const QWidget *parent, const TImageP &image,
                         const TPaletteP &palette)
    : m_widget(parent), m_image(image), m_palette(palette) {}

//---------------------------------------------------------

TPoint StylePicker::getRasterPoint(const TPointD &p) const {
  if (TToonzImageP ti = m_image) {
    // DpiScale dpiScale(ti);
    TDimension size = ti->getSize();
    return TPoint(tround(0.5 * size.lx + p.x),   /// dpiScale.getSx()),
                  tround(0.5 * size.ly + p.y));  /// dpiScale.getSy()));
  } else if (TRasterImageP ri = m_image) {
    // DpiScale dpiScale(ri);
    TDimension size = ri->getRaster()->getSize();
    return TPoint(tround(0.5 * size.lx + p.x),   // /dpiScale.getSx()),
                  tround(0.5 * size.ly + p.y));  // /dpiScale.getSy()));
  } else
    return TPoint(tround(p.x), tround(p.y));
}

//---------------------------------------------------------
/*-- (StylePickerTool内で)LineとAreaを切り替えてPickできる。mode: 0=Area,
 * 1=Line, 2=Line&Areas(default)  --*/
int StylePicker::pickStyleId(const TPointD &pos, double radius, double scale2,
                             int mode) const {
  int styleId = 0;
  if (TToonzImageP ti = m_image) {
    TRasterCM32P ras = ti->getRaster();
    TPoint point     = getRasterPoint(pos);
    if (!ras->getBounds().contains(point)) return -1;
    TPixelCM32 col = ras->pixels(point.y)[point.x];

    switch (mode) {
    case 0:  // AREAS
      styleId = col.getPaint();
      break;
    case 1:  // LINES
      styleId = col.getInk();
      break;
    case 2:  // ALL (Line & Area)
    default:
      styleId = col.isPurePaint() ? col.getPaint() : col.getInk();
      break;
    }
  } else if (TRasterImageP ri = m_image) {
    const TPalette *palette = m_palette.getPointer();
    if (!palette) return -1;
    TRaster32P ras = ri->getRaster();
    if (!ras) return -1;
    TPoint point = getRasterPoint(pos);
    if (!ras->getBounds().contains(point)) return -1;
    TPixel32 col = ras->pixels(point.y)[point.x];
    styleId      = palette->getClosestStyle(col);
  } else if (TVectorImageP vi = m_image) {
    // prima cerca lo stile della regione piu' vicina
    TRegion *r = vi->getRegion(pos);
    if (r) styleId = r->getStyle();
    bool strokeFound;
    double dist2, w, thick;
    UINT index;
    //! funzionerebbe ancora meglio con un getNearestStroke che considera
    // la thickness, cioe' la min distance dalla outline e non dalla centerLine
    strokeFound = vi->getNearestStroke(pos, w, index, dist2);
    if (strokeFound) {
      int devPixRatio = getDevicePixelRatio(m_widget);
      dist2 *= scale2;
      TStroke *stroke = vi->getStroke(index);
      thick           = stroke->getThickPoint(w).thick;
      double len2     = thick * thick * scale2;
      const double minDist2 =
          (styleId == 0) ? radius * radius * (double)(devPixRatio * devPixRatio)
                         : 0;
      double checkDist = std::max(minDist2, len2);
      if (dist2 < checkDist) {
        assert(stroke);
        styleId = stroke->getStyle();
      }
    }
  }
  return styleId;
}

//---------------------------------------------------------
/*--- Toonz Raster LevelのToneを拾う。 ---*/
int StylePicker::pickTone(const TPointD &pos) const {
  if (TToonzImageP ti = m_image) {
    TRasterCM32P ras = ti->getRaster();
    if (!ras) return -1;
    TPoint point = getRasterPoint(pos);
    if (!ras->getBounds().contains(point)) return -1;
    TPixelCM32 col = ras->pixels(point.y)[point.x];

    return col.getTone();
  } else
    return -1;
}

//---------------------------------------------------------

TPixel32 StylePicker::pickColor(const TPointD &pos, double radius,
                                double scale2) const {
  TToonzImageP ti  = m_image;
  TRasterImageP ri = m_image;
  TVectorImageP vi = m_image;
  if (!!ri)  // !!ti || !!ri)
  {
    TRasterP raster;
    // if(ti)
    //  raster = ti->getRGBM(true);
    // else
    raster = ri->getRaster();

    TPoint point = getRasterPoint(pos);
    if (!raster->getBounds().contains(point)) return TPixel32::Transparent;

    TRaster32P raster32 = raster;
    if (raster32) return raster32->pixels(point.y)[point.x];

    TRasterGR8P rasterGR8 = raster;
    if (rasterGR8) return toPixel32(rasterGR8->pixels(point.y)[point.x]);
  } else if (vi) {
    const TPalette *palette = m_palette.getPointer();
    if (!palette) return TPixel32::Transparent;
    int styleId = pickStyleId(pos, radius, scale2);
    if (0 <= styleId && styleId < palette->getStyleCount())
      return palette->getStyle(styleId)->getAverageColor();
  } else if (ti) {
    const TPalette *palette = m_palette.getPointer();
    if (!palette) return TPixel32::Transparent;
    int paintId = pickStyleId(pos, radius, scale2, 0);
    int inkId   = pickStyleId(pos, radius, scale2, 1);
    int tone    = pickTone(pos);
    TPixel32 ink, paint;
    if (0 <= inkId && inkId < palette->getStyleCount())
      ink = palette->getStyle(inkId)->getAverageColor();
    if (0 <= paintId && paintId < palette->getStyleCount())
      paint = palette->getStyle(paintId)->getAverageColor();

    if (tone == 0)
      return ink;
    else if (tone == 255)
      return paint;
    else
      return blend(ink, paint, tone, TPixelCM32::getMaxTone());
  }
  return TPixel32::Transparent;
}

//---------------------------------------------------------

TPixel64 StylePicker::pickColor16(const TPointD &pos, double radius,
                                  double scale2) const {
  TToonzImageP ti  = m_image;
  TRasterImageP ri = m_image;
  TVectorImageP vi = m_image;
  assert(ri && !ti && !vi);
  if (!ri || ti || vi) return TPixel64::Transparent;
  TRasterP raster = ri->getRaster();
  if (raster->getPixelSize() != 8) return TPixel64::Transparent;
  TPoint point = getRasterPoint(pos);
  if (!raster->getBounds().contains(point)) return TPixel64::Transparent;
  TRaster64P raster64 = raster;
  if (!raster64) return TPixel64::Transparent;
  return raster64->pixels(point.y)[point.x];
}

//---------------------------------------------------------

TPixelF StylePicker::pickColor32F(const TPointD &pos, double radius,
                                  double scale2) const {
  TToonzImageP ti  = m_image;
  TRasterImageP ri = m_image;
  TVectorImageP vi = m_image;
  assert(ri && !ti && !vi);
  if (!ri || ti || vi) return TPixelF::Transparent;
  TRasterP raster = ri->getRaster();
  if (raster->getPixelSize() != 16) return TPixelF::Transparent;
  TPoint point = getRasterPoint(pos);
  if (!raster->getBounds().contains(point)) return TPixelF::Transparent;
  TRasterFP rasterF = raster;
  if (!rasterF) return TPixelF::Transparent;
  return rasterF->pixels(point.y)[point.x];
}

//---------------------------------------------------------

TPixel32 StylePicker::pickAverageColor(const TRectD &rect) const {
  TRasterImageP ri = m_image;
  assert(ri);
  if (!!ri) {
    TRasterP raster;
    raster = ri->getRaster();

    TPoint topLeft     = getRasterPoint(rect.getP00());
    TPoint bottomRight = getRasterPoint(rect.getP11());

    if (!raster->getBounds().overlaps(TRect(topLeft, bottomRight)))
      return TPixel32::Transparent;

    topLeft.x     = std::max(0, topLeft.x);
    topLeft.y     = std::max(0, topLeft.y);
    bottomRight.x = std::min(raster->getLx(), bottomRight.x);
    bottomRight.y = std::min(raster->getLy(), bottomRight.y);

    TRaster32P raster32 = raster;
    assert(raster32);
    if (raster32) {
      UINT r = 0, g = 0, b = 0, m = 0, size = 0;
      for (int y = topLeft.y; y < bottomRight.y; y++) {
        TPixel32 *p = &raster32->pixels(y)[topLeft.x];
        for (int x = topLeft.x; x < bottomRight.x; x++, p++) {
          r += p->r;
          g += p->g;
          b += p->b;
          m += p->m;
          size++;
        }
      }

      if (size)
        return TPixel32(r / size, g / size, b / size, m / size);
      else
        return TPixel32::Transparent;
    }
  }
  return TPixel32::Transparent;
}

//---------------------------------------------------------

TPixel64 StylePicker::pickAverageColor16(const TRectD &rect) const {
  TRasterImageP ri = m_image;
  assert(ri);
  if (!!ri) {
    TRasterP raster;
    raster = ri->getRaster();

    TPoint topLeft     = getRasterPoint(rect.getP00());
    TPoint bottomRight = getRasterPoint(rect.getP11());

    if (!raster->getBounds().overlaps(TRect(topLeft, bottomRight)))
      return TPixel64::Transparent;

    topLeft.x     = std::max(0, topLeft.x);
    topLeft.y     = std::max(0, topLeft.y);
    bottomRight.x = std::min(raster->getLx(), bottomRight.x);
    bottomRight.y = std::min(raster->getLy(), bottomRight.y);

    TRaster64P raster64 = raster;
    assert(raster64);
    if (raster64) {
      UINT r = 0, g = 0, b = 0, m = 0, size = 0;
      for (int y = topLeft.y; y < bottomRight.y; y++) {
        TPixel64 *p = &raster64->pixels(y)[topLeft.x];
        for (int x = topLeft.x; x < bottomRight.x; x++, p++) {
          r += p->r;
          g += p->g;
          b += p->b;
          m += p->m;
          size++;
        }
      }

      if (size)
        return TPixel64(r / size, g / size, b / size, m / size);
      else
        return TPixel64::Transparent;
    }
  }
  return TPixel64::Transparent;
}

//---------------------------------------------------------

TPixelF StylePicker::pickAverageColor32F(const TRectD &rect) const {
  TRasterImageP ri = m_image;
  assert(ri);
  if (!ri) return TPixelF::Transparent;
  TRasterFP raster = ri->getRaster();
  if (!raster) return TPixelF::Transparent;

  TPoint topLeft     = getRasterPoint(rect.getP00());
  TPoint bottomRight = getRasterPoint(rect.getP11());

  if (!raster->getBounds().overlaps(TRect(topLeft, bottomRight)))
    return TPixelF::Transparent;

  topLeft.x     = std::max(0, topLeft.x);
  topLeft.y     = std::max(0, topLeft.y);
  bottomRight.x = std::min(raster->getLx(), bottomRight.x);
  bottomRight.y = std::min(raster->getLy(), bottomRight.y);

  float r = 0, g = 0, b = 0, m = 0, size = 0;
  for (int y = topLeft.y; y < bottomRight.y; y++) {
    TPixelF *p = &raster->pixels(y)[topLeft.x];
    for (int x = topLeft.x; x < bottomRight.x; x++, p++) {
      r += p->r;
      g += p->g;
      b += p->b;
      m += p->m;
      size++;
    }
  }

  if (size)
    return TPixelF(r / size, g / size, b / size, m / size);
  else
    return TPixelF::Transparent;
}

//---------------------------------------------------------

namespace {
TPixel32 getAverageColor(const TRect &rect) {
  GLenum fmt =
#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
      GL_RGBA;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
      GL_ABGR_EXT;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
      GL_RGBA;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
      GL_BGRA;
#else
//   Error  PLATFORM NOT SUPPORTED
#error "unknown channel order!"
#endif
  UINT r = 0, g = 0, b = 0, m = 0;
  std::vector<TPixel32> buffer(rect.getLx() * rect.getLy());
  glReadPixels(rect.x0, rect.y0, rect.getLx(), rect.getLy(), fmt, TGL_TYPE,
               &buffer[0]);
  int size = rect.getLx() * rect.getLy();
  for (int i = 0; i < size; i++) {
    r += buffer[i].r;
    g += buffer[i].g;
    b += buffer[i].b;
  }

  return TPixel32(b / size, g / size, r / size, TPixel32::maxChannelValue);
}

//---------------------------------------------------------

TPixel32 getAverageColor(TStroke *stroke) {
  GLenum fmt =
#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
      GL_RGBA;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
      GL_ABGR_EXT;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
      GL_RGBA;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
      GL_BGRA;
#else
//   Error  PLATFORM NOT SUPPORTED
#error "unknown channel order"
#endif

  // leggo il buffer e mi prendo i pixels
  UINT r = 0, g = 0, b = 0, m = 0;
  TRect rect = convert(stroke->getBBox());
  std::vector<TPixel32> buffer(rect.getLx() * rect.getLy());
  glReadPixels(rect.x0, rect.y0, rect.getLx(), rect.getLy(), fmt,
               GL_UNSIGNED_BYTE, &buffer[0]);

  // calcolo le regioni dello stroke
  TVectorImage aux;
  aux.addStroke(stroke);
  aux.transform(TTranslation(convert(-rect.getP00())));
  aux.findRegions();
  int regionCount = aux.getRegionCount();
  int size = 0, lx = rect.getLx();

  for (int j = 0; j < regionCount; j++) {
    TRegion *reg  = aux.getRegion(j);
    TRect regRect = convert(reg->getBBox());
    for (int y = regRect.y0; y < regRect.y1; y++) {
      std::vector<double> intersections;
      reg->computeScanlineIntersections(y, intersections);
      assert(!(intersections.size() & 0x1));
      for (UINT i = 0; i < intersections.size(); i += 2) {
        if (intersections[i] == intersections[i + 1]) continue;
        int firstInters  = (int)intersections[i];
        int secondInters = (int)intersections[i + 1];
        for (int x = firstInters + 1; x < secondInters - 1; x++) {
          r += buffer[y * lx + x].r;
          g += buffer[y * lx + x].g;
          b += buffer[y * lx + x].b;
          size++;
        }
      }
    }
  }

  if (size != 0)
    return TPixel32(b / size, g / size, r / size, 255);
  else
    return TPixel32(buffer[0].b, buffer[0].g, buffer[0].r, 255);
}

}  // namespace

//---------------------------------------------------------

TPixel32 StylePicker::pickColor(const TRectD &area) const {
  // TRectD rect=area.enlarge(-1,-1);
  return getAverageColor(convert(area));
}

//---------------------------------------------------------

TPixel32 StylePicker::pickColor(TStroke *stroke) const {
  return getAverageColor(stroke);
}

//---------------------------------------------------------
