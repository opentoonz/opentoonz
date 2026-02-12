

#include "filmstripcommand.h"

// Tnz6 includes
#include "tapp.h"
#include "tundo.h"
#include "historytypes.h"
#include "drawingdata.h"
#include "tinbetween.h"
#include "timagecache.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/rasterselection.h"

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/strokesdata.h"
#include "toonzqt/rasterimagedata.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/palettecontroller.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tframehandle.h"

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

// TnzCore includes
#include "trop.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

// C++ includes
#include <memory>
#include <algorithm>

//=============================================================================

TFrameId operator+(const TFrameId &fid, int d) {
  return TFrameId(fid.getNumber() + d, fid.getLetter(), fid.getZeroPadding(),
                  fid.getStartSeqInd());
}

//-----------------------------------------------------------------------------
/*
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
          if (level && std::find(childLevels.begin(), childLevels.end(),
                                 level) == childLevels.end()) {
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
*/
//-----------------------------------------------------------------------------

static void updateXSheet(TXshSimpleLevel *sl, std::vector<TFrameId> oldFids,
                         std::vector<TFrameId> newFids) {
  std::vector<TXshChildLevel *> childLevels;
  TXsheet *xsh =
      TApp::instance()->getCurrentScene()->getScene()->getTopXsheet();
  bool changed =
      ToolUtils::doUpdateXSheet(sl, oldFids, newFids, xsh, childLevels);
  if (changed) TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================================
// makeSpaceForFid
// TODO: If necessary, renumber other frame IDs in the level
// before inserting framesToInsert, so that the IDs are free.
//-----------------------------------------------------------------------------

void makeSpaceForFids(TXshSimpleLevel *sl,
                      const std::set<TFrameId> &framesToInsert) {
  std::vector<TFrameId> fids, oldFids;
  sl->getFids(fids);
  oldFids = fids;
  std::set<TFrameId> touchedFids;
  for (const TFrameId &fidBase : framesToInsert) {
    TFrameId fid = fidBase;
    auto j       = fids.begin();
    while ((j = std::find(j, fids.end(), fid)) != fids.end()) {
      touchedFids.insert(fid);
      fid = fid + 1;
      touchedFids.insert(fid);
      *j = fid;
 // Now I need to check that the new fid is not already
// in the remaining part of the fids array
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

void copyFramesWithoutUndo(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  QClipboard *clipboard = QApplication::clipboard();
  auto *data            = new DrawingData();
  data->setLevelFrames(sl, frames);
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------
// frames is a selected frames in the film strip
bool pasteAreasWithoutUndo(const QMimeData *data, TXshSimpleLevel *sl,
                           std::set<TFrameId> &frames,
                           std::unique_ptr<TTileSet> &tileSet,
                           std::map<TFrameId, std::set<int>> &indices) {
  // paste between the same level must keep the palette unchanged
  // Not emitting PaletteChanged signal can avoid the update of color model
  bool paletteHasChanged = true;

  if (const auto *strokesData = dynamic_cast<const StrokesData *>(data)) {
    for (const TFrameId &fid : frames) {
      TImageP img = sl->getFrame(fid, true);
      if (!img) {
        img = sl->createEmptyFrame();
        sl->setFrame(fid, img);
      }
      TVectorImageP vi = img;
      TToonzImageP ti  = img;
      TRasterImageP ri = img;
      if (vi) {
        std::set<int> imageIndices;
        strokesData->getImage(vi, imageIndices, true);
        indices[fid] = imageIndices;
      } else if (ti) {
        std::unique_ptr<ToonzImageData> toonzImageData(
            strokesData->toToonzImageData(ti));
        return pasteAreasWithoutUndo(toonzImageData.get(), sl, frames, tileSet,
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
        std::unique_ptr<FullColorImageData> fullColorImageData(
            strokesData->toFullColorImageData(ri));
        return pasteAreasWithoutUndo(fullColorImageData.get(), sl, frames,
                                     tileSet, indices);
      }
    }
  } else if (const auto *rasterImageData =
                 dynamic_cast<const RasterImageData *>(data)) {
    for (const TFrameId &fid : frames) {
      if ((sl->getType() == PLI_XSHLEVEL || sl->getType() == TZP_XSHLEVEL) &&
          dynamic_cast<const FullColorImageData *>(rasterImageData)) {
        DVGui::error(QObject::tr(
            "The copied selection cannot be pasted in the current drawing."));
        return false;
      }
      TImageP img = sl->getFrame(fid, true);
      if (!img) {
        img = sl->createEmptyFrame();
        sl->setFrame(fid, img);
      }
      TToonzImageP ti  = img;
      TRasterImageP ri = img;
      TVectorImageP vi = img;
      if (ti) {
        TRasterP ras;
        double dpiX, dpiY;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes, originalStrokes;
        TAffine affine;
        int styleCountBeforePasteImage = ti->getPalette()->getStyleCount();

        rasterImageData->getData(ras, dpiX, dpiY, rects, strokes,
                                 originalStrokes, affine, ti->getPalette());

        if (styleCountBeforePasteImage == ti->getPalette()->getStyleCount())
          paletteHasChanged = false;

        double imgDpiX, imgDpiY;
        ti->getDpi(imgDpiX, imgDpiY);
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;

        TRectD boxD;
        if (!rects.empty()) boxD = rects[0];
        if (!strokes.empty()) boxD = strokes[0].getBBox();
        for (const auto &r : rects) boxD += r;
        for (const auto &s : strokes) boxD += s.getBBox();
        boxD       = affine * boxD;
        TRect box  = ToonzImageUtils::convertWorldToRaster(boxD, ti);
        TPoint pos = box.getP00();

        if (pos.x < 0) pos.x = 0;
        if (pos.y < 0) pos.y = 0;

        if (!tileSet)
          tileSet = std::make_unique<TTileSetCM32>(ti->getRaster()->getSize());
        if (box.overlaps(ti->getRaster()->getBounds()))
          tileSet->add(ti->getRaster(), box);
        else
          tileSet->add(nullptr);
        TRasterCM32P app = ras;
        if (app) {
          TRop::over(ti->getRaster(), app, pos, affine);
          ToolUtils::updateSaveBox(sl, fid);
        }
      } else if (ri) {
        TRasterP ras;
        double dpiX = 0, dpiY = 0, imgDpiX = 0, imgDpiY = 0;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes, originalStrokes;
        TAffine affine;

        ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
        if (!scene) return false;
        TCamera *camera   = scene->getCurrentCamera();
        TPointD cameraDpi = camera->getDpi();
        if (cameraDpi.x == 0.0 || cameraDpi.y == 0.0) return false;

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

        TRectD boxD;
        if (!rects.empty()) boxD = rects[0];
        if (!strokes.empty()) boxD = strokes[0].getBBox();
        for (const auto &r : rects) boxD += r;
        for (const auto &s : strokes) boxD += s.getBBox();
        boxD       = affine * boxD;
        TRect box  = TRasterImageUtils::convertWorldToRaster(boxD, ri);
        TPoint pos = box.getP00();

        if (!tileSet)
          tileSet =
              std::make_unique<TTileSetFullColor>(ri->getRaster()->getSize());
        if (box.overlaps(ri->getRaster()->getBounds()))
          tileSet->add(ri->getRaster(), box);
        else
          tileSet->add(nullptr);

        TRasterCM32P app = ras;
        if (app)
          TRop::over(ri->getRaster(), app, ri->getPalette(), pos, affine);
        else
          TRop::over(ri->getRaster(), ras, pos, affine);
      } else if (vi) {
        std::unique_ptr<StrokesData> strokesData(
            rasterImageData->toStrokesData(sl->getScene()));
        return pasteAreasWithoutUndo(strokesData.get(), sl, frames, tileSet,
                                     indices);
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
// If insert == true, paste the frames into the level by inserting them,
// moving the existing frames downward if necessary.
// Otherwise, paste the frames by replacing existing ones.
// The parameter cloneImages is set to false when this function
// is used for undo/redo operations.

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

// "Clear" the frames: the existing frames are discarded
// and replaced with empty frames.
std::map<TFrameId, QString> clearFramesWithoutUndo(
    const TXshSimpleLevelP &sl, const std::set<TFrameId> &frames) {
  std::map<TFrameId, QString> clearedFrames;
  if (!sl || frames.empty()) return clearedFrames;

  for (const TFrameId &frameId : frames) {
/* You shouldn’t cast to UINT */
// QString id =
// "clearFrames"+QString::number((UINT)sl.getPointer())+"-"+QString::number(it->getNumber());
    QString id = "clearFrames" + QString::number((uintptr_t)sl.getPointer()) +
                 "-" + QString::number(frameId.getNumber());
    TImageCache::instance()->add(id, sl->getFrame(frameId, false));
    clearedFrames[frameId] = id;
    TImageP emptyFrame     = sl->createEmptyFrame();
    sl->eraseFrame(frameId);
    sl->setFrame(frameId, emptyFrame);
  }
  invalidateIcons(sl.getPointer(), frames);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
  sl->setDirtyFlag(true);
  return clearedFrames;
}

//-----------------------------------------------------------------------------

void removeFramesWithoutUndo(const TXshSimpleLevelP &sl,
                             const std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;
  for (const TFrameId &fid : frames) sl->eraseFrame(fid);
  removeIcons(sl.getPointer(), frames);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void cutFramesWithoutUndo(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  std::map<TFrameId, QString> imageSet;

  HookSet *levelHooks   = sl->getHookSet();
  int currentFrameIndex = TApp::instance()->getCurrentFrame()->getFrameIndex();
  int i                 = 0;
  for (const TFrameId &frameId : frames) {
    QString id = "cutFrames" + QString::number((uintptr_t)sl) + "-" +
                 QString::number(frameId.getNumber());
    TImageCache::instance()->add(id, sl->getFrame(frameId, false));
    imageSet[frameId] = id;
  }
  removeIcons(sl, frames);
  sl->setDirtyFlag(true);

  QClipboard *clipboard = QApplication::clipboard();
  auto *data            = new DrawingData();
  data->setFrames(imageSet, sl, *levelHooks);
  clipboard->setMimeData(data, QClipboard::Clipboard);

  for (const TFrameId &fid : frames) sl->eraseFrame(fid);

  std::vector<TFrameId> newFids;
  sl->getFids(newFids);
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
  for (const auto &[fid, _] : framesToInsert) frames.insert(fid);

  makeSpaceForFids(sl.getPointer(), frames);

  for (const auto &[fid, cacheId] : framesToInsert) {
    TImageP img = TImageCache::instance()->get(cacheId, false);
    TImageCache::instance()->remove(cacheId);
    assert(img);
    sl->setFrame(fid, img);
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
  std::unique_ptr<TTileSet> m_tiles;
  std::unique_ptr<RasterImageData> m_data;
  TPaletteP m_oldPalette;
  TPaletteP m_newPalette;
  bool m_isFrameInserted;

public:
  PasteRasterAreasUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                       TTileSet *tiles, RasterImageData *data, TPalette *plt,
                       bool isFrameInserted)
      : m_level(sl)
      , m_frames(frames)
      , m_tiles(tiles)
      , m_data(data->clone())
      , m_oldPalette(plt->clone())
      , m_isFrameInserted(isFrameInserted) {
    assert(m_tiles->getTileCount() == m_frames.size());
  }

  void onAdd() override { m_newPalette = m_level->getPalette()->clone(); }

  void undo() const override {
    if (!m_level || m_frames.empty()) return;
    std::set<TFrameId> frames = m_frames;

    if (m_isFrameInserted) {
      removeFramesWithoutUndo(m_level, frames);
    } else {
      int i = 0;
      const TTileSetCM32 *tileSetCM =
          dynamic_cast<const TTileSetCM32 *>(m_tiles.get());
      const TTileSetFullColor *tileSetFC =
          dynamic_cast<const TTileSetFullColor *>(m_tiles.get());
      for (const TFrameId &fid : m_frames) {
        TImageP image = m_level->getFrame(fid, true);
        if (!image) continue;
        TRasterImageP ri(image);
        TToonzImageP ti(image);
        if (tileSetCM) {
          const TTileSetCM32::Tile *tile = tileSetCM->getTile(i);
          if (!tile) {
            ++i;
            continue;
          }
          TRasterCM32P tileRas;
          tile->getRaster(tileRas);
          assert(ti);
          ti->getRaster()->copy(tileRas, tile->m_rasterBounds.getP00());
          ToolUtils::updateSaveBox(m_level, fid);
        } else if (tileSetFC) {
          const TTileSetFullColor::Tile *tile = tileSetFC->getTile(i);
          if (!tile) {
            ++i;
            continue;
          }
          TRasterP tileRas;
          tile->getRaster(tileRas);
          assert(ri);
          ri->getRaster()->copy(tileRas, tile->m_rasterBounds.getP00());
        }
        ++i;
      }
    }

    if (m_oldPalette) {
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

    for (const TFrameId &fid : m_frames) {
      TImageP image    = m_level->getFrame(fid, true);
      TRasterImageP ri = image;
      TToonzImageP ti  = image;
      if (ti) {
        TRasterP ras;
        double dpiX, dpiY;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes, originalStrokes;
        TAffine affine;
        m_data->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes,
                        affine, ti->getPalette());
        double imgDpiX, imgDpiY;
        ti->getDpi(imgDpiX, imgDpiY);
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;

        TRectD boxD;
        if (!rects.empty()) boxD = rects[0];
        if (!strokes.empty()) boxD = strokes[0].getBBox();
        for (const auto &r : rects) boxD += r;
        for (const auto &s : strokes) boxD += s.getBBox();
        boxD       = affine * boxD;
        TRect box  = ToonzImageUtils::convertWorldToRaster(boxD, ti);
        TPoint pos = box.getP00();

        TRasterCM32P app = ras;
        TRop::over(ti->getRaster(), app, pos, affine);
        ToolUtils::updateSaveBox(m_level, fid);
      } else if (ri) {
        TRasterP ras;
        double dpiX, dpiY;
        std::vector<TRectD> rects;
        std::vector<TStroke> strokes, originalStrokes;
        TAffine affine;
        m_data->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes,
                        affine, ri->getPalette());
        double imgDpiX, imgDpiY;
        if (dpiX == 0 && dpiY == 0) {
          ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
          if (scene) {
            TCamera *camera = scene->getCurrentCamera();
            TPointD dpi     = camera->getDpi();
            dpiX            = dpi.x;
            dpiY            = dpi.y;
          } else
            return;
        }
        ri->getDpi(imgDpiX, imgDpiY);
        TScale sc(imgDpiX / dpiX, imgDpiY / dpiY);
        affine *= sc;

        TRectD boxD;
        if (!rects.empty()) boxD = rects[0];
        if (!strokes.empty()) boxD = strokes[0].getBBox();
        for (const auto &r : rects) boxD += r;
        for (const auto &s : strokes) boxD += s.getBBox();
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

    if (m_newPalette) {
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
    bool first = true;
    for (const TFrameId &fid : m_frames) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(PasteRasterAreasUndo);
};

//=============================================================================
// PasteVectorAreasUndo
//-----------------------------------------------------------------------------

class PasteVectorAreasUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;
  std::map<TFrameId, std::set<int>> m_indices;
  std::unique_ptr<StrokesData> m_data;
  TPaletteP m_oldPalette;
  TPaletteP m_newPalette;
  bool m_isFrameInserted;

public:
  PasteVectorAreasUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                       std::map<TFrameId, std::set<int>> &indices,
                       StrokesData *data, TPalette *plt, bool isFrameInserted)
      : m_level(sl)
      , m_frames(frames)
      , m_indices(indices)
      , m_data(data->clone())
      , m_oldPalette(plt->clone())
      , m_isFrameInserted(isFrameInserted) {}

  void onAdd() override { m_newPalette = m_level->getPalette()->clone(); }

  void undo() const override {
    if (!m_level || m_frames.empty()) return;

    if (m_isFrameInserted) {
      removeFramesWithoutUndo(m_level, m_frames);
    } else {
      for (const TFrameId &fid : m_frames) {
        TVectorImageP img = m_level->getFrame(fid, true);
        if (!img) continue;
        auto it = m_indices.find(fid);
        if (it == m_indices.end()) continue;
        const std::set<int> &imageIndices = it->second;
        // remove strokes in reverse order
        for (auto it2 = imageIndices.rbegin(); it2 != imageIndices.rend();
             ++it2)
          img->removeStroke(*it2);
      }
    }

    if (m_oldPalette) {
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

    for (const TFrameId &fid : m_frames) {
      TVectorImageP img = m_level->getFrame(fid, true);
      assert(img);
      if (!img) continue;
      std::set<int> app;
      m_data->getImage(img, app, true);
    }

    if (m_newPalette) {
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
    bool first = true;
    for (const TFrameId &fid : m_frames) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(PasteVectorAreasUndo);
};

//=============================================================================
// PasteFramesUndo
//-----------------------------------------------------------------------------

class PasteFramesUndo final : public TUndo {
  TXshSimpleLevelP m_sl;
  std::set<TFrameId> m_frames;
  std::vector<TFrameId> m_oldLevelFrameId;
  TPaletteP m_oldPalette;
  std::unique_ptr<DrawingData> m_oldData;
  std::unique_ptr<DrawingData> m_newData;
  DrawingData::ImageSetType m_setType;
  std::unique_ptr<HookSet> m_oldLevelHooks;
  bool m_updateXSheet;
  bool m_keepOriginalPalette;

public:
  PasteFramesUndo(TXshSimpleLevel *sl, const std::set<TFrameId> &frames,
                  const std::vector<TFrameId> &oldLevelFrameId,
                  TPaletteP oldPalette, DrawingData::ImageSetType setType,
                  HookSet *oldLevelHooks, bool keepOriginalPalette,
                  DrawingData *oldData = nullptr)
      : m_sl(sl)
      , m_frames(frames)
      , m_oldLevelFrameId(oldLevelFrameId)
      , m_oldPalette(oldPalette)
      , m_oldData(oldData)
      , m_setType(setType)
      , m_oldLevelHooks(oldLevelHooks)
      , m_keepOriginalPalette(keepOriginalPalette) {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *data       = cloneData(clipboard->mimeData());
    m_newData.reset(dynamic_cast<DrawingData *>(data));
    assert(m_newData);
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  ~PasteFramesUndo() {
    if (m_oldData) m_oldData->releaseData();
    // m_newData automatically deleted by unique_ptr
  }

  void undo() const override {
    TSelection *selection = TSelection::getCurrent();
    if (selection) selection->selectNone();
    std::set<TFrameId> frames = m_frames;

    if (m_setType != DrawingData::OVER_SELECTION)
      removeFramesWithoutUndo(m_sl, frames);

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

    if (m_setType != DrawingData::INSERT && m_oldData) {
      std::set<TFrameId> framesToModify;
      m_oldData->getFrames(framesToModify);
      bool dummy = true;
      pasteFramesWithoutUndo(m_oldData.get(), m_sl.getPointer(), framesToModify,
                             DrawingData::OVER_SELECTION, true, dummy);
    }

    if (m_oldPalette) {
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
    pasteFramesWithoutUndo(m_newData.get(), m_sl.getPointer(), frames,
                           m_setType, true, keepOriginalPalette, true);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Paste  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));
    bool first = true;
    for (const TFrameId &fid : m_frames) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(PasteFramesUndo);
};

//=============================================================================
// DeleteFramesUndo
//-----------------------------------------------------------------------------

class DeleteFramesUndo final : public TUndo {
  TXshSimpleLevel *m_sl;
  std::set<TFrameId> m_frames;
  std::unique_ptr<DrawingData> m_oldData;
  std::unique_ptr<DrawingData> m_newData;

public:
  DeleteFramesUndo(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                   DrawingData *oldData, DrawingData *newData)
      : m_sl(sl), m_frames(frames), m_oldData(oldData), m_newData(newData) {}

  void pasteFramesFromData(const DrawingData *data) const {
    std::set<TFrameId> frames = m_frames;
    bool dummy                = true;
    pasteFramesWithoutUndo(data, m_sl, frames, DrawingData::OVER_SELECTION,
                           true, dummy);
  }

  void undo() const override { pasteFramesFromData(m_oldData.get()); }
  // NOTE: I cannot use the method "clearFramesWithoutUndo(...)" because
// it creates a NEW empty frame, discarding the previous one
// and any modifications that may have been made to it afterwards.

  void redo() const override { pasteFramesFromData(m_newData.get()); }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Delete Frames  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));
    bool first = true;
    for (const TFrameId &fid : m_frames) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(DeleteFramesUndo);
};

//-----------------------------------------------------------------------------

class CutFramesUndo final : public TUndo {
  TXshSimpleLevel *m_sl;
  std::set<TFrameId> m_framesCutted;
  std::vector<TFrameId> m_oldFrames;
  std::unique_ptr<DrawingData> m_newData;

public:
  CutFramesUndo(TXshSimpleLevel *sl, std::set<TFrameId> &framesCutted,
                std::vector<TFrameId> &oldFrames)
      : m_sl(sl), m_framesCutted(framesCutted), m_oldFrames(oldFrames) {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *data       = cloneData(clipboard->mimeData());
    m_newData.reset(dynamic_cast<DrawingData *>(data));
    assert(m_newData);
  }

  void undo() const override {
    std::set<TFrameId> frames = m_framesCutted;
    bool dummy                = true;
    pasteFramesWithoutUndo(m_newData.get(), m_sl, frames,
                           DrawingData::OVER_SELECTION, true, dummy);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    std::unique_ptr<QMimeData> currentData(cloneData(clipboard->mimeData()));

    std::set<TFrameId> frames = m_framesCutted;
    cutFramesWithoutUndo(m_sl, frames);

    clipboard->setMimeData(currentData.release(), QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Cut Frames  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));
    bool first = true;
    for (const TFrameId &fid : m_framesCutted) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(CutFramesUndo);
};

//-----------------------------------------------------------------------------

class RemoveFramesUndo final : public TUndo {
  TXshSimpleLevel *m_sl;
  std::set<TFrameId> m_framesRemoved;
  std::unique_ptr<DrawingData> m_oldData;

public:
  RemoveFramesUndo(TXshSimpleLevel *sl, std::set<TFrameId> &framesRemoved,
                   DrawingData *oldData)
      : m_sl(sl), m_framesRemoved(framesRemoved), m_oldData(oldData) {}

  void undo() const override {
    std::set<TFrameId> frames = m_framesRemoved;
    bool dummy                = true;
    pasteFramesWithoutUndo(m_oldData.get(), m_sl, frames,
                           DrawingData::OVER_SELECTION, true, dummy);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    std::set<TFrameId> frames = m_framesRemoved;
    removeFramesWithoutUndo(m_sl, frames);
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Remove Frames  : Level %1 : Frame ")
                      .arg(QString::fromStdWString(m_sl->getName()));
    bool first = true;
    for (const TFrameId &fid : m_framesRemoved) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(RemoveFramesUndo);
};

}  // namespace

//=============================================================================
// AddFramesUndo
//-----------------------------------------------------------------------------

namespace {

class AddFramesUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_insertedFids;
  std::vector<TFrameId> m_oldFids;
  bool m_updateXSheet;

public:
  AddFramesUndo(const TXshSimpleLevelP &level,
                const std::set<TFrameId> &insertedFids,
                std::vector<TFrameId> oldFids)
      : m_level(level)
      , m_insertedFids(insertedFids)
      , m_oldFids(std::move(oldFids)) {
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

    for (const TFrameId &fid : m_insertedFids) {
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
    bool first = true;
    for (const TFrameId &fid : m_insertedFids) {
      if (!first) str += QString(", ");
      first = false;
      str += QString::number(fid.getNumber());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(AddFramesUndo);
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

  TFrameId tmplFid;
  if (!oldFids.empty()) tmplFid = oldFids.front();

  std::set<TFrameId> fidsToInsert;
  for (int frame = start; frame <= end; frame += step) {
    fidsToInsert.insert(TFrameId(frame, "", tmplFid.getZeroPadding(),
                                 tmplFid.getStartSeqInd()));
  }

  makeSpaceForFids(sl, fidsToInsert);

  for (const TFrameId &fid : fidsToInsert) {
    sl->setFrame(fid, sl->createEmptyFrame());
    IconGenerator::instance()->invalidate(sl, fid);
  }
  sl->setDirtyFlag(true);

  auto *undo = new AddFramesUndo(sl, fidsToInsert, std::move(oldFids));
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
  bool m_updateXSheet;

public:
  RenumberUndo(const TXshSimpleLevelP &level, const std::vector<TFrameId> &fids,
               bool forceCallUpdateXSheet = false)
      : m_level(level), m_fids(fids) {
    assert(m_level);
    std::vector<TFrameId> oldFids;
    m_level->getFids(oldFids);
    assert(oldFids.size() == m_fids.size());
    for (size_t i = 0; i < m_fids.size(); ++i) {
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
    for (size_t i = 0; i < fids.size(); ++i) {
      auto it = m_mapOldFrameId.find(fids[i]);
      if (it != m_mapOldFrameId.end()) fids[i] = TFrameId(it->second);
    }
    renumber(fids);
  }

  void redo() const override { renumber(m_fids); }

  int getSize() const override {
    return sizeof(*this) + int(sizeof(TFrameId) * m_fids.size());
  }

  QString getHistoryString() override {
    return QObject::tr("Renumber  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(RenumberUndo);
};

QString getNextLetter(const QString &letter) {
  if (letter.isEmpty()) return QString('a');
  if (letter == 'z' || letter == 'Z') return QString();
  QByteArray byteArray = letter.toUtf8();
  ++byteArray.data()[byteArray.size() - 1];
  return QString::fromUtf8(byteArray);
}

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

  for (const auto &[srcFid, dstFid] : table) {
    if (!sl->isFid(srcFid)) return;
  }

  std::vector<TFrameId> fids, oldFrames;
  sl->getFids(oldFrames);
  fids = oldFrames;
  std::set<TFrameId> tmp(fids.begin(), fids.end());
  for (const auto &[srcFid, _] : table) tmp.erase(srcFid);

  for (size_t i = 0; i < fids.size(); ++i) {
    TFrameId srcFid = fids[i];
    auto it =
        std::find_if(table.begin(), table.end(),
                     [&srcFid](const auto &p) { return p.first == srcFid; });
    if (it != table.end()) {
      TFrameId tarFid = it->second;
      if (tmp.count(tarFid) > 0) {
        do {
          tarFid =
              TFrameId(tarFid.getNumber(), getNextLetter(tarFid.getLetter()),
                       tarFid.getZeroPadding(), tarFid.getStartSeqInd());
        } while (!tarFid.getLetter().isEmpty() && tmp.count(tarFid) > 0);
        if (tarFid.getLetter().isEmpty()) return;
      }
      tmp.insert(tarFid);
      fids[i] = tarFid;
    }
  }

  TXshSimpleLevelP slP(sl);
  TUndoManager::manager()->add(
      new RenumberUndo(slP, fids, forceCallUpdateXSheet));

  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled() ||
      forceCallUpdateXSheet) {
    updateXSheet(sl, oldFrames, fids);
  }
  sl->renumber(fids);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

void FilmstripCmd::renumber(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                            int startFrame, int stepFrame) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  assert(startFrame > 0 && stepFrame > 0);
  if (startFrame <= 0 || stepFrame <= 0 || frames.empty()) return;

  std::vector<TFrameId> fids, oldFrames;
  sl->getFids(oldFrames);
  fids = oldFrames;

  std::set<TFrameId> modifiedFids;
  std::set<TFrameId> tmp(fids.begin(), fids.end());
  for (const TFrameId &fid : frames) tmp.erase(fid);

  int frame = startFrame;
  auto j    = fids.begin();
  for (const TFrameId &srcFid : frames) {
    TFrameId dstFid(frame, "", srcFid.getZeroPadding(),
                    srcFid.getStartSeqInd());
    frame += stepFrame;
    // I perform the check on tmp instead of fids. Consider:
// fids = [1,2,3,4], renumber = [2->3, 3->5]
    if (tmp.count(dstFid) > 0) {
      DVGui::error(("can't renumber: frame conflict"));
      return;
    }
    j = std::find(j, fids.end(), srcFid);
    assert(j != fids.end());
    if (j == fids.end()) continue;
    int k = int(std::distance(fids.begin(), j));
    if (srcFid != dstFid) {
      modifiedFids.insert(srcFid);
      modifiedFids.insert(dstFid);
      *j = dstFid;
      ++j;
    }
  }

  TXshSimpleLevelP slP(sl);
  TUndoManager::manager()->add(new RenumberUndo(slP, fids));

  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl, oldFrames, fids);
  }
  sl->renumber(fids);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);

  std::set<TFrameId> newFrames;
  int i = 0;
  for (const TFrameId &fid : frames) {
    newFrames.insert(TFrameId(startFrame + (i * stepFrame), fid.getLetter(),
                              fid.getZeroPadding(), fid.getStartSeqInd()));
    ++i;
  }
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
  std::unique_ptr<QMimeData> data(cloneData(clipboard->mimeData()));

  if (auto *drawingData = dynamic_cast<DrawingData *>(data.get())) {
    if (sl->isSubsequence()) return;

    bool keepOriginalPalette;
    auto *oldLevelHooks = new HookSet();
    *oldLevelHooks      = *sl->getHookSet();

    bool isPaste =
        pasteFramesWithoutUndo(drawingData, sl, frames, DrawingData::INSERT,
                               true, keepOriginalPalette);
    if (!isPaste) {
      delete oldLevelHooks;
      return;
    }
    TUndoManager::manager()->add(new PasteFramesUndo(
        sl, frames, oldLevelFrameId, oldPalette, DrawingData::INSERT,
        oldLevelHooks, keepOriginalPalette));
    return;
  }

  bool isFrameToInsert =
      (frames.size() == 1) ? !sl->isFid(*frames.begin()) : false;
  std::unique_ptr<TTileSet> tileSet;
  std::map<TFrameId, std::set<int>> indices;
  std::unique_ptr<TUndo> undo;
  TPaletteP plt    = sl->getPalette()->clone();
  QImage clipImage = clipboard->image();

  auto *fullColorData = dynamic_cast<FullColorImageData *>(data.get());

  if ((!clipImage.isNull() || fullColorData) && sl->getType() != OVL_XSHLEVEL) {
    DVGui::error(QObject::tr(
        "Can't paste full raster data on a non full raster level."));
    return;
  }

  if (sl->getType() == OVL_XSHLEVEL && !clipImage.isNull()) {
    if (sl->getResolution().lx < clipImage.width() ||
        sl->getResolution().ly < clipImage.height()) {
      clipImage = clipImage.scaled(sl->getResolution().lx,
                                   sl->getResolution().ly, Qt::KeepAspectRatio);
    }

    std::vector<TRectD> rects;
    const std::vector<TStroke> strokes, originalStrokes;
    TAffine aff;
    TRasterP ras = rasterFromQImage(clipImage);
    rects.emplace_back(0.0 - clipImage.width() / 2,
                       0.0 - clipImage.height() / 2, clipImage.width() / 2,
                       clipImage.height() / 2);
    auto *qimageData = new FullColorImageData();
    TDimension dim   = sl->getResolution();
    qimageData->setData(ras, plt, 120.0, 120.0, dim, rects, strokes,
                        originalStrokes, aff);
    data.reset(qimageData);
  }

  if (sl && sl->getType() == OVL_XSHLEVEL) {
    ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
    if (toolHandle->getTool()->getName() == "T_Selection") {
      TSelection *ts = toolHandle->getTool()->getSelection();
      if (auto *rs = dynamic_cast<RasterSelection *>(ts)) {
        toolHandle->getTool()->onDeactivate();
        toolHandle->getTool()->onActivate();
        rs->pasteSelection();
        return;
      }
    }
  }

  bool isPaste =
      pasteAreasWithoutUndo(data.get(), sl, frames, tileSet, indices);
  if (!isPaste) return;

  if (auto *rasterImageData = dynamic_cast<RasterImageData *>(data.get())) {
    if (tileSet) {
      undo = std::make_unique<PasteRasterAreasUndo>(
          sl, frames, tileSet.release(), rasterImageData, plt.getPointer(),
          isFrameToInsert);
    }
    if (!indices.empty()) {
      undo = std::make_unique<PasteVectorAreasUndo>(
          sl, frames, indices, rasterImageData->toStrokesData(sl->getScene()),
          plt.getPointer(), isFrameToInsert);
    }
  } else if (auto *strokesData = dynamic_cast<StrokesData *>(data.get())) {
    if (tileSet) {
      TImageP img      = sl->getFrame(*frames.begin(), false);
      TRasterImageP ri = img;
      TToonzImageP ti  = img;
      if (ti) {
        undo = std::make_unique<PasteRasterAreasUndo>(
            sl, frames, tileSet.release(), strokesData->toToonzImageData(ti),
            plt.getPointer(), isFrameToInsert);
      } else if (ri) {
        double dpix, dpiy;
        ri->getDpi(dpix, dpiy);
        if (dpix == 0 || dpiy == 0) {
          TPointD dpi = sl->getScene()->getCurrentCamera()->getDpi();
          dpix        = dpi.x;
          dpiy        = dpi.y;
          ri->setDpi(dpix, dpiy);
        }
        undo = std::make_unique<PasteRasterAreasUndo>(
            sl, frames, tileSet.release(),
            strokesData->toFullColorImageData(ri), plt.getPointer(),
            isFrameToInsert);
      }
    } else if (!indices.empty()) {
      undo = std::make_unique<PasteVectorAreasUndo>(
          sl, frames, indices, strokesData, plt.getPointer(), isFrameToInsert);
    }
  }

  if (undo) {
    TUndoManager::manager()->add(undo.release());
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
  if (const auto *drawingData =
          dynamic_cast<const DrawingData *>(clipboard->mimeData())) {
    drawingData->getFrames(frameIdToChange);
    auto *data = new DrawingData();
    data->setLevelFrames(sl, frameIdToChange);
    auto *oldLevelHooks = new HookSet();
    *oldLevelHooks      = *sl->getHookSet();

    bool keepOriginalPalette = true;

    bool isPaste = pasteFramesWithoutUndo(drawingData, sl, frames,
                                          DrawingData::OVER_FRAMEID, true,
                                          keepOriginalPalette);
    if (!isPaste) {
      delete data;
      delete oldLevelHooks;
      return;
    }
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
  if (const auto *drawingData =
          dynamic_cast<const DrawingData *>(clipboard->mimeData())) {
    auto *data = new DrawingData();
    data->setLevelFrames(sl, frames);

    auto *oldLevelHooks = new HookSet();
    *oldLevelHooks      = *sl->getHookSet();

    bool keepOriginalPalette = true;
    bool isPaste             = pasteFramesWithoutUndo(drawingData, sl, frames,
                                                      DrawingData::OVER_SELECTION, true,
                                                      keepOriginalPalette);
    if (!isPaste) {
      delete data;
      delete oldLevelHooks;
      return;
    }

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
    for (const TFrameId &frameId : frames) {
      if (editableFrames.count(frameId) == 0) return;
    }
  }

  HookSet *levelHooks          = sl->getHookSet();
  std::set<TFrameId> oldFrames = frames;
  std::map<TFrameId, QString> clearedFrames =
      clearFramesWithoutUndo(sl, frames);
  auto *oldData = new DrawingData();
  oldData->setFrames(clearedFrames, sl, *levelHooks);
  auto *newData = new DrawingData();
  newData->setLevelFrames(sl, frames);
  frames.clear();
  TUndoManager::manager()->add(
      new DeleteFramesUndo(sl, oldFrames, oldData, newData));
}

//=============================================================================
// remove
//-----------------------------------------------------------------------------

void FilmstripCmd::remove(TXshSimpleLevel *sl, std::set<TFrameId> &frames) {
  if (!sl || frames.empty() || sl->isSubsequence() || sl->isReadOnly()) return;

  std::map<TFrameId, QString> imageSet;
  int i = 0;
  for (const TFrameId &frameId : frames) {
    QString id = "removeFrames" + QString::number((uintptr_t)sl) + "-" +
                 QString::number(frameId.getNumber());
    TImageCache::instance()->add(id, sl->getFrame(frameId, false));
    imageSet[frameId] = id;
  }
  HookSet *levelHooks = sl->getHookSet();
  auto *oldData       = new DrawingData();
  oldData->setFrames(imageSet, sl, *levelHooks);

  removeFramesWithoutUndo(sl, frames);
  TUndoManager::manager()->add(new RemoveFramesUndo(sl, frames, oldData));
}

//-----------------------------------------------------------------------------
namespace {

void insertEmptyFilmstripFrames(const TXshSimpleLevelP &sl,
                                const std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;
  makeSpaceForFids(sl.getPointer(), frames);
  for (const TFrameId &fid : frames) sl->setFrame(fid, sl->createEmptyFrame());
  invalidateIcons(sl.getPointer(), frames);
  sl->setDirtyFlag(true);
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
      : m_level(level)
      , m_frames(std::move(frames))
      , m_oldFrames(std::move(oldFrames)) {
    if (m_level->getType() == TZP_XSHLEVEL) {
      for (const TFrameId &fid : m_frames) {
        TToonzImageP img = m_level->getFrame(fid, true);
        TImageCache::instance()->add(
            "UndoInsertEmptyFrames" + QString::number((uintptr_t)this), img);
      }
    }
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  ~UndoInsertEmptyFrames() {
    TImageCache::instance()->remove("UndoInsertEmptyFrames" +
                                    QString::number((uintptr_t)this));
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
    if (m_level->getType() == PLI_XSHLEVEL) {
      FilmstripCmd::insert(m_level.getPointer(), m_frames, false);
    } else if (m_level->getType() == TZP_XSHLEVEL) {
      makeSpaceForFids(m_level.getPointer(), m_frames);
      TToonzImageP image = (TToonzImageP)TImageCache::instance()->get(
          "UndoInsertEmptyFrames" + QString::number((uintptr_t)this), true);
      if (!image) return;
      for (const TFrameId &fid : m_frames) m_level->setFrame(fid, image);
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

private:
  Q_DISABLE_COPY_MOVE(UndoInsertEmptyFrames);
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
        new UndoInsertEmptyFrames(sl, frames, std::move(oldFrames)));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {

void performReverse(const TXshSimpleLevelP &sl,
                    const std::set<TFrameId> &frames) {
  if (!sl || frames.empty()) return;

  std::vector<TFrameId> fids, oldFrames;
  sl->getFids(oldFrames);
  fids  = oldFrames;
  int i = 0, j = int(fids.size()) - 1;
  for (;;) {
    while (i < j && frames.count(fids[i]) == 0) ++i;
    while (i < j && frames.count(fids[j]) == 0) --j;
    if (i >= j) break;
    std::swap(fids[i], fids[j]);
    ++i;
    --j;
  }
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl.getPointer(), oldFrames, fids);
  }
  sl->renumber(fids);
  sl->setDirtyFlag(true);
  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//-----------------------------------------------------------------------------

class FilmstripReverseUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frames;

public:
  FilmstripReverseUndo(TXshSimpleLevelP level, std::set<TFrameId> frames)
      : m_level(std::move(level)), m_frames(std::move(frames)) {}

  void undo() const override { performReverse(m_level, m_frames); }
  void redo() const override { performReverse(m_level, m_frames); }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Reverse  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(FilmstripReverseUndo);
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

void performSwing(const TXshSimpleLevelP &sl,
                  const std::set<TFrameId> &frames) {
  if (!sl) return;
  int count = int(frames.size()) - 1;
  if (count <= 0) return;
  TFrameId lastFid     = *frames.rbegin();
  TFrameId insertPoint = lastFid + 1;
  std::set<TFrameId> framesToInsert;
  for (int i = 0; i < count; ++i) framesToInsert.insert(insertPoint + i);

  std::vector<TImageP> clonedImages;
  for (auto k = frames.rbegin(); k != frames.rend(); ++k) {
    TImageP img = sl->getFrame(*k, false);
    clonedImages.push_back(img ? img->cloneImage() : nullptr);
  }

  makeSpaceForFids(sl.getPointer(), framesToInsert);
  assert(count == (int)clonedImages.size());
  for (int i = 0; i < count; ++i) {
    TImage *img = clonedImages[i].getPointer();
    if (img) sl->setFrame(insertPoint + i, img);
  }
  invalidateIcons(sl.getPointer(), framesToInsert);
  sl->setDirtyFlag(true);
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
    int count = int(frames.size()) - 1;
    if (count <= 0) return;
    TFrameId lastFid     = *frames.rbegin();
    TFrameId insertPoint = lastFid + 1;
    for (int i = 0; i < count; ++i) m_newFrames.insert(insertPoint + i);
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
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Swing  : Level %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(FilmstripSwingUndo);
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

void stepFilmstripFrames(const TXshSimpleLevelP &sl,
                         const std::set<TFrameId> &frames, int step = 2) {
  if (!sl || frames.empty() || step < 2) return;
  std::vector<TFrameId> fids, oldFrames;
  sl->getFids(oldFrames);
  fids = oldFrames;
  std::set<TFrameId> changedFids;
  std::vector<int> insertIndices;
  int offset = 0;
  for (size_t i = 0; i < fids.size(); ++i) {
    bool frameToStep = (frames.count(fids[i]) > 0);
    if (offset > 0) {
      changedFids.insert(fids[i]);
      fids[i] = fids[i] + offset;
      changedFids.insert(fids[i]);
    }
    if (frameToStep) {
      insertIndices.push_back(int(i));
      offset += step - 1;
    }
  }
  if (Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled()) {
    updateXSheet(sl.getPointer(), oldFrames, fids);
  }
  sl->renumber(fids);
  for (int idx : insertIndices) {
    TFrameId fid = fids[idx];
    TImageP img  = sl->getFrame(fid, false);
    if (img) {
      for (int h = 1; h < step; ++h) {
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
    for (const TFrameId &fid : frames) {
      for (int j = 1; j < step; ++j) m_insertedFrames.insert(fid + (++d));
    }
    m_updateXSheet =
        Preferences::instance()->isSyncLevelRenumberWithXsheetEnabled();
  }

  void undo() const override {
    removeFramesWithoutUndo(m_level, m_insertedFrames);
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
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Step %1  : Level %2")
        .arg(QString::number(m_step))
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(StepFilmstripUndo);
};

}  // namespace

//=============================================================================
// step
//-----------------------------------------------------------------------------

void FilmstripCmd::step(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                        int step) {
  if (!sl || sl->isSubsequence() || sl->isReadOnly()) return;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  auto *undo = new StepFilmstripUndo(sl, frames, step);
  stepFilmstripFrames(sl, frames, step);
  TUndoManager::manager()->add(undo);
  frames.clear();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  QApplication::restoreOverrideCursor();
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {

std::map<TFrameId, QString> eachFilmstripFrames(
    const TXshSimpleLevelP &sl, const std::set<TFrameId> &frames, int each) {
  if (frames.empty() || !sl || each < 2) return {};
  std::vector<TFrameId> framesToDelete;
  int k = 0;
  for (const TFrameId &fid : frames) {
    if ((k % each) > 0) framesToDelete.push_back(fid);
    ++k;
  }
  std::map<TFrameId, QString> cutFrames;
  for (auto fit = framesToDelete.rbegin(); fit != framesToDelete.rend();
       ++fit) {
    TImageP img = sl->getFrame(*fit, false);
    if (img) {
      QString id = "eachFrames" + QString::number((uintptr_t)sl.getPointer()) +
                   "-" + QString::number(fit->getNumber());
      TImageCache::instance()->add(id, img);
      cutFrames[*fit] = id;
    }
    sl->eraseFrame(*fit);
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
      , m_cutFrames(std::move(deletedFrames))
      , m_each(each)
      , m_frames(frames) {}
  ~EachFilmstripUndo() {
    for (const auto &[_, id] : m_cutFrames) TImageCache::instance()->remove(id);
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
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Each %1  : Level %2")
        .arg(QString::number(m_each))
        .arg(QString::fromStdWString(m_level->getName()));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(EachFilmstripUndo);
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
      new EachFilmstripUndo(sl, each, frames, std::move(deletedFrames)));
  frames.clear();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

//-----------------------------------------------------------------------------
namespace {

class UndoDuplicateDrawing final : public TUndo {
  TXshSimpleLevelP m_level;
  std::set<TFrameId> m_frameInserted;
  std::vector<TFrameId> m_oldFrames;
  std::set<TFrameId> m_framesForRedo;
  bool m_updateXSheet;

public:
  UndoDuplicateDrawing(const TXshSimpleLevelP &level,
                       const std::vector<TFrameId> &oldFrames,
                       std::set<TFrameId> frameInserted,
                       std::set<TFrameId> framesForRedo)
      : m_level(level)
      , m_oldFrames(oldFrames)
      , m_frameInserted(std::move(frameInserted))
      , m_framesForRedo(std::move(framesForRedo)) {
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

private:
  Q_DISABLE_COPY_MOVE(UndoDuplicateDrawing);
};

}  // namespace

//=============================================================================
// duplicate
//-----------------------------------------------------------------------------

void FilmstripCmd::duplicateFrameWithoutUndo(TXshSimpleLevel *sl,
                                             TFrameId srcFrame,
                                             TFrameId targetFrame) {
  if (srcFrame.isNoFrame() || targetFrame.isNoFrame()) return;
  if (srcFrame.isEmptyFrame()) return;

  std::set<TFrameId> frames;
  frames.insert(srcFrame);
  auto *data = new DrawingData();
  data->setLevelFrames(sl, frames);

  frames.clear();
  frames.insert(targetFrame);

  bool keepOriginalPalette = true;
  pasteFramesWithoutUndo(data, sl, frames, DrawingData::OVER_SELECTION, true,
                         keepOriginalPalette);
  delete data;
}

//-----------------------------------------------------------------------------

void FilmstripCmd::duplicate(TXshSimpleLevel *sl, std::set<TFrameId> &frames,
                             bool withUndo) {
  if (frames.empty() || !sl || sl->isSubsequence() || sl->isReadOnly()) return;

  TFrameId insertPoint = (*frames.rbegin()) + 1;

  std::map<TFrameId, QString> framesToInsert;
  std::set<TFrameId> newFrames;
  int i = 0;
  for (const TFrameId &fid : frames) {
    TImageP img      = sl->getFrame(fid, false);
    TImageP imgClone = (img) ? img->cloneImage() : nullptr;
    QString id       = "dupFrames" + QString::number((uintptr_t)sl) + "-" +
                 QString::number(fid.getNumber());
    TImageCache::instance()->add(id, imgClone);
    framesToInsert[insertPoint + i] = id;
    newFrames.insert(insertPoint + i);
    ++i;
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

void moveToSceneFrames(TXshLevel *level, const std::set<TFrameId> &frames) {
  if (frames.empty() || !level) return;

  TXsheetHandle *xh = TApp::instance()->getCurrentXsheet();
  TXsheet *xsh      = xh->getXsheet();
  int row           = 0;
  int col           = xsh->getFirstFreeColumnIndex();
  if (level->getPaletteLevel()) {
    auto *column = new TXshPaletteColumn;
    xsh->insertColumn(col, column);
  }
  for (const TFrameId &fid : frames) {
    xsh->setCell(row, col, TXshCell(level, fid));
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
      : m_levelName(std::move(levelName))
      , m_col(col)
      , m_fids(std::move(fids)) {}

  void undo() const override {
    TApp *app         = TApp::instance();
    TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXshLevel *xl     = scene->getLevelSet()->getLevel(m_levelName);
    if (xl->getPaletteLevel()) xsh->removeColumn(m_col);
    xsh->clearCells(0, m_col, int(m_fids.size()));
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXshLevel *xl     = scene->getLevelSet()->getLevel(m_levelName);
    if (!xl) return;
    moveToSceneFrames(xl, m_fids);
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Move Level to Scene  : Level %1")
        .arg(QString::fromStdWString(m_levelName));
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(MoveLevelToSceneUndo);
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
  for (const TFrameId &fid : frames) {
    xsh->setCell(row, col, TXshCell(sl, fid));
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

  TXsheetHandle *xh = TApp::instance()->getCurrentXsheet();
  TXsheet *xsh      = xh->getXsheet();
  int row           = 0;
  int col           = xsh->getFirstFreeColumnIndex();
  auto *column      = new TXshPaletteColumn;
  xsh->insertColumn(col, column);
  for (const TFrameId &fid : fidsSet) {
    xsh->setCell(row, col, TXshCell(pl, fid));
    ++row;
  }
  xh->notifyXsheetChanged();
  TUndoManager::manager()->add(
      new MoveLevelToSceneUndo(pl->getName(), col, std::move(fidsSet)));
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
      : m_level(xl), m_fids(std::move(fids)), m_interpolation(interpolation) {
    for (const TFrameId &fid : m_fids) {
      m_images.push_back(m_level->getFrame(
          fid, false));  // no clone, level releases these images
    }
  }

  void undo() const override {
    for (size_t count = 1; count < m_fids.size(); ++count) {
      TVectorImageP vImage = m_images[count];
      m_level->setFrame(m_fids[count], vImage);
      IconGenerator::instance()->invalidate(m_level.getPointer(),
                                            m_fids[count]);
    }
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
  }

  void redo() const override {
    TFrameId fid0 = m_fids.front();
    TFrameId fid1 = m_fids.back();
    FilmstripCmd::inbetweenWithoutUndo(m_level.getPointer(), fid0, fid1,
                                       m_interpolation);
  }

  int getSize() const override {
    assert(!m_images.empty());
    return int(m_images.size() * m_images.front()->getStrokeCount() * 100);
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Inbetween  : Level %1,  ")
                      .arg(QString::fromStdWString(m_level->getName()));
    switch (m_interpolation) {
    case FilmstripCmd::II_Linear:
      str += QString("Linear Interpolation");
      break;
    case FilmstripCmd::II_EaseIn:
      str += QString("Ease In Interpolation");
      break;
    case FilmstripCmd::II_EaseOut:
      str += QString("Ease Out Interpolation");
      break;
    case FilmstripCmd::II_EaseInOut:
      str += QString("Ease In-Out Interpolation");
      break;
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::FilmStrip; }

private:
  Q_DISABLE_COPY_MOVE(UndoInbetween);
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
  auto it0 = std::find(fids.begin(), fids.end(), fid0);
  if (it0 == fids.end()) return;
  int ia   = int(std::distance(fids.begin(), it0));
  auto it1 = std::find(fids.begin(), fids.end(), fid1);
  if (it1 == fids.end()) return;
  int ib = int(std::distance(fids.begin(), it1));
  if (ib - ia < 2) return;

  TVectorImageP img0 = sl->getFrame(fid0, false);
  TVectorImageP img1 = sl->getFrame(fid1, false);
  if (!img0 || !img1) return;

  TInbetween::TweenAlgorithm algorithm;
  switch (interpolation) {
  case II_Linear:
    algorithm = TInbetween::LinearInterpolation;
    break;
  case II_EaseIn:
    algorithm = TInbetween::EaseInInterpolation;
    break;
  case II_EaseOut:
    algorithm = TInbetween::EaseOutInterpolation;
    break;
  case II_EaseInOut:
    algorithm = TInbetween::EaseInOutInterpolation;
    break;
  }

  TInbetween inbetween(img0, img1);
  for (int i = ia + 1; i < ib; ++i) {
    double t         = double(i - ia) / double(ib - ia);
    double s         = TInbetween::interpolation(t, algorithm);
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
  for (const TFrameId &fid : levelFids) {
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
  std::vector<TFrameId> fids, oldFrames;
  sl->getFids(oldFrames);
  fids    = oldFrames;
  auto it = std::find(fids.begin(), fids.end(), oldFid);
  if (it == fids.end()) return;
  TFrameId newFid = desiredNewFid;
  while (std::find(fids.begin(), fids.end(), newFid) != fids.end()) {
    QString nextLetter = getNextLetter(newFid.getLetter());
    if (nextLetter.isEmpty()) return;
    newFid = TFrameId(newFid.getNumber(), nextLetter);
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
