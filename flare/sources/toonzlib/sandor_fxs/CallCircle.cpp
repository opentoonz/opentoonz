

// CallCircle.cpp: implementation of the CCallCircle class.
//
//////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdlib.h>
#include <search.h>
//#include "myetc.h"

#include "CallCircle.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static int callcircle_xydwCompare(const void *a, const void *b) {
  SXYDW *aa = (SXYDW *)a;
  SXYDW *bb = (SXYDW *)b;

  if (aa->w < bb->w) return -1;
  if (aa->w > bb->w) return 1;
  return 0;
}

CCallCircle::CCallCircle(const double r) : m_r(r), m_nb(0) {
  int rr = (int)r + 1;
  rr *= 2;
  int dd  = rr * 2 + 1;
  int dd2 = dd * dd;

  if (dd2 == 0) {
    null();
    return;
  }

  m_c.reset(new SXYDW[dd2]);
  if (!m_c) throw SMemAllocError("in callCircle");
  for (int y = -rr; y <= rr; y++)
    for (int x = -rr; x <= rr; x++) {
      double d = sqrt((double)(x * x + y * y));
      if (d <= r && m_nb < dd2) {
        m_c[m_nb].x = x;
        m_c[m_nb].y = y;
        m_c[m_nb].w = d;
        m_nb++;
      }
    }
  qsort(m_c.get(), m_nb, sizeof(SXYDW), callcircle_xydwCompare);
}

void CCallCircle::null() {
  m_nb = 0;
  m_r  = 0.0;
  m_c.reset();
}

CCallCircle::~CCallCircle() { null(); }

void CCallCircle::print() {
  /*smsg_info(" --- CCallCircle ---");
  smsg_info(" m_nb=%d", m_nb);
  smsg_info(" m_r=%f", m_r);
  for( int i=0; i<m_nb; i++ )
          smsg_info(" %d. = (%d,%d,%f)", i, m_c[i].x,m_c[i].y,m_c[i].w);
  smsg_info(" --- ----------- ---\n");
*/
}

void CCallCircle::draw(UCHAR *drawB, const int lX, const int lY, const int xx,
                       const int yy, const double r) {
  double aa = 2.0 * r / 3.0;  // Size of antialiasing

  for (int i = 0; i < m_nb && m_c[i].w <= r; i++) {
    double w = m_c[i].w;
    int x    = xx + m_c[i].x;
    int y    = yy + m_c[i].y;
    if (x >= 0 && y >= 0 && x < lX && y < lY) {
      UCHAR *pDB = drawB + y * lX + x;
      if (w <= aa) {
        *pDB = (UCHAR)255;
      } else {
        double q  = 255.0 * (r - w) / (r - aa);
        q         = D_CUT_0_255(q);
        UCHAR ucq = UC_ROUND(q);
        (*pDB)    = (*pDB) < ucq ? ucq : (*pDB);
      }
    }
  }
}
