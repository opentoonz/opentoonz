

#include "trop.h"
#include "loop_macros.h"
#include "tpixelutils.h"
#include "quickputP.h"

#ifndef TNZCORE_LIGHT
#include "tpalette.h"
#include "tcolorstyles.h"
#endif

/*
#ifndef __sgi
#include <algorithm>
#endif
*/

// The following must be old IRIX code. Should be re-tested.
// It seems that gcc compiles it, but requiring a LOT of
// resources... very suspect...

/*#ifdef __LP64__
#include "optimize_for_lp64.h"
#endif*/

//=============================================================================
//=============================================================================
//=============================================================================

#ifdef OPTIMIZE_FOR_LP64
void quickResample_optimized(const TRasterP &dn, const TRasterP &up,
                             const TAffine &aff,
                             TRop::ResampleFilterType filterType);
#endif

namespace {

inline TPixel32 applyColorScale(const TPixel32 &color,
                                const TPixel32 &colorScale,
                                bool toBePremultiplied = false) {
  /*--
   * Prevent colors from being darkened when quickputting a semi-transparent
   * raster on the Viewer
   * --*/
  if (colorScale.r == 0 && colorScale.g == 0 && colorScale.b == 0) {
    /*--
     * When toBePremultiplied is ON, Premultiply is done later, so it is not
     * done here.
     * --*/
    if (toBePremultiplied)
      return TPixel32(color.r, color.g, color.b, color.m * colorScale.m / 255);
    else
      return TPixel32(
          color.r * colorScale.m / 255, color.g * colorScale.m / 255,
          color.b * colorScale.m / 255, color.m * colorScale.m / 255);
  }
  int r = 255 - (255 - color.r) * (255 - colorScale.r) / 255;
  int g = 255 - (255 - color.g) * (255 - colorScale.g) / 255;
  int b = 255 - (255 - color.b) * (255 - colorScale.b) / 255;
  return premultiply(TPixel32(r, g, b, color.m * colorScale.m / 255));
}

//------------------------------------------------------------------------------

inline TPixel32 applyColorScaleCMapped(const TPixel32 &color,
                                       const TPixel32 &colorScale) {
  int r = 255 - (255 - color.r) * (255 - colorScale.r) / 255;
  int g = 255 - (255 - color.g) * (255 - colorScale.g) / 255;
  int b = 255 - (255 - color.b) * (255 - colorScale.b) / 255;
  return premultiply(TPixel32(r, g, b, color.m * colorScale.m / 255));
}

//------------------------------------------------------------------------------

void doQuickPutFilter(const TRaster32P &dn, const TRaster32P &up,
                      const TAffine &aff) {
  //  se aff e' degenere la controimmagine di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD = TRectD(convert(dn->getSize())) *
                        (aff * TRectD(0, 0, up->getLx() - 2, up->getLy() - 2));
  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel successivo
  //  comporta l'incremento (deltaXD, deltaYD) delle coordinate del pixel
  //  corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  // segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //	naturale predecessore di up->getLx() - 1
  int lxPred = (up->getLx() - 2) * (1 << PADN);

  //	naturale predecessore di up->getLy() - 1
  int lyPred = (up->getLy() - 2) * (1 << PADN);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();
  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel32 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //	(1)    equazione k-parametrica della y-esima
    //               scanline di boundingBoxD:
    //	       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //	(2)    equazione k-parametrica dell'immagine mediante
    //               invAff di (1):
    //	       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //	       k = kMin, ..., kMax
    //               con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente intersecando la (2)
    //  con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff
    //  della porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //        TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //	(xL0, yL0) sono le coordinate di a in versione "TLonghizzata"
    //	0 <= xL0 + k*deltaXL
    //          <= (up->getLx() - 2)*(1<<PADN), 0
    //          <= kMinX
    //          <= kMin
    //          <= k
    //          <= kMax
    //          <= kMaxX
    //          <= (xMax - xMin)
    //
    //	0 <= yL0 + k*deltaYL
    //          <= (up->getLy() - 2)*(1<<PADN), 0
    //          <= kMinY
    //          <= kMin
    //          <= k
    //          <= kMax
    //          <= kMaxY
    //          <= (yMax - yMin)
    int xL0 = tround(a.x * (1 << PADN));  //  xL0 inizializzato
    int yL0 = tround(a.y * (1 << PADN));  //  yL0 inizializzato

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

    //        0 <= xL0 + k*deltaXL <= (up->getLx() - 2)*(1<<PADN)
    //                   <=>
    //        0 <= xL0 + k*deltaXL <= lxPred
    //
    //
    //	0 <= yL0 + k*deltaYL <= (up->getLy() - 2)*(1<<PADN)
    //                   <=>
    //        0 <= yL0 + k*deltaYL <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      //  [a, b] verticale esterno ad up contratto
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      if (lxPred < xL0)  //  [a, b] esterno ad up+(bordo destro)
        continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      if (xL0 < 0)  //  [a, b] esterno ad up contratto
        continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up contratto
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      //  altrimenti usa solo
      //  kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      if (lyPred < yL0)  //  [a, b] esterno ad up contratto
        continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      if (yL0 < 0)  //  [a, b] esterno ad up contratto
        continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clipping su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata"
    //	del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN))
      //  e' approssimato con (xI, yI)
      int xI = xL >> PADN;  //	troncato
      int yI = yL >> PADN;  //	troncato

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      //  (xI, yI)
      TPixel32 *upPix00 = upBasePix + (yI * upWrap + xI);

      //  (xI + 1, yI)
      TPixel32 *upPix10 = upPix00 + 1;

      //  (xI, yI + 1)
      TPixel32 *upPix01 = upPix00 + upWrap;

      //  (xI + 1, yI + 1)
      TPixel32 *upPix11 = upPix00 + upWrap + 1;

      //  filtro bilineare 4 pixels: calcolo dei pesi
      int xWeight1 = (xL & MASKN);
      int xWeight0 = (1 << PADN) - xWeight1;
      int yWeight1 = (yL & MASKN);
      int yWeight0 = (1 << PADN) - yWeight1;

      //  filtro bilineare 4 pixels: media pesata sui singoli canali
      int rColDownTmp =
          (xWeight0 * (upPix00->r) + xWeight1 * ((upPix10)->r)) >> PADN;

      int gColDownTmp =
          (xWeight0 * (upPix00->g) + xWeight1 * ((upPix10)->g)) >> PADN;

      int bColDownTmp =
          (xWeight0 * (upPix00->b) + xWeight1 * ((upPix10)->b)) >> PADN;

      int rColUpTmp =
          (xWeight0 * ((upPix01)->r) + xWeight1 * ((upPix11)->r)) >> PADN;

      int gColUpTmp =
          (xWeight0 * ((upPix01)->g) + xWeight1 * ((upPix11)->g)) >> PADN;

      int bColUpTmp =
          (xWeight0 * ((upPix01)->b) + xWeight1 * ((upPix11)->b)) >> PADN;

      unsigned char rCol =
          (unsigned char)((yWeight0 * rColDownTmp + yWeight1 * rColUpTmp) >>
                          PADN);

      unsigned char gCol =
          (unsigned char)((yWeight0 * gColDownTmp + yWeight1 * gColUpTmp) >>
                          PADN);

      unsigned char bCol =
          (unsigned char)((yWeight0 * bColDownTmp + yWeight1 * bColUpTmp) >>
                          PADN);

      TPixel32 upPix = TPixel32(rCol, gCol, bCol, upPix00->m);

      if (upPix.m == 0)
        continue;
      else if (upPix.m == 255)
        *dnPix = upPix;
      else
        *dnPix = quickOverPix(*dnPix, upPix);
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
//=============================================================================
//=============================================================================

void doQuickPutNoFilter(const TRaster32P &dn, const TRaster32P &up,
                        const TAffine &aff, const TPixel32 &colorScale,
                        bool doPremultiply, bool whiteTransp, bool firstColumn,
                        bool doRasterDarkenBlendedView) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel32 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixel32 upPix = *(upBasePix + (yI * upWrap + xI));

      if (firstColumn) upPix.m = 255;
      if (upPix.m == 0 || (whiteTransp && upPix == TPixel::White)) continue;

      if (colorScale != TPixel32::Black)
        upPix = applyColorScale(upPix, colorScale, doPremultiply);

      if (doRasterDarkenBlendedView)
        *dnPix = quickOverPixDarkenBlended(*dnPix, upPix);
      else {
        if (upPix.m == 255)
          *dnPix = upPix;
        else if (doPremultiply)
          *dnPix = quickOverPixPremult(*dnPix, upPix);
        else
          *dnPix = quickOverPix(*dnPix, upPix);
      }
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
//=============================================================================
//=============================================================================

void doQuickPutNoFilter(const TRaster32P &dn, const TRaster64P &up,
                        const TAffine &aff, bool doPremultiply,
                        bool firstColumn) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel64 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixel64 *upPix = upBasePix + (yI * upWrap + xI);
      if (firstColumn) upPix->m = 65535;
      if (upPix->m == 0)
        continue;
      else if (upPix->m == 65535)
        *dnPix = PixelConverter<TPixel32>::from(*upPix);
      else if (doPremultiply)
        *dnPix =
            quickOverPixPremult(*dnPix, PixelConverter<TPixel32>::from(*upPix));
      else
        *dnPix = quickOverPix(*dnPix, PixelConverter<TPixel32>::from(*upPix));
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================
//=============================================================================

void doQuickPutNoFilter(const TRaster32P &dn, const TRasterGR8P &up,
                        const TAffine &aff, const TPixel32 &colorScale) {
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;
  const int PADN = 16;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  int deltaXL = tround(deltaXD * (1 << PADN));

  int deltaYL = tround(deltaYD * (1 << PADN));

  if ((deltaXL == 0) && (deltaYL == 0)) return;

  int lxPred = up->getLx() * (1 << PADN) - 1;

  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow      = dn->pixels(yMin);
  TPixelGR8 *upBasePix = up->pixels();

  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    TPointD a = invAff * TPointD(xMin, y);

    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    if (deltaXL == 0) {
      if ((xL0 < 0) || (lxPred < xL0)) continue;
    } else if (deltaXL > 0) {
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelGR8 *upPix = upBasePix + (yI * upWrap + xI);
      if (colorScale == TPixel32::Black) {
        if (upPix->value == 0)
          dnPix->r = dnPix->g = dnPix->b = 0;
        else if (upPix->value == 255)
          dnPix->r = dnPix->g = dnPix->b = upPix->value;
        else
          *dnPix = quickOverPix(*dnPix, *upPix);
        dnPix->m = 255;
      } else {
        TPixel32 upPix32(upPix->value, upPix->value, upPix->value, 255);
        upPix32 = applyColorScale(upPix32, colorScale);

        if (upPix32.m == 255)
          *dnPix = upPix32;
        else
          *dnPix = quickOverPix(*dnPix, upPix32);
      }
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================

void doQuickPutNoFilter(const TRaster64P &dn, const TRaster64P &up,
                        const TAffine &aff, bool doPremultiply,
                        bool firstColumn) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel64 *dnRow     = dn->pixels(yMin);
  TPixel64 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel64 *dnPix    = dnRow + xMin + kMin;
    TPixel64 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixel64 upPix = *(upBasePix + (yI * upWrap + xI));

      if (firstColumn) upPix.m = 65535;
      if (upPix.m == 0) continue;

      if (upPix.m == 65535)
        *dnPix = upPix;
      else if (doPremultiply)
        *dnPix = quickOverPixPremult(*dnPix, upPix);
      else
        *dnPix = quickOverPix(*dnPix, upPix);
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================

void doQuickPutNoFilter(const TRaster64P &dn, const TRasterFP &up,
                        const TAffine &aff, bool doPremultiply,
                        bool firstColumn) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel64 *dnRow    = dn->pixels(yMin);
  TPixelF *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel64 *dnPix    = dnRow + xMin + kMin;
    TPixel64 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelF upPix = *(upBasePix + (yI * upWrap + xI));

      if (firstColumn) upPix.m = 1.0;
      if (upPix.m <= 0.0) continue;

      TPixel64 upPix64 = toPixel64(upPix);
      if (upPix.m >= 1.f)
        *dnPix = upPix64;
      else if (doPremultiply)
        *dnPix = quickOverPixPremult(*dnPix, upPix64);
      else
        *dnPix = quickOverPix(*dnPix, upPix64);
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================

void doQuickPutNoFilter(const TRasterFP &dn, const TRasterFP &up,
                        const TAffine &aff, bool doPremultiply,
                        bool firstColumn) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixelF *dnRow     = dn->pixels(yMin);
  TPixelF *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixelF *dnPix    = dnRow + xMin + kMin;
    TPixelF *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelF upPix = *(upBasePix + (yI * upWrap + xI));

      if (firstColumn) upPix.m = 1.0;
      if (upPix.m <= 0.0) continue;

      if (upPix.m >= 1.f)
        *dnPix = upPix;
      else if (doPremultiply)
        *dnPix = quickOverPixPremult(*dnPix, upPix);
      else
        *dnPix = quickOverPix(*dnPix, upPix);
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================

void doQuickPutNoFilter(const TRaster64P &dn, const TRaster32P &up,
                        const TAffine &aff, bool doPremultiply,
                        bool firstColumn) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel64 *dnRow     = dn->pixels(yMin);
  TPixel32 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel64 *dnPix    = dnRow + xMin + kMin;
    TPixel64 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixel32 upPix = *(upBasePix + (yI * upWrap + xI));

      if (firstColumn) upPix.m = 255;
      if (upPix.m == 0) continue;

      TPixel64 upPix64 = toPixel64(upPix);
      if (upPix.m == 255)
        *dnPix = upPix64;
      else if (doPremultiply)
        *dnPix = quickOverPixPremult(*dnPix, upPix64);
      else
        *dnPix = quickOverPix(*dnPix, upPix64);
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================

void doQuickPutFilter(const TRaster32P &dn, const TRaster32P &up, double sx,
                      double sy, double tx, double ty) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;

  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD = TRectD(convert(dn->getSize())) *
                        (aff * TRectD(0, 0, up->getLx() - 2, up->getLy() - 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //	nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //    successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //    pixels corrispondenti di up

  //    nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //    successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //    pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //	(1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //	       (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //	         kX = 0, ..., (xMax - xMin),
  //             kY = 0, ..., (yMax - yMin)

  //	(2)  equazione (kX, kY)-parametrica dell'immagine
  //         mediante invAff di (1):
  //	       invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //	         kX = kMinX, ..., kMaxX
  //               con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //
  //	         kY = kMinY, ..., kMaxY
  //               con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up (con gli estremi eventualmente invertiti) e'
  //  la controimmagine
  //  mediante aff della porzione di scanline  [ (xMin, yMin), (xMax, yMin) ]
  //  di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round) in
  //  versione "TLonghizzata"
  //
  //    0 <= xL0 + kX*deltaXL
  //      <= (up->getLx() - 2)*(1<<PADN),
  //    0 <= kMinX <= kX
  //      <= kMaxX <= (xMax - xMin)
  //
  //	0 <= yL0 + kY*deltaYL
  //      <= (up->getLy() - 2)*(1<<PADN),
  //    0 <= kMinY <= kY
  //      <= kMaxY <= (yMax - yMin)
  int xL0 = tround(a.x * (1 << PADN));  //  xL0 inizializzato
  int yL0 = tround(a.y * (1 << PADN));  //  yL0 inizializzato

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati
  //  di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

  //  TINT32 predecessore di (up->getLx() - 1)
  int lxPred = (up->getLx() - 2) * (1 << PADN);

  //  TINT32 predecessore di (up->getLy() - 1)
  int lyPred = (up->getLy() - 2) * (1 << PADN);

  //  0 <= xL0 + k*deltaXL
  //    <= (up->getLx() - 2)*(1<<PADN)
  //             <=>
  //  0 <= xL0 + k*deltaXL <= lxPred
  //
  //  0 <= yL0 + k*deltaYL
  //    <= (up->getLy() - 2)*(1<<PADN)
  //             <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY intersecando la (2) con
  //  i lati (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(yL0 <= lyPred);
    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //	calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX intersecando la (2) con
  //  i lati (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *upBasePix = up->pixels();
  TPixel32 *dnRow     = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata" del pixel corrente di up

  //  inizializza yL
  int yL = yL0 + (kMinY - 1) * deltaYL;

  //  scorre le scanline di boundingBoxD
  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    //  inizializza xL
    int xL = xL0 + (kMinX - 1) * deltaXL;
    yL += deltaYL;
    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  troncato

    //  filtro bilineare 4 pixels: calcolo degli y-pesi
    int yWeight1 = (yL & MASKN);
    int yWeight0 = (1 << PADN) - yWeight1;

    TPixel32 *dnPix    = dnRow + xMin + kMinX;
    TPixel32 *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  troncato

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      //  (xI, yI)
      TPixel32 *upPix00 = upBasePix + (yI * upWrap + xI);

      //  (xI + 1, yI)
      TPixel32 *upPix10 = upPix00 + 1;

      //  (xI, yI + 1)
      TPixel32 *upPix01 = upPix00 + upWrap;

      //  (xI + 1, yI + 1)
      TPixel32 *upPix11 = upPix00 + upWrap + 1;

      //  filtro bilineare 4 pixels: calcolo degli x-pesi
      int xWeight1 = (xL & MASKN);
      int xWeight0 = (1 << PADN) - xWeight1;

      //  filtro bilineare 4 pixels: media pesata sui singoli canali
      int rColDownTmp =
          (xWeight0 * (upPix00->r) + xWeight1 * ((upPix10)->r)) >> PADN;

      int gColDownTmp =
          (xWeight0 * (upPix00->g) + xWeight1 * ((upPix10)->g)) >> PADN;

      int bColDownTmp =
          (xWeight0 * (upPix00->b) + xWeight1 * ((upPix10)->b)) >> PADN;

      int rColUpTmp =
          (xWeight0 * ((upPix01)->r) + xWeight1 * ((upPix11)->r)) >> PADN;

      int gColUpTmp =
          (xWeight0 * ((upPix01)->g) + xWeight1 * ((upPix11)->g)) >> PADN;

      int bColUpTmp =
          (xWeight0 * ((upPix01)->b) + xWeight1 * ((upPix11)->b)) >> PADN;

      unsigned char rCol =
          (unsigned char)((yWeight0 * rColDownTmp + yWeight1 * rColUpTmp) >>
                          PADN);

      unsigned char gCol =
          (unsigned char)((yWeight0 * gColDownTmp + yWeight1 * gColUpTmp) >>
                          PADN);

      unsigned char bCol =
          (unsigned char)((yWeight0 * bColDownTmp + yWeight1 * bColUpTmp) >>
                          PADN);

      TPixel32 upPix = TPixel32(rCol, gCol, bCol, upPix00->m);

      if (upPix.m == 0)
        continue;
      else if (upPix.m == 255)
        *dnPix = upPix;
      else
        *dnPix = quickOverPix(*dnPix, upPix);
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================
//=============================================================================
void doQuickPutNoFilter(const TRaster32P &dn, const TRaster32P &up, double sx,
                        double sy, double tx, double ty,
                        const TPixel32 &colorScale, bool doPremultiply,
                        bool whiteTransp, bool firstColumn,
                        bool doRasterDarkenBlendedView) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));
  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  // disponibili per la parte intera di xL, yL)

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //  successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //  pixels corrispondenti di up

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //  pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //	(1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //	       (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //	         kX = 0, ..., (xMax - xMin),
  //             kY = 0, ..., (yMax - yMin)

  //	(2)  equazione (kX, kY)-parametrica dell'immagine
  //         mediante invAff di (1):
  //	       invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //	         kX = kMinX, ..., kMaxX
  //             con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //
  //             kY = kMinY, ..., kMaxY
  //             con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up e' la controimmagine mediante aff della
  // porzione di scanline  [ (xMin, yMin), (xMax, yMin) ] di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
  // in versione "TLonghizzata"
  //	0 <= xL0 + kX*deltaXL
  //      < up->getLx()*(1<<PADN),
  //
  //    0 <= kMinX
  //      <= kX
  //      <= kMaxX
  //      <= (xMax - xMin)

  //	0 <= yL0 + kY*deltaYL
  //      < up->getLy()*(1<<PADN),
  //
  //    0 <= kMinY
  //      <= kY
  //      <= kMaxY
  //      <= (yMax - yMin)

  //  xL0 inizializzato per il round
  int xL0 = tround((a.x + 0.5) * (1 << PADN));

  //  yL0 inizializzato per il round
  int yL0 = tround((a.y + 0.5) * (1 << PADN));

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  //  0 <= xL0 + k*deltaXL < up->getLx()*(1<<PADN)
  //            <=>
  //  0 <= xL0 + k*deltaXL <= lxPred

  //  0 <= yL0 + k*deltaYL < up->getLy()*(1<<PADN)
  //            <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY intersecando la (2) con i lati
  //  (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(yL0 <= lyPred);

    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //  calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX intersecando la (2) con i lati
  //  (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *upBasePix = up->pixels();
  TPixel32 *dnRow     = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata" del pixel corrente di up

  //  inizializza yL
  int yL = yL0 + (kMinY - 1) * deltaYL;

  //  scorre le scanline di boundingBoxD
  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    //  inizializza xL
    int xL = xL0 + (kMinX - 1) * deltaXL;
    yL += deltaYL;

    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  round

    TPixel32 *dnPix    = dnRow + xMin + kMinX;
    TPixel32 *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //	round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixel32 upPix = *(upBasePix + (yI * upWrap + xI));

      if (firstColumn) upPix.m = 255;

      if (upPix.m == 0 || (whiteTransp && upPix == TPixel::White)) continue;

      if (colorScale != TPixel32::Black)
        upPix = applyColorScale(upPix, colorScale, doPremultiply);

      if (doRasterDarkenBlendedView)
        *dnPix = quickOverPixDarkenBlended(*dnPix, upPix);
      else {
        if (upPix.m == 255)
          *dnPix = upPix;
        else if (doPremultiply)
          *dnPix = quickOverPixPremult(*dnPix, upPix);
        else
          *dnPix = quickOverPix(*dnPix, upPix);
      }
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
void doQuickPutNoFilter(const TRaster32P &dn, const TRasterGR8P &up, double sx,
                        double sy, double tx, double ty,
                        const TPixel32 &colorScale) {
  if ((sx == 0) || (sy == 0)) return;

  const int PADN = 16;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;
  int deltaXL    = tround(deltaXD * (1 << PADN));
  int deltaYL    = tround(deltaYD * (1 << PADN));
  if ((deltaXL == 0) || (deltaYL == 0)) return;
  TPointD a = invAff * TPointD(xMin, yMin);

  int xL0   = tround((a.x + 0.5) * (1 << PADN));
  int yL0   = tround((a.y + 0.5) * (1 << PADN));
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn
  int lxPred = up->getLx() * (1 << PADN) - 1;
  int lyPred = up->getLy() * (1 << PADN) - 1;

  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    assert(yL0 <= lyPred);

    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixelGR8 *upBasePix = up->pixels();
  TPixel32 *dnRow      = dn->pixels(yMin + kMinY);

  int yL = yL0 + (kMinY - 1) * deltaYL;

  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    //  inizializza xL
    int xL = xL0 + (kMinX - 1) * deltaXL;
    yL += deltaYL;

    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  round

    TPixel32 *dnPix    = dnRow + xMin + kMinX;
    TPixel32 *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      int xI = xL >> PADN;  //	round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelGR8 *upPix = upBasePix + (yI * upWrap + xI);
      if (colorScale == TPixel32::Black) {
        dnPix->r = dnPix->g = dnPix->b = upPix->value;
        dnPix->m                       = 255;
      } else {
        TPixel32 upPix32(upPix->value, upPix->value, upPix->value, 255);
        upPix32 = applyColorScale(upPix32, colorScale);

        if (upPix32.m == 255)
          *dnPix = upPix32;
        else
          *dnPix = quickOverPix(*dnPix, upPix32);
      }

      /*
if (upPix->value == 0)
dnPix->r = dnPix->g = dnPix->b = dnPix->m = upPix->value;
else if (upPix->value == 255)
dnPix->r = dnPix->g = dnPix->b = dnPix->m = upPix->value;
else
*dnPix = quickOverPix(*dnPix, *upPix);
*/
    }
  }
  dn->unlock();
  up->unlock();
}

void doQuickResampleFilter(const TRaster32P &dn, const TRaster32P &up,
                           const TAffine &aff) {
  //  se aff e' degenere la controimmagine di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));
  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)

  TRectD boundingBoxD = TRectD(convert(dn->getSize())) *
                        (aff * TRectD(0, 0, up->getLx() - 2, up->getLy() - 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate
  //  del pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  // naturale predecessore di up->getLx() - 1
  int lxPred = (up->getLx() - 2) * (1 << PADN);

  //  naturale predecessore di up->getLy() - 1
  int lyPred = (up->getLy() - 2) * (1 << PADN);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel32 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //         (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)
    //
    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //         invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //           k = kMin, ..., kMax
    //           con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente intersecando
    //  la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <= (up->getLx() - 2)*(1<<PADN),
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)

    //  0 <= yL0 + k*deltaYL
    //    <= (up->getLy() - 2)*(1<<PADN),
    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato
    int xL0 = tround(a.x * (1 << PADN));

    //  yL0 inizializzato
    int yL0 = tround(a.y * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL <= (up->getLx() - 2)*(1<<PADN)
    //             <=>
    //  0 <= xL0 + k*deltaXL <= lxPred

    //  0 <= yL0 + k*deltaYL <= (up->getLy() - 2)*(1<<PADN)
    //             <=>
    //  0 <= yL0 + k*deltaYL <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      //  [a, b] verticale esterno ad up contratto
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up contratto
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      // [a, b] orizzontale esterno ad up contratto
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      //  altrimenti usa solo
      //  kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up contratto
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up contratto
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "longhizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //	troncato
      int yI = yL >> PADN;  //	troncato

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      //  (xI, yI)
      TPixel32 *upPix00 = upBasePix + (yI * upWrap + xI);

      //  (xI + 1, yI)
      TPixel32 *upPix10 = upPix00 + 1;

      //  (xI, yI + 1)
      TPixel32 *upPix01 = upPix00 + upWrap;

      //  (xI + 1, yI + 1)
      TPixel32 *upPix11 = upPix00 + upWrap + 1;

      //  filtro bilineare 4 pixels: calcolo dei pesi
      int xWeight1 = (xL & MASKN);
      int xWeight0 = (1 << PADN) - xWeight1;
      int yWeight1 = (yL & MASKN);
      int yWeight0 = (1 << PADN) - yWeight1;

      //  filtro bilineare 4 pixels: media pesata sui singoli canali
      int rColDownTmp =
          (xWeight0 * (upPix00->r) + xWeight1 * ((upPix10)->r)) >> PADN;

      int gColDownTmp =
          (xWeight0 * (upPix00->g) + xWeight1 * ((upPix10)->g)) >> PADN;

      int bColDownTmp =
          (xWeight0 * (upPix00->b) + xWeight1 * ((upPix10)->b)) >> PADN;

      int mColDownTmp =
          (xWeight0 * (upPix00->m) + xWeight1 * ((upPix10)->m)) >> PADN;

      int rColUpTmp =
          (xWeight0 * ((upPix01)->r) + xWeight1 * ((upPix11)->r)) >> PADN;

      int gColUpTmp =
          (xWeight0 * ((upPix01)->g) + xWeight1 * ((upPix11)->g)) >> PADN;

      int bColUpTmp =
          (xWeight0 * ((upPix01)->b) + xWeight1 * ((upPix11)->b)) >> PADN;

      int mColUpTmp =
          (xWeight0 * ((upPix01)->m) + xWeight1 * ((upPix11)->m)) >> PADN;

      dnPix->r =
          (unsigned char)((yWeight0 * rColDownTmp + yWeight1 * rColUpTmp) >>
                          PADN);
      dnPix->g =
          (unsigned char)((yWeight0 * gColDownTmp + yWeight1 * gColUpTmp) >>
                          PADN);
      dnPix->b =
          (unsigned char)((yWeight0 * bColDownTmp + yWeight1 * bColUpTmp) >>
                          PADN);
      dnPix->m =
          (unsigned char)((yWeight0 * mColDownTmp + yWeight1 * mColUpTmp) >>
                          PADN);
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================
//=============================================================================

void doQuickResampleFilter(const TRaster32P &dn, const TRasterGR8P &up,
                           const TAffine &aff) {
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  const int PADN = 16;

  const int MASKN = (1 << PADN) - 1;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD = TRectD(convert(dn->getSize())) *
                        (aff * TRectD(0, 0, up->getLx() - 2, up->getLy() - 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;
  int deltaXL    = tround(deltaXD * (1 << PADN));
  int deltaYL    = tround(deltaYD * (1 << PADN));
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  int lxPred = (up->getLx() - 2) * (1 << PADN);
  int lyPred = (up->getLy() - 2) * (1 << PADN);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow      = dn->pixels(yMin);
  TPixelGR8 *upBasePix = up->pixels();

  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    TPointD a = invAff * TPointD(xMin, y);
    int xL0   = tround(a.x * (1 << PADN));
    int yL0   = tround(a.y * (1 << PADN));
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    if (deltaXL == 0) {
      if ((xL0 < 0) || (lxPred < xL0)) continue;
    } else if (deltaXL > 0) {
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else {
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    if (deltaYL == 0) {
      if ((yL0 < 0) || (lyPred < yL0)) continue;
    } else if (deltaYL > 0) {
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      int xI = xL >> PADN;  //	troncato
      int yI = yL >> PADN;  //	troncato

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      //  (xI, yI)
      TPixelGR8 *upPix00 = upBasePix + (yI * upWrap + xI);

      //  (xI + 1, yI)
      TPixelGR8 *upPix10 = upPix00 + 1;

      //  (xI, yI + 1)
      TPixelGR8 *upPix01 = upPix00 + upWrap;

      //  (xI + 1, yI + 1)
      TPixelGR8 *upPix11 = upPix00 + upWrap + 1;

      //  filtro bilineare 4 pixels: calcolo dei pesi
      int xWeight1 = (xL & MASKN);
      int xWeight0 = (1 << PADN) - xWeight1;
      int yWeight1 = (yL & MASKN);
      int yWeight0 = (1 << PADN) - yWeight1;

      //  filtro bilineare 4 pixels: media pesata sui singoli canali
      int colDownTmp =
          (xWeight0 * (upPix00->value) + xWeight1 * ((upPix10)->value)) >> PADN;

      int colUpTmp =
          (xWeight0 * ((upPix01)->value) + xWeight1 * ((upPix11)->value)) >>
          PADN;

      dnPix->r = dnPix->g = dnPix->b =
          (unsigned char)((yWeight0 * colDownTmp + yWeight1 * colUpTmp) >>
                          PADN);

      dnPix->m = 255;
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================
//=============================================================================

void doQuickResampleColorFilter(const TRaster32P &dn, const TRaster32P &up,
                                const TAffine &aff, UCHAR colorMask) {
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;
  const int PADN = 16;

  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);  //  clipping y su dn
  int yMax =
      std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);  //  clipping y su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);        //  clipping x su dn
  int xMax =
      std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);  //  clipping x su dn

  TAffine invAff = inv(aff);  //  inversa di aff

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  int lxPred =
      up->getLx() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLx()
  int lyPred =
      up->getLy() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLy()

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel32 *upBasePix = up->pixels();

  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    TPointD a = invAff * TPointD(xMin, y);
    int xL0   = tround((a.x + 0.5) * (1 << PADN));
    int yL0   = tround((a.y + 0.5) * (1 << PADN));
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn
    if (deltaXL == 0) {
      if ((xL0 < 0) || (lxPred < xL0)) continue;
    } else if (deltaXL > 0) {
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;                       //  floor
      if (xL0 < 0) kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    } else                                                    //  (deltaXL < 0)
    {
      if (xL0 < 0) continue;
      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0)
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
    if (deltaYL == 0) {
      if ((yL0 < 0) || (lyPred < yL0)) continue;
    } else if (deltaYL > 0) {
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;                       //  floor
      if (yL0 < 0) kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    } else                                                    //  (deltaYL < 0)
    {
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0)
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
    int kMin           = std::max({kMinX, kMinY, (int)0});
    int kMax           = std::min({kMaxX, kMaxY, xMax - xMin});
    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;
    int xL             = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL             = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      if (colorMask == TRop::MChan)
        dnPix->r = dnPix->g = dnPix->b = (upBasePix + (yI * upWrap + xI))->m;
      else {
        TPixel32 *pix = upBasePix + (yI * upWrap + xI);
        dnPix->r      = ((colorMask & TRop::RChan) ? pix->r : 0);
        dnPix->g      = ((colorMask & TRop::GChan) ? pix->g : 0);
        dnPix->b      = ((colorMask & TRop::BChan) ? pix->b : 0);
      }
      dnPix->m = 255;
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================
//=============================================================================

//=============================================================================
//=============================================================================
//=============================================================================

void doQuickResampleColorFilter(const TRaster32P &dn, const TRaster64P &up,
                                const TAffine &aff, UCHAR colorMask) {
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;
  const int PADN = 16;

  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);  //  clipping y su dn
  int yMax =
      std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);  //  clipping y su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);        //  clipping x su dn
  int xMax =
      std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);  //  clipping x su dn

  TAffine invAff = inv(aff);  //  inversa di aff

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  int lxPred =
      up->getLx() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLx()
  int lyPred =
      up->getLy() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLy()

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel64 *upBasePix = up->pixels();

  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    TPointD a = invAff * TPointD(xMin, y);
    int xL0   = tround((a.x + 0.5) * (1 << PADN));
    int yL0   = tround((a.y + 0.5) * (1 << PADN));
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn
    if (deltaXL == 0) {
      if ((xL0 < 0) || (lxPred < xL0)) continue;
    } else if (deltaXL > 0) {
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;                       //  floor
      if (xL0 < 0) kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    } else                                                    //  (deltaXL < 0)
    {
      if (xL0 < 0) continue;
      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0)
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
    if (deltaYL == 0) {
      if ((yL0 < 0) || (lyPred < yL0)) continue;
    } else if (deltaYL > 0) {
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;                       //  floor
      if (yL0 < 0) kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    } else                                                    //  (deltaYL < 0)
    {
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0)
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
    int kMin           = std::max({kMinX, kMinY, (int)0});
    int kMax           = std::min({kMaxX, kMaxY, xMax - xMin});
    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;
    int xL             = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL             = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      if (colorMask == TRop::MChan)
        dnPix->r = dnPix->g = dnPix->b =
            byteFromUshort((upBasePix + (yI * upWrap + xI))->m);
      else {
        TPixel64 *pix = upBasePix + (yI * upWrap + xI);
        dnPix->r = byteFromUshort(((colorMask & TRop::RChan) ? pix->r : 0));
        dnPix->g = byteFromUshort(((colorMask & TRop::GChan) ? pix->g : 0));
        dnPix->b = byteFromUshort(((colorMask & TRop::BChan) ? pix->b : 0));
      }
      dnPix->m = 255;
    }
  }
  dn->unlock();
  up->unlock();
}
//=============================================================================
//=============================================================================
//=============================================================================

void doQuickResampleFilter(const TRaster32P &dn, const TRaster32P &up,
                           double sx, double sy, double tx, double ty) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  // disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD =
      TRectD(convert(dn->getSize())) *
      (aff * TRectD(0, 0, up->getLx() - /*1*/ 2, up->getLy() - /*1*/ 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //  successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //  pixels corrispondenti di up

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //  pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e'
  //  un segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //  (1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //         (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //           kX = 0, ..., (xMax - xMin),
  //           kY = 0, ..., (yMax - yMin)

  //  (2)  equazione (kX, kY)-parametrica dell'immagine mediante invAff di (1):
  //         invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //           kX = kMinX, ..., kMaxX
  //             con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //
  //           kY = kMinY, ..., kMaxY
  //             con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up (con gli estremi eventualmente invertiti) e'
  //  la controimmagine mediante aff della porzione di scanline
  //  [ (xMin, yMin), (xMax, yMin) ] di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  0 <= xL0 + kX*deltaXL <= (up->getLx() - 2)*(1<<PADN),
  //  0 <= kMinX <= kX <= kMaxX <= (xMax - xMin)
  //  0 <= yL0 + kY*deltaYL <= (up->getLy() - 2)*(1<<PADN),
  //  0 <= kMinY <= kY <= kMaxY <= (yMax - yMin)

  int xL0 = tround(a.x * (1 << PADN));  //  xL0 inizializzato
  int yL0 = tround(a.y * (1 << PADN));  //  yL0 inizializzato

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

  //  TINT32 predecessore di (up->getLx() - 1)
  int lxPred = (up->getLx() - /*1*/ 2) * (1 << PADN);

  //  TINT32 predecessore di (up->getLy() - 1)
  int lyPred = (up->getLy() - /*1*/ 2) * (1 << PADN);

  //  0 <= xL0 + k*deltaXL <= (up->getLx() - 2)*(1<<PADN)
  //               <=>
  //  0 <= xL0 + k*deltaXL <= lxPred

  //  0 <= yL0 + k*deltaYL <= (up->getLy() - 2)*(1<<PADN)
  //               <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY intersecando la (2) con i lati
  //  (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(yL0 <= lyPred);

    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //  calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX intersecando la (2) con i lati
  //  (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();

  dn->lock();
  up->lock();
  TPixel32 *upBasePix = up->pixels();
  TPixel32 *dnRow     = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  del pixel corrente di up
  int yL = yL0 + (kMinY - 1) * deltaYL;  //  inizializza yL

  //  scorre le scanline di boundingBoxD
  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    int xL = xL0 + (kMinX - 1) * deltaXL;  //  inizializza xL
    yL += deltaYL;
    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  troncato

    //  filtro bilineare 4 pixels: calcolo degli y-pesi
    int yWeight1 = (yL & MASKN);
    int yWeight0 = (1 << PADN) - yWeight1;

    TPixel32 *dnPix    = dnRow + xMin + kMinX;
    TPixel32 *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  troncato

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      //  (xI, yI)
      TPixel32 *upPix00 = upBasePix + (yI * upWrap + xI);

      //  (xI + 1, yI)
      TPixel32 *upPix10 = upPix00 + 1;

      //  (xI, yI + 1)
      TPixel32 *upPix01 = upPix00 + upWrap;

      //  (xI + 1, yI + 1)
      TPixel32 *upPix11 = upPix00 + upWrap + 1;

      //  filtro bilineare 4 pixels: calcolo degli x-pesi
      int xWeight1 = (xL & MASKN);
      int xWeight0 = (1 << PADN) - xWeight1;

      //  filtro bilineare 4 pixels: media pesata sui singoli canali
      int rColDownTmp =
          (xWeight0 * (upPix00->r) + xWeight1 * ((upPix10)->r)) >> PADN;

      int gColDownTmp =
          (xWeight0 * (upPix00->g) + xWeight1 * ((upPix10)->g)) >> PADN;

      int bColDownTmp =
          (xWeight0 * (upPix00->b) + xWeight1 * ((upPix10)->b)) >> PADN;

      int mColDownTmp =
          (xWeight0 * (upPix00->m) + xWeight1 * ((upPix10)->m)) >> PADN;

      int rColUpTmp =
          (xWeight0 * ((upPix01)->r) + xWeight1 * ((upPix11)->r)) >> PADN;

      int gColUpTmp =
          (xWeight0 * ((upPix01)->g) + xWeight1 * ((upPix11)->g)) >> PADN;

      int bColUpTmp =
          (xWeight0 * ((upPix01)->b) + xWeight1 * ((upPix11)->b)) >> PADN;

      int mColUpTmp =
          (xWeight0 * ((upPix01)->m) + xWeight1 * ((upPix11)->m)) >> PADN;

      dnPix->r =
          (unsigned char)((yWeight0 * rColDownTmp + yWeight1 * rColUpTmp) >>
                          PADN);
      dnPix->g =
          (unsigned char)((yWeight0 * gColDownTmp + yWeight1 * gColUpTmp) >>
                          PADN);
      dnPix->b =
          (unsigned char)((yWeight0 * bColDownTmp + yWeight1 * bColUpTmp) >>
                          PADN);
      dnPix->m =
          (unsigned char)((yWeight0 * mColDownTmp + yWeight1 * mColUpTmp) >>
                          PADN);
    }
  }
  dn->unlock();
  up->unlock();
}

//------------------------------------------------------------------------------------------

void doQuickResampleFilter(const TRaster32P &dn, const TRasterGR8P &up,
                           double sx, double sy, double tx, double ty) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  // disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD =
      TRectD(convert(dn->getSize())) *
      (aff * TRectD(0, 0, up->getLx() - /*1*/ 2, up->getLy() - /*1*/ 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //  successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //  pixels corrispondenti di up

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //  pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e'
  //  un segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //  (1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //         (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //           kX = 0, ..., (xMax - xMin),
  //           kY = 0, ..., (yMax - yMin)

  //  (2)  equazione (kX, kY)-parametrica dell'immagine mediante invAff di (1):
  //         invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //           kX = kMinX, ..., kMaxX
  //             con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //
  //           kY = kMinY, ..., kMaxY
  //             con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up (con gli estremi eventualmente invertiti) e'
  //  la controimmagine mediante aff della porzione di scanline
  //  [ (xMin, yMin), (xMax, yMin) ] di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  0 <= xL0 + kX*deltaXL <= (up->getLx() - 2)*(1<<PADN),
  //  0 <= kMinX <= kX <= kMaxX <= (xMax - xMin)
  //  0 <= yL0 + kY*deltaYL <= (up->getLy() - 2)*(1<<PADN),
  //  0 <= kMinY <= kY <= kMaxY <= (yMax - yMin)

  int xL0 = tround(a.x * (1 << PADN));  //  xL0 inizializzato
  int yL0 = tround(a.y * (1 << PADN));  //  yL0 inizializzato

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

  //  TINT32 predecessore di (up->getLx() - 1)
  int lxPred = (up->getLx() - /*1*/ 2) * (1 << PADN);

  //  TINT32 predecessore di (up->getLy() - 1)
  int lyPred = (up->getLy() - /*1*/ 2) * (1 << PADN);

  //  0 <= xL0 + k*deltaXL <= (up->getLx() - 2)*(1<<PADN)
  //               <=>
  //  0 <= xL0 + k*deltaXL <= lxPred

  //  0 <= yL0 + k*deltaYL <= (up->getLy() - 2)*(1<<PADN)
  //               <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY intersecando la (2) con i lati
  //  (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(yL0 <= lyPred);

    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //  calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX intersecando la (2) con i lati
  //  (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();

  dn->lock();
  up->lock();
  TPixelGR8 *upBasePix = up->pixels();
  TPixel32 *dnRow      = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  del pixel corrente di up
  int yL = yL0 + (kMinY - 1) * deltaYL;  //  inizializza yL

  //  scorre le scanline di boundingBoxD
  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    int xL = xL0 + (kMinX - 1) * deltaXL;  //  inizializza xL
    yL += deltaYL;
    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  troncato

    //  filtro bilineare 4 pixels: calcolo degli y-pesi
    int yWeight1 = (yL & MASKN);
    int yWeight0 = (1 << PADN) - yWeight1;

    TPixel32 *dnPix    = dnRow + xMin + kMinX;
    TPixel32 *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  troncato

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      //  (xI, yI)
      TPixelGR8 *upPix00 = upBasePix + (yI * upWrap + xI);

      //  (xI + 1, yI)
      TPixelGR8 *upPix10 = upPix00 + 1;

      //  (xI, yI + 1)
      TPixelGR8 *upPix01 = upPix00 + upWrap;

      //  (xI + 1, yI + 1)
      TPixelGR8 *upPix11 = upPix00 + upWrap + 1;

      //  filtro bilineare 4 pixels: calcolo degli x-pesi
      int xWeight1 = (xL & MASKN);
      int xWeight0 = (1 << PADN) - xWeight1;

      //  filtro bilineare 4 pixels: media pesata sui singoli canali
      int colDownTmp =
          (xWeight0 * (upPix00->value) + xWeight1 * (upPix10->value)) >> PADN;

      int colUpTmp =
          (xWeight0 * ((upPix01)->value) + xWeight1 * (upPix11->value)) >> PADN;

      dnPix->m = 255;
      dnPix->r = dnPix->g = dnPix->b =
          (unsigned char)((yWeight0 * colDownTmp + yWeight1 * colUpTmp) >>
                          PADN);
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
//=============================================================================
//=============================================================================
template <typename PIX>
void doQuickResampleNoFilter(const TRasterPT<PIX> &dn, const TRasterPT<PIX> &up,
                             double sx, double sy, double tx, double ty) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));
  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //  successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //  pixels corrispondenti di up

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //  pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //  (1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //         (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //           kX = 0, ..., (xMax - xMin),
  //           kY = 0, ..., (yMax - yMin)

  //  (2)  equazione (kX, kY)-parametrica dell'immagine mediante invAff di (1):
  //         invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //           kX = kMinX, ..., kMaxX
  //             con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //           kY = kMinY, ..., kMaxY
  //             con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up e' la controimmagine mediante aff della
  //  porzione di scanline  [ (xMin, yMin), (xMax, yMin) ] di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  0 <= xL0 + kX*deltaXL < up->getLx()*(1<<PADN),
  //  0 <= kMinX <= kX <= kMaxX <= (xMax - xMin)

  //  0 <= yL0 + kY*deltaYL < up->getLy()*(1<<PADN),
  //  0 <= kMinY <= kY <= kMaxY <= (yMax - yMin)
  int xL0 =
      tround((a.x + 0.5) * (1 << PADN));  //  xL0 inizializzato per il round
  int yL0 =
      tround((a.y + 0.5) * (1 << PADN));  //  yL0 inizializzato per il round

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn
  int lxPred =
      up->getLx() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLx()
  int lyPred =
      up->getLy() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLy()

  //  0 <= xL0 + k*deltaXL < up->getLx()*(1<<PADN)
  //                  <=>
  //  0 <= xL0 + k*deltaXL <= lxPred

  //  0 <= yL0 + k*deltaYL < up->getLy()*(1<<PADN)
  //                  <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY  intersecando la (2) con i lati
  //  (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    assert(yL0 <= lyPred);  //  [a, b] interno ad up+(bordo destro/basso)
    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //	calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX  intersecando la (2) con i lati
  //  (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  PIX *upBasePix = up->pixels();
  PIX *dnRow     = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata" del pixel corrente di up
  int yL = yL0 + (kMinY - 1) * deltaYL;  //  inizializza yL

  //  scorre le scanline di boundingBoxD
  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    int xL = xL0 + (kMinX - 1) * deltaXL;  //  inizializza xL
    yL += deltaYL;
    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  round

    PIX *dnPix    = dnRow + xMin + kMinX;
    PIX *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      *dnPix = *(upBasePix + (yI * upWrap + xI));
    }
  }

  dn->unlock();
  up->unlock();
}

//=============================================================================
//=============================================================================
//=============================================================================

#ifndef TNZCORE_LIGHT

//=============================================================================
//=============================================================================
//=============================================================================
//=============================================================================
//
// doQuickPutCmapped
//
//=============================================================================

void doQuickPutCmapped(const TRaster32P &dn, const TRasterCM32P &up,
                       const TPaletteP &palette, const TAffine &aff,
                       const TPixel32 &globalColorScale, bool inksOnly) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate del
  //  pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();

  std::vector<TPixel32> colors(palette->getStyleCount());
  // vector<TPixel32> inks(palette->getStyleCount());

  if (globalColorScale != TPixel::Black)
    for (int i = 0; i < palette->getStyleCount(); i++)
      colors[i] = applyColorScaleCMapped(
          palette->getStyle(i)->getAverageColor(), globalColorScale);
  else
    for (int i = 0; i < palette->getStyleCount(); i++)
      colors[i] = ::premultiply(palette->getStyle(i)->getAverageColor());

  dn->lock();
  up->lock();

  TPixel32 *dnRow       = dn->pixels(yMin);
  TPixelCM32 *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //       (xMin, y) + k*(1, 0),  k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //       invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //       k = kMin, ..., kMax con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente
    //  intersecando la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL
    //    <  up->getLx()*(1<<PADN)
    //
    //  0 <= kMinX
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxX
    //    <= (xMax - xMin)
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)

    //  0 <= kMinY
    //    <= kMin
    //    <= k
    //    <= kMax
    //    <= kMaxY
    //    <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn

    //  0 <= xL0 + k*deltaXL
    //    < up->getLx()*(1<<PADN)
    //           <=>
    //  0 <= xL0 + k*deltaXL
    //    <= lxPred
    //
    //  0 <= yL0 + k*deltaYL
    //    < up->getLy()*(1<<PADN)
    //           <=>
    //  0 <= yL0 + k*deltaYL
    //    <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      // [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      // altrimenti usa solo
      // kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;

      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelCM32 *upPix = upBasePix + (yI * upWrap + xI);
      int t             = upPix->getTone();
      int p             = upPix->getPaint();

      if (t == 0xff && p == 0)
        continue;
      else {
        int i = upPix->getInk();
        TPixel32 colorUp;
        if (inksOnly) switch (t) {
          case 0:
            colorUp = colors[i];
            break;
          case 255:
            colorUp = TPixel::Transparent;
            break;
          default:
            colorUp = antialias(colors[i], 255 - t);
            break;
          }
        else
          switch (t) {
          case 0:
            colorUp = colors[i];
            break;
          case 255:
            colorUp = colors[p];
            break;
          default:
            colorUp = blend(colors[i], colors[p], t, TPixelCM32::getMaxTone());
            break;
          }

        if (colorUp.m == 255)
          *dnPix = colorUp;
        else if (colorUp.m != 0)
          *dnPix = quickOverPix(*dnPix, colorUp);
      }
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
//
// doQuickPutCmapped + transparencyCheck + inkCheck + paintcheck
//
//=============================================================================
/*
TPixel TransparencyCheckBlackBgInk = TPixel(255,255,255); //bg
TPixel TransparencyCheckWhiteBgInk = TPixel(0,0,0);    //ink
TPixel TransparencyCheckPaint = TPixel(127,127,127);  //paint*/

void doQuickPutCmapped(const TRaster32P &dn, const TRasterCM32P &up,
                       const TPaletteP &palette, const TAffine &aff,
                       const TRop::CmappedQuickputSettings &s)
/*const TPixel32& globalColorScale,
                 bool inksOnly,
                 bool transparencyCheck,
                 bool blackBgCheck,
                 int inkIndex,
                 int paintIndex)*/
{
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;
  const int PADN = 16;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));
  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;
  int yMin       = std::max(tfloor(boundingBoxD.y0), 0);
  int yMax       = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);
  int xMin       = std::max(tfloor(boundingBoxD.x0), 0);
  int xMax       = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);
  TAffine invAff = inv(aff);
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;
  int deltaXL    = tround(deltaXD * (1 << PADN));
  int deltaYL    = tround(deltaYD * (1 << PADN));
  if ((deltaXL == 0) && (deltaYL == 0)) return;
  int lxPred = up->getLx() * (1 << PADN) - 1;
  int lyPred = up->getLy() * (1 << PADN) - 1;
  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  std::vector<TPixel32> paints(palette->getStyleCount());
  std::vector<TPixel32> inks(palette->getStyleCount());

  if (s.m_transparencyCheck && !s.m_isOnionSkin) {
    for (int i = 0; i < palette->getStyleCount(); i++) {
      // if the style's autopaint flag is ON, show its original color
      // so that users can easily find lines left unpainted.
      if (i == s.m_gapCheckIndex || palette->getStyle(i)->getFlags() != 0) {
        paints[i] = inks[i] = applyColorScaleCMapped(
            palette->getStyle(i)->getAverageColor(), s.m_globalColorScale);
      } else {
        paints[i] = s.m_transpCheckPaint;
        inks[i]   = s.m_blackBgCheck ? s.m_transpCheckBg : s.m_transpCheckInk;
      }
    }
  } else if (s.m_globalColorScale == TPixel::Black)
    for (int i = 0; i < palette->getStyleCount(); i++)
      paints[i] = inks[i] =
          ::premultiply(palette->getStyle(i)->getAverageColor());
  else
    for (int i = 0; i < palette->getStyleCount(); i++)
      paints[i] = inks[i] = applyColorScaleCMapped(
          palette->getStyle(i)->getAverageColor(), s.m_globalColorScale);

  dn->lock();
  up->lock();

  TPixel32 *dnRow       = dn->pixels(yMin);
  TPixelCM32 *upBasePix = up->pixels();
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    TPointD a = invAff * TPointD(xMin, y);
    int xL0   = tround((a.x + 0.5) * (1 << PADN));
    int yL0   = tround((a.y + 0.5) * (1 << PADN));
    int kMinX = 0, kMaxX = xMax - xMin;
    int kMinY = 0, kMaxY = xMax - xMin;
    if (deltaXL == 0) {
      if ((xL0 < 0) || (lxPred < xL0)) continue;
    } else if (deltaXL > 0) {
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;                       //  floor
      if (xL0 < 0) kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    } else {
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0)
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
    if (deltaYL == 0) {
      if ((yL0 < 0) || (lyPred < yL0)) continue;
    } else if (deltaYL > 0) {
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;
      if (yL0 < 0) kMinY = ((-yL0) + deltaYL - 1) / deltaYL;
    } else {
      if (yL0 < 0) continue;
      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0)
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;
    int xL             = xL0 + (kMin - 1) * deltaXL;
    int yL             = yL0 + (kMin - 1) * deltaYL;
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;
      int xI = xL >> PADN;
      int yI = yL >> PADN;
      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));
      TPixelCM32 *upPix = upBasePix + (yI * upWrap + xI);
      int t             = upPix->getTone();
      int p             = upPix->getPaint();

      if (t == 0xff && p == 0)
        continue;
      else {
        int i = upPix->getInk();
        TPixel32 colorUp;
        if (s.m_inksOnly) switch (t) {
          case 0:
            colorUp = (i == s.m_inkIndex) ? TPixel::Red : inks[i];
            break;
          case 255:
            colorUp = TPixel::Transparent;
            break;
          default: {
            TPixel inkColor;
            if (i == s.m_inkIndex) {
              inkColor = TPixel::Red;
              if (p == 0) {
                t = t / 2;  // transparency check(for a bug!) darken
                            // semitrasparent pixels; ghibli likes it, and wants
                            // it also for ink checks...
                // otherwise, ramps goes always from reds towards grey...
              }
            } else
              inkColor = inks[i];

            colorUp = antialias(inkColor, 255 - t);
            break;
          }
          }
        else
          switch (t) {
          case 0:
            colorUp = (i == s.m_inkIndex) ? TPixel::Red : inks[i];
            break;
          case 255:
            colorUp = (p == s.m_paintIndex) ? TPixel::Red : paints[p];
            break;
          default: {
            TPixel paintColor = (p == s.m_paintIndex) ? TPixel::Red : paints[p];
            TPixel inkColor;
            if (i == s.m_inkIndex) {
              inkColor = TPixel::Red;
              if (p == 0) {
                paintColor = TPixel::Transparent;
              }
            } else
              inkColor = inks[i];

            if (s.m_transparencyCheck) t = t / 2;

            colorUp = blend(inkColor, paintColor, t, TPixelCM32::getMaxTone());
            break;
          }
          }

        if (colorUp.m == 255)
          *dnPix = colorUp;
        else if (colorUp.m != 0)
          *dnPix = quickOverPix(*dnPix, colorUp);
      }
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
//=============================================================================
void doQuickPutCmapped(const TRaster32P &dn, const TRasterCM32P &up,
                       const TPaletteP &palette, double sx, double sy,
                       double tx, double ty, const TPixel32 &globalColorScale,
                       bool inksOnly) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));
  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  // disponibili per la parte intera di xL, yL)

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //  successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //  pixels corrispondenti di up

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //  pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //	(1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //	       (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //	         kX = 0, ..., (xMax - xMin),
  //             kY = 0, ..., (yMax - yMin)

  //	(2)  equazione (kX, kY)-parametrica dell'immagine
  //         mediante invAff di (1):
  //	       invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //	         kX = kMinX, ..., kMaxX
  //             con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //
  //             kY = kMinY, ..., kMaxY
  //             con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up e' la controimmagine mediante aff della
  // porzione di scanline  [ (xMin, yMin), (xMax, yMin) ] di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
  // in versione "TLonghizzata"
  //	0 <= xL0 + kX*deltaXL
  //      < up->getLx()*(1<<PADN),
  //
  //    0 <= kMinX
  //      <= kX
  //      <= kMaxX
  //      <= (xMax - xMin)

  //	0 <= yL0 + kY*deltaYL
  //      < up->getLy()*(1<<PADN),
  //
  //    0 <= kMinY
  //      <= kY
  //      <= kMaxY
  //      <= (yMax - yMin)

  //  xL0 inizializzato per il round
  int xL0 = tround((a.x + 0.5) * (1 << PADN));

  //  yL0 inizializzato per il round
  int yL0 = tround((a.y + 0.5) * (1 << PADN));

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

  //  TINT32 predecessore di up->getLx()
  int lxPred = up->getLx() * (1 << PADN) - 1;

  //  TINT32 predecessore di up->getLy()
  int lyPred = up->getLy() * (1 << PADN) - 1;

  //  0 <= xL0 + k*deltaXL < up->getLx()*(1<<PADN)
  //            <=>
  //  0 <= xL0 + k*deltaXL <= lxPred

  //  0 <= yL0 + k*deltaYL < up->getLy()*(1<<PADN)
  //            <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY intersecando la (2) con i lati
  //  (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(yL0 <= lyPred);

    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //  calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX intersecando la (2) con i lati
  //  (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up+(bordo destro/basso)
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();

  int count = std::max({palette->getStyleCount(), TPixelCM32::getMaxInk(),
                        TPixelCM32::getMaxPaint()});

  std::vector<TPixel32> paints(count, TPixel32::Red);
  std::vector<TPixel32> inks(count, TPixel32::Red);
  if (globalColorScale != TPixel::Black)
    for (int i = 0; i < palette->getStyleCount(); i++)
      paints[i] = inks[i] = applyColorScaleCMapped(
          palette->getStyle(i)->getAverageColor(), globalColorScale);
  else
    for (int i = 0; i < palette->getStyleCount(); i++)
      paints[i] = inks[i] =
          ::premultiply(palette->getStyle(i)->getAverageColor());

  dn->lock();
  up->lock();
  TPixelCM32 *upBasePix = up->pixels();
  TPixel32 *dnRow       = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata" del pixel corrente di up

  //  inizializza yL
  int yL = yL0 + (kMinY - 1) * deltaYL;

  //  scorre le scanline di boundingBoxD
  for (int kY = kMinY; kY <= kMaxY; kY++, dnRow += dnWrap) {
    //  inizializza xL
    int xL = xL0 + (kMinX - 1) * deltaXL;
    yL += deltaYL;

    //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e' approssimato
    //  con (xI, yI)
    int yI = yL >> PADN;  //  round

    TPixel32 *dnPix    = dnRow + xMin + kMinX;
    TPixel32 *dnEndPix = dnRow + xMin + kMaxX + 1;

    //  scorre i pixel sulla (yMin + kY)-esima scanline di dn
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //	round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelCM32 *upPix = upBasePix + (yI * upWrap + xI);
      int t             = upPix->getTone();
      int p             = upPix->getPaint();
      assert(0 <= t && t < 256);
      assert(0 <= p && p < (int)paints.size());

      if (t == 0xff && p == 0)
        continue;
      else {
        int i = upPix->getInk();
        assert(0 <= i && i < (int)inks.size());
        TPixel32 colorUp;
        if (inksOnly) switch (t) {
          case 0:
            colorUp = inks[i];
            break;
          case 255:
            colorUp = TPixel::Transparent;
            break;
          default:
            colorUp = antialias(inks[i], 255 - t);
            break;
          }
        else
          switch (t) {
          case 0:
            colorUp = inks[i];
            break;
          case 255:
            colorUp = paints[p];
            break;
          default:
            colorUp = blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());
            break;
          }

        if (colorUp.m == 255)
          *dnPix = colorUp;
        else if (colorUp.m != 0)
          *dnPix = quickOverPix(*dnPix, colorUp);
      }
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================
//=============================================================================
//=============================================================================

void doQuickResampleColorFilter(const TRaster32P &dn, const TRasterCM32P &up,
                                const TPaletteP &plt, const TAffine &aff,
                                UCHAR colorMask) {
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;
  const int PADN = 16;

  std::vector<TPixel32> paints(plt->getStyleCount());
  std::vector<TPixel32> inks(plt->getStyleCount());

  for (int i = 0; i < plt->getStyleCount(); i++)
    paints[i] = inks[i] = ::premultiply(plt->getStyle(i)->getAverageColor());

  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));

  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);  //  clipping y su dn
  int yMax =
      std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);  //  clipping y su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);        //  clipping x su dn
  int xMax =
      std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);  //  clipping x su dn

  TAffine invAff = inv(aff);  //  inversa di aff

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  int lxPred =
      up->getLx() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLx()
  int lyPred =
      up->getLy() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLy()

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow       = dn->pixels(yMin);
  TPixelCM32 *upBasePix = up->pixels();

  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    TPointD a = invAff * TPointD(xMin, y);
    int xL0   = tround((a.x + 0.5) * (1 << PADN));
    int yL0   = tround((a.y + 0.5) * (1 << PADN));
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn
    if (deltaXL == 0) {
      if ((xL0 < 0) || (lxPred < xL0)) continue;
    } else if (deltaXL > 0) {
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;                       //  floor
      if (xL0 < 0) kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    } else                                                    //  (deltaXL < 0)
    {
      if (xL0 < 0) continue;
      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0)
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
    if (deltaYL == 0) {
      if ((yL0 < 0) || (lyPred < yL0)) continue;
    } else if (deltaYL > 0) {
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;                       //  floor
      if (yL0 < 0) kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    } else                                                    //  (deltaYL < 0)
    {
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0)
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
    int kMin           = std::max({kMinX, kMinY, (int)0});
    int kMax           = std::min({kMaxX, kMaxY, xMax - xMin});
    TPixel32 *dnPix    = dnRow + xMin + kMin;
    TPixel32 *dnEndPix = dnRow + xMin + kMax + 1;
    int xL             = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL             = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      TPixelCM32 *upPix = upBasePix + (yI * upWrap + xI);
      int t             = upPix->getTone();
      int p             = upPix->getPaint();
      int i             = upPix->getInk();
      TPixel32 colorUp;
      switch (t) {
      case 0:
        colorUp = inks[i];
        break;
      case 255:
        colorUp = paints[p];
        break;
      default:
        colorUp = blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());
        break;
      }

      if (colorMask == TRop::MChan)
        dnPix->r = dnPix->g = dnPix->b = colorUp.m;
      else {
        dnPix->r = ((colorMask & TRop::RChan) ? colorUp.r : 0);
        dnPix->g = ((colorMask & TRop::GChan) ? colorUp.g : 0);
        dnPix->b = ((colorMask & TRop::BChan) ? colorUp.b : 0);
      }
      dnPix->m = 255;
    }
  }
  dn->unlock();
  up->unlock();
}

//==========================================================

#endif  // TNZCORE_LIGHT

#ifdef OPTIMIZE_FOR_LP64
void doQuickResampleFilter_optimized(const TRaster32P &dn, const TRaster32P &up,
                                     const TAffine &aff) {
  //  se aff e' degenere la controimmagine di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));
  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)

  TRectD boundingBoxD = TRectD(convert(dn->getSize())) *
                        (aff * TRectD(0, 0, up->getLx() - 2, up->getLy() - 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate
  //  del pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;

  //  deltaXD "TLonghizzato" (round)
  int deltaXL = tround(deltaXD * (1 << PADN));

  //  deltaYD "TLonghizzato" (round)
  int deltaYL = tround(deltaYD * (1 << PADN));

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e' un
  //  segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  // naturale predecessore di up->getLx() - 1
  int lxPred = (up->getLx() - 2) * (1 << PADN);

  //  naturale predecessore di up->getLy() - 1
  int lyPred = (up->getLy() - 2) * (1 << PADN);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *dnRow     = dn->pixels(yMin);
  TPixel32 *upBasePix = up->pixels();

  long c1;
  long c2;

  long c3;
  long c4;

  long c5;
  long c6;

  long s_rg;
  long s_br;
  long s_gb;

  UINT32 rColDownTmp;
  UINT32 gColDownTmp;
  UINT32 bColDownTmp;

  UINT32 rColUpTmp;
  UINT32 gColUpTmp;
  UINT32 bColUpTmp;

  unsigned char rCol;
  unsigned char gCol;
  unsigned char bCol;

  int xI;
  int yI;

  int xWeight1;
  int xWeight0;
  int yWeight1;
  int yWeight0;

  TPixel32 *upPix00;
  TPixel32 *upPix10;
  TPixel32 *upPix01;
  TPixel32 *upPix11;

  TPointD a;
  int xL0;
  int yL0;
  int kMinX;
  int kMaxX;
  int kMinY;
  int kMaxY;

  int kMin;
  int kMax;
  TPixel32 *dnPix;
  TPixel32 *dnEndPix;
  int xL;
  int yL;

  int y = yMin;
  ++yMax;
  //  scorre le scanline di boundingBoxD
  for (; y < yMax - 32; ++y, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_FIRST_X_32
  }
  for (; y < yMax - 16; ++y, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_FIRST_X_16
  }
  for (; y < yMax - 8; ++y, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_FIRST_X_8
  }
  for (; y < yMax - 4; ++y, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_FIRST_X_4
  }
  for (; y < yMax - 2; ++y, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_FIRST_X_2
  }
  for (; y < yMax; ++y, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_FIRST
  }
  dn->unlock();
  up->unlock();
}
#endif

//=============================================================================
//=============================================================================
//=============================================================================

#ifdef OPTIMIZE_FOR_LP64

void doQuickResampleFilter_optimized(const TRaster32P &dn, const TRaster32P &up,
                                     double sx, double sy, double tx,
                                     double ty) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((sx == 0) || (sy == 0)) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  maschera del filtro bilineare
  const int MASKN = (1 << PADN) - 1;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  // disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TAffine aff(sx, 0, tx, 0, sy, ty);
  TRectD boundingBoxD = TRectD(convert(dn->getSize())) *
                        (aff * TRectD(0, 0, up->getLx() - 2, up->getLy() - 2));

  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  //  clipping y su dn
  int yMin = std::max(tfloor(boundingBoxD.y0), 0);

  //  clipping y su dn
  int yMax = std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);

  //  clipping x su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);

  //  clipping x su dn
  int xMax = std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);

  //  inversa di aff
  TAffine invAff = inv(aff);

  //  nello scorrere le scanline di boundingBoxD, il passaggio alla scanline
  //  successiva comporta l'incremento (0, deltaYD) delle coordinate dei
  //  pixels corrispondenti di up

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, 0) delle coordinate del
  //  pixel corrispondente di up

  double deltaXD = invAff.a11;
  double deltaYD = invAff.a22;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e'
  //  un segmento (o un punto)
  if ((deltaXL == 0) || (deltaYL == 0)) return;

  //  (1)  equazione (kX, kY)-parametrica di boundingBoxD:
  //         (xMin, yMin) + kX*(1, 0) + kY*(0, 1),
  //           kX = 0, ..., (xMax - xMin),
  //           kY = 0, ..., (yMax - yMin)

  //  (2)  equazione (kX, kY)-parametrica dell'immagine mediante invAff di (1):
  //         invAff*(xMin, yMin) + kX*(deltaXD, 0) + kY*(0, deltaYD),
  //           kX = kMinX, ..., kMaxX
  //             con 0 <= kMinX <= kMaxX <= (xMax - xMin)
  //
  //           kY = kMinY, ..., kMaxY
  //             con 0 <= kMinY <= kMaxY <= (yMax - yMin)

  //  calcola kMinX, kMaxX, kMinY, kMaxY intersecando la (2) con i lati di up

  //  il segmento [a, b] di up (con gli estremi eventualmente invertiti) e'
  //  la controimmagine mediante aff della porzione di scanline
  //  [ (xMin, yMin), (xMax, yMin) ] di dn

  //  TPointD b = invAff*TPointD(xMax, yMin);
  TPointD a = invAff * TPointD(xMin, yMin);

  //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  0 <= xL0 + kX*deltaXL <= (up->getLx() - 2)*(1<<PADN),
  //  0 <= kMinX <= kX <= kMaxX <= (xMax - xMin)
  //  0 <= yL0 + kY*deltaYL <= (up->getLy() - 2)*(1<<PADN),
  //  0 <= kMinY <= kY <= kMaxY <= (yMax - yMin)

  int xL0 = tround(a.x * (1 << PADN));  //  xL0 inizializzato
  int yL0 = tround(a.y * (1 << PADN));  //  yL0 inizializzato

  //  calcola kMinY, kMaxY, kMinX, kMaxX intersecando la (2) con i lati di up
  int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
  int kMinY = 0, kMaxY = yMax - yMin;  //  clipping su dn

  //  TINT32 predecessore di (up->getLx() - 1)
  int lxPred = (up->getLx() - 2) * (1 << PADN);

  //  TINT32 predecessore di (up->getLy() - 1)
  int lyPred = (up->getLy() - 2) * (1 << PADN);

  //  0 <= xL0 + k*deltaXL <= (up->getLx() - 2)*(1<<PADN)
  //               <=>
  //  0 <= xL0 + k*deltaXL <= lxPred

  //  0 <= yL0 + k*deltaYL <= (up->getLy() - 2)*(1<<PADN)
  //               <=>
  //  0 <= yL0 + k*deltaYL <= lyPred

  //  calcola kMinY, kMaxY intersecando la (2) con i lati
  //  (y = yMin) e (y = yMax) di up
  if (deltaYL > 0)  //  (deltaYL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(yL0 <= lyPred);

    kMaxY = (lyPred - yL0) / deltaYL;  //  floor
    if (yL0 < 0) {
      kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
    }
  } else  //  (deltaYL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= yL0);

    kMaxY = yL0 / (-deltaYL);  //  floor
    if (lyPred < yL0) {
      kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
    }
  }
  //  calcola kMinY, kMaxY effettuando anche il clippind su dn
  kMinY = std::max(kMinY, (int)0);
  kMaxY = std::min(kMaxY, yMax - yMin);

  //  calcola kMinX, kMaxX intersecando la (2) con i lati
  //  (x = xMin) e (x = xMax) di up
  if (deltaXL > 0)  //  (deltaXL != 0)
  {
    //  [a, b] interno ad up contratto
    assert(xL0 <= lxPred);

    kMaxX = (lxPred - xL0) / deltaXL;  //  floor
    if (xL0 < 0) {
      kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
    }
  } else  //  (deltaXL < 0)
  {
    //  [a, b] interno ad up contratto
    assert(0 <= xL0);

    kMaxX = xL0 / (-deltaXL);  //  floor
    if (lxPred < xL0) {
      kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
    }
  }
  //  calcola kMinX, kMaxX effettuando anche il clippind su dn
  kMinX = std::max(kMinX, (int)0);
  kMaxX = std::min(kMaxX, xMax - xMin);

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  TPixel32 *upBasePix = up->pixels();
  TPixel32 *dnRow     = dn->pixels(yMin + kMinY);

  //  (xL, yL) sono le coordinate (inizializzate per il round)
  //  in versione "TLonghizzata"
  //  del pixel corrente di up
  int yL = yL0 + (kMinY - 1) * deltaYL;  //  inizializza yL

  long c1;
  long c2;

  long c3;
  long c4;

  long c5;
  long c6;

  long s_rg;
  long s_br;
  long s_gb;

  UINT32 rColDownTmp;
  UINT32 gColDownTmp;
  UINT32 bColDownTmp;

  UINT32 rColUpTmp;
  UINT32 gColUpTmp;
  UINT32 bColUpTmp;

  int xI;
  TPixel32 *upPix00;
  TPixel32 *upPix10;
  TPixel32 *upPix01;
  TPixel32 *upPix11;
  int xWeight1;
  int xWeight0;

  unsigned char rCol;
  unsigned char gCol;
  unsigned char bCol;

  int xL;
  int yI;
  int yWeight1;
  int yWeight0;
  TPixel32 *dnPix;
  TPixel32 *dnEndPix;
  int kY = kMinY;
  ++kMaxY;

  //  scorre le scanline di boundingBoxD
  for (; kY < kMaxY - 32; kY++, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_SECOND_X_32
  }
  for (; kY < kMaxY - 16; kY++, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_SECOND_X_16
  }
  for (; kY < kMaxY - 8; kY++, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_SECOND_X_8
  }
  for (; kY < kMaxY - 4; kY++, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_SECOND_X_4
  }
  for (; kY < kMaxY - 2; kY++, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_SECOND_X_2
  }
  for (; kY < kMaxY; kY++, dnRow += dnWrap) {
    EXTERNAL_LOOP_THE_SECOND
  }
  dn->unlock();
  up->unlock();
}
#endif

// namespace
};  // namespace

#ifndef TNZCORE_LIGHT
//=============================================================================
//
// quickPut (paletted)
//
//=============================================================================

void TRop::quickPut(const TRasterP &dn, const TRasterCM32P &upCM32,
                    const TPaletteP &plt, const TAffine &aff,
                    const TPixel32 &globalColorScale, bool inksOnly) {
  TRaster32P dn32 = dn;
  if (dn32 && upCM32)
    if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
      doQuickPutCmapped(dn32, upCM32, plt, aff.a11, aff.a22, aff.a13, aff.a23,
                        globalColorScale, inksOnly);
    else
      doQuickPutCmapped(dn32, upCM32, plt, aff, globalColorScale, inksOnly);
  else
    throw TRopException("raster type mismatch");
}

//=============================================================================
//
// quickPut (paletted + transparency check + ink check + paint check)
//
//=============================================================================

void TRop::quickPut(const TRasterP &dn, const TRasterCM32P &upCM32,
                    const TPaletteP &plt, const TAffine &aff,
                    const CmappedQuickputSettings &settings)  // const TPixel32&
// globalColorScale, bool
// inksOnly, bool
// transparencyCheck, bool
// blackBgCheck, int inkIndex, int
// paintIndex)
{
  TRaster32P dn32 = dn;
  if (dn32 && upCM32)
    doQuickPutCmapped(dn32, upCM32, plt, aff,
                      settings);  // globalColorScale, inksOnly,
                                  // transparencyCheck, blackBgCheck, inkIndex,
                                  // paintIndex);
  else
    throw TRopException("raster type mismatch");
}

void TRop::quickResampleColorFilter(const TRasterP &dn, const TRasterP &up,
                                    const TAffine &aff, const TPaletteP &plt,
                                    UCHAR colorMask) {
  TRaster32P dn32     = dn;
  TRaster32P up32     = up;
  TRaster64P up64     = up;
  TRasterCM32P upCM32 = up;
  if (dn32 && up32)
    doQuickResampleColorFilter(dn32, up32, aff, colorMask);
  else if (dn32 && upCM32)
    doQuickResampleColorFilter(dn32, upCM32, plt, aff, colorMask);
  else if (dn32 && up64)
    doQuickResampleColorFilter(dn32, up64, aff, colorMask);
  // else if (dn32 && upCM32)
  //  doQuickResampleColorFilter(dn32, upCM32, aff, plt, colorMask);
  else
    throw TRopException("raster type mismatch");
}

#endif  // TNZCORE_LIGHT

//=============================================================================
//=============================================================================
//
// quickPut (Bilinear/Closest)
//
//=============================================================================

void quickPut(const TRasterP &dn, const TRasterP &up, const TAffine &aff,
              TRop::ResampleFilterType filterType, const TPixel32 &colorScale,
              bool doPremultiply, bool whiteTransp, bool firstColumn,
              bool doRasterDarkenBlendedView) {
  assert(filterType == TRop::Bilinear || filterType == TRop::ClosestPixel);

  bool bilinear = filterType == TRop::Bilinear;

  TRaster32P dn32 = dn;
  TRaster32P up32 = up;
  TRasterGR8P dn8 = dn;
  TRasterGR8P up8 = up;
  TRaster64P dn64 = dn;
  TRaster64P up64 = up;
  TRasterFP upF   = up;
  TRasterFP dnF   = dn;

  if (up8 && dn32) {
    assert(filterType == TRop::ClosestPixel);
    if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
      doQuickPutNoFilter(dn32, up8, aff.a11, aff.a22, aff.a13, aff.a23,
                         colorScale);
    else
      doQuickPutNoFilter(dn32, up8, aff, colorScale);
  } else if (dn32 && up32) {
    if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0)) {
      if (bilinear)
        doQuickPutFilter(dn32, up32, aff.a11, aff.a22, aff.a13, aff.a23);
      else {
        doQuickPutNoFilter(dn32, up32, aff.a11, aff.a22, aff.a13, aff.a23,
                           colorScale, doPremultiply, whiteTransp, firstColumn,
                           doRasterDarkenBlendedView);
      }
    } else if (bilinear)
      doQuickPutFilter(dn32, up32, aff);
    else {
      doQuickPutNoFilter(dn32, up32, aff, colorScale, doPremultiply,
                         whiteTransp, firstColumn, doRasterDarkenBlendedView);
    }
  } else if (dn32 && up64)
    doQuickPutNoFilter(dn32, up64, aff, doPremultiply, firstColumn);
  else if (dn64 && up64)
    doQuickPutNoFilter(dn64, up64, aff, doPremultiply, firstColumn);
  else if (dn64 && up32)
    doQuickPutNoFilter(dn64, up32, aff, doPremultiply, firstColumn);
  else if (dn64 && upF)
    doQuickPutNoFilter(dn64, upF, aff, doPremultiply, firstColumn);
  else if (dnF && upF)
    doQuickPutNoFilter(dnF, upF, aff, doPremultiply, firstColumn);
  else
    throw TRopException("raster type mismatch");
}

//=============================================================================
template <typename PIX>
void doQuickResampleNoFilter(const TRasterPT<PIX> &dn, const TRasterPT<PIX> &up,
                             const TAffine &aff) {
  //  se aff := TAffine(sx, 0, tx, 0, sy, ty) e' degenere la controimmagine
  //  di up e' un segmento (o un punto)
  if ((aff.a11 * aff.a22 - aff.a12 * aff.a21) == 0) return;

  //  contatore bit di shift
  const int PADN = 16;

  //  max dimensioni di up gestibili (limite imposto dal numero di bit
  //  disponibili per la parte intera di xL, yL)
  assert(std::max(up->getLx(), up->getLy()) <
         (1 << (8 * sizeof(int) - PADN - 1)));

  TRectD boundingBoxD =
      TRectD(convert(dn->getBounds())) *
      (aff * TRectD(-0.5, -0.5, up->getLx() - 0.5, up->getLy() - 0.5));
  //  clipping
  if (boundingBoxD.x0 >= boundingBoxD.x1 || boundingBoxD.y0 >= boundingBoxD.y1)
    return;

  int yMin = std::max(tfloor(boundingBoxD.y0), 0);  //  clipping y su dn
  int yMax =
      std::min(tceil(boundingBoxD.y1), dn->getLy() - 1);  //  clipping y su dn
  int xMin = std::max(tfloor(boundingBoxD.x0), 0);        //  clipping x su dn
  int xMax =
      std::min(tceil(boundingBoxD.x1), dn->getLx() - 1);  //  clipping x su dn

  TAffine invAff = inv(aff);  //  inversa di aff

  //  nel disegnare la y-esima scanline di dn, il passaggio al pixel
  //  successivo comporta l'incremento (deltaXD, deltaYD) delle coordinate
  //  del pixel corrispondente di up
  double deltaXD = invAff.a11;
  double deltaYD = invAff.a21;
  int deltaXL =
      tround(deltaXD * (1 << PADN));  //  deltaXD "TLonghizzato" (round)
  int deltaYL =
      tround(deltaYD * (1 << PADN));  //  deltaYD "TLonghizzato" (round)

  //  se aff "TLonghizzata" (round) e' degenere la controimmagine di up e'
  //  un segmento (o un punto)
  if ((deltaXL == 0) && (deltaYL == 0)) return;

  int lxPred =
      up->getLx() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLx()
  int lyPred =
      up->getLy() * (1 << PADN) - 1;  //  TINT32 predecessore di up->getLy()

  int dnWrap = dn->getWrap();
  int upWrap = up->getWrap();
  dn->lock();
  up->lock();

  PIX *dnRow     = dn->pixels(yMin);
  PIX *upBasePix = up->pixels();

  //  scorre le scanline di boundingBoxD
  for (int y = yMin; y <= yMax; y++, dnRow += dnWrap) {
    //  (1)  equazione k-parametrica della y-esima scanline di boundingBoxD:
    //         (xMin, y) + k*(1, 0),
    //           k = 0, ..., (xMax - xMin)

    //  (2)  equazione k-parametrica dell'immagine mediante invAff di (1):
    //         invAff*(xMin, y) + k*(deltaXD, deltaYD),
    //           k = kMin, ..., kMax
    //           con 0 <= kMin <= kMax <= (xMax - xMin)

    //  calcola kMin, kMax per la scanline corrente intersecando
    //  la (2) con i lati di up

    //  il segmento [a, b] di up e' la controimmagine mediante aff della
    //  porzione di scanline  [ (xMin, y), (xMax, y) ] di dn

    //  TPointD b = invAff*TPointD(xMax, y);
    TPointD a = invAff * TPointD(xMin, y);

    //  (xL0, yL0) sono le coordinate di a (inizializzate per il round)
    //  in versione "TLonghizzata"
    //  0 <= xL0 + k*deltaXL < up->getLx()*(1<<PADN),
    //  0 <= kMinX <= kMin <= k <= kMax <= kMaxX <= (xMax - xMin)

    //  0 <= yL0 + k*deltaYL < up->getLy()*(1<<PADN),
    //  0 <= kMinY <= kMin <= k <= kMax <= kMaxY <= (xMax - xMin)

    //  xL0 inizializzato per il round
    int xL0 = tround((a.x + 0.5) * (1 << PADN));

    //  yL0 inizializzato per il round
    int yL0 = tround((a.y + 0.5) * (1 << PADN));

    //  calcola kMinX, kMaxX, kMinY, kMaxY
    int kMinX = 0, kMaxX = xMax - xMin;  //  clipping su dn
    int kMinY = 0, kMaxY = xMax - xMin;  //  clipping su dn
    //  0 <= xL0 + k*deltaXL < up->getLx()*(1<<PADN)
    //               <=>
    //  0 <= xL0 + k*eltaXL <= lxPred

    //  0 <= yL0 + k*deltaYL < up->getLy()*(1<<PADN)
    //               <=>
    //  0 <= yL0 + k*deltaYL <= lyPred

    //  calcola kMinX, kMaxX
    if (deltaXL == 0) {
      //  [a, b] verticale esterno ad up+(bordo destro/basso)
      if ((xL0 < 0) || (lxPred < xL0)) continue;
      //  altrimenti usa solo
      //  kMinY, kMaxY ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaXL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lxPred < xL0) continue;

      kMaxX = (lxPred - xL0) / deltaXL;  //  floor
      if (xL0 < 0) {
        kMinX = ((-xL0) + deltaXL - 1) / deltaXL;  //  ceil
      }
    } else  //  (deltaXL < 0)
    {
      // [a, b] esterno ad up+(bordo destro/basso)
      if (xL0 < 0) continue;

      kMaxX = xL0 / (-deltaXL);  //  floor
      if (lxPred < xL0) {
        kMinX = (xL0 - lxPred - deltaXL - 1) / (-deltaXL);  //  ceil
      }
    }

    //  calcola kMinY, kMaxY
    if (deltaYL == 0) {
      //  [a, b] orizzontale esterno ad up+(bordo destro/basso)
      if ((yL0 < 0) || (lyPred < yL0)) continue;
      //  altrimenti usa solo
      //  kMinX, kMaxX ((deltaXL != 0) || (deltaYL != 0))
    } else if (deltaYL > 0) {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (lyPred < yL0) continue;

      kMaxY = (lyPred - yL0) / deltaYL;  //  floor
      if (yL0 < 0) {
        kMinY = ((-yL0) + deltaYL - 1) / deltaYL;  //  ceil
      }
    } else  //  (deltaYL < 0)
    {
      //  [a, b] esterno ad up+(bordo destro/basso)
      if (yL0 < 0) continue;

      kMaxY = yL0 / (-deltaYL);  //  floor
      if (lyPred < yL0) {
        kMinY = (yL0 - lyPred - deltaYL - 1) / (-deltaYL);  //  ceil
      }
    }

    //  calcola kMin, kMax effettuando anche il clippind su dn
    int kMin = std::max({kMinX, kMinY, (int)0});
    int kMax = std::min({kMaxX, kMaxY, xMax - xMin});

    PIX *dnPix    = dnRow + xMin + kMin;
    PIX *dnEndPix = dnRow + xMin + kMax + 1;

    //  (xL, yL) sono le coordinate (inizializzate per il round)
    //  in versione "TLonghizzata" del pixel corrente di up
    int xL = xL0 + (kMin - 1) * deltaXL;  //  inizializza xL
    int yL = yL0 + (kMin - 1) * deltaYL;  //  inizializza yL

    //  scorre i pixel sulla y-esima scanline di boundingBoxD
    for (; dnPix < dnEndPix; ++dnPix) {
      xL += deltaXL;
      yL += deltaYL;
      //  il punto di up TPointD(xL/(1<<PADN), yL/(1<<PADN)) e'
      //  approssimato con (xI, yI)
      int xI = xL >> PADN;  //  round
      int yI = yL >> PADN;  //  round

      assert((0 <= xI) && (xI <= up->getLx() - 1) && (0 <= yI) &&
             (yI <= up->getLy() - 1));

      *dnPix = *(upBasePix + (yI * upWrap + xI));
    }
  }
  dn->unlock();
  up->unlock();
}

//=============================================================================

#ifdef OPTIMIZE_FOR_LP64

void quickResample_optimized(const TRasterP &dn, const TRasterP &up,
                             const TAffine &aff,
                             TRop::ResampleFilterType filterType) {
  assert(filterType == TRop::Bilinear || filterType == TRop::ClosestPixel);

  bool bilinear = filterType == TRop::Bilinear;

  TRaster32P dn32 = dn;
  TRaster32P up32 = up;
  TRasterGR8P dn8 = dn;
  TRasterGR8P up8 = up;
  if (dn32 && up32)
    if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
      if (bilinear)
        doQuickResampleFilter_optimized(dn32, up32, aff.a11, aff.a22, aff.a13,
                                        aff.a23);
      else
        doQuickResampleNoFilter(dn32, up32, aff.a11, aff.a22, aff.a13, aff.a23);
    else if (bilinear)
      doQuickResampleFilter_optimized(dn32, up32, aff);
    else
      doQuickResampleNoFilter(dn32, up32, aff);
  else
    throw TRopException("raster type mismatch");
}

#endif

//=============================================================================

void quickResample(const TRasterP &dn, const TRasterP &up, const TAffine &aff,
                   TRop::ResampleFilterType filterType) {
#ifdef OPTIMIZE_FOR_LP64

  quickResample_optimized(dn, up, aff, filterType);

#else

  assert(filterType == TRop::Bilinear || filterType == TRop::ClosestPixel);

  bool bilinear = filterType == TRop::Bilinear;

  TRaster32P dn32     = dn;
  TRaster32P up32     = up;
  TRasterCM32P dnCM32 = dn;
  TRasterCM32P upCM32 = up;
  TRasterGR8P dn8     = dn;
  TRasterGR8P up8     = up;
  dn->clear();

  if (bilinear) {
    if (dn32 && up32) {
      if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
        doQuickResampleFilter(dn32, up32, aff.a11, aff.a22, aff.a13, aff.a23);
      else
        doQuickResampleFilter(dn32, up32, aff);
    } else if (dn32 && up8) {
      if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
        doQuickResampleFilter(dn32, up8, aff.a11, aff.a22, aff.a13, aff.a23);
      else
        doQuickResampleFilter(dn32, up8, aff);
    } else
      throw TRopException("raster type mismatch");
  } else {
    if (dn32 && up32) {
      if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
        doQuickResampleNoFilter(dn32, up32, aff.a11, aff.a22, aff.a13, aff.a23);
      else
        doQuickResampleNoFilter(dn32, up32, aff);

    } else if (dnCM32 && upCM32) {
      if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
        doQuickResampleNoFilter(dnCM32, upCM32, aff.a11, aff.a22, aff.a13,
                                aff.a23);
      else
        doQuickResampleNoFilter(dnCM32, upCM32, aff);
    } else if (dn8 && up8) {
      if (areAlmostEqual(aff.a12, 0) && areAlmostEqual(aff.a21, 0))
        doQuickResampleNoFilter(dn8, up8, aff.a11, aff.a22, aff.a13, aff.a23);
      else
        doQuickResampleNoFilter(dn8, up8, aff);
    } else
      throw TRopException("raster type mismatch");
  }

#endif
}

void quickPut(const TRasterP &out, const TRasterCM32P &up, const TAffine &aff,
              const TPixel32 &inkCol, const TPixel32 &paintCol);
