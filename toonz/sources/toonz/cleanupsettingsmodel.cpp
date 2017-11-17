

// TnzCore includes
#include "tstream.h"
#include "tsystem.h"

// ToonzLib includes
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/cleanupparameters.h"
#include "toonz/tcleanupper.h"

#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tproject.h"

// ToonzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/tselectionhandle.h"

// Toonz includes
#include "tapp.h"
#include "filebrowserpopup.h"

// Qt includes
#include <QMetaType>
#include <QApplication>

#include "cleanupsettingsmodel.h"

//**********************************************************************
//    Local classes
//**********************************************************************

namespace {

//==================================================================
//    IOSettingsPopup
//==================================================================

class IOSettingsPopup : public FileBrowserPopup {
public:
  IOSettingsPopup(const QString &title) : FileBrowserPopup(title) {
    setModal(true);
    addFilterType("cln");
  }

  void setPath(const TFilePath &levelPath) {
    TFilePath folderPath(levelPath);
    if (!folderPath.isEmpty())
      folderPath = folderPath.getParentDir();
    else {
      TProject *currentProject =
          TProjectManager::instance()->getCurrentProject().getPointer();
      if (currentProject)
        folderPath =
            currentProject->decode(currentProject->getFolder(TProject::Inputs));
    }

    setFolder(folderPath);
    setFilename(levelPath.withoutParentDir());
  }
};

//==================================================================
//    SaveSettingsPopup
//==================================================================

class SaveSettingsPopup final : public IOSettingsPopup {
public:
  SaveSettingsPopup() : IOSettingsPopup(tr("Save Cleanup Settings")) {
    // addFilterType("tif");
    // addFilterType("tga");
    // addFilterType("png");

    setOkText(tr("Save"));
  }

  bool execute() override {
    if (m_selectedPaths.empty()) return false;

    TFilePath savePath(*m_selectedPaths.begin());
    if (savePath.isEmpty()) return false;

    savePath = savePath.withNoFrame().withType("cln");  // Just to be sure
    if (TFileStatus(savePath).doesExist()) {
      int ret = DVGui::MsgBox(
          QObject::tr("The cleanup settings file for the %1 level already "
                      "exists.\n Do you want to overwrite it?")
              .arg(toQString(savePath.withoutParentDir())),
          QObject::tr("Overwrite"), QObject::tr("Don't Overwrite"), 0);

      if (ret == 2) return false;
    }

    CleanupSettingsModel::instance()->saveSettings(savePath);
    return true;
  }
};

//==================================================================
//    LoadSettingsPopup
//==================================================================

class LoadSettingsPopup final : public IOSettingsPopup {
public:
  LoadSettingsPopup() : IOSettingsPopup(tr("Load Cleanup Settings")) {
    setOkText(tr("Load"));
  }

  bool execute() override {
    if (m_selectedPaths.empty()) return false;

    const TFilePath &loadPath = *m_selectedPaths.begin();
    if (loadPath.isEmpty()) return false;

    if (!TSystem::doesExistFileOrLevel(loadPath)) {
      DVGui::error(tr("%1 does not exist.").arg(toQString(loadPath)));
      return false;
    }

    CleanupSettingsModel::instance()->loadSettings(loadPath);
    return true;
  }
};

}  // namespace

//******************************************************************************
//    CleanupSettingsModel implementation
//******************************************************************************

CleanupSettingsModel::CleanupSettingsModel()
    : m_listenersCount(0)
    , m_previewersCount(0)
    , m_cameraTestsCount(0)
    , m_allowedActions(FULLPROCESS)
    , m_action(NONE)
    , m_c(-1)
    , m_sl(0) {
  CleanupParameters *currentParams = getCurrentParameters();

  // Don't clone the palette. Palette changes are tracked through
  // palette handle signals.
  m_backupParams.assign(currentParams, false);

  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();

  bool ret = true;
  ret      = ret &&
        connect(sceneHandle, SIGNAL(sceneSwitched()), SLOT(onSceneSwitched()));
  assert(ret);

  onSceneSwitched();
}

//-----------------------------------------------------------------------

CleanupSettingsModel::~CleanupSettingsModel() {}

//-----------------------------------------------------------------------

CleanupSettingsModel *CleanupSettingsModel::instance() {
  static CleanupSettingsModel theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------------

CleanupParameters *CleanupSettingsModel::getCurrentParameters() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return scene ? scene->getProperties()->getCleanupParameters() : 0;
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::getCleanupFrame(TXshSimpleLevel *&sl,
                                           TFrameId &fid) {
  TApp *app = TApp::instance();

  int r = app->getCurrentFrame()->getFrame();
  int c = app->getCurrentColumn()->getColumnIndex();

  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  assert(xsh);

  const TXshCell &cell = xsh->getCell(r, c);

  fid = cell.getFrameId();
  sl  = cell.isEmpty() ? 0 : cell.getSimpleLevel();
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::attach(int objectFlag, bool doPreview) {
  if ((objectFlag & LISTENER) && (m_listenersCount++ <= 0)) connectSignals();

  bool doRebuild = false;

  if ((objectFlag & PREVIEWER) && (m_previewersCount++ <= 0)) doRebuild = true;

  if ((objectFlag & CAMERATEST) && (m_cameraTestsCount++ <= 0))
    doRebuild = true;

  if (doRebuild && doPreview) rebuildPreview();
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::detach(int objectFlag) {
  if ((objectFlag & LISTENER) && (--m_listenersCount <= 0)) disconnectSignals();

  if ((objectFlag & PREVIEWER) && (--m_previewersCount <= 0)) {
    m_previewTransformed = TRasterImageP();
    m_transform          = TAffine();
  }

  if ((objectFlag & CAMERATEST) && (--m_cameraTestsCount <= 0))
    m_cameraTestTransformed = TRasterImageP();

  assert(m_listenersCount >= 0);
  assert(m_previewersCount >= 0);
  assert(m_cameraTestsCount >= 0);
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::connectSignals() {
  TApp *app = TApp::instance();

  TFrameHandle *frameHandle   = app->getCurrentFrame();
  TColumnHandle *columnHandle = app->getCurrentColumn();
  TPaletteHandle *paletteHandle =
      app->getPaletteController()->getCurrentCleanupPalette();

  // The following are queued. This is best to prevent double-updates and
  // selection issues with cln cancels.
  bool ret = true;

  ret = ret && connect(frameHandle, SIGNAL(frameSwitched()),
                       SLOT(onCellChanged()), Qt::QueuedConnection);
  ret = ret && connect(columnHandle, SIGNAL(columnIndexSwitched()),
                       SLOT(onCellChanged()), Qt::QueuedConnection);
  ret = ret && connect(paletteHandle, SIGNAL(colorStyleChanged(bool)),
                       SLOT(onPaletteChanged()), Qt::QueuedConnection);
  ret = ret && connect(paletteHandle, SIGNAL(paletteChanged()),
                       SLOT(onPaletteChanged()), Qt::QueuedConnection);

  assert(ret);

  QMetaObject::invokeMethod(this, "onCellChanged", Qt::QueuedConnection);
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::disconnectSignals() {
  TApp *app = TApp::instance();

  TFrameHandle *frameHandle   = app->getCurrentFrame();
  TColumnHandle *columnHandle = app->getCurrentColumn();
  TPaletteHandle *paletteHandle =
      app->getPaletteController()->getCurrentCleanupPalette();

  bool ret = true;

  ret = ret && frameHandle->disconnect(this);
  ret = ret && columnHandle->disconnect(this);
  ret = ret && paletteHandle->disconnect(this);

  assert(ret);
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::setCommitMask(CommitMask allowedProcessing) {
  m_allowedActions = allowedProcessing;
  commitChanges();
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::commitChanges() {
  CleanupParameters *currentParams = getCurrentParameters();

  int action = NONE;

  // Analyze param changes
  {
    // Check for simple changes
    if (currentParams->m_path != m_backupParams.m_path) action = INTERFACE;

    // Check post-processing-related changes
    if (currentParams->m_transparencyCheckEnabled !=
            m_backupParams.m_transparencyCheckEnabled ||
        currentParams->m_noAntialias != m_backupParams.m_noAntialias ||
        currentParams->m_postAntialias != m_backupParams.m_postAntialias ||
        currentParams->m_despeckling != m_backupParams.m_despeckling ||
        currentParams->m_aaValue != m_backupParams.m_aaValue)

      action = POSTPROCESS;

    // Check for full preview rebuilds
    if (currentParams->m_lineProcessingMode !=
            m_backupParams.m_lineProcessingMode ||
        currentParams->m_autocenterType != m_backupParams.m_autocenterType ||
        currentParams->m_autoAdjustMode != m_backupParams.m_autoAdjustMode ||
        currentParams->m_camera.getRes() != m_backupParams.m_camera.getRes() ||
        currentParams->m_camera.getSize() !=
            m_backupParams.m_camera.getSize() ||
        currentParams->m_rotate != m_backupParams.m_rotate ||
        currentParams->m_flipx != m_backupParams.m_flipx ||
        currentParams->m_flipy != m_backupParams.m_flipy ||
        currentParams->m_offx != m_backupParams.m_offx ||
        currentParams->m_offy != m_backupParams.m_offy ||
        currentParams->m_sharpness != m_backupParams.m_sharpness ||
        currentParams->m_closestField != m_backupParams.m_closestField)

      action = FULLPROCESS;

    if (currentParams->m_autocenterType != CleanupTypes::AUTOCENTER_NONE) {
      // Check Autocenter changes
      if (!(currentParams->getFdgInfo() == m_backupParams.getFdgInfo()) ||
          currentParams->m_pegSide != m_backupParams.m_pegSide)

        action = FULLPROCESS;
    }
  }

  commitChanges(action);
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::commitChanges(int action) {
  CleanupParameters *currentParams = getCurrentParameters();

  if (action > NONE) {
    m_backupParams.assign(currentParams, false);

    currentParams->setDirtyFlag(true);

    // Deal with scene stuff
    if (m_clnPath.isEmpty()) {
      TApp::instance()->getCurrentScene()->setDirtyFlag(
          true);  // This should be moved outside... sadly not supported at the
                  // moment...
      CleanupParameters::GlobalParameters.assign(
          currentParams);  // The global settings are being changed
    }
  }

  // Perform actions
  int maxAction =
      std::max(action, m_action);  // Add previuosly required actions
  action = std::min(maxAction,
                    m_allowedActions);  // But only up to the allowed action
  m_action = (action == maxAction)
                 ? NONE
                 : maxAction;  // Then, update the previously required action

  if (action >= FULLPROCESS) rebuildPreview();

  if (action >= INTERFACE) emit modelChanged(action == POSTPROCESS);
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::rebuildPreview() {
  // If no previewer is active, do nothing
  if (m_previewersCount <= 0 && m_cameraTestsCount <= 0) return;

  // Reset data
  m_original = m_previewTransformed = m_cameraTestTransformed = TRasterImageP();
  m_transform                                                 = TAffine();

  // Retrieve frame to cleanup
  TXshSimpleLevel *sl;
  TFrameId fid;
  getCleanupFrame(sl, fid);

  if (sl) {
    // Inform that we're beginning a long computation
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Perform the preview
    processFrame(sl, fid);

    QApplication::restoreOverrideCursor();
  }

  emit previewDataChanged();
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::processFrame(TXshSimpleLevel *sl, TFrameId fid) {
  assert(sl);

  TRasterImageP imageToCleanup = sl->getFrameToCleanup(fid);
  if (!imageToCleanup) return;

  // Store the original image
  m_original = imageToCleanup;

  // Retrieve cleanup params
  CleanupParameters *params = getCurrentParameters();
  TCleanupper *cl           = TCleanupper::instance();

  bool doProcessing =
      (params->m_lineProcessingMode != lpNone) && (m_previewersCount > 0);
  bool doCameraTest = (m_cameraTestsCount > 0);

  // Retrieve new image dpi
  TPointD dpi;
  imageToCleanup->getDpi(dpi.x, dpi.y);
  if (dpi.x == 0 && dpi.y == 0) dpi = sl->getProperties()->getDpi();
  cl->setSourceDpi(dpi);

  // Perform primary cleanup processing
  if (doProcessing) {
    // Warning: the process() call below will put imageToCleanup = 0.
    CleanupPreprocessedImage *cpi =
        cl->process(imageToCleanup, true, m_previewTransformed, false, true,
                    true, &m_transform);
    delete cpi;
  }

  // Perform camera test processing
  if (doCameraTest) {
    m_cameraTestTransformed = m_original;
    if (params->m_autocenterType != CleanupTypes::AUTOCENTER_NONE ||
        params->m_rotate != 0 || params->m_flipx || params->m_flipy) {
      bool autocentered;
      m_cameraTestTransformed =
          cl->autocenterOnly(m_original.getPointer(), true, autocentered);

      if (params->m_autocenterType != CleanupTypes::AUTOCENTER_NONE &&
          !autocentered)
        DVGui::warning(
            QObject::tr("The autocentering failed on the current drawing."));
    }
  }
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::onSceneSwitched() {
  // When the scene switches, current parameters are reverted the scene's
  // originals.
  // So, since we store originals in CleanupParameters::GlobalParameters, we
  // copy them there.
  CleanupParameters *params = getCurrentParameters();
  CleanupParameters::GlobalParameters.assign(params);

  // The cleanupper always uses current cleanup parameters. It has to be
  // specified somewhere,
  // so let's do it once here.
  TCleanupper::instance()->setParameters(params);

  // Finally, send notifications, deal with backups, etc.
  restoreGlobalSettings();
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::onCellChanged() {
  // Process events. This slot wants to be invoked when no other slot/event
  // is pending. This is best since current selection may change inside.
  QCoreApplication::processEvents();

  TApp *app = TApp::instance();

  // Update xsheet position
  int oldC = m_c;
  m_c      = app->getCurrentColumn()->getColumnIndex();

  // Retrieve new cleanup image reference (not updated yet)
  TXshSimpleLevel *sl;
  TFrameId fid;

  /*---
   * カラムを上から走査して、最初にLevelの入っているセルのLevelパスを取ってくる
   * ---*/
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (xsh->getFrameCount() == 0) return;

  for (int f = 0; f < xsh->getFrameCount(); f++) {
    TXshCell cell = xsh->getCell(f, m_c);
    sl            = cell.getSimpleLevel();
    if (sl) {
      fid = cell.getFrameId();
      break;
    }
  }

  // Deal with cln switches
  bool rebuiltPreview = false;

  if (sl != m_sl) {
    // Build new cln path
    const TFilePath &clnPath = getClnPath(sl);

    // If there is a switch in cln paths
    if (m_clnPath != clnPath) {
      // In case, save the old cln
      if (!m_clnPath.isEmpty() && TFileStatus(m_clnPath).doesExist()) {
        if (!saveSettingsIfNeeded()) {
          // Unselect current selection, and return to the old cell
          app->getCurrentSelection()->setSelection(0);
          app->getCurrentColumn()->setColumnIndex(oldC);
          m_c = oldC;
          return;
        }
      }

      // Load the new cleanup settings, if any
      if (loadSettings(
              clnPath))  // modelChanged() emitted here, plust preview rebuild
      {
        m_clnPath      = clnPath;
        rebuiltPreview = true;
      } else if (!m_clnPath.isEmpty()) {
        // Otherwise, in case there was an old cln, reload global settings
        restoreGlobalSettings();  // modelChanged() emitted here
        rebuiltPreview = true;
      }
    }
  }

  m_sl = sl, m_fid = fid;

  if ((m_previewersCount > 0 || m_cameraTestsCount > 0) && !rebuiltPreview)
    rebuildPreview();

  emit imageSwitched();
}

//-----------------------------------------------------------------------

void CleanupSettingsModel::onPaletteChanged() { commitChanges(POSTPROCESS); }

//-----------------------------------------------------------------------

bool CleanupSettingsModel::saveSettingsIfNeeded() {
  CleanupParameters *params = getCurrentParameters();

  if (params->getDirtyFlag() && !m_clnPath.isEmpty() &&
      TFileStatus(m_clnPath).doesExist()) {
    QString question(
        QObject::tr("The cleanup settings for the current level have been "
                    "modified...\n\nDo you want to save your changes?"));

    int ret = DVGui::MsgBox(question, QObject::tr("Save"),
                            QObject::tr("Discard"), QObject::tr("Cancel"), 0);
    if (ret == 1) {
      // WARNING: This is legacy behavior, but is it really needed? I think
      // there should be no further
      // request of user interaction - why invoking the popup to choose the save
      // path?
      promptSave();
    } else if (ret == 3)
      return false;
  }

  return true;
}

//-----------------------------------------------------------------------------

void CleanupSettingsModel::restoreGlobalSettings() {
  CleanupParameters *currentParams = getCurrentParameters();
  currentParams->assign(&CleanupParameters::GlobalParameters);

  // Make sure that the current cleanup palette is set to currentParams' palette
  TApp::instance()
      ->getPaletteController()
      ->getCurrentCleanupPalette()
      ->setPalette(currentParams->m_cleanupPalette.getPointer());

  m_clnPath = TFilePath();
  m_backupParams.assign(currentParams, false);

  if (m_previewersCount > 0 || m_cameraTestsCount > 0) rebuildPreview();

  emit modelChanged(false);
  emit clnLoaded();
}

//-----------------------------------------------------------------------------

void CleanupSettingsModel::saveSettings(CleanupParameters *params,
                                        const TFilePath &clnPath) {
  if (clnPath.isEmpty() || !params) return;

  TOStream os(clnPath);

  os.openChild("version");
  os << 1 << 19;
  os.closeChild();

  params->saveData(os);
  params->setDirtyFlag(false);
}

//-----------------------------------------------------------------------------

void CleanupSettingsModel::saveSettings(const TFilePath &clnPath) {
  saveSettings(getCurrentParameters(), clnPath);

  // Saving current settings should put the model in the same situation we
  // have after a model is loaded (ie see loadSettings(clnPath))
  m_clnPath = clnPath;

  emit clnLoaded();
}

//-----------------------------------------------------------------------------

bool CleanupSettingsModel::loadSettings(CleanupParameters *params,
                                        const TFilePath &clnPath) {
  assert(params);
  if (clnPath.isEmpty() || !TSystem::doesExistFileOrLevel(clnPath))
    return false;

  TIStream *is = new TIStream(clnPath);

  CleanupParameters defaultParameters;
  params->assign(&defaultParameters);

  std::string tagName;
  is->matchTag(tagName);
  if (tagName == "version") {
    int minor, major;
    *is >> major >> minor;

    is->matchEndTag();
    is->setVersion(VersionNumber(major, minor));
  } else {
    delete is;
    is = new TIStream(clnPath);
  }

  params->loadData(*is, false);
  delete is;

  return true;
}

//-----------------------------------------------------------------------------

bool CleanupSettingsModel::loadSettings(const TFilePath &clnPath) {
  CleanupParameters *cp = getCurrentParameters();
  if (!loadSettings(cp, clnPath)) return false;

  // The same as restoreGlobalSettings()
  TApp::instance()
      ->getPaletteController()
      ->getCurrentCleanupPalette()
      ->setPalette(cp->m_cleanupPalette.getPointer());

  /*---
   * LoadSettingsPopupからこの関数が呼ばれたとき、ロードしたパラメータをGlobal設定に格納する---*/
  if (m_clnPath.isEmpty()) {
    TApp::instance()->getCurrentScene()->setDirtyFlag(
        true);  // This should be moved outside... sadly not supported at the
                // moment...
    CleanupParameters::GlobalParameters.assign(
        cp);  // The global settings are being changed
  }

  m_backupParams.assign(cp, false);

  if (m_previewersCount > 0 || m_cameraTestsCount > 0) rebuildPreview();

  emit modelChanged(false);
  emit clnLoaded();

  return true;
}

//-----------------------------------------------------------------------------

void CleanupSettingsModel::promptSave() {
  static SaveSettingsPopup *savePopup = 0;
  if (!savePopup) savePopup           = new SaveSettingsPopup;

  savePopup->setPath(getClnPath(m_sl));
  savePopup->exec();
}

//-----------------------------------------------------------------------------

void CleanupSettingsModel::promptLoad() {
  static LoadSettingsPopup *loadPopup = 0;
  if (!loadPopup) loadPopup           = new LoadSettingsPopup;

  loadPopup->setPath(getClnPath(m_sl));
  loadPopup->exec();
}

//-----------------------------------------------------------------------------

TFilePath CleanupSettingsModel::getClnPath(TXshSimpleLevel *sl) {
  if (!sl) return TFilePath();

  TFilePath clnPath(sl->getScannedPath());
  if (clnPath == TFilePath()) clnPath = sl->getPath();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return scene->decodeFilePath(clnPath).withNoFrame().withType("cln");
}

//-----------------------------------------------------------------------------

TFilePath CleanupSettingsModel::getInputPath(TXshSimpleLevel *sl) {
  const TFilePath &scannedPath = sl->getScannedPath();
  return scannedPath.isEmpty() ? sl->getPath() : scannedPath;
}

//-----------------------------------------------------------------------------

TFilePath CleanupSettingsModel::getOutputPath(TXshSimpleLevel *sl,
                                              const CleanupParameters *params) {
  const TFilePath &inPath = getInputPath(sl);  // Used just to get level name

  if (inPath.isEmpty() || !params) return TFilePath();

  bool lineProcessing = (params->m_lineProcessingMode != lpNone);
  ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();

  // Check if the cleaned up level already exists
  const TFilePath &outDir = params->getPath(scene);

  return lineProcessing ? (outDir + inPath.getWideName()).withType("tlv")
                        : (outDir + inPath.getLevelNameW()).withType("tif");
}
