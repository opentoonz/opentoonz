#pragma once

#ifndef T_SEGMENTADJUSTER_H
#define T_SEGMENTADJUSTER_H

#include "tgeometry.h"
#include "tstroke.h"

#undef DVAPI
#undef DVVAR
#ifdef TVECTORIMAGE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

/*!
  The TSegmentAdjuster minimizes the distance between

     stroke0->getPoint(w0) and stroke1->getPoint(w1)

  changing the values of w0 and w1 (in a defined range).

  Usage:

  TSegmentAdjuster::End p0(stroke0, w0, 0, 1);
  TSegmentAdjuster::End p1(stroke1, w1, 0, 1);
  TSegmentAdjuster adjuster;
  adjuster.compute(p0,p1);

  // the adjusted segment is p0.getPoint(), p1.getPoint()

  if you want to adjust the segment only in the w-growing-direction:

  TSegmentAdjuster::End p0(stroke0, w0, w0, 1);
  TSegmentAdjuster::End p1(stroke1, w1, w1, 1);
  TSegmentAdjuster adjuster;
  adjuster.compute(p0,p1);


 */

class DVAPI TSegmentAdjuster {
public:
  class End {
  public:
    TStroke *m_stroke;
    double m_w;
    double m_wmin, m_wmax;
    End() : m_stroke(0), m_w(0), m_wmin(0), m_wmax(1) {}
    End(TStroke *stroke, double w)
        : m_stroke(stroke), m_w(w), m_wmin(0), m_wmax(1) {}
    End(TStroke *stroke, double w, double wmin, double wmax)
        : m_stroke(stroke), m_w(w), m_wmin(wmin), m_wmax(wmax) {}
    TPointD getPoint() const { return m_stroke->getPoint(m_w); }
    TPointD getSpeed() const { return m_stroke->getSpeed(m_w); }
  };

  TSegmentAdjuster();

  // debug
  void enableTrace(bool enabled) { m_traceEnabled = enabled; }
  void draw();
  void clear() { m_links.clear(); }

  void compute(End &a, End &b);

private:
  End m_a, m_b, m_c, m_d;
  std::vector<std::pair<TPointD, TPointD>> m_links;
  bool m_traceEnabled;

  inline double fun(double u, double v) const {
    return norm2(m_a.m_stroke->getPoint(u) - m_b.m_stroke->getPoint(v));
  }

  inline void gradient(double &dfdu, double &dfdv, double u, double v) {
    const double h = 0.0001;
    dfdu           = (fun(u + h, v) - fun(u - h, v)) / (2 * h);
    dfdv           = (fun(u, v + h) - fun(u, v - h)) / (2 * h);
  }

  inline static double dsda(TStroke *stroke, double w, double dwda) {
    const double h = 0.0001;
    return norm(stroke->getPoint(w + dwda * h) -
                stroke->getPoint(w - dwda * h)) /
           (2 * h);
  }
};
//---------------------------------------------------------------------------------------

#endif  // T_SEGMENTADJUSTER_H
