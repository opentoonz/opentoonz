

#include "tgeometry.h"
#include "cornerdetector.h"

//======================================================================

//! Classe che definisce dei punti che consentono di trovare gli angoli
class AlgorithmPointI final : public TPointI {
public:
  //! Indice originale del punto
  int m_originalIndex;
  //! Quantita' che dice quanto l'angolo e' acuto
  double m_sharpness;
  //! Dice se il punto e' un angolo
  bool m_isCorner;

  //! Costruttore
  /*!
    Gli viene passato un T3DPointD e restituisce un AlgorithmPointI
          */
  AlgorithmPointI(const T3DPointD &value, int index)
      : TPointI((int)value.x, (int)value.y)
      , m_originalIndex(index)
      , m_sharpness(0)
      , m_isCorner(false){};

  //! Costruttore
  /*!
    Gli viene passato un TPointI e restituisce un AlgorithmPointI
          */
  AlgorithmPointI(const TPointI &value, int index)
      : TPointI(value)
      , m_originalIndex(index)
      , m_sharpness(0)
      , m_isCorner(false){};

  //! Dstruttore
  ~AlgorithmPointI(){};
};

//! Definisce l'operatore meno tra due AlgorithmPointI
static AlgorithmPointI operator-(const AlgorithmPointI &op1,
                                 const AlgorithmPointI &op2) {
  return AlgorithmPointI(op1.operator-(op2), 0);
}

//======================================================================

//! Definisce point_container come un vettore di AlgorithmPointI
typedef std::vector<AlgorithmPointI> point_container;

//! Definisce corner_container come un vettore di interi
typedef std::vector<int> corner_container;

int gMinSampleNum;
int gMinDist;
int gMaxDist;
int gSquaredMinDist;
int gSquaredMaxDist;
double gMaxAngle;

//! Definisce un point_container
point_container gPoints;

//======================================================================

//! Ritorna true se e' possibile effettuare l'interpolazione
/*!
  Calcola gli AlgorithmPointI sulla base di points, verificando di punto in
  punto lo step
        tra ascissa e ordinata; inserisce i punti calcolati in gPoints e in base
  alla quntita' di
        AlgorithmPointI trovati restituisce true(e' possibile interpolare) o
  false(non e' possibile).

        \param points Vettore di T3DPoints
*/
static bool interpolate(const std::vector<T3DPointD> &points) {
  unsigned int curr, next;

  TPointI currStep, xStep, yStep, guideLine;
  TPointI xUnit(1, 0);
  TPointI yUnit(0, 1);

  bool chooseByDistance = false;

  curr = 0;
  next = 1;
  // int i = points.size();

  while (next <= points.size() - 1) {
    if (points[next] != points[curr]) {
      gPoints.push_back(AlgorithmPointI(points[curr], curr));
      guideLine = TPointI((int)(points[curr].x - points[next].x),
                          (int)(points[curr].y - points[next].y));
      currStep = gPoints.back();
      double xStepTheta, yStepTheta;
      // TPointI a;

      while (norm2(TPointI((int)(points[next].x - currStep.x),
                           (int)(points[next].y - currStep.y))) > 1) {
        TPointI nextPoint = TPointI((int)points[next].x, (int)points[next].y);

        // TPointI a = nextPoint - currStep;
        // int i = 0;

        if (currStep.x > nextPoint.x)
          xStep = currStep - xUnit;
        else if (currStep.x < nextPoint.x)
          xStep = currStep + xUnit;
        else {
          xStep            = currStep;
          chooseByDistance = true;
        }

        if (currStep.y > nextPoint.y)
          yStep = currStep - yUnit;
        else if (currStep.y < nextPoint.y)
          yStep = currStep + yUnit;
        else {
          yStep            = currStep;
          chooseByDistance = true;
        }

        if (chooseByDistance) {
          chooseByDistance = false;
          if (norm2(xStep - nextPoint) < norm2(yStep - nextPoint))
            currStep = xStep;
          else
            currStep = yStep;
        } else {
          double aux = norm2(guideLine);
          xStepTheta = acos((xStep - nextPoint) * guideLine /
                            (sqrt(norm2(xStep - nextPoint) * aux)));
          yStepTheta = acos((yStep - nextPoint) * guideLine /
                            (sqrt(norm2(yStep - nextPoint) * aux)));

          if (xStepTheta > yStepTheta)
            currStep = yStep;
          else
            currStep = xStep;
        }

        gPoints.push_back(AlgorithmPointI(currStep, next));
      }
    }

    curr++;
    next++;
  }
  gPoints.push_back(AlgorithmPointI(points[curr], curr));
  if ((int)gPoints.size() < (2 * gMaxDist + 1)) return false;
  return true;
}

//----------------------------------------------------------------------

//! Verifica se currIndex e' un possibile angolo e in caso affermativo salva
//! l'"acutezza"
inline bool isAdmissibleCorner(int currIndex, int precIndex, int nextIndex) {
  int size = gPoints.size();
  if (currIndex < 0 || currIndex >= size || precIndex < 0 ||
      precIndex >= size || nextIndex < 0 || nextIndex >= size)
    return false;

  AlgorithmPointI a = gPoints[currIndex] - gPoints[nextIndex];
  AlgorithmPointI b = gPoints[currIndex] - gPoints[precIndex];
  AlgorithmPointI c = gPoints[nextIndex] - gPoints[precIndex];

  double norm2_a = norm2(a);
  double norm2_b = norm2(b);

  if (!(norm2_a <= gSquaredMaxDist && norm2_a >= gSquaredMinDist) ||
      !(norm2_b <= gSquaredMaxDist && norm2_b >= gSquaredMinDist))
    return false;

  double norm2_c = norm2(c);
  double cosineOfAlpha =
      (norm2_a + norm2_b - norm2_c) / sqrt(4 * norm2_a * norm2_b);

  if (cosineOfAlpha < -1) cosineOfAlpha = -1;
  if (cosineOfAlpha > 1) cosineOfAlpha  = 1;

  double alpha = (180 / 3.14) * acos(cosineOfAlpha);

  if (alpha <= gMaxAngle) {
    gPoints[currIndex].m_sharpness += 180 - fabs(alpha);
    return true;
  }
  return false;
}

//----------------------------------------------------------------------

//! Trova i possibili angoli tra i punti di gPoints
static void findCornerCandidates() {
  unsigned int curr, prec, next;
  curr = gMaxDist;

  while (curr != gPoints.size() - gMaxDist) {
    prec                       = curr - 1;
    next                       = curr + 1;
    int admissibleCornersCount = 0;

    int countDown = 5;
    while (countDown) {
      --countDown;
      while (isAdmissibleCorner(curr, prec, next)) {
        admissibleCornersCount++;
        countDown = 0;
        --prec;
        ++next;
      }
      --prec;
      ++next;
    }

    if (admissibleCornersCount) {
      gPoints[curr].m_sharpness /= admissibleCornersCount;
      if ((gPoints[curr].m_sharpness > (180 - gMaxAngle)) &&
          (admissibleCornersCount > gMinSampleNum))
        gPoints[curr].m_isCorner = true;
    }

    ++curr;
  }
}

//----------------------------------------------------------------------

//! Trova gli angoli tra i punti di gPoints
static void findCorners(int neighborLimit, std::vector<int> &cornerIndexes) {
  unsigned int curr, prec, next;
  curr = gMaxDist;

  while (curr != gPoints.size() - gMaxDist) {
    prec = curr - 1;
    next = curr + 1;
    while ((norm2(gPoints[curr] - gPoints[prec]) <= neighborLimit) &&
           (norm2(gPoints[curr] - gPoints[next]) <= neighborLimit) &&
           gPoints[curr].m_isCorner) {
      if (gPoints[curr].m_sharpness <= gPoints[prec].m_sharpness ||
          gPoints[curr].m_sharpness <= gPoints[next].m_sharpness) {
        gPoints[curr].m_isCorner = false;
        break;
      }
      --prec;
      ++next;
    }

    if (gPoints[curr].m_isCorner)
      cornerIndexes.push_back(gPoints[curr].m_originalIndex);

    ++curr;
  }
}

//----------------------------------------------------------------------

//! Individua gli eventuali angoli presenti nella curva da calcolare
void detectCorners(const std::vector<T3DPointD> &inputPoints, int minSampleNum,
                   int minDist, int maxDist, double maxAngle,
                   std::vector<int> &cornerIndexes) {
  gMinSampleNum   = minSampleNum;
  gMinDist        = minDist;
  gMaxDist        = maxDist;
  gSquaredMinDist = minDist * minDist;
  gSquaredMaxDist = maxDist * maxDist;
  gMaxAngle       = maxAngle;

  interpolate(inputPoints);
  if ((int)gPoints.size() > 2 * gMaxDist) {
    findCornerCandidates();
    findCorners((int)sqrt((float)gSquaredMaxDist) + 10, cornerIndexes);
  }
  gPoints.clear();

  // check for no index equal to an adjacent
  std::vector<int>::iterator it1, it2;
  it1 = it2 = cornerIndexes.begin();
  ++it2;

  while (it2 != cornerIndexes.end()) {
    if (*it1 == *it2) {
      it2 = cornerIndexes.erase(it2);
    } else {
      ++it1;
      ++it2;
    }
  }
}
