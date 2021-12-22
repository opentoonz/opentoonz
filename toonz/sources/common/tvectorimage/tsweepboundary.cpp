

//#include "tgeometry.h"

#include <set>
#include <map>

#include "tgl.h"
#include "tstroke.h"
//#include "tstrokeoutline.h"
#include "tcurveutil.h"
//#include "drawutil.h"
#include "tvectorimage.h"

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

#include "tsweepboundary.h"
#include "tcurves.h"

// Some using declaration
using namespace std;

inline bool operator<(const TPointD &a, const TPointD &b) {
  if (a.x < b.x)
    return true;
  else if (a.x > b.x)
    return false;
  else if (a.y < b.y)
    return true;
  else if (a.y > b.y)
    return false;
  else
    return true;
}

namespace {
const double delta                     = 0.000001;
const double zero                      = delta;
const double one                       = 1 - delta;
const double thicknessLimit            = 0.3;
const double nonSimpleLoopsMaxDistance = 0.5;
const int nonSimpleLoopsMaxSize        = 5;
const int smallStrokeDim               = nonSimpleLoopsMaxSize * 5;
bool isSmallStroke                     = false;

set<TPointD> simpleCrossing;
set<TPointD> nonSimpleCrossing;

class LinkedQuadratic final : public TQuadratic {
public:
  LinkedQuadratic *prev, *next;
  LinkedQuadratic() : TQuadratic(), prev(0), next(0){};
  LinkedQuadratic(const TPointD &p0, const TPointD &p1, const TPointD &p2)
      : TQuadratic(p0, p1, p2), prev(0), next(0) {}
  LinkedQuadratic(TQuadratic &Quadratic)
      : TQuadratic(Quadratic), prev(0), next(0) {}
};

typedef enum Direction {
  inward         = 0,
  outward        = 1,
  deletedInward  = 2,
  deletedOutward = 3
} Direction;

/*
        class CompareOutlines {
        public:
                bool operator()(const vector<TQuadratic*> &v1,
                                                const vector<TQuadratic*> &v2)
                {
                        if(v1.empty()) return false;
                        else if(v2.empty()) return true;
                        else return v1[0]->getBBox().y1 > v2[0]->getBBox().y1;
                }
        };

        class CompareQuadratics {
        public:
                bool operator()(TQuadratic *const q1,
                                                TQuadratic *const q2)
                {
                        if (q1->getBBox().y1 > q2->getBBox().y1) return true;
                        else if (q1->getBBox().y1 < q2->getBBox().y1) return
   false;
                        else if (q1->getBBox().x1 > q2->getBBox().x1) return
   true;
                        else if (q1->getBBox().x1 < q2->getBBox().x1) return
   false;
                        else return false;
                }
        };
*/
class CompareLinkedQuadratics {
public:
  bool operator()(const LinkedQuadratic &q1, const LinkedQuadratic &q2) {
    if (q1.getBBox().y1 > q2.getBBox().y1)
      return true;
    else if (q1.getBBox().y1 < q2.getBBox().y1)
      return false;
    else if (q1.getBBox().x1 > q2.getBBox().x1)
      return true;
    else if (q1.getBBox().x1 < q2.getBBox().x1)
      return false;
    else
      return false;
  }
};

class CompareBranches {
public:
  bool operator()(const pair<LinkedQuadratic *, Direction> &b1,
                  const pair<LinkedQuadratic *, Direction> &b2) {
    TPointD p1, p2;
    if (b1.second == inward) {
      p1 = b1.first->getP1() - b1.first->getP2();
    } else  //(b1.second == outward)
    {
      p1 = b1.first->getP1() - b1.first->getP0();
    }

    if (b2.second == inward) {
      p2 = b2.first->getP1() - b2.first->getP2();
    } else  //(b1.second == outward)
    {
      p2 = b2.first->getP1() - b2.first->getP0();
    }

    double alpha1, alpha2;

    if (p1.x > 0)
      alpha1 = -p1.y / sqrt(norm2(p1));
    else if (p1.x < 0)
      alpha1 = 2 + p1.y / sqrt(norm2(p1));
    else  //(p1.x = 0)
    {
      if (p1.y > 0)
        alpha1 = -1;
      else if (p1.y < 0)
        alpha1 = 1;
      else
        assert(true);
    }

    if (p2.x > 0)
      alpha2 = -p2.y / sqrt(norm2(p2));
    else if (p2.x < 0)
      alpha2 = 2 + p2.y / sqrt(norm2(p2));
    else  //(p2.x = 0)
    {
      if (p2.y > 0)
        alpha2 = -1;
      else if (p2.y < 0)
        alpha2 = 1;
      else
        assert(true);
    }

    if (alpha2 - alpha1 > 0)
      return true;
    else if (alpha2 - alpha1 < 0)
      return false;
    else
      return false;
  }
};

typedef list<LinkedQuadratic> LinkedQuadraticList;
typedef list<TQuadratic> QuadraticList;
}  // namespace {

//---------------------------------------------------------------------------

static void splitCircularArcIntoQuadraticCurves(
    const TPointD &Center, const TPointD &Pstart, const TPointD &Pend,
    vector<TQuadratic *> &quadArray) {
  // It splits a circular anticlockwise arc into a sequence of quadratic bezier
  // curves
  // Every quadratic curve can approximate an arc no longer than 45 degrees (or
  // 60).
  // It supposes that Pstart and Pend are onto the circumference (so that their
  // lengths
  // are equal to tha radius of the circumference), otherwise the resulting
  // curves could
  // be unpredictable.
  // The last component in quadCurve[] is an ending void curve

  /*
----------------------------------------------------------------------------------
*/
  // If you want to split the arc into arcs no longer than 45 degrees (so that
  // the whole
  // curve will be splitted into 8 pieces) you have to set these constants as
  // follows:
  // cos_ang     ==> cos_45   = 0.5 * sqrt(2);
  // sin_ang     ==> sin_45   = 0.5 * sqrt(2);
  // tan_semiang ==> tan_22p5 = 0.4142135623730950488016887242097;
  // N_QUAD                   = 8;

  // If you want to split the arc into arcs no longer than 60 degrees (so that
  // the whole
  // curve will be splitted into 6 pieces) you have to set these constants as
  // follows:
  // cos_ang     ==> cos_60 = 0.5;
  // sin_ang     ==> sin_60 = 0.5 * sqrt(3);
  // tan_semiang ==> tan_30 = 0.57735026918962576450914878050196;
  // N_QUAD                 = 6;
  /*
----------------------------------------------------------------------------------
*/

  // Defines some useful constant to split the arc into arcs no longer than
  // 'ang' degrees
  // (the whole circumference will be splitted into 360/ang quadratic curves).
  const double cos_ang     = 0.5 * sqrt(2.);
  const double sin_ang     = 0.5 * sqrt(2.);
  const double tan_semiang = 0.4142135623730950488016887242097;
  const int N_QUAD         = 8;  // it's 360/ang

  // First of all, it computes the vectors from the center to the circumference,
  // in Pstart and Pend, and their cross and dot products
  TPointD Rstart = Pstart - Center;  // its length is R (radius of the circle)
  TPointD Rend   = Pend - Center;    // its length is R (radius of the circle)
  double cross_prod       = cross(Rstart, Rend);  // it's Rstart x Rend
  double dot_prod         = Rstart * Rend;
  const double sqr_radius = Rstart * Rstart;
  TPointD aliasPstart     = Pstart;
  TQuadratic *quad;

  while ((cross_prod <= 0) ||
         (dot_prod <= cos_ang * sqr_radius))  // the circular arc is longer
                                              // than a 'ang' degrees arc
  {
    if (quadArray.size() == (UINT)N_QUAD)  // this is possible if Pstart or Pend
                                           // is not onto the circumference
      return;
    TPointD Rstart_rot_ang(cos_ang * Rstart.x - sin_ang * Rstart.y,
                           sin_ang * Rstart.x + cos_ang * Rstart.y);
    TPointD Rstart_rot_90(-Rstart.y, Rstart.x);
    quad =
        new TQuadratic(aliasPstart, aliasPstart + tan_semiang * Rstart_rot_90,
                       Center + Rstart_rot_ang);
    quadArray.push_back(quad);

    // quad->computeMinStepAtNormalSize ();

    // And moves anticlockwise the starting point on the circumference by 'ang'
    // degrees
    Rstart      = Rstart_rot_ang;
    aliasPstart = quad->getP2();
    cross_prod  = cross(Rstart, Rend);  // it's Rstart x Rend
    dot_prod    = Rstart * Rend;

    // after the rotation of 'ang' degrees, the remaining part of the arc could
    // be a 0 degree
    // arc, so it must stop and exit from the function
    if ((cross_prod <= 0) && (dot_prod > 0.95 * sqr_radius)) return;
  }

  if ((cross_prod > 0) && (dot_prod > 0))  // the last quadratic curve
                                           // approximates an arc shorter than a
                                           // 'ang' degrees arc
  {
    TPointD Rstart_rot_90(-Rstart.y, Rstart.x);

    double deg_index = (sqr_radius - dot_prod) / (sqr_radius + dot_prod);

    quad = new TQuadratic(aliasPstart,
                          (deg_index < 0)
                              ? 0.5 * (aliasPstart + Pend)
                              : aliasPstart + sqrt(deg_index) * Rstart_rot_90,
                          Pend);
    quadArray.push_back(quad);

  } else  // the last curve, already computed, is as long as a 'ang' degrees arc
    quadArray.back()->setP2(Pend);
}

inline bool left(const TPointD &a, const TPointD &b, const TPointD &c) {
  double area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
  return area > 0;
}

inline bool right(const TPointD &a, const TPointD &b, const TPointD &c) {
  double area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
  return area < 0;
}

inline bool collinear(const TPointD &a, const TPointD &b, const TPointD &c) {
  double area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
  return area == 0;
}

void computeStrokeBoundary(const TStroke &stroke,
                           LinkedQuadraticList &inputBoundaries,
                           unsigned int &chunkIndex);
void normalizeTThickQuadratic(const TThickQuadratic *&sourceThickQuadratic,
                              TThickQuadratic &tempThickQuadratic);
inline void normalizeTQuadratic(TQuadratic *&sourceQuadratic);
void getBoundaryPoints(const TPointD &P0, const TPointD &P1,
                       const TThickPoint &center, TPointD &fwdPoint,
                       TPointD &rwdPoint);
void getAverageBoundaryPoints(const TPointD &P0, const TThickPoint &center,
                              const TPointD &P2, TPointD &fwdPoint,
                              TPointD &rwdPoint);
void linkQuadraticList(LinkedQuadraticList &inputBoundaries);
void computeInputBoundaries(LinkedQuadraticList &inputBoundaries);
void processAdjacentQuadratics(LinkedQuadraticList &inputBoundaries);
void findIntersections(
    LinkedQuadratic *quadratic, set<LinkedQuadratic *> &intersectionWindow,
    map<LinkedQuadratic *, vector<double>> &intersectedQuadratics);
void refreshIntersectionWindow(LinkedQuadratic *quadratic,
                               set<LinkedQuadratic *> &intersectionWindow);
void segmentate(LinkedQuadraticList &inputBoundaries,
                LinkedQuadratic *thickQuadratic, vector<double> &splitPoints);
void processIntersections(LinkedQuadraticList &intersectionBoundary);
bool processNonSimpleLoops(
    TPointD &intersectionPoint,
    vector<pair<LinkedQuadratic *, Direction>> &crossing);
bool deleteUnlinkedLoops(LinkedQuadraticList &inputBoundaries);
bool getOutputOutlines(LinkedQuadraticList &inputBoundaries,
                       vector<TStroke *> &sweepStrokes);
void removeFalseHoles(const vector<TStroke *> &strokes);

inline void TraceLinkedQuadraticList(LinkedQuadraticList &quadraticList) {
#ifdef _WIN32
  _RPT0(_CRT_WARN, "\n__________________________________________________\n");
  LinkedQuadraticList::iterator it = quadraticList.begin();
  while (it != quadraticList.end()) {
    _RPT4(_CRT_WARN, "\nP0( %f, %f)   P2( %f, %f)", it->getP0().x,
          it->getP0().y, it->getP2().x, it->getP2().y);
    _RPT3(_CRT_WARN, " currAddress = %p, nextAddress = %p prevAddress = %p\n",
          &(*it), it->next, it->prev);
    ++it;
  }
#endif
}

inline void drawPointSquare(const TPointD &point, double R, double G,
                            double B) {
#define SQUARE_DIM 0.04
  glBegin(GL_LINE_LOOP);
  glColor3d(R, G, B);
  glVertex2d(point.x + SQUARE_DIM, point.y + SQUARE_DIM);
  glVertex2d(point.x + SQUARE_DIM, point.y - SQUARE_DIM);
  glVertex2d(point.x - SQUARE_DIM, point.y - SQUARE_DIM);
  glVertex2d(point.x - SQUARE_DIM, point.y + SQUARE_DIM);
  glEnd();
}

inline void drawPointCross(const TPointD &point, double R, double G, double B) {
#define CROSS_DIM 0.04
  glBegin(GL_LINES);
  glColor3d(R, G, B);
  glVertex2d(point.x - CROSS_DIM, point.y - CROSS_DIM);
  glVertex2d(point.x + CROSS_DIM, point.y + CROSS_DIM);
  glVertex2d(point.x + CROSS_DIM, point.y - CROSS_DIM);
  glVertex2d(point.x - CROSS_DIM, point.y + CROSS_DIM);
  glEnd();
}

//-------------------------------------------------------------------

static TStroke *getOutStroke(LinkedQuadraticList &inputBoundaries) {
  vector<TPointD> aux;
  LinkedQuadraticList::iterator it = inputBoundaries.begin();

  aux.push_back(inputBoundaries.front().getP0());
  for (; it != inputBoundaries.end(); ++it)

  {
    // if (tdistance2(aux.back(), it->getP2())>0.25)
    {
      aux.push_back(it->getP1());
      aux.push_back(it->getP2());
    }
    // inputBoundaries.remove(*it);
  }
  return new TStroke(aux);
}

//-------------------------------------------------------------------

inline bool getOutputOutlines(LinkedQuadraticList &inputBoundaries,
                              vector<TStroke *> &sweepStrokes) {
  // int count=0;

  while (!inputBoundaries.empty()) {
    vector<TPointD> v;
    LinkedQuadraticList::iterator it = inputBoundaries.begin();
    // std::advance(it, count+1);
    LinkedQuadratic *first = &(*it);
    LinkedQuadratic *toRemove, *current = first;
    v.push_back(current->getP0());
    do {
      // if (tdistance2(v.back(), current->getP2())>0.25)
      {
        v.push_back(current->getP1());
        v.push_back(current->getP2());
      }
      // count++;
      // outputOutlines.back().m_quads.push_back(new TQuadratic(*current));
      toRemove = current;
      current  = current->next;
      inputBoundaries.remove(*toRemove);

      //			assert(current);
      if (!current) {
        inputBoundaries.clear();
        //				outputOutlines.pop_back();
        return false;
      }

    } while (current != first && !inputBoundaries.empty());
    sweepStrokes.push_back(new TStroke(v));
    //		sort(outputOutlines[count].begin(), outputOutlines[count].end(),
    // CompareQuadratics());
  }
  inputBoundaries.clear();
  return true;
}

//-------------------------------------------------------------------

static bool computeBoundaryStroke(const TStroke &_stroke,
                                  vector<TStroke *> &sweepStrokes) {
  // if(!outlines.empty()) return false;

  TStroke *oriStroke = const_cast<TStroke *>(&_stroke);
  TStroke *stroke    = oriStroke;
  for (int i = 0; i < stroke->getControlPointCount(); i++) {
    TThickPoint p = stroke->getControlPoint(i);
    // se ci sono punti a spessore nullo, viene male il boundary.
    if (areAlmostEqual(p.thick, 0, 1e-8)) {
      if (stroke == oriStroke) stroke = new TStroke(_stroke);
      stroke->setControlPoint(i, TThickPoint(p.x, p.y, 0.0001));
    }
  }

  unsigned int chunkIndex = 0;
  while (chunkIndex < (UINT)stroke->getChunkCount()) {
    LinkedQuadraticList tempBoundary;
    LinkedQuadraticList inputBoundaries;
    simpleCrossing.clear();
    nonSimpleCrossing.clear();
    isSmallStroke = false;

    computeStrokeBoundary(*stroke, inputBoundaries, chunkIndex);
    inputBoundaries.sort(CompareLinkedQuadratics());

    computeInputBoundaries(inputBoundaries);
    if (!deleteUnlinkedLoops(inputBoundaries)) return false;
    if (!getOutputOutlines(inputBoundaries, sweepStrokes)) return false;
    // TStroke *sout = getOutStroke(inputBoundaries);
    // sweepStrokes.push_back(sout);

    // if(!getOutputOutlines(inputBoundaries, outlines)) return false;
  }

  if (stroke != &_stroke) delete stroke;
  return true;
}

//-------------------------------------------------------------------

inline void computeStrokeBoundary(const TStroke &stroke,
                                  LinkedQuadraticList &inputBoundaries,
                                  unsigned int &chunkIndex) {
  unsigned int chunkCount = stroke.getChunkCount();
  assert(chunkCount - chunkIndex > 0);

  if ((int)(chunkCount - chunkIndex) <= smallStrokeDim) isSmallStroke = true;

  unsigned int startIndex               = chunkIndex;
  const TThickQuadratic *thickQuadratic = 0, *nextThickQuadratic = 0;
  TThickQuadratic tempThickQuadratic, tempNextThickQuadratic;

  TPointD fwdP0, fwdP1, fwdP2;
  TPointD rwdP0, rwdP1, rwdP2;
  TPointD nextFwdP0, nextRwdP2;

  thickQuadratic = stroke.getChunk(chunkIndex);
  while (thickQuadratic->getP0() == thickQuadratic->getP2()) {
    double thickness;
    thickness = std::max({thickQuadratic->getThickP0().thick,
                          thickQuadratic->getThickP1().thick,
                          thickQuadratic->getThickP2().thick});

    ++chunkIndex;
    if (chunkIndex == chunkCount) {
      vector<TQuadratic *> quadArray;
      double thickness = std::max({thickQuadratic->getThickP0().thick,
                                   thickQuadratic->getThickP1().thick,
                                   thickQuadratic->getThickP2().thick});

      if (thickness < thicknessLimit) thickness = thicknessLimit;

      TPointD center        = thickQuadratic->getP0();
      TPointD diameterStart = thickQuadratic->getP0();
      diameterStart.y += thickness;
      TPointD diameterEnd = thickQuadratic->getP0();
      diameterEnd.y -= thickness;

      splitCircularArcIntoQuadraticCurves(center, diameterStart, diameterEnd,
                                          quadArray);
      unsigned int i = 0;
      for (; i < quadArray.size(); ++i) {
        assert(!(quadArray[i]->getP0() == quadArray[i]->getP2()));
        normalizeTQuadratic(quadArray[i]);
        inputBoundaries.push_back(*quadArray[i]);
        delete quadArray[i];
      }
      quadArray.clear();

      splitCircularArcIntoQuadraticCurves(center, diameterEnd, diameterStart,
                                          quadArray);
      for (i = 0; i < quadArray.size(); ++i) {
        assert(!(quadArray[i]->getP0() == quadArray[i]->getP2()));
        normalizeTQuadratic(quadArray[i]);
        inputBoundaries.push_back(*quadArray[i]);
        delete quadArray[i];
      }
      quadArray.clear();

      linkQuadraticList(inputBoundaries);
      return;
    }
    thickQuadratic = stroke.getChunk(chunkIndex);
  }

  normalizeTThickQuadratic(thickQuadratic, tempThickQuadratic);
  getBoundaryPoints(thickQuadratic->getP0(), thickQuadratic->getP1(),
                    thickQuadratic->getThickP0(), fwdP0, rwdP2);

  if (!(rwdP2 == fwdP0)) {
    //		inputBoundaries.push_front(TQuadratic(rwdP2, (rwdP2+fwdP0)*0.5,
    // fwdP0));
    vector<TQuadratic *> quadArray;
    splitCircularArcIntoQuadraticCurves((rwdP2 + fwdP0) * 0.5, rwdP2, fwdP0,
                                        quadArray);
    for (unsigned int i = 0; i < quadArray.size(); ++i) {
      if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
        normalizeTQuadratic(quadArray[i]);
        inputBoundaries.push_back(*quadArray[i]);
      }
      delete quadArray[i];
    }
    quadArray.clear();
  }

  for (/*chunkIndex*/; chunkIndex < chunkCount; ++chunkIndex) {
    thickQuadratic = stroke.getChunk(chunkIndex);
    while (thickQuadratic->getP0() == thickQuadratic->getP2()) {
      ++chunkIndex;
      if (chunkIndex == chunkCount) break;
      thickQuadratic = stroke.getChunk(chunkIndex);
    }
    if (chunkIndex >= chunkCount - 1) {
      chunkIndex = chunkCount;
      break;
    }

    unsigned int nextChunkIndex = chunkIndex + 1;
    nextThickQuadratic          = stroke.getChunk(nextChunkIndex);
    while (nextThickQuadratic->getP0() == nextThickQuadratic->getP2()) {
      ++nextChunkIndex;
      if (nextChunkIndex == chunkCount) {
        chunkIndex = chunkCount;
        break;
      }
      nextThickQuadratic = stroke.getChunk(nextChunkIndex);
    }
    if (nextChunkIndex == chunkCount) {
      chunkIndex = chunkCount;
      break;
    }

    if (thickQuadratic->getP0() == nextThickQuadratic->getP2() &&
        thickQuadratic->getP2() == nextThickQuadratic->getP0()) {
      chunkIndex = nextChunkIndex;
      continue;
    }

    if (chunkIndex == startIndex + 2 &&
        norm(stroke.getChunk(startIndex)->getP0() -
             stroke.getChunk(chunkCount - 1)->getP2()) <
            stroke.getChunk(startIndex)->getThickP0().thick / 2) {
      ++chunkIndex;
      break;
    }

    normalizeTThickQuadratic(thickQuadratic, tempThickQuadratic);
    normalizeTThickQuadratic(nextThickQuadratic, tempNextThickQuadratic);

    vector<DoublePair> intersections;
    TQuadratic quadratic(thickQuadratic->getP0(), thickQuadratic->getP1(),
                         thickQuadratic->getP2());
    TQuadratic nextQuadratic(nextThickQuadratic->getP0(),
                             nextThickQuadratic->getP1(),
                             nextThickQuadratic->getP2());

    if (intersect(quadratic, nextQuadratic, intersections) > 1) {
      double currSplit = 1, nextSplit = 0;
      for (unsigned int i = 0; i < intersections.size(); ++i) {
        if (currSplit > intersections[i].first)
          currSplit = intersections[i].first;
        if (nextSplit < intersections[i].second)
          nextSplit = intersections[i].second;
      }
      if (currSplit < one && nextSplit > zero && currSplit > 0.5 &&
          nextSplit < 0.5) {
        TQuadratic firstSplit, secondSplit;

        quadratic.split(currSplit, firstSplit, secondSplit);
        const_cast<TThickQuadratic *>(thickQuadratic)
            ->setP1(firstSplit.getP1());
        const_cast<TThickQuadratic *>(thickQuadratic)
            ->setP2(firstSplit.getP2());

        nextQuadratic.split(nextSplit, firstSplit, secondSplit);
        const_cast<TThickQuadratic *>(nextThickQuadratic)
            ->setP0(secondSplit.getP0());
        const_cast<TThickQuadratic *>(nextThickQuadratic)
            ->setP1(secondSplit.getP1());
      }
    }

    getAverageBoundaryPoints(thickQuadratic->getP0(),
                             thickQuadratic->getThickP1(),
                             thickQuadratic->getP2(), fwdP1, rwdP1);

    getBoundaryPoints(thickQuadratic->getP1(), thickQuadratic->getP2(),
                      thickQuadratic->getThickP2(), fwdP2, rwdP0);
    getBoundaryPoints(thickQuadratic->getP2(), nextThickQuadratic->getP1(),
                      thickQuadratic->getThickP2(), nextFwdP0, nextRwdP2);

    TPointD v1 = thickQuadratic->getP2() - thickQuadratic->getP1();
    TPointD v2 = nextThickQuadratic->getP1() - nextThickQuadratic->getP0();

    if ((v1 * v2) / (norm(v1) * norm(v2)) < -0.95) {
      ++chunkIndex;
      break;
    }
    if (nextFwdP0 == fwdP2 && nextRwdP2 == rwdP0) {
      inputBoundaries.push_front(LinkedQuadratic(rwdP0, rwdP1, rwdP2));
      inputBoundaries.push_back(LinkedQuadratic(fwdP0, fwdP1, fwdP2));
      fwdP0 = fwdP2;
      rwdP2 = rwdP0;
    } else if (!(nextFwdP0 == fwdP2) && !(nextRwdP2 == rwdP0)) {
      bool turnLeft, turnRight;
      turnLeft = left(thickQuadratic->getP1(), thickQuadratic->getP2(),
                      nextThickQuadratic->getP1());
      turnRight = right(thickQuadratic->getP1(), thickQuadratic->getP2(),
                        nextThickQuadratic->getP1());
      if (turnLeft) {
        double thickness = thickQuadratic->getThickP2().thick;
        if (thickness < thicknessLimit) thickness = thicknessLimit;

        TPointD temp;
        if (rwdP0 + nextRwdP2 - 2 * thickQuadratic->getP2() != TPointD(0, 0)) {
          temp = (normalize(rwdP0 + nextRwdP2 - 2 * thickQuadratic->getP2()) *
                  thickness) +
                 thickQuadratic->getP2();
        } else
          temp = TPointD(0, 0);

        inputBoundaries.push_front(LinkedQuadratic(temp, rwdP1, rwdP2));
        inputBoundaries.push_back(LinkedQuadratic(fwdP0, fwdP1, fwdP2));

        vector<TQuadratic *> quadArray;
        splitCircularArcIntoQuadraticCurves(thickQuadratic->getP2(), fwdP2,
                                            nextFwdP0, quadArray);
        for (unsigned int i = 0; i < quadArray.size(); ++i) {
          if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
            normalizeTQuadratic(quadArray[i]);
            inputBoundaries.push_back(*quadArray[i]);
          }
          delete quadArray[i];
        }
        quadArray.clear();

        fwdP0 = nextFwdP0;
        rwdP2 = temp;
      } else if (turnRight) {
        double thickness = thickQuadratic->getThickP2().thick;
        if (thickness < thicknessLimit) thickness = thicknessLimit;

        TPointD temp;
        if (fwdP2 + nextFwdP0 - 2 * thickQuadratic->getP2() != TPointD(0, 0)) {
          temp = (normalize(fwdP2 + nextFwdP0 - 2 * thickQuadratic->getP2()) *
                  thickness) +
                 thickQuadratic->getP2();
        } else
          temp = TPointD(0, 0);

        inputBoundaries.push_front(LinkedQuadratic(rwdP0, rwdP1, rwdP2));
        inputBoundaries.push_back(LinkedQuadratic(fwdP0, fwdP1, temp));

        vector<TQuadratic *> quadArray;
        splitCircularArcIntoQuadraticCurves(thickQuadratic->getP2(), nextRwdP2,
                                            rwdP0, quadArray);
        for (int i = quadArray.size() - 1; i >= 0; --i) {
          if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
            normalizeTQuadratic(quadArray[i]);
            inputBoundaries.push_front(*quadArray[i]);
          }
          delete quadArray[i];
        }
        quadArray.clear();

        fwdP0 = temp;
        rwdP2 = nextRwdP2;
      } else if (nextFwdP0 == rwdP0 && nextRwdP2 == fwdP2) {
        ++chunkIndex;
        break;

        //				assert(collinear(thickQuadratic->getP0(),
        //								thickQuadratic->getP2(),
        //								nextThickQuadratic->getP2()));

        if (!collinear(thickQuadratic->getP0(), thickQuadratic->getP2(),
                       nextThickQuadratic->getP2())) {
          inputBoundaries.push_back(LinkedQuadratic(fwdP0, fwdP1, fwdP2));
          inputBoundaries.push_front(
              LinkedQuadratic(thickQuadratic->getP2(), rwdP1, rwdP2));

          vector<TQuadratic *> quadArray;
          splitCircularArcIntoQuadraticCurves(thickQuadratic->getP2(), fwdP2,
                                              nextFwdP0, quadArray);
          for (unsigned int i = 0; i < quadArray.size(); ++i) {
            if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
              normalizeTQuadratic(quadArray[i]);
              inputBoundaries.push_back(*quadArray[i]);
            }
            delete quadArray[i];
          }
          quadArray.clear();

          fwdP0 = nextFwdP0;
          rwdP2 = thickQuadratic->getP2();
        } else if (left(thickQuadratic->getP0(), thickQuadratic->getP1(),
                        nextThickQuadratic->getP2())) {
          inputBoundaries.push_back(LinkedQuadratic(fwdP0, fwdP1, fwdP2));
          vector<TQuadratic *> quadArray;
          splitCircularArcIntoQuadraticCurves(thickQuadratic->getP2(), fwdP2,
                                              nextFwdP0, quadArray);
          for (unsigned int i = 0; i < quadArray.size(); ++i) {
            if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
              normalizeTQuadratic(quadArray[i]);
              inputBoundaries.push_back(*quadArray[i]);
            }
            delete quadArray[i];
          }
          quadArray.clear();
          fwdP0 = nextFwdP0;
          rwdP2 = rwdP0;
        } else if (right(thickQuadratic->getP0(), thickQuadratic->getP1(),
                         nextThickQuadratic->getP2())) {
          inputBoundaries.push_front(LinkedQuadratic(rwdP0, rwdP1, rwdP2));
          vector<TQuadratic *> quadArray;
          splitCircularArcIntoQuadraticCurves(thickQuadratic->getP2(), fwdP2,
                                              nextFwdP0, quadArray);
          for (int i = quadArray.size() - 1; i >= 0; --i) {
            if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
              normalizeTQuadratic(quadArray[i]);
              inputBoundaries.push_front(*quadArray[i]);
            }
            delete quadArray[i];
          }
          quadArray.clear();
          fwdP0 = fwdP2;
          rwdP2 = nextRwdP2;
        } else {
          //					inputBoundaries.push_back(TQuadratic(fwdP0,
          // fwdP1, fwdP2));
          //					inputBoundaries.push_front(TQuadratic(rwdP0,
          // rwdP1, rwdP2));
          //					fwdP0 = nextFwdP0;
          //					rwdP2 = nextRwdP2;
          ++chunkIndex;
          break;
        }
      } else
        assert(false);
    } else
      assert(false);
  }

  normalizeTThickQuadratic(thickQuadratic, tempThickQuadratic);

  //	if(	stroke->getChunk(0)->getP0() ==
  // stroke->getChunk(chunkCount-1)->getP2() )
  /*	if(	norm(stroke->getChunk(0)->getP0() - stroke->getChunk(chunkCount-1)->getP2()) <
		stroke->getChunk(0)->getThickP0().thick/2)
	{
		getAverageBoundaryPoints(thickQuadratic->getP0(),
					thickQuadratic->getThickP1(),
					thickQuadratic->getP2(),
					fwdP1,
					rwdP1);

		getBoundaryPoints(thickQuadratic->getP1(),
						thickQuadratic->getP2(),
						thickQuadratic->getThickPoint(one),
						fwdP2,
						rwdP0);

		inputBoundaries.push_front(TQuadratic(rwdP0, rwdP1, rwdP2));
		inputBoundaries.push_back(TQuadratic(fwdP0, fwdP1, fwdP2));
		inputBoundaries.push_back(TQuadratic(fwdP2, (fwdP2+rwdP0)*0.5, rwdP0));
	}
	else
*/ {
    getAverageBoundaryPoints(thickQuadratic->getP0(),
                             thickQuadratic->getThickP1(),
                             thickQuadratic->getP2(), fwdP1, rwdP1);

    getBoundaryPoints(thickQuadratic->getP1(), thickQuadratic->getP2(),
                      thickQuadratic->getThickP2(), fwdP2, rwdP0);

    inputBoundaries.push_front(LinkedQuadratic(rwdP0, rwdP1, rwdP2));
    inputBoundaries.push_back(LinkedQuadratic(fwdP0, fwdP1, fwdP2));

    if (!(fwdP2 == rwdP0)) {
      vector<TQuadratic *> quadArray;
      splitCircularArcIntoQuadraticCurves((fwdP2 + rwdP0) * 0.5, fwdP2, rwdP0,
                                          quadArray);
      for (unsigned int i = 0; i < quadArray.size(); ++i) {
        if (!(quadArray[i]->getP0() == quadArray[i]->getP2())) {
          normalizeTQuadratic(quadArray[i]);
          inputBoundaries.push_back(*quadArray[i]);
        }
        delete quadArray[i];
      }
      quadArray.clear();
    }
  }

  linkQuadraticList(inputBoundaries);
}

//-------------------------------------------------------------------

inline void normalizeTThickQuadratic(
    const TThickQuadratic *&sourceThickQuadratic,
    TThickQuadratic &tempThickQuadratic) {
  assert(!(sourceThickQuadratic->getP0() == sourceThickQuadratic->getP2()));
  if (sourceThickQuadratic->getP0() == sourceThickQuadratic->getP1() ||
      sourceThickQuadratic->getP1() == sourceThickQuadratic->getP2() ||
      collinear(sourceThickQuadratic->getP0(), sourceThickQuadratic->getP1(),
                sourceThickQuadratic->getP2())) {
    tempThickQuadratic = *sourceThickQuadratic;
    TThickPoint middleThickPoint(
        (sourceThickQuadratic->getP0() + sourceThickQuadratic->getP2()) * 0.5);
    middleThickPoint.thick = tempThickQuadratic.getThickP1().thick;
    tempThickQuadratic.setThickP1(middleThickPoint);
    sourceThickQuadratic = &tempThickQuadratic;
  }
}

inline void normalizeTQuadratic(TQuadratic *&sourceQuadratic) {
  assert(!(sourceQuadratic->getP0() == sourceQuadratic->getP2()));
  if (sourceQuadratic->getP0() == sourceQuadratic->getP1() ||
      sourceQuadratic->getP1() == sourceQuadratic->getP2() ||
      collinear(sourceQuadratic->getP0(), sourceQuadratic->getP1(),
                sourceQuadratic->getP2())) {
    TPointD middleThickPoint(
        (sourceQuadratic->getP0() + sourceQuadratic->getP2()) * 0.5);
    sourceQuadratic->setP1(middleThickPoint);
  }
}

//-------------------------------------------------------------------

inline void getBoundaryPoints(const TPointD &P0, const TPointD &P1,
                              const TThickPoint &center, TPointD &fwdPoint,
                              TPointD &rwdPoint) {
  double thickness                          = center.thick;
  if (thickness < thicknessLimit) thickness = thicknessLimit;
  //	if(P1.y == P0.y)
  if (fabs(P1.y - P0.y) <= 1e-12) {
    if (P1.x - P0.x > 0) {
      fwdPoint.x = center.x;
      fwdPoint.y = center.y - thickness;
      rwdPoint.x = center.x;
      rwdPoint.y = center.y + thickness;
    } else if (P1.x - P0.x < 0) {
      fwdPoint.x = center.x;
      fwdPoint.y = center.y + thickness;
      rwdPoint.x = center.x;
      rwdPoint.y = center.y - thickness;
    } else
      assert(false);
  } else {
    double m = -(P1.x - P0.x) / (P1.y - P0.y);

    fwdPoint.x = center.x + (thickness) / sqrt(1 + m * m);
    fwdPoint.y = center.y + m * (fwdPoint.x - center.x);

    rwdPoint.x = center.x - (thickness) / sqrt(1 + m * m);
    rwdPoint.y = center.y + m * (rwdPoint.x - center.x);

    assert(!collinear(P0, P1, rwdPoint));

    if (left(P0, P1, rwdPoint))
      return;
    else {
      TPointD temp = fwdPoint;
      fwdPoint     = rwdPoint;
      rwdPoint     = temp;
    }
  }
}

//-------------------------------------------------------------------

inline void getAverageBoundaryPoints(const TPointD &P0,
                                     const TThickPoint &center,
                                     const TPointD &P2, TPointD &fwdPoint,
                                     TPointD &rwdPoint) {
  TPointD fwdP0, fwdP2;
  TPointD rwdP0, rwdP2;

  getBoundaryPoints(P0, center, center, fwdP0, rwdP0);
  getBoundaryPoints(center, P2, center, fwdP2, rwdP2);

  double thickness                          = center.thick;
  if (thickness < thicknessLimit) thickness = thicknessLimit;
  if (fwdP0.x + fwdP2.x == rwdP0.x + rwdP2.x) {
    if ((fwdP0.y + fwdP2.y) - (rwdP0.y + rwdP2.y) > 0) {
      fwdPoint.x = center.x;
      fwdPoint.y = center.y + thickness;
      rwdPoint.x = center.x;
      rwdPoint.y = center.y - thickness;
    } else if ((fwdP0.y + fwdP2.y) - (rwdP0.y + rwdP2.y) < 0) {
      fwdPoint.x = center.x;
      fwdPoint.y = center.y - thickness;
      rwdPoint.x = center.x;
      rwdPoint.y = center.y + thickness;
    } else
      assert(false);
  } else {
    double m = ((fwdP0.y + fwdP2.y) - (rwdP0.y + rwdP2.y)) /
               ((fwdP0.x + fwdP2.x) - (rwdP0.x + rwdP2.x));

    fwdPoint.x = center.x + (thickness) / sqrt(1 + m * m);
    fwdPoint.y = center.y + m * (fwdPoint.x - center.x);

    rwdPoint.x = center.x - (thickness) / sqrt(1 + m * m);
    rwdPoint.y = center.y + m * (rwdPoint.x - center.x);

    if (right(P0, center, rwdPoint)) {
      TPointD temp = fwdPoint;
      fwdPoint     = rwdPoint;
      rwdPoint     = temp;
    }
  }
}

//-------------------------------------------------------------------

inline void linkQuadraticList(LinkedQuadraticList &inputBoundaries) {
  LinkedQuadraticList::iterator it_curr, it_prev, it_next, it_last;
  it_last = inputBoundaries.end();
  it_last--;

  it_curr = inputBoundaries.begin();
  it_next = it_curr;
  it_next++;
  it_curr->prev = &(*it_last);
  it_curr->next = &(*it_next);

  it_curr++;
  it_prev = inputBoundaries.begin();
  it_next++;
  while (it_curr != it_last) {
    it_curr->prev = &(*it_prev);
    it_curr->next = &(*it_next);
    it_curr++;
    it_prev++;
    it_next++;
  }
  it_curr->prev = &(*it_prev);
  it_curr->next = &(*inputBoundaries.begin());
}

//-------------------------------------------------------------------

inline void computeInputBoundaries(LinkedQuadraticList &inputBoundaries) {
  set<LinkedQuadratic *> intersectionWindow;
  map<LinkedQuadratic *, vector<double>> intersectedQuadratics;
  LinkedQuadraticList intersectionBoundary;

  // detect adjacent quadratics intersections
  processAdjacentQuadratics(inputBoundaries);

  // detect Intersections
  LinkedQuadraticList::iterator it;
  it = inputBoundaries.begin();
  while (it != inputBoundaries.end()) {
    assert(!(it->getP0() == it->getP2()));
    refreshIntersectionWindow(&*it, intersectionWindow);
    findIntersections(&*it, intersectionWindow, intersectedQuadratics);
    intersectionWindow.insert(&*it);
    ++it;
  }

  /*	map<LinkedQuadratic*, vector<double> >::iterator it1 =
  intersectedQuadratics.begin();
  while(it1 != intersectedQuadratics.end())
  {
          _RPT2(	_CRT_WARN,
                  "\nP0( %f, %f )\n",
                  it1->first->getP0().x,
                  it1->first->getP0().y);
          _RPT2(	_CRT_WARN,
                  "\nP1( %f, %f )\n",
                  it1->first->getP1().x,
                  it1->first->getP1().y);
          _RPT2(	_CRT_WARN,
                  "\nP2( %f, %f )\n",
                  it1->first->getP2().x,
                  it1->first->getP2().y);

          ++it1;
  }*/

  // segmentate curves
  map<LinkedQuadratic *, vector<double>>::iterator it_intersectedQuadratics =
      intersectedQuadratics.begin();
  while (it_intersectedQuadratics != intersectedQuadratics.end()) {
    segmentate(intersectionBoundary, it_intersectedQuadratics->first,
               it_intersectedQuadratics->second);
    inputBoundaries.remove(*it_intersectedQuadratics->first);
    ++it_intersectedQuadratics;
  }

  // process intersections
  processIntersections(intersectionBoundary);

  inputBoundaries.sort(CompareLinkedQuadratics());
  intersectionBoundary.sort(CompareLinkedQuadratics());
  inputBoundaries.merge(intersectionBoundary, CompareLinkedQuadratics());
}

//-------------------------------------------------------------------

inline void processAdjacentQuadratics(LinkedQuadraticList &inputBoundaries) {
  LinkedQuadratic *start = &inputBoundaries.front();
  LinkedQuadratic *curr  = start;
  do {
    vector<DoublePair> intersections;

    LinkedQuadratic *next, *temp;
    next = curr->next;

    //		assert(curr->getP2() == next->getP0());
    if (curr->getP0() == curr->getP2()) {
      (curr->prev)->next = curr->next;
      (curr->next)->prev = curr->prev;
      temp               = curr->prev;
      inputBoundaries.remove(*curr);
      curr = temp;
    } else if (curr->getP0() == next->getP2()) {
      (curr->prev)->next = next->next;
      (next->next)->prev = curr->prev;
      temp               = curr->prev;
      inputBoundaries.remove(*curr);
      inputBoundaries.remove(*next);
      curr = temp;
    } else if ((curr->getP0() == next->getP0()) &&
               (curr->getP1() == next->getP1()) &&
               (curr->getP2() == next->getP2())) {
      assert(false);
      (curr)->next       = next->next;
      (next->next)->prev = curr;
      inputBoundaries.remove(*next);
    } else if (intersect(*curr, *next, intersections) > 1) {
      double currSplit = 1, nextSplit = 0;
      for (unsigned int i = 0; i < intersections.size(); ++i) {
        if (currSplit > intersections[i].first)
          currSplit = intersections[i].first;
        if (nextSplit < intersections[i].second)
          nextSplit = intersections[i].second;
      }
      if (currSplit < one && nextSplit > zero) {
        TQuadratic firstSplit, secondSplit;

        curr->split(currSplit, firstSplit, secondSplit);
        curr->setP1(firstSplit.getP1());
        curr->setP2(firstSplit.getP2());

        next->split(nextSplit, firstSplit, secondSplit);
        next->setP0(secondSplit.getP0());
        next->setP1(secondSplit.getP1());
      }
    }
    intersections.clear();
    curr = curr->next;
  } while (curr != start);
}
//-------------------------------------------------------------------

inline void findIntersections(
    LinkedQuadratic *quadratic, set<LinkedQuadratic *> &intersectionWindow,
    map<LinkedQuadratic *, vector<double>> &intersectedQuadratics) {
  set<LinkedQuadratic *>::iterator it = intersectionWindow.begin();
  while (it != intersectionWindow.end()) {
    vector<DoublePair> intersections;

    if ((quadratic->getP0() == (*it)->getP2()) &&
        (quadratic->getP1() == (*it)->getP1()) &&
        (quadratic->getP2() == (*it)->getP0()))
      assert(false);
    else if ((quadratic->getP0() == (*it)->getP0()) &&
             (quadratic->getP1() == (*it)->getP1()) &&
             (quadratic->getP2() == (*it)->getP2()))
      assert(false);
    else if (quadratic->prev == *it) {
    } else if (quadratic->next == *it) {
    } else if (intersect(*quadratic, *(*it), intersections)) {
      for (unsigned int i = 0; i < intersections.size(); ++i) {
        intersectedQuadratics[quadratic].push_back(intersections[i].first);
        intersectedQuadratics[*it].push_back(intersections[i].second);
      }
    }
    intersections.clear();
    ++it;
  }
}

inline void refreshIntersectionWindow(
    LinkedQuadratic *quadratic, set<LinkedQuadratic *> &intersectionWindow) {
  set<LinkedQuadratic *>::iterator it = intersectionWindow.begin();
  while (it != intersectionWindow.end()) {
    if ((*it)->getBBox().y0 > quadratic->getBBox().y1) {
      set<LinkedQuadratic *>::iterator erase_it;
      erase_it = it;
      ++it;
      intersectionWindow.erase(erase_it);
    } else
      ++it;
  }
}

//-------------------------------------------------------------------

inline void segmentate(LinkedQuadraticList &intersectionBoundary,
                       LinkedQuadratic *quadratic,
                       vector<double> &splitPoints) {
  for (unsigned int k = 0; k < splitPoints.size(); k++) {
    /*		_RPT1(	_CRT_WARN,
            "\n%f\n",
            splitPoints[k]);*/
    if (splitPoints[k] > 1) {
      splitPoints[k] = 1;
    } else if (splitPoints[k] < 0) {
      splitPoints[k] = 0;
    }
  }

  sort(splitPoints.begin(), splitPoints.end());
  vector<double>::iterator it_duplicates =
      unique(splitPoints.begin(), splitPoints.end());
  splitPoints.erase(it_duplicates, splitPoints.end());

  vector<TQuadratic *> segments;
  split<TQuadratic>(*quadratic, splitPoints, segments);

  LinkedQuadratic *prevQuadratic = quadratic->prev;

  vector<TQuadratic *>::iterator it = segments.begin();
  while (it != segments.end()) {
    if (!((*it)->getP0() == (*it)->getP2())) {
      TQuadratic quad = *(*it);
      normalizeTQuadratic(*it);
      quad = *(*it);
      intersectionBoundary.push_back(*(*it));
      prevQuadratic->next              = &intersectionBoundary.back();
      intersectionBoundary.back().prev = prevQuadratic;
      prevQuadratic                    = &intersectionBoundary.back();
    }
    delete (*it);
    ++it;
  }

  prevQuadratic->next = quadratic->next;

  if (quadratic->next) quadratic->next->prev = prevQuadratic;
}

//-------------------------------------------------------------------

inline void processIntersections(LinkedQuadraticList &intersectionBoundary) {
  vector<pair<LinkedQuadratic *, Direction>> crossing;

  LinkedQuadraticList::iterator it1, it2;

  it1 = intersectionBoundary.begin();
  while (it1 != intersectionBoundary.end()) {
    TPointD intersectionPoint = it1->getP0();
    crossing.push_back(pair<LinkedQuadratic *, Direction>(&(*it1), outward));

    it2 = intersectionBoundary.begin();
    while (it2 != intersectionBoundary.end()) {
      if (it1 != it2) {
        if (it2->getP0() == intersectionPoint) {
          crossing.push_back(
              pair<LinkedQuadratic *, Direction>(&(*it2), outward));
        }
        if (it2->getP2() == intersectionPoint) {
          crossing.push_back(
              pair<LinkedQuadratic *, Direction>(&(*it2), inward));
        }
      }
      ++it2;
    }

    unsigned int branchNum = crossing.size();
    if (branchNum > 4) {
      if (crossing[0].second == inward)
        nonSimpleCrossing.insert(crossing[0].first->getP2());
      else if (crossing[0].second == outward)
        nonSimpleCrossing.insert(crossing[0].first->getP0());
      else
        assert(false);
    } else if (branchNum > 2 && branchNum <= 4) {
      if (!isSmallStroke) processNonSimpleLoops(intersectionPoint, crossing);
      assert(crossing.size() != 1);

      if (crossing[0].second == inward)
        simpleCrossing.insert(crossing[0].first->getP2());
      else if (crossing[0].second == outward)
        simpleCrossing.insert(crossing[0].first->getP0());
      else
        assert(false);

      if (crossing.size() > 2) {
        sort(crossing.begin(), crossing.end(), CompareBranches());

        /*				_RPT0(	_CRT_WARN,
"\n__________________________________________________\n");
        for(unsigned int j=0;j<crossing.size();++j)
        {
                if(crossing[j].second == inward)
                        _RPT2(	_CRT_WARN,
                        "\ninward P( %f, %f )\n",
                        crossing[j].first->getP1().x -
crossing[j].first->getP2().x,
                        crossing[j].first->getP1().y -
crossing[j].first->getP2().y);
                else if(crossing[j].second == outward)
                        _RPT2(	_CRT_WARN,
                        "\noutward P( %f, %f )\n",
                        crossing[j].first->getP1().x -
crossing[j].first->getP0().x,
                        crossing[j].first->getP1().y -
crossing[j].first->getP0().y);
                else assert(false);
        }*/

        vector<pair<LinkedQuadratic *, Direction>>::iterator it, it_prev,
            it_next, it_nextnext, it_prevprev;
        it = crossing.begin();
        while (it != crossing.end()) {
          if (it->second == outward) {
            it_next                                = it + 1;
            if (it_next == crossing.end()) it_next = crossing.begin();
            while (((it->first)->getP0() == (it_next->first)->getP2() &&
                    (it->first)->getP2() == (it_next->first)->getP0() &&
                    (it->first)->getP1() == (it_next->first)->getP1()) ||
                   ((it->first)->getP0() == (it_next->first)->getP0() &&
                    (it->first)->getP2() == (it_next->first)->getP2() &&
                    (it->first)->getP1() == (it_next->first)->getP1())) {
              it_next                                = it_next + 1;
              if (it_next == crossing.end()) it_next = crossing.begin();
            }
            it_nextnext                                    = it_next + 1;
            if (it_nextnext == crossing.end()) it_nextnext = crossing.begin();
            if (((it_nextnext->first)->getP0() == (it_next->first)->getP2() &&
                 (it_nextnext->first)->getP2() == (it_next->first)->getP0() &&
                 (it_nextnext->first)->getP1() == (it_next->first)->getP1()) ||
                ((it_nextnext->first)->getP0() == (it_next->first)->getP0() &&
                 (it_nextnext->first)->getP2() == (it_next->first)->getP2() &&
                 (it_nextnext->first)->getP1() == (it_next->first)->getP1())) {
              if (it_nextnext->second == outward ||
                  it_nextnext->second == deletedOutward) {
                it->first->prev = 0;
                it->second      = deletedOutward;
              }
            }
            if (it_next->second == outward ||
                it_next->second == deletedOutward) {
              it->first->prev = 0;
              it->second      = deletedOutward;
            }
          } else  //(it->second == inward)
          {
            if (it == crossing.begin())
              it_prev = crossing.end() - 1;
            else
              it_prev = it - 1;
            while (((it->first)->getP0() == (it_prev->first)->getP2() &&
                    (it->first)->getP2() == (it_prev->first)->getP0() &&
                    (it->first)->getP1() == (it_prev->first)->getP1()) ||
                   ((it->first)->getP0() == (it_prev->first)->getP0() &&
                    (it->first)->getP2() == (it_prev->first)->getP2() &&
                    (it->first)->getP1() == (it_prev->first)->getP1())) {
              if (it_prev == crossing.begin())
                it_prev = crossing.end() - 1;
              else
                it_prev = it_prev - 1;
            }
            if (it_prev == crossing.begin())
              it_prevprev = crossing.end() - 1;
            else
              it_prevprev = it_prev - 1;
            if (((it_prevprev->first)->getP0() == (it_prev->first)->getP2() &&
                 (it_prevprev->first)->getP2() == (it_prev->first)->getP0() &&
                 (it_prevprev->first)->getP1() == (it_prev->first)->getP1()) ||
                ((it_prevprev->first)->getP0() == (it_prev->first)->getP0() &&
                 (it_prevprev->first)->getP2() == (it_prev->first)->getP2() &&
                 (it_prevprev->first)->getP1() == (it_prev->first)->getP1())) {
              if (it_prevprev->second == inward ||
                  it_prevprev->second == deletedInward) {
                it->first->next = 0;
                it->second      = deletedInward;
              }
            }
            if (it_prev->second == inward || it_prev->second == deletedInward) {
              it->first->next = 0;
              it->second      = deletedInward;
            }
          }
          ++it;
        }

        it = crossing.begin();
        while (it != crossing.end()) {
          if (it->second == deletedOutward || it->second == deletedInward)
            it = crossing.erase(it);
          else
            ++it;
        }
      }

      assert(crossing.size() > 0 && crossing.size() <= 4);
      if (crossing.size() == 0) {
      } else if (crossing.size() == 2) {
        if (crossing[0].second == inward) {
          assert(crossing[1].second == outward);
          crossing[0].first->next = crossing[1].first;
          crossing[1].first->prev = crossing[0].first;
        } else  // if(crossing[0].second == outward)
        {
          assert(crossing[1].second == inward);
          crossing[0].first->prev = crossing[1].first;
          crossing[1].first->next = crossing[0].first;
        }
      }
    }
    crossing.clear();
    ++it1;
  }
}

//-------------------------------------------------------------------

bool processNonSimpleLoops(
    TPointD &intersectionPoint,
    vector<pair<LinkedQuadratic *, Direction>> &crossing) {
  vector<pair<LinkedQuadratic *, Direction>>::iterator it, last;
  it = crossing.begin();
  while (it != crossing.end()) {
    if (it->second == outward || it->second == deletedOutward) {
      LinkedQuadratic *loopStart = it->first;
      LinkedQuadratic *loopCurr  = loopStart;
      for (int i = 0; i < nonSimpleLoopsMaxSize; ++i) {
        if (loopCurr->getP2() == intersectionPoint) {
          loopStart->prev = 0;
          crossing.erase(it);
          loopCurr->next = 0;
          last           = remove(crossing.begin(), crossing.end(),
                        pair<LinkedQuadratic *, Direction>(loopCurr, inward));
          crossing.erase(last, crossing.end());
          return true;
          break;
        }
        if (!loopCurr->next) break;
        double distance = norm2(loopCurr->getP0() - loopCurr->next->getP2());
        if (distance > nonSimpleLoopsMaxDistance) break;
        loopCurr = loopCurr->next;
      }
    }
    ++it;
  }
  return false;
}

//-------------------------------------------------------------------

inline bool deleteUnlinkedLoops(LinkedQuadraticList &inputBoundaries) {
  LinkedQuadratic *current, *temp;

  LinkedQuadraticList::iterator it = inputBoundaries.begin();
  while (it != inputBoundaries.end()) {
    //		bool isNonSimpleBranch;
    int count;
    if (it->prev == 0) {
      //			if( nonSimpleCrossing.find(it->getP0()) !=
      // nonSimpleCrossing.end() )
      //				isNonSimpleBranch = true;
      //			else isNonSimpleBranch = false;
      count   = inputBoundaries.size();
      current = &(*it);
      while (current != 0) {
        assert(count > 0);
        if (count == 0) return false;
        if (nonSimpleCrossing.find(current->getP2()) != nonSimpleCrossing.end())
        //					||
        // simpleCrossing.find(current->getP2())
        //!= simpleCrossing.end() )
        {
          if (current->next) current->next->prev = 0;
          it                                     = inputBoundaries.begin();
          break;
        }

        if (&(*it) == current) ++it;
        temp = current->next;
        inputBoundaries.remove(*current);
        if (temp) {
          assert(temp->next != current);
          if (temp->next == current) {
            temp->next = 0;
            it         = inputBoundaries.begin();
            break;
          }
        }
        current = temp;
        --count;
      }
    } else if (it->next == 0) {
      //			if( nonSimpleCrossing.find(it->getP2()) !=
      // nonSimpleCrossing.end() )
      //				isNonSimpleBranch = true;
      //			else isNonSimpleBranch = false;
      count   = inputBoundaries.size();
      current = &(*it);
      while (current != 0) {
        assert(count > 0);
        if (count == 0) return false;
        if (nonSimpleCrossing.find(current->getP0()) != nonSimpleCrossing.end())
        //					||
        // simpleCrossing.find(current->getP0())
        //!= simpleCrossing.end() )
        {
          if (current->prev) current->prev->next = 0;
          it                                     = inputBoundaries.begin();
          break;
        }

        if (&(*it) == current) ++it;
        temp = current->prev;
        inputBoundaries.remove(*current);
        if (temp) {
          assert(temp->prev != current);
          if (temp->prev == current) {
            temp->prev = 0;
            it         = inputBoundaries.begin();
            break;
          }
        }
        current = temp;
        --count;
      }
    } else
      ++it;
  }
  return true;
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
#ifdef LEVO
namespace {

void computeIntersections(IntersectionData &intData,
                          const vector<TStroke *> &strokeArray);

//-------------------------------------------------------------------

void addBranch(IntersectionData &intData, list<IntersectedStroke> &strokeList,
               const vector<TStroke *> &s, int ii, double w) {
  list<IntersectedStroke>::iterator it;
  TPointD tan1, tan2;
  double crossVal;

  IntersectedStroke item(intData.m_intList.end(), strokeList.end());

  item.m_edge.m_s     = s[ii];
  item.m_edge.m_index = ii;
  item.m_edge.m_w0    = w;

  tan1 = item.m_edge.m_s->getSpeed(w);
  tan2 = ((strokeList.back().m_gettingOut) ? 1 : -1) *
         strokeList.back().m_edge.m_s->getSpeed(strokeList.back().m_edge.m_w0);

  if (strokeList.size() == 2)  // potrebbero essere orientati male; due branch
                               // possono stare come vogliono, ma col terzo no.
  {
    TPointD aux = ((strokeList.begin()->m_gettingOut) ? 1 : -1) *
                  strokeList.begin()->m_edge.m_s->getSpeed(
                      strokeList.begin()->m_edge.m_w0);
    if (cross(aux, tan2) > 0) {
      std::reverse(strokeList.begin(), strokeList.end());
      tan2 =
          ((strokeList.back().m_gettingOut) ? 1 : -1) *
          strokeList.back().m_edge.m_s->getSpeed(strokeList.back().m_edge.m_w0);
    }
  }

  double lastCross = cross(tan1, tan2);
  // UINT size = strokeList.size();

  UINT added    = 0;
  bool endPoint = (w == 0.0 || w == 1.0);

  for (it = strokeList.begin(); it != strokeList.end(); it++) {
    tan2 = (((*it).m_gettingOut) ? 1 : -1) *
           (*it).m_edge.m_s->getSpeed((*it).m_edge.m_w0);
    crossVal = cross(tan1, tan2);

    if (lastCross > 0 && crossVal < 0 && w != 1.0) {
      assert(added != 0x1);
      item.m_gettingOut = true;
      strokeList.insert(it, item);
      added |= 0x1;
      if (endPoint || added == 0x3) return;
    } else if (lastCross < 0 && crossVal > 0 && w != 0.0) {
      assert(added != 0x2);
      item.m_gettingOut = false;
      strokeList.insert(it, item);
      added |= 0x2;
      if (endPoint || added == 0x3) return;
    }
    lastCross = crossVal;
  }

  if (endPoint) {
    item.m_gettingOut = (w == 0.0);
    strokeList.push_back(item);
  } else {
    item.m_gettingOut = (crossVal >= 0);
    strokeList.push_back(item);
    item.m_gettingOut = !item.m_gettingOut;
    strokeList.push_back(item);
  }
}

//-----------------------------------------------------------------------------

void addBranches(IntersectionData &intData, Intersection &intersection,
                 const vector<TStroke *> &s, int ii, int jj,
                 DoublePair intersectionPair) {
  bool foundS1 = false, foundS2 = false;
  list<IntersectedStroke>::iterator it;

  assert(!intersection.m_strokeList.empty());

  for (it = intersection.m_strokeList.begin();
       it != intersection.m_strokeList.end(); it++) {
    if ((ii >= 0 && (*it).m_edge.m_s == s[ii])) foundS1 = true;
    if ((jj >= 0 && (*it).m_edge.m_s == s[jj])) foundS2 = true;
  }

  if (foundS1 && foundS2) return;

  if (!foundS1) {
    int size = intersection.m_strokeList.size();
    addBranch(intData, intersection.m_strokeList, s, ii,
              intersectionPair.first);
    assert(intersection.m_strokeList.size() - size > 0);
  }
  if (!foundS2) {
    int size = intersection.m_strokeList.size();
    addBranch(intData, intersection.m_strokeList, s, jj,
              intersectionPair.second);
    // intersection.m_numInter+=intersection.m_strokeList.size()-size;
    assert(intersection.m_strokeList.size() - size > 0);
  }
}

//-----------------------------------------------------------------------------

#ifdef IS_DOTNET
#define NULL_ITER list<IntersectedStroke>::iterator()
#else
#define NULL_ITER 0
#endif

//-----------------------------------------------------------------------------
Intersection makeIntersection(IntersectionData &intData,
                              const vector<TStroke *> &s, int ii, int jj,
                              DoublePair inter) {
  Intersection interList;
  IntersectedStroke item1(intData.m_intList.end(), NULL_ITER),
      item2(intData.m_intList.end(), NULL_ITER);

  interList.m_intersection = s[ii]->getPoint(inter.first);

  item1.m_edge.m_w0 = inter.first;
  item2.m_edge.m_w0 = inter.second;

  item1.m_edge.m_s     = s[ii];
  item1.m_edge.m_index = ii;

  item2.m_edge.m_s     = s[jj];
  item2.m_edge.m_index = jj;

  bool reversed = false;

  if (cross(item1.m_edge.m_s->getSpeed(inter.first),
            item2.m_edge.m_s->getSpeed(inter.second)) > 0)
    reversed = true;  // std::reverse(interList.m_strokeList.begin(),
                      // interList.m_strokeList.end());

  if (item1.m_edge.m_w0 != 1.0) {
    item1.m_gettingOut = true;
    interList.m_strokeList.push_back(item1);
  }
  if (item2.m_edge.m_w0 != (reversed ? 0.0 : 1.0)) {
    item2.m_gettingOut = !reversed;
    interList.m_strokeList.push_back(item2);
  }
  if (item1.m_edge.m_w0 != 0.0) {
    item1.m_gettingOut = false;
    interList.m_strokeList.push_back(item1);
  }
  if (item2.m_edge.m_w0 != (reversed ? 1.0 : 0.0)) {
    item2.m_gettingOut = reversed;
    interList.m_strokeList.push_back(item2);
  }

  return interList;
}

//-----------------------------------------------------------------------------

void addIntersection(IntersectionData &intData, const vector<TStroke *> &s,
                     int ii, int jj, DoublePair intersection) {
  list<Intersection>::iterator it;
  TPointD p;

  if (areAlmostEqual(intersection.first, 0.0, 1e-9))
    intersection.first = 0.0;
  else if (areAlmostEqual(intersection.first, 1.0, 1e-9))
    intersection.first = 1.0;

  if (areAlmostEqual(intersection.second, 0.0, 1e-9))
    intersection.second = 0.0;
  else if (areAlmostEqual(intersection.second, 1.0, 1e-9))
    intersection.second = 1.0;

  p = s[ii]->getPoint(intersection.first);

  for (it = intData.m_intList.begin(); it != intData.m_intList.end(); it++)
    if (areAlmostEqual((*it).m_intersection,
                       p))  // devono essere rigorosamente uguali, altrimenti
    // il calcolo dell'ordine dei rami con le tangenti sballa
    {
      if ((*it).m_intersection == p)
        addBranches(intData, *it, s, ii, jj, intersection);
      return;
    }

  intData.m_intList.push_back(
      makeIntersection(intData, s, ii, jj, intersection));
}

//-----------------------------------------------------------------------------

void findNearestIntersection(list<Intersection> &interList) {
  list<Intersection>::iterator i1;
  list<IntersectedStroke>::iterator i2;

  for (i1 = interList.begin(); i1 != interList.end(); i1++) {
    for (i2 = (*i1).m_strokeList.begin(); i2 != (*i1).m_strokeList.end();
         i2++) {
      if ((*i2).m_nextIntersection != interList.end())  // already set
        continue;

      int versus      = (i2->m_gettingOut) ? 1 : -1;
      double minDelta = (std::numeric_limits<double>::max)();
      list<Intersection>::iterator it1, it1Res;
      list<IntersectedStroke>::iterator it2, it2Res;

      for (it1 = i1; it1 != interList.end(); ++it1) {
        if (it1 == i1)
          it2 = i2, it2++;
        else
          it2 = (*it1).m_strokeList.begin();

        for (; it2 != (*it1).m_strokeList.end(); ++it2) {
          if ((*it2).m_edge.m_index == i2->m_edge.m_index &&
              (*it2).m_gettingOut == !i2->m_gettingOut) {
            double delta = versus * (it2->m_edge.m_w0 - i2->m_edge.m_w0);

            if (delta > 0 && delta < minDelta) {
              it1Res   = it1;
              it2Res   = it2;
              minDelta = delta;
            }
          }
        }
      }

      if (minDelta != (std::numeric_limits<double>::max)()) {
        (*it2Res).m_nextIntersection = i1;
        (*it2Res).m_nextStroke       = i2;
        (*it2Res).m_edge.m_w1        = i2->m_edge.m_w0;
        (*i2).m_nextIntersection     = it1Res;
        (*i2).m_nextStroke           = it2Res;
        (*i2).m_edge.m_w1            = it2Res->m_edge.m_w0;
        i1->m_numInter++;
        it1Res->m_numInter++;
      }
    }
  }
}
//-----------------------------------------------------------------------------

int myIntersect(const TStroke *s1, const TStroke *s2,
                std::vector<DoublePair> &intersections) {
  int k = 0;
  assert(s1 != s2);
  intersections.clear();

  for (int i = 0; i < s1->getChunkCount(); i++)
    for (int j = 0; j < s2->getChunkCount(); j++) {
      const TQuadratic *q1 = s1->getChunk(i);
      const TQuadratic *q2 = s2->getChunk(j);
      if (!q1->getBBox().overlaps(q2->getBBox())) continue;
      if (intersect(*q1, *q2, intersections) > k)
        while (k < (int)intersections.size()) {
          intersections[k].first =
              getWfromChunkAndT(s1, i, intersections[k].first);
          intersections[k].second =
              getWfromChunkAndT(s2, j, intersections[k].second);
          k++;
        }
    }
  return k;
}

//-----------------------------------------------------------------------------

void computeIntersections(IntersectionData &intData,
                          const vector<TStroke *> &strokeArray) {
  int i, j;

  assert(intData.m_intersectedStrokeArray.empty());

  list<Intersection>::iterator it1;
  list<IntersectedStroke>::iterator it2;

  for (i = 0; i < (int)strokeArray.size(); i++) {
    TStroke *s1 = strokeArray[i];
    addIntersection(intData, strokeArray, i, i,
                    DoublePair(0, 1));  // le stroke sono sicuramente selfloop!
    for (j = i + 1; j < (int)strokeArray.size(); j++) {
      TStroke *s2 = strokeArray[j];
      vector<DoublePair> intersections;
      if (s1->getBBox().overlaps(s2->getBBox()) &&
          myIntersect(s1, s2, intersections))
        for (int k = 0; k < (int)intersections.size(); k++)
          addIntersection(intData, strokeArray, i, j, intersections[k]);
    }
  }

  // la struttura delle intersezioni viene poi visitata per trovare
  // i link tra un'intersezione e la successiva

  findNearestIntersection(intData.m_intList);

  // for (it1=intData.m_intList.begin(); it1!=intData.m_intList.end();) //la
  // faccio qui, e non nella eraseIntersection. vedi commento li'.
  // eraseDeadIntersections(intData.m_intList);

  // for (it1=intData.m_intList.begin(); it1!=intData.m_intList.end(); it1++)
  //   markDeadIntersections(intData.m_intList, it1);

  // checkInterList(intData.m_intList);
}

//-----------------------------------------------------------------------------

TRegion *findRegion(list<Intersection> &intList,
                    list<Intersection>::iterator it1,
                    list<IntersectedStroke>::iterator it2);

bool isValidArea(const TRegion &r);

//-----------------------------------------------------------------------------

}  // namespace

#endif

//-----------------------------------------------------------------------------

bool computeSweepBoundary(const vector<TStroke *> &strokes,
                          vector<vector<TQuadratic *>> &outlines) {
  if (strokes.empty()) return false;
  // if(!outlines.empty()) return false;
  vector<TStroke *> sweepStrokes;

  UINT i = 0;
  for (i = 0; i < strokes.size(); i++)
    computeBoundaryStroke(*strokes[i], sweepStrokes);

  /*if (Count)
return true;
  Count++;*/

  // ofstream of("c:\\temp\\boh.txt");

  for (i = 0; i < sweepStrokes.size(); i++) {
    // of<<"****sweepstroke #"<<i<<"*****"<<endl;
    outlines.push_back(vector<TQuadratic *>());
    vector<TQuadratic *> &q = outlines.back();
    for (int j = 0; j < sweepStrokes[i]->getChunkCount(); j++) {
      const TThickQuadratic *q0 = sweepStrokes[i]->getChunk(j);
      // of<<"q"<<j<<": "<<q0->getP0().x<<", "<<q0->getP0().y<<endl;
      // of<<"     "<<     q0->getP1().x<<", "<<q0->getP1().y<<endl;
      // of<<"     "<<     q0->getP2().x<<", "<<q0->getP2().y<<endl;

      q.push_back(new TQuadratic(*q0));
    }
  }

  // return true;

  // computeRegions(sweepStrokes, outlines);
  clearPointerContainer(sweepStrokes);

  return true;
}

//-----------------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
//-------------------------------------------------------------------
#ifdef LEVO
namespace {

TRegion *findRegion(list<Intersection> &intList,
                    list<Intersection>::iterator it1,
                    list<IntersectedStroke>::iterator it2) {
  TRegion *r = new TRegion();
  // int currStyle=0;

  list<IntersectedStroke>::iterator itStart = it2;

  list<Intersection>::iterator nextIt1;
  list<IntersectedStroke>::iterator nextIt2;

  while (!(*it2).m_visited) {
    (*it2).m_visited = true;
    if ((*it2).m_edge.m_w0 >= (*it2).m_edge.m_w1) {
      delete r;
      return 0;
    }

    do {
      it2++;
      if (it2 == ((*it1).m_strokeList.end()))  // la lista e' circolare
        it2 = (*it1).m_strokeList.begin();
    } while (it2->m_nextIntersection == intList.end());

    nextIt1 = (*it2).m_nextIntersection;
    nextIt2 = (*it2).m_nextStroke;

    r->addEdge(&(*it2).m_edge);

    if (nextIt2 == itStart) return r;

    it1 = nextIt1;
    it2 = nextIt2;
  }

  delete r;
  return 0;
}

//-----------------------------------------------------------------------------
bool isValidArea(const TRegion &r) {
  int size = r.getEdgeCount();

  if (size == 0) return false;

  for (int i = 0; i < size; i++) {
    TEdge *e = r.getEdge(i);
    if (e->m_w0 < e->m_w1) return false;
  }
  return true;
}
}

#endif
