

// PatternPosition.cpp: implementation of the CPatternPosition class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <memory>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "PatternPosition.h"
#include "SError.h"
#include "SDef.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

using namespace std;

CPatternPosition::~CPatternPosition() { m_pos.clear(); }

bool CPatternPosition::isInSet(const int nbSet, const int *set, const int val) {
  for (int i = 0; i < nbSet; i++)
    if (set[i] == val) return true;
  return false;
}

static int pp_intCompare(const void *a, const void *b) {
  int *aa = (int *)a;
  int *bb = (int *)b;

  if (*aa < *bb) return -1;
  if (*aa > *bb) return 1;
  return 0;
}

void CPatternPosition::makeRandomPositions(const int nbPat, const int nbPixel,
                                           const int lX, const int lY,
                                           const UCHAR *sel) {
  try {
    const UCHAR *pSel = sel;
    int threshold     = RAND_MAX * (double)nbPat / (double)nbPixel;
    for (int y = 0; y < lY; y++)
      for (int x = 0; x < lX; x++, pSel++)
        if (*pSel > (UCHAR)0) {
          if (rand() < threshold) {
            SPOINT xyp = {x, y};
            m_pos.push_back(xyp);
          }
        }
  } catch (exception) {
    char s[50];
    snprintf(s, sizeof(s), "in Pattern Position Generation");
    throw SMemAllocError(s);
  }
}

void CPatternPosition::getPosAroundThis(const int lX, const int lY,
                                        const UCHAR *lSel, const int xx,
                                        const int yy, int &xPos, int &yPos) {
  vector<SPOINT> ddc;
  prepareCircle(ddc, 2.0);

  int qx = 0, qy = 0, q = 0;
  for (vector<SPOINT>::iterator p = ddc.begin(); p != ddc.end(); p++) {
    int x = xx + p->x;
    int y = yy + p->y;
    if (x >= 0 && y >= 0 && x < lX && y < lY)
      if (*(lSel + y * lX + x) > (UCHAR)0) {
        qx += x;
        qy += y;
        q++;
      }
  }
  if (q > 0) {
    double dx = (double)qx / (double)q;
    double dy = (double)qy / (double)q;
    xPos      = I_ROUND(dx);
    yPos      = I_ROUND(dy);
  } else {
    xPos = xx;
    yPos = yy;
  }
}

bool CPatternPosition::findEmptyPos(const int lX, const int lY,
                                    const UCHAR *lSel, int &xPos, int &yPos,
                                    SRECT &bb) {
  int y = 0;
  for (y = 0; y <= yPos; y++)
    for (int x = xPos; x <= bb.x1; x++)
      if (*(lSel + y * lX + x) == (UCHAR)1) {
        //				getPosAroundThis(lX,lY,lSel,x,y,xPos,yPos);
        xPos = x;
        yPos = y;
        return true;
      }

  for (y = yPos; y <= bb.y1; y++)
    for (int x = bb.x0; x <= bb.x1; x++)
      if (*(lSel + y * lX + x) == (UCHAR)1) {
        //				getPosAroundThis(lX,lY,lSel,x,y,xPos,yPos);
        xPos = x;
        yPos = y;
        return true;
      }
  return false;
}

void CPatternPosition::eraseCurrentArea(const int lX, const int lY, UCHAR *lSel,
                                        vector<SPOINT> &ddc, const int xx,
                                        const int yy) {
  for (vector<SPOINT>::iterator pDdc = ddc.begin(); pDdc != ddc.end(); pDdc++) {
    int x = xx + pDdc->x;
    int y = yy + pDdc->y;
    if (x >= 0 && y >= 0 && x < lX && y < lY) {
      UCHAR *pSel                      = lSel + y * lX + x;
      if (*(pSel) == (UCHAR)1) *(pSel) = (UCHAR)2;
    }
  }
}

void CPatternPosition::sel0255To01(const int lX, const int lY, UCHAR *sel,
                                   SRECT &bb) {
  UCHAR *pSel = sel;
  bb.x0       = lX;
  bb.y0       = lY;
  bb.x1       = -1;
  bb.y1       = -1;
  for (int y = 0; y < lY; y++)
    for (int x = 0; x < lX; x++, pSel++)
      if (*pSel >= (UCHAR)1) {
        *pSel = (UCHAR)1;
        bb.x0 = std::min(x, bb.x0);
        bb.x1 = std::max(x, bb.x1);
        bb.y0 = std::min(y, bb.y0);
        bb.y1 = std::max(y, bb.y1);
      }
}

void CPatternPosition::prepareCircle(vector<SPOINT> &v, const double r) {
  try {
    double r2 = r * r;
    int rr    = (int)r + 1;
    for (int y = -rr; y <= rr; y++)
      for (int x = -rr; x <= rr; x++)
        if ((double)(x * x + y * y) <= r2) {
          SPOINT sp = {x, y};
          v.push_back(sp);
        }
  } catch (exception) {
    char s[50];
    snprintf(s, sizeof(s), "Position Generation");
    throw SMemAllocError(s);
  }
}

void CPatternPosition::makeDDPositions(const int lX, const int lY, UCHAR *sel,
                                       const double minD, const double maxD) {
  const int maxNbDDC = 20;
  vector<SPOINT> ddc[maxNbDDC];

  // Checking parameters
  if (lX <= 0 || lY <= 0 || !sel) return;
  if (minD > maxD) return;
  // Preparing circles
  int nbDDC = fabs(maxD - minD) < 0.001 ? 1 : maxNbDDC;
  try {
    if (nbDDC == 1) {
      prepareCircle(ddc[0], minD);
    } else {
      double dDist = (maxD - minD) / (double)(nbDDC - 1);
      double dist  = minD;
      for (int i = 0; i < nbDDC; i++, dist += dDist)
        prepareCircle(ddc[i], dist);
    }
  } catch (SMemAllocError) {
    throw;
  }
  // Preparing local selection
  std::unique_ptr<UCHAR[]> lSel(new UCHAR[lX * lY]);
  if (!lSel) {
    char s[50];
    snprintf(s, sizeof(s), "in Pattern Position Generation");
    throw SMemAllocError(s);
  }
  memcpy(lSel.get(), sel, lX * lY * sizeof(UCHAR));

  SRECT bb;
  sel0255To01(lX, lY, lSel.get(), bb);
  if (bb.x0 > bb.x1 || bb.y0 > bb.y1) {
    return;
  }

  try {
    int x = 0, y = 0;
    while (findEmptyPos(lX, lY, lSel.get(), x, y, bb)) {
      SPOINT sp = {x, y};
      m_pos.push_back(sp);
      int iddc = nbDDC == 1 ? 0 : rand() % nbDDC;
      eraseCurrentArea(lX, lY, lSel.get(), ddc[iddc], sp.x, sp.y);
    }
  } catch (exception) {
    char s[50];
    snprintf(s, sizeof(s), "in Pattern Position Generation");
    throw SMemAllocError(s);
  }
  //	memcpy(sel,lSel,lX*lY);
}
