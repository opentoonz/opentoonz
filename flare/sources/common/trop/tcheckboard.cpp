

#include "trop.h"
#include "tpixelutils.h"

#define FLOOR(x) ((int)(x) > (x) ? (int)(x)-1 : (int)(x))

/*
inline bool isInFirstColor(const TDimensionD &dim,int x, int y,const TPointD
&offset)
{
  int lx=dim.lx;
  int ly=dim.ly;
  int offX,offY;
  offX = offset.x>=0 ? (int)offset.x : (int)(lx-offset.x);
  offY = offset.y>=0 ? (int)offset.y : (int)(ly-offset.y);
  if ((( ((int)(offX+x))/lx + ((int)(offY+y))/ly )%2 ) == 0)
    return false;
  return true;
}
*/

//-----------------------------------------------------------------------------

namespace {

template <typename PIXEL>
void do_checkBoard(TRasterPT<PIXEL> rout, const PIXEL &pix1, const PIXEL &pix2,
                   const TDimensionD &dim, const TPointD &offset) {
  assert(dim.lx > 0);
  assert(dim.ly > 0);

  double freqX = 0.5 / dim.lx;
  double freqY = 0.5 / dim.ly;

  double phaseX = 0;
  if (offset.x >= 0) {
    double q = offset.x * freqX;
    phaseX   = (q - floor(q));
  } else {
    double q = (-offset.x * freqX);
    phaseX   = 1.0 - (q - floor(q));
  }

  double phaseY = 0;
  if (offset.y >= 0) {
    double q = offset.y * freqY;
    phaseY   = (q - floor(q));
  } else {
    double q = (-offset.y * freqY);
    phaseY   = 1.0 - (q - floor(q));
  }

  int lx = rout->getLx();
  int ly = rout->getLy();

  for (int y = 0; y < ly; y++) {
    double yy = 2.0 * (phaseY + y * freqY);
    int iy    = FLOOR(yy);
    assert(iy == (int)floor(yy));
    rout->lock();
    PIXEL *pix = rout->pixels(y);
    for (int x = 0; x < lx; x++) {
      double xx = 2.0 * (phaseX + x * freqX);
      int ix    = FLOOR(xx);
      assert(ix == (int)floor(xx));
      if ((ix ^ iy) & 1)
        *pix++ = pix1;
      else
        *pix++ = pix2;
    }
    rout->unlock();
  }
}

}  // namespace

//-----------------------------------------------------------------------------

void TRop::checkBoard(TRasterP rout, const TPixel32 &pix1, const TPixel32 &pix2,
                      const TDimensionD &dim, const TPointD &offset) {
  // assert(offset.x<=dim.lx && offset.y<=dim.ly);

  TRaster32P rout32 = rout;
  TRaster64P rout64 = rout;
  TRasterFP routF   = rout;
  if (rout32)
    do_checkBoard<TPixel32>(rout32, pix1, pix2, dim, offset);
  else if (rout64)
    do_checkBoard<TPixel64>(rout64, toPixel64(pix1), toPixel64(pix2), dim,
                            offset);
  else if (routF)
    do_checkBoard<TPixelF>(routF, toPixelF(pix1), toPixelF(pix2), dim, offset);
  else
    throw TRopException("unsupported pixel type");
}
