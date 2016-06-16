

#include "studiopalettecmd.h"
#include "tapp.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "tundo.h"
#include "tcolorstyles.h"
#include "tsystem.h"
#include "toonzqt/icongenerator.h"
#include "toonzutil.h"
#include "tconvert.h"

#include "toonz/studiopalette.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/sceneproperties.h"

// DA FARE
// Mi serve per effettuare il cambiamento della StudioPalette corrente
#include "studiopaletteviewer.h"

/*! \namespace StudioPaletteCmd
                \brief Provides a collection of methods to manage \b
   StudioPalette.
*/

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// PaletteAssignUndo

class PaletteAssignUndo : public TUndo {
  TPaletteP m_targetPalette, m_oldPalette, m_newPalette;

public:
  PaletteAssignUndo(const TPaletteP &targetPalette, const TPaletteP &oldPalette,
                    const TPaletteP &newPalette)
      : m_targetPalette(targetPalette)
      , m_oldPalette(oldPalette)
      , m_newPalette(newPalette) {}

  void undo() const {
    m_targetPalette->assign(m_oldPalette.getPointer());
    TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
  }
  void redo() const {
    m_targetPalette->assign(m_newPalette.getPointer());
    TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
  }

  int getSize() const {
    return sizeof(*this) +
           (m_targetPalette->getStyleCount() + m_oldPalette->getStyleCount() +
            m_newPalette->getStyleCount()) *
               100;
  }
};

//=============================================================================
// StudioPaletteAssignUndo

class StudioPaletteAssignUndo : public TUndo {
  TPaletteP m_oldPalette, m_newPalette;
  TFilePath m_fp;

public:
  StudioPaletteAssignUndo(const TFilePath &targetPath,
                          const TPaletteP &oldPalette,
                          const TPaletteP &newPalette)
      : m_fp(targetPath), m_oldPalette(oldPalette), m_newPalette(newPalette) {}

  void undo() const {
    StudioPalette *sp = StudioPalette::instance();
    sp->setPalette(m_fp, m_oldPalette.getPointer());
    TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
  }
  void redo() const {
    StudioPalette *sp = StudioPalette::instance();
    sp->setPalette(m_fp, m_newPalette.getPointer());
    TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
  }

  int getSize() const {
    return sizeof(*this) +
           (m_oldPalette->getStyleCount() + m_newPalette->getStyleCount()) *
               100;
  }
};

//=============================================================================
// DeletePaletteUndo

class DeletePaletteUndo : public TUndo {
  TFilePath m_palettePath;
  TPaletteP m_palette;

public:
  DeletePaletteUndo(const TFilePath &palettePath) : m_palettePath(palettePath) {
    m_palette = StudioPalette::instance()->getPalette(m_palettePath);
  }

  void undo() const {
    StudioPalette::instance()->setPalette(m_palettePath, m_palette->clone());
  }
  void redo() const { StudioPalette::instance()->deletePalette(m_palettePath); }
  int getSize() const { return sizeof(*this) + sizeof(TPalette); }
};

//=============================================================================
// CreatePaletteUndo

class CreatePaletteUndo : public TUndo {
  TFilePath m_palettePath;
  TPaletteP m_palette;

public:
  CreatePaletteUndo(const TFilePath &palettePath) : m_palettePath(palettePath) {
    m_palette = StudioPalette::instance()->getPalette(m_palettePath);
  }

  void undo() const { StudioPalette::instance()->deletePalette(m_palettePath); }
  void redo() const {
    StudioPalette::instance()->setPalette(m_palettePath, m_palette->clone());
  }
  int getSize() const { return sizeof(*this) + sizeof(TPalette); }
};

//=============================================================================
// DeleteFolderUndo
// Oss.: l'undo non ricorda eventuali sottoFolder o palette, si limita a
// ricreare
//			il folder!!!

class DeleteFolderUndo : public TUndo {
  TFilePath m_folderPath;

public:
  DeleteFolderUndo(const TFilePath &folderPath) : m_folderPath(folderPath) {}

  void undo() const {
    StudioPalette::instance()->createFolder(m_folderPath.getParentDir(),
                                            m_folderPath.getWideName());
  }
  void redo() const { StudioPalette::instance()->deleteFolder(m_folderPath); }
  int getSize() const { return sizeof(*this) + sizeof(TPalette); }
};

//=============================================================================
// CreateFolderUndo

class CreateFolderUndo : public TUndo {
  TFilePath m_folderPath;

public:
  CreateFolderUndo(const TFilePath &folderPath) : m_folderPath(folderPath) {}

  void undo() const { StudioPalette::instance()->deleteFolder(m_folderPath); }
  void redo() const {
    StudioPalette::instance()->createFolder(m_folderPath.getParentDir(),
                                            m_folderPath.getWideName());
  }
  int getSize() const { return sizeof(*this) + sizeof(TPalette); }
};

//=============================================================================
// MovePaletteUndo

class MovePaletteUndo : public TUndo {
  TFilePath m_dstPath, m_srcPath;

public:
  MovePaletteUndo(const TFilePath &dstPath, const TFilePath &srcPath)
      : m_dstPath(dstPath), m_srcPath(srcPath) {}

  void undo() const {
    StudioPalette::instance()->movePalette(m_srcPath, m_dstPath);
  }
  void redo() const {
    StudioPalette::instance()->movePalette(m_dstPath, m_srcPath);
  }
  int getSize() const { return sizeof(*this); }
};

//-----------------------------------------------------------------------------
}  // namespace
//=============================================================================

//-----------------------------------------------------------------------------

void StudioPaletteCmd::loadIntoCurrentPalette() {
  // DA FARE
  //  ColorController *cc = ColorController::instance();
  //  TPalette *palette = cc->getStudioPalette();
  TPalette *palette = TApp::instance()->getCurrentPalette()->getPalette();

  wstring gname = palette->getGlobalName();
  TFilePath fp  = StudioPalette::instance()->getPalettePath(gname);
  if (fp != TFilePath()) loadIntoCurrentPalette(fp);

  // DA FARE
  if (!palette->isCleanupPalette())
    TApp::instance()->getCurrentLevel()->getLevel()->setPaletteDirtyFlag(true);
  else
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::loadIntoCurrentPalette(const TFilePath &fp) {
  // DA FARE
  //  ColorController *cc = ColorController::instance();
  //  int styleId = cc->getCurrentStyleIndex();
  TApp *app   = TApp::instance();
  int styleId = app->getCurrentPalette()->getStyleIndex();

  TPalette *palette = StudioPalette::instance()->getPalette(fp, true);
  if (!palette) return;

  // DA FARE
  TPalette *current = !palette->isCleanupPalette()
                          ? app->getCurrentPalette()->getPalette()
                          : app->getCurrentScene()
                                ->getScene()
                                ->getProperties()
                                ->getCleanupPalette();
  //    ? cc->getCleanupPalette()
  //    : cc->getLevelPalette();

  if (!current) return;
  TPalette *old = current->clone();
  while (palette->getStyleCount() < current->getStyleCount()) {
    int index = palette->getStyleCount();
    assert(index < current->getStyleCount());
    TColorStyle *style = current->getStyle(index)->clone();
    palette->addStyle(style);
  }
  current->assign(palette);
  // DA FARE
  //  if(cc->getCurrentStylePalette() == current &&
  //  styleId>=current->getStyleCount())
  //    cc->setCurrentStyle(1);
  TUndoManager::manager()->add(
      new PaletteAssignUndo(current, old, current->clone()));

  // DA FARE
  if (  // current == cc->getCurrentStylePalette() &&
      app->getCurrentLevel()->getLevel()) {
    TXshSimpleLevel *sl = app->getCurrentLevel()->getSimpleLevel();
    if (sl) {
      sl->invalidateIcons();
      sl->setDirtyFlag(true);
    }
    app->instance()->getCurrentLevel()->notifyLevelChange();
  }

  app->getCurrentPalette()->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::mergeIntoCurrentPalette(const TFilePath &fp) {
  //  ColorController *cc = ColorController::instance();
  TPalette *palette = StudioPalette::instance()->getPalette(fp);

  TApp *app = TApp::instance();

  // DA FARE
  TPalette *current = !palette->isCleanupPalette()
                          ? app->getCurrentPalette()->getPalette()
                          : app->getCurrentScene()
                                ->getScene()
                                ->getProperties()
                                ->getCleanupPalette();
  //    ? cc->getCleanupPalette()
  //    : cc->getLevelPalette();

  if (!current) return;
  TPalette *old = current->clone();
  current->merge(palette);
  TUndoManager::manager()->add(
      new PaletteAssignUndo(current, old, current->clone()));

  app->getCurrentPalette()->notifyPaletteChanged();

  // DA FARE
  if (  // current == cc->getCurrentStylePalette() &&
      app->getCurrentLevel()->getLevel())
    app->getCurrentLevel()->getLevel()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::replaceWithCurrentPalette(const TFilePath &fp) {
  // DA FARE
  // ColorController *cc = ColorController::instance();
  StudioPalette *sp = StudioPalette::instance();
  TPalette *palette = sp->getPalette(fp);

  // DA FARE
  TPalette *current = !palette->isCleanupPalette()
                          ? TApp::instance()->getCurrentPalette()->getPalette()
                          : TApp::instance()
                                ->getCurrentScene()
                                ->getScene()
                                ->getProperties()
                                ->getCleanupPalette();
  //    ? cc->getCleanupPalette()
  //    : cc->getLevelPalette();
  if (!current) return;
  TPalette *old = palette->clone();
  palette->assign(current);
  sp->setPalette(fp, current);
  TUndoManager::manager()->add(
      new StudioPaletteAssignUndo(fp, old, current->clone()));
  // DA FARE
  // Cambio la studioPalette corrente
  //  ColorController::instance()->setStudioPalette(palette);
  DAFARE::setCurrentStudioPalette(current);
  DAFARE::setDirtyFlag(true);

  TApp::instance()->getCurrentPalette()->notifyPaletteSwitched();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::loadIntoCleanupPalette(const TFilePath &fp) {
  // DA FARE
  //  ColorController *cc = ColorController::instance();
  //  int styleId = cc->getCurrentStyleIndex();
  TPalette *palette = StudioPalette::instance()->getPalette(fp, true);
  if (!palette) return;
  // DA FARE
  //  TPalette *current = cc->getCleanupPalette();
  TPalette *current = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getCleanupPalette();
  assert(current);
  TPalette *old = current->clone();
  current->assign(palette);
  // DA FARE
  //  if(cc->getCurrentStylePalette() == current &&
  //  styleId>=current->getStyleCount())
  //    cc->setCurrentStyle(1);
  TUndoManager::manager()->add(
      new PaletteAssignUndo(current, old, current->clone()));
  TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::replaceWithCleanupPalette(const TFilePath &fp) {
  // DA FARE
  // ColorController *cc = ColorController::instance();
  StudioPalette *sp = StudioPalette::instance();
  TPalette *palette = sp->getPalette(fp);

  // DA FARE
  // TPalette *current = cc->getCleanupPalette();
  TPalette *current = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getCleanupPalette();
  if (!current) return;
  TPalette *old = palette->clone();
  palette->assign(current);
  sp->setPalette(fp, current);
  TUndoManager::manager()->add(
      new StudioPaletteAssignUndo(fp, old, current->clone()));
  TApp::instance()->getCurrentPalette()->notifyPaletteSwitched();
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::updateAllLinkedStyles() {
  StudioPalette *sp   = StudioPalette::instance();
  TApp *app           = TApp::instance();
  ToonzScene *scene   = app->getCurrentScene()->getScene();
  TLevelSet *levelSet = scene->getLevelSet();
  for (int i = 0; i < levelSet->getLevelCount(); i++) {
    TXshLevel *xl       = levelSet->getLevel(i);
    TXshSimpleLevel *sl = xl ? xl->getSimpleLevel() : 0;
    if (!sl) continue;
    TPalette *palette = sl->getPalette();
    if (palette) {
      sp->updateLinkedColors(palette);
      if (sl->getType() == TZP_XSHLEVEL) {
        std::vector<TFrameId> fids;
        sl->getFids(fids);
        std::vector<TFrameId>::iterator it;
        for (it = fids.begin(); it != fids.end(); ++it) {
          TFrameId fid = *it;
          string id    = sl->getImageId(fid);
          IconGenerator::instance()->invalidate(sl, fid);
          // ImageManager::instance()->invalidate(id);
        }
      }
    }
  }
  TApp::instance()->getCurrentPalette()->notifyPaletteChanged();
}

//-----------------------------------------------------------------------------
/*! Delete palette identified by \b fp in \b StudioPalette. If there are any
                problems in loading send an error message.
*/
void StudioPaletteCmd::deletePalette(const TFilePath &fp) {
  TUndo *undo = new DeletePaletteUndo(fp);
  try {
    StudioPalette::instance()->deletePalette(fp);
    TUndoManager::manager()->add(undo);
  } catch (TException &e) {
    delete undo;
    error("Can't delete palette: " + QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    delete undo;
    error("Can't delete palette");
  }
}

//-----------------------------------------------------------------------------
/*! Move palette from TFilePath \b srcPath to \b TFilePath \b dstPath. If there
                are any problems in moving send an error message.
*/
void StudioPaletteCmd::movePalette(const TFilePath &dstPath,
                                   const TFilePath &srcPath) {
  try {
    TSystem::touchParentDir(dstPath);
    StudioPalette::instance()->movePalette(dstPath, srcPath);
    TUndoManager::manager()->add(new MovePaletteUndo(dstPath, srcPath));
  } catch (TException &e) {
    error("Can't rename palette: " + QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't rename palette");
  }
}

//-----------------------------------------------------------------------------
/*! Create palette \b palette in folder \b folderName with name \b paletteName.
                If there are any problems send an error message.
*/
TFilePath StudioPaletteCmd::createPalette(const TFilePath &folderName,
                                          string paletteName,
                                          const TPalette *palette) {
  TFilePath palettePath;
  try {
    if (!TFileStatus(folderName).doesExist()) TSystem::mkDir(folderName);
    palettePath =
        StudioPalette::instance()->createPalette(folderName, paletteName);
    if (palette) StudioPalette::instance()->setPalette(palettePath, palette);
    TUndoManager::manager()->add(new CreatePaletteUndo(palettePath));
  } catch (TException &e) {
    error("Can't create palette: " + QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't create palette");
  }
  return palettePath;
}

//-----------------------------------------------------------------------------
/*! Add a folder in StudioPalette TFilePath \b parentFolderPath. If there
                are any problems send an error message.
*/
TFilePath StudioPaletteCmd::addFolder(const TFilePath &parentFolderPath) {
  TFilePath folderPath;
  try {
    folderPath = StudioPalette::instance()->createFolder(parentFolderPath);
    TUndoManager::manager()->add(new CreateFolderUndo(folderPath));
  } catch (TException &e) {
    error("Can't create palette folder: " +
          QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't create palette folder");
  }
  return folderPath;
}

//-----------------------------------------------------------------------------
/*! Delete folder \b folderPath. If there are any problems in deleting send an
                error message.
*/
void StudioPaletteCmd::deleteFolder(const TFilePath &folderPath) {
  TUndo *undo = new DeleteFolderUndo(folderPath);
  try {
    StudioPalette::instance()->deleteFolder(folderPath);
    TUndoManager::manager()->add(undo);
  } catch (TException &e) {
    delete undo;
    error("Can't delete palette: " + QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    delete undo;
    error("Can't delete palette");
  }
}

//-----------------------------------------------------------------------------

void StudioPaletteCmd::scanPalettes(const TFilePath &folder,
                                    const TFilePath &sourcePath) {
  error("uh oh");
}
