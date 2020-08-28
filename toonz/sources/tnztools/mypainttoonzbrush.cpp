
#include <algorithm>

#include "mypainttoonzbrush.h"
#include "tropcm.h"
#include "tpixelutils.h"
#include <toonz/mypainthelpers.hpp>

#include <QColor>

namespace {
void putOnRasterCM(const TRasterCM32P &out, const TRaster32P &in, int styleId) {
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
      bool sameStyleId   = styleId == outPix->getInk();
      // line with the same style : multiply tones
      // line with different style : pick darker tone
      int tone = sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                             : std::min(255 - inPix->m, outPix->getTone());
      int ink = !sameStyleId && outPix->getTone() < 255 - inPix->m
                    ? outPix->getInk()
                    : styleId;
      *outPix = TPixelCM32(ink, outPix->getPaint(), tone);
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
    : ras(ras)
    , mypaintSurface(ras, controller)
    , brush(brush)
    , interpolation(interpolation)
    , reset(true)
{
  // read brush antialiasing settings
  float aa = this->brush.getBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING);
  mypaintSurface.setAntialiasing(aa > 0.5f);

  // reset brush antialiasing to zero to avoid radius and hardness correction
  this->brush.setBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING, 0.f);
  for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i)
    this->brush.setMappingN(MYPAINT_BRUSH_SETTING_ANTI_ALIASING,
                            (MyPaintBrushInput)i, 0);
}

void MyPaintToonzBrush::beginStroke() {
  brush.reset();
  brush.newStroke();
  reset = true;
}

void MyPaintToonzBrush::endStroke() {
  if (!reset) {
    if (interpolation)
      strokeTo(
        TPointD(current.x, current.y),
        current.pressure,
        TPointD(current.tilt_x, current.tilt_y),
        0.0 );
    beginStroke();
  }
}

void MyPaintToonzBrush::strokeTo(const Params &p, double prevtime) {
  brush.strokeTo(
    mypaintSurface,
    p.x, p.y,
    std::max(0.0, p.pressure),
    p.tilt_x, p.tilt_y,
    std::max(0.0, p.time - prevtime) );
}

void MyPaintToonzBrush::strokeBezierSegment(const Params &p0, const Params &p1,
                                            const Params &p2, const Params &p3, int level)
{
  // accuracy
  const double threshold = 1.0;
  const double thresholdSqr = threshold*threshold;
  
  // use 'not greater' inversion handle NaNs immediatelly without subdivisions
  if (level <= 0 || !((p3.x-p0.x)*(p3.x-p0.x) + (p3.y-p0.y)*(p3.y-p0.y) > thresholdSqr)) {
    // stroke immediatelly
    strokeTo(p3, p0.time);
    return;
  }
  
  // make subdivisions
  Params pp0 = (p0 + p1)/2;
  Params pp1 = (p1 + p2)/2;
  Params pp2 = (p2 + p3)/2;
  Params ppp0 = (pp0 + pp1)/2;
  Params ppp1 = (pp1 + pp2)/2;
  Params pppp = (ppp0 + ppp1)/2;
  
  strokeBezierSegment(p0, pp0, ppp0, pppp, level - 1);
  strokeBezierSegment(pppp, ppp1, pp2, p3, level - 1);
}


void MyPaintToonzBrush::strokeTo(const TPointD &position, double pressure,
                                 const TPointD &tilt, double dtime)
{
  Params next(position.x, position.y, pressure, tilt.x, tilt.y, 0.0);
  
  if (reset) {
    prevprev = prev = current = next;
    reset = false;
    // we need to jump to initial point (heuristic)
    brush.setState(MYPAINT_BRUSH_STATE_X, position.x);
    brush.setState(MYPAINT_BRUSH_STATE_Y, position.y);
    brush.setState(MYPAINT_BRUSH_STATE_ACTUAL_X, position.x);
    brush.setState(MYPAINT_BRUSH_STATE_ACTUAL_Y, position.y);
    return;
  }

  if (interpolation) {
    next.time = current.time + dtime;

    // make hermite spline
    const Params pp = prevprev;
    const Params p0 = prev;
    const Params p1 = current;
    const Params pn = next;
    Params t0 = (p1 - pp)/2;
    Params t1 = (pn - p0)/2;
    
    // clamp tangents
    for(int i = 2; i < Params::Count; ++i) { // do not clamp first two (x and y)
      double dp = p0[i] - pp[i];
      double dc = p1[i] - p0[i];
      double dn = pn[i] - p1[i];

      t0[i] = dp > 0 ? std::max(0.0, std::min(dp, t0[i]))
                     : std::min(0.0, std::max(dp, t0[i]));
      t0[i] = dc > 0 ? std::max(0.0, std::min(dc, t0[i]))
                     : std::min(0.0, std::max(dc, t0[i]));
      t1[i] = dc > 0 ? std::max(0.0, std::min(dc, t1[i]))
                     : std::min(0.0, std::max(dc, t1[i]));
      t1[i] = dn > 0 ? std::max(0.0, std::min(dn, t1[i]))
                     : std::min(0.0, std::max(dn, t1[i]));
    }
    
    // make bezier spline
    const Params pp0 = p0 + t0/3;
    const Params pp1 = p1 - t1/3;
    
    // stroke
    strokeBezierSegment(p0, pp0, pp1, p1, 16);
    
    // keep parameters for future interpolation
    prevprev = prev;
    prev = current;
    current = next;

    // shift time
    prev.time -= prevprev.time;
    current.time -= prevprev.time;
    prevprev.time = 0.0;
  } else {
    next.time = dtime;
    strokeTo(next, 0.0);
  }
}

//----------------------------------------------------------------------------------

void MyPaintToonzBrush::updateDrawing(const TRasterCM32P rasCM,
                                      const TRasterCM32P rasBackupCM,
                                      const TRect &bbox, int styleId) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->copy(rasBackupCM->extract(targetRect), targetRect.getP00());
  putOnRasterCM(rasCM->extract(targetRect), ras->extract(targetRect), styleId);
}
