

// TnzTools includes
#include "assistantline.h"



//*****************************************************************************************
//    TAssistantLine implementation
//*****************************************************************************************


TAssistantLine::TAssistantLine(TMetaObject &object):
  TAssistant(object),
  m_idRestricktA("restrictA"),
  m_idRestricktB("restrictB"),
  m_idParallel("parallel"),
  m_idGrid("grid"),
  m_idPerspective("perspective"),
  m_a( addPoint("a", TAssistantPoint::CircleCross) ),
  m_b( addPoint("b", TAssistantPoint::Circle, TPointD(100.0, 0.0)) ),
  m_grid0( addPoint("grid0",  TAssistantPoint::CircleDoubleDots, TPointD(  0.0,-50.0)) ),
  m_grid1( addPoint("grid1",  TAssistantPoint::CircleDots,       TPointD( 25.0,-75.0)) )
{
  addProperty( new TBoolProperty(m_idRestricktA.str(), getRestrictA()) );
  addProperty( new TBoolProperty(m_idRestricktB.str(), getRestrictB()) );
  addProperty( new TBoolProperty(m_idParallel.str(), getParallel()) );
  addProperty( new TBoolProperty(m_idGrid.str(), getGrid()) );
  addProperty( new TBoolProperty(m_idPerspective.str(), getPerspective()) );
}


QString TAssistantLine::getLocalName()
  { return tr("Line"); }


void TAssistantLine::updateTranslation() const {
  TAssistant::updateTranslation();
  setTranslation(m_idRestricktA, tr("Restrict A"));
  setTranslation(m_idRestricktB, tr("Restrict B"));
  setTranslation(m_idParallel, tr("Parallel"));
  setTranslation(m_idGrid, tr("Grid"));
  setTranslation(m_idPerspective, tr("Perspective"));
}


void TAssistantLine::onDataChanged(const TVariant &value) {
  TAssistant::onDataChanged(value);
  m_grid0.visible = getGrid()
                  || (getParallel() && (getRestrictA() || getRestrictB()));
  m_grid1.visible = getGrid();
}


void TAssistantLine::fixGrid(const TPointD &prevA, const TPointD &prevB) {
  TPointD dx0 = prevB - prevA;
  TPointD dx1 = m_b.position - m_a.position;
  double l0 = norm2(dx0);
  double l1 = norm2(dx1);
  if (!( l0 > TConsts::epsilon*TConsts::epsilon
     &&  l1 > TConsts::epsilon*TConsts::epsilon )) return;
  dx0 *= 1/sqrt(l0);
  dx1 *= 1/sqrt(l1);
  TPointD dy0(-dx0.y, dx0.x);
  TPointD dy1(-dx1.y, dx1.x);

  if (getParallel()) {
    TPointD g1 = m_grid1.position - m_grid0.position;
    m_grid1.position = m_grid0.position + g1*dx0*dx1 + g1*dy0*dy1;
  } else {
    TPointD g0 = m_grid0.position - prevA;
    TPointD g1 = m_grid1.position - prevA;
    m_grid0.position = m_a.position + g0*dx0*dx1 + g0*dy0*dy1;
    m_grid1.position = m_a.position + g1*dx0*dx1 + g1*dy0*dy1;
  }
}


void TAssistantLine::onMovePoint(TAssistantPoint &point, const TPointD &position) {
  TPointD prevA = m_a.position;
  TPointD prevB = m_b.position;
  point.position = position;
  if (&point == &m_a)
    m_b.position += m_a.position - prevA;
  if (&point != &m_grid1)
    fixGrid(prevA, prevB);
}


void TAssistantLine::getGuidelines(
  const TPointD &position,
  const TAffine &toTool,
  const TPixelD &color,
  TGuidelineList &outGuidelines ) const
{
  bool restrictA = getRestrictA();
  bool restrictB = getRestrictB();
  bool parallel = getParallel();
  bool perspective = getPerspective();

  TPointD a = toTool*m_a.position;
  TPointD b = toTool*m_b.position;
  TPointD ab = b - a;
  double abLen2 = norm2(ab);
  if (abLen2 < TConsts::epsilon*TConsts::epsilon) return;

  if (parallel) {
    TPointD abp = rotate90(ab);
    TPointD ag = toTool*m_grid0.position - a;
    double k = abp*ag;
    if (fabs(k) <= TConsts::epsilon) {
      if (restrictA || restrictB) return;
      a = position;
    } else {
      k = (abp*(position - a))/k;
      a = a + ag*k;
    }
    if (perspective && (restrictA || restrictB))
      ab = ab*k;
    b = a + ab;
  }

  if (restrictA && restrictB)
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineLine(
        getEnabled(), getMagnetism(), color, a,  b )));
  else if (restrictA)
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineRay(
        getEnabled(), getMagnetism(), color, a,  b )));
  else if (restrictB)
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineRay(
        getEnabled(), getMagnetism(), color, b,  a ))); // b first
  else
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineInfiniteLine(
        getEnabled(), getMagnetism(), color, a,  b )));
}


void TAssistantLine::drawRuler(
  const TPointD &a,
  const TPointD &b,
  const TPointD &grid0,
  const TPointD &grid1,
  const TPointD *perspectiveBase,
  double alpha )
{
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = (perspectiveBase ? 5 : 10)*pixelSize;

  TPointD direction = b - a;
  double l2 = norm2(direction);
  if (l2 <= TConsts::epsilon*TConsts::epsilon) return;
  double dirLen = sqrt(l2);
  TPointD dirProj = direction*(1.0/l2);
  TPointD normal = TPointD(-direction.y, direction.x)*(1.0/dirLen);

  double xg0 = dirProj*(grid0 - a);
  double xg1 = dirProj*(grid1 - a);

  if (perspectiveBase) {
    // draw perspective
    double xa0 = dirProj*(*perspectiveBase - a);
    double w, i0, i1;
    if (!calcPerspectiveStep(minStep/dirLen, 0, 1, xa0, xg0, xg1, w, i0, i1)) return;
    for(double i = i0; i < i1; i += 1) {
      double x = xa0 + 1/(i*w + 1);
      drawMark(a + direction*x, normal, pixelSize, alpha);
    }
  } else {
    // draw linear
    double dx = fabs(xg1 - xg0);
    if (dx*dirLen < minStep) return;
    for(double x = xg0 - floor(xg0/dx)*dx; x < 1.0; x += dx)
      drawMark(a + direction*x, normal, pixelSize, alpha);
  }
}


void TAssistantLine::drawLine(
  const TAffine &matrix,
  const TAffine &matrixInv,
  double pixelSize,
  const TPointD &a,
  const TPointD &b,
  bool restrictA,
  bool restrictB,
  double alpha )
{
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  TPointD aa = matrix*a;
  TPointD bb = matrix*b;
  if ( restrictA && restrictB ? TGuidelineLineBase::truncateLine(oneBox, aa, bb)
      : restrictA             ? TGuidelineLineBase::truncateRay (oneBox, aa, bb)
      : restrictB             ? TGuidelineLineBase::truncateRay (oneBox, bb, aa) // aa first
      :                 TGuidelineLineBase::truncateInfiniteLine(oneBox, aa, bb) )
        drawSegment(matrixInv*aa, matrixInv*bb, pixelSize, alpha);
}


void TAssistantLine::drawGrid(
  const TPointD &a,
  const TPointD &b,
  const TPointD &grid0,
  const TPointD &grid1,
  bool restrictA,
  bool restrictB,
  bool perspective,
  double alpha )
{
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine matrix = (projection*modelview).get2d();
  TAffine matrixInv = matrix.inv();
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = (perspective ? 2.5 : 10)*pixelSize;

  TPointD ab = b - a;
  double abLen2 = norm2(ab);
  if (abLen2 < TConsts::epsilon*TConsts::epsilon) return;
  double abLen = sqrt(abLen2);

  TPointD abp = rotate90(ab);
  TPointD ag = grid0 - a;
  if (fabs(abp*ag) <= TConsts::epsilon) {
    if (restrictA || restrictB) return;
    ag = abp;
  }
  double agLen2 = norm2(ag);
  if (agLen2 < TConsts::epsilon*TConsts::epsilon) return;
  double abpAgK = 1.0/(abp*ag);
  TPointD abpAgProj = abp*abpAgK;

  // draw restriction lines
  if (perspective) {
    if (restrictA) drawLine(matrix, matrixInv, pixelSize, a, a + ag, false, false, alpha);
    if (restrictB) drawLine(matrix, matrixInv, pixelSize, a, a + ag + ab, false, false, alpha);
    // horizon
    if (!restrictA) drawLine(matrix, matrixInv, pixelSize, a - ab, a, restrictA, restrictB, alpha); else
    if (!restrictB) drawLine(matrix, matrixInv, pixelSize, a, a + ab, restrictA, restrictB, alpha);
  } else {
    if (restrictA) drawLine(matrix, matrixInv, pixelSize, a, a + ag, false, false, alpha);
    if (restrictB) drawLine(matrix, matrixInv, pixelSize, b, b + ag, false, false, alpha);
  }

  double minStepX = fabs(minStep*abLen*abpAgK);
  if (minStepX <= TConsts::epsilon) return;

  // calculate bounds
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  TPointD corners[4] = {
    TPointD(oneBox.x0, oneBox.y0),
    TPointD(oneBox.x0, oneBox.y1),
    TPointD(oneBox.x1, oneBox.y0),
    TPointD(oneBox.x1, oneBox.y1) };
  double minX = 0.0, maxX = 0.0;
  for(int i = 0; i < 4; ++i) {
    double x = abpAgProj * (matrixInv*corners[i] - a);
    if (i == 0 || x < minX) minX = x;
    if (i == 0 || x > maxX) maxX = x;
  }
  if (maxX <= minX) return;

  double x0 = abpAgProj*(grid0 - a);
  double x1 = abpAgProj*(grid1 - a);

  if (perspective) {
    double w, i0, i1;
    minStepX /= 2;
    
    if (!calcPerspectiveStep(minStepX, minX, maxX, 0, x0, x1, w, i0, i1)) return;
    double abk = 1.0/fabs(x0);
    for(double i = i0; i < i1; i += 1) {
      double x = 1/(i*w + 1);
      
      double curStep = fabs(w*x*x);
      double curAlpha = (curStep - minStepX)/minStepX;
      if (curAlpha < 0) continue;
      if (curAlpha > 1) curAlpha = 1;
      
      TPointD ca = a + ag*x;
      TPointD cb = ca + ab*(abk*x);
      drawLine(matrix, matrixInv, pixelSize, ca, cb, restrictA, restrictB, alpha*curAlpha);
    }
  } else {
    double dx = fabs(x1 - x0);
    if (dx < minStepX) return;
    for(double x = x0 + ceil((minX - x0)/dx)*dx; x < maxX; x += dx) {
      TPointD ca = a + ag*x;
      drawLine(matrix, matrixInv, pixelSize, ca, ca + ab, restrictA, restrictB, alpha);
    }
  }
}


void TAssistantLine::draw(TToolViewer*, bool enabled) const {
  double alpha = getDrawingAlpha(enabled);
  bool restrictA = getRestrictA();
  bool restrictB = getRestrictB();
  bool parallel = getParallel();
  bool grid = getGrid();
  bool perspective = getPerspective();

  // common data about viewport
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine matrix = (projection*modelview).get2d();
  TAffine matrixInv = matrix.inv();
  double pixelSize = sqrt(tglGetPixelSize2());

  // calculate range
  TPointD aa = matrix*m_a.position;
  TPointD bb = matrix*m_b.position;
  bool success = false;
  if (restrictA && restrictB)
    success = TGuidelineLineBase::truncateLine(oneBox, aa, bb);
  else if (restrictA)
    success = TGuidelineLineBase::truncateRay(oneBox, aa, bb);
  else if (restrictB)
    success = TGuidelineLineBase::truncateRay(oneBox, bb, aa);
  else
    success = TGuidelineLineBase::truncateInfiniteLine(oneBox, aa, bb);

  if (!success) {
    // line is out of screen, bud grid still can be visible
    if (grid && getParallel())
        drawGrid(
          m_a.position,
          m_b.position,
          m_grid0.position,
          m_grid1.position,
          restrictA,
          restrictB,
          perspective,
          getDrawingGridAlpha() );
    return;
  }
  
  TPointD a = matrixInv*aa;
  TPointD b = matrixInv*bb;

  // draw line
  drawSegment(a, b, pixelSize, alpha);

  // draw restriction marks
  if (restrictA || (!parallel && grid && perspective))
    drawDot(m_a.position);
  if (restrictB)
    drawDot(m_b.position);

  if (grid) {
    if (getParallel()) {
      drawGrid(
        m_a.position,
        m_b.position,
        m_grid0.position,
        m_grid1.position,
        restrictA,
        restrictB,
        perspective,
        getDrawingGridAlpha() );
    } else {
      drawRuler(
        a, b, m_grid0.position, m_grid1.position,
        perspective ? &m_a.position : nullptr, getDrawingAlpha() );
    }
  }
}



//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantLine> assistantLine("assistantLine");
