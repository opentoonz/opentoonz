

#include "tinbetween.h"
#include "tcurves.h"
#include "tstroke.h"
#include "tpalette.h"
//#include "tcolorstyles.h"
//#include "tregion.h"
#include "tmathutil.h"
//#include "tstrokeutil.h"
//#include "tsystem.h"
#include <utility>
#include <limits>
#include <list>

#include "../tvectorimage/tvectorimageP.h"

//#include "tdebugmessage.h"
//--------------------------------------------------------------------------------------

static double average(std::vector<double> &values, double range = 2.5) {
  UINT size = values.size();
  if (size == 0) return std::numeric_limits<double>::signaling_NaN();

  if (size == 1) return values[0];

  double sum = 0;
  UINT j     = 0;
  for (; j < size; j++) {
    sum += values[j];
  }

  double average = sum / size;

  double variance = 0;
  for (j = 0; j < size; j++) {
    variance += (average - values[j]) * (average - values[j]);
  }
  variance /= size;

  double err;
  int acceptedNum = 0;
  sum             = 0;
  for (j = 0; j < size; j++) {
    err = values[j] - average;
    err *= err;
    if (err <= range * variance) {
      sum += values[j];
      acceptedNum++;
    }
  }

  assert(acceptedNum > 0);
  return (acceptedNum > 0) ? sum / (double)acceptedNum : average;
}

//--------------------------------------------------------------------------------------

static double weightedAverage(std::vector<double> &values,
                              std::vector<double> &weights,
                              double range = 2.5) {
  UINT size = values.size();
  if (size == 0) return std::numeric_limits<double>::signaling_NaN();

  double sum         = 0;
  double totalWeight = 0;
  UINT j             = 0;
  for (; j < size; j++) {
    sum += weights[j] * values[j];
    totalWeight += weights[j];
  }

  assert(totalWeight > 0);
  if (totalWeight == 0) return std::numeric_limits<double>::signaling_NaN();

  double average = sum / totalWeight;

  double variance = 0;
  for (j = 0; j < size; j++) {
    variance += weights[j] * (average - values[j]) * (average - values[j]);
  }
  variance /= totalWeight;

  double err;
  totalWeight = 0;
  sum         = 0;
  for (j = 0; j < size; j++) {
    err = values[j] - average;
    err *= err;
    if (err <= range * variance) {
      sum += weights[j] * values[j];
      totalWeight += weights[j];
    }
  }

  assert(totalWeight > 0);
  return (totalWeight != 0) ? sum / totalWeight : average;
}

//--------------------------------------------------------------------------------------

inline UINT angleNumber(const std::vector<std::pair<int, double>> &corners,
                        double angle) {
  UINT count = 0;
  UINT size  = corners.size();
  for (UINT j = 0; j < size; j++)
    if (corners[j].second >= angle) count++;

  return count;
}

//--------------------------------------------------------------------------------------

inline bool isTooComplex(UINT n1, UINT n2, UINT maxSubsetNumber = 100) {
  UINT n, r;
  if (n1 > n2) {
    n = n1;
    r = n2;
  } else if (n1 < n2) {
    n = n2;
    r = n1;
  } else {
    assert(!"equal angle number");
    return false;
  }

  if (n - r < r) r = n - r;

  /*

n*(n-1)* ...(n-r+1) < n^r that must be <= 2^(num bits of UINT)-1

s:=sizeof(UINT)*8

if
n <= 2^( (s-1)/r)   =>

log n <= (s-1)/r    (the base of log is 2)  =>

r*log n <= s-1      =>

log n^r <= log 2^(s-1)   =>

n^r <= 2^(s-1) <  (2^s)-1

*/

  const UINT one = 1;
  if (n > (one << ((sizeof(UINT) * 8 - 1) / r))) return true;

  UINT product1 = n;  // product1 = n*(n-1)* ...(n-r+1)
  for (UINT i = 1; i < r; i++) product1 *= --n;

  UINT rFact = r;

  while (r > 1) rFact *= --r;

  return (product1 / rFact > maxSubsetNumber);
  // product1/rFact is number of combination
  //  ( n )
  //  ( r )
}

//--------------------------------------------------------------------------------------

static void eraseSmallAngles(std::vector<std::pair<int, double>> &corners,
                             double angle) {
  std::vector<std::pair<int, double>>::iterator it = corners.begin();

  while (it != corners.end()) {
    if ((*it).second < angle)
      it = corners.erase(it);
    else
      ++it;
  }
}

//--------------------------------------------------------------------------------------
// output:
// min is the minimum angle greater or equal to minDegree (i.e the minimum angle
// of the corners)
// max is the maximum angle greater or equal to minDegree

static void detectCorners(const TStroke *stroke, double minDegree,
                          std::vector<std::pair<int, double>> &corners,
                          double &min, double &max) {
  const double minSin = fabs(sin(minDegree * M_PI_180));
  double angle, vectorialProduct, metaCornerLen, partialLen;

  UINT quadCount1 = stroke->getChunkCount();
  TPointD speed1, speed2;
  int j;

  TPointD tan1, tan2;
  min = 180;
  max = minDegree;
  for (j = 1; j < (int)quadCount1; j++) {
    speed1 = stroke->getChunk(j - 1)->getSpeed(1);
    speed2 = stroke->getChunk(j)->getSpeed(0);
    if (!(speed1 == TPointD() || speed2 == TPointD())) {
      tan1 = normalize(speed1);
      tan2 = normalize(speed2);

      vectorialProduct = fabs(cross(tan1, tan2));

      if (tan1 * tan2 < 0) {
        angle = 180 - asin(tcrop(vectorialProduct, -1.0, 1.0)) * M_180_PI;
        corners.push_back(std::make_pair(j, angle));

        //------------------------------------------
        //        TDebugMessage::getStream()<<j<<" real angle";
        //        TDebugMessage::flush();
        //------------------------------------------

        if (min > angle) min = angle;
        if (max < angle) max = angle;
      } else if (vectorialProduct >= minSin) {
        angle = asin(tcrop(vectorialProduct, -1.0, 1.0)) * M_180_PI;
        corners.push_back(std::make_pair(j, angle));

        //------------------------------------------
        //        TDebugMessage::getStream()<<j<<" real angle";
        //        TDebugMessage::flush();
        //------------------------------------------

        if (min > angle) min = angle;
        if (max < angle) max = angle;
      }
    }
  }

  const double ratioLen                            = 2.5;
  const double ratioAngle                          = 0.2;
  std::vector<std::pair<int, double>>::iterator it = corners.begin();

  for (j = 1; j < (int)quadCount1;
       j++)  //"meta angoli" ( meta perche' derivabili)
  {
    if (it != corners.end() && j == (*it).first) {
      ++it;
      continue;
    }

    if (j - 2 >= 0 &&
        (corners.empty() || it == corners.begin() ||
         j - 1 != (*(it - 1)).first) &&
        j + 1 < (int)quadCount1 &&
        (corners.empty() || it == corners.end() || j + 1 != (*it).first)) {
      speed1 = stroke->getChunk(j - 2)->getSpeed(1);
      speed2 = stroke->getChunk(j + 1)->getSpeed(0);
      if (!(speed1 == TPointD() || speed2 == TPointD())) {
        tan1             = normalize(speed1);
        tan2             = normalize(speed2);
        vectorialProduct = fabs(cross(tan1, tan2));

        if (tan1 * tan2 < 0) {
          angle = 180 - asin(tcrop(vectorialProduct, -1.0, 1.0)) * M_180_PI;

          metaCornerLen  = ratioLen * (stroke->getChunk(j - 1)->getLength() +
                                      stroke->getChunk(j)->getLength());
          partialLen     = 0;
          bool goodAngle = false;

          for (int i = j - 3;
               i >= 0 && (corners.empty() || it == corners.begin() ||
                          i + 1 != (*(it - 1)).first);
               i--) {
            tan1 = stroke->getChunk(i)->getSpeed(1);
            if (tan1 == TPointD()) continue;
            tan1 = normalize(tan1);

            tan2 = stroke->getChunk(i + 1)->getSpeed(1);
            if (tan2 == TPointD()) continue;
            tan2 = normalize(tan2);

            vectorialProduct = fabs(cross(tan1, tan2));
            double nearAngle =
                asin(tcrop(vectorialProduct, -1.0, 1.0)) * M_180_PI;
            if (tan1 * tan2 < 0) nearAngle = 180 - nearAngle;

            if (nearAngle > ratioAngle * angle) break;

            partialLen += stroke->getChunk(i)->getLength();
            if (partialLen > metaCornerLen) {
              goodAngle = true;
              break;
            }
          }
          if (goodAngle) {
            partialLen = 0;
            for (int i = j + 2; i + 1 < (int)quadCount1 &&
                                (corners.empty() || it == corners.end() ||
                                 i + 1 != (*it).first);
                 i++) {
              tan1 = stroke->getChunk(i)->getSpeed(0);
              if (tan1 == TPointD()) continue;
              tan1 = normalize(tan1);

              tan2 = stroke->getChunk(i + 1)->getSpeed(0);
              if (tan2 == TPointD()) continue;
              tan2 = normalize(tan2);

              vectorialProduct = fabs(cross(tan1, tan2));
              double nearAngle =
                  asin(tcrop(vectorialProduct, -1.0, 1.0)) * M_180_PI;
              if (tan1 * tan2 < 0) nearAngle = 180 - nearAngle;

              if (nearAngle > 0.1 * angle) break;

              partialLen += stroke->getChunk(i)->getLength();
              if (partialLen > metaCornerLen) {
                goodAngle = true;
                break;
              }
            }
          }

          if (goodAngle) {
            // l'angolo viene un po' declassato in quanto meta
            it = corners.insert(it, std::make_pair(j, angle * 0.7)) + 1;
            if (min > angle) min = angle;
            if (max < angle) max = angle;

            //            TDebugMessage::getStream()<<j<<" meta angle";
            //            TDebugMessage::flush();
          }
        }
      }
    }
  }
}

//--------------------------------------------------------------------------------------

static double variance(std::vector<double> &values) {
  UINT size = values.size();
  if (size == 0) return std::numeric_limits<double>::signaling_NaN();

  double sum = 0;
  UINT j     = 0;
  for (; j < size; j++) {
    sum += values[j];
  }

  double average = sum / size;

  double variance = 0;
  for (j = 0; j < size; j++) {
    variance += (average - values[j]) * (average - values[j]);
  }

  return variance / size;
}

//--------------------------------------------------------------------------------------

static void findBestSolution(const TStroke *stroke1, const TStroke *stroke2,
                             std::pair<int, double> *partialAngles1,
                             UINT partialAngles1Size,
                             const std::vector<std::pair<int, double>> &angles2,
                             UINT r,
                             std::list<std::pair<int, double>> &partialSolution,
                             double &bestValue, std::vector<int> &bestVector) {
  //-------------------------------------------------------------------
  if (r == partialAngles1Size) {
    UINT j;
    std::vector<std::pair<int, double>> angles1;

    std::list<std::pair<int, double>>::iterator it = partialSolution.begin();

    for (; it != partialSolution.end(); ++it) {
      angles1.push_back(*it);
    }
    for (j = 0; j < partialAngles1Size; j++) {
      angles1.push_back(partialAngles1[j]);
    }

    UINT angles1Size = angles1.size();
    std::vector<double> rationAngles(angles1Size), ratioLength(angles1Size + 1);
    std::vector<double> ratioX, ratioY;

    for (j = 0; j < angles1Size; j++) {
      rationAngles[j] = fabs(angles1[j].second - angles2[j].second) /
                        (angles1[j].second + angles2[j].second);
    }

    UINT firstQuad1 = 0;
    UINT firstQuad2 = 0;
    UINT nextQuad1, nextQuad2;

    TRectD bbox1 = stroke1->getBBox();
    TRectD bbox2 = stroke2->getBBox();

    double app, div;
    double invTotalLen1 = stroke1->getLength();
    assert(invTotalLen1 > 0);
    invTotalLen1 = 1.0 / invTotalLen1;

    double invTotalLen2 = stroke2->getLength();
    assert(invTotalLen2 > 0);
    invTotalLen2 = 1.0 / invTotalLen2;

    for (j = 0; j <= angles1Size; j++) {
      if (j < angles1Size) {
        nextQuad1 = angles1[j].first;
        nextQuad2 = angles2[j].first;
      } else {
        nextQuad1 = stroke1->getChunkCount();
        nextQuad2 = stroke2->getChunkCount();
      }

      ratioLength[j] =
          fabs(stroke1->getLengthAtControlPoint(nextQuad1 * 2) * invTotalLen1 -
               stroke2->getLengthAtControlPoint(nextQuad2 * 2) * invTotalLen2);

      TPointD p1(stroke1->getChunk(nextQuad1 - 1)->getP2() -
                 stroke1->getChunk(firstQuad1)->getP0());
      p1.x = fabs(p1.x);
      p1.y = fabs(p1.y);

      TPointD p2(stroke2->getChunk(nextQuad2 - 1)->getP2() -
                 stroke2->getChunk(firstQuad2)->getP0());
      p2.x = fabs(p2.x);
      p2.y = fabs(p2.y);

      app = fabs(bbox1.getLx() * p2.x - bbox2.getLx() * p1.x);
      div = (bbox1.getLx() * p2.x + bbox2.getLx() * p1.x);
      if (div) ratioX.push_back(app / div);

      app = fabs(bbox1.getLy() * p2.y - bbox2.getLy() * p1.y);
      div = (bbox1.getLy() * p2.y + bbox2.getLy() * p1.y);
      if (div) ratioY.push_back(app / div);

      firstQuad1 = nextQuad1;
      firstQuad2 = nextQuad2;
    }

    double varAng, varX, varY, varLen;

    varX   = average(ratioX);
    varY   = average(ratioY);
    varLen = average(ratioLength);
    varAng = average(rationAngles);

    double estimate = varX + varY + varAng + varLen;
    /*
#ifdef _DEBUG
for(UINT dI=0; dI<angles1Size; dI++)
{
TDebugMessage::getStream() << angles1[dI].first<<"  ";
}
TDebugMessage::flush();

TDebugMessage::getStream() <<"estimate "<< estimate<<"=" ;
TDebugMessage::flush();
TDebugMessage::getStream()<<varAng <<"+" ;
TDebugMessage::flush();
TDebugMessage::getStream()<<varX <<"+" ;
TDebugMessage::flush();
TDebugMessage::getStream()<<varY<<"+";
TDebugMessage::flush();
TDebugMessage::getStream()<<varLen;
TDebugMessage::flush();

#endif
*/

    if (estimate < bestValue) {
      bestValue = estimate;
      if (bestVector.size() != angles1Size) {
        assert(!"bad size for bestVector");
        bestVector.resize(angles1Size);
      }

      for (j = 0; j < angles1Size; j++) {
        bestVector[j] = angles1[j].first;
      }
    }

    return;
  }

  //-------------------------------------------------------------------

  if (r == 1) {
    for (UINT i = 0; i < partialAngles1Size; i++) {
      findBestSolution(stroke1, stroke2, partialAngles1 + i, 1, angles2, 1,
                       partialSolution, bestValue, bestVector);
    }
    return;
  }

  partialSolution.push_back(partialAngles1[0]);
  findBestSolution(stroke1, stroke2, partialAngles1 + 1, partialAngles1Size - 1,
                   angles2, r - 1, partialSolution, bestValue, bestVector);

  partialSolution.pop_back();
  findBestSolution(stroke1, stroke2, partialAngles1 + 1, partialAngles1Size - 1,
                   angles2, r, partialSolution, bestValue, bestVector);
}

//--------------------------------------------------------------------------------------

static void findBestSolution(const TStroke *stroke1, const TStroke *stroke2,
                             std::vector<std::pair<int, double>> &angles1,
                             const std::vector<std::pair<int, double>> &angles2,
                             double &bestValue, std::vector<int> &bestVector) {
  assert(angles1.size() > angles2.size());

  std::list<std::pair<int, double>> partialSolution;

  findBestSolution(stroke1, stroke2, &(angles1[0]), angles1.size(), angles2,
                   angles2.size(), partialSolution, bestValue, bestVector);
}

//--------------------------------------------------------------------------------------

static void trivialSolution(const TStroke *stroke1, const TStroke *stroke2,
                            const std::vector<std::pair<int, double>> &angles1,
                            const std::vector<std::pair<int, double>> &angles2,
                            std::vector<int> &solution) {
  assert(angles1.size() > angles2.size());

  UINT j;
  double subStrokeRatio2;

  double invTotalLen2 = stroke2->getLength();
  assert(invTotalLen2);
  invTotalLen2 = 1.0 / invTotalLen2;

  double invTotalLen1 = stroke1->getLength();
  assert(invTotalLen1 > 0);
  invTotalLen1 = 1.0 / invTotalLen1;

  if (solution.size() != angles2.size()) {
    assert(!"bad size for solution");
    solution.resize(angles2.size());
  }

  int toBeErased = angles1.size() - angles2.size();
  UINT count     = 0;

  double diff, ratio, oldRatio = 100;

  subStrokeRatio2 =
      stroke2->getLengthAtControlPoint(angles2[count].first * 2) * invTotalLen2;

  for (j = 0; j < angles1.size() && count < solution.size(); j++) {
    if (toBeErased == 0) {
      solution[count++] = angles1[j].first;
    } else {
      ratio =
          stroke1->getLengthAtControlPoint(angles1[j].first * 2) * invTotalLen1;
      assert(ratio > 0 && ratio <= 1);

      diff = ratio - subStrokeRatio2;
      if (diff >= 0) {
        if (fabs(diff) < fabs(oldRatio - subStrokeRatio2)) {
          solution[count] = angles1[j].first;
          oldRatio        = 100;
        } else {
          assert(j > 0);
          solution[count] = angles1[j - 1].first;
        }
        count++;
        if (angles2.size() < count)
          subStrokeRatio2 =
              stroke2->getLengthAtControlPoint(angles2[count].first * 2) *
              invTotalLen2;
        else
          subStrokeRatio2 = 1;
      } else {
        toBeErased--;
        oldRatio = ratio;
      }
    }
  }
  assert(count == solution.size());
}

//--------------------------------------------------------------------------------------

static TStroke *extract(const TStroke *source, UINT firstQuad, UINT lastQuad) {
  UINT quadCount = source->getChunkCount();
  if (firstQuad >= quadCount) {
    assert(!"bad quadric index");
    firstQuad = quadCount - 1;
  }
  if (lastQuad < firstQuad) {
    assert(!"bad quadric index");
    lastQuad = firstQuad;
  }
  if (lastQuad >= quadCount) {
    assert(!"bad quadric index");
    lastQuad = quadCount - 1;
  }

  UINT cpIndex0 = firstQuad * 2;
  UINT cpIndex1 = lastQuad * 2 + 2;

  std::vector<TThickPoint> points(cpIndex1 - cpIndex0 + 1);
  UINT count = 0;
  for (UINT j = cpIndex0; j <= cpIndex1; j++) {
    points[count++] = source->getControlPoint(j);
  }

  return new TStroke(points);
}

//--------------------------------------------------------------------------------------

static void sample(const TStroke *stroke, int samplingSize,
                   std::vector<TPointD> &sampledPoint) {
  double samplingFrequency = 1.0 / (double)samplingSize;
  sampledPoint.resize(samplingSize);

  double totalLen = stroke->getLength();
  double step     = totalLen * samplingFrequency;
  double len      = 0;

  for (int p = 0; p < samplingSize - 1; p++) {
    sampledPoint[p] = stroke->getPointAtLength(len);
    len += step;
  }
  sampledPoint.back() =
      stroke->getControlPoint(stroke->getControlPointCount() - 1);
}

//--------------------------------------------------------------------------------------

class TInbetween::Imp {
public:
  //----------------------

  struct StrokeTransform {
    typedef enum { EQUAL, POINT, GENERAL } TransformationType;

    TPointD m_translate;
    TPointD m_rotationAndScaleCenter;
    double m_scaleX, m_scaleY, m_rotation;

    TransformationType m_type;

    // saved for optimization
    TAffine m_inverse;

    std::vector<int> m_firstStrokeCornerIndexes;
    std::vector<int> m_secondStrokeCornerIndexes;
  };

  //----------------------

  TVectorImageP m_firstImage, m_lastImage;

  std::vector<StrokeTransform> m_transformation;

  void computeTransformation();

  void transferColor(const TVectorImageP &destination) const;

  TVectorImageP tween(double t) const;

  Imp(const TVectorImageP firstImage, const TVectorImageP lastImage)
      : m_firstImage(firstImage), m_lastImage(lastImage) {
    computeTransformation();
  }
};
//-------------------------------------------------------------------

TInbetween::TInbetween(const TVectorImageP firstImage,
                       const TVectorImageP lastImage)
    : m_imp(new TInbetween::Imp(firstImage, lastImage)) {}

//-------------------------------------------------------------------

TInbetween::~TInbetween() {}

//-------------------------------------------------------------------

void TInbetween::Imp::computeTransformation() {
  const UINT samplingPointNumber = 10;
  const UINT bboxSamplingWeight  = samplingPointNumber / 2;

  StrokeTransform transform;
  double cs, sn, totalLen1, totalLen2, constK, constQ, constB, constD, constA;
  UINT cpCount1, cpCount2;
  TPointD stroke1Centroid, stroke2Centroid, stroke1Begin, stroke2Begin,
      stroke1End, stroke2End, versor1, versor2;
  std::vector<TPointD> samplingPoint1(samplingPointNumber),
      samplingPoint2(samplingPointNumber);
  TStroke *stroke1, *stroke2;
  std::vector<double> ratioSampling, weights, subStrokeXScaling,
      subStrokeYScaling;

  UINT strokeCount1 = m_firstImage->getStrokeCount();
  UINT strokeCount2 = m_lastImage->getStrokeCount();
  if (strokeCount1 > strokeCount2) strokeCount1 = strokeCount2;

  m_transformation.clear();
  m_transformation.reserve(strokeCount1);

  const int maxSubSetNum = (strokeCount1) ? 1000 / strokeCount1 : 1;

  UINT j, p;

  for (UINT i = 0; i < strokeCount1; i++) {
    stroke1 = m_firstImage->getStroke(i);
    stroke2 = m_lastImage->getStroke(i);

    // check if the strokes are equal
    cpCount1 = stroke1->getControlPointCount();
    cpCount2 = stroke2->getControlPointCount();

    transform.m_firstStrokeCornerIndexes.clear();
    transform.m_secondStrokeCornerIndexes.clear();
    transform.m_translate              = TPointD();
    transform.m_rotationAndScaleCenter = TPointD();
    transform.m_scaleX                 = 0;
    transform.m_scaleY                 = 0;
    transform.m_rotation               = 0;

    bool isEqual = true;

    if (cpCount1 == cpCount2) {
      for (j = 0; j < cpCount1 && isEqual; j++) {
        isEqual = (stroke1->getControlPoint(j) == stroke2->getControlPoint(j));
      }
    } else
      isEqual = false;

    if (isEqual) {
      transform.m_type = StrokeTransform::EQUAL;
    } else {
      totalLen1 = stroke1->getLength();
      totalLen2 = stroke2->getLength();

      if (totalLen1 == 0 || totalLen2 == 0) {
        if (totalLen1 == 0 && totalLen2 == 0) {
          transform.m_type = StrokeTransform::POINT;
        } else {
          transform.m_inverse = TAffine();
          transform.m_firstStrokeCornerIndexes.resize(2);
          transform.m_firstStrokeCornerIndexes[0] = 0;
          transform.m_firstStrokeCornerIndexes[1] = stroke1->getChunkCount();
          transform.m_secondStrokeCornerIndexes.resize(2);
          transform.m_secondStrokeCornerIndexes[0] = 0;
          transform.m_secondStrokeCornerIndexes[1] = stroke2->getChunkCount();
        }
      } else {
        const double startMinAngle = 30.0;
        std::vector<std::pair<int, double>> angles1, angles2;

        transform.m_type = StrokeTransform::GENERAL;

        double minAngle, maxAngle;
        int minAngle1, maxAngle1, minAngle2, maxAngle2;

        angles1.clear();
        angles2.clear();

        //        TDebugMessage::getStream()<<j<<" stroke1 corner detection";
        //        TDebugMessage::flush();

        detectCorners(stroke1, startMinAngle, angles1, minAngle, maxAngle);
        minAngle1 = (int)minAngle;
        maxAngle1 = (int)maxAngle;

        //        TDebugMessage::getStream()<<j<<" stroke2 corner detection";
        //        TDebugMessage::flush();

        detectCorners(stroke2, startMinAngle, angles2, minAngle, maxAngle);
        minAngle2 = (int)minAngle;
        maxAngle2 = (int)maxAngle;

        if (angles1.empty()) angles2.clear();
        if (angles2.empty()) angles1.clear();

        /*
debugStream.open("c:\\temp\\inbetween.txt", ios_base::out);
debugStream <<"num angoli 1: "<< angles1.size() << endl;
debugStream <<"num angoli 2: "<< angles2.size() << endl;
*/

        double bestValue = (std::numeric_limits<double>::max)();

        if (angles1.size() != angles2.size()) {
          bestValue = (std::numeric_limits<double>::max)();

          //--------------------------------------------------------------------------

          if (isTooComplex(angles1.size(), angles2.size(), maxSubSetNum)) {
            // debugStream <<"is too complex" << endl;
            int firstAngle =
                (int)((angles1.size() < angles2.size()) ? minAngle2
                                                        : minAngle1);
            int lastAngle =
                (int)((angles1.size() < angles2.size()) ? maxAngle1
                                                        : maxAngle2);
            int bestAngle = (int)startMinAngle;

            if ((int)(angles1.size() + angles2.size()) <
                lastAngle - firstAngle + 1) {
              double tempAngle;
              std::vector<double> sortedAngles1, sortedAngles2;
              sortedAngles1.reserve(angles1.size());
              sortedAngles2.reserve(angles2.size());
              for (j = 0; j < angles1.size(); j++) {
                tempAngle = angles1[j].second;
                if (tempAngle >= firstAngle && tempAngle <= lastAngle)
                  sortedAngles1.push_back(tempAngle);
              }
              for (j = 0; j < angles2.size(); j++) {
                tempAngle = angles2[j].second;
                if (tempAngle >= firstAngle && tempAngle <= lastAngle)
                  sortedAngles2.push_back(tempAngle);
              }
              std::vector<double> sortedAngles(sortedAngles1.size() +
                                               sortedAngles2.size());

              std::sort(sortedAngles1.begin(), sortedAngles1.end());
              std::sort(sortedAngles2.begin(), sortedAngles2.end());
              std::merge(sortedAngles1.begin(), sortedAngles1.end(),
                         sortedAngles2.begin(), sortedAngles2.end(),
                         sortedAngles.begin());

              for (j = 0; j < sortedAngles.size(); j++) {
                int numAng1 = angleNumber(angles1, sortedAngles[j]);
                int numAng2 = angleNumber(angles2, sortedAngles[j]);
                double val  = (numAng1 == numAng2)
                                 ? 0
                                 : fabs((float)(numAng1 - numAng2)) /
                                       (numAng1 + numAng2);
                if (val < bestValue) {
                  bestValue = val;
                  bestAngle = (int)(sortedAngles[j]);
                  if (bestValue == 0 ||
                      !isTooComplex(numAng1, numAng2, maxSubSetNum))
                    break;
                }
              }

            } else  //-----------------------------------------------------
            {
              for (int angle = firstAngle; angle <= lastAngle; angle++) {
                int numAng1 = angleNumber(angles1, angle);
                int numAng2 = angleNumber(angles2, angle);
                double val  = (numAng1 == numAng2)
                                 ? 0
                                 : fabs((float)(numAng1 - numAng2)) /
                                       (numAng1 + numAng2);
                if (val < bestValue) {
                  bestValue = val;
                  bestAngle = angle;
                  if (bestValue == 0 ||
                      !isTooComplex(numAng1, numAng2, maxSubSetNum))
                    break;
                }
              }
            }

            eraseSmallAngles(angles1, bestAngle);
            eraseSmallAngles(angles2, bestAngle);

            /*
debugStream <<"bestAngle: "<< bestAngle << endl;
debugStream <<"num angoli 1: "<< angles1.size() << endl;
debugStream <<"num angoli 2: "<< angles2.size() << endl;
*/
          }
          //--------------------------------------------------------------------------

          bestValue = (std::numeric_limits<double>::max)();

          if (angles1.size() == angles2.size()) {
            transform.m_firstStrokeCornerIndexes.push_back(0);
            for (j = 0; j < angles1.size(); j++)
              transform.m_firstStrokeCornerIndexes.push_back(angles1[j].first);
            transform.m_firstStrokeCornerIndexes.push_back(
                stroke1->getChunkCount());

            transform.m_secondStrokeCornerIndexes.push_back(0);
            for (j = 0; j < angles2.size(); j++)
              transform.m_secondStrokeCornerIndexes.push_back(angles2[j].first);
            transform.m_secondStrokeCornerIndexes.push_back(
                stroke2->getChunkCount());
          } else {
            if (isTooComplex(angles1.size(), angles2.size(), maxSubSetNum)) {
              if (angles1.size() > angles2.size()) {
                transform.m_firstStrokeCornerIndexes.resize(angles2.size());
                trivialSolution(stroke1, stroke2, angles1, angles2,
                                transform.m_firstStrokeCornerIndexes);
                transform.m_firstStrokeCornerIndexes.insert(
                    transform.m_firstStrokeCornerIndexes.begin(), 0);
                transform.m_firstStrokeCornerIndexes.push_back(
                    stroke1->getChunkCount());

                transform.m_secondStrokeCornerIndexes.push_back(0);
                for (j = 0; j < angles2.size(); j++)
                  transform.m_secondStrokeCornerIndexes.push_back(
                      angles2[j].first);
                transform.m_secondStrokeCornerIndexes.push_back(
                    stroke2->getChunkCount());
              } else {
                transform.m_firstStrokeCornerIndexes.push_back(0);
                for (j = 0; j < angles1.size(); j++)
                  transform.m_firstStrokeCornerIndexes.push_back(
                      angles1[j].first);
                transform.m_firstStrokeCornerIndexes.push_back(
                    stroke1->getChunkCount());

                transform.m_secondStrokeCornerIndexes.resize(angles1.size());
                trivialSolution(stroke2, stroke1, angles2, angles1,
                                transform.m_secondStrokeCornerIndexes);
                transform.m_secondStrokeCornerIndexes.insert(
                    transform.m_secondStrokeCornerIndexes.begin(), 0);
                transform.m_secondStrokeCornerIndexes.push_back(
                    stroke2->getChunkCount());
              }
            } else {
              if (angles1.size() > angles2.size()) {
                transform.m_firstStrokeCornerIndexes.resize(angles2.size());
                findBestSolution(stroke1, stroke2, angles1, angles2, bestValue,
                                 transform.m_firstStrokeCornerIndexes);
                transform.m_firstStrokeCornerIndexes.insert(
                    transform.m_firstStrokeCornerIndexes.begin(), 0);
                transform.m_firstStrokeCornerIndexes.push_back(
                    stroke1->getChunkCount());

                transform.m_secondStrokeCornerIndexes.push_back(0);
                for (j = 0; j < angles2.size(); j++)
                  transform.m_secondStrokeCornerIndexes.push_back(
                      angles2[j].first);
                transform.m_secondStrokeCornerIndexes.push_back(
                    stroke2->getChunkCount());
              } else {
                transform.m_firstStrokeCornerIndexes.push_back(0);
                for (j = 0; j < angles1.size(); j++)
                  transform.m_firstStrokeCornerIndexes.push_back(
                      angles1[j].first);
                transform.m_firstStrokeCornerIndexes.push_back(
                    stroke1->getChunkCount());

                transform.m_secondStrokeCornerIndexes.resize(angles1.size());
                findBestSolution(stroke2, stroke1, angles2, angles1, bestValue,
                                 transform.m_secondStrokeCornerIndexes);
                transform.m_secondStrokeCornerIndexes.insert(
                    transform.m_secondStrokeCornerIndexes.begin(), 0);
                transform.m_secondStrokeCornerIndexes.push_back(
                    stroke2->getChunkCount());
              }
            }
          }
        } else {
          transform.m_firstStrokeCornerIndexes.push_back(0);
          for (j = 0; j < angles1.size(); j++)
            transform.m_firstStrokeCornerIndexes.push_back(angles1[j].first);
          transform.m_firstStrokeCornerIndexes.push_back(
              stroke1->getChunkCount());

          transform.m_secondStrokeCornerIndexes.push_back(0);
          for (j = 0; j < angles2.size(); j++)
            transform.m_secondStrokeCornerIndexes.push_back(angles2[j].first);
          transform.m_secondStrokeCornerIndexes.push_back(
              stroke2->getChunkCount());
        }

        UINT cornerSize = transform.m_firstStrokeCornerIndexes.size();
        assert(cornerSize == transform.m_secondStrokeCornerIndexes.size());
        assert(cornerSize >= 2);

        double totalRadRotation = 0;

        TStroke *subStroke1 = 0;
        TStroke *subStroke2 = 0;

        stroke1Centroid = stroke1->getCentroid();
        stroke2Centroid = stroke2->getCentroid();

#ifdef _DEBUG
        assert(transform.m_firstStrokeCornerIndexes[0] == 0);
        assert(transform.m_secondStrokeCornerIndexes[0] == 0);
        assert(transform.m_firstStrokeCornerIndexes[cornerSize - 1] ==
               stroke1->getChunkCount());
        assert(transform.m_secondStrokeCornerIndexes[cornerSize - 1] ==
               stroke2->getChunkCount());
        for (j = 0; j < cornerSize - 1; j++) {
          assert(transform.m_firstStrokeCornerIndexes[j] <
                 transform.m_firstStrokeCornerIndexes[j + 1]);
          assert(transform.m_secondStrokeCornerIndexes[j] <
                 transform.m_secondStrokeCornerIndexes[j + 1]);

          assert(transform.m_firstStrokeCornerIndexes[j] <
                 stroke1->getChunkCount());
          assert(transform.m_secondStrokeCornerIndexes[j] <
                 stroke2->getChunkCount());
        }
#endif

        for (j = 0; j < cornerSize - 1; j++) {
          ///////////////////////////////////////// sampling

          subStroke1 = extract(stroke1, transform.m_firstStrokeCornerIndexes[j],
                               transform.m_firstStrokeCornerIndexes[j + 1] - 1);
          sample(subStroke1, samplingPointNumber, samplingPoint1);
          subStroke2 =
              extract(stroke2, transform.m_secondStrokeCornerIndexes[j],
                      transform.m_secondStrokeCornerIndexes[j + 1] - 1);
          sample(subStroke2, samplingPointNumber, samplingPoint2);

          ///////////////////////////////////////// compute Rotation

          ratioSampling.clear();
          ratioSampling.reserve(samplingPointNumber);
          weights.clear();
          weights.reserve(samplingPointNumber);

          TPointD pOld, pNew;
          // double totalW=0;

          for (p = 0; p < samplingPointNumber; p++) {
            pOld = samplingPoint1[p];
            pNew = samplingPoint2[p];
            if (pOld == stroke1Centroid) continue;
            if (pNew == stroke2Centroid) continue;
            versor1 = normalize(pOld - stroke1Centroid);
            versor2 = normalize(pNew - stroke2Centroid);
            weights.push_back(tdistance(pOld, stroke1Centroid) +
                              tdistance(pNew, stroke2Centroid));
            cs       = versor1 * versor2;
            sn       = cross(versor1, versor2);
            double v = atan2(sn, cs);
            ratioSampling.push_back(v);
          }
          delete subStroke1;
          delete subStroke2;
          subStroke1 = 0;
          subStroke2 = 0;

          double radRotation = weightedAverage(ratioSampling, weights);

          totalRadRotation += radRotation;
        }
        totalRadRotation /= (cornerSize - 1);
        transform.m_rotation = totalRadRotation * M_180_PI;

        if (isAlmostZero(transform.m_rotation, 2)) {
          transform.m_rotation = 0.0;
          totalRadRotation     = 0.0;
        }

#ifdef _DEBUG
//        TDebugMessage::getStream()<<"rotation "<< transform.m_rotation;
//        TDebugMessage::flush();
#endif

        ///////////////////////////////////////// compute Scale

        if (transform.m_rotation == 0.0) {
          subStrokeXScaling.clear();
          subStrokeXScaling.reserve(cornerSize - 1);
          subStrokeYScaling.clear();
          subStrokeYScaling.reserve(cornerSize - 1);

          for (j = 0; j < cornerSize - 1; j++) {
            ///////////////////////////////////////// sampling

            subStroke1 =
                extract(stroke1, transform.m_firstStrokeCornerIndexes[j],
                        transform.m_firstStrokeCornerIndexes[j + 1] - 1);
            sample(subStroke1, samplingPointNumber, samplingPoint1);
            subStroke2 =
                extract(stroke2, transform.m_secondStrokeCornerIndexes[j],
                        transform.m_secondStrokeCornerIndexes[j + 1] - 1);
            sample(subStroke2, samplingPointNumber, samplingPoint2);

            ///////////////////////////////////////// compute X Scale

            ratioSampling.clear();
            ratioSampling.reserve(samplingPointNumber + bboxSamplingWeight);
            double appX, appY;

            TPointD appPoint;
            double bboxXMin, bboxYMin, bboxXMax, bboxYMax;
            bboxXMin = bboxYMin = (std::numeric_limits<double>::max)();
            bboxXMax = bboxYMax = -(std::numeric_limits<double>::max)();
            int h;

            for (h = 0; h < subStroke1->getControlPointCount(); ++h) {
              appPoint = subStroke1->getControlPoint(h);
              if (appPoint.x < bboxXMin) bboxXMin = appPoint.x;
              if (appPoint.x > bboxXMax) bboxXMax = appPoint.x;
              if (appPoint.y < bboxYMin) bboxYMin = appPoint.y;
              if (appPoint.y > bboxYMax) bboxYMax = appPoint.y;
            }

            appX = bboxXMax - bboxXMin;
            appY = bboxYMax - bboxYMin;

            if (appX) {
              bboxXMin = (std::numeric_limits<double>::max)();
              bboxXMax = -(std::numeric_limits<double>::max)();
              for (h = 0; h < subStroke2->getControlPointCount(); ++h) {
                appPoint = subStroke2->getControlPoint(h);
                if (appPoint.x < bboxXMin) bboxXMin = appPoint.x;
                if (appPoint.x > bboxXMax) bboxXMax = appPoint.x;
              }
              appX = (isAlmostZero(appX, 1e-01)) ? -1
                                                 : (bboxXMax - bboxXMin) / appX;
              for (UINT tms = 0; tms < bboxSamplingWeight && appX >= 0; tms++)
                ratioSampling.push_back(appX);

              for (p = 0; p < samplingPointNumber; p++) {
                appX = fabs(samplingPoint1[p].x - stroke1Centroid.x);
                if (appX)
                  ratioSampling.push_back(
                      fabs(samplingPoint2[p].x - stroke2Centroid.x) / appX);
              }

              if (!ratioSampling.empty()) {
                subStrokeXScaling.push_back(average(ratioSampling));
              }
            }

            ///////////////////////////////////////// compute Y Scale

            ratioSampling.clear();
            ratioSampling.reserve(samplingPointNumber + bboxSamplingWeight);

            if (appY) {
              bboxYMin = (std::numeric_limits<double>::max)();
              bboxYMax = -(std::numeric_limits<double>::max)();
              for (h = 0; h < subStroke2->getControlPointCount(); ++h) {
                appPoint = subStroke2->getControlPoint(h);
                if (appPoint.y < bboxYMin) bboxYMin = appPoint.y;
                if (appPoint.y > bboxYMax) bboxYMax = appPoint.y;
              }

              appY = (isAlmostZero(appY, 1e-01)) ? -1
                                                 : (bboxYMax - bboxYMin) / appY;
              for (UINT tms = 0; tms < bboxSamplingWeight && appY >= 0; tms++)
                ratioSampling.push_back(appY);

              for (p = 0; p < samplingPointNumber; p++) {
                appY = fabs(samplingPoint1[p].y - stroke1Centroid.y);
                if (appY)
                  ratioSampling.push_back(
                      fabs(samplingPoint2[p].y - stroke2Centroid.y) / appY);
              }

              if (!ratioSampling.empty()) {
                subStrokeYScaling.push_back(average(ratioSampling));
              }
            }

            delete subStroke1;
            delete subStroke2;
            subStroke1 = 0;
            subStroke2 = 0;
          }

          if (subStrokeXScaling.empty()) {
            transform.m_scaleX = 1.0;
          } else {
            transform.m_scaleX = average(subStrokeXScaling);
            if (isAlmostZero(transform.m_scaleX - 1.0))
              transform.m_scaleX = 1.0;
          }

          if (subStrokeYScaling.empty()) {
            transform.m_scaleY = 1.0;
          } else {
            transform.m_scaleY = average(subStrokeYScaling);
            if (isAlmostZero(transform.m_scaleY - 1.0))
              transform.m_scaleY = 1.0;
          }
          /*
#ifdef _DEBUG

TDebugMessage::getStream()<<"x scale "<< transform.m_scaleX ;
TDebugMessage::flush();
TDebugMessage::getStream()<<"y scale "<< transform.m_scaleY ;
TDebugMessage::flush();
#endif
*/
        } else {
          subStrokeXScaling.clear();
          subStrokeXScaling.reserve(cornerSize - 1);

          for (j = 0; j < cornerSize - 1; j++) {
            ///////////////////////////////////////// sampling

            subStroke1 =
                extract(stroke1, transform.m_firstStrokeCornerIndexes[j],
                        transform.m_firstStrokeCornerIndexes[j + 1] - 1);
            sample(subStroke1, samplingPointNumber, samplingPoint1);
            subStroke2 =
                extract(stroke2, transform.m_secondStrokeCornerIndexes[j],
                        transform.m_secondStrokeCornerIndexes[j + 1] - 1);
            sample(subStroke2, samplingPointNumber, samplingPoint2);

            ///////////////////////////////////////// compute Scale

            ratioSampling.clear();
            ratioSampling.reserve(samplingPointNumber + bboxSamplingWeight);

            TRectD bbox1 = subStroke1->getBBox();
            double app   = tdistance2(bbox1.getP00(), bbox1.getP11());
            if (app) {
              TRectD bbox2 =
                  TRotation(transform.m_rotation).inv() * subStroke2->getBBox();
              app = sqrt(tdistance2(bbox2.getP00(), bbox2.getP11()) / app);

              for (UINT tms = 0; tms < bboxSamplingWeight; tms++)
                ratioSampling.push_back(app);

              double app;
              for (p = 0; p < samplingPointNumber; p++) {
                app = tdistance(samplingPoint1[p], stroke1Centroid);
                if (app) {
                  ratioSampling.push_back(
                      tdistance(samplingPoint2[p], stroke2Centroid) / app);
                }
              }
            }
            if (!ratioSampling.empty()) {
              subStrokeXScaling.push_back(average(ratioSampling));
            }

            delete subStroke1;
            delete subStroke2;
            subStroke1 = 0;
            subStroke2 = 0;
          }

          if (subStrokeXScaling.empty()) {
            transform.m_scaleX = transform.m_scaleY = 1.0;
          } else {
            transform.m_scaleX = transform.m_scaleY =
                average(subStrokeXScaling);
            if (isAlmostZero(transform.m_scaleX - 1.0, 0.00001))
              transform.m_scaleX = transform.m_scaleY = 1.0;
          }
          /*
#ifdef _DEBUG

TDebugMessage::getStream()<<"scale "<< transform.m_scaleX ;
TDebugMessage::flush();
#endif
*/
        }

        ///////////////////////////////////////// compute centre of Rotation and
        /// Scaling

        std::vector<TPointD> vpOld(cornerSize), vpNew(cornerSize);
        TPointD pOld, pNew;

        for (j = 0; j < cornerSize - 1; j++) {
          vpOld[j] = stroke1->getChunk(transform.m_firstStrokeCornerIndexes[j])
                         ->getP0();
          vpNew[j] = stroke2->getChunk(transform.m_secondStrokeCornerIndexes[j])
                         ->getP0();
        }
        vpOld[j] = stroke1->getControlPoint(stroke1->getControlPointCount());
        vpNew[j] = stroke2->getControlPoint(stroke2->getControlPointCount());

        if (transform.m_rotation == 0.0) {
          if (transform.m_scaleX == 1.0 && transform.m_scaleY == 1.0) {
            transform.m_translate = stroke2Centroid - stroke1Centroid;
            transform.m_rotationAndScaleCenter = TPointD();
          } else {
            if (transform.m_scaleX == 1.0) {
              transform.m_rotationAndScaleCenter.x = 0;
              transform.m_translate.x              = 0;
              for (j = 0; j < cornerSize; j++) {
                transform.m_translate.x += vpNew[j].x - vpOld[j].x;
              }
              transform.m_translate.x = transform.m_translate.x / cornerSize;
            } else {
              transform.m_rotationAndScaleCenter.x = 0;

              for (j = 0; j < cornerSize; j++) {
                pOld = vpOld[j];
                pNew = vpNew[j];
                transform.m_rotationAndScaleCenter.x +=
                    (transform.m_scaleX * pOld.x - pNew.x) /
                    (transform.m_scaleX - 1.0);
              }
              transform.m_rotationAndScaleCenter.x =
                  transform.m_rotationAndScaleCenter.x / cornerSize;
              transform.m_translate.x = 0;
            }

            if (transform.m_scaleY == 1.0) {
              transform.m_rotationAndScaleCenter.y = 0;
              transform.m_translate.y              = 0;
              for (j = 0; j < cornerSize; j++) {
                transform.m_translate.y += vpNew[j].y - vpOld[j].y;
              }
              transform.m_translate.y = transform.m_translate.y / cornerSize;
            } else {
              transform.m_rotationAndScaleCenter.y = 0;

              for (j = 0; j < cornerSize; j++) {
                pOld = vpOld[j];
                pNew = vpNew[j];
                transform.m_rotationAndScaleCenter.y +=
                    (transform.m_scaleY * pOld.y - pNew.y) /
                    (transform.m_scaleY - 1.0);
              }
              transform.m_rotationAndScaleCenter.y =
                  transform.m_rotationAndScaleCenter.y / cornerSize;
              transform.m_translate.y = 0;
            }
          }

        } else {
          assert(transform.m_scaleX == transform.m_scaleY);

          cs = transform.m_scaleX * cos(totalRadRotation);
          sn = transform.m_scaleX * sin(totalRadRotation);

          // scelgo punti da usare come vincolo, per ottenere la translazione,
          // dato un centro di rotazione

          // dato il punto pOld e pNew si calcola analiticamnete il punto di
          // rotazione e scala
          // che minimizza la traslazione aggiuntiva e la traslazione stessa

          for (j = 0; j < cornerSize; j++) {
            pOld   = vpOld[j];
            pNew   = vpNew[j];
            constK = pNew.x - cs * pOld.x + sn * pOld.y;
            constQ = pNew.y - sn * pOld.x - cs * pOld.y;
            constB = 2 * (constK * (cs - 1.) + sn * constQ);
            constD = 2 * (constQ * (cs - 1.) - sn * constK);
            constA = transform.m_scaleX * transform.m_scaleX + 1 - 2 * cs;
            assert(constA > 0);
            constA = 1.0 / (2 * constA);
            transform.m_rotationAndScaleCenter.x += -constB * constA;
            transform.m_rotationAndScaleCenter.y += -constD * constA;
          }

          transform.m_rotationAndScaleCenter =
              transform.m_rotationAndScaleCenter * (1.0 / (double)cornerSize);

          transform.m_translate.x =
              (cs - 1.0) * transform.m_rotationAndScaleCenter.x -
              sn * transform.m_rotationAndScaleCenter.y + constK;

          transform.m_translate.y =
              sn * transform.m_rotationAndScaleCenter.x +
              (cs - 1.0) * transform.m_rotationAndScaleCenter.y + constQ;
        }

        /////////////////////////////////////////

        transform.m_inverse = (TTranslation(transform.m_translate) *
                               TScale(transform.m_rotationAndScaleCenter,
                                      transform.m_scaleX, transform.m_scaleY) *
                               TRotation(transform.m_rotationAndScaleCenter,
                                         transform.m_rotation))
                                  .inv();

        // debugStream.close();
      }  // end if !isPoint

    }  // end if !isEqual

    m_transformation.push_back(transform);

  }  // end for each stroke
}

//-------------------------------------------------------------------

TVectorImageP TInbetween::Imp::tween(double t) const {
  const double step             = 5.0;
  const double interpolateError = 1.0;

  TVectorImageP vi = new TVectorImage;

  UINT strokeCount1 = m_firstImage->getStrokeCount();
  UINT strokeCount2 = m_lastImage->getStrokeCount();
  vi->setPalette(m_firstImage->getPalette());
  if (strokeCount1 > strokeCount2) strokeCount1 = strokeCount2;

  assert(m_transformation.size() == strokeCount1);

  double totalLen1, totalLen2, len1, len2, step1, step2;
  std::vector<TThickPoint> points;
  TStroke *stroke1, *stroke2, *subStroke1, *subStroke2, *stroke;

  TAffine mt, invMatrix;
  TThickPoint point2, finalPoint;
  UINT i, j, cp, cpSize;

  for (i = 0; i < strokeCount1; i++) {
    stroke1 = m_firstImage->getStroke(i);
    stroke2 = m_lastImage->getStroke(i);

    if (m_transformation[i].m_type == StrokeTransform::EQUAL) {
      stroke = new TStroke(*stroke1);
    } else {
      points.clear();
      totalLen1 = stroke1->getLength();
      totalLen2 = stroke2->getLength();
      ;

      if (stroke1->getControlPointCount() == stroke2->getControlPointCount() &&
          stroke1->isSelfLoop() && stroke2->isSelfLoop()) {
        for (int i = 0; i < stroke1->getControlPointCount(); i++) {
          TThickPoint p0 = stroke1->getControlPoint(i);
          TThickPoint p1 = stroke2->getControlPoint(i);
          points.push_back(p0 * (1 - t) + p1 * t);
        }
        stroke = new TStroke(points);
      } else if (m_transformation[i].m_type == StrokeTransform::POINT) {
        TThickPoint pOld = stroke1->getThickPointAtLength(0.5 * totalLen1);
        TThickPoint pNew = stroke2->getThickPointAtLength(0.5 * totalLen2);
        points.push_back(pOld * (1 - t) + pNew * t);
        points.push_back(points[0]);
        points.push_back(points[0]);
        stroke = new TStroke(points);
      } else {
        mt = (m_transformation[i].m_inverse.isIdentity())
                 ? TAffine()
                 : TTranslation(m_transformation[i].m_translate * t) *
                       TScale(m_transformation[i].m_rotationAndScaleCenter,
                              (1 - t) + t * m_transformation[i].m_scaleX,
                              (1 - t) + t * m_transformation[i].m_scaleY) *
                       TRotation(m_transformation[i].m_rotationAndScaleCenter,
                                 m_transformation[i].m_rotation * t);

        UINT cornerSize = m_transformation[i].m_firstStrokeCornerIndexes.size();
        assert(cornerSize ==
               m_transformation[i].m_secondStrokeCornerIndexes.size());
        if (cornerSize > m_transformation[i].m_secondStrokeCornerIndexes.size())
          cornerSize = m_transformation[i].m_secondStrokeCornerIndexes.size();

        assert(cornerSize >= 2);

        std::vector<TThickPoint> controlPoints;

        // if not m_transformation[i].m_findCorners => detect corner return
        // different size =>cornerSize==2
        // assert(!m_transformation[i].m_findCorners || cornerSize==2);

        for (j = 0; j < cornerSize - 1; j++) {
          points.clear();
          subStroke1 = extract(
              stroke1, m_transformation[i].m_firstStrokeCornerIndexes[j],
              m_transformation[i].m_firstStrokeCornerIndexes[j + 1] - 1);
          subStroke2 = extract(
              stroke2, m_transformation[i].m_secondStrokeCornerIndexes[j],
              m_transformation[i].m_secondStrokeCornerIndexes[j + 1] - 1);

          totalLen1 = subStroke1->getLength();
          totalLen2 = subStroke2->getLength();

          if (totalLen1 > totalLen2) {
            step1 = step;
            step2 = (totalLen2 / totalLen1) * step;
          } else {
            step1 = (totalLen1 / totalLen2) * step;
            step2 = step;
          }

          len1 = 0;
          len2 = 0;

          while (len1 <= totalLen1 && len2 <= totalLen2) {
            point2 = subStroke2->getThickPointAtLength(len2);
            point2 = TThickPoint(m_transformation[i].m_inverse *
                                     subStroke2->getThickPointAtLength(len2),
                                 point2.thick);
            finalPoint =
                subStroke1->getThickPointAtLength(len1) * (1 - t) + t * point2;

            points.push_back(TThickPoint(mt * (finalPoint), finalPoint.thick));
            len1 += step1;
            len2 += step2;
          }
          point2     = subStroke2->getThickPointAtLength(totalLen2);
          point2     = TThickPoint(m_transformation[i].m_inverse *
                                   subStroke2->getThickPointAtLength(totalLen2),
                               point2.thick);
          finalPoint = subStroke1->getThickPointAtLength(totalLen1) * (1 - t) +
                       t * point2;

          points.push_back(TThickPoint(mt * (finalPoint), finalPoint.thick));

          stroke = TStroke::interpolate(points, interpolateError, false
                                        /*m_transformation[i].m_findCorners*/);

          if (j == 0)
            controlPoints.push_back(stroke->getControlPoint(0));
          else
            controlPoints.back() =
                (controlPoints.back() + stroke->getControlPoint(0)) * 0.5;

          cpSize = stroke->getControlPointCount();
          for (cp = 1; cp < cpSize; cp++) {
            controlPoints.push_back(stroke->getControlPoint(cp));
          }

          delete subStroke1;
          delete subStroke2;
          delete stroke;
          subStroke1 = 0;
          subStroke2 = 0;
          stroke     = 0;
        }

        stroke = new TStroke(controlPoints);
      }
    }

    if (stroke1->isSelfLoop() && stroke2->isSelfLoop()) stroke->setSelfLoop();

    stroke->setStyle(stroke1->getStyle());
    stroke->outlineOptions() = stroke1->outlineOptions();
    VIStroke *vs =
        new VIStroke(stroke, m_firstImage->getVIStroke(i)->m_groupId);
    vi->m_imp->m_strokes.push_back(vs);

  }  // end for each stroke

  if (m_firstImage->isComputedRegionAlmostOnce()) transferColor(vi);

  return vi;
}

//-------------------------------------------------------------------

void TInbetween::Imp::transferColor(const TVectorImageP &destination) const {
  const TVectorImageP &original = m_firstImage;
  destination->setPalette(original->getPalette());

  destination->findRegions();

  if (destination->getRegionCount()) {
    UINT strokeCount1 = original->getStrokeCount();
    UINT strokeCount2 = destination->getStrokeCount();
    if (strokeCount1 > strokeCount2) strokeCount1 = strokeCount2;

    for (UINT i = 0; i < strokeCount1; i++) {
      TVectorImage::transferStrokeColors(original, i, destination, i);
    }
  }
}

//-------------------------------------------------------------------

TVectorImageP TInbetween::tween(double t) const { return m_imp->tween(t); }

//-------------------------------------------------------------------

double TInbetween::interpolation(double t, enum TweenAlgorithm algorithm) {
  // in tutte le interpolazioni : s(0) = 0, s(1) = 1
  switch (algorithm) {
  case EaseInInterpolation:  // s'(1) = 0
    return t * (2 - t);
  case EaseOutInterpolation:  // s'(0) = 0
    return t * t;
  case EaseInOutInterpolation:  // s'(0) = s'(1) = 0
    return t * t * (3 - 2 * t);
  case LinearInterpolation:
  default:
    return t;
  }
}

//-------------------------------------------------------------------
