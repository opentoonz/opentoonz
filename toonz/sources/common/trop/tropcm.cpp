

#include "tsystem.h"

#include <set>

#include "tropcm.h"

#include "tcolorstyles.h"
#include "tpixelutils.h"
#include "ttile.h"
#include "tpalette.h"
#include "timage_io.h"
#include "trasterimage.h"
#include "tsimplecolorstyles.h"

//#include "tlevel.h"
//#include "ttoonzimage.h"
//#include "tgeometry.h"
//#include "timage_io.h"

extern "C" {
#include "toonz4.6/raster.h"
}

#ifdef _WIN32
#define USE_SSE2
#endif

#ifdef USE_SSE2
#include <emmintrin.h>

//---------------------------------------------------------

namespace {

DV_ALIGNED(16) class TPixelFloat {
public:
  TPixelFloat() : b(0), g(0), r(0), m(0) {}

  TPixelFloat(float rr, float gg, float bb, float mm)
      : b(bb), g(gg), r(rr), m(mm) {}

  TPixelFloat(const TPixel32 &pix) : b(pix.b), g(pix.g), r(pix.r), m(pix.m) {}

  float b, g, r, m;
};

}  // anonymous namespace

#endif

//-----------------------------------------------------------------------------

bool renderRas32(const TTile &tileOut, const TTile &tileIn,
                 const TPaletteP palette);

//-----------------------------------------------------------------------------

const TPixel32 c_transparencyCheckPaint = TPixel32(80, 80, 80, 255);
const TPixel32 c_transparencyCheckInk   = TPixel32::Black;

void TRop::convert(const TRaster32P &rasOut, const TRasterCM32P &rasIn,
                   const TPaletteP palette, bool transparencyCheck) {
  int count = palette->getStyleCount();
  int count2 =
      std::max({count, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint()});

  // per poter utilizzare lo switch (piu' efficiente) si utilizza 255
  // anziche' TPixelCM32::getMaxTone()
  assert(TPixelCM32::getMaxTone() == 255);

  int rasLx = rasOut->getLx();
  int rasLy = rasOut->getLy();

  rasOut->lock();
  rasIn->lock();
#ifdef _WIN32
  if (TSystem::getCPUExtensions() & TSystem::CpuSupportsSse2) {
    __m128i zeros = _mm_setzero_si128();
    TPixelFloat *paints =
        (TPixelFloat *)_aligned_malloc(count2 * sizeof(TPixelFloat), 16);
    TPixelFloat *inks =
        (TPixelFloat *)_aligned_malloc(count2 * sizeof(TPixelFloat), 16);

    std::vector<TPixel32> paints2(count2);
    std::vector<TPixel32> inks2(count2);
    if (transparencyCheck) {
      for (int i = 0; i < palette->getStyleCount(); i++) {
        paints2[i] = c_transparencyCheckPaint;
        inks2[i]   = c_transparencyCheckInk;
        paints[i]  = TPixelFloat(paints2[i]);
        inks[i]    = TPixelFloat(inks2[i]);
      }
      paints2[0] = TPixel32::Transparent;
      paints[0]  = TPixelFloat(TPixel32::Transparent);
    }
    /*
else if (true)
{
            for(int i=0;i<palette->getStyleCount();i++)
{
paints2[i] = c_transparencyCheckPaint;
                    inks2[i]   = c_transparencyCheckInk;
paints[i]  = TPixelFloat(paints2[i]);
                    inks[i]    = TPixelFloat(inks2[i]);
            paints2[i] = TPixel32::Transparent;
            paints[i] = TPixelFloat(TPixel32::Transparent);
}
            paints2[0] = TPixel32::Transparent;
            paints[0] = TPixelFloat(TPixel32::Transparent);
}
*/
    else
      for (int i = 0; i < palette->getStyleCount(); i++) {
        TPixel32 color = ::premultiply(palette->getStyle(i)->getAverageColor());
        paints[i] = inks[i] = TPixelFloat(color);
        paints2[i] = inks2[i] = color;
      }

    float maxTone     = (float)TPixelCM32::getMaxTone();
    __m128 den_packed = _mm_load1_ps(&maxTone);

    for (int y = 0; y < rasLy; ++y) {
      TPixel32 *pix32   = rasOut->pixels(y);
      TPixelCM32 *pixIn = rasIn->pixels(y);

      TPixelCM32 *endPixIn = pixIn + rasLx;

      while (pixIn < endPixIn) {
        int tt = pixIn->getTone();
        int p  = pixIn->getPaint();
        int i  = pixIn->getInk();
        switch (tt) {
        case 255:
          *pix32++ = paints2[p];
          break;
        case 0:
          *pix32++ = inks2[i];
          break;
        default: {
          float t         = (float)tt;
          __m128 a_packed = _mm_load_ps((float *)&(inks[i]));
          __m128 b_packed = _mm_load_ps((float *)&(paints[p]));

          __m128 num_packed  = _mm_load1_ps(&t);
          __m128 diff_packed = _mm_sub_ps(den_packed, num_packed);

          // calcola in modo vettoriale out = ((den-num)*a + num*b)/den
          __m128 outPix_packed = _mm_mul_ps(diff_packed, a_packed);
          __m128 tmpPix_packed = _mm_mul_ps(num_packed, b_packed);

          outPix_packed = _mm_add_ps(outPix_packed, tmpPix_packed);
          outPix_packed = _mm_div_ps(outPix_packed, den_packed);

          // converte i canali da float a char
          __m128i outPix_packed_i = _mm_cvtps_epi32(outPix_packed);
          outPix_packed_i         = _mm_packs_epi32(outPix_packed_i, zeros);
          outPix_packed_i         = _mm_packus_epi16(outPix_packed_i, zeros);

          *(DWORD *)(pix32) = _mm_cvtsi128_si32(outPix_packed_i);
          ++pix32;
        }
        }
        ++pixIn;
      }
    }

    _aligned_free(paints);
    _aligned_free(inks);

  } else  // SSE2 not supported
#endif    // _WIN32
  {

    std::vector<TPixel32> paints(count2, TPixel32(255, 0, 0));
    std::vector<TPixel32> inks(count2, TPixel32(255, 0, 0));
    if (transparencyCheck) {
      for (int i = 0; i < palette->getStyleCount(); i++) {
        paints[i] = c_transparencyCheckPaint;
        inks[i]   = c_transparencyCheckInk;
      }
      paints[0] = TPixel32::Transparent;
    } else
      for (int i  = 0; i < palette->getStyleCount(); i++)
        paints[i] = inks[i] =
            ::premultiply(palette->getStyle(i)->getAverageColor());

    for (int y = 0; y < rasLy; ++y) {
      TPixel32 *pix32      = rasOut->pixels(y);
      TPixelCM32 *pixIn    = rasIn->pixels(y);
      TPixelCM32 *endPixIn = pixIn + rasLx;

      while (pixIn < endPixIn) {
        int t = pixIn->getTone();
        int p = pixIn->getPaint();
        int i = pixIn->getInk();

        if (t == TPixelCM32::getMaxTone())
          *pix32++ = paints[p];
        else if (t == 0)
          *pix32++ = inks[i];
        else
          *pix32++ = blend(inks[i], paints[p], t, TPixelCM32::getMaxTone());

        ++pixIn;
      }
    }
  }
  rasOut->unlock();
  rasIn->unlock();
}

//-----------------------------------------------------------------------------------------------

static void do_convert(const TTile &dst, const TTile &src,
                       const TPaletteP palette, bool transparencyCheck,
                       bool applyFx) {
  // assert(palette);
  // assert(_rasOut && _rasIn);
  // assert(rasOut->getSize() == rasIn->getSize());

  if (applyFx && renderRas32(dst, src, palette)) return;

  TRaster32P rasOut;
  TRasterCM32P rasIn;

  if (dst.getRaster()->getSize() != src.getRaster()->getSize()) {
    TRect rect = TRect(convert(dst.m_pos), dst.getRaster()->getSize()) *
                 TRect(convert(src.m_pos), src.getRaster()->getSize());
    TRect rectOut = rect - convert(dst.m_pos);
    rasOut        = dst.getRaster()->extract(rectOut);
    TRect rectIn  = rect - convert(src.m_pos);
    rasIn         = src.getRaster()->extract(rectIn);
  } else {
    rasOut = dst.getRaster();
    rasIn  = src.getRaster();
  }
  TRop::convert(rasOut, rasIn, palette, transparencyCheck);
}

//-----------------------------------------------------------------------------------------------

void TRop::convert(
    const TRaster32P &rasOut, const TRasterCM32P &rasIn, TPaletteP palette,
    const TRect &theClipRect,  // il rect su cui e' applicata la conversione
    bool transparencyCheck, bool applyFx) {
  assert(palette);
  assert(rasIn && rasOut);

  TRect clipRect(theClipRect);
  if (clipRect.isEmpty())
    clipRect = rasIn->getBounds();
  else {
    if (!clipRect.overlaps(rasIn->getBounds())) return;
    clipRect = clipRect * rasIn->getBounds();
  }
  if (clipRect.isEmpty()) return;

  TRect clipRectIn, clipRectOut;

  if (applyFx && palette->getFxRects(clipRect, clipRectIn, clipRectOut)) {
    TRect rAux = clipRectIn;
    TRasterP rAuxIn =
        rasIn->extract(clipRectIn);  // la extract modifica clipRectIn
    if (rAux != clipRectIn && rAux != rasIn->getBounds()) {
      TRasterCM32P rNew(rAux.getSize());
      TRect tmpRect = clipRectIn - rAux.getP00();
      rNew->extract(tmpRect)->copy(rAuxIn);
      rAuxIn     = rNew;
      clipRectIn = rAux;
    }

    TTile tileIn(rAuxIn, ::convert(clipRectIn.getP00()));

    rAux = clipRectOut;
    TRasterP rAuxOut =
        rasOut->extract(clipRectOut);  // la extract modifica clipRectOut
    TTile tileOut(rAuxOut, ::convert(clipRectOut.getP00()));
    TRop::convert(tileOut, tileIn, palette, transparencyCheck, true);
  } else {
    TRect clipRectIn  = clipRect;
    TRect clipRectOut = clipRect;
    TRasterP _rasOut  = rasOut->extract(clipRectOut);
    TRasterP _rasIn   = rasIn->extract(clipRectIn);

    TTile t1(_rasOut, ::convert(clipRectOut.getP00()));
    TTile t2(_rasIn, ::convert(clipRectIn.getP00()));

    TRop::convert(t1, t2, palette, transparencyCheck, false);
  }
}

//-----------------------------------------------------------------------------

void TRop::convert(const TTile &dst, const TTile &src, const TPaletteP plt,
                   bool transparencyCheck, bool applyFxs) {
  // if (dst->getSize() != src->getSize())
  //  throw TRopException("convert: size mismatch");

  // assert(plt);
  if ((TRaster32P)dst.getRaster())
    do_convert(dst, src, plt, transparencyCheck, applyFxs);
  else if ((TRaster64P)dst.getRaster()) {
    TRaster32P aux(dst.getRaster()->getLx(), dst.getRaster()->getLy());
    TTile taux(aux, dst.m_pos);
    do_convert(taux, src, plt, transparencyCheck, applyFxs);
    TRop::convert(dst.getRaster(), aux);
  } else
    throw TRopException("unsupported pixel type");
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

namespace {

TRasterP putSinglePaintInRaster(TRasterCM32P &rasIn, int paintId,
                                TPixel32 color) {
  TRaster32P rasOut;

  for (int y = 0; y < rasIn->getLy(); y++) {
    TPixelCM32 *pixIn  = rasIn->pixels(y);
    TPixelCM32 *endPix = pixIn + rasIn->getLx();
    TPixel32 *pixOut   = 0;

    if (rasOut) pixOut = rasOut->pixels(y);

    while (pixIn < endPix) {
      if (pixIn->getPaint() == paintId) {
        if (!rasOut) {
          rasOut = TRaster32P(rasIn->getLx(), rasIn->getLy());
          rasOut->fill(TPixel32::Transparent);
          pixOut = rasOut->pixels(y) + (pixIn - rasIn->pixels(y));
        }

        if (!pixIn->isPureInk()) *pixOut = color;
        /*else if (!pixIn->isPureInk())
  {
        assert(TPixelCM32::getMaxTone()==0xff);
        pixOut->m = pixIn->getTone();
        pixOut->r = 255*color.r/pixOut->m;
        pixOut->g = 255*color.g/pixOut->m;
        pixOut->b = 255*color.b/pixOut->m;
}*/
        pixIn->setPaint(0);
        // pixIn->setTone(0);
      }
      pixIn++;
      pixOut++;
    }
  }
  return rasOut;
}

/*------------------------------------------------------------------------*/

TRasterP putSingleInkInRasterGR8(TRasterCM32P &rasIn, int inkId) {
  TRasterGR8P rasOut;

  for (int y = 0; y < rasIn->getLy(); y++) {
    TPixelCM32 *pixIn  = rasIn->pixels(y);
    TPixelCM32 *endPix = pixIn + rasIn->getLx();
    TPixelGR8 *pixOut;

    if (rasOut) pixOut = rasOut->pixels(y);

    while (pixIn < endPix) {
      if (pixIn->getInk() == inkId) {
        assert(TPixelCM32::getMaxTone() == 0xff);
        if (!rasOut) {
          rasOut = TRasterGR8P(rasIn->getLx(), rasIn->getLy());
          rasOut->fill(0);
          pixOut = rasOut->pixels(y) + (pixIn - rasIn->pixels(y));
        }

        *pixOut = 255 - pixIn->getTone();
        // pixIn->setInk(0);
        pixIn->setTone(TPixelCM32::getMaxTone());
      }

      pixIn++;
      pixOut++;
    }
  }
  return rasOut;
}

/*------------------------------------------------------------------------*/
TRasterP putSingleInkInRasterRGBM(TRasterCM32P &rasIn, int inkId) {
  TRaster32P rasOut;

  for (int y = 0; y < rasIn->getLy(); y++) {
    TPixelCM32 *pixIn  = rasIn->pixels(y);
    TPixelCM32 *endPix = pixIn + rasIn->getLx();
    TPixel *pixOut;

    if (rasOut) pixOut = rasOut->pixels(y);

    while (pixIn < endPix) {
      if (pixIn->getInk() == inkId) {
        assert(TPixelCM32::getMaxTone() == 0xff);
        if (!rasOut) {
          rasOut = TRaster32P(rasIn->getLx(), rasIn->getLy());
          rasOut->fill(TPixel32::Transparent);
          pixOut = rasOut->pixels(y) + (pixIn - rasIn->pixels(y));
        }

        pixOut->r = pixOut->g = pixOut->b = pixOut->m = 255 - pixIn->getTone();
        // pixIn->setInk(0);
        pixIn->setTone(TPixelCM32::getMaxTone());
      }
      pixIn++;
      if (rasOut) pixOut++;
    }
  }
  return rasOut;
}

/*------------------------------------------------------------------------*/
/*------------------------------------------------------------------------*/

// filename!!
// interactive!!

bool computePaletteFx(const std::vector<std::pair<TColorStyle *, int>> &fx,
                      const TTile &tileOut, const TTile &tileIn,
                      const TPaletteP plt) {
  int i;
  TRasterCM32P rasIn = tileIn.getRaster();
  TRaster32P rAux32, rasOut = tileOut.getRaster();
  TRasterGR8P rAuxGR;

  int frame = plt->getFrame();

  std::vector<TRasterP> paintLayers(fx.size());
  std::vector<TRasterP> inkLayers(fx.size());

  // tolgo dal raster d'ingresso gli ink e i paint con gli effetti, mettendoli
  // in dei raster layer
  for (i = 0; i < (int)fx.size(); i++) {
    TRasterStyleFx *rfx = fx[i].first->getRasterStyleFx();

    if (rfx->isPaintStyle())
      paintLayers[i] = putSinglePaintInRaster(rasIn, fx[i].second,
                                              fx[i].first->getMainColor());
    if (rfx->isInkStyle()) {
      if (rfx->inkFxNeedRGBMRaster())
        inkLayers[i] = putSingleInkInRasterRGBM(rasIn, fx[i].second);
      else
        inkLayers[i] = putSingleInkInRasterGR8(rasIn, fx[i].second);
    }
  }

  // raster d'ingresso senza i colori fx, viene renderizzato nel raster d'uscita

  do_convert(tileOut, tileIn, plt, false, false);

  // ogni layer viene "effettato".; se il risultato e' non nullo, viene
  // sovrapposto sul raster d'uscita
  // prima vengono messi tutti i layer di paint, poi quelli di ink

  TRect rectOut =
      TRect(convert(tileOut.m_pos), tileOut.getRaster()->getSize()) -
      convert(tileIn.m_pos);

  for (i = 0; i < (int)fx.size(); i++)
    if (paintLayers[i]) {
      TRasterStyleFx::Params params(paintLayers[i], convert(tileIn.m_pos),
                                    rasIn, fx[i].second, frame);
      if (fx[i].first->getRasterStyleFx()->compute(params))
        TRop::over(rasOut, paintLayers[i]->extract(rectOut), rasOut);
    }

  for (i = 0; i < (int)fx.size(); i++)
    if (inkLayers[i]) {
      TRasterStyleFx *rfx = fx[i].first->getRasterStyleFx();

      TRasterStyleFx::Params params(inkLayers[i], convert(tileIn.m_pos), rasIn,
                                    fx[i].second, frame);
      if (rfx->compute(params)) {
        if (rfx->inkFxNeedRGBMRaster())
          TRop::over(rasOut, rasOut, inkLayers[i]->extract(rectOut));
        else
          TRop::over(rasOut, inkLayers[i]->extract(rectOut),
                     fx[i].first->getMainColor());
      }
    }

  return true;
}

/*------------------------------------------------------------------------*/

}  // namespace

/*------------------------------------------------------------------------*/

bool renderRas32(const TTile &tileOut, const TTile &tileIn,
                 const TPaletteP palette) {
  assert(TRect(convert(tileIn.m_pos), tileIn.getRaster()->getSize())
             .contains(TRect(convert(tileOut.m_pos),
                             tileOut.getRaster()->getSize())));
  assert((TRasterCM32P)tileIn.getRaster());

  // Shrink = shrink;
  // INIT_TCM(rin)

  /* mark up are made apart */
  // computeMarkup(rasIn, palette);

  std::vector<std::pair<TColorStyle *, int>> fxArray;

  for (int i = 0; i < palette->getStyleCount(); i++)
    if (palette->getStyle(i)->isRasterStyle())
      fxArray.push_back(std::pair<TColorStyle *, int>(palette->getStyle(i), i));

  if (fxArray.empty()) return false;

  TTile _tileIn(tileIn.getRaster()->clone(), tileIn.m_pos);

  tileIn.getRaster()->lock();
  tileOut.getRaster()->lock();
  computePaletteFx(fxArray, tileOut, _tileIn, palette);
  tileIn.getRaster()->unlock();
  tileOut.getRaster()->unlock();

  return true;
}

/*------------------------------------------------------------------------*/

//--------------------------------------------------------------------------------------------

namespace {

void addColor(TPaletteP plt, int index, const TPaletteP &upPlt,
              std::map<int, int> &usedInks) {
  TColorStyle *cs = upPlt->getStyle(index);
  if (cs && cs->getMainColor() == plt->getStyle(index)->getMainColor()) {
    usedInks[index] = index;
    return;
  }
  int firstStyleId                     = plt->getFirstUnpagedStyle();
  if (firstStyleId == -1) firstStyleId = plt->getStyleCount();
  usedInks[index]                      = firstStyleId;
  plt->getPage(0)->addStyle(TPixel32::Red);
}

void addColor(TPaletteP plt, int index, std::map<int, int> &usedInks) {
  int firstStyleId                     = plt->getFirstUnpagedStyle();
  if (firstStyleId == -1) firstStyleId = plt->getStyleCount();
  usedInks[index]                      = firstStyleId;
  plt->getPage(0)->addStyle(TPixel32::Red);
}

// Check if the downPlt has the same style (same color, same index) and use it.
// If the same style is not found, then add it as a new style.
void tryMergeInkOrAddColor(TPaletteP downPlt, const TPaletteP matchPlt,
                           int index, std::map<int, int> &usedInks) {
  if (0 <= index && index < downPlt->getStyleCount() &&
      index < matchPlt->getStyleCount()) {
    TSolidColorStyle *solidDownStyle =
        dynamic_cast<TSolidColorStyle *>(downPlt->getStyle(index));
    TSolidColorStyle *solidMatchStyle =
        dynamic_cast<TSolidColorStyle *>(matchPlt->getStyle(index));
    if (solidDownStyle && solidMatchStyle &&
        solidDownStyle->getMainColor() == solidMatchStyle->getMainColor()) {
      usedInks[index] = index;
      return;
    }
  }
  addColor(downPlt, index, usedInks);
}

//------------------------------------------------------------
// std::map<int,int>& usedInk  upInkId -> downInkId
void doMergeCmapped(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                    const TPaletteP &pltOut, const TPaletteP &matchPlt,
                    bool onlyInks, int matchlinePrevalence,
                    std::map<int, int> &usedInks, bool mergePalette) {
  double val = matchlinePrevalence / 100.0;  // matchlinePrevalence ==0 always
                                             // ink down; matchlinePrevalence ==
                                             // 100 always ink up;
  assert(rasOut && rasUp);
  assert(rasOut->getSize() == rasUp->getSize());

  TPaletteP downPlt = pltOut->clone();
  std::map<int, int>::iterator it;
  for (it = usedInks.begin(); it != usedInks.end(); it++)
    downPlt->getPage(0)->addStyle(TPixel32::Red);
  for (int y = 0; y < rasOut->getLy(); y++) {
    TPixelCM32 *pixDown = rasOut->pixels(y),
               *endPix  = pixDown + rasOut->getLx();
    TPixelCM32 *pixUp   = rasUp->pixels(y);

    while (pixDown < endPix) {
      // there is lines in matchline
      if (!pixUp->isPurePaint()) {
        int toneDown = pixDown->getTone();
        int toneUp   = pixUp->getTone();
        if (usedInks.find(pixUp->getInk()) == usedInks.end()) {
          if (mergePalette)
            tryMergeInkOrAddColor(downPlt, matchPlt, pixUp->getInk(), usedInks);
          else
            addColor(downPlt, pixUp->getInk(), usedInks);
        }

        if (val == 1) {  // Matchline is on top, with gap
          pixDown->setTone(toneUp);
          pixDown->setInk(usedInks[pixUp->getInk()]);
        } else if (val == 0) {  // Matchline is on bottom, with gap
          if (pixDown->isPurePaint()) {
            pixDown->setTone(toneUp);
            pixDown->setInk(usedInks[pixUp->getInk()]);
          } else {
            //*pixOut = *pixDown;
          }
        } else {
          if ((val > 0 && toneUp < toneDown) ||
              (val == 0 && toneDown == 255))  //(toneUp<toneDown)
            pixDown->setTone(toneUp);

          if ((255 - toneDown) * (1 - val) <=
              val * (255 - toneUp - 1))  // val==0 -> if (toneDown== 255)....
            // val==0.5 -> if (toneup<toneDown)...
            // val==1 -> if (toneup<255)...
            pixDown->setInk(usedInks[pixUp->getInk()]);
        }
      }
      int paintIndex;
      if (!onlyInks && (paintIndex = pixUp->getPaint()) > 0) {
        if (usedInks.find(paintIndex) == usedInks.end())
          addColor(downPlt, paintIndex, usedInks);
        pixDown->setPaint(usedInks[paintIndex]);
      }
      pixUp++;
      pixDown++;
      // pixOut++;
    }
  }
}
/*------------------------------------------------------------------------*/

void addColors(const TPixelCM32 &color, TPaletteP plt, const TPaletteP &upPlt,
               std::map<int, int> &usedColors) {
  if (usedColors.find(color.getInk()) == usedColors.end())
    addColor(plt, color.getInk(), upPlt, usedColors);
  if (usedColors.find(color.getPaint()) == usedColors.end())
    addColor(plt, color.getPaint(), upPlt, usedColors);
}

//---------------------------------------------------------------------------
void addColors(const TPixelCM32 &color, TPaletteP plt,
               std::map<int, int> &usedColors) {
  if (usedColors.find(color.getInk()) == usedColors.end())
    addColor(plt, color.getInk(), usedColors);
  if (usedColors.find(color.getPaint()) == usedColors.end())
    addColor(plt, color.getPaint(), usedColors);
}

/*------------------------------------------------------------------------*/

void doApplyMatchLines(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                       int inkIndex, int matchlinePrevalence) {
  double val = matchlinePrevalence / 100.0;  // matchlinePrevalence ==0->always
                                             // ink down; matchlinePrevalence ==
                                             // 100 always ink up;

  assert(rasOut && rasUp);
  assert(rasOut->getSize() == rasUp->getSize());

  for (int y = 0; y < rasOut->getLy(); y++) {
    TPixelCM32 *pixDown = rasOut->pixels(y),
               *endPix  = pixDown + rasOut->getLx();
    TPixelCM32 *pixUp   = rasUp->pixels(y);

    while (pixDown < endPix) {
      if (!pixUp->isPurePaint()) {
        int toneDown = pixDown->getTone();
        int toneUp   = pixUp->getTone();
        if (val == 1) {  // Matchline is on top, with gap
          pixDown->setTone(toneUp);
          pixDown->setInk(inkIndex);
        } else if (val == 0) {  // Matchline is on bottom, with gap
          if (pixDown->isPurePaint()) {
            pixDown->setTone(toneUp);
            pixDown->setInk(inkIndex);
          } else {
          }
          //*pixOut = *pixDown;
        }
        if ((val > 0 && toneUp < toneDown) || (val == 0 && toneDown == 255))
          pixDown->setTone(toneUp);

        if ((255 - toneDown) * (1 - val) <=
            val * (255 - toneUp - 1))  // val==0 -> if (toneDown == 255)....
          // val==0.5 -> if (toneup<toneDown)...
          // val==1 -> if (toneup<255)...
          pixDown->setInk(inkIndex);
      }

      pixUp++;
      pixDown++;
    }
  }
}
/*------------------------------------------------------------------------*/

}  // namespace

#ifdef LEVO

void TRop::eraseColors(TRasterCM32P ras, vector<int> &colorIds, bool eraseInks,
                       bool keepColor) {
  assert(ras);

  vector<int> curColorIds;
  std::sort(colorIds.begin(), colorIds.end());

  if (!keepColor)
    curColorIds = colorIds;
  else {
    // prendo tutti i colori ECCETTO quelli nel vettore colorIds
    unsigned int count1 = 0, count2 = 0;
    int size = eraseInks ? TPixelCM32::getMaxInk() : TPixelCM32::getMaxPaint();

    curColorIds.resize(size + 1 - colorIds.size());
    for (int i = 0; i < size; i++)
      if (count1 < colorIds.size() && colorIds[count1] == i)
        count1++;
      else
        curColorIds[count2++] = i;
  }

  for (int y = 0; y < ras->getLy(); y++) {
    TPixelCM32 *pix = ras->pixels(y), *endPix = pix + ras->getLx();

    while (pix < endPix) {
      unsigned int i;
      int color = eraseInks ? pix->getInk() : pix->getPaint();

      if (color != 0)
        for (i = 0; i < curColorIds.size() && curColorIds[i] <= color; i++)
          if (color == curColorIds[i]) {
            *pix = eraseInks ? TPixelCM32(0, pix->getPaint(),
                                          TPixelCM32::getMaxTone())
                             : TPixelCM32(pix->getInk(), 0, pix->getTone());
            break;
          }
      pix++;
    }
  }
}

#endif

/*------------------------------------------------------------------------*/

void TRop::eraseColors(TRasterCM32P ras, std::vector<int> *colorIds,
                       bool eraseInks) {
  if (colorIds) std::sort(colorIds->begin(), colorIds->end());

  for (int y = 0; y < ras->getLy(); y++) {
    TPixelCM32 *pix = ras->pixels(y), *endPix = pix + ras->getLx();

    for (; pix < endPix; pix++) {
      unsigned int i = 0;
      int color      = eraseInks ? pix->getInk() : pix->getPaint();

      if (color == 0) continue;

      if (colorIds) {
        while (i < colorIds->size() && (*colorIds)[i] < color) i++;
        if (i == colorIds->size() || color != (*colorIds)[i]) continue;
      }

      if (eraseInks) {
        pix->setInk(0);
        pix->setTone(TPixelCM32::getMaxTone());
      } else
        pix->setPaint(0);
    }
  }
}

//--------------------------------
/*
void TRop::overaCmapped(TRasterCM32P rasOut, const TRasterCM32P& rasUp, const
TPaletteP &pltOut, int matchlinePrevalence, std::map<int,int>& usedColors)
{
doMergeCmapped(rasOut, rasUp, pltOut, false, matchlinePrevalence, usedColors);
}
*/
//-----------------------------------------------------------------

void TRop::applyMatchLines(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                           const TPaletteP &pltOut, const TPaletteP &matchPlt,
                           int inkIndex, int matchlinePrevalence,
                           std::map<int, int> &usedInks) {
  assert(matchlinePrevalence >= 0);
  if (inkIndex == -1)
    doMergeCmapped(rasOut, rasUp, pltOut, matchPlt, true, matchlinePrevalence,
                   usedInks, false);
  else if (inkIndex == -2)
    doMergeCmapped(rasOut, rasUp, pltOut, matchPlt, true, matchlinePrevalence,
                   usedInks, true);
  else
    doApplyMatchLines(rasOut, rasUp, inkIndex, matchlinePrevalence);
}

/*------------------------------------------------------------------------*/

void TRop::eraseStyleIds(TToonzImage *image, const std::vector<int> styleIds) {
  assert(image);
  TRasterCM32P ras = image->getRaster();

  int i;
  for (i = 0; i < (int)styleIds.size(); i++) {
    int styleId = styleIds[i];
    ras->lock();
    for (int y = 0; y < ras->getLy(); y++) {
      TPixelCM32 *pix = ras->pixels(y), *endPix = pix + ras->getLx();
      while (pix < endPix) {
        bool isPaint = (pix->getPaint() == styleId);
        bool isInk   = (pix->getInk() == styleId);
        if (!isPaint && !isInk) {
          pix++;
          continue;
        } else if (isPaint && !isInk)
          *pix = TPixelCM32(pix->getInk(), 0, pix->getTone());
        else if (!isPaint && isInk)
          *pix = TPixelCM32(0, pix->getPaint(), TPixelCM32::getMaxTone());
        else if (isPaint && isInk)
          *pix = TPixelCM32(0, 0, pix->getTone());
        else
          assert(0);
        pix++;
      }
    }
    ras->unlock();
  }
}

/*------------------------------------------------------------------------*/

inline bool isTransparent(TPixelCM32 *pix) {
  return (((*((ULONG *)pix)) & 0x000fffff) == 0xff);
}

void TRop::overlayCmapped(TRasterCM32P rasOut, const TRasterCM32P &rasUp,
                          const TPaletteP &pltOut, const TPaletteP &upPlt,
                          std::map<int, int> &usedColors) {
  assert(rasOut && rasUp);
  assert(rasOut->getSize() == rasUp->getSize());

  TPaletteP downPlt = pltOut->clone();
  std::map<int, int>::iterator it;
  for (it = usedColors.begin(); it != usedColors.end(); it++)
    downPlt->getPage(0)->addStyle(TPixel32::Red);

  for (int y = 0; y < rasOut->getLy(); y++) {
    TPixelCM32 *pixDown = rasOut->pixels(y);
    TPixelCM32 *pixUp   = rasUp->pixels(y);

    for (int x = 0; x < rasOut->getLx(); x++, pixUp++, pixDown++) {
      if (isTransparent(pixUp))  // WARNING! cannot check transparent pixels
                                 // with *pixup==TPixelCM32() since also
                                 // 0x183000ff i.e., is a valid transparent
                                 // value
        continue;
      int tone = pixUp->getTone();
      if (isTransparent(pixDown) || tone == 255 || tone == 0 ||
          pixUp->getPaint() !=
              0)  // up e' punto interno, o esterno non antialiasato
      {
        addColors(*pixUp, downPlt, upPlt, usedColors);
        pixDown->setInk(usedColors[pixUp->getInk()]);
        pixDown->setPaint(usedColors[pixUp->getPaint()]);
        pixDown->setTone(tone);
      } else if (tone <= pixDown->getTone() || tone < 128)  // up e' bordo
                                                            // esterno
                                                            // antialiasato piu'
                                                            // opaco di down
      {
        int ink = pixUp->getInk();
        if (usedColors.find(ink) == usedColors.end())
          addColor(downPlt, ink, upPlt, usedColors);
        pixDown->setInk(usedColors[ink]);
        pixDown->setTone(tone < 128 ? 0 : tone);
      }
    }
  }
}

//-----------------------------------------------------

namespace {

#define MAGICFAC (257U * 256U + 1U)
/*
#define PIX_CM32_PENMAP_IDX(PIX) ((PIX)>>12 & 0xfff00 | (PIX) & 0xff)
#define PIX_CM32_COLMAP_IDX(PIX) ((PIX) & 0xfffff)


#define PIX_CM32_PENMAP_COLMAP_TO_RGBM(PIX,PENMAP,COLMAP,RES) \
  { \
  ULONG _r = (PIX); \
  ULONG _s = ((ULONG *)(PENMAP))[MY_PIX_CM32_PENMAP_IDX(_r)] + \
             ((ULONG *)(COLMAP))[MY_PIX_CM32_COLMAP_IDX(_r)]; \
  (RES) = *(LPIXEL *)(&_s); \
  }
  */

/*
void fillCmapRamp (RAS_CMAP& cmap, const TPixel32& color, int index)
{
index = index << cmap.info.tone_bits;
int xedni = index + cmap.info.n_tones-1;

TPixel32 _color=color;
        UINT fac = MAGICFAC * _color.m;
        _color.r=(UINT)(_color.r * fac + (1U<<23))>>24;
        _color.b=(UINT)(_color.b * fac + (1U<<23))>>24;
        _color.g=(UINT)(_color.g * fac + (1U<<23))>>24;

for (int tone = 0; tone < cmap.info.n_tones; tone++)
  {
  LPIXEL val;
  UINT magic_tone = tone * MAGICFAC;
  val.r = (UCHAR)((_color.r * magic_tone + (1<<23)) >> 24);
  val.g = (UCHAR)((_color.g * magic_tone + (1<<23)) >> 24);
  val.b = (UCHAR)((_color.b * magic_tone + (1<<23)) >> 24);
  val.m = (UCHAR)((_color.m * magic_tone + (1<<23)) >> 24);

  cmap.colbuffer[index++] = val;
  cmap.penbuffer[xedni--] = val;
  }
}
*/

/*---------------------------------------------------------------------------*/

}  // namespace

/*---------------------------------------------------------------------------*/

// \b NOTE: Starting from Toonz 6.1, some important improvements are introduced:
// a) The passed raster is now referenced by the returned _RASTER*, just the
// same way
//    smartpointer to rasters do.
// b) The cache is made aware of the passed raster, mainly because these old 4.6
// raster
//    structures are essentially used for memory-consuming operations with tlv
//    fxs and may
//    need to be shipped to hard disk on critical situations (a matter handled
//    by the cache).
// c) The lockRaster and unlockRaster functions are provided. They are meant to
// specify whether
//    the raster is actively referenced by the raster pointer, or is rather in a
//    lazy state -
//    so that the cache may move it to hard disk if necessary.

/*
static const TCM_INFO Tcm_my_default_info  = {  8,  8, 12,  20, 12,
                                               0x0000, 0x00ff,
                                               256, 4096, 256
                                             };*/

/*---------------------------------------------------------------------------*/

inline LPIXEL premultiplyLPIXEL(const TPixel32 &pix) {
  // int MAGICFAC = (257U * 256U + 1U);
  UINT fac = MAGICFAC * pix.m;
  LPIXEL out;
  out.r = ((UINT)(pix.r * fac + (1U << 23)) >> 24);
  out.g = ((UINT)(pix.g * fac + (1U << 23)) >> 24);
  out.b = ((UINT)(pix.b * fac + (1U << 23)) >> 24);
  out.m = pix.m;
  return out;
}

_RASTER *TRop::convertRaster50to46(const TRasterP &inRas,
                                   const TPaletteP &inPalette) {
  int lx   = inRas->getLx();
  int ly   = inRas->getLy();
  int wrap = inRas->getWrap();

  assert(lx > 0 && ly > 0);
  assert(inRas->getRawData());
  TRasterGR8P rgr8   = (TRasterGR8P)inRas;
  TRasterGR16P rgr16 = (TRasterGR16P)inRas;
  TRaster32P r32     = (TRaster32P)inRas;
  TRaster64P r64     = (TRaster64P)inRas;
  TRasterCM32P rcm   = (TRasterCM32P)inRas;

  RASTER *rout = new RASTER;
  memset(rout, 0, sizeof(RASTER));

  std::string id(TImageCache::instance()->getUniqueId());
  rout->cacheIdLength = id.size();
  rout->cacheId       = new char[rout->cacheIdLength];
  memcpy(rout->cacheId, id.data(), sizeof(char) * rout->cacheIdLength);
  {
    TImageP img;
    if (rcm)
      img = TToonzImageP(rcm, rcm->getBounds());  // saveBox is not considered
                                                  // in RASTER struct anyway
    else
      img = TRasterImageP(inRas);
    TImageCache::instance()->add(id, img);
  }
  inRas->addRef();

  rout->buffer        = inRas->getRawData();
  TRasterP parent     = inRas->getParent();
  rout->native_buffer = (parent) ? parent->getRawData() : inRas->getRawData();
  rout->lx            = lx;
  rout->ly            = ly;
  rout->wrap          = wrap;

  if (rgr8)
    rout->type = RAS_GR8;
  else if (rgr16)
    rout->type = RAS_GR16;
  else if (r32)
    rout->type = RAS_RGBM;
  else if (r64)
    rout->type = RAS_RGBM64;
  else if (rcm)
    rout->type = RAS_CM32;
  else
    assert(!"raster type not convertible!");

  if (rout->type != RAS_CM32) return rout;

  if (!inPalette) {
    assert(!"missing palette!");
    return NULL;
  }

  rout->cmap.info   = Tcm_32_default_info;
  rout->cmap.buffer = new LPIXEL[TCM_CMAP_PENBUFFER_SIZE(rout->cmap.info)];
  // rout->cmap.penbuffer = new LPIXEL[TCM_CMAP_PENBUFFER_SIZE
  // (rout->cmap.info)];
  // rout->cmap.colbuffer = new LPIXEL[TCM_CMAP_COLBUFFER_SIZE
  // (rout->cmap.info)];

  for (int i = 0; i < inPalette->getStyleCount(); i++)
    rout->cmap.buffer[i] =
        ::premultiplyLPIXEL(inPalette->getStyle(i)->getAverageColor());
  // fillCmapRamp (rout->cmap, inPalette->getStyle(i)->getMainColor(), i);

  return rout;
}

/*------------------------------------------------------------------------*/

void TRop::releaseRaster46(_RASTER *&r, bool doReleaseBuffer) {
  // Buffer release no more supported. These are now intended as smart
  // pointers to rasters - they release themselves on their own.
  assert(!doReleaseBuffer);

  if (r->type == RAS_CM32) {
    delete[] r->cmap.buffer;
    // delete [] r->cmap.penbuffer;
    // delete [] r->cmap.colbuffer;
  }

  if (doReleaseBuffer && r->native_buffer == r->buffer)
    delete r->buffer;  // Should not happen

  // Unlock if locked, and remove the cache reference
  if (r->buffer) unlockRaster(r);
  TImageCache::instance()->remove(std::string(r->cacheId, r->cacheIdLength));
  delete[] r->cacheId;

  delete r;
  r = 0;
}

/*------------------------------------------------------------------------*/

void TRop::lockRaster(RASTER *raster) {
  TImageP img(TImageCache::instance()->get(
      std::string(raster->cacheId, raster->cacheIdLength), true));
  TRasterP cacheRas;
  if (raster->type == RAS_CM32)
    cacheRas = TToonzImageP(img)->getRaster();
  else
    cacheRas = TRasterImageP(img)->getRaster();
  cacheRas->addRef();
  raster->buffer  = cacheRas->getRawData();
  TRasterP parent = cacheRas->getParent();
  raster->native_buffer =
      (parent) ? parent->getRawData() : cacheRas->getRawData();
}

/*------------------------------------------------------------------------*/

void TRop::unlockRaster(RASTER *raster) {
  TImageP img(TImageCache::instance()->get(
      std::string(raster->cacheId, raster->cacheIdLength), true));
  TRasterP cacheRas;
  if (raster->type == RAS_CM32)
    cacheRas = TToonzImageP(img)->getRaster();
  else
    cacheRas = TRasterImageP(img)->getRaster();
  cacheRas->release();
  raster->buffer        = 0;
  raster->native_buffer = 0;
}

/*------------------------------------------------------------------------*/

_RASTER *TRop::readRaster46(const char *filename) {
  // No more called in Toonz...

  TImageP img;

  TImageReader::load(TFilePath(filename), img);

  if ((TToonzImageP)img) return 0;

  TRasterImageP rimg = (TRasterImageP)img;

  if (!rimg) return 0;
  TRasterP ras = rimg->getRaster();

  return TRop::convertRaster50to46(ras, 0);
}

namespace {

inline TPixel32 getPix32(const TPixelCM32 &pixcm,
                         const std::vector<TPixel32> &colors) {
  int t = pixcm.getTone();
  int p = pixcm.getPaint();
  int i = pixcm.getInk();

  if (t == TPixelCM32::getMaxTone())
    return colors[p];
  else if (t == 0)
    return colors[i];
  else
    return blend(colors[i], colors[p], t, TPixelCM32::getMaxTone());
}

}  // namespace

// from 4.6

#ifdef LEVO

void TRop::zoomOutCm32Rgbm(const TRasterCM32P &rin, TRaster32P &rout,
                           const TPalette &plt, int x1, int y1, int x2, int y2,
                           int newx, int newy, int absZoomLevel) {
  TPixelCM32 *rowin, *pixin, *in, win;
  TPixel32 *rowout, *pixout, valin, valout;
  int tmp_r, tmp_g, tmp_b, tmp_m;
  int wrapin, wrapout;
  int x, y, lx, ly, xlast, ylast, xrest, yrest, i, j;
  int factor, fac_fac_2_bits;
  int fac_fac, yrest_fac, fac_xrest, yrest_xrest;
  int fac_fac_2, yrest_fac_2, fac_xrest_2, yrest_xrest_2;
  int fac_fac_4;

  int count = plt.getStyleCount();
  int count2 =
      std::max(count, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint());
  std::vector<TPixel32> colors(count2);
  for (i      = 0; i < plt.getStyleCount(); i++)
    colors[i] = ::premultiply(plt.getStyle(i)->getAverageColor());

  lx             = x2 - x1 + 1;
  ly             = y2 - y1 + 1;
  factor         = 1 << absZoomLevel;
  xrest          = lx & (factor - 1);
  yrest          = ly & (factor - 1);
  xlast          = x2 - xrest + 1;
  ylast          = y2 - yrest + 1;
  fac_fac        = factor * factor;
  fac_fac_2      = fac_fac >> 1;
  fac_fac_4      = fac_fac >> 2;
  fac_fac_2_bits = 2 * absZoomLevel - 1;
  yrest_fac      = yrest * factor;
  yrest_fac_2    = yrest_fac >> 1;
  fac_xrest      = factor * xrest;
  fac_xrest_2    = fac_xrest >> 1;
  yrest_xrest    = yrest * xrest;
  yrest_xrest_2  = yrest_xrest >> 1;
  wrapin         = rin->getWrap();
  wrapout        = rout->getWrap();
  valout.m       = 0xff;

  rowin  = (TPixelCM32 *)rin->getRawData() + wrapin * y1 + x1;
  rowout = (TPixel32 *)rout->getRawData() + wrapout * newy + newx;
  for (y = y1; y < ylast; y += factor) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      in                            = pixin;
      for (j = 0; j < factor; j += 2) {
        for (i = 0; i < factor; i += 2) {
          win   = *in;
          valin = getPix32(win, colors);
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          in += wrapin;
          in++;
          win   = *in;
          valin = getPix32(win, colors);
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
          in -= wrapin;
          in++;
        }
        in += 2 * wrapin - factor;
      }
      valout.r  = (tmp_r + fac_fac_4) >> fac_fac_2_bits;
      valout.g  = (tmp_g + fac_fac_4) >> fac_fac_2_bits;
      valout.b  = (tmp_b + fac_fac_4) >> fac_fac_2_bits;
      valout.m  = (tmp_m + fac_fac_4) >> fac_fac_2_bits;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < factor; j++)
        for (i = 0; i < xrest; i++) {
          win   = pixin[i + j * wrapin];
          valin = getPix32(win, colors);
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + fac_xrest_2) / fac_xrest;
      valout.g = (tmp_g + fac_xrest_2) / fac_xrest;
      valout.b = (tmp_b + fac_xrest_2) / fac_xrest;
      valout.m = (tmp_m + fac_xrest_2) / fac_xrest;
      *pixout  = valout;
    }
    rowin += wrapin * factor;
    rowout += wrapout;
  }
  if (yrest) {
    pixin  = rowin;
    pixout = rowout;
    for (x = x1; x < xlast; x += factor) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < factor; i++) {
          win   = pixin[i + j * wrapin];
          valin = getPix32(win, colors);
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r  = (tmp_r + yrest_fac_2) / yrest_fac;
      valout.g  = (tmp_g + yrest_fac_2) / yrest_fac;
      valout.b  = (tmp_b + yrest_fac_2) / yrest_fac;
      valout.m  = (tmp_m + yrest_fac_2) / yrest_fac;
      *pixout++ = valout;
      pixin += factor;
    }
    if (xrest) {
      tmp_r = tmp_g = tmp_b = tmp_m = 0;
      for (j = 0; j < yrest; j++)
        for (i = 0; i < xrest; i++) {
          win   = pixin[i + j * wrapin];
          valin = getPix32(win, colors);
          tmp_r += valin.r;
          tmp_g += valin.g;
          tmp_b += valin.b;
          tmp_m += valin.m;
        }
      valout.r = (tmp_r + yrest_xrest_2) / yrest_xrest;
      valout.g = (tmp_g + yrest_xrest_2) / yrest_xrest;
      valout.b = (tmp_b + yrest_xrest_2) / yrest_xrest;
      valout.m = (tmp_m + yrest_xrest_2) / yrest_xrest;
      *pixout  = valout;
    }
  }
}

#endif

/*-----------------------------------------------------------------------------*/

#ifdef LEVO

void TRop::makeIcon(TRaster32P &_rout, const TRasterCM32P &rin,
                    const TPaletteP &palette, bool onBlackBg) {
  int i, j;
  int lx = rin->getLx();
  int ly = rin->getLy();

  int count = palette->getStyleCount();
  int count2 =
      std::max(count, TPixelCM32::getMaxInk(), TPixelCM32::getMaxPaint());
  std::vector<TPixel32> colors(count2);
  for (i = 0; i < palette->getStyleCount(); i++)
    colors[i] =
        /*::premultiply(*/ palette->getStyle(i)->getAverageColor();  //);

  TDimension dim(_rout->getSize());
  TRaster32P rout;

  double arIn  = (double)lx / ly;
  double arOut = (double)dim.lx / dim.ly;
  if (!areAlmostEqual(arIn, arOut, 1e-2))  // do not want a stretched icon! I
                                           // extract a subraster with same
                                           // aspect ration of rin
  {
    int newlx, newly;
    if (arOut < arIn) {
      newlx = dim.lx;
      newly = dim.lx / arIn;
    } else {
      newly = dim.ly;
      newlx = dim.ly * arIn;
    }
    _rout->clear();
    rout = _rout->extract((dim.lx - newlx) / 2, (dim.ly - newly) / 2,
                          (dim.lx - newlx) / 2 + newlx - 1,
                          (dim.ly - newly) / 2 + newly - 1);
    dim = rout->getSize();
  } else
    rout = _rout;

  TPixel32 *pixOut0 = (TPixel32 *)rout->getRawData();

  int countY   = 0;
  bool newRow  = true;
  int currTone = TPixelCM32::getMaxTone();

  for (i = 0; i < ly; i++) {
    TPixelCM32 *pixIn = (TPixelCM32 *)rin->pixels(i);
    TPixel32 *pixOut  = pixOut0;
    int countX        = 0;
    for (j = 0; j < lx; j++) {
      if (newRow || currTone > pixIn->getTone()) {
        currTone = pixIn->getTone();
        if (onBlackBg)
          *pixOut = overPixOnBlack(getPix32(*pixIn, colors));
        else
          *pixOut = overPixOnWhite(getPix32(*pixIn, colors));
      }
      pixIn++;
      countX += dim.lx;
      if (countX >= lx)
        countX -= lx, pixOut++, currTone = TPixelCM32::getMaxTone();
    }

    countY += dim.ly;
    if (countY >= ly) {
      countY -= ly;
      pixOut0 += rout->getWrap();
      currTone = TPixelCM32::getMaxTone();
      newRow   = true;
    } else
      newRow = false;
  }
}

#endif

/*-----------------------------------------------------------------------------*/

void TRop::makeIcon(TRasterCM32P &_rout, const TRasterCM32P &rin) {
  int i, j;
  int lx = rin->getLx();
  int ly = rin->getLy();

  TDimension dim(_rout->getSize());
  TRasterCM32P rout;

  rout = _rout;

  rout->lock();
  rin->lock();
  TPixelCM32 *pixOut0 = (TPixelCM32 *)rout->getRawData();

  int countY  = 0;
  bool newRow = true;

  for (i = 0; i < ly; i++) {
    TPixelCM32 *pixIn  = (TPixelCM32 *)rin->pixels(i);
    TPixelCM32 *pixOut = pixOut0;
    int countX         = 0;
    bool newColumn     = true;
    for (j = 0; j < lx; j++) {
      if ((newRow && newColumn) || pixOut->getTone() > pixIn->getTone())
        *pixOut = *pixIn;

      pixIn++;
      countX += dim.lx;
      if (countX >= lx) {
        countX -= lx, pixOut++;
        newColumn = true;
      } else
        newColumn = false;
    }

    countY += dim.ly;
    if (countY >= ly) {
      countY -= ly;
      pixOut0 += rout->getWrap();
      newRow = true;
    } else
      newRow = false;
  }
  rout->unlock();
  rin->unlock();
}

/*-----------------------------------------------------------------------------*/
