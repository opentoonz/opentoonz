

#include "drawingdata.h"
#include "filmstripcommand.h"
#include "timagecache.h"
#include "tpaletteutil.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "trop.h"
#include "tstroke.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzimageutils.h"
#include "toonz/txshleveltypes.h"
#include "toonz/stage.h"
#include "toonz/tcenterlinevectorizer.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "vectorizerpopup.h"

#include <QApplication>

//=============================================================================
// DrawingData
//-----------------------------------------------------------------------------
namespace {

// aggiorna gli hook: sono stati inseriti shiftRange (>0) drawings a partire da
// startFrameId
// quindi tutti i riferimenti a frameId >= startFrameId devono essere aggiornati

void shiftHooks(TXshSimpleLevel *sl, const TFrameId &startFrameId,
                int shiftRange) {
  int i, startIndex = sl->guessIndex(startFrameId);
  assert(startIndex >= 0);
  if (startIndex < 0) return;
  HookSet *levelHooks = sl->getHookSet();
  for (i = sl->getFrameCount() - 1; i >= startIndex + shiftRange; i--) {
    TFrameId fid     = sl->index2fid(i);
    TFrameId precFid = sl->index2fid(i - shiftRange);
    assert(precFid >= startFrameId);
    if (precFid == TFrameId::NO_FRAME) break;
    int j, hookCount = levelHooks->getHookCount();
    for (j = 0; j < hookCount; j++) {
      Hook *hook = levelHooks->getHook(j);
      if (hook) {
        hook->setAPos(fid, hook->getAPos(precFid));
        hook->setBPos(fid, hook->getBPos(precFid));
      }
    }
  }
}

//-----------------------------------------------------------------------------

QString makeCacheId(UINT id, const TFrameId &fid) {
  return "DrawingData" + QString::number(id) + "-" +
         QString::number(fid.getNumber());
}

//-----------------------------------------------------------------------------

//
// frames[] is replaced by a sequence of imageSet.size() consecutive TFrameId's
// frames[0] is used to determine the 'pastePosition', i.e. the starting frame
// of the new sequence

void getFrameIdToInsert(std::set<TFrameId> &frames, TXshSimpleLevel *sl,
                        const std::map<TFrameId, QString> &imageSet) {
  assert(!frames.empty());
  int count              = (int)imageSet.size();
  TFrameId pastePosition = *frames.begin();
  // devo inserire prima di pastePosition. ci puo' essere un "buco".
  // es: frames=[1,3,5] selected=[3]  => devo inserire in 2 (creando un nuovo
  // frameid).

  std::vector<TFrameId> fids;
  sl->getFids(fids);
  // cerca fids[j] <= pastePosition
  // fids.beginからfids.endまで、frames.begin()に比べFrameIdが同じか大きいところまでjを進める
  //要するに、ペースト場所を選択フレーム範囲の一番上の部分にする。
  std::vector<TFrameId>::iterator j =
      std::lower_bound(fids.begin(), fids.end(), pastePosition);
  if (j != fids.begin()) {
    // pastePosition is not the first frameid of the level
    if (j != fids.end() && frames.find(*j) != frames.end()) {
      // Se il frameId e' gia' presente nel livello lo modifico.

      --j;
      pastePosition = *j + 1;
    }
  } else {
    // pastePosition e' il primo frame (o perche' fids[0]==pastePosition
    // o perche' pastePosition < fids[j], per ogni j
    if (pastePosition.getNumber() > 1)
      pastePosition = TFrameId(pastePosition.getNumber() - 1);
  }

  // pastePositionからコピーされたフレームの枚数分framesに挿入する。
  // frames <- i frames da inserire
  frames.clear();
  int i;
  for (i = 0; i < count; i++) frames.insert(pastePosition + i);
}

//-----------------------------------------------------------------------------

void rasterize(TToonzImageP &target, const TVectorImageP &source,
               const std::map<int, int> &styleTable) {
  double dpix, dpiy;
  target->getDpi(dpix, dpiy);
  assert(dpix != 0 && dpiy != 0);
  TScale sc(dpix / Stage::inch, dpiy / Stage::inch);

  TRectD bbox = sc * source->getBBox();
  bbox.x0     = tfloor(bbox.x0);
  bbox.y0     = tfloor(bbox.y0);
  bbox.x1     = tceil(bbox.x1);
  bbox.y1     = tceil(bbox.y1);
  TDimension size(bbox.getLx(), bbox.getLy());
  TToonzImageP app = ToonzImageUtils::vectorToToonzImage(
      source, sc, source->getPalette(), bbox.getP00(), size, 0, true);
  TRect rBbox = ToonzImageUtils::convertWorldToRaster(bbox, target);
  target->getCMapped()->copy(app->getCMapped(), rBbox.getP00());

  ToonzImageUtils::scrambleStyles(target, styleTable);
}

//-----------------------------------------------------------------------------

void vectorize(TVectorImageP &target, const TToonzImageP &source,
               const std::map<int, int> &styleTable,
               const VectorizerConfiguration &config) {
  VectorizerCore vc;
  TVectorImageP vi =
      vc.vectorize(source.getPointer(), config, source->getPalette());
  assert(vi);
  vi->setPalette(source->getPalette());

  double dpiX, dpiY;
  source->getDpi(dpiX, dpiY);

  TScale sc(dpiX / Stage::inch, dpiY / Stage::inch);
  TRect rBox   = source->getCMapped()->getBounds();
  TRectD wBbox = ToonzImageUtils::convertRasterToWorld(rBox, source);
  TTranslation tr(wBbox.getP00());

  int i;
  for (i = 0; i < (int)vi->getStrokeCount(); i++) {
    TStroke *stroke = vi->getStroke(i);
    stroke->transform(sc.inv() * tr, true);
  }
  target->mergeImage(vi, TAffine(), styleTable);
}

}  // namespace

DrawingData *DrawingData::clone() const { return new DrawingData(this); }

//-----------------------------------------------------------------------------

void DrawingData::setLevelFrames(TXshSimpleLevel *sl,
                                 std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;

  m_level = sl;
  m_imageSet.clear();

  std::set<TFrameId>::iterator it;

  HookSet *levelHooks = m_level->getHookSet();
  int i, hookCount = levelHooks->getHookCount();

  for (it = frames.begin(); it != frames.end(); ++it) {
    TFrameId frameId = *it;

    // Retrieve the image to be stored in this instance
    TImageP img =
        sl->getFullsampledFrame(  // Subsampling is explicitly removed, here.
            frameId, ImageManager::dontPutInCache);  // Furthermore, will not
                                                     // force the IM to cache
                                                     // it.
    if (!img) continue;

    // Clone the image and store it in the image cache
    QString id = makeCacheId(
        (uintptr_t) this,
        it->getNumber());  // Cloning is necessary since the user may
    TImageCache::instance()->add(
        id, img->cloneImage());  // modify and save the original AFTER the copy
    m_imageSet[frameId] = id;    // has been done.

    for (i = 0; i < hookCount; i++) {
      Hook *levelHook = levelHooks->getHook(i);
      if (!levelHook || levelHook->isEmpty()) continue;

      Hook *copiedHook            = m_levelHooks.getHook(i);
      if (!copiedHook) copiedHook = m_levelHooks.addHook();

      copiedHook->setAPos(frameId, levelHook->getAPos(frameId));
      copiedHook->setBPos(frameId, levelHook->getBPos(frameId));
    }
  }
}

//-----------------------------------------------------------------------------

//
// sl <= frames
//
// setType = INSERT | OVER_FRAMEID | OVER_SELECTION

//-----------------------------------------------------------------------------
// paste DrawingData to sl.
// frames: selected frames
// setType : INSERT->InsertPaste,  OVER_FRAMEID->Merge,
// OVER_SELECTION->PasteInto

bool DrawingData::getLevelFrames(TXshSimpleLevel *sl,
                                 std::set<TFrameId> &frames,
                                 ImageSetType setType, bool cloneImages,
                                 bool &keepOriginalPalette, bool isRedo) const {
  int slType = m_level->getType();
  if (m_imageSet.empty() || !sl ||
      (frames.empty() &&
       setType != DrawingData::OVER_FRAMEID) ||  // If setType == OVER_FRAMEID
                                                 // ignore frames
      (sl->getType() != slType &&  // Level types must be compatible...
       (sl->getType() != TZP_XSHLEVEL ||
        slType != PLI_XSHLEVEL) &&  // ...unless they are of type PLI and TLV
       (sl->getType() != PLI_XSHLEVEL || slType != TZP_XSHLEVEL)))  //
    return false;

  if (m_level.getPointer() == sl || isRedo) {
  }
  // when pasting the frame to different level
  else {
    QString question;
    question = "Replace palette ?";
    int ret  = DVGui::MsgBox(
        question, QObject::tr("Replace with copied palette"),
        QObject::tr("Keep original palette"), QObject::tr("Cancel"), 0);

    if (ret == 0 || ret == 3)
      return false;
    else if (ret == 1)
      keepOriginalPalette = false;
    else
      keepOriginalPalette = true;
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);

  // oldFids = old frame ids
  // renumberTable = old -> new
  // framesToInsert = new frames
  std::vector<TFrameId> oldFids;
  sl->getFids(oldFids);

  std::set<TFrameId> framesToInsert;
  if (setType == INSERT) {
    framesToInsert = frames;
    getFrameIdToInsert(framesToInsert, sl, m_imageSet);
    // faccio posto (se serve)
    makeSpaceForFids(sl, framesToInsert);
    assert(sl->getFrameCount() == (int)oldFids.size());
  } else if (setType == OVER_SELECTION)
    framesToInsert = frames;

  frames.clear();

  std::map<TFrameId, QString> usedImageSet;
  TPaletteP imgPlt;
  // selected frames
  std::set<TFrameId>::iterator it = framesToInsert.begin();

  // Preprocessing to keep used styles
  for (auto const &image : m_imageSet) {
    QString imageId = image.second;
    // paste destination
    TFrameId frameId;

    // merge
    if (setType == OVER_FRAMEID)  // If setType == OVER_FRAMEID ignore frames
      frameId = image.first;
    else {
      // if type == OVER_SELECTION, pasting ends at the end of selected range
      if (it == framesToInsert.end())
        continue;  // If setType == INSERT this is not possible.

      frameId = *it;
    }
    frames.insert(frameId);
    usedImageSet[frameId] = imageId;

    if (!imgPlt.getPointer()) {
      TImageP img     = TImageCache::instance()->get(imageId, false);
      if (img) imgPlt = img->getPalette();
    }

    if (setType != OVER_FRAMEID) ++it;
  }

  // Merge Palette :
  TPalette *slPlt = sl->getPalette();
  bool styleAdded = mergePalette_Overlap(slPlt, imgPlt, keepOriginalPalette);

  std::map<int, int> styleTable;
  for (int s = 0; s < slPlt->getStyleCount(); s++) styleTable[s] = s;

  // Merge Image
  for (auto const &image : usedImageSet) {
    QString imageId = image.second;
    TImageP img     = getImage(imageId, sl, styleTable, m_keepVectorFills);
    if (!cloneImages) TImageCache::instance()->remove(imageId);
    sl->setFrame(image.first, cloneImages ? img->cloneImage() : img);
  }

  // merge Hooks
  HookSet *levelHooks = sl->getHookSet();

  int hookCount = m_levelHooks.getHookCount();
  // shiftHooks(sl,usedImageSet.begin()->first,framesToInsert.size());

  auto frameIt = m_imageSet.begin();
  for (auto const &image : usedImageSet) {
    for (int i = 0; i < hookCount; i++) {
      Hook *levelHook           = levelHooks->getHook(i);
      if (!levelHook) levelHook = levelHooks->addHook();
      Hook *copiedHook          = m_levelHooks.getHook(i);
      assert(copiedHook);
      levelHook->setAPos(image.first, copiedHook->getAPos((*frameIt).first));
      levelHook->setBPos(image.first, copiedHook->getBPos((*frameIt).first));
    }
    ++frameIt;
  }

  sl->setDirtyFlag(true);

  // notify if there are any modifications to the palette
  if (styleAdded && !isRedo) {
    QString message;
    if (keepOriginalPalette)
      message = "NOTICE: Some styles were added from copied palette.";
    else
      message = "NOTICE: Some styles were added from original palette.";
    DVGui::info(message);
  }

  QApplication::restoreOverrideCursor();
  return true;
}

//-----------------------------------------------------------------------------

TImageP DrawingData::getImage(QString imageId, TXshSimpleLevel *sl,
                              const std::map<int, int> &styleTable,
                              bool keepVectorFills) const {
  TImageP img = TImageCache::instance()->get(imageId, false);
  int slType  = m_level->getType();
  if (TToonzImageP ti = img) {
    assert(slType == PLI_XSHLEVEL || slType == TZP_XSHLEVEL);
    TImageP slImg = sl->createEmptyFrame();
    slImg->setPalette(sl->getPalette());
    // raster -> raster
    if (sl->getType() == TZP_XSHLEVEL) {
      TToonzImageP slTi = slImg;
      // Immagine di appoggio per effettuare lo scramble
      TToonzImageP newImg = ti->clone();
      ToonzImageUtils::scrambleStyles(newImg, styleTable);
      TRasterCM32P slRaster  = slTi->getRaster();
      TRasterCM32P imgRaster = newImg->getRaster();
      TRop::over(slRaster, imgRaster, TTranslation(slRaster->getCenterD() -
                                                   imgRaster->getCenterD()));
      TRect savebox;
      TRop::computeBBox(slTi->getRaster(), savebox);
      slTi->setSavebox(savebox);
      img = slTi;
    }
    // raster -> vector
    else if (sl->getType() == PLI_XSHLEVEL) {
      TVectorImageP slVi = slImg;

      ToonzScene *scene = sl->getScene();
      assert(scene);

      std::unique_ptr<VectorizerConfiguration> config(
          scene->getProperties()
              ->getVectorizerParameters()
              ->getCurrentConfiguration(0.0));
      bool leaveUnpainted                           = config->m_leaveUnpainted;
      if (keepVectorFills) config->m_leaveUnpainted = false;
      vectorize(slVi, ti, styleTable, *config);
      config->m_leaveUnpainted = leaveUnpainted;
      img                      = slVi;
    }
  } else if (TVectorImageP vi = img) {
    assert(slType == PLI_XSHLEVEL || slType == TZP_XSHLEVEL);
    TImageP slImg = sl->createEmptyFrame();
    slImg->setPalette(sl->getPalette());
    // vector -> vector
    if (sl->getType() == PLI_XSHLEVEL) {
      TVectorImageP slVi = slImg;
      slVi->mergeImage(vi, TAffine(), styleTable);
      img = slVi;
    }
    // vector -> raster
    else if (sl->getType() == TZP_XSHLEVEL) {
      TToonzImageP slTi = slImg;
      rasterize(slTi, vi, styleTable);
      TRect savebox;
      TRop::computeBBox(slTi->getRaster(), savebox);
      slTi->setSavebox(savebox);
      img = slTi;
    }
  }
  return img;
}

//-----------------------------------------------------------------------------

void DrawingData::setFrames(const std::map<TFrameId, QString> &imageSet,
                            TXshSimpleLevel *level, const HookSet &levelHooks) {
  m_levelHooks = levelHooks;
  m_imageSet.clear();

  assert(!imageSet.empty());

  m_imageSet = imageSet;
  m_level    = level;
}

//-----------------------------------------------------------------------------

void DrawingData::getFrames(std::set<TFrameId> &frames) const {
  for (auto const &image : m_imageSet) frames.insert(image.first);
}

//-----------------------------------------------------------------------------

DrawingData::~DrawingData() {
  // cannot do that here! if you have cloned this class, the images in the cahce
  // are still used...
  // int i;
  // for(i=0; i<m_imageSet.size(); i++)
  //  TImageCache::instance()->remove(m_imageSet[i]);
}

//-----------------------------------------------------------------------------

void DrawingData::releaseData() {
  // do it when you're sure you no t need images anymore... (for example in an
  // undo distructor)
  // int i;
  // for(i=0; i<m_imageSet.size(); i++)
  //  TImageCache::instance()->remove(m_imageSet[i]);
}
