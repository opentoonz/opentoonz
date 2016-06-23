

#include "tutil.h"
#include "tmathutil.h"
#include "tconvert.h"
#include <iterator>

using TConsts::epsilon;

TMathException::TMathException(std::string msg) : m_msg(::to_wstring(msg)) {}

namespace {

//-------------------------------------------------------------------------

//! maximum order for a polynomial
const int MAX_ORDER = 12;

//! smallest relative error we want
const double RELERROR = 1.0e-14;

//! max power of 10 we wish to search to
const int MAXPOW = 32;

//! max number of iterations
const int MAXIT = 800;

//! a coefficient smaller than SMALL_ENOUGH is considered to be zero (0.0).
const double SMALL_ENOUGH = 1.0e-12;

//-------------------------------------------------------------------------

inline int getEl(int i, int j, int n) { return (j - 1) + (i - 1) * n; }
//-------------------------------------------------------------------------

//! structure type for representing a polynomial
typedef struct {
  int ord;
  double coef[MAX_ORDER + 1];
} poly;

//-------------------------------------------------------------------------

//  compute number of root in (min,max)
double evalpoly(int ord, double *coef, double x);
int numchanges(int np, poly *sseq, double a);
int buildsturm(int ord, poly *sseq);
int modrf(int ord, double *coef, double a, double b, double *val);

//-------------------------------------------------------------------------

void convert(const std::vector<double> &v, poly &p) {
  assert((int)v.size() <= MAX_ORDER);
  if ((int)v.size() > MAX_ORDER) return;

  p.ord = v.size() - 1;

  std::copy(v.begin(), v.end(), p.coef);
}
//-------------------------------------------------------------------------

void convert(const poly &p, std::vector<double> &v) {
  v.resize(p.ord);
  std::copy(p.coef, p.coef + p.ord, v.begin());
}

//-------------------------------------------------------------------------

/*
  * modp
  *
  *  calculates the modulus of u(x) / v(x) leaving it in r, it
  *  returns 0 if r(x) is a constant.
  *  note: this function assumes the leading coefficient of v
  *  is 1 or -1
  */
int modp(poly *u, poly *v, poly *r) {
  int k, j;
  double *nr, *end, *uc;

  nr  = r->coef;
  end = &u->coef[u->ord];

  uc                      = u->coef;
  while (uc <= end) *nr++ = *uc++;

  if (v->coef[v->ord] < 0.0) {
    for (k = u->ord - v->ord - 1; k >= 0; k -= 2) r->coef[k] = -r->coef[k];

    for (k = u->ord - v->ord; k >= 0; k--)
      for (j       = v->ord + k - 1; j >= k; j--)
        r->coef[j] = -r->coef[j] - r->coef[v->ord + k] * v->coef[j - k];
  } else {
    for (k = u->ord - v->ord; k >= 0; k--)
      for (j = v->ord + k - 1; j >= k; j--)
        r->coef[j] -= r->coef[v->ord + k] * v->coef[j - k];
  }

  k = v->ord - 1;
  while (k >= 0 && fabs(r->coef[k]) < SMALL_ENOUGH) {
    r->coef[k] = 0.0;
    k--;
  }

  r->ord = (k < 0) ? 0 : k;

  return (r->ord);
}

//-------------------------------------------------------------------------

/*
  * buildsturm
  *
  *  build up a sturm sequence for a polynomial in sseq, returning
  * the number of polynomials in the sequence
  */
int buildsturm(int ord, poly *sseq) {
  int i;
  double f, *fp, *fc;
  poly *sp;

  sseq[0].ord = ord;
  sseq[1].ord = ord - 1;

  //  calculate the derivative and normalise the leadingcoefficient.
  f  = fabs(sseq[0].coef[ord] * ord);
  fp = sseq[1].coef;
  fc = sseq[0].coef + 1;
  for (i = 1; i <= ord; i++) *fp++ = *fc++ * i / f;

  // construct the rest of the Sturm sequence
  for (sp = sseq + 2; modp(sp - 2, sp - 1, sp); sp++) {
    //  reverse the sign and normalise
    f = -fabs(sp->coef[sp->ord]);
    for (fp = &sp->coef[sp->ord]; fp >= sp->coef; fp--) *fp /= f;
  }

  sp->coef[0] = -sp->coef[0];  // reverse the sign

  return (sp - sseq);
}

//-------------------------------------------------------------------------

/*
  return the number of distinct real roots of the polynomial
  described in sseq
  */
int numroots(int np, poly *sseq, int &atneg, int &atpos) {
  int atposinf = 0, atneginf = 0;

  poly *s;
  double f, lf;

  // change at positive infinity
  lf = sseq[0].coef[sseq[0].ord];

  for (s = sseq + 1; s <= sseq + np; ++s) {
    f = s->coef[s->ord];
    if (0.0 == lf || lf * f < 0.0) ++atposinf;
    lf = f;
  }

  // change at negative infinity
  if (sseq[0].ord & 1)
    lf = -sseq[0].coef[sseq[0].ord];
  else
    lf = sseq[0].coef[sseq[0].ord];

  for (s = sseq + 1; s <= sseq + np; ++s) {
    if (s->ord & 1)
      f = -s->coef[s->ord];
    else
      f = s->coef[s->ord];

    if (0.0 == lf || lf * f < 0.0) ++atneginf;
    lf = f;
  }

  atneg = atneginf;
  atpos = atposinf;

  return atneginf - atposinf;
}

//-------------------------------------------------------------------------

/*
  * numchanges
  *
  * return the number of sign changes in the Sturm sequence in
  * sseq at the value a.
  */
int numchanges(int np, poly *sseq, double a) {
  int changes;
  double f, lf;
  poly *s;

  changes = 0;

  lf = evalpoly(sseq[0].ord, sseq[0].coef, a);

  for (s = sseq + 1; s <= sseq + np; s++) {
    f = evalpoly(s->ord, s->coef, a);
    if (lf == 0.0 || lf * f < 0) changes++;
    lf = f;
  }

  return (changes);
}

//-------------------------------------------------------------------------

/*
  * sbisect
  *
  * uses a bisection based on the sturm sequence for the polynomial
  * described in sseq to isolate intervals in which roots occur,
  * the roots are returned in the roots array in order of magnitude.
  */
void sbisect(int np, poly *sseq, double min, double max, int atmin, int atmax,
             double *roots) {
  double mid;
  int n1 = 0, n2 = 0, its, atmid;

  if ((atmin - atmax) == 1) {
    // first try a less expensive technique.

    if (modrf(sseq->ord, sseq->coef, min, max, &roots[0])) return;

    /*
* if we get here we have to evaluate the root the hard
* way by using the Sturm sequence.
*/
    for (its = 0; its < MAXIT; its++) {
      mid = (min + max) / 2;

      atmid = numchanges(np, sseq, mid);

      if (fabs(mid) > RELERROR) {
        if (fabs((max - min) / mid) < RELERROR) {
          roots[0] = mid;
          return;
        }
      } else if (fabs(max - min) < RELERROR) {
        roots[0] = mid;
        return;
      }

      if ((atmin - atmid) == 0)
        min = mid;
      else
        max = mid;
    }

    if (its == MAXIT) {
      /*
fprintf(stderr, "sbisect: overflow min %f max %f\
diff %e nroot %d n1 %d n2 %d\n",
min, max, max - min, nroot, n1, n2);
*/
      roots[0] = mid;
    }

    return;
  }

  /*
* more than one root in the interval, we have to bisect...
*/
  for (its = 0; its < MAXIT; its++) {
    mid = (min + max) / 2;

    atmid = numchanges(np, sseq, mid);

    n1 = atmin - atmid;
    n2 = atmid - atmax;

    if (n1 != 0 && n2 != 0) {
      sbisect(np, sseq, min, mid, atmin, atmid, roots);
      sbisect(np, sseq, mid, max, atmid, atmax, &roots[n1]);
      break;
    }

    if (n1 == 0)
      min = mid;
    else
      max = mid;
  }

  if (its == MAXIT) {
    /*
fprintf(stderr, "sbisect: roots too close together\n");
fprintf(stderr, "sbisect: overflow min %f max %f diff %e\
nroot %d n1 %d n2 %d\n",
min, max, max - min, nroot, n1, n2);
*/
    for (n1 = atmax; n1 < atmin; n1++) roots[n1 - atmax] = mid;
  }
}

//-------------------------------------------------------------------------

/*
  * evalpoly
  *
  * evaluate polynomial defined in coef returning its value.
    */
double evalpoly(int ord, double *coef, double x) {
  double *fp, f;

  fp = &coef[ord];
  f  = *fp;

  for (fp--; fp >= coef; fp--) f = x * f + *fp;

  return (f);
}

//-------------------------------------------------------------------------

/*
    * modrf
    *
    * uses the modified regula-falsi method to evaluate the root
    * in interval [a,b] of the polynomial described in coef. The
    * root is returned is returned in *val. The routine returns zero
    * if it can't converge.
    */
int modrf(int ord, double *coef, double a, double b, double *val) {
  int its;
  double fa, fb, x, fx, lfx;
  double *fp, *scoef, *ecoef;

  scoef = coef;
  ecoef = &coef[ord];

  fb = fa = *ecoef;
  for (fp = ecoef - 1; fp >= scoef; fp--) {
    fa = a * fa + *fp;
    fb = b * fb + *fp;
  }

  //  if there is no sign difference the method won't work
  if (fa * fb > 0.0) return (0);

  if (fabs(fa) < RELERROR) {
    *val = a;
    return (1);
  }

  if (fabs(fb) < RELERROR) {
    *val = b;
    return (1);
  }

  lfx = fa;

  for (its = 0; its < MAXIT; its++) {
    x = (fb * a - fa * b) / (fb - fa);

    fx = *ecoef;
    for (fp = ecoef - 1; fp >= scoef; fp--) fx = x * fx + *fp;

    if (fabs(x) > RELERROR) {
      if (fabs(fx / x) < RELERROR) {
        *val = x;
        return (1);
      }
    } else if (fabs(fx) < RELERROR) {
      *val = x;
      return (1);
    }

    if ((fa * fx) < 0) {
      b  = x;
      fb = fx;
      if ((lfx * fx) > 0) fa /= 2;
    } else {
      a  = x;
      fa = fx;
      if ((lfx * fx) > 0) fb /= 2;
    }

    lfx = fx;
  }

  // fprintf(stderr, "modrf overflow %f %f %f\n", a, b, fx);

  return (0);
}

//-------------------------------------------------------------------------

/*!
    a x^2 + b x + c

      Remark:
      poly[0] = c
      poly[1] = b
      poly[2] = a
    */
int rootForQuadraticEquation(const std::vector<double> &v,
                             std::vector<double> &sol) {
  double q, delta;

  /*
if( isAlmostZero(v[2]))
{
if ( isAlmostZero(v[1]) )
return -1;

  sol.push_back(-v[0]/v[1]);
  return 1;
  }
*/

  if (isAlmostZero(v[1])) {
    q = -v[0] / v[2];
    if (q < 0)
      return 0;
    else if (isAlmostZero(q)) {
      sol.push_back(0);
      return 1;
    }

    q = sqrt(q);
    sol.push_back(-q);
    sol.push_back(q);

    return 2;
  }

  delta = sq(v[1]) - 4.0 * v[0] * v[2];

  if (delta < 0.0) return 0;

  assert(v[2] != 0);

  if (isAlmostZero(delta)) {
    sol.push_back(-v[1] / (v[2] * 2.0));
    return 1;
  }

  q = -0.5 * (v[1] + tsign(v[1]) * sqrt(delta));

  assert(q != 0);

  sol.push_back(v[0] / q);
  sol.push_back(q / v[2]);

  return 2;
}

//-----------------------------------------------------------------------------

/*
    a x^3+b x^2 + c x + d

      Remark:
      poly[0] = d
      poly[1] = c
      poly[2] = b
      poly[3] = a
    */
int rootForCubicEquation(const std::vector<double> &p,
                         std::vector<double> &sol) {
  /*
if( isAlmostZero(p[3]) )
return rootForQuadraticEquation(p,sol);
*/

  if (isAlmostZero(p[0])) {
    int numberOfSol;
    std::vector<double> redPol(3);
    redPol[0] = p[1];
    redPol[1] = p[2];
    redPol[2] = p[3];

    numberOfSol = rootForQuadraticEquation(redPol, sol);

    for (int i = 0; i < numberOfSol; ++i)
      if (0.0 == sol[i]) return numberOfSol;

    // altrimenti devo contare la soluzione nulla
    ++numberOfSol;
    sol.push_back(0);
    return numberOfSol;
  }

  double inv_v3 = 1.0 / p[3], a = p[2] * inv_v3, b = p[1] * inv_v3,
         c = p[0] * inv_v3;

  static const double inv_3  = 1.0 / 3.0;
  static const double inv_9  = 1.0 / 9.0;
  static const double inv_54 = 1.0 / 54.0;

  double Q = (sq(a) - 3.0 * b) * inv_9,
         R = (2.0 * sq(a) * a - 9.0 * a * b + 27.0 * c) * inv_54;

  double R_2 = sq(R), Q_3 = sq(Q) * Q;

  if (R_2 < Q_3) {
    double Q_sqrt = sqrt(Q);
    double theta  = acos(R / (Q * Q_sqrt));
    sol.push_back(-2 * Q_sqrt * cos(theta * inv_3) - a * inv_3);
    sol.push_back(-2 * Q_sqrt * cos((theta - M_2PI) * inv_3) - a * inv_3);
    sol.push_back(-2 * Q_sqrt * cos((theta + M_2PI) * inv_3) - a * inv_3);
    std::sort(sol.begin(), sol.end());
    return 3;
  }

  double A = -tsign(R) * pow((fabs(R) + sqrt(R_2 - Q_3)), inv_3);
  double B = A != 0 ? Q / A : 0;
  sol.push_back((A + B) - a * inv_3);
  return 1;
}

//-----------------------------------------------------------------------------

int rootForGreaterThanThreeEquation(const std::vector<double> &p,
                                    std::vector<double> &sol) {
  poly sseq[MAX_ORDER];

  convert(p, sseq[0]);

  int np = buildsturm(p.size() - 1, sseq);

  int atmin, atmax;

  int nroot = numroots(np, sseq, atmin, atmax);

  if (nroot == 0) return 0;

  double minVal = -1.0;

  UINT i = 0;

  int nchanges = numchanges(np, sseq, minVal);

  for (i = 0; nchanges != atmin && i != (UINT)MAXPOW; ++i) {
    minVal *= 10.0;
    nchanges = numchanges(np, sseq, minVal);
  }

  if (nchanges != atmin) {
    atmin = nchanges;
  }

  double maxVal = 1.0;
  nchanges      = numchanges(np, sseq, maxVal);

  for (i = 0; nchanges != atmax && i != (UINT)MAXPOW; ++i) {
    maxVal *= 10.0;
    nchanges = numchanges(np, sseq, maxVal);
  }

  if (nchanges != atmax) {
    atmax = nchanges;
  }

  nroot = atmin - atmax;

  assert(nroot > 0);

  poly outPoly;

  outPoly.ord = nroot;

  sbisect(np, sseq, minVal, maxVal, atmin, atmax, outPoly.coef);

  convert(outPoly, sol);

  return nroot < 0 ? -1 : nroot;
}

//-----------------------------------------------------------------------------

}  // end of unnamed namespace

//-----------------------------------------------------------------------------

void tLUDecomposition(double *a, int n, int *indx, double &d) {
  int i, imax, j, k;
  double big, dum, sum, temp;
  std::vector<double> vv(n);

  d = 1.0;
  for (i = 1; i <= n; i++) {
    big = 0.0;
    for (j = 1; j <= n; j++)
      if ((temp = fabs(a[getEl(i, j, n)])) > big) big = temp;

    if (big == 0.0)
      throw TMathException("Singular matrix in routine tLUDecomposition\n");

    vv[i - 1] = 1.0 / big;
  }

  for (j = 1; j <= n; j++) {
    for (i = 1; i < j; i++) {
      sum = a[getEl(i, j, n)];
      for (k = 1; k < i; k++) sum -= a[getEl(i, k, n)] * a[getEl(k, j, n)];
      a[getEl(i, j, n)] = sum;
    }
    big = 0.0;
    for (i = j; i <= n; i++) {
      sum = a[getEl(i, j, n)];
      for (k = 1; k < j; k++) sum -= a[getEl(i, k, n)] * a[getEl(k, j, n)];
      a[getEl(i, j, n)] = sum;
      if ((dum = vv[i - 1] * fabs(sum)) >= big) {
        big  = dum;
        imax = i;
      }
    }

    if (j != imax) {
      for (k = 1; k <= n; k++) {
        dum = a[getEl(imax, k, n)];
        a[getEl(imax, k, n)] = a[getEl(j, k, n)];
        a[getEl(j, k, n)]    = dum;
      }
      d            = -(d);
      vv[imax - 1] = vv[j - 1];
    }

    indx[j - 1] = imax;

    if (a[getEl(j, j, n)] == 0.0) a[getEl(j, j, n)] = TConsts::epsilon;

    if (j != n) {
      dum = 1.0 / (a[getEl(j, j, n)]);
      for (i = j + 1; i <= n; i++) a[getEl(i, j, n)] *= dum;
    }
  }
}

//-----------------------------------------------------------------------------

void tbackSubstitution(double *a, int n, int *indx, double *b) {
  int i, ii = 0, ip, j;
  double sum;

  for (i = 1; i <= n; i++) {
    ip        = indx[i - 1];
    sum       = b[ip - 1];
    b[ip - 1] = b[i - 1];
    if (ii)
      for (j = ii; j <= i - 1; j++) sum -= a[getEl(i, j, n)] * b[j - 1];
    else if (sum)
      ii     = i;
    b[i - 1] = sum;
  }
  for (i = n; i >= 1; i--) {
    sum = b[i - 1];
    for (j   = i + 1; j <= n; j++) sum -= a[getEl(i, j, n)] * b[j - 1];
    b[i - 1] = sum / a[getEl(i, i, n)];
  }
}

//-----------------------------------------------------------------------------

double tdet(double *LUa, int n, double d) {
  for (int i = 1; i <= n; ++i) d *= LUa[getEl(i, i, n)];

  return d;
}

//-----------------------------------------------------------------------------

double tdet(double *a, int n) {
  double d;
  std::vector<int> indx(n);

  tLUDecomposition(a, n, &indx[0], d);
  for (int i = 1; i <= n; ++i) d *= a[getEl(i, i, n)];

  return d;
}

//-----------------------------------------------------------------------------

void tsolveSistem(double *a, int n, double *res) {
  double d;
  std::vector<int> indx(n);

  tLUDecomposition(a, n, &indx[0], d);
  assert(tdet(a, n, d) != 0);
  /*
if( isAlmostZero(tdet(a, n, d)) )
throw TMathException("Singular matrix in routine tLUDecomposition\n");
*/
  tbackSubstitution(a, n, &indx[0], res);
}

//-----------------------------------------------------------------------------

int rootFinding(const std::vector<double> &in_poly, std::vector<double> &sol) {
  // per ora risolvo solo i polinomi di grado al piu' pari a 3
  assert((int)in_poly.size() <= MAX_ORDER);
  if (in_poly.empty() || (int)in_poly.size() > MAX_ORDER) return -1;

  std::vector<double> p;
  std::copy(in_poly.begin(), in_poly.end(), std::back_inserter(p));

  // eat zero in poly
  while (!p.empty() && isAlmostZero(p.back())) p.pop_back();

  sol.clear();

  while (!p.empty() && p.front() == 0) {
    sol.push_back(0.0);
    p.erase(p.begin());  // se i coefficienti bassi sono zero, ci sono soluzioni
                         // pari a 0.0. le metto,
  }                      // e abbasso il grado del polinomio(piu' veloce)

  switch (p.size()) {
  case 0:
    if (sol.empty()) return INFINITE_SOLUTIONS;
    break;
  case 1:  // no solutions
    break;
  case 2:
    sol.push_back(-p[0] / p[1]);
    break;
  case 3:
    rootForQuadraticEquation(p, sol);
    break;
  case 4:
    rootForCubicEquation(p, sol);
    break;
  default:
    rootForGreaterThanThreeEquation(p, sol);
  }
  return sol.size();
}

//-----------------------------------------------------------------------------

/*
*/
int numberOfRootsInInterval(int order, const double *polyH, double min,
                            double max) {
  poly sseq[MAX_ORDER];

  int i, nchanges_0, nchanges_1, np;

  if (order > MAX_ORDER) return -1;

  while (polyH[order] == 0.0 && order > 0) --order;

  // init a sturm's sequence with our polynomious
  for (i = order; i >= 0; --i) sseq[0].coef[i] = polyH[i];

  //  build the Sturm sequence
  np = buildsturm(order, sseq);

  // compute number of variation on 0.0
  nchanges_0 = numchanges(np, sseq, min);

  nchanges_1 = numchanges(np, sseq, max);

  return (nchanges_0 - nchanges_1);
}

//-----------------------------------------------------------------------------

double quadraticRoot(double a, double b, double c) {
  double bb, q;
  // caso lineare
  if (fabs(a) < epsilon) {
    if (fabs(b) >= epsilon) return -c / b;
    return 1;
  }
  bb = b * b;
  q  = bb - 4.0 * a * c;
  if (q < 0.0) return 1;
  q                             = sqrt(q);
  if (b < 0.0) q                = -q;
  q                             = -0.5 * (b + q);
  double root1                  = -1;
  double root2                  = -1;
  if (fabs(q) >= epsilon) root1 = c / q;
  if (fabs(a) >= epsilon) root2 = q / a;
  if (0.0 - epsilon <= root1 && root1 <= 1.0 + epsilon) return root1;
  if (0.0 - epsilon <= root2 && root2 <= 1.0 + epsilon) return root2;
  return 1;
}

//-----------------------------------------------------------------------------

double cubicRoot(double a, double b, double c, double d) {
  double A, Q, R, QQQ, RR;
  double theta;
  /* Test for a quadratic or linear degeneracy			*/
  if (fabs(a) < epsilon) return quadraticRoot(b, c, d);
  /* Normalize							*/
  b /= a;
  c /= a;
  d /= a;
  a = 1.0;
  /* Compute discriminants						*/
  Q   = (b * b - 3.0 * c) / 9.0;
  QQQ = Q * Q * Q;
  R   = (2.0 * b * b * b - 9.0 * b * c + 27.0 * d) / 54.0;
  RR  = R * R;
  /* Three real roots							*/
  if (RR < QQQ) {
    theta = acos(R / sqrt(QQQ));
    double root[3];
    root[0] = root[1] = root[2] = -2.0 * sqrt(Q);
    root[0] *= cos(theta / 3.0);
    root[1] *= cos((theta + M_2PI) / 3.0);
    root[2] *= cos((theta - M_2PI) / 3.0);
    root[0] -= b / 3.0;
    root[1] -= b / 3.0;
    root[2] -= b / 3.0;
    if (0.0 - epsilon < root[0] && root[0] < 1.0 + epsilon) return root[0];
    if (0.0 - epsilon < root[1] && root[1] < 1.0 + epsilon) return root[1];
    if (0.0 - epsilon < root[2] && root[2] < 1.0 + epsilon) return root[2];
    return 1;
  }
  /* One real root							*/
  else {
    double root = 0;
    A           = -pow(fabs(R) + sqrt(RR - QQQ), 1.0 / 3.0);
    if (A != 0.0) {
      if (R < 0.0) A = -A;
      root           = A + Q / A;
    }
    root -= b / 3.0;
    if (0.0 - epsilon < root && root < 1.0 + epsilon) return root;
    return 1;
  }
}

//-----------------------------------------------------------------------------
//  End Of File
//-----------------------------------------------------------------------------
