#pragma once

// STPic.h: interface for the CSTPic class.
//
//////////////////////////////////////////////////////////////////////

#include <assert.h>

#ifndef STPIC_H
#define STPIC_H

/****** SASA Picture Class Template **********************

Currently there are two types of this object
ST_RGBM		- CSTPic<UC_PIXEL>	- UCHAR r,g,b,m channels
ST_RGBM64	- CSTPic<US_PIXEL>	- USHORT r,g,b,m channels

The CMAP RASTER pictures have to be converted to ST_RGBM or
ST_RGBM64, but the CMAP information can be used with the help
of m_raster.

m_lX,m_lY - the length of the picture in X,Y direction
m_pic - the buffer of the picture
m_ras - stores the pointer to the original RASTER picture

************************************************************/

#include "toonz4.6/udit.h"
#include "toonz4.6/raster.h"
#include "toonz4.6/pixel.h"
#include "SDef.h"
#include "SError.h"

#include "timagecache.h"
#include "trasterimage.h"

#define ISINPIC(x, y) (m_pic && x >= 0 && x < m_lX && y >= 0 && y < m_lY)

typedef enum {
  ST_NIL,    // EMPTY
  ST_RGBM,   // UC_PIXEL
  ST_RGBM64  // US_PIXEL
} ST_TYPE;

//! Old 4.6 picture class template (sandor fxs). There is a significant
//! modification
//! to be considered starting from Toonz 6.1 - the allocated raster is now
//! managed
//! by the image cache. Basically, when constructing one such object, the image
//! is locked
//! in the Toonz cache - and you must remember to unlock and relock it along
//! inactivity periods.
template <class P>
class CSTPic {
  std::string m_cacheId;
  TRasterImageP m_picP;

public:
  int m_lX, m_lY;
  P *m_pic;
  const RASTER *m_ras;

  CSTPic(void)
      : m_cacheId(TImageCache::instance()->getUniqueId())
      , m_lX(0)
      , m_lY(0)
      , m_pic(0)
      , m_ras(0) {}

  void nullPic() {
    // if ( m_pic ) { delete [] m_pic; m_pic=0; }
    unlock();
    TImageCache::instance()->remove(m_cacheId);
  }

  //! Retrieves the raster image from the cache to work with it.
  void lock() {
    m_picP = TImageCache::instance()->get(m_cacheId, true);
    m_pic  = (P *)m_picP->getRaster()->getRawData();
  }

  //! Release the raster image for inactivity periods.
  void unlock() {
    m_picP = 0;
    m_pic  = 0;
  }

  CSTPic(const int lX, const int lY)
      :  // throw(SMemAllocError) :
      m_cacheId(TImageCache::instance()->getUniqueId())
      , m_lX(lX)
      , m_lY(lY)
      , m_pic(0)
      , m_ras(0) {
    try {
      initPic();
    } catch (SMemAllocError) {
      null();
      throw;
    }
  }

  CSTPic(const CSTPic<P> &sp)
      :  // throw(SMemAllocError) :
      m_lX(sp.m_lX)
      , m_lY(sp.m_lY)
      , m_pic(0)
      , m_ras(sp.m_ras) {
    try {
      initPic();
      copyPic(sp);
    } catch (SMemAllocError) {
      null();
      throw;
    }
  }

  ~CSTPic(void) { null(); }

  //! Allocates the specified raster and surrenders it to the image cache.
  //!\b NOTE: The raster being initialized is LOCKED after this call.
  //! You must remember to UNLOCK it when no more used.
  void initPic()  // throw(SMemAllocError)
  {
    nullPic();
    if (m_lX > 0 && m_lY > 0) {
      // m_pic=new P[m_lX*m_lY];
      TRasterGR8P ras(m_lX * m_lY * sizeof(P), 1);
      if (!ras) throw SMemAllocError("in initPic");
      TImageCache::instance()->add(m_cacheId, TRasterImageP(ras));
      lock();
    } else {
      char s[200];
      snprintf(s, sizeof(s), "in initPic lXY=(%d,%d)\n", m_lX, m_lY);
      throw SMemAllocError(s);
    }
  }

  void null() {
    nullPic();
    m_lX = m_lY = 0;
    m_ras       = 0;
  }

  // Draws the border of the CSTPic
  void drawRect(const P &ip) {
    SRECT rect = {0, 0, m_lX - 1, m_lY - 1};
    drawRect(rect, ip);
  }

  //------------------- The following need to be *LOCKED* before the call
  //------------------

  // Currently there are two types of STPic
  // ST_RGBM   -	UCHAR r,g,b,m channels
  // ST_RGBM64 -	USHORT r,g,b,m channels
  ST_TYPE getType() const {
    if (!m_pic) return ST_NIL;
    if (sizeof(m_pic->r) == sizeof(UCHAR)) return ST_RGBM;
    if (sizeof(m_pic->r) == sizeof(USHORT)) return ST_RGBM64;
    return ST_NIL;
  }

  // Copies the 'sp' CSTPic into 'm_pic'

  void copyPic(const CSTPic<P> &sp) {
    P p;
    for (int y = 0; y < m_lY && y < sp.m_lY; y++)
      for (int x = 0; x < m_lX && x < sp.m_lX; x++) {
        sp.getPixel(x, y, p);
        setRGBM(x, y, p);
      }
  }

  // Copies the 'r' rectangle of 'sp' CSTPic into the 'p' position
  // of 'm_pic'
  void copyPic(const CSTPic<P> &sp, const SRECT &r, const SPOINT &p) {
    P ip;
    int xs, ys, xd, yd;
    for (ys = r.y0, yd = p.y; ys <= r.y1; ys++, yd++)
      for (xs = r.x0, xd = p.x; xs <= r.x1; xs++, xd++) {
        sp.getPixel(xs, ys, ip);
        setRGBM(xd, yd, ip);
      }
  }

  // Draws a rectangle into CSTPic
  void drawRect(const SRECT &rect, const P &ip) {
    for (int x = rect.x0; x <= rect.x1; x++) {
      setRGBM(x, rect.y0, ip);
      setRGBM(x, rect.y1, ip);
    }
    for (int y = rect.y0; y <= rect.y1; y++) {
      setRGBM(rect.x0, y, ip);
      setRGBM(rect.x1, y, ip);
    }
  }

  void erease() {
    P p = {0, 0, 0, 0};
    fill(p);
  }

  void fill(const P &ip) {
    for (int y = 0; y < m_lY; y++)
      for (int x = 0; x < m_lX; x++) setRGBM(x, y, ip);
  }

  // Checks whether (x,y) is a proper coordinate
  inline bool isInPic(const int x, const int y) const {
    return (m_pic && x >= 0 && x < m_lX && y >= 0 && y < m_lY);
  }

  // Sets the color of the CSTPic pixel
  void setRGB(const int x, const int y, const P &ip) {
    if (ISINPIC(x, y)) {
      P *p = m_pic + y * m_lX + x;
      p->r = ip.r;
      p->g = ip.g;
      p->b = ip.b;
    }
  }

  void setRGBM(const int x, const int y, const P &ip) {
    if (ISINPIC(x, y)) {
      P *p = m_pic + y * m_lX + x;
      p->r = ip.r;
      p->g = ip.g;
      p->b = ip.b;
      p->m = ip.m;
    }
  }

  // Sets the color of the CSTPic pixel
  void setRGB(const int x, const int y, const I_PIXEL &ip) {
    if (ISINPIC(x, y)) {
      P *p = m_pic + y * m_lX + x;
      if (getType() == ST_RGBM) {
        p->r = (UCHAR)I_CUT_0_255(ip.r);
        p->g = (UCHAR)I_CUT_0_255(ip.g);
        p->b = (UCHAR)I_CUT_0_255(ip.b);
      } else if (getType() == ST_RGBM64) {
        p->r = (USHORT)I_CUT_0_65535(ip.r);
        p->g = (USHORT)I_CUT_0_65535(ip.g);
        p->b = (USHORT)I_CUT_0_65535(ip.b);
      }
    }
  }

  void setRGBM(const int x, const int y, const I_PIXEL &ip) {
    if (ISINPIC(x, y)) {
      P *p = m_pic + y * m_lX + x;
      if (getType() == ST_RGBM) {
        p->r = (UCHAR)I_CUT_0_255(ip.r);
        p->g = (UCHAR)I_CUT_0_255(ip.g);
        p->b = (UCHAR)I_CUT_0_255(ip.b);
        p->m = (UCHAR)I_CUT_0_255(ip.m);
      } else if (getType() == ST_RGBM64) {
        p->r = I_CUT_0_65535(ip.r);
        p->g = I_CUT_0_65535(ip.g);
        p->b = I_CUT_0_65535(ip.b);
        p->m = I_CUT_0_65535(ip.m);
      }
    }
  }

  // Gets the color of the CSTPic pixel
  void getPixel(const int x, const int y, P &ip) const {
    if (ISINPIC(x, y)) {
      const P *p = m_pic + y * m_lX + x;
      ASSIGN_PIXEL(&ip, p);
      return;
    }
    ip.r = ip.g = ip.b = ip.m = 0;
  }

  void getPixel(const int x, const int y, I_PIXEL &ip) const {
    if (ISINPIC(x, y)) {
      const P *p = m_pic + y * m_lX + x;
      ip.r       = (int)p->r;
      ip.g       = (int)p->g;
      ip.b       = (int)p->b;
      ip.m       = (int)p->m;
      return;
    }
    ip.r = ip.g = ip.b = ip.m = 0;
  }

  // Gets the color of a RASTER pixel
  // RASTER operation always gets I_PIXEL color and I_PIXEL is casted
  // to the right type
  void getRasterPixel(const RASTER *ras, const int x, const int y,
                      I_PIXEL &ip) const {
    if (x >= 0 && x < ras->lx && y >= 0 && y < ras->ly && ras->buffer) {
      LPIXEL *pL;
      LPIXEL pLL;
      SPIXEL *pS;
      UD44_CMAPINDEX32 *ci32;
      // UD44_PIXEL32* pen;
      UD44_PIXEL32 *col;
      switch (ras->type) {
      case (RAS_RGBM):
        pL   = (LPIXEL *)(ras->buffer) + y * ras->wrap + x;
        ip.r = (int)pL->r;
        ip.g = (int)pL->g;
        ip.b = (int)pL->b;
        ip.m = (int)pL->m;
        break;
      case (RAS_RGBM64):
        pS   = (SPIXEL *)(ras->buffer) + y * ras->wrap + x;
        ip.r = (int)pS->r;
        ip.g = (int)pS->g;
        ip.b = (int)pS->b;
        ip.m = (int)pS->m;
        break;
      case (RAS_CM32):
        ci32 = (UD44_CMAPINDEX32 *)(ras->buffer) + y * ras->wrap + x;
        col  = (UD44_PIXEL32 *)(ras->cmap.buffer);
        PIX_CM32_MAP_TO_RGBM(*ci32, col, pLL)
        ip.r = (int)pLL.r;
        ip.g = (int)pLL.g;
        ip.b = (int)pLL.b;
        ip.m = (int)pLL.m;
        break;
      default:
         break;
     }
    } else
      ip.r = ip.g = ip.b = ip.m = 0;
  }

  void getRasterPixel(const int x, const int y, I_PIXEL &ip) const {
    if (m_ras)
      getRasterPixel(m_ras, x, y, ip);
    else
      ip.r = ip.g = ip.b = ip.m = 0;
  }

  // Sets the color of a RASTER pixel
  // RASTER operation always gets I_PIXEL color and I_PIXEL is casted
  // to the right type

  void setRasterPixel(RASTER *ras, const int x, const int y,
                      const I_PIXEL &ip) const {
    if (x >= 0 && x < ras->lx && y >= 0 && y < ras->ly && ras->buffer) {
      LPIXEL *pL;
      SPIXEL *pS;
      switch (ras->type) {
      case (RAS_RGBM):
        pL    = (LPIXEL *)(ras->buffer) + y * ras->wrap + x;
        pL->r = (UCHAR)ip.r;
        pL->g = (UCHAR)ip.g;
        pL->b = (UCHAR)ip.b;
        pL->m = (UCHAR)ip.m;
        break;
      case (RAS_RGBM64):
        pS    = (SPIXEL *)(ras->buffer) + y * ras->wrap + x;
        pS->r = (USHORT)ip.r;
        pS->g = (USHORT)ip.g;
        pS->b = (USHORT)ip.b;
        pS->m = (USHORT)ip.m;
        break;
      default:
         break;
      }
    }
  }

  bool copy_raster(const RASTER *ir, RASTER *out_r, const int xBeg,
                   const int yBeg, const int xEnd, const int yEnd, const int ox,
                   const int oy)

  {
    if ((ir->lx <= 0) || (ir->ly <= 0) || (out_r->lx <= 0) || (out_r->ly <= 0))
      return false;
    if (ir->buffer == NULL || out_r->buffer == NULL) return false;
    if (out_r->type == RAS_CM32) return false;
    if (ir->type == RAS_CM32 && (ir->cmap.buffer == NULL)) return false;

    for (int y = yBeg, yy = oy; y <= yEnd; y++, yy++)
      for (int x = xBeg, xx = ox; x <= xEnd; x++, xx++) {
        I_PIXEL ip;
        getRasterPixel(ir, x, y, ip);
        if (xx >= 0 && yy >= 0 && xx < out_r->lx && yy < out_r->ly) {
          LPIXEL *pL;
          SPIXEL *pS;
          if (out_r->type == RAS_RGBM &&
              (ir->type == RAS_RGBM || ir->type == RAS_CM32)) {
            pL    = (LPIXEL *)(out_r->buffer) + yy * out_r->wrap + xx;
            pL->r = (UCHAR)ip.r;
            pL->g = (UCHAR)ip.g;
            pL->b = (UCHAR)ip.b;
            pL->m = (UCHAR)ip.m;
          } else if (out_r->type == RAS_RGBM && ir->type == RAS_RGBM64) {
            pL    = (LPIXEL *)(out_r->buffer) + yy * out_r->wrap + xx;
            pL->r = PIX_BYTE_FROM_USHORT((USHORT)ip.r);
            pL->g = PIX_BYTE_FROM_USHORT((USHORT)ip.g);
            pL->b = PIX_BYTE_FROM_USHORT((USHORT)ip.b);
            pL->m = PIX_BYTE_FROM_USHORT((USHORT)ip.m);
          } else if (out_r->type == RAS_RGBM64 &&
                     (ir->type == RAS_RGBM || ir->type == RAS_CM32)) {
            pS    = (SPIXEL *)(out_r->buffer) + yy * out_r->wrap + xx;
            pS->r = PIX_USHORT_FROM_BYTE((UCHAR)ip.r);
            pS->g = PIX_USHORT_FROM_BYTE((UCHAR)ip.g);
            pS->b = PIX_USHORT_FROM_BYTE((UCHAR)ip.b);
            pS->m = PIX_USHORT_FROM_BYTE((UCHAR)ip.m);
          } else if (out_r->type == RAS_RGBM64 && ir->type == RAS_RGBM64) {
            pS    = (SPIXEL *)(out_r->buffer) + yy * out_r->wrap + xx;
            pS->r = (USHORT)ip.r;
            pS->g = (USHORT)ip.g;
            pS->b = (USHORT)ip.b;
            pS->m = (USHORT)ip.m;
          }
        }
      }
    return true;
  }

  // Generates and reads the CSTPic using RASTER.
  // \b NOTE: This LOCKS the raster being read. You must remember to unlock it
  // afterwards
  // when it is needed no more.
  virtual void read(const RASTER *ras)  // throw(SMemAllocError)
  {
    try {
      null();
      if (((ras->type == RAS_RGBM || ras->type == RAS_RGBM64) && ras->buffer &&
           ras->lx > 0 && ras->ly > 0) ||
          (ras->type == RAS_CM32 && ras->buffer && ras->cmap.buffer &&
           ras->lx > 0 && ras->ly > 0)) {
        m_lX  = ras->lx;
        m_lY  = ras->ly;
        m_ras = ras;
        initPic();
        lock();
        ST_TYPE type = getType();
        P *p         = m_pic;
        I_PIXEL ip;
        memset(&ip, 0, sizeof(I_PIXEL));
        for (int y = 0; y < m_lY; y++)
          for (int x = 0; x < m_lX; x++, p++) {
            getRasterPixel(ras, x, y, ip);
            switch (type) {
            case (ST_RGBM):
              if (ras->type == RAS_RGBM64) {
                // RGBM have to be 'scaled' from USHORT to UCHAR
                p->r = PIX_BYTE_FROM_USHORT((USHORT)ip.r);
                p->g = PIX_BYTE_FROM_USHORT((USHORT)ip.g);
                p->b = PIX_BYTE_FROM_USHORT((USHORT)ip.b);
                p->m = PIX_BYTE_FROM_USHORT((USHORT)ip.m);
              } else {
                // RGBM are UCHAR in the raster
                p->r = (UCHAR)ip.r;
                p->g = (UCHAR)ip.g;
                p->b = (UCHAR)ip.b;
                p->m = (UCHAR)ip.m;
              }
              break;
            case (ST_RGBM64):
              if (ras->type == RAS_RGBM64) {
                // RGBM are USHORT in the raster
                p->r = ip.r;
                p->g = ip.g;
                p->b = ip.b;
                p->m = ip.m;
              } else {
                // RGBM have to be 'scaled' from UCHAR to USHORT
                p->r = PIX_USHORT_FROM_BYTE((UCHAR)ip.r);
                p->g = PIX_USHORT_FROM_BYTE((UCHAR)ip.g);
                p->b = PIX_USHORT_FROM_BYTE((UCHAR)ip.b);
                p->m = PIX_USHORT_FROM_BYTE((UCHAR)ip.m);
              }
              break;
            default:
              break;
            }
          }
      }
    } catch (SMemAllocError) {
      null();
      throw;
    }
  }

  const CSTPic<P> &operator=(const CSTPic<P> &sp)  // throw(SMemAllocError)
  {
    try {
      null();
      m_lX  = sp.m_lX;
      m_lY  = sp.m_lY;
      m_ras = sp.m_ras;
      initPic();
      copyPic(sp);
    } catch (SMemAllocError) {
      null();
      throw;
    }

    return (*this);
  }

  // Writes the CSTPic into the RASTER
  virtual void write(RASTER *ras) const  // throw(SWriteRasterError)
  {
    if ((ras->type == RAS_RGBM || ras->type == RAS_RGBM64) && ras->lx > 0 &&
        ras->ly > 0 && ras->buffer) {
      int x, y;
      I_PIXEL ip;
      const P *p;
      for (y = 0; y < m_lY && y < ras->ly; y++)
        for (x = 0; x < m_lX && x < ras->lx; x++) {
          p    = m_pic + y * m_lX + x;
          ip.r = (int)p->r;
          ip.g = (int)p->g;
          ip.b = (int)p->b;
          ip.m = (int)p->m;
          setRasterPixel(ras, x, y, ip);
        }
    } else
      throw SWriteRasterError("(bad Raster type)");
  }

  // Writes the 'r' rectangle of CSTPic into the 'p' position in RASTER
  virtual void write(RASTER *ras, const SRECT &r, const SPOINT &p) const
  // throw(SWriteRasterError)
  {
    if (ras->type == RAS_RGBM || ras->type == RAS_RGBM64) {
      int xs, ys, xd, yd;
      P pp;
      I_PIXEL ip;
      for (ys = r.y0, yd = p.y; ys <= r.y1; ys++, yd++)
        for (xs = r.x0, xd = p.x; xs <= r.x1; xs++, xd++) {
          getPixel(xs, ys, pp);
          ip.r = (int)pp.r;
          ip.g = (int)pp.g;
          ip.b = (int)pp.b;
          ip.m = (int)pp.m;
          setRasterPixel(ras, xd, yd, ip);
        }
    } else
      throw SWriteRasterError("(bad Raster type)");
  }

  virtual void writeOutBorder(const RASTER *rasin, const int border,
                              RASTER *ras, const SRECT &r,
                              const SPOINT &p) const
  // throw(SWriteRasterError)
  {
    assert(rasin->type == RAS_CM32);
    UD44_PIXEL32 *col = (UD44_PIXEL32 *)(rasin->cmap.buffer);

    if (ras->type == RAS_RGBM || ras->type == RAS_RGBM64) {
      int xs, ys, xd, yd;

      I_PIXEL ip;
      for (ys = r.y0, yd = p.y; ys <= r.y1; ys++, yd++)
        for (xs = r.x0, xd = p.x; xs <= r.x1; xs++, xd++) {
          int x = xd - border;
          int y = yd - border;
          if (x >= 0 && y >= 0 && x < rasin->lx && y < rasin->ly) {
            UD44_CMAPINDEX32 pixel =
                *(((UD44_CMAPINDEX32 *)rasin->buffer) + y * rasin->wrap + x);
            // int tone = (pix&0xff);
            // int paint = ((pix>>8)&0xfff);
            if ((pixel & 0xff) == 0 || ((pixel >> 8) & 0xfff) != 0) {
              LPIXEL pLL;
              PIX_CM32_MAP_TO_RGBM(pixel, col, pLL)
              ip.r = (int)pLL.r;
              ip.g = (int)pLL.g;
              ip.b = (int)pLL.b;
              ip.m = (int)pLL.m;
              setRasterPixel(ras, xd, yd, ip);
              continue;
            }
          }

          P pp;
          getPixel(xs, ys, pp);
          ip.r = (int)pp.r;
          ip.g = (int)pp.g;
          ip.b = (int)pp.b;
          ip.m = (int)pp.m;
          setRasterPixel(ras, xd, yd, ip);
        }
    } else
      throw SWriteRasterError("(bad Raster type)");
  }

  void convertToCurrentType(P &d, const I_PIXEL &s) const {
    if (getType() == ST_RGBM) {
      d.r = (UCHAR)s.r;
      d.g = (UCHAR)s.g;
      d.b = (UCHAR)s.b;
      d.m = (UCHAR)s.m;
    }
    if (getType() == ST_RGBM64) {
      d.r = (USHORT)s.r;
      d.g = (USHORT)s.g;
      d.b = (USHORT)s.b;
      d.m = (USHORT)s.m;
    }
  }

  bool isSameColor(const P *a, const P *b) const {
    if (a->r == b->r && a->g == b->g && a->b == b->b) return true;
    return false;
  }

  void colorNoise(const I_PIXEL &cc1, const I_PIXEL &cc2, const double d) {
    P c1, c2;

    if (d <= 0) return;

    convertToCurrentType(c1, cc1);
    convertToCurrentType(c2, cc2);
    int xy  = m_lX * m_lY;
    P *pPic = m_pic;
    for (int i = 0; i < xy; i++, pPic++)
      if (isSameColor(&c1, pPic)) {
        double q = (rand() % 101) / 100.0;
        q        = d * q;
        q        = q > 1.0 ? 1.0 : q;
        double r = (1.0 - q) * (double)c1.r + q * (double)c2.r;
        double g = (1.0 - q) * (double)c1.g + q * (double)c2.g;
        double b = (1.0 - q) * (double)c1.b + q * (double)c2.b;
        r        = D_CUT_0_255(r);
        g        = D_CUT_0_255(g);
        b        = D_CUT_0_255(b);
        pPic->r  = UC_ROUND(r);
        pPic->g  = UC_ROUND(g);
        pPic->b  = UC_ROUND(b);
      }
  }

  /*
void hlsNoise(const double d)
{	int xy=m_lX*m_lY;
  P* p=m_pic;
  for( int i=0; i<xy; i++,p++ )
          if ( p->m>0 ) {
                  double h,l,s,q;
                  rgb2hls(p->r,p->g,p->b,&h,&l,&s);
                  q=1.0-d*(double)((rand()%201)-100)/100.0;
                  l*=q;
                  hls2rgb(h,l,s,&(p->r),&(p->g),&(p->b));
          }
}
*/

  void rgb2hls(UCHAR r, UCHAR g, UCHAR b, double *h, double *l, double *s) {
    double ma, mi, delta, sum;
    double rf, gf, bf;

    rf = (double)r / 255.0;
    gf = (double)g / 255.0;
    bf = (double)b / 255.0;

    ma    = rf > gf ? rf : gf;
    ma    = ma > bf ? ma : bf;
    mi    = rf < gf ? rf : gf;
    mi    = mi < bf ? mi : bf;
    sum   = ma + mi;
    delta = ma - mi;
    *l    = sum / 2.0;
    if (fabs(delta) < 0.000001) {
      *s = 0.0;
      *h = UNDEFINED;
    } else {
      *s = *l <= 0.5 ? delta / sum : delta / (2.0 - sum);
      *h = fabs((rf - ma)) < 0.000001
               ? (gf - bf) / delta
               : (fabs((gf - ma)) < 0.000001 ? 2.0 + (bf - rf) / delta
                                             : 4.0 + (rf - gf) / delta);
      *h *= 60;
      *h = *h < 0.0 ? *h + 360.0 : *h;
    }
  }

  double fromHue(double n1, double n2, double hue) {
    double v, h;

    h = hue > 360.0 ? hue - 360.0 : hue;
    h = h < 0.0 ? h + 360.0 : h;

    if (h < 60.0) {
      v = n1 + (n2 - n1) * h / 60.0;
    } else if (h < 180.0) {
      v = n2;
    } else if (h < 240.0) {
      v = n1 + (n2 - n1) * (240.0 - h) / 60.0;
    } else
      v = n1;
    return (v);
  }

  void hls2rgb(double h, double l, double s, UCHAR *r, UCHAR *g, UCHAR *b) {
    double rf, gf, bf;
    double m1, m2;

    m2 = l <= 0.5 ? l * (1.0 + s) : l + s - l * s;
    m1 = 2 * l - m2;
    if (fabs(s - 0.0) < 0.000001) {
      if (fabs(h - UNDEFINED) < 0.000001) {
        rf = gf = bf = l * 255.0;
      } else
        rf = gf = bf = 0.0;
    } else {
      rf = fromHue(m1, m2, h + 120.0) * 255.0;
      gf = fromHue(m1, m2, h) * 255.0;
      bf = fromHue(m1, m2, h - 120.0) * 255.0;
    }
    rf = D_CUT_0_255(rf);
    gf = D_CUT_0_255(gf);
    bf = D_CUT_0_255(bf);
    *r = UC_ROUND(rf);
    *g = UC_ROUND(gf);
    *b = UC_ROUND(bf);
  }
};
#endif  // !defined(AFX_STPIC_H__BABE9488_F054_11D5_B927_0040F674BE6A__INCLUDED_)
