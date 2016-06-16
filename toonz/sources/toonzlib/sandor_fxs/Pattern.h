#pragma once

// Pattern.h: interface for the CPattern class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PATTERN_H__8E417023_35C0_11D6_B9EA_0040F674BE6A__INCLUDED_)
#define AFX_PATTERN_H__8E417023_35C0_11D6_B9EA_0040F674BE6A__INCLUDED_

#include <memory>
#include "STColSelPic.h"
#include "SDef.h"
#include "toonz4.6/pixel.h"

class CPattern {
  int m_lX, m_lY;
  std::unique_ptr<UC_PIXEL[]> m_pat;
  char m_fn[1024];

  void null();
  bool readPattern(RASTER *imgContour);
  bool isOK() {
    if (m_lX > 0 && m_lY > 0 && m_pat) return true;
    return false;
  }
  void getMapPixel(const int xx, const int yy, const double invScale,
                   const double si, const double co, UC_PIXEL *&pucp);
  void getMapPixel(const int xx, const int yy, const double invScale,
                   UC_PIXEL *&pucp);
  bool readTTT(const char *fn);
  void getBBox(SRECT &bb);
  void eraseBuffer(const int lX, const int lY, UC_PIXEL *buffer);

public:
  CPattern() : m_lX(0), m_lY(0) { m_fn[0] = '\0'; };
  CPattern(RASTER *imgContour);
  virtual ~CPattern();
  void rotate(const double angle);
  void optimalizeSize();

  template <class P>
  void test(CSTColSelPic<P> &pic, I_PIXEL &eCol, int ox, int oy) {
    for (int y = 0; y < m_lY; y++)
      for (int x = 0; x < m_lX; x++)
        if ((x + ox) < pic.m_lX && (y + oy) < pic.m_lY) {
          P *p         = pic.m_pic + (y + oy) * pic.m_lX + x + ox;
          UC_PIXEL *pP = m_pat.get() + y * m_lX + x;

          if (pP->m > (UCHAR)0) {
            double q = ((double)(pP->m) / 255.0) * ((double)eCol.m / 255.0);
            double r = (1.0 - q) * (double)p->r + q * (double)eCol.r;
            double g = (1.0 - q) * (double)p->g + q * (double)eCol.g;
            double b = (1.0 - q) * (double)p->b + q * (double)eCol.b;

            r = D_CUT_0_255(r);
            g = D_CUT_0_255(g);
            b = D_CUT_0_255(b);

            p->r = UC_ROUND(r);
            p->g = UC_ROUND(g);
            p->b = UC_ROUND(b);
            p->m = pP->m;
          }
        }
  }

  // Maps the pattern. Long and optimalized version.
  template <class P>
  void mapIt(CSTColSelPic<P> &pic, const CSTColSelPic<P> &oriPic, const int xx,
             const int yy, const double scale, const double rot,
             const bool isUseOriColor, const bool isIncludeAlpha) {
    if (scale < 0.01) return;
    if (xx < 0 || yy < 0 || xx >= pic.m_lX || yy >= pic.m_lY) return;

    double sDiag = scale * sqrt(double(m_lX * m_lX + m_lY * m_lY));
    int iSDiag   = (int)sDiag + 1;
    int iSDiag2  = iSDiag / 2 + 1;
    if (iSDiag <= 0) return;

    double invScale  = 1.0 / scale;
    double co        = cos(-DEG2RAD(rot));
    double si        = sin(-DEG2RAD(rot));
    bool isUC        = pic.getType() == ST_RGBM ? true : false;
    double maxPixVal = isUC ? 255.0 : 65535.0;
    double ddiv      = 1.0 / (maxPixVal * 255.0);
    int yBeg         = std::max(yy - iSDiag2, 0);
    int yEnd         = std::min(yy + iSDiag2, pic.m_lY - 1);
    int xBeg         = std::max(xx - iSDiag2, 0);
    int xEnd         = std::min(xx + iSDiag2, pic.m_lX - 1);
    double lxm105    = (double)(m_lX - 1) * 0.5;
    double lym105    = (double)(m_lY - 1) * 0.5;

    I_PIXEL eCol;
    memset(&eCol, 0, sizeof(I_PIXEL));
    if (isUseOriColor) {
      P *pOriPic = oriPic.m_pic + yy * oriPic.m_lX + xx;
      eCol.r     = (int)pOriPic->r;
      eCol.g     = (int)pOriPic->g;
      eCol.b     = (int)pOriPic->b;
      if (isIncludeAlpha)
        eCol.m = (int)pOriPic->m;
      else {
        if ((int)pOriPic->m != 255)
          eCol.m = 0;
        else
          eCol.m = 255;
      }
    }
    int oriMatte = (int)((oriPic.m_pic + yy * oriPic.m_lX + xx)->m);
    for (int y = yBeg; y <= yEnd; y++)
      for (int x = xBeg; x <= xEnd; x++)
        if (x >= 0 && x < pic.m_lX && y >= 0 && y < pic.m_lY) {
          // ----- Calculates the pattern pixel coordinates -----
          UC_PIXEL *pucp = 0;
          //					getMapPixel(x-xx,y-yy,invScale,si,co,pucp);
          double dxx  = (double)(x - xx) * invScale;
          double dyy  = (double)(y - yy) * invScale;
          double d2xx = (dxx * co - dyy * si) + lxm105;
          double d2yy = (dxx * si + dyy * co) + lym105;
          int x1      = I_ROUND(d2xx);
          int y1      = I_ROUND(d2yy);
          if (x1 >= 0 && x1 < m_lX && y1 >= 0 && y1 < m_lY) {
            pucp = m_pat.get() + y1 * m_lX + x1;
            pucp = (pucp->m) == (UCHAR)0 ? 0 : pucp;
          }
          // ----------------------------------------------------
          if (pucp) {
            int xy  = y * pic.m_lX + x;
            P *pPic = pic.m_pic + xy;
            if (!isUseOriColor) {
              eCol.r = (int)pucp->r;
              eCol.g = (int)pucp->g;
              eCol.b = (int)pucp->b;
              if (isIncludeAlpha)
                eCol.m = oriMatte;
              else {
                if (oriMatte != 255)
                  eCol.m = 0;
                else
                  eCol.m = 255;
              }

              if (!isUC) {
                eCol.r = PIX_USHORT_FROM_BYTE((UCHAR)eCol.r);
                eCol.g = PIX_USHORT_FROM_BYTE((UCHAR)eCol.g);
                eCol.b = PIX_USHORT_FROM_BYTE((UCHAR)eCol.b);
              }
            }
            double q  = ((double)pucp->m) * ((double)eCol.m) * ddiv;
            double qq = 1.0 - q;
            double r  = qq * (double)pPic->r + q * (double)eCol.r;
            double g  = qq * (double)pPic->g + q * (double)eCol.g;
            double b  = qq * (double)pPic->b + q * (double)eCol.b;
            double m  = qq * (double)pPic->m + q * (double)eCol.m;
            r         = D_CUT(r, 0.0, maxPixVal);
            g         = D_CUT(g, 0.0, maxPixVal);
            b         = D_CUT(b, 0.0, maxPixVal);
            m         = D_CUT(m, 0.0, maxPixVal);
            if (isUC) {
              pPic->r = UC_ROUND(r);
              pPic->g = UC_ROUND(g);
              pPic->b = UC_ROUND(b);
              pPic->m = UC_ROUND(m);
            } else {
              pPic->r = (UCHAR)US_ROUND(r);
              pPic->g = (UCHAR)US_ROUND(g);
              pPic->b = (UCHAR)US_ROUND(b);
              pPic->m = (UCHAR)US_ROUND(m);
            }
          }
        }
  }
};

#endif  // !defined(AFX_PATTERN_H__8E417023_35C0_11D6_B9EA_0040F674BE6A__INCLUDED_)
