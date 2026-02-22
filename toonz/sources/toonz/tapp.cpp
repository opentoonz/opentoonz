

#include "tapp.h"

// Tnz6 includes
#include "cleanupsettingspopup.h"
#include "iocommand.h"
#include "mainwindow.h"
#include "cellselection.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/cleanupparameters.h"
#include "toonz/stage2.h"
#include "toutputproperties.h"
#include "toonz/palettecontroller.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/txshlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "toonz/txshsoundcolumn.h"

// TnzCore includes
#include "tbigmemorymanager.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tunit.h"
#include "tsystem.h"
#include "sceneviewer.h"

// Qt includes
#include <QTimer>
#include <QLabel>
#include <QDebug>
#include <QEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QDesktopWidget>

//===================================================================

namespace {

double getCurrentCameraSize() {
  return TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getCurrentCamera()
      ->getSize()
      .lx;
}

std::pair<double, double> getCurrentDpi() {
  TPointD dpi = TApp::instance()
                    ->getCurrentScene()
                    ->getScene()
                    ->getCurrentCamera()
                    ->getDpi();
  return std::make_pair(dpi.x, dpi.y);
}

}  // namespace

//=============================================================================
// TApp
//-----------------------------------------------------------------------------

TApp::TApp()
    : m_currentScene(0)
    , m_currentXsheet(0)
    , m_currentFrame(0)
    , m_currentColumn(0)
    , m_currentLevel(0)
    , m_currentTool(0)
    , m_currentObject(0)
    , m_currentSelection(0)
    , m_currentOnionSkinMask(0)
    , m_currentFx(0)
    , m_mainWindow(0)
    , m_autosaveTimer(0)
    , m_autosaveSuspended(false)
    , m_saveInProgress(false)
    , m_isStarting(false)
    , m_isPenCloseToTablet(false) {
  m_currentScene         = new TSceneHandle();
  m_currentXsheet        = new TXsheetHandle();
  m_currentFrame         = new TFrameHandle();
  m_currentColumn        = new TColumnHandle();
  m_currentLevel         = new TXshLevelHandle();
  m_currentTool          = new ToolHandle();
  m_currentObject        = new TObjectHandle();
  m_currentOnionSkinMask = new TOnionSkinMaskHandle();
  m_currentFx            = new TFxHandle();
  m_currentSelection     = TSelectionHandle::getCurrent();

  m_paletteController = new PaletteController();

  connect(m_currentXsheet, &TXsheetHandle::xsheetChanged, this,
          &TApp::onXsheetChanged);
  connect(m_currentXsheet, &TXsheetHandle::xsheetSoundChanged, this,
          &TApp::onXsheetSoundChanged);
  connect(m_currentScene, &TSceneHandle::sceneSwitched, this,
          &TApp::onSceneSwitched);
  connect(m_currentScene, &TSceneHandle::sceneChanged, this,
          &TApp::onSceneChanged);
  connect(m_currentXsheet, &TXsheetHandle::xsheetSwitched, this,
          &TApp::onXsheetSwitched);
  connect(m_currentFrame, &TFrameHandle::frameSwitched, this,
          &TApp::onFrameSwitched);
  connect(m_currentFrame, &TFrameHandle::frameSwitched, this,
          &TApp::onImageChanged);
  connect(m_currentFx, &TFxHandle::fxSwitched, this, &TApp::onFxSwitched);
  connect(m_currentColumn, &TColumnHandle::columnIndexSwitched, this,
          &TApp::onColumnIndexSwitched);
  connect(m_currentColumn, &TColumnHandle::columnIndexSwitched, this,
          &TApp::onImageChanged);
  connect(m_currentLevel, &TXshLevelHandle::xshLevelSwitched, this,
          &TApp::onImageChanged);
  connect(m_currentLevel, &TXshLevelHandle::xshLevelSwitched, this,
          &TApp::onXshLevelSwitched);
  connect(m_currentLevel, &TXshLevelHandle::xshLevelChanged, this,
          &TApp::onXshLevelChanged);
  connect(m_currentObject, &TObjectHandle::objectSwitched, this,
          &TApp::onObjectSwitched);
  connect(m_currentObject, &TObjectHandle::splineChanged, this,
          &TApp::onSplineChanged);
  connect(m_paletteController->getCurrentLevelPalette(),
          &TPaletteHandle::paletteChanged, this, &TApp::onPaletteChanged);
  connect(m_paletteController->getCurrentLevelPalette(),
          &TPaletteHandle::colorStyleSwitched, this,
          &TApp::onLevelColorStyleSwitched);
  connect(m_paletteController->getCurrentLevelPalette(),
          &TPaletteHandle::colorStyleChangedOnMouseRelease, this,
          &TApp::onLevelColorStyleChanged);
  connect(m_paletteController->getCurrentCleanupPalette(),
          &TPaletteHandle::paletteChanged, m_currentScene,
          &TSceneHandle::sceneChanged);

  TMeasureManager::instance()->addCameraMeasures(getCurrentCameraSize);

  m_autosaveTimer = new QTimer(this);
  connect(m_autosaveTimer, &QTimer::timeout, this, &TApp::autosave);

  Preferences *preferences = Preferences::instance();

  if (preferences->isRasterOptimizedMemory()) {
    if (!TBigMemoryManager::instance()->init(
            (int)(/*15*1024*/ TSystem::getFreeMemorySize(true) * .8)))
      DVGui::warning(tr("Error allocating memory: not enough memory."));
  }
  connect(preferences, &Preferences::stopAutoSave, this, &TApp::onStopAutoSave);
  connect(preferences, &Preferences::startAutoSave, this,
          &TApp::onStartAutoSave);
  connect(m_currentTool, &ToolHandle::toolEditingFinished, this,
          &TApp::onToolEditingFinished);

  if (preferences->isAutosaveEnabled())
    m_autosaveTimer->start(preferences->getAutosavePeriod() * 1000 * 60);

  UnitParameters::setCurrentDpiGetter(getCurrentDpi);
}

//-----------------------------------------------------------------------------

TApp *TApp::instance() {
  static TApp _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

TApp::~TApp() {}

//-----------------------------------------------------------------------------

void TApp::init() {
  m_isStarting = true;
  IoCmd::newScene();
  m_currentColumn->setColumnIndex(0);
  m_currentFrame->setFrame(0);
  m_isStarting = false;
}

//-----------------------------------------------------------------------------

TMainWindow *TApp::getCurrentRoom() const {
  MainWindow *mainWindow = dynamic_cast<MainWindow *>(getMainWindow());
  if (mainWindow)
    return mainWindow->getCurrentRoom();
  else
    return 0;
}

//-----------------------------------------------------------------------------

void TApp::writeSettings() {
  MainWindow *mainWindow = dynamic_cast<MainWindow *>(getMainWindow());
  if (mainWindow) mainWindow->refreshWriteSettings();
}

//-----------------------------------------------------------------------------

TPaletteHandle *TApp::getCurrentPalette() const {
  return m_paletteController->getCurrentPalette();
}

//-----------------------------------------------------------------------------

TColorStyle *TApp::getCurrentLevelStyle() const {
  return m_paletteController->getCurrentLevelPalette()->getStyle();
}

//-----------------------------------------------------------------------------

int TApp::getCurrentLevelStyleIndex() const {
  return m_paletteController->getCurrentLevelPalette()->getStyleIndex();
}

//-----------------------------------------------------------------------------

void TApp::setCurrentLevelStyleIndex(int index, bool forceUpdate) {
  m_paletteController->getCurrentLevelPalette()->setStyleIndex(index,
                                                               forceUpdate);
}

//-----------------------------------------------------------------------------

int TApp::getCurrentImageType() {
  // When a spline is selected, allow vector editing regardless of the current
  // cell type.
  if (getCurrentObject()->isSpline()) return TImage::VECTOR;

  TXshSimpleLevel *sl = 0;

  if (getCurrentFrame()->isEditingScene()) {
    int row = getCurrentFrame()->getFrame();
    int col = getCurrentColumn()->getColumnIndex();
    if (col < 0) {
      int levelType = Preferences::instance()->getDefLevelType();
      return (levelType == PLI_XSHLEVEL)
                 ? TImage::VECTOR
                 :  // RASTER image type includes both TZI_XSHLEVEL
                 (levelType == TZP_XSHLEVEL)
                 ? TImage::TOONZ_RASTER
                 : TImage::RASTER;  // and OVL_XSHLEVEL level types
    }

    TXsheet *xsh = getCurrentXsheet()->getXsheet();
    if (xsh->getColumn(col) && xsh->getColumn(col)->getSoundColumn())
      return TImage::VECTOR;
    TXshCell cell = xsh->getCell(row, col);
    if (cell.isEmpty()) {
      int r0, r1;
      xsh->getCellRange(col, r0, r1);
      if (0 <= r0 && r0 <= r1) {
        // Use the type of the topmost level stored in the column
        cell = xsh->getCell(r0, col);
      } else /* Column is empty */
      {
        return TImage::NONE;
        /*
        int levelType = Preferences::instance()->getDefLevelType();
        return (levelType == PLI_XSHLEVEL)
                   ? TImage::VECTOR
                   : (levelType == TZP_XSHLEVEL) ? TImage::TOONZ_RASTER
                                                 : TImage::RASTER;
        */
      }
    }

    sl = cell.getSimpleLevel();
  } else if (getCurrentFrame()->isEditingLevel())
    sl = getCurrentLevel()->getSimpleLevel();

  if (sl) {
    switch (sl->getType()) {
    case TZP_XSHLEVEL:
      return TImage::TOONZ_RASTER;
    case OVL_XSHLEVEL:
      return TImage::RASTER;
    case META_XSHLEVEL:
      return TImage::META;
    case MESH_XSHLEVEL:
      return TImage::MESH;
    case PLI_XSHLEVEL:
      return TImage::VECTOR;
    default:
      return TImage::NONE;
    }
  }

  return TImage::NONE;
}

//-----------------------------------------------------------------------------

void TApp::updateXshLevel() {
  TXshLevel *xl = 0;
  if (m_currentFrame->isEditingScene()) {
    int frame       = m_currentFrame->getFrame();
    int column      = m_currentColumn->getColumnIndex();
    TXsheet *xsheet = m_currentXsheet->getXsheet();

    // sound column case
    if (xsheet->getColumn(column) &&
        xsheet->getColumn(column)->getSoundColumn()) {
      if (xsheet->getColumn(column)->getSoundColumn()->m_levels.size() > 0) {
        xl = static_cast<TXshLevel *>(xsheet->getColumn(column)
                                          ->getSoundColumn()
                                          ->m_levels.at(0)
                                          ->getSoundLevel());
      }
    } else if (xsheet && column >= 0 && frame >= 0 &&
               !xsheet->isColumnEmpty(column)) {
      TXshCell cell = xsheet->getCell(frame, column);
      xl            = cell.m_level.getPointer();

      // If the current cell is empty but there is a level in the previous cell
      // of the same column, take that as the current level.
      if (!xl && frame > 0) {
        TXshCell cell = xsheet->getCell(frame - 1, column);
        xl            = cell.m_level.getPointer();
      }
    }

    m_currentLevel->setLevel(xl);

    // level could be the same, but palette could have changed
    if (xl && xl->getSimpleLevel()) {
      TPalette *currentPalette =
          m_paletteController->getCurrentPalette()->getPalette();
      int styleIndex =
          m_paletteController->getCurrentLevelPalette()->getStyleIndex();

      m_paletteController->getCurrentLevelPalette()->setPalette(
          xl->getSimpleLevel()->getPalette(), styleIndex);

      // If the new selected level is an OVL and the current palette is a
      // cleanup palette, then set the current handle to the cleanup palette.
      if (xl->getType() == OVL_XSHLEVEL && currentPalette &&
          currentPalette->isCleanupPalette())

        m_paletteController->editCleanupPalette();
    } else if (xl && xl->getPaletteLevel()) {
      int styleIndex =
          m_paletteController->getCurrentLevelPalette()->getStyleIndex();
      m_paletteController->getCurrentLevelPalette()->setPalette(
          xl->getPaletteLevel()->getPalette(), styleIndex);
    } else
      m_paletteController->getCurrentLevelPalette()->setPalette(0);
  }
}

//-----------------------------------------------------------------------------

void TApp::updateCurrentFrame() {
  ToonzScene *scene = m_currentScene->getScene();
  m_currentFrame->setSceneFrameSize(scene->getFrameCount());
  int f0, f1, step;
  scene->getProperties()->getPreviewProperties()->getRange(f0, f1, step);
  if (f0 > f1) {
    f0 = 0;
    f1 = scene->getFrameCount() - 1;
  }
  if (f0 != m_currentFrame->getStartFrame() ||
      f1 != m_currentFrame->getEndFrame()) {
    m_currentFrame->setFrameRange(f0, f1);
    std::vector<TFrameId> fids;
    TXshSimpleLevel *sl = m_currentLevel->getSimpleLevel();
    if (sl) {
      sl->getFids(fids);
      m_currentFrame->setFrameIds(fids);
    }
  }
}

//-----------------------------------------------------------------------------

void TApp::onSceneSwitched() {
  // update XSheet
  m_currentXsheet->setXsheet(m_currentScene->getScene()->getXsheet());

  // reset current frame
  m_currentFrame->setFrame(0);

  // clear current onionSkinMask
  m_currentOnionSkinMask->clear();

  // update currentFrames
  updateCurrentFrame();

  // update current tool
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}

//-----------------------------------------------------------------------------

void TApp::onImageChanged() {
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}

//-----------------------------------------------------------------------------

void TApp::onXsheetSwitched() {
  // update current object
  int columnIndex = m_currentColumn->getColumnIndex();
  if (columnIndex >= 0)
    m_currentObject->setObjectId(TStageObjectId::ColumnId(columnIndex));

  // update xsheetlevel
  updateXshLevel();

  // no Fx is set to current.
  m_currentFx->setFx(0);
}

//-----------------------------------------------------------------------------

void TApp::onXsheetChanged() {
  updateXshLevel();
  updateCurrentFrame();
  // update current tool
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}

//-----------------------------------------------------------------------------

void TApp::onXsheetSoundChanged() {
  m_currentXsheet->getXsheet()->invalidateSound();
}

//-----------------------------------------------------------------------------

void TApp::onFrameSwitched() {
  updateXshLevel();
  int row = m_currentFrame->getFrameIndex();
  TCellSelection *sel =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());

  if (sel && !sel->isRowSelected(row) &&
      !Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled()) {
    sel->selectNone();
  }
}

//-----------------------------------------------------------------------------

void TApp::onFxSwitched() {
  //  if(m_currentFx->getFx() != 0)
  //    m_currentTool->setTool(T_Edit);
}

//-----------------------------------------------------------------------------

void TApp::onColumnIndexSwitched() {
  // update xsheetlevel
  updateXshLevel();

  // update current object
  int columnIndex = m_currentColumn->getColumnIndex();
  if (columnIndex >= 0)
    m_currentObject->setObjectId(TStageObjectId::ColumnId(columnIndex));
  else {
    TXsheet *xsh = getCurrentXsheet()->getXsheet();
    m_currentObject->setObjectId(
        TStageObjectId::CameraId(xsh->getCameraColumnIndex()));
  }
}

//-----------------------------------------------------------------------------

void TApp::onXshLevelSwitched(TXshLevel *) {
  TXshLevel *level = m_currentLevel->getLevel();
  if (level) {
    TXshSimpleLevel *simpleLevel = level->getSimpleLevel();

    // Update the current palette
    if (simpleLevel) {
      m_paletteController->getCurrentLevelPalette()->setPalette(
          simpleLevel->getPalette());

      // If the new selected level is an OVL and the current palette is a
      // cleanup palette, then set the current handle to the cleanup palette.
      TPalette *currentPalette =
          m_paletteController->getCurrentPalette()->getPalette();

      if (simpleLevel->getType() == OVL_XSHLEVEL && currentPalette &&
          currentPalette->isCleanupPalette())
        m_paletteController->editCleanupPalette();

      return;
    }

    TXshPaletteLevel *paletteLevel = level->getPaletteLevel();
    if (paletteLevel) {
      m_paletteController->getCurrentLevelPalette()->setPalette(
          paletteLevel->getPalette());
      return;
    }
  }

  m_paletteController->getCurrentLevelPalette()->setPalette(0);
}

//-----------------------------------------------------------------------------

void TApp::onXshLevelChanged() {
  TXshLevel *level = m_currentLevel->getLevel();
  std::vector<TFrameId> fids;
  if (level != 0) level->getFids(fids);
  m_currentFrame->setFrameIds(fids);
  // update current tool
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}
//-----------------------------------------------------------------------------

void TApp::onObjectSwitched() {
  if (m_currentObject->isSpline()) {
    TXsheet *xsh = m_currentXsheet->getXsheet();
    TStageObject *currentObject =
        xsh->getStageObject(m_currentObject->getObjectId());
    TStageObjectSpline *ps = currentObject->getSpline();
    m_currentObject->setSplineObject(ps);
  } else
    m_currentObject->setSplineObject(0);
  onImageChanged();
}

//-----------------------------------------------------------------------------

void TApp::onSplineChanged() {
  if (m_currentObject->isSpline()) {
    TXsheet *xsh = m_currentXsheet->getXsheet();
    TStageObject *currentObject =
        xsh->getStageObject(m_currentObject->getObjectId());
    currentObject->setOffset(
        currentObject->getOffset());  // invalidate currentObject
  }
}

//-----------------------------------------------------------------------------

void TApp::onSceneChanged() {
  updateCurrentFrame();
  m_currentTool->updateMatrix();
}

//-----------------------------------------------------------------------------

void TApp::onPaletteChanged() { m_currentScene->setDirtyFlag(true); }

//-----------------------------------------------------------------------------

void TApp::onLevelColorStyleSwitched() {
  TXshLevel *sl = m_currentLevel->getLevel();
  if (!sl) return;

  TPaletteHandle *ph = m_paletteController->getCurrentLevelPalette();
  assert(ph);

  ToonzCheck *tc = ToonzCheck::instance();
  int styleIndex = ph->getStyleIndex();

  // Sync the current style index
  tc->setColorIndex(styleIndex);

  int mask = tc->getChecks();

  // Update IconGenerator settings.
  // We do this every time to ensure that if checks are turned off (mask == 0),
  // the generator knows to stop highlighting.
  IconGenerator::Settings s;
  s.m_blackBgCheck      = mask & ToonzCheck::eBlackBg;
  s.m_transparencyCheck = mask & ToonzCheck::eTransparency;
  s.m_inksOnly          = mask & ToonzCheck::eInksOnly;
  s.m_inkIndex          = (mask & ToonzCheck::eInk) ? styleIndex : -1;
  s.m_paintIndex        = (mask & ToonzCheck::ePaint) ? styleIndex : -1;

  IconGenerator::instance()->setSettings(s);

  // To fix the "frozen icons" in other columns, we must invalidate the global
  // icon cache. This ensures that all visible levels in the filmstrip/xsheet
  // reflect the current check state.
  invalidateIcons();

  // Notify the system that level views need refreshing
  m_currentLevel->notifyLevelViewChange();

  // Notify scene change for UI consistency (e.g., Scene Cast) without setting
  // the dirty flag
  m_currentScene->notifySceneChanged(false);
}

//-----------------------------------------------------------------------------

static void notifyPaletteChanged(TXshSimpleLevel *simpleLevel) {
  simpleLevel->onPaletteChanged();
  // palette change can update icons only for ToonzVector / ToonzRaster types
  if (simpleLevel->getType() != TZP_XSHLEVEL &&
      simpleLevel->getType() != PLI_XSHLEVEL)
    return;
  std::vector<TFrameId> fids;
  simpleLevel->getFids(fids);
  // ToonzRaster level does not need to re-generate icons along with palette
  // changes since the icons are cached as color mapped images and the current
  // palette is applied just before using it. So here we just emit the signal to
  // update related panels.
  if (simpleLevel->getType() == TZP_XSHLEVEL)
    IconGenerator::instance()->notifyIconGenerated();
  else {  // ToonzVector needs to re-generate icons since it includes colors in
          // the cache.
    for (int i = 0; i < (int)fids.size(); i++)
      IconGenerator::instance()->invalidate(simpleLevel, fids[i]);
  }
}

//-----------------------------------------------------------------------------

void TApp::onLevelColorStyleChanged() {
  onPaletteChanged();
  TXshLevel *level = m_currentLevel->getLevel();
  if (!level) return;
  TPalette *palette            = getCurrentPalette()->getPalette();
  TXshSimpleLevel *simpleLevel = level->getSimpleLevel();
  if (simpleLevel && simpleLevel->getPalette() == palette) {
    notifyPaletteChanged(simpleLevel);
  } else {
    TLevelSet *levelSet = getCurrentScene()->getScene()->getLevelSet();
    for (int i = 0; i < levelSet->getLevelCount(); i++) {
      if (levelSet->getLevel(i)) {
        simpleLevel = levelSet->getLevel(i)->getSimpleLevel();
        if (simpleLevel && simpleLevel->getPalette() == palette) {
          notifyPaletteChanged(simpleLevel);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

void TApp::autosave() {
  ToonzScene *scene = getCurrentScene()->getScene();

  if (!getCurrentScene()->getDirtyFlag()) return;

  if (getCurrentTool()->isToolBusy()) {
    m_autosaveSuspended = true;
    return;
  } else
    m_autosaveSuspended = false;

  if (m_saveInProgress) return;

  // DVGui::ProgressDialog pb(
  //     "Autosaving scene..." + toQString(scene->getScenePath()), 0, 0, 1);

  Preferences *pref = Preferences::instance();
  if (pref->isAutosaveSceneEnabled() && pref->isAutosaveOtherFilesEnabled()) {
    IoCmd::saveAll(IoCmd::AUTO_SAVE);
  } else if (pref->isAutosaveSceneEnabled()) {
    IoCmd::saveScene(IoCmd::AUTO_SAVE);
  } else if (pref->isAutosaveOtherFilesEnabled()) {
    IoCmd::saveNonSceneFiles();
  }
}

//-----------------------------------------------------------------------------

void TApp::onToolEditingFinished() {
  if (!Preferences::instance()->isAutosaveEnabled() || !m_autosaveSuspended)
    return;
  autosave();
}

//-----------------------------------------------------------------------------

void TApp::onStartAutoSave() {
  assert(Preferences::instance()->isAutosaveEnabled());
  m_autosaveTimer->start(Preferences::instance()->getAutosavePeriod() * 1000 *
                         60);
}

//-----------------------------------------------------------------------------

void TApp::onStopAutoSave() {
  // assert(!Preferences::instance()->isAutosaveEnabled());
  m_autosaveTimer->stop();
}

//-----------------------------------------------------------------------------

bool TApp::eventFilter(QObject *watched, QEvent *e) {
  if (e->type() == QEvent::TabletEnterProximity) {
    m_isPenCloseToTablet = true;
  } else if (e->type() == QEvent::TabletLeaveProximity) {
    // if the user is painting very quickly with the pen, a number of events
    // could be still in the queue
    // they must be processed as tablet events (not mouse events)
    if (m_isPenCloseToTablet) qApp->processEvents();

    m_isPenCloseToTablet = false;
    emit tabletLeft();
  }

  return false;  // I just want to peek at the event. It must be processed
                 // anyway.
}

//-----------------------------------------------------------------------------

QString TApp::getCurrentRoomName() const {
  Room *currentRoom = dynamic_cast<Room *>(getCurrentRoom());
  if (!currentRoom) return QString();

  return currentRoom->getName();
}
