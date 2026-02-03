

// TnzTools includes
#include "assistantvanishingpoint.h"



//*****************************************************************************************
//    TAssistantVanishingPoint implementation
//*****************************************************************************************


TAssistantVanishingPoint::TAssistantVanishingPoint(TMetaObject &object):
  TAssistant(object),
  m_idPassThrough("passThrough"),
  m_idGrid("grid"),
  m_idPerspective("perspective"),
  m_center( addPoint("center", TAssistantPoint::CircleCross) ),
  m_a0    ( addPoint("a0",     TAssistantPoint::Circle, TPointD(-50.0, 0.0)) ),
  m_a1    ( addPoint("a1",     TAssistantPoint::Circle, TPointD(-75.0, 0.0)) ),
  m_b0    ( addPoint("b0",     TAssistantPoint::Circle, TPointD( 50.0, 0.0)) ),
  m_b1    ( addPoint("b1",     TAssistantPoint::Circle, TPointD( 75.0, 0.0)) ),
  m_grid0 ( addPoint("grid0",  TAssistantPoint::CircleDoubleDots, TPointD(  0.0,-50.0)) ),
  m_grid1 ( addPoint("grid1",  TAssistantPoint::CircleDots,       TPointD( 25.0,-50.0)) )
{
  addProperty( new TBoolProperty(m_idPassThrough.str(), getPassThrough()) );
  addProperty( new TBoolProperty(m_idGrid.str(), getGrid()) );
  addProperty( new TBoolProperty(m_idPerspective.str(), getPerspective()) );
}


QString TAssistantVanishingPoint::getLocalName()
    { return tr("Vanishing Point"); }


void TAssistantVanishingPoint::updateTranslation() const {
  TAssistant::updateTranslation();
  setTranslation(m_idPassThrough, tr("Pass Through"));
  setTranslation(m_idGrid, tr("Grid"));
  setTranslation(m_idPerspective, tr("Perspective"));
}


void TAssistantVanishingPoint::onDataChanged(const TVariant &value) {
  TAssistant::onDataChanged(value);
  m_grid0.visible = m_grid1.visible = getGrid();
}


void TAssistantVanishingPoint::fixCenter() {
  if ( !(m_a0.position == m_a1.position)
    && !(m_b0.position == m_b1.position) )
  {
    const TPointD &a = m_a0.position;
    const TPointD &b = m_b0.position;
    const TPointD da = m_a1.position - a;
    const TPointD db = m_b1.position - b;
    const TPointD ab = b - a;
    double k = db.x*da.y - db.y*da.x;
    if (fabs(k) > TConsts::epsilon) {
      double lb = (da.x*ab.y - da.y*ab.x)/k;
      m_center.position.x = lb*db.x + b.x;
      m_center.position.y = lb*db.y + b.y;
    }
  }
}


void TAssistantVanishingPoint::fixSidePoint(TAssistantPoint &p0, TAssistantPoint &p1, TPointD previousP0) {
  if (p0.position != m_center.position && p0.position != p1.position) {
    TPointD dp0 = p0.position - m_center.position;
    TPointD dp1 = p1.position - previousP0;
    double l0 = norm(dp0);
    double l1 = norm(dp1);
    if (l0 > TConsts::epsilon && l0 + l1 > TConsts::epsilon)
      p1.position = m_center.position + dp0*((l0 + l1)/l0);
  }
}


void TAssistantVanishingPoint::fixSidePoint(TAssistantPoint &p0, TAssistantPoint &p1)
  { fixSidePoint(p0, p1, p0.position); }


void TAssistantVanishingPoint::fixGrid1(const TPointD &previousCenter, const TPointD &previousGrid0) {
  TPointD dx = previousCenter - previousGrid0;
  double l = norm2(dx);
  if (l <= TConsts::epsilon*TConsts::epsilon) return;
  dx = dx*(1.0/sqrt(l));
  TPointD dy(-dx.y, dx.x);

  TPointD d = m_grid1.position - previousGrid0;
  double x = (dx*d);
  double y = (dy*d);

  dx = m_center.position - m_grid0.position;
  l = norm2(dx);
  if (l <= TConsts::epsilon*TConsts::epsilon) return;
  dx = dx*(1.0/sqrt(l));
  dy = TPointD(-dx.y, dx.x);

  m_grid1.position = m_grid0.position + dx*x + dy*y;
}


void TAssistantVanishingPoint::onFixPoints() {
  fixSidePoint(m_a0, m_a1);
  fixSidePoint(m_b0, m_b1);
  fixCenter();
}


void TAssistantVanishingPoint::onMovePoint(TAssistantPoint &point, const TPointD &position) {
  TPointD previousCenter = m_center.position;
  TPointD previous = point.position;
  point.position = position;
  if (&point == &m_center) {
    fixSidePoint(m_a0, m_a1);
    fixSidePoint(m_b0, m_b1);
  } else
  if (&point == &m_a0) {
    fixSidePoint(m_a0, m_a1, previous);
    fixSidePoint(m_b0, m_b1);
  } else
  if (&point == &m_b0) {
    fixSidePoint(m_a0, m_a1);
    fixSidePoint(m_b0, m_b1, previous);
  } else
  if (&point == &m_a1) {
    fixCenter();
    fixSidePoint(m_a0, m_a1);
    fixSidePoint(m_b0, m_b1);
  } else
  if (&point == &m_b1) {
    fixCenter();
    fixSidePoint(m_b0, m_b1);
    fixSidePoint(m_a0, m_a1);
  }

  if (&point == &m_grid0) {
    fixGrid1(previousCenter, previous);
  } else
  if (&point != &m_grid1) {
    fixGrid1(previousCenter, m_grid0.position);
  }
}


void TAssistantVanishingPoint::getGuidelines(
  const TPointD &position,
  const TAffine &toTool,
  const TPixelD &color,
  TGuidelineList &outGuidelines ) const
{
  if (getPassThrough()) {
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineInfiniteLine(
        getEnabled(),
        getMagnetism(),
        color,
        toTool * m_center.position,
        position )));
  } else {
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineRay(
        getEnabled(),
        getMagnetism(),
        color,
        toTool * m_center.position,
        position )));
  }
}


void TAssistantVanishingPoint::drawSimpleGrid(
  const TPointD &center,
  const TPointD &grid0,
  const TPointD &grid1,
  double alpha )
{
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = 5.0*pixelSize;

  // calculate rays count and step
  TPointD d0 = grid0 - center;
  TPointD d1 = grid1 - center;
  TPointD dp = d0;
  double l = norm(d0);
  if (l <= TConsts::epsilon) return;
  if (norm2(d1) <= TConsts::epsilon*TConsts::epsilon) return;
  double a0 = atan(d0);
  double a1 = atan(d1);
  double da = fabs(a1 - a0);
  if (da > M_PI) da = M_PI - da;
  if (da < TConsts::epsilon) da = TConsts::epsilon;
  double count = M_2PI/da;
  if (count > 1e6) return;
  double radiusPart = minStep/(da*l);
  if (radiusPart > 1.0) return;
  int raysCount = (int)round(count);
  double step = M_2PI/(double)raysCount;

  // common data about viewport
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine matrix = (projection*modelview).get2d();
  TAffine matrixInv = matrix.inv();

  // calculate range
  if (!(matrixInv*oneBox).contains(center)) {
    TPointD corners[4] = {
      TPointD(oneBox.x0, oneBox.y0),
      TPointD(oneBox.x0, oneBox.y1),
      TPointD(oneBox.x1, oneBox.y0),
      TPointD(oneBox.x1, oneBox.y1) };
    double angles[4];
    double a0 = 0.0, a1 = 0.0, da = 0.0;
    for(int i = 0; i < 4; ++i) {
      angles[i] = atan(matrixInv*corners[i] - center) + M_2PI;
      for(int j = 0; j < i; ++j) {
        double d = fabs(angles[i] - angles[j]);
        if (d > M_PI) d = M_2PI - d;
        if (d > da) da = d, a0 = angles[i], a1 = angles[j];
      }
    }
    if (a1 < a0) std::swap(a1, a0);
    if (a1 - a0 > M_PI) { std::swap(a1, a0); a1 += M_2PI; }
    double a = atan(dp) + M_2PI;
    a0 = ceil ((a0 - a)/step)*step + a;
    a1 = floor((a1 - a)/step)*step + a;

    double s = sin(a0 - a);
    double c = cos(a0 - a);
    dp = TPointD(c*dp.x - s*dp.y, s*dp.x + c*dp.y);
    raysCount = (int)round((a1 - a0)/step);
  }

  // draw rays
  double s = sin(step);
  double c = cos(step);
  for(int i = 0; i < raysCount; ++i) {
    TPointD p0 = matrix*(center + dp*radiusPart);
    TPointD p1 = matrix*(center + dp);
    if (TGuidelineLineBase::truncateRay(oneBox, p0, p1))
      drawSegment(matrixInv*p0, matrixInv*p1, pixelSize, alpha);
    dp = TPointD(c*dp.x - s*dp.y, s*dp.x + c*dp.y);
  }
}


void TAssistantVanishingPoint::drawPerspectiveGrid(
  const TPointD &center,
  const TPointD &grid0,
  const TPointD &grid1,
  double alpha )
{
  // initial calculations
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = 5.0*pixelSize;
  
  TPointD ox = grid1 - grid0;
  double lx = norm2(ox);
  if (!(lx > TConsts::epsilon*TConsts::epsilon)) return;

  // common data about viewport
  const TRectD oneBox(-1, -1, 1, 1);
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine matrix = (projection*modelview).get2d();
  TAffine matrixInv = matrix.inv();
  
  // draw horizon
  TPointD p0 = matrix*center;
  TPointD p1 = matrix*(center + ox);
  if (TGuidelineLineBase::truncateInfiniteLine(oneBox, p0, p1))
    drawSegment(matrixInv*p0, matrixInv*p1, pixelSize, alpha);

  // build grid matrix
  TPointD dg = grid0 - center;
  lx = sqrt(lx);
  ox *= 1/lx;
  TPointD oy(-ox.y, ox.x);
  double baseY = dg*oy;
  if (!(fabs(baseY) > TConsts::epsilon)) return;
  if (baseY < 0) { oy = -oy; baseY = -baseY; }
  double baseX = dg*ox/baseY;
  double stepX = lx/baseY;
  double k = stepX*4/minStep;
  double dk = 1/k;
  TAffine gridMatrix( ox.x, oy.x, center.x,
                      ox.y, oy.y, center.y );
  matrix = matrix*gridMatrix;
  matrixInv = matrix.inv();
  
  const TRectD bounds = matrixInv*oneBox;
  
  // top bound
  double x1 = bounds.y1*k - 1;
  if (x1 < TConsts::epsilon) return;
  double x0 = -x1;
  
  // angle bounds
  double y;
  y = bounds.x0 < 0 ? bounds.y0 : bounds.y1;
  if (y > TConsts::epsilon) x0 = std::max(x0, bounds.x0/y);
  y = bounds.x1 < 0 ? bounds.y1 : bounds.y0;
  if (y > TConsts::epsilon) x1 = std::min(x1, bounds.x1/y);
  
  // delta bounds
  if (bounds.x0 < 0)
    x0 = std::max( x0, (1 - sqrt(1 - 4*bounds.x0*dk))*k/2 );
  if (bounds.x1 > 0)
    x1 = std::min( x1, (sqrt(1 + 4*bounds.x1*dk) - 1)*k/2 );
  
  // draw grid
  double i0 = ceil((x0 - baseX)/stepX);
  x0 = baseX + stepX*i0;
  for(double x = x0; x < x1; x += stepX) {
    double l = dk*(fabs(x) + 1);
    TPointD p0 = gridMatrix*TPointD(x*l, l);
    TPointD p1 = gridMatrix*TPointD(x*l*2, l*2);
    drawSegment(p0, p1, pixelSize, 0, alpha);
    if (bounds.y1 > l*2 + TConsts::epsilon) {
      TPointD p2 = gridMatrix*TPointD(x*bounds.y1, bounds.y1);
      drawSegment(p1, p2, pixelSize, alpha);
    }
  }
}


void TAssistantVanishingPoint::draw(TToolViewer*, bool enabled) const {
  double pixelSize = sqrt(tglGetPixelSize2());
  const TPointD &p = m_center.position;
  TPointD dx(20.0*pixelSize, 0.0);
  TPointD dy(0.0, 10.0*pixelSize);
  double alpha = getDrawingAlpha(enabled);
  drawSegment(p-dx-dy, p+dx+dy, pixelSize, alpha);
  drawSegment(p-dx+dy, p+dx-dy, pixelSize, alpha);
  if (getGrid()) {
    const TPointD &p0 = m_grid0.position;
    const TPointD &p1 = m_grid1.position;
    double gridAlpha = getDrawingGridAlpha();
    if (getPerspective())
      drawPerspectiveGrid(p, p0, p1, gridAlpha);
    else
      drawSimpleGrid(p, p0, p1, gridAlpha);
  }
}


void TAssistantVanishingPoint::drawEdit(TToolViewer *viewer) const {
  double pixelSize = sqrt(tglGetPixelSize2());
  drawSegment(m_center.position, m_a1.position, pixelSize);
  drawSegment(m_center.position, m_b1.position, pixelSize);
  TAssistant::drawEdit(viewer);
}



//*****************************************************************************************
//    Registration
//*****************************************************************************************


static TAssistantTypeT<TAssistantVanishingPoint> assistantVanishingPoint("assistantVanishingPoint");
