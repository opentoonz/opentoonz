#pragma once

#ifndef T_INTERVAL_INCLUDED
#define T_INTERVAL_INCLUDED

#include <assert.h>
#include <limits.h>
#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//  class TInterval implementa "Interval Arithmetic" (non vengono computati gli
//  errori macchina):
//  TInterval(min, max) (min <= max) rapresenta l'intervallo reale chiuso [min,
//  max];
//  per m_min == m_max si ottiene l'algebra dei reali;
//  TInterval (m_min = 1, m_max = -1) rappresenta l'intervallo vuoto

class TInterval {
  double m_min, m_max;

public:
  //  costruisce l'intervallo vuoto
  //  TInterval (m_min = 1, m_max = -1) rappresenta l'intervallo vuoto
  TInterval() : m_min(1), m_max(-1) {}
  //-----------------------------------------------------
  //  costruisce gli intervalli degeneri [x, x] che rappresentano i reali
  TInterval(double x) : m_min(x), m_max(x) {}
  //-----------------------------------------------------
  TInterval(double min, double max) {
    assert(min <= max);  //  non vuoto
    m_min = min;
    m_max = max;
  }
  //-----------------------------------------------------
  TInterval(const TInterval &w) {
    assert(w.m_min <= w.m_max ||
           (w.m_min == 1 && w.m_max == -1));  //  intervallo vuoto
    m_min = w.m_min;
    m_max = w.m_max;
  }
  //-----------------------------------------------------
  inline TInterval &operator=(const TInterval &w) {
    assert(w.m_min <= w.m_max ||
           (w.m_min == 1 && w.m_max == -1));  //  intervallo vuoto
    m_min = w.m_min;
    m_max = w.m_max;
    return *this;
  }
  //-----------------------------------------------------
  inline TInterval operator+() const {
    assert(m_min <= m_max);  //  non vuoto
    return *this;
  }
  //-----------------------------------------------------
  inline TInterval operator-() const {
    assert(m_min <= m_max);  //  non vuoto
    return TInterval(-m_max, -m_min);
  }
  //-----------------------------------------------------
  inline TInterval operator+(const TInterval &w) const {
    assert(m_min <= m_max);      //  non vuoto
    assert(w.m_min <= w.m_max);  //  non vuoto
    return TInterval(m_min + w.m_min, m_max + w.m_max);
  }
  //-----------------------------------------------------
  inline TInterval operator-(const TInterval &w) const {
    assert(m_min <= m_max);      //  non vuoto
    assert(w.m_min <= w.m_max);  //  non vuoto
    return TInterval(m_min - w.m_max, m_max - w.m_min);
  }
  //-----------------------------------------------------
  inline TInterval operator*(const TInterval &w) const {
    assert(m_min <= m_max);      //  non vuoto
    assert(w.m_min <= w.m_max);  //  non vuoto
    double value[4];
    value[0]        = m_min * w.m_min;
    value[1]        = m_min * w.m_max;
    value[2]        = m_max * w.m_min;
    value[3]        = m_max * w.m_max;
    double minValue = value[0];
    double maxValue = value[0];
    int i           = 1;
    for (i = 1; i <= 3; ++i) {
      if (value[i] < minValue) minValue = value[i];
      if (value[i] > maxValue) maxValue = value[i];
    }
    return TInterval(minValue, maxValue);
  }
  //-----------------------------------------------------
  inline TInterval operator/(const TInterval &w) const {
    assert(m_min <= m_max);      //  non vuoto
    assert(w.m_min <= w.m_max);  //  non vuoto
    assert((0 < w.m_min && 0 < w.m_max) ||
           (0 > w.m_min && 0 > w.m_max));  //  divisore non nullo
    double value[4];
    value[0]        = m_min / w.m_min;
    value[1]        = m_min / w.m_max;
    value[2]        = m_max / w.m_min;
    value[3]        = m_max / w.m_max;
    double minValue = value[0];
    double maxValue = value[0];
    int i           = 1;
    for (i = 1; i <= 3; ++i) {
      if (value[i] < minValue) minValue = value[i];
      if (value[i] > maxValue) maxValue = value[i];
    }
    return TInterval(minValue, maxValue);
  }
  //-----------------------------------------------------
  inline bool operator==(const TInterval &w) const {
    assert(w.m_min <= w.m_max ||
           (w.m_min == 1 && w.m_max == -1));  //  intervallo vuoto
    return (m_min == w.m_min && m_max == w.m_max);
  }
  //-----------------------------------------------------
  /*  la definizione e' discutibile...
inline bool operator!=(const TInterval &w) const
{return (m_min != w.m_min  ||  m_max != w.m_max);}
*/
  //-----------------------------------------------------
  inline bool operator>(const TInterval &w) const {
    assert(m_min <= m_max);      //  non vuoto
    assert(w.m_min <= w.m_max);  //  non vuoto
    return (m_min > w.m_max);
  }
  //-----------------------------------------------------
  inline bool operator<(const TInterval &w) const {
    assert(m_min <= m_max);      //  non vuoto
    assert(w.m_min <= w.m_max);  //  non vuoto
    return (m_max < w.m_min);
  }
  //-----------------------------------------------------
  /*  la definizione e' discutibile...
//  A >= B
inline bool operator>=(const TInterval &w) const {
assert (m_min <= m_max);  //  non vuoto
assert (w.m_min <= w.m_max);  //  non vuoto
return (m_min >= w.m_min && m_max >= w.m_max);}
//-----------------------------------------------------
//  A <= B
inline bool operator<=(const TInterval &w) const {
assert (m_min <= m_max);  //  non vuoto
assert (w.m_min <= w.m_max);  //  non vuoto
return (m_min <= w.m_min  && m_max <= w.m_max);}
*/
  //-----------------------------------------------------
  inline void setMin(double min) {
    assert(m_min <= m_max);  //  non vuoto
    assert(min <= m_max);    //  non vuoto dopo setMin
    m_min = min;
  }
  //-----------------------------------------------------
  inline void setMax(double max) {
    assert(m_min <= m_max);  //  non vuoto
    assert(m_min <= max);    //  non vuoto dopo setMax
    m_max = max;
  }
  //-----------------------------------------------------
  inline double getMin() const {
    //  isEmpty() => return 1
    return m_min;
  }
  //-----------------------------------------------------
  inline double getMax() const {
    //  isEmpty() => return -1
    return m_max;
  }
  //-----------------------------------------------------
  inline double getLength() const {
    return m_max - m_min;
  }  //  isEmpty() => (getLength() < 0)
  //-----------------------------------------------------
  inline double getCenter() const {
    assert(m_min <= m_max);  //  non vuoto
    return (m_max + m_min) / 2;
  }
  //-----------------------------------------------------
  inline double getRadius() const {
    assert(m_min <= m_max);  //  non vuoto
    return (m_max - m_min) / 2;
  }
  //-----------------------------------------------------
  inline bool isEmpty() const {
    //  TInterval (m_min = 1, m_max = -1) rappresenta l'intervallo vuoto
    assert(m_min <= m_max || (m_min == 1 && m_max == -1));  //  intervallo vuoto
    return (m_min > m_max);
  }

  //-----------------------------------------------------
  inline bool isProper() const {
    //  isProper() <=> !isEmpty() && m_min < m_max (cioe' non degenere)
    //  TInterval (m_min = 1, m_max = -1) rappresenta l'intervallo vuoto
    assert(m_min <= m_max || (m_min == 1 && m_max == -1));  //  intervallo vuoto
    return (m_min < m_max);
  }
  //-----------------------------------------------------
  inline bool contain(double t) const {
    assert(m_min <= m_max || (m_min == 1 && m_max == -1));  //  intervallo vuoto
    return (m_min <= t && t <= m_max);
  }  //  isEmpty() => return false
  //-----------------------------------------------------
  inline bool include(const TInterval &interval) const {
    if (interval.isEmpty())
      return true;
    else if (isEmpty())
      return false;
    else {
      assert(!interval.isEmpty() && !isEmpty());
      return (m_min <= interval.m_min && interval.m_max <= m_max);
    }
  }
  //-----------------------------------------------------
  inline bool isIncluded(const TInterval &interval) const {
    if (isEmpty())
      return true;
    else if (interval.isEmpty())
      return false;
    else {
      assert(!isEmpty() && !interval.isEmpty());
      return (interval.m_min <= m_min && m_max <= interval.m_max);
    }
  }
  //-----------------------------------------------------
  // Friend helper function declarations
  friend TInterval operator*(const double s, const TInterval &w);
  friend TInterval intersection(const TInterval &a, const TInterval &b);
  friend TInterval square(const TInterval &w);
  friend TInterval sqrt(const TInterval &w);
};
//---------------------------------------------------------------------------
// friend functions
inline TInterval operator*(const double s, const TInterval &w) {
  assert(w.m_min <= w.m_max);  //  non vuoto
  if (s >= 0)
    return TInterval(s * w.m_min, s * w.m_max);
  else
    return TInterval(s * w.m_max, s * w.m_min);
}
//---------------------------------------------------------------------------
inline TInterval square(const TInterval &w) {
  //  return w^2
  assert(w.m_min <= w.m_max);  //  non vuoto
  double a = w.m_min * w.m_min;
  double b = w.m_max * w.m_max;
  if (0 <= w.m_min)
    return TInterval(a, b);  //  return [m_min^2, m_max^2]
  else if (w.m_max <= 0)
    return TInterval(b, a);  //  return [m_max^2, m_min^2]
  else {
    assert(w.m_min < 0 && 0 < w.m_max);
    return TInterval(0, std::max(a, b));  //  [0, max(w.m_min^2, w.m_max^2)]
  }
}
//-----------------------------------------------------
inline TInterval sqrt(const TInterval &w) {
  //  return sqrt(w)
  assert(w.m_min <= w.m_max);  //  non vuoto
  assert(0 <= w.m_min);
  return TInterval(sqrt(w.m_min), sqrt(w.m_max));
}  //  [sqrt(w.m_min), sqrt(w.m_max)]
//-----------------------------------------------------
//  helper function
inline TInterval intersection(const TInterval &a, const TInterval &b) {
  //  return a_intersezione_b (insiemistico) se questa e' non vuota, altrimenti
  //  return TInterval() (intervallo vuoto)
  double min = std::max(a.m_min, b.m_min);
  double max = std::min(a.m_max, b.m_max);
  if (min <= max)  //  a.isEmpty() || b.isEmpty() => (min >= 1 && max <= -1) =>
                   //  min > max
    return TInterval(min, max);
  else
    return TInterval();  //  intervallo vuoto
}
//-----------------------------------------------------
inline TInterval createTInterval(double center, double radius) {
  if (radius >= 0)
    return TInterval(center - radius, center + radius);
  else
    return TInterval();  //  intervallo vuoto
}
//-----------------------------------------------------
inline TInterval createErrorTInterval(
    double center, double minError = (std::numeric_limits<double>::min)()) {
  //  type double standard IEEE (1 bit segno + 11 bit esponente + 52 bit
  //  mantissa)
  //  corrisponde ad almeno 15 decimali significativi.
  //  minError serve ad assegnare un errore positivo quando center = 0.
  //  Aumentare 100 volte (da 1e-15 a 1e-13) l'errore relativo serve a
  //  compensare
  //  la propagazione degli errori macchina (TInterval non li computa) ed a
  //  stabilizzare il codice relativamente a processori che potrebbero male
  //  implementare l'artimetica double standard IEEE.
  assert(minError >= 0);
  const double relativeDoubleError = 1e-13;
  double error = std::max(fabs(center) * relativeDoubleError, minError);
  return TInterval(center - error, center + error);
}
//-----------------------------------------------------

#endif  //  __T_INTERVAL_INCLUDED__
