

#include "toonz/studiopalettecmd.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "tundo.h"
#include "tcolorstyles.h"
#include "tsystem.h"
#include "tconvert.h"
#include "ttoonzimage.h"
#include "timagecache.h"
#include "tmsgcore.h"

#include "toonz/studiopalette.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzfolders.h"
#include "toonz/txsheet.h"
#include <QApplication>

#include "historytypes.h"

/*! \namespace StudioPaletteCmd
                \brief Provides a collection of methods to manage \b
   StudioPalette.
*/

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// PaletteAssignUndo : Undo for the "Load into Current Palette" command.

class PaletteAssignUndo final : public TUndo {
  TPaletteP m_targetPalette, m_oldPalette, m_newPalette;
  TPaletteHandle *m_paletteHandle;

public:
  PaletteAssignUndo(const TPaletteP &targetPalette, const TPaletteP &oldPalette,
                    const TPaletteP &newPalette, TPaletteHandle *paletteHandle)
      : m_targetPalette(targetPalette)
      , m_oldPalette(oldPalette)
      , m_newPalette(newPalette)
      , m_paletteHandle(paletteHandle) {}

  void undo() const override {
    m_targetPalette->assign(m_oldPalette.getPointer());
    m_paletteHandle->notifyPaletteChanged();
  }
  void redo() const override {
    m_targetPalette->assign(m_newPalette.getPointer());
    m_paletteHandle->notifyPaletteChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           (m_targetPalette->getStyleCount() + m_oldPalette->getStyleCount() +
            m_newPalette->getStyleCount()) *
               100;
  }

  QString getHistoryString() override {
    return QObject::tr("Load into Current Palette  > %1")
        .arg(QString::fromStdWString(m_targetPalette->getPaletteName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// StudioPaletteAssignUndo : Undo for the "Replace with Current Palette"
// command.

class StudioPaletteAssignUndo final : public TUndo {
  TPaletteP m_oldPalette, m_newPalette;
  TFilePath m_fp;
  TPaletteHandle *m_paletteHandle;

public:
  StudioPaletteAssignUndo(const TFilePath &targetPath,
                          const TPaletteP &oldPalette,
                          const TPaletteP &newPalette,
                          TPaletteHandle *paletteHandle)
      : m_fp(targetPath)
      , m_oldPalette(oldPalette)
      , m_newPalette(newPalette)
      , m_paletteHandle(paletteHandle) {}

  void undo() const override {
    StudioPalette *sp = StudioPalette::instance();
    sp->setPalette(m_fp, m_oldPalette.getPointer(), true);
    m_paletteHandle->notifyPaletteChanged();
  }
  void redo() const override {
    StudioPalette *sp = StudioPalette::instance();
    sp->setPalette(m_fp, m_newPalette.getPointer(), true);
    m_paletteHandle->notifyPaletteChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           (m_oldPalette->getStyleCount() + m_newPalette->getStyleCount()) *
               100;
  }

  QString getHistoryString() override {
    return QObject::tr("Replace with Current Palette  > %1")
        .arg(QString::fromStdString(m_fp.getLevelName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// DeletePaletteUndo

class DeletePaletteUndo final : public TUndo {
  TFilePath m_palettePath;
  TPaletteP m_palette;

public:
  DeletePaletteUndo(const TFilePath &palettePath) : m_palettePath(palettePath) {
    m_palette = StudioPalette::instance()->getPalette(m_palettePath);
  }

  void undo() const override {
    StudioPalette::instance()->setPalette(m_palettePath, m_palette->clone(),
                                          true);
  }
  void redo() const override {
    StudioPalette::instance()->deletePalette(m_palettePath);
  }
  int getSize() const override { return sizeof(*this) + sizeof(TPalette); }
  QString getHistoryString() override {
    return QObject::tr("Delete Studio Palette  : %1")
        .arg(QString::fromStdString(m_palettePath.getLevelName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// CreatePaletteUndo

class CreatePaletteUndo final : public TUndo {
  TFilePath m_palettePath;
  TPaletteP m_palette;

public:
  CreatePaletteUndo(const TFilePath &palettePath) : m_palettePath(palettePath) {
    m_palette = StudioPalette::instance()->getPalette(m_palettePath);
  }

  void undo() const override {
    StudioPalette::instance()->deletePalette(m_palettePath);
  }
  void redo() const override {
    StudioPalette::instance()->setPalette(m_palettePath, m_palette->clone(),
                                          true);
  }
  int getSize() const override { return sizeof(*this) + sizeof(TPalette); }
  QString getHistoryString() override {
    return QObject::tr("Create Studio Palette  : %1")
        .arg(QString::fromStdString(m_palettePath.getLevelName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// DeleteFolderUndo

class DeleteFolderUndo final : public TUndo {
  TFilePath m_path;
  TFilePathSet m_pathSet;
  QList<TPaletteP> m_paletteList;

public:
  DeleteFolderUndo(const TFilePath &path, const TFilePathSet &pathSet)
      : m_path(path), m_pathSet(pathSet), m_paletteList() {
    for (TFilePathSet::const_iterator it = m_pathSet.begin();
         it != m_pathSet.end(); it++) {
      TFilePath path = *it;
      if (path.getType() == "tpl")
        m_paletteList.push_back(StudioPalette::instance()->getPalette(path));
    }
  }

  void undo() const override {
    StudioPalette::instance()->createFolder(m_path.getParentDir(),
                                            m_path.getWideName());
    int paletteCount = -1;
    for (TFilePathSet::const_iterator it = m_pathSet.begin();
         it != m_pathSet.end(); it++) {
      TFilePath path = *it;
      if (path.getType() == "tpl")  // Is a palette
        StudioPalette::instance()->setPalette(
            path, m_paletteList.at(++paletteCount)->clone(), true);
      else  // Is a folder
        StudioPalette::instance()->createFolder(path.getParentDir(),
                                                path.getWideName());
    }
  }
  void redo() const override {
    StudioPalette::instance()->deleteFolder(m_path);
  }
  int getSize() const override { return sizeof(*this) + sizeof(TPalette); }
  QString getHistoryString() override {
    return QObject::tr("Delete Studio Palette Folder  : %1")
        .arg(QString::fromStdString(m_path.getName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// CreateFolderUndo

class CreateFolderUndo final : public TUndo {
  TFilePath m_folderPath;

public:
  CreateFolderUndo(const TFilePath &folderPath) : m_folderPath(folderPath) {}

  void undo() const override {
    StudioPalette::instance()->deleteFolder(m_folderPath);
  }
  void redo() const override {
    StudioPalette::instance()->createFolder(m_folderPath.getParentDir(),
                                            m_folderPath.getWideName());
  }
  int getSize() const override { return sizeof(*this) + sizeof(TPalette); }
  QString getHistoryString() override {
    return QObject::tr("Create Studio Palette Folder  : %1")
        .arg(QString::fromStdString(m_folderPath.getName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//=============================================================================
// MovePaletteUndo

class MovePaletteUndo final : public TUndo {
  TFilePath m_dstPath, m_srcPath;
  bool m_isRename;

public:
  MovePaletteUndo(const TFilePath &dstPath, const TFilePath &srcPath)
      : m_dstPath(dstPath), m_srcPath(srcPath) {
    m_isRename = (m_srcPath.getParentDir() == m_dstPath.getParentDir());
  }

  void undo() const override {
    QString errorStr = (m_isRename) ? QObject::tr("Can't undo rename palette")
                                    : QObject::tr("Can't undo move palette");
    try {
      StudioPalette::instance()->movePalette(m_srcPath, m_dstPath);
    } catch (TException &e) {
      DVGui::error(errorStr + " : " +
                   QString(::to_string(e.getMessage()).c_str()));
    } catch (...) {
      DVGui::error(errorStr);
    }
  }

  void redo() const override {
    QString errorStr = (m_isRename) ? QObject::tr("Can't redo rename palette")
                                    : QObject::tr("Can't redo move palette");
    try {
      StudioPalette::instance()->movePalette(m_dstPath, m_srcPath);
    } catch (TException &e) {
      DVGui::error(errorStr + ":" +
                   QString(::to_string(e.getMessage()).c_str()));
    } catch (...) {
      DVGui::error(errorStr);
    }
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    if (m_isRename)
      return QObject::tr("Rename Studio Palette : %1 > %2")
          .arg(QString::fromStdString(m_srcPath.getName()))
          .arg(QString::fromStdString(m_dstPath.getName()));
    else
      return QObject::tr("Move Studio Palette Folder  : %1 : %2 > %3")
          .arg(QString::fromStdString(m_srcPath.getName()))
          .arg(QString::fromStdString(m_srcPath.getParentDir().getName()))
          .arg(QString::fromStdString(m_dstPath.getParentDir().getName()));
  }
  int getHistoryType() override { return HistoryType::Palette; }
};

//-------------------------------------------------------------
void adaptLevelToPalette(TXshLevelHandle *currentLevelHandle,
                         TPaletteHandle *paletteHandle, TPalette *plt,
                         int tolerance, bool noUndo);

class AdjustIntoCurrentPaletteUndo final : public TUndo {
  TXshLevelHandle *m_currentLevelHandle;
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_oldPalette, m_newPalette;
  TFrameId m_fid;
  std::string m_oldImageId;
  static int m_idCount;
  int m_undoSize;
  int m_tolerance;

public:
  AdjustIntoCurrentPaletteUndo(TXshLevelHandle *currentLevelHandle,
                               TPaletteHandle *paletteHandle,
                               const TFrameId &fid, const TImageP &oldImage,
                               const TPaletteP &oldPalette,
                               const TPaletteP &newPalette, int tolerance)
      : TUndo()
      , m_currentLevelHandle(currentLevelHandle)
      , m_paletteHandle(paletteHandle)
      , m_fid(fid)
      , m_oldPalette(oldPalette)
      , m_newPalette(newPalette)
      , m_tolerance(tolerance) {
    m_oldImageId =
        "AdjustIntoCurrentPaletteUndo_oldImage_" + std::to_string(m_idCount++);

    TImageCache::instance()->add(m_oldImageId, (const TImageP)oldImage);

    m_undoSize = ((TToonzImageP)oldImage)->getRaster()->getLx() *
                 ((TToonzImageP)oldImage)->getRaster()->getLy() *
                 ((TToonzImageP)oldImage)->getRaster()->getPixelSize();
  }

  ~AdjustIntoCurrentPaletteUndo() {
    TImageCache::instance()->remove(m_oldImageId);
  }

  void undo() const override {
    TImageP img            = TImageCache::instance()->get(m_oldImageId, true);
    TXshSimpleLevel *level = m_currentLevelHandle->getSimpleLevel();
    level->setPalette(m_oldPalette.getPointer());
    // img->setPalette(m_oldPalette.getPointer());
    level->setFrame(m_fid, img->cloneImage());

    level->touchFrame(m_fid);
    if (level->getFirstFid() == m_fid) {
      m_currentLevelHandle->notifyLevelChange();
      m_paletteHandle->setPalette(m_oldPalette.getPointer());

      m_oldPalette->setDirtyFlag(true);
      m_paletteHandle->notifyPaletteChanged();
    }
  }

  void redo() const override {
    adaptLevelToPalette(m_currentLevelHandle, m_paletteHandle,
                        m_newPalette.getPointer(), m_tolerance, true);
  }

  int getSize() const override { return m_undoSize; }
};

//-----------------------------------------------------------------------------
int AdjustIntoCurrentPaletteUndo::m_idCount = 0;

//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

void StudioPaletteCmd::loadIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                              const TFilePath &fp) {
  TPalette *palette = StudioPalette::instance()->getPalette(fp, false);
  if (!palette) return;

  loadIntoCurrentPalette(paletteHandle, palette);
}

//-----------------------------------------------------------------------------
namespace {

std::map<TPixel, int> ToleranceMap;

int findClosest(const TPixel &color, std::map<TPixel, int> &colorMap,
                int tolerance) {
  std::map<TPixel, int>::const_iterator it;
  it = ToleranceMap.find(color);
  if (it != ToleranceMap.end()) return it->second;

  tolerance *= tolerance;

  it        = colorMap.begin();
  int index = -1, minDist = 99999999;
  int dr, dg, db, dm;

  for (; it != colorMap.end(); ++it) {
    if ((dr = (color.r - it->first.r) * (color.r - it->first.r)) > tolerance)
      continue;
    if ((dg = (color.g - it->first.g) * (color.g - it->first.g)) > tolerance)
      continue;
    if ((db = (color.b - it->first.b) * (color.b - it->first.b)) > tolerance)
      continue;
    if ((dm = (color.m - it->first.m) * (color.m - it->first.m)) > tolerance)
      continue;

    int currDist                    = dr + dg + db + dm;
    if (currDist < minDist) minDist = currDist, index = it->second;
  }
  ToleranceMap[color] = index;
  return index;
}

//--------------------------------------------------------------------------------------

int getIndex(const TPixel &color, std::map<TPixel, int> &colorMap,
             TPalette *plt, int tolerance) {
  std::map<TPixel, int>::const_iterator it;
  it = colorMap.find(color);
  if (it != colorMap.end()) return it->second;

  if (tolerance > 0) {
    int index = findClosest(color, colorMap, tolerance);
    if (index != -1) return index;
  }

  int index = plt->addStyle(color);
  plt->getPage(0)->addStyle(index);
  colorMap[color] = index;
  return index;
}

//-------------------------------------------

void adaptIndexes(TToonzImageP timg, std::map<TPixel, int> &colorMap,
                  TPalette *plt, int tolerance) {
  TPalette *origPlt  = timg->getPalette();
  TRasterCM32P r     = timg->getRaster();
  TPixelCM32 oldPrev = TPixelCM32(), newPrev = TPixelCM32();

  for (int i = 0; i < r->getLy(); i++) {
    TPixelCM32 *pix = r->pixels(i);
    for (int j = 0; j < r->getLx(); j++, pix++) {
      if (*pix == TPixelCM32()) continue;
      if (*pix == oldPrev) {
        *pix = newPrev;
        continue;
      }
      oldPrev   = *pix;
      int ink   = pix->getInk();
      int paint = pix->getPaint();
      if (ink > 0) {
        TPixel color = origPlt->getStyle(ink)->getMainColor();
        int newIndex = getIndex(color, colorMap, plt, tolerance);
        pix->setInk(newIndex);
      }
      if (paint > 0) {
        TPixel color = origPlt->getStyle(paint)->getMainColor();
        int newIndex = getIndex(color, colorMap, plt, tolerance);
        pix->setPaint(newIndex);
      }
      newPrev = *pix;
    }
  }
}

//-----------------------------------------------

void adaptLevelToPalette(TXshLevelHandle *currentLevelHandle,
                         TPaletteHandle *paletteHandle, TPalette *plt,
                         int tolerance, bool noUndo) {
  TXshSimpleLevel *sl = currentLevelHandle->getSimpleLevel();
  QApplication::setOverrideCursor(Qt::WaitCursor);

  TPalette *oldPalette = sl->getPalette();

  ToleranceMap.clear();

  std::map<TPixel, int> colorMap;
  for (int i = 0; i < plt->getStyleCount(); i++) {
    if (!plt->getStylePage(i)) continue;
    colorMap[plt->getStyle(i)->getMainColor()] = i;
  }
  std::vector<TFrameId> fids;

  sl->getFids(fids);
  for (int i = 0; i < (int)fids.size(); i++) {
    TToonzImageP timg = (TToonzImageP)sl->getFrame(fids[i], true);
    if (!timg) continue;
    if (!noUndo)
      TUndoManager::manager()->add(new AdjustIntoCurrentPaletteUndo(
          currentLevelHandle, paletteHandle, fids[i], timg->cloneImage(),
          oldPalette->clone(), plt->clone(), tolerance));

    adaptIndexes(timg, colorMap, plt, tolerance);
    timg->setPalette(plt);
  }
  QApplication::restoreOverrideCursor();

  currentLevelHandle->getSimpleLevel()->setPalette(plt);
  paletteHandle->setPalette(plt);
  plt->setDirtyFlag(true);
  paletteHandle->notifyPaletteChanged();
  currentLevelHandle->notifyLevelChange();
}

}  // namespaxce

//-----------------------------------------------------------------------------

void StudioPaletteCmd::loadIntoCurrentPalette(
    TPaletteHandle *paletteHandle, TPalette *plt,
    TXshLevelHandle *currentLevelHandle, int tolerance) {
  adaptLevelToPalette(currentLevelHandle, paletteHandle, plt, tolerance, false);
}

//---------------------------------------------------------------------------------------

void StudioPaletteCmd::loadIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                              TPalette *palette) {
  assert(paletteHandle);
  TPalette *current = paletteHandle->getPalette();
  if (!current) return;

  int styleId = paletteHandle->getStyleIndex();

  TPalette *old = current->clone();
  while (palette->getStyleCount() < current->getStyleCount()) {
    int index = palette->getStyleCount();
    assert(index < current->getStyleCount());
    TColorStyle *style = current->getStyle(index)->clone();
    palette->addStyle(style);
  }
  // keep the color model path unchanged
  TFilePath oldRefImagePath = current->getRefImgPath();

  std::wstring oldGlobalName = current->getGlobalName();
  current->assign(palette, true);
  // keep global name unchanged
  current->setGlobalName(oldGlobalName);

  current->setDirtyFlag(true);

  current->setRefImgPath(oldRefImagePath);

  if (paletteHandle->getPalette() == current &&
      styleId >= current->getStyleCount())
    paletteHandle->setStyleIndex(1);
  TUndoManager::manager()->add(
      new PaletteAssignUndo(current, old, current->clone(), paletteHandle));

  palette->setDirtyFlag(true);

  paletteHandle->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::mergeIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                               const TFilePath &fp) {
  TPalette *palette = StudioPalette::instance()->getPalette(fp);

  if (!palette) return;

  mergeIntoCurrentPalette(paletteHandle, palette);
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::mergeIntoCurrentPalette(TPaletteHandle *paletteHandle,
                                               TPalette *palette) {
  assert(paletteHandle);
  TPalette *current = paletteHandle->getPalette();

  if (!current) return;
  if (current->isLocked()) return;

  TPalette *old = current->clone();
  current->merge(palette, true);
  TUndoManager::manager()->add(
      new PaletteAssignUndo(current, old, current->clone(), paletteHandle));

  palette->setDirtyFlag(true);
  paletteHandle->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::replaceWithCurrentPalette(
    TPaletteHandle *paletteHandle, TPaletteHandle *stdPaletteHandle,
    const TFilePath &fp) {
  StudioPalette *sp = StudioPalette::instance();
  TPalette *palette = sp->getPalette(fp);

  if (!palette || palette->isLocked()) return;

  assert(paletteHandle);
  TPalette *current = paletteHandle->getPalette();

  if (!current) return;
  // keep the studio palette's global name unchanged
  std::wstring oldGlobalName = palette->getGlobalName();

  TPalette *old = palette->clone();
  palette->assign(current);

  // put back the global name
  palette->setGlobalName(oldGlobalName);

  sp->setPalette(fp, current, true);
  TUndoManager::manager()->add(
      new StudioPaletteAssignUndo(fp, old, current->clone(), paletteHandle));

  // Cambio la studioPalette correntette(palette);
  stdPaletteHandle->setPalette(palette);

  stdPaletteHandle->notifyPaletteSwitched();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::updateAllLinkedStyles(TPaletteHandle *paletteHandle,
                                             TXsheetHandle *xsheetHandle) {
  if (!xsheetHandle) return;
  TXsheet *xsheet = xsheetHandle->getXsheet();
  if (!xsheet) return;
  ToonzScene *scene = xsheet->getScene();
  if (!scene) return;

  // emit signal only if something changed
  bool somethingChanged = false;

  StudioPalette *sp   = StudioPalette::instance();
  TLevelSet *levelSet = scene->getLevelSet();
  for (int i = 0; i < levelSet->getLevelCount(); i++) {
    TXshLevel *xl       = levelSet->getLevel(i);
    TXshSimpleLevel *sl = xl ? xl->getSimpleLevel() : 0;
    if (!sl) continue;
    TPalette *palette = sl->getPalette();
    if (palette) {
      somethingChanged = somethingChanged | sp->updateLinkedColors(palette);
      if (sl->getType() == TZP_XSHLEVEL) {
        std::vector<TFrameId> fids;
        sl->getFids(fids);
        std::vector<TFrameId>::iterator it;
        for (it = fids.begin(); it != fids.end(); ++it) {
          TFrameId fid   = *it;
          std::string id = sl->getImageId(fid);
        }
      }
    }
  }
  if (!paletteHandle || !paletteHandle->getPalette()) return;
  if (somethingChanged) paletteHandle->notifyColorStyleChanged();
}

//-----------------------------------------------------------------------------
/*! Delete palette identified by \b fp in \b StudioPalette. If there are any
                problems in loading send an error message.
*/
void StudioPaletteCmd::deletePalette(const TFilePath &fp) {
  TUndo *undo = new DeletePaletteUndo(fp);
  StudioPalette::instance()->deletePalette(fp);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------
/*! Move palette from TFilePath \b srcPath to \b TFilePath \b dstPath. If there
                are any problems in moving send an error message.
*/
void StudioPaletteCmd::movePalette(const TFilePath &dstPath,
                                   const TFilePath &srcPath) {
  TSystem::touchParentDir(dstPath);
  StudioPalette::instance()->movePalette(dstPath, srcPath);
  TUndoManager::manager()->add(new MovePaletteUndo(dstPath, srcPath));
}

//-----------------------------------------------------------------------------
/*! Create palette \b palette in folder \b folderName with name \b paletteName.
                If there are any problems send an error message.
*/
TFilePath StudioPaletteCmd::createPalette(const TFilePath &folderName,
                                          std::string paletteName,
                                          const TPalette *palette) {
  TFilePath palettePath;
  TFileStatus status(folderName);
  if (!status.isDirectory()) throw TException("Select a folder.");
  if (!status.doesExist()) {
    TSystem::mkDir(folderName);
    FolderListenerManager::instance()->notifyFolderChanged(
        folderName.getParentDir());
  }
  palettePath =
      StudioPalette::instance()->createPalette(folderName, paletteName);
  if (palette)
    StudioPalette::instance()->setPalette(palettePath, palette, true);
  TUndoManager::manager()->add(new CreatePaletteUndo(palettePath));
  return palettePath;
}

//-----------------------------------------------------------------------------
/*! Add a folder in StudioPalette TFilePath \b parentFolderPath. If there
                are any problems send an error message.
*/
TFilePath StudioPaletteCmd::addFolder(const TFilePath &parentFolderPath) {
  TFilePath folderPath;
  folderPath = StudioPalette::instance()->createFolder(parentFolderPath);
  if (!folderPath.isEmpty())
    TUndoManager::manager()->add(new CreateFolderUndo(folderPath));
  return folderPath;
}

//-----------------------------------------------------------------------------
/*! Delete folder \b folderPath. If there are any problems in deleting send an
                error message.
*/
void StudioPaletteCmd::deleteFolder(const TFilePath &folderPath) {
  TFilePathSet folderPathSet;
  TSystem::readDirectoryTree(folderPathSet, folderPath, true, false);
  TUndo *undo = new DeleteFolderUndo(folderPath, folderPathSet);
  StudioPalette::instance()->deleteFolder(folderPath);
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::scanPalettes(const TFilePath &folder,
                                    const TFilePath &sourcePath) {
  //  error("uh oh");
}
