
#include <algorithm>

#include "mypainttoonzbrush.h"
#include "tropcm.h"
#include "tpixelutils.h"
#include <toonz/mypainthelpers.hpp>

#include <QColor>


//=======================================================
//
// Raster32PMyPaintSurface::Internal
//
//=======================================================

class Raster32PMyPaintSurface::Internal:
  public mypaint::helpers::SurfaceCustom<readPixel, writePixel, askRead, askWrite>
{
public:
  typedef SurfaceCustom Parent;
  Internal(Raster32PMyPaintSurface &owner):
    SurfaceCustom( owner.m_ras->pixels(),
                   owner.m_ras->getLx(),
                   owner.m_ras->getLy(),
                   owner.m_ras->getPixelSize(),
                   owner.m_ras->getRowSize(),
                   &owner )
  { }
};

//=======================================================
//
// Raster32PMyPaintSurface
//
//=======================================================

Raster32PMyPaintSurface::Raster32PMyPaintSurface(const TRaster32P &ras):
  m_ras(ras),
  controller(),
  internal()
{
  assert(ras);
  internal = new Internal(*this);
}

Raster32PMyPaintSurface::Raster32PMyPaintSurface(const TRaster32P &ras, RasterController &controller):
  m_ras(ras),
  controller(&controller),
  internal()
{
  assert(ras);
  internal = new Internal(*this);
}

Raster32PMyPaintSurface::~Raster32PMyPaintSurface()
  { delete internal; }

bool Raster32PMyPaintSurface::getColor(float x, float y, float radius,
                                       float &colorR, float &colorG, float &colorB, float &colorA)
{ return internal->getColor(x, y, radius, colorR, colorG, colorB, colorA); }

bool Raster32PMyPaintSurface::drawDab(const mypaint::Dab &dab)
  { return internal->drawDab(dab); }

bool Raster32PMyPaintSurface::getAntialiasing() const
  { return internal->antialiasing; }

void Raster32PMyPaintSurface::setAntialiasing(bool value)
  { internal->antialiasing = value; }

//=======================================================
//
// MyPaintToonzBrush
//
//=======================================================

MyPaintToonzBrush::MyPaintToonzBrush(const TRaster32P &ras, RasterController &controller, const mypaint::Brush &brush):
  m_ras(ras),
  m_mypaintSurface(m_ras, controller),
  brush(brush),
  reset(true)
{
  // read brush antialiasing settings
  float aa = this->brush.getBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING);
  m_mypaintSurface.setAntialiasing(aa > 0.5f);

  // reset brush antialiasing to zero to avoid radius and hardness correction
  this->brush.setBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING, 0.f);
  for(int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i)
    this->brush.setMappingN(MYPAINT_BRUSH_SETTING_ANTI_ALIASING, (MyPaintBrushInput)i, 0);
}

void MyPaintToonzBrush::beginStroke() {
  brush.reset();
  brush.newStroke();
  reset = true;
}

void MyPaintToonzBrush::endStroke() {
  if (!reset) {
    strokeTo(TPointD(current.x, current.y), current.pressure, 0.f);
    beginStroke();
  }
}

void MyPaintToonzBrush::strokeTo(const TPointD &point, double pressure, double dtime) {
  Params next(point.x, point.y, pressure, 0.0);

  if (reset) {
    current = next;
    previous = current;
    reset = false;
    // we need to jump to initial point (heuristic)
    brush.setState(MYPAINT_BRUSH_STATE_X, current.x);
    brush.setState(MYPAINT_BRUSH_STATE_Y, current.y);
    brush.setState(MYPAINT_BRUSH_STATE_ACTUAL_X, current.x);
    brush.setState(MYPAINT_BRUSH_STATE_ACTUAL_Y, current.y);
    return;
  } else {
    next.time = current.time + dtime;
  }

  // accuracy
  const double threshold = 1.0;
  const double thresholdSqr = threshold*threshold;
  const int maxLevel = 16;

  // set initial segment
  Segment stack[maxLevel+1];
  Params p0;
  Segment *segment = stack;
  Segment *maxSegment = segment + maxLevel;
  p0.setMedian(previous, current);
  segment->p1 = current;
  segment->p2.setMedian(current, next);

  // process
  while(true) {
    double dx = segment->p2.x - p0.x;
    double dy = segment->p2.y - p0.y;
    if (dx*dx + dy*dy > thresholdSqr && segment != maxSegment) {
      Segment *sub = segment + 1;
      sub->p1.setMedian(p0, segment->p1);
      segment->p1.setMedian(segment->p1, segment->p2);
      sub->p2.setMedian(sub->p1, segment->p1);
      segment = sub;
    } else {
      brush.strokeTo(m_mypaintSurface, segment->p2.x, segment->p2.y, segment->p2.pressure, 0.f, 0.f, segment->p2.time - p0.time);
      if (segment == stack) break;
      p0 = segment->p2;
      --segment;
    }
  }

  // keep parameters for future interpolation
  previous = current;
  current  = next;

  // shift time
  previous.time = 0.0;
  current.time = dtime;
}

