

#include "toonz/toonzimageutils.h"
#include "tstroke.h"
#include "tpalette.h"
#include "tofflinegl.h"
#include "tvectorrenderdata.h"
#include "tvectorgl.h"
#include "trop.h"
#include "toonz/ttileset.h"
#include "toonz/tcenterlinevectorizer.h"
#include "tregion.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "ttzpimagefx.h"
#include "tlevel_io.h"
#include "tsystem.h"
#include "tstream.h"
#include "tsimplecolorstyles.h"
//-------------------------------------------------------------------

namespace {

const int BackgroundStyle = 0;

//-------------------------------------------------------------------

TRect rasterizeStroke(TOfflineGL *&gl, const TRect &rasBounds, TStroke *stroke,
                      TPalette *palette, const TRectD &clip, bool doAntialias) {
  TRectD bbox = clip * stroke->getBBox();
  TRect rect  = convert(bbox).enlarge(1) * rasBounds;

  if (rect.getLx() <= 0 || rect.getLy() <= 0) return TRect();

  gl = new TOfflineGL(rect.getSize());
  gl->makeCurrent();
  gl->clear(TPixel32::White);

  TPaletteP plt      = palette->clone();
  int styleId        = stroke->getStyle();
  TColorStyleP style = plt->getStyle(styleId);
  assert(style);
  style->setMainColor(TPixel32::Black);

  TTranslation affine(-convert(rect.getP00()));
  TVectorRenderData rd(affine, gl->getBounds(), plt.getPointer(), 0, false,
                       doAntialias);
  tglDraw(rd, stroke);

  glFinish();

  return rect;
}

//-------------------------------------------------------------------

TRect rasterizeStroke(TOfflineGL *&gl, TRect rasBounds, TStroke *stroke,
                      TRectD clip, bool filled = false) {
  TDimension d = rasBounds.getSize();

  TPointD offset(d.lx * 0.5, d.ly * 0.5);
  TTranslation offsetMatrix(offset);

  TRectD strokeBBox = stroke->getBBox().enlarge(2);
  if (!clip.isEmpty())
    strokeBBox = offsetMatrix * (strokeBBox * clip);
  else
    strokeBBox = offsetMatrix * strokeBBox;

  TRect rect = ToonzImageUtils::convertWorldToRaster(strokeBBox, 0) * rasBounds;
  if (!rect.isEmpty()) {
    gl = new TOfflineGL(rect.getSize());
    gl->makeCurrent();
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    TPalette *palette = new TPalette();
    TTranslation affine(-strokeBBox.getP00() + offset);
    TVectorRenderData rd(affine, gl->getBounds(), palette, 0, false, true);

    int oldStyle = stroke->getStyle();
    stroke->setStyle(1);

    if (filled) {
      TVectorImage vi;
      vi.addStroke(new TStroke(*stroke));
      vi.findRegions();
      vi.selectFill(vi.getBBox().enlarge(2), 0, 1, false, true, false);
      tglDraw(rd, &vi);
    } else
      tglDraw(rd, stroke);

    delete palette;
    stroke->setStyle(oldStyle);
    glFinish();
  }
  return rect;
}

//-------------------------------------------------------------------

TRect rasterizeRegion(TOfflineGL *&gl, TRect rasBounds, TRegion *region,
                      TRectD clip) {
  TRectD regionBBox               = region->getBBox();
  if (!clip.isEmpty()) regionBBox = regionBBox * clip;

  TRect rect = convert(regionBBox) * rasBounds;
  if (!rect.isEmpty()) {
    gl = new TOfflineGL(rect.getSize());
    gl->makeCurrent();
    gl->clear(TPixel32::White);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

    TPalette *palette = new TPalette();
    TTranslation affine(-convert(rect.getP00()));
    TVectorRenderData rd(affine, gl->getBounds(), palette, 0, true, true);

    int oldStyle = region->getStyle();
    region->setStyle(1);
    tglDraw(rd, region);

    glDisable(GL_ALPHA_TEST);
    glPopAttrib();

    region->setStyle(oldStyle);
    glFinish();
    delete palette;
  }
  return rect;
}

//-------------------------------------------------------------------

void fastAddPaintRegion(const TToonzImageP &ti, TRegion *region, int newPaintId,
                        int maxStyleId, TRectD clip = TRectD()) {
  TRasterCM32P ras = ti->getRaster();
  TOfflineGL *gl;
  TRect rect = rasterizeRegion(gl, ras->getBounds(), region, clip);
  if (rect.isEmpty()) return;

  TRaster32P glRas = gl->getRaster();
  assert(TPixelCM32::getMaxTone() == 255);
  ras->lock();
  glRas->lock();

  for (int y = rect.y0; y <= rect.y1; y++) {
    TPixel32 *inPix    = glRas->pixels(y - rect.y0);
    TPixelCM32 *outPix = ras->pixels(y) + rect.x0;
    TPixel32 *inEndPix = inPix + rect.x1 - rect.x0 + 1;
    for (; inPix < inEndPix; ++outPix, ++inPix)
      if (inPix->r < 128) {
        outPix->setPaint(newPaintId);
        outPix->setTone(255);
      }
  }

  ras->unlock();
  glRas->unlock();

  delete gl;

  TRegion *subregion;
  UINT i = 0;
  for (; i < region->getSubregionCount(); ++i) {
    subregion = region->getSubregion(i);
    fastAddPaintRegion(ti, subregion,
                       std::min(maxStyleId, subregion->getStyle()), maxStyleId);
  }
}
}

//-------------------------------------------------------------------

TRect ToonzImageUtils::convertWorldToRaster(const TRectD area,
                                            const TToonzImageP ti) {
  if (area.isEmpty()) return TRect();
  if (!ti || !ti->getRaster())
    return TRect(tfloor(area.x0), tfloor(area.y0), tfloor(area.x1) - 1,
                 tfloor(area.y1) - 1);
  TRasterCM32P ras = ti->getRaster();
  TRectD rect(area + ras->getCenterD());
  return TRect(tfloor(rect.x0), tfloor(rect.y0), tceil(rect.x1) - 1,
               tceil(rect.y1) - 1);
}

//-------------------------------------------------------------------

TRectD ToonzImageUtils::convertRasterToWorld(const TRect area,
                                             const TToonzImageP ti) {
  if (area.isEmpty()) return TRectD();

  TRectD rect(area.x0, area.y0, area.x1 + 1, area.y1 + 1);
  if (ti && ti->getRaster()) rect = rect - ti->getRaster()->getCenterD();
  return rect;
}

//-------------------------------------------------------------------

// clip in coordinate world (cioe' della stroke)

// overlaying inks, blend inks always "lose" on normal inks

static TRect fastAddInkStroke(
    const TToonzImageP &ti, TStroke *stroke, int inkId, bool selective,
    bool filled, TRectD clip, bool doAntialiasing = true,
    const std::set<int> &blendInks = std::set<int>()) {
  TRasterCM32P ras = ti->getRaster();
  TOfflineGL *gl   = 0;

  TRect rectRender = rasterizeStroke(gl, ras->getBounds(), stroke,
                                     ti->getPalette(), clip, doAntialiasing);
  if (!gl) return TRect();

  TRaster32P glRas    = gl->getRaster();
  TRasterCM32P outRas = ras->extract(rectRender);
  assert(glRas->getSize() == outRas->getSize());

  assert(TPixelCM32::getMaxTone() == 255);
  outRas->lock();
  glRas->lock();
  bool isBlendInkUp = (blendInks.find(inkId) != blendInks.end());
  for (int y = 0; y < outRas->getLy(); ++y) {
    TPixel32 *inPix    = glRas->pixels(y);
    TPixelCM32 *outPix = outRas->pixels(y);
    for (int x = 0; x < outRas->getLx(); ++outPix, ++inPix, ++x) {
      int upTone = inPix->r;
      int dnTone = outPix->getTone();

      if (doAntialiasing) {
        // overlaying inks, blend inks always "lose" on normal inks

        bool isBlendInkDn =
            (blendInks.find(outPix->getInk()) != blendInks.end());

        if (isBlendInkUp && !isBlendInkDn && dnTone < 255) continue;

        if (isBlendInkDn && !isBlendInkUp && upTone < 255) {
          *outPix = TPixelCM32(inkId, outPix->getPaint(), upTone);
          continue;
        }

        // The upper pixel replaces the lower one, *if*
        //  a) It's more opaque
        //  b) If the ink id id not the same, ink is not completely transparent
        //  c)  ...
        if ((upTone <= dnTone) &&
            ((outPix->getInk() == inkId) || (upTone != 255)) &&
            (!selective || !outPix->isPureInk()))
          *outPix = TPixelCM32(inkId, outPix->getPaint(), upTone);
      } else {
        if (!selective || !outPix->isPureInk()) {
          // if(upTone<=192 && upTone<=dnTone)
          if (upTone == 0) *outPix = TPixelCM32(inkId, outPix->getPaint(), 0);
        }
      }
    }
  }
  outRas->unlock();
  glRas->unlock();
  delete gl;

  return rectRender;
}

// clip in coordinate world (cioe' della stroke)
TRect ToonzImageUtils::addInkStroke(const TToonzImageP &ti, TStroke *stroke,
                                    int inkId, bool selective, bool filled,
                                    TRectD clip, bool doAntialiasing) {
  TStroke *s      = new TStroke(*stroke);
  TPoint tiCenter = ti->getRaster()->getCenter();
  s->transform(TTranslation(tiCenter.x, tiCenter.y));
  TRect rect =
      fastAddInkStroke(ti, s, inkId, selective, filled, clip, doAntialiasing);
  rect -= tiCenter;
  return rect;
}

//-------------------------------------------------------------------

TRect ToonzImageUtils::changeColorStroke(
    const TToonzImageP &ti, const ChangeColorStrokeSettings &settings) {
  if (!settings.changeInk && !settings.changePaint) return TRect();

  TRasterCM32P ras = ti->getRaster();
  TOfflineGL *gl;
  TRect rect =
      rasterizeStroke(gl, ras->getBounds(), settings.stroke, settings.clip);
  if (rect.isEmpty()) {
    return rect;
  }
  TRaster32P glRas = gl->getRaster();
  ras->lock();
  glRas->lock();

  for (int y = rect.y0; y <= rect.y1; ++y) {
    TPixel32 *inPix    = glRas->pixels(y - rect.y0);
    TPixelCM32 *outPix = ras->pixels(y) + rect.x0;
    TPixel32 *inEndPix = inPix + rect.x1 - rect.x0 + 1;
    for (; inPix < inEndPix; ++outPix, ++inPix) {
      if (inPix->r < 128 && settings.changeInk && !outPix->isPurePaint())
        outPix->setInk(settings.colorId);
      if (inPix->r < 128 && settings.changePaint &&
          (settings.maskPaintId == -1 ||
           outPix->getPaint() == settings.maskPaintId))
        outPix->setPaint(settings.colorId);
    }
  }
  delete gl;
  ras->unlock();
  glRas->unlock();
  return rect;
}

//-------------------------------------------------------------------

TRect ToonzImageUtils::eraseRect(const TToonzImageP &ti, const TRectD &area,
                                 int maskId, bool onInk, bool onPaint) {
  assert(onInk || onPaint);
  TRasterCM32P ras = ti->getRaster();
  TRect rect       = convertWorldToRaster(area, ti) * ras->getBounds();
  if (rect.isEmpty()) return rect;
  ras->lock();

  for (int y = rect.y0; y <= rect.y1; y++) {
    TPixelCM32 *pix    = ras->pixels(y) + rect.x0;
    TPixelCM32 *endPix = ras->pixels(y) + rect.x1 + 1;
    for (; pix < endPix; ++pix) {
      if (onPaint && (maskId == -1 || maskId == pix->getPaint()))
        pix->setPaint(BackgroundStyle);

      if (onInk && (maskId == -1 || maskId == pix->getInk()))
        *pix = TPixelCM32(BackgroundStyle, pix->getPaint(),
                          TPixelCM32::getMaxTone());
    }
  }
  ras->unlock();
  return rect;
}

//-------------------------------------------------------------------

std::vector<TRect> ToonzImageUtils::paste(const TToonzImageP &ti,
                                          const TTileSetCM32 *tileSet) {
  std::vector<TRect> rects;
  TRasterCM32P raster = ti->getRaster();
  // for(int i=0;i<tileSet->getTileCount();i++)
  for (int i = tileSet->getTileCount() - 1; i >= 0; i--) {
    const TTileSetCM32::Tile *tile = tileSet->getTile(i);
    TRasterCM32P rasCM32;
    tile->getRaster(rasCM32);
    assert(!!rasCM32);
    raster->copy(rasCM32, tile->m_rasterBounds.getP00());
    rects.push_back(tile->m_rasterBounds);
  }
  return rects;
}

//-------------------------------------------------------------------

// DA RIFARE
// e' lenta da far schifo

//! Converts a TVectorImage into a TToonzImage. The input vector image
//! is transformed through the passed affine \b aff, and put into a
//! TToonzImage strictly covering the bounding box of the transformed
//! vector image. The output image has its lower-left position in the
//! world reference specified by the \b pos parameter, which is granted to
//! be an integer displacement of the passed value. Additional parameters
//! include an integer \b enlarge by which the output image is enlarged with
//! respect to the transformed image's bbox, and the bool \b transformThickness
//! to specify whether the transformation should involve strokes' thickensses
//! or not.
TToonzImageP ToonzImageUtils::vectorToToonzImage(
    const TVectorImageP &vimage, const TAffine &aff, TPalette *palette,
    const TPointD &outputPos, const TDimension &outputSize,
    const std::vector<TRasterFxRenderDataP> *fxs, bool transformThickness) {
  if (!vimage || !palette) return 0;

  // Transform the vector image through aff
  TVectorImageP vi = vimage->clone();
  vi->transform(aff, transformThickness);

  // Allocate the output ToonzImage
  TRasterCM32P raster(outputSize.lx, outputSize.ly);
  raster->clear();
  TToonzImageP ti(raster, raster->getBounds());
  ti->setPalette(palette->clone());

  // Shift outputPos to the origin
  vi->transform(TTranslation(-outputPos));

  int strokeCount = vi->getStrokeCount();
  std::vector<int> strokeIndex(strokeCount);
  std::vector<TStroke *> strokes(strokeCount);
  int maxStyleId = palette->getStyleCount() - 1;

  int i;
  for (i = 0; i < strokeCount; ++i) {
    strokeIndex[i] = i;
    strokes[i]     = vi->getStroke(i);
  }
  vi->notifyChangedStrokes(strokeIndex, strokes);
  int regionCount = vi->getRegionCount();

  // In such reference, the clip for rendering strokes is the output size
  TRectD clip(TDimensionD(outputSize.lx, outputSize.ly));

  std::set<int> colors;
  if (fxs) {
    for (i = 0; i < (int)fxs->size(); i++) {
      SandorFxRenderData *sandorData =
          dynamic_cast<SandorFxRenderData *>((*fxs)[i].getPointer());
      if (sandorData && sandorData->m_type == BlendTz) {
        std::string indexes =
            ::to_string(sandorData->m_blendParams.m_colorIndex);
        std::vector<std::string> items;
        parseIndexes(indexes, items);
        PaletteFilterFxRenderData paletteFilterData;
        insertIndexes(items, &paletteFilterData);
        colors.insert(paletteFilterData.m_colors.begin(),
                      paletteFilterData.m_colors.end());
      }
    }
  }

  int k, l;
  for (i = 0; i < strokeCount;) {
    // Draw all regions which have the same group.
    for (k = 0; k < regionCount; ++k)
      if (vi->areDifferentGroup(i, false, k, true) == -1) {
        TRegion *region = vi->getRegion(k);
        fastAddPaintRegion(ti, region, std::min(maxStyleId, region->getStyle()),
                           maxStyleId);
      }

    // Find the first stroke which does not belong to the group
    for (k = i;
         k < strokeCount && vi->areDifferentGroup(i, false, k, false) == -1;
         ++k)
      ;

    // Draw all found strokes
    for (l = i; l < k; ++l) {
      TStroke *stroke = vi->getStroke(l);

      bool visible       = false;
      int styleId        = stroke->getStyle();
      TColorStyleP style = palette->getStyle(styleId);
      assert(style);
      int colorCount = style->getColorParamCount();
      if (colorCount == 0)
        visible = true;
      else {
        visible = false;
        for (int j = 0; j < style->getColorParamCount() && !visible; j++) {
          TPixel32 color            = style->getColorParamValue(j);
          if (color.m != 0) visible = true;
        }
      }
      if (visible)
        fastAddInkStroke(ti, stroke, std::min(maxStyleId, stroke->getStyle()),
                         false, false, clip, true, colors);
    }
    i = k;
  }

  return ti;
}

//-------------------------------------------------------------------

TPalette *ToonzImageUtils::loadTzPalette(const TFilePath &pltFile) {
  TImageP pltImg;
  TImageReader loader(pltFile);
  pltImg = loader.load();

  TRasterImageP pltRasImg(pltImg);
  if (!pltRasImg) return 0;

  TRaster32P rasPlt = pltRasImg->getRaster();
  if (!rasPlt) return 0;

  std::map<int, std::pair<std::string, std::string>> pltColorNames;
  std::map<int, std::pair<std::string, std::string>>::iterator it;
  loader.getTzpPaletteColorNames(pltColorNames);

  TPalette *palette = new TPalette();

  const int offset = 0;

  assert(rasPlt->getLy() == 2);
  rasPlt->lock();
  TPixel32 *pixelRow = rasPlt->pixels(0);
  int x              = 0;
  int count          = palette->getStyleCount();

  for (; x < rasPlt->getLx(); ++x) {
    TSolidColorStyle *style = new TSolidColorStyle(pixelRow[x]);
    if ((it = pltColorNames.find(x)) != pltColorNames.end()) {
      std::string styleName = it->second.second;
      style->setName(::to_wstring(styleName));
    }
    if (x < count) palette->setStyle(x, style);
    // palette->setStyle(x, pixelRow[x]);
    else
      palette->addStyle(style);
  }

  // aggiungo solo i colori usati (salvo il BG)

  TPalette::Page *page = palette->getPage(0);
  // nella pagina c'e' gia' lo sfondo e il colore 1:
  // tolgo quest'ultimo
  page->removeStyle(1);
  // aggiungo gli altri
  std::map<std::wstring, int> pages;
  std::map<std::wstring, int>::iterator itpage;

  pixelRow = rasPlt->pixels(1);
  for (x = 0; x < rasPlt->getLx(); ++x) {
    if ((it = pltColorNames.find(x)) != pltColorNames.end()) {
      std::wstring pageName;
      pageName = ::to_wstring(it->second.first);
      if (x == 0) {
        page = palette->getPage(0);
        page->setName(pageName);
        pages[pageName] = 0;
      } else if ((itpage = pages.find(pageName)) != pages.end())
        page = palette->getPage(itpage->second);
      else {
        page            = palette->addPage(pageName);
        pages[pageName] = page->getIndex();
      }
    } else
      page = palette->getPage(0);

    if (pixelRow[x].r == 255) page->addStyle(offset + x);
  }
  rasPlt->unlock();

  return palette;
}

//-------------------------------------------------------------------

void ToonzImageUtils::getUsedStyles(std::set<int> &styles,
                                    const TToonzImageP &ti) {
  TRasterCM32P ras = ti->getRaster();
  if (!ras) return;
  int lx = ras->getLx();
  int ly = ras->getLy();
  ras->lock();
  for (int y = 0; y < ly; y++) {
    TPixelCM32 *pix    = ras->pixels(y);
    TPixelCM32 *endPix = pix + lx;
    while (pix < endPix) {
      styles.insert(pix->getInk());
      styles.insert(pix->getPaint());
      ++pix;
    }
  }
  ras->unlock();
}

void ToonzImageUtils::scrambleStyles(const TToonzImageP &ti,
                                     std::map<int, int> styleTable) {
  TRasterCM32P ras = ti->getRaster();
  if (!ras) return;
  if (styleTable.empty()) return;
  std::map<int, int>::iterator it;
  std::vector<int> lut(4096, -1);
  bool isIdentity = true;
  for (it = styleTable.begin(); it != styleTable.end(); ++it) {
    int j = it->first, k = it->second;
    assert(j >= 0);
    assert(j < 1000000);
    if (j >= (int)lut.size()) lut.resize(j + 1, -1);
    lut[j]                 = k;
    if (j != k) isIdentity = false;
  }
  if (isIdentity) return;

  int m  = lut.size();
  int lx = ras->getLx();
  int ly = ras->getLy();
  ras->lock();
  for (int y = 0; y < ly; y++) {
    TPixelCM32 *pix    = ras->pixels(y);
    TPixelCM32 *endPix = pix + lx;
    while (pix < endPix) {
      int ink                                               = pix->getInk();
      if (0 <= ink && ink < m && lut[ink] >= 0) ink         = lut[ink];
      int paint                                             = pix->getPaint();
      if (0 <= paint && paint < m && lut[paint] >= 0) paint = lut[paint];
      if (ink != pix->getInk() || paint != pix->getPaint()) {
        *pix = TPixelCM32(ink, paint, pix->getTone());
      }
      ++pix;
    }
  }
  ras->unlock();
}

//----------------------------------------------------------------------------------

#ifdef LEVO

bool ToonzImageUtils::convertToTlv(const TFilePath &levelPathIn) {
  try {
    TFilePath levelPathOut = levelPathIn.getParentDir() +
                             TFilePath(levelPathIn.getWideName() + L".tlv");

    TLevelReaderP lr(levelPathIn);
    TLevelP level = lr->loadInfo();

    TLevelWriterP lw(levelPathOut, 0);

    TPalette *plt = new TPalette();

    TLevel::Iterator it  = level->begin();
    TLevel::Iterator end = level->end();
    for (; it != level->end(); ++it) {
      try {
        TImageReaderP ir  = lr->getFrameReader(it->first);
        TRasterImageP img = ir->load();
        double dpix, dpiy;
        img->getDpi(dpix, dpiy);
        TRasterCM32P raster(convert(img->getBBox()).getSize());

        TRop::convert(raster, img->getRaster());

        TImageWriterP iw = lw->getFrameWriter(it->first);
        TToonzImageP outimg(raster, raster->getBounds());
        outimg->setDpi(dpix, dpiy);
        outimg->setPalette(plt);
        iw->save(outimg);

      } catch (...) {
        return false;
        // string msg="Frame "+toString(it->first.getNumber())+": conversion
        // failed!";
        // cout << msg << endl;
      }
    }

    TFilePath pltPath = lw->getFilePath().withNoFrame().withType("tpl");
    if (TSystem::touchParentDir(pltPath)) {
      if (TSystem::doesExistFileOrLevel(pltPath))
        TSystem::removeFileOrLevel(pltPath);
      TOStream os(pltPath);
      os << plt;
    }

    lr = TLevelReaderP();
    lw = TLevelWriterP();
    // delete plt;

    return true;
  } catch (...) {
    return false;
  }
}

#endif

//----------------------------------------------------------------------------------

void ToonzImageUtils::eraseImage(const TToonzImageP &ti,
                                 const TRaster32P &image, const TPoint &pos,
                                 bool invert, bool eraseInk, bool erasePaint,
                                 bool selective, int styleId) {
  TRect rasBounds   = ti->getRaster()->getBounds();
  TRect imageBounds = image->getBounds() + pos;

  if (invert) {
    /*----------------------------------------
    ┌───┬─┐
    │③　　│②│
    ├─┬─┤　│
    │④│★│　│
    │　├─┴─┤
    │　│①　　│
    └─┴───┘
★はFreeHandで囲んだ領域のバウンディングボックス
外側のワクはラスタ画像のフチ
-----------------------------------------*/
    TRect rect;
    /*- ①の部分を消す -*/
    if (rasBounds.y0 != imageBounds.y0) {
      rect = TRect(imageBounds.x0, rasBounds.y0, rasBounds.x1, imageBounds.y0);
      ToonzImageUtils::eraseRect(
          ti, ToonzImageUtils::convertRasterToWorld(rect, ti),
          selective ? styleId : -1, eraseInk, erasePaint);
    }
    /*- ②の部分を消す -*/
    if (imageBounds.x1 != rasBounds.x1) {
      rect = TRect(imageBounds.x1, imageBounds.y0, rasBounds.x1, rasBounds.y1);
      ToonzImageUtils::eraseRect(
          ti, ToonzImageUtils::convertRasterToWorld(rect, ti),
          selective ? styleId : -1, eraseInk, erasePaint);
    }
    /*- ③の部分を消す -*/
    if (imageBounds.y1 != rasBounds.y1) {
      rect = TRect(rasBounds.x0, imageBounds.y1, imageBounds.x1, rasBounds.y1);
      ToonzImageUtils::eraseRect(
          ti, ToonzImageUtils::convertRasterToWorld(rect, ti),
          selective ? styleId : -1, eraseInk, erasePaint);
    }
    /*- ④の部分を消す -*/
    if (rasBounds.x0 != imageBounds.x0) {
      rect = TRect(rasBounds.x0, rasBounds.y0, imageBounds.x0, imageBounds.y1);
      ToonzImageUtils::eraseRect(
          ti, ToonzImageUtils::convertRasterToWorld(rect, ti),
          selective ? styleId : -1, eraseInk, erasePaint);
    }
  }

  TRasterCM32P workRas = ti->getRaster()->extract(imageBounds);

  int y;
  for (y = 0; y < workRas->getLy(); y++) {
    TPixelCM32 *outPix    = workRas->pixels(y);
    TPixelCM32 *outEndPix = outPix + workRas->getLx();
    TPixel32 *inPix       = image->pixels(y);
    for (; outPix != outEndPix; outPix++, inPix++) {
      bool canEraseInk =
          !selective || (selective && styleId == outPix->getInk());
      bool canErasePaint =
          !selective || (selective && styleId == outPix->getPaint());

      int paint, tone;
      if (!invert) {
        paint = inPix->m > 0 && erasePaint && canErasePaint
                    ? 0
                    : outPix->getPaint();
        tone = inPix->m > 0 && eraseInk && canEraseInk
                   ? std::max(outPix->getTone(), (int)inPix->m)
                   : outPix->getTone();
      } else {
        paint = inPix->m < 255 && erasePaint && canErasePaint
                    ? 0
                    : outPix->getPaint();
        tone = inPix->m < 255 && eraseInk && canEraseInk
                   ? std::max(outPix->getTone(), 255 - (int)inPix->m)
                   : outPix->getTone();
      }
      *outPix = TPixelCM32(outPix->getInk(), paint, tone);
    }
  }
}

//-----------------------------------------------------------------------

std::string ToonzImageUtils::premultiply(const TFilePath &levelPath) {
  assert(0);
  /*
if (levelPath==TFilePath())
return "";

if(!TSystem::doesExistFileOrLevel(levelPath))
return string("Can't find level")+toString(levelPath.getWideString());

TFileType::Type type = TFileType::getInfo(levelPath);
if(type == TFileType::CMAPPED_LEVEL)
return "Cannot premultiply the selected file.";

if (type == TFileType::VECTOR_LEVEL || type == TFileType::VECTOR_IMAGE)
return "Cannot premultiply a vector-based level.";

if (type != TFileType::RASTER_LEVEL && type != TFileType::RASTER_IMAGE)
return "Cannot premultiply the selected file.";

try
{
TLevelReaderP lr = TLevelReaderP(levelPath);
if (!lr) return "";
TLevelP level =  lr->loadInfo();
if(!level || level->getFrameCount()==0) return "";
string format = levelPath.getType();


TPropertyGroup* prop =
TApplication::instance()
->getCurrentScene()
->getProperties()
->getOutputProperties()
->getFileFormatProperties(format)
->clone();
assert(prop);

TEnumProperty *p = (TEnumProperty*)prop->getProperty("Bits Per Pixel");
int bpp = p?atoi((toString(p->getValue()).c_str())):32;
if (bpp!=32 && bpp!=64) //non ha senso premoltiplicare senza il canale alpha...
{
if (bpp<32)
p->setValue(L"32(RGBM)");
else
p->setValue(L"64(RGBM)");
}

bool isMovie = (format=="mov" || format=="avi" || format=="3gp");


TLevelWriterP lw;
if (!isMovie)
{
lw = TLevelWriterP(levelPath, prop);
if (!lw) return "";
}

int count = 0;
TLevel::Iterator it = level->begin();

for (;it!=level->end(); ++it)
{
TImageReaderP ir = lr->getFrameReader(it->first);

TRasterImageP rimg = (TRasterImageP)ir->load();
if (!rimg)
continue;

TRop::premultiply(rimg->getRaster());
ir = 0;

if (isMovie)
level->setFrame(it->first, rimg);
else
{
TImageWriterP iw = lw->getFrameWriter(it->first);
iw->save(rimg);
iw = 0;
}
}

lr = TLevelReaderP();

if (isMovie)
{
TSystem::deleteFile(levelPath);
lw = TLevelWriterP(levelPath, prop);
if (!lw) return "";
lw->save(level);
}
if (prop)
delete prop;
}catch(...)
{
  return "Cannot premultiply the selected file.";
}
*/

  return "";
}
