

#include "tcenterlinevectP.h"

//==========================================================================

//============================================
//      Sequence conversion into TStroke
//============================================

// Globals

namespace {
const double Polyg_eps_max = 1;     // Sequence simplification max error
const double Polyg_eps_mul = 0.75;  // Sequence simpl. thickness-multiplier
                                    // error

const double Quad_eps_max =
    infinity;  // As above, for sequence conversion into strokes
// const double Quad_eps_mul= 0.2;     //NOTE: Substituted by
// globals->currConfig->m_penalty
}

//--------------------------------------------------------------------------

//-------------------------------
//      Simplify Sequences
//-------------------------------

// EXPLANATION:  Before converting sequences in strokes, we simplify them
// by eliminating sequence of points which lie on the same straight line,
// leaving the extremities only.

class SequenceSimplifier {
  const Sequence *m_s;
  const SkeletonGraph *m_graph;

private:
  class Length {
  public:
    int n;
    double l;
    UINT firstNode, secondNode;

    Length() : n(0), l(0) {}
    Length(int n_, double l_) : n(n_), l(l_) {}

    inline void infty(void) {
      n = infinity;
      l = infinity;
    }
    inline bool operator<(Length sl) {
      return n < sl.n ? 1 : n > sl.n ? 0 : l < sl.l ? 1 : 0;
    }
    inline Length operator+(Length sl) { return Length(n + sl.n, l + sl.l); }
  };

  Length lengthOf(UINT a, UINT aLink, UINT b);

public:
  // Methods
  SequenceSimplifier(const Sequence *s) : m_s(s), m_graph(m_s->m_graphHolder) {}

  void simplify(std::vector<unsigned int> &result);
};

//--------------------------------------------------------------------------

// Bellman algorithm for Sequences
// NOTE: Circular Sequences are dealt.
void SequenceSimplifier::simplify(std::vector<unsigned int> &result) {
  // Initialize variables
  unsigned int n;
  unsigned int i, j, iLink, jLink;

  // NOTE: If s is circular, we have to protect

  i     = m_s->m_head;
  iLink = m_s->m_headLink;
  // NOTE: If m_head==m_tail then we have to force the first step by "|| n==1"
  for (n = 1; i != m_s->m_tail || n == 1; ++n, m_s->next(i, iLink))
    ;

  Length L_att, L_min, l_min, l_ji;
  unsigned int p_i, a, b;

  std::vector<Length> M(n);
  std::vector<Length> K(n);
  std::vector<unsigned int> P(n);

  // Search for minimal path
  i     = m_s->m_head;
  iLink = m_s->m_headLink;
  for (a = 1; i != m_s->m_tail || a == 1; m_s->next(i, iLink), ++a) {
    L_min.infty();
    l_min.infty();
    p_i = 0;

    j                  = m_s->m_head;
    jLink              = m_s->m_headLink;
    unsigned int iNext = m_graph->getNode(i).getLink(iLink).getNext();
    for (b = 0; j != iNext || b == 0; m_s->next(j, jLink), ++b) {
      if ((L_att = M[b] + (l_ji = lengthOf(j, jLink, iNext))) < L_min) {
        L_min = L_att;
        p_i   = b;
        l_min = l_ji;
      }
    }
    M[a] = L_min;
    K[a] = l_min;
    P[a] = p_i;
  }

  // Copies minimal path found to the output reducedIndices vector
  // NOTE: size() is added due to circular sequences case handling
  unsigned int redSize = result.size();

  result.resize(redSize + M[n - 1].n + 1);

  result[redSize + M[n - 1].n] = K[n - 1].secondNode;
  for (b = n - 1, a = redSize + M[n - 1].n - 1; b > 0; b = P[b], --a)
    result[a] = K[b].firstNode;
}

//--------------------------------------------------------------------------

// Length between two sequence points
SequenceSimplifier::Length SequenceSimplifier::lengthOf(UINT a, UINT aLink,
                                                        UINT b) {
  UINT curr, old;
  T3DPointD v;
  double d, vv;
  Length res;

  res.n          = 1;
  res.firstNode  = a;
  res.secondNode = b;

  v  = *m_graph->getNode(b) - *m_graph->getNode(a);
  vv = norm(v);

  curr = m_graph->getNode(a).getLink(aLink).getNext();
  old  = a;

  // If the distance between extremities is small, check if the same holds
  // for internal points; if so, ok - otherwise set infty().
  if (vv < 0.1) {
    for (; curr != b; m_s->advance(old, curr)) {
      d = tdistance(*m_graph->getNode(curr), *m_graph->getNode(a));
      if (d > 0.1) res.infty();
    }
    return res;
  }

  // Otherwise, check distances from line passing from a and b
  v = v * (1 / vv);

  for (; curr != b; m_s->advance(old, curr)) {
    d = tdistance2(*m_graph->getNode(curr), v, *m_graph->getNode(a));
    if (d >
        std::min(m_graph->getNode(curr)->z * Polyg_eps_mul, Polyg_eps_max)) {
      res.infty();
      return res;
    } else
      res.l += d;
  }

  return res;
}

//==========================================================================

//===============================
//      Sequence conversion
//===============================

// EXPLANATION: Sequences convert into TStrokes by applying a SequenceConverter
//  class. A graph minimal-path algorithm is run by using a
//  lexicographic-ordered
//  (number of quadratics, error) length.

class SequenceConverter {
  const Sequence *m_s;
  const SkeletonGraph *m_graph;

  double m_penalty;

public:
  // Length construction globals (see 'lengthOf' method)
  unsigned int middle;
  std::vector<double> pars;

  class Length {
  public:
    int n;
    double l;
    std::vector<T3DPointD> CPs;

    Length() : n(0), l(0) {}
    Length(int n_, double l_) : n(n_), l(l_) {}

    inline void infty(void) {
      n = infinity;
      l = infinity;
    }
    inline bool operator<(Length sl) {
      return n < sl.n ? 1 : n > sl.n ? 0 : l < sl.l ? 1 : 0;
    }
    inline Length operator+(Length sl) { return Length(n + sl.n, l + sl.l); }

    void set_CPs(const T3DPointD &a, const T3DPointD &b, const T3DPointD &c) {
      CPs.resize(3);
      CPs[0] = a;
      CPs[1] = b;
      CPs[2] = c;
    }
    void set_CPs(const T3DPointD &a, const T3DPointD &b, const T3DPointD &c,
                 const T3DPointD &d, const T3DPointD &e) {
      CPs.resize(5);
      CPs[0] = a;
      CPs[1] = b;
      CPs[2] = c;
      CPs[3] = d;
      CPs[4] = e;
    }
  };

  // Intermediate Sequence form
  std::vector<T3DPointD> middleAddedSequence;
  std::vector<unsigned int> *inputIndices;

  // Methods
  SequenceConverter(const Sequence *s, double penalty)
      : m_s(s), m_graph(m_s->m_graphHolder), m_penalty(penalty) {}

  Length lengthOf(unsigned int a, unsigned int b);
  void addMiddlePoints();
  TStroke *operator()(std::vector<unsigned int> *indices);

  // Length construction methods
  bool parametrize(unsigned int a, unsigned int b);
  void lengthOfTriplet(unsigned int i, Length &len);
  bool calculateCPs(unsigned int i, unsigned int j, Length &len);
  bool penalty(unsigned int a, unsigned int b, Length &len);
};

//--------------------------------------------------------------------------

// Changes in stroke thickness are considered more penalizating
inline double ellProd(const T3DPointD &a, const T3DPointD &b) {
  return a.x * b.x + a.y * b.y + 5 * a.z * b.z;
}

//--------------------------------------------------------------------------

// EXPLANATION:  After simplification, we receive a vector<UINT> of indices
// corresponding to the vertices of the simplified current sequence.
// Before beginning conversion, we need to add middle points between the
// above vertex points.

inline void SequenceConverter::addMiddlePoints() {
  unsigned int i, j, n;

  n = inputIndices->size();
  middleAddedSequence.clear();

  if (n == 2) {
    middleAddedSequence.resize(3);
    middleAddedSequence[0] = *m_graph->getNode((*inputIndices)[0]);
    middleAddedSequence[1] = (*m_graph->getNode((*inputIndices)[0]) +
                              *m_graph->getNode((*inputIndices)[1])) *
                             0.5;
    middleAddedSequence[2] = *m_graph->getNode((*inputIndices)[1]);
  } else {
    middleAddedSequence.resize(2 * n - 3);
    middleAddedSequence[0] = *m_graph->getNode((*inputIndices)[0]);
    for (i = j = 1; i < n - 2; ++i, j += 2) {
      middleAddedSequence[j]     = *m_graph->getNode((*inputIndices)[i]);
      middleAddedSequence[j + 1] = (*m_graph->getNode((*inputIndices)[i]) +
                                    *m_graph->getNode((*inputIndices)[i + 1])) *
                                   0.5;
    }
    middleAddedSequence[j]     = *m_graph->getNode((*inputIndices)[n - 2]);
    middleAddedSequence[j + 1] = *m_graph->getNode((*inputIndices)[n - 1]);
  }
}

//--------------------------------------------------------------------------

TStroke *SequenceConverter::operator()(std::vector<unsigned int> *indices) {
  // Prepare Sequence
  inputIndices = indices;
  addMiddlePoints();

  // Initialize local variables
  unsigned int n =
      (middleAddedSequence.size() + 1) / 2;  // Number of middle points
  // unsigned int i, j;
  unsigned int i;
  int j;

  Length L_att, L_min, l_min, l_ji;
  unsigned int p_i, a, b;

  std::vector<Length> M(n);
  std::vector<Length> K(n);
  std::vector<unsigned int> P(n);

  // Bellman algorithm
  for (i = 2, a = 1; i < middleAddedSequence.size(); i += 2, ++a) {
    L_min.infty();
    l_min.infty();
    p_i = 0;
    // for(j=0, b=0; j<i; j+=2, ++b)
    for (j = i - 2, b = j / 2; j >= 0; j -= 2, --b) {
      if ((L_att = M[b] + (l_ji = lengthOf(j, i))) < L_min) {
        L_min = L_att;
        p_i   = b;
        l_min = l_ji;
      }
      // NOTE: The following else may be taken out to perform a deeper
      // search for optimal result. However, it prevents quadratic complexities
      // on large-scale images.
      else if (l_ji.n == infinity)
        break;  // Stops searching for current i
    }
    M[a] = L_min;
    K[a] = l_min;
    P[a] = p_i;
  }

  // Read off the output
  std::vector<TThickPoint> controlPoints(2 * M[n - 1].n + 1);

  for (b = n - 1, a = 2 * M[n - 1].n; b > 0; b = P[b]) {
    for (i             = K[b].CPs.size() - 1; i > 0; --i, --a)
      controlPoints[a] = K[b].CPs[i];
  }
  controlPoints[0] = middleAddedSequence[0];

  TStroke *res = new TStroke(controlPoints);

  return res;
}

//--------------------------------------------------------------------------

//--------------------------------------
//      Conversion Length build-up
//--------------------------------------

SequenceConverter::Length SequenceConverter::lengthOf(unsigned int a,
                                                      unsigned int b) {
  Length l;

  // If we have a triplet, apply a specific procedure
  if (b == a + 2) {
    lengthOfTriplet(a, l);
    return l;
  }
  // otherwise
  if (!parametrize(a, b) || !calculateCPs(a, b, l) || !penalty(a, b, l))
    l.infty();
  return l;
}

//--------------------------------------------------------------------------

void SequenceConverter::lengthOfTriplet(unsigned int i, Length &len) {
  T3DPointD A = middleAddedSequence[i];
  T3DPointD B = middleAddedSequence[i + 1];
  T3DPointD C = middleAddedSequence[i + 2];

  // We assume that this conversion is faithful, avoiding length penalty
  len.l    = 0;
  double d = tdistance(B, C - A, A);
  if (d <= 2) {
    len.n = 1;
    len.set_CPs(A, B, C);
  } else if (d <= 6) {
    len.n       = 2;
    d           = (d - 1) / d;
    T3DPointD U = A + d * (B - A), V = C + d * (B - C);
    len.set_CPs(A, U, (U + V) * 0.5, V, C);
  } else {
    len.n = 2;
    len.set_CPs(A, (A + B) * 0.5, B, (B + C) * 0.5, C);
  }
}

//--------------------------------------------------------------------------

bool SequenceConverter::parametrize(unsigned int a, unsigned int b) {
  unsigned int curr, old;
  unsigned int i;
  double w, t;
  double den;

  pars.clear();
  pars.push_back(0);

  for (old = a, curr = a + 1, den = 0; curr < b; old = curr, curr += 2) {
    w = norm(middleAddedSequence[curr] - middleAddedSequence[old]);
    den += w;
    pars.push_back(w);
  }
  w = norm(middleAddedSequence[b] - middleAddedSequence[old]);
  den += w;
  pars.push_back(w);

  if (den < 0.1) return 0;

  for (i = 1, t = 0; i < pars.size(); ++i) {
    t += 2 * pars[i] / den;
    pars[i] = t;
  }

  // Seek the interval which holds 1 - the middle interval
  for (middle = 0; middle < pars.size() && pars[middle + 1] <= 1; ++middle)
    ;

  return 1;
}

//==========================================================================

//------------------------
//    CP construction
//------------------------

// NOTE: Check my thesis for variable meanings (int_ stands for 'integral').

// Some integrals (int_) for the CP linear system resolution

inline T3DPointD int_H(const T3DPointD &A, const T3DPointD &B, double t1,
                       double t2) {
  return -(0.375 * (pow(t2, 4) - pow(t1, 4))) * B +
         (pow(t2, 3) - pow(t1, 3)) * (B * 0.6667 - A * 0.5) +
         (pow(t2, 2) - pow(t1, 2)) * A;
}

//--------------------------------------------------------------------------

inline T3DPointD int_K(const T3DPointD &A, const T3DPointD &B, double t1,
                       double t2) {
  return (pow(t2, 4) - pow(t1, 4)) * (B * 0.125) +
         (pow(t2, 3) - pow(t1, 3)) * (A * 0.1667);
}

//--------------------------------------------------------------------------

bool SequenceConverter::calculateCPs(unsigned int i, unsigned int j,
                                     Length &len) {
  unsigned int curr, old;

  TAffine M;
  TPointD l;
  T3DPointD a, e, x, y, A, B;
  T3DPointD IH, IK, IM, IN_;  //"IN" seems to be reserved word
  double HxL, KyL, MxO, NyO;
  unsigned int k;

  a = middleAddedSequence[i];
  e = middleAddedSequence[j];
  x = middleAddedSequence[i + 1] - a;
  y = middleAddedSequence[j - 1] - e;

  // Build TAffine M
  double par = ellProd(x, y) / 5;
  M          = TAffine(ellProd(x, x) / 3, par, 0, par, ellProd(y, y) / 3, 0);

  // Costruisco il termine noto b:
  // Calculate polygonal integrals

  // Integral from 0.0 to 1.0
  for (k = 0, old = i, curr = i + 1; k < middle; ++k, old = curr, curr += 2) {
    B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
        (1 / (pars[k + 1] - pars[k]));
    A = middleAddedSequence[old] - pars[k] * B;
    IH += int_H(A, B, pars[k], pars[k + 1]);
    IK += int_K(A, B, pars[k], pars[k + 1]);
  }

  if (curr == j + 1) curr = j;
  B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
      (1 / (pars[k + 1] - pars[k]));
  A = middleAddedSequence[old] - pars[k] * B;
  IH += int_H(A, B, pars[k], 1.0);
  IK += int_K(A, B, pars[k], 1.0);

  // Integral from 1.0 to 2.0
  for (k = pars.size() - 1, old = j, curr = j - 1; k > middle + 1;
       --k, old = curr, curr -= 2) {
    B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
        (1 / (pars[k] - pars[k - 1]));
    A = middleAddedSequence[curr] - (2 - pars[k - 1]) * B;
    IM += int_K(A, B, 2 - pars[k], 2 - pars[k - 1]);
    IN_ += int_H(A, B, 2 - pars[k], 2 - pars[k - 1]);
  }

  if (old == i + 1) curr = i;
  B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
      (1 / (pars[k] - pars[k - 1]));
  A = middleAddedSequence[curr] - (2 - pars[k - 1]) * B;
  IM += int_K(A, B, 2 - pars[k], 1.0);
  IN_ += int_H(A, B, 2 - pars[k], 1.0);

  // Polygonal-free integrals
  T3DPointD f = (a + e) * 0.5;
  HxL         = (ellProd(a, x) * 0.3) + (ellProd(f, x) / 5.0);
  NyO         = (ellProd(e, y) * 0.3) + (ellProd(f, y) / 5.0);
  KyL         = (ellProd(a, y) / 15.0) + (ellProd(f, y) / 10.0);
  MxO         = ((e * x) / 15.0) + (ellProd(f, x) / 10.0);

  // Infine, ho il termine noto
  l = TPointD(ellProd(IH, x) - HxL + ellProd(IM, x) - MxO,
              ellProd(IK, y) - KyL + ellProd(IN_, y) - NyO);
  M.a13 = -l.x;
  M.a23 = -l.y;

  // Check validity conditions:
  //  a) System is not singular
  if (fabs(M.det()) < 0.01) return 0;

  M = M.inv();

  //  b) Shift (solution) is positive
  if (M.a13 < 0 || M.a23 < 0) return 0;
  T3DPointD b = a + M.a13 * x;
  T3DPointD d = e + M.a23 * y;

  //  c) The height of every CP must be >=0
  if (b.z < 0 || d.z < 0) return 0;
  len.set_CPs(a, b, (b + d) * 0.5, d, e);

  return 1;
}

//==========================================================================

//------------------------
//      Penalties
//------------------------

inline T3DPointD int_B0a(const T3DPointD &A, const T3DPointD &B, double t1,
                         double t2) {
  return (0.25 * (pow(t2, 4) - pow(t1, 4))) * B +
         ((pow(t2, 3) - pow(t1, 3)) / 3.0) * (A - 2.0 * B) +
         (0.5 * (pow(t2, 2) - pow(t1, 2))) * (B - 2.0 * A) + (t2 - t1) * A;
}

//--------------------------------------------------------------------------

inline T3DPointD int_B1a(const T3DPointD &A, const T3DPointD &B, double t1,
                         double t2) {
  return -(0.5 * (pow(t2, 4) - pow(t1, 4))) * B +
         (2.0 * ((pow(t2, 3) - pow(t1, 3)) / 3.0) * (B - A) +
          (pow(t2, 2) - pow(t1, 2)) * A);
}

//--------------------------------------------------------------------------

inline T3DPointD int_B2a(const T3DPointD &A, const T3DPointD &B, double t1,
                         double t2) {
  return (0.25 * (pow(t2, 4) - pow(t1, 4))) * B +
         ((pow(t2, 3) - pow(t1, 3)) / 3.0) * A;
}

//--------------------------------------------------------------------------

inline double int_a2(const T3DPointD &A, const T3DPointD &B, double t1,
                     double t2) {
  return ellProd(A, A) * (t2 - t1) + ellProd(A, B) * (pow(t2, 2) - pow(t1, 2)) +
         (ellProd(B, B) * (pow(t2, 3) - pow(t1, 3)) / 3.0);
}

//--------------------------------------------------------------------------

// Penalty is the integral of the square norm of differences between polygonal
// and quadratics.
bool SequenceConverter::penalty(unsigned int a, unsigned int b, Length &len) {
  unsigned int curr, old;

  const std::vector<T3DPointD> &CPs = len.CPs;
  T3DPointD A, B, P0, P1, P2;
  double p, p_max;
  unsigned int k;

  len.n = 2;  // A couple of arcs

  // Prepare max penalty p_max
  p_max = 0;
  for (curr = a + 1, old = a, k = 0; curr < b; ++k, old = curr, curr += 2) {
    p_max += (middleAddedSequence[curr].z + middleAddedSequence[old].z) *
             (pars[k + 1] - pars[k]) / 2;
  }
  p_max += (middleAddedSequence[b].z + middleAddedSequence[old].z) *
           (pars[k + 1] - pars[k]) / 2;

  // Confronting 4th power of error with mean polygonal thickness
  // - can be changed
  p_max = std::min(sqrt(p_max) * m_penalty, Quad_eps_max);

  // CP only integral
  p = (ellProd(CPs[0], CPs[0]) + 2 * ellProd(CPs[2], CPs[2]) +
       ellProd(CPs[4], CPs[4]) + ellProd(CPs[0], CPs[1]) +
       ellProd(CPs[1], CPs[2]) + ellProd(CPs[2], CPs[3]) +
       ellProd(CPs[3], CPs[4])) /
          5.0 +
      (2 * (ellProd(CPs[1], CPs[1]) + ellProd(CPs[3], CPs[3])) +
       ellProd(CPs[0], CPs[2]) + ellProd(CPs[2], CPs[4])) /
          15.0;

  // Penalty from 0.0 to 1.0
  P0 = P1 = P2 = T3DPointD();
  for (k = 0, old = a, curr = a + 1; k < middle; ++k, old = curr, curr += 2) {
    B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
        (1 / (pars[k + 1] - pars[k]));
    A = middleAddedSequence[old] - pars[k] * B;

    // Mixed integral
    P0 += int_B0a(A, B, pars[k], pars[k + 1]);
    P1 += int_B1a(A, B, pars[k], pars[k + 1]);
    P2 += int_B2a(A, B, pars[k], pars[k + 1]);

    // Sequence integral
    p += int_a2(A, B, pars[k], pars[k + 1]);
  }
  if (curr == b + 1) curr = b;
  B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
      (1 / (pars[k + 1] - pars[k]));
  A = middleAddedSequence[old] - pars[k] * B;

  // Mixed integral
  P0 += int_B0a(A, B, pars[k], 1.0);
  P1 += int_B1a(A, B, pars[k], 1.0);
  P2 += int_B2a(A, B, pars[k], 1.0);

  // Sequence integral
  p += int_a2(A, B, pars[k], 1.0);

  p -= 2 * (ellProd(P0, CPs[0]) + ellProd(P1, CPs[1]) + ellProd(P2, CPs[2]));

  // Penalty from 1.0 to 2.0
  P0 = P1 = P2 = T3DPointD();
  for (k = pars.size() - 1, old = b, curr = b - 1; k > middle + 1;
       --k, old = curr, curr -= 2) {
    B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
        (1 / (pars[k] - pars[k - 1]));
    A = middleAddedSequence[curr] - (2 - pars[k - 1]) * B;

    // Mixed integral
    P0 += int_B0a(A, B, 2 - pars[k], 2 - pars[k - 1]);
    P1 += int_B1a(A, B, 2 - pars[k], 2 - pars[k - 1]);
    P2 += int_B2a(A, B, 2 - pars[k], 2 - pars[k - 1]);

    // Sequence integral
    p += int_a2(A, B, 2 - pars[k], 2 - pars[k - 1]);
  }
  if (old == a + 1) curr = a;
  B = (middleAddedSequence[curr] - middleAddedSequence[old]) *
      (1 / (pars[k] - pars[k - 1]));
  A = middleAddedSequence[curr] - (2 - pars[k - 1]) * B;

  // Mixed integral
  P0 += int_B0a(A, B, 2 - pars[k], 1.0);
  P1 += int_B1a(A, B, 2 - pars[k], 1.0);
  P2 += int_B2a(A, B, 2 - pars[k], 1.0);

  // Sequence integral
  p += int_a2(A, B, 2 - pars[k], 1.0);

  p -= 2 * (ellProd(P0, CPs[4]) + ellProd(P1, CPs[3]) + ellProd(P2, CPs[2]));

  // OCCHIO! Ho visto ancora qualche p<0! Da rivedere - non dovrebbe...
  if (p > p_max || p < 0)
    return 0;
  else
    len.l = p;

  return 1;
}

//--------------------------------------------------------------------------

//-----------------------------
//      Conversion Mains
//-----------------------------

inline TStroke *convert(const Sequence &s, double penalty) {
  SkeletonGraph *graph = s.m_graphHolder;

  TStroke *result;

  // First, we simplify the skeleton sequences found
  std::vector<unsigned int> reducedIndices;

  // NOTE: If s is circular, we have to protect head==tail 's adjacent nodes.
  // We then move away s tail and head, and insert them in the reducedIndices
  // apart from simplification.
  if (s.m_head == s.m_tail && graph->getNode(s.m_head).degree() == 2) {
    Sequence t = s;

    SequenceSimplifier simplifier(&t);
    reducedIndices.push_back(s.m_head);

    t.m_head     = graph->getNode(s.m_head).getLink(0).getNext();
    t.m_headLink = !graph->getNode(t.m_head).linkOfNode(s.m_head);
    t.m_tail     = graph->getNode(s.m_tail).getLink(1).getNext();
    t.m_tailLink = !graph->getNode(t.m_tail).linkOfNode(s.m_tail);

    simplifier.simplify(reducedIndices);
    reducedIndices.push_back(s.m_tail);
  } else {
    SequenceSimplifier simplifier(&s);
    simplifier.simplify(reducedIndices);
  }

  // For segments, apply this immediate conversion
  if (reducedIndices.size() == 2) {
    std::vector<TThickPoint> segment(3);
    segment[0] = *graph->getNode(s.m_head);
    segment[1] = (*graph->getNode(s.m_head) + *graph->getNode(s.m_tail)) * 0.5;
    segment[2] = *graph->getNode(s.m_tail);
    return new TStroke(segment);
  }

  // Then, we convert the sequence in a quadratic stroke
  SequenceConverter converter(&s, penalty);
  result = converter(&reducedIndices);

  // If it is a circular stroke, setSelfLoop
  // NOTA: Sembra che pero' in questo modo non venga assegnato colore al confine
  // con la cornice!!!
  //        => Solo nel caso outline...?
  // NOTA: Considera anche che pure le outline possono essere splittate per la
  // colorazione!!
  // if(globals->currConfig->m_maxThickness == 0.0 && s.m_head == s.m_tail &&
  // s.m_graphHolder->getNode(s.m_head).degree() == 2)
  // //globals->currConfig->m_outline
  //  result->setSelfLoop(true);

  // Pass the SkeletonArc::SS_OUTLINE attribute to the output stroke
  if (graph->getNode(s.m_head)
          .getLink(s.m_headLink)
          ->hasAttribute(SkeletonArc::SS_OUTLINE))
    result->setFlag(SkeletonArc::SS_OUTLINE, true);
  else if (graph->getNode(s.m_head)
               .getLink(s.m_headLink)
               ->hasAttribute(SkeletonArc::SS_OUTLINE_REVERSED))
    result->setFlag(SkeletonArc::SS_OUTLINE_REVERSED, true);

  return result;
}

//--------------------------------------------------------------------------

// Converts each forward or single Sequence of the image in its corresponding
// TStroke. Output is a vector<TStroke*>* whose ownership belongs to the user.
// This allow sorts on the TStroke vector *before* adding any stroke to the
// output TVectorImage.
// std::vector<TStroke*>* conversionToStrokes(void)
void conversionToStrokes(std::vector<TStroke *> &strokes,
                         VectorizerCoreGlobals &g) {
  SequenceList &singleSequences           = g.singleSequences;
  JointSequenceGraphList &organizedGraphs = g.organizedGraphs;
  double penalty                          = g.currConfig->m_penalty;

  unsigned int i, j, k;

  // Convert single sequences
  for (i = 0; i < singleSequences.size(); ++i) {
    if (singleSequences[i].m_head == singleSequences[i].m_tail) {
      // If the sequence is circular, move your endpoints to an edge middle, in
      // order
      // to allow a soft junction
      SkeletonGraph *currGraph = singleSequences[i].m_graphHolder;

      unsigned int head     = singleSequences[i].m_head;
      unsigned int headLink = singleSequences[i].m_headLink;
      unsigned int next = currGraph->getNode(head).getLink(headLink).getNext();
      unsigned int nextLink = currGraph->getNode(next).linkOfNode(head);

      unsigned int addedNode = singleSequences[i].m_graphHolder->newNode(
          (*currGraph->getNode(head) + *currGraph->getNode(next)) * 0.5);

      singleSequences[i].m_graphHolder->insert(addedNode, head, headLink);
      *singleSequences[i].m_graphHolder->node(addedNode).link(0) =
          *singleSequences[i].m_graphHolder->node(head).link(headLink);

      singleSequences[i].m_graphHolder->insert(addedNode, next, nextLink);
      *singleSequences[i].m_graphHolder->node(addedNode).link(1) =
          *singleSequences[i].m_graphHolder->node(next).link(nextLink);

      singleSequences[i].m_head     = addedNode;
      singleSequences[i].m_headLink = 0;
      singleSequences[i].m_tail     = addedNode;
      singleSequences[i].m_tailLink = 1;
    }

    strokes.push_back(convert(singleSequences[i], penalty));
  }

  // Convert graph sequences
  for (i = 0; i < organizedGraphs.size(); ++i)
    for (j = 0; j < organizedGraphs[i].getNodesCount(); ++j)
      if (!organizedGraphs[i].getNode(j).hasAttribute(
              JointSequenceGraph::ELIMINATED))
        // Otherwise eliminated by junction recovery
        for (k = 0; k < organizedGraphs[i].getNode(j).getLinksCount(); ++k) {
          // A sequence is taken at both extremities in our organized graphs
          if (organizedGraphs[i].getNode(j).getLink(k)->isForward())
            strokes.push_back(
                convert(*organizedGraphs[i].getNode(j).getLink(k), penalty));
        }
}
