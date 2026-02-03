

// EraseContour.cpp: implementation of the CEraseContour class.
//
//////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <search.h>
#include <math.h>

//#include "myetc.h"
#include "EraseContour.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEraseContour::~CEraseContour() { null(); }

void CEraseContour::null() {
  m_lX = m_lY = 0;
  m_picUC     = 0;
  m_picUS     = 0;
  m_sel.reset();
  m_ras      = 0;
  m_cil.m_nb = 0;
}

int CEraseContour::makeSelectionCMAP32() {
  UCHAR *pSel = m_sel.get();
  int xy = 0, nbSel = 0;

  for (int y = 0; y < m_lY; y++)
    for (int x = 0; x < m_lX; x++, xy++, pSel++) {
      int xyRas             = y * m_ras->wrap + x;
      UD44_CMAPINDEX32 ci32 = *((UD44_CMAPINDEX32 *)(m_ras->buffer) + xyRas);
      if ((ci32 & 0x000000ff) == 0xff) {
        // Paint color, if transparent 0
        *pSel = (UCHAR)3;
      } else if ((ci32 & 0x000000ff) == 0x0) {
        // Ink color
        if (m_cil.isIn((int)((ci32 >> 20) & 0x00000fff))) {
          *pSel = (UCHAR)1;
          nbSel++;
        }
      } else {
        // Tone color ( antialiasing pixel )
        if (m_cil.isIn((int)((ci32 >> 20) & 0x00000fff))) {
          *pSel = (UCHAR)1;
          nbSel++;
          //					*pSel=(UCHAR)2;
        }
      }
    }

  return nbSel;
}

//  Makes the selection of ink and antialiasing pixels.
//	0 - 0 matte (transparent) OR ink color which is not in m_cil
//  1 - ink pixel, which is in the m_cil
//  1 - antialiasing pixel
//  3 - paint color
int CEraseContour::makeSelection(const CCIL &iil) {
  int nb = 0;
  m_cil  = iil;
  if (m_cil.m_nb <= 0) return 0;
  if (m_lX <= 0 || m_lY <= 0 || !m_sel || !m_ras || !(m_picUC || m_picUS))
    return 0;
  memset(m_sel.get(), 0, m_lX * m_lY);
  if (m_ras->type == RAS_CM32) nb = makeSelectionCMAP32();
  if (nb > 0) sel0123To01();
  return nb;
}

//  Makes the selection of ink and antialiasing pixels.
//  m_cil has been prepared!!
//  0 - 0 matte (transparent) OR ink color which is not in m_cil
//  1 - ink pixel, which is in the m_cil
//  1 - antialiasing pixel
//  3 - paint color
int CEraseContour::makeSelection() {
  memset(m_sel.get(), 0, m_lX * m_lY);
  if (m_ras->type == RAS_CM32) return makeSelectionCMAP32();
  return 0;
}

int CEraseContour::doIt(const CCIL &iil) {
  int nb = 0;

  m_cil = iil;
  if (m_cil.m_nb <= 0) return 0;
  if (m_lX <= 0 || m_lY <= 0 || !m_sel || !m_ras || !(m_picUC || m_picUS))
    return 0;
  nb = makeSelection();
  if (nb > 0) {
    eraseInkColors();
    sel0123To01();
  }
  return nb;
}

void CEraseContour::sel0123To01() {
  int xy      = m_lX * m_lY;
  UCHAR *pSel = m_sel.get();
  for (int i = 0; i < xy; i++, pSel++)
    *pSel = *(pSel) == (UCHAR)1 ? (UCHAR)1 : (UCHAR)0;
}

void CEraseContour::eraseInkColors() {
  UCHAR *pSel = m_sel.get();
  prepareNeighbours();
  for (int y = 0; y < m_lY; y++)
    for (int x = 0; x < m_lX; x++, pSel++)
      if (*pSel == (UCHAR)1 || *pSel == (UCHAR)2) {
        I_PIXEL ip = {0, 0, 0, 0};
        if (findClosestPaint(x, y, ip)) {
          if (m_picUC) {
            UC_PIXEL *uc = m_picUC + y * m_lX + x;
            uc->r        = (UCHAR)ip.r;
            uc->g        = (UCHAR)ip.g;
            uc->b        = (UCHAR)ip.b;
            uc->m        = (UCHAR)ip.m;
          } else {
            US_PIXEL *us = m_picUS + y * m_lX + x;
            us->r        = (USHORT)ip.r;
            us->g        = (USHORT)ip.g;
            us->b        = (USHORT)ip.b;
            us->m        = (USHORT)ip.m;
          }
        }
      }
}

static int erasecontour_xydwCompare(const void *a, const void *b) {
  SXYDW *aa = (SXYDW *)a;
  SXYDW *bb = (SXYDW *)b;

  if (aa->w < bb->w) return -1;
  if (aa->w > bb->w) return 1;
  return 0;
}

void CEraseContour::prepareNeighbours() {
  int q          = ((int)sqrt((double)MAXNB_NEIGHBOURS) - 1) / 2;
  m_nbNeighbours = 0;
  for (int y = -q; y <= q; y++)
    for (int x = -q; x <= q; x++) {
      m_neighbours[m_nbNeighbours].x = x;
      m_neighbours[m_nbNeighbours].y = y;
      m_neighbours[m_nbNeighbours].w = sqrt((double)(x * x + y * y));
      m_nbNeighbours++;
    }
  qsort(m_neighbours.data(), m_nbNeighbours, sizeof(SXYDW),
        erasecontour_xydwCompare);
}

bool CEraseContour::findClosestPaint(const int xx, const int yy, I_PIXEL &ip) {
  for (int i = 0; i < m_nbNeighbours; i++) {
    int x = xx + m_neighbours[i].x;
    int y = yy + m_neighbours[i].y;
    if (x >= 0 && y >= 0 && x < m_lX && y < m_lY)
      if (*(m_sel.get() + y * m_lX + x) == (UCHAR)3) {
        if (m_picUC) {
          UC_PIXEL *uc = m_picUC + y * m_lX + x;
          ip.r         = (int)uc->r;
          ip.g         = (int)uc->g;
          ip.b         = (int)uc->b;
          ip.m         = (int)uc->m;
        } else {
          US_PIXEL *us = m_picUS + y * m_lX + x;
          ip.r         = (int)us->r;
          ip.g         = (int)us->g;
          ip.b         = (int)us->b;
          ip.m         = (int)us->m;
        }
        return true;
      }
  }
  ip.r = ip.g = ip.b = ip.m = 0;
  return false;
}
