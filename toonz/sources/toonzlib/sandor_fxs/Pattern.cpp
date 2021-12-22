

// Pattern.cpp: implementation of the CPattern class.
//
//////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <string.h>
//#include <io.h>
#include <fcntl.h>

#include "Pattern.h"
#include "toonz4.6/raster.h"
//#include "toonz4.6/img.h"
#include "toonz4.6/casm.h"
//#include "toonz4.6/casm_msg.h"
#include "toonz4.6/file.h"
#include "trop.h"
#include "toonz4_6staff.h"

#include "SDef.h"
#include "SError.h"
#include "STPic.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//#define P(a) tmsg_warning("-- %d --",a)

CPattern::CPattern(RASTER *imgContour) : m_lX(0), m_lY(0) {
  if (!readPattern(imgContour)) {
    throw SFileReadError();
  }
  try {
    optimalizeSize();
  } catch (SMemAllocError) {
    throw;
  }
}

CPattern::~CPattern() { null(); }

void CPattern::null() {
  m_lX = m_lY = 0;
  m_pat.reset();
  m_fn[0] = '\0';
}

bool CPattern::readTTT(const char *fn) {
  /*
  int fd;

  if ( (fd=_open(fn, _O_RDONLY | _O_BINARY ))==-1 )
          return false;
  if ( _read(fd,&m_lX,sizeof(int))!=sizeof(int) ) {
          _close(fd);
          return false;
  }
  if ( _read(fd,&m_lY,sizeof(int))!=sizeof(int) ) {
          _close(fd);
          return false;
  }
  if ( m_lX<=0 || m_lY<=0 ) {
          _close(fd);
          return false;
  }

  UCHAR *buffer=0;
  buffer=new UCHAR[sizeof(UCHAR)*4*m_lX*m_lY];
  if ( !buffer ) {
          _close(fd);
          return false;
  }

  m_pat=new UC_PIXEL[m_lX*m_lY];
  if ( !m_pat ) {
          delete [] buffer;
          _close(fd);
          return false;
  }

  int length=4*m_lX*m_lY*sizeof(UCHAR);
  if ( _read(fd,buffer,length)!=length ) {
          delete [] buffer;
          _close(fd);
          return false;
  }

  int xy=m_lX*m_lY;
  UCHAR* pb=buffer;
  UC_PIXEL* pPat=m_pat;
  for( int i=0; i<xy; i++,pPat++,pb+=4 ) {
          pPat->r=pb[0];
          pPat->g=pb[1];
          pPat->b=pb[2];
          pPat->m=pb[3];
  }

  delete [] buffer;
  _close(fd);

  return true;
*/
  return false;
}
//---------------------------------------------------------------------

bool CPattern::readPattern(RASTER *imgContour) {
  null();
  if (!imgContour) {
    return false;
  } else {
    RASTER *r = imgContour;
    if (r) {
      CSTPic<UC_PIXEL> lpic;
      lpic.read(r);
      if (lpic.m_lX > 0 && lpic.m_lY > 0 && lpic.m_pic) {
        m_lX = lpic.m_lX;
        m_lY = lpic.m_lY;
        m_pat.reset(new UC_PIXEL[m_lX * m_lY]);
        if (!m_pat) {
          m_lX = m_lY = 0;
          lpic.null();
          //			TRop::releaseRaster46(r, true);
          return false;
        }
        for (int y = 0; y < m_lY; y++)
          for (int x = 0; x < m_lX; x++) {
            UC_PIXEL *plp = lpic.m_pic + y * lpic.m_lX + x;
            UC_PIXEL *ucp = m_pat.get() + y * m_lX + x;
            ucp->r        = plp->r;
            ucp->g        = plp->g;
            ucp->b        = plp->b;
            ucp->m        = plp->m;
          }
      } else {
        lpic.null();
        //	TRop::releaseRaster46(r, true);
        return false;
      }
      // TRop::releaseRaster46(r, true);
    } else
      return false;
  }
  return true;
}

void CPattern::getMapPixel(const int xx, const int yy, const double invScale,
                           const double si, const double co, UC_PIXEL *&pucp) {
  pucp        = 0;
  double dxx  = (double)xx * invScale;
  double dyy  = (double)yy * invScale;
  double d2xx = (dxx * co - dyy * si) + (double)(m_lX - 1) * 0.5;
  double d2yy = (dxx * si + dyy * co) + (double)(m_lY - 1) * 0.5;
  int x       = I_ROUND(d2xx);
  int y       = I_ROUND(d2yy);
  if (x >= 0 && x < m_lX && y >= 0 && y < m_lY) {
    pucp = m_pat.get() + y * m_lX + x;
    pucp = (pucp->m) == (UCHAR)0 ? 0 : pucp;
  }
}

void CPattern::getMapPixel(const int xx, const int yy, const double invScale,
                           UC_PIXEL *&pucp) {
  pucp       = 0;
  double dxx = (double)xx * invScale + (double)(m_lX - 1) * 0.5;
  double dyy = (double)yy * invScale + (double)(m_lY - 1) * 0.5;
  int x      = I_ROUND(dxx);
  int y      = I_ROUND(dyy);
  if (x >= 0 && x < m_lX && y >= 0 && y < m_lY) {
    pucp = m_pat.get() + y * m_lX + x;
    pucp = (pucp->m) == (UCHAR)0 ? 0 : pucp;
  }
}

void CPattern::getBBox(SRECT &bb) {
  bb.x0          = m_lX;
  bb.y0          = m_lY;
  bb.x1          = -1;
  bb.y1          = -1;
  UC_PIXEL *pPic = m_pat.get();
  for (int y = 0; y < m_lY; y++)
    for (int x = 0; x < m_lX; x++, pPic++)
      if (pPic->m > (UCHAR)0) {
        bb.x0 = std::min(bb.x0, x);
        bb.y0 = std::min(bb.y0, y);
        bb.x1 = std::max(bb.x1, x);
        bb.y1 = std::max(bb.y1, y);
      }
}

void CPattern::optimalizeSize() {
  SRECT bb;

  getBBox(bb);
  if (bb.x0 <= bb.x1 && bb.y0 <= bb.y1) {
    int nLX = bb.x1 - bb.x0 + 1;
    int nLY = bb.y1 - bb.y0 + 1;
    std::unique_ptr<UC_PIXEL[]> nPat(new UC_PIXEL[nLX * nLY]);
    if (!nPat) {
      char s[200];
      snprintf(s, sizeof(s), "in Pattern Optimalization \n");
      throw SMemAllocError(s);
    }
    for (int y = bb.y0; y <= bb.y1; y++)
      for (int x = bb.x0; x <= bb.x1; x++) {
        UC_PIXEL *pPat  = m_pat.get() + y * m_lX + x;
        UC_PIXEL *pNPat = nPat.get() + (y - bb.y0) * nLX + x - bb.x0;
        pNPat->r        = pPat->r;
        pNPat->g        = pPat->g;
        pNPat->b        = pPat->b;
        pNPat->m        = pPat->m;
      }
    m_lX  = nLX;
    m_lY  = nLY;
    m_pat = std::move(nPat);
  }
}

void CPattern::eraseBuffer(const int lX, const int lY, UC_PIXEL *buffer) {
  int xy       = lX * lY;
  UC_PIXEL *pb = buffer;
  for (int i = 0; i < xy; i++, pb++) pb->r = pb->g = pb->b = pb->m = (UCHAR)0;
}

void CPattern::rotate(const double angle) {
  if (m_lX <= 0 || m_lY <= 0 || !m_pat) return;

  double sDiag = sqrt(double(m_lX * m_lX + m_lY * m_lY));
  int nLXY     = (int)sDiag + 5;
  int xx       = nLXY / 2;
  int yy       = xx;
  double co    = cos(-DEG2RAD(angle));
  double si    = sin(-DEG2RAD(angle));

  std::unique_ptr<UC_PIXEL[]> nPat(new UC_PIXEL[nLXY * nLXY]);
  if (!nPat) {
    char s[200];
    snprintf(s, sizeof(s), "in Pattern Rotation \n");
    throw SMemAllocError(s);
  }
  eraseBuffer(nLXY, nLXY, nPat.get());

  UC_PIXEL *pNPat = nPat.get();
  for (int y = 0; y < nLXY; y++)
    for (int x = 0; x < nLXY; x++, pNPat++) {
      UC_PIXEL *pucp = 0;
      getMapPixel(x - xx, y - yy, 1.0, si, co, pucp);
      if (pucp) {
        pNPat->r = pucp->r;
        pNPat->g = pucp->g;
        pNPat->b = pucp->b;
        pNPat->m = pucp->m;
      }
    }
  m_lX = m_lY = nLXY;
  m_pat       = std::move(nPat);

  try {
    optimalizeSize();
  } catch (SMemAllocError) {
    throw;
  }
}
