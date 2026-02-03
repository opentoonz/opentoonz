

#include <tools/assistants/guidelineellipse.h>

// TnzCore includes
#include <tgl.h>
#include <tmathutil.h>



// returns a point nearest to the ellipse with center in zero
static TPointD findNearestPoint(const TPointD &p, double Rx, double Ry) {
  Rx = fabs(Rx);
  Ry = fabs(Ry);
  TPointD ep;
  
  if (isAlmostZero(Rx)) {
    ep.y = p.y < -Ry ? -Ry
         : p.y >  Ry ?  Ry : p.y;
    return ep;
  }

  if (isAlmostZero(Ry)) {
    ep.x = p.x < -Rx ? -Rx
         : p.x >  Rx ?  Rx : p.x;
    return ep;
  }

  double k = 1/Rx;
  double x0 = p.x*k;
  double y0 = p.y*k;
  k *= Ry;
  k *= k; // k = (Ry/Rx)^2
  double l = k - 1;

  double a = l*l;
  double b = 2*l*x0;
  double c = x0*x0 + y0*y0*k - l*l;
  double d = -b;
  double e = -x0*x0;

  double dist = INFINITY;
  std::complex<double> roots[4];
  int cnt = solveEquation4(roots, a, b, c, d, e);
  for(int i = 0; i < cnt; ++i) {
    if (!isAlmostZero(roots[i].imag()))
      continue;
    
    double x = roots[i].real();
    double y;
    if (isAlmostZero(fabs(x) - 1)) {
      y = 0;
    } else
    if (fabs(x) < 1) {
      y = sqrt(k*(1 - x*x));
      if (y0 < 0) y = -y;
    } else {
      continue;
    }

    double dd = (x0-x)*(x0-x) + (y0-y)*(y0-y);
    if (dd < dist)
      { ep.x = x*Rx; ep.y = y*Rx; dist = dd; }
  }
  return ep;
}



//*****************************************************************************************
//    TGuidelineEllipse implementation
//*****************************************************************************************


TGuidelineEllipse::TGuidelineEllipse(
  bool enabled,
  double magnetism,
  const TPixelD &color,
  const TAffine &matrix,
  const TAffine &matrixInv,
  double Rx,
  double Ry
):
  TGuideline(enabled, magnetism, color),
  matrix(matrix),
  matrixInv(matrixInv),
  Rx(Rx),
  Ry(Ry)
  { }


TGuidelineEllipse::TGuidelineEllipse(
  bool enabled,
  double magnetism,
  const TPixelD &color,
  const TAffine &matrix,
  const TAffine &matrixInv
):
  TGuidelineEllipse(
    enabled, magnetism, color, matrix, matrixInv,
    sqrt(matrix.a11*matrix.a11 + matrix.a21*matrix.a21),
    sqrt(matrix.a12*matrix.a12 + matrix.a22*matrix.a22) )
  { }


TGuidelineEllipse::TGuidelineEllipse(
  bool enabled,
  double magnetism,
  const TPixelD &color,
  const TAffine &matrix
):
  TGuidelineEllipse(enabled, magnetism, color, matrix, matrix.inv())
  { }



TTrackPoint
TGuidelineEllipse::transformPoint(const TTrackPoint &point) const
{
  TTrackPoint p = point;
  TPointD pp = matrixInv*p.position;
  double l2 = norm2(pp);
  if (l2 > TConsts::epsilon*TConsts::epsilon)
    p.position = matrix*(pp*(1.0/sqrt(l2)));
  return p;
}


TPointD
TGuidelineEllipse::nearestPoint(const TPointD &point) const
{
  TPointD p = matrixInv*point;
  p = findNearestPoint(TPointD(p.x*Rx, p.y*Ry), Rx, Ry);
  if (!isAlmostZero(Rx)) p.x /= Rx;
  if (!isAlmostZero(Ry)) p.y /= Ry;
  return matrix*p;
}



bool
TGuidelineEllipse::truncateEllipse(
  TAngleRangeSet &ranges,
  const TAffine &ellipseMatrixInv,
  const TRectD &bounds )
{
  if (ranges.isEmpty()) return false;
  if (bounds.isEmpty()) { ranges.clear(); return false; }

  TPointD o  = ellipseMatrixInv*bounds.getP00();
  TPointD dx = ellipseMatrixInv.transformDirection(TPointD(bounds.getLx(), 0.0));
  TPointD dy = ellipseMatrixInv.transformDirection(TPointD(0.0, bounds.getLy()));
  double lx2 = norm2(dx);
  double ly2 = norm2(dy);
  if ( lx2 < TConsts::epsilon*TConsts::epsilon
    || ly2 < TConsts::epsilon*TConsts::epsilon )
      { ranges.clear(); return false; }
  TPointD nx = rotate90(dx)*(1.0/sqrt(lx2));
  TPointD ny = rotate90(dy)*(1.0/sqrt(ly2));

  TAngleI ax = TAngleRangeSet::fromDouble(atan(dx));
  TAngleI ay = TAngleRangeSet::fromDouble(atan(dy));

  double sign = nx*dy;
  if (fabs(sign) <= TConsts::epsilon)
    { ranges.clear(); return false; }

  if (sign < 0.0) {
    nx = -nx; ny = -ny;
    ax ^= TAngleRangeSet::half;
    ay ^= TAngleRangeSet::half;
  }

  TAngleI angles[] = {
    ax,
    ax^TAngleRangeSet::half,
    ay,
    ay^TAngleRangeSet::half };

  double heights[] = {
    o*nx,
    -((o+dx+dy)*nx),
    (o+dx)*ny,
    -((o+dy)*ny) };

  for(int i = 0; i < 4; ++i) {
    double h = heights[i];
    if (heights[i] <= TConsts::epsilon - 1.0)
      continue;
    if (h >= 1.0 - TConsts::epsilon)
      { ranges.clear(); return false; }
    TAngleI a = TAngleRangeSet::fromDouble(asin(h));
    TAngleI da = angles[i];
    ranges.subtract(da - a, (da + a)^TAngleRangeSet::half );
    if (ranges.isEmpty()) return false;
  }

  return true;
}


int
TGuidelineEllipse::calcSegmentsCount(const TAffine &ellipseMatrix, double pixelSize) {
  const TAffine &em = ellipseMatrix;
  const int min = 4, max = 1000;
  double r = sqrt(0.5*(norm2(TPointD(em.a11, em.a21)) + norm2(TPointD(em.a12, em.a22))));
  double h = 0.5*pixelSize/r;
  if (h <= TConsts::epsilon) return max;
  if (h >= 1.0 - TConsts::epsilon) return min;
  double segments = round(M_2PI/acos(1.0 - h));
  return segments <= (double)min ? min
       : segments >= (double)max ? max
       : (int)segments;
}


void
TGuidelineEllipse::draw(bool active, bool enabled) const {
  TAffine4 modelview, projection;
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview.a);
  glGetDoublev(GL_PROJECTION_MATRIX, projection.a);
  TAffine screenMatrix = (projection*modelview).get2d();
  TAffine screenMatrixInv = screenMatrix.inv();
  double pixelSize = sqrt(tglGetPixelSize2());

  const TRectD oneBox(-1.0, -1.0, 1.0, 1.0);
  TAngleRangeSet ranges(true);
  if (!truncateEllipse(ranges, matrixInv*screenMatrixInv, oneBox))
      return;

  int segments = calcSegmentsCount(matrix, pixelSize);
  double da = M_2PI/segments;
  double s = sin(da);
  double c = cos(da);

  for(TAngleRangeSet::Iterator i(ranges); i; ++i) {
    double a0 = i.d0();
    double a1 = i.d1greater();
    TPointD r(cos(a0), sin(a0));
    TPointD p0 = matrix*r;
    int cnt = (int)floor((a1 - a0)/da);
    for(int j = 0; j < cnt; ++j) {
      r = TPointD(r.x*c - r.y*s, r.y*c + r.x*s);
      TPointD p1 = matrix*r;
      drawSegment(p0, p1, pixelSize, active, enabled);
      p0 = p1;
    }
    drawSegment(p0, matrix*TPointD(cos(a1), sin(a1)), pixelSize, active, enabled);
  }
}

