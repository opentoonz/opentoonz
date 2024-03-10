

#include <tools/assistants/guidelineline.h>

// TnzCore includes
#include "tgl.h"


//*****************************************************************************************
//    TGuidelineLineBase implementation
//*****************************************************************************************

TGuidelineLineBase::TGuidelineLineBase(
  bool enabled, double magnetism, const TPixelD &color,
  const TPointD &p0, const TPointD &p1
):
  TGuideline(enabled, magnetism, color), p0(p0), p1(p1) { }

TPointD
TGuidelineLineBase::calcDirection(const TPointD &p0, const TPointD &p1) {
  TPointD d = p1 - p0;
  double k = norm2(d);
  return k > TConsts::epsilon*TConsts::epsilon ? d*(1.0/sqrt(k)) : TPointD();
}


static bool
fitX(double &x0, double &y0, double &x1, double &y1, double minx, double maxx) {
  double dx = x1 - x0;
  if (fabs(dx) < TConsts::epsilon) return false;
  double k = (y1 - y0)/dx;
  if (dx > 0.0) {
    y0 += k*(minx - x0); x0 = minx;
    y1 += k*(maxx - x1); x1 = maxx;
  } else {
    y0 += k*(maxx - x0); x0 = maxx;
    y1 += k*(minx - x1); x1 = minx;
  }
  return true;
}

static bool
truncateX(double &x0, double &y0, double &x1, double &y1, double minx, double maxx) {
  if (x0 <= minx && x1 <= minx) return false;
  if (x0 >= maxx && x1 >= maxx) return false;
  double dx = x1 - x0;
  if (fabs(dx) < TConsts::epsilon) return true;
  double k = (y1 - y0)/dx;
  if (dx > 0.0) {
    if (x0 < minx) { y0 += k*(minx - x0); x0 = minx; }
    if (x1 > maxx) { y1 += k*(maxx - x1); x1 = maxx; }
  } else {
    if (x0 > maxx) { y0 += k*(maxx - x0); x0 = maxx; }
    if (x1 < minx) { y1 += k*(minx - x1); x1 = minx; }
  }
  return true;
}

bool
TGuidelineLineBase::truncateInfiniteLine(const TRectD &bounds, TPointD &p0, TPointD &p1) {
  if (bounds.isEmpty()) return false;
  TPointD d = p0 - p1;
  TDimensionD size = bounds.getSize();
  if (fabs(d.x)*size.ly > fabs(d.y)*size.lx) {
    // horizontal
    if (!fitX     (p0.x, p0.y, p1.x, p1.y, bounds.x0, bounds.x1)) return false;
    if (!truncateX(p0.y, p0.x, p1.y, p1.x, bounds.y0, bounds.y1)) return false;
  } else {
    // vertical
    if (!fitX     (p0.y, p0.x, p1.y, p1.x, bounds.y0, bounds.y1)) return false;
    if (!truncateX(p0.x, p0.y, p1.x, p1.y, bounds.x0, bounds.x1)) return false;
  }
  return true;
}

bool
TGuidelineLineBase::truncateRay(const TRectD &bounds, TPointD &p0, TPointD &p1) {
  if (bounds.isEmpty()) return false;

  // restrict bounds by p0
  // slightly expand this restriction to include vert and hor rays
  TRectD b(bounds);
  if (p0.x <= p1.x) { if (b.x0 < p0.x - TConsts::epsilon) b.x0 = p0.x - TConsts::epsilon; }
               else { if (b.x1 > p0.x + TConsts::epsilon) b.x1 = p0.x + TConsts::epsilon; }
  if (p0.y <= p1.y) { if (b.y0 < p0.y - TConsts::epsilon) b.y0 = p0.y - TConsts::epsilon; }
               else { if (b.y1 > p0.y + TConsts::epsilon) b.y1 = p0.y + TConsts::epsilon; }

  if (b.isEmpty()) return false;
  return truncateInfiniteLine(b, p0, p1);
}

bool
TGuidelineLineBase::truncateLine(const TRectD &bounds, TPointD &p0, TPointD &p1) {
  // restrict bounds by p0 and p1
  // slightly expand this restriction to include vert and hor lines
  TRectD b( std::min(p0.x, p1.x) - TConsts::epsilon,
            std::min(p0.y, p1.y) - TConsts::epsilon,
            std::max(p0.x, p1.x) + TConsts::epsilon,
            std::max(p0.y, p1.y) + TConsts::epsilon );
  b *= bounds;
  if (b.isEmpty()) return false;
  return truncateInfiniteLine(b, p0, p1);
}

void
TGuidelineLineBase::drawLine(const TPointD &p0, const TPointD &p1, bool restrict0, bool restrict1, bool active, bool enabled) const {
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);

  TAffine matrix = (projection*modelview).get2d();
  TPointD pp0 = matrix*p0;
  TPointD pp1 = matrix*p1;
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  if ( restrict0 && restrict1 ? !truncateLine(oneBox, pp0, pp1)
     : restrict0              ? !truncateRay (oneBox, pp0, pp1)
     : restrict1              ? !truncateRay (oneBox, pp1, pp0) // pp1 first
     :                  !truncateInfiniteLine(oneBox, pp0, pp1) ) return;

  double pixelSize = sqrt(tglGetPixelSize2());
  TAffine matrixInv = matrix.inv();
  drawSegment(matrixInv*pp0, matrixInv*pp1, pixelSize, active, enabled);
}


//*****************************************************************************************
//    TGuidelineLine implementation
//*****************************************************************************************

TGuidelineLine::TGuidelineLine(
  bool enabled, double magnetism, const TPixelD &color, const TPointD &p0, const TPointD &p1
):
  TGuidelineLineBase(enabled, magnetism, color, p0, p1),
  dir(calcDirection(p0, p1)),
  dist(norm(p1 - p0)) { }

TTrackPoint
TGuidelineLine::transformPoint(const TTrackPoint &point) const {
  TTrackPoint p(point);
  p.position = p0 + dir * std::max(0.0, std::min(dist, ((p.position - p0)*dir)));
  return p;
}

void
TGuidelineLine::draw(bool active, bool enabled) const
  { drawLine(p0, p1, true, true, active, enabled); }


//*****************************************************************************************
//    TGuidelineInfiniteLine implementation
//*****************************************************************************************

TGuidelineInfiniteLine::TGuidelineInfiniteLine(
  bool enabled, double magnetism, const TPixelD &color, const TPointD &p0, const TPointD &p1
):
  TGuidelineLineBase(enabled, magnetism, color, p0, p1),
  dir(calcDirection(p0, p1)) { }

TTrackPoint
TGuidelineInfiniteLine::transformPoint(const TTrackPoint &point) const {
  TTrackPoint p(point);
  p.position = p0 + dir * ((p.position - p0)*dir);
  return p;
}

void
TGuidelineInfiniteLine::draw(bool active, bool enabled) const
  { drawLine(p0, p1, false, false, active, enabled); }


//*****************************************************************************************
//    TGuidelineRay implementation
//*****************************************************************************************

TGuidelineRay::TGuidelineRay(
  bool enabled, double magnetism, const TPixelD &color, const TPointD &p0, const TPointD &p1
):
  TGuidelineLineBase(enabled, magnetism, color, p0, p1),
  dir(calcDirection(p0, p1)) { }

TTrackPoint
TGuidelineRay::transformPoint(const TTrackPoint &point) const {
  TTrackPoint p(point);
  p.position = p0 + dir * std::max(0.0, ((p.position - p0)*dir));
  return p;
}

void
TGuidelineRay::draw(bool active, bool enabled) const
  { drawLine(p0, p1, true, false, active, enabled); }

