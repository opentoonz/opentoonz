#pragma once

// CallCircle.h: interface for the CCallCircle class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CALLCIRCLE_H__D2565814_2151_11D6_B9B8_0040F674BE6A__INCLUDED_)
#define AFX_CALLCIRCLE_H__D2565814_2151_11D6_B9B8_0040F674BE6A__INCLUDED_

#include <memory.h>
#include <string.h>
#include "SDef.h"
#include "STColSelPic.h"

int xydwCompare(const void *a, const void *b);

class CCallCircle {
  double m_r;
  int m_nb;
  std::unique_ptr<SXYDW[]> m_c;

  void draw(UCHAR *drawB, const int lX, const int lY, const int xx,
            const int yy, const double r);
  void null();

public:
  CCallCircle() : m_r(0.0), m_nb(0) {}
  CCallCircle(const double r);
  virtual ~CCallCircle();
  void print();

  template <class P>
  void getCC(CSTColSelPic<P> &pic, P &col) {
    int xy      = pic.m_lX * pic.m_lY;
    UCHAR *pSel = pic.m_sel.get();
    for (int i = 0; i < xy; i++, pSel++)
      if (*pSel > (UCHAR)0) {
        P *pPic = pic.m_pic + i;
        col.r   = pPic->r;
        col.g   = pPic->g;
        col.b   = pPic->b;
        col.m   = pPic->m;
        return;
      }
  }

  template <class P>
  void getCC(CSTColSelPic<P> &pic, const int xx, const int yy, P &col) {
    for (int i = 0; i < m_nb; i++) {
      int x = xx + m_c[i].x;
      int y = yy + m_c[i].y;
      if (x >= 0 && y >= 0 && x < pic.m_lX && y < pic.m_lY) {
        UCHAR *pSel = pic.m_sel.get() + y * pic.m_lX + x;
        if (*pSel > (UCHAR)0) {
          P *pPic = pic.m_pic + y * pic.m_lX + x;
          col.r   = pPic->r;
          col.g   = pPic->g;
          col.b   = pPic->b;
          col.m   = pPic->m;
          return;
        }
      }
    }
  }

  template <class P>
  void setNewContour(CSTColSelPic<P> &picOri, CSTColSelPic<P> &pic,
                     UCHAR *drawB, const bool isOneCC) {
    UCHAR *pDB      = drawB;
    P *p            = pic.m_pic;
    P *pOri         = picOri.m_pic;
    int xy          = pic.m_lX * pic.m_lY;
    P col           = {0, 255, 0, 255};
    int rgbmMax     = pic.getType() == ST_RGBM64 ? US_MAX : UC_MAX;
    double rgbmMaxD = (double)rgbmMax;

    if (isOneCC) getCC(picOri, col);
    for (int i = 0; i < xy; i++, pDB++, p++, pOri++) {
      if (*pDB == 255) {
        if (!isOneCC) getCC(picOri, i % pic.m_lX, i / pic.m_lX, col);
        p->r = col.r;
        p->g = col.g;
        p->b = col.b;
        p->m = col.m;
      } else if (*pDB > 0) {
        if (!isOneCC) getCC(picOri, i % pic.m_lX, i / pic.m_lX, col);
        double q  = ((double)(*pDB) / 255.0) * (double)col.m / (double)rgbmMax;
        double qq = 1.0 - q;
        double r  = q * (double)col.r + qq * (double)(pOri->r);
        double g  = q * (double)col.g + qq * (double)(pOri->g);
        double b  = q * (double)col.b + qq * (double)(pOri->b);
        double m  = q * (double)col.m + qq * (double)(pOri->m);
        r         = D_CUT(r, 0.0, rgbmMaxD);
        g         = D_CUT(g, 0.0, rgbmMaxD);
        b         = D_CUT(b, 0.0, rgbmMaxD);
        m         = D_CUT(m, 0.0, rgbmMaxD);
        if (rgbmMax == 255) {
          p->r = UC_ROUND(r);
          p->g = UC_ROUND(g);
          p->b = UC_ROUND(b);
          p->m = UC_ROUND(m);
        } else {
          p->r = (UCHAR)US_ROUND(r);
          p->g = (UCHAR)US_ROUND(g);
          p->b = (UCHAR)US_ROUND(b);
          p->m = (UCHAR)US_ROUND(m);
        }
      }
    }
  }

  template <class P>
  void draw(CSTColSelPic<P> &pic, const bool isOneCC, const double random)
  // throw(SMemAllocError)
  {
    if (m_nb <= 0 || m_c == 0) return;

    try {
      CSTColSelPic<P> picOri;
      picOri = pic;
      if (pic.m_lX > 0 && pic.m_lY > 0) {
        std::unique_ptr<UCHAR[]> drawB(new UCHAR[pic.m_lX * pic.m_lY]);
        if (!drawB) throw SMemAllocError("in callCircle");
        memset(drawB.get(), 0, pic.m_lX * pic.m_lY);
        UCHAR *pSel = pic.m_sel.get();
        for (int y = 0; y < pic.m_lY; y++)
          for (int x = 0; x < pic.m_lX; x++, pSel++)
            if (*pSel > (UCHAR)0) {
              double q  = m_r * (double)((*pSel) - 1) / 254.0;
              int rani  = I_ROUND(random);
              int ranii = rani > 0 ? rand() % (2 * rani) - 15 * rani / 8 : 0;
              q         = q * (1.0 + (double)ranii / 100.0);
              draw(drawB.get(), pic.m_lX, pic.m_lY, x, y, q);
            }
        setNewContour(picOri, pic, drawB.get(), isOneCC);
      }
    } catch (SMemAllocError) {
      throw;
    }
  }
};

#endif  // !defined(AFX_CALLCIRCLE_H__D2565814_2151_11D6_B9B8_0040F674BE6A__INCLUDED_)
