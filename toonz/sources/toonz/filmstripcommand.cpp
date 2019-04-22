

#include "filmstripcommand.h"
#include "tapp.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tframehandle.h"
#include "tinbetween.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "toonzqt/selection.h"
#include "toonzqt/dvdialog.h"
#include "drawingdata.h"
#include "toonzqt/strokesdata.h"
#include "toonzqt/rasterimagedata.h"
#include "timagecache.h"
#include "tools/toolutils.h"
#include "toonzqt/icongenerator.h"

#include "tundo.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzimageutils.h"
#include "toonz/trasterimageutils.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "trop.h"

#include "toonzqt/gutil.h"

#include "historytypes.h"

#include <QApplication>
#include <QClipboard>

//=============================================================================

TFrameId operator+(const TFrameId &fid, int d) {
  return TFrameId(fid.getNumber() + d, fid.getLetter());
}

//-----------------------------------------------------------------------------

void doUpdateXSheet(TXshSimpleLevel *sl, std::vector<TFrameId> oldFids,
                    std::vector<TFrameId> newFids, TXsheet *xsh,
                    std::vector<TXshChildLevel *> &childLevels) {
  for (int c = 0; c < xsh->getColumnCount(); ++c) {
    int r0, r1;
    int n = xsh->getCellRange(c, r0, r1);
    if (n > 0) {
      bool changed = false;
      std::vector<TXshCell> cells(n);
      xsh->getCells(r0, c, n, &cells[0]);
      for (int i = 0; i < n; i++) {
        TXshCell currCell = cells[i];
        // check the sub xsheets too
        if (!cells[i].isEmpty() &&
            cells[i].m_level->getType() == CHILD_XSHLEVEL) {
          TXshChildLevel *level = cells[i].m_level->getChildLevel();
          // make sure we haven't already checked the level
          if (level &&
              std::find(childLevels.begin(), childLevels.end(), level) ==
                  childLevels.end()) {
            childLevels.push_back(level);
            TXsheet *subXsh = level->getXsheet();
            doUpdateXSheet(sl, oldFids, newFids, subXsh, childLevels);
          }
        }
        for (int j = 0; j < oldFids.size(); j++) {
          if (oldFids.at(j) == newFids.at(j)) continue;
          TXshCell tempCell(sl, oldFids.at(j));
          bool sameSl  = tempCell.getSimpleLevel() == currCell.getSimpleLevel();
          bool sameFid = tempCell.getFrameId() == currCell.getFrameId();
          if (sameSl && sameFid) {
            TXshCell newCell(sl, newFids.at(j));
            cells[i] = newCell;
            changed  = true;
            break;
          }
        }
      }
      if (changed) {
        xsh->setCells(r0, c, n, &cells[0]);
        TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      }
    }
  }
}

//-----------------------------------------------------------------------------

void updateXSheet(TXshSimpleLevel *sl, std::vector<TFrameId> oldFids,
                  std::vector<TFrameId> newFids) {
  std::vector<TXshChildLevel *> childLevels;
  TXsheet *xsh =
      TApp::instance()->getCurrentScene()->getScene()->getTopXsheet();
  doUpdateXSheet(sl, oldFids, newFids, xsh, childLevels);
}

//=============================================================================
// makeSpaceForFid
// se necessario renumera gli altri fid del livello in modo che
// framesToInsert siano liberi
// n.b. modifica
//-----------------------------------------------------------------------------

void makeSpaceForFids(TXshSimpleLevel *sl,
                      const std::set<TFrameId> &framesToInsert) {
  std::vector<TFrameId> fids;
  std::vector<TFrameId> oldFids;
  sl->getFids(fids);
  sl->getFids(oldFids);
  std::set<TFrameId>::const_iterator it;
  std::set<TFrameId> touchedFids;
  for (it = framesToInsert.begin(); it != framesToInsert.end(); ++it) {
    // devo inserire fid
    TFrameId fid(*it);
    std::vector<TFrameId>::iterator j;
    // controllo che non ci sia gia'
    j = fids.begin();
    while (j = std::find(j, fids.end(), fid), j != fids.end()) {
      // c'e' gia' un fid. faccio fid -> fid + 1
      touchedFids.insert(fid);
      fid = fid + 1;
      touchedFids.insert(fid);
      *j = fid;
      // adesso devo controllare che il nuovo fid non ci sia gia' nella
      // parte restante dell'array fids
      ++j;
    }
  }
  if (!touchedFids.empty()) {
    if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled())
      updateXSheet(sl, oldFids, fids);
    sl->renumber(fids);
	sl->setDirtyFlag(true);
  }
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

void copyFramesWithoutUndo(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  QClipboard *clipboard = QApplication::clipboard();
  TXsheet *xsh          = TApp::instance()->getCurrentXsheet()->getXsheet();
  DrawingData *data     = new DrawingData();
  data->setLevelFrames(sl, frames);
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------
// frames is a selected frames in the film strip
bool pasteAreasWithoutUndo(const QMimeData *data, TXshSimpleLevel *sl,
                           std::set<TFrameId> &frames, TTileSet **tileSet,
                           std::map<TFrameId, std::set<int>> &indices) {
  // paste between the same level must keep the palette unchanged
  // Not emitting PaletteChanged signal can avoid the update of color model
  bool paletteHasChanged = true;

  if (const StrokesData *strokesData =
          dynamic_cast<const StrokesData *>(data)) {
    std::set<TFrameId>::iterator it;
    for (it = frames.begin(); it != frames.end(); ++it) {
      TImageP img = sl->getFrame(*it, true);
      if (!img) {
        img = sl->createEmptyFrame();
        sl->setFrame(*it, img);
      }
      TVectorImageP vi = img;
      TToonzImageP ti  = img;
      TRasterImageP ri = img;
      if (vi) {
        std::set<int> imageIndices;
        strokesData->getImage(vi, imageIndices, true);
        indices[*it] = imageIndices;
      } else if (ti) {
        ToonzImageData *toonzImageData = strokesData->toToonzImageData(ti);
        return pasteAreasWithoutUndo(toonzImageData, sl, frames, tileSet,
                                     indices);
      } else if (ri) {
        double dpix, dpiy;
        ri->getDpi(dpix, dpiy);
        if (dpix == 0 || dpiy == 0) {
          TPointD dpi = sl->getScene()->getCurrentCamera()->getDpi();
          dpix        = dpi.x;
          dpiy        = dpi.y;
          ri->setDpi(dpix, dpiy);
        }
        FullColorImageData *fullColorImageData =
            strokesData->toFullColorImageData(ri);
        return pasteAreasWithoutUndo(fullColorImageData, sl, frames, tileSet,
                                     indices);
      }
    }
  }
  // when pasting the copied area selected with the selection tool
  else if (const RasterImageData *rasterImageData =
               dynamic_cast<const RasterImageData *>(data)) {
    std::set<TFrameId>::iterator it;
    for (it = frames.begin(); it != frames.end(); ++it) {
      if ((sl->getType() == PLI_XSHLEVEL || sl->getType() == TZP_XSHLEVEL) &&
          dynamic_cast<const FullColorImageData *>(rasterImageData)) {
        DVGui::error(QObject::tr(
            "The copied selection cannot be pasted in the current drawing."));
        return false;
      }
      // obtain the image data to be pasted
      TImageP img = sl->getFrame(*it, true);
      if (!img) {
        img = sl->createEmptyFrame();
        sl->setFrame(*it, img);
      }
      TToonzImageP ti  = img;
      TRasterImageP ri = img;
      TVectorImageP vi = img;
      // pasting TLV
      if (ti) {
        TRasterP ras;
        double dpiX, dpiY;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes;
        std::vector<TStroke> originalStrokes;
        TAffine affine;

        // style will be merged in getData() if the palettes are different
        int styleCountBeforePasteImage = ti->getPalette()->getStyleCount();

        rasterImageData->getData(ras, dpiX, dpiY, rects, strokes,
                                 originalStrokes, affine, ti->getPalette());

        if (styleCountBeforePasteImage == ti->getPalette()->getStyleCount())
          paletteHasChanged = false;

        double imgDpiX, imgDpiY;
        ti->getDpi(imgDpiX, imgDpiY);
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;
        int i;
        TRectD boxD;
        if (rects.size() > 0) boxD   = rects[0];
        if (strokes.size() > 0) boxD = strokes[0].getBBox();
        for (i = 0; i < rects.size(); i++) boxD += rects[i];
        for (i     = 0; i < strokes.size(); i++) boxD += strokes[i].getBBox();
        boxD       = affine * boxD;
        TRect box  = ToonzImageUtils::convertWorldToRaster(boxD, ti);
        TPoint pos = box.getP00();

        if (pos.x < 0) pos.x = 0;
        if (pos.y < 0) pos.y = 0;

        if (*tileSet == 0)
          *tileSet = new TTileSetCM32(ti->getRaster()->getSize());
        if (box.overlaps(ti->getRaster()->getBounds()))
          (*tileSet)->add(ti->getRaster(), box);
        else
          (*tileSet)->add(0);
        TRasterCM32P app = ras;
        if (app) {
          TRop::over(ti->getRaster(), app, pos, affine);
          ToolUtils::updateSaveBox(sl, *it);
        }
      } else if (ri) {
        TRasterP ras;
        double dpiX = 0, dpiY = 0;
        double imgDpiX = 0, imgDpiY = 0;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes;
        std::vector<TStroke> originalStrokes;
        TAffine affine;

        TPointD cameraDpi;

        ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
        if (scene) {
          TCamera *camera = scene->getCurrentCamera();
          cameraDpi       = camera->getDpi();
          if (cameraDpi.x == 0.0 ||
              cameraDpi.y == 0.0)  // it should never happen. just in case...
            return false;
        } else
          return false;

        rasterImageData->getData(ras, dpiX, dpiY, rects, strokes,
                                 originalStrokes, affine, ri->getPalette());
        if (dpiX == 0 || dpiY == 0) {
          dpiX = cameraDpi.x;
          dpiY = cameraDpi.y;
        }

        ri->getDpi(imgDpiX, imgDpiY);
        if (imgDpiX == 0 || imgDpiY == 0) {
          imgDpiX = cameraDpi.x;
          imgDpiY = cameraDpi.y;
        }
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;
        int i;
        TRectD boxD;
        if (rects.size() > 0) boxD   = rects[0];
        if (strokes.size() > 0) boxD = strokes[0].getBBox();
        for (i = 0; i < rects.size(); i++) boxD += rects[i];
        for (i     = 0; i < strokes.size(); i++) boxD += strokes[i].getBBox();
        boxD       = affine * boxD;
        TRect box  = TRasterImageUtils::convertWorldToRaster(boxD, ri);
        TPoint pos = box.getP00();
        if (*tileSet == 0)
          *tileSet = new TTileSetFullColor(ri->getRaster()->getSize());
        if (box.overlaps(ri->getRaster()->getBounds()))
          (*tileSet)->add(ri->getRaster(), box);
        else
          (*tileSet)->add(0);
        TRasterCM32P app = ras;
        if (app)
          TRop::over(ri->getRaster(), app, ri->getPalette(), pos, affine);
        else
          TRop::over(ri->getRaster(), ras, pos, affine);
      } else if (vi) {
        StrokesData *strokesData =
            rasterImageData->toStrokesData(sl->getScene());
        return pasteAreasWithoutUndo(strokesData, sl, frames, tileSet, indices);
      }
    }
  } else
    return false;

  if (paletteHasChanged)
    TApp::instance()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();

  invalidateIcons(sl, frames);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  return true;
}

//-----------------------------------------------------------------------------
// Se insert == true incolla i frames nel livello inserendoli, se necessario
// spostando
// verso il basso i preesistenti. Altrimenti incolla i frames sostituendoli.
// il parametro clone images si mette a false quando questa funzione viene usata
// per un undo/redo.

bool pasteFramesWithoutUndo(const DrawingData *data, TXshSimpleLevel *sl,
                            std::set<TFrameId> &frames,
                            DrawingData::ImageSetType setType, bool cloneImages,
                            bool &keepOriginalPalette, bool isRedo = false) {
  if (!data || (frames.empty() && setType != DrawingData::OVER_FRAMEID))
    return false;

  bool isPaste = data->getLevelFrames(sl, frames, setType, cloneImages,
                                      keepOriginalPalette, isRedo);
  if (!isPaste) return false;

  if (keepOriginalPalette)
    invalidateIcons(sl, frames);
  else {
    std::vector<TFrameId> sl_fids;
    sl->getFids(sl_fids);
    invalidateIcons(sl, sl_fids);
  }
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->notifyPaletteChanged();

  return true;
}

//-----------------------------------------------------------------------------

// "Svuota" i frames: i frames vengono buttati e al loro posto
// vengono inseriti frames vuoti.
std::map<TFrameId, QString> clearFramesWithoutUndo(
    const TXshSimpleLevelP &sl, const std::set<TFrameId> &frames) {
  std::map<TFrameId, QString> clearedFrames;
  if (!sl || frames.empty()) return clearedFrames;

  std::set<TFrameId>::const_iterator it;
  for (it = frames.begin(); it != frames.end(); ++it) {
    TFrameId frameId = *it;
    /* UINT にキャストしたらだめだろ */
    // QString id =
    // "clearFrames"+QString::number((UINT)sl.getPointer())+"-"+QString::number(it->getNumber());
    QString id = "clearFrames" + QString::number((uintptr_t)sl.getPointer()) +
                 "-" + QString::number(it->getNumber());
    TImageCache::instance()->add(id, sl->getFrame(frameId, false));
    clearedFrames[frameId] = id;
    sl->eraseFrame(frameId);
    sl->setFrame(*it, sl->createEmptyFrame());
  }
  invalidateIcons(sl.getPointer(), frames);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  sl->setDirtyFlag(true);
  return clearedFrames;
}

//-----------------------------------------------------------------------------

// Rimuove i frames dal livello
void removeFramesWithoutUndo(const TXshSimpleLevelP &sl,
                             const std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;
  std::set<TFrameId>::const_iterator it;
  for (it = frames.begin(); it != frames.end(); ++it) sl->eraseFrame(*it);
  removeIcons(sl.getPointer(), frames);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void cutFramesWithoutUndo(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  std::map<TFrameId, QString> imageSet;

  HookSet *levelHooks   = sl->getHookSet();
  int currentFrameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  std::set<TFrameId>::const_iterator it;
  int i = 0;
  for (it = frames.begin(); it != frames.end(); ++it, i++) {
    TFrameId frameId = *it;
    // QString id =
    // "cutFrames"+QString::number((UINT)sl)+"-"+QString::number(it->getNumber());
    QString id = "cutFrames" + QString::number((uintptr_t)sl) + "-" +
                 QString::number(it->getNumber());
    TImageCache::instance()->add(id, sl->getFrame(frameId, false));
    imageSet[frameId] = id;
  }
  removeIcons(sl, frames);

  sl->setDirtyFlag(true);

  QClipboard *clipboard = QApplication::clipboard();
  DrawingData *data     = new DrawingData();
  data->setFrames(imageSet, sl, *levelHooks);
  clipboard->setMimeData(data, QClipboard::Clipboard);

  for (it = frames.begin(); it != frames.end(); ++it, i++) {
    sl->eraseFrame(*it);
  }

  std::vector<TFrameId> newFids;
  sl->getFids(newFids);
  // Devo settare i nuovi fids al frame handle prima di mandare la notifica di
  // cambiamento del livello
  TApp::instance()->getCurrentFrame()->setFrameIds(newFids);
  TApp::instance()->getCurrentFrame()->setFrameIndex(currentFrameIndex);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void insertNotEmptyframes(const TXshSimpleLevelP &sl,
                          const std::map<TFrameId, QString> &framesToInsert) {
  if (framesToInsert.empty() || !sl) return;
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  std::set<TFrameId> frames;
  for (auto const &frame : framesToInsert) {
    frames.insert(frame.first);
  }
  makeSpaceForFids(sl.getPointer(), frames);

  for (auto const &frame : framesToInsert) {
    TImageP img = TImageCache::instance()->get(frame.second, false);
    TImageCache::instance()->remove(frame.second);
    assert(img);
    sl->setFrame(frame.first, img);
  }
  invalidateIcons(sl.getPointer(), frames);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//=============================================================================
// PasteRasterAreasUndo
//-----------------------------------------------------------------------------

class PasteRasterAreasUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;
  TTileSet *m_tiles;
  RasterImageData *m_data;
  TPaletteP m_oldPalette;
  TPaletteP m_newPalette;
  bool m_isFrameInserted;

public:
  PasteRasterAreasUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                       TTileSet *tiles, RasterImageData *data, TPalette *plt,
                       bool isFrameInserted)
      : TUndo()
      , m_level(sl)
      , m_frames(frames)
      , m_tiles(tiles)
      , m_oldPalette(plt->clone())
      , m_isFrameInserted(isFrameInserted) {
    m_data = data->clone();
    assert(m_tiles->getTileCount() == m_frames.size());
  }

  ~PasteRasterAreasUndo() {
    if (m_tiles) delete m_tiles;
    if (m_data) delete m_data;
  }

  void onAdd() override { m_newPalette = m_level->getPalette()->clone(); }

  void undo() const override {
    if (!m_level || m_frames.empty()) return;
    std::set<TFrameId> frames = m_frames;

    if (m_isFrameInserted) {
      // Faccio remove dei frame incollati
      removeFramesWithoutUndo(m_level, frames);
    } else {
      std::set<TFrameId>::const_iterator it;
      int i                        = 0;
      TTileSetCM32 *tileSetCM      = dynamic_cast<TTileSetCM32 *>(m_tiles);
      TTileSetFullColor *tileSetFC = dynamic_cast<TTileSetFullColor *>(m_tiles);
      for (it = m_frames.begin(); it != m_frames.end(); it++, i++) {
        TImageP image = m_level->getFrame(*it, true);
        if (!image) continue;
        TRasterImageP ri(image);
        TToonzImageP ti(image);
        if (tileSetCM) {
          const TTileSetCM32::Tile *tile = tileSetCM->getTile(i);
          if (!tile) continue;
          TRasterCM32P tileRas;
          tile->getRaster(tileRas);
          assert(ti);
          ti->getRaster()->copy(tileRas, tile->m_rasterBounds.getP00());
          ToolUtils::updateSaveBox(m_level, *it);
        } else if (tileSetFC) {
          const TTileSetFullColor::Tile *tile = tileSetFC->getTile(i);
          if (!tile) continue;
          TRasterP tileRas;
          tile->getRaster(tileRas);
          assert(ri);
          ri->getRaster()->copy(tileRas, tile->m_rasterBounds.getP00());
        }
      }
    }
    // Setto la vecchia paletta al livello
    if (m_oldPalette.getPointer()) {
      m_level->getPalette()->assign(m_oldPalette->clone());
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->notifyPaletteChanged();
    }

    invalidateIcons(m_level.getPointer(), m_frames);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const override {
    if (!m_level || m_frames.empty()) return;
    if (m_isFrameInserted) {
      assert(m_frames.size() == 1);
      TImageP img = m_level->createEmptyFrame();
      m_level->setFrame(*m_frames.begin(), img);
    }

    std::set<TFrameId> frames = m_frames;

    std::set<TFrameId>::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); it++) {
      TImageP image    = m_level->getFrame(*it, true);
      TRasterImageP ri = image;
      TToonzImageP ti  = image;
      if (ti) {
        TRasterP ras;
        double dpiX, dpiY;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes;
        std::vector<TStroke> originalStrokes;
        TAffine affine;
        m_data->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes,
                        affine, ti->getPalette());
        double imgDpiX, imgDpiY;
        ti->getDpi(imgDpiX, imgDpiY);
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;

        int i;
        TRectD boxD;
        if (rects.size() > 0) boxD   = rects[0];
        if (strokes.size() > 0) boxD = strokes[0].getBBox();
        for (i = 0; i < rects.size(); i++) boxD += rects[i];
        for (i     = 0; i < strokes.size(); i++) boxD += strokes[i].getBBox();
        boxD       = affine * boxD;
        TRect box  = ToonzImageUtils::convertWorldToRaster(boxD, ti);
        TPoint pos = box.getP00();
        TRasterCM32P app = ras;
        TRop::over(ti->getRaster(), app, pos, affine);
        ToolUtils::updateSaveBox(m_level, *it);
      } else if (ri) {
        TRasterP ras;
        double dpiX, dpiY;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes;
        std::vector<TStroke> originalStrokes;
        TAffine affine;
        m_data->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes,
                        affine, ri->getPalette());
        double imgDpiX, imgDpiY;
        if (dpiX == 0 && dpiY == 0) {
          ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
          if (scene) {
            TCamera *camera = scene->getCurrentCamera();
            TPointD dpi = dpi = camera->getDpi();
            dpiX              = dpi.x;
            dpiY              = dpi.y;
          } else
            return;
        }
        ri->getDpi(imgDpiX, imgDpiY);
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;
        int i;
        TRectD boxD;
        if (rects.size() > 0) boxD   = rects[0];
        if (strokes.size() > 0) boxD = strokes[0].getBBox();
        for (i = 0; i < rects.size(); i++) boxD += rects[i];
        for (i     = 0; i < strokes.size(); i++) boxD += strokes[i].getBBox();
        boxD       = affine * boxD;
        TRect box  = TRasterImageUtils::convertWorldToRaster(boxD, ri);
        TPoint pos = box.getP00();
        TRasterCM32P app = ras;
        if (app)
          TRop::over(ri->getRaster(), app, ri->getPalette(), pos, affine);
        else
          TRop::over(ri->getRaster(), ras, pos, affine);
      }
    }

    if (m_newPalette.getPointer()) {
      m_level->getPalette()->assign(m_newPalette->clone());
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->notifyPaletteChanged();
    }

    invalidateIcons(m_level.getPointer(), m_frames);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  int getSize() const override {
    return sizeof(*this) + sizeof(*m_data) + sizeof(*m_tiles);
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Paste  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_level->getName()));

    std::set<TFrameId>::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); it++) {
      if (it != m_frames.begin()) str += QString(", ");
      str += QString::number((*it).getNumber());
    }

    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

//=============================================================================
// PasteVectorAreasUndo
//-----------------------------------------------------------------------------

class PasteVectorAreasUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;
  std::map<TFrameId, std::set<int>> m_indices;
  StrokesData *m_data;
  TPaletteP m_oldPalette;
  TPaletteP m_newPalette;
  bool m_isFrameInserted;

public:
  PasteVectorAreasUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                       std::map<TFrameId, std::set<int>> &indices,
                       StrokesData *data, TPalette *plt, bool isFrameInserted)
      : TUndo()
      , m_level(sl)
      , m_frames(frames)
      , m_indices(indices)
      , m_oldPalette(plt->clone())
      , m_isFrameInserted(isFrameInserted) {
    m_data = data->clone();
  }

  ~PasteVectorAreasUndo() {
    if (m_data) delete m_data;
  }

  void onAdd() override { m_newPalette = m_level->getPalette()->clone(); }

  void undo() const override {
    if (!m_level || m_frames.empty()) return;

    std::set<TFrameId> frames = m_frames;

    if (m_isFrameInserted) {
      // Faccio remove dei frame incollati
      removeFramesWithoutUndo(m_level, frames);
    } else {
      std::set<TFrameId>::const_iterator it;
      for (it = m_frames.begin(); it != m_frames.end(); it++) {
        TVectorImageP img = m_level->getFrame(*it, true);
        assert(img);
        if (!img) continue;
        std::map<TFrameId, std::set<int>>::const_iterator mapIt =
            m_indices.find(*it);
        if (mapIt == m_indices.end()) continue;
        std::set<int> imageIndices = mapIt->second;
        ;
        std::set<int>::const_iterator it2 = imageIndices.end();
        while (it2 != imageIndices.begin()) {
          it2--;
          img->removeStroke(*it2);
        }
      }
    }

    // Setto la vecchia paletta al livello
    if (m_oldPalette.getPointer()) {
      m_level->getPalette()->assign(m_oldPalette->clone());
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->notifyPaletteChanged();
    }

    invalidateIcons(m_level.getPointer(), m_frames);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const override {
    if (!m_level || m_frames.empty()) return;

    if (m_isFrameInserted) {
      assert(m_frames.size() == 1);
      TVectorImageP img = m_level->createEmptyFrame();
      m_level->setFrame(*m_frames.begin(), img);
    }

    std::set<TFrameId> frames = m_frames;
    std::set<TFrameId>::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); it++) {
      TVectorImageP img = m_level->getFrame(*it, true);
      assert(img);
      if (!img) continue;
      std::set<int> app;
      m_data->getImage(img, app, true);
    }

    if (m_newPalette.getPointer()) {
      m_level->getPalette()->assign(m_newPalette->clone());
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->notifyPaletteChanged();
    }

    invalidateIcons(m_level.getPointer(), m_frames);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  int getSize() const override { return sizeof(*this) + sizeof(*m_data); }

  QString getHistoryString() override {
    QString str = QObject::tr("Paste  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_level->getName()));

    std::set<TFrameId>::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); it++) {
      if (it != m_frames.begin()) str += QString(", ");
      str += QString::number((*it).getNumber());
    }

    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

/*//=============================================================================
// PasteAreasUndo
//-----------------------------------------------------------------------------

class PasteAreasUndo final : public TUndo
{
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;
        TPaletteP m_oldPalette;
        TPaletteP m_newPalette;
        bool m_isFrameInserted;

public:
  PasteAreasUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames, bool
isFrameInserted)
      : TUndo()
      , m_level(sl)
                        , m_frames(frames)
                        , m_isFrameInserted(isFrameInserted)
        {
                m_oldPalette = m_level->getPalette()->clone();
                if(!m_isFrameInserted)
                {
                        std::set<TFrameId>::iterator it;
                        for(it=m_frames.begin(); it!=m_frames.end(); it++)
                        {
                                TImageP img = m_level->getFrame(*it,true);
                                TImageCache::instance()->add("PasteAreasUndoOld"+QString::number((UINT)this)+QString::number((*it).getNumber()),
img->cloneImage());
                        }
                }
  }

        void onAdd()
        {
                m_newPalette = m_level->getPalette()->clone();
                std::set<TFrameId>::iterator it;
                for(it=m_frames.begin(); it!=m_frames.end(); it++)
                {
                        TImageP img = m_level->getFrame(*it,true);
                        TImageCache::instance()->add("PasteAreasUndoNew"+QString::number((UINT)this)+QString::number((*it).getNumber()),
img->cloneImage());
                }
        }

  ~PasteAreasUndo() {
                std::set<TFrameId>::const_iterator it;

                if(!m_isFrameInserted)
                {
                        for(it=m_frames.begin(); it!=m_frames.end(); it++)
                                TImageCache::instance()->remove("PasteAreasUndoOld"+QString::number((UINT)this)+QString::number((*it).getNumber()));
                }
                for(it=m_frames.begin(); it!=m_frames.end(); it++)
                        TImageCache::instance()->remove("PasteAreasUndoNew"+QString::number((UINT)this)+QString::number((*it).getNumber()));
  }

        void undo() const
        {
                std::set<TFrameId> frames = m_frames;

                if(m_isFrameInserted)
                {
                        // Faccio remove dei frame incollati
                        removeFramesWithoutUndo(m_level, frames);
                }
                else
                {
                        std::set<TFrameId>::const_iterator it;
                        for(it=m_frames.begin(); it!=m_frames.end(); it++)
                        {
                                TImageP img =
TImageCache::instance()->get("PasteAreasUndoOld"+QString::number((UINT)this)+QString::number((*it).getNumber()),true);
                                assert(img);
                                m_level->setFrame(*it,img);
                        }
                }

                //Setto la vecchia paletta al livello
                if(m_oldPalette.getPointer())
                {
                        m_level->getPalette()->assign(m_oldPalette->clone());
      TApp::instance()->getPaletteController()->getCurrentLevelPalette()->notifyPaletteChanged();
                }

    invalidateIcons(m_level.getPointer(), m_frames);
                TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const
        {
                if(!m_level || m_frames.empty()) return;

    std::set<TFrameId> frames = m_frames;

                std::set<TFrameId>::const_iterator it;
                for(it=m_frames.begin(); it!=m_frames.end(); it++)
                {
                        TImageP img =
TImageCache::instance()->get("PasteAreasUndoNew"+QString::number((UINT)this)+QString::number((*it).getNumber()),true);
                        if(!img) continue;
                        m_level->setFrame(*it, img, true);
                }

                if(m_newPalette.getPointer())
                {
      m_level->getPalette()->assign(m_newPalette->clone());
      TApp::instance()->getPaletteController()->getCurrentLevelPalette()->notifyPaletteChanged();
                }

                invalidateIcons(m_level.getPointer(), m_frames);
                TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  int getSize() const{
    return sizeof(*this);
  }
};*/

//=============================================================================
// PasteFramesUndo
//-----------------------------------------------------------------------------

class PasteFramesUndo final : public TUndo {
  TXshSimpleLevelP m_sl;
  std::set<TFrameId> m_frames;
  std::vector<TFrameId> m_oldLevelFrameId;
  TPaletteP m_oldPalette;
  DrawingData *m_oldData;
  DrawingData *m_newData;
  DrawingData::ImageSetType m_setType;
  HookSet *m_oldLevelHooks;
  bool m_updateXSheet;
  bool m_keepOriginalPalette;

public:
  PasteFramesUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                  const std::vector<TFrameId> &oldLevelFrameId,
                  TPaletteP oldPalette, DrawingData::ImageSetType setType,
                  HookSet *oldLevelHooks, bool keepOriginalPalette,
                  DrawingData *oldData = 0)
      : m_sl(sl)
      , m_frames(frames)
      , m_oldLevelFrameId(oldLevelFrameId)
      , m_oldPalette(oldPalette)
      , m_setType(setType)
      , m_keepOriginalPalette(keepOriginalPalette)
      , m_oldData(oldData)
      , m_oldLevelHooks(oldLevelHooks) {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *data       = cloneData(clipboard->mimeData());
    m_newData             = dynamic_cast<DrawingData *>(data);
    assert(m_newData);
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  ~PasteFramesUndo() {
    if (m_oldData) m_oldData->releaseData();
    if (m_newData) m_newData->releaseData();
  }

  void undo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    std::set<TFrameId> frames = m_frames;

    // Faccio remove dei frame incollati
    if (m_setType != DrawingData::OVER_SELECTION)
      removeFramesWithoutUndo(m_sl, frames);

    // Renumero i frame con i vecchi fids
    if (m_setType == DrawingData::INSERT) {
      assert(m_sl->getFrameCount() == m_oldLevelFrameId.size());
      if (m_updateXSheet) {
        std::vector<TFrameId> newFrames;
        m_sl->getFids(newFrames);
        updateXSheet(m_sl.getPointer(), newFrames, m_oldLevelFrameId);
      }
      m_sl->renumber(m_oldLevelFrameId);
	  m_sl->setDirtyFlag(true);
      TApp::instance()
          ->getPaletteController()
          ->getCurrentLevelPalette()
          ->notifyPaletteChanged();
      TApp::instance()->getCurrentLevel()->notifyLevelChange();
    }

    // Reinserisco i vecchi frame che ho sovrascritto
    if (m_setType != DrawingData::INSERT) {
      std::set<TFrameId> framesToModify;
      m_oldData->getFrames(framesToModify);

      bool dummy = true;
      // Incollo i frames sovrascrivendoli
      pasteFramesWithoutUndo(m_oldData, m_sl.getPointer(), framesToModify,
                             DrawingData::OVER_SELECTION, true, dummy);
    }

    // Setto la vecchia paletta al livello
    if (m_oldPalette.getPointer()) {
      TPalette *levelPalette = m_sl->getPalette();
      if (levelPalette) levelPalette->assign(m_oldPalette.getPointer());

      TApp *app = TApp::instance();
      if (app->getCurrentLevel()->getLevel() == m_sl.getPointer())
        app->getPaletteController()
            ->getCurrentLevelPalette()
            ->notifyPaletteChanged();
    }

    // update all icons
    std::vector<TFrameId> sl_fids;
    m_sl.getPointer()->getFids(sl_fids);
    invalidateIcons(m_sl.getPointer(), sl_fids);

    *m_sl->getHookSet() = *m_oldLevelHooks;
  }

  void redo() const override {
    if (!m_sl || m_frames.empty()) return;
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    std::set<TFrameId> frames = m_frames;

    bool keepOriginalPalette = m_keepOriginalPalette;
    pasteFramesWithoutUndo(m_newData, m_sl.getPointer(), frames, m_setType,
                           true, keepOriginalPalette, true);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Paste  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));

    std::set<TFrameId>::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); it++) {
      if (it != m_frames.begin()) str += QString(", ");
      str += QString::number((*it).getNumber());
    }

    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

//=============================================================================
// DeleteFramesUndo
//-----------------------------------------------------------------------------

class DeleteFramesUndo final : public TUndo {
  TXshSimpleLevel *m_sl;
  std::set<TFrameId> m_frames;
  DrawingData *m_oldData;
  DrawingData *m_newData;

public:
  DeleteFramesUndo(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                   DrawingData *oldData, DrawingData *newData)
      : m_sl(sl), m_frames(frames), m_oldData(oldData), m_newData(newData) {}

  ~DeleteFramesUndo() {
    if (m_oldData) m_oldData->releaseData();
    if (m_newData) m_newData->releaseData();
  }

  void pasteFramesFromData(const DrawingData *data) const {
    std::set<TFrameId> frames = m_frames;

    bool dummy = true;
    // Incollo i frames sovrascrivendoli
    pasteFramesWithoutUndo(data, m_sl, frames, DrawingData::OVER_SELECTION,
                           true, dummy);
  }

  void undo() const override { pasteFramesFromData(m_oldData); }

  // OSS.: Non posso usare il metodo "clearFramesWithoutUndo(...)" perche'
  //  genera un NUOVO frame vuoto, perdendo quello precedente e le eventuali
  //  modifiche che ad esso possono essere state fatte successivamente.
  void redo() const override { pasteFramesFromData(m_newData); }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Delete Frames  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));

    std::set<TFrameId>::const_iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); it++) {
      if (it != m_frames.begin()) str += QString(", ");
      str += QString::number((*it).getNumber());
    }

    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

//=============================================================================

//-----------------------------------------------------------------------------

class CutFramesUndo final : public TUndo {
  TXshSimpleLevel *m_sl;
  std::set<TFrameId> m_framesCutted;
  std::vector<TFrameId> m_oldFrames;
  DrawingData *m_newData;

public:
  CutFramesUndo(TXshSimpleLevel *sl, std::set<TFrameId> &framesCutted,
                std::vector<TFrameId> &oldFrames)
      : m_sl(sl), m_framesCutted(framesCutted), m_oldFrames(oldFrames) {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *data       = cloneData(clipboard->mimeData());
    m_newData             = dynamic_cast<DrawingData *>(data);
    assert(m_newData);
  }

  ~CutFramesUndo() {
    if (m_newData) m_newData->releaseData();
  }

  void undo() const override {
    std::set<TFrameId> frames = m_framesCutted;
    bool dummy                = true;
    pasteFramesWithoutUndo(m_newData, m_sl, frames, DrawingData::OVER_SELECTION,
                           true, dummy);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    // Prendo il clipboard corrente.
    QClipboard *clipboard  = QApplication::clipboard();
    QMimeData *currentData = cloneData(clipboard->mimeData());

    std::set<TFrameId> frames = m_framesCutted;
    cutFramesWithoutUndo(m_sl, frames);

    // Setto il clipboard corrente
    clipboard->setMimeData(currentData, QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Cut Frames  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));

    std::set<TFrameId>::const_iterator it;
    for (it = m_framesCutted.begin(); it != m_framesCutted.end(); it++) {
      if (it != m_framesCutted.begin()) str += QString(", ");
      str += QString::number((*it).getNumber());
    }

    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================

//-----------------------------------------------------------------------------

namespace {
//-----------------------------------------------------------------------------

class AddFramesUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_insertedFids;
  std::vector<TFrameId> m_oldFids;
  bool m_updateXSheet;

public:
  AddFramesUndo(const TXshSimpleLevelP &level,
                const std::set<TFrameId> insertedFids,
                std::vector<TFrameId> oldFids)
      : m_level(level), m_insertedFids(insertedFids), m_oldFids(oldFids) {
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  void undo() const override {
    removeFramesWithoutUndo(m_level, m_insertedFids);
    if (m_updateXSheet) {
      std::vector<TFrameId> newFrames;
      m_level->getFids(newFrames);
      updateXSheet(m_level.getPointer(), newFrames, m_oldFids);
    }
    m_level->renumber(m_oldFids);
    m_level->setDirtyFlag(true);

    TApp *app = TApp::instance();
    app->getCurrentScene()->setDirtyFlag(true);
    app->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const override {
    makeSpaceForFids(m_level.getPointer(), m_insertedFids);

    for (auto const &fid : m_insertedFids) {
      m_level->setFrame(fid, m_level->createEmptyFrame());
      IconGenerator::instance()->invalidate(m_level.getPointer(), fid);
    }
    m_level->setDirtyFlag(true);

    TApp *app = TApp::instance();
    app->getCurrentScene()->setDirtyFlag(true);
    app->getCurrentLevel()->notifyLevelChange();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Add Frames  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_level->getName()));

    std::set<TFrameId>::const_iterator it;
    for (it = m_insertedFids.begin(); it != m_insertedFids.end(); it++) {
      if (it != m_insertedFids.begin()) str += QString(", ");
      str += QString::number((*it).getNumber());
    }

    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// AddFrames
//-----------------------------------------------------------------------------

void FilmstripCmd::addFrames(TXshSimpleLevel *sl, int start, int end,
                             int step) {
  if (start < 1 || step < 1 || start > end || !sl || sl->isSubsequence() ||
      sl->isReadOnly())
    return;

  std::vector<TFrameId> oldFids;
  sl->getFids(oldFids);

  std::set<TFrameId> fidsToInsert;
  int frame = 0;
  for (frame = start; frame <= end; frame += step)
    fidsToInsert.insert(TFrameId(frame));

  makeSpaceForFids(sl, fidsToInsert);

  for (auto const &fid : fidsToInsert) {
    sl->setFrame(fid, sl->createEmptyFrame());
    IconGenerator::instance()->invalidate(sl, fid);
  }
  sl->setDirtyFlag(true);

  AddFramesUndo *undo = new AddFramesUndo(sl, fidsToInsert, oldFids);
  TUndoManager::manager()->add(undo);

  TApp *app = TApp::instance();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentLevel()->notifyLevelChange();
}

//=============================================================================
// RenumberUndo
//-----------------------------------------------------------------------------

namespace {

class RenumberUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::vector<TFrameId> m_fids;
  std::map<TFrameId, TFrameId> m_mapOldFrameId;
  bool m_updateXSheet = false;

public:
  RenumberUndo(const TXshSimpleLevelP &level, const std::vector<TFrameId> &fids,
               bool forceCallUpdateXSheet = false)
      : m_level(level), m_fids(fids) {
    assert(m_level);
    std::vector<TFrameId> oldFids;
    m_level->getFids(oldFids);
    assert(oldFids.size() == m_fids.size());
    int i;
    for (i = 0; i < m_fids.size(); i++) {
      if (m_fids[i] != oldFids[i]) m_mapOldFrameId[m_fids[i]] = oldFids[i];
    }
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled() ||
        forceCallUpdateXSheet;
  }
  void renumber(std::vector<TFrameId> fids) const {
    if (m_updateXSheet) {
      std::vector<TFrameId> oldFrames;
      m_level->getFids(oldFrames);
      updateXSheet(m_level.getPointer(), oldFrames, fids);
    }
    m_level->renumber(fids);
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
	m_level->setDirtyFlag(true);
	TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }
  void undo() const override {
    std::vector<TFrameId> fids;
    m_level->getFids(fids);
    assert(fids.size() == m_fids.size());
    int i;
    for (i = 0; i < fids.size(); i++) {
      if (m_mapOldFrameId.count(fids[i]) > 0) {
        std::map<TFrameId, TFrameId>::const_iterator it;
        it = m_mapOldFrameId.find(fids[i]);
        assert(it != m_mapOldFrameId.end());
        if (it != m_mapOldFrameId.end()) fids[i] = TFrameId(it->second);
      }
    }
    renumber(fids);
  }
  void redo() const override { renumber(m_fids); }
  int getSize() const override {
    return sizeof(*this) + sizeof(TFrameId) * m_fids.size();
  }

  QString getHistoryString() override {
    return QObject::tr("Renumber  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// Renumber
//-----------------------------------------------------------------------------

void FilmstripCmd::renumber(
    TXshSimpleLevel *sl,
    const std::vector<std::pair<TFrameId, TFrameId>> &table,
    bool forceCallUpdateXSheet) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  if (table.empty()) return;

  // table:src->dst; check that src is a fid of the level
  std::vector<std::pair<TFrameId, TFrameId>>::const_iterator it;
  for (it = table.begin(); it != table.end(); ++it) {
    TFrameId srcFid = it->first;
    if (!sl->isFid(srcFid)) {
      // todo: error messages
      return;
    }
  }

  // tmp contains all the level fids that are not affected by the renumbering
  std::vector<TFrameId> fids, rfids, oldFrames;
  sl->getFids(fids);
  sl->getFids(oldFrames);
  std::set<TFrameId> tmp;
  for (int i = 0; i < (int)fids.size(); i++) tmp.insert(fids[i]);
  for (it = table.begin(); it != table.end(); ++it) tmp.erase(it->first);

  // fids contain the new numbering of all the level drawings
  // (note: fids can be not ordered)
  for (int i = 0; i < (int)fids.size(); i++) {
    TFrameId srcFid = fids[i];
    for (it = table.begin(); it != table.end() && it->first != srcFid; ++it) {
    }
    if (it != table.end()) {
      // srcFid is affected by the renumbering
      TFrameId tarFid = it->second;
      // make sure that srcFid has not been used. add a letter if this is needed
      if (tmp.count(tarFid) > 0) {
        do {
          char letter = tarFid.getLetter();
          tarFid = TFrameId(tarFid.getNumber(), letter == 0 ? 'a' : letter + 1);
        } while (tarFid.getLetter() <= 'z' && tmp.count(tarFid) > 0);
        if (tarFid.getLetter() > 'z') {
          // todo: error message
          return;
        }
      }
      tmp.insert(tarFid);
      fids[i] = tarFid;
    }
  }

  TUndoManager::manager()->add(
      new RenumberUndo(sl, fids, forceCallUpdateXSheet));
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled() ||
      forceCallUpdateXSheet) {
    updateXSheet(sl, oldFrames, fids);
  }
  sl->renumber(fids);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  /*
int i;
std::set<TFrameId>::iterator it2;
it2=frames.begin();
std::set<TFrameId> newFrames;
for (i=0; i<frames.size(); i++, it2++)
newFrames.insert(TFrameId(startFrame+(i*stepFrame),it2->getLetter()));
assert(frames.size()==newFrames.size());
frames.swap(newFrames);
*/
}

//-----------------------------------------------------------------------------

void FilmstripCmd::renumber(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                            int startFrame, int stepFrame) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  assert(startFrame > 0 && stepFrame > 0);
  if (startFrame <= 0 || stepFrame <= 0 || frames.empty()) return;

  std::vector<TFrameId> fids;
  std::vector<TFrameId> oldFrames;
  sl->getFids(oldFrames);
  sl->getFids(fids);

  std::set<TFrameId> modifiedFids;

  // tmp contiene i frames del livello, meno quelli da renumerare
  std::set<TFrameId> tmp(fids.begin(), fids.end());
  std::set<TFrameId>::const_iterator it;
  for (it = frames.begin(); it != frames.end(); ++it) tmp.erase(*it);

  int frame                         = startFrame;
  std::vector<TFrameId>::iterator j = fids.begin();
  for (it = frames.begin(); it != frames.end(); ++it) {
    TFrameId srcFid(*it);
    TFrameId dstFid(frame);
    frame += stepFrame;
    // faccio il controllo su tmp e non su fids. considera:
    // fids = [1,2,3,4], renumber = [2->3,3->5]
    if (tmp.count(dstFid) > 0) {
      DVGui::error(("can't renumber: frame conflict"));
      return;
    }
    j = std::find(j, fids.end(), srcFid);
    // i frames selezionati fanno parte del livello sl. Quindi:
    assert(j != fids.end());
    assert(srcFid == *j);
    if (j == fids.end()) continue;  // per sicurezza
    int k = std::distance(fids.begin(), j);
    if (srcFid != dstFid) {
      modifiedFids.insert(srcFid);
      modifiedFids.insert(dstFid);
      *j = dstFid;
      ++j;
    }
  }
  TUndoManager::manager()->add(new RenumberUndo(sl, fids));
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl, oldFrames, fids);
  }
  sl->renumber(fids);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  int i;
  std::set<TFrameId>::iterator it2;
  it2 = frames.begin();
  std::set<TFrameId> newFrames;
  for (i = 0; i < frames.size(); i++, it2++)
    newFrames.insert(TFrameId(startFrame + (i * stepFrame), it2->getLetter()));
  assert(frames.size() == newFrames.size());
  frames.swap(newFrames);

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//=============================================================================
// copy
//-----------------------------------------------------------------------------

void FilmstripCmd::copy(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;
  copyFramesWithoutUndo(sl, frames);
}

//=============================================================================
// paste
//-----------------------------------------------------------------------------

void FilmstripCmd::paste(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || sl->isReadOnly() || frames.empty()) return;

  std::vector<TFrameId> oldLevelFrameId;
  sl->getFids(oldLevelFrameId);

  TPaletteP oldPalette;
  if (TPalette *pal = sl->getPalette()) oldPalette = pal->clone();

  QClipboard *clipboard = QApplication::clipboard();
  QMimeData *data       = cloneData(clipboard->mimeData());
  // when pasting the filmstrip frames
  DrawingData *drawingData = dynamic_cast<DrawingData *>(data);
  if (drawingData) {
    if (sl->isSubsequence()) return;

    // keep the choosed option of "Keep Original Palette" and reproduce it in
    // undo
    bool keepOriginalPalette;

    HookSet *oldLevelHooks = new HookSet();
    *oldLevelHooks         = *sl->getHookSet();

    bool isPaste =
        pasteFramesWithoutUndo(drawingData, sl, frames, DrawingData::INSERT,
                               true, keepOriginalPalette);
    if (!isPaste) return;
    TUndoManager::manager()->add(new PasteFramesUndo(
        sl, frames, oldLevelFrameId, oldPalette, DrawingData::INSERT,
        oldLevelHooks, keepOriginalPalette));
  }
  // when pasting the copied part of the image which is selected with the
  // selection tool
  else {
    bool isFrameToInsert =
        (frames.size() == 1) ? !sl->isFid((*frames.begin())) : false;
    TTileSet *tileSet = 0;
    std::map<TFrameId, std::set<int>> indices;
    TUndo *undo   = 0;
    TPaletteP plt = sl->getPalette()->clone();
    bool isPaste  = pasteAreasWithoutUndo(data, sl, frames, &tileSet, indices);
    RasterImageData *rasterImageData = dynamic_cast<RasterImageData *>(data);
    StrokesData *strokesData         = dynamic_cast<StrokesData *>(data);
    if (rasterImageData && tileSet)
      undo = new PasteRasterAreasUndo(sl, frames, tileSet, rasterImageData,
                                      plt.getPointer(), isFrameToInsert);
    if (strokesData && tileSet) {
      TImageP img      = sl->getFrame(*frames.begin(), false);
      TRasterImageP ri = img;
      TToonzImageP ti  = img;
      assert(img);
      if (ti)
        undo = new PasteRasterAreasUndo(sl, frames, tileSet,
                                        strokesData->toToonzImageData(ti),
                                        plt.getPointer(), isFrameToInsert);
      else if (ri) {
        double dpix, dpiy;
        ri->getDpi(dpix, dpiy);
        if (dpix == 0 || dpiy == 0) {
          TPointD dpi = sl->getScene()->getCurrentCamera()->getDpi();
          dpix        = dpi.x;
          dpiy        = dpi.y;
          ri->setDpi(dpix, dpiy);
        }
        undo = new PasteRasterAreasUndo(sl, frames, tileSet,
                                        strokesData->toFullColorImageData(ri),
                                        plt.getPointer(), isFrameToInsert);
      }
    }
    if (strokesData && !indices.empty())
      undo = new PasteVectorAreasUndo(sl, frames, indices, strokesData,
                                      plt.getPointer(), isFrameToInsert);
    if (rasterImageData && !indices.empty())
      undo = new PasteVectorAreasUndo(
          sl, frames, indices, rasterImageData->toStrokesData(sl->getScene()),
          plt.getPointer(), isFrameToInsert);
    if (!isPaste) return;
    if (undo) TUndoManager::manager()->add(undo);
  }
}

//=============================================================================
// merge
//-----------------------------------------------------------------------------

void FilmstripCmd::merge(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || sl->isReadOnly() || sl->isSubsequence()) return;

  std::vector<TFrameId> oldLevelFrameId;
  sl->getFids(oldLevelFrameId);
  TPaletteP oldPalette = sl->getPalette()->clone();
  std::set<TFrameId> frameIdToChange;

  QClipboard *clipboard = QApplication::clipboard();
  if (const DrawingData *drawingData =
          dynamic_cast<const DrawingData *>(clipboard->mimeData())) {
    drawingData->getFrames(frameIdToChange);
    DrawingData *data = new DrawingData();
    data->setLevelFrames(sl, frameIdToChange);
    HookSet *oldLevelHooks = new HookSet();
    *oldLevelHooks         = *sl->getHookSet();

    bool keepOriginalPalette = true;

    bool isPaste = pasteFramesWithoutUndo(drawingData, sl, frames,
                                          DrawingData::OVER_FRAMEID, true,
                                          keepOriginalPalette);
    if (!isPaste) return;
    TUndoManager::manager()->add(new PasteFramesUndo(
        sl, frames, oldLevelFrameId, oldPalette, DrawingData::OVER_FRAMEID,
        oldLevelHooks, keepOriginalPalette, data));
  }
}

//=============================================================================
// pasteInto
//-----------------------------------------------------------------------------

void FilmstripCmd::pasteInto(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || sl->isReadOnly() || sl->isSubsequence()) return;

  std::vector<TFrameId> oldLevelFrameId;
  sl->getFids(oldLevelFrameId);

  TPaletteP oldPalette;
  if (TPalette *pal = sl->getPalette()) oldPalette = pal->clone();

  QClipboard *clipboard = QApplication::clipboard();
  if (const DrawingData *drawingData =
          dynamic_cast<const DrawingData *>(clipboard->mimeData())) {
    DrawingData *data = new DrawingData();
    data->setLevelFrames(sl, frames);

    HookSet *oldLevelHooks = new HookSet();
    *oldLevelHooks         = *sl->getHookSet();

    bool keepOriginalPalette = true;
    bool isPaste             = pasteFramesWithoutUndo(drawingData, sl, frames,
                                          DrawingData::OVER_SELECTION, true,
                                          keepOriginalPalette);
    if (!isPaste) return;

    TUndoManager::manager()->add(new PasteFramesUndo(
        sl, frames, oldLevelFrameId, oldPalette, DrawingData::OVER_SELECTION,
        oldLevelHooks, keepOriginalPalette, data));
  }
}

//=============================================================================
// cut
//-----------------------------------------------------------------------------

void FilmstripCmd::cut(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || frames.empty() || sl->isSubsequence() || sl->isReadOnly()) return;

  std::set<TFrameId> framesToCut = frames;
  std::vector<TFrameId> oldFrames;
  sl->getFids(oldFrames);
  cutFramesWithoutUndo(sl, frames);
  TUndoManager::manager()->add(new CutFramesUndo(sl, framesToCut, oldFrames));
}

//=============================================================================
// clear
//-----------------------------------------------------------------------------

void FilmstripCmd::clear(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;

  if (sl->isReadOnly()) {
    std::set<TFrameId> editableFrames = sl->getEditableRange();
    if (editableFrames.empty()) return;

    // Browser all the frames and return if some frames are not editable
    std::set<TFrameId>::const_iterator it;
    for (it = frames.begin(); it != frames.end(); ++it) {
      TFrameId frameId = *it;
      if (editableFrames.count(frameId) == 0) return;
    }
  }

  HookSet *levelHooks          = sl->getHookSet();
  std::set<TFrameId> oldFrames = frames;
  std::map<TFrameId, QString> clearedFrames =
      clearFramesWithoutUndo(sl, frames);
  DrawingData *oldData = new DrawingData();
  oldData->setFrames(clearedFrames, sl, *levelHooks);
  DrawingData *newData = new DrawingData();
  newData->setLevelFrames(sl, frames);
  frames.clear();
  TUndoManager::manager()->add(
      new DeleteFramesUndo(sl, oldFrames, oldData, newData));
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void insertEmptyFilmstripFrames(const TXshSimpleLevelP &sl,
                                const std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;
  makeSpaceForFids(sl.getPointer(), frames);
  std::set<TFrameId>::const_iterator it;
  for (it = frames.begin(); it != frames.end(); ++it)
    sl->setFrame(*it, sl->createEmptyFrame());
  invalidateIcons(sl.getPointer(), frames);
  sl->setDirtyFlag(true);
  //  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

class UndoInsertEmptyFrames final : public TUndo {
  TXshSimpleLevelP m_level;
  std::vector<TFrameId> m_oldFrames;
  std::set<TFrameId> m_frames;
  bool m_updateXSheet;

public:
  UndoInsertEmptyFrames(const TXshSimpleLevelP &level,
                        std::set<TFrameId> frames,
                        std::vector<TFrameId> oldFrames)
      : m_level(level), m_frames(frames), m_oldFrames(oldFrames) {
    if (m_level->getType() == TZP_XSHLEVEL) {
      std::set<TFrameId>::iterator it;
      for (it = m_frames.begin(); it != m_frames.end(); it++) {
        TToonzImageP img = m_level->getFrame(*it, true);
        // TImageCache::instance()->add("UndoInsertEmptyFrames"+QString::number((UINT)this),
        // img);
        TImageCache::instance()->add(
            "UndoInsertEmptyFrames" + QString::number((uintptr_t) this), img);
      }
    }
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  ~UndoInsertEmptyFrames() {
    // TImageCache::instance()->remove("UndoInsertEmptyFrames"+QString::number((UINT)this));
    TImageCache::instance()->remove("UndoInsertEmptyFrames" +
                                    QString::number((uintptr_t) this));
  }

  void undo() const override {
    removeFramesWithoutUndo(m_level, m_frames);
    assert(m_oldFrames.size() == m_level->getFrameCount());
    if (m_updateXSheet) {
      std::vector<TFrameId> newFrames;
      m_level->getFids(newFrames);
      updateXSheet(m_level.getPointer(), newFrames, m_oldFrames);
    }
    m_level->renumber(m_oldFrames);
	m_level->setDirtyFlag(true);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const override {
    if (!m_level || m_frames.empty()) return;
    if (m_level->getType() == PLI_XSHLEVEL)
      FilmstripCmd::insert(m_level.getPointer(), m_frames, false);
    else if (m_level->getType() == TZP_XSHLEVEL) {
      makeSpaceForFids(m_level.getPointer(), m_frames);
      std::set<TFrameId>::const_iterator it;
      // TToonzImageP image =
      // (TToonzImageP)TImageCache::instance()->get("UndoInsertEmptyFrames"+QString::number((UINT)this),
      // true);
      TToonzImageP image = (TToonzImageP)TImageCache::instance()->get(
          "UndoInsertEmptyFrames" + QString::number((uintptr_t) this), true);
      if (!image) return;
      for (it = m_frames.begin(); it != m_frames.end(); ++it)
        m_level->setFrame(*it, image);
      invalidateIcons(m_level.getPointer(), m_frames);
      m_level->setDirtyFlag(true);
      TApp::instance()->getCurrentLevel()->notifyLevelChange();
    }
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Insert  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// insert
//-----------------------------------------------------------------------------

void FilmstripCmd::insert(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                          bool withUndo) {
  if (frames.empty() || !sl || sl->isSubsequence() || sl->isReadOnly()) return;
  std::vector<TFrameId> oldFrames;
  if (withUndo) sl->getFids(oldFrames);

  insertEmptyFilmstripFrames(sl, frames);
  if (withUndo)
    TUndoManager::manager()->add(
        new UndoInsertEmptyFrames(sl, frames, oldFrames));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void performReverse(const TXshSimpleLevelP &sl,
                    const std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;

  std::vector<TFrameId> fids;
  std::vector<TFrameId> oldFrames;
  sl->getFids(oldFrames);
  sl->getFids(fids);
  int i = 0, j = (int)fids.size() - 1;
  for (;;) {
    while (i < j && frames.count(fids[i]) == 0) i++;
    while (i < j && frames.count(fids[j]) == 0) j--;
    if (i >= j) break;
    std::swap(fids[i], fids[j]);
    i++;
    j--;
  }
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl.getPointer(), oldFrames, fids);
  }
  sl->renumber(fids);
  sl->setDirtyFlag(true);
  //  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

class FilmstripReverseUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;

public:
  FilmstripReverseUndo(TXshSimpleLevelP level, std::set<TFrameId> frames)
      : m_level(level), m_frames(frames) {}

  void undo() const override { performReverse(m_level, m_frames); }
  void redo() const override { performReverse(m_level, m_frames); }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Reverse  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// reverse
//-----------------------------------------------------------------------------

void FilmstripCmd::reverse(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  performReverse(sl, frames);
  TUndoManager::manager()->add(new FilmstripReverseUndo(sl, frames));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void performSwing(const TXshSimpleLevelP &sl,
                  const std::set<TFrameId> &frames) {
  if (!sl) return;
  int count = frames.size() - 1;
  if (count <= 0) return;  // niente swing con un solo frame
  TFrameId lastFid     = *frames.rbegin();
  TFrameId insertPoint = lastFid + 1;
  std::set<TFrameId> framesToInsert;
  int i;
  for (i = 0; i < count; i++) framesToInsert.insert(insertPoint + i);

  std::vector<TImage *> clonedImages;
  std::set<TFrameId>::const_reverse_iterator k;
  k = frames.rbegin();
  for (++k; k != frames.rend(); ++k) {
    TImageP img = sl->getFrame(*k, false);
    clonedImages.push_back(img ? img->cloneImage() : 0);
  }

  makeSpaceForFids(sl.getPointer(), framesToInsert);
  assert(count == (int)clonedImages.size());
  for (i = 0; i < count && (int)i < (int)clonedImages.size(); i++) {
    TImage *img = clonedImages[i];
    if (img) sl->setFrame(insertPoint + i, img);
  }
  invalidateIcons(sl.getPointer(), framesToInsert);
  sl->setDirtyFlag(true);
  //  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

class FilmstripSwingUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;
  std::set<TFrameId> m_newFrames;

public:
  FilmstripSwingUndo(const TXshSimpleLevelP &level,
                     const std::set<TFrameId> &frames)
      : m_level(level), m_frames(frames) {
    int count = frames.size() - 1;
    if (count <= 0) return;  // niente swing con un solo frame
    TFrameId lastFid     = *frames.rbegin();
    TFrameId insertPoint = lastFid + 1;
    std::set<TFrameId> framesToInsert;
    int i;
    for (i = 0; i < count; i++) m_newFrames.insert(insertPoint + i);
  }

  void undo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    removeFramesWithoutUndo(m_level, m_newFrames);
  }
  void redo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    performSwing(m_level, m_frames);
  }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Swing  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// swing
//-----------------------------------------------------------------------------

void FilmstripCmd::swing(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  performSwing(sl, frames);
  TUndoManager::manager()->add(new FilmstripSwingUndo(sl, frames));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void stepFilmstripFrames(const TXshSimpleLevelP &sl,
                         const std::set<TFrameId> &frames, int step = 2) {
  if (!sl || frames.empty() || step < 2) return;
  std::vector<TFrameId> fids;
  std::set<TFrameId> changedFids;
  std::vector<int> insertIndices;
  std::vector<TFrameId> oldFrames;
  sl->getFids(oldFrames);
  sl->getFids(fids);
  int i, offset = 0;
  for (i = 0; i < (int)fids.size(); i++) {
    bool frameToStep = (frames.count(fids[i]) > 0);
    if (offset > 0) {
      changedFids.insert(fids[i]);
      fids[i] = fids[i] + offset;
      changedFids.insert(fids[i]);
    }
    if (frameToStep) {
      insertIndices.push_back(i);
      offset += step - 1;
    }
  }
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl.getPointer(), oldFrames, fids);
  }
  sl->renumber(fids);
  for (i = 0; i < (int)insertIndices.size(); i++) {
    int j        = insertIndices[i];
    TFrameId fid = fids[j];
    TImageP img  = sl->getFrame(fid, false);
    if (img) {
      int h;
      for (h = 1; h < step; h++) {
        sl->setFrame(fid + h, img->cloneImage());
        changedFids.insert(fid + h);
      }
    }
  }
  invalidateIcons(sl.getPointer(), changedFids);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

class StepFilmstripUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_insertedFrames;
  std::set<TFrameId> m_frames;
  std::vector<TFrameId> m_oldFrames;
  int m_step;
  bool m_updateXSheet;

public:
  StepFilmstripUndo(const TXshSimpleLevelP &level,
                    const std::set<TFrameId> &frames, int step)
      : m_level(level), m_frames(frames), m_step(step) {
    assert(m_level);
    m_level->getFids(m_oldFrames);
    int d = 0;
    std::set<TFrameId>::const_iterator it;
    for (it = frames.begin(); it != frames.end(); ++it)
      for (int j = 1; j < step; j++) m_insertedFrames.insert(*it + (++d));
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  void undo() const override {
    removeFramesWithoutUndo(m_level, m_insertedFrames);
    std::set<TFrameId>::const_iterator it = m_frames.begin();
    if (m_updateXSheet) {
      std::vector<TFrameId> newFrames;
      m_level->getFids(newFrames);
      updateXSheet(m_level.getPointer(), newFrames, m_oldFrames);
    }
    m_level->renumber(m_oldFrames);
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
	m_level->setDirtyFlag(true);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }
  void redo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    stepFilmstripFrames(m_level, m_frames, m_step);
  }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Step %1  : Level %2")
        .arg(QString::number(m_step))
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// step
//-----------------------------------------------------------------------------

void FilmstripCmd::step(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                        int step) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  StepFilmstripUndo *undo = new StepFilmstripUndo(sl, frames, step);
  stepFilmstripFrames(sl, frames, step);
  TUndoManager::manager()->add(undo);
  frames.clear();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  QApplication::restoreOverrideCursor();
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

std::map<TFrameId, QString> eachFilmstripFrames(
    const TXshSimpleLevelP &sl, const std::set<TFrameId> &frames, int each) {
  if (frames.empty() || !sl || each < 2) return std::map<TFrameId, QString>();
  std::vector<TFrameId> framesToDelete;
  std::set<TFrameId>::const_iterator it;
  int k = 0;
  for (it = frames.begin(); it != frames.end(); ++it, ++k)
    if ((k % each) > 0) framesToDelete.push_back(*it);
  int i = 0;
  std::vector<TFrameId>::reverse_iterator fit;
  std::map<TFrameId, QString> cutFrames;

  for (fit = framesToDelete.rbegin(); fit != framesToDelete.rend(); ++fit) {
    TImageP img = sl->getFrame(*fit, false);
    if (img) {
      // QString id =
      // "eachFrames"+QString::number((UINT)sl.getPointer())+"-"+QString::number(fit->getNumber());
      QString id = "eachFrames" + QString::number((uintptr_t)sl.getPointer()) +
                   "-" + QString::number(fit->getNumber());
      TImageCache::instance()->add(id, img);

      cutFrames[*fit] = id;
    }
    sl->eraseFrame(*fit);  // toglie da cache?
    IconGenerator::instance()->remove(sl.getPointer(), *fit);
  }

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  return cutFrames;
}

//-----------------------------------------------------------------------------

class EachFilmstripUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;
  std::map<TFrameId, QString> m_cutFrames;
  int m_each;

public:
  EachFilmstripUndo(const TXshSimpleLevelP &level, int each,
                    const std::set<TFrameId> &frames,
                    std::map<TFrameId, QString> deletedFrames)
      : m_level(level)
      , m_cutFrames(deletedFrames)
      , m_each(each)
      , m_frames(frames) {}
  ~EachFilmstripUndo() {
    std::map<TFrameId, QString>::iterator it = m_cutFrames.begin();
    for (; it != m_cutFrames.end(); ++it)
      TImageCache::instance()->remove(it->second);
  }
  void undo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    insertNotEmptyframes(m_level, m_cutFrames);
  }
  void redo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    eachFilmstripFrames(m_level, m_frames, m_each);
  }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Each %1  : Level %2")
        .arg(QString::number(m_each))
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// each
//-----------------------------------------------------------------------------

void FilmstripCmd::each(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                        int each) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  std::map<TFrameId, QString> deletedFrames =
      eachFilmstripFrames(sl, frames, each);
  TUndoManager::manager()->add(
      new EachFilmstripUndo(sl, each, frames, deletedFrames));
  frames.clear();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class UndoDuplicateDrawing final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frameInserted;
  std::vector<TFrameId> m_oldFrames;
  std::set<TFrameId> m_framesForRedo;
  bool m_updateXSheet;

public:
  UndoDuplicateDrawing(const TXshSimpleLevelP &level,
                       const std::vector<TFrameId> oldFrames,
                       std::set<TFrameId> frameInserted,
                       std::set<TFrameId> framesForRedo)
      : m_level(level)
      , m_oldFrames(oldFrames)
      , m_frameInserted(frameInserted)
      , m_framesForRedo(framesForRedo) {
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  void undo() const override {
    assert(m_level);
    removeFramesWithoutUndo(m_level, m_frameInserted);
    if (m_updateXSheet) {
      std::vector<TFrameId> newFrames;
      m_level->getFids(newFrames);
      updateXSheet(m_level.getPointer(), newFrames, m_oldFrames);
    }
    m_level->renumber(m_oldFrames);
	m_level->setDirtyFlag(true);
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }
  void redo() const override {
    std::set<TFrameId> framesForRedo = m_framesForRedo;
    FilmstripCmd::duplicate(m_level.getPointer(), framesForRedo, false);
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Duplicate  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// duplicate
//-----------------------------------------------------------------------------

void FilmstripCmd::duplicate(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                             bool withUndo) {
  if (frames.empty() || !sl || sl->isSubsequence() || sl->isReadOnly()) return;

  TFrameId insertPoint = (*frames.rbegin()) + 1;

  std::map<TFrameId, QString> framesToInsert;
  std::set<TFrameId> newFrames;
  int i = 0;
  for (auto const &fid : frames) {
    TImageP img      = sl->getFrame(fid, false);
    TImageP imgClone = (img) ? img->cloneImage() : 0;
    QString id       = "dupFrames" + QString::number((uintptr_t)sl) + "-" +
                 QString::number(fid.getNumber());
    TImageCache::instance()->add(id, imgClone);
    framesToInsert[insertPoint + i] = id;
    newFrames.insert(insertPoint + i);
    i++;
  }
  std::vector<TFrameId> oldFrames;
  sl->getFids(oldFrames);
  insertNotEmptyframes(sl, framesToInsert);
  if (withUndo)
    TUndoManager::manager()->add(
        new UndoDuplicateDrawing(sl, oldFrames, newFrames, frames));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void moveToSceneFrames(TXshLevel *level, const std::set<TFrameId> &frames) {
  if (frames.empty() || !level) return;

  TXsheetHandle *xh = TApp::instance()->getCurrentXsheet();
  TXsheet *xsh      = xh->getXsheet();
  int row           = 0;
  int col           = xsh->getFirstFreeColumnIndex();
  std::set<TFrameId>::const_iterator it;
  if (level->getPaletteLevel()) {
    TXshPaletteColumn *column = new TXshPaletteColumn;
    xsh->insertColumn(col, column);
  }
  for (it = frames.begin(); it != frames.end(); ++it) {
    xsh->setCell(row, col, TXshCell(level, *it));
    ++row;
  }
  xh->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

class MoveLevelToSceneUndo final : public TUndo {
  std::wstring m_levelName;
  int m_col;
  std::set<TFrameId> m_fids;

public:
  MoveLevelToSceneUndo(std::wstring levelName, int col, std::set<TFrameId> fids)
      : m_levelName(levelName), m_col(col), m_fids(fids) {}

  void undo() const override {
    TApp *app         = TApp::instance();
    TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXshLevel *xl     = scene->getLevelSet()->getLevel(m_levelName);
    if (xl->getPaletteLevel()) xsh->removeColumn(m_col);
    xsh->clearCells(0, m_col, m_fids.size());
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXshLevel *xl     = scene->getLevelSet()->getLevel(m_levelName);
    if (!xl) return;
    moveToSceneFrames(xl, m_fids);
  }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Move Level to Scene  : Level %1")
        .arg(QString::fromStdWString(m_levelName));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// moveToScene
//-----------------------------------------------------------------------------

void FilmstripCmd::moveToScene(TXshLevel *sl, std::set<TFrameId> &frames) {
  if (frames.empty() || !sl) return;

  TXsheetHandle *xh = TApp::instance()->getCurrentXsheet();
  TXsheet *xsh      = xh->getXsheet();
  int row           = 0;
  int col           = xsh->getFirstFreeColumnIndex();
  std::set<TFrameId>::const_iterator it;
  for (it = frames.begin(); it != frames.end(); ++it) {
    xsh->setCell(row, col, TXshCell(sl, *it));
    ++row;
  }
  xh->notifyXsheetChanged();
  TUndoManager::manager()->add(
      new MoveLevelToSceneUndo(sl->getName(), col, frames));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void FilmstripCmd::moveToScene(TXshSimpleLevel *sl) {
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  std::set<TFrameId> fidsSet(fids.begin(), fids.end());
  moveToScene(sl, fidsSet);
}

//-----------------------------------------------------------------------------

void FilmstripCmd::moveToScene(TXshPaletteLevel *pl) {
  if (!pl) return;
  std::set<TFrameId> fidsSet;
  fidsSet.insert(TFrameId(1));

  TXsheetHandle *xh         = TApp::instance()->getCurrentXsheet();
  TXsheet *xsh              = xh->getXsheet();
  int row                   = 0;
  int col                   = xsh->getFirstFreeColumnIndex();
  TXshPaletteColumn *column = new TXshPaletteColumn;
  xsh->insertColumn(col, column);
  std::set<TFrameId>::const_iterator it;
  for (it = fidsSet.begin(); it != fidsSet.end(); ++it) {
    xsh->setCell(row, col, TXshCell(pl, *it));
    ++row;
  }
  xh->notifyXsheetChanged();
  TUndoManager::manager()->add(
      new MoveLevelToSceneUndo(pl->getName(), col, fidsSet));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void FilmstripCmd::moveToScene(TXshSoundLevel *sl) {
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  std::set<TFrameId> fidsSet(fids.begin(), fids.end());
  moveToScene(sl, fidsSet);
}

//=============================================================================
// UndoInbetween
//-----------------------------------------------------------------------------

namespace {

class UndoInbetween final : public TUndo {
  TXshSimpleLevelP m_level;
  std::vector<TFrameId> m_fids;
  std::vector<TVectorImageP> m_images;
  FilmstripCmd::InbetweenInterpolation m_interpolation;

public:
  UndoInbetween(TXshSimpleLevel *xl, std::vector<TFrameId> fids,
                FilmstripCmd::InbetweenInterpolation interpolation)
      : m_level(xl), m_fids(fids), m_interpolation(interpolation) {
    std::vector<TFrameId>::iterator it = fids.begin();
    // mi salvo tutte le immagine
    for (; it != fids.end(); ++it)
      m_images.push_back(m_level->getFrame(
          *it, false));  // non si fa clone perche' il livello subito dopo
                         // rilascia queste immagini a causa dell'inbetweener
  }

  void undo() const override {
    UINT levelSize = m_fids.size() - 1;
    for (UINT count = 1; count != levelSize; count++) {
      TVectorImageP vImage = m_images[count];
      m_level->setFrame(m_fids[count], vImage);
      IconGenerator::instance()->invalidate(m_level.getPointer(),
                                            m_fids[count]);
    }

    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const override {
    TFrameId fid0 = *m_fids.begin();
    TFrameId fid1 = *(--m_fids.end());
    FilmstripCmd::inbetweenWithoutUndo(m_level.getPointer(), fid0, fid1,
                                       m_interpolation);
  }

  int getSize() const override {
    assert(!m_images.empty());
    return m_images.size() * m_images.front()->getStrokeCount() * 100;
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Inbetween  : Level %1,  ")
                      .arg(QString::fromStdWString(m_level->getName()));
    switch (m_interpolation) {
    case FilmstripCmd::II_Linear:
      str += QString("Linear Interporation");
      break;
    case FilmstripCmd::II_EaseIn:
      str += QString("Ease In Interporation");
      break;
    case FilmstripCmd::II_EaseOut:
      str += QString("Ease Out Interporation");
      break;
    case FilmstripCmd::II_EaseInOut:
      str += QString("Ease In-Out Interporation");
      break;
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }
};

}  // namespace

//=============================================================================
// inbetween
//-----------------------------------------------------------------------------

void FilmstripCmd::inbetweenWithoutUndo(
    TXshSimpleLevel *sl, const TFrameId &fid0, const TFrameId &fid1,
    FilmstripCmd::InbetweenInterpolation interpolation) {
  if (!sl) return;
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  std::vector<TFrameId>::iterator it;
  it = std::find(fids.begin(), fids.end(), fid0);
  if (it == fids.end()) return;
  int ia = std::distance(fids.begin(), it);
  it     = std::find(fids.begin(), fids.end(), fid1);
  if (it == fids.end()) return;
  int ib = std::distance(fids.begin(), it);
  if (ib - ia < 2) return;

  TVectorImageP img0 = sl->getFrame(fid0, false);
  TVectorImageP img1 = sl->getFrame(fid1, false);
  if (!img0 || !img1) return;

  TInbetween inbetween(img0, img1);
  int i;
  for (i = ia + 1; i < ib; i++) {
    double t = (double)(i - ia) / (double)(ib - ia);
    double s = t;
    // in tutte le interpolazioni : s(0) = 0, s(1) = 1
    switch (interpolation) {
    case II_Linear:
      break;
    case II_EaseIn:
      s = t * t;
      break;  // s'(0) = 0
    case II_EaseOut:
      s = t * (2 - t);
      break;  // s'(1) = 0
    case II_EaseInOut:
      s = t * t * (3 - 2 * t);
      break;  // s'(0) = s'(1) = 0
    }

    TVectorImageP vi = inbetween.tween(s);
    sl->setFrame(fids[i], vi);
    IconGenerator::instance()->invalidate(sl, fids[i]);
  }
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void FilmstripCmd::inbetween(
    TXshSimpleLevel *sl, const TFrameId &fid0, const TFrameId &fid1,
    FilmstripCmd::InbetweenInterpolation interpolation) {
  std::vector<TFrameId> fids;
  std::vector<TFrameId> levelFids;
  sl->getFids(levelFids);
  for (auto const &fid : levelFids) {
    int curFid = fid.getNumber();
    if (fid0.getNumber() <= curFid && curFid <= fid1.getNumber())
      fids.push_back(fid);
  }

  TUndoManager::manager()->add(new UndoInbetween(sl, fids, interpolation));

  inbetweenWithoutUndo(sl, fid0, fid1, interpolation);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void FilmstripCmd::renumberDrawing(TXshSimpleLevel *sl, const TFrameId &oldFid,
                                   const TFrameId &desiredNewFid) {
  if (oldFid == desiredNewFid) return;
  std::vector<TFrameId> fids;
  std::vector<TFrameId> oldFrames;
  sl->getFids(oldFrames);
  sl->getFids(fids);
  std::vector<TFrameId>::iterator it =
      std::find(fids.begin(), fids.end(), oldFid);
  if (it == fids.end()) return;
  TFrameId newFid = desiredNewFid;
  while (std::find(fids.begin(), fids.end(), newFid) != fids.end()) {
    char letter = newFid.getLetter();
    if (letter == 'z') return;
    if (letter == 0)
      letter = 'a';
    else
      letter++;
    newFid = TFrameId(newFid.getNumber(), letter);
  }
  *it = newFid;
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl, oldFrames, fids);
  }
  sl->renumber(fids);
  sl->setDirtyFlag(true);

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}
