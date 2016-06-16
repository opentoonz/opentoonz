

#include "tsegmentadjuster.h"
#include "tgl.h"

#include <QDebug>

TSegmentAdjuster::TSegmentAdjuster() : m_traceEnabled(false) {}

void TSegmentAdjuster::compute(End &pa, End &pb) {
  m_a = m_c = pa;
  m_b = m_d = pb;
  m_links.clear();
  if (m_traceEnabled)
    m_links.push_back(std::make_pair(m_a.getPoint(), m_b.getPoint()));

  double u = m_a.m_w, v = m_b.m_w;
  // double ua=w0, va=w1;
  // TPointD c = (stroke0->getPoint(ua)+stroke1->getPoint(va))*0.5;
  int count = 0;
  for (;;) {
    if (++count > 1000) break;
    double du, dv;
    gradient(du, dv, u, v);
    double dd = sqrt(du * du + dv * dv);
    if (dd < 0.01) break;
    du *= 1.0 / dd;
    dv *= 1.0 / dd;

    double a_dsda = dsda(m_a.m_stroke, u, du);
    double b_dsda = dsda(m_b.m_stroke, v, dv);

    const double ds  = 0.5;
    const double eps = 0.0001;
    double a         = 0.01;
    double aa;
    if (a_dsda > eps && (aa = ds / a_dsda, aa < a)) a = aa;
    if (b_dsda > eps && (aa = ds / b_dsda, aa < a)) a = aa;

    u -= a * du;
    v -= a * dv;
    u = tcrop(u, m_a.m_wmin, m_a.m_wmax);
    v = tcrop(v, m_b.m_wmin, m_b.m_wmax);
    if (m_traceEnabled)
      m_links.push_back(
          std::make_pair(m_a.m_stroke->getPoint(u), m_b.m_stroke->getPoint(v)));
  }
  m_c.m_w = u;
  m_d.m_w = v;
  pa.m_w  = u;
  pb.m_w  = v;
}

void TSegmentAdjuster::draw() {
  for (int i = 0; i < (int)m_links.size(); i++) {
    glColor3d(0.9, 0.8, 0.7);
    tglDrawSegment(m_links[i].first, m_links[i].second);
  }
}
