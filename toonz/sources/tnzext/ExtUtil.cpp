

#ifdef _DEBUG
#define _STLP_DEBUG 1
#endif
#include <tcurves.h>
#include <tstroke.h>
#include <tmathutil.h>
#include <tcurveutil.h>
#include <tgl.h>
#include <set>
#include <iterator>
// please do not move, it is necessary to be the last
// to work properely with DVAPI macro
#include "ext/ExtUtil.h"

// just to override assert
#ifdef _DEBUG
#define EXT_NORMALIZE(a) norm2(a) != 0.0 ? normalize(a) : a
#define DEBUG_EXPORT DVAPI
#else
#define EXT_NORMALIZE(a) normalize(a)
#define DEBUG_EXPORT static
#endif

namespace {
//---------------------------------------------------------------------------

inline bool isWGood(double first, double w, double second, const TStroke *s) {
  if (!ToonzExt::isValid(first) || !ToonzExt::isValid(second) ||
      !ToonzExt::isValid(w))
    return false;

  if (s) {
    if (s->isSelfLoop()) {
      if (first > second) {
        if ((first < w && w <= 1.0) || (0.0 <= w && w < second)) return true;
      } else if (first == second) {
        if (areAlmostEqual(w, first)) return true;
      }
    }
  }

  if (first < w && w < second) return true;
  return false;
}

//---------------------------------------------------------------------------

inline int normalizeAngle(int angle) {
  if (angle < 0) angle = -angle;
  return angle %= 181;
}

//---------------------------------------------------------------------------

int getStrokeId(const TStroke *s, const TVectorImageP &vi) {
  if (!ToonzExt::isValid(s) || !vi) return -1;

  int count = vi->getStrokeCount();
  if (!count) return -1;

  while (count--) {
    if (s == vi->getStroke(count)) return count;
  }
  return -1;
}

//---------------------------------------------------------------------------

/**
   * Verify if a curve is a "almost" point.
   */
template <class T>
bool isImproper(const T *tq) {
  TPointD v1 = tq->getP0() - tq->getP1(), v2 = tq->getP2() - tq->getP1();

  if (isAlmostZero(norm2(v1)) && isAlmostZero(norm2(v2))) return true;

  return false;
}

//---------------------------------------------------------------------------

bool areParallel(const TPointD &v1, const TPointD &v2) {
  double res = cross(EXT_NORMALIZE(v1), EXT_NORMALIZE(v2));
  if (isAlmostZero(res)) return true;
  return false;
}

//---------------------------------------------------------------------------

bool areInSameDirection(const TPointD &v1, const TPointD &v2) {
  if (v1 * v2 >= 0) return true;
  return false;
}

//---------------------------------------------------------------------------

/**
   * Verify if a curve is a straight line.
   */
template <class T>
bool curveIsStraight(const T *tq, double &t) {
  t = -1;
  assert(tq);
  if (!tq) return false;

  TPointD v1 = tq->getP1() - tq->getP0();
  TPointD v2 = tq->getP2() - tq->getP1();

  double res = cross(v1, v2);
  if (isAlmostZero(res)) {
    if (v1 * v2 < 0) {
      t = tq->getT(tq->getP1());
    }
    return true;
  }

  return false;
}

//---------------------------------------------------------------------------

/**
   * Verify if junction between two curves, is smooth.
   */
template <class T>
bool corner(const T *q1, const T *q2, double tolerance) {
  if (!q1 || !q2 || !areAlmostEqual(q1->getP2(), q2->getP0())) return false;

  // really near to the extremes
  TPointD
      // pnt = q1->getP2(),
      v1 = q1->getSpeed(1.0),  // q1->getPoint(1.0-TConsts::epsilon) - pnt,
      v2 = q2->getSpeed(0.0);  // q2->getPoint(TConsts::epsilon) - pnt;

  v1 = EXT_NORMALIZE(v1);
  v2 = EXT_NORMALIZE(v2);

  double res = cross(v1, v2);

  if (!isAlmostZero(res, tolerance)) return true;

  return false;
}

//---------------------------------------------------------------------------

int detectStraightIntervals_(const TThickQuadratic *ttq,
                             ToonzExt::Intervals &intervals, double tolerance) {
  if (!ttq) return 0;

  TPointD v0 = ttq->getP1() - ttq->getP0(), v1 = ttq->getP2() - ttq->getP1();

  double v0_norm2 = norm(v0), v1_norm2 = norm(v1);

  if (v0_norm2 != 0.0) v0 = v0 * (1.0 / v0_norm2);
  if (v1_norm2 != 0.0) v1 = v1 * (1.0 / v1_norm2);

  double v0xv1 = v0 * v1;

  if (v0xv1 == 0.0 && v0_norm2 == 0.0 &&
      v1_norm2 == 0.0)  // null value with all zero
    return 0;

  if (isAlmostZero(cross(v0, v1) /*,tolerance*/)) {
    if (v0xv1 >= 0) {
      intervals.push_back(ToonzExt::Interval(0.0, 1.0));
      return 1;
    } else {
      double t = ttq->getT(ttq->getP1());
      intervals.push_back(ToonzExt::Interval(0.0, t));
      intervals.push_back(ToonzExt::Interval(t, 1.0));
      return 2;
    }
  }
#if 0
    // need to be analysed
    else
    {
      double
        pixelSize = 1.0;
#ifndef _DEBUG
      pixelSize = sqrt(tglGetPixelSize2());
#endif
      double
        step = computeStep(*ttq,
        pixelSize);
      
      //assert( step < 1.0 );
      if( step > 1.0 )
        return 0;
      
      double 
        t = 0.0;
      
      TPointD
        pnt ,
        p0 = ttq->getP0(),
        pn;
      
      ToonzExt::Interval
        last_interval(-1,-1),
        curr_interval;
      
      pnt = ttq->getPoint(step);
      v0 = pnt - p0;
      if( t+step < 1.0)
        pn = ttq->getPoint( step+step );
      else
        pn = ttq->getP2();
      v1 = pn - pnt;
      
      v0 = EXT_NORMALIZE(v0);
      v1 = EXT_NORMALIZE(v1);
      if( isAlmostZero( cross(v0,v1), tolerance ) )
      {
        assert( v0*v1 >0);
        last_interval.first = ttq->getT(p0);
        last_interval.second= ttq->getT(pn);
      }
      p0 = pn;
      
      for(t=2.0*step;
      t<1.0;
      t+=2.0*step)
      {
        pnt = ttq->getPoint(t);
        v0 = pnt - p0;
        if( t+step < 1.0)
          pn = ttq->getPoint( t+step );
        else
          pn = ttq->getP2();
        v1 = pn - pnt;
        
        v0 = EXT_NORMALIZE(v0);
        v1 = EXT_NORMALIZE(v1);
        if( isAlmostZero( cross(v0,v1), tolerance ) )
        {
          //assert( v0 * v1 > 0 );
          curr_interval.first  = ttq->getT(p0);
          curr_interval.second = ttq->getT(pn);
          
          if( curr_interval.first != last_interval.second ||
            v0 * v1 < 0)
          {
            intervals.push_back(last_interval);
            last_interval.first =
              last_interval.second = curr_interval.second;
          }
          else
          {
            last_interval.second = curr_interval.second;
          }
        }
        // for quadratic is not possible
        std::swap(p0,pn);
      }
      
      pn=ttq->getP2();
      v1 = pn - pnt;
      if( isAlmostZero( cross(v0,v1), tolerance ) )
      {
        if( v0 * v1 > 0 )
        {
          curr_interval.first  = ttq->getT(p0);
          curr_interval.second = ttq->getT(pn);
          
          if( curr_interval.first == last_interval.second )
            last_interval.second = curr_interval.second;
          
          intervals.push_back(last_interval);
        }
      }
      return intervals.size();
    }
#endif
  return 0;
}

//---------------------------------------------------------------------------

/*
  int
  detectStraightIntervals_(const TThickQuadratic* ttq,
                           std::set<double>& values)
  {
    if(!ttq ||
       isImproper( ttq ))
      return 0;

    TPointD
      v0 = ttq->getP1() - ttq->getP0(),
      v1 = ttq->getP2() - ttq->getP1();

    if( isAlmostZero( cross(v0,v1) ) )
    {
      values.insert(0.0);
      values.insert(1.0);
      if(v0 * v1 > 0 )
      {
        return values.size()-1;
      }
      else
      {
        double
          t = ttq->getT(ttq->getP1());
        values.insert( t );
        return values.size()-1;
      }
    }
    else
    {
      double
        pixelSize = 1.0;
#ifndef  _DEBUG
      pixelSize = sqrt(tglGetPixelSize2());
#endif
      double
        step = computeStep(*ttq,
                           pixelSize);

      double
        t = 0.0;

      TPointD
        pnt ,
        p0 = ttq->getP0(),
        pn;
      for(t=step;
          t<1.0;
          t+=2.0*step)
      {
        pnt = ttq->getPoint(t);
        v0 = pnt - p0;
        if( t+step < 1.0)
          pn = ttq->getPoint( t+step );
        else
          pn = ttq->getP2();
        v1 = pn - pnt;
        if( isAlmostZero( cross(v0,v1) ) )
        {
          assert( v0 * v1 > 0 );
          values.insert(ttq->getT(p0));
          values.insert(ttq->getT(pn));
        }
        // for quadratic is not possible
        std::swap(p0,pn);
      }

      pn=ttq->getP2();
      v1 = pn - pnt;
      if( isAlmostZero( cross(v0,v1) ) )
      {
        assert( v0 * v1 > 0 );
        values.insert(ttq->getT(p0));
        values.insert(ttq->getT(pn));
      }
    }

    return values.size()-1;
  }
  */
//---------------------------------------------------------------------------

bool mapValueInStroke(const TStroke *stroke, const TThickQuadratic *ttq,
                      double value, double &out) {
  assert(ttq);

  if (!ttq || !ToonzExt::isValid(stroke) || !ToonzExt::isValid(value))
    return false;

  if (value == 1.0) {
    if (ttq->getPoint(1.0) == stroke->getPoint(1.0)) {
      if (!stroke->isSelfLoop())
        out = 1.0;
      else
        out = 0.0;
      return true;
    }
  }

  out = stroke->getW(ttq->getPoint(value));

  return true;
}

//---------------------------------------------------------------------------

bool mapIntervalInStroke(const TStroke *stroke, const TThickQuadratic *ttq,
                         const ToonzExt::Interval &ttq_interval,
                         ToonzExt::Interval &stroke_interval) {
  if (!ttq || !ToonzExt::isValid(stroke)) return false;

  if (ttq_interval.first > ttq_interval.second || 0.0 > ttq_interval.first ||
      ttq_interval.second > 1.0)
    return false;

  bool check =
      mapValueInStroke(stroke, ttq, ttq_interval.first, stroke_interval.first);
  assert(check);
  if (!check) return false;
  check = mapValueInStroke(stroke, ttq, ttq_interval.second,
                           stroke_interval.second);

  assert(check);
  if (!check) return false;
  return true;
}

//---------------------------------------------------------------------------

bool addQuadraticIntervalInStroke(const TStroke *stroke,
                                  ToonzExt::Intervals &stroke_intervals,
                                  const TThickQuadratic *ttq,
                                  ToonzExt::Intervals &ttq_intervals) {
  if (!ttq || !ToonzExt::isValid(stroke)) return false;

  const int size = ttq_intervals.size();

  if (size == 0) return false;

  int i = 0;

  for (i = 0; i < size; ++i) {
    const ToonzExt::Interval &tmp = ttq_intervals[i];

    if (tmp.first > tmp.second || 0.0 > tmp.first || tmp.second > 1.0)
      return false;
  }

  for (i = 0; i < size; ++i) {
    const ToonzExt::Interval &ttq_interval = ttq_intervals[i];
    ToonzExt::Interval stroke_interval;

    // start to add interval in stroke
    if (mapIntervalInStroke(stroke, ttq, ttq_interval, stroke_interval)) {
      stroke_intervals.push_back(stroke_interval);
    }
  }
  return true;
}

//---------------------------------------------------------------------------

bool addQuadraticSetInStroke(const TStroke *stroke,
                             ToonzExt::Intervals &stroke_intervals,
                             const TThickQuadratic *ttq,
                             const std::set<double> &ttq_set) {
  if (!ttq || !ToonzExt::isValid(stroke)) return false;

  const int size = ttq_set.size();

  if (size < 1) return false;

  std::set<double>::const_iterator cit, cit_end = ttq_set.end();

  for (cit = ttq_set.begin(); cit != cit_end; ++cit) {
    if (0.0 > *cit || *cit > 1.0) return false;
  }

  cit                                       = ttq_set.begin();
  std::set<double>::const_iterator next_cit = cit;
  std::advance(next_cit, 1);

  for (; next_cit != cit_end; ++next_cit) {
    ToonzExt::Interval stroke_interval;

    bool check = mapValueInStroke(stroke, ttq, *cit, stroke_interval.first);
    assert(check);
    if (!check) return false;

    check = mapValueInStroke(stroke, ttq, *next_cit, stroke_interval.second);

    assert(check);
    if (!check) return false;

    assert(stroke_interval.first <= stroke_interval.second);
    if (stroke_interval.first > stroke_interval.second) return false;
    stroke_intervals.push_back(stroke_interval);
    cit = next_cit;
  }

  return true;
}

//---------------------------------------------------------------------------

bool isThere(double value, const ToonzExt::Intervals &intervals) {
  ToonzExt::Intervals::const_iterator cit     = intervals.begin(),
                                      cit_end = intervals.end();

  while (cit != cit_end) {
    if (cit->first == value || cit->second == value) return true;
    ++cit;
  }

  return false;
}

//---------------------------------------------------------------------------

bool isThere(double value, const std::set<double> &mySet) {
  std::set<double>::const_iterator cit_end = mySet.end();

  if (cit_end == mySet.find(value)) return false;

  return true;
}

//---------------------------------------------------------------------------

bool isCorner(const ToonzExt::Intervals &values, double w, double tolerance) {
  ToonzExt::Interval prev = values[0], curr = prev;

  // first point
  if (areAlmostEqual(prev.first, w, tolerance)) return true;

  const int size = values.size();
  for (int i = 1; i < size; ++i) {
    curr = values[i];

    if (areAlmostEqual(prev.second, curr.first) &&
        areAlmostEqual(w, curr.first, tolerance))
      return true;
    prev = curr;
  }

  // last point
  if (areAlmostEqual(curr.second, w, tolerance)) return true;

  return false;
}

//--------------------------------------------------------------------------

}  // end of unnamed namespace

//---------------------------------------------------------------------------

DEBUG_EXPORT bool isThereACornerMinusThan(double minCos, double minSin,
                                          const TThickQuadratic *quad1,
                                          const TThickQuadratic *quad2) {
  if (!quad1 || !quad2 || !ToonzExt::isValid(fabs(minCos)) ||
      !ToonzExt::isValid(fabs(minSin)))
    return false;

  TPointD
      // speed1,
      // speed2,
      tan1,
      tan2;

  // speed1 = quad1->getSpeed(1);
  // speed2 = -quad2->getSpeed(0);

  tan1 = quad1->getSpeed(1);
  tan2 = -quad2->getSpeed(0);

  if (norm2(tan1) == 0.0 || norm2(tan2) == 0.0) return false;

  tan1 = normalize(tan1);
  tan2 = normalize(tan2);

  // move cos value to compare just positive values
  minCos += 1.0;
  double cosVal = 1.0 + (tan1 * tan2);  //, sinVal = fabs(cross(tan1,tan2));

  assert(minCos >= 0.0);
  if (cosVal >= minCos) return true;

  return false;
}

//-----------------------------------------------------------------------------

DEBUG_EXPORT double degree2cos(int degree) {
  int tmp = degree < 0 ? -degree : degree;
  tmp %= 360;
  degree = degree < 0 ? 360 - tmp : degree;

  if (degree == 0) return 1.0;

  if (degree == 180) return -1.0;

  if (degree == 90 || degree == 270) return 0.0;

  return cos(degree * M_PI_180);
}

//-----------------------------------------------------------------------------

DEBUG_EXPORT double degree2sin(int degree) { return degree2cos(degree - 90); }

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::findNearestSpireCorners(
    const TStroke *stroke, double w, ToonzExt::Interval &out, int cornerSize,
    const ToonzExt::Intervals *const cl, double tolerance) {
  if (!ToonzExt::isValid(stroke) || !ToonzExt::isValid(w)) return false;

  const ToonzExt::Intervals *values = cl;

  ToonzExt::Intervals tmpValues;

  if (!cl) {
    cornerSize = normalizeAngle(cornerSize);
    if (!ToonzExt::detectSpireIntervals(stroke, tmpValues, cornerSize))
      return false;
    values = &tmpValues;
  }

  if (!values || values->empty()) {
    return false;
  }

  return findNearestCorners(stroke, w, out, *values, tolerance);
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::isASpireCorner(const TStroke *s, double w, int cornerSize,
                                    const ToonzExt::Intervals *const cl,
                                    double tolerance) {
  if (!ToonzExt::isValid(s) || !ToonzExt::isValid(w)) return false;

  ToonzExt::Intervals tmpValues;

  const ToonzExt::Intervals *values = cl;

  if (!cl) {
    if (!ToonzExt::detectSpireIntervals(s, tmpValues, cornerSize)) return false;
    values = &tmpValues;
  }

  if (!values || values->empty()) {
    return false;
  }

  return isCorner(*values, w, tolerance);
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::detectStraightIntervals(const TStroke *stroke,
                                             ToonzExt::Intervals &intervals,
                                             double tolerance) {
  if (!ToonzExt::isValid(stroke)) return false;

  assert(tolerance > 0.0 && tolerance < 1.0 &&
         "Strange tolerance are you sure???");

  intervals.clear();

  // first step:
  //  store information about chunk and straight interval
  typedef std::pair<const TThickQuadratic *, ToonzExt::Intervals>
      ChunkStraights;

  std::map<int, ChunkStraights> arrayOfChunkIntervals;

  int chunkCount = stroke->getChunkCount();
  int counter    = 0;

  for (int i = 0; i < chunkCount; ++i) {
    ToonzExt::Intervals values;

    const TThickQuadratic *chunk = stroke->getChunk(i);
    if (chunk->getLength() == 0.0) continue;
    int howMany = detectStraightIntervals_(chunk, values, tolerance);
    if (howMany > 0) {
      arrayOfChunkIntervals[counter] = ChunkStraights(chunk, values);
    }
    ++counter;
  }

  // second step:
  //  add intervals vs stroke
  std::map<int, ChunkStraights>::iterator it = arrayOfChunkIntervals.begin(),
                                          end = arrayOfChunkIntervals.end(),
                                          aux;

  ToonzExt::Interval myRange = ToonzExt::Interval(-1, -1);

  for (; it != end; ++it) {
    aux = it;
    std::advance(aux, 1);
    ChunkStraights &cs1 = it->second;
    if (aux != end) {
      ChunkStraights &cs2 = aux->second;
      const int i1 = it->first, i2 = aux->first;

      // if chunk are a sequence
      if ((i1 + 1 == i2) && !corner(cs1.first, cs2.first, tolerance)) {
        const ToonzExt::Intervals &cs1Intervals = cs1.second;

        const int size = cs1Intervals.size();
        int i;
        ToonzExt::Interval tmp;

        for (i = 0; i < size; ++i) {
          tmp = cs1Intervals[i];

          if (tmp.second == 1.0) break;

          ToonzExt::Interval stroke_interval;
          if (mapIntervalInStroke(stroke, cs1.first, tmp, stroke_interval))
            intervals.push_back(stroke_interval);
        }
        // assert( i == size-1 );

        if (myRange.first == -1) {
          if (!mapValueInStroke(stroke, cs1.first, tmp.first, myRange.first))
            assert(!"Ops problemone!!!");
          // remove value added to merge
          //  adjacent straight interval
          cs1.second.pop_back();
        }

        tmp = cs2.second.front();

        if (!mapValueInStroke(stroke, cs2.first, tmp.second, myRange.second))
          assert(!"Ops problemone!!!");
        cs2.second.erase(cs2.second.begin());
      } else {
        if (myRange.first != -1 && myRange.second != -1) {
          intervals.push_back(myRange);
          myRange = ToonzExt::Interval(-1, -1);
        }

        // add remaining interval of current stroke
        addQuadraticIntervalInStroke(stroke, intervals, cs1.first, cs1.second);
      }
    } else {
      if (myRange.first != -1 && myRange.second != -1) {
        intervals.push_back(myRange);
        myRange = ToonzExt::Interval(-1, -1);
      }

      // add remaining interval of current stroke
      addQuadraticIntervalInStroke(stroke, intervals, cs1.first, cs1.second);
    }
  }

  if (stroke->isSelfLoop()) {
    TPointD v0 = stroke->getSpeed(0.0), vn = stroke->getSpeed(1.0);

    if (areParallel(v0, vn) && areInSameDirection(v0, vn) &&
        intervals.size() > 1) {
      // then first interval probably can be merged
      ToonzExt::Interval first = intervals.front(), last = intervals.back();
      if (first.first == 0.0 && last.second == 0.0) {
        intervals.pop_back();
        first.first  = last.first;
        intervals[0] = first;
      }
    }
  }

  return !intervals.empty();
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::detectSpireIntervals(const TStroke *stroke,
                                          ToonzExt::Intervals &intervals,
                                          int minDegree) {
  if (!ToonzExt::isValid(stroke)) return false;

  minDegree = normalizeAngle(minDegree);
  std::vector<double> corners;

  bool found = ToonzExt::cornersDetector(stroke, minDegree, corners);
  if (!found) return false;

  assert(!corners.empty());

  intervals.clear();

  double first = corners[0], last = first;

  int size = corners.size();

  for (int i = 1; i < size; ++i) {
    last = corners[i];
    intervals.push_back(ToonzExt::Interval(first, last));
    first = last;
  }

  // intervals.push_back( ToonzExt::Interval(last,1.0));

  if (stroke->isSelfLoop()) {
    if (corners.size() == 1) {
      double val = corners[0];
      intervals.push_back(ToonzExt::Interval(val, val));
    } else if (!intervals.empty()) {
      ToonzExt::Interval first = intervals.front(), last = intervals.back();

      intervals.insert(intervals.begin(),
                       ToonzExt::Interval(last.second, first.first));
    }
  }

  return !intervals.empty();
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::isAStraightCorner(const TStroke *stroke, double w,
                                       const ToonzExt::Intervals *const cl,
                                       double tolerance) {
  if (!ToonzExt::isValid(stroke) || !ToonzExt::isValid(w)) return false;

  ToonzExt::Intervals tmpValues;

  const ToonzExt::Intervals *values = cl;

  if (!cl) {
    if (!ToonzExt::detectStraightIntervals(stroke, tmpValues, tolerance))
      return false;
    values = &tmpValues;
  }

  if (!values || values->empty()) {
    return false;
  }

  return isCorner(*values, w, tolerance);
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::findNearestStraightCorners(
    const TStroke *stroke, double w, ToonzExt::Interval &out,
    const ToonzExt::Intervals *const cl, double tolerance) {
  if (!stroke || w < 0.0 || w > 1.0) return false;

  const ToonzExt::Intervals *values = cl;

  ToonzExt::Intervals tmpValues;

  if (!cl) {
    if (!ToonzExt::detectStraightIntervals(stroke, tmpValues, tolerance))
      return false;
    values = &tmpValues;
  }

  if (!values || values->empty()) {
    return false;
  }

  return findNearestCorners(stroke, w, out, *values, tolerance);
}

//-----------------------------------------------------------------------------

TStroke *ToonzExt::rotateControlPoint(const TStroke *stroke,
                                      const ToonzExt::EvenInt &evenControlPoint,
                                      double atLength) {
  if (!stroke || !stroke->isSelfLoop() || !evenControlPoint.isEven()) return 0;
#ifdef _DEBUG
  TThickPoint tp1 = stroke->getControlPoint(0);
  TThickPoint tp2 = stroke->getControlPoint(stroke->getControlPointCount() - 1);
#endif
  int controlPoint = (int)evenControlPoint;

  const double length = stroke->getLength();

  // invalid length
  if (0.0 > atLength || atLength > length) return 0;

  const int cpCountAtBegin = stroke->getControlPointCount();
  // invalid control point
  if (0 > controlPoint || controlPoint > cpCountAtBegin) return 0;

  // identity
  if ((controlPoint == 0 || controlPoint == (cpCountAtBegin - 1)) &&
      (areAlmostEqual(atLength, length) || isAlmostZero(atLength)))
    return new TStroke(*stroke);

  TStroke tmpStroke(*stroke);

  std::vector<TThickPoint> cp;
  {
    int count = stroke->getControlPointCount();
    for (int i = 0; i < count; ++i) {
      cp.push_back(stroke->getControlPoint(i));
    }
  }

  // add a control point where is necessary to have rotation (head_queue cp)
  tmpStroke.insertControlPointsAtLength(atLength);

#ifdef _DEBUG
  {
    int count  = stroke->getControlPointCount(),
        count2 = tmpStroke.getControlPointCount();
    int i, firstDifference = -1;
    for (i = 0; i < count; ++i) {
      if ((tp1 = stroke->getControlPoint(i)) !=
          (tp2 = tmpStroke.getControlPoint(i))) {
        firstDifference = i;
        break;
      }
    }

    for (i = 0; i < count; ++i) {
      if (i < firstDifference)
        assert((tp1 = stroke->getControlPoint(i)) ==
               (tp2 = tmpStroke.getControlPoint(i)));
      else {
        //        assert( (tp1=stroke->getControlPoint(i)) ==
        //                (tp2=tmpStroke.getControlPoint(i+2)));
      }

      tp1 = stroke->getControlPoint(i);
      tp2 = tmpStroke.getControlPoint(i);
    }
  }
#endif
  const int cpCount = tmpStroke.getControlPointCount();

  double w = tmpStroke.getParameterAtLength(atLength);

  double butta = tmpStroke.getLength(w);
  assert(areAlmostEqual(butta, atLength));

  // retrieve head_queue control point
  TThickPoint head_queue = tmpStroke.getControlPointAtParameter(w);

  // recover index
  int i;
  for (i = 0; i < cpCount; ++i) {
    if (head_queue == tmpStroke.getControlPoint(i)) break;
  }

  const int head_index = i;

  // uhmmm really strange
  if (head_index == cpCount) {
    assert(!"Error on procedure!!! Not control point found!!!"
				" Wrong insert control point!!!");
    return 0;
  }

  // head_index is the new head of stroke
  std::vector<TThickPoint> new_stroke_cp;

  for (i = head_index; i < cpCount; ++i) {
    TThickPoint to_add = tmpStroke.getControlPoint(i);
    new_stroke_cp.push_back(to_add);
  }

  TThickPoint tmpCP = tmpStroke.getControlPoint(0);

  bool check = areAlmostEqual(new_stroke_cp.back(), tmpCP, 0.01);
  // relaxed check
  assert(check);

  if (!check) {
    assert(!"Error on procedure!!! Please verify algorithm!!!");
    return 0;
  }

  // 0 position is already inserted
  for (i = 1; i < head_index; ++i) {
    TThickPoint to_add = tmpStroke.getControlPoint(i);
    new_stroke_cp.push_back(to_add);
  }

  // now add queue == head cp
  new_stroke_cp.push_back(new_stroke_cp[0]);

  assert((int)new_stroke_cp.size() == cpCount);

  if (new_stroke_cp.back() != tmpStroke.getControlPoint(head_index)) {
    assert(!"Error on procedure!!! Please verify algorithm!!!");
    return 0;
  }

  TStroke *out = new TStroke(new_stroke_cp);
  out->setSelfLoop();
  return out;
}

//--------------------------------------------------------------------------------------

/*
 * A corner is straight, only if is contained two times
 * in the list of intervals.
 */
DVAPI bool ToonzExt::straightCornersDetector(const TStroke *stroke,
                                             std::vector<double> &corners) {
  ToonzExt::Intervals intervals;

  assert(corners.empty());
  if (!corners.empty()) corners.clear();

  if (!ToonzExt::detectStraightIntervals(stroke, intervals)) return false;

  assert(!intervals.empty() && "Intervals are empty!!!");
  if (intervals.empty()) return false;

  double first;

  ToonzExt::Interval prev = intervals[0], curr;

  if (stroke->isSelfLoop()) first = prev.first;

  int size = intervals.size();
  for (int i = 1; i < size; ++i) {
    curr = intervals[i];
    if (prev.second == curr.first) corners.push_back(curr.first);
    prev = curr;
  }

  if (stroke->isSelfLoop() && curr.second == first) corners.push_back(first);

  return !corners.empty();
}

//--------------------------------------------------------------------------------------

DVAPI bool ToonzExt::cornersDetector(const TStroke *stroke, int minDegree,
                                     std::vector<double> &corners) {
  assert(stroke);
  if (!stroke) return false;
  assert(corners.empty());
  if (!corners.empty()) corners.clear();

  assert(0 <= minDegree && minDegree <= 180);

  minDegree = normalizeAngle(minDegree);

  const double minSin = degree2sin(minDegree), minCos = degree2cos(minDegree);

  assert(0.0 <= minSin && minSin <= 1.0);

  // first step remove chunks with null length

  const TThickQuadratic *quad1 = 0, *quad2 = 0;

  UINT chunkCount = stroke->getChunkCount();

  quad1 = stroke->getChunk(0);
  assert(quad1);
  if (!quad1) return false;

  bool error = false, check = false;

  std::set<double> internal_corners;

  double t;

  if (curveIsStraight(quad1, t)) {
    if (t != -1) {
      check = mapValueInStroke(stroke, quad1, t, t);
      assert(check);
      if (check) internal_corners.insert(t);
    }
  }

  for (UINT j = 1; j < chunkCount; j++) {
    quad2 = stroke->getChunk(j);
    if (curveIsStraight(quad2, t)) {
      if (t != -1) {
        check = mapValueInStroke(stroke, quad2, t, t);
        assert(check);
        if (check) internal_corners.insert(t);
      }
    }

    assert(quad2);
    if (!quad2) error = true;

    double tmp = stroke->getW(quad2->getP0());

    // jump zero length curves
    if (!isAlmostZero(quad1->getLength()) &&
        !isAlmostZero(quad2->getLength()) &&
        isThereACornerMinusThan(minCos, minSin, quad1, quad2))
      internal_corners.insert(tmp);

    if (!isAlmostZero(quad2->getLength())) quad1 = quad2;
  }

  if (stroke->isSelfLoop() && chunkCount > 0) {
    quad2 = stroke->getChunk(0);
    quad1 = stroke->getChunk(chunkCount - 1);

    if (isThereACornerMinusThan(minCos, minSin, quad1, quad2))
      internal_corners.insert(0.0);
  } else {
    internal_corners.insert(0.0);
    internal_corners.insert(1.0);
  }

  if (error) return false;

  std::copy(internal_corners.begin(), internal_corners.end(),
            std::back_inserter(corners));

#ifdef _DEBUG
  double temp = 0;
  for (unsigned int k = 0; k < corners.size(); ++k) {
    assert(corners[k] >= temp);
    temp = corners[k];
  }
#endif

  return !corners.empty();
}

//---------------------------------------------------------------------------

void ToonzExt::cloneStrokeStatus(const TStroke *from, TStroke *to) {
  if (!ToonzExt::isValid(from) || !ToonzExt::isValid(to)) return;

  to->setId(from->getId());
  to->setSelfLoop(from->isSelfLoop());
  to->setStyle(from->getStyle());
  to->setAverageThickness(from->getAverageThickness());

  to->invalidate();
  to->enableComputeOfCaches();
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::replaceStroke(TStroke *old_stroke, TStroke *new_stroke,
                                   unsigned int n_, TVectorImageP &vi) {
  if (!ToonzExt::isValid(old_stroke) || !ToonzExt::isValid(new_stroke) || !vi)
    return false;

  const unsigned int strokesCount = vi->getStrokeCount();

  assert(n_ <= strokesCount);
  if (n_ > strokesCount) return false;

  assert(vi->getStroke(n_) == old_stroke);
  if (vi->getStroke(n_) != old_stroke) return false;

  // stroke is replaced (old stroke is deleted)
  // in vector image
  vi->replaceStroke(n_, new_stroke);

  int new_id = getStrokeId(new_stroke, vi);
  if (new_id == -1) return false;

  n_ = new_id;
  return true;
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::getAllW(const TStroke *stroke, const TPointD &pnt,
                             double &dist2, std::vector<double> &parameters) {
  assert(!"To be finished!!!");
  std::set<double> tmp;

  assert(stroke);
  if (ToonzExt::isValid(stroke)) return false;

  double outT, w;

  const TThickQuadratic *tq = 0;

  int chunkFound = -1;

  double distance2;

  if (stroke->getNearestChunk(pnt, outT, chunkFound, distance2, false)) {
    dist2 = distance2;
    tq    = stroke->getChunk(chunkFound);
    if (tq) {
      w = stroke->getW(tq->getPoint(outT));
      if (ToonzExt::isValid(w)) tmp.insert(w);
    }
  }

  int i, chunkCount = stroke->getChunkCount();

  for (i = 0; i < chunkCount; i++) {
    if (i != chunkFound) {
      tq              = stroke->getChunk(i);
      TPointD tmp_pnt = tq->getPoint(tq->getT(pnt));
      if (areAlmostEqual(tdistance2(tmp_pnt, pnt), dist2)) {
        w = stroke->getW(tmp_pnt);
        if (ToonzExt::isValid(w)) tmp.insert(w);
      }
    }
  }

  std::copy(tmp.begin(), tmp.end(), std::back_inserter(parameters));

  return !tmp.empty();
}

//-----------------------------------------------------------------------------

DVAPI bool ToonzExt::findNearestCorners(const TStroke *stroke, double w,
                                        ToonzExt::Interval &out,
                                        const ToonzExt::Intervals &values,
                                        double tolerance) {
  out = ToonzExt::Interval(-1.0, -1.0);

  if (!ToonzExt::isValid(stroke) || !ToonzExt::isValid(w) || values.empty())
    return false;

  ToonzExt::Interval prev = values[0], curr = prev;

  if (!stroke->isSelfLoop() && areAlmostEqual(w, prev.first, tolerance)) {
    out = prev;
    return true;
  }

  const int size = values.size();

  for (int i = 1; i <= size; ++i) {
    if (i < size)
      curr = values[i];
    else
      curr = values[0];

    // this case, w is an extreme and needs to be considered
    //  the nearest external interval
    if (areAlmostEqual(w, curr.first, tolerance) &&
        (prev.second == curr.first)) {
      if (isWGood(prev.first, w, curr.second, stroke) ||
          (prev.first == curr.second)) {
        out.first  = prev.first;
        out.second = curr.second;
        return true;
      }
    }

    // normal case
    if (isWGood(curr.first, w, curr.second, stroke)) {
      out = curr;
      return true;
    }

    prev = curr;
  }

  curr = values.back();

  // last point
  if (!stroke->isSelfLoop() && areAlmostEqual(curr.second, w, tolerance)) {
    out = curr;
    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

DVAPI void ToonzExt::findCorners(const TStroke *stroke,
                                 ToonzExt::Intervals &corners,
                                 ToonzExt::Intervals &intervals, int angle,
                                 double tolerance) {
  assert(stroke && "Stroke is null!!!");
  if (!ToonzExt::isValid(stroke)) return;

  angle = normalizeAngle(angle);

  ToonzExt::detectSpireIntervals(stroke, corners, angle);

  // a corner SPIRE is also a STRAIGHT
  ToonzExt::detectStraightIntervals(stroke, intervals, tolerance);
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
