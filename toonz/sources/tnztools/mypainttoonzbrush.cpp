
#include <algorithm>

#include "mypainttoonzbrush.h"
#include "tropcm.h"
#include "tpixelutils.h"
#include <toonz/mypainthelpers.hpp>

#include <QColor>

namespace {
void putOnRasterCM(const TRasterCM32P &out, const TRaster32P &in, int styleId,
                   bool lockAlpha) {
  if (!out.getPointer() || !in.getPointer()) return;
  assert(out->getSize() == in->getSize());
  int x, y;
  for (y = 0; y < out->getLy(); y++) {
    for (x = 0; x < out->getLx(); x++) {
#ifdef _DEBUG
      assert(x >= 0 && x < in->getLx());
      assert(y >= 0 && y < in->getLy());
      assert(x >= 0 && x < out->getLx());
      assert(y >= 0 && y < out->getLy());
#endif
      TPixel32 *inPix = &in->pixels(y)[x];
      if (inPix->m == 0) continue;
      TPixelCM32 *outPix = &out->pixels(y)[x];
      if (lockAlpha && !outPix->isPureInk() && outPix->getPaint() == 0 &&
          outPix->getTone() == 255) {
        *outPix =
            TPixelCM32(outPix->getInk(), outPix->getPaint(), outPix->getTone());
        continue;
      }
      bool sameStyleId = styleId == outPix->getInk();
      // line with lock alpha : use original pixel's tone
      // line with the same style : multiply tones
      // line with different style : pick darker tone
      int tone = lockAlpha     ? outPix->getTone()
                 : sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                               : std::min(255 - inPix->m, outPix->getTone());
      int ink  = !sameStyleId && outPix->getTone() < 255 - inPix->m
                     ? outPix->getInk()
                     : styleId;
      *outPix  = TPixelCM32(ink, outPix->getPaint(), tone);
    }
  }
}
}  // namespace

//=======================================================
//
// Raster32PMyPaintSurface::Internal
//
//=======================================================

class Raster32PMyPaintSurface::Internal
    : public mypaint::helpers::SurfaceCustom<readPixel, writePixel, askRead,
                                             askWrite> {
public:
  typedef SurfaceCustom Parent;
  Internal(Raster32PMyPaintSurface &owner)
      : SurfaceCustom(owner.ras->pixels(), owner.ras->getLx(),
                      owner.ras->getLy(), owner.ras->getPixelSize(),
                      owner.ras->getRowSize(), &owner) {}
};

//=======================================================
//
// Raster32PMyPaintSurface
//
//=======================================================

Raster32PMyPaintSurface::Raster32PMyPaintSurface(const TRaster32P &ras)
    : ras(ras), controller(), internal() {
  assert(ras);
  internal = new Internal(*this);
}

Raster32PMyPaintSurface::Raster32PMyPaintSurface(const TRaster32P &ras,
                                                 RasterController &controller)
    : ras(ras), controller(&controller), internal() {
  assert(ras);
  internal = new Internal(*this);
}

Raster32PMyPaintSurface::~Raster32PMyPaintSurface() { delete internal; }

bool Raster32PMyPaintSurface::getColor(float x, float y, float radius,
                                       float &colorR, float &colorG,
                                       float &colorB, float &colorA) {
  return internal->getColor(x, y, radius, colorR, colorG, colorB, colorA);
}

bool Raster32PMyPaintSurface::drawDab(const mypaint::Dab &dab) {
  return internal->drawDab(dab);
}

bool Raster32PMyPaintSurface::getAntialiasing() const {
  return internal->antialiasing;
}

void Raster32PMyPaintSurface::setAntialiasing(bool value) {
  internal->antialiasing = value;
}

//=======================================================
//
// MyPaintToonzBrush
//
//=======================================================

MyPaintToonzBrush::MyPaintToonzBrush(const TRaster32P &ras,
                                     RasterController &controller,
                                     const mypaint::Brush &brush,
                                     bool interpolation)
    : m_ras(ras)
    , m_mypaintSurface(ras, controller)
    , m_brush(brush)
    , m_reset(true)
    , m_interpolation(interpolation) {
  // read brush antialiasing settings
  float aa = this->m_brush.getBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING);
  m_mypaintSurface.setAntialiasing(aa > 0.5f);

  // reset brush antialiasing to zero to avoid radius and hardness correction
  this->m_brush.setBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING, 0.f);
  for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i)
    this->m_brush.setMappingN(MYPAINT_BRUSH_SETTING_ANTI_ALIASING,
                              (MyPaintBrushInput)i, 0);
}

void MyPaintToonzBrush::beginStroke() {
  m_brush.reset();
  m_brush.newStroke();
  m_reset = true;
}

void MyPaintToonzBrush::endStroke() {
  if (!m_reset) {
    if (m_interpolation)
      strokeTo(TPointD(m_current.x, m_current.y), m_current.pressure,
               TPointD(m_current.tx, m_current.ty), 0.f);
    beginStroke();
  }
}

void MyPaintToonzBrush::strokeTo(const TPointD &position, double pressure,
                                 const TPointD &tilt, double dtime) {
  Params next(position.x, position.y, pressure, tilt.x, tilt.y, 0.0);

  if (m_reset) {
    m_current  = next;
    m_previous = m_current;
    m_reset    = false;
    // we need to jump to initial point (heuristic)
    m_brush.setState(MYPAINT_BRUSH_STATE_X, m_current.x);
    m_brush.setState(MYPAINT_BRUSH_STATE_Y, m_current.y);
    m_brush.setState(MYPAINT_BRUSH_STATE_ACTUAL_X, m_current.x);
    m_brush.setState(MYPAINT_BRUSH_STATE_ACTUAL_Y, m_current.y);
    return;
  }

  if (m_interpolation) {
    next.time = m_current.time + dtime;

    // accuracy
    const double threshold    = 1.0;
    const double thresholdSqr = threshold * threshold;
    const int maxLevel        = 16;

    // set initial segment
    Segment stack[maxLevel + 1];
    Params p0;
    Segment *segment    = stack;
    Segment *maxSegment = segment + maxLevel;
    p0.setMedian(m_previous, m_current);
    segment->p1 = m_current;
    segment->p2.setMedian(m_current, next);

    // process
    while (true) {
      double dx = segment->p2.x - p0.x;
      double dy = segment->p2.y - p0.y;
      if (dx * dx + dy * dy > thresholdSqr && segment != maxSegment) {
        Segment *sub = segment + 1;
        sub->p1.setMedian(p0, segment->p1);
        segment->p1.setMedian(segment->p1, segment->p2);
        sub->p2.setMedian(sub->p1, segment->p1);
        segment = sub;
      } else {
        m_brush.strokeTo(m_mypaintSurface, segment->p2.x, segment->p2.y,
                         segment->p2.pressure, segment->p2.tx, segment->p2.ty,
                         segment->p2.time - p0.time);
        if (segment == stack) break;
        p0 = segment->p2;
        --segment;
      }
    }

    // keep parameters for future interpolation
    m_previous = m_current;
    m_current  = next;

    // shift time
    m_previous.time = 0.0;
    m_current.time  = dtime;
  } else {
    m_brush.strokeTo(m_mypaintSurface, position.x, position.y, pressure, tilt.x,
                     tilt.y, dtime);
  }
}

//----------------------------------------------------------------------------------

void MyPaintToonzBrush::updateDrawing(const TRasterCM32P rasCM,
                                      const TRasterCM32P rasBackupCM,
                                      const TRect &bbox, int styleId,
                                      bool lockAlpha) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->copy(rasBackupCM->extract(targetRect), targetRect.getP00());
  putOnRasterCM(rasCM->extract(targetRect), m_ras->extract(targetRect), styleId,
                lockAlpha);
}
