

#include "drawutil.h"
#include "tstroke.h"
#include "tmathutil.h"
//#include "tregion.h"
#include "tcurveutil.h"
#include "tcurves.h"

namespace {
void drawQuadraticCenterline(const TQuadratic &inQuad, double pixelSize,
                             double from, double to) {
  assert(0.0 <= from && from <= to && to <= 1.0);

  to   = (std::max)(0.0, (std::min)(to, 1.0));
  from = (std::max)(0.0, (std::min)(from, to));

  TQuadratic tmp(inQuad), s1, s2;

  TQuadratic *quad = &tmp;

  double newFrom = from;
  if (to != 1.0) {
    tmp.split(to, s1, s2);
    quad    = &s1;
    newFrom = from / to;
  }

  if (from != 0.0) {
    tmp = *quad;
    tmp.split(newFrom, s1, s2);
    quad = &s2;
  }

  // glColor( TPixel32::Black );
  double step = computeStep(*quad, pixelSize);

  // It draws the curve as a linear piecewise approximation

  double invSqrtScale = 1.0;

  // First of all, it computes the control circles of the curve in screen
  // coordinates
  TPointD scP0 = quad->getP0();
  TPointD scP1 = quad->getP1();
  TPointD scP2 = quad->getP2();

  TPointD A = scP0 - 2 * scP1 + scP2;
  TPointD B = scP0 - scP1;

  double h;
  h         = invSqrtScale * step;
  double h2 = h * h;

  TPointD P = scP0, D2 = 2 * h2 * A, D1 = A * h2 - 2 * B * h;

  if (h < 0 || isAlmostZero(h)) return;

  //    if ( h < from )
  //      h = from;

  // It draws the whole curve, using forward differencing
  glBegin(GL_LINE_STRIP);  // The curve starts from scP0

  // scP0 = quad.getPoint(from);
  glVertex2d(scP0.x, scP0.y);

  for (double t = from + h; t < to; t = t + h) {
    P  = P + D1;
    D1 = D1 + D2;
    glVertex2d(P.x, P.y);
  }

  // scP2 = quad.getPoint(to);
  glVertex2d(scP2.x, scP2.y);  // The curve ends in scP2
  glEnd();
}
}

//-----------------------------------------------------------------------------

void stroke2polyline(std::vector<TPointD> &pnts, const TStroke &stroke,
                     double pixelSize, double w0, double w1,
                     bool lastRepeatable) {
  TPointD p;
  double step;
  int i, index0, index1;
  double t0, t1;

  if (isAlmostZero(w0)) w0     = 0.0;
  if (isAlmostZero(w1)) w1     = 0.0;
  if (isAlmostZero(1 - w0)) w0 = 1.0;
  if (isAlmostZero(1 - w1)) w1 = 1.0;

  assert(w0 >= 0.0);
  assert(w0 <= 1.0);
  assert(w1 >= 0.0);
  assert(w1 <= 1.0);

  stroke.getChunkAndT(w0, index0, t0);
  stroke.getChunkAndT(w1, index1, t1);

  double t;
  double endT;

  if (index1 < index0 || (index1 == index0 && t1 < t0)) {
    for (i = index0; i >= index1; i--) {
      step = computeStep(*(stroke.getChunk(i)), pixelSize);

      if (step < TConsts::epsilon) step = TConsts::epsilon;

      p = stroke.getChunk(i)->getPoint(t0);

      if (pnts.empty() || pnts.back() != p) pnts.push_back(p);

      endT = (i == index1) ? t1 : 0;

      pnts.reserve((UINT)((t0 - endT) / step) + 1 + pnts.size());

      for (t = t0 - step; t >= endT; t -= step)
        pnts.push_back(stroke.getChunk(i)->getPoint(t));

      t0 = 1;
    }

  } else {
    for (i = index0; i <= index1; i++) {
      step = computeStep(*(stroke.getChunk(i)), pixelSize);

      assert(step);
      if (!step) step = TConsts::epsilon;  // non dovrebbe accadere mai!!!

      p = stroke.getChunk(i)->getPoint(t0);

      if (pnts.empty() || pnts.back() != p) pnts.push_back(p);

      endT = (i == index1) ? t1 : 1;

      pnts.reserve((UINT)((endT - t0) / step) + 1 + pnts.size());

      for (t = t0 + step; t <= endT; t += step)
        pnts.push_back(stroke.getChunk(i)->getPoint(t));

      t0 = 0;
    }
  }

  p = stroke.getPoint(w1);

  if (pnts.empty() ||
      (p != pnts.back() && (p != pnts.front() || lastRepeatable)))
    pnts.push_back(p);
}

//-----------------------------------------------------------------------------

/*
void  region2polyline(vector<T3DPointD>& pnts,
                      const TRegion* reg,
                      double pixelSize )
{
assert( reg );
if(!reg) return;

const TStroke* stroke;
double w0;
double w1;
TPointD lastPnt;
for(UINT i=0; i<reg->getEdgeCount(); i++)
  {
  TRegion::edge* edge = reg->getEdge(i);
  stroke = edge->m_stroke;
  assert(stroke);

  if (edge->m_w0==-1)
    {
    int index;
    double t, dummy;
    stroke->getNearestChunk(edge->m_p0, t, index, dummy);
    edge->m_w0 = getWfromChunkAndT(stroke, index, t);

    stroke->getNearestChunk(edge->m_p1, t, index, dummy);
    edge->m_w1 = getWfromChunkAndT(stroke, index, t);
    }

  w0 = edge->m_w0;
  w1 = edge->m_w1;

  assert( 0 <= w0 && w0 <= 1.0 );
  assert( 0 <= w1 && w1 <= 1.0 );

  double step = computeStep( *stroke, pixelSize );

    // assert( step != 2 && step != 0.0 );

  if( isAlmostZero( step ) )
    step = 1.0;

  step/= stroke->getChunkCount();

  double direction = 1;

  if( w0 > w1 )
    direction *=-1;

  T3DPointD pnt;

  double incr = direction*step;

  pnt = T3DPointD( stroke->getPoint( w0 ), 0 );

  if ( pnts.empty() || pnt != pnts.back() )
    pnts.push_back( pnt );

  for( double w = w0 + incr; direction*w < direction*w1; w+= incr)
    {
    pnt = T3DPointD( stroke->getPoint( w ), 0 );
      if ( pnt != pnts.back() )
        pnts.push_back( pnt );
    }
  }
}
*/

//-----------------------------------------------------------------------------

#if defined(MACOSX)
void lefttRotateBits(UCHAR *buf, int bufferSize) {
  UINT *buffer = (UINT *)buf;
  UINT app;
  for (int i = 0; i < bufferSize; i++, buffer++) {
    app     = *buffer;
    *buffer = app << 8 | app >> 24;
  }
}
#endif

double computeStep(const TStroke &s, double pixelSize) {
  double minVal = (std::numeric_limits<double>::max)();
  double tempVal;
  for (int i = 0; i < s.getChunkCount(); ++i)
    if ((tempVal = computeStep(*s.getChunk(i), pixelSize)) < minVal)
      minVal = tempVal;

  return minVal;
}

/*
*/
TRasterP prepareTexture(const TRasterP &ras, TextureInfoForGL &texinfo) {
  TDimension size = ras->getSize();

  texinfo.width          = size.lx;
  texinfo.height         = size.ly;
  texinfo.internalformat = ras->getPixelSize();
  texinfo.format         = GL_UNSIGNED_BYTE;
  texinfo.pixels         = ras->getRawData();

  switch (texinfo.internalformat) {
  case 1:
    texinfo.type = GL_LUMINANCE;
    break;
  case 2:
    texinfo.type = GL_LUMINANCE_ALPHA;
    break;
  }

  if (texinfo.internalformat > 2) {
    switch (texinfo.internalformat) {
    case 3:
      texinfo.type = GL_RGB;
      break;
    case 4:
      texinfo.type = GL_RGBA;
      break;  // GL_RGBA;  break;
    }

#ifdef TNZ_MACHINE_CHANNEL_ORDER_BGRM  // under win32 pixel are in reverse order

#ifdef GL_EXT_bgra  // if extension exists...

    // and it's  present at run time all okay
    // if(TGLArea::isBGRASupported())
    {
      switch (texinfo.internalformat) {
      case 3:
        texinfo.type = GL_BGR_EXT;
        break;
      case 4:
        texinfo.type = GL_BGRA_EXT;
        break;
      }
      return ras;
    }
#else

    TDimension size = ras->getSize();
    TRasterP outRas = ras->clone();
    outRas->lock();
    int pixelSize  = ras->getPixelSize();
    texinfo.pixels = outRas->getRawData();

    UCHAR *p1, *p2;

    for (int i = 0; i < size.lx; ++i)
      for (int j = 0; j < size.ly; ++j) {
        p1 = outRas->getRawData(i, j);
        p2 = p1 + 2;
        std::swap(*p1, *p2);
      }
    outRas->unlock();
    return outRas;
#endif

#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
// mrgb

#warning "ottimizzare in qualche modo"

    TDimension size = ras->getSize();
    TRasterP outRas = ras->clone();

    texinfo.pixels = outRas->getRawData();

    lefttRotateBits((UCHAR *)texinfo.pixels, size.lx * size.ly);

    return outRas;

#endif
  }

  texinfo.pixels = ras->getRawData();
  return ras;
}

void drawStrokeCenterline(const TStroke &stroke, double pixelSize, double from,
                          double to) {
  int c1 = 0, c2 = 0;
  double t1 = 1.0, t2 = 0.0;

  if (stroke.getChunkCount() == 0) return;

  stroke.getChunkAndT(from, c1, t1);
  stroke.getChunkAndT(to, c2, t2);

  if (c1 == c2) {
    if (from == to) return;

    drawQuadraticCenterline(*stroke.getChunk(c1), pixelSize, t1, t2);
  } else {
    // partial first chunk
    drawQuadraticCenterline(*stroke.getChunk(c1), pixelSize, t1, 1.0);
    // next chunk
    ++c1;
    if (c1 < c2) {
      for (int i = c1; i < c2; ++i)
        drawQuadraticCenterline(*stroke.getChunk(i), pixelSize, 0.0, 1.0);
    }

    // partial last chunk
    drawQuadraticCenterline(*stroke.getChunk(c2), pixelSize, 0.0, t2);
  }
}

//============================================================================

DVAPI TStroke *makeEllipticStroke(double thick, TPointD center, double radiusX,
                                  double radiusY) {
  std::vector<TThickPoint> points(17);

  double xmin = center.x - radiusX;  // x coordinate of the upper left corner of
                                     // the bounding rectangle
  double ymin = center.y - radiusY;  // y coordinate of the upper left corner of
                                     // the bounding rectangle
  double xmax = center.x + radiusX;  // x coordinate of the bottom right corner
                                     // of the bounding rectangle
  double ymax = center.y + radiusY;  // y coordinate of the bottom right corner
                                     // of the bounding rectangle
  const double C1 = 0.1465;          // magic number for coefficient1
  const double C2 = 0.2070;          // magic number for coefficient2
  double dx       = xmax - xmin;     // dx is width diameter
  double dy       = ymax - ymin;     // dy is height diameter
  const double begin =
      0.8535;  // starting position to draw (bounding square is 1x1)

  double c1dx = (double)(C1 * dx);
  double c1dy = (double)(C1 * dy);
  double c2dx = (double)(C2 * dx);
  double c2dy = (double)(C2 * dy);

  points[0] = TThickPoint(xmin + begin * dx, ymin + begin * dy, thick);

  points[1]  = points[0] + TThickPoint(-c1dx, c1dy, 0);   //
  points[2]  = points[1] + TThickPoint(-c2dx, 0, 0);      //
  points[3]  = points[2] + TThickPoint(-c2dx, 0, 0);      //
  points[4]  = points[3] + TThickPoint(-c1dx, -c1dy, 0);  //
  points[5]  = points[4] + TThickPoint(-c1dx, -c1dy, 0);  //
  points[6]  = points[5] + TThickPoint(0, -c2dy, 0);      //
  points[7]  = points[6] + TThickPoint(0, -c2dy, 0);      //
  points[8]  = points[7] + TThickPoint(c1dx, -c1dy, 0);   //
  points[9]  = points[8] + TThickPoint(c1dx, -c1dy, 0);   //
  points[10] = points[9] + TThickPoint(c2dx, 0, 0);       //
  points[11] = points[10] + TThickPoint(c2dx, 0, 0);      //
  points[12] = points[11] + TThickPoint(c1dx, c1dy, 0);   //
  points[13] = points[12] + TThickPoint(c1dx, c1dy, 0);   //
  points[14] = points[13] + TThickPoint(0, c2dy, 0);      //
  points[15] = points[14] + TThickPoint(0, c2dy, 0);      //
  points[16] = points[0];  // need to be closed!!!
                           // points[15]+TThickPoint(-c1dx,  c1dy,0);//

  TStroke *stroke = new TStroke(points);
  stroke->setSelfLoop();
  return stroke;
}
