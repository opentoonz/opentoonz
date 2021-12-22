#pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

// STColSelPic.h: interface for the CSTColSelPic class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STCOLSELPIC_H__80D708B0_FCA2_11D5_B949_0040F674BE6A__INCLUDED_)
#define AFX_STCOLSELPIC_H__80D708B0_FCA2_11D5_B949_0040F674BE6A__INCLUDED_

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
#include <streambuf>
#include <vector>
#include <memory>

#include <math.h>
#include <memory.h>
#include "YOMBInputParam.h"
#include "STPic.h"
#include "SDef.h"
#include "CIL.h"
#include "SError.h"

template <class P>
class CSTColSelPic final : public CSTPic<P> {
public:
  std::shared_ptr<UCHAR> m_sel;

  CSTColSelPic() : CSTPic<P>() {}
  virtual ~CSTColSelPic(){};

  void nullSel() { m_sel.reset(); }

  void initSel()  // throw(SMemAllocError)
  {
    nullSel();
    if (CSTPic<P>::m_lX > 0 && CSTPic<P>::m_lY > 0) {
      m_sel.reset(new UCHAR[CSTPic<P>::m_lX * CSTPic<P>::m_lY],
                  std::default_delete<UCHAR[]>());
      if (!m_sel) throw SMemAllocError(" in initColorSelection");
    } else {
      char s[200];
      snprintf(s, sizeof(s), " in initColorSelection lXY=(%d,%d)\n",
               CSTPic<P>::m_lX, CSTPic<P>::m_lY);
      throw SMemAllocError(s);
    }
  }

  void copySel(const UCHAR *sel) {
    memcpy(m_sel.get(), sel, CSTPic<P>::m_lX * CSTPic<P>::m_lY * sizeof(UCHAR));
  }

  void copySel(const UCHAR sel) {
    memset(m_sel.get(), sel, CSTPic<P>::m_lX * CSTPic<P>::m_lY * sizeof(UCHAR));
  }

  CSTColSelPic(const CSTColSelPic &csp) /*throw(SMemAllocError) */
      : CSTPic<P>(csp) {}

  const CSTColSelPic<P> &operator=(
      const CSTColSelPic<P> &sp)  // throw(SMemAllocError)
  {
    const CSTPic<P> *spp;
    CSTPic<P> *dpp;

    try {
      CSTPic<P>::null();
      spp  = static_cast<const CSTPic<P> *>(&sp);
      dpp  = static_cast<CSTPic<P> *>(this);
      *dpp = *spp;
      if (sp.m_sel && CSTPic<P>::m_lX > 0 && CSTPic<P>::m_lY > 0) {
        initSel();
        copySel(sp.m_sel.get());
      }
    } catch (SMemAllocError) {
      throw;
    }
    return (*this);
  }

  int isInCIL(USHORT c, const COLOR_INDEX_LIST &cil) {
    for (int i = 0; i < cil.nb; i++)
      if (cil.ci[i] == c) return i;
    return -1;
  }

  int makeSelectionCMAP32(const COLOR_INDEX_LIST &ink,
                          const COLOR_INDEX_LIST &paint) {
    UCHAR *pSel = m_sel.get();
    P *pic      = CSTPic<P>::m_pic;
    int xy = 0, nbSel = 0;

    for (int y = 0; y < CSTPic<P>::m_lY; y++)
      for (int x = 0; x < CSTPic<P>::m_lX; x++, xy++, pSel++, pic++) {
        int xyRas = y * CSTPic<P>::m_ras->wrap + x;
        UD44_CMAPINDEX32 ci32 =
            *((UD44_CMAPINDEX32 *)(CSTPic<P>::m_ras->buffer) + xyRas);
        if ((ci32 & 0x000000ff) == 0xff) {
          // Paint color
          int p = isInCIL((int)((ci32 >> 8) & 0x00000fff), paint);
          if (p >= 0) {
            *pSel = 255 - p;
            nbSel++;
          }

        } else if ((ci32 & 0x000000ff) == 0x0) {
          // Ink color
          int p = isInCIL((int)((ci32 >> 20) & 0x00000fff), ink);
          if (p >= 0) {
            //*pSel=paint.nb<uc ? uc-(UCHAR)paint.nb : (UCHAR)1;
            *pSel = 255 - p;
            nbSel++;
          }

        } else {
          // Tone color
          int p = isInCIL((int)((ci32 >> 8) & 0x00000fff), paint);
          if (p >= 0)
            if (isInCIL((int)((ci32 >> 20) & 0x00000fff), ink) >= 0) {
              *pSel = 255 - p;
              nbSel++;
            }
        }
      }
    return nbSel;
  }

  int makeSelectionCMAP(const COLOR_INDEX_LIST &ink,
                        const COLOR_INDEX_LIST &paint) {
    copySel((UCHAR)0);
    if (CSTPic<P>::m_lX > 0 && CSTPic<P>::m_lY > 0 && m_sel &&
        CSTPic<P>::m_pic && CSTPic<P>::m_ras) {
      if (CSTPic<P>::m_ras->type == RAS_CM32)
        return makeSelectionCMAP32(ink, paint);
    }
    return 0;
  }

  int makeSelectionCMAP(const CCIL &ink, const CCIL &paint) {
    copySel((UCHAR)0);
    COLOR_INDEX_LIST hink, hpaint;

    hink.nb = ink.m_nb;
    int i;
    for (i = 0; i < ink.m_nb; i++) hink.ci[i] = (USHORT)ink.m_ci[i];

    hpaint.nb = paint.m_nb;
    for (i = 0; i < paint.m_nb; i++) hpaint.ci[i] = (USHORT)paint.m_ci[i];

    if (CSTPic<P>::m_lX > 0 && CSTPic<P>::m_lY > 0 && m_sel &&
        CSTPic<P>::m_pic && CSTPic<P>::m_ras) {
      if (CSTPic<P>::m_ras->type == RAS_CM32)
        return makeSelectionCMAP32(hink, hpaint);
    }
    return 0;
  }

  int makeSelectionRGB(const std::vector<I_PIXEL> &col, const double dA,
                       const double dAB) {
    int nbCol;

    copySel((UCHAR)0);
    nbCol = col.size();
    if (CSTPic<P>::m_lX > 0 && CSTPic<P>::m_lY > 0 && m_sel &&
        CSTPic<P>::m_pic) {
      switch (nbCol) {
      case 1:
        return makeSelectionRGB1(col, dA);
        break;
      case 2:
        return makeSelectionRGB2(col, dA, dAB);
        break;
      case 3:
        return makeSelectionRGB3(col, dA, dAB);
        break;
      default:
        return makeSelectionRGBMore(col, dA, dAB);
        break;
      }
    }
    return 0;
  }

  double distRGB2(const I_PIXEL &c1, const I_PIXEL &c2) const {
    double q = (double)(c1.r - c2.r) * (double)(c1.r - c2.r);
    q += (double)(c1.g - c2.g) * (double)(c1.g - c2.g);
    q += (double)(c1.b - c2.b) * (double)(c1.b - c2.b);
    return q;
  }

  double distRGBM2(const I_PIXEL &c1, const I_PIXEL &c2) const {
    double q = (double)(c1.r - c2.r) * (double)(c1.r - c2.r);
    q += (double)(c1.g - c2.g) * (double)(c1.g - c2.g);
    q += (double)(c1.b - c2.b) * (double)(c1.b - c2.b);
    q += (double)(c1.m - c2.m) * (double)(c1.m - c2.m);
    return q;
  }

  double distRGB(const I_PIXEL &c1, const I_PIXEL &c2) const {
    int qd = (c1.r - c2.r) * (c1.r - c2.r) + (c1.g - c2.g) * (c1.g - c2.g) +
             (c1.b - c2.b) * (c1.b - c2.b);
    return sqrt((double)qd);
  }

  double distRGBM(const I_PIXEL &c1, const I_PIXEL &c2) const {
    int qd = (c1.r - c2.r) * (c1.r - c2.r) + (c1.g - c2.g) * (c1.g - c2.g) +
             (c1.b - c2.b) * (c1.b - c2.b) * (c1.m - c2.m) * (c1.m - c2.m);
    return sqrt((double)qd);
  }

  inline bool isSameColor(const I_PIXEL &c1, const I_PIXEL &c2) const {
    return (c1.r == c2.r && c1.g == c2.g && c1.b == c2.b);
  }

  inline void setIntPixel(I_PIXEL &dp, const P &sp) const {
    dp.r = (int)sp.r;
    dp.g = (int)sp.g;
    dp.b = (int)sp.b;
    dp.m = (int)sp.m;
  }

  inline void setIntPixel(I_PIXEL &dp, const P *sp) const {
    dp.r = (int)sp->r;
    dp.g = (int)sp->g;
    dp.b = (int)sp->b;
    dp.m = (int)sp->m;
  }

  int makeSelectionRGB1(const std::vector<I_PIXEL> &col, const double dA) {
    P *pPic     = CSTPic<P>::m_pic;
    UCHAR *pSel = m_sel.get();
    I_PIXEL ip;
    int xy, lxy = CSTPic<P>::m_lX * CSTPic<P>::m_lY;
    int nbPixel = 0;
    double dA2  = dA * dA;

    for (xy = 0; xy < lxy; xy++, pPic++, pSel++) {
      //		if ( pPic->m>0 ) {
      setIntPixel(ip, pPic);
      if (ip.m == col[0].m && distRGB2(ip, col[0]) <= dA2) {
        *pSel = 255;
        nbPixel++;
      }
      //		}
    }
    return nbPixel;
  }

  bool isBetween(const I_PIXEL &a, const I_PIXEL &b, const I_PIXEL &c) const {
    if (c.r < std::min(a.r, b.r)) return false;
    if (c.r > std::max(a.r, b.r)) return false;
    if (c.g < std::min(a.g, b.g)) return false;
    if (c.g > std::max(a.g, b.g)) return false;
    if (c.b < std::min(a.b, b.b)) return false;
    if (c.b > std::max(a.b, b.b)) return false;
    if (c.m < std::min(a.m, b.m)) return false;
    if (c.m > std::max(a.m, b.m)) return false;
    /*
//if ( c.m!=0 )
//	tmsg_info("A=(%d,%d,%d,%d) B=(%d,%d,%d,%d)
C=(%d,%d,%d,%d)\n",a.r,a.g,a.b,a.m,b.r,b.g,b.b,b.m,c.r,c.g,c.b,c.m);
if ( c.r<a.r && c.r<b.r )
    return false;
if ( c.r>a.r && c.r>b.r )
    return false;
if ( c.g<a.g && c.g<b.g )
    return false;
if ( c.g>a.g && c.g>b.g )
    return false;
if ( c.b<a.b && c.b<b.b )
    return false;
if ( c.b>a.b && c.b>b.b )
    return false;
if ( c.m<a.m && c.m<b.m )
    return false;
if ( c.m>a.m && c.m>b.m )
    return false;
//if ( c.m!=0 )
//tmsg_info("IS_BETWEEN");
*/
    return true;
  }

  bool isLinComb(const I_PIXEL &a, const I_PIXEL &b, const I_PIXEL &c,
                 const double dAB) {
    //	if ( isSameColor(a,b) && isSameColor(a,c) )
    //		return true;
    if (!isBetween(a, b, c)) return false;
    return true;
    /*	double dab=distRGBM(a,b);
double dac=distRGBM(a,c);
double dbc=distRGBM(b,c);
double d= dab-(dac+dbc);
d= d<0.0 ? -d : d;
if ( d<=dAB )
    return true;
return false;
*/
  }

  int makeSelectionRGB2(const std::vector<I_PIXEL> &col, const double dA,
                        const double dAB) {
    P *pPic;
    UCHAR *pSel;
    I_PIXEL ip;
    int xy, lxy, nbPixel;
    double dA2 = dA * dA;

    nbPixel = 0;
    pPic    = CSTPic<P>::m_pic;
    pSel    = m_sel.get();
    lxy     = CSTPic<P>::m_lX * CSTPic<P>::m_lY;
    for (xy = 0; xy < lxy; xy++, pPic++, pSel++) {
      //		if ( pPic->m>0 ) {
      setIntPixel(ip, pPic);
      if (ip.m == col[0].m && distRGB2(ip, col[0]) <= dA2) {
        *pSel = 255;
        nbPixel++;
      } else if (ip.m == col[1].m && distRGB2(ip, col[1]) <= dA2) {
        *pSel = (UCHAR)(255 - 1);
        nbPixel++;
      } else if (isLinComb(col[0], col[1], ip, dAB)) {
        //				*pSel=(UCHAR)253;
        *pSel = distRGBM2(col[0], ip) < distRGBM2(col[1], ip)
                    ? 255
                    : (UCHAR)(255 - 1);
        nbPixel++;
      }

      //		}
    }
    return nbPixel;
  }

  int makeSelectionRGB3(const std::vector<I_PIXEL> &col, const double dA,
                        const double dAB) {
    P *pPic     = CSTPic<P>::m_pic;
    UCHAR *pSel = m_sel.get();
    I_PIXEL ip;
    int xy, nbPixel = 0;
    int lxy    = CSTPic<P>::m_lX * CSTPic<P>::m_lY;
    double dA2 = dA * dA;

    for (xy = 0; xy < lxy; xy++, pPic++, pSel++) {
      //		if ( pPic->m>0 ) {
      setIntPixel(ip, pPic);
      if (ip.m == col[0].m && distRGB2(ip, col[0]) <= dA2) {
        *pSel = 255;
        nbPixel++;
      } else if (ip.m == col[1].m && distRGB2(ip, col[1]) <= dA2) {
        *pSel = (UCHAR)(255 - 1);
        nbPixel++;
      } else if (ip.m == col[2].m && distRGB2(ip, col[2]) <= dA2) {
        *pSel = (UCHAR)(255 - 2);
        nbPixel++;
      } else if (isLinComb(col[0], col[1], ip, dAB)) {
        *pSel = distRGBM2(col[0], ip) < distRGBM2(col[1], ip) ? 255 : 255 - 1;
        nbPixel++;
      } else if (isLinComb(col[0], col[2], ip, dAB)) {
        *pSel = distRGBM2(col[0], ip) < distRGBM2(col[2], ip) ? 255 : 255 - 2;
        nbPixel++;
      } else if (isLinComb(col[1], col[2], ip, dAB)) {
        *pSel =
            distRGBM2(col[1], ip) < distRGBM2(col[2], ip) ? 255 - 1 : 255 - 2;
        nbPixel++;
      }
      //		}
    }
    return nbPixel;
  }

  int makeSelectionRGBMore(const std::vector<I_PIXEL> &col, const double dA,
                           const double dAB) {
    int nbPixel = 0, i, j, k;
    int nbCol   = col.size();
    for (i = 1, k = 0; i < nbCol; i++)
      for (j = 0; j < i; j++) {
        nbPixel += makeSelectionRGBMore(col, dA, dAB, i, j, k);
        k++;
      }
    return nbPixel;
  }

  int makeSelectionRGBMore(const std::vector<I_PIXEL> &col, const double dA,
                           const double dAB, const int i, const int j,
                           const int k) {
    P *pPic     = CSTPic<P>::m_pic;
    UCHAR *pSel = m_sel.get();
    I_PIXEL ip;
    int nbPixel = 0;
    double dA2  = dA * dA;
    int nbCol   = col.size();

    for (int y = 0; y < CSTPic<P>::m_lY; y++)
      for (int x = 0; x < CSTPic<P>::m_lX; x++, pPic++, pSel++) {
        //			if ( pPic->m>0 ) {
        setIntPixel(ip, pPic);
        if (ip.m == col[i].m && distRGB2(ip, col[i]) <= dA2) {
          *pSel = (UCHAR)(255 - i);
          nbPixel++;
        } else if (ip.m == col[j].m && distRGB2(ip, col[j]) <= dA2) {
          *pSel = (UCHAR)(255 - j);
          nbPixel++;
        } else if (isLinComb(col[i], col[j], ip, dAB)) {
          *pSel =
              distRGBM2(col[i], ip) < distRGBM2(col[j], ip) ? 255 - i : 255 - j;
          nbPixel++;
        }
        //			}
      }
    return nbPixel;
  }

  void setSel01() {
    int xy   = CSTPic<P>::m_lX * CSTPic<P>::m_lY;
    UCHAR *p = m_sel.get();
    for (int i = 0; i < xy; i++, p++) *p = *p > (UCHAR)0 ? (UCHAR)1 : (UCHAR)0;
  }

  void showSelection() {
    P *pPic;
    UCHAR *pSel;
    int lxy, xy;
    int fcolor;

    lxy             = CSTPic<P>::m_lX * CSTPic<P>::m_lY;
    pSel            = m_sel.get();
    pPic            = CSTPic<P>::m_pic;
    bool isRGBMType = CSTPic<P>::getType() == ST_RGBM;
    fcolor          = isRGBMType ? 255 : 65535;
    for (xy = 0; xy < lxy; xy++, pPic++, pSel++) {
      /*		if (*pSel==(UCHAR)0) {
      pPic->r=fcolor/2;
      pPic->g=0;
      pPic->b=0;
      pPic->m=fcolor;
}
*/
      if (*pSel > (UCHAR)0) {
        pPic->r = 255;
        pPic->g = 0;
        pPic->b = 0;
        pPic->m = fcolor;
      }
      /*		if (*pSel==(UCHAR)2) {
      pPic->r=0;
      pPic->g=0;
      pPic->b=fcolor;
      pPic->m=fcolor;
}
if (*pSel==(UCHAR)3) {
      pPic->r=fcolor;
      pPic->g=0;
      pPic->b=0;
      pPic->m=fcolor;
}
*/
      /*		if (*pSel==(UCHAR)255) {
      pPic->r=0;
      pPic->g=0;
      pPic->b=fcolor;
      pPic->m=fcolor;
}
if (*pSel==(UCHAR)254) {
      pPic->r=fcolor;
      pPic->g=fcolor;
      pPic->b=0;
      pPic->m=fcolor;
}
if (*pSel==(UCHAR)253) {
      pPic->r=fcolor;
      pPic->g=0;
      pPic->b=0;
      pPic->m=fcolor;
}

if (*pSel==(UCHAR)251) {
      pPic->r=fcolor;
      pPic->g=0;
      pPic->b=fcolor;
      pPic->m=fcolor;
}
if (*pSel==(UCHAR)250) {
      pPic->r=0;
      pPic->g=fcolor;
      pPic->b=fcolor;
      pPic->m=fcolor;
}
*/
    }
  }

  void selBox(SRECT &box) {
    box.x0      = CSTPic<P>::m_lX;
    box.x1      = -1;
    box.y0      = CSTPic<P>::m_lY;
    box.y1      = -1;
    UCHAR *pSel = m_sel.get();
    for (int y = 0; y < CSTPic<P>::m_lY; y++)
      for (int x = 0; x < CSTPic<P>::m_lX; x++, pSel++)
        if (*pSel > (UCHAR)0) {
          box.x0 = std::min(box.x0, x);
          box.x1 = std::max(box.x1, x);
          box.y0 = std::min(box.y0, y);
          box.y1 = std::max(box.y1, y);
        }
  }

  void selRunLengthH(const SRECT &box, const int step, int &rl, int &nbRl) {
    for (int y = box.y0; y <= (box.y1); y += step) {
      int l       = 0;
      bool isIn   = false;
      UCHAR *pSel = m_sel.get() + y * CSTPic<P>::m_lX + box.x0;
      for (int x = box.x0; x <= box.x1; x++, pSel++) {
        if (isIn && *pSel == (UCHAR)0) {
          rl += l;
          nbRl++;
          l    = 0;
          isIn = false;
        } else if (isIn && *pSel > (UCHAR)0) {
          l++;
        } else if (!isIn && *pSel > (UCHAR)0) {
          l    = 1;
          isIn = true;
        }
      }
      if (isIn) {
        rl += l;
        nbRl++;
      }
    }
  }

  void selRunLengthV(const SRECT &box, const int step, int &rl, int &nbRl) {
    for (int x = box.x0; x <= (box.x1); x += step) {
      int l     = 0;
      bool isIn = false;
      for (int y = box.y0; y <= box.y1; y++) {
        UCHAR *pSel = m_sel.get() + y * CSTPic<P>::m_lX + x;
        if (isIn && *pSel == (UCHAR)0) {
          rl += l;
          nbRl++;
          l    = 0;
          isIn = false;
        } else if (isIn && *pSel > (UCHAR)0) {
          l++;
        } else if (!isIn && *pSel > (UCHAR)0) {
          l    = 1;
          isIn = true;
        }
      }
      if (isIn) {
        rl += l;
        nbRl++;
      }
    }
  }

  int selRunLength(const int step) {
    int s = step < 1 ? 1 : step;
    SRECT box;
    selBox(box);
    int rl   = 0;
    int nbRl = 0;
    selRunLengthH(box, s, rl, nbRl);
    selRunLengthV(box, s, rl, nbRl);
    if (nbRl > 0) {
      double d = (double)rl / (double)nbRl;
      return I_ROUNDP(d);
    }
    return 0;
  }

  void hlsNoise(const double d) {
    int xy      = CSTPic<P>::m_lX * CSTPic<P>::m_lY;
    P *p        = CSTPic<P>::m_pic;
    UCHAR *pSel = m_sel.get();
    for (int i = 0; i < xy; i++, p++, pSel++)
      if (p->m > 0 && (*pSel) > (UCHAR)0) {
        double h, l, s, q;
        rgb2hls(p->r, p->g, p->b, &h, &l, &s);
        q = 1.0 - d * (double)((rand() % 201) - 100) / 100.0;
        l *= q;
        hls2rgb(h, l, s, &(p->r), &(p->g), &(p->b));
      }
  }
  /*
void expand(const int border) throw(SMemAllocError)
{	try {
          int olX=m_lX;
          int olY=m_lY;
          CSTPic<P>::expand(border);
          UCHAR* nSel=new UCHAR[m_lX*m_lY];
          if ( !nSel )
                  throw SMemAllocError("in expand");
          UCHAR* pNSel=nSel;
          for( int y=0; y<m_lY; y++ )
                  for( int x=0; x<m_lX; x++,pNSel++ ) {
                          int ox=x-border;
                          int oy=y-border;
                          if ( ox>=0 && ox<olX && oy>=0 && oy<olY ) {
                                  *pNSel=*(m_sel+oy*olX+ox);
                          } else
                                  *pNSel=(UCHAR)0;
                  }
          delete [] m_sel;
          m_sel=nSel;
  }
  catch (SMemAllocError) {
          throw;
  }
}
*/
};

#endif  // !defined(AFX_STCOLSELPIC_H__80D708B0_FCA2_11D5_B949_0040F674BE6A__INCLUDED_)
