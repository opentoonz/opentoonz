

// TnzTools includes
#include "assistantellipse.h"


//*****************************************************************************************
//    TAssistantEllipse implementation
//*****************************************************************************************

TAssistantEllipse::TAssistantEllipse(TMetaObject &object):
  TAssistant(object),
  m_idCircle("circle"),
  m_idRestrictA("restrictA"),
  m_idRestrictB("restrictB"),
  m_idRepeat("repeat"),
  m_idGrid("grid"),
  m_idPerspective("perspective"),
  m_idPerspectiveDepth("perspectiveDepth"),
  m_center( addPoint("center", TAssistantPoint::CircleCross) ),
  m_a( addPoint("a", TAssistantPoint::CircleFill, TPointD(150,   0)) ),
  m_b( addPoint("b", TAssistantPoint::Circle,     TPointD(  0, 100)) ),
  m_grid0( addPoint("grid0",  TAssistantPoint::CircleDoubleDots, TPointD( 10, -30)) ),
  m_grid1( addPoint("grid1",  TAssistantPoint::CircleDots,       TPointD( 60, -60)) )
{
  addProperty( new TBoolProperty(m_idCircle.str(), getCircle()) );
  addProperty( new TBoolProperty(m_idRestrictA.str(), getRestrictA()) );
  addProperty( new TBoolProperty(m_idRestrictB.str(), getRestrictB()) );
  addProperty( new TBoolProperty(m_idRepeat.str(), getRepeat()) );
  addProperty( new TBoolProperty(m_idGrid.str(), getGrid()) );
  addProperty( new TBoolProperty(m_idPerspective.str(), getPerspective()) );
  addProperty( new TBoolProperty(m_idPerspectiveDepth.str(), getPerspectiveDepth()) );
}


QString TAssistantEllipse::getLocalName()
  { return tr("Ellipse"); }


void TAssistantEllipse::updateTranslation() const {
  TAssistant::updateTranslation();
  setTranslation(m_idCircle, tr("Circle"));
  setTranslation(m_idRestrictA, tr("Restrict A"));
  setTranslation(m_idRestrictB, tr("Restrict B"));
  setTranslation(m_idRepeat, tr("Repeat"));
  setTranslation(m_idGrid, tr("Grid"));
  setTranslation(m_idPerspective, tr("Perspective"));
  setTranslation(m_idPerspectiveDepth, tr("Depth"));
}


void TAssistantEllipse::onDataChanged(const TVariant &value) {
  TAssistant::onDataChanged(value);
  m_grid0.visible = m_grid1.visible = getGrid();
  if (getCircle() == m_b.visible) {
    m_b.visible = !getCircle();
    if (!m_b.visible)
      fixBAndGrid(m_center.position, m_a.position, m_b.position);
  }
}


void TAssistantEllipse::fixBAndGrid(
  TPointD prevCenter,
  TPointD prevA,
  TPointD prevB )
{
  const TPointD &center = m_center.position;
  TPointD da0 = prevA - prevCenter;
  TPointD da1 = m_a.position - center;
  double la0 = norm2(da0);
  double la1 = norm2(da1);
  if (!(la0 > TConsts::epsilon) || !(la1 > TConsts::epsilon))
    return;
  
  TPointD db = m_b.position - center;
  TPointD dp0 = TPointD(-da0.y, da0.x);
  TPointD dp1 = TPointD(-da1.y, da1.x);
  if (getCircle()) {
    m_b.position = center + (db*dp0 < 0 ? -dp1 : dp1);
  } else {
    m_b.position = db*dp0/la0*dp1 + center;
  }

  TPointD db0 = prevB - prevCenter;
  TPointD db1 = m_b.position - center;
  double lb0 = norm2(db0);
  double lb1 = norm2(db1);
  if (!(lb0 > TConsts::epsilon) || !(lb1 > TConsts::epsilon))
    return;

  TPointD dg0 = m_grid0.position - center;
  TPointD dg1 = m_grid1.position - center;
  m_grid0.position = dg0*da0/la0*da1 + dg0*db0/lb0*db1 + center;
  m_grid1.position = dg1*da0/la0*da1 + dg1*db0/lb0*db1 + center;
}


void TAssistantEllipse::onMovePoint(TAssistantPoint &point, const TPointD &position) {
  TPointD prevCenter = m_center.position;
  TPointD prevA = m_a.position;
  TPointD prevB = m_b.position;
  point.position = position;
  if (&point == &m_center) {
    TPointD d = m_center.position - prevCenter;
    m_a.position += d;
    m_b.position += d;
    m_grid0.position += d;
    m_grid1.position += d;
  } else
  if (&point == &m_a || &point == &m_b) {
    fixBAndGrid(prevCenter, prevA, prevB);
  }
}


TAffine TAssistantEllipse::calcEllipseMatrix() const {
  TPointD da = m_a.position - m_center.position;
  TPointD db = m_b.position - m_center.position;
  double r1 = norm(da);
  if (r1 <= TConsts::epsilon) return TAffine::zero();
  double r2 = fabs( (rotate90(da)*db)*(1.0/r1) );
  if (r2 <= TConsts::epsilon) return TAffine::zero();
  return TAffine::translation(m_center.position)
        * TAffine::rotation(atan(da))
        * TAffine::scale(r1, r2);
}


void TAssistantEllipse::getGuidelines(
  const TPointD &position,
  const TAffine &toTool,
  const TPixelD &color,
  TGuidelineList &outGuidelines ) const
{
  bool restrictA = getRestrictA();
  bool restrictB = getRestrictB();
  bool repeat = getRepeat();

  TAffine matrix = calcEllipseMatrix();
  if (matrix.isZero()) return;
  if (!restrictA && restrictB) {
    std::swap(matrix.a11, matrix.a12);
    std::swap(matrix.a21, matrix.a22);
    std::swap(restrictA, restrictB);
  }

  matrix = toTool*matrix;
  TAffine matrixInv = matrix.inv();

  if (restrictA && restrictB) {
    // ellipse
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineEllipse(
        getEnabled(),
        getMagnetism(),
        color,
        matrix,
        matrixInv )));
  } else
  if (!restrictA && !restrictB) {
    // scaled ellipse
    TPointD p = matrixInv*position;
    double l = norm(p);
    outGuidelines.push_back(TGuidelineP(
      new TGuidelineEllipse(
        getEnabled(),
        getMagnetism(),
        color,
        matrix * TAffine::scale(l) )));
  } else { // restrictA
    TPointD p = matrixInv*position;
    if (repeat) {
      double ox = round(0.5*p.x)*2.0;
      p.x -= ox;
      matrix *= TAffine::translation(ox, 0.0);
    }

    // scale by Y
    if (p.x <= TConsts::epsilon - 1.0) {
      // line x = -1
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineInfiniteLine(
          getEnabled(),
          getMagnetism(),
          color,
          matrix*TPointD(-1.0, 0.0),
          matrix*TPointD(-1.0, 1.0) )));
    } else
    if (p.x >= 1.0 - TConsts::epsilon) {
      // line x = 1
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineInfiniteLine(
          getEnabled(),
          getMagnetism(),
          color,
          matrix*TPointD(1.0, 0.0),
          matrix*TPointD(1.0, 1.0) )));
    } else {
      // ellipse scaled by Y
      double k = fabs(p.y/sqrt(1.0 - p.x*p.x));
      outGuidelines.push_back(TGuidelineP(
        new TGuidelineEllipse(
          getEnabled(),
          getMagnetism(),
          color,
          matrix * TAffine::scale(1.0, k) )));
    }
  }
}


void TAssistantEllipse::drawEllipseRanges(
  const TAngleRangeSet &ranges,
  const TAffine &ellipseMatrix,
  const TAffine &screenMatrixInv,
  double pixelSize,
  double alpha )
{
  assert(ranges.check());
  TAngleRangeSet actualRanges(ranges);
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  if (!TGuidelineEllipse::truncateEllipse(actualRanges, ellipseMatrix.inv()*screenMatrixInv, oneBox))
    return;
  assert(actualRanges.check());

  int segments = TGuidelineEllipse::calcSegmentsCount(ellipseMatrix, pixelSize);
  double da = M_2PI/segments;
  double s = sin(da);
  double c = cos(da);

  for(TAngleRangeSet::Iterator i(actualRanges); i; ++i) {
    double a0 = i.d0();
    double a1 = i.d1greater();
    int cnt = (int)floor((a1 - a0)/da);
    TPointD r(cos(a0), sin(a0));
    TPointD p0 = ellipseMatrix*r;
    for(int j = 0; j < cnt; ++j) {
      r = TPointD(r.x*c - r.y*s, r.y*c + r.x*s);
      TPointD p1 = ellipseMatrix*r;
      drawSegment(p0, p1, pixelSize, alpha);
      p0 = p1;
    }
    drawSegment(p0, ellipseMatrix*TPointD(cos(a1), sin(a1)), pixelSize, alpha);
  }
}


void TAssistantEllipse::drawRuler(
  const TAffine &ellipseMatrix,
  const TPointD &grid0,
  const TPointD &grid1,
  bool perspective,
  double alpha
) {
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = (perspective ? 5 : 10)*pixelSize;

  TAffine em = ellipseMatrix;
  TAffine ellipseMatrixInv = ellipseMatrix.inv();
  TPointD g0 = ellipseMatrixInv * grid0;
  TPointD g1 = ellipseMatrixInv * grid1;
  if (norm2(g0) <= TConsts::epsilon*TConsts::epsilon) return;
  if (norm2(g1) <= TConsts::epsilon*TConsts::epsilon) return;
  double ga0 = atan(g0);
  double ga1 = atan(g1);

  // x and y radiuses
  TPointD r( norm2(TPointD(em.a11, em.a21)), norm2(TPointD(em.a12, em.a22)) );
  double avgR = 0.5*(r.x + r.y);
  if (avgR <= TConsts::epsilon*TConsts::epsilon) return;
  avgR = sqrt(avgR);
  double actualMinStep = minStep/avgR;
  r.x = sqrt(r.x);
  r.y = sqrt(r.y);
  
  // remove radiuses from ellipse matrix
  double rkx = r.x > TConsts::epsilon ? 1.0/r.x : 0.0;
  double rky = r.y > TConsts::epsilon ? 1.0/r.y : 0.0;
  em.a11 *= rkx; em.a21 *= rkx;
  em.a12 *= rky; em.a22 *= rky;
  
  if (perspective) {
    // draw perspective
    if (ga0 < 0.0) { if (ga1 > 0.0) ga1 -= M_2PI; }
              else { if (ga1 < 0.0) ga1 += M_2PI; }
    double w, i0, i1;
    double bound0 = ga0 < 0 ? -M_2PI : 0;
    double bound1 = ga0 < 0 ? 0 : M_2PI;
    if (!calcPerspectiveStep(actualMinStep, bound0, bound1, 0, ga0, ga1, w, i0, i1)) return;
    for(double i = i0; i < i1; i += 1) {
      double a = 1/(i*w + 1);
      TPointD p( cos(a), sin(a) );
      TPointD n( p.x*r.y, p.y*r.x ); // perp to allipse
      double nl2 = norm2(n);
      if (nl2 > TConsts::epsilon*TConsts::epsilon) {
        p.x *= r.x;
        p.y *= r.y;
        n = n*(1.0/sqrt(nl2));
        drawMark(em*p, em.transformDirection(n), pixelSize, alpha);
      }
    }
  } else {
    // draw linear
    double da = ga1 - ga0;
    if (da < 0.0)         { da = -da;        std::swap(ga0, ga1); }
    if (ga1 - ga0 > M_PI) { da = M_2PI - da; std::swap(ga0, ga1); }
    if (da < actualMinStep) return;
    for(double a = ga0 - floor(M_PI/da)*da; a < ga0 + M_PI; a += da) {
      TPointD p( cos(a), sin(a) );
      TPointD n( p.x*r.y, p.y*r.x ); // perp to allipse
      double nl2 = norm2(n);
      if (nl2 > TConsts::epsilon*TConsts::epsilon) {
        p.x *= r.x;
        p.y *= r.y;
        n = n*(1.0/sqrt(nl2));
        drawMark(em*p, em.transformDirection(n), pixelSize, alpha);
      }
    }
  }
}


void TAssistantEllipse::drawConcentricGrid(
  const TAffine &ellipseMatrix,
  const TPointD &grid0,
  const TPointD &grid1,
  bool perspective,
  double alpha )
{
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine screenMatrix = (projection*modelview).get2d();
  TAffine screenMatrixInv = screenMatrix.inv();
  
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = (perspective ? 2.5 : 10.0)*pixelSize;
  TAffine ellipseMatrixInv = ellipseMatrix.inv();

  // calculate bounds
  TAffine matrixInv = ellipseMatrixInv * screenMatrixInv;
  TPointD o  = matrixInv * TPointD(-1.0, -1.0);
  TPointD dx = matrixInv.transformDirection( TPointD(2.0, 0.0) );
  TPointD dy = matrixInv.transformDirection( TPointD(0.0, 2.0) );
  double max = 0.0;
  double min = std::numeric_limits<double>::infinity();

  // distance to points
  TPointD corners[] = { o, o+dx, o+dx+dy, o+dy };
  for(int i = 0; i < 4; ++i) {
    double k = norm(corners[i]);
    if (k < min) min = k;
    if (k > max) max = k;
  }

  // distance to sides
  TPointD lines[] = { dx, dy, -1.0*dx, -1.0*dy };
  int positive = 0, negative = 0;
  for(int i = 0; i < 4; ++i) {
    double len2 = norm2(lines[i]);
    if (len2 <= TConsts::epsilon*TConsts::epsilon) continue;
    double k = (corners[i]*rotate90(lines[i]))/sqrt(len2);
    if (k > TConsts::epsilon) ++positive;
    if (k < TConsts::epsilon) ++negative;
    double l = -(corners[i]*lines[i]);
    if (l <= TConsts::epsilon || l >= len2 - TConsts::epsilon) continue;
    k = fabs(k);
    if (k < min) min = k;
    if (k > max) max = k;
  }

  // if center is inside bounds
  if (min < 0.0 || positive == 0 || negative == 0) min = 0.0;
  if (max <= min) return;

  // draw
  const TAffine &em = ellipseMatrix;
  double r = sqrt(std::min( norm2(TPointD(em.a11, em.a21)), norm2(TPointD(em.a12, em.a22)) ));
  double actualMinStep = minStep/r;
  double gs0 = norm(ellipseMatrixInv*grid0);
  double gs1 = norm(ellipseMatrixInv*grid1);
  if (gs0 <= TConsts::epsilon*TConsts::epsilon) return;
  if (gs1 <= TConsts::epsilon*TConsts::epsilon) return;

  if (perspective) {
    // draw perspective
    double w, i0, i1;
    actualMinStep /= 2;
    if (!calcPerspectiveStep(actualMinStep, min, max, 0, gs0, gs1, w, i0, i1)) return;
    for(double i = i0; i < i1; i += 1) {
      double x = 1/(i*w + 1);
      
      double curStep = fabs(w*x*x);
      double curAlpha = (curStep - actualMinStep)/actualMinStep;
      if (curAlpha < 0) continue;
      if (curAlpha > 1) curAlpha = 1;
      
      drawEllipse(ellipseMatrix * TAffine::scale(x), screenMatrixInv, pixelSize, alpha*curAlpha);
    }
  } else {
    // draw linear
    double dx = fabs(gs1 - gs0);
    if (dx*r < minStep) return;
    for(double x = gs0 + ceil((min - gs0)/dx)*dx; x < max; x += dx)
      drawEllipse(ellipseMatrix * TAffine::scale(x), screenMatrixInv, pixelSize, alpha);
  }
}


void TAssistantEllipse::drawParallelGrid(
  const TAffine &ellipseMatrix,
  const TPointD &grid0,
  const TPointD &grid1,
  const TPointD *bound0,
  const TPointD *bound1,
  bool perspective,
  bool perspectiveDepth,
  bool repeat,
  double alpha )
{
  struct {
    const bool repeat;
    const TAffine ellipseMatrixInv;
    double convert(const TPointD &p) {
      TPointD pp = ellipseMatrixInv*p;
      if (repeat)
        pp.x -= round(pp.x/2)*2;
      if (!(fabs(pp.x) < 1 - TConsts::epsilon))
        return pp.y < 0 ? -INFINITY : INFINITY;
      return pp.y/sqrt(1 - pp.x*pp.x);
    }
  } helper = { repeat, ellipseMatrix.inv() };
  
  
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine screenMatrix = (projection*modelview).get2d();
  TAffine screenMatrixInv = screenMatrix.inv();
  
  double pixelSize = sqrt(tglGetPixelSize2());
  double minStep = (perspective ? 5 : 10)*pixelSize;

  const TAffine &em = ellipseMatrix;
  double r = sqrt(0.5*(norm2(TPointD(em.a11, em.a21)) + norm2(TPointD(em.a12, em.a22))));
  double actualMinStep = minStep/r;
  
  // convert grid
  double gs0 = helper.convert(grid0);
  double gs1 = helper.convert(grid1);
  if (!(fabs(gs0) < 1 - TConsts::epsilon)) return;
  if (!(fabs(gs1) < 1 - TConsts::epsilon)) return;
  
  // convert bounds
  double bs0 = -INFINITY, bs1 = INFINITY;
  if (bound0 && bound1) {
    bs0 = helper.convert(*bound0);
    bs1 = helper.convert(*bound1);
  } else
  if (bound0) {
    bs0 = helper.convert(*bound0);
    if (bs0 < 0) bs1 = -INFINITY;
  } else
  if (bound1) {
    bs1 = helper.convert(*bound1);
    if (bs1 < 0) bs0 = INFINITY;
  }
  
  if (bs0 > bs1) std::swap(bs0, bs1);
  bs0 -= TConsts::epsilon;
  bs1 += TConsts::epsilon;
  
  // prepare ranges
  TAngleRangeSet ranges;
  ranges.add( TAngleRangeSet::fromDouble(0.0), TAngleRangeSet::fromDouble(M_PI) );

  if (perspectiveDepth) {
    // draw perspective depth
    if (!( fabs(gs0) > TConsts::epsilon
        && fabs(gs1) > TConsts::epsilon
        && fabs(gs0) < 1 - TConsts::epsilon
        && fabs(gs1) < 1 - TConsts::epsilon
        && fabs(gs1 - gs0) > TConsts::epsilon )) return;

    // the formula is: x = 1/sqrt(1 + i*i)
    double ig0 = sqrt(1/(gs0*gs0) - 1);
    double ig1 = sqrt(1/(gs1*gs1) - 1);
    double di = fabs(ig1 - ig0);
    double i0 = ig0 - floor(ig0/di)*di;
    if (i0 <= TConsts::epsilon) i0 += di;
    
    actualMinStep /= 2;
    double sign = gs0 < 0 ? -1 : 1;
    for(double i = i0; i+di != i; i += di) {
      double x = 1/sqrt(1 + i*i);
      
      double curStep = fabs( di*i*x*x*x );
      double curAlpha = (curStep - actualMinStep)/actualMinStep;
      if (curAlpha < 0) { if (i == i0) continue; else break; }
      if (curAlpha > 1) curAlpha = 1;
      
      x *= sign;
      if (x < bs0 || bs1 < x) continue;
      
      drawEllipseRanges(
        ranges,
        ellipseMatrix*TAffine::scale(sign, x),
        screenMatrixInv,
        pixelSize,
        alpha * curAlpha );
    }
  } else
  if (perspective) {
    // draw perspective
    if (!( fabs(gs0) < 1 - TConsts::epsilon
        && fabs(gs1) < 1 - TConsts::epsilon
        && fabs(gs1 - gs0) > TConsts::epsilon )) return;

    // the formula is: x = i/sqrt(1 + i*i)
    double ig0 = gs0/sqrt(1 - gs0*gs0);
    double ig1 = gs1/sqrt(1 - gs1*gs1);
    double di = fabs(ig1 - ig0);
    double i0 = ig0 - round(ig0/di)*di;
    
    actualMinStep /= 2;
    for(int j = 0; j < 2; ++j, di = -di, i0 += di)
    for(double i = i0; i+di != i; i += di) {
      double x = i/sqrt(1 + i*i);
      
      double curAlpha = 1;
      if (fabs(i) > TConsts::epsilon) {
        double curStep = fabs( di*x*(1 - x*x)/i );
        curAlpha = (curStep - actualMinStep)/actualMinStep;
        if (curAlpha < 0) { if (i == i0) continue; else break; }
        if (curAlpha > 1) curAlpha = 1;
      }
      if (x < bs0 || bs1 < x) continue;
        
      drawEllipseRanges(
        ranges,
        ellipseMatrix*TAffine::scale(x < 0.0 ? -1.0 : 1.0, x),
        screenMatrixInv,
        pixelSize,
        alpha * curAlpha );
    }
  } else {
    // draw linear
    double dx = fabs(gs1 - gs0);
    if (dx < actualMinStep) return;
    for(double x = gs0 + ceil((-1.0 - gs0)/dx)*dx; x < 1.0; x += dx) {
      if (x < bs0 || bs1 < x) continue;
      drawEllipseRanges(
        ranges,
        ellipseMatrix*TAffine::scale(x < 0.0 ? -1.0 : 1.0, x),
        screenMatrixInv,
        pixelSize,
        alpha );
    }
  }
}


void TAssistantEllipse::draw(
  const TAffine &ellipseMatrix,
  const TAffine &screenMatrixInv,
  double ox,
  double pixelSize,
  bool enabled ) const
{
  const double crossSize = 0.1;

  double alpha = getDrawingAlpha(enabled);
  double gridAlpha = getDrawingGridAlpha();
  bool grid = getGrid();
  bool ruler = getRestrictA() && getRestrictB();
  bool concentric = !getRestrictA() && !getRestrictB();

  drawSegment( ellipseMatrix*TPointD(-crossSize, 0.0),
                ellipseMatrix*TPointD( crossSize, 0.0), pixelSize, alpha);
  drawSegment( ellipseMatrix*TPointD(0.0, -crossSize),
                ellipseMatrix*TPointD(0.0,  crossSize), pixelSize, alpha);
  drawEllipse(ellipseMatrix, screenMatrixInv, pixelSize, alpha);
  if (ox > 1.0)
    drawSegment( ellipseMatrix*TPointD(-1.0, -1.0),
                  ellipseMatrix*TPointD(-1.0,  1.0), pixelSize, alpha);
  else if (ox < -1.0)
    drawSegment( ellipseMatrix*TPointD( 1.0, -1.0),
                  ellipseMatrix*TPointD( 1.0,  1.0), pixelSize, alpha);

  if (!grid) return;

  if (ruler) {
    drawRuler(
      ellipseMatrix,
      m_grid0.position,
      m_grid1.position,
      getPerspective(),
      alpha );
  } else
  if (concentric) {
    drawConcentricGrid(
      ellipseMatrix,
      m_grid0.position,
      m_grid1.position,
      getPerspective(),
      gridAlpha );
  } else {
    drawParallelGrid(
      ellipseMatrix,
      m_grid0.position,
      m_grid1.position,
      nullptr,
      nullptr,
      getPerspective(),
      getPerspectiveDepth(),
      getRepeat(),
      gridAlpha );
  }
}


void TAssistantEllipse::draw(TToolViewer*, bool enabled) const {
  bool restrictA = getRestrictA();
  bool restrictB = getRestrictB();
  bool repeat = getRepeat();
  double minStep = 30.0;

  TAffine ellipseMatrix = calcEllipseMatrix();
  if (ellipseMatrix.isZero()) return;
  if (!restrictA && restrictB) {
    std::swap(ellipseMatrix.a11, ellipseMatrix.a12);
    std::swap(ellipseMatrix.a21, ellipseMatrix.a22);
  }

  // common data about viewport
  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine matrix = (projection*modelview).get2d();
  TAffine matrixInv = matrix.inv();
  double pixelSize = sqrt(tglGetPixelSize2());

  if (!repeat || restrictA == restrictB || norm(TPointD(ellipseMatrix.a11, ellipseMatrix.a21)) < minStep*pixelSize) {
    draw(ellipseMatrix, matrixInv, 0.0, pixelSize, enabled);
  } else {
    // calculate bounds
    TPointD o(ellipseMatrix.a13, ellipseMatrix.a23);
    TPointD proj(ellipseMatrix.a11, ellipseMatrix.a21);
    proj = proj * (1.0/norm2(proj));
    TPointD corners[4] = {
      TPointD(oneBox.x0, oneBox.y0),
      TPointD(oneBox.x0, oneBox.y1),
      TPointD(oneBox.x1, oneBox.y0),
      TPointD(oneBox.x1, oneBox.y1) };
    double minX = 0.0, maxX = 0.0;
    for(int i = 0; i < 4; ++i) {
      double x = proj * (matrixInv*corners[i] - o);
      if (i == 0 || x < minX) minX = x;
      if (i == 0 || x > maxX) maxX = x;
    }
    if (maxX <= minX) return;

    // draw
    for(double ox = round(0.5*minX)*2.0; ox - 1.0 < maxX; ox += 2.0)
      draw(ellipseMatrix*TAffine::translation(ox, 0.0), matrixInv, ox, pixelSize, enabled);
  }
}


//*****************************************************************************************
//    Registration
//*****************************************************************************************

static TAssistantTypeT<TAssistantEllipse> assistantEllipse("assistantEllipse");

