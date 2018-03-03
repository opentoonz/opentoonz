

#include "colorfxutils.h"
#include "drawutil.h"
#include "tregion.h"
#include "tflash.h"

RubberDeform::RubberDeform() : m_pPolyOri(0), m_polyLoc() {}

RubberDeform::RubberDeform(std::vector<T3DPointD> *pPolyOri, const double rf)
    : m_pPolyOri(pPolyOri), m_polyLoc() {
  copyOri2Loc();
  TRectD bbox;
  getBBox(bbox);
  double d = tdistance(TPointD(bbox.x0, bbox.y0), TPointD(bbox.x1, bbox.y1));
  d        = d / 20;
  refinePoly(d);
}

RubberDeform::~RubberDeform() {}

void RubberDeform::deformStep() {
  std::vector<T3DPointD> tmpv;
  std::vector<T3DPointD>::iterator itb = m_polyLoc.begin();
  std::vector<T3DPointD>::iterator ite = m_polyLoc.end();
  for (std::vector<T3DPointD>::iterator it = itb; it != ite; ++it) {
    std::vector<T3DPointD>::iterator it1 = it == (ite - 1) ? itb : it + 1;
    double q                             = 0.5;
    double qq                            = 1.0 - q;
    tmpv.push_back(T3DPointD(qq * it->x + q * it1->x, qq * it->y + q * it1->y,
                             qq * it->z + q * it1->z));
  }
  m_polyLoc = tmpv;
}

void RubberDeform::deform(const double n) {
  if (n <= 0 || n >= 100) return;
  double q = (double)n / 100.0;
  TRectD bbox;
  getBBox(bbox);
  double d0 = ((bbox.y1 - bbox.y0) * 0.5 + (bbox.x1 - bbox.x0) * 0.5) * 0.5;
  double d  = d0;
  while ((d / d0) > q) {
    deformStep();
    getBBox(bbox);
    d = ((bbox.y1 - bbox.y0) * 0.5 + (bbox.x1 - bbox.x0) * 0.5) * 0.5;
  }
  copyLoc2Ori();
}

double RubberDeform::avgLength() {
  if (m_polyLoc.size() <= 0) return 0.0;

  double avgD                          = 0.0;
  std::vector<T3DPointD>::iterator itb = m_polyLoc.begin();
  std::vector<T3DPointD>::iterator ite = m_polyLoc.end();
  for (std::vector<T3DPointD>::iterator it = itb; it != ite; ++it) {
    std::vector<T3DPointD>::iterator it1 = it == (ite - 1) ? itb : it + 1;
    avgD += tdistance(*it, *it1);
  }
  return avgD / (double)m_polyLoc.size();
}

void RubberDeform::getBBox(TRectD &bbox) {
  if (m_polyLoc.size() <= 0) {
    bbox.x0 = bbox.y0 = 0;
    bbox.x1 = bbox.y1 = -1;
    return;
  }
  bbox.x0 = bbox.x1 = m_polyLoc[0].x;
  bbox.y0 = bbox.y1 = m_polyLoc[0].y;
  for (int i = 1; i < (int)m_polyLoc.size(); i++) {
    bbox.x0 = std::min(bbox.x0, m_polyLoc[i].x);
    bbox.x1 = std::max(bbox.x1, m_polyLoc[i].x);
    bbox.y0 = std::min(bbox.y0, m_polyLoc[i].y);
    bbox.y1 = std::max(bbox.y1, m_polyLoc[i].y);
  }
}

void RubberDeform::refinePoly(const double rf) {
  double refineL = rf <= 0.0 ? avgLength() : rf;
  std::vector<T3DPointD> tmpv;
  int nb = m_polyLoc.size();
  for (int j = 0; j < nb; j++) {
    T3DPointD a(m_polyLoc[j]);
    T3DPointD b(j == (nb - 1) ? m_polyLoc[0] : m_polyLoc[j + 1]);
    tmpv.push_back(a);
    double d = tdistance(a, b);
    if (d > refineL) {
      int n    = (int)(d / refineL) + 1;
      double q = 1.0 / (double)n;
      for (int i = 1; i < n; i++) {
        double qq  = q * (double)i;
        double qq1 = 1.0 - qq;
        T3DPointD p(T3DPointD(qq1 * a.x + qq * b.x, qq1 * a.y + qq * b.y,
                              qq1 * a.z + qq * b.z));
        tmpv.push_back(p);
      }
    }
  }
  m_polyLoc = tmpv;
}

// ------------------- SFlashUtils -------------------------------------------

void SFlashUtils::computeOutline(const TRegion *region,
                                 TRegionOutline::PointVector &polyline) const {
  if (!region) return;

  const double pixelSize = 1.0;
  polyline.clear();

  std::vector<TPointD> polyline2d;

  int edgeSize = region->getEdgeCount();

  for (int i = 0; i < edgeSize; i++) {
    TEdge &edge = *region->getEdge(i);
    stroke2polyline(polyline2d, *edge.m_s, pixelSize, edge.m_w0, edge.m_w1);
  }
  int pointNumber = polyline2d.size();
  polyline.reserve(pointNumber);
  for (int j = 0; j < pointNumber; j++) {
    polyline.push_back(T3DPointD(polyline2d[j], 0.0));
  }
}

void SFlashUtils::computeRegionOutline() {
  if (!m_r) return;

  int subRegionNumber = m_r->getSubregionCount();
  TRegionOutline::PointVector app;

  m_ro.m_exterior.clear();
  computeOutline(m_r, app);

  m_ro.m_exterior.push_back(app);
  m_ro.m_interior.clear();
  m_ro.m_interior.reserve(subRegionNumber);
  for (int i = 0; i < subRegionNumber; i++) {
    app.clear();
    computeOutline(m_r->getSubregion(i), app);
    m_ro.m_interior.push_back(app);
  }

  m_ro.m_bbox = m_r->getBBox();
}

void SFlashUtils::PointVector2QuadsArray(const std::vector<T3DPointD> &pv,
                                         std::vector<TQuadratic *> &quadArray,
                                         std::vector<TQuadratic *> &toBeDeleted,
                                         const bool isRounded) const {
  int nbPv                                    = pv.size();
  quadArray.clear();

  if (isRounded) {
    if (nbPv <= 2) {
      if (nbPv == 1) {
        TPointD p0, p1, p2;
        p0 = TPointD(pv[0].x, pv[0].y);
        p1 = TPointD(pv[0].x, pv[0].y);
        p2 = TPointD(pv[0].x, pv[0].y);
        quadArray.push_back(new TQuadratic(p0, p1, p2));
        toBeDeleted.push_back(quadArray.back());
      }
      if (nbPv == 2) {
        TPointD p0, p1, p2;
        p0 = TPointD(pv[0].x, pv[0].y);
        p1 = TPointD((pv[0].x + pv[1].x) / 2, (pv[0].y + pv[1].y) / 2);
        p2 = TPointD(pv[1].x, pv[1].y);
        quadArray.push_back(new TQuadratic(p0, p1, p2));
        toBeDeleted.push_back(quadArray.back());
      }
      return;
    }

    for (int i = 0; i < (nbPv - 2); i++) {
      TPointD p0, p1, p2;
      p0 = TPointD((pv[i].x + pv[i + 1].x) / 2, (pv[i].y + pv[i + 1].y) / 2);
      p1 = TPointD(pv[i + 1].x, pv[i + 1].y);
      p2 = TPointD((pv[i + 1].x + pv[i + 2].x) / 2,
                   (pv[i + 1].y + pv[i + 2].y) / 2);
      quadArray.push_back(new TQuadratic(p0, p1, p2));
      toBeDeleted.push_back(quadArray.back());
    }
    TPointD p0, p1, p2;
    p0 = TPointD((pv[nbPv - 2].x + pv[nbPv - 1].x) / 2,
                 (pv[nbPv - 2].y + pv[nbPv - 1].y) / 2);
    p1 = TPointD(pv[nbPv - 1].x, pv[nbPv - 1].y);
    p2 = TPointD((pv[0].x + pv[1].x) / 2, (pv[0].y + pv[1].y) / 2);
    quadArray.push_back(new TQuadratic(p0, p1, p2));
    toBeDeleted.push_back(quadArray.back());
  } else {
    for (int i = 0; i < (nbPv - 1); i++) {
      TPointD p0, p1, p2;
      p0 = TPointD(pv[i].x, pv[i].y);
      p2 = TPointD(pv[i + 1].x, pv[i + 1].y);
      p1 = TPointD((p0.x + p2.x) * 0.5, (p0.y + p2.y) * 0.5);
      quadArray.push_back(new TQuadratic(p0, p1, p2));
      toBeDeleted.push_back(quadArray.back());
    }
    TPointD p0, p1, p2;
    p0 = TPointD(pv[nbPv - 1].x, pv[nbPv - 1].y);
    p2 = TPointD(pv[0].x, pv[0].y);
    p1 = TPointD((p0.x + p2.x) * 0.5, (p0.y + p2.y) * 0.5);
    quadArray.push_back(new TQuadratic(p0, p1, p2));
    toBeDeleted.push_back(quadArray.back());
  }
}

void SFlashUtils::drawRegionOutline(TFlash &flash, const bool isRounded) const {
  if (!m_r) return;

  std::vector<std::vector<TQuadratic *>> quads;
  std::vector<TQuadratic *> toBeDeleted;
  std::vector<TQuadratic *> quadArray;
  PointVector2QuadsArray(*(m_ro.m_exterior.begin()), quadArray, toBeDeleted,
                         isRounded);
  quads.push_back(quadArray);

  TRegionOutline::Boundary::const_iterator iinter     = m_ro.m_interior.begin();
  TRegionOutline::Boundary::const_iterator iinter_end = m_ro.m_interior.end();
  for (; iinter != iinter_end; iinter++) {
    PointVector2QuadsArray(*iinter, quadArray, toBeDeleted, isRounded);
    quads.push_back(quadArray);
  }

  flash.drawPolygon(quads);
  clearPointerContainer(toBeDeleted);
}

int SFlashUtils::nbDiffVerts(const std::vector<TPointD> &pv) const {
  std::vector<TPointD> lpv;
  if (pv.size() == 0) return 0;
  lpv.push_back(pv[0]);
  for (int i = 1; i < (int)pv.size(); i++) {
    bool isDiff = true;
    for (int j = 0; j < (int)lpv.size() && isDiff; j++)
      isDiff = lpv[j] == pv[i] ? false : isDiff;
    if (isDiff) {
      lpv.push_back(pv[i]);
    }
  }
  return lpv.size();
}

void SFlashUtils::Triangle2Quad(std::vector<TPointD> &p) const {
  TPointD e;
  int i, j;
  i = j = -1;
  if (p[0] == p[1]) {
    i = 0;
    j = 1;
    e = p[2] - p[3];
  } else if (p[0] == p[2]) {
  } else if (p[0] == p[3]) {
    i = 0;
    j = 3;
    e = p[2] - p[1];
  } else if (p[1] == p[2]) {
    i = 1;
    j = 2;
    e = p[3] - p[0];
  } else if (p[1] == p[3]) {
  } else if (p[2] == p[3]) {
    i = 2;
    j = 3;
    e = p[0] - p[1];
  }
  e    = normalize(e);
  p[j] = p[i] + e * 0.001;
}

void SFlashUtils::drawGradedPolyline(TFlash &flash, std::vector<TPointD> &pvv,
                                     const TPixel32 &c1,
                                     const TPixel32 &c2) const {
  std::vector<TPointD> pv;
  pv       = pvv;
  int nbDV = nbDiffVerts(pv);
  if (nbDV < 3 || nbDV > 4) return;
  if (nbDV == 3) Triangle2Quad(pv);

  // Direction Of polyline
  TPointD u   = pv[0] - pv[1];
  TPointD up  = (pv[0] + pv[1]) * 0.5;
  u           = normalize(u);
  u           = rotate90(u);
  TPointD up1 = up + u;
  TPointD up2 = up - u;
  double d1   = (tdistance(up1, pv[2]) + tdistance(up1, pv[3])) * 0.5;
  double d2   = (tdistance(up2, pv[2]) + tdistance(up2, pv[3])) * 0.5;

  std::vector<TPointD> lpv;
  if (d1 > d2) {
    lpv = pv;
  } else {
    lpv.push_back(pv[1]);
    lpv.push_back(pv[0]);
    lpv.push_back(pv[3]);
    lpv.push_back(pv[2]);
  }

  // Transformation of gradient square
  const double flashGrad = 16384.0;  // size of gradient square
  flash.setGradientFill(true, c1, c2, 0);
  TPointD p0((lpv[0] + lpv[3]) * 0.5);
  TPointD p1((lpv[1] + lpv[2]) * 0.5);
  double lv = (tdistance(p0, p1));
  double lh = 0.5 * (tdistance(lpv[0], lpv[3]) + tdistance(lpv[1], lpv[2]));
  TPointD center =
      0.25 * lpv[0] + 0.25 * lpv[1] + 0.25 * lpv[2] + 0.25 * lpv[3];
  TPointD e(p0 - p1);

  double angle = rad2degree(atan(e));
  angle        = angle <= 0 ? 270 + angle : angle - 90;
  TRotation rM(angle);
  TTranslation tM(center.x, center.y);
  TScale sM(lh / (flashGrad), lv / (flashGrad));

  flash.setFillStyleMatrix(tM * sM * rM);
  flash.drawPolyline(pv);
}

//------------------------------------------------------------

//------------------------------------------------------------
void SFlashUtils::drawGradedRegion(TFlash &flash, std::vector<TPointD> &pvv,
                                   const TPixel32 &c1, const TPixel32 &c2,
                                   const TRegion &r) const {
  std::vector<TPointD> pv;
  pv       = pvv;
  int nbDV = nbDiffVerts(pv);
  if (nbDV < 3 || nbDV > 4) return;
  if (nbDV == 3) Triangle2Quad(pv);

  // Direction Of polyline
  TPointD u   = pv[0] - pv[1];
  TPointD up  = (pv[0] + pv[1]) * 0.5;
  u           = normalize(u);
  u           = rotate90(u);
  TPointD up1 = up + u;
  TPointD up2 = up - u;
  double d1   = (tdistance(up1, pv[2]) + tdistance(up1, pv[3])) * 0.5;
  double d2   = (tdistance(up2, pv[2]) + tdistance(up2, pv[3])) * 0.5;

  std::vector<TPointD> lpv;
  if (d1 > d2) {
    lpv = pv;
  } else {
    lpv.push_back(pv[1]);
    lpv.push_back(pv[0]);
    lpv.push_back(pv[3]);
    lpv.push_back(pv[2]);
  }

  // Transformation of gradient square
  const double flashGrad = 16384.0;  // size of gradient square
  flash.setGradientFill(true, c1, c2, 0);
  TPointD p0((lpv[0] + lpv[3]) * 0.5);
  TPointD p1((lpv[1] + lpv[2]) * 0.5);
  double lv = (tdistance(p0, p1));
  double lh = 0.5 * (tdistance(lpv[0], lpv[3]) + tdistance(lpv[1], lpv[2]));
  TPointD center =
      0.25 * lpv[0] + 0.25 * lpv[1] + 0.25 * lpv[2] + 0.25 * lpv[3];
  TPointD e(p0 - p1);

  double angle = rad2degree(atan(e));
  angle        = angle <= 0 ? 270 + angle : angle - 90;
  TRotation rM(angle);
  TTranslation tM(center.x, center.y);
  TScale sM(lh / (flashGrad), lv / (flashGrad));

  flash.setFillStyleMatrix(tM * sM * rM);
  flash.drawRegion(r);
}
