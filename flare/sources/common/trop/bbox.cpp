

#include "trop.h"

namespace {

//-------------------------------------------------------------------

template <typename PIXEL, typename CHANNEL_TYPE>
void computeBBox(TRasterPT<PIXEL> ras, TRect &bbox) {
  bbox   = ras->getBounds();
  int lx = ras->getLx();
  int ly = ras->getLy();

  // se c'e' un pixel opaco in alto a sin e in basso a destra allora bbox =
  // bounds
  if (ras->pixels(0)[0].m != 0 && ras->pixels(ly - 1)[lx - 1].m != 0) return;

  int y;
  ras->lock();
  for (y = 0; y < ly; y++) {
    CHANNEL_TYPE *pix    = &(ras->pixels(y)->m);
    CHANNEL_TYPE *endPix = pix + 4 * lx;
    while (pix < endPix && *pix == 0) pix += 4;
    if (pix < endPix) break;
  }
  if (y == ly) {
    // tutta trasparente
    bbox = TRect();
    ras->unlock();
    return;
  }
  bbox.y0 = y;
  for (y = ly - 1; y > bbox.y0; y--) {
    CHANNEL_TYPE *pix    = &(ras->pixels(y)->m);
    CHANNEL_TYPE *endPix = pix + 4 * lx;
    while (pix < endPix && *pix == 0) pix += 4;
    if (pix < endPix) break;
  }
  bbox.y1 = y;
  assert(bbox.y0 <= bbox.y1);
  bbox.x0 = lx;
  bbox.x1 = -1;
  for (y = bbox.y0; y <= bbox.y1; y++) {
    CHANNEL_TYPE *row = &(ras->pixels(y)->m);
    int x             = 0;
    for (x = 0; x < bbox.x0 && row[x * 4] == 0; x++) {
    }
    bbox.x0 = x;
    for (x = lx - 1; x > bbox.x1 && row[x * 4] == 0; x--) {
    }
    bbox.x1 = x;
  }
  assert(bbox.x0 <= bbox.x1);

  ras->unlock();
  /*

  UCHAR *row_m = &(ras->pixels()->m);
// se c'e' un pixel opaco in alto a sin e in basso a destra allora bbox = bounds
if(row_m[0] && row_m[(ly-1)*wrap4 + (lx-1)*4])
    return;

int y;
UCHAR *m, *max_m, *min_m;

for(y=0;y<ly;y++)
    {
max_m = row_m + lx * 4;
           for(m = row_m; m<max_m && *m==0; m+=4) {}
           if(m<max_m) break;
           row_m += wrap4;
          }
if(y>=ly)
    {
           // tutta trasparente
           return;
          }

  bbox.y0 = bbox.y1 = y;
bbox.x0 = (m - row_m)/4;
assert(0<=bbox.x0 && bbox.x0<lx);
assert(ras->pixels(bbox.y0)[bbox.x0].m>0);
assert(bbox.x0 == 0 || ras->pixels(bbox.y0)[bbox.x0-1].m==0);

min_m = m;
for(m = max_m - 4;m>min_m && *m==0;m-=4) {}
bbox.x1 = (m - row_m)/4;
assert(0<=bbox.x1 && bbox.x1<lx);
assert(ras->pixels(bbox.y0)[bbox.x1].m>0);
assert(bbox.x1 == lx-1 || ras->pixels(bbox.y0)[bbox.x1+1].m==0);

  row_m += wrap4;

for(y++;y<ly;y++)
    {
max_m = row_m + lx * 4;
           for(m = row_m; m<max_m && *m==0; m+=4) {}
           if(m<max_m)
             {
                    int x = (m - row_m)/4;
                          if(x<bbox.x0)
                            {
                                   bbox.x0 = x;
     assert(0<=bbox.x0 && bbox.x0<lx);
     assert(ras->pixels(y)[bbox.x0].m>0);
     assert(bbox.x0 == 0 || ras->pixels(y)[bbox.x0-1].m==0);
    }
  min_m = row_m + bbox.x1*4;
                    for(m = max_m - 4;m>min_m && *m==0;m-=4) {}
                          if(m>min_m)
                            {
                                   x = (m - row_m)/4;
                                   assert(x>bbox.x1);
                                   bbox.x1 = x;
                                  }
                          bbox.y1 = y;
                   }
           row_m += wrap4;
          }
*/
}

//-------------------------------------------------------------------

inline bool isTransparent(TPixelCM32 *pix) {
  return (!pix->isPureInk() &&
          //  (pix->getPaint()==BackgroundStyle
          (pix->getPaint() == 0 && pix->isPurePaint()));
}

//-------------------------------------------------------------------

void computeBBoxCM32(TRasterCM32P ras, TRect &bbox) {
  bbox   = ras->getBounds();
  int lx = ras->getLx();
  int ly = ras->getLy();

  // se c'e' un pixel opaco in alto a sin e in basso a destra allora bbox =
  // bounds
  if (!isTransparent(&(ras->pixels(0)[0])) &&
      !isTransparent(&(ras->pixels(ly - 1)[lx - 1])))
    return;

  int y;
  ras->lock();
  for (y = 0; y < ly; y++) {
    TPixelCM32 *pix    = ras->pixels(y);
    TPixelCM32 *endPix = pix + lx;
    while (pix < endPix && isTransparent(pix)) ++pix;
    if (pix < endPix) break;
  }
  if (y == ly) {
    // tutta trasparente
    bbox = TRect();
    ras->unlock();
    return;
  }
  bbox.y0 = y;
  for (y = ly - 1; y > bbox.y0; y--) {
    TPixelCM32 *pix    = ras->pixels(y);
    TPixelCM32 *endPix = pix + lx;
    while (pix < endPix && isTransparent(pix)) ++pix;
    if (pix < endPix) break;
  }
  bbox.y1 = y;
  assert(bbox.y0 <= bbox.y1);
  bbox.x0 = lx;
  bbox.x1 = -1;
  for (y = bbox.y0; y <= bbox.y1; y++) {
    TPixelCM32 *row = ras->pixels(y);
    int x           = 0;
    for (x = 0; x < bbox.x0 && isTransparent(&row[x]); x++) {
    }
    bbox.x0 = x;
    for (x = lx - 1; x > bbox.x1 && isTransparent(&row[x]); x--) {
    }
    bbox.x1 = x;
  }
  assert(bbox.x0 <= bbox.x1);
  ras->unlock();
}

//-------------------------------------------------------------------

}  // namespace

//-------------------------------------------------------------------

void TRop::computeBBox(TRasterP ras, TRect &bbox) {
  TRaster32P ras32 = ras;
  if (ras32) {
    ::computeBBox<TPixel32, UCHAR>(ras32, bbox);
    return;
  }

  TRaster64P ras64 = ras;
  if (ras64) {
    ::computeBBox<TPixel64, USHORT>(ras64, bbox);
    return;
  }

  TRasterCM32P rasCM32 = ras;
  if (rasCM32) {
    computeBBoxCM32(rasCM32, bbox);
    return;
  }

  TRasterGR8P ras8 = ras;
  if (ras8) {
    bbox = ras->getBounds();
    return;
  }
  assert(0);
}
