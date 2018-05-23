

#include "tmachine.h"
#include "tmathutil.h"
#include "tstrokeutil.h"
#include "tstrokeoutline.h"
#include "tcurves.h"
#include "tbezier.h"
#include "tzerofinder.h"
#include "tcurveutil.h"
#include "cornerdetector.h"

#include <limits>

#include "tstroke.h"

//=============================================================================

#define USE_NEW_3D_ERROR_COMPUTE 1

//=============================================================================

// Some using declaration

using namespace TConsts;
using namespace std;

// Some useful typedefs

typedef std::vector<double> DoubleArray;
typedef DoubleArray::iterator DoubleIt;
typedef std::vector<TThickQuadratic *> QuadStrokeChunkArray;

static int numSaved = 0;

namespace {
//---------------------------------------------------------------------------

void extractStrokeControlPoints(const QuadStrokeChunkArray &curves,
                                vector<TThickPoint> &ctrlPnts) {
  const TThickQuadratic *prev = curves[0];
  assert(prev);
  const TThickQuadratic *curr;

  ctrlPnts.push_back(prev->getThickP0());
  ctrlPnts.push_back(prev->getThickP1());

  for (UINT i = 1; i < curves.size(); ++i) {
    curr = curves[i];
    assert(curr);
    TThickPoint middlePnt = (prev->getThickP2() + curr->getThickP0()) * 0.5;
    ctrlPnts.push_back(middlePnt);
    ctrlPnts.push_back(curr->getThickP1());
    prev = curr;
  }

  ctrlPnts.push_back(prev->getThickP2());
}
//---------------------------------------------------------------------------

inline TThickPoint adapter(const TThickPoint &tp) { return tp; }

//---------------------------------------------------------------------------

inline TThickPoint adapter(const TPointD &p) { return TThickPoint(p); }

//---------------------------------------------------------------------------

template <typename T>
void buildChunksFromControlPoints(QuadStrokeChunkArray &tq,
                                  const vector<T> &v) {
  TThickQuadratic *chunk;
  T temp;
  switch (v.size()) {
  case 0:
    tq.push_back(new TThickQuadratic);
    break;
  case 1:
    temp  = adapter(v.front());
    chunk = new TThickQuadratic(temp, temp, temp);
    tq.push_back(chunk);
    break;
  case 2: {
    TThickSegment s(adapter(v.front()), adapter(v.back()));
    chunk = new TThickQuadratic(s.getThickP0(), s.getThickPoint(0.5),
                                s.getThickP1());
    tq.push_back(chunk);
  } break;
  default:
    assert(v.size() & 1);  //  v.size() == 2 * chunk + 1
    for (UINT i = 0; i < v.size() - 1; i += 2) {
      chunk = new TThickQuadratic(adapter(v[i]), adapter(v[i + 1]),
                                  adapter(v[i + 2]));
      tq.push_back(chunk);
    }
    break;
  }
}
//---------------------------------------------------------------------------

// WARNING  duplicata in tellipticbrush
//  dovrebbe essere eliminata da tellipticbrush perche' qui
//  viene usata per  eliminare le strokes a thickness negativa
//  evitandone la gestione in tellip...
inline bool pairValuesAreEqual(const DoublePair &p) {
  return p.first == p.second;
}

//---------------------------------------------------------------------------

// WARNING  duplicata in tellipticbrush
//  dovrebbe essere eliminata da tellipticbrush perche' qui
//  viene usata per  eliminare le strokes a thickness negativa
//  evitandone la gestione in tellip...
void analyzeSolution(const vector<double> &coeff,
                     vector<DoublePair> &interval) {
  //  risolve la disequazione  coeff[2]*t^2 + coeff[1]*t + coeff[0] >= 0  in [0,
  //  1] ritornando le soluzioni
  //  come sotto-intervalli chiusi di [0, 1] (gli intervalli degeneri [s, s]
  //  isolati vengono eliminati)
  vector<double> sol;
  // int numberOfIntervalSolution = 0;

  rootFinding(coeff, sol);

  if (isAlmostZero(coeff[2])) {
    //  disequazione di 1^ grado
    if (isAlmostZero(coeff[1])) {
      if (coeff[0] >= 0) interval.push_back(DoublePair(0.0, 1.0));
      return;
    }

    double singleSol = -coeff[0] / coeff[1];

    if (coeff[1] > 0) {
      if (singleSol < 1)
        interval.push_back(DoublePair(std::max(0.0, singleSol), 1.0));
    } else {
      if (singleSol > 0)
        interval.push_back(DoublePair(0.0, std::min(1.0, singleSol)));
    }
    return;
  }

  //  disequazione di 2^ grado effettivo
  // double delta = sq(coeff[1]) - 4*coeff[2]*coeff[0];

  sort(sol.begin(), sol.end());

  if (coeff[2] > 0) {
    switch (sol.size()) {
    case 0:
      interval.push_back(DoublePair(0.0, 1.0));
      break;

    case 1:
      interval.push_back(DoublePair(0.0, 1.0));
      break;

    case 2:
      interval.push_back(DoublePair(0.0, std::min(std::max(sol[0], 0.0), 1.0)));
      interval.push_back(DoublePair(std::max(std::min(sol[1], 1.0), 0.0), 1.0));
      break;
    }
  } else if (coeff[2] < 0 && sol.size() == 2)
    interval.push_back(DoublePair(std::min(std::max(sol[0], 0.0), 1.0),
                                  std::max(std::min(sol[1], 1.0), 0.0)));

  // eat not valid interval
  std::vector<DoublePair>::iterator it =
      std::remove_if(interval.begin(), interval.end(), pairValuesAreEqual);

  interval.erase(it, interval.end());
}

//---------------------------------------------------------------------------

void floorNegativeThickness(TThickQuadratic *quad) {
  assert(quad);
  double val = quad->getThickP0().thick;

  if (val < 0 && isAlmostZero(val))
    quad->setThickP0(TThickPoint(quad->getP0(), 0.0));

  val = quad->getThickP1().thick;
  if (val < 0 && isAlmostZero(val))
    quad->setThickP1(TThickPoint(quad->getP1(), 0.0));

  val = quad->getThickP2().thick;
  if (val < 0 && isAlmostZero(val))
    quad->setThickP2(TThickPoint(quad->getP2(), 0.0));
}

//---------------------------------------------------------------------------

// potrebbe essere realizzata come unary function da usare in una transform
void roundNegativeThickess(QuadStrokeChunkArray &v) {
  QuadStrokeChunkArray protoStroke, tempVectTQ;

  TThickQuadratic *tempTQ = 0, *tempTQ_1 = 0;

  int chunkCount = v.size();

  vector<double> coeff;
  double alpha, beta, gamma;

  vector<DoublePair> positiveIntervals;

  for (int i = 0; i < chunkCount; ++i) {
    const TThickQuadratic &ttq = *v[i];

    alpha = ttq.getThickP0().thick - 2 * ttq.getThickP1().thick +
            ttq.getThickP2().thick;
    beta  = 2.0 * (ttq.getThickP1().thick - ttq.getThickP0().thick);
    gamma = ttq.getThickP0().thick;

    coeff.push_back(gamma);
    coeff.push_back(beta);
    coeff.push_back(alpha);

    //  return sotto-intervalli non degeneri di [0, 1] in cui coeff[2]*t^2 +
    //  coeff[1]*t + coeff[0] >= 0
    analyzeSolution(coeff, positiveIntervals);

    //  il caso isAlmostZero(r(t)) per t in [0, 1] e' gestito direttamente da
    //  computeOutline
    switch (positiveIntervals.size()) {
    case 0:  //  r(t) <= 0 per t in [0, 1]
      tempTQ = new TThickQuadratic(ttq);
      tempTQ->setThickP0(TThickPoint(tempTQ->getP0(), 0.0));
      tempTQ->setThickP1(TThickPoint(tempTQ->getP1(), 0.0));
      tempTQ->setThickP2(TThickPoint(tempTQ->getP2(), 0.0));
      protoStroke.push_back(tempTQ);
      break;
    case 1:
      if (positiveIntervals[0].first == 0.0 &&
          positiveIntervals[0].second == 1.0)
        protoStroke.push_back(new TThickQuadratic(ttq));
      else if (positiveIntervals[0].first == 0.0) {
        tempTQ   = new TThickQuadratic;
        tempTQ_1 = new TThickQuadratic;
        ttq.split(positiveIntervals[0].first, *tempTQ, *tempTQ_1);
        tempTQ_1->setThickP0(TThickPoint(tempTQ_1->getP0(), 0.0));
        tempTQ_1->setThickP1(TThickPoint(tempTQ_1->getP1(), 0.0));
        tempTQ_1->setThickP2(TThickPoint(tempTQ_1->getP2(), 0.0));

        floorNegativeThickness(tempTQ);
        protoStroke.push_back(tempTQ);
        protoStroke.push_back(tempTQ_1);
      } else if (positiveIntervals[0].second == 1.0) {
        tempTQ   = new TThickQuadratic;
        tempTQ_1 = new TThickQuadratic;
        ttq.split(positiveIntervals[0].first, *tempTQ, *tempTQ_1);
        tempTQ->setThickP0(TThickPoint(tempTQ->getP0(), 0.0));
        tempTQ->setThickP1(TThickPoint(tempTQ->getP1(), 0.0));
        tempTQ->setThickP2(TThickPoint(tempTQ->getP2(), 0.0));

        protoStroke.push_back(tempTQ);
        floorNegativeThickness(tempTQ_1);
        protoStroke.push_back(tempTQ_1);
      } else {
        coeff.clear();
        coeff.push_back(positiveIntervals[0].first);
        coeff.push_back(positiveIntervals[0].second);
        split<TThickQuadratic>(ttq, coeff, tempVectTQ);
        assert(tempVectTQ.size() == 3);

        tempVectTQ[0]->setThickP0(TThickPoint(tempVectTQ[0]->getP0(), 0.0));
        tempVectTQ[0]->setThickP1(TThickPoint(tempVectTQ[0]->getP1(), 0.0));
        tempVectTQ[0]->setThickP2(TThickPoint(tempVectTQ[0]->getP2(), 0.0));

        // controllo che i valori prossimi a zero siano in ogni caso positivi
        floorNegativeThickness(tempVectTQ[1]);

        tempVectTQ[2]->setThickP0(TThickPoint(tempVectTQ[2]->getP0(), 0.0));
        tempVectTQ[2]->setThickP1(TThickPoint(tempVectTQ[2]->getP1(), 0.0));
        tempVectTQ[2]->setThickP2(TThickPoint(tempVectTQ[2]->getP2(), 0.0));

        copy(tempVectTQ.begin(), tempVectTQ.end(), back_inserter(protoStroke));
        tempVectTQ
            .clear();  // non serve una clearPointerArray perchè il possesso
                       // va alla protoStroke
      }
      break;
    case 2:
      assert(positiveIntervals[0].first == 0.0);
      assert(positiveIntervals[1].second == 1.0);

      coeff.clear();
      coeff.push_back(positiveIntervals[0].second);
      coeff.push_back(positiveIntervals[1].first);
      split<TThickQuadratic>(ttq, coeff, tempVectTQ);
      assert(tempVectTQ.size() == 3);

      floorNegativeThickness(tempVectTQ[0]);

      tempVectTQ[1]->setThickP0(TThickPoint(tempVectTQ[1]->getP0(), 0.0));
      tempVectTQ[1]->setThickP1(TThickPoint(tempVectTQ[1]->getP1(), 0.0));
      tempVectTQ[1]->setThickP2(TThickPoint(tempVectTQ[1]->getP2(), 0.0));

      floorNegativeThickness(tempVectTQ[2]);

      copy(tempVectTQ.begin(), tempVectTQ.end(), back_inserter(protoStroke));
      tempVectTQ.clear();  // non serve una clearPointerArray perchè il possesso

      break;
    }

    positiveIntervals.clear();
    coeff.clear();
  }

  swap(protoStroke, v);
  clearPointerContainer(protoStroke);
}

//---------------------------------------------------------------------------

// Some usefuf constant
// used in printContainer to set number of row to print
const int MAX_ROW = 10;

inline void changeTQDirection(TThickQuadratic *tq) {
  TThickPoint p = tq->getThickP2();
  tq->setThickP2(tq->getThickP0());
  tq->setThickP0(p);
}

/*
  //---------------------------------------------------------------------------

  double getTQDistance2(const TThickQuadratic& tq,
    const TPointD& p,
    double maxDistance2,
    double& currT)
  {
    TRectD rect = tq.getBBox();
    if (!rect.contains(p))
    {
      double dist21 = tdistance2(p, rect.getP00());
      double dist22 = tdistance2(p, rect.getP01());
      double dist23 = tdistance2(p, rect.getP10());
      double dist24 = tdistance2(p, rect.getP11());

      if (std::min(dist21, dist22, dist23, dist24)>=maxDistance2)
        return maxDistance2;
    }
    currT = tq.getT(p);
    double dist2 = tdistance2(tq.getPoint(currT), p);
    return (dist2<maxDistance2)?dist2:maxDistance2;
  }

  //---------------------------------------------------------------------------

  template <class K, class T>
    void clearMap(std::map<K,T>& v)
  {
    typedef std::map<K,T>::iterator TypeIt;

    TypeIt it = v.begin();

    for( ;it!=v.end(); ++it)
      delete it->second;

    v.clear();
  }
  */

//---------------------------------------------------------------------------

/*
  Local binary function to find approx values in a vector.
  */
bool bfAreAlmostEqual(double x, double y) {
  double x_y = (x > y) ? x - y : y - x;
  return x_y < TConsts::epsilon;
}

//---------------------------------------------------------------------------

/*
  Sends values of a container in standard input.
  */
template <class T>
void printContainer(const T &c, int maxRow = MAX_ROW) {
  /*  DA DECOMMENTARE SE NECESSARIO!!!!!
      //(commentato per non avere dipendenze da tsystem.lib 7/1/2004)
if (maxRow <=0 ) maxRow = MAX_ROW;

typename T::const_iterator cit;
cit = c.begin();
stringstream  oss1;
oss1<<'['<<c.size()<<']'<<"=\n";
TSystem::outputDebug( oss1.str() );

int counter = 0;
for( ; cit != c.end(); ++cit)
{
stringstream  oss;
if( ++counter == maxRow-1)
{
  oss<<'\n';
  counter = 0;
}
oss<<(*cit)<<'\n'<<'\0';
TSystem::outputDebug( oss.str() );
}
          */
}

//---------------------------------------------------------------------------

template <class T>
void printContainer(ostream &os, const T &c, int maxRow = MAX_ROW) {
  if (maxRow <= 0) maxRow = MAX_ROW;
  typename T::const_iterator cit;
  cit = c.begin();
  os << '[' << c.size() << ']' << "=\n";
  int counter = 0;
  for (; cit != c.end(); ++cit) {
    if (++counter == maxRow - 1) {
      os << '\n';
      counter = 0;
    }
    os << (*cit) << ' ';
  }
}

//---------------------------------------------------------------------------

template <class T>
void printContainerOfPointer(ostream &os, const T &c, int maxRow = MAX_ROW) {
  if (maxRow <= 0) maxRow = MAX_ROW;
  typename T::const_iterator cit;
  cit = c.begin();
  os << '[' << c.size() << ']' << " - ";
  int counter = 0;
  for (; cit != c.end(); ++cit) {
    if (++counter == maxRow - 1) {
      os << '\n';
      counter = 0;
    }
    os << **cit << ' ';
  }
}

//---------------------------------------------------------------------------

/*
  Compute a proportion of type x:a=b:c
  */
template <class T>
T proportion(T a, T b, T c) {
  assert(c != T(0));
  return a * b / c;
}

//---------------------------------------------------------------------------

/*
  Compute a proportion of type x-off:a-off=b:c
  */
template <class T>
T proportion(T a, T b, T c, T offset) {
  assert(c != T(0));
  return (a - offset) * b / c + offset;
}

//---------------------------------------------------------------------------

/*!
    backinserter
   */
template <typename type_, typename container_>
class TBackInserterPointer {
  container_ &m_c;

public:
  explicit TBackInserterPointer(container_ &c) : m_c(c){};

  TBackInserterPointer &operator=(const type_ *value) {
    m_c.push_back(new type_(*value));
    return *this;
  }

  TBackInserterPointer &operator=(const type_ &value) {
    m_c.push_back(new type_(value));
    return *this;
  };
#ifdef MACOSX
  typedef type_ value_type;
  typedef type_ &reference;
  typedef type_ *pointer;
  typedef output_iterator_tag iterator_category;
  typedef ptrdiff_t difference_type;
#endif
  /* SIC */
  TBackInserterPointer &operator*() { return *this; }
  TBackInserterPointer &operator++() { return *this; }
  TBackInserterPointer operator++(int val) { return *this; }
};

typedef TBackInserterPointer<TThickQuadratic, QuadStrokeChunkArray>
    TThickQuadraticArrayInsertIterator;

//---------------------------------------------------------------------------

/*!
  simple adapter for find zero algorithm
  */
struct computeOffset_ {
  TQuadraticLengthEvaluator m_lengthEval;
  double m_offset;

  computeOffset_(const TThickQuadratic *ttq, double offset)
      : m_lengthEval(*ttq), m_offset(offset) {}

  double operator()(double par) {
    return m_lengthEval.getLengthAt(par) - m_offset;
  }
};

//---------------------------------------------------------------------------

/*!
  simple adapter for find zero algorithm
  */
struct computeSpeed_ {
  const TThickQuadratic *ref_;

  computeSpeed_(const TThickQuadratic *ref) : ref_(ref) {}

  double operator()(double par) { return norm(ref_->getSpeed(par)); }
};

//---------------------------------------------------------------------------
}  // end of unnamed namespace

//=============================================================================

const BYTE TStroke::c_selected_flag       = 0x1;
const BYTE TStroke::c_changed_region_flag = 0x2;
const BYTE TStroke::c_dirty_flag          = 0x4;

//=============================================================================
//
// TStroke::Imp
//
//-----------------------------------------------------------------------------

struct TStroke::Imp {
  // Geometry-related infos

  BYTE m_flag;

  //! This flag checks if changes occurs and if it is neccessary to update
  //! length.
  bool m_isValidLength;

  //! This flag checks if changes occurs and if it is neccessary to update
  //! outline.
  bool m_isOutlineValid;

  //! Control calculus of cache vector.
  bool m_areDisabledComputeOfCaches;

  //! Bounding Box of a stroke
  TRectD m_bBox;

  //! This vector contains length computed for  each control point of stroke.
  DoubleArray m_partialLengthArray;

  //! This vector contains parameter computed for each control point of stroke.
  DoubleArray m_parameterValueAtControlPoint;

  //! This vector contains outline of stroke.
  QuadStrokeChunkArray m_centerLineArray;

  bool m_selfLoop;

  int m_negativeThicknessPoints;
  double m_averageThickness;
  double m_maxThickness;

  //-------------------------------

  // Not-geometrical vars (style infos)

  int m_id;

  int m_styleId;
  TStrokeProp *m_prop;

  TStroke::OutlineOptions m_outlineOptions;

  //-------------------------------

  Imp();

  Imp(const vector<TPointD> &v);

  Imp(const vector<TThickPoint> &v);

  ~Imp() {
    delete m_prop;
    clearPointerContainer(m_centerLineArray);
    // delete m_style;
  }

  void init();

  // void  computeOutlines( double pixelSize );

  inline double getW(int index) {
    return ((int)m_parameterValueAtControlPoint.size() > index)
               ? m_parameterValueAtControlPoint[index]
               : m_parameterValueAtControlPoint.back();
  }

  TRectD computeCenterlineBBox();
  /*!
It computes the bounding box of the subStroke in the parameter range w0-w1.
*/

  void computeMaxThickness();

  TRectD computeSubBBox(double w0, double w1) const;

  inline QuadStrokeChunkArray &getTQArray() { return m_centerLineArray; }

  /*!
Swaps the geometrical infos only
*/
  void swapGeometry(TStroke::Imp &other) throw();

  //! compute cache vector
  void computeCacheVector();

  /*!
Set value in m_parameterValueAtControlPoint
*/
  void computeParameterInControlPoint();

  /*!
Update parameter in m_parameterValueAtControlPoint
after insert of control point in stroke.
*/
  void updateParameterValue(double w, UINT chunk, TThickQuadratic *tq1,
                            TThickQuadratic *tq2);

  /*!
From parameter w retrieves chunk and its parameter ( t in [0,1] ).
Return
true  ->  error (parameter w out of range, etc)
false ->  ok
*/
  bool retrieveChunkAndItsParamameter(double w, int &chunk, double &t);

  /*!
From length s retrieves chunk and its parameter ( t in [0,1] ).
Return
true  ->  error (parameter w out of range, etc)
false ->  ok
*/
  bool retrieveChunkAndItsParamameterAtLength(double s, int &chunk, double &t);

  /*!
Retrieve chunk which contains the n-th control point of stroke.
If control point is between two chunks return the left point.
*/
  int retrieveChunkFromControlPointIndex(int n) {
    assert(0 <= n && n < getControlPointCount());

    if (n & 1) ++n;

    n >>= 1;

    return n ? n - 1 : n;
  };

  /*!
Retrieve range for a chunk.
*/
  DoublePair retrieveParametersFromChunk(UINT chunk) {
    DoublePair outPar;

    int nFirst, nSecond;

    nFirst  = chunk * 2;
    nSecond = (chunk + 1) * 2;

    outPar.first  = getW(nFirst);
    outPar.second = getW(nSecond);

    return outPar;
  }

  //---------------------------------------------------------------------------

  void print(ostream &os);

  //---------------------------------------------------------------------------

  inline int getChunkCount() const { return m_centerLineArray.size(); }

  //---------------------------------------------------------------------------

  inline int getControlPointCount() const {
    UINT out = 2 * getChunkCount() + 1;
    return out;
  }

  //-----------------------------------------------------------------------------

  TThickQuadratic *getChunk(int index) {
    if (0 <= index && index < getChunkCount()) return m_centerLineArray[index];

    return 0;
  }

private:
  // Declared but not defined.
  Imp(const Imp &other);
  Imp &operator=(const Imp &other);
};

//-----------------------------------------------------------------------------

namespace {
int maxStrokeId = 0;
}

/*! init() is required to initialize all variable
Call init after centerline initialization, because
it's necessary to compute BBox
*/
void TStroke::Imp::init() {
  m_flag = c_dirty_flag;

  m_styleId = 1;  // DefaultStrokeStyle;
  m_prop    = 0;

  m_id                         = ++maxStrokeId;
  m_isValidLength              = false;
  m_isOutlineValid             = false;
  m_areDisabledComputeOfCaches = false;
  m_selfLoop                   = false;
  m_averageThickness           = 0;
  m_maxThickness               = -1;
  m_negativeThicknessPoints    = 0;
  for (UINT j = 0; j < m_centerLineArray.size(); j++) {
    if (m_centerLineArray[j]->getThickP0().thick <= 0)
      m_negativeThicknessPoints++;
    if (m_centerLineArray[j]->getThickP1().thick <= 0)
      m_negativeThicknessPoints++;
  }
  if (!m_centerLineArray.empty() &&
      m_centerLineArray.back()->getThickP2().thick <= 0)
    m_negativeThicknessPoints++;

  computeParameterInControlPoint();
}

//-----------------------------------------------------------------------------

TStroke::Imp::Imp() { init(); }

//-----------------------------------------------------------------------------

TStroke::Imp::Imp(const std::vector<TThickPoint> &v) {
  buildChunksFromControlPoints(m_centerLineArray, v);
  roundNegativeThickess(m_centerLineArray);

  init();
}

//-----------------------------------------------------------------------------

TStroke::Imp::Imp(const std::vector<TPointD> &v) {
  buildChunksFromControlPoints(m_centerLineArray, v);
  roundNegativeThickess(m_centerLineArray);

  init();
}

//-----------------------------------------------------------------------------

void TStroke::Imp::swapGeometry(Imp &other) throw() {
  std::swap(m_flag, other.m_flag);
  std::swap(m_isValidLength, other.m_isValidLength);
  std::swap(m_isOutlineValid, other.m_isOutlineValid);
  std::swap(m_areDisabledComputeOfCaches, other.m_areDisabledComputeOfCaches);
  std::swap(m_bBox, other.m_bBox);
  std::swap(m_partialLengthArray, other.m_partialLengthArray);
  std::swap(m_parameterValueAtControlPoint,
            other.m_parameterValueAtControlPoint);
  std::swap(m_centerLineArray, other.m_centerLineArray);
  std::swap(m_selfLoop, other.m_selfLoop);
  std::swap(m_negativeThicknessPoints, other.m_negativeThicknessPoints);
  std::swap(m_averageThickness, other.m_averageThickness);
  std::swap(m_maxThickness, other.m_maxThickness);
}

//-----------------------------------------------------------------------------

void TStroke::Imp::computeMaxThickness() {
  m_maxThickness = m_centerLineArray[0]->getThickP0().thick;
  for (UINT i = 0; i < m_centerLineArray.size(); i++)
    m_maxThickness =
        std::max({m_maxThickness, m_centerLineArray[i]->getThickP1().thick,
                  m_centerLineArray[i]->getThickP2().thick});
}

void TStroke::Imp::computeCacheVector() {
  // se la stroke e' stata invalidata a causa dell'inserimento di punti
  //  di controllo o dal ricampionamento
  if (!m_areDisabledComputeOfCaches && !m_isValidLength) {
    if (getChunkCount() > 0)  // se ci sono cionchi
    {
      // (re)inizializzo un vettore
      m_partialLengthArray.resize(getControlPointCount(),
                                  (std::numeric_limits<double>::max)());

      m_partialLengthArray[0] = 0.0;

      double length = 0.0;
      int j         = 0;
      const TThickQuadratic *tq;

      TQuadraticLengthEvaluator lengthEvaluator;

      for (int i = 0; i < getChunkCount(); ++i) {
        assert(j <= getControlPointCount());
        tq = getChunk(i);
        lengthEvaluator.setQuad(*tq);
        m_partialLengthArray[j++] = length;
        m_partialLengthArray[j++] = length + lengthEvaluator.getLengthAt(0.5);
        length += lengthEvaluator.getLengthAt(1.0);
      }

      m_partialLengthArray[j++] = length;
      assert(j == getControlPointCount());
      // assert( m_parameterValueAtControlPoint.size() ==
      // m_partialLengthArray.size() );
    }
    m_isValidLength = true;
  }
}

//-----------------------------------------------------------------------------

void TStroke::Imp::computeParameterInControlPoint() {
  if (!m_areDisabledComputeOfCaches) {
    // questa funzione ricalcola i valori dei parametri nei cionchi
    //  N.B. deve essere richiamata quando si effettuano inserimenti
    //    di punti di controllo che cambiano la lunghezza della curva
    //  insert, push e costruttore
    if (!getChunkCount()) {
      m_parameterValueAtControlPoint.clear();
      return;
    }

    int controlPointCount = getControlPointCount();

    m_parameterValueAtControlPoint.resize(controlPointCount, 0);

    // N.B. number of control point is reduced of 1
    --controlPointCount;

    double val = 0.0;

    assert(controlPointCount >= 0.0);

    for (int i = 0; i <= controlPointCount; ++i) {
      val                               = i / (double)controlPointCount;
      m_parameterValueAtControlPoint[i] = val;
    }
  }
}

//-----------------------------------------------------------------------------

void TStroke::Imp::updateParameterValue(double w, UINT chunk,
                                        TThickQuadratic *tq1,
                                        TThickQuadratic *tq2) {
  DoublePair p = retrieveParametersFromChunk(chunk);

  UINT controlPointToErase = 2 * chunk + 1;
  DoubleIt it              = m_parameterValueAtControlPoint.begin();
  std::advance(it, controlPointToErase);
  m_parameterValueAtControlPoint.erase(it);

  double normalizedParam = tq2->getT(tq2->getP1());

  std::vector<double>::iterator first;

  normalizedParam = proportion(p.second, normalizedParam, 1.0, w);

  first =
      std::upper_bound(m_parameterValueAtControlPoint.begin(),
                       m_parameterValueAtControlPoint.end(), normalizedParam);

  if (first != m_parameterValueAtControlPoint.end()) {
    first = m_parameterValueAtControlPoint.insert(first, normalizedParam);

    first = m_parameterValueAtControlPoint.insert(first, w);

    normalizedParam = tq1->getT(tq1->getP1());
    normalizedParam = proportion(w, normalizedParam, 1.0, p.first);

    m_parameterValueAtControlPoint.insert(first, normalizedParam);
  }

  /* FAB
assert( getControlPointCount() <=
    (int)m_parameterValueAtControlPoint.size() );
//*/
}

//-----------------------------------------------------------------------------

bool TStroke::getChunkAndTAtLength(double s, int &chunk, double &t) const {
  return m_imp->retrieveChunkAndItsParamameterAtLength(s, chunk, t);
}

//-----------------------------------------------------------------------------

bool TStroke::Imp::retrieveChunkAndItsParamameterAtLength(double s, int &chunk,
                                                          double &t) {
  vector<double>::iterator first;

  // cerco nella cache la posizione che compete alla lunghezza s
  first = std::upper_bound(m_partialLengthArray.begin(),
                           m_partialLengthArray.end(), s);

  // se s e' interna al vettore di cache
  if (first != m_partialLengthArray.end()) {
    // individuo il punto di controllo della stroke...
    int controlPointOffset = distance(m_partialLengthArray.begin(), first);

    // ...e da questo il cionco relativo.
    chunk = retrieveChunkFromControlPointIndex(controlPointOffset);

    if (first != m_partialLengthArray.begin() && s == *(first - 1)) {
      controlPointOffset--;
      if (controlPointOffset & 1) {
        const DoublePair &p = retrieveParametersFromChunk(chunk);
        t = proportion(1.0, getW(controlPointOffset) - p.first,
                       p.second - p.first);
      } else
        t = 0.0;

      return false;
    }

    // fisso un offset per l'algoritmo di bisezione
    double offset = (first == m_partialLengthArray.begin())
                        ? s
                        : s - m_partialLengthArray[chunk * 2];

    // cerco il parametro minimo a meno di una tolleranza epsilon

    const double tol = TConsts::epsilon * 0.1;
    int err;

    computeOffset_ op(getChunk(chunk), offset);
    computeSpeed_ op2(getChunk(chunk));

    if (!findZero_Newton(0.0, 1.0, op, op2, tol, tol, 100, t, err))
      t = -1;  // if can not find a good value set parameter to error value

    // se l'algoritmo di ricerca ha fallito fissa il valore ad uno dei due
    // estremi
    if (t == -1) {
      if (s <= m_partialLengthArray[controlPointOffset]) t = 0.0;
      t                                                    = 1.0;
    }

    return false;
  }

  if (s <= 0.0) {
    chunk = 0;
    t     = 0.0;
  } else if (s >= m_partialLengthArray.back()) {
    chunk = getChunkCount() - 1;
    t     = 1.0;
  }

  return false;
}

//-----------------------------------------------------------------------------

bool TStroke::getChunkAndT(double w, int &chunk, double &t) const {
  return m_imp->retrieveChunkAndItsParamameter(w, chunk, t);
}

//-----------------------------------------------------------------------------

bool TStroke::Imp::retrieveChunkAndItsParamameter(double w, int &chunk,
                                                  double &t) {
  vector<double>::iterator first;

  // trova l'iteratore alla prima posizione che risulta maggiore o uguale a w
  first = std::lower_bound(m_parameterValueAtControlPoint.begin(),
                           m_parameterValueAtControlPoint.end(), w);

  // se non e' stato possibile trovare w nel vettore ritorna errore
  if (first == m_parameterValueAtControlPoint.end()) return true;
  /* FAB
double
found = *first;
assert(found  <=  *first);
//*/
  // individuo il punto di controllo che compete alla posizione nel vettore
  int controlPointOffset =
      distance(m_parameterValueAtControlPoint.begin(), first);

  // individuo il cionco relativo al punto di controllo
  chunk = retrieveChunkFromControlPointIndex(controlPointOffset);

  // calcolo il parametro relativo al cionco
  DoublePair p = retrieveParametersFromChunk(chunk);
  /* FAB
assert( p.first <= w &&
    w <= p.second );

#ifdef  _DEBUG
chunk = retrieveChunkFromControlPointIndex( controlPointOffset );
p = retrieveParametersFromChunk( chunk );
#endif
//*/

  if (w < p.first || w > p.second) {
    t = (p.first + p.second) * 0.5;
  } else
    t = proportion(1.0, w - p.first, p.second - p.first);

  /* FAB
assert( 0.0 <= t && t <= 1.0 );
//*/

  return false;
}

//-----------------------------------------------------------------------------

TRectD TStroke::Imp::computeCenterlineBBox() {
  UINT n = m_centerLineArray.size();
  if (m_centerLineArray.empty()) return TRectD();
  TQuadratic q(m_centerLineArray[0]->getP0(), m_centerLineArray[0]->getP1(),
               m_centerLineArray[0]->getP2());
  TRectD bbox = q.getBBox();
  for (UINT i = 1; i < n; i++) {
    q = TQuadratic(m_centerLineArray[i]->getP0(), m_centerLineArray[i]->getP1(),
                   m_centerLineArray[i]->getP2());
    bbox += q.getBBox();
  }
  return bbox;
}

//-----------------------------------------------------------------------------

TRectD TStroke::Imp::computeSubBBox(double w0, double w1) const {
  if (m_centerLineArray.empty()) return TRectD();

  int n = m_centerLineArray.size();

  TRectD bBox;
  const double eps = 0.000000001;
  int i;

  if (w0 > w1) tswap(w0, w1);

  double nw0 = w0 * n;
  double nw1 = w1 * n;

  int i0 = (int)nw0;  // indice della quadrica che contiene w0
  int i1 = (int)nw1;  // idem per w1

  double t0 =
      nw0 -
      (double)i0;  // parametro di w0 rispetto alla quadrica che lo contiene
  double t1 = nw1 - (double)i1;  // idem per w1

  if (t0 < eps)  // se t0 e' quasi uguale a zero, evito di fare lo split e
                 // considero tutta la quadrica i0
  {
    i0--;
    t0 = 1.0;
  }
  if (t1 > (1 - eps))  // se t1 e' quasi uguale a uno, evito di fare lo split e
                       // considero tutta la quadrica i1
  {
    i1++;
    t1 = 0.0;
  }

  TThickQuadratic quadratic1, quadratic2, quadratic3;

  if (i0 == i1)  // i due punti di taglio capitano nella stessa quadratica
  {
    if (t0 < eps && t1 > (1 - eps)) return m_centerLineArray[i0]->getBBox();

    if (t0 < eps) {
      m_centerLineArray[i0]->split(t1, quadratic1, quadratic2);
      return quadratic1.getBBox();
    }

    if (t1 > (1 - eps)) {
      m_centerLineArray[i0]->split(t0, quadratic1, quadratic2);
      return quadratic2.getBBox();
    }

    // quadratic1 e' la quadratica risultante dallo split tra t0 e t1 di
    // m_centerLineArray[i0]
    m_centerLineArray[i0]->split(t0, quadratic1, quadratic2);
    quadratic2.split((t1 - t0) / (1 - t0), quadratic1, quadratic3);

    return quadratic1.getBBox();
  }

  // se i due punti di taglio capitano in quadratiche diverse

  // sommo le bbox di quelle interne
  for (i = i0 + 1; i < i1; i++) bBox += m_centerLineArray[i]->getBBox();

  // e sommo le bbox delle quadratiche splittate agli estremi se non sono
  // irrilevanti
  if (i0 >= 0 && t0 < (1 - eps)) {
    m_centerLineArray[i0]->split(t0, quadratic1, quadratic2);
    bBox += quadratic2.getBBox();
  }

  if (i1 < n && t1 > eps) {
    m_centerLineArray[i1]->split(t1, quadratic1, quadratic2);
    bBox += quadratic1.getBBox();
  }

  return bBox;
}

//-----------------------------------------------------------------------------

void TStroke::Imp::print(ostream &os) {
#if defined(_DEBUG) || defined(DEBUG)

  os << "m_isValidLength:" << m_isValidLength << endl;

  os << "m_isOutlineValid:" << m_isOutlineValid << endl;

  os << "m_areDisabledComputeOfCaches:" << m_areDisabledComputeOfCaches << endl;

  os << "m_bBox:" << m_bBox << endl;

  os << "m_partialLengthArray";
  printContainer(os, m_partialLengthArray);
  os << endl;

  os << "m_parameterValueAtControlPoint";
  printContainer(os, m_parameterValueAtControlPoint);
  os << endl;

  os << "m_centerLineArray";
  // os.setf(myIOFlags::scientific);
  printContainerOfPointer(os, m_centerLineArray);
  os << endl;

/*
  vector<TPixel> m_outlineColorArray;

    vector<TPointD> m_texArray;

      TFilePath m_filePath;
      TRasterP  m_texture;
  */
// TSystem::outputDebug( os.str());
#else
#endif
}

//=============================================================================

//  Needed to DEBUG
DEFINE_CLASS_CODE(TStroke, 15)

//-----------------------------------------------------------------------------

// Costructor
TStroke::TStroke() : TSmartObject(m_classCode) {
  vector<TThickPoint> p(3);
  p[0] = TThickPoint(0, 0, 0);
  p[1] = p[0];
  p[2] = p[1];

  m_imp.reset(new TStroke::Imp(p));

  /*
// da fissare deve trovarsi prima della init
m_imp->m_centerLineArray.push_back ( new TThickQuadratic );
*/
}

//-----------------------------------------------------------------------------

// Build a stroke from a set of ThickPoint
TStroke::TStroke(const vector<TThickPoint> &v)
    : TSmartObject(m_classCode), m_imp(new TStroke::Imp(v)) {}

//-----------------------------------------------------------------------------

TStroke::TStroke(const vector<TPointD> &v)
    : TSmartObject(m_classCode), m_imp(new TStroke::Imp(v)) {}

//-----------------------------------------------------------------------------

TStroke::~TStroke() {}

//-----------------------------------------------------------------------------

TStroke::TStroke(const TStroke &other)
    : TSmartObject(m_classCode), m_imp(new TStroke::Imp()) {
  m_imp->m_bBox           = other.getBBox();
  m_imp->m_isValidLength  = other.m_imp->m_isValidLength;
  m_imp->m_isOutlineValid = other.m_imp->m_isOutlineValid;
  m_imp->m_areDisabledComputeOfCaches =
      other.m_imp->m_areDisabledComputeOfCaches;
  m_imp->m_flag           = other.m_imp->m_flag;
  m_imp->m_outlineOptions = other.m_imp->m_outlineOptions;

  // Are they sure as regards exceptions ?
  m_imp->m_centerLineArray.resize(other.m_imp->m_centerLineArray.size());
  int i;
  for (i = 0; i < (int)other.m_imp->m_centerLineArray.size(); i++)
    m_imp->m_centerLineArray[i] =
        new TThickQuadratic(*other.m_imp->m_centerLineArray[i]);

  // copy(  other.m_imp->m_centerLineArray.begin(),
  //   other.m_imp->m_centerLineArray.end(),
  //   TThickQuadraticArrayInsertIterator(m_imp->m_centerLineArray));

  copy(other.m_imp->m_partialLengthArray.begin(),
       other.m_imp->m_partialLengthArray.end(),
       back_inserter<DoubleArray>(m_imp->m_partialLengthArray));
  copy(other.m_imp->m_parameterValueAtControlPoint.begin(),
       other.m_imp->m_parameterValueAtControlPoint.end(),
       back_inserter<DoubleArray>(m_imp->m_parameterValueAtControlPoint));

  m_imp->m_styleId = other.m_imp->m_styleId;
  m_imp->m_prop =
      0;  // other.m_imp->m_prop ? other.m_imp->m_prop->clone(this) : 0;
  m_imp->m_selfLoop                = other.m_imp->m_selfLoop;
  m_imp->m_negativeThicknessPoints = other.m_imp->m_negativeThicknessPoints;
}

//-----------------------------------------------------------------------------

TStroke &TStroke::operator=(const TStroke &other) {
  TStroke temp(other);
  swap(temp);
  return *this;
}

//-----------------------------------------------------------------------------

bool TStroke::getNearestW(const TPointD &p, double &outW, double &dist2,
                          bool checkBBox) const {
  double outT;
  int chunkIndex;
  bool ret      = getNearestChunk(p, outT, chunkIndex, dist2, checkBBox);
  if (ret) outW = getW(chunkIndex, outT);
  return ret;
}

bool TStroke::getNearestChunk(const TPointD &p, double &outT, int &chunkIndex,
                              double &dist2, bool checkBBox) const {
  dist2 = (numeric_limits<double>::max)();

  for (UINT i = 0; i < m_imp->m_centerLineArray.size(); i++) {
    if (checkBBox &&
        !m_imp->m_centerLineArray[i]->getBBox().enlarge(30).contains(p))
      continue;

    double t    = (m_imp->m_centerLineArray)[i]->getT(p);
    double dist = tdistance2((m_imp->m_centerLineArray)[i]->getPoint(t), p);

    if (dist < dist2) {
      dist2      = dist;
      chunkIndex = i;
      outT       = t;
    }
  }

  return dist2 < (numeric_limits<double>::max)();
}

//-----------------------------------------------------------------------------
// finds all points on stroke which are "enough" close to point p. return the
// number of such points.

int TStroke::getNearChunks(const TThickPoint &p,
                           vector<TThickPoint> &pointsOnStroke,
                           bool checkBBox) const {
  int currIndex    = -100;
  double currDist2 = 100000;

  for (UINT i = 0; i < m_imp->m_centerLineArray.size(); i++) {
    TThickQuadratic *q = m_imp->m_centerLineArray[i];

    if (checkBBox && !q->getBBox().enlarge(30).contains(p)) continue;

    double t       = q->getT(p);
    TThickPoint p1 = q->getThickPoint(t);
    double dist2   = tdistance2(p1, p);

    if (dist2 < (p1.thick + p.thick + 5) * (p1.thick + p.thick + 5)) {
      if (!pointsOnStroke.empty() &&
          areAlmostEqual(p1, pointsOnStroke.back(), 1e-3))
        continue;

      if (currIndex == i - 1) {
        if (dist2 < currDist2)
          pointsOnStroke.pop_back();
        else
          continue;
      }

      currIndex = i;
      currDist2 = dist2;
      pointsOnStroke.push_back(p1);
    }
  }

  return pointsOnStroke.size();  // dist2 < (numeric_limits<double>::max)();
}

//-----------------------------------------------------------------------------

void TStroke::getControlPoints(vector<TThickPoint> &v) const {
  assert(v.empty());
  v.resize(m_imp->m_centerLineArray.size() * 2 + 1);

  v[0] = m_imp->m_centerLineArray[0]->getThickP0();

  for (UINT i = 0; i < m_imp->m_centerLineArray.size(); i++) {
    TThickQuadratic *q = m_imp->m_centerLineArray[i];
    v[2 * i + 1]       = q->getThickP1();
    v[2 * i + 2]       = q->getThickP2();
  }
}

TThickPoint TStroke::getControlPoint(int n) const {
  if (n <= 0) return m_imp->m_centerLineArray.front()->getThickP0();

  if (n >= getControlPointCount())
    return m_imp->m_centerLineArray.back()->getThickP2();

  // calcolo l'offset del chunk risolvendo l'equazione
  //  2 * chunkNumber + 1 = n
  //  chunkNumber = tceil((n - 1) / 2)
  int chunkNumber = tceil((n - 1) * 0.5);
  assert(chunkNumber <= getChunkCount());

  int pointOffset = n - chunkNumber * 2;

  if (chunkNumber == getChunkCount())  // e' l'ultimo punto della stroke
    return getChunk(chunkNumber - 1)->getThickP2();

  switch (pointOffset) {
  case 0:
    return getChunk(chunkNumber)->getThickP0();
  case 1:
    return getChunk(chunkNumber)->getThickP1();
  case 2:
    return getChunk(chunkNumber)->getThickP2();
  }

  assert("Not yet finished" && false);
  return getControlPoint(0);
}

//-----------------------------------------------------------------------------

TThickPoint TStroke::getControlPointAtParameter(double w) const {
  if (w <= 0) return m_imp->m_centerLineArray.front()->getThickP0();

  if (w >= 1.0) return m_imp->m_centerLineArray.back()->getThickP2();

  vector<double>::iterator it_begin =
                               m_imp->m_parameterValueAtControlPoint.begin(),
                           first,
                           it_end = m_imp->m_parameterValueAtControlPoint.end();

  // find iterator at position greater or equal to w
  first = std::lower_bound(it_begin, it_end, w);

  assert(first != it_end);

  // now is possible to get control point
  // if( areAlmostEqual(*first, w, 0.1) )
  //  return  getControlPoint( distance(it_begin, first) );
  if (first == it_begin)
    return getControlPoint(0);
  else if ((*first - w) <= w - *(first - 1))
    return getControlPoint(distance(it_begin, first));
  else
    return getControlPoint(distance(it_begin, first - 1));
}

//-----------------------------------------------------------------------------

int TStroke::getControlPointIndexAfterParameter(double w) const {
  const vector<double>::const_iterator
      begin = m_imp->m_parameterValueAtControlPoint.begin(),
      end   = m_imp->m_parameterValueAtControlPoint.end();

  vector<double>::const_iterator it = std::upper_bound(begin, end, w);

  if (it == end)
    return getControlPointCount();
  else
    return std::distance(begin, it);
}

//-----------------------------------------------------------------------------

void TStroke::setControlPoint(int n, const TThickPoint &pos) {
  assert(n >= 0);
  assert(n < getControlPointCount());
  if (n < 0 || n >= getControlPointCount()) return;

  invalidate();

  QuadStrokeChunkArray &chunkArray = m_imp->m_centerLineArray;

  if (getControlPoint(n).thick <= 0 && pos.thick > 0)
    m_imp->m_negativeThicknessPoints--;
  else if (getControlPoint(n).thick > 0 && pos.thick <= 0)
    m_imp->m_negativeThicknessPoints++;

  if (n == 0) {
    chunkArray[0]->setThickP0(pos);
    // m_imp->computeBBox();
    return;
  }

  int chunkNumber = tceil((n - 1) * 0.5);
  assert(chunkNumber <= getChunkCount());

  int pointOffset = n - chunkNumber * 2;

  if (chunkNumber == getChunkCount())  // e' l'ultimo punto della stroke
  {
    chunkArray[chunkNumber - 1]->setThickP2(pos);
    // m_imp->computeBBox();
    return;
  }

  if (0 == pointOffset) {
    chunkArray[chunkNumber]->setThickP0(pos);

    if (chunkNumber >= 1) {
      chunkNumber--;
      chunkArray[chunkNumber]->setThickP2(pos);
    }
  } else if (1 == pointOffset)
    chunkArray[chunkNumber]->setThickP1(pos);
  else if (2 == pointOffset) {
    chunkArray[chunkNumber]->setThickP2(pos);

    if (chunkNumber < getChunkCount() - 1) {
      chunkNumber++;
      chunkArray[chunkNumber]->setThickP0(pos);
    }
  }
  // m_imp->computeBBox();
}

//-----------------------------------------------------------------------------

//! Ridisegna lo stroke
void TStroke::reshape(const TThickPoint pos[], int count) {
  // count deve essere dispari e maggiore o uguale a tre
  assert(count >= 3);
  assert(count & 1);
  QuadStrokeChunkArray &chunkArray = m_imp->m_centerLineArray;
  clearPointerContainer(chunkArray);

  m_imp->m_negativeThicknessPoints = 0;
  for (int i = 0; i < count - 1; i += 2) {
    chunkArray.push_back(new TThickQuadratic(pos[i], pos[i + 1], pos[i + 2]));
    if (pos[i].thick <= 0) m_imp->m_negativeThicknessPoints++;
    if (pos[i + 1].thick <= 0) m_imp->m_negativeThicknessPoints++;
  }
  if (pos[count - 1].thick <= 0) m_imp->m_negativeThicknessPoints++;

  invalidate();
  // m_imp->computeBBox();
  m_imp->computeParameterInControlPoint();
}

//-----------------------------------------------------------------------------

double TStroke::getApproximateLength(double w0, double w1, double error) const {
  m_imp->computeCacheVector();

  assert((int)m_imp->m_partialLengthArray.size() == getControlPointCount());

  if (w0 == w1) return 0.0;

  w0 = min(max(0.0, w0), 1.0);
  w1 = min(max(0.0, w1), 1.0);

  if (w0 > w1) std::swap(w0, w1);

  // vede se la lunghezza e' individuabile nella cache
  if (0.0 == w0) {
    vector<double>::iterator first;

    // trova l'iteratore alla prima posizione che risulta maggiore di w
    first = std::upper_bound(m_imp->m_parameterValueAtControlPoint.begin(),
                             m_imp->m_parameterValueAtControlPoint.end(),
                             w1 - TConsts::epsilon);

    if (first != m_imp->m_parameterValueAtControlPoint.end() &&
        *first < w1 + TConsts::epsilon) {
      int offset =
          distance(m_imp->m_parameterValueAtControlPoint.begin(), first);
      return m_imp->m_partialLengthArray[offset];
    }
  }

  int firstChunk, secondChunk;
  double firstT, secondT;

  // calcolo i chunk interessati ed i valori del parametro t
  bool val1 = m_imp->retrieveChunkAndItsParamameter(w0, firstChunk, firstT);
  assert(val1);

  bool val2 = m_imp->retrieveChunkAndItsParamameter(w1, secondChunk, secondT);
  assert(val2);

  if (firstChunk == secondChunk)
    return getChunk(firstChunk)->getApproximateLength(firstT, secondT, error);

  double totalLength = 0;

  totalLength += getChunk(firstChunk)->getApproximateLength(firstT, 1, error);

  // lunghezza dei pezzi intermedi
  for (int i = firstChunk + 1; i != secondChunk; i++)
    totalLength += getChunk(i)->getApproximateLength(0.0, 1.0, error);

  totalLength +=
      getChunk(secondChunk)->getApproximateLength(0.0, secondT, error);

  return totalLength;
}

//-----------------------------------------------------------------------------

double TStroke::getLength(double w0, double w1) const {
  if (w0 == w1) return 0.0;

  // If necessary, swap values
  w0 = min(max(0.0, w0), 1.0);
  w1 = min(max(0.0, w1), 1.0);

  if (w0 > w1) std::swap(w0, w1);

  // Retrieve s1
  int chunk;
  double t;

  bool ok = !m_imp->retrieveChunkAndItsParamameter(w1, chunk, t);
  assert(ok);

  double s1 = getLength(chunk, t);

  if (w0 == 0.0) return s1;

  // Retrieve s0
  ok = !m_imp->retrieveChunkAndItsParamameter(w0, chunk, t);
  assert(ok);

  return s1 - getLength(chunk, t);
}

//-----------------------------------------------------------------------------

double TStroke::getLength(int chunk, double t) const {
  // Compute length caches
  m_imp->computeCacheVector();
  assert((int)m_imp->m_partialLengthArray.size() == getControlPointCount());

  if (t == 1.0) ++chunk, t = 0.0;

  double s = m_imp->m_partialLengthArray[chunk << 1];
  if (t > 0.0) s += getChunk(chunk)->getLength(t);

  return s;
}

//-----------------------------------------------------------------------------

void TStroke::invalidate() {
  m_imp->m_maxThickness   = -1;
  m_imp->m_isOutlineValid = false;
  m_imp->m_isValidLength  = false;
  m_imp->m_flag           = m_imp->m_flag | c_dirty_flag;
  if (m_imp->m_prop) m_imp->m_prop->notifyStrokeChange();
}

//-----------------------------------------------------------------------------
/*!
N.B. Questa funzione e' piu' lenta rispetto alla insertCP
perche' ricerca la posizione di s con un algoritmo di bisezione.
*/
void TStroke::insertControlPointsAtLength(double s) {
  if (0 > s || s > getLength()) return;

  int chunk;
  double t;

  // cerca il cionco ed il parametro alla lunghezza s
  if (!m_imp->retrieveChunkAndItsParamameterAtLength(s, chunk, t)) {
    if (isAlmostZero(t) || areAlmostEqual(t, 1)) return;

    // calcolo i due "cionchi"
    TThickQuadratic *tqfirst  = new TThickQuadratic,
                    *tqsecond = new TThickQuadratic;

    getChunk(chunk)->split(t, *tqfirst, *tqsecond);

    double parameterInStroke;

    if (0 == chunk)
      parameterInStroke = m_imp->getW(2) * t;
    else
      parameterInStroke =
          t * m_imp->getW((chunk + 1) * 2) + (1 - t) * m_imp->getW(chunk * 2);

    m_imp->updateParameterValue(parameterInStroke, chunk, tqfirst, tqsecond);

    // recupero la posizione nella lista delle curve
    QuadStrokeChunkArray::iterator it = m_imp->m_centerLineArray.begin();

    // elimino la curva vecchia
    advance(it, chunk);
    delete *it;
    it = m_imp->m_centerLineArray.erase(it);

    // ed aggiungo le nuove
    it = m_imp->m_centerLineArray.insert(it, tqsecond);
    it = m_imp->m_centerLineArray.insert(it, tqfirst);
  }
  /* FAB
#ifdef  _DEBUG
{
const int
size = m_imp->m_parameterValueAtControlPoint.size();
double
prev = m_imp->m_parameterValueAtControlPoint[0];
for( int i = 1;
   i < size;
   ++i )
{
assert( prev <= m_imp->m_parameterValueAtControlPoint[i] );
prev = m_imp->m_parameterValueAtControlPoint[i];
}
}
#endif
//*/
  invalidate();
}

//-----------------------------------------------------------------------------

void TStroke::insertControlPoints(double w) {
  if (0.0 > w || w > 1.0) return;

  int chunk;
  double tOfDivision = -1;

  if (m_imp->retrieveChunkAndItsParamameter(w, chunk, tOfDivision)) return;

  if (isAlmostZero(tOfDivision) || areAlmostEqual(tOfDivision, 1)) return;

  assert(0 <= chunk && chunk < getChunkCount());
  assert(0 <= tOfDivision && tOfDivision <= 1.0);

  // calcolo i due "cionchi"
  TThickQuadratic *tqfirst  = new TThickQuadratic,
                  *tqsecond = new TThickQuadratic;

  getChunk(chunk)->split(tOfDivision, *tqfirst, *tqsecond);

  m_imp->updateParameterValue(w, chunk, tqfirst, tqsecond);

  // recupero la posizione nella lista delle curve
  QuadStrokeChunkArray::iterator it = m_imp->m_centerLineArray.begin();

  // elimino la curva vecchia
  advance(it, chunk);
  delete *it;
  it = m_imp->m_centerLineArray.erase(it);

  // ed aggiungo le nuove
  it = m_imp->m_centerLineArray.insert(it, tqsecond);
  m_imp->m_centerLineArray.insert(it, tqfirst);

  invalidate();
  m_imp->computeCacheVector();

  m_imp->m_negativeThicknessPoints = 0;
  for (UINT j = 0; j < m_imp->m_centerLineArray.size(); j++) {
    if (m_imp->m_centerLineArray[j]->getThickP0().thick <= 0)
      m_imp->m_negativeThicknessPoints++;
    if (m_imp->m_centerLineArray[j]->getThickP1().thick <= 0)
      m_imp->m_negativeThicknessPoints++;
  }
  if (!m_imp->m_centerLineArray.empty() &&
      m_imp->m_centerLineArray.back()->getThickP2().thick <= 0)
    m_imp->m_negativeThicknessPoints++;
}

//-----------------------------------------------------------------------------

void TStroke::reduceControlPoints(double error, vector<int> corners) {
  double step, quadLen;
  vector<TThickPoint> tempVect, controlPoints;
  TStroke *tempStroke = 0;
  double missedLen    = 0;
  UINT cp, nextQuad, quadI, cornI, cpSize, size;

  const TThickQuadratic *quad = m_imp->m_centerLineArray.front();

  size = corners.size();
  assert(size > 1);
  if (size < 2) {
    // Have at least the first and last stroke points as corners
    corners.resize(2);
    corners[0] = 0;
    corners[1] = m_imp->m_centerLineArray.size();
  }

  // For every corners interval
  for (cornI = 0; cornI < size - 1; ++cornI) {
    tempVect.clear();

    nextQuad = corners[cornI + 1];
    if (nextQuad > m_imp->m_centerLineArray.size()) {
      assert(!"bad quadric index");
      return;
    }
    if (corners[cornI] >= (int)m_imp->m_centerLineArray.size()) {
      assert(!"bad quadric index");
      return;
    }

    for (quadI = corners[cornI]; quadI < nextQuad; quadI++) {
      quad    = getChunk(quadI);
      quadLen = quad->getLength();

      missedLen += quadLen;
      if (quadLen && (missedLen > 1 || quadI == 0 ||
                      quadI == nextQuad - 1))  // err instead of 1?
      {
        missedLen = 0;
        step      = 1.0 / quadLen;  // err instead of 1.0?

        // si, lo so che t non e' lineare sulla lunghezza, ma secondo me
        // funziona benissimo
        // cosi'. tanto devo interpolare dei punto e non e' richiesto che siano
        // a distanze
        // simili. e poi difficilmete il punto p1 di una quadratica e' cosi'
        // asimmetrico
        for (double t = 0; t < 1.0; t += step)
          tempVect.push_back(quad->getThickPoint(t));
      }
    }
    tempVect.push_back(quad->getThickP2());
    tempStroke = TStroke::interpolate(tempVect, error, false);

    cpSize = tempStroke->getControlPointCount();
    for (cp = 0; cp < cpSize - 1; cp++) {
      controlPoints.push_back(tempStroke->getControlPoint(cp));
    }
    delete tempStroke;
    tempStroke = 0;
  }
  controlPoints.push_back(m_imp->m_centerLineArray.back()->getThickP2());

#ifdef _DEBUG
  cpSize = controlPoints.size();
  for (cp = 1; cp < cpSize; cp++)
    assert(!(controlPoints[cp - 1] == controlPoints[cp]));
#endif

  reshape(&(controlPoints[0]), controlPoints.size());
  invalidate();
}

//-----------------------------------------------------------------------------

void TStroke::reduceControlPoints(double error) {
  vector<int> corners;
  corners.push_back(0);
  detectCorners(this, 10, corners);
  corners.push_back(getChunkCount());
  reduceControlPoints(error, corners);
}

//-----------------------------------------------------------------------------

double TStroke::getAverageThickness() const {
  return m_imp->m_averageThickness;
}

double TStroke::getMaxThickness() {
  if (m_imp->m_maxThickness == -1) m_imp->computeMaxThickness();
  return m_imp->m_maxThickness;
}

void TStroke::setAverageThickness(double thickness) {
  m_imp->m_averageThickness = thickness;
}

bool TStroke::operator==(const TStroke &s) const {
  if (getChunkCount() != s.getChunkCount()) return false;
  int i;
  for (i = 0; i < getChunkCount(); i++) {
    const TThickQuadratic *chanck  = getChunk(i);
    const TThickQuadratic *sChanck = s.getChunk(i);
    if (chanck->getThickP0() != sChanck->getThickP0() ||
        chanck->getThickP1() != sChanck->getThickP1() ||
        chanck->getThickP2() != sChanck->getThickP2())
      return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

int TStroke::getChunkCount() const { return m_imp->getChunkCount(); }

//-----------------------------------------------------------------------------

int TStroke::getControlPointCount() const {
  return m_imp->getControlPointCount();
}

//-----------------------------------------------------------------------------

const TThickQuadratic *TStroke::getChunk(int index) const {
  return m_imp->getChunk(index);
}

//-----------------------------------------------------------------------------

TRectD TStroke::getBBox(double w0, double w1) const {
  if (w0 > w1) tswap(w0, w1);

  if (w0 != 0.0 || w1 != 1.0) return m_imp->computeSubBBox(w0, w1);

  if (m_imp->m_flag & c_dirty_flag) ((TStroke *)this)->computeBBox();

  return m_imp->m_bBox;
}

//-----------------------------------------------------------------------------

void TStroke::computeBBox() {
  m_imp->m_bBox = TOutlineUtil::computeBBox(*this);
  m_imp->m_flag &= ~c_dirty_flag;
}

//-----------------------------------------------------------------------------

TRectD TStroke::getCenterlineBBox() const {
  return m_imp->computeCenterlineBBox();
}

//-----------------------------------------------------------------------------

void TStroke::disableComputeOfCaches() {
  m_imp->m_areDisabledComputeOfCaches = true;
}

//-----------------------------------------------------------------------------

void TStroke::enableComputeOfCaches() {
  m_imp->m_areDisabledComputeOfCaches = false;
}

//-----------------------------------------------------------------------------

// DEL double TStroke::getDistance2(const   TPointD &p,
// DEL                              double  maxDistance2,
// DEL                              UINT    &chunkIndex,
// DEL                              double  &currT)
// DEL {
// DEL   TRectD rect = getBBox();
// DEL
// DEL   if (!rect.contains(p))
// DEL   {
// DEL     double dist21 = tdistance2(p, rect.getP00());
// DEL     double dist22 = tdistance2(p, rect.getP01());
// DEL     double dist23 = tdistance2(p, rect.getP10());
// DEL     double dist24 = tdistance2(p, rect.getP11());
// DEL
// DEL     if (std::min(dist21, dist22, dist23, dist24)>=maxDistance2)
// DEL       return maxDistance2;
// DEL   }
// DEL
// DEL   double distance2, curMaxDistance2=maxDistance2;
// DEL
// DEL   for (UINT i=0; i < m_imp->m_centerLineArray.size(); i++)
// DEL   {
// DEL     distance2 = getTQDistance2(*(m_imp->m_centerLineArray)[i], p,
// maxDistance2, currT);
// DEL     if (distance2 < curMaxDistance2)
// DEL     {
// DEL       curMaxDistance2 = distance2;
// DEL       chunkIndex = i;
// DEL     }
// DEL   }
// DEL
// DEL   return curMaxDistance2;
// DEL }

//-----------------------------------------------------------------------------

TThickPoint TStroke::getThickPointAtLength(double s) const {
  assert(!m_imp->m_centerLineArray.empty());

  if (s <= 0) return getControlPoint(0);

  if (s >= getLength()) return getControlPoint(getControlPointCount() - 1);

  int chunk;
  double tOfDivision;

  bool error =
      m_imp->retrieveChunkAndItsParamameterAtLength(s, chunk, tOfDivision);

  if (error)
    error =
        m_imp->retrieveChunkAndItsParamameterAtLength(s, chunk, tOfDivision);

  assert(!error);

  if (error) return getControlPoint(0);

  return getChunk(chunk)->getThickPoint(tOfDivision);
}

//-----------------------------------------------------------------------------

TThickPoint TStroke::getThickPoint(double w) const {
  assert(!m_imp->m_centerLineArray.empty());

  if (w < 0) return getControlPoint(0);

  if (w > 1.0) return getControlPoint(getControlPointCount() - 1);

  int chunk = 0;
  double t  = 0;

  bool error = m_imp->retrieveChunkAndItsParamameter(w, chunk, t);

  assert(!error);

  if (error) return getControlPoint(0);

  return getChunk(chunk)->getThickPoint(t);
}

//-----------------------------------------------------------------------------

double TStroke::getParameterAtLength(double s) const {
  if (s <= 0)
    return 0;
  else if (s >= getLength())
    return 1;

  int chunk;
  double t;

  if (!m_imp->retrieveChunkAndItsParamameterAtLength(s, chunk, t)) {
    DoublePair p = m_imp->retrieveParametersFromChunk(chunk);

    return proportion(p.second, t, 1.0, p.first);
  } else if (chunk < (int)getChunkCount() && t == -1)
    return getParameterAtControlPoint(2 * chunk);

  return 1.0;
}

//-----------------------------------------------------------------------------

double TStroke::getParameterAtControlPoint(int n) const {
  double out = -1;

  if (0 <= n && n < getControlPointCount()) out = m_imp->getW(n);
  /* FAB
assert( 0.0 <= out &&
    out <= 1.0 );
*/
  if (0.0 > out) return 0.0;
  if (out > 1.0) return 1.0;
  return out;
}

//-----------------------------------------------------------------------------

double TStroke::getW(int chunkIndex, double t) const {
  DoublePair parRange = m_imp->retrieveParametersFromChunk(chunkIndex);

  double w = proportion(parRange.second, t, 1.0, parRange.first);

  assert(0 <= w && w <= 1.0);

  return w;
}

//-----------------------------------------------------------------------------

double TStroke::getW(const TPointD &p) const {
  int chunkIndex;

  double tOfchunk, distance2 = (numeric_limits<double>::max)();

  // cerca il chunk piu' vicino senza testare la BBox
  getNearestChunk(p, tOfchunk, chunkIndex, distance2, false);

  assert(0 <= chunkIndex && chunkIndex <= getChunkCount());

  DoublePair parRange = m_imp->retrieveParametersFromChunk(chunkIndex);

  double t = proportion(parRange.second, tOfchunk, 1.0, parRange.first);

  assert(0 <= t && t <= 1.0);

  return t;
}

//-----------------------------------------------------------------------------

bool TStroke::getSpeedTwoValues(double w, TPointD &speed0,
                                TPointD &speed1) const {
  bool ret = false;

  assert(!m_imp->m_centerLineArray.empty());

  if (w < 0) {
    speed0 = m_imp->m_centerLineArray.front()->getSpeed(0.0);
    return ret;
  }

  if (w > 1.0) {
    speed0 = m_imp->m_centerLineArray.back()->getSpeed(1.0);
    return ret;
  }

  int chunk;
  double t;

  bool error = m_imp->retrieveChunkAndItsParamameter(w, chunk, t);

  assert(!error);

  if (error) {
    speed0 = m_imp->m_centerLineArray.front()->getSpeed(0.0);
    speed1 = -speed0;
    return ret;
  }

  speed0 = getChunk(chunk)->getSpeed(t);
  speed1 = -speed0;

  if (areAlmostEqual(t, 0.0, 1e-9) && chunk > 0 &&
      (speed1 = -getChunk(chunk - 1)->getSpeed(1.0)) != -speed0)
    ret = true;
  else if (areAlmostEqual(t, 1.0, 1e-9) && chunk < getChunkCount() - 1 &&
           (speed1 = -getChunk(chunk + 1)->getSpeed(0.0)) != -speed0) {
    TPointD aux = -speed0;
    speed0      = -speed1;
    speed1      = aux;
    ret         = true;
  }

  if (speed0 == TPointD())  // la quadratica e' degenere!!!
  {
    while (speed0 == TPointD()) {
      speed0 = getChunk(chunk--)->getSpeed(1.0);
      if (chunk <= 0) break;
    }
    chunk = 0;
    while (speed0 == TPointD()) {
      speed0 = getChunk(chunk++)->getSpeed(0.0);
      if (chunk >= getChunkCount() - 1) break;
    }

    if (speed0 == TPointD()) {
      if (getChunkCount() == 1) {
        const TThickQuadratic *q = getChunk(0);

        if (q->getP0() == q->getP1() && q->getP1() != q->getP2())
          speed0 = TSegment(q->getP1(), q->getP2()).getSpeed(t);
        else if (q->getP1() == q->getP2() && q->getP0() != q->getP1())
          speed0 = TSegment(q->getP0(), q->getP1()).getSpeed(t);
        else
          assert(speed0 != TPointD());
      } else
        assert(speed0 != TPointD());
    }
  }
  return ret;
}

//-----------------------------------------------------------------------------
TPointD TStroke::getSpeed(double w, bool outSpeed) const {
  assert(!m_imp->m_centerLineArray.empty());

  if (w < 0) return m_imp->m_centerLineArray.front()->getSpeed(0.0);

  if (w > 1.0) return m_imp->m_centerLineArray.back()->getSpeed(1.0);

  int chunk;
  double t;

  bool error = m_imp->retrieveChunkAndItsParamameter(w, chunk, t);
  if (t == 1 && outSpeed && chunk < getChunkCount() - 1) {
    chunk++;
    t = 0;
  }
  assert(!error);

  if (error) return m_imp->m_centerLineArray.front()->getSpeed(0.0);

  TPointD speed = getChunk(chunk)->getSpeed(t);

  if (speed == TPointD())  // la quadratica e' degenere!!!
  {
    while (speed == TPointD()) {
      speed = getChunk(chunk--)->getSpeed(1.0);
      if (chunk <= 0) break;
    }
    chunk = 0;
    while (speed == TPointD()) {
      speed = getChunk(chunk++)->getSpeed(0.0);
      if (chunk >= getChunkCount() - 1) break;
    }
    if (speed == TPointD()) {
      if (getChunkCount() == 1) {
        const TThickQuadratic *q = getChunk(0);

        if (q->getP0() == q->getP1() && q->getP1() != q->getP2())
          return TSegment(q->getP1(), q->getP2()).getSpeed(t);
        else if (q->getP1() == q->getP2() && q->getP0() != q->getP1())
          return TSegment(q->getP0(), q->getP1()).getSpeed(t);
        else
          assert(speed != TPointD());
      } else
        assert(speed != TPointD());
    }
  }
  return speed;
}

//-----------------------------------------------------------------------------

TPointD TStroke::getSpeedAtLength(double s) const {
  double t = getParameterAtLength(s);
  return getSpeed(t);
}

//-----------------------------------------------------------------------------

double TStroke::getLengthAtControlPoint(int n) const {
  m_imp->computeCacheVector();

  if (n >= getControlPointCount()) return m_imp->m_partialLengthArray.back();

  if (n <= 0) return m_imp->m_partialLengthArray.front();

  return m_imp->m_partialLengthArray[n];
}

//-----------------------------------------------------------------------------

void TStroke::split(double w, TStroke &f, TStroke &s) const {
  int chunk;
  double t;
  f.m_imp->m_maxThickness = -1;
  s.m_imp->m_maxThickness = -1;
  if (!m_imp->retrieveChunkAndItsParamameter(w, chunk, t)) {
    assert(0 <= chunk && chunk < getChunkCount());
    assert(0 <= w && w <= 1.0);
    assert(0 <= t && t <= 1.0);

    QuadStrokeChunkArray &chunkArray = m_imp->m_centerLineArray;

    // build two temporary quadratic
    TThickQuadratic *tq1 = new TThickQuadratic, *tq2 = new TThickQuadratic;
    // make split
    chunkArray[chunk]->split(t, *tq1, *tq2);

    // a temporary vector of ThickQuadratic
    QuadStrokeChunkArray vTQ;

    // copy all chunk of stroke in a vTQ
    int i;
    for (i = 0; i < chunk; ++i) vTQ.push_back(chunkArray[i]);

    // not insert null length chunk (unless vTQ is empty....)
    if (tq1->getLength() != 0.0 || w == 0.0 || vTQ.empty()) vTQ.push_back(tq1);

    // build a temp and swap
    TStroke *ts1  = TStroke::create(vTQ);
    if (!ts1) ts1 = new TStroke;
    ts1->swapGeometry(f);

    // clear vector (chunks are now under TStroke control)
    vTQ.clear();

    // idem...
    if (tq2->getLength() != 0.0 || w == 1.0 || getChunkCount() == 0)
      vTQ.push_back(tq2);

    for (i = chunk + 1; i < getChunkCount(); ++i) vTQ.push_back(chunkArray[i]);

    TStroke *ts2  = TStroke::create(vTQ);
    if (!ts2) ts2 = new TStroke;
    ts2->swapGeometry(s);

    // Copy style infos
    f.setStyle(getStyle());
    s.setStyle(getStyle());
    f.outlineOptions() = s.outlineOptions() = outlineOptions();

    delete ts2;
    delete ts1;
    // delete temporary quadratic
    delete tq1;
    delete tq2;
    if (f.getControlPointCount() == 3 &&
        f.getControlPoint(0) !=
            f.getControlPoint(2))  // gli stroke con solo 1 chunk vengono fatti
                                   // dal tape tool...e devono venir
                                   // riconosciuti come speciali di autoclose
                                   // proprio dal fatto che hanno 1 solo chunk.
      f.insertControlPoints(0.5);
    if (s.getControlPointCount() == 3 &&
        s.getControlPoint(0) !=
            s.getControlPoint(2))  // gli stroke con solo 1 chunk vengono fatti
                                   // dal tape tool...e devono venir
                                   // riconosciuti come speciali di autoclose
                                   // proprio dal fatto che hanno 1 solo chunk.
      s.insertControlPoints(0.5);
  }
}

//-----------------------------------------------------------------------------

void TStroke::print(ostream &os) const {
  // m_imp->print(os);
  const TThickQuadratic *q;

  os << "Punti di controllo\n";
  for (int i = 0; i < getChunkCount(); ++i) {
    os << "quad #" << i << ":" << endl;
    q = getChunk(i);

    os << "    P0:" << q->getThickP0().x << ", " << q->getThickP0().y << ", "
       << q->getThickP0().thick << endl;
    os << "    P1:" << q->getThickP1().x << ", " << q->getThickP1().y << ", "
       << q->getThickP1().thick << endl;
    assert(i == getChunkCount() - 1 ||
           (getChunk(i)->getThickP2() == getChunk(i + 1)->getThickP0()));
  }

  q = getChunk(getChunkCount() - 1);

  os << "    P2:" << q->getThickP2().x << ", " << q->getThickP2().y << ", "
     << q->getThickP2().thick << endl;
}

//-----------------------------------------------------------------------------

void TStroke::transform(const TAffine &aff, bool doChangeThickness) {
  for (UINT i = 0; i < m_imp->m_centerLineArray.size(); ++i) {
    TThickQuadratic &ref = *m_imp->m_centerLineArray[i];
    ref                  = transformQuad(aff, ref, doChangeThickness);

    if (doChangeThickness) {
      double det                                     = aff.det();
      if (det == 0) m_imp->m_negativeThicknessPoints = getControlPointCount();
      if (m_imp->m_maxThickness != -1) m_imp->m_maxThickness *= sqrt(fabs(det));
      // else if(det<0)
      //  m_imp->m_negativeThicknessPoints=getControlPointCount()-m_imp->m_negativeThicknessPoints;
    }
  }

  invalidate();
}

//-----------------------------------------------------------------------------

/*
void TStroke::draw(const TVectorRenderData &rd) const
{
  if(! m_imp->m_styleId)
    return;

  TColorStyle * style = rd.m_palette->getStyle(m_imp->m_styleId);

  if( !style->isStrokeStyle() || style->isEnabled() == false )
    return;



  if( !m_imp->m_prop || style != m_imp->m_prop->getColorStyle() )
  {
    delete m_imp->m_prop;
    m_imp->m_prop = style->makeStrokeProp(this);
  }

  m_imp->m_prop->draw(rd);
}

*/

//-----------------------------------------------------------------------------

TStrokeProp *TStroke::getProp() const {
#if !defined(DISEGNO_OUTLINE)
  if (!m_imp->m_styleId) return 0;
#endif

  /*
TColorStyle * style = palette->getStyle(m_imp->m_styleId);
if( !style->isStrokeStyle() || style->isEnabled() == false )
return 0;

if( !m_imp->m_prop || style != m_imp->m_prop->getColorStyle() )
{
delete m_imp->m_prop;
m_imp->m_prop = style->makeStrokeProp(this);
}
*/
  return m_imp->m_prop;
}

//-----------------------------------------------------------------------------

void TStroke::setProp(TStrokeProp *prop) {
  assert(prop);
  delete m_imp->m_prop;
  m_imp->m_prop = prop;
}

//-----------------------------------------------------------------------------

int TStroke::getStyle() const { return m_imp->m_styleId; }

//-----------------------------------------------------------------------------

void TStroke::setStyle(int styleId) {
  m_imp->m_styleId = styleId;

  /*
if (!colorStyle || (colorStyle && colorStyle->isStrokeStyle())  )
{
m_imp->m_colorStyle = colorStyle;
delete m_imp->m_prop;
m_imp->m_prop = 0;
}
*/
}

//-----------------------------------------------------------------------------

TStroke &TStroke::changeDirection() {
  UINT chunkCount = getChunkCount();
  UINT to         = tfloor(chunkCount * 0.5);
  UINT i;

  if (chunkCount & 1) changeTQDirection(m_imp->m_centerLineArray[to]);

  --chunkCount;

  for (i = 0; i < to; ++i) {
    changeTQDirection(m_imp->m_centerLineArray[i]);
    changeTQDirection(m_imp->m_centerLineArray[chunkCount - i]);
    TThickQuadratic *q1         = m_imp->m_centerLineArray[i];
    m_imp->m_centerLineArray[i] = m_imp->m_centerLineArray[chunkCount - i];
    m_imp->m_centerLineArray[chunkCount - i] = q1;
  }
  invalidate();

  return *this;
}

//-----------------------------------------------------------------------------

void TStroke::setFlag(TStrokeFlag flag, bool status) {
  if (status)
    m_imp->m_flag |= flag;
  else
    m_imp->m_flag &= ~flag;
}

//-----------------------------------------------------------------------------

bool TStroke::getFlag(TStrokeFlag flag) const {
  return (m_imp->m_flag & flag) != 0;
}

//-----------------------------------------------------------------------------

void TStroke::swap(TStroke &ref) {
  std::swap(m_imp, ref.m_imp);

  // Stroke props need to update their stroke owners
  if (m_imp->m_prop) m_imp->m_prop->setStroke(this);
  if (ref.m_imp->m_prop) ref.m_imp->m_prop->setStroke(&ref);

  // The id is retained. This is coherent as the stroke id is supposedly
  // not an exchangeable info.
  std::swap(m_imp->m_id, ref.m_imp->m_id);
}

//-----------------------------------------------------------------------------

void TStroke::swapGeometry(TStroke &ref) { m_imp->swapGeometry(*ref.m_imp); }

//-----------------------------------------------------------------------------

int TStroke::getId() const { return m_imp->m_id; }

//-----------------------------------------------------------------------------

void TStroke::setId(int id) { m_imp->m_id = id; }

//-----------------------------------------------------------------------------

// magari poi la sposto in un altro file
TThickPoint TStroke::getCentroid() const {
  double totalLen = getLength();

  if (totalLen == 0) return getControlPoint(0);

  double step           = totalLen * 0.1;
  double len            = 0;
  if (step > 10.0) step = 10.0;
  int count             = 0;
  TThickPoint point;
  for (; len <= totalLen; len += step) {
    count++;
    point += getThickPointAtLength(len);
  }
  return point * (1.0 / (double)count);
}

//-----------------------------------------------------------------------------

void TStroke::setSelfLoop(bool loop) {
  if (loop) {
    // assert that a self loop is a stroke where first and last control points
    // are the same
    const int cpCount = this->getControlPointCount();

    TThickPoint p, p0 = this->getControlPoint(0),
                   pn = this->getControlPoint(cpCount - 1);

    p = (p0 + pn) * 0.5;

    this->setControlPoint(0, p);
    this->setControlPoint(cpCount - 1, p);
  }
  m_imp->m_selfLoop = loop;
}

//-----------------------------------------------------------------------------

bool TStroke::isSelfLoop() const { return m_imp->m_selfLoop; }

//-----------------------------------------------------------------------------

bool TStroke::isCenterLine() const {
  assert(m_imp->m_negativeThicknessPoints <= getControlPointCount());

  return m_imp->m_negativeThicknessPoints == getControlPointCount();
}

//-----------------------------------------------------------------------------

TStroke::OutlineOptions &TStroke::outlineOptions() {
  return m_imp->m_outlineOptions;
}

//-----------------------------------------------------------------------------

const TStroke::OutlineOptions &TStroke::outlineOptions() const {
  return m_imp->m_outlineOptions;
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

TStroke *joinStrokes(const TStroke *s0, const TStroke *s1) {
  TStroke *newStroke;

  if (s0 == s1) {
    newStroke = new TStroke(*s0);
    newStroke->setSelfLoop();
    return newStroke;
  }
  int i;
  vector<TThickPoint> v;
  for (i = 0; i < s0->getControlPointCount(); i++)
    v.push_back(s0->getControlPoint(i));
  if (areAlmostEqual(v.back(), s1->getControlPoint(0), 1e-3))
    for (i = 1; i < s1->getControlPointCount(); i++)
      v.push_back(s1->getControlPoint(i));
  else if (areAlmostEqual(v.back(),
                          s1->getControlPoint(s1->getControlPointCount() - 1),
                          1e-3))
    for (i = s1->getControlPointCount() - 2; i >= 0; i--)
      v.push_back(s1->getControlPoint(i));
  else
    assert(false);

  newStroke = new TStroke(v);
  newStroke->setStyle(s0->getStyle());
  newStroke->outlineOptions() = s0->outlineOptions();

  return newStroke;

  /*
int cpCount1 = stroke1->getControlPointCount();
int cpCount2 = stroke2->getControlPointCount();

int qCount1 = stroke1->getChunkCount();
const TThickQuadratic* q1=stroke1->getChunk(qCount1-1 );
const TThickQuadratic* q2=stroke2->getChunk(0);

TThickPoint extreme1 = q1->getThickP2() ;
TThickPoint extreme2 = q2->getThickP0();

vector<TThickPoint> points;
points.reserve(cpCount1+cpCount2-1);

int i =  0;
for(; i!=cpCount1-1; i++)
points.push_back( stroke1->getControlPoint(i));

if(extreme1==extreme2)
{
points.push_back(extreme1);
}
else
{
double len1=q1->getLength();
assert(len1>=0);
if(len1<=0)
len1=0;
double w1= exp(-len1*0.01);

double len2=q2->getLength();
assert(len2>=0);
if(len2<=0)
len2=0;
double w2= exp(-len2*0.01);


TThickPoint m1=q1->getThickP1();
TThickPoint m2=q2->getThickP1();

TThickPoint p1= extreme1*(1-w1)+m1*w1;
TThickPoint p2= extreme2*(1-w2)+m2*w2;

TThickPoint middleP= (p1 + p2)*0.5;

double angle=fabs(cross(normalize(m1-middleP),normalize(m2-middleP)));
if(  angle< 0.05)
middleP=(m1+m2)*0.5;

points.push_back(middleP);
}

for(i = 1; i!=cpCount2; i++)
points.push_back( stroke2->getControlPoint(i));

newStroke = new TStroke(points);
newStroke->setStyle(stroke1->getStyle());
return newStroke;
*/
}
//-----------------------------------------------------------------------------
namespace {

int local_intersect(const TStroke &stroke, const TSegment &segment,
                    std::vector<DoublePair> &intersections,
                    bool strokeIsFirst) {
  const TThickQuadratic *chunk;
  for (int i = 0; i < stroke.getChunkCount(); i++) {
    std::vector<DoublePair> localIntersections;
    chunk = stroke.getChunk(i);

    if (intersect(*chunk, segment, localIntersections)) {
      for (UINT j = 0; j < localIntersections.size(); j++) {
        double strokePar = localIntersections[j].first;

        strokePar = stroke.getW(chunk->getPoint(strokePar));

        // check for multiple solution
        DoublePair sol(
            strokeIsFirst ? strokePar : localIntersections[j].second,
            strokeIsFirst ? localIntersections[j].second : strokePar);

        std::vector<DoublePair>::iterator it_end = intersections.end();

        if (it_end == std::find(intersections.begin(), it_end, sol))
          intersections.push_back(sol);
      }
    }
  }
  return intersections.size();
}

//-----------------------------------------------------------------------------

bool almostOverlaps(const TRectD &r0, const TRectD &r1) {
  if (r0.overlaps(r1)) return true;

  if ((r0.x1 < r1.x0) && (areAlmostEqual(r0.x1, r1.x0, 1e-5)))
    return true;
  else if ((r1.x1 < r0.x0) && (areAlmostEqual(r1.x1, r0.x0, 1e-5)))
    return true;
  if ((r0.y1 < r1.y0) && (areAlmostEqual(r0.y1, r1.y0, 1e-5)))
    return true;
  else if ((r1.y1 < r0.y0) && (areAlmostEqual(r1.y1, r0.y0, 1e-5)))
    return true;

  return false;
}

//---------------------------------------------------------------------------

bool greaterThanOneAndLesserThanZero(double val) {
  if (val > 1.0 || val < 0.0) return true;
  return false;
}
}

//---------------------------------------------------------------------------

int intersect(const TStroke *s1, const TStroke *s2,
              std::vector<DoublePair> &intersections, bool checkBBox) {
  UINT k = 0;

  intersections.clear();

  if (checkBBox && !s1->getBBox().overlaps(s2->getBBox())) return 0;

  for (int i = 0; i < s1->getChunkCount(); i++) {
    const TQuadratic *q1 = s1->getChunk(i);
    if (q1->getP0() == q1->getP2() && q1->getP1() == q1->getP2()) continue;
    int j = 0;
    if (s1 ==
        s2)  // this 'if': if after i-th stroke there are degenere strokes,
    // I do not consider them; so, instead of starting from j = i+2
    // I start from j = i+2+numDegeneres
    {
      j = i + 2;

      while (j <= s2->getChunkCount()) {
        const TQuadratic *qAux = s2->getChunk(j - 1);
        if (qAux->getP0() != qAux->getP2() || qAux->getP1() != qAux->getP2())
          break;
        j++;
      }
    }

    for (; j < s2->getChunkCount(); j++) {
      const TQuadratic *q2 = s2->getChunk(j);
      if (q2->getP0() == q2->getP2() && q2->getP1() == q2->getP2()) continue;

      if (!almostOverlaps(q1->getBBox(), q2->getBBox())) continue;
#ifdef CHECK_DEGLI_ESTREMI
      vector<DoublePair> quadIntersections1;
      if (i == 0 || i == s1->getChunkCount() - 1) && (j==0 || j==s2->getChunkCount()-1))
				{
          TPointD pp1 = (i == 0 ? q1->getP0() : q1->getP2());
          TPointD pp2 = (j == 0 ? q2->getP0() : q2->getP2());
          if (areAlmostEqual(pp1, pp2)) {
            intersections.push_back(
                DoublePair(i == 0 ? 0.0 : 1.0, j == 0 ? 0.0 : 1.0));
            k++;
            continue;
          }
          if (s1->getChunkCount() == 1 && s2->getChunkCount() == 1) {
            TPointD pp1 = q1->getP2();
            TPointD pp2 = q2->getP2();
            if (areAlmostEqual(pp1, pp2)) {
              intersections.push_back(DoublePair(1.0, 1.0));
              k++;
              continue;
            }
          }
          if (s1->getChunkCount() == 1) {
            TPointD pp1 = q1->getP2();
            TPointD pp2 = (j == 0 ? q2->getP0() : q2->getP2());
            if (areAlmostEqual(pp1, pp2)) {
              intersections.push_back(DoublePair(1.0, j == 0 ? 0.0 : 1.0));
              k++;
              continue;
            }
          }
          if (s2->getChunkCount() == 1) {
            TPointD pp1 = (i == 0 ? q1->getP0() : q1->getP2());
            TPointD pp2 = q2->getP2();
            if (areAlmostEqual(pp1, pp2)) {
              intersections.push_back(DoublePair(i == 0 ? 0.0 : 1.0, 1.0));
              k++;
              continue;
            }
          }
        }
#endif

      // if (j<s2->getChunkCount()-1)
      //  {
      // const TQuadratic *q3= s2->getChunk(j==0?0:j+1);
      // evito di intersecare se le quadratiche sono uguali
      if (*q1 == *q2 ||
          (q1->getP0() == q2->getP2() && q1->getP1() == q2->getP1() &&
           q1->getP2() == q2->getP0())) {
        // j+=((j==0)?1:2);
        continue;
      }
      //  }

      vector<DoublePair> quadIntersections;
      if (intersect(*q1, *q2, quadIntersections))
        for (int h = 0; h < (int)quadIntersections.size(); h++) {
          DoublePair res(getWfromChunkAndT(s1, i, quadIntersections[h].first),
                         getWfromChunkAndT(s2, j, quadIntersections[h].second));

          if (areAlmostEqual(quadIntersections[h].first, 0) ||
              areAlmostEqual(quadIntersections[h].first, 1) ||
              areAlmostEqual(quadIntersections[h].second, 0) ||
              areAlmostEqual(quadIntersections[h].second, 1)) {
            int q = 0;
            for (q = 0; q < (int)intersections.size(); q++)
              if (areAlmostEqual(intersections[q].first, res.first, 1e-8) &&
                  areAlmostEqual(intersections[q].second, res.second, 1e-8))
                break;
            if (q < (int)intersections.size()) continue;
          }
          intersections.push_back(res);
          // if (k==0 || intersections[k].first!=intersections[k-1].first ||
          // intersections[k].second!=intersections[k-1].second)
          k++;
        }
    }
  }
  if (s1 == s2 &&
      (s1->isSelfLoop() || s1->getPoint(0.0) == s1->getPoint(1.0))) {
    int i;
    for (i = 0; i < (int)intersections.size(); i++) {
      assert(!(areAlmostEqual(intersections[i].first, 1.0, 1e-1) &&
               areAlmostEqual(intersections[i].second, 0.0, 1e-1)));
      if (areAlmostEqual(intersections[i].first, 0.0, 1e-1) &&
          areAlmostEqual(intersections[i].second, 1.0, 1e-1))
        break;
    }
    if (i == (int)intersections.size()) {
      intersections.push_back(DoublePair(0.0, 1.0));
      k++;
    }
  }

  return k;
}

//-----------------------------------------------------------------------------

int intersect(const TSegment &segment, const TStroke &stroke,
              std::vector<DoublePair> &intersections) {
  return local_intersect(stroke, segment, intersections, false);
}

//-----------------------------------------------------------------------------

int intersect(const TStroke &stroke, const TSegment &segment,
              std::vector<DoublePair> &intersections) {
  return local_intersect(stroke, segment, intersections, true);
}

//-----------------------------------------------------------------------------

int intersect(const TStroke &stroke, const TPointD &center, double radius,
              vector<double> &intersections) {
  //            2    2    2
  // riconduco x  + y  = r  con x(t) y(t) alla forma
  //        2                2           2              2    2
  // ( a_x t + b_x t + c_x  ) + ( a_y t + b_y t + c_y  )  = r

  //     2
  //  a x  + b x + c

  const int a = 2;
  const int b = 1;
  const int c = 0;

  vector<TPointD> bez(3);
  vector<TPointD> pol(3);
  vector<double> coeff(5);

  for (int chunk = 0; chunk < stroke.getChunkCount(); ++chunk) {
    const TThickQuadratic *tq = stroke.getChunk(chunk);

    bez[0] = tq->getP0();
    bez[1] = tq->getP1();
    bez[2] = tq->getP2();

    bezier2poly(bez, pol);

    pol[c] -= center;

    coeff[4] = sq(pol[a].x) + sq(pol[a].y);
    coeff[3] = 2.0 * (pol[a].x * pol[b].x + pol[a].y * pol[b].y);
    coeff[2] = 2.0 * (pol[a].x * pol[c].x + pol[a].y * pol[c].y) +
               sq(pol[b].x) + sq(pol[b].y);
    coeff[1] = 2.0 * (pol[b].x * pol[c].x + pol[b].y * pol[c].y);
    coeff[0] = sq(pol[c].x) + sq(pol[c].y) - sq(radius);

    vector<double> sol;
    rootFinding(coeff, sol);

    sol.erase(
        remove_if(sol.begin(), sol.end(), greaterThanOneAndLesserThanZero),
        sol.end());

    for (UINT j = 0; j < sol.size(); ++j)
      intersections.push_back(getWfromChunkAndT(&stroke, chunk, sol[j]));
  }

#if defined(DEBUG) || defined(_DEBUG)
  /*
cout<<"interesections:";
copy( intersections.begin(), intersections.end(), ostream_iterator<double>(
cout, " " ) );
cout<<endl;
*/
  // assert to test intersections are not decrescent
  vector<double> test;

  adjacent_difference(intersections.begin(), intersections.end(),
                      back_inserter(test));

  while (!test.empty()) {
    assert(test.back() >= 0);
    test.pop_back();
  }

#endif

  return intersections.size();
}

//-----------------------------------------------------------------------------

void splitStroke(const TStroke &tq, const vector<double> &pars,
                 std::vector<TStroke *> &v) {
  if (pars.empty()) return;

  UINT i, vSize = pars.size();

  vector<double> length(vSize);

  // primo passo estraggo i parametri di lunghezza
  for (i = 0; i < vSize; ++i) length[i] = tq.getLength(pars[i]);

  std::adjacent_difference(length.begin(), length.end(), length.begin());

  TStroke *q1, q2, q3;

  q1 = new TStroke();
  tq.split(pars[0], *q1, q2);

  assert(areAlmostEqual(q1->getLength(), length[0], 1e-4));
  v.push_back(q1);

  for (i = 1; i < vSize; ++i) {
    q1         = new TStroke();
    double par = q2.getParameterAtLength(length[i]);
    assert(0 <= par && par <= 1.0);
    q2.split(par, *q1, q3);

    assert(areAlmostEqual(q1->getLength(), length[i], 1e-4));
    v.push_back(q1);
    q2 = q3;
  }

  v.push_back(new TStroke(q2));
}

//--------------------------------------------------------------------------------------

void detectCorners(const TStroke *stroke, double minDegree,
                   std::vector<int> &corners) {
  const double minSin = fabs(sin(minDegree * M_PI_180));

  const TThickQuadratic *quad1 = 0;
  const TThickQuadratic *quad2 = 0;
  UINT quadCount1              = stroke->getChunkCount();
  TPointD speed1, speed2;

  TPointD tan1, tan2;
  quad1 = stroke->getChunk(0);
  for (UINT j = 1; j < quadCount1; j++) {
    quad2 = stroke->getChunk(j);

    speed1 = quad1->getSpeed(1);
    speed2 = quad2->getSpeed(0);
    if (!(speed1 == TPointD() || speed2 == TPointD())) {
      tan1 = normalize(speed1);
      tan2 = normalize(speed2);
      if (tan1 * tan2 < 0 || fabs(cross(tan1, tan2)) >= minSin)
        corners.push_back(j);
    }
    quad1 = quad2;
  }
}

//--------------------------------------------------------------------------------------

//---------------------------------------------------------------------------
namespace {

//! Rimuove le quadratiche nulle, cioe' i chunk in cui i TThickPoint sono quasi
//! uguali
bool removeNullQuadratic(TStroke *stroke, bool checkThickness = true) {
  vector<TThickPoint> points;
  UINT i, qCount = stroke->getChunkCount();
  const TThickQuadratic *q;
  TThickPoint p1, p2, p3;

  // se i punti coincidono e' una stroke puntiforme ed e' ammessa
  if (qCount == 1) return false;

  bool check = false;

  for (i = 0; i != qCount; i++) {
    q  = stroke->getChunk(i);
    p1 = q->getThickP0();
    p2 = q->getThickP1();
    p3 = q->getThickP2();

    if (areAlmostEqual(p1.x, p2.x) && areAlmostEqual(p2.x, p3.x) &&
        areAlmostEqual(p1.y, p2.y) && areAlmostEqual(p2.y, p3.y) &&
        (!checkThickness || (areAlmostEqual(p1.thick, p2.thick) &&
                             areAlmostEqual(p2.thick, p3.thick)))) {
      // assert(!"null quadratic");
      check = true;
    } else {
      points.push_back(p1);
      points.push_back(p2);
    }
  }

  if (check) {
    points.push_back(p3);
    stroke->reshape(&(points[0]), points.size());
  }

  return check;
}
//---------------------------------------------------------------------------

//! Converte un TThickPoint p0 in un T3DPoint
inline T3DPointD thickPntTo3DPnt(const TThickPoint &p0) {
  return T3DPointD(p0.x, p0.y, p0.thick);
}

//---------------------------------------------------------------------------

//! Converte un vettore di TThickPoint from in un vettore di T3DPointD to
void convert(const vector<TThickPoint> &from, vector<T3DPointD> &to) {
  to.resize(from.size());
  transform(from.begin(), from.end(), to.begin(), thickPntTo3DPnt);
}

typedef vector<TThickCubic *> TThickCubicArray;
typedef vector<TThickQuadratic *> QuadStrokeChunkArray;

//---------------------------------------------------------------------------

/*!
          Restituisce un puntatore ad un array di double che contiene la
   distanza tra
                il primo punto e i successivi diviso la distanza tra il primo
   punto e l'ultimo
                */
double *chordLengthParameterize3D(const T3DPointD *pointsArrayBegin, int size) {
  double *u = new double[size];
  u[0]      = 0.0;
  int i;
  for (i = 1; i < size; i++)
    u[i] = u[i - 1] +
           tdistance(*(pointsArrayBegin + i), *(pointsArrayBegin + i - 1));
  for (i = 1; i < size; i++) {
    assert(!isAlmostZero(u[size - 1]));
    u[i] = u[i] / u[size - 1];
  }
  return u;
}

//---------------------------------------------------------------------------

/*!
    Returns a \a measure of the maximal discrepancy of the points specified
    by pointsArray from the corresponding cubic(u[]) points.
  */
double computeMaxError3D(const TThickCubic &cubic,
                         const T3DPointD *pointsArrayBegin, int size, double *u,
                         int &splitPoint) {
  double err, maxErr = 0;
  splitPoint = 0;

  for (int i = 1; i < size - 1; i++) {
#ifdef USE_NEW_3D_ERROR_COMPUTE

    // Being cubic a THICK cubic, we assume that the supplied points' z
    // refers to thicknesses as well.

    // So, given 2 thick points in the plane, we use the maximal distance
    // of 'corresponding' outline points. Correspondence refers to the
    // same relative position from centers. It's easily verifiable that
    // such maximal distance is found when the relative positions both lie
    // along the line connecting the two centers.

    const TThickPoint &A(cubic.getThickPoint(u[i]));
    const T3DPointD &B(pointsArrayBegin[i]);

    err = sqrt(sq(B.x - A.x) + sq(B.y - A.y)) + fabs(B.z - A.thick);

#else

    // Old version, less intuitive. Similar to the above, except that the
    // 2d-norm is
    // roughly multiplied by  norm((TPointD) B-A) / A.thick      ... I wonder
    // why ....

    T3DPointD delta =
        thickPntTo3DPnt(cubic.getThickPoint(u[i])) - *(pointsArrayBegin + i);

    double thick            = cubic.getThickPoint(u[i]).thick;
    if (thick <= 2.0) thick = 2.0;

    err = norm2(TPointD(delta.x, delta.y)) / thick;

    if (fabs(delta.z) > 2.0) err = err + fabs(delta.z);

#endif

    if (err >= maxErr) {
      maxErr     = err;
      splitPoint = i;
    }
  }

  return maxErr;
}
//---------------------------------------------------------------------------

double NewtonRaphsonRootFind3D(const TThickCubic &cubic, const T3DPointD &p3D,
                               double u) {
  TPointD qU  = cubic.getPoint(u);
  TPointD q1U = cubic.getSpeed(u);
  TPointD q2U = cubic.getAcceleration(u);

  TPointD p(p3D.x, p3D.y);

  return u - ((qU - p) * q1U) / (norm2(q1U) + (qU - p) * q2U);
}

//---------------------------------------------------------------------------
int compareDouble(const void *e1, const void *e2) {
  return (*(double *)e1 < *(double *)e2)
             ? -1
             : (*(double *)e1 == *(double *)e2) ? 0 : 1;
}
//---------------------------------------------------------------------------
//! Ricalcola i valori di u[] sulla base del metodo di Newton Raphson
double *reparameterize3D(const TThickCubic &cubic,
                         const T3DPointD *pointsArrayBegin, int size,
                         double *u) {
  double *uPrime = new double[size];

  for (int i = 0; i < size; i++) {
    uPrime[i] = NewtonRaphsonRootFind3D(cubic, *(pointsArrayBegin + i), u[i]);
    if (!_finite(uPrime[i])) {
      delete[] uPrime;
      return NULL;
    }
  }

  qsort(uPrime, size, sizeof(double), compareDouble);
  // std::sort( uPrime, uPrime+size );

  if (uPrime[0] < 0.0 || uPrime[size - 1] > 1.0) {
    delete[] uPrime;
    return NULL;
  }

  assert(uPrime[0] >= 0.0);
  assert(uPrime[size - 1] <= 1.0);

  return uPrime;
}
//---------------------------------------------------------------------------

inline double B1(double u) {
  double tmp = 1.0 - u;
  return 3 * u * tmp * tmp;
}
inline double B2(double u) { return 3 * u * u * (1 - u); }
inline double B0plusB1(double u) {
  double tmp = 1.0 - u;
  return tmp * tmp * (1.0 + 2.0 * u);
}
inline double B2plusB3(double u) { return u * u * (3.0 - 2.0 * u); }

}  // end of unnamed namespace
//-----------------------------------------------------------------------------

//! Classe che definisce uno stroke cubico

class TCubicStroke {
private:
  //! Genera un TThickCubic
  TThickCubic *generateCubic3D(const T3DPointD pointsArrayBegin[],
                               const double uPrime[], int size,
                               const T3DPointD &tangentLeft,
                               const T3DPointD &tangentRight);

  //! Genera una cubica in modo ricorsivo
  void fitCubic3D(const T3DPointD pointsArrayBegin[], int size,
                  const T3DPointD &tangentLeft, const T3DPointD &tangentRight,
                  double error);

  //! Rettangolo che contiene il TCubicStroke
  TRectD m_bBox;

public:
  //! Puntatore a vettore di puntatori a TThickCubic
  vector<TThickCubic *> *m_cubicChunkArray;

  //! Costruttore
  TCubicStroke();
  //! Costruttore
  /*!
            Costruisce un TCubicStroke uguale al TCubicStroke datogli
            */
  TCubicStroke(const TCubicStroke &stroke);
  //! Costruttore
  /*!
            Costruisce un TCubicStroke da un array di T3DPointD,
            in funzione dei due parametri error e doDetectCorners
            */
  TCubicStroke(const vector<T3DPointD> &pointsArray3D, double error,
               bool doDetectCorners = true);
  /*
TCubicStroke(       vector<TPointD> &pointsArray,
double          error,
const TPointD         &tangentLeft,
const TPointD         &tangentRight);
*/
  //! Distruttore
  ~TCubicStroke();
};

namespace {

// It implements the degree reduction algorithm, returning the times
// that the cubic must be splitted (in the half), in order to approximate
// it with a sequence of quadratic bezier curves.
// This method doesn't take into count the thickness of the control points,
// so it just operates on the center line of the fat cubic curve

//---------------------------------------------------------------------------

/*! It splits a cubic bezier curve 'splits' times in the half. Then each
      subcurve is converted into quadratic curve, computing the middle control
      circle as the weighted averege of the four control circles of the
      original cubic subcurve.
  */
void doComputeQuadraticsFromCubic(const TThickCubic &cubic, int splits,
                                  vector<TThickQuadratic *> &chunkArray) {
  TThickPoint p0r, p1r, p2r, p3r, p0l, p1l, p2l, p3l;

  p0l = cubic.getThickP0();
  p1l = cubic.getThickP1();
  p2l = cubic.getThickP2();
  p3l = cubic.getThickP3();

  p3l = 0.5 * (p3l + p2l);
  p2l = 0.5 * (p2l + p1l);
  p1l = 0.5 * (p1l + p0l);
  p3l = 0.5 * (p3l + p2l);
  p2l = 0.5 * (p2l + p1l);
  p3l = 0.5 * (p3l + p2l);

  //  cubic.getControlPoints(p0r, p1r, p2r, p3r);
  p0r = cubic.getThickP0();
  p1r = cubic.getThickP1();
  p2r = cubic.getThickP2();
  p3r = cubic.getThickP3();

  p0r = 0.5 * (p0r + p1r);
  p1r = 0.5 * (p1r + p2r);
  p2r = 0.5 * (p2r + p3r);
  p0r = 0.5 * (p0r + p1r);
  p1r = 0.5 * (p1r + p2r);
  p0r = 0.5 * (p0r + p1r);

  if (splits > 0) {
    TThickCubic cubic1(cubic);
    TThickCubic tmp_1(p0l, p1l, p2l, p3l);
    std::swap(cubic1, tmp_1);
    doComputeQuadraticsFromCubic(cubic1, splits - 1, chunkArray);
    TThickCubic tmp_2(p0r, p1r, p2r, p3r);
    std::swap(cubic1, tmp_2);
    doComputeQuadraticsFromCubic(cubic1, splits - 1, chunkArray);
  } else {
    TThickQuadratic *chunkL =
        new TThickQuadratic(p0l, 0.25 * (3 * (p1l + p2l) - (p0l + p3l)), p3l);
    TThickQuadratic *chunkR =
        new TThickQuadratic(p0r, 0.25 * (3 * (p1r + p2r) - (p0r + p3r)), p3r);

    // int size = chunkArray.size();
    chunkArray.push_back(chunkL);
    chunkArray.push_back(chunkR);
  }
}

//---------------------------------------------------------------------------

/*!
          Genera una o piu' quadratiche da una cubica.
    Pone una serie di condizioni , se sono verificate crea una unica quadratica,
                altrimenti richiama doComputeQuadraticsFromCubic
          */
void computeQuadraticsFromCubic(const TThickCubic &cubic, double error,
                                vector<TThickQuadratic *> &chunkArray) {
  const double T = 0.21132486540518711775; /* 1/2 - sqrt(3)/6 */
  const double S = (1 - T);
  int splits;
  double dist2 = tdistance2(cubic.getP1(), cubic.getP2());
  if (dist2 < 2) {
    chunkArray.push_back(new TThickQuadratic(
        cubic.getThickP0(), 0.5 * (cubic.getThickP1() + cubic.getThickP2()),
        cubic.getThickP3()));
    return;
  }

  TPointD dp = ((S * S * S) - (S * S - S * T / 2)) * cubic.getP0() +
               ((3 * S * S * T) - (S * T * 3 / 2)) * cubic.getP1() +
               ((3 * S * T * T) - (S * T * 3 / 2)) * cubic.getP2() +
               ((T * T * T) - (T * T - S * T / 2)) * cubic.getP3();

  double dist_sq = norm2(dp);

  for (splits = 0; dist_sq > error; splits++) dist_sq *= 0.125 * 0.125;

  if (splits == 0) {
    TPointD p0 = cubic.getP0();
    TPointD p1 = cubic.getP1();
    TPointD p2 = cubic.getP2();
    TPointD p3 = cubic.getP3();

    TPointD side01 = p1 - p0;
    TPointD side32 = p2 - p3;
    //  se sono verificate TUTTE le condizioni dei seguenti if nidificati,
    //  allora viene
    //  generata un'unica quadratica per approssimare cubic. Altrimenti,
    //  se NON viene verificata ALMENO una delle condizioni dei seguenti if
    //  nidificati, allora
    //  cubic viene splittata ulteriormente e trattata da
    //  doComputeQuadraticsFromCubic
    double det =
        -side01.x * side32.y + side01.y * side32.x;  //  -cross(side01, side32)
    if (!isAlmostZero(det))  // side01 incidente side32 (verra' calcolata
                             // l'intersezione...)
    {
      TPointD side03 = p3 - p0;
      double det01   = -side03.x * side32.y + side03.y * side32.x;
      double det32   = side01.x * side03.y - side01.y * side03.x;
      double t01     = det01 / det;
      double t32     = det32 / det;
// unused variable
#if 0 
        TPointD p01 = p0 + t01*(p1 - p0); // debug
        TPointD p32 = p3 + t32*(p2 - p3); // debug
#endif
      // TPointD intersection = p0 + t01*(p1 - p0) = p3 + t32*(p2 - p3)
      // assert (areAlmostEqual(p0 + t01*(p1 - p0), p3 + t32*(p2 - p3)));
      if (t01 >= 1 && t32 >= 1) {  //  poligonale p0_p1_p2_p3 NON e' a "zig-zag"
        double norm2_side0p = norm2(t01 * side01);
        double norm2_side3p = norm2(t32 * side32);
        if (!isAlmostZero(norm2_side0p, 1e-20) &&
            !isAlmostZero(norm2_side3p,
                          1e-20)) {  //  la condizione puo' essere violata anche
                                     //  nel caso !isAlmostZero(det) == true
          double norm2_side03 = norm2(side03);
          double tmp          = norm2_side0p + norm2_side3p - norm2_side03;
          // double cs = tmp/(2*sqrt(norm2_side0p)*sqrt(norm2_side3p));  //
          // debug
          double cs_sign =
              tmp >= 0 ? 1 : -1;  //  cs2 "perde" il segno (cs2 = cs*cs)
          //  th coseno: gli assert del tipo acos(sqrt(cs2)) sono commentati
          //  perche' puo' essere cs2 > 1 per errori
          //  di approssimazione: tuttavia la cosa non costituisce problema
          //  (acos non viene mai eseguta...)
          double cs2 = sq(tmp) / (4 * norm2_side0p * norm2_side3p);
          // assert (0 <= cs2 && cs2 <= 1 + TConsts::epsilon);
          assert(areAlmostEqual(
              tsign(cs_sign) * sqrt(cs2),
              tmp / (2 * sqrt(norm2_side0p) * sqrt(norm2_side3p))));
          if (cs_sign < 0 || cs2 < 0.969846)  //  cos(10°)^2 = 0.969846
          {  //  limita distanza di intersection: elimina quadratiche "cappio"
             //  (con p1 "lontano")
            // assert (acos(tsign(cs_sign)*sqrt(cs2)) > 10*M_PI_180);
            assert(tsign(cs_sign) * sqrt(cs2) < cos(10 * M_PI_180));
            TPointD intersection =
                p0 + t01 * (p1 - p0);  //  = p2 + t32*(p2 - p3)
            TThickPoint p(
                intersection.x, intersection.y,
                0.5 * (cubic.getThickP1().thick +
                       cubic.getThickP2()
                           .thick));  //  compatibilita' precedente funzione
            chunkArray.push_back(
                new TThickQuadratic(cubic.getThickP0(), p, cubic.getThickP3()));

#ifdef _DEBUG
            TThickQuadratic *lastTq = chunkArray.back();
            TThickPoint pDeb        = lastTq->getThickP0();
            assert(_finite(pDeb.x));
            assert(_finite(pDeb.y));
            assert(_finite(pDeb.thick));
            pDeb = lastTq->getThickP1();
            assert(_finite(pDeb.x));
            assert(_finite(pDeb.y));
            assert(_finite(pDeb.thick));
            pDeb = lastTq->getThickP2();
            assert(_finite(pDeb.x));
            assert(_finite(pDeb.y));
            assert(_finite(pDeb.thick));
#endif
            numSaved++;  //  variabile debug: compatibilita' precedente funzione
            return;
          }
        }
      }
    } else {
      TPointD side03 = p3 - p0;
      double det01   = -side03.x * side32.y + side03.y * side32.x;
      if (isAlmostZero(det01)) {
        // e' una retta!, crea unica quadratica con p in mezzo a p1 e p2
        chunkArray.push_back(new TThickQuadratic(
            cubic.getThickP0(), (cubic.getThickP1() + cubic.getThickP2()) * 0.5,
            cubic.getThickP3()));
        numSaved++;  //  variabile debug: compatibilita' precedente funzione
        return;
      }
    }
    // else: se arriva qui, almeno una delle condizioni negli if nidificati
    // precedenti e' falsa
    splits++;
  }
  doComputeQuadraticsFromCubic(cubic, splits - 1, chunkArray);
}

//---------------------------------------------------------------------------
//  TStroke *computeQuadStroke(const TCubicStroke& cubic, double error)

//! Ricava uno stroke quadratico da uno stroke cubico
TStroke *computeQuadStroke(const TCubicStroke &cubic) {
  vector<TThickQuadratic *> chunkArray;

  for (UINT i = 0; i < cubic.m_cubicChunkArray->size(); i++) {
    TThickCubic tmp(*(*cubic.m_cubicChunkArray)[i]);

#ifdef _DEBUG
    {
      TThickPoint p = tmp.getThickP0();
      assert(_finite(p.x));
      assert(_finite(p.y));
      assert(_finite(p.thick));
      p = tmp.getThickP1();
      assert(_finite(p.x));
      assert(_finite(p.y));
      assert(_finite(p.thick));
      p = tmp.getThickP2();
      assert(_finite(p.x));
      assert(_finite(p.y));
      assert(_finite(p.thick));
      p = tmp.getThickP3();
      assert(_finite(p.x));
      assert(_finite(p.y));
      assert(_finite(p.thick));
    }
#endif

    // this code use a Chebychev version of degree reduction

    // This code is for old algorithm only:
    computeQuadraticsFromCubic(tmp, 2.0 /*0.5*/, chunkArray);
  }

  TStroke *outStroke = TStroke::create(chunkArray);

  clearPointerContainer(chunkArray);

  return outStroke;
}

}  // namespace

//-----------------------------------------------------------------------------

// helper function (defined in tstroke.h)

void computeQuadraticsFromCubic(const TThickPoint &p0, const TThickPoint &p1,
                                const TThickPoint &p2, const TThickPoint &p3,
                                double error,
                                vector<TThickQuadratic *> &chunkArray) {
  computeQuadraticsFromCubic(TThickCubic(p0, p1, p2, p3), error, chunkArray);
}

//-----------------------------------------------------------------------------

TCubicStroke::TCubicStroke() : m_bBox() {
  m_cubicChunkArray = new TThickCubicArray();
}

//-----------------------------------------------------------------------------

TCubicStroke::TCubicStroke(const TCubicStroke &stroke)
    : m_bBox(stroke.m_bBox), m_cubicChunkArray(stroke.m_cubicChunkArray) {
  m_cubicChunkArray = new TThickCubicArray(*stroke.m_cubicChunkArray);
}

//-----------------------------------------------------------------------------

TCubicStroke::~TCubicStroke() {
  if (m_cubicChunkArray) {
    while (!m_cubicChunkArray->empty()) {
      delete m_cubicChunkArray->back();
      m_cubicChunkArray->pop_back();
    }

    delete m_cubicChunkArray;
  }
}

//-----------------------------------------------------------------------------

TCubicStroke::TCubicStroke(const vector<T3DPointD> &pointsArray3D, double error,
                           bool doDetectCorners) {
  vector<int> corners;
  corners.push_back(0);
  if (doDetectCorners) detectCorners(pointsArray3D, 3, 3, 15, 100, corners);
  corners.push_back(pointsArray3D.size() - 1);

#ifndef USE_NEW_3D_ERROR_COMPUTE
  error *= error;
#endif

  m_cubicChunkArray = new vector<TThickCubic *>();

  for (int i = 1; i < (int)corners.size(); i++) {
    int size       = corners[i] - corners[i - 1] + 1;
    int firstPoint = corners[i - 1];
    T3DPointD tanLeft, tanRigth;
    assert(size > 0);
    if (size > 1)  //  capita che corners[i] = corners[i - 1] ("clic" senza drag
                   //  oppure bug (noto!!!) del cornerDetector)
    {
      tanLeft  = -pointsArray3D[firstPoint + 1] + pointsArray3D[firstPoint];
      tanRigth = pointsArray3D[firstPoint + size - 2] -
                 pointsArray3D[firstPoint + size - 1];

      if (norm2(tanLeft) > 0) tanLeft = normalize(tanLeft);

      if (norm2(tanRigth) > 0) tanRigth = normalize(tanRigth);

      fitCubic3D(&pointsArray3D[firstPoint], size, tanLeft, tanRigth, error);
    } else if (pointsArray3D.size() == 1) {
      //  caso in cui i non calcola nessun corner a meno di quello iniziale
      //  e finale coincidenti: 1 solo punto campionato ("clic" senza drag)
      assert(size == 1);
      assert(corners.size() == 2);
      assert(corners[0] == 0);
      assert(corners[1] == 0);
      m_cubicChunkArray->push_back(
          new TThickCubic(pointsArray3D[0], pointsArray3D[0], pointsArray3D[0],
                          pointsArray3D[0]));
    }
  }
}

//-----------------------------------------------------------------------------

void TCubicStroke::fitCubic3D(const T3DPointD pointsArrayBegin[], int size,
                              const T3DPointD &tangentLeft,
                              const T3DPointD &tangentRight, double error) {
  int maxIterations = 4;

  if (size == 2) {
    double dist = tdistance(*pointsArrayBegin, *(pointsArrayBegin + 1)) / 3.0;
    TThickCubic *strokeCubicChunk = new TThickCubic(
        *pointsArrayBegin, *pointsArrayBegin - dist * tangentLeft,
        *(pointsArrayBegin + 1) + dist * tangentRight, *(pointsArrayBegin + 1));

    m_cubicChunkArray->push_back(strokeCubicChunk);
    return;
  }

  double *u = chordLengthParameterize3D(pointsArrayBegin, size);
  TThickCubic *cubic =
      generateCubic3D(pointsArrayBegin, u, size, tangentLeft, tangentRight);

  int splitPoint;
  double maxError =
      computeMaxError3D(*cubic, pointsArrayBegin, size, u, splitPoint);

  if (maxError < error) {
    delete[] u;
    m_cubicChunkArray->push_back(cubic);
    return;
  }

  // if (maxError<error)
  {
    double *uPrime = NULL;
    for (int i = 0; i < maxIterations; i++) {
      // delete uPrime;
      uPrime = reparameterize3D(*cubic, pointsArrayBegin, size, u);
      if (!uPrime) break;

      delete cubic;
      cubic = generateCubic3D(pointsArrayBegin, uPrime, size, tangentLeft,
                              tangentRight);
      maxError =
          computeMaxError3D(*cubic, pointsArrayBegin, size, uPrime, splitPoint);
      if (maxError < error) {
        delete[] uPrime;
        delete[] u;
        m_cubicChunkArray->push_back(cubic);
        return;
      }
      delete[] u;
      u = uPrime;
    }
  }

  delete[] u;
  delete cubic;

  T3DPointD centralTangent;
  if (*(pointsArrayBegin + splitPoint - 1) ==
      *(pointsArrayBegin + splitPoint + 1))
    centralTangent = normalize(*(pointsArrayBegin + splitPoint) -
                               *(pointsArrayBegin + splitPoint + 1));
  else
    centralTangent = normalize(*(pointsArrayBegin + splitPoint - 1) -
                               *(pointsArrayBegin + splitPoint + 1));

  fitCubic3D(pointsArrayBegin, splitPoint + 1, tangentLeft, centralTangent,
             error);

  fitCubic3D(pointsArrayBegin + splitPoint, size - splitPoint, centralTangent,
             tangentRight, error);
}

//-----------------------------------------------------------------------------

TThickCubic *TCubicStroke::generateCubic3D(const T3DPointD pointsArrayBegin[],
                                           const double uPrime[], int size,
                                           const T3DPointD &tangentLeft,
                                           const T3DPointD &tangentRight) {
  double X[2], C[2][2];
  int i;

  T3DPointD p0 = *pointsArrayBegin;
  T3DPointD p3 = *(pointsArrayBegin + size - 1);

  C[0][0] = C[0][1] = X[0] = 0;
  C[1][0] = C[1][1] = X[1] = 0;

  for (i = 0; i < size; i++) {
    const T3DPointD A[2] = {
        tangentLeft * B1(uPrime[i]), tangentRight * B2(uPrime[i]),
    };

    C[0][0] += A[0] * A[0];
    C[0][1] += A[0] * A[1];
    C[1][1] += A[1] * A[1];

    C[1][0] = C[0][1];

    T3DPointD tmp = *(pointsArrayBegin + i) - (B0plusB1(uPrime[i]) * p0) +
                    B2plusB3(uPrime[i]) * p3;
    X[0] += A[0] * tmp;
    X[1] += A[1] * tmp;
  }

  double detC0C1 = C[0][0] * C[1][1] - C[0][1] * C[1][0];
  double detC0X  = X[1] * C[0][0] - X[0] * C[0][1];
  double detXC1  = X[0] * C[1][1] - X[1] * C[0][1];

  if (isAlmostZero(detC0C1)) detC0C1 = C[0][0] * C[1][1] * 10e-12;

  double alphaL = detXC1 / detC0C1;
  double alphaR = detC0X / detC0C1;

  /////////////////////////////////////////////////////////////////////////////////////////////////
  //
  //  il problema e' che i valori stupidi per alpha non adrebbere messi solo
  //  quando ci si accorge
  //  che il segno e' sbagliato, ma anche se i valori sono troppo alti. Ma come
  //  fare a valutarlo?

  //  Intanto bisognerebbe accertarsi che 'sti valori alcune volte sono cosi'
  //  assurdi solo per problemi
  //  di approssimazione e non per bachi nel codice
  //
  /////////////////////////////////////////////////////////////////////////////////////////////////

  double xmin     = (numeric_limits<double>::max)();
  double ymin     = (numeric_limits<double>::max)();
  double xmax     = -(numeric_limits<double>::max)();
  double ymax     = -(numeric_limits<double>::max)();
  double thickmin = (numeric_limits<double>::max)();
  double thickmax = -(numeric_limits<double>::max)();

  for (i = 0; i < size; i++) {
    if (pointsArrayBegin[i].x < xmin) xmin         = pointsArrayBegin[i].x;
    if (pointsArrayBegin[i].x > xmax) xmax         = pointsArrayBegin[i].x;
    if (pointsArrayBegin[i].y < ymin) ymin         = pointsArrayBegin[i].y;
    if (pointsArrayBegin[i].y > ymax) ymax         = pointsArrayBegin[i].y;
    if (pointsArrayBegin[i].z < thickmin) thickmin = pointsArrayBegin[i].z;
    if (pointsArrayBegin[i].z > thickmax) thickmax = pointsArrayBegin[i].z;
  }

  double lx = xmax - xmin;
  assert(lx >= 0);
  double ly = ymax - ymin;
  assert(ly >= 0);
  double lt = thickmax - thickmin;
  assert(lt >= 0);

  xmin -= lx;
  xmax += lx;
  ymin -= ly;
  ymax += ly;
  thickmin -= lt;
  thickmax += lt;

  TRectD bbox(xmin, ymin, xmax, ymax);

  if (alphaL < 0.0 || alphaR < 0.0)
    alphaL = alphaR = sqrt(tdistance2(p0, p3)) / 3.0;

  T3DPointD p1 = p0 - tangentLeft * alphaL;
  T3DPointD p2 = p3 + tangentRight * alphaR;

  if (!bbox.contains(TPointD(p1.x, p1.y)) ||
      !bbox.contains(TPointD(p2.x, p2.y))) {
    alphaL = alphaR = sqrt(tdistance2(p0, p3)) / 3.0;
    p1              = p0 - tangentLeft * alphaL;
    p2              = p3 + tangentRight * alphaR;
  }

  if (p1.z < thickmin)
    p1.z = thickmin;
  else if (p1.z > thickmax)
    p1.z = thickmax;

  if (p2.z < thickmin)
    p2.z = thickmin;
  else if (p2.z > thickmax)
    p2.z = thickmax;

  TThickCubic *thickCubic = new TThickCubic(
      TThickPoint(p0.x, p0.y, p0.z), TThickPoint(p1.x, p1.y, p1.z),
      TThickPoint(p2.x, p2.y, p2.z), TThickPoint(p3.x, p3.y, p3.z));

  return thickCubic;
}

//-----------------------------------------------------------------------------
//! Restituisce uno stroke da un vettore di TThickPoint
/*!
  Trasforma un vettore di TThickPoint in un vettore di T3DPointD che usa per
  trovare un
        TCubicStroke, cioe' uno stroke cubico. Da questo trova lo stroke
  quadratico.
  */
TStroke *TStroke::interpolate(const vector<TThickPoint> &points, double error,
                              bool findCorners) {
  vector<T3DPointD> pointsArray3D;
  convert(points, pointsArray3D);

  TCubicStroke cubicStroke(pointsArray3D, error, findCorners);
  numSaved = 0;

  //  TStroke *stroke = computeQuadStroke(cubicStroke,error);
  TStroke *stroke = computeQuadStroke(cubicStroke);

#ifdef _DEBUG

  UINT cpIndex = 0;
  TThickPoint p;
  for (; cpIndex != (UINT)stroke->getControlPointCount(); cpIndex++) {
    p = stroke->getControlPoint(cpIndex);
    assert(_finite(p.x));
    assert(_finite(p.y));
    assert(_finite(p.thick));
  }
#endif

  removeNullQuadratic(stroke);
  stroke->invalidate();

  return stroke;
}
//-----------------------------------------------------------------------------
//! Restituisce uno stroke da un vettore di TThickQuadratic
/*!
  Estrae dalle curve i punti di controllo e crea con questi un nuovo stroke
  */
TStroke *TStroke::create(const vector<TThickQuadratic *> &curves) {
  if (curves.empty()) return 0;

  // make a vector of control points
  vector<TThickPoint> ctrlPnts;

  extractStrokeControlPoints(curves, ctrlPnts);

  TStroke *stroke = new TStroke(ctrlPnts);

  stroke->invalidate();

  return stroke;
}

//============================================================================

TStrokeProp::TStrokeProp(const TStroke *stroke)
    : m_stroke(stroke), m_strokeChanged(true), m_mutex() {}

//============================================================================

TStroke::OutlineOptions::OutlineOptions()
    : m_capStyle(ROUND_CAP)
    , m_joinStyle(ROUND_JOIN)
    , m_miterLower(0.0)
    , m_miterUpper(4.0) {}

//-----------------------------------------------------------------------------

TStroke::OutlineOptions::OutlineOptions(UCHAR capStyle, UCHAR joinStyle,
                                        double lower, double upper)
    : m_capStyle(capStyle)
    , m_joinStyle(joinStyle)
    , m_miterLower(lower)
    , m_miterUpper(upper) {}
