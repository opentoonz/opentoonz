

// TnzCore includes
#include "tsystem.h"
#include "tstream.h"
#include "tthreadmessage.h"
#include "tconvert.h"
#include "tstopwatch.h"
#include "tlevel_io.h"
#include "tflash.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "timagecache.h"
#include "timageinfo.h"
#include "tropcm.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"

// TnzBase includes
#include "ttzpimagefx.h"
#include "trasterfx.h"
#include "tzeraryfx.h"
#include "trenderer.h"
#include "tfxcachemanager.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/txshchildlevel.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/stage.h"
#include "toonz/fill.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "toonz/levelproperties.h"
#include "toonz/imagemanager.h"
#include "toonz/toonzimageutils.h"
#include "toonz/tvectorimageutils.h"
#include "toonz/preferences.h"
#include "toonz/dpiscale.h"
#include "imagebuilders.h"

// 4.6 compatibility - sandor fxs
#include "toonz4.6/raster.h"
#include "sandor_fxs/blend.h"
extern "C" {
#include "sandor_fxs/calligraph.h"
#include "sandor_fxs/patternmap.h"
}

#include "toonz/tcolumnfx.h"

//****************************************************************************************
//    Preliminaries
//****************************************************************************************

#ifdef _WIN32
template class DV_EXPORT_API TFxDeclarationT<TLevelColumnFx>;
template class DV_EXPORT_API TFxDeclarationT<TZeraryColumnFx>;
template class DV_EXPORT_API TFxDeclarationT<TXsheetFx>;
template class DV_EXPORT_API TFxDeclarationT<TOutputFx>;
#endif

TFxDeclarationT<TLevelColumnFx> columnFxInfo(TFxInfo("Toonz_columnFx", true));
TFxDeclarationT<TPaletteColumnFx> paletteColumnFxInfo(
    TFxInfo("Toonz_paletteColumnFx", true));
TFxDeclarationT<TZeraryColumnFx> zeraryColumnFxInfo(
    TFxInfo("Toonz_zeraryColumnFx", true));
TFxDeclarationT<TXsheetFx> infoTXsheetFx(TFxInfo("Toonz_xsheetFx", true));
TFxDeclarationT<TOutputFx> infoTOutputFx(TFxInfo("Toonz_outputFx", true));

//****************************************************************************************
//    Local namespace  -  misc functions
//****************************************************************************************

namespace {

void setMaxMatte(TRasterP r) {
  TRaster32P r32 = (TRaster32P)r;

  TRaster64P r64 = (TRaster64P)r;

  if (r32)
    for (int i = 0; i < r32->getLy(); i++) {
      TPixel *pix = r32->pixels(i);
      for (int j = 0; j < r32->getLx(); j++, pix++) pix->m = 255;
    }
  else if (r64)
    for (int i = 0; i < r64->getLy(); i++) {
      TPixel64 *pix = r64->pixels(i);
      for (int j = 0; j < r64->getLx(); j++, pix++) pix->m = 65535;
    }
}

//---------------------------------------------------------------------------------------------------------

char *strsave(const char *t) {
  char *s;
  s = (char *)malloc(
      strlen(t) +
      1);  // I'm almost sure that this malloc is LEAKED! Please, check that !
  strcpy(s, t);
  return s;
}

//-------------------------------------------------------------------

inline TRect myConvert(const TRectD &r) {
  return TRect(tfloor(r.x0), tfloor(r.y0), tceil(r.x1), tceil(r.y1));
}

//-------------------------------------------------------------------

inline TRect myConvert(const TRectD &r, TPointD &dp) {
  TRect ri(tfloor(r.x0), tfloor(r.y0), tceil(r.x1), tceil(r.y1));
  dp.x = r.x0 - ri.x0;
  dp.y = r.y0 - ri.y0;
  assert(dp.x >= 0 && dp.y >= 0);
  return ri;
}

//--------------------------------------------------

// Currently used on debug only
inline QString traduce(const TRectD &rect) {
  return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " " +
         QString::number(rect.x1) + " " + QString::number(rect.y1) + "]";
}

//--------------------------------------------------

// Currently used on debug only
inline QString traduce(const TRectD &rect, const TRenderSettings &info) {
  return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " " +
         QString::number(rect.x1) + " " + QString::number(rect.y1) +
         "]; aff = (" + QString::number(info.m_affine.a11, 'f') + " " +
         QString::number(info.m_affine.a12, 'f') + " " +
         QString::number(info.m_affine.a13, 'f') + " " +
         QString::number(info.m_affine.a21, 'f') + " " +
         QString::number(info.m_affine.a22, 'f') + " " +
         QString::number(info.m_affine.a23, 'f') + ")";
}

//-------------------------------------------------------------------

inline int colorDistance(const TPixel32 &c0, const TPixel32 &c1) {
  return (c0.r - c1.r) * (c0.r - c1.r) + (c0.g - c1.g) * (c0.g - c1.g) +
         (c0.b - c1.b) * (c0.b - c1.b);
}

//-------------------------------------------------------------------

std::string getAlias(TXsheet *xsh, double frame, const TRenderSettings &info) {
  TFxSet *fxs = xsh->getFxDag()->getTerminalFxs();
  std::string alias;

  // Add the alias for each
  for (int i = 0; i < fxs->getFxCount(); ++i) {
    TRasterFx *fx = dynamic_cast<TRasterFx *>(fxs->getFx(i));
    assert(fx);
    if (!fx) continue;

    alias += fx->getAlias(frame, info) + ";";
  }

  return alias;
}

}  // namespace

//****************************************************************************************
//    Local namespace  -  Colormap (Sandor) Fxs stuff
//****************************************************************************************

static bool vectorMustApplyCmappedFx(
    const std::vector<TRasterFxRenderDataP> &fxs) {
  std::vector<TRasterFxRenderDataP>::const_iterator ft, fEnd(fxs.end());
  for (ft = fxs.begin(); ft != fEnd; ++ft) {
    PaletteFilterFxRenderData *paletteFilterData =
        dynamic_cast<PaletteFilterFxRenderData *>(ft->getPointer());
    SandorFxRenderData *sandorData =
        dynamic_cast<SandorFxRenderData *>(ft->getPointer());

    // (Daniele) Sandor fxs perform on raster colormaps *only* - while texture
    // fxs use palette filters to work.
    // In the latter case, vector-to-colormap conversion makes sure that regions
    // are not rendered under full ink
    // pixels (which causes problems**).
    if (sandorData || (paletteFilterData &&
                       paletteFilterData->m_type != ::eApplyToInksAndPaints))
      return true;

    /*
(Daniele) Disregarding the above reasons - palette filter fxs do not forcibly
return true.

Err... ok, my fault - when I wrote that, I forgot to specify WHICH problems**
occurred. Whops!

Now, it happens that if palette filters DO NOT convert to colormapped forcedly,
special styles can be
retained... so, let's see what happens.

Will have to inquire further, though...
*/
  }

  return false;
}

//-------------------------------------------------------------------

static bool mustApplySandorFx(const std::vector<TRasterFxRenderDataP> &fxs) {
  std::vector<TRasterFxRenderDataP>::const_iterator ft, fEnd(fxs.end());
  for (ft = fxs.begin(); ft != fEnd; ++ft) {
    SandorFxRenderData *sandorData =
        dynamic_cast<SandorFxRenderData *>(ft->getPointer());

    if (sandorData) return true;
  }
  return false;
}

//-------------------------------------------------------------------

static int getEnlargement(const std::vector<TRasterFxRenderDataP> &fxs,
                          double scale) {
  int enlargement = 1;

  std::vector<TRasterFxRenderDataP>::const_iterator ft, fEnd(fxs.end());
  for (ft = fxs.begin(); ft != fEnd; ++ft) {
    SandorFxRenderData *sandorData =
        dynamic_cast<SandorFxRenderData *>(ft->getPointer());

    if (sandorData) {
      switch (sandorData->m_type) {
      case BlendTz: {
        // Nothing happen, unless we have color 0 among the blended ones. In
        // such case,
        // we have to enlarge the bbox proportionally to the amount param.
        std::vector<std::string> items;
        std::string indexes = std::string(sandorData->m_argv[0]);
        parseIndexes(indexes, items);
        PaletteFilterFxRenderData paletteFilterData;
        insertIndexes(items, &paletteFilterData);

        if (!paletteFilterData.m_colors.empty() &&
            *paletteFilterData.m_colors.begin() == 0) {
          BlendTzParams &params = sandorData->m_blendParams;
          enlargement           = params.m_amount * scale;
        }

        break;
      }

      case Calligraphic:
      case OutBorder: {
        CalligraphicParams &params = sandorData->m_callParams;
        enlargement                = params.m_thickness * scale;

        break;
      }

      case ArtAtContour: {
        ArtAtContourParams &params = sandorData->m_contourParams;
        enlargement = std::max(tceil(sandorData->m_controllerBBox.getLx()),
                               tceil(sandorData->m_controllerBBox.getLy())) *
                      params.m_maxSize;

        break;
      }
      }
    }
  }

  return enlargement;
}

//-------------------------------------------------------------------

static void applyPaletteFilter(TPalette *&plt, bool keep,
                               const set<int> &colors, const TPalette *srcPlt) {
  if (colors.empty()) return;

  if (!plt) plt = srcPlt->clone();

  if (keep) {
    for (int i = 0; i < plt->getStyleCount(); ++i) {
      if (colors.find(i) == colors.end())
        plt->setStyle(i, TPixel32::Transparent);
    }
  } else {
    std::set<int>::const_iterator ct, cEnd(colors.end());
    for (ct = colors.begin(); ct != cEnd; ++ct) {
      TColorStyle *style = plt->getStyle(*ct);
      if (style) plt->setStyle(*ct, TPixel32::Transparent);
    }
  }
}

//-------------------------------------------------------------------

static TPalette *getPliPalette(const TFilePath &path) {
  TLevelReaderP levelReader = TLevelReaderP(path);
  if (!levelReader.getPointer()) return 0;

  TLevelP level = levelReader->loadInfo();
  TPalette *plt = level->getPalette();

  return plt ? plt->clone() : (TPalette *)0;
}

//-------------------------------------------------------------------

inline bool fxLess(TRasterFxRenderDataP a, TRasterFxRenderDataP b) {
  SandorFxRenderData *sandorDataA =
      dynamic_cast<SandorFxRenderData *>(a.getPointer());
  if (!sandorDataA) return false;

  SandorFxRenderData *sandorDataB =
      dynamic_cast<SandorFxRenderData *>(b.getPointer());
  if (!sandorDataB) return true;

  int aIndex = sandorDataA->m_type == OutBorder
                   ? 2
                   : sandorDataA->m_type == BlendTz ? 1 : 0;
  int bIndex = sandorDataB->m_type == OutBorder
                   ? 2
                   : sandorDataB->m_type == BlendTz ? 1 : 0;

  return aIndex < bIndex;
}

//-------------------------------------------------------------------

inline void sortCmappedFxs(std::vector<TRasterFxRenderDataP> &fxs) {
  std::stable_sort(fxs.begin(), fxs.end(), fxLess);
}

//-------------------------------------------------------------------

static std::vector<int> getAllBut(std::vector<int> &colorIds) {
  assert(TPixelCM32::getMaxInk() == TPixelCM32::getMaxPaint());

  std::vector<int> curColorIds;
  std::sort(colorIds.begin(), colorIds.end());

  // Taking all colors EXCEPT those in colorIds
  unsigned int count1 = 0, count2 = 0;
  int size = TPixelCM32::getMaxInk();

  curColorIds.resize(size + 1 - colorIds.size());
  for (int i = 0; i < size; i++)
    if (count1 < colorIds.size() && colorIds[count1] == i)
      count1++;
    else
      curColorIds[count2++] = i;

  return curColorIds;
}

//-------------------------------------------------------------------

//! \b IMPORTANT \b NOTE: This function is now written so that the passed Toonz
//! Image
//! will be destroyed at the most appropriate time. You should definitely *COPY*
//! all
//! necessary informations before calling it - however, since the intent was
//! that of
//! optimizing memory usage, please avoid copying the entire image buffer...

static TImageP applyCmappedFx(TToonzImageP &ti,
                              const std::vector<TRasterFxRenderDataP> &fxs,
                              int frame, double scale) {
  TImageP result = ti;
  TTile resultTile;  // Just a quick wrapper to the ImageCache
  TPalette *inPalette, *tempPlt;
  TPaletteP filteredPalette;
  TRasterCM32P copyRas;
  std::string cmRasCacheId;

  // Retrieve the image dpi
  double dpiX, dpiY;
  ti->getDpi(dpiX, dpiY);
  double dpi = (dpiX > 0) ? dpiX / Stage::inch : 1.0;

  // First, sort the fxs.
  std::vector<TRasterFxRenderDataP> fxsCopy = fxs;
  sortCmappedFxs(fxsCopy);

  // First, let's deal with all fxs working on palettes
  inPalette = ti->getPalette();

  std::vector<TRasterFxRenderDataP>::reverse_iterator it;
  for (it = fxsCopy.rbegin(); it != fxsCopy.rend(); ++it) {
    ExternalPaletteFxRenderData *extpltData =
        dynamic_cast<ExternalPaletteFxRenderData *>((*it).getPointer());
    PaletteFilterFxRenderData *PaletteFilterData =
        dynamic_cast<PaletteFilterFxRenderData *>((*it).getPointer());
    if (extpltData && extpltData->m_palette) {
      filteredPalette = extpltData->m_palette->clone();
      filteredPalette->setFrame(frame);
    } else if (PaletteFilterData &&
               PaletteFilterData->m_type == eApplyToInksAndPaints) {
      bool keep = PaletteFilterData->m_keep;

      set<int> colors;
      colors.insert(PaletteFilterData->m_colors.begin(),
                    PaletteFilterData->m_colors.end());

      // Apply the palette filters
      tempPlt = filteredPalette.getPointer();
      applyPaletteFilter(tempPlt, keep, colors, inPalette);
      filteredPalette = tempPlt;

      inPalette = filteredPalette.getPointer();
    }
  }

  if (filteredPalette) {
    // result= ti = ti->clone();   //Is copying truly necessary??
    result = ti = TToonzImageP(ti->getRaster(), ti->getSavebox());
    filteredPalette->setFrame(frame);
    ti->setPalette(filteredPalette.getPointer());
  }

  // Next, deal with fxs working on colormaps themselves
  bool firstSandorFx = true;
  TRasterCM32P cmRas = ti->getRaster();
  TRect tiSaveBox(ti->getSavebox());
  TPaletteP tiPalette(ti->getPalette());
  ti = 0;  // Release the reference to colormap

  // Now, apply cmapped->cmapped fxs
  for (it = fxsCopy.rbegin(); it != fxsCopy.rend(); ++it) {
    PaletteFilterFxRenderData *PaletteFilterData =
        dynamic_cast<PaletteFilterFxRenderData *>(it->getPointer());
    if (PaletteFilterData &&
        PaletteFilterData->m_type != eApplyToInksAndPaints) {
      std::vector<int> indexes;
      indexes.resize(PaletteFilterData->m_colors.size());

      set<int>::const_iterator jt = PaletteFilterData->m_colors.begin();
      for (int j = 0; j < (int)indexes.size(); ++j, ++jt) indexes[j] = *jt;

      if (copyRas == TRasterCM32P())
        copyRas =
            cmRas->clone();  // Pixels are literally cleared on a copy buffer

      /*-- 処理するIndexを反転 --*/
      if (PaletteFilterData->m_keep) indexes = getAllBut(indexes);

      /*-- Paintの消去（"Lines Including All
       * Areas"のみ、Areaに何も操作をしない） --*/
      if (PaletteFilterData->m_type !=
          eApplyToInksKeepingAllPaints)  // se non e'
                                         // eApplyToInksKeepingAllPaints,
                                         // sicuramente devo cancellare dei
                                         // paint
        TRop::eraseColors(
            copyRas, PaletteFilterData->m_type == eApplyToInksDeletingAllPaints
                         ? 0
                         : &indexes,
            false);

      /*-- Inkの消去 --*/
      if (PaletteFilterData->m_type !=
          eApplyToPaintsKeepingAllInks)  // se non e'
                                         // eApplyToPaintsKeepingAllInks,
                                         // sicuramente devo cancellare degli
                                         // ink
        TRop::eraseColors(
            copyRas, PaletteFilterData->m_type == eApplyToPaintsDeletingAllInks
                         ? 0
                         : &indexes,
            true);
    }
  }

  if (copyRas) {
    cmRas  = copyRas;
    result = TToonzImageP(cmRas, tiSaveBox);
    result->setPalette(tiPalette.getPointer());
  }

  // Finally, apply cmapped->fullcolor fxs

  // Prefetch all Blend fxs
  std::vector<BlendParam> blendParams;
  for (it = fxsCopy.rbegin(); it != fxsCopy.rend(); ++it) {
    SandorFxRenderData *sandorData =
        dynamic_cast<SandorFxRenderData *>(it->getPointer());
    if (sandorData && sandorData->m_type == BlendTz) {
      BlendParam param;

      param.intensity =
          std::stod(std::string(sandorData->m_argv[3])) * scale * dpi;
      param.smoothness     = sandorData->m_blendParams.m_smoothness;
      param.stopAtCountour = sandorData->m_blendParams.m_noBlending;

      param.superSampling = sandorData->m_blendParams.m_superSampling;

      // Build the color indexes
      std::vector<std::string> items;
      std::string indexes = std::string(sandorData->m_argv[0]);
      parseIndexes(indexes, items);
      PaletteFilterFxRenderData paletteFilterData;
      insertIndexes(items, &paletteFilterData);

      param.colorsIndexes.reserve(paletteFilterData.m_colors.size());

      std::set<int>::iterator it;
      for (it = paletteFilterData.m_colors.begin();
           it != paletteFilterData.m_colors.end(); ++it)
        param.colorsIndexes.push_back(*it);

      blendParams.push_back(param);
    }
  }

  // Apply each sandor
  for (it = fxsCopy.rbegin(); it != fxsCopy.rend(); ++it) {
    SandorFxRenderData *sandorData =
        dynamic_cast<SandorFxRenderData *>(it->getPointer());

    if (sandorData) {
      // Avoid dealing with specific cases
      if ((sandorData->m_type == BlendTz && blendParams.empty()) ||
          (sandorData->m_type == OutBorder && !firstSandorFx))
        continue;

      if (!firstSandorFx) {
        // Retrieve the colormap from cache
        cmRas = TToonzImageP(TImageCache::instance()->get(cmRasCacheId, true))
                    ->getRaster();

        // Apply a palette filter in order to keep only the colors specified in
        // the sandor argv
        std::vector<std::string> items;
        std::string indexes = std::string(sandorData->m_argv[0]);
        parseIndexes(indexes, items);
        PaletteFilterFxRenderData paletteFilterData;
        insertIndexes(items, &paletteFilterData);

        filteredPalette = tempPlt = 0;
        applyPaletteFilter(tempPlt, true, paletteFilterData.m_colors,
                           tiPalette.getPointer());
        filteredPalette = tempPlt;
      } else {
        // Pass the input colormap to the cache and release its reference as
        // final result
        cmRasCacheId = TImageCache::instance()->getUniqueId();
        TImageCache::instance()->add(cmRasCacheId,
                                     TToonzImageP(cmRas, tiSaveBox));
        result = 0;
      }

      // Convert current smart pointers to a 4.6 'fashion'. The former ones are
      // released - so they
      // do not occupy smart object references.
      RASTER *oldRasterIn, *oldRasterOut;
      oldRasterIn = TRop::convertRaster50to46(
          cmRas, filteredPalette ? filteredPalette : tiPalette.getPointer());
      cmRas = TRasterCM32P(0);
      {
        TRaster32P rasterOut(TDimension(oldRasterIn->lx, oldRasterIn->ly));
        oldRasterOut = TRop::convertRaster50to46(rasterOut, 0);
      }

      switch (sandorData->m_type) {
      case BlendTz: {
        if (blendParams.empty()) continue;

        // Retrieve the colormap from cache
        cmRas = TToonzImageP(TImageCache::instance()->get(cmRasCacheId, true))
                    ->getRaster();

        TToonzImageP ti(cmRas, tiSaveBox);
        ti->setPalette(filteredPalette ? filteredPalette.getPointer()
                                       : tiPalette.getPointer());

        TRasterImageP riOut(TImageCache::instance()->get(
            std::string(oldRasterOut->cacheId, oldRasterOut->cacheIdLength),
            true));
        TRaster32P rasterOut = riOut->getRaster();

        blend(ti, rasterOut, blendParams);

        blendParams.clear();
        break;
      }

      case Calligraphic:
      case OutBorder: {
        if (sandorData->m_type == OutBorder && !firstSandorFx) continue;

        const char *argv[12];
        memcpy(argv, sandorData->m_argv, 12 * sizeof(const char *));

        double thickness =
            std::stod(std::string(sandorData->m_argv[7])) * scale * dpi;
        argv[7] = strsave(std::to_string(thickness).c_str());

        calligraph(oldRasterIn, oldRasterOut, sandorData->m_border,
                   sandorData->m_argc, argv, sandorData->m_shrink,
                   sandorData->m_type == OutBorder);

        break;
      }
      case ArtAtContour: {
        const char *argv[12];
        memcpy(argv, sandorData->m_argv, 12 * sizeof(const char *));

        double distance =
            std::stod(std::string(sandorData->m_argv[6])) * scale * dpi;
        argv[6]  = strsave(std::to_string(distance).c_str());
        distance = std::stod(std::string(sandorData->m_argv[7])) * scale * dpi;
        argv[7]  = strsave(std::to_string(distance).c_str());
        double density =
            std::stod(std::string(sandorData->m_argv[8])) / sq(scale * dpi);
        argv[8] = strsave(std::to_string(density).c_str());
        double size =
            std::stod(std::string(sandorData->m_argv[1])) * scale * dpi;
        argv[1] = strsave(std::to_string(size).c_str());
        size    = std::stod(std::string(sandorData->m_argv[2])) * scale * dpi;
        argv[2] = strsave(std::to_string(size).c_str());
        RASTER *imgContour =
            TRop::convertRaster50to46(sandorData->m_controller, 0);
        patternmap(oldRasterIn, oldRasterOut, sandorData->m_border,
                   sandorData->m_argc, argv, sandorData->m_shrink, imgContour);
        TRop::releaseRaster46(imgContour);

        break;
      }
      default:
        assert(false);
      }

      TRasterImageP riOut(TImageCache::instance()->get(
          std::string(oldRasterOut->cacheId, oldRasterOut->cacheIdLength),
          true));
      TRaster32P rasterOut = riOut->getRaster();

      TRop::releaseRaster46(oldRasterIn);
      TRop::releaseRaster46(oldRasterOut);

      if (firstSandorFx) {
        resultTile.setRaster(rasterOut);
        firstSandorFx = false;
      } else
        TRop::over(resultTile.getRaster(), rasterOut);
    }
  }

  // Release cmRas cache identifier if any
  TImageCache::instance()->remove(cmRasCacheId);

  if (!result) result = TRasterImageP(resultTile.getRaster());

  return result;
}

//-------------------------------------------------------------------

static void applyCmappedFx(TVectorImageP &vi,
                           const std::vector<TRasterFxRenderDataP> &fxs,
                           int frame) {
  TRasterP ras;
  bool keep = false;
  TPaletteP modPalette;
  set<int> colors;
  std::vector<TRasterFxRenderDataP>::const_iterator it = fxs.begin();

  // prima tutti gli effetti che agiscono sulla paletta....
  for (; it != fxs.end(); ++it) {
    ExternalPaletteFxRenderData *extpltData =
        dynamic_cast<ExternalPaletteFxRenderData *>((*it).getPointer());
    PaletteFilterFxRenderData *pltFilterData =
        dynamic_cast<PaletteFilterFxRenderData *>((*it).getPointer());

    if (extpltData && extpltData->m_palette)
      modPalette = extpltData->m_palette->clone();
    else if (pltFilterData) {
      assert(
          pltFilterData->m_type ==
          eApplyToInksAndPaints);  // Must have been converted to CM32 otherwise

      keep = pltFilterData->m_keep;
      colors.insert(pltFilterData->m_colors.begin(),
                    pltFilterData->m_colors.end());
    }
  }

  TPalette *tempPlt = modPalette.getPointer();
  applyPaletteFilter(tempPlt, keep, colors, vi->getPalette());
  modPalette = tempPlt;

  if (modPalette) {
    vi = vi->clone();
    vi->setPalette(modPalette.getPointer());
  }
}

//****************************************************************************************
//    LevelFxResourceBuilder  definition
//****************************************************************************************

class LevelFxBuilder final : public ResourceBuilder {
  TRasterP m_loadedRas;
  TPaletteP m_palette;

  TXshSimpleLevel *m_sl;
  TFrameId m_fid;
  TRectD m_tileGeom;
  bool m_64bit;

  TRect m_rasBounds;

public:
  LevelFxBuilder(const std::string &resourceName, double frame,
                 const TRenderSettings &rs, TXshSimpleLevel *sl, TFrameId fid)
      : ResourceBuilder(resourceName, 0, frame, rs)
      , m_loadedRas()
      , m_palette()
      , m_sl(sl)
      , m_fid(fid)
      , m_64bit(rs.m_bpp == 64) {}

  void setRasBounds(const TRect &rasBounds) { m_rasBounds = rasBounds; }

  void compute(const TRectD &tileRect) override {
    // Load the image
    TImageP img(m_sl->getFullsampledFrame(
        m_fid, (m_64bit ? ImageManager::is64bitEnabled : 0) |
                   ImageManager::dontPutInCache));

    if (!img) return;

    TRasterImageP rimg(img);
    TToonzImageP timg(img);

    m_loadedRas = rimg ? (TRasterP)rimg->getRaster()
                       : timg ? (TRasterP)timg->getRaster() : TRasterP();
    assert(m_loadedRas);

    if (timg) m_palette = timg->getPalette();

    assert(tileRect ==
           TRectD(0, 0, m_loadedRas->getLx(), m_loadedRas->getLy()));
  }

  void simCompute(const TRectD &rect) override {}

  void upload(TCacheResourceP &resource) override {
    assert(m_loadedRas);
    resource->upload(TPoint(), m_loadedRas);
    if (m_palette) resource->uploadPalette(m_palette);
  }

  bool download(TCacheResourceP &resource) override {
    // If the image has been loaded in this builder, just use it
    if (m_loadedRas) return true;

    // If the image has yet to be loaded by this builder, skip without
    // allocating anything
    if (resource->canDownloadAll(m_rasBounds)) {
      m_loadedRas = resource->buildCompatibleRaster(m_rasBounds.getSize());
      resource->downloadPalette(m_palette);
      return resource->downloadAll(TPoint(), m_loadedRas);
    } else
      return false;
  }

  TImageP getImage() const {
    if (!m_loadedRas) return TImageP();

    TRasterCM32P cm(m_loadedRas);

    TImageP result(cm ? TImageP(TToonzImageP(cm, cm->getBounds()))
                      : TImageP(TRasterImageP(m_loadedRas)));
    if (m_palette) result->setPalette(m_palette.getPointer());

    return result;
  }
};

//****************************************************************************************
//    TLevelColumnFx  implementation
//****************************************************************************************

TLevelColumnFx::TLevelColumnFx()
    : m_levelColumn(0), m_isCachable(true), m_mutex(), m_offlineContext(0) {
  setName(L"LevelColumn");
}

//--------------------------------------------------

TLevelColumnFx::~TLevelColumnFx() {
  if (m_offlineContext) delete m_offlineContext;
}

//-------------------------------------------------------------------

bool TLevelColumnFx::canHandle(const TRenderSettings &info, double frame) {
  // NOTE 1: Currently, it is necessary that level columns return FALSE for
  // raster levels - just a quick way to tell the cache functions that they
  // have to be cached.

  if (!m_levelColumn) return true;

  TXshCell cell = m_levelColumn->getCell(m_levelColumn->getFirstRow());
  if (cell.isEmpty()) return true;

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (!sl) return true;

  return (sl->getType() == PLI_XSHLEVEL &&
          !vectorMustApplyCmappedFx(info.m_data));
}

//-------------------------------------------------------------------

TAffine TLevelColumnFx::handledAffine(const TRenderSettings &info,
                                      double frame) {
  if (!m_levelColumn) return TAffine();

  TXshCell cell = m_levelColumn->getCell(m_levelColumn->getFirstRow());
  if (cell.isEmpty()) return TAffine();

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (!sl) return TAffine();

  if (sl->getType() == PLI_XSHLEVEL)
    return vectorMustApplyCmappedFx(info.m_data)
               ? TRasterFx::handledAffine(info, frame)
               : info.m_affine;

  // Accept any translation consistent with the image's pixels geometry
  TImageInfo imageInfo;
  getImageInfo(imageInfo, sl, cell.m_frameId);

  TPointD pixelsOrigin(-0.5 * imageInfo.m_lx, -0.5 * imageInfo.m_ly);

  const TAffine &aff = info.m_affine;
  if (aff.a11 != 1.0 || aff.a22 != 1.0 || aff.a12 != 0.0 || aff.a21 != 0.0)
    return TTranslation(-pixelsOrigin);

  // This is a translation, ok. Just ensure it is consistent.
  TAffine consistentAff(aff);

  consistentAff.a13 -= pixelsOrigin.x, consistentAff.a23 -= pixelsOrigin.y;
  consistentAff.a13 = tfloor(consistentAff.a13),
  consistentAff.a23 = tfloor(consistentAff.a23);
  consistentAff.a13 += pixelsOrigin.x, consistentAff.a23 += pixelsOrigin.y;

  return consistentAff;
}

//-------------------------------------------------------------------

TFilePath TLevelColumnFx::getPalettePath(int frame) const {
  if (!m_levelColumn) return TFilePath();

  TXshCell cell = m_levelColumn->getCell(frame);
  if (cell.isEmpty()) return TFilePath();

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (!sl) return TFilePath();

  if (sl->getType() == TZP_XSHLEVEL)
    return sl->getScene()->decodeFilePath(
        sl->getPath().withNoFrame().withType("tpl"));

  if (sl->getType() == PLI_XSHLEVEL)
    return sl->getScene()->decodeFilePath(sl->getPath());

  return TFilePath();
}

//-------------------------------------------------------------------

TPalette *TLevelColumnFx::getPalette(int frame) const {
  if (!m_levelColumn) return 0;

  TXshCell cell = m_levelColumn->getCell(frame);
  if (cell.isEmpty()) return 0;

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (!sl) return 0;

  return sl->getPalette();
}

//--------------------------------------------------

TFx *TLevelColumnFx::clone(bool recursive) const {
  TLevelColumnFx *clonedFx =
      dynamic_cast<TLevelColumnFx *>(TFx::clone(recursive));
  assert(clonedFx);
  clonedFx->m_levelColumn = m_levelColumn;
  clonedFx->m_isCachable  = m_isCachable;
  return clonedFx;
}

//--------------------------------------------------

void TLevelColumnFx::doDryCompute(TRectD &rect, double frame,
                                  const TRenderSettings &info) {
  if (!m_levelColumn) return;

  int row       = (int)frame;
  TXshCell cell = m_levelColumn->getCell(row);
  if (cell.isEmpty()) return;

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (!sl) return;

  // In case this is a vector level, the image is renderized quickly and
  // directly at the
  // correct resolution. Caching is disabled in such case, at the moment.
  if (sl->getType() == PLI_XSHLEVEL) return;

  int renderStatus =
      TRenderer::instance().getRenderStatus(TRenderer::renderId());

  std::string alias = getAlias(frame, TRenderSettings()) + "_image";

  TImageInfo imageInfo;
  getImageInfo(imageInfo, sl, cell.m_frameId);
  TRectD imgRect(0, 0, imageInfo.m_lx, imageInfo.m_ly);

  if (renderStatus == TRenderer::FIRSTRUN) {
    ResourceBuilder::declareResource(alias, 0, imgRect, frame, info, false);
  } else {
    LevelFxBuilder builder(alias, frame, info, sl, cell.m_frameId);
    builder.setRasBounds(TRect(0, 0, imageInfo.m_lx - 1, imageInfo.m_ly - 1));
    builder.simBuild(imgRect);
  }
}

//--------------------------------------------------

bool isSubsheetChainOnColumn0(TXsheet *topXsheet, TXsheet *subsheet,
                              int frame) {
  if (topXsheet == subsheet) return true;

  const TXshCell cell = topXsheet->getCell(frame, 0);
  if (!cell.m_level) return false;
  TXshChildLevel *cl = cell.m_level->getChildLevel();
  if (!cl) return false;
  return isSubsheetChainOnColumn0(cl->getXsheet(), subsheet, frame);
}

//-----------------------------------------------------

void TLevelColumnFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &info) {
  if (!m_levelColumn) return;

  // Ensure that a corresponding cell and level exists
  int row       = (int)frame;
  TXshCell cell = m_levelColumn->getCell(row);

  if (cell.isEmpty()) return;

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  if (!sl) return;

  TFrameId fid = cell.m_frameId;

  TImageP img;
  TImageInfo imageInfo;

  // Now, fetch the image
  if (sl->getType() != PLI_XSHLEVEL) {
    // Raster case
    LevelFxBuilder builder(getAlias(frame, TRenderSettings()) + "_image", frame,
                           info, sl, fid);

    getImageInfo(imageInfo, sl, fid);
    TRectD imgRect(0, 0, imageInfo.m_lx, imageInfo.m_ly);

    builder.setRasBounds(TRect(0, 0, imageInfo.m_lx - 1, imageInfo.m_ly - 1));
    builder.build(imgRect);

    img = builder.getImage();
  } else {
    // Vector case (loading is immediate)
    if (!img) {
      img = sl->getFullsampledFrame(
          fid, ((info.m_bpp == 64) ? ImageManager::is64bitEnabled : 0) |
                   ImageManager::dontPutInCache);
    }
  }

  // Extract the required geometry
  TRect tileBounds(tile.getRaster()->getBounds());
  TRectD tileRectD = TRectD(tileBounds.x0, tileBounds.y0, tileBounds.x1 + 1,
                            tileBounds.y1 + 1) +
                     tile.m_pos;

  // To be sure, if there is no image, return.
  if (!img) return;

  TRectD bBox = img->getBBox();

  // Discriminate image type
  if (TVectorImageP vectorImage = img) {
    // Vector case

    if (vectorMustApplyCmappedFx(info.m_data)) {
      // Deal separately
      applyTzpFxsOnVector(vectorImage, tile, frame, info);
    } else {
      QMutexLocker m(&m_mutex);
      bBox = info.m_affine * vectorImage->getBBox();
      TDimension size(tile.getRaster()->getSize());

      TAffine aff = TTranslation(-tile.m_pos.x, -tile.m_pos.y) *
                    TScale(1.0 / info.m_shrinkX, 1.0 / info.m_shrinkY) *
                    info.m_affine;

      applyCmappedFx(vectorImage, info.m_data, (int)frame);
      TPalette *vpalette = vectorImage->getPalette();
      assert(vpalette);
      m_isCachable = !vpalette->isAnimated();
      int oldFrame = vpalette->getFrame();

      TVectorRenderData rd(TVectorRenderData::ProductionSettings(), aff,
                           TRect(size), vpalette);

      if (!m_offlineContext || m_offlineContext->getLx() < size.lx ||
          m_offlineContext->getLy() < size.ly) {
        if (m_offlineContext) delete m_offlineContext;
        m_offlineContext = new TOfflineGL(size);
      }

      m_offlineContext->makeCurrent();
      m_offlineContext->clear(TPixel32(0, 0, 0, 0));

      // If level has animated palette, it is necessary to lock palette's color
      // against
      // concurrents TPalette::setFrame.
      if (!m_isCachable) vpalette->mutex()->lock();

      vpalette->setFrame((int)frame);
      m_offlineContext->draw(vectorImage, rd, true);
      vpalette->setFrame(oldFrame);

      if (!m_isCachable) vpalette->mutex()->unlock();

      m_offlineContext->getRaster(tile.getRaster());

      m_offlineContext->doneCurrent();
    }
  } else {
    // Raster case

    TRasterP ras;
    TAffine aff;

    TRasterImageP ri = img;
    TToonzImageP ti  = img;
    img              = 0;

    if (ri) {
      // Fullcolor case

      ras = ri->getRaster();
      ri  = 0;
      TRaster32P ras32(ras);
      TRaster64P ras64(ras);

      // Ensure that ras is either a 32 or 64 fullcolor.
      // Otherwise, we have to convert it.
      if (!ras32 && !ras64) {
        TRasterP tileRas(tile.getRaster());
        TRaster32P tileRas32(tileRas);
        TRaster64P tileRas64(tileRas);

        if (tileRas32) {
          ras32 = TRaster32P(ras->getLx(), ras->getLy());
          TRop::convert(ras32, ras);
          ras = ras32;
        } else if (tileRas64) {
          ras64 = TRaster64P(ras->getLx(), ras->getLy());
          TRop::convert(ras64, ras);
          ras = ras64;
        } else
          assert(0);
      }
      TXsheet *xsh  = m_levelColumn->getLevelColumn()->getXsheet();
      TXsheet *txsh = sl->getScene()->getTopXsheet();

      LevelProperties *levelProp = sl->getProperties();
      if (Preferences::instance()->isIgnoreAlphaonColumn1Enabled() &&
          m_levelColumn->getIndex() == 0 &&
          isSubsheetChainOnColumn0(txsh, xsh, frame)) {
        TRasterP appRas(ras->clone());
        setMaxMatte(appRas);
        ras = appRas;
      }
      if (levelProp->doPremultiply()) {
        TRasterP appRas = ras->clone();
        TRop::premultiply(appRas);
        ras = appRas;
      }
      if (levelProp->whiteTransp()) {
        TRasterP appRas(ras->clone());
        TRop::whiteTransp(appRas);
        ras = appRas;
      }
      if (levelProp->antialiasSoftness() > 0) {
        TRasterP appRas = ras->create(ras->getLx(), ras->getLy());
        TRop::antialias(ras, appRas, 10, levelProp->antialiasSoftness());
        ras = appRas;
      }

      if (TXshSimpleLevel::m_fillFullColorRaster) {
        TRasterP appRas(ras->clone());
        FullColorAreaFiller filler(appRas);
        TPaletteP palette = new TPalette();
        int styleId       = palette->getPage(0)->addStyle(TPixel32::White);
        FillParameters params;
        params.m_palette      = palette.getPointer();
        params.m_styleId      = styleId;
        params.m_minFillDepth = 0;
        params.m_maxFillDepth = 15;
        filler.rectFill(appRas->getBounds(), params, false);
        ras = appRas;
      }

    } else if (ti) {
      // Colormap case

      // Use the imageInfo's dpi
      ti->setDpi(imageInfo.m_dpix, imageInfo.m_dpiy);

      TImageP newImg = applyTzpFxs(ti, frame, info);

      ri = newImg;
      ti = newImg;

      if (ri)
        ras = ri->getRaster();
      else
        ras = ti->getRaster();

      if (sl->getProperties()->antialiasSoftness() > 0) {
        TRasterP appRas = ras->create(ras->getLx(), ras->getLy());
        TRop::antialias(ras, appRas, 10,
                        sl->getProperties()->antialiasSoftness());
        ras = appRas;
      }

      ri = 0;
    }

    if (ras) {
      double lx_2 = ras->getLx() / 2.0;
      double ly_2 = ras->getLy() / 2.0;

      TRenderSettings infoAux(info);
      assert(info.m_affine.isTranslation());
      infoAux.m_data.clear();

      // Place the output rect in the image's reference
      tileRectD += TPointD(lx_2 - info.m_affine.a13, ly_2 - info.m_affine.a23);

      // Then, retrieve loaded image's interesting region
      TRectD inTileRectD;
      if (ti) {
        TRect saveBox(ti->getSavebox());
        inTileRectD =
            TRectD(saveBox.x0, saveBox.y0, saveBox.x1 + 1, saveBox.y1 + 1);
      } else {
        TRect rasBounds(ras->getBounds());
        inTileRectD = TRectD(rasBounds.x0, rasBounds.y0, rasBounds.x1 + 1,
                             rasBounds.y1 + 1);
      }

      // And intersect the two
      inTileRectD *= tileRectD;

      // Should the intersection be void, return
      if (inTileRectD.x0 >= inTileRectD.x1 || inTileRectD.y0 >= inTileRectD.y1)
        return;

      // Output that intersection in the requested tile
      TRect inTileRect(tround(inTileRectD.x0), tround(inTileRectD.y0),
                       tround(inTileRectD.x1) - 1, tround(inTileRectD.y1) - 1);
      TTile inTile(ras->extract(inTileRect),
                   inTileRectD.getP00() + TPointD(-lx_2, -ly_2));

      // Observe that inTile is in the standard reference, ie image's minus the
      // center coordinates

      if (ti) {
        // In the colormapped case, we have to convert the cmap to fullcolor
        TPalette *palette = ti->getPalette();

        if (!m_isCachable) palette->mutex()->lock();

        int prevFrame = palette->getFrame();

        palette->setFrame((int)frame);

        TTile auxtile(TRaster32P(inTile.getRaster()->getSize()), inTile.m_pos);
        TRop::convert(auxtile, inTile, palette, false, true);
        palette->setFrame(prevFrame);

        if (!m_isCachable) palette->mutex()->unlock();

        inTile.setRaster(auxtile.getRaster());
      }

      TRasterFx::applyAffine(tile, inTile, infoAux);
    }
  }
}

//-------------------------------------------------------------------

TImageP TLevelColumnFx::applyTzpFxs(TToonzImageP &ti, double frame,
                                    const TRenderSettings &info) {
  double scale = sqrt(fabs(info.m_affine.det()));

  int prevFrame = ti->getPalette()->getFrame();
  m_isCachable  = !ti->getPalette()->isAnimated();

  if (!m_isCachable) ti->getPalette()->mutex()->lock();

  TPaletteP palette(ti->getPalette());
  palette->setFrame((int)frame);
  TImageP newImg = applyCmappedFx(ti, info.m_data, (int)frame, scale);
  palette->setFrame(prevFrame);

  if (!m_isCachable) palette->mutex()->unlock();

  return newImg;
}

//-------------------------------------------------------------------

void TLevelColumnFx::applyTzpFxsOnVector(const TVectorImageP &vi, TTile &tile,
                                         double frame,
                                         const TRenderSettings &info) {
  TRect tileBounds(tile.getRaster()->getBounds());
  TRectD tileRectD = TRectD(tileBounds.x0, tileBounds.y0, tileBounds.x1 + 1,
                            tileBounds.y1 + 1) +
                     tile.m_pos;

  // Info's affine is not the identity only in case the loaded image is a vector
  // one.
  double scale    = sqrt(fabs(info.m_affine.det()));
  int enlargement = getEnlargement(info.m_data, scale);

  // Extract the vector image's groups
  std::vector<TVectorImageP> groupsList;
  getGroupsList(vi, groupsList);

  // For each group, apply the tzp fxs stored in info. The result is immediately
  // converted
  // to a raster image if necessary.
  unsigned int i;
  for (i = 0; i < groupsList.size(); ++i) {
    TVectorImageP &groupVi = groupsList[i];

    // Extract the group's bbox.
    TRectD groupBBox(info.m_affine * groupVi->getBBox());
    if (!mustApplySandorFx(info.m_data)) groupBBox *= tileRectD;

    groupBBox = groupBBox.enlarge(enlargement);

    if (groupBBox.x0 >= groupBBox.x1 || groupBBox.y0 >= groupBBox.y1) continue;

    // Ensure that groupBBox and tile have the same integer geometry
    groupBBox -= tile.m_pos;
    groupBBox.x0 = tfloor(groupBBox.x0);
    groupBBox.y0 = tfloor(groupBBox.y0);
    groupBBox.x1 = tceil(groupBBox.x1);
    groupBBox.y1 = tceil(groupBBox.y1);
    groupBBox += tile.m_pos;

    // Build groupBBox's relative position to the tile
    TPoint groupRelativePosToTile(groupBBox.x0 - tile.m_pos.x,
                                  groupBBox.y0 - tile.m_pos.y);

    // Convert the group to a strictly sufficient Toonz image
    TToonzImageP groupTi = ToonzImageUtils::vectorToToonzImage(
        groupVi, info.m_affine, groupVi->getPalette(), groupBBox.getP00(),
        TDimension(groupBBox.getLx(), groupBBox.getLy()), &info.m_data, true);

    // Apply the tzp fxs to the converted Toonz image
    TImageP groupResult = applyTzpFxs(groupTi, frame, info);

    // If necessary, convert the result to fullcolor
    TRasterImageP groupRi = groupResult;
    if (!groupRi) {
      groupTi = groupResult;
      assert(groupTi);

      TRasterP groupTiRas(groupTi->getRaster());
      TRaster32P tempRas(groupTiRas->getSize());
      groupRi = TRasterImageP(tempRas);

      TRop::convert(tempRas, groupTiRas, groupTi->getPalette());
    }

    // Over the group image on the output
    TRasterP tileRas(tile.getRaster());
    TRop::over(tileRas, groupRi->getRaster(), groupRelativePosToTile);
  }
}

//-------------------------------------------------------------------

int TLevelColumnFx::getMemoryRequirement(const TRectD &rect, double frame,
                                         const TRenderSettings &info) {
  // Sandor fxs are currently considered *VERY* inefficient upon tile
  // subdivision
  if (mustApplySandorFx(info.m_data)) {
    return -1;
  }
  return 0;
}

//-------------------------------------------------------------------

void TLevelColumnFx::getImageInfo(TImageInfo &info, TXshSimpleLevel *sl,
                                  TFrameId frameId) {
  int type = sl->getType();
  assert(type != PLI_XSHLEVEL);
  if (type == PLI_XSHLEVEL) return;

  std::string imageId = sl->getImageId(frameId);

  const TImageInfo *storedInfo =
      ImageManager::instance()->getInfo(imageId, ImageManager::none, 0);

  if (!storedInfo)  // sulle pict caricate info era nullo, ma l'immagine c'e'!
  // con la getFullSampleFrame riprendo   l'immagine e ricalcolo la savebox...
  {
    TImageP img;
    if (!(img =
              sl->getFullsampledFrame(frameId, ImageManager::dontPutInCache))) {
      assert(false);
      return;
    }
    // Raster levels from ffmpeg were not giving the right dimensions without
    // the raster cast and check
    TRasterImageP rasterImage = (TRasterImageP)img;
    if (rasterImage) {
      info.m_lx = (int)rasterImage->getRaster()->getLx();
      info.m_ly = (int)rasterImage->getRaster()->getLy();
    } else {
      info.m_lx = (int)img->getBBox().getLx();
      info.m_ly = (int)img->getBBox().getLy();
    }
    info.m_x0 = info.m_y0 = 0;
    info.m_x1             = (int)img->getBBox().getP11().x;
    info.m_y1             = (int)img->getBBox().getP11().y;
  } else
    info = *storedInfo;
}

//-------------------------------------------------------------------

bool TLevelColumnFx::doGetBBox(double frame, TRectD &bBox,
                               const TRenderSettings &info) {
  // Usual preliminaries (make sure a level/cell exists, etc...)
  if (!m_levelColumn) return false;

  int row              = (int)frame;
  const TXshCell &cell = m_levelColumn->getCell(row);
  if (cell.isEmpty()) return false;

  TXshLevelP xshl = cell.m_level;
  if (!xshl) return false;

  TXshSimpleLevel *sl = xshl->getSimpleLevel();
  if (!sl) return false;

  double dpi = 1.0;

  // Discriminate for level type
  int type = xshl->getType();
  if (type != PLI_XSHLEVEL) {
    TImageInfo imageInfo;
    getImageInfo(imageInfo, sl, cell.m_frameId);

    TRect imageSavebox(imageInfo.m_x0, imageInfo.m_y0, imageInfo.m_x1,
                       imageInfo.m_y1);
    double cx = 0.5 * imageInfo.m_lx;
    double cy = 0.5 * imageInfo.m_ly;
    double x0 = (imageSavebox.x0 - cx);
    double y0 = (imageSavebox.y0 - cy);
    double x1 = x0 + imageSavebox.getLx();
    double y1 = y0 + imageSavebox.getLy();
    bBox      = TRectD(x0, y0, x1, y1);

    dpi = imageInfo.m_dpix / Stage::inch;
  } else {
    TImageP img = m_levelColumn->getCell(row).getImage(false);
    if (!img) return false;
    bBox = img->getBBox();
  }

  // Add the enlargement of the bbox due to Tzp render datas
  if (info.m_data.size()) {
    TRectD imageBBox(bBox);
    for (unsigned int i = 0; i < info.m_data.size(); ++i) {
      TRectD enlargedImageBBox = info.m_data[i]->getBBoxEnlargement(imageBBox);
      double enlargement       = enlargedImageBBox.x1 - imageBBox.x1;
      bBox += imageBBox.enlarge(enlargement * dpi);
    }
  }

  return true;
}

//-------------------------------------------------------------------

const TPersistDeclaration *TLevelColumnFx::getDeclaration() const {
  return &columnFxInfo;
}

//-------------------------------------------------------------------

std::string TLevelColumnFx::getPluginId() const { return "Toonz_"; }

//-------------------------------------------------------------------

TFxTimeRegion TLevelColumnFx::getTimeRegion() const {
  if (!m_levelColumn) return TFxTimeRegion();

  int first = m_levelColumn->getFirstRow();
  int last  = m_levelColumn->getRowCount();

  return TFxTimeRegion(first, last);
}

//-------------------------------------------------------------------

void TLevelColumnFx::setColumn(TXshLevelColumn *column) {
  m_levelColumn = column;
}

//-------------------------------------------------------------------

std::wstring TLevelColumnFx::getColumnId() const {
  if (!m_levelColumn) return L"Col?";
  return L"Col" + std::to_wstring(m_levelColumn->getIndex() + 1);
}

//-------------------------------------------------------------------

std::wstring TLevelColumnFx::getColumnName() const {
  if (!m_levelColumn) return L"";
  int idx = getColumnIndex();
  return ::to_wstring(m_levelColumn->getXsheet()
                          ->getStageObject(TStageObjectId::ColumnId(idx))
                          ->getName());
}

//-------------------------------------------------------------------

std::string TLevelColumnFx::getAlias(double frame,
                                     const TRenderSettings &info) const {
  if (!m_levelColumn || m_levelColumn->getCell((int)frame).isEmpty())
    return std::string();

  const TXshCell &cell = m_levelColumn->getCell((int)frame);

  TFilePath fp;
  TXshSimpleLevel *sl = cell.getSimpleLevel();

  if (!sl) {
    // Try with the sub-xsheet case
    TXshChildLevel *childLevel = cell.m_level->getChildLevel();
    if (childLevel) return ::getAlias(childLevel->getXsheet(), frame, info);

    return std::string();
  }

  TFilePath path = sl->getPath();
  if (!cell.m_frameId.isNoFrame())
    fp = path.withFrame(cell.m_frameId);
  else
    fp = path;

  std::string rdata;
  std::vector<TRasterFxRenderDataP>::const_iterator it = info.m_data.begin();
  for (; it != info.m_data.end(); ++it) {
    TRasterFxRenderDataP data = *it;
    if (data) rdata += data->toString();
  }

  if (sl->getType() == PLI_XSHLEVEL || sl->getType() == TZP_XSHLEVEL) {
    TPalette *palette = cell.getPalette();
    if (palette && palette->isAnimated())
      rdata += "animatedPlt" + std::to_string(frame);
  }

  if (Preferences::instance()->isIgnoreAlphaonColumn1Enabled()) {
    TXsheet *xsh  = m_levelColumn->getLevelColumn()->getXsheet();
    TXsheet *txsh = sl->getScene()->getTopXsheet();

    if (m_levelColumn->getIndex() == 0 &&
        isSubsheetChainOnColumn0(txsh, xsh, frame))
      rdata += "column_0";
  }

  return getFxType() + "[" + ::to_string(fp.getWideString()) + "," + rdata +
         "]";
}

//-------------------------------------------------------------------

int TLevelColumnFx::getColumnIndex() const {
  return m_levelColumn ? m_levelColumn->getIndex() : -1;
}

//-------------------------------------------------------------------

TXshColumn *TLevelColumnFx::getXshColumn() const { return m_levelColumn; }

//-------------------------------------------------------------------

void TLevelColumnFx::compute(TFlash &flash, int frame) {
  if (!m_levelColumn) return;

  TImageP img = m_levelColumn->getCell(frame).getImage(false);
  if (!img) return;

  flash.draw(img, 0);
}

//-------------------------------------------------------------------

TAffine TLevelColumnFx::getDpiAff(int frame) {
  if (!m_levelColumn) return TAffine();

  TXshCell cell = m_levelColumn->getCell(frame);
  if (cell.isEmpty()) return TAffine();

  TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
  TFrameId fid        = cell.m_frameId;

  if (sl) return ::getDpiAffine(sl, cell.m_frameId, true);

  return TAffine();
}

//-------------------------------------------------------------------

void TLevelColumnFx::saveData(TOStream &os) {
  assert(m_levelColumn);
  TFx::saveData(os);
}

//-------------------------------------------------------------------

void TLevelColumnFx::loadData(TIStream &is) {
  // The internal numeric identifier is set here but not in constructors.
  // This is *intended* as fx cloning needs to retain the original id
  // through the fxs tree deployment procedure happening just before a
  // render process (see scenefx.cpp).

  TFx::loadData(is);
  setNewIdentifier();
}

//****************************************************************************************
//    TPaletteColumnFx  implementation
//****************************************************************************************

TPaletteColumnFx::TPaletteColumnFx() : m_paletteColumn(0) {}

//-------------------------------------------------------------------

TPaletteColumnFx::~TPaletteColumnFx() {}

//--------------------------------------------------

TFx *TPaletteColumnFx::clone(bool recursive) const {
  TPaletteColumnFx *clonedFx =
      dynamic_cast<TPaletteColumnFx *>(TFx::clone(recursive));
  assert(clonedFx);
  clonedFx->m_paletteColumn = m_paletteColumn;
  return clonedFx;
}

//-------------------------------------------------------------------

void TPaletteColumnFx::doCompute(TTile &tile, double frame,
                                 const TRenderSettings &) {}

//-------------------------------------------------------------------

bool TPaletteColumnFx::doGetBBox(double frame, TRectD &bBox,
                                 const TRenderSettings &info) {
  return false;
}

//-------------------------------------------------------------------

TAffine TPaletteColumnFx::getDpiAff(int frame) { return TAffine(); }

//-------------------------------------------------------------------

bool TPaletteColumnFx::canHandle(const TRenderSettings &info, double frame) {
  return false;
}

//-------------------------------------------------------------------

TFilePath TPaletteColumnFx::getPalettePath(int frame) const {
  if (!m_paletteColumn) return TFilePath();
  TXshCell cell = m_paletteColumn->getCell(frame);
  if (cell.isEmpty() || cell.m_level->getPaletteLevel() == 0)
    return TFilePath();

  TXshPaletteLevel *paletteLevel = cell.m_level->getPaletteLevel();
  TFilePath path                 = paletteLevel->getPath();
  path = paletteLevel->getScene()->decodeFilePath(path);
  return path;
}

//-------------------------------------------------------------------

TPalette *TPaletteColumnFx::getPalette(int frame) const {
  if (!m_paletteColumn) return 0;
  TXshCell cell = m_paletteColumn->getCell(frame);
  if (cell.isEmpty() || cell.m_level->getPaletteLevel() == 0) return 0;

  TXshPaletteLevel *paletteLevel = cell.m_level->getPaletteLevel();
  return paletteLevel->getPalette();
}

//-------------------------------------------------------------------

const TPersistDeclaration *TPaletteColumnFx::getDeclaration() const {
  return &paletteColumnFxInfo;
}

//-------------------------------------------------------------------

std::string TPaletteColumnFx::getPluginId() const { return "Toonz_"; }

//-------------------------------------------------------------------

TFxTimeRegion TPaletteColumnFx::getTimeRegion() const {
  int first = 0;
  int last  = 11;
  return TFxTimeRegion(first, last);
}

//-------------------------------------------------------------------

std::wstring TPaletteColumnFx::getColumnName() const {
  if (!m_paletteColumn) return L"Col?";
  return L"Col" + std::to_wstring(m_paletteColumn->getIndex() + 1);
}

//-------------------------------------------------------------------

std::wstring TPaletteColumnFx::getColumnId() const {
  if (!m_paletteColumn) return L"Col?";
  return L"Col" + std::to_wstring(m_paletteColumn->getIndex() + 1);
}

//-------------------------------------------------------------------

std::string TPaletteColumnFx::getAlias(double frame,
                                       const TRenderSettings &info) const {
  TFilePath palettePath = getPalettePath(frame);
  return "TPaletteColumnFx[" + ::to_string(palettePath.getWideString()) + "]";
}

//-------------------------------------------------------------------

void TPaletteColumnFx::compute(TFlash &flash, int frame) {}

//-------------------------------------------------------------------

int TPaletteColumnFx::getColumnIndex() const {
  return m_paletteColumn ? m_paletteColumn->getIndex() : -1;
}

//-------------------------------------------------------------------

TXshColumn *TPaletteColumnFx::getXshColumn() const { return m_paletteColumn; }

//****************************************************************************************
//    TZeraryColumnFx  implementation
//****************************************************************************************

TZeraryColumnFx::TZeraryColumnFx() : m_zeraryFxColumn(0), m_fx(0) {
  setName(L"ZeraryColumn");
}

//-------------------------------------------------------------------

TZeraryColumnFx::~TZeraryColumnFx() {
  if (m_zeraryFxColumn) m_zeraryFxColumn->release();
  if (m_fx) {
    m_fx->m_columnFx = 0;
    m_fx->release();
  }
}

//-------------------------------------------------------------------

void TZeraryColumnFx::doCompute(TTile &tile, double frame,
                                const TRenderSettings &info) {
  if (m_zeraryFxColumn) {
    TRasterFx *fx = dynamic_cast<TRasterFx *>(m_fx);
    if (fx) fx->compute(tile, frame, info);
  }
}

//-------------------------------------------------------------------

bool TZeraryColumnFx::doGetBBox(double frame, TRectD &bBox,
                                const TRenderSettings &info) {
  if (m_zeraryFxColumn) {
    TRasterFx *fx = dynamic_cast<TRasterFx *>(m_fx);
    if (fx) return fx->doGetBBox(frame, bBox, info);
  }

  return false;
}

//-------------------------------------------------------------------

const TPersistDeclaration *TZeraryColumnFx::getDeclaration() const {
  return &zeraryColumnFxInfo;
}

//-------------------------------------------------------------------

std::string TZeraryColumnFx::getPluginId() const { return "Toonz_"; }

//-------------------------------------------------------------------

TFxTimeRegion TZeraryColumnFx::getTimeRegion() const {
  return TFxTimeRegion(0, (std::numeric_limits<double>::max)());
}

//-------------------------------------------------------------------

void TZeraryColumnFx::setColumn(TXshZeraryFxColumn *column) {
  m_zeraryFxColumn = column;
}

//-------------------------------------------------------------------

std::wstring TZeraryColumnFx::getColumnName() const {
  return getZeraryFx()->getName();
}

//-------------------------------------------------------------------

std::wstring TZeraryColumnFx::getColumnId() const {
  return getZeraryFx()->getFxId();
}

//-------------------------------------------------------------------

void TZeraryColumnFx::setZeraryFx(TFx *fx) {
  TZeraryFx *zfx = static_cast<TZeraryFx *>(fx);

  if (fx) {
    assert(dynamic_cast<TZeraryFx *>(fx));

    fx->addRef();
    fx->setNewIdentifier();  // When adopting zerary fxs, ensure that they
                             // have a new numeric identifier
    zfx->m_columnFx = this;
  }

  if (m_fx) {
    m_fx->m_columnFx = 0;
    m_fx->release();
  }

  m_fx = zfx;
}

//-------------------------------------------------------------------

std::string TZeraryColumnFx::getAlias(double frame,
                                      const TRenderSettings &info) const {
  return "TZeraryColumnFx[" + m_fx->getAlias(frame, info) + "]";
}

//-------------------------------------------------------------------

void TZeraryColumnFx::loadData(TIStream &is) {
  if (m_fx) m_fx->release();

  m_fx        = 0;
  TPersist *p = 0;
  is >> p;

  m_fx = dynamic_cast<TZeraryFx *>(p);
  if (m_fx) {
    m_fx->addRef();
    m_fx->m_columnFx = this;
    m_fx->setNewIdentifier();
  }

  TFx::loadData(is);
  setNewIdentifier();  // Same as with TColumnFx
}

//-------------------------------------------------------------------

void TZeraryColumnFx::saveData(TOStream &os) {
  assert(m_fx);
  os << m_fx;
  TFx::saveData(os);
}

//-------------------------------------------------------------------

int TZeraryColumnFx::getColumnIndex() const {
  return m_zeraryFxColumn ? m_zeraryFxColumn->getIndex() : -1;
}

//-------------------------------------------------------------------

TXshColumn *TZeraryColumnFx::getXshColumn() const { return m_zeraryFxColumn; }

//****************************************************************************************
//    TXSheetFx  implementation
//****************************************************************************************

const TPersistDeclaration *TXsheetFx::getDeclaration() const {
  return &infoTXsheetFx;
}

//-------------------------------------------------------------------

TXsheetFx::TXsheetFx() : m_fxDag(0) { setName(L"Xsheet"); }

//-------------------------------------------------------------------

void TXsheetFx::doCompute(TTile &tile, double frame, const TRenderSettings &) {}

//-------------------------------------------------------------------

bool TXsheetFx::doGetBBox(double frame, TRectD &bBox,
                          const TRenderSettings &info) {
  return false;
}

//-------------------------------------------------------------------

std::string TXsheetFx::getPluginId() const { return "Toonz_"; }

//-------------------------------------------------------------------

std::string TXsheetFx::getAlias(double frame,
                                const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  // Add each terminal fx's alias
  TFxSet *terminalFxs = m_fxDag->getTerminalFxs();
  int i, fxsCount = terminalFxs->getFxCount();
  for (i = 0; i < fxsCount; ++i) {
    alias +=
        static_cast<TRasterFx *>(terminalFxs->getFx(i))->getAlias(frame, info) +
        ",";
  }

  return alias + "]";
}

//-------------------------------------------------------------------

void TXsheetFx::setFxDag(FxDag *fxDag) { m_fxDag = fxDag; }

//****************************************************************************************
//    TOutputFx  implementation
//****************************************************************************************

TOutputFx::TOutputFx() {
  addInputPort("source", m_input);
  setName(L"Output");
}

//-------------------------------------------------------------------

const TPersistDeclaration *TOutputFx::getDeclaration() const {
  return &infoTOutputFx;
}

//-------------------------------------------------------------------

void TOutputFx::doCompute(TTile &tile, double frame, const TRenderSettings &) {}

//-------------------------------------------------------------------

bool TOutputFx::doGetBBox(double frame, TRectD &bBox,
                          const TRenderSettings &info) {
  return false;
}

//-------------------------------------------------------------------

std::string TOutputFx::getPluginId() const { return "Toonz_"; }
