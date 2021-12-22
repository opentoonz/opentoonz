

// BlurMatrix.cpp: implementation of the CBlurMatrix class.
//
//////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#include "windows.h"
#else
#ifdef __sgi

#include <time.h>
typedef struct timespec {
  time_t tv_sec; /* seconds */
  long tv_nsec;  /* and nanoseconds */
} timespec_t;
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

#endif
#endif

#include <vector>
#include <algorithm>

#include <cmath>
#include <cstdlib>

//#include "SError.h"
#include "BlurMatrix.h"
//#include "tmsg.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

using namespace std;

CBlurMatrix::CBlurMatrix(const CBlurMatrix &m)
    :  // throw(SBlurMatrixError) :
    m_isSAC(m.m_isSAC)
    , m_isRS(m.m_isRS) {
  try {
    for (int i = 0; i < NBRS; i++) m_m[i] = m.m_m[i];
  } catch (exception) {
    throw SBlurMatrixError();
  }
}

CBlurMatrix::CBlurMatrix(const double d, const int nb, const bool isSAC,
                         const bool isRS)
// throw(SBlurMatrixError)
{
  try {
    m_isRS  = isRS;
    m_isSAC = isSAC;

    for (int i = 0; i < NBRS; i++) m_m[i].clear();

    if (m_isRS) {
      createRandom(d, nb);
    } else
      createEqual(d, nb);

    if (m_isSAC) addPath();
  } catch (SBlurMatrixError) {
    throw;
  }
}

bool CBlurMatrix::isIn(const vector<BLURSECTION> &m, const SXYD &xyd) const {
  for (vector<BLURSECTION>::const_iterator p = m.begin(); p != m.end(); ++p) {
    vector<SXYD>::const_iterator pp = p->begin();
    if (pp->x == xyd.x && pp->y == xyd.y) return true;
  }
  return false;
}

void CBlurMatrix::createRandom(const double d, const int nb)
//   throw(SBlurMatrixError)
{
  int iNb   = 0;
  int l     = I_ROUNDP(ceil(d));
  int ll    = 2 * l + 1;
  double d2 = d * d;
  //	int maxNb=(int)(4.0*d*d*PI/5.0);
  int maxNb = (int)((double)(d * d) * 2.8) + 1;

  try {
    int i = 0;
    for (i = 0; i < NBRS; i++) {
      BLURSECTION bs;
      SXYD xyd = {0, 0, 0.0};
      bs.push_back(xyd);
      m_m[i].push_back(bs);
      iNb++;
    }

    if (nb <= 0 || d <= 0.01) return;

    for (i = 0; i < NBRS; i++) {
      iNb = 1;
      while (iNb < nb && iNb <= maxNb) {
        int x = (rand() % (ll + 1)) - l;
        int y = (rand() % (ll + 1)) - l;
        if (((double)(x * x + y * y) <= d2) && (x != 0 || y != 0)) {
          SXYD xyd1 = {x, y, 0.0};
          if (!isIn(m_m[i], xyd1)) {
            BLURSECTION bs;
            SXYD xyd = {x, y, sqrt((double)(x * x + y * y))};
            bs.push_back(xyd);
            m_m[i].push_back(bs);
            iNb++;
          }
        }
      }
    }
  } catch (exception) {
    throw SBlurMatrixError();
  }
}

void CBlurMatrix::createEqual(const double d, const int nb)
// throw(SBlurMatrixError)
{
  int nq = 0;

  try {
    {
      BLURSECTION bs;
      SXYD xyd = {0, 0, 0.0};
      bs.push_back(xyd);
      m_m[0].push_back(bs);
    }

    if (nb <= 0 || d < 1.0) return;

    double b  = (double)nb * (2.0 * d + 1.0) * (2.0 * d + 1.0) / (d * d * PI);
    b         = sqrt(b);
    b         = ceil(b);
    b         = (2.0 * d + 1.0) / b;
    int di    = (int)ceil(d);
    double d2 = d * d;
    double yd = 0.0;
    for (int y = 0; y <= di;) {
      double xd = 0.0;
      for (int x = 0; x <= di;) {
        double dist = sqrt((double)(x * x + y * y));
        if (dist <= d) {
          {
            BLURSECTION bs;
            SXYD xyd = {x, y, dist};
            bs.push_back(xyd);
            m_m[0].push_back(bs);
            nq++;
          }
          if (x > 0 && y > 0) {
            BLURSECTION bs;
            SXYD xyd = {-x, -y, dist};
            bs.push_back(xyd);
            m_m[0].push_back(bs);
            nq++;
          }
          if (x > 0 && y >= 0) {
            BLURSECTION bs;
            SXYD xyd = {-x, y, dist};
            bs.push_back(xyd);
            m_m[0].push_back(bs);
            nq++;
          }
          if (x >= 0 && y > 0) {
            BLURSECTION bs;
            SXYD xyd = {x, -y, dist};
            bs.push_back(xyd);
            m_m[0].push_back(bs);
            nq++;
          }
        }
        xd += b;
        x = I_ROUND(xd);
      }
      yd += b;
      y = I_ROUND(yd);
    }
  } catch (exception) {
    throw SBlurMatrixError();
  }
}

void CBlurMatrix::addPath(vector<BLURSECTION>::iterator pBS)
// throw(exception)
{
  try {
    if (pBS->size() > 0) {
      BLURSECTION::iterator xyd = pBS->begin();
      // SXYD* xyd= pBS->begin();
      int x     = xyd->x;
      int y     = xyd->y;
      int l     = std::max(std::abs(x), std::abs(y));
      double dx = -(double)x / (double)l;
      double dy = -(double)y / (double)l;
      double xx = (double)x + dx;
      double yy = (double)y + dy;
      for (int i = 1; i <= l; i++, xx += dx, yy += dy) {
        SXYD xyd1 = {I_ROUND(xx), I_ROUND(yy), 0.0};
        pBS->push_back(xyd1);
      }
    }
  } catch (exception) {
    throw;
  }
}

void CBlurMatrix::addPath()  // throw(SBlurMatrixError)
{
  try {
    for (int i = 0; i < (m_isRS ? NBRS : 1); i++) {
      for (vector<BLURSECTION>::iterator pBS = m_m[i].begin();
           pBS != m_m[i].end(); ++pBS) {
        addPath(pBS);
      }
    }
  } catch (exception) {
    throw SBlurMatrixError();
  }
}

CBlurMatrix::~CBlurMatrix() {}

void CBlurMatrix::print() const {
  /*
  char s[1000];
  OutputDebugString("  ====== Blur Matrix ======\n");
  int nb=m_m[0].size();
  for( int i=0; i<nb; i++ ) {
                   int j=0;
                   for( vector<SXYD>::const_iterator pp=m_m[0][i].begin();
                            pp!=m_m[0][i].end();
                            ++pp,++j ) {
                          snprintf(s, sizeof(s),
                            "(%d,%d,%d,%d) ",i,j,pp->x,pp->y);
                          OutputDebugString(s);
                  }
                  OutputDebugString("\n");

  }
  OutputDebugString("  ========================\n");
*/
}
