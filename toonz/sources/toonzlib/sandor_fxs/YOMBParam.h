#pragma once

// YOMBParam.h: interface for the CYOMBParam class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_YOMBPARAM_H__41D42152_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_)
#define AFX_YOMBPARAM_H__41D42152_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_

#include <vector>
#include "STColSelPic.h"
#include "SDef.h"
#include "InputParam.h"
#include "YOMBInputParam.h"
#include "BlurMatrix.h"
//#pragma warning(disable: 4786)
//#include "tmsg.h"

#define P(d) tmsg_info(" - %d -\n", d)

class CYOMBParam {
public:
  bool m_isRandomSampling;
  bool m_isShowSelection;
  bool m_isStopAtContour;
  bool m_isBlurOnPair;
  double m_dSample;
  int m_nbSample;
  double m_dA, m_dAB;
  std::string m_name;

  bool m_isCM;
  // for RGBM color groups
  std::vector<I_PIXEL> m_color;
  // for CMAP color indides
  COLOR_INDEX_LIST m_ink, m_paint;

  CYOMBParam()
      : m_isRandomSampling(false)
      , m_isShowSelection(false)
      , m_isStopAtContour(false)
      , m_isBlurOnPair(false)
      , m_dSample(0.0)
      , m_nbSample(0)
      , m_dA(0.0)
      , m_dAB(0.0)
      , m_name("")
      , m_isCM(false)
      , m_color(0) {
    m_ink.nb = m_paint.nb = 0;
  };

  CYOMBParam(const CYOMBParam &bp)
      : m_color(bp.m_color)
      , m_isRandomSampling(bp.m_isRandomSampling)
      , m_isShowSelection(bp.m_isShowSelection)
      , m_isStopAtContour(bp.m_isStopAtContour)
      , m_isBlurOnPair(bp.m_isBlurOnPair)
      , m_dSample(bp.m_dSample)
      , m_nbSample(bp.m_nbSample)
      , m_dA(bp.m_dA)
      , m_dAB(bp.m_dAB)
      , m_name(bp.m_name)
      , m_isCM(bp.m_isCM) {
    if (m_isCM) {
      m_ink.nb = bp.m_ink.nb;
      int i    = 0;
      for (i = 0; i < m_ink.nb; i++) m_ink.ci[i] = bp.m_ink.ci[i];
      m_paint.nb                                 = bp.m_paint.nb;
      for (i = 0; i < m_paint.nb; i++) m_paint.ci[i] = bp.m_paint.ci[i];
    }
  };

  virtual ~CYOMBParam(){};

  void print();
  void null();
  void read(const CInputParam &ip);
#ifdef _WIN32
  bool read(std::basic_ifstream<char> &in);
#endif
  void makeColorsUS();
  void makeItUS();
  static void adjustToMatte(I_PIXEL &p);

  //	void blurPixel(CSTColSelPic<UC_PIXEL>& pic, const int xx, const int yy,
  //				   const CBlurMatrix& bm, UC_PIXEL& col,const
  // UCHAR
  // osel);
  //	bool isContourOnPath(const int xx, const int yy,
  //						 std::vector<BLURSECTION>::const_iterator
  // pBS,
  //						 CSTColSelPic<UC_PIXEL>& pic);
  //	void addPixel(I_PIXEL& p, const UC_PIXEL* pic);
  //	void addPixel(I_PIXEL& p, const CSTColSelPic<UC_PIXEL>& pic,
  //                 const int xxyy, const int xy );
  void addPixel(I_PIXEL &p, const I_PIXEL &pp);
  int getColorIndex(const UCHAR c);
  void scale(const double d);

  template <class P>
  void addPixel(I_PIXEL &p, const P *pic) {
    p.r += ((int)pic->r);
    p.g += ((int)pic->g);
    p.b += ((int)pic->b);
    p.m += ((int)pic->m);
  }

  // Checks the path from blurred pixel to the sample pixel.
  template <class P>
  bool isContourOnPath(const int xx, const int yy,
                       std::vector<BLURSECTION>::const_iterator pBS,
                       CSTColSelPic<P> &pic) {
    for (std::vector<SXYD>::const_iterator p = pBS->begin(); p != pBS->end();
         ++p) {
      int x  = xx + p->x;
      int y  = yy + p->y;
      int xy = y * pic.m_lX + x;
      if (x >= 0 && y >= 0 && x < pic.m_lX && y < pic.m_lY)
        if (*(pic.m_sel + xy) == (UCHAR)0) return true;
    }
    return false;
  }

  template <class P>
  void addPixel(I_PIXEL &ip, const CSTColSelPic<P> &pic, const int xxyy,
                const int xy) {
    if (!m_isBlurOnPair) {
      addPixel(ip, pic.m_pic + xy);
    } else {
      int i  = getColorIndex(*(pic.m_sel + xy));
      int ii = getColorIndex(*(pic.m_sel + xxyy));
      int l  = m_color.size() / 2;

      if (i == ii || abs(ii - i) == l) {
        addPixel(ip, pic.m_pic + xy);
      } else {
        if (ii < l) {
          if (i < l) {
            addPixel(ip, m_color[ii]);
          } else
            addPixel(ip, m_color[ii + l]);
        } else {
          if (i < l) {
            addPixel(ip, m_color[ii - l]);
          } else
            addPixel(ip, m_color[ii]);
        }
      }
    }
  }

  template <class P>
  void blurPixel(CSTColSelPic<P> &pic, const int xx, const int yy,
                 const CBlurMatrix &bm, I_PIXEL &col, const int iBm) {
    I_PIXEL p = {0, 0, 0, 0};
    int nb    = 0;

    int xxyy = yy * pic.m_lX + xx;
    for (std::vector<BLURSECTION>::const_iterator pBS = bm.m_m[iBm].begin();
         pBS != bm.m_m[iBm].end(); ++pBS) {
      //		const SXYD* xyd=pBS->begin();
      BLURSECTION::const_iterator xyd = pBS->begin();

      int x = xx + xyd->x;
      int y = yy + xyd->y;
      if (x >= 0 && y >= 0 && x < pic.m_lX && y < pic.m_lY) {
        int xy = y * pic.m_lX + x;
        if (*(pic.m_sel + xy) > (UCHAR)0) {
          if (bm.m_isSAC) {
            if (!isContourOnPath(xx, yy, pBS, pic)) {
              addPixel(p, pic, xxyy, xy);
              nb++;
            }
          } else {
            addPixel(p, pic, xxyy, xy);
            nb++;
          }
	}
      }
    }

    if (nb > 0) {
      double r = (double)p.r / (double)nb;
      double g = (double)p.g / (double)nb;
      double b = (double)p.b / (double)nb;
      double m = (double)p.m / (double)nb;
      col.r    = I_ROUND(r);
      col.g    = I_ROUND(g);
      col.b    = I_ROUND(b);
      col.m    = I_ROUND(m);
    } else {
      P *pPic = pic.m_pic + yy * pic.m_lX + xx;
      col.r   = (int)(pPic->r);
      col.g   = (int)(pPic->g);
      col.b   = (int)(pPic->b);
      col.m   = (int)(pPic->m);
    }
  }

  template <class P>
  bool isRealMixture(CSTColSelPic<P> &pic, const int xx, const int yy,
                     const CBlurMatrix &bm, const int iBm,
                     const UCHAR osel) const {
    for (std::vector<BLURSECTION>::const_iterator pBS = bm.m_m[iBm].begin();
         pBS != bm.m_m[iBm].end(); ++pBS) {
      //		const SXYD* xyd= pBS->begin();
      BLURSECTION::const_iterator xyd = pBS->begin();
      int x                           = xx + xyd->x;
      int y                           = yy + xyd->y;
      if (x >= 0 && x < pic.m_lX && y >= 0 && y < pic.m_lY) {
        int xy      = y * pic.m_lX + x;
        UCHAR *pSel = pic.m_sel + xy;
        if (*pSel > (UCHAR)0 && *pSel != osel) return true;
      }
    }
    return false;
  }

  template <class P>
  void doIt(CSTColSelPic<P> &ipic, CSTColSelPic<P> &opic) {
    CBlurMatrix bm(m_dSample, m_nbSample, m_isStopAtContour,
                   m_isRandomSampling);
    UCHAR *pSel = ipic.m_sel;
    for (int y = 0; y < ipic.m_lY; y++)
      for (int x = 0; x < ipic.m_lX; x++, pSel++)
        if (*pSel > (UCHAR)0) {
          I_PIXEL col;
          int iBm = bm.m_isRS ? rand() % NBRS : 0;
          if (isRealMixture(ipic, x, y, bm, iBm, *pSel))
            blurPixel(ipic, x, y, bm, col, iBm);
          else
            ipic.getPixel(x, y, col);
          opic.setRGBM(x, y, col);
        }
  }
};

#endif  // !defined(AFX_YOMBPARAM_H__41D42152_F2EE_11D5_B92D_0040F674BE6A__INCLUDED_)
