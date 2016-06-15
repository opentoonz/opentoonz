

#include "tgraphics.h"

extern "C" {
#include "Tw/Gf.h"
#include "tvis.h"
}

TGraphics::TGraphics(_TWIDGET *_gf, int ras_x0, int ras_y0, int ras_x1,
                     int ras_y1, int gf_x0, int gf_y0, int gf_x1, int gf_y1,
                     int zoom_level)
    : gf(_gf)
    , currentPoint(0, 0)
    , gfRegion(gf_x0, gf_y0, gf_x1, gf_y1)
    , rasterRegion(ras_x0 - 1, ras_y0 - 1, ras_x1 + 1, ras_y1 + 1)
    //, rasterRegion(ras_x0,ras_y0,ras_x1,ras_y1)
    , zoomFactor(1)
    , pixelSize(1) {
  double dx, dy;
  int blx, bly;

  if (zoom_level > 0)
    zoomFactor = 1 << zoom_level;
  else if (zoom_level < 0)
    zoomFactor = 1.0 / (1 << -zoom_level);
  pixelSize    = 1.0 / zoomFactor;

  blx = ras_x0 - gf_x0 / zoomFactor;
  bly = ras_y0 - gf_y0 / zoomFactor;
  dx  = 0.5 - blx;
  dy  = 0.5 - bly;

  GfPushMatrix(gf);
  GfTranslate(gf, -0.5, -0.5);
  GfScale(gf, zoomFactor, zoomFactor);
  GfTranslate(gf, dx, dy);
}

//---------------------------------------------------

TGraphics::~TGraphics() { GfPopMatrix(gf); }

//---------------------------------------------------

void TGraphics::setColor(int r, int g, int b) {
  _r = r;
  _g = g;
  _b = b;
  GfCpack(gf, r, g, b);
}

//---------------------------------------------------

void TGraphics::drawLine(const TPointI &a, const TPointI &b) {
  if (a.x < rasterRegion.x0 && b.x < rasterRegion.x0 ||
      a.x > rasterRegion.x1 && b.x > rasterRegion.x1 ||
      a.y < rasterRegion.y0 && b.y < rasterRegion.y0 ||
      a.y > rasterRegion.y1 && b.y > rasterRegion.y1)
    return;
  int v[2];
  GfBgnLine(gf);
  v[0] = a.x;
  v[1] = a.y;
  GfV2i(gf, v);
  v[0] = b.x;
  v[1] = b.y;
  GfV2i(gf, v);
  GfEndLine(gf);
}

//---------------------------------------------------

void TGraphics::drawLine(const TPointD &a, const TPointD &b) {
  if (a.x < rasterRegion.x0 && b.x < rasterRegion.x0 ||
      a.x > rasterRegion.x1 && b.x > rasterRegion.x1 ||
      a.y < rasterRegion.y0 && b.y < rasterRegion.y0 ||
      a.y > rasterRegion.y1 && b.y > rasterRegion.y1)
    return;
  double v[2];
  GfBgnLine(gf);
  v[0] = a.x;
  v[1] = a.y;
  GfV2d(gf, v);
  v[0] = b.x;
  v[1] = b.y;
  GfV2d(gf, v);
  GfEndLine(gf);
}

//---------------------------------------------------

void TGraphics::beginPolygon() { GfBgnPolygon(gf); }

//---------------------------------------------------

void TGraphics::endPolygon() { GfEndPolygon(gf); }

//---------------------------------------------------

void TGraphics::beginLine() { GfBgnLine(gf); }

//---------------------------------------------------

void TGraphics::endLine() { GfEndLine(gf); }

//---------------------------------------------------

void TGraphics::vertex(const TPointI &a) {
  int v[2];
  v[0] = a.x;
  v[1] = a.y;
  GfV2i(gf, v);
}

//---------------------------------------------------

void TGraphics::vertex(const TPointD &a) {
  double v[2];
  v[0] = a.x;
  v[1] = a.y;
  GfV2d(gf, v);
}

//---------------------------------------------------

void TGraphics::drawRect(const TPointI &a, const TPointI &b) {
  GfRecti(gf, a.x, a.y, b.x, b.y);
}

//---------------------------------------------------

void TGraphics::drawRect(const TPointD &a, const TPointD &b) {
  GfRect(gf, a.x, a.y, b.x, b.y);
}

//---------------------------------------------------

void TGraphics::drawRect(const TRectI &rect) {
  GfRecti(gf, rect.x0, rect.y0, rect.x1, rect.y1);
}

//---------------------------------------------------

void TGraphics::drawRect(const TRectD &rect) {
  GfRect(gf, rect.x0, rect.y0, rect.x1, rect.y1);
}

//---------------------------------------------------

void TGraphics::drawArc(const TBezierArc &arc) {
  int n = 50;
  beginLine();
  for (int i = 0; i <= n; i++) vertex(arc.getPoint((double)i / (double)n));
  endLine();
}

//---------------------------------------------------

void TGraphics::drawArc(const TCubicCurve &arc) {
  int n = 80;
  beginLine();
  for (int i = 0; i <= n; i++) vertex(arc.getPoint((double)i / (double)n));
  endLine();
}

//---------------------------------------------------

void TGraphics::drawArcTo(const TPointD &d1, const TPointD &d2,
                          const TPointD &d3) {
  TPointD oldPoint = currentPoint;
  currentPoint += d1 + d2 + d3;
  drawArc(
      TBezierArc(oldPoint, oldPoint + d1, oldPoint + d1 + d2, currentPoint));
}

//---------------------------------------------------

void TGraphics::drawDiamond(const TPointD &p, double r) {
  beginPolygon();
  vertex(p + TPointD(r, 0));
  vertex(p + TPointD(0, r));
  vertex(p + TPointD(-r, 0));
  vertex(p + TPointD(0, -r));
  endPolygon();
}

//---------------------------------------------------

void TGraphics::drawCross(const TPointD &p, double r) {
  drawLine(p - TPointD(r, r), p + TPointD(r, r));
  drawLine(p - TPointD(-r, r), p + TPointD(-r, r));
}

//---------------------------------------------------

void TGraphics::drawSquare(const TPointD &p, double r) {
  beginLine();
  vertex(p + TPointD(-r, -r));
  vertex(p + TPointD(-r, r));
  vertex(p + TPointD(r, r));
  vertex(p + TPointD(r, -r));
  vertex(p + TPointD(-r, -r));
  endLine();
}

//---------------------------------------------------

void TGraphics::drawArc(const TPointD &p0, const TPointD &p1,
                        const TPointD &p2) {
  TRectD rect = convert(rasterRegion.enlarge(+10));
  // TRectD rect(rasterRegion.x0, rasterRegion.y0, rasterRegion.x1,
  // rasterRegion.y1);
  TRectD bBox = boundingBox(p0, p1, p2);

  if (!rect.overlaps(bBox)) {
    /*
unsigned char tmp_r = _r;
unsigned char tmp_g = _g;
unsigned char tmp_b = _b;
setColor(100,100,100);
drawRect(bBox);
drawLine(TLineD(bBox.x0, bBox.y0, bBox.x1, bBox.y1));
drawLine(TLineD(bBox.x0, bBox.y1, bBox.x1, bBox.y0));
setColor(tmp_r, tmp_g, tmp_b);
*/
    return;
  }

  double threshold = pixelSize * 0.125;

  TPointD v = p2 - p0;
  TPointD u = p1 - p0;
  TPointD r = rotate90(v);

  double sqr_tsh = (threshold * threshold) * (v * v);
  double dist    = r * u;

  if ((dist * dist) > sqr_tsh) {
    TPointD l1 = 0.5 * (p0 + p1);
    TPointD r1 = 0.5 * (p1 + p2);
    TPointD l2 = 0.5 * (l1 + r1);
    drawArc(p0, l1, l2);
    drawArc(l2, r1, p2);
  } else {
    beginLine();
    vertex(p0);
    vertex(p2);
    endLine();
  }
}

//---------------------------------------------------

void TGraphics::drawCircle(TPointD p, double radius) {
  GfCirc(gf, p.x, p.y, radius);
}

//---------------------------------------------------

void TGraphics::fillCircle(TPointD p, double radius) {
  GfCircf(gf, p.x, p.y, radius);
}

//---------------------------------------------------

void TGraphics::rectWrap(int wrap_pixels) { GfRectWrap(gf, wrap_pixels); }

//---------------------------------------------------

void TGraphics::rectWrite(int x0, int y0, int x1, int y1, void *buffer) {
  GfRectWrite(gf, x0, y0, x1, y1, buffer);
}
