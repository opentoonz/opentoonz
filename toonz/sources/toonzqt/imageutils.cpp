

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/namebuilder.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/Naa2TlvConverter.h"
#include "toonz/toonzimageutils.h"

#ifdef _WIN32
#include "avicodecrestrictions.h"
#endif

// TnzCore includes
#include "tsystem.h"
#include "tfilepath_io.h"
#include "timage_io.h"
#include "tlevel.h"
#include "tlevel_io.h"
#include "tpalette.h"
#include "tropcm.h"
#include "trasterimage.h"
#include "tvectorrenderdata.h"
#include "tofflinegl.h"
#include "tconvert.h"
#include "trop.h"
#include "timageinfo.h"
#include "tfiletype.h"
#include "tfilepath.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "tstroke.h"

// TnzQt includes
#include <QApplication>
#ifdef _WIN32
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

// boost includes
#include <boost/iterator/transform_iterator.hpp>

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

void addOverlappedRegions(TRegion *reg, std::vector<TFilledRegionInf> &regInf) {
  regInf.push_back(TFilledRegionInf(reg->getId(), reg->getStyle()));
  UINT regNum = reg->getSubregionCount();
  for (UINT i = 0; i < regNum; i++)
    addOverlappedRegions(reg->getSubregion(i), regInf);
}

//--------------------------------------------------------------------

void addRegionsInArea(TRegion *reg, std::vector<TFilledRegionInf> &regs,
                      const TRectD &area) {
  if (area.contains(reg->getBBox()))
    regs.push_back(TFilledRegionInf(reg->getId(), reg->getStyle()));

  if (reg->getBBox().overlaps(area)) {
    UINT regNum = reg->getSubregionCount();
    for (UINT i = 0; i < regNum; i++)
      addRegionsInArea(reg->getSubregion(i), regs, area);
  }
}

//--------------------------------------------------------------------

void getFrameIds(TFrameId from, TFrameId to, const TLevelP &level,
                 std::vector<TFrameId> &frames) {
  struct locals {
    static inline TFrameId getFrame(const TLevel::Table::value_type &pair) {
      return pair.first;
    }
  };  // locals

  if (from.isEmptyFrame()) from = TFrameId(-(std::numeric_limits<int>::max)());

  if (to.isEmptyFrame()) to = TFrameId((std::numeric_limits<int>::max)());

  if (from > to) std::swap(from, to);

  const TLevel::Table &table = *level->getTable();

  TLevel::Table::const_iterator lBegin = table.lower_bound(from),
                                lEnd   = table.upper_bound(to);

  assert(frames.empty());
  frames.insert(frames.end(),
                boost::make_transform_iterator(lBegin, locals::getFrame),
                boost::make_transform_iterator(lEnd, locals::getFrame));
}

}  // namespace

//**********************************************************************************
//    ImageUtils namespace  stuff
//**********************************************************************************

namespace ImageUtils {

// todo: spostare da qualche altra parte. rendere accessibile a tutti.
// utilizzare
// anche nella libreria dovunque si usi direttamente "_files"
static TFilePath getResourceFolder(const TFilePath &scenePath) {
  return scenePath.getParentDir() + (scenePath.getName() + "_files");
}

//-----------------------------------------------------------------------------

static void copyScene(const TFilePath &dst, const TFilePath &src) {
  TSystem::copyFile(dst, src);
  if (TProjectManager::instance()->isTabModeEnabled())
    TSystem::copyDir(getResourceFolder(dst), getResourceFolder(src));
  TFilePath srcIcon = ToonzScene::getIconPath(src);
  if (TFileStatus(srcIcon).doesExist()) {
    TFilePath dstIcon = ToonzScene::getIconPath(dst);
    TSystem::copyFile(dstIcon, srcIcon);
  }
}

//-----------------------------------------------------------------------------

TFilePath duplicate(const TFilePath &levelPath) {
  if (levelPath == TFilePath()) return TFilePath();

  if (!TSystem::doesExistFileOrLevel(levelPath)) {
    DVGui::warning(
        QObject::tr("It is not possible to find the %1 level.")
            .arg(QString::fromStdWString(levelPath.getWideString())));
    return TFilePath();
  }

  NameBuilder *nameBuilder =
      NameBuilder::getBuilder(::to_wstring(levelPath.getName()));
  std::wstring levelNameOut;
  do
    levelNameOut = nameBuilder->getNext();
  while (TSystem::doesExistFileOrLevel(levelPath.withName(levelNameOut)));

  TFilePath levelPathOut = levelPath.withName(levelNameOut);

  try {
    if (levelPath.getType() == "tnz")
      copyScene(levelPathOut, levelPath);
    else {
      TSystem::copyFileOrLevel_throw(levelPathOut, levelPath);
      if (levelPath.getType() == "tlv") {
        TFilePath pltPath = levelPath.withType("tpl");
        if (TSystem::doesExistFileOrLevel(pltPath))
          TSystem::copyFileOrLevel_throw(levelPathOut.withType("tpl"), pltPath);
      }
    }
  } catch (...) {
    QString msg =
        QObject::tr("There was an error copying %1").arg(toQString(levelPath));
    DVGui::error(msg);
    return TFilePath();
  }

  //  QString msg = QString("Copied ") +
  //                QString::fromStdWString(levelPath.withoutParentDir().getWideString())
  //                +
  //                QString(" to " +
  //                QString::fromStdWString(levelPathOut.withoutParentDir().getWideString()));
  //  DVGui::info(msg);

  return levelPathOut;
}

//-----------------------------------------------------------------------------

void premultiply(const TFilePath &levelPath) {
  if (levelPath == TFilePath()) return;

  if (!TSystem::doesExistFileOrLevel(levelPath)) {
    DVGui::warning(
        QObject::tr("It is not possible to find the level %1")
            .arg(QString::fromStdWString(levelPath.getWideString())));
    return;
  }

  TFileType::Type type = TFileType::getInfo(levelPath);
  if (type == TFileType::CMAPPED_LEVEL) {
    DVGui::warning(QObject::tr("Cannot premultiply the selected file."));
    return;
  }
  if (type == TFileType::VECTOR_LEVEL || type == TFileType::VECTOR_IMAGE) {
    DVGui::warning(QObject::tr("Cannot premultiply a vector-based level."));
    return;
  }
  if (type != TFileType::RASTER_LEVEL && type != TFileType::RASTER_IMAGE) {
    DVGui::info(QObject::tr("Cannot premultiply the selected file."));
    return;
  }

  try {
    TLevelReaderP lr = TLevelReaderP(levelPath);
    if (!lr) return;
    TLevelP level = lr->loadInfo();
    if (!level || level->getFrameCount() == 0) return;

    bool isMovie = type == TFileType::RASTER_LEVEL;
    /*
if (!isMovie && lr->getImageInfo()->m_samplePerPixel!=4)
{
QMessageBox::information(0, QString("ERROR"), QString("Only rgbm images can be
premultiplied!"));
return;
}
*/

    TLevelWriterP lw;
    if (!isMovie) {
      lw = TLevelWriterP(levelPath);
      if (!lw) return;
    }

    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
    qApp->processEvents();

    int count           = 0;
    TLevel::Iterator it = level->begin();

    for (; it != level->end(); ++it) {
      TImageReaderP ir = lr->getFrameReader(it->first);

      TRasterImageP rimg = (TRasterImageP)ir->load();
      if (!rimg) continue;

      TRop::premultiply(rimg->getRaster());
      ir = 0;

      if (isMovie)
        level->setFrame(it->first, rimg);
      else {
        TImageWriterP iw = lw->getFrameWriter(it->first);
        iw->save(levelPath.withFrame(it->first), rimg);
        iw = 0;
      }
    }
    lr = TLevelReaderP();

    if (isMovie) {
      TSystem::deleteFile(levelPath);
      lw = TLevelWriterP(levelPath);
      if (!lw) {
        QApplication::restoreOverrideCursor();
        return;
      }
      lw->save(level);
    }
  } catch (...) {
    DVGui::warning(QObject::tr("Cannot premultiply the selected file."));
    QApplication::restoreOverrideCursor();
    return;
  }

  QApplication::restoreOverrideCursor();
  QString msg = QObject::tr("Level %1 premultiplied.")
                    .arg(QString::fromStdString(levelPath.getLevelName()));
  DVGui::info(msg);
}

//-----------------------------------------------------------------------------

void getFillingInformationOverlappingArea(const TVectorImageP &vi,
                                          std::vector<TFilledRegionInf> &regInf,
                                          const TRectD &area1,
                                          const TRectD &area2) {
  if (!vi->isComputedRegionAlmostOnce()) return;

  assert(area1 != TRectD() || area2 != TRectD());

  vi->findRegions();
  UINT regNum = vi->getRegionCount();
  TRegion *reg;

  for (UINT i = 0; i < regNum; i++) {
    reg = vi->getRegion(i);
    if (reg->getBBox().overlaps(area1) || reg->getBBox().overlaps(area2))
      addOverlappedRegions(reg, regInf);
  }
}

//-----------------------------------------------------------------------------

void getFillingInformationInArea(const TVectorImageP &vi,
                                 std::vector<TFilledRegionInf> &regs,
                                 const TRectD &area) {
  if (!vi->isComputedRegionAlmostOnce()) return;
  vi->findRegions();
  UINT regNum = vi->getRegionCount();

  for (UINT i = 0; i < regNum; i++)
    addRegionsInArea(vi->getRegion(i), regs, area);
}

//--------------------------------------------------------------------

void assignFillingInformation(TVectorImage &vi,
                              const std::vector<TFilledRegionInf> &regs) {
  vi.findRegions();

  UINT r, rCount = UINT(regs.size());
  for (r = 0; r != rCount; ++r) {
    const TFilledRegionInf &rInfo = regs[r];

    if (TRegion *region = vi.getRegion(rInfo.m_regionId))
      region->setStyle(rInfo.m_styleId);
  }
}

//--------------------------------------------------------------------

void getStrokeStyleInformationInArea(
    const TVectorImageP &vi, std::vector<std::pair<int, int>> &strokesInfo,
    const TRectD &area) {
  if (!vi->isComputedRegionAlmostOnce()) return;
  vi->findRegions();
  UINT regNum = vi->getStrokeCount();

  for (UINT i = 0; i < vi->getStrokeCount(); i++) {
    if (!vi->inCurrentGroup(i)) continue;
    if (area.contains(vi->getStroke(i)->getBBox()))
      strokesInfo.push_back(
          std::pair<int, int>(i, vi->getStroke(i)->getStyle()));
  }
}

//--------------------------------------------------------------------
static void convertFromCM(
    const TLevelReaderP &lr, const TPaletteP &plt, const TLevelWriterP &lw,
    const std::vector<TFrameId> &frames, const TAffine &aff,
    const TRop::ResampleFilterType &resType, FrameTaskNotifier *frameNotifier,
    const TPixel &bgColor, bool removeDotBeforeFrameNumber = false) {
  for (int i = 0; i < (int)frames.size(); i++) {
    if (frameNotifier->abortTask()) break;
    try {
      TImageReaderP ir = lr->getFrameReader(frames[i]);
      TImageP img      = ir->load();
      TToonzImageP toonzImage(img);
      double xdpi, ydpi;
      toonzImage->getDpi(xdpi, ydpi);
      assert(toonzImage);
      if (toonzImage) {
        TRasterCM32P rasCMImage = toonzImage->getRaster();
        TRaster32P ras(
            convert(aff * convert(rasCMImage->getBounds())).getSize());
        if (!aff.isIdentity())
          TRop::resample(ras, rasCMImage, plt, aff, resType);
        else
          TRop::convert(ras, rasCMImage, plt);
        if (bgColor != TPixel::Transparent) TRop::addBackground(ras, bgColor);

        TRasterImageP rasImage(ras);
        rasImage->setDpi(xdpi, ydpi);

        /*---
                ConvertPopup
        での指定に合わせて、[レベル名].[フレーム番号].[拡張子]
                のうち、[レベル名]と[フレーム番号]の間のドットを消す。
                例：A.0001.tga → A0001.tga になる。
        ---*/
        if (removeDotBeforeFrameNumber) {
          TFilePath outPath = lw->getFilePath().withFrame(frames[i]);
          /*--- フレーム番号と拡張子の文字数の合計。大抵8 ---*/
          int index               = 4 + 1 + outPath.getType().length();
          std::wstring renamedStr = outPath.getWideString();
          if (renamedStr[renamedStr.length() - index - 1] == L'.')
            renamedStr = renamedStr.substr(0, renamedStr.length() - index - 1) +
                         renamedStr.substr(renamedStr.length() - index, index);

          const TFilePath fp(renamedStr);
          TImageWriterP writer(fp);
          writer->setProperties(lw->getProperties());
          writer->save(rasImage);
        } else {
          TImageWriterP iw = lw->getFrameWriter(frames[i]);
          iw->save(rasImage);
        }
      }
    } catch (...) {
      // QString msg=QObject::tr("Frame %1 : conversion
      // failed!").arg(QString::number(i+1));
      //      DVGui::info(msg);
    }
    /*--
     * これはプログレスバーを進めるものなので、動画番号ではなく、完了したフレームの枚数を投げる
     * --*/
    frameNotifier->notifyFrameCompleted(100 * (i + 1) / frames.size());
  }
}

//--------------------------------------------------------------------

static void convertFromVI(const TLevelReaderP &lr, const TPaletteP &plt,
                          const TLevelWriterP &lw,
                          const std::vector<TFrameId> &frames,
                          const TRop::ResampleFilterType &resType, int width,
                          FrameTaskNotifier *frameNotifier) {
  QString msg;
  int i;
  std::vector<TVectorImageP> images;
  TRectD maxBbox;
  for (i = 0; i < (int)frames.size();
       i++) {  // trovo la bbox che possa contenere tutte le immagini
    try {
      TImageReaderP ir  = lr->getFrameReader(frames[i]);
      TVectorImageP img = ir->load();
      images.push_back(img);
      maxBbox += img->getBBox();
    } catch (...) {
      msg = QObject::tr("Frame %1 : conversion failed!")
                .arg(QString::fromStdString(frames[i].expand()));
      DVGui::info(msg);
    }
  }
  maxBbox = maxBbox.enlarge(2);
  TAffine aff;
  if (width)  // calcolo l'affine
    aff   = TScale((double)width / maxBbox.getLx());
  maxBbox = aff * maxBbox;

  for (i = 0; i < (int)images.size(); i++) {
    if (frameNotifier->abortTask()) break;
    try {
      TVectorImageP vectorImage = images[i];
      assert(vectorImage);
      if (vectorImage) {
        // faccio il render dell'immagine
        vectorImage->transform(aff, true);
        const TVectorRenderData rd(TTranslation(-maxBbox.getP00()), TRect(),
                                   plt.getPointer(), 0, true, true);
        TOfflineGL *glContext = new TOfflineGL(convert(maxBbox).getSize());
        glContext->clear(TPixel32::Transparent);
        glContext->draw(vectorImage, rd);
        TRaster32P rasImage = (glContext->getRaster());
        TImageWriterP iw    = lw->getFrameWriter(TFrameId(i + 1));
        iw->save(TRasterImageP(rasImage));
      }
    } catch (...) {
      msg = QObject::tr("Frame %1 : conversion failed!")
                .arg(QString::fromStdString(frames[i].expand()));
      DVGui::info(msg);
    }
    frameNotifier->notifyFrameCompleted(100 * (i + 1) / frames.size());
  }
}

//-----------------------------------------------------------------------

static void convertFromFullRaster(
    const TLevelReaderP &lr, const TLevelWriterP &lw,
    const std::vector<TFrameId> &_frames, const TAffine &aff,
    const TRop::ResampleFilterType &resType, FrameTaskNotifier *frameNotifier,
    const TPixel &bgColor, bool removeDotBeforeFrameNumber = false) {
  std::vector<TFrameId> frames = _frames;
  if (frames.empty() &&
      lr->loadInfo()->getFrameCount() ==
          1)  // e' una immagine singola, non un livello
    frames.push_back(TFrameId());

  for (int i = 0; i < (int)frames.size(); i++) {
    if (frameNotifier->abortTask()) break;
    try {
      TRasterImageP img;
      if (frames[i] == TFrameId())  // immagine singola, non livello
      {
        assert(frames.size() == 1);
        TImageP _img;
        if (!TImageReader::load(lr->getFilePath(), _img)) continue;
        img = _img;
      } else {
        TImageReaderP ir = lr->getFrameReader(frames[i]);
        img              = ir->load();
      }
      TRaster32P raster(convert(aff * img->getBBox()).getSize());
      if (!aff.isIdentity())
        TRop::resample(raster, img->getRaster(), aff, resType);
      else {
        if ((TRaster32P)img->getRaster())
          raster = img->getRaster();
        else
          TRop::convert(raster, img->getRaster());
      }
      if (bgColor != TPixel::Transparent) TRop::addBackground(raster, bgColor);

      TRasterImageP newImg(raster);
      double xDpi, yDpi;
      img->getDpi(xDpi, yDpi);
      if (xDpi == 0 && yDpi == 0)
        xDpi = yDpi = Preferences::instance()->getDefLevelDpi();
      newImg->setDpi(xDpi, yDpi);
      if (frames[i] == TFrameId())
        TImageWriter::save(lw->getFilePath(), newImg);
      else {
        /*---
ConvertPopup での指定に合わせて、[レベル名].[フレーム番号].[拡張子]
のうち、[レベル名]と[フレーム番号]の間のドットを消す。
例：A.0001.tga → A0001.tga になる。
---*/
        if (removeDotBeforeFrameNumber) {
          TFilePath outPath = lw->getFilePath().withFrame(frames[i]);
          /*--- フレーム番号と拡張子の文字数の合計。大抵8 ---*/
          int index               = 4 + 1 + outPath.getType().length();
          std::wstring renamedStr = outPath.getWideString();
          if (renamedStr[renamedStr.length() - index - 1] == L'.')
            renamedStr = renamedStr.substr(0, renamedStr.length() - index - 1) +
                         renamedStr.substr(renamedStr.length() - index, index);
          const TFilePath fp(renamedStr);
          TImageWriterP writer(fp);
          writer->setProperties(lw->getProperties());
          writer->save(newImg);
        } else {
          TImageWriterP iw = lw->getFrameWriter(frames[i]);
          iw->save(newImg);
        }
      }
    } catch (...) {
      // QString msg=QObject::tr("Frame %1 : conversion
      // failed!").arg(QString::number(i+1));
      // DVGui::info(msg);
    }
    /*--
     * これはプログレスバーを進めるものなので、動画番号ではなく、完了したフレームの枚数を投げる
     * --*/
    frameNotifier->notifyFrameCompleted(100 * (i + 1) / frames.size());
  }
}

//-----------------------------------------------------------------------

static void convertFromVector(const TLevelReaderP &lr, const TLevelWriterP &lw,
                              const std::vector<TFrameId> &_frames,
                              FrameTaskNotifier *frameNotifier) {
  std::vector<TFrameId> frames = _frames;
  TLevelP lv                   = lr->loadInfo();
  if (frames.empty() &&
      lv->getFrameCount() == 1)  // e' una immagine singola, non un livello
    frames.push_back(TFrameId());

  for (int i = 0; i < (int)frames.size(); i++) {
    if (frameNotifier->abortTask()) break;
    try {
      TVectorImageP img;
      if (frames[i] == TFrameId())  // immagine singola, non livello
      {
        assert(frames.size() == 1);
        TImageP _img;
        if (!TImageReader::load(lr->getFilePath(), _img)) continue;
        img = _img;

      } else {
        TImageReaderP ir = lr->getFrameReader(frames[i]);
        img              = ir->load();
      }

      img->setPalette(lv->getPalette());

      if (frames[i] == TFrameId())
        TImageWriter::save(lw->getFilePath(), img);
      else {
        TImageWriterP iw = lw->getFrameWriter(frames[i]);
        iw->save(img);
      }
    } catch (...) {
      // QString msg=QObject::tr("Frame %1 : conversion
      // failed!").arg(QString::number(i+1));
      // DVGui::info(msg);
    }
    frameNotifier->notifyFrameCompleted(100 * (i + 1) / frames.size());
  }
}

//-----------------------------------------------------------------------

void convert(const TFilePath &source, const TFilePath &dest,
             const TFrameId &from, const TFrameId &to, double framerate,
             TPropertyGroup *prop, FrameTaskNotifier *frameNotifier,
             const TPixel &bgColor, bool removeDotBeforeFrameNumber) {
  std::string dstExt = dest.getType(), srcExt = source.getType();

  // Load source level structure
  TLevelReaderP lr(source);
  TLevelP level = lr->loadInfo();

#ifdef _WIN32
  if (dstExt == "avi") {
    TDimension res(0, 0);

    const TImageInfo *info = lr->getImageInfo(level->begin()->first);
    res.lx                 = info->m_lx;
    res.ly                 = info->m_ly;

    std::string codecName = prop->getProperty(0)->getValueAsString();
    if (!AviCodecRestrictions::canWriteMovie(::to_wstring(codecName), res)) {
      return;
    }
  }
#endif

  // Get the frames available in level inside the [from, to] range
  std::vector<TFrameId> frames;
  getFrameIds(from, to, level, frames);

  // Write the destination level
  TLevelWriterP lw(dest, prop);
  lw->setFrameRate(framerate);

  if (srcExt == "tlv")
    convertFromCM(lr, level->getPalette(), lw, frames, TAffine(),
                  TRop::Triangle, frameNotifier, bgColor,
                  removeDotBeforeFrameNumber);
  else if (srcExt == "pli")
    // assert(!"Conversion from pli files is currently diabled");
    convertFromVector(lr, lw, frames, frameNotifier);
  else
    convertFromFullRaster(lr, lw, frames, TAffine(), TRop::Triangle,
                          frameNotifier, bgColor, removeDotBeforeFrameNumber);
}

//=============================================================================

void convertNaa2Tlv(const TFilePath &source, const TFilePath &dest,
                    const TFrameId &from, const TFrameId &to,
                    FrameTaskNotifier *frameNotifier, TPalette *palette,
                    bool removeUnusedStyles, double dpi) {
  std::string dstExt = dest.getType(), srcExt = source.getType();

  // Load source level structure
  TLevelReaderP lr(source);
  TLevelP level = lr->loadInfo();

  // Get the frames available in level inside the [from, to] range
  std::vector<TFrameId> frames;
  getFrameIds(from, to, level, frames);

  if (frames.empty()) return;

  // Write the destination level
  TLevelWriterP lw;  // Initialized just before a sure write

  Naa2TlvConverter converter;
  converter.setPalette(palette);

  QList<int> usedStyleIds({0});

  int f, fCount = int(frames.size());
  for (f = 0; f != fCount; ++f) {
    if (frameNotifier->abortTask()) break;

    try {
      TImageReaderP ir = lr->getFrameReader(frames[f]);
      TImageP img      = ir->load();

      if (!img) continue;

      TRasterImageP ri = img;
      if (!ri) continue;

      TRaster32P raster =
          ri->getRaster();  // Converts only 32-bit images, it seems...
      if (!raster)          //
        continue;           //

      converter.process(raster);

      if (TToonzImageP dstImg = converter.makeTlv(
              false, usedStyleIds, dpi))  // Opaque synthetic inks
      {
        if (converter.getPalette() == 0)
          converter.setPalette(dstImg->getPalette());

        if (!lw)  // Initialize output file only on a sure write
          lw = TLevelWriterP(dest);  //

        TImageWriterP iw = lw->getFrameWriter(frames[f]);
        iw->save(dstImg);
      } else {
        DVGui::warning(QObject::tr(
            "The source image seems not suitable for this kind of conversion"));
        frameNotifier->notifyError();
      }
    } catch (...) {
    }

    frameNotifier->notifyFrameCompleted(100 * (f + 1) / frames.size());
  }

  if (removeUnusedStyles) converter.removeUnusedStyles(usedStyleIds);
}

//=============================================================================

void convertOldLevel2Tlv(const TFilePath &source, const TFilePath &dest,
                         const TFrameId &from, const TFrameId &to,
                         FrameTaskNotifier *frameNotifier) {
  TFilePath destPltPath = TFilePath(dest.getParentDir().getWideString() +
                                    L"\\" + dest.getWideName() + L".tpl");
  if (TSystem::doesExistFileOrLevel(destPltPath))
    TSystem::removeFileOrLevel(destPltPath);

  TLevelWriterP lw(dest);
  lw->setIconSize(Preferences::instance()->getIconSize());
  TPaletteP palette =
      ToonzImageUtils::loadTzPalette(source.withType("plt").withNoFrame());
  TLevelReaderP lr(source);
  if (!lr) {
    DVGui::warning(QObject::tr(
        "The source image seems not suitable for this kind of conversion"));
    frameNotifier->notifyError();
    return;
  }
  TLevelP inLevel = lr->loadInfo();
  if (!inLevel || inLevel->getFrameCount() == 0) {
    DVGui::warning(QObject::tr(
        "The source image seems not suitable for this kind of conversion"));
    frameNotifier->notifyError();
    return;
  }
  // Get the frames available in level inside the [from, to] range
  std::vector<TFrameId> frames;
  getFrameIds(from, to, inLevel, frames);
  if (frames.empty()) return;

  TLevelP outLevel;
  outLevel->setPalette(palette.getPointer());
  try {
    int f, fCount = int(frames.size());
    for (f = 0; f != fCount; ++f) {
      if (frameNotifier->abortTask()) break;
      TToonzImageP img = lr->getFrameReader(frames[f])->load();
      if (!img) continue;
      img->setPalette(palette.getPointer());
      lw->getFrameWriter(frames[f])->save(img);

      frameNotifier->notifyFrameCompleted(100 * (f + 1) / frames.size());
    }
  } catch (TException &e) {
    QString msg = QString::fromStdWString(e.getMessage());
    DVGui::warning(msg);
    lw = TLevelWriterP();
    if (TSystem::doesExistFileOrLevel(dest)) TSystem::removeFileOrLevel(dest);
    frameNotifier->notifyError();
    return;
  }
  lw = TLevelWriterP();
}

//=============================================================================

#define ZOOMLEVELS 13
#define NOZOOMINDEX 6
double ZoomFactors[ZOOMLEVELS] = {
    0.015625, 0.03125, 0.0625, 0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32, 64};

double getQuantizedZoomFactor(double zf, bool forward) {
  if (forward && (zf > ZoomFactors[ZOOMLEVELS - 1] ||
                  areAlmostEqual(zf, ZoomFactors[ZOOMLEVELS - 1], 1e-5)))
    return zf;
  else if (!forward &&
           (zf < ZoomFactors[0] || areAlmostEqual(zf, ZoomFactors[0], 1e-5)))
    return zf;

  assert((!forward && zf > ZoomFactors[0]) ||
         (forward && zf < ZoomFactors[ZOOMLEVELS - 1]));
  int i = 0;
  for (i = 0; i <= ZOOMLEVELS - 1; i++)
    if (areAlmostEqual(zf, ZoomFactors[i], 1e-5)) zf = ZoomFactors[i];

  if (forward && zf < ZoomFactors[0])
    return ZoomFactors[0];
  else if (!forward && zf > ZoomFactors[ZOOMLEVELS - 1])
    return ZoomFactors[ZOOMLEVELS - 1];

  for (i = 0; i < ZOOMLEVELS - 1; i++)
    if (ZoomFactors[i + 1] - zf >= 0 && zf - ZoomFactors[i] >= 0) {
      if (forward && ZoomFactors[i + 1] == zf)
        return ZoomFactors[i + 2];
      else if (!forward && ZoomFactors[i] == zf)
        return ZoomFactors[i - 1];
      else
        return forward ? ZoomFactors[i + 1] : ZoomFactors[i];
    }
  return ZoomFactors[NOZOOMINDEX];
}

}  // namespace ImageUtils

namespace {

void getViewerShortcuts(int &zoomIn, int &zoomOut, int &zoomReset, int &zoomFit,
                        int &showHideFullScreen, int &actualPixelSize,
                        int &flipX, int &flipY) {
  CommandManager *cManager = CommandManager::instance();

  zoomIn = cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_ZoomIn));
  zoomOut =
      cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_ZoomOut));
  zoomReset =
      cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_ZoomReset));
  zoomFit =
      cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_ZoomFit));
  showHideFullScreen = cManager->getKeyFromShortcut(
      cManager->getShortcutFromId(V_ShowHideFullScreen));
  actualPixelSize = cManager->getKeyFromShortcut(
      cManager->getShortcutFromId(V_ActualPixelSize));
  flipX = cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_FlipX));
  flipY = cManager->getKeyFromShortcut(cManager->getShortcutFromId(V_FlipY));
}

}  // namespace

//--------------------------------------------------------------------------

namespace ImageUtils {

ShortcutZoomer::ShortcutZoomer(QWidget *zoomingWidget)
    : m_widget(zoomingWidget) {}

//--------------------------------------------------------------------------

bool ShortcutZoomer::exec(QKeyEvent *event) {
  int zoomInKey, zoomOutKey, zoomResetKey, zoomFitKey, showHideFullScreenKey,
      actualPixelSize, flipX, flipY;
  getViewerShortcuts(zoomInKey, zoomOutKey, zoomResetKey, zoomFitKey,
                     showHideFullScreenKey, actualPixelSize, flipX, flipY);

  int key = event->key();
  if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt)
    return false;

  key =
      key |
      event->modifiers() & (~0xf0000000);  // Ignore if the key is a numpad key

  return (key == showHideFullScreenKey)
             ? toggleFullScreen()
             : (key == Qt::Key_Escape)
                   ? toggleFullScreen(true)
                   : (key == actualPixelSize)
                         ? setActualPixelSize()
                         : (key == zoomFitKey)
                               ? fit()
                               : (key == zoomInKey || key == zoomOutKey ||
                                  key == zoomResetKey)
                                     ? zoom(key == zoomInKey,
                                            key == zoomResetKey)
                                     : (key == flipX)
                                           ? setFlipX()
                                           : (key == flipY) ? setFlipY()
                                                            : false;
}

//*********************************************************************************************
//    FullScreenWidget  implementation
//*********************************************************************************************

FullScreenWidget::FullScreenWidget(QWidget *parent) : QWidget(parent) {
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(0);

  setLayout(layout);

#ifdef _WIN32
  // http://doc.qt.io/qt-5/windows-issues.html#fullscreen-opengl-based-windows
  winId();
  QWindowsWindowFunctions::setHasBorderInFullScreen(windowHandle(), true);
#endif
}

//---------------------------------------------------------------------------------

void FullScreenWidget::setWidget(QWidget *widget) {
  QLayout *layout = this->layout();

  delete layout->takeAt(0);
  if ((m_widget = widget)) layout->addWidget(m_widget);
}

//---------------------------------------------------------------------------------

bool FullScreenWidget::toggleFullScreen(bool quit) {
  if (windowState() & Qt::WindowFullScreen) {
    hide();
    setWindowFlags(windowFlags() & ~(Qt::Window | Qt::WindowStaysOnTopHint));
    showNormal();
    m_widget->setFocus();
    return true;
  } else if (!quit) {
    setWindowFlags(windowFlags() | Qt::Window | Qt::WindowStaysOnTopHint);
    showFullScreen();

    return true;
  }

  return false;
}

}  // imageutils
