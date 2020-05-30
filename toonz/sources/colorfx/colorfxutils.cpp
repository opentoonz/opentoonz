

#include "colorfxutils.h"
#include "drawutil.h"
#include "tregion.h"

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
