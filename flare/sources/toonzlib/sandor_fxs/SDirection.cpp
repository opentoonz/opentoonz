#include <memory>

// SDirection.cpp: implementation of the CSDirection class.
//
//////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <memory.h>
#include <algorithm>
#include "SDirection.h"
#include "SError.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSDirection::CSDirection() : m_lX(0), m_lY(0), m_lDf(0) {
  for (int i = 0; i < NBDIR; i++) m_df[i] = 0;
}

CSDirection::CSDirection(const int lX, const int lY, const UCHAR *sel,
                         const int sens)
    : m_lX(lX), m_lY(lY), m_lDf(0) {
  for (int i = 0; i < NBDIR; i++) m_df[i] = 0;

  try {
    if (m_lX > 0 && m_lY > 0) {
      m_dir.reset(new UCHAR[m_lX * m_lY]);
      if (!m_dir) {
        null();
        throw SMemAllocError("in directionMap");
      }
      memcpy(m_dir.get(), sel, sizeof(UCHAR) * m_lX * m_lY);
      setDir01();
      //			For optimalization purpose.
      //			The quality is better, if it is removed.
      //			setContourBorder(2);
      makeDirFilter(sens);
    }
  } catch (SMemAllocError) {
    throw;
  }
}

CSDirection::CSDirection(const int lX, const int lY, const UCHAR *sel,
                         const int sens, const int border)
    : m_lX(lX), m_lY(lY), m_lDf(0) {
  for (int i = 0; i < NBDIR; i++) m_df[i] = 0;

  try {
    if (m_lX > 0 && m_lY > 0) {
      m_dir.reset(new UCHAR[m_lX * m_lY]);
      if (!m_dir) {
        null();
        throw SMemAllocError("in directionMap");
      }
      memcpy(m_dir.get(), sel, sizeof(UCHAR) * m_lX * m_lY);
      setDir01();
      if (border > 0) setContourBorder(border);
      makeDirFilter(sens);
    }
  } catch (SMemAllocError) {
    throw;
  }
}

bool CSDirection::isContourBorder(const int xx, const int yy,
                                  const int border) {
  for (int y = yy - border; y <= (yy + border); y++)
    for (int x = xx - border; x <= (xx + border); x++)
      if (x >= 0 && y >= 0 && x < m_lX && y < m_lY)
        if (*(m_dir.get() + y * m_lX + x) == (UCHAR)0) return true;
  return false;
}

void CSDirection::setContourBorder(const int border) {
  UCHAR *pDir = m_dir.get();
  int y       = 0;
  for (y = 0; y < m_lY; y++)
    for (int x = 0; x < m_lX; x++, pDir++)
      if (*pDir == (UCHAR)1)
        if (!isContourBorder(x, y, border)) *pDir = (UCHAR)2;

  int xy = m_lX * m_lY;
  pDir   = m_dir.get();
  for (y = 0; y < xy; y++, pDir++) *pDir = *pDir == (UCHAR)2 ? (UCHAR)0 : *pDir;
}

void CSDirection::null() {
  m_dir.reset();
  for (auto &&df : m_df) {
    df.reset();
  }
  m_lX = m_lY = 0;
  m_lDf       = 0;
}

CSDirection::~CSDirection() { null(); }

double CSDirection::adjustAngle(const short sum[4], const int Ima,
                                const int Im45, const int Ip45) {
  short ma = std::max(sum[Im45], sum[Ip45]);

  if (ma < 0) return 0.0;
  if ((double)ma < (double)sum[Ima] / 10.0) return 0.0;
  if (((double)abs(sum[Im45] - sum[Ip45]) / (double)ma) < 0.5) return 0.0;

  double d = 45.0 * (double)ma / (double)(sum[Ima] + ma);
  if (ma == sum[Im45]) return -d;
  return d;
}

double CSDirection::getAngle(const short sum[4], short ma) {
  int nbMax   = ma == sum[0] ? 1 : 0;
  nbMax       = ma == sum[1] ? nbMax + 1 : nbMax;
  nbMax       = ma == sum[2] ? nbMax + 1 : nbMax;
  nbMax       = ma == sum[3] ? nbMax + 1 : nbMax;
  double diff = 50.0;

  if (nbMax == 1) {
    double angle, corrAngle;
    if (ma == sum[0]) {
      angle                      = 0.0;
      corrAngle                  = adjustAngle(sum, 0, 3, 1);
      if (corrAngle < 0.0) angle = 180.0;
    } else if (ma == sum[1]) {
      angle     = 45.0;
      corrAngle = adjustAngle(sum, 1, 0, 2);
    } else if (ma == sum[2]) {
      angle     = 90.0;
      corrAngle = adjustAngle(sum, 2, 1, 3);
    } else {
      angle     = 135.0;
      corrAngle = adjustAngle(sum, 3, 2, 0);
    }
    return angle + corrAngle + diff;
  }
  ///* ?????????????????????? kiprobalni
  if (nbMax == 2) {
    if (ma == sum[0] && ma == sum[1]) return 22.5 + diff;
    if (ma == sum[0] && ma == sum[3]) return 157.5 + diff;
    if (ma == sum[1] && ma == sum[2]) return 67.5 + diff;
    if (ma == sum[2] && ma == sum[3]) return 112.5 + diff;
  }
  //*/
  return 1.0;
}

UCHAR CSDirection::getDir(const int xx, const int yy, UCHAR *sel) {
  short sum[4] = {0, 0, 0, 0};
  short w      = 0;

  for (int i = 0; i < m_lDf; i++) {
    int x = xx + m_df[0][i].x;
    int y = yy + m_df[0][i].y;
    if (y >= 0 && y < m_lY && x >= 0 && x < m_lX) {
      UCHAR *pSel = sel + y * m_lX + x;
      for (int j = 0; j < NBDIR; j++) sum[j] += (short)(*pSel) * m_df[j][i].w;
      w += (short)(*pSel);
    }
  }
  if (w == 0) return 0;
  short ma     = *std::max_element(sum, sum + 4);
  double angle = getAngle(sum, ma);
  // tmsg_info(" - dir - %d, %d, %d, %d angle=%f", sum[0],sum[1],sum[2],sum[3],
  //		                                          angle-50.0);
  return UC_ROUND(angle);
}

void CSDirection::makeDir(UCHAR *sel) {
  UCHAR *pSel = sel;
  UCHAR *pDir = m_dir.get();
  for (int y = 0; y < m_lY; y++)
    for (int x = 0; x < m_lX; x++, pSel++, pDir++) {
      *pDir                = 0;
      if (*pSel > 0) *pDir = getDir(x, y, sel);
    }
}

UCHAR CSDirection::equalizeDir_GTE50(UCHAR *sel, const int xx, const int yy,
                                     const int d) {
  int sum = 0;
  int w   = 0;
  int aa;

  aa = (int)*(sel + yy * m_lX + xx) - 50;
  for (int y = yy - d; y <= (yy + d); y++)
    for (int x = xx - d; x <= (xx + d); x++)
      if (x >= 0 && y >= 0 && x < m_lX && y < m_lY) {
        UCHAR *pSel = sel + y * m_lX + x;
        int a       = (int)*pSel;
        if (a >= 50) {
          a -= 50;
          if (aa < 90 && a > 135) {
            a = -(180 - a);
          } else if (aa > 90 && a < 45) {
            a = 180 + a;
          }
          sum += a;
          w++;
        }
      }
  if (w > 0) {
    double q = (double)sum / (double)w;
    int a    = I_ROUND(q);
    if (a >= 180) {
      a -= 180;
    } else if (a < 0) {
      a = 180 + a;
    }
    return (UCHAR)(a + 50);
  }
  return *(sel + yy * m_lX + xx);
}

UCHAR CSDirection::equalizeDir_LT50(UCHAR *sel, const int xx, const int yy,
                                    const int d) {
  int sum = 0;
  int w   = 0;

  for (int y = yy - d; y <= (yy + d); y++)
    for (int x = xx - d; x <= (xx + d); x++)
      if (x >= 0 && y >= 0 && x < m_lX && y < m_lY) {
        int a = (int)*(sel + y * m_lX + x);
        if (a >= 50) {
          a -= 50;
          sum += a;
          w++;
        }
      }
  if (w > 0) {
    double q = (double)sum / (double)w;
    int a    = I_ROUND(q);
    if (a >= 180) {
      a -= 180;
    } else if (a < 0) {
      a = 180 + a;
    }
    return (UCHAR)(a + 50);
  }
  return *(sel + yy * m_lX + xx);
}

void CSDirection::equalizeDir(UCHAR *sel, const int d) {
  UCHAR *pSel = sel;
  UCHAR *pDir = m_dir.get();
  for (int y = 0; y < m_lY; y++)
    for (int x = 0; x < m_lX; x++, pSel++) {
      if (*pSel > (UCHAR)0) {
        if (*pSel >= (UCHAR)50)
          *pDir = equalizeDir_GTE50(sel, x, y, d);
        else
          *pDir = equalizeDir_LT50(sel, x, y, d);
      } else
        *pDir = (UCHAR)0;
    }
}

static UCHAR getRadius(const double angle, const double r[4]) {
  double p, q;
  if (angle >= 0.0 && angle < 45.0) {
    q = angle / 45.0;
    p = r[0] * (1.0 - q) + r[1] * q;
  } else if (angle >= 45.0 && angle < 90.0) {
    q = (angle - 45.0) / 45.0;
    p = r[1] * (1.0 - q) + r[2] * q;
  } else if (angle >= 90.0 && angle < 135.0) {
    q = (angle - 90.0) / 45.0;
    p = r[2] * (1.0 - q) + r[3] * q;
  } else {
    q = (angle - 135.0) / 45.0;
    p = r[3] * (1.0 - q) + r[0] * q;
  }
  return (UCHAR)(1 + I_ROUND(p * 254.0));
}

void CSDirection::makeDirFilter(const int sens) {
  const int size   = 2 * sens + 1;
  const int middle = size / 2;
  const int maxVal = size - 1;

  m_lDf = size * size;
  for (int i = 0; i < NBDIR; i++) {
    m_df[i].reset(new SXYW[m_lDf]);
    if (!m_df[i]) {
      null();
      throw SMemAllocError("in directionMap");
    }
    for (int y = 0; y < size; y++)
      for (int x = 0; x < size; x++) {
        int xy        = y * size + x;
        m_df[i][xy].x = x - middle;
        m_df[i][xy].y = y - middle;
        switch (i) {
        case 0:
          // Horizontal
          m_df[i][xy].w = y == middle ? maxVal : -1;
          break;
        case 1:
          // Left - Right
          m_df[i][xy].w = x == y ? maxVal : -1;
          break;
        case 2:
          // Vertical
          m_df[i][xy].w = x == middle ? maxVal : -1;
          break;
        case 3:
          // Right - Left
          m_df[i][xy].w = (x + y) == (size - 1) ? maxVal : -1;
          break;
        }
      }
  }
}

UCHAR CSDirection::blurRadius(UCHAR *sel, const int xx, const int yy,
                              const int dBlur) {
  int sum, w;

  sum = w = 0;
  for (int y = yy - dBlur; y <= (yy + dBlur); y++)
    for (int x = xx - dBlur; x <= (xx + dBlur); x++)
      if (x >= 0 && y >= 0 && x < m_lX && y < m_lY) {
        UCHAR *pSel = sel + y * m_lX + x;
        if (*pSel > (UCHAR)0) {
          sum += (int)(*pSel);
          w++;
        }
      }
  if (w > 0) {
    double d = (double)sum / (double)w;
    d        = D_CUT_0_255(d);
    return UC_ROUND(d);
  }
  return *(sel + yy * m_lX + xx);
}

void CSDirection::blurRadius(const int dBlur) {
  if (m_lX > 0 && m_lY > 0 && m_dir) {
    std::unique_ptr<UCHAR[]> sel(new UCHAR[m_lX * m_lY]);
    if (!sel) throw SMemAllocError("in directionMap");
    memcpy(sel.get(), m_dir.get(), m_lX * m_lY * sizeof(UCHAR));
    UCHAR *pSel = sel.get();
    UCHAR *pDir = m_dir.get();
    for (int y = 0; y < m_lY; y++)
      for (int x                    = 0; x < m_lX; x++, pSel++, pDir++)
        if (*pSel > (UCHAR)0) *pDir = blurRadius(sel.get(), x, y, dBlur);
  }
}

void CSDirection::setDir01() {
  int xy      = m_lX * m_lY;
  UCHAR *pDir = m_dir.get();
  for (int i = 0; i < xy; i++, pDir++)
    *pDir = *pDir > (UCHAR)0 ? (UCHAR)1 : (UCHAR)0;
}

void CSDirection::doDir() {
  if (m_lX > 0 && m_lY > 0 && m_dir) {
    std::unique_ptr<UCHAR[]> sel(new UCHAR[m_lX * m_lY]);
    if (!sel) throw SMemAllocError("in directionMap");
    size_t length = (size_t)(m_lX * m_lY * sizeof(UCHAR));
    memcpy(sel.get(), m_dir.get(), length);
    makeDir(sel.get());
    memcpy(sel.get(), m_dir.get(), length);
    equalizeDir(sel.get(), 3);
  }
}

void CSDirection::doRadius(const double rH, const double rLR, const double rV,
                           const double rRL, const int dBlur) {
  try {
    int xy      = m_lX * m_lY;
    UCHAR *pDir = m_dir.get();
    double r[4] = {D_CUT_0_1(rH), D_CUT_0_1(rLR), D_CUT_0_1(rV),
                   D_CUT_0_1(rRL)};

    for (int i = 0; i < xy; i++, pDir++)
      if (*pDir < (UCHAR)50) {
        *pDir = (UCHAR)0;
      } else
        *pDir = getRadius((double)(*pDir - 50), r);

    if (dBlur > 0) blurRadius(dBlur);
  } catch (SMemAllocError) {
    throw;
  }
}

void CSDirection::getResult(UCHAR *sel) {
  memcpy(sel, m_dir.get(), (size_t)(m_lX * m_lY * sizeof(UCHAR)));
}
