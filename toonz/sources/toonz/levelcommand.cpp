
#include "levelcommand.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "filmstripselection.h"
#include "castselection.h"
#include "cellselection.h"
#include "timagecache.h"

#include "toonz/txshsimplelevel.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/txshcell.h"
#include "toonz/childstack.h"
#include "toonz/txshchildlevel.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"

#include "tundo.h"
#include "tconvert.h"
#include "tlevel_io.h"
#include "ttoonzimage.h"
#include "tsystem.h"

#include "toonzqt/gutil.h"
#include "toonz/namebuilder.h"

namespace {

class DeleteLevelUndo final : public TUndo {
  TXshLevelP m_xl;

public:
  DeleteLevelUndo(TXshLevel *xl) : m_xl(xl) {}

  void undo() const override {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    scene->getLevelSet()->insertLevel(m_xl.getPointer());
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }
  void redo() const override {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    scene->getLevelSet()->removeLevel(m_xl.getPointer());
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }

  int getSize() const override { return sizeof *this + 100; }

  QString getHistoryString() override {
    return QObject::tr("Delete Level  : %1")
        .arg(QString::fromStdWString(m_xl->getName()));
  }
};

}  // namespace

//-----------------------------------------------------------------------------

bool LevelCmd::removeUnusedLevelsFromCast(bool showMessage) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  TLevelSet *levelSet = scene->getLevelSet();

  std::set<TXshLevel *> usedLevels;
  scene->getTopXsheet()->getUsedLevels(usedLevels);

  std::vector<TXshLevel *> unused;

  for (int i = 0; i < levelSet->getLevelCount(); i++) {
    TXshLevel *xl = levelSet->getLevel(i);
    if (usedLevels.count(xl) == 0) unused.push_back(xl);
  }
  if (unused.empty()) {
    if (showMessage) DVGui::error(QObject::tr("No unused levels"));
    return false;
  } else {
    TUndoManager *um = TUndoManager::manager();
    um->beginBlock();
    for (int i = 0; i < (int)unused.size(); i++) {
      TXshLevel *xl = unused[i];
      um->add(new DeleteLevelUndo(xl));
      scene->getLevelSet()->removeLevel(xl);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifyCastChange();

    um->endBlock();
  }
  return true;
}

bool LevelCmd::removeLevelFromCast(TXshLevel *level, ToonzScene *scene,
                                   bool showMessage) {
  if (!scene) scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene->getChildStack()->getTopXsheet()->isLevelUsed(level)) {
    if (showMessage) {
      DVGui::error(
          QObject::tr("It is not possible to delete the used level %1.")
              .arg(QString::fromStdWString(
                  level->getName())));  //"E_CantDeleteUsedLevel_%1"
    }
    return false;
  } else {
    TUndoManager *um = TUndoManager::manager();
    um->add(new DeleteLevelUndo(level));
    scene->getLevelSet()->removeLevel(level);
  }
  return true;
}

//=============================================================================
// RemoveUnusedLevelCommand
//-----------------------------------------------------------------------------

class RemoveUnusedLevelsCommand final : public MenuItemHandler {
public:
  RemoveUnusedLevelsCommand() : MenuItemHandler(MI_RemoveUnused) {}

  void execute() override { LevelCmd::removeUnusedLevelsFromCast(); }
} removeUnusedLevelsCommand;

//=============================================================================
// RemoveLevelCommand
//-----------------------------------------------------------------------------

class RemoveLevelCommand final : public MenuItemHandler {
public:
  RemoveLevelCommand() : MenuItemHandler(MI_RemoveLevel) {}

  void execute() override {
    TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
    CastSelection *castSelection =
        dynamic_cast<CastSelection *>(TSelection::getCurrent());
    if (!castSelection) return;

    std::vector<TXshLevel *> levels;
    castSelection->getSelectedLevels(levels);
    if (levels.empty()) {
      DVGui::error("No level selected");  // E_NoSelectedLevel
      return;
    }
    int count = 0;
    for (int i = 0; i < (int)levels.size(); i++)
      if (LevelCmd::removeLevelFromCast(levels[i])) count++;
    if (count == 0) return;
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->notifyCastChange();
  }

} removeLevelCommand;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

TFilePath getUnpaintedLevelPath(TXshSimpleLevel *simpleLevel) {
  ToonzScene *scene   = simpleLevel->getScene();
  TFilePath levelPath = scene->decodeFilePath(simpleLevel->getPath());
  if (levelPath.isEmpty()) return TFilePath();
  std::string name = levelPath.getName() + "_np." + levelPath.getType();
  return levelPath.getParentDir() + "nopaint\\" + TFilePath(name);
}

//-----------------------------------------------------------------------------

void getLevelSelectedFids(std::set<TFrameId> &fids, TXshSimpleLevel *level,
                          int r0, int c0, int r1, int c1) {
  TXsheet *xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  int c, r;
  for (c = c0; c <= c1; c++)
    for (r = r0; r <= r1; r++) {
      TXshCell cell             = xsheet->getCell(r, c);
      TXshSimpleLevel *curLevel = (!cell.isEmpty()) ? cell.getSimpleLevel() : 0;
      if (curLevel != level) continue;
      fids.insert(cell.getFrameId());
    }
}

//-----------------------------------------------------------------------------

bool loadFids(const TFilePath &path, TXshSimpleLevel *sl,
              const std::set<TFrameId> &selectedFids) {
  assert(sl && !selectedFids.empty());

  TLevelReaderP levelReader = TLevelReaderP(path);
  if (!levelReader.getPointer()) return false;

  // Carico il livello e sostituisco i frames.
  TLevelP level = levelReader->loadInfo();
  if (!level || level->getFrameCount() == 0) return false;
  TLevel::Iterator levelIt         = level->begin();
  bool almostOneUnpaintedFidLoaded = false;
  for (; levelIt != level->end(); ++levelIt) {
    TFrameId fid                          = levelIt->first;
    std::set<TFrameId>::const_iterator it = selectedFids.find(fid);
    if (it == selectedFids.end()) continue;
    TImageP img = levelReader->getFrameReader(fid)->load();
    if (!img.getPointer()) continue;
    almostOneUnpaintedFidLoaded = true;
    sl->setFrame(fid, img);
  }
  if (almostOneUnpaintedFidLoaded) {
    invalidateIcons(sl, selectedFids);
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

bool loadUnpaintedFids(TXshSimpleLevel *sl,
                       const std::set<TFrameId> &selectedFids) {
  TFilePath path = getUnpaintedLevelPath(sl);

  if (!TSystem::doesExistFileOrLevel(path)) {
    DVGui::error(QObject::tr(
        "No cleaned up drawings available for the current selection."));
    return false;
  }

  return loadFids(path, sl, selectedFids);
}

//-----------------------------------------------------------------------------

bool loadLastSaveFids(TXshSimpleLevel *sl,
                      const std::set<TFrameId> &selectedFids) {
  TFilePath path = sl->getPath();
  path           = sl->getScene()->decodeFilePath(path);

  if (!TSystem::doesExistFileOrLevel(path)) {
    DVGui::error(
        QObject::tr("No saved drawings available for the current selection."));
    return false;
  }

  return loadFids(path, sl, selectedFids);
}

//=============================================================================
// Undo RevertToCommandUndo
//-----------------------------------------------------------------------------

class RevertToCommandUndo final : public TUndo {
  TXshSimpleLevel *m_sl;
  std::vector<QString> m_replacedImgsId;
  std::set<TFrameId> m_selectedFids;
  bool m_isCleanedUp;

public:
  RevertToCommandUndo(TXshSimpleLevel *sl, std::set<TFrameId> &selectedFids,
                      bool isCleanedUp)
      : m_sl(sl), m_selectedFids(selectedFids), m_isCleanedUp(isCleanedUp) {
    static int revertToCommandCount = 0;
    for (auto const &fid : m_selectedFids) {
      if (!sl->isFid(fid)) continue;
      TImageP image = sl->getFrame(fid, false);
      assert(image);
      QString newImageId = "RevertToUndo" +
                           QString::number(revertToCommandCount) + "-" +
                           QString::number(fid.getNumber());
      TImageCache::instance()->add(newImageId, image->cloneImage());
      m_replacedImgsId.push_back(newImageId);
    }
    revertToCommandCount++;
  }

  ~RevertToCommandUndo() {
    int i;
    for (i = 0; i < (int)m_replacedImgsId.size(); i++)
      TImageCache::instance()->remove(m_replacedImgsId[i]);
  }

  void undo() const override {
    assert((int)m_replacedImgsId.size() == (int)m_selectedFids.size());
    int i = 0;
    for (auto const &fid : m_selectedFids) {
      QString imageId = m_replacedImgsId[i];
      TImageP img = TImageCache::instance()->get(imageId, false)->cloneImage();
      if (!img.getPointer()) continue;
      m_sl->setFrame(fid, img);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    invalidateIcons(m_sl, m_selectedFids);
  }

  void redo() const override {
    if (m_isCleanedUp)
      loadUnpaintedFids(m_sl, m_selectedFids);
    else
      loadLastSaveFids(m_sl, m_selectedFids);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof(*this) + m_selectedFids.size() * sizeof(TFrameId);
  }

  QString getHistoryString() override {
    return QObject::tr("Revert To %1  : Level %2")
        .arg((m_isCleanedUp) ? QString("Cleaned Up") : QString("Last Saved"))
        .arg(QString::fromStdWString(m_sl->getName()));
  }
};

//-----------------------------------------------------------------------------
/*--isCleanedUpが	Trueのとき： "revert to cleaned up" コマンド
                                        Falseのとき："revert to last saved"
コマンド
--*/
void revertTo(bool isCleanedUp) {
  TApp *app = TApp::instance();

  TFilmstripSelection *filmstripSelection =
      dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());

  /*-- FilmStrip選択の場合 --*/
  if (filmstripSelection) {
    std::set<TFrameId> selectedFids = filmstripSelection->getSelectedFids();
    TXshSimpleLevel *sl             = app->getCurrentLevel()->getSimpleLevel();
    if (!sl || selectedFids.empty()) {
      DVGui::error(QObject::tr("The current selection is invalid."));
      return;
    }
    RevertToCommandUndo *undo =
        new RevertToCommandUndo(sl, selectedFids, isCleanedUp);
    bool commandExecuted = false;
    if (isCleanedUp)
      commandExecuted = loadUnpaintedFids(sl, selectedFids);
    else
      commandExecuted = loadLastSaveFids(sl, selectedFids);
    if (!commandExecuted)
      delete undo;
    else {
      TUndoManager::manager()->add(undo);
      if (isCleanedUp) sl->setDirtyFlag(true);
    }
    app->getCurrentLevel()->notifyLevelChange();
  }
  /*-- セル選択の場合 --*/
  else if (cellSelection) {
    std::set<TXshSimpleLevel *> levels;
    int r0, r1, c0, c1;
    cellSelection->getSelectedCells(r0, c0, r1, c1);
    TXsheet *xsheet = app->getCurrentXsheet()->getXsheet();
    // Cerco tutti i livelli, con estensensione "tlv", contenuti nella selezione
    bool selectionContainLevel = false;
    /*-- セル選択範囲の各セルについて --*/
    int c, r;
    for (c = c0; c <= c1; c++)
      for (r = r0; r <= r1; r++) {
        TXshCell cell          = xsheet->getCell(r, c);
        TXshSimpleLevel *level = (!cell.isEmpty()) ? cell.getSimpleLevel() : 0;
        if (!level) continue;
        std::string ext = level->getPath().getType();
        int type        = level->getType();
        /*-- Revert可能なLevelタイプの条件 --*/
        if ((isCleanedUp && type == TZP_XSHLEVEL) ||
            (!isCleanedUp && (type == TZP_XSHLEVEL || type == PLI_XSHLEVEL ||
                              type == OVL_XSHLEVEL))) {
          levels.insert(level);
          selectionContainLevel = true;
        }
      }
    if (levels.empty() || !selectionContainLevel) {
      DVGui::error(
          QObject::tr("The Reload command is not supported for "
                      "the current selection."));
      return;
    }
    // Per ogni livello trovo i TFrameId contenuti nella selezione e richiamo
    // loadLastSaveFids.
    TUndoManager::manager()->beginBlock();
    std::set<TXshSimpleLevel *>::iterator it = levels.begin();
    /*-- Revert対象の各レベルについて --*/
    for (auto const sl : levels) {
      std::set<TFrameId> selectedFids;
      /*- 選択範囲のFrameIdを取得する -*/
      getLevelSelectedFids(selectedFids, *it, r0, c0, r1, c1);
      RevertToCommandUndo *undo =
          new RevertToCommandUndo(sl, selectedFids, isCleanedUp);
      bool commandExecuted = false;
      /*- "Revert to Cleaned up" の場合 -*/
      if (isCleanedUp) commandExecuted = loadUnpaintedFids(sl, selectedFids);
      /*- "Revert to Last Saved" の場合 -*/
      else
        commandExecuted = loadLastSaveFids(sl, selectedFids);
      if (!commandExecuted)
        delete undo;
      else {
        TUndoManager::manager()->add(undo);
        if (isCleanedUp) sl->setDirtyFlag(true);
      }
    }
    TUndoManager::manager()->endBlock();
    app->getCurrentXsheet()->notifyXsheetChanged();
  } else
    DVGui::error(QObject::tr("The current selection is invalid."));
}

//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//=============================================================================
// RevertToCleanedUpCommand
//-----------------------------------------------------------------------------

class RevertToCleanedUpCommand final : public MenuItemHandler {
public:
  RevertToCleanedUpCommand() : MenuItemHandler(MI_RevertToCleanedUp) {}

  void execute() override { revertTo(true); }

} revertToCleanedUpCommand;

//=============================================================================
// RevertToLastSaveCommand
//-----------------------------------------------------------------------------

class RevertToLastSaveCommand final : public MenuItemHandler {
public:
  RevertToLastSaveCommand() : MenuItemHandler(MI_RevertToLastSaved) {}

  void execute() override { revertTo(false); }

} revertToLastSaveCommand;

//-----------------------------------------------------------------------------
namespace {
class addLevelToCastUndo final : public TUndo {
  TXshLevelP m_xl;
  std::wstring m_newName, m_oldName;

public:
  addLevelToCastUndo(TXshLevel *xl, const std::wstring newName = L"",
                     const std::wstring oldName = L"")
      : m_xl(xl), m_newName(newName), m_oldName(oldName) {}

  void undo() const override {
    TLevelSet *levelSet =
        TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
    levelSet->removeLevel(m_xl.getPointer());
    if (m_oldName != L"") m_xl->setName(m_oldName);
    if (m_isLastInBlock)
      TApp::instance()->getCurrentScene()->notifyCastChange();
  }
  void redo() const override {
    TLevelSet *levelSet =
        TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
    if (m_newName != L"") m_xl->setName(m_newName);
    levelSet->insertLevel(m_xl.getPointer());
    if (m_isLastInRedoBlock)
      TApp::instance()->getCurrentScene()->notifyCastChange();
  }

  int getSize() const override { return sizeof *this + 100; }

  QString getHistoryString() override {
    return QObject::tr("Add Level to Scene Cast : %1")
        .arg(QString::fromStdWString(m_xl->getName()));
  }
};

};  // namespace

void LevelCmd::addMissingLevelsToCast(const QList<TXshColumnP> &columns) {
  // make sure that the levels contained in the pasted columns are registered in
  // the scene cast it may rename the level if there is another level with the
  // same name
  std::set<TXshLevel *> levels;
  // obtain level set contained in the specified columns
  // it is used for checking and updating the scene cast when pasting
  // based on TXsheet::getUsedLevels
  for (auto column : columns) {
    if (!column) continue;

    TXshCellColumn *cellColumn = column->getCellColumn();
    if (!cellColumn) continue;

    int r0, r1;
    if (!cellColumn->getRange(r0, r1)) continue;

    TXshLevel *level = 0;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = cellColumn->getCell(r);
      if (cell.isEmpty() || !cell.m_level) continue;

      if (level != cell.m_level.getPointer()) {
        level = cell.m_level.getPointer();
        levels.insert(level);
        if (level->getChildLevel()) {
          TXsheet *childXsh = level->getChildLevel()->getXsheet();
          childXsh->getUsedLevels(levels);
        }
      }
    }
  }
  LevelCmd::addMissingLevelsToCast(levels);
}

void LevelCmd::addMissingLevelsToCast(std::set<TXshLevel *> &levels) {
  // remove zerary fx levels which are not registered in the cast
  for (auto it = levels.begin(); it != levels.end();) {
    if ((*it)->getZeraryFxLevel())
      it = levels.erase(it);
    else
      ++it;
  }

  if (levels.empty()) return;
  TUndoManager::manager()->beginBlock();
  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  bool castChanged = false;
  // for each level
  for (auto level : levels) {
    std::wstring levelName = level->getName();

    // search by level name
    if (TXshLevel *levelInCast = levelSet->getLevel(levelName)) {
      // continue if it is the same level. This should be in most cases
      if (level == levelInCast) continue;

      // if the the name is occupied by another level, then rename and register
      // it
      std::wstring oldName = levelName;
      NameModifier nm(levelName);
      levelName = nm.getNext();
      while (1) {
        TXshLevel *existingLevel = levelSet->getLevel(levelName);
        // if the level name is not used in the cast, nothing to do
        if (!existingLevel) break;
        // try if the existing level is unused in the xsheet and remove from the
        // cast
        else if (Preferences::instance()->isAutoRemoveUnusedLevelsEnabled() &&
                 LevelCmd::removeLevelFromCast(
                     existingLevel,
                     TApp::instance()->getCurrentScene()->getScene(), false)) {
          DVGui::info(
              QObject::tr("Removed unused level %1 from the scene cast. (This "
                          "behavior can be disabled in Preferences.)")
                  .arg(QString::fromStdWString(levelName)));
          break;
        }
        levelName = nm.getNext();
      }
      addLevelToCastUndo *undo =
          new addLevelToCastUndo(level, levelName, oldName);
      undo->m_isLastInRedoBlock = false;  // prevent to emit signal
      undo->redo();
      TUndoManager::manager()->add(undo);
      castChanged = true;
    }
    // if not found
    else {
      // register the level
      addLevelToCastUndo *undo = new addLevelToCastUndo(level);
      undo->redo();
      TUndoManager::manager()->add(undo);
      castChanged = true;
    }
  }

  TUndoManager::manager()->endBlock();

  if (castChanged) TApp::instance()->getCurrentScene()->notifyCastChange();
}