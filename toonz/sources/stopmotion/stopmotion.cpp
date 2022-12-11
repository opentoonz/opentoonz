#include "stopmotion.h"

// TnzCore includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "tenv.h"
#include "tsystem.h"
#include "filebrowsermodel.h"
#include "penciltestpopup.h"
#include "tlevel_io.h"
#include "toutputproperties.h"
#include "filebrowserpopup.h"
#include "tunit.h"

#include "toonz/namebuilder.h"
#include "toonz/preferences.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/levelset.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/stage.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/levelproperties.h"
#include "toonz/tstageobjecttree.h"
#include "toonzqt/menubarcommand.h"

#include <QApplication>
#include <QCamera>
#include <QCameraInfo>
#include <QCoreApplication>
#include <QDesktopWidget>
#include <QDialog>
#include <QFile>
#include <QString>
#include <QTimer>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>

// Connected camera
TEnv::IntVar StopMotionOpacity("StopMotionOpacity", 100);
TEnv::IntVar StopMotionAlwaysLiveView("StopMotionAlwaysLiveView", 0);
TEnv::IntVar StopMotionPlaceOnXSheet("StopMotionPlaceOnXSheet", 1);
TEnv::IntVar StopMotionReviewDSec("StopMotionReviewDSec", 10);
TEnv::IntVar StopMotionIntervalDSec("StopMotionIntervalDSec", 100);
TEnv::IntVar StopMotionUseNumpad("StopMotionUseNumpad", 0);
TEnv::IntVar StopMotionDrawBeneathLevels("StopMotionDrawBeneathLevels", 1);
TEnv::IntVar StopMotionPlayCaptureSound("StopMotionPlayCaptureSound", 0);

// Connected camera
TEnv::StringVar StopMotionCameraName("CamCapCameraName", "");
// Camera resolution
TEnv::StringVar StopMotionCameraResolution("CamCapCameraResolution", "");

namespace {

TPointD getCurrentCameraDpi() {
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TDimensionD size = camera->getSize();
  TDimension res   = camera->getRes();
  return TPointD(res.lx / size.lx, res.ly / size.ly);
}

//-----------------------------------------------------------------------------

bool findCell(TXsheet *xsh, int col, const TXshCell &targetCell,
              int &bottomRowWithTheSameLevel) {
  bottomRowWithTheSameLevel = -1;
  TXshColumnP column        = const_cast<TXsheet *>(xsh)->getColumn(col);
  if (!column) return false;

  TXshCellColumn *cellColumn = column->getCellColumn();
  if (!cellColumn) return false;

  int r0, r1;
  if (!cellColumn->getRange(r0, r1)) return false;

  for (int r = r0; r <= r1; r++) {
    TXshCell cell = cellColumn->getCell(r);
    if (cell == targetCell) {
      bottomRowWithTheSameLevel = r;
      return true;
    }
    if (cell.m_level == targetCell.m_level) bottomRowWithTheSameLevel = r;
  }

  return false;
}

//-----------------------------------------------------------------------------

QChar numToLetter(int letterNum) {
  switch (letterNum) {
  case 0:
    return QChar();
    break;
  case 1:
    return 'A';
    break;
  case 2:
    return 'B';
    break;
  case 3:
    return 'C';
    break;
  case 4:
    return 'D';
    break;
  case 5:
    return 'E';
    break;
  case 6:
    return 'F';
    break;
  case 7:
    return 'G';
    break;
  case 8:
    return 'H';
    break;
  case 9:
    return 'I';
    break;
  default:
    return QChar();
    break;
  }
}

//-----------------------------------------------------------------------------

QString convertToFrameWithLetter(int value, int length = -1) {
  QString str;
  str.setNum((int)(value / 10));
  while (str.length() < length) str.push_front("0");
  QChar letter = numToLetter(value % 10);
  if (!letter.isNull()) str.append(letter);
  return str;
}

//-----------------------------------------------------------------------------

QString fidsToString(const std::vector<TFrameId> &fids,
                     bool letterOptionEnabled) {
  if (fids.empty()) return StopMotion::tr("No", "frame id");
  QString retStr("");
  if (letterOptionEnabled) {
    bool beginBlock = true;
    for (int f = 0; f < fids.size() - 1; f++) {
      int num      = fids[f].getNumber();
      int next_num = fids[f + 1].getNumber();

      if (num % 10 == 0 && num + 10 == next_num) {
        if (beginBlock) {
          retStr += convertToFrameWithLetter(num) + " - ";
          beginBlock = false;
        }
      } else {
        retStr += convertToFrameWithLetter(num) + ", ";
        beginBlock = true;
      }
    }
    retStr += convertToFrameWithLetter(fids.back().getNumber());
  } else {
    bool beginBlock = true;
    for (int f = 0; f < fids.size() - 1; f++) {
      int num      = fids[f].getNumber();
      int next_num = fids[f + 1].getNumber();
      if (num + 1 == next_num) {
        if (beginBlock) {
          retStr += QString::number(num) + " - ";
          beginBlock = false;
        }
      } else {
        retStr += QString::number(num) + ", ";
        beginBlock = true;
      }
    }
    retStr += QString::number(fids.back().getNumber());
  }
  return retStr;
}

//-----------------------------------------------------------------------------

bool getRasterLevelSize(TXshLevel *level, TDimension &dim) {
  std::vector<TFrameId> fids;
  level->getFids(fids);
  if (fids.empty()) return false;
  TXshSimpleLevel *simpleLevel = level->getSimpleLevel();
  if (!simpleLevel) return false;
  TRasterImageP rimg = (TRasterImageP)simpleLevel->getFrame(fids[0], false);
  if (!rimg || rimg->isEmpty()) return false;

  dim = rimg->getRaster()->getSize();
  return true;
}

};  // namespace

//-----------------------------------------------------------------------------

StopMotion::StopMotion() {
  m_opacity = StopMotionOpacity;

  m_webcam = new Webcam();
  m_canon  = Canon::instance();
  m_serial = new StopMotionSerial();
  m_light  = new StopMotionLight();

  m_alwaysLiveView = StopMotionAlwaysLiveView;

  m_placeOnXSheet      = StopMotionPlaceOnXSheet;
  m_reviewTimeDSec     = StopMotionReviewDSec;
  m_intervalDSec       = StopMotionIntervalDSec;
  m_useNumpadShortcuts = StopMotionUseNumpad;
  m_drawBeneathLevels  = StopMotionDrawBeneathLevels;
  m_playCaptureSound   = StopMotionPlayCaptureSound;
  m_numpadForStyleSwitching =
      Preferences::instance()->isUseNumpadForSwitchingStylesEnabled();
  setUseNumpadShortcuts(m_useNumpadShortcuts);
  m_turnOnRewind = Preferences::instance()->rewindAfterPlaybackEnabled();
  m_timer        = new QTimer(this);
  m_reviewTimer  = new QTimer(this);
  m_reviewTimer->setSingleShot(true);
  m_intervalTimer      = new QTimer(this);
  m_countdownTimer     = new QTimer(this);
  m_webcamOverlayTimer = new QTimer(this);
  m_camSnapSound       = new QSound(":Resources/camera_snap.wav");

  // Make the interval timer single-shot. When the capture finished, restart
  // timer for next frame.
  // This is because capturing and saving the image needs some time.
  m_intervalTimer->setSingleShot(true);
  m_webcamOverlayTimer->setSingleShot(true);

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  TSceneHandle *sceneHandle   = TApp::instance()->getCurrentScene();
  TFrameHandle *frameHandle   = TApp::instance()->getCurrentFrame();
  bool ret                    = true;

  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetSwitched()), this, SLOT(update()));
  ret = ret && connect(m_reviewTimer, SIGNAL(timeout()), this,
                       SLOT(onReviewTimeout()));
  ret = ret && connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));

  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));
  ret = ret && connect(frameHandle, SIGNAL(isPlayingStatusChanged()), this,
                       SLOT(onPlaybackChanged()));
  ret = ret && connect(m_intervalTimer, SIGNAL(timeout()), this,
                       SLOT(onIntervalCaptureTimerTimeout()));
  ret = ret && connect(m_webcamOverlayTimer, SIGNAL(timeout()), this,
                       SLOT(captureWebcamOnTimeout()));
  ret = ret && connect(m_canon, SIGNAL(newCanonImageReady()), this,
                       SLOT(importImage()));
  assert(ret);
  ret = ret && connect(m_canon, SIGNAL(canonCameraChanged(QString)), this,
                       SLOT(onCanonCameraChanged(QString)));
  assert(ret);

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  setToNextNewLevel();
  m_filePath = scene->getDefaultLevelPath(OVL_TYPE, m_levelName.toStdWString())
                   .getParentDir()
                   .getQString();
}

//-----------------------------------------------------------------

StopMotion::~StopMotion() {
  disconnectAllCameras();
#ifdef WITH_CANON
  m_canon->closeAll();
#endif
}

//-----------------------------------------------------------------------------

void StopMotion::onSceneSwitched() {
  disconnectAllCameras();

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = TApp::instance()->getCurrentXsheet()->getXsheet();
  setToNextNewLevel();
  m_filePath = scene->getDefaultLevelPath(OVL_TYPE, m_levelName.toStdWString())
                   .getParentDir()
                   .getQString();
  m_frameNumber = 1;
  m_liveViewImageMap.clear();

  TLevelSet *levelSet = scene->getLevelSet();
  std::vector<TXshLevel *> levels;
  levelSet->listLevels(levels);
  int size   = levels.size();
  bool found = false;
  for (int i = 0; i < size; i++) {
    TXshLevel *level = levels[i];
    if (level->getType() == OVL_XSHLEVEL) {
      TXshSimpleLevel *sl    = 0;
      sl                     = level->getSimpleLevel();
      bool isStopMotionLevel = sl->getProperties()->isStopMotionLevel();
      if (isStopMotionLevel) {
        m_filePath    = sl->getPath().getParentDir().getQString();
        m_levelName   = QString::fromStdWString(sl->getName());
        m_frameNumber = sl->getFrameCount() + 1;
        setXSheetFrameNumber(xsh->getFrameCount() + 1);
        loadXmlFile();
        buildLiveViewMap(sl);
        m_sl  = sl;
        found = true;
        break;
      }
    }
  }

  if (!found) {
    setXSheetFrameNumber(1);
  }
  emit(levelNameChanged(m_levelName));
  emit(filePathChanged(m_filePath));
  emit(frameNumberChanged(m_frameNumber));
  emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  refreshFrameInfo();
}

//-----------------------------------------------------------------

bool StopMotion::buildLiveViewMap(TXshSimpleLevel *sl) {
  m_liveViewImageMap.clear();
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  std::wstring levelName = m_levelName.toStdWString();
  if (levelName.empty()) {
    return false;
  }

  if (m_usingWebcam) {
    return false;
  }

  TFilePath liveViewFolder = scene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_LiveView"));

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());
  TFilePath actualLevelFp = scene->decodeFilePath(levelFp);
  TFilePath liveViewFp =
      scene->decodeFilePath(liveViewFolder + TFilePath(levelName + L"..jpg"));

  if (!TSystem::doesExistFileOrLevel(liveViewFolder)) return false;

  int count = sl->getFrameCount();
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  for (TFrameId id : fids) {
    int frameNumber = id.getNumber();
    if (TSystem::doesExistFileOrLevel(liveViewFp.withFrame(frameNumber))) {
      TRaster32P image;
      JpgConverter::loadJpg(liveViewFp.withFrame(frameNumber), image);
      m_liveViewImageMap.insert(std::pair<int, TRaster32P>(frameNumber, image));
    }
  }

  if (m_liveViewImageMap.size() > 0) {
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------

void StopMotion::disconnectAllCameras() {
#ifdef WITH_CANON
  if (m_liveViewStatus > LiveViewClosed) {
    m_canon->resetCanon(true);
  } else {
    m_canon->resetCanon(false);
  }
#endif

  if (m_usingWebcam) {
    m_webcam->releaseWebcam();
    m_usingWebcam = false;
  }
  m_webcam->clearWebcam();

  m_liveViewStatus = LiveViewClosed;
  setTEnvCameraName("");

  m_isTimeLapse     = false;
  m_intervalStarted = false;
  m_intervalTimer->stop();
  m_countdownTimer->stop();
  emit(intervalToggled(false));

  emit(liveViewChanged(false));
  emit(liveViewStopped());
  emit(newCameraSelected(0, false));
  toggleNumpadShortcuts(false);
}

//-----------------------------------------------------------------

void StopMotion::onPlaybackChanged() {
  if (TApp::instance()->getCurrentFrame()->isPlaying() ||
      m_liveViewStatus == LiveViewClosed)
    return;

  int r0, r1, step;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->getProperties()->getPreviewProperties()->getRange(r0, r1, step);
  if (r1 > -1) return;

  int frame     = TApp::instance()->getCurrentFrame()->getFrame();
  int lastFrame = TApp::instance()->getCurrentFrame()->getMaxFrameIndex();
  if (m_xSheetFrameNumber - 1 == frame + 1) {
    TApp::instance()->getCurrentFrame()->setFrame(m_xSheetFrameNumber - 1);
  }
}

//-----------------------------------------------------------------

void StopMotion::setOpacity(int opacity) {
  m_opacity         = opacity;
  StopMotionOpacity = m_opacity;
  emit(opacityChanged(m_opacity));
}

//-----------------------------------------------------------------

void StopMotion::lowerOpacity() {
  int opacity = round((double)m_opacity / 255.0 * 10.0);
  opacity *= 10;
  opacity -= 10;
  m_opacity         = double(opacity) / 100.0 * 255.0;
  m_opacity         = std::max(0, m_opacity);
  StopMotionOpacity = m_opacity;
  emit(opacityChanged(m_opacity));
}

//-----------------------------------------------------------------

void StopMotion::raiseOpacity() {
  int opacity = round((double)m_opacity / 255.0 * 10.0);
  opacity *= 10;
  opacity += 10;
  m_opacity         = double(opacity) / 100.0 * 255.0;
  m_opacity         = std::min(255, m_opacity);
  StopMotionOpacity = m_opacity;
  emit(opacityChanged(m_opacity));
}

//-----------------------------------------------------------------

void StopMotion::setAlwaysLiveView(bool on) {
  m_alwaysLiveView         = on;
  StopMotionAlwaysLiveView = int(on);
  emit(liveViewOnAllFramesSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setPlaceOnXSheet(bool on) {
  m_placeOnXSheet         = on;
  StopMotionPlaceOnXSheet = int(on);
  emit(placeOnXSheetSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setReviewTimeDSec(int timeDSec) {
  m_reviewTimeDSec     = timeDSec;
  StopMotionReviewDSec = timeDSec;
  emit(reviewTimeChangedSignal(timeDSec));
}

//-----------------------------------------------------------------

void StopMotion::jumpToCameraFrame() {
  if (m_liveViewStatus == LiveViewPaused && !m_userCalledPause) {
    m_liveViewStatus = LiveViewOpen;
  }
  if (m_hasLineUpImage) m_showLineUpImage = true;
  TApp::instance()->getCurrentFrame()->setFrame(m_xSheetFrameNumber - 1);
}

//-----------------------------------------------------------------

void StopMotion::removeStopMotionFrame() {
  if (m_xSheetFrameNumber == 1) return;
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  int row = m_xSheetFrameNumber - 2;

  // find which column the level is on.
  // check with the current column first
  int col       = app->getCurrentColumn()->getColumnIndex();
  TXshCell cell = xsh->getCell(row, col);
  TXshSimpleLevelP sl;
  bool found = false;
  if (!cell.isEmpty()) {
    if (cell.getSimpleLevel() != 0) {
      sl = cell.getSimpleLevel();
      if (sl.getPointer()->getName() == m_levelName.toStdWString()) {
        found = true;
      }
    }
  }

  if (!found) {
    int cols = xsh->getColumnCount();
    for (int i = 0; i < cols; i++) {
      cell = xsh->getCell(row, i);
      if (!cell.isEmpty()) {
        if (cell.getSimpleLevel() != 0) {
          sl = cell.getSimpleLevel();
          if (sl.getPointer()->getName() == m_levelName.toStdWString()) {
            found = true;
            col   = i;
            break;
          }
        }
      }
    }
  }
  if (!found) {
    // DVGui::error(tr("Could not find an xsheet level with  the current
    // level"));
    return;
  }

  TXshCellColumn *xshCellColumn = xsh->getColumn(col)->getCellColumn();
  if (!xshCellColumn) return;

  xshCellColumn->removeCells(row, 1);

  app->getCurrentScene()->getScene()->getXsheet()->updateFrameCount();
  setXSheetFrameNumber(m_xSheetFrameNumber - 1);
  app->getCurrentFrame()->prevFrame();
  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------

void StopMotion::setUseNumpadShortcuts(bool on) {
  m_useNumpadShortcuts = on;
  StopMotionUseNumpad  = int(on);
  emit(useNumpadSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setDrawBeneathLevels(bool on) {
  m_drawBeneathLevels         = on;
  StopMotionDrawBeneathLevels = int(on);
  emit(drawBeneathLevelsSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::toggleInterval(bool on) {
  m_isTimeLapse = on;
  emit(intervalToggled(on));
}

//-----------------------------------------------------------------

void StopMotion::startInterval() {
  if (m_liveViewStatus > 1) {
    m_intervalTimer->start(m_intervalDSec * 100);
    if (m_intervalDSec != 0) m_countdownTimer->start(100);
    m_intervalStarted = true;
    emit(intervalStarted());
  } else {
    DVGui::warning(tr("Please start live view before using time lapse."));
    m_intervalStarted = false;
    emit(intervalStopped());
  }
}

//-----------------------------------------------------------------

void StopMotion::stopInterval() {
  m_intervalTimer->stop();
  m_countdownTimer->stop();
  m_intervalStarted = false;
  emit(intervalStopped());
}

//-----------------------------------------------------------------

void StopMotion::setIntervalDSec(int value) {
  m_intervalDSec         = value;
  StopMotionIntervalDSec = value;
  emit(intervalAmountChanged(value));
}

//-----------------------------------------------------------------

void StopMotion::onIntervalCaptureTimerTimeout() {
  if (m_liveViewStatus > 0) {
    captureImage();
  } else {
    DVGui::warning(tr("Please start live view before using time lapse."));
    m_intervalStarted = false;
    emit(intervalStopped());
  }
}

//-----------------------------------------------------------------

void StopMotion::restartInterval() {
  // restart interval timer for capturing next frame (it is single shot)
  if (m_isTimeLapse && m_intervalStarted) {
    m_intervalTimer->start(m_intervalDSec * 100);
    // restart the count down as well (for aligning the timing. It is not
    // single shot)
    if (m_intervalDSec != 0) m_countdownTimer->start(100);
  }
}

//-----------------------------------------------------------------

void StopMotion::toggleNumpadShortcuts(bool on) {
  // can't just return if this feature is off
  // it could have been toggled while the camera was active
  if (!m_useNumpadShortcuts) on = false;
  CommandManager *comm = CommandManager::instance();

  if (on) {
    m_oldActionMap.clear();
    // if turning it on, get all old shortcuts
    if (m_numpadForStyleSwitching) {
      Preferences::instance()->setValue(useNumpadForSwitchingStyles, false);
    }
    std::string shortcut;
    QAction *action;
    for (int i = 0; i <= 9; i++) {
      shortcut = QString::number(i).toStdString();
      action   = comm->getActionFromShortcut(shortcut);
      if (action) {
        m_oldActionMap.insert(
            std::pair<std::string, QAction *>(shortcut, action));
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
    }
    shortcut = "+";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = "-";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = "Enter";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = "Backspace";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = "Return";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = "*";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = ".";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }
    shortcut = "/";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action->setShortcut(QKeySequence(""));
      action = NULL;
    }

    // now set all new shortcuts
    action = comm->getAction(MI_PrevFrame);
    if (action) {
      action->setShortcut(QKeySequence("1"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionNextFrame);
    if (action) {
      action->setShortcut(QKeySequence("2"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionJumpToCamera);
    if (action) {
      action->setShortcut(QKeySequence("3"));
      action = NULL;
    }
    action = comm->getAction(MI_Loop);
    if (action) {
      action->setShortcut(QKeySequence("8"));
      action = NULL;
    }
    action = comm->getAction(MI_Play);
    if (action) {
      action->setShortcut(QKeySequence("0"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionRaiseOpacity);
    if (action) {
      action->setShortcut(QKeySequence("+"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionLowerOpacity);
    if (action) {
      action->setShortcut(QKeySequence("-"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionCapture);
    if (action) {
      action->setShortcut(QKeySequence("Return"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionRemoveFrame);
    if (action) {
      action->setShortcut(QKeySequence("Backspace"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionToggleLiveView);
    if (action) {
      action->setShortcut(QKeySequence("5"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionToggleUseLiveViewImages);
    if (action) {
      action->setShortcut(QKeySequence("."));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionToggleZoom);
    if (action) {
      action->setShortcut(QKeySequence("*"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionPickFocusCheck);
    if (action) {
      action->setShortcut(QKeySequence("/"));
      action = NULL;
    }
    action = comm->getAction(MI_ShortPlay);
    if (action) {
      action->setShortcut(QKeySequence("6"));
      action = NULL;
    }

  } else {
    // unset the new shortcuts first
    if (m_oldActionMap.size() > 0) {
      QAction *action;
      action = comm->getAction(MI_PrevFrame);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionNextFrame);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionJumpToCamera);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_Loop);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_Play);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionCapture);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionLowerOpacity);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionRaiseOpacity);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionToggleLiveView);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionToggleUseLiveViewImages);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionToggleZoom);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_ShortPlay);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionPickFocusCheck);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionRemoveFrame);
      if (action) {
        action->setShortcut(
            QKeySequence(comm->getShortcutFromAction(action).c_str()));
        action = NULL;
      }

      // now put back the old shortcuts
      auto it = m_oldActionMap.begin();
      while (it != m_oldActionMap.end()) {
        it->second->setShortcut(QKeySequence(it->first.c_str()));
        it++;
      }
      if (m_numpadForStyleSwitching) {
        std::string shortcut;
        QAction *action;
        for (int i = 0; i <= 9; i++) {
          shortcut = QString::number(i).toStdString();
          action   = comm->getActionFromShortcut(shortcut);
          if (action) {
            action->setShortcut(QKeySequence(""));
            action = NULL;
          }
        }
        Preferences::instance()->setValue(useNumpadForSwitchingStyles, true);
      }
    }
  }
}

//-----------------------------------------------------------------

void StopMotion::toggleNumpadForFocusCheck(bool on) {
  CommandManager *comm = CommandManager::instance();
  if (m_useNumpadShortcuts) {
    if (on) {
      QAction *action;
      action = comm->getAction(MI_PrevFrame);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionNextFrame);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionJumpToCamera);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_Loop);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_Play);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionToggleLiveView);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_ShortPlay);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionCapture);
      if (action) {
        action->setShortcut(QKeySequence(""));
        action = NULL;
      }
    } else {
      QAction *action;
      action = comm->getAction(MI_PrevFrame);
      if (action) {
        action->setShortcut(QKeySequence("1"));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionNextFrame);
      if (action) {
        action->setShortcut(QKeySequence("2"));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionJumpToCamera);
      if (action) {
        action->setShortcut(QKeySequence("3"));
        action = NULL;
      }
      action = comm->getAction(MI_Loop);
      if (action) {
        action->setShortcut(QKeySequence("8"));
        action = NULL;
      }
      action = comm->getAction(MI_Play);
      if (action) {
        action->setShortcut(QKeySequence("0"));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionToggleLiveView);
      if (action) {
        action->setShortcut(QKeySequence("5"));
        action = NULL;
      }
      action = comm->getAction(MI_ShortPlay);
      if (action) {
        action->setShortcut(QKeySequence("6"));
        action = NULL;
      }
      action = comm->getAction(MI_StopMotionCapture);
      if (action) {
        action->setShortcut(QKeySequence("Return"));
        action = NULL;
      }
    }
  }
}

//-----------------------------------------------------------------

void StopMotion::setXSheetFrameNumber(int frameNumber) {
  m_xSheetFrameNumber = frameNumber;
  TApp::instance()->getCurrentFrame()->setFrame(frameNumber - 1);
  loadLineUpImage();
  emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  m_serial->sendSerialData();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------

bool StopMotion::loadLineUpImage() {
  if (m_liveViewStatus == LiveViewClosed || m_userCalledPause) return false;
  int row;
  if (m_xSheetFrameNumber == 1) {
    row = 0;
  } else {
    row = m_xSheetFrameNumber - 2;
  }
  m_hasLineUpImage = loadLiveViewImage(row, m_lineUpImage);
  return m_hasLineUpImage;
}

//-----------------------------------------------------------------

bool StopMotion::loadLiveViewImage(int row, TRaster32P &image) {
  // first see if the level exists in the current level set
  ToonzScene *currentScene = TApp::instance()->getCurrentScene()->getScene();
  TLevelSet *levelSet      = currentScene->getLevelSet();

  std::wstring levelName = m_levelName.toStdWString();

  // level with the same name
  TXshLevel *level_sameName = levelSet->getLevel(levelName);

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());

  // level with the same path
  TXshLevel *level_samePath = levelSet->getLevel(*(currentScene), levelFp);

  // TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);
  TXshSimpleLevelP sl;
  if (level_sameName && level_samePath && level_sameName == level_samePath) {
    sl                 = dynamic_cast<TXshSimpleLevel *>(level_sameName);
    bool isRasterLevel = sl && (sl->getType() == OVL_XSHLEVEL);
    if (!isRasterLevel) {
      return false;
    }
  } else
    return false;

  // next we need to find the column the level is on
  TApp *app    = TApp::instance();
  TXsheet *xsh = currentScene->getXsheet();
  int col      = app->getCurrentColumn()->getColumnIndex();

  int foundCol = -1;
  // most possibly, it's in the current column
  int rowCheck;
  findCell(xsh, col, TXshCell(level_sameName, TFrameId(1)), rowCheck);
  if (rowCheck >= 0) {
    foundCol = col;
  } else {
    // search entire xsheet
    for (int c = 0; c < xsh->getColumnCount(); c++) {
      if (c == col) continue;
      findCell(xsh, c, TXshCell(level_sameName, TFrameId(1)), rowCheck);
      if (rowCheck >= 0) {
        foundCol = c;
      }
    }
  }
  if (rowCheck < 0) return false;

  // note found row represents the last row found that uses
  // the active level
  TXshCell cell = xsh->getCell(row, foundCol);
  if (!(cell.getSimpleLevel() != 0 &&
        cell.getSimpleLevel() == level_sameName->getSimpleLevel())) {
    return false;
  }
  TFrameId frameId = xsh->getCell(row, foundCol).getFrameId();

  int frameNumber = frameId.getNumber();

  if (m_usingWebcam) {
    if (frameNumber > 0) {
      image = sl->getFrame(frameId, false)->raster();
      return true;
    } else
      return false;
  }

  // first check if the image is in the live view map
  std::map<int, TRaster32P>::iterator it;
  it = m_liveViewImageMap.find(frameNumber);
  if (it != m_liveViewImageMap.end()) {
    image = m_liveViewImageMap.find(frameNumber)->second;
    return true;
  }

  // it's not in the map
  // now check to see if a file actually exists
  // then put it in the map
  TFilePath liveViewFolder = currentScene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_LiveView"));
  TFilePath liveViewFp = currentScene->decodeFilePath(
      liveViewFolder + TFilePath(levelName + L"..jpg"));
  TFilePath liveViewFile(liveViewFp.withFrame(frameNumber));
  if (TFileStatus(liveViewFile).doesExist()) {
    if (JpgConverter::loadJpg(liveViewFile, image)) {
      m_liveViewImageMap.insert(std::pair<int, TRaster32P>(frameNumber, image));
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------

void StopMotion::setFrameNumber(int frameNumber) {
  m_frameNumber = frameNumber;
  emit(frameNumberChanged(m_frameNumber));
}

//-----------------------------------------------------------------------------

void StopMotion::nextFrame() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    int f = m_frameNumber;
    if (f % 10 == 0)  // next number
      m_frameNumber = ((int)(f / 10) + 1) * 10;
    else  // next alphabet
      m_frameNumber = f + 1;
  } else
    m_frameNumber = m_frameNumber + 1;
  emit(frameNumberChanged(m_frameNumber));
  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void StopMotion::lastFrame() {}

//-----------------------------------------------------------------------------

void StopMotion::previousFrame() {
  int f = m_frameNumber;
  if (f > 1) {
    if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
      if (f % 10 == 0)  // next number
        m_frameNumber = ((int)(f / 10) - 1) * 10;
      else  // next alphabet
        m_frameNumber = f - 1;
    } else
      m_frameNumber = f - 1;
    emit(frameNumberChanged(m_frameNumber));
    refreshFrameInfo();
  }
}

//-----------------------------------------------------------------

void StopMotion::setLevelName(QString levelName) { m_levelName = levelName; }

//-----------------------------------------------------------------

void StopMotion::nextName() {
  std::unique_ptr<FlexibleNameCreator> nameCreator(new FlexibleNameCreator());
  if (!nameCreator->setCurrent(m_levelName.toStdWString())) {
    setToNextNewLevel();
    return;
  }

  std::wstring levelName = nameCreator->getNext();
  updateLevelNameAndFrame(levelName);
}

//-----------------------------------------------------------------

void StopMotion::previousName() {
  std::unique_ptr<FlexibleNameCreator> nameCreator(new FlexibleNameCreator());

  std::wstring levelName;

  // if the current level name is non-sequential, then try to switch the last
  // sequential level in the scene.
  if (!nameCreator->setCurrent(m_levelName.toStdWString())) {
    TLevelSet *levelSet =
        TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
    nameCreator->setCurrent(L"ZZ");
    for (;;) {
      levelName = nameCreator->getPrevious();
      if (levelSet->getLevel(levelName) != 0) break;
      if (levelName == L"A") {
        setToNextNewLevel();
        return;
      }
    }
  } else
    levelName = nameCreator->getPrevious();

  updateLevelNameAndFrame(levelName);
}

//-----------------------------------------------------------------

void StopMotion::setFileType(QString fileType) {
  m_fileType = fileType;
  emit(fileTypeChanged(m_fileType));
}

//-----------------------------------------------------------------

void StopMotion::setFilePath(QString filePath) {
  m_filePath = filePath;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath saveInPath(filePath.toStdWString());
  scene->getProperties()->setCameraCaptureSaveInPath(saveInPath);
  refreshFrameInfo();

  emit(filePathChanged(m_filePath));
}

//-----------------------------------------------------------------

void StopMotion::setSubsamplingValue(int subsampling) {
  m_subsampling = subsampling;
}

//-----------------------------------------------------------------

void StopMotion::getSubsampling() {
  ToonzScene *currentScene = TApp::instance()->getCurrentScene()->getScene();
  TLevelSet *levelSet      = currentScene->getLevelSet();

  std::wstring levelName = m_levelName.toStdWString();

  // level with the same name
  TXshLevel *level_sameName = levelSet->getLevel(levelName);

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());

  // level with the same path
  TXshLevel *level_samePath = levelSet->getLevel(*(currentScene), levelFp);

  TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);

  if (level_sameName && level_samePath && level_sameName == level_samePath) {
    TXshSimpleLevelP m_sl;
    m_sl               = dynamic_cast<TXshSimpleLevel *>(level_sameName);
    bool isRasterLevel = m_sl && (m_sl->getType() == OVL_XSHLEVEL);
    if (isRasterLevel) {
      int currSubsampling = m_sl->getProperties()->getSubsampling();
      m_subsampling       = currSubsampling;
      emit(subsamplingChanged(m_subsampling));
    } else
      emit(subsamplingChanged(-1));
  } else
    emit(subsamplingChanged(-1));
}

//-----------------------------------------------------------------------------

void StopMotion::setPlayCaptureSound(bool on) {
  m_playCaptureSound         = on;
  StopMotionPlayCaptureSound = on;
  emit(playCaptureSignal(on));
}

//-----------------------------------------------------------------------------

void StopMotion::update() { getSubsampling(); }

//-----------------------------------------------------------------------------

void StopMotion::setSubsampling() {
  ToonzScene *currentScene = TApp::instance()->getCurrentScene()->getScene();
  TLevelSet *levelSet      = currentScene->getLevelSet();

  std::wstring levelName = m_levelName.toStdWString();

  // level with the same name
  TXshLevel *level_sameName = levelSet->getLevel(levelName);

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());

  // level with the same path
  TXshLevel *level_samePath = levelSet->getLevel(*(currentScene), levelFp);

  TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);

  if (level_sameName && level_samePath && level_sameName == level_samePath) {
    TXshSimpleLevelP m_sl;
    m_sl               = dynamic_cast<TXshSimpleLevel *>(level_sameName);
    bool isRasterLevel = m_sl && (m_sl->getType() & RASTER_TYPE);
    if (isRasterLevel) {
      int currSubsampling = m_sl->getProperties()->getSubsampling();
      int newSubsampling  = m_subsampling;
      if (currSubsampling != newSubsampling) {
        m_sl->getProperties()->setSubsampling(newSubsampling);
        m_sl->invalidateFrames();
        TApp::instance()->getCurrentScene()->setDirtyFlag(true);
        TApp::instance()
            ->getCurrentXsheet()
            ->getXsheet()
            ->getStageObjectTree()
            ->invalidateAll();
        TApp::instance()->getCurrentLevel()->notifyLevelChange();
        emit(subsamplingChanged(m_subsampling));
      }
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotion::onTimeout() {
  int currentFrame = TApp::instance()->getCurrentFrame()->getFrame();

  if ((m_liveViewStatus > LiveViewClosed && m_liveViewStatus < LiveViewPaused &&
       !TApp::instance()->getCurrentFrame()->isPlaying()) ||
      (m_liveViewStatus == LiveViewPaused && !m_userCalledPause)) {
    if (getAlwaysLiveView() || (currentFrame >= m_xSheetFrameNumber - 2)) {
      if (!m_usingWebcam) {
#ifdef WITH_CANON
        bool success = m_canon->downloadEVFData();
        if (success) {
          setLiveViewImage();
        } else {
          m_hasLiveViewImage = false;
        }
#endif
      } else {
        bool success = m_webcam->getWebcamImage(m_liveViewImage);
        if (success) {
          setLiveViewImage();
        } else {
          m_hasLiveViewImage = false;
        }
      }
      if ((!getAlwaysLiveView() &&
           !(currentFrame >= m_xSheetFrameNumber - 2)) ||
          m_canon->m_pickLiveViewZoom) {
        m_showLineUpImage = false;
      } else {
        m_showLineUpImage = true;
      }
    } else if (m_liveViewStatus == LiveViewOpen) {
      m_liveViewStatus = LiveViewPaused;
      TApp::instance()->getCurrentScene()->notifySceneChanged();
    }
  }
  //  else if (m_liveViewStatus == LiveViewPaused && !m_userCalledPause) {
  //    if (getAlwaysLiveView() || (currentFrame == m_xSheetFrameNumber - 1)) {
  //      if (!m_usingWebcam) {
  //#ifdef WITH_CANON
  //        bool success = m_canon->downloadEVFData();
  //        if (success) {
  //          setLiveViewImage();
  //        } else {
  //          m_hasLiveViewImage = false;
  //        }
  //#endif
  //      } else {
  //        bool success = m_webcam->getWebcamImage(m_liveViewImage);
  //        if (success) {
  //          setLiveViewImage();
  //        } else {
  //          m_hasLiveViewImage = false;
  //        }
  //      }
  //    }
  //  }
}

//-----------------------------------------------------------------------------

void StopMotion::setLiveViewImage() {
  m_hasLiveViewImage = true;

  // make sure not to set to LiveViewOpen if it has been turned off
  if (m_liveViewStatus > LiveViewClosed && !m_userCalledPause) {
    m_liveViewStatus = LiveViewOpen;
  }

  if (m_liveViewDpi.x == 0.0 || m_liveViewImageDimensions.lx == 0) {
    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimensionD size = camera->getSize();
    m_liveViewImageDimensions =
        TDimension(m_liveViewImage->getLx(), m_liveViewImage->getLy());
    double minimumDpi = std::min(m_liveViewImageDimensions.lx / size.lx,
                                 m_liveViewImageDimensions.ly / size.ly);
    m_liveViewDpi     = TPointD(minimumDpi, minimumDpi);

    if (!m_usingWebcam) {
      minimumDpi     = std::min(m_fullImageDimensions.lx / size.lx,
                            m_fullImageDimensions.ly / size.ly);
      m_fullImageDpi = TPointD(minimumDpi, minimumDpi);
    } else {
      m_fullImageDimensions = m_liveViewImageDimensions;
      m_fullImageDpi        = m_liveViewDpi;
    }

    emit(newDimensions());
  }
  emit(newLiveViewImageReady());
}
//
////-----------------------------------------------------------------------------
//
// void StopMotion::setLiveViewImage() {
//    m_hasLiveViewImage = true;
//    m_liveViewStatus = LiveViewOpen;
//    if (m_hasLiveViewImage &&
//        (m_liveViewDpi.x == 0.0 || m_liveViewImageDimensions.lx == 0)) {
//        TCamera* camera =
//            TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
//        TDimensionD size = camera->getSize();
//        m_liveViewImageDimensions =
//            TDimension(m_liveViewImage->getLx(), m_liveViewImage->getLy());
//        double minimumDpi = std::min(m_liveViewImageDimensions.lx / size.lx,
//            m_liveViewImageDimensions.ly / size.ly);
//        m_liveViewDpi = TPointD(minimumDpi, minimumDpi);
//
//        m_fullImageDimensions = m_liveViewImageDimensions;
//
//        m_fullImageDpi = m_liveViewDpi;
//
//        emit(newDimensions());
//    }
//
//    emit(newLiveViewImageReady());
//}

//-----------------------------------------------------------------------------

void StopMotion::onReviewTimeout() {
  if (m_liveViewStatus > LiveViewClosed) {
    m_liveViewStatus = LiveViewOpen;
    m_timer->start(40);
  }
  TApp::instance()->getCurrentFrame()->setFrame(m_xSheetFrameNumber - 1);
}

//-----------------------------------------------------------------------------

bool StopMotion::importImage() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  std::wstring levelName = m_levelName.toStdWString();
  if (levelName.empty()) {
    DVGui::error(
        tr("No level name specified: please choose a valid level name"));
    return false;
  }

  if (m_usingWebcam) {
    m_newImage = m_liveViewImage;
  }

  m_light->hideOverlays();

  int frameNumber = m_frameNumber;

  /* create parent directory if it does not exist */
  TFilePath parentDir     = scene->decodeFilePath(TFilePath(m_filePath));
  TFilePath fullResFolder = scene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_FullRes"));
  TFilePath liveViewFolder = scene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_LiveView"));

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());
  TFilePath actualLevelFp = scene->decodeFilePath(levelFp);
  TFilePath actualFile(actualLevelFp.withFrame(frameNumber));
  TFilePath fullResFp =
      scene->decodeFilePath(fullResFolder + TFilePath(levelName + L"..jpg"));
  TFilePath fullResFile(fullResFp.withFrame(frameNumber));

  TFilePath liveViewFp =
      scene->decodeFilePath(liveViewFolder + TFilePath(levelName + L"..jpg"));
  TFilePath liveViewFile(liveViewFp.withFrame(frameNumber));

  TFilePath tempFile = parentDir + "temp.jpg";

  TXshSimpleLevel *sl = 0;
  TXshLevel *level    = scene->getLevelSet()->getLevel(levelName);
  enum State { NEWLEVEL = 0, ADDFRAME, OVERWRITE } state;

  /* if the level already exists in the scene cast */
  if (level) {
    /* if the existing level is not a raster level, then return */
    if (level->getType() != OVL_XSHLEVEL) {
      DVGui::error(
          tr("The level name specified is already used: please choose a "
             "different level name."));
      return false;
    }
    /* if the existing level does not match file path and pixel size, then
     * return */
    sl = level->getSimpleLevel();
    if (scene->decodeFilePath(sl->getPath()) != actualLevelFp) {
      DVGui::error(
          tr("The save in path specified does not match with the existing "
             "level."));
      return false;
    }
    if (sl->getProperties()->getImageRes() !=
        TDimension(m_newImage->getLx(), m_newImage->getLy())) {
      DVGui::error(tr(
          "The captured image size does not match with the existing level."));
      return false;
    }
    /* if the level already have the same frame, then ask if overwrite it */
    TFilePath frameFp(actualLevelFp.withFrame(frameNumber));
    if (TFileStatus(frameFp).doesExist()) {
      QString question =
          tr("File %1 already exists.\nDo you want to overwrite it?")
              .arg(toQString(frameFp));
      int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                              QObject::tr("Cancel"));
      if (ret == 0 || ret == 2) return false;
      state = OVERWRITE;
    } else
      state = ADDFRAME;
  }
  /* if the level does not exist in the scene cast */
  else {
    /* if the file does exist, load it first */
    if (TSystem::doesExistFileOrLevel(actualLevelFp)) {
      level = scene->loadLevel(actualLevelFp);
      if (!level) {
        DVGui::error(tr("Failed to load %1.").arg(toQString(actualLevelFp)));
        return false;
      }

      /* if the loaded level does not match in pixel size, then return */
      sl = level->getSimpleLevel();
      if (!sl || sl->getProperties()->getImageRes() !=
                     TDimension(m_newImage->getLx(), m_newImage->getLy())) {
        DVGui::error(
            tr("The captured image size does not match with the existing "
               "level."));
        return false;
      }

      /* confirm overwrite */
      TFilePath frameFp(actualLevelFp.withFrame(frameNumber));
      if (TFileStatus(frameFp).doesExist()) {
        QString question =
            tr("File %1 already exists.\nDo you want to overwrite it?")
                .arg(toQString(frameFp));
        int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                                QObject::tr("Cancel"));
        if (ret == 0 || ret == 2) return false;
      }
    }
    /* if the file does not exist, then create a new level */
    else {
      TXshLevel *level = scene->createNewLevel(OVL_XSHLEVEL, levelName,
                                               TDimension(), 0, levelFp);
      sl               = level->getSimpleLevel();
      sl->setPath(levelFp, true);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
      TPointD dpi;
      // Right now always set the dpi to scale to the camera width
      if (Preferences::instance()->getPixelsOnly() && false)
        dpi = getCurrentCameraDpi();
      // Compute the dpi so that the image will fit
      // to the camera frame
      else {
        TCamera *camera =
            TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
        TDimensionD size  = camera->getSize();
        double minimumDpi = std::min(m_newImage->getLx() / size.lx,
                                     m_newImage->getLy() / size.ly);
        dpi               = TPointD(minimumDpi, minimumDpi);
      }
      sl->getProperties()->setDpi(dpi.x);
      sl->getProperties()->setImageDpi(dpi);
      sl->getProperties()->setImageRes(
          TDimension(m_newImage->getLx(), m_newImage->getLy()));
    }

    state = NEWLEVEL;
    getSubsampling();
  }

  if (!TFileStatus(parentDir).doesExist()) {
    QString question;
    question = tr("Folder %1 doesn't exist.\nDo you want to create it?")
                   .arg(toQString(parentDir));
    int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
    if (ret == 0 || ret == 2) return false;
    try {
      TSystem::mkDir(parentDir);
      DvDirModel::instance()->refreshFolder(parentDir.getParentDir());
    } catch (...) {
      DVGui::error(tr("Unable to create") + toQString(parentDir));
      return false;
    }
  }
  if (!m_usingWebcam) {
    if (!TFileStatus(fullResFolder).doesExist()) {
      try {
        TSystem::mkDir(fullResFolder);
        DvDirModel::instance()->refreshFolder(fullResFolder.getParentDir());
      } catch (...) {
        DVGui::error(tr("Unable to create") + toQString(fullResFolder));
        return false;
      }
    }
    if (!TFileStatus(liveViewFolder).doesExist()) {
      try {
        TSystem::mkDir(liveViewFolder);
        DvDirModel::instance()->refreshFolder(liveViewFolder.getParentDir());
      } catch (...) {
        DVGui::error(tr("Unable to create") + toQString(liveViewFolder));
        return false;
      }
    }
  }

  // move the temp file
  if (!m_usingWebcam) {
    if (m_canon->m_useScaledImages) {
      TSystem::copyFile(fullResFile, tempFile);
      TSystem::deleteFile(tempFile);
    }
    if (m_hasLineUpImage) {
      JpgConverter::saveJpg(m_lineUpImage, liveViewFile);
      // check the live view image map to see if there is already
      // an image with this framenumber.  Overwrite if it exists
      std::map<int, TRaster32P>::iterator it;
      it = m_liveViewImageMap.find(frameNumber);
      if (it != m_liveViewImageMap.end()) {
        m_liveViewImageMap.find(frameNumber)->second = m_lineUpImage;
        return true;
      } else {
        m_liveViewImageMap.insert(
            std::pair<int, TRaster32P>(m_frameNumber, m_lineUpImage));
      }
    }
  }

  TFrameId fid(frameNumber);
  TPointD levelDpi = sl->getDpi();
  /* create the raster */
  TRaster32P raster = m_newImage;

  TRasterImageP ri(raster);
  ri->setDpi(levelDpi.x, levelDpi.y);

  /* setting the frame */
  sl->setFrame(fid, ri);

  /* set dirty flag */
  sl->getProperties()->setDirtyFlag(true);
  sl->getProperties()->setIsStopMotion(true);
  sl->setIsReadOnly(true);

  // if (m_saveOnCaptureCB->isChecked()) sl->save();
  // for now always save.  This can be tweaked later
  sl->save();
  m_sl = sl;
  if (getReviewTimeDSec() > 0 && !m_isTimeLapse) {
    m_liveViewStatus = LiveViewPaused;
    m_reviewTimer->start(getReviewTimeDSec() * 100);
  }

  /* placement in xsheet */
  if (!getPlaceOnXSheet()) {
    postImportProcess();
    return true;
  }
  int row = m_xSheetFrameNumber - 1;
  int col = app->getCurrentColumn()->getColumnIndex();

  // if the level is newly created or imported, then insert a new column
  if (state == NEWLEVEL) {
    if (!xsh->isColumnEmpty(col)) {
      col += 1;
      xsh->insertColumn(col);
    }
    xsh->insertCells(row, col);
    xsh->setCell(row, col, TXshCell(sl, fid));
    app->getCurrentColumn()->setColumnIndex(col);
    if (getReviewTimeDSec() == 0 || m_isTimeLapse)
      app->getCurrentFrame()->setFrame(row + 1);
    m_xSheetFrameNumber = row + 2;
    emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
    postImportProcess();
    // if (m_newImage->getLx() > 2000) {
    //  m_subsampling = 4;
    //  setSubsampling();
    //}
    return true;
  }

  // state == OVERWRITE, ADDFRAME

  // if the same cell is already in the column, then just replace the content
  // and do not set a new cell
  int foundCol, foundRow = -1;
  // most possibly, it's in the current column
  int rowCheck;
  if (findCell(xsh, col, TXshCell(sl, fid), rowCheck)) {
    postImportProcess();
    return true;
  }
  if (rowCheck >= 0) {
    foundRow = rowCheck;
    foundCol = col;
  }
  // search entire xsheet
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    if (c == col) continue;
    if (findCell(xsh, c, TXshCell(sl, fid), rowCheck)) {
      postImportProcess();
      return true;
    }
    if (rowCheck >= 0) {
      foundRow = rowCheck;
      foundCol = c;
    }
  }

  // note found row represents the last row found that uses
  // the active level

  // if there is a column containing the same level
  if (foundRow >= 0) {
    // put the cell at the bottom
    xsh->insertCells(row, foundCol);
    xsh->setCell(row, foundCol, TXshCell(sl, fid));
    app->getCurrentColumn()->setColumnIndex(foundCol);
    if (getReviewTimeDSec() == 0 || m_isTimeLapse)
      app->getCurrentFrame()->setFrame(row + 1);
    m_xSheetFrameNumber = row + 2;
    emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  }
  // if the level is registered in the scene, but is not placed in the xsheet,
  // then insert a new column
  else {
    if (!xsh->isColumnEmpty(col)) {
      col += 1;
      xsh->insertColumn(col);
    }
    xsh->setCell(row, col, TXshCell(sl, fid));
    app->getCurrentColumn()->setColumnIndex(col);
    if (getReviewTimeDSec() == 0 || m_isTimeLapse)
      app->getCurrentFrame()->setFrame(row + 1);
    m_xSheetFrameNumber = row + 2;
    emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  }
  postImportProcess();
  return true;
}

//-----------------------------------------------------------------

void StopMotion::captureImage() {
  if (m_playCaptureSound) {
    m_camSnapSound->play();
  }
  if (m_isTimeLapse && !m_intervalStarted) {
    startInterval();
    return;
  }
  if (m_isTimeLapse && m_intervalStarted && m_intervalTimer->isActive()) {
    stopInterval();
    return;
  }
  if (!m_hasLiveViewImage || m_liveViewStatus != LiveViewOpen) {
    DVGui::warning(tr("Cannot capture image unless live view is active."));
    return;
  }

  bool sessionOpen = false;
#ifdef WITH_CANON
  sessionOpen = m_canon->m_sessionOpen;
#endif
  if ((!m_usingWebcam && !sessionOpen) || m_userCalledPause) {
    DVGui::warning(tr("Please start live view before capturing an image."));
    return;
  }

  if (m_usingWebcam) {
    captureWebcamImage();
  } else {
    captureDslrImage();
  }
}

//-----------------------------------------------------------------------------

void StopMotion::captureWebcamImage() {
  if (m_light->useOverlays()) {
    m_light->showOverlays();
    m_webcamOverlayTimer->start(500);
  } else {
    if (getReviewTimeDSec() > 0 && !m_isTimeLapse) {
      m_timer->stop();
      if (m_liveViewStatus > LiveViewClosed) {
        // m_liveViewStatus = LiveViewPaused;
      }
    }
    m_lineUpImage    = m_liveViewImage;
    m_hasLineUpImage = true;
    emit(newLiveViewImageReady());
    importImage();
    return;
  }
}

//-----------------------------------------------------------------------------
void StopMotion::captureWebcamOnTimeout() {
  if (getReviewTimeDSec() > 0 && !m_isTimeLapse) {
    m_timer->stop();
    if (m_liveViewStatus > LiveViewClosed) {
      // m_liveViewStatus = LiveViewPaused;
    }
  }
  m_lineUpImage    = m_liveViewImage;
  m_hasLineUpImage = true;
  emit(newLiveViewImageReady());
  importImage();
  return;
}

//-----------------------------------------------------------------------------

void StopMotion::captureDslrImage() {
  m_light->showOverlays();

  if (getReviewTimeDSec() > 0 && !m_isTimeLapse) {
    m_timer->stop();
  }

  if (m_liveViewStatus > LiveViewClosed && !m_isTimeLapse) {
    // m_liveViewStatus = LiveViewPaused;
  }

  if (m_hasLiveViewImage) {
    m_lineUpImage    = m_liveViewImage;
    m_hasLineUpImage = true;
    emit(newLiveViewImageReady());
  }

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  int frameNumber        = m_frameNumber;
  std::wstring levelName = m_levelName.toStdWString();

  TFilePath parentDir = scene->decodeFilePath(TFilePath(m_filePath));
  TFilePath tempFile  = parentDir + "temp.jpg";

  if (!TFileStatus(parentDir).doesExist()) {
    TSystem::mkDir(parentDir);
  }
  m_tempFile = tempFile.getQString();

#ifdef WITH_CANON
  m_canon->takePicture();
#endif
}

//-----------------------------------------------------------------------------

void StopMotion::postImportProcess() {
  saveXmlFile();
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    int f = m_frameNumber;
    if (f % 10 == 0)  // next number
      m_frameNumber = ((int)(f / 10) + 1) * 10;
    else  // next alphabet
      m_frameNumber = f + 1;
  } else
    m_frameNumber += 1;
  emit(frameNumberChanged(m_frameNumber));
  /* notify */
  refreshFrameInfo();

  if (m_isTimeLapse && m_intervalStarted) restartInterval();

  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void StopMotion::saveXmlFile() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  std::wstring levelName = m_levelName.toStdWString();
  TFilePath parentDir    = scene->decodeFilePath(TFilePath(m_filePath));
  TFilePath tempFile     = parentDir + TFilePath(levelName + L".xml");
  QString xmlFileName    = tempFile.getQString();
  QFile xmlFile(xmlFileName);
  xmlFile.open(QIODevice::WriteOnly);
  QXmlStreamWriter xmlWriter(&xmlFile);
  xmlWriter.setAutoFormatting(true);
  xmlWriter.writeStartDocument();
  xmlWriter.writeStartElement("body");
  xmlWriter.writeStartElement("SceneInfo");
  xmlWriter.writeTextElement("LevelName", QString::fromStdWString(levelName));
  xmlWriter.writeTextElement("CurrentFrame",
                             QString::number(m_xSheetFrameNumber));
  xmlWriter.writeEndElement();

  xmlWriter.writeStartElement("CameraInfo");
  if (m_usingWebcam) {
    xmlWriter.writeTextElement("Webcam", "yes");
    xmlWriter.writeTextElement("CameraName", m_webcam->getWebcamDescription());
    xmlWriter.writeTextElement("CameraResolutionX",
                               QString::number(m_webcam->getWebcamWidth()));
    xmlWriter.writeTextElement("CameraResolutionY",
                               QString::number(m_webcam->getWebcamHeight()));
  } else {
    xmlWriter.writeTextElement("Webcam", "no");
#ifdef WITH_CANON
    xmlWriter.writeTextElement("CameraName",
                               QString::fromStdString(m_canon->m_cameraName));
    xmlWriter.writeTextElement("Aperture", m_canon->getCurrentAperture());
    xmlWriter.writeTextElement("ShutterSpeed",
                               m_canon->getCurrentShutterSpeed());
    xmlWriter.writeTextElement("ISO", m_canon->getCurrentIso());
    xmlWriter.writeTextElement("PictureStyle",
                               m_canon->getCurrentPictureStyle());
    xmlWriter.writeTextElement("ImageQuality",
                               m_canon->getCurrentImageQuality());
    xmlWriter.writeTextElement("WhiteBalance",
                               m_canon->getCurrentWhiteBalance());
    xmlWriter.writeTextElement("ColorTemperature",
                               m_canon->getCurrentColorTemperature());
    xmlWriter.writeTextElement("ExposureCompensation",
                               m_canon->getCurrentExposureCompensation());
    xmlWriter.writeTextElement("FocusCheckLocationX",
                               QString::number(m_canon->m_finalZoomPoint.x));
    xmlWriter.writeTextElement("FocusCheckLocationY",
                               QString::number(m_canon->m_finalZoomPoint.y));
#endif
  }
  xmlWriter.writeEndElement();

  xmlWriter.writeStartElement("Images");
  xmlWriter.writeTextElement("Frame", "Yes");
  xmlWriter.writeEndElement();
  xmlWriter.writeEndElement();
  xmlWriter.writeEndDocument();
  xmlFile.close();
}

//-----------------------------------------------------------------------------

bool StopMotion::loadXmlFile() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  bool webcam      = false;
  bool foundCamera = false;
  QString text;
  int x;
  int y;

  std::wstring levelName = m_levelName.toStdWString();
  TFilePath parentDir    = scene->decodeFilePath(TFilePath(m_filePath));
  TFilePath tempFile     = parentDir + TFilePath(levelName + L".xml");
  QString xmlFileName    = tempFile.getQString();
  QFile xmlFile(xmlFileName);
  if (!xmlFile.exists()) return false;
  xmlFile.open(QIODevice::ReadOnly);

  QXmlStreamReader xmlReader;
  xmlReader.setDevice(&xmlFile);
  xmlReader.readNext();
  while (!xmlReader.atEnd()) {
    if (xmlReader.isStartElement()) {
      if (xmlReader.name() == "Webcam") {
        text = xmlReader.readElementText();
        if (text == "yes") webcam = true;
      }
      if (xmlReader.name() == "CameraName") {
        text = xmlReader.readElementText();
        if (webcam) {
          QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
          if (cameras.size() > 0) {
            for (int i = 0; i < cameras.size(); i++) {
              if (cameras.at(i).description() == text) {
                changeCameras(i + 1);
                foundCamera = true;
                break;
              }
            }
          }
        } else {
#ifdef WITH_CANON
          QString camName = "";
          if (m_canon->getCameraCount() > 0) {
            m_canon->openCameraSession();
            camName = QString::fromStdString(m_canon->getCameraName());
            m_canon->closeCameraSession();
          }
          if (text == camName) {
            changeCameras(-1);
            foundCamera = true;
          }
#endif
        }
      }
      if (xmlReader.name() == "CameraResolutionX") {
        text = xmlReader.readElementText();
        x    = text.toInt();
      }
      if (xmlReader.name() == "CameraResolutionY") {
        text = xmlReader.readElementText();
        y    = text.toInt();
        if (foundCamera == true && webcam == true) {
          setWebcamResolution(
              QString(QString::number(x) + " x " + QString::number(y)));
        }
      }
#ifdef WITH_CANON
      if (xmlReader.name() == "Aperture") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setAperture(text);
        }
      }
      if (xmlReader.name() == "ShutterSpeed") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setShutterSpeed(text);
        }
      }
      if (xmlReader.name() == "ISO") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setIso(text);
        }
      }
      if (xmlReader.name() == "PictureStyle") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setPictureStyle(text);
        }
      }
      if (xmlReader.name() == "ImageQuality") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setImageQuality(text);
        }
      }
      if (xmlReader.name() == "WhiteBalance") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setWhiteBalance(text);
        }
      }
      if (xmlReader.name() == "ColorTemperature") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setColorTemperature(text);
        }
      }
      if (xmlReader.name() == "ExposureCompensation") {
        text = xmlReader.readElementText();
        if (foundCamera == true && webcam == false) {
          m_canon->setExposureCompensation(text);
        }
      }
      if (xmlReader.name() == "FocusCheckLocationX") {
        text                        = xmlReader.readElementText();
        m_canon->m_finalZoomPoint.x = text.toInt();
      }
      if (xmlReader.name() == "FocusCheckLocationY") {
        text                        = xmlReader.readElementText();
        m_canon->m_finalZoomPoint.y = text.toInt();
      }
#endif
    }
    xmlReader.readNext();
  }
  return true;
}

//-----------------------------------------------------------------------------

bool StopMotion::exportImageSequence() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();

  std::wstring levelName = m_levelName.toStdWString();

  if (levelName.empty()) {
    DVGui::error(
        tr("No level name specified: please choose a valid level name"));
    return false;
  }
  int frameNumber = m_frameNumber;

  TFilePath parentDir     = scene->decodeFilePath(TFilePath(m_filePath));
  TFilePath fullResFolder = scene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_FullRes"));
  TFilePath liveViewFolder = scene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_LiveView"));

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());
  TFilePath actualLevelFp = scene->decodeFilePath(levelFp);

  TFilePath fullResFp =
      scene->decodeFilePath(fullResFolder + TFilePath(levelName + L"..jpg"));
  TFilePath fullResFile(fullResFp.withFrame(frameNumber));

  TFilePath liveViewFp =
      scene->decodeFilePath(liveViewFolder + TFilePath(levelName + L"..jpg"));
  TFilePath liveViewFile(liveViewFp.withFrame(frameNumber));

  TFilePath tempFile = parentDir + "temp.jpg";

  TXshSimpleLevel *sl = 0;
  TXshLevel *level    = scene->getLevelSet()->getLevel(levelName);

  if (level == NULL) {
    DVGui::error(tr("No level exists with the current name."));
    return false;
  }

  /* if the existing level is not a raster level, then return */
  if (level->getType() != OVL_XSHLEVEL) {
    DVGui::error(tr("This is not an image level."));
    return false;
  }
  if (!level->getSimpleLevel()->getProperties()->isStopMotionLevel()) {
    DVGui::error(tr("This is not a stop motion level."));
    return false;
  }
  sl = level->getSimpleLevel();

  if (scene->decodeFilePath(sl->getPath()) != actualLevelFp) {
    DVGui::error(
        tr("The save in path specified does not match with the existing "
           "level."));
    return false;
  }

  // find which column the level is on.
  // check with the first column
  int col = TApp::instance()->getCurrentColumn()->getColumnIndex();
  int r0, r1, row;
  xsh->getColumn(col)->getRange(r0, r1);
  TXshSimpleLevel *colLevel = xsh->getCell(r0, col).getSimpleLevel();
  if (colLevel != sl) {
    int cols = xsh->getColumnCount();
    for (int i = 0; i < cols; i++) {
      xsh->getColumn(col)->getRange(r0, r1);
      colLevel = xsh->getCell(r0, col).getSimpleLevel();
      if (colLevel == sl) {
        col = i;
        break;
      }
    }
    DVGui::error(tr("Could not find an xsheet level with  the current level"));
    return false;
  }

  row = r0;
  xsh->getColumn(col)->getLevelRange(row, r0, r1);

  GenericSaveFilePopup *m_saveSequencePopup =
      new GenericSaveFilePopup("Export Image Sequence");
  m_saveSequencePopup->setFileMode(true);
  TFilePath fp = m_saveSequencePopup->getPath();
  if (fp == TFilePath()) {
    DVGui::error(tr("No export path given."));
    return false;
  }

  TFilePath sourceFile;
  TFilePath exportFilePath =
      scene->decodeFilePath(fp + TFilePath(levelName + L"..jpg"));
  TFilePath exportFile;
  int exportFrameNumber = 1;
  for (int i = r0; i <= r1; i++) {
    int cellNumber = xsh->getCell(i, col).getFrameId().getNumber();
    fullResFile    = fullResFp.withFrame(cellNumber);
    if (TFileStatus(fullResFile).doesExist()) {
      sourceFile = fullResFile;
    } else {
      sourceFile = actualLevelFp.withFrame(cellNumber);
    }

    if (!TFileStatus(sourceFile).doesExist()) {
      DVGui::error(tr("Could not find the source file."));
      return false;
    }
    exportFile = exportFilePath.withFrame(exportFrameNumber);
    if (TFileStatus(exportFile).doesExist()) {
      QString question = tr("Overwrite existing files?");
      int ret          = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                              QObject::tr("Cancel"));
      if (ret == 0 || ret == 2) return false;
    }
    TSystem::copyFile(exportFile, sourceFile);
    exportFrameNumber++;
    if (!TFileStatus(exportFile).doesExist()) {
      DVGui::error(tr("An error occurred.  Aborting."));
      return false;
    }
  }
  QString msg = tr("Successfully exported %1 images.")
                    .arg(QString::number(exportFrameNumber - 1));
  DVGui::MsgBoxInPopup(DVGui::MsgType(DVGui::INFORMATION), msg);
  return true;
}

//-----------------------------------------------------------------------------

// Refresh information that how many & which frames are saved for the current
// level
void StopMotion::refreshFrameInfo() {
  bool sessionOpen = false;

#ifdef WITH_CANON
  sessionOpen = m_canon->m_sessionOpen;
#endif

  if ((!sessionOpen && m_liveViewStatus < LiveViewOpen) && !m_usingWebcam) {
    m_frameInfoText = "";
    return;
  }

  QString tooltipStr, labelStr;
  enum InfoType { NEW = 0, ADD, OVERWRITE, WARNING } infoType(WARNING);

  static QColor infoColors[4] = {Qt::cyan, Qt::green, Qt::yellow, Qt::red};

  ToonzScene *currentScene = TApp::instance()->getCurrentScene()->getScene();
  TLevelSet *levelSet      = currentScene->getLevelSet();

  std::wstring levelName = m_levelName.toStdWString();
  int frameNumber        = m_frameNumber;

  TDimension stopMotionRes;
  bool checkRes = true;
  if (m_usingWebcam) stopMotionRes = m_liveViewImageDimensions;
#ifdef WITH_CANON
  else if (m_canon->m_useScaledImages ||
           !m_canon->getCurrentImageQuality().contains("Large")) {
    stopMotionRes = m_canon->m_proxyImageDimensions;
    if (m_canon->m_proxyImageDimensions == TDimension(0, 0)) {
      checkRes = false;
    }
  }
#endif
  else
    stopMotionRes = m_fullImageDimensions;

  bool letterOptionEnabled =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled();

  // level with the same name
  TXshLevel *level_sameName = levelSet->getLevel(levelName);

  TFilePath levelFp = TFilePath(m_filePath) +
                      TFilePath(levelName + L".." + m_fileType.toStdWString());

  // level with the same path
  TXshLevel *level_samePath = levelSet->getLevel(*(currentScene), levelFp);

  TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);

  // level existence
  bool levelExist = TSystem::doesExistFileOrLevel(actualLevelFp);

  // frame existence
  TFilePath frameFp(actualLevelFp.withFrame(frameNumber));
  bool frameExist = false;
  if (levelExist) frameExist = TFileStatus(frameFp).doesExist();

  // reset acceptable camera size
  m_allowedCameraSize = QSize();

  // ### CASE 1 ###
  // If there is no same level registered in the scene cast
  if (!level_sameName && !level_samePath) {
    // If there is a level in the file system
    if (levelExist) {
      TLevelReaderP lr;
      TLevelP level_p;
      try {
        lr = TLevelReaderP(actualLevelFp);
      } catch (...) {
        // TODO: output something
        m_frameInfoText = tr("UNDEFINED WARNING");
        return;
      }
      if (!lr) {
        // TODO: output something
        m_frameInfoText = tr("UNDEFINED WARNING");
        return;
      }
      try {
        level_p = lr->loadInfo();
      } catch (...) {
        // TODO: output something
        m_frameInfoText = tr("UNDEFINED WARNING");
        return;
      }
      if (!level_p) {
        // TODO: output something
        m_frameInfoText = tr("UNDEFINED WARNING");
        return;
      }
      int frameCount      = level_p->getFrameCount();
      TLevel::Iterator it = level_p->begin();
      std::vector<TFrameId> fids;
      for (int i = 0; it != level_p->end(); ++it, ++i)
        fids.push_back(it->first);

      tooltipStr +=
          tr("The level is not registered in the scene, but exists in the file "
             "system.");

      // check resolution
      const TImageInfo *ii;
      try {
        ii = lr->getImageInfo(fids[0]);
      } catch (...) {
        // TODO: output something
        m_frameInfoText = tr("UNDEFINED WARNING");
        return;
      }
      TDimension dim(ii->m_lx, ii->m_ly);
      // if the saved images has not the same resolution as the current camera
      // resolution
      if (checkRes && m_hasLiveViewImage && stopMotionRes != dim) {
        tooltipStr += tr("\nWARNING : Image size mismatch. The saved image "
                         "size is %1 x %2.")
                          .arg(dim.lx)
                          .arg(dim.ly);
        labelStr += tr("WARNING ");
        infoType = WARNING;
      }
      // if the resolutions are matched
      {
        if (frameCount == 1)
          tooltipStr += tr("\nFrame %1 exists.")
                            .arg(fidsToString(fids, letterOptionEnabled));
        else
          tooltipStr += tr("\nFrames %1 exist.")
                            .arg(fidsToString(fids, letterOptionEnabled));
        // if the frame exists, then it will be overwritten
        if (frameExist) {
          labelStr += tr("OVERWRITE 1 of");
          infoType = OVERWRITE;
        } else {
          labelStr += tr("ADD to");
          infoType = ADD;
        }
        if (frameCount == 1)
          labelStr += tr(" %1 frame").arg(frameCount);
        else
          labelStr += tr(" %1 frames").arg(frameCount);
      }
      m_allowedCameraSize = QSize(dim.lx, dim.ly);
    }
    // If no level exists in the file system, then it will be a new level
    else {
      tooltipStr += tr("The level will be newly created.");
      labelStr += tr("NEW");
      infoType = NEW;
    }
  }
  // ### CASE 2 ###
  // If there is already the level registered in the scene cast
  else if (level_sameName && level_samePath &&
           level_sameName == level_samePath) {
    tooltipStr += tr("The level is already registered in the scene.");
    if (!levelExist) tooltipStr += tr("\nNOTE : The level is not saved.");

    std::vector<TFrameId> fids;
    level_sameName->getFids(fids);

    // check resolution
    TDimension dim;
    bool ret = getRasterLevelSize(level_sameName, dim);
    if (!ret) {
      tooltipStr +=
          tr("\nWARNING : Failed to get image size of the existing level %1.")
              .arg(QString::fromStdWString(levelName));
      labelStr += tr("WARNING ");
      infoType = WARNING;
    }
    // if the saved images has not the same resolution as the current camera
    // resolution
    else if (checkRes && m_hasLiveViewImage && stopMotionRes != dim) {
      tooltipStr += tr("\nWARNING : Image size mismatch. The existing level "
                       "size is %1 x %2.")
                        .arg(dim.lx)
                        .arg(dim.ly);
      labelStr += tr("WARNING ");
      infoType = WARNING;
    }
    // if the resolutions are matched
    {
      int frameCount = fids.size();
      if (fids.size() == 1)
        tooltipStr += tr("\nFrame %1 exists.")
                          .arg(fidsToString(fids, letterOptionEnabled));
      else
        tooltipStr += tr("\nFrames %1 exist.")
                          .arg(fidsToString(fids, letterOptionEnabled));
      // Check if the target frame already exist in the level
      bool hasFrame = false;
      for (int f = 0; f < frameCount; f++) {
        if (fids.at(f).getNumber() == frameNumber) {
          hasFrame = true;
          break;
        }
      }
      // If there is already the frame then it will be overwritten
      if (hasFrame) {
        labelStr += tr("OVERWRITE 1 of");
        infoType = OVERWRITE;
      }
      // Or, the frame will be added to the level
      else {
        labelStr += tr("ADD to");
        infoType = ADD;
      }
      if (frameCount == 1)
        labelStr += tr(" %1 frame").arg(frameCount);
      else
        labelStr += tr(" %1 frames").arg(frameCount);
    }
    m_allowedCameraSize = QSize(dim.lx, dim.ly);
  }
  // ### CASE 3 ###
  // If there are some conflicts with the existing level.
  else {
    if (level_sameName) {
      TFilePath anotherPath = level_sameName->getPath();
      tooltipStr +=
          tr("WARNING : Level name conflicts. There already is a level %1 in the scene with the path\
                        \n          %2.")
              .arg(QString::fromStdWString(levelName))
              .arg(toQString(anotherPath));
      // check resolution
      TDimension dim;
      bool ret = getRasterLevelSize(level_sameName, dim);
      if (ret && checkRes && m_hasLiveViewImage && stopMotionRes != dim)
        tooltipStr += tr("\nWARNING : Image size mismatch. The size of level "
                         "with the same name is is %1 x %2.")
                          .arg(dim.lx)
                          .arg(dim.ly);
      m_allowedCameraSize = QSize(dim.lx, dim.ly);
    }
    if (level_samePath) {
      std::wstring anotherName = level_samePath->getName();
      if (!tooltipStr.isEmpty()) tooltipStr += QString("\n");
      tooltipStr +=
          tr("WARNING : Level path conflicts. There already is a level with the path %1\
                        \n          in the scene with the name %2.")
              .arg(toQString(levelFp))
              .arg(QString::fromStdWString(anotherName));
      // check resolution
      TDimension dim;
      bool ret = getRasterLevelSize(level_samePath, dim);
      if (ret && checkRes && m_hasLiveViewImage && stopMotionRes != dim)
        tooltipStr += tr("\nWARNING : Image size mismatch. The size of level "
                         "with the same path is %1 x %2.")
                          .arg(dim.lx)
                          .arg(dim.ly);
      m_allowedCameraSize = QSize(dim.lx, dim.ly);
    }
    labelStr += tr("WARNING");
    infoType = WARNING;
  }

  QColor infoColor   = infoColors[(int)infoType];
  m_infoColorName    = infoColor.name();
  m_frameInfoText    = labelStr;
  m_frameInfoToolTip = tooltipStr;
  emit(frameInfoTextChanged(m_frameInfoText));
}

//-----------------------------------------------------------------------------

void StopMotion::updateLevelNameAndFrame(std::wstring levelName) {
  if (levelName != m_levelName.toStdWString()) {
    m_levelName = QString::fromStdWString(levelName);
  }
  m_liveViewImageMap.clear();
  emit(levelNameChanged(m_levelName));
  // m_previousLevelButton->setDisabled(levelName == L"A");

  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  bool withLetter =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled();

  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  TXshLevel *level_p = levelSet->getLevel(levelName);
  int startFrame;
  if (!level_p) {
    startFrame = withLetter ? 10 : 1;
  } else {
    std::vector<TFrameId> fids;
    level_p->getFids(fids);
    if (fids.empty()) {
      startFrame = withLetter ? 10 : 1;
    } else {
      int lastNum = fids.back().getNumber();
      startFrame  = withLetter ? ((int)(lastNum / 10) + 1) * 10 : lastNum + 1;
    }
  }
  if (level_p) {
    TXshSimpleLevel *sl = level_p->getSimpleLevel();
    if (sl && sl->getType() == OVL_XSHLEVEL &&
        sl->getProperties()->isStopMotionLevel()) {
      buildLiveViewMap(sl);
    }
  }
  loadLineUpImage();
  m_frameNumber = startFrame;
  emit(frameNumberChanged(startFrame));
  refreshFrameInfo();
  getSubsampling();
}

//-----------------------------------------------------------------------------

void StopMotion::setToNextNewLevel() {
  const std::unique_ptr<NameBuilder> nameBuilder(NameBuilder::getBuilder(L""));

  TLevelSet *levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  ToonzScene *scene      = TApp::instance()->getCurrentScene()->getScene();
  std::wstring levelName = L"";

  // Select a different unique level name in case it already exists (either in
  // scene or on disk)
  TFilePath fp;
  TFilePath actualFp;
  for (;;) {
    levelName = nameBuilder->getNext();

    if (levelSet->getLevel(levelName) != 0) continue;

    fp = TFilePath(m_filePath) +
         TFilePath(levelName + L".." + m_fileType.toStdWString());
    actualFp = scene->decodeFilePath(fp);

    if (TSystem::doesExistFileOrLevel(actualFp)) {
      continue;
    }

    break;
  }

  updateLevelNameAndFrame(levelName);
}

//-----------------------------------------------------------------

void StopMotion::refreshCameraList() {
  QString camera = "";
  bool hasCamera = false;
#ifdef WITH_CANON
  if (m_canon->m_sessionOpen && m_canon->getCameraCount() > 0 &&
      !m_usingWebcam && m_canon->m_cameraName == m_canon->getCameraName()) {
    hasCamera = true;
    camera    = QString::fromStdString(m_canon->m_cameraName);
  }
#endif
  if (m_usingWebcam) {
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    if (m_usingWebcam && cameras.size() > 0) {
      for (int i = 0; i < cameras.size(); i++) {
        if (cameras.at(i).description() == m_webcam->getWebcamDescription()) {
          hasCamera = true;
          camera    = m_webcam->getWebcamDescription();
          break;
        }
      }
    }
  }
  if (!hasCamera) disconnectAllCameras();
  emit(updateCameraList(camera));
}

//-----------------------------------------------------------------

void StopMotion::changeCameras(int index) {
  // note: index is negative if this is called to load DSLR from load settings

  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

  // if selected the non-connected state, then disconnect the current camera
  if (index == 0) {
    disconnectAllCameras();
    stopInterval();
    return;
  }

  // There is a "Select Camera" as the first index
  index -= 1;

  // first see if the index didn't actually change
  if (cameras.size() > 0 && index < cameras.size() && index >= 0) {
    if (cameras.at(index).deviceName() == m_webcam->getWebcamDeviceName()) {
      return;
    }
  }

#ifdef WITH_CANON
  if ((index > cameras.size() - 1) && m_canon->m_sessionOpen) return;
#endif

  // close live view if open
  if (m_liveViewStatus > LiveViewClosed) {
    toggleLiveView();
  }

  // Check if its a webcam or DSLR
  // Webcams are listed first, so see if one of them is selected
  if (index < 0 || index > cameras.size() - 1) {
    m_usingWebcam = false;
  } else {
    m_usingWebcam = true;
    m_webcam->setWebcamIndex(index);
  }

  // in case the camera is not changed
  if (m_usingWebcam) {
#ifdef WITH_CANON
    if (m_canon->m_sessionOpen && m_canon->getCameraCount() > 0) {
      m_canon->closeCameraSession();
    }
#endif

    m_webcam->setWebcam(new QCamera(cameras.at(index)));
    m_webcam->setWebcamDeviceName(cameras.at(index).deviceName());
    m_webcam->setWebcamDescription(cameras.at(index).description());

    // loading new camera
    m_webcam->getWebcam()->load();

    m_webcam->refreshWebcamResolutions();

    QList<QSize> webcamResolutions = m_webcam->getWebcamResolutions();
    int sizeCount                  = webcamResolutions.count() - 1;

    int width;
    int height;
    for (int s = 0; s < webcamResolutions.size(); s++) {
      width  = webcamResolutions.at(s).width();
      height = webcamResolutions.at(s).height();
    }
    // see if we can set the webcam resolution to the current camera resolution
    TDimension res = TApp::instance()
                         ->getCurrentScene()
                         ->getScene()
                         ->getCurrentCamera()
                         ->getRes();
    if (webcamResolutions.contains(QSize(res.lx, res.ly))) {
      width     = res.lx;
      height    = res.ly;
      sizeCount = webcamResolutions.indexOf(QSize(res.lx, res.ly));
    }

    m_webcam->getWebcam()->unload();

    setWebcamResolution(
        QString(QString::number(width) + " x " + QString::number(height)));
    setTEnvCameraName(m_webcam->getWebcamDescription().toStdString());
    emit(newCameraSelected(index + 1, true));
    emit(webcamResolutionsChanged());
    emit(newWebcamResolutionSelected(sizeCount));

  } else {
#ifdef WITH_CANON
    m_canon->openCameraSession();
    setTEnvCameraName(m_canon->getCameraName());
    if (index == -2) {
      index = cameras.size();
    }
    m_webcam->clearWebcam();

    emit(newCameraSelected(index + 1, false));
#endif
  }
  if (m_useNumpadShortcuts) toggleNumpadShortcuts(true);
  m_liveViewDpi      = TPointD(0.0, 0.0);
  m_hasLineUpImage   = false;
  m_hasLiveViewImage = false;
  if (m_isTimeLapse && m_intervalStarted) {
    stopInterval();
  }
  emit(liveViewStopped());
  emit(liveViewChanged(false));
  refreshFrameInfo();
  // after all live view data is cleared, start it again.
  toggleLiveView();
}
//-----------------------------------------------------------------

void StopMotion::setWebcamResolution(QString resolution) {
  m_webcam->releaseWebcam();

  // resolution is written in the itemText with the format "<width> x
  // <height>" (e.g. "800 x 600")
  QStringList texts = resolution.split(' ');
  // the split text must be "<width>" "x" and "<height>"
  if (texts.size() != 3) return;

  int tempStatus   = m_liveViewStatus;
  m_liveViewStatus = LiveViewClosed;

  bool startTimer = false;
  if (m_timer->isActive()) {
    m_timer->stop();
    startTimer = true;
  }

  qApp->processEvents(QEventLoop::AllEvents, 1000);

  m_webcam->setWebcamWidth(texts[0].toInt());
  m_webcam->setWebcamHeight(texts[2].toInt());

  m_liveViewDpi    = TPointD(0.0, 0.0);
  m_liveViewStatus = tempStatus;
  if (startTimer) m_timer->start(40);

  // update env
  setTEnvCameraResolution(resolution.toStdString());

  refreshFrameInfo();

  int index = m_webcam->getIndexOfResolution();
  emit(newWebcamResolutionSelected(index));
}

//-----------------------------------------------------------------

bool StopMotion::toggleLiveView() {
  bool sessionOpen = false;

#ifdef WITH_CANON
  sessionOpen = m_canon->m_sessionOpen;
#endif

  if ((sessionOpen || m_usingWebcam) && m_liveViewStatus == LiveViewClosed) {
    m_liveViewDpi             = TPointD(0.0, 0.0);
    m_liveViewImageDimensions = TDimension(0, 0);
    if (!m_usingWebcam) {
#ifdef WITH_CANON
      m_canon->startCanonLiveView();
#endif
    } else
      m_liveViewStatus = LiveViewStarting;
    loadLineUpImage();
    m_timer->start(40);
    emit(liveViewChanged(true));
    Preferences::instance()->setValue(rewindAfterPlayback, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    return true;
  } else if ((sessionOpen || m_usingWebcam) &&
             m_liveViewStatus > LiveViewClosed) {
    if (!m_usingWebcam) {
#ifdef WITH_CANON
      m_canon->endCanonLiveView();
#endif
    } else {
      m_webcam->releaseWebcam();
    }

    m_timer->stop();
    emit(liveViewStopped());
    emit(liveViewChanged(false));
    if (m_turnOnRewind) {
      Preferences::instance()->setValue(rewindAfterPlayback, true);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    m_liveViewStatus = LiveViewClosed;
    return false;
  } else {
    DVGui::warning(tr("No camera selected."));
    return false;
  }
}

//-----------------------------------------------------------------

void StopMotion::pauseLiveView() {
  if (m_liveViewStatus == LiveViewOpen) {
    m_liveViewStatus  = LiveViewPaused;
    m_userCalledPause = true;
    emit(liveViewStopped());
  } else if (m_liveViewStatus == LiveViewPaused) {
    m_liveViewStatus  = LiveViewOpen;
    m_userCalledPause = false;
  } else if (m_liveViewStatus == LiveViewClosed) {
    toggleLiveView();
  }
}

//-----------------------------------------------------------------

void StopMotion::toggleAlwaysUseLiveViewImages() {
  m_alwaysUseLiveViewImages = !m_alwaysUseLiveViewImages;
  emit(alwaysUseLiveViewImagesToggled(m_alwaysUseLiveViewImages));
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------

void StopMotion::onCanonCameraChanged(QString camera) {
  emit(cameraChanged(camera));
}

//-----------------------------------------------------------------
std::string StopMotion::getTEnvCameraName() { return StopMotionCameraName; }
//-----------------------------------------------------------------
void StopMotion::setTEnvCameraName(std::string name) {
  StopMotionCameraName = name;
}
//-----------------------------------------------------------------
std::string StopMotion::getTEnvCameraResolution() {
  return StopMotionCameraResolution;
}
//-----------------------------------------------------------------
void StopMotion::setTEnvCameraResolution(std::string resolution) {
  StopMotionCameraResolution = resolution;
}

//=============================================================================

class StopMotionCaptureCommand : public MenuItemHandler {
public:
  StopMotionCaptureCommand() : MenuItemHandler(MI_StopMotionCapture) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    if (sm->m_liveViewStatus > 0) sm->captureImage();
  }
} StopMotionCaptureCommand;

//=============================================================================

class StopMotionRaiseOpacityCommand : public MenuItemHandler {
public:
  StopMotionRaiseOpacityCommand()
      : MenuItemHandler(MI_StopMotionRaiseOpacity) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    if (sm->m_liveViewStatus > 0) sm->raiseOpacity();
  }
} StopMotionRaiseOpacityCommand;

//=============================================================================

class StopMotionLowerOpacityCommand : public MenuItemHandler {
public:
  StopMotionLowerOpacityCommand()
      : MenuItemHandler(MI_StopMotionLowerOpacity) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    if (sm->m_liveViewStatus > 0) sm->lowerOpacity();
  }
} StopMotionLowerOpacityCommand;

//=============================================================================

class StopMotionToggleLiveViewCommand : public MenuItemHandler {
public:
  StopMotionToggleLiveViewCommand()
      : MenuItemHandler(MI_StopMotionToggleLiveView) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->pauseLiveView();
  }
} StopMotionToggleLiveViewCommand;

//=============================================================================

class StopMotionLowerSubsamplingCommand : public MenuItemHandler {
public:
  StopMotionLowerSubsamplingCommand()
      : MenuItemHandler(MI_StopMotionLowerSubsampling) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->setSubsamplingValue(std::max(1, sm->getSubsamplingValue() - 1));
    sm->setSubsampling();
  }
} StopMotionLowerSubsamplingCommand;

//=============================================================================

class StopMotionRaiseSubsamplingCommand : public MenuItemHandler {
public:
  StopMotionRaiseSubsamplingCommand()
      : MenuItemHandler(MI_StopMotionRaiseSubsampling) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->setSubsamplingValue(std::min(30, sm->getSubsamplingValue() + 1));
    sm->setSubsampling();
  }
} StopMotionRaiseSubsamplingCommand;

//=============================================================================

class StopMotionJumpToCameraCommand : public MenuItemHandler {
public:
  StopMotionJumpToCameraCommand()
      : MenuItemHandler(MI_StopMotionJumpToCamera) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->jumpToCameraFrame();
  }
} StopMotionJumpToCameraCommand;

//=============================================================================

class StopMotionExportImageSequence : public MenuItemHandler {
public:
  StopMotionExportImageSequence()
      : MenuItemHandler(MI_StopMotionExportImageSequence) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->exportImageSequence();
  }
} StopMotionExportImageSequence;

//=============================================================================

class StopMotionRemoveFrame : public MenuItemHandler {
public:
  StopMotionRemoveFrame() : MenuItemHandler(MI_StopMotionRemoveFrame) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->removeStopMotionFrame();
  }
} StopMotionRemoveFrame;

//=============================================================================

class StopMotionNextFrame : public MenuItemHandler {
public:
  StopMotionNextFrame() : MenuItemHandler(MI_StopMotionNextFrame) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    int index      = TApp::instance()->getCurrentFrame()->getFrameIndex();
    int maxInXSheet =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount();
    int max = std::max(maxInXSheet, sm->getXSheetFrameNumber() - 1);
    if (index < max) {
      TApp::instance()->getCurrentFrame()->setFrame(index + 1);
    }
  }
} StopMotionNextFrame;

//=============================================================================

class StopMotionToggleUseLiveViewImagesCommand : public MenuItemHandler {
public:
  StopMotionToggleUseLiveViewImagesCommand()
      : MenuItemHandler(MI_StopMotionToggleUseLiveViewImages) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->toggleAlwaysUseLiveViewImages();
  }
} StopMotionToggleUseLiveViewImagesCommand;

#ifdef WITH_CANON
//=============================================================================

class StopMotionPickFocusCheck : public MenuItemHandler {
public:
  StopMotionPickFocusCheck() : MenuItemHandler(MI_StopMotionPickFocusCheck) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->m_canon->toggleZoomPicking();
  }
} StopMotionPickFocusCheck;

//=============================================================================

class StopMotionToggleZoomCommand : public MenuItemHandler {
public:
  StopMotionToggleZoomCommand() : MenuItemHandler(MI_StopMotionToggleZoom) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->m_canon->zoomLiveView();
  }
} StopMotionToggleZoomCommand;
#endif
