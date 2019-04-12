#include "stopmotion.h"

#ifdef WIN32
#include <Windows.h>
#include <mfobjects.h>
#include <mfapi.h>
#include <mfidl.h>
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

// TnzCore includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "tenv.h"
#include "tsystem.h"
#include "filebrowsermodel.h"
#include "penciltestpopup.h"
#include "tlevel_io.h"
#include "toutputproperties.h"

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
#include "toonz/txshsimplelevel.h"
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

// Connected camera
TEnv::IntVar StopMotionUseScaledImages("StopMotionUseScaledImages", 0);
TEnv::IntVar StopMotionOpacity("StopMotionOpacity", 100);
TEnv::IntVar StopMotionUseDirectShow("StopMotionUseDirectShow", 1);
TEnv::IntVar StopMotionAlwaysLiveView("StopMotionAlwaysLiveView", 0);

TEnv::IntVar StopMotionBlackCapture("StopMotionBlackCapture", 0);
TEnv::IntVar StopMotionPlaceOnXSheet("StopMotionPlaceOnXSheet", 1);
TEnv::IntVar StopMotionReviewTime("StopMotionReviewTime", 1);
TEnv::IntVar StopMotionUseMjpg("StopMotionUseMjpg", 1);
TEnv::IntVar StopMotionUseNumpad("StopMotionUseNumpad", 0);

// Connected camera
TEnv::StringVar StopMotionCameraName("CamCapCameraName", "");
// Camera resolution
TEnv::StringVar StopMotionCameraResolution("CamCapCameraResolution", "");

namespace {
bool l_quitLoop = false;

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

//=============================================================================
//=============================================================================

JpgConverter::JpgConverter() {}
JpgConverter::~JpgConverter() {}

void JpgConverter::setStream(EdsStreamRef stream) { m_stream = stream; }

void JpgConverter::convertFromJpg() {
  unsigned __int64 mySize = 0;
  unsigned char *data     = NULL;
  EdsError err            = EdsGetPointer(m_stream, (EdsVoid **)&data);
  err                     = EdsGetLength(m_stream, &mySize);

  int width, height, pixelFormat;
  int inSubsamp, inColorspace;
  tjhandle tjInstance   = NULL;
  unsigned char *imgBuf = NULL;
  tjInstance            = tjInitDecompress();
  tjDecompressHeader3(tjInstance, data, mySize, &width, &height, &inSubsamp,
                      &inColorspace);

  if (width < 0 || height < 0) {
    emit(imageReady(false));
    return;
  }

  pixelFormat = TJPF_BGRX;
  imgBuf = (unsigned char *)tjAlloc(width * height * tjPixelSize[pixelFormat]);
  int flags = 0;
#ifdef WIN32
  flags |= TJFLAG_BOTTOMUP;
#endif
  int factorsNum;
  tjscalingfactor scalingFactor = {1, 1};
  tjscalingfactor *factor       = tjGetScalingFactors(&factorsNum);
  int i                         = 0;
  int tempWidth, tempHeight;
  while (i < factorsNum) {
    scalingFactor = factor[i];
    i++;
    tempWidth  = TJSCALED(width, scalingFactor);
    tempHeight = TJSCALED(height, scalingFactor);
  }
  tjDecompress2(tjInstance, data, mySize, imgBuf, width,
                width * tjPixelSize[pixelFormat], height, pixelFormat, flags);

  m_finalImage = TRaster32P(width, height);
  m_finalImage->lock();
  uchar *rawData = m_finalImage->getRawData();
  memcpy(rawData, imgBuf, width * height * tjPixelSize[pixelFormat]);
  m_finalImage->unlock();

  tjFree(imgBuf);
  imgBuf = NULL;
  tjDestroy(tjInstance);
  tjInstance = NULL;

  if (m_stream != NULL) {
    EdsRelease(m_stream);
    m_stream = NULL;
  }
  data = NULL;
  emit(imageReady(true));
}

void JpgConverter::run() { convertFromJpg(); }

//-----------------------------------------------------------------------------

StopMotion::StopMotion() {
  m_opacity         = StopMotionOpacity;
  m_useScaledImages = StopMotionUseScaledImages;
  buildAvMap();
  buildIsoMap();
  buildTvMap();
  buildModeMap();
  buildExposureMap();
  buildWhiteBalanceMap();
  buildColorTemperatures();
  buildImageQualityMap();
  buildPictureStyleMap();
  m_useDirectShow      = StopMotionUseDirectShow;
  m_useMjpg            = StopMotionUseMjpg;
  m_alwaysLiveView     = StopMotionAlwaysLiveView;
  m_blackCapture       = StopMotionBlackCapture;
  m_placeOnXSheet      = StopMotionPlaceOnXSheet;
  m_reviewTime         = StopMotionReviewTime;
  m_useNumpadShortcuts = StopMotionUseNumpad;
  m_numpadForStyleSwitching =
      Preferences::instance()->isUseNumpadForSwitchingStylesEnabled();
  setUseNumpadShortcuts(m_useNumpadShortcuts);
  m_turnOnRewind = Preferences::instance()->rewindAfterPlaybackEnabled();
  m_timer        = new QTimer(this);
  m_reviewTimer  = new QTimer(this);
  m_reviewTimer->setSingleShot(true);

  m_fullScreen1 = new QDialog();
  m_fullScreen1->setModal(false);
  m_fullScreen1->setStyleSheet("background-color:black;");
  m_screenCount = QApplication::desktop()->screenCount();
  if (m_screenCount > 1) {
    m_fullScreen2 = new QDialog();
    m_fullScreen2->setModal(false);
    m_fullScreen2->setStyleSheet("background-color:black;");
    if (m_screenCount == 3) {
      m_fullScreen3 = new QDialog();
      m_fullScreen3->setModal(false);
      m_fullScreen3->setStyleSheet("background-color:black;");
    }
  }

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  TSceneHandle *sceneHandle   = TApp::instance()->getCurrentScene();
  TFrameHandle *frameHandle   = TApp::instance()->getCurrentFrame();
  bool ret                    = true;

  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetSwitched()), this, SLOT(update()));
  ret = ret && connect(m_reviewTimer, SIGNAL(timeout()), this,
                       SLOT(onReviewTimeout()));
  ret = ret && connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
  ret =
      ret && connect(this, SIGNAL(newImageReady()), this, SLOT(importImage()));
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));
  ret = ret && connect(frameHandle, SIGNAL(isPlayingStatusChanged()), this,
                       SLOT(onPlaybackChanged()));
  assert(ret);

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  setToNextNewLevel();
  m_filePath = scene->getDefaultLevelPath(OVL_TYPE, m_levelName.toStdWString())
                   .getParentDir()
                   .getQString();

  // set handler for a camera detected
  initializeCanonSDK();
  if (!m_error)
    m_error = EdsSetCameraAddedHandler(StopMotion::handleCameraAddedEvent,
                                       (EdsVoid *)this);
}

//-----------------------------------------------------------------

StopMotion::~StopMotion() {
  if (m_liveViewStatus != LiveViewClosed) endLiveView();
  if (m_sessionOpen) closeCameraSession();
  if (m_camera) releaseCamera();
  if (m_cameraList != NULL) releaseCameraList();
  if (m_isSDKLoaded) closeCanonSDK();
  setUseNumpadShortcuts(false);
}

//-----------------------------------------------------------------------------

void StopMotion::onSceneSwitched() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = TApp::instance()->getCurrentXsheet()->getXsheet();
  setToNextNewLevel();
  m_filePath = scene->getDefaultLevelPath(OVL_TYPE, m_levelName.toStdWString())
                   .getParentDir()
                   .getQString();
  m_frameNumber       = 1;
  m_xSheetFrameNumber = 0;

  TLevelSet *levelSet = scene->getLevelSet();
  std::vector<TXshLevel *> levels;
  levelSet->listLevels(levels);
  int size = levels.size();
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
        break;
      }
    }
  }
  emit(levelNameChanged(m_levelName));
  emit(filePathChanged(m_filePath));
  emit(frameNumberChanged(m_frameNumber));
  emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  refreshFrameInfo();
}

//-----------------------------------------------------------------

void StopMotion::onPlaybackChanged() {
  if (TApp::instance()->getCurrentFrame()->isPlaying() || m_liveViewStatus == 0)
    return;

  int r0, r1, step;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->getProperties()->getPreviewProperties()->getRange(r0, r1, step);
  if (r1 > -1) return;

  int frame     = TApp::instance()->getCurrentFrame()->getFrame();
  int lastFrame = TApp::instance()->getCurrentFrame()->getMaxFrameIndex();
  if (frame == 0 || frame == lastFrame) {
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

void StopMotion::setUseScaledImages(bool on) {
  m_useScaledImages         = on;
  StopMotionUseScaledImages = int(on);
  emit(scaleFullSizeImagesSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setAlwaysLiveView(bool on) {
  m_alwaysLiveView         = on;
  StopMotionAlwaysLiveView = int(on);
  emit(liveViewOnAllFramesSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setBlackCapture(bool on) {
  m_blackCapture         = on;
  StopMotionBlackCapture = int(on);
  emit(blackCaptureSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setPlaceOnXSheet(bool on) {
  m_placeOnXSheet         = on;
  StopMotionPlaceOnXSheet = int(on);
  emit(placeOnXSheetSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::setReviewTime(int time) {
  m_reviewTime         = time;
  StopMotionReviewTime = time;
  emit(reviewTimeChangedSignal(time));
}

//-----------------------------------------------------------------

void StopMotion::setUseMjpg(bool on) {
  m_useMjpg         = on;
  StopMotionUseMjpg = int(on);
  emit(useMjpgSignal(on));
}

//-----------------------------------------------------------------

void StopMotion::jumpToCameraFrame() {
  TApp::instance()->getCurrentFrame()->setFrame(m_xSheetFrameNumber - 1);
}

//-----------------------------------------------------------------

void StopMotion::setUseNumpadShortcuts(bool on) {
  m_useNumpadShortcuts = on;
  StopMotionUseNumpad  = int(on);
  emit(useNumpadSignal(on));
}

void StopMotion::toggleNumpadShortcuts(bool on) {
  // can't just return if this feature is off
  // it could have been toggled while the camera was active
  if (!m_useNumpadShortcuts) on = false;
  CommandManager *comm = CommandManager::instance();

  if (on) {
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
      action = NULL;
    }
    shortcut = "-";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action = NULL;
    }
    shortcut = "Return";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action = NULL;
    }
    shortcut = "*";
    action   = comm->getActionFromShortcut(shortcut);
    if (action) {
      m_oldActionMap.insert(
          std::pair<std::string, QAction *>(shortcut, action));
      action = NULL;
    }

    // now set all new shortcuts
    action = comm->getAction(MI_PrevDrawing);
    if (action) {
      action->setShortcut(QKeySequence("1"));
      action = NULL;
    }
    action = comm->getAction(MI_NextDrawing);
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
      action->setShortcut(QKeySequence("Enter"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionToggleLiveView);
    if (action) {
      action->setShortcut(QKeySequence("5"));
      action = NULL;
    }
    action = comm->getAction(MI_StopMotionToggleZoom);
    if (action) {
      action->setShortcut(QKeySequence("*"));
      action = NULL;
    }

  } else {
    // unset the new shortcuts first
    QAction *action;
    action = comm->getAction(MI_PrevDrawing);
    if (action) {
      action->setShortcut(
          QKeySequence(comm->getShortcutFromAction(action).c_str()));
      action = NULL;
    }
    action = comm->getAction(MI_NextDrawing);
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
    action = comm->getAction(MI_StopMotionToggleZoom);
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

//-----------------------------------------------------------------

void StopMotion::setXSheetFrameNumber(int frameNumber) {
  m_xSheetFrameNumber = frameNumber;
  loadLineUpImage();
  emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------

bool StopMotion::loadLineUpImage() {
  if (m_liveViewStatus < 1) return false;
  m_hasLineUpImage = false;
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

  TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);
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
  int row;
  if (m_xSheetFrameNumber == 1) {
    row = 0;
  } else {
    row = m_xSheetFrameNumber - 2;
  }
  int col = app->getCurrentColumn()->getColumnIndex();

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

  TFrameId frameId = xsh->getCell(row, foundCol).getFrameId();

  int frameNumber = frameId.getNumber();

  if (m_usingWebcam) {
    if (frameNumber > 0) {
      m_lineUpImage    = sl->getFrame(frameId, false)->raster();
      m_hasLineUpImage = true;
      return true;
    } else
      return false;
  }

  // now check to see if a file actually exists there
  TFilePath liveViewFolder = currentScene->decodeFilePath(
      TFilePath(m_filePath) + TFilePath(levelName + L"_LiveView"));
  TFilePath liveViewFp = currentScene->decodeFilePath(
      liveViewFolder + TFilePath(levelName + L"..jpg"));
  TFilePath liveViewFile(liveViewFp.withFrame(frameNumber));
  if (TFileStatus(liveViewFile).doesExist()) {
    if (loadJpg(liveViewFile, m_lineUpImage)) {
      m_hasLineUpImage = true;
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

  // if the current level name is non-sequencial, then try to switch the last
  // sequencial level in the scene.
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
  // int destinationFrame = m_xSheetFrameNumber - 1;
  if (m_liveViewStatus > 0 && m_liveViewStatus < 3 &&
      !TApp::instance()->getCurrentFrame()->isPlaying()) {
    if (getAlwaysLiveView() || (currentFrame == m_xSheetFrameNumber - 1)) {
      if (!m_usingWebcam)
        downloadEVFData();
      else
        getWebcamImage();
      if (getAlwaysLiveView() && currentFrame != m_xSheetFrameNumber - 1 ||
          m_pickLiveViewZoom) {
        m_showLineUpImage = false;
      } else {
        m_showLineUpImage = true;
      }
    } else if (m_liveViewStatus == 2) {
      m_liveViewStatus = 3;
      TApp::instance()->getCurrentScene()->notifySceneChanged();
    }
  } else if (m_liveViewStatus == 3 && !m_userCalledPause) {
    if (getAlwaysLiveView() || (currentFrame == m_xSheetFrameNumber - 1)) {
      if (!m_usingWebcam)
        downloadEVFData();
      else
        getWebcamImage();
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotion::onReviewTimeout() {
  if (m_liveViewStatus > 0) {
    m_liveViewStatus = 2;
    m_timer->start(40);
  }
  TApp::instance()->getCurrentFrame()->setFrame(m_xSheetFrameNumber - 1);
}

//-----------------------------------------------------------------------------

bool StopMotion::importImage() {
  if (getBlackCapture()) {
    m_fullScreen1->hide();

    if (m_screenCount > 1) {
      m_fullScreen2->hide();

      if (m_screenCount == 3) {
        m_fullScreen3->hide();
      }
    }
  }
  if (getReviewTime() > 0) {
    m_reviewTimer->start(getReviewTime() * 1000);
  }

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
    if (m_useScaledImages) {
      TSystem::copyFile(fullResFile, tempFile);
      TSystem::deleteFile(tempFile);
    }
    if (m_hasLineUpImage) {
      saveJpg(m_lineUpImage, liveViewFile);
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
    if (getReviewTime() == 0) app->getCurrentFrame()->setFrame(row + 1);
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
    if (getReviewTime() == 0) app->getCurrentFrame()->setFrame(row + 1);
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
    if (getReviewTime() == 0) app->getCurrentFrame()->setFrame(row + 1);
    m_xSheetFrameNumber = row + 2;
    emit(xSheetFrameNumberChanged(m_xSheetFrameNumber));
  }
  postImportProcess();
  return true;
}

//-----------------------------------------------------------------

void StopMotion::captureImage() {
  if (!m_usingWebcam && !m_sessionOpen) {
    DVGui::warning(tr("Please start live view before capturing an image."));
    return;
  }
  if (m_usingWebcam) {
    if (!m_hasLiveViewImage) {
      DVGui::warning(
          tr("Cannot capture webcam image unless live view is active."));
      return;
    }
    if (getReviewTime() > 0) {
      m_timer->stop();
      if (m_liveViewStatus > 0) {
        m_liveViewStatus = 3;
      }
    }
    m_lineUpImage    = m_liveViewImage;
    m_hasLineUpImage = true;
    emit(newLiveViewImageReady());
    importImage();
    return;
  }
  if (getBlackCapture()) {
    m_fullScreen1->showFullScreen();
    m_fullScreen1->setGeometry(QApplication::desktop()->screenGeometry(0));
    if (m_screenCount > 1) {
      m_fullScreen2->showFullScreen();
      m_fullScreen2->setGeometry(QApplication::desktop()->screenGeometry(1));

      if (m_screenCount == 3) {
        m_fullScreen3->showFullScreen();
        m_fullScreen3->setGeometry(QApplication::desktop()->screenGeometry(2));
      }
    }
    // this allows the full screen qdialogs to go full screen before
    // taking a photo
    qApp->processEvents(QEventLoop::AllEvents, 1500);
  }

  if (getReviewTime() > 0) {
    m_timer->stop();
  }

  if (m_liveViewStatus > 0) {
    m_liveViewStatus = 3;
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
  takePicture();
}

//-----------------------------------------------------------------------------

void StopMotion::saveJpg(TRaster32P image, TFilePath path) {
  unsigned char *jpegBuf = NULL; /* Dynamically allocate the JPEG buffer */
  unsigned long jpegSize = 0;
  int pixelFormat        = TJPF_BGRX;
  int outQual            = 95;
  int subSamp            = TJSAMP_411;
  bool success           = false;
  tjhandle tjInstance;

  int width  = image->getLx();
  int height = image->getLy();
  int flags  = 0;
#ifdef WIN32
  flags |= TJFLAG_BOTTOMUP;
#endif

  image->lock();
  uchar *rawData = image->getRawData();
  if ((tjInstance = tjInitCompress()) != NULL) {
    if (tjCompress2(tjInstance, rawData, width, 0, height, pixelFormat,
                    &jpegBuf, &jpegSize, subSamp, outQual, flags) >= 0) {
      success = true;
    }
  }
  image->unlock();
  tjDestroy(tjInstance);
  tjInstance = NULL;

  if (success) {
    /* Write the JPEG image to disk. */
    QFile fullImage(path.getQString());
    fullImage.open(QIODevice::WriteOnly);
    QDataStream dataStream(&fullImage);
    dataStream.writeRawData((const char *)jpegBuf, jpegSize);
    fullImage.close();
  }
  tjFree(jpegBuf);
  jpegBuf = NULL;
}

//-----------------------------------------------------------------------------

bool StopMotion::loadJpg(TFilePath path, TRaster32P &image) {
  long size;
  int inSubsamp, inColorspace, width, height;
  unsigned long jpegSize;
  unsigned char *jpegBuf;
  FILE *jpegFile;
  QString qPath      = path.getQString();
  QByteArray ba      = qPath.toLocal8Bit();
  const char *c_path = ba.data();
  bool success       = true;
  tjhandle tjInstance;

  /* Read the JPEG file into memory. */
  if ((jpegFile = fopen(c_path, "rb")) == NULL) success = false;
  if (success && fseek(jpegFile, 0, SEEK_END) < 0 ||
      ((size = ftell(jpegFile)) < 0) || fseek(jpegFile, 0, SEEK_SET) < 0)
    success = false;
  if (success && size == 0) success = false;
  jpegSize = (unsigned long)size;
  if (success && (jpegBuf = (unsigned char *)tjAlloc(jpegSize)) == NULL)
    success = false;
  if (success && fread(jpegBuf, jpegSize, 1, jpegFile) < 1) success = false;
  fclose(jpegFile);
  jpegFile = NULL;

  if (success && (tjInstance = tjInitDecompress()) == NULL) success = false;

  if (success && tjDecompressHeader3(tjInstance, jpegBuf, jpegSize, &width,
                                     &height, &inSubsamp, &inColorspace) < 0)
    success = false;

  int pixelFormat       = TJPF_BGRX;
  unsigned char *imgBuf = NULL;
  if (success &&
      (imgBuf = tjAlloc(width * height * tjPixelSize[pixelFormat])) == NULL)
    success = false;

  int flags = 0;
#ifdef WIN32
  flags |= TJFLAG_BOTTOMUP;
#endif
  if (success && tjDecompress2(tjInstance, jpegBuf, jpegSize, imgBuf, width, 0,
                               height, pixelFormat, flags) < 0)
    success = false;
  tjFree(jpegBuf);
  jpegBuf = NULL;
  tjDestroy(tjInstance);
  tjInstance = NULL;

  image = TRaster32P(width, height);
  image->lock();
  uchar *rawData = image->getRawData();
  memcpy(rawData, imgBuf, width * height * tjPixelSize[pixelFormat]);
  image->unlock();

  tjFree(imgBuf);
  imgBuf = NULL;

  return success;
}

//-----------------------------------------------------------------------------

void StopMotion::postImportProcess() {
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
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

// Refresh information that how many & which frames are saved for the current
// level
void StopMotion::refreshFrameInfo() {
  if ((!m_sessionOpen && m_liveViewStatus < 2) && !m_usingWebcam) {
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
  if (m_usingWebcam)
    stopMotionRes = m_liveViewImageDimensions;

  else if (m_useScaledImages || !getCurrentImageQuality().contains("Large")) {
    stopMotionRes = m_proxyImageDimensions;
    if (m_proxyImageDimensions == TDimension(0, 0)) {
      checkRes = false;
    }
  } else
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
    loadLineUpImage();
  }
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

void StopMotion::refreshCameraList() { emit(updateCameraList()); }

//-----------------------------------------------------------------

void StopMotion::changeCameras(int index) {
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();

  // if selected the non-connected state, then disconnect the current camera
  if (index == 0) {
    m_active               = false;
    m_webcamDeviceName     = QString();
    m_webcamDescription    = QString();
    m_webcamIndex          = -1;
    m_proxyDpi             = TPointD(0.0, 0.0);
    m_proxyImageDimensions = TDimension(0, 0);

    if (m_sessionOpen || m_usingWebcam) {
      if (m_liveViewStatus > 0) {
        endLiveView();
      }
      closeCameraSession();
      m_usingWebcam = false;
    }

    setTEnvCameraName("");

    emit(newCameraSelected(index, false));
    toggleNumpadShortcuts(false);
    return;
  }

  // There is a "Select Camera" as the first index
  index -= 1;
  m_active = true;

  // Check if its a webcam or DSLR
  // Webcams are listed first, so see if one of them is selected
  if (index > cameras.size() - 1) {
    m_usingWebcam = false;
  } else {
    m_usingWebcam = true;
    m_webcamIndex = index;
  }

  // in case the camera is not changed
  if (m_usingWebcam) {
    if (cameras.at(index).deviceName() == m_webcamDeviceName) {
      return;
    }

    if (m_sessionOpen) {
      if (m_liveViewStatus > 0) {
        endLiveView();
        closeCameraSession();
      }
    }

    setWebcam(new QCamera(cameras.at(index)));
    m_webcamDeviceName  = cameras.at(index).deviceName();
    m_webcamDescription = cameras.at(index).description();

#ifdef MACOSX
    // this line is needed only in macosx
    m_stopMotion->getWebcam()->setViewfinder(m_dummyViewFinder);
#endif

    // loading new camera
    getWebcam()->load();

    m_webcamResolutions.clear();

    m_webcamResolutions = getWebcam()->supportedViewfinderResolutions();
    int sizeCount       = m_webcamResolutions.count();

    int width;
    int height;
    for (int s = 0; s < m_webcamResolutions.size(); s++) {
      width  = m_webcamResolutions.at(s).width();
      height = m_webcamResolutions.at(s).height();
    }

    getWebcam()->unload();

    setWebcamResolution(
        QString(QString::number(width) + " x " + QString::number(height)));
    setTEnvCameraName(m_webcamDescription.toStdString());
    emit(newCameraSelected(index + 1, true));
    emit(webcamResolutionsChanged());
    emit(newWebcamResolutionSelected(sizeCount - 1));

  } else {
    m_webcamDeviceName  = QString();
    m_webcamDescription = QString();
    m_webcamIndex       = -1;
    openCameraSession();
    setTEnvCameraName(getCameraName());
    emit(newCameraSelected(index + 1, false));
  }
  if (m_useNumpadShortcuts) toggleNumpadShortcuts(true);
  m_liveViewDpi = TPointD(0.0, 0.0);
  refreshFrameInfo();
}

//-----------------------------------------------------------------

QList<QCameraInfo> StopMotion::getWebcams() {
  m_webcams.clear();
  m_webcams = QCameraInfo::availableCameras();
  return m_webcams;
}

//-----------------------------------------------------------------

void StopMotion::setWebcam(QCamera *camera) { m_webcam = camera; }

//-----------------------------------------------------------------

bool StopMotion::translateIndex(int index) {
  // We are using Qt to get the camera info and supported resolutions, but
  // we are using OpenCV to actually get the images.
  // The camera index from OpenCV and from Qt don't always agree,
  // So this checks the name against the correct index.
  m_webcamIndex = index;

#ifdef WIN32

  // Thanks to:
  // https://elcharolin.wordpress.com/2017/08/28/webcam-capture-with-the-media-foundation-sdk/
  // for the webcam enumeration here

#define CLEAN_ATTRIBUTES()                                                     \
  if (attributes) {                                                            \
    attributes->Release();                                                     \
    attributes = NULL;                                                         \
  }                                                                            \
  for (DWORD i = 0; i < count; i++) {                                          \
    if (&devices[i]) {                                                         \
      devices[i]->Release();                                                   \
      devices[i] = NULL;                                                       \
    }                                                                          \
  }                                                                            \
  CoTaskMemFree(devices);                                                      \
  return hr;

  HRESULT hr = S_OK;

  // this is important!!
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  UINT32 count              = 0;
  IMFAttributes *attributes = NULL;
  IMFActivate **devices     = NULL;

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Create an attribute store to specify enumeration parameters.
  hr = MFCreateAttributes(&attributes, 1);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }

  // The attribute to be requested is devices that can capture video
  hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Enummerate the video capture devices
  hr = MFEnumDeviceSources(attributes, &devices, &count);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // if there are any available devices
  if (count > 0) {
    WCHAR *nameString = NULL;
    // Get the human-friendly name of the device
    UINT32 cchName;

    for (int i = 0; i < count; i++) {
      hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                          &nameString, &cchName);
      std::string desc = m_webcamDescription.toStdString();
      if (nameString == m_webcamDescription.toStdWString()) {
        m_webcamIndex = i;
        break;
      }
      // devices[0]->ShutdownObject();
    }

    CoTaskMemFree(nameString);
  }
  // clean
  CLEAN_ATTRIBUTES()
#else
  return true;
#endif
}

//-----------------------------------------------------------------

bool StopMotion::initWebcam(int index) {
  if (!m_useDirectShow) {
    // the webcam order obtained from Qt isn't always the same order as
    // the one obtained from OpenCV without DirectShow
    translateIndex(index);
    m_cvWebcam.open(m_webcamIndex);
  } else {
    m_webcamIndex = index;
    m_cvWebcam.open(m_webcamIndex, cv::CAP_DSHOW);
  }
  if (m_cvWebcam.isOpened() == false) {
    return false;
  }

  return true;
}

//-----------------------------------------------------------------

void StopMotion::releaseWebcam() {
  m_cvWebcam.release();
  m_liveViewStatus = 0;
  emit(liveViewStopped());
}

//-----------------------------------------------------------------

void StopMotion::setWebcamResolution(QString resolution) {
  m_cvWebcam.release();

  // resolution is written in the itemText with the format "<width> x
  // <height>" (e.g. "800 x 600")
  QStringList texts = resolution.split(' ');
  // the splited text must be "<width>" "x" and "<height>"
  if (texts.size() != 3) return;
  int tempStatus   = m_liveViewStatus;
  m_liveViewStatus = 0;
  bool startTimer  = false;
  if (m_timer->isActive()) {
    m_timer->stop();
    startTimer = true;
  }
  qApp->processEvents(QEventLoop::AllEvents, 1000);

  m_webcamWidth  = texts[0].toInt();
  m_webcamHeight = texts[2].toInt();

  m_liveViewDpi    = TPointD(0.0, 0.0);
  m_liveViewStatus = tempStatus;
  if (startTimer) m_timer->start(40);

  // update env
  setTEnvCameraResolution(resolution.toStdString());

  refreshFrameInfo();

  int index = m_webcamResolutions.indexOf(QSize(m_webcamWidth, m_webcamHeight));
  emit(newWebcamResolutionSelected(index));
}

//-----------------------------------------------------------------

void StopMotion::getWebcamImage() {
  bool error = false;
  cv::Mat imgOriginal;
  cv::Mat imgCorrected;

  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);
    // mjpg is used by many webcams
    // opencv runs very slow on some webcams without it.
    if (m_useMjpg) {
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('m', 'j', 'p', 'g'));
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }
    m_cvWebcam.set(3, m_webcamWidth);
    m_cvWebcam.set(4, m_webcamHeight);
    if (!m_cvWebcam.isOpened()) {
      m_hasLiveViewImage = false;
      error              = true;
    }
  }

  bool blnFrameReadSuccessfully =
      m_cvWebcam.read(imgOriginal);  // get next frame

  if (!blnFrameReadSuccessfully ||
      imgOriginal.empty()) {  // if frame not read successfully
    std::cout << "error: frame not read from webcam\n";
    error = true;  // print error message to std out
  }

  if (!error) {
    cv::cvtColor(imgOriginal, imgCorrected, cv::COLOR_BGR2BGRA);
    cv::flip(imgCorrected, imgCorrected, 0);
    int width  = m_cvWebcam.get(3);
    int height = m_cvWebcam.get(4);
    int size   = imgCorrected.total() * imgCorrected.elemSize();

    m_liveViewImage = TRaster32P(width, height);
    m_liveViewImage->lock();
    uchar *imgBuf  = imgCorrected.data;
    uchar *rawData = m_liveViewImage->getRawData();
    memcpy(rawData, imgBuf, size);
    m_liveViewImage->unlock();
    m_hasLiveViewImage = true;
    m_liveViewStatus   = 2;
    if (m_hasLiveViewImage &&
        (m_liveViewDpi.x == 0.0 || m_liveViewImageDimensions.lx == 0)) {
      TCamera *camera =
          TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
      TDimensionD size = camera->getSize();
      m_liveViewImageDimensions =
          TDimension(m_liveViewImage->getLx(), m_liveViewImage->getLy());
      double minimumDpi = std::min(m_liveViewImageDimensions.lx / size.lx,
                                   m_liveViewImageDimensions.ly / size.ly);
      m_liveViewDpi     = TPointD(minimumDpi, minimumDpi);

      m_fullImageDimensions = m_liveViewImageDimensions;

      m_fullImageDpi = m_liveViewDpi;

      emit(newDimensions());
    }

    emit(newLiveViewImageReady());
  } else
    m_hasLiveViewImage = false;
}

//-----------------------------------------------------------------

void StopMotion::setUseDirectShow(int state) {
  m_useDirectShow         = state;
  StopMotionUseDirectShow = state;
  emit(useDirectShowSignal(state));
}

//-----------------------------------------------------------------

bool StopMotion::toggleLiveView() {
  if ((m_sessionOpen || m_usingWebcam) && m_liveViewStatus == 0) {
    m_liveViewDpi             = TPointD(0.0, 0.0);
    m_liveViewImageDimensions = TDimension(0, 0);
    if (!m_usingWebcam) {
      startLiveView();
    } else
      m_liveViewStatus = 1;
    loadLineUpImage();
    m_timer->start(40);
    emit(liveViewChanged(true));
    Preferences::instance()->setValue(rewindAfterPlayback, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    return true;
  } else if ((m_sessionOpen || m_usingWebcam) && m_liveViewStatus > 0) {
    if (!m_usingWebcam)
      endLiveView();
    else
      releaseWebcam();
    m_timer->stop();
    emit(liveViewChanged(false));
    if (m_turnOnRewind) {
      Preferences::instance()->setValue(rewindAfterPlayback, true);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    return false;
  } else {
    DVGui::warning(tr("No camera selected."));
    return false;
  }
}

//-----------------------------------------------------------------

void StopMotion::pauseLiveView() {
  if (m_liveViewStatus == 2) {
    m_liveViewStatus  = 3;
    m_userCalledPause = true;
    emit(liveViewStopped());
  } else if (m_liveViewStatus == 3) {
    m_liveViewStatus  = 2;
    m_userCalledPause = false;
  }
}

//-----------------------------------------------------------------

EdsError StopMotion::initializeCanonSDK() {
  m_error = EdsInitializeSDK();
  if (m_error == EDS_ERR_OK) {
    m_isSDKLoaded = true;
  }
  return m_error;
}

//-----------------------------------------------------------------

EdsCameraListRef StopMotion::getCameraList() {
  if (!m_isSDKLoaded) initializeCanonSDK();
  if (m_isSDKLoaded) {
    m_error = EdsGetCameraList(&m_cameraList);
  }
  return m_cameraList;
}

//-----------------------------------------------------------------

EdsError StopMotion::releaseCameraList() {
  if (m_cameraList != NULL) m_error = EdsRelease(m_cameraList);
  return m_error;
}

//-----------------------------------------------------------------

int StopMotion::getCameraCount() {
  if (m_cameraList == NULL) {
    getCameraList();
  }
  if (m_cameraList != NULL) {
    m_error = EdsGetChildCount(m_cameraList, &m_count);
    if (m_count == 0) {
      m_error          = EDS_ERR_DEVICE_NOT_FOUND;
      m_sessionOpen    = false;
      m_liveViewStatus = 0;
    }
    return m_count;
  } else
    return -1;
}

//-----------------------------------------------------------------

EdsError StopMotion::getCamera(int index) {
  if (m_count == 0) {
    m_error = EDS_ERR_DEVICE_NOT_FOUND;
  }
  if (m_count > 0) {
    m_error = EdsGetChildAtIndex(m_cameraList, index, &m_camera);
  }
  return m_error;
}

//-----------------------------------------------------------------

EdsError StopMotion::releaseCamera() {
  if (m_camera != NULL) {
    m_error = EdsRelease(m_camera);
  }
  return m_error;
}

//-----------------------------------------------------------------

void StopMotion::cameraAdded() {
  if (!m_active) refreshCameraList();
}

//-----------------------------------------------------------------

void StopMotion::closeCanonSDK() {
  if (m_isSDKLoaded) {
    EdsTerminateSDK();
  }
}

//-----------------------------------------------------------------

EdsError StopMotion::openCameraSession() {
  if (m_camera != NULL) {
    m_error = EdsOpenSession(m_camera);
    if (m_error == EDS_ERR_OK) m_sessionOpen = true;
  }
  m_error =
      EdsSetObjectEventHandler(m_camera, kEdsObjectEvent_All,
                               StopMotion::handleObjectEvent, (EdsVoid *)this);

  m_error = EdsSetPropertyEventHandler(m_camera, kEdsPropertyEvent_All,
                                       StopMotion::handlePropertyEvent,
                                       (EdsVoid *)this);

  m_error = EdsSetCameraStateEventHandler(m_camera, kEdsStateEvent_All,
                                          StopMotion::handleStateEvent,
                                          (EdsVoid *)this);

  // We can't handle raw images yet, so make sure we are getting jpgs
  if (getCurrentImageQuality().contains("RAW"))
    setImageQuality("Large Fine Jpeg");

  EdsUInt32 saveto        = kEdsSaveTo_Host;
  m_error                 = EdsSetPropertyData(m_camera, kEdsPropID_SaveTo, 0,
                               sizeof(EdsUInt32), &saveto);
  EdsCapacity newCapacity = {0x7FFFFFFF, 0x1000, 1};
  m_error                 = EdsSetCapacity(m_camera, newCapacity);
  return m_error;
}

//-----------------------------------------------------------------

EdsError StopMotion::closeCameraSession() {
  if (m_camera != NULL) {
    m_error       = EdsCloseSession(m_camera);
    m_sessionOpen = false;
  }
  return m_error;
}

//-----------------------------------------------------------------

void StopMotion::refreshOptions() {
  getAvailableShutterSpeeds();
  getAvailableIso();
  getAvailableApertures();
  getAvailableExposureCompensations();
  getAvailableWhiteBalances();
  buildColorTemperatures();
  getAvailableImageQualities();
  getAvailablePictureStyles();
}

//-----------------------------------------------------------------

std::string StopMotion::getCameraName() {
  EdsChar name[EDS_MAX_NAME];
  EdsError err = EDS_ERR_OK;
  EdsDataType dataType;
  EdsUInt32 dataSize;
  m_error = EdsGetPropertySize(m_camera, kEdsPropID_ProductName, 0, &dataType,
                               &dataSize);

  if (m_error == EDS_ERR_OK) {
    m_error =
        EdsGetPropertyData(m_camera, kEdsPropID_ProductName, 0, dataSize, name);
  }
  m_cameraName = name;
  return m_cameraName;
}

//-----------------------------------------------------------------

QString StopMotion::getMode() {
  EdsError err = EDS_ERR_OK;
  EdsDataType modeType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_AEMode, 0, &modeType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_AEMode, 0, sizeof(size), &data);

  return QString::fromStdString(m_modeMap[data]);
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailableIso() {
  EdsPropertyDesc *IsoDesc = new EdsPropertyDesc;
  EdsError err             = EDS_ERR_OK;
  m_isoOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_ISOSpeed, IsoDesc);
  int count = IsoDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_isoOptions.push_back(
          QString::fromStdString(m_isoMap[IsoDesc->propDesc[i]]));
      i++;
    }
  }
  delete IsoDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailableShutterSpeeds() {
  EdsPropertyDesc *TvDesc = new EdsPropertyDesc;
  EdsError err            = EDS_ERR_OK;
  m_shutterSpeedOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_Tv, TvDesc);
  int count = TvDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_shutterSpeedOptions.push_back(
          QString::fromStdString(m_tvMap[TvDesc->propDesc[i]]));
      i++;
    }
  }
  delete TvDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailableApertures() {
  EdsPropertyDesc *AvDesc = new EdsPropertyDesc;
  EdsError err            = EDS_ERR_OK;
  m_apertureOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_Av, AvDesc);
  int count = AvDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_apertureOptions.push_back(
          QString::fromStdString(m_avMap[AvDesc->propDesc[i]]));
      i++;
    }
  }
  delete AvDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailableExposureCompensations() {
  EdsPropertyDesc *exposureDesc = new EdsPropertyDesc;
  EdsError err                  = EDS_ERR_OK;
  m_exposureOptions.clear();

  err       = EdsGetPropertyDesc(m_camera, kEdsPropID_ExposureCompensation,
                           exposureDesc);
  int count = exposureDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_exposureOptions.push_back(
          QString::fromStdString(m_exposureMap[exposureDesc->propDesc[i]]));
      i++;
    }
  }
  delete exposureDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailableWhiteBalances() {
  EdsPropertyDesc *whiteBalanceDesc = new EdsPropertyDesc;
  EdsError err                      = EDS_ERR_OK;
  m_whiteBalanceOptions.clear();

  err = EdsGetPropertyDesc(m_camera, kEdsPropID_WhiteBalance, whiteBalanceDesc);
  int count = whiteBalanceDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_whiteBalanceOptions.push_back(QString::fromStdString(
          m_whiteBalanceMap[whiteBalanceDesc->propDesc[i]]));
      i++;
    }
  }
  delete whiteBalanceDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailableImageQualities() {
  EdsPropertyDesc *imageQualityDesc = new EdsPropertyDesc;
  EdsError err                      = EDS_ERR_OK;
  m_imageQualityOptions.clear();

  err = EdsGetPropertyDesc(m_camera, kEdsPropID_ImageQuality, imageQualityDesc);
  int count = imageQualityDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      QString quality = QString::fromStdString(
          m_imageQualityMap[imageQualityDesc->propDesc[i]]);
      if (!quality.contains("RAW")) {
        m_imageQualityOptions.push_back(quality);
      }
      i++;
    }
  }
  delete imageQualityDesc;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::getAvailablePictureStyles() {
  EdsPropertyDesc *pictureStyleDesc = new EdsPropertyDesc;
  EdsError err                      = EDS_ERR_OK;
  m_pictureStyleOptions.clear();

  err = EdsGetPropertyDesc(m_camera, kEdsPropID_PictureStyle, pictureStyleDesc);
  int count = pictureStyleDesc->numElements;
  if (count > 0) {
    int i = 0;
    while (i < count) {
      m_pictureStyleOptions.push_back(QString::fromStdString(
          m_pictureStyleMap[pictureStyleDesc->propDesc[i]]));
      i++;
    }
  }
  delete pictureStyleDesc;
  return err;
}

//-----------------------------------------------------------------

void StopMotion::buildColorTemperatures() {
  m_colorTempOptions.clear();
  int i = 2800;
  while (i <= 10000) {
    m_colorTempOptions.push_back(QString::number(i));
    i += 100;
  }
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentShutterSpeed() {
  EdsError err = EDS_ERR_OK;
  EdsDataType tvType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_Tv, 0, &tvType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_Tv, 0, sizeof(size), &data);

  return QString::fromStdString(m_tvMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentIso() {
  EdsError err = EDS_ERR_OK;
  EdsDataType isoType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ISOSpeed, 0, &isoType, &size);
  err =
      EdsGetPropertyData(m_camera, kEdsPropID_ISOSpeed, 0, sizeof(size), &data);

  return QString::fromStdString(m_isoMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentAperture() {
  EdsError err = EDS_ERR_OK;
  EdsDataType avType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_Av, 0, &avType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_Av, 0, sizeof(size), &data);

  return QString::fromStdString(m_avMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentExposureCompensation() {
  EdsError err = EDS_ERR_OK;
  EdsDataType exposureType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ExposureCompensation, 0,
                           &exposureType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_ExposureCompensation, 0,
                           sizeof(size), &data);

  return QString::fromStdString(m_exposureMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentWhiteBalance() {
  EdsError err = EDS_ERR_OK;
  EdsDataType whiteBalanceType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_WhiteBalance, 0,
                           &whiteBalanceType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_WhiteBalance, 0, sizeof(size),
                           &data);
  std::string wbString = m_whiteBalanceMap[data];
  return QString::fromStdString(m_whiteBalanceMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentImageQuality() {
  EdsError err = EDS_ERR_OK;
  EdsDataType imageQualityType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ImageQuality, 0,
                           &imageQualityType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_ImageQuality, 0, sizeof(size),
                           &data);
  std::string wbString = m_imageQualityMap[data];
  return QString::fromStdString(m_imageQualityMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentPictureStyle() {
  EdsError err = EDS_ERR_OK;
  EdsDataType pictureStyleType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_PictureStyle, 0,
                           &pictureStyleType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_PictureStyle, 0, sizeof(size),
                           &data);
  std::string wbString = m_pictureStyleMap[data];
  return QString::fromStdString(m_pictureStyleMap[data]);
}

//-----------------------------------------------------------------

QString StopMotion::getCurrentColorTemperature() {
  EdsError err = EDS_ERR_OK;
  EdsDataType colorTempType;
  EdsUInt32 size;
  EdsUInt32 data;

  err = EdsGetPropertySize(m_camera, kEdsPropID_ColorTemperature, 0,
                           &colorTempType, &size);
  err = EdsGetPropertyData(m_camera, kEdsPropID_ColorTemperature, 0,
                           sizeof(size), &data);

  return QString::number(data);
}

//-----------------------------------------------------------------

EdsError StopMotion::setShutterSpeed(QString shutterSpeed) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_tvMap.begin();
  while (it != m_tvMap.end()) {
    if (it->second == shutterSpeed.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_Tv, 0, sizeof(value), &value);
  emit(shutterSpeedChangedSignal(shutterSpeed));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setIso(QString iso) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_isoMap.begin();
  while (it != m_isoMap.end()) {
    if (it->second == iso.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_ISOSpeed, 0, sizeof(value),
                           &value);
  emit(isoChangedSignal(iso));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setAperture(QString aperture) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_avMap.begin();
  while (it != m_avMap.end()) {
    if (it->second == aperture.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_Av, 0, sizeof(value), &value);
  emit(apertureChangedSignal(aperture));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setExposureCompensation(QString exposure) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_exposureMap.begin();
  while (it != m_exposureMap.end()) {
    if (it->second == exposure.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_ExposureCompensation, 0,
                           sizeof(value), &value);
  emit(exposureChangedSignal(exposure));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setWhiteBalance(QString whiteBalance) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_whiteBalanceMap.begin();
  while (it != m_whiteBalanceMap.end()) {
    if (it->second == whiteBalance.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_WhiteBalance, 0, sizeof(value),
                           &value);
  emit(whiteBalanceChangedSignal(whiteBalance));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setImageQuality(QString quality) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_imageQualityMap.begin();
  while (it != m_imageQualityMap.end()) {
    if (it->second == quality.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_ImageQuality, 0, sizeof(value),
                           &value);
  emit(imageQualityChangedSignal(quality));
  m_proxyImageDimensions = TDimension(0, 0);
  m_proxyDpi             = TPointD(0.0, 0.0);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setPictureStyle(QString style) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  auto it = m_pictureStyleMap.begin();
  while (it != m_pictureStyleMap.end()) {
    if (it->second == style.toStdString()) {
      value = it->first;
      break;
    }
    it++;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_PictureStyle, 0, sizeof(value),
                           &value);
  err = EdsSetPropertyData(m_camera, kEdsPropID_PictureStyle, 0, sizeof(value),
                           &value);
  emit(pictureStyleChangedSignal(style));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::setColorTemperature(QString temp) {
  EdsError err = EDS_ERR_OK;
  EdsUInt32 value;
  value = temp.toInt();

  err = EdsSetPropertyData(m_camera, kEdsPropID_ColorTemperature, 0,
                           sizeof(value), &value);
  err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_ColorTemperature, 0,
                           sizeof(value), &value);
  emit(colorTemperatureChangedSignal(temp));
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::downloadImage(EdsBaseRef object) {
  EdsError err        = EDS_ERR_OK;
  EdsStreamRef stream = NULL;
  EdsDirectoryItemInfo dirItemInfo;

  err = EdsGetDirectoryItemInfo(object, &dirItemInfo);
  err = EdsCreateMemoryStream(0, &stream);
  err = EdsDownload(object, dirItemInfo.size, stream);
  EdsDownloadComplete(object);

  // tj code

  unsigned __int64 mySize = 0;
  unsigned char *data     = NULL;
  err                     = EdsGetPointer(stream, (EdsVoid **)&data);
  err                     = EdsGetLength(stream, &mySize);

  int width, height, pixelFormat;
  // long size;
  int inSubsamp, inColorspace;
  // unsigned long jpegSize;
  tjhandle tjInstance   = NULL;
  unsigned char *imgBuf = NULL;
  tjInstance            = tjInitDecompress();
  tjDecompressHeader3(tjInstance, data, mySize, &width, &height, &inSubsamp,
                      &inColorspace);

  pixelFormat = TJPF_BGRX;
  imgBuf = (unsigned char *)tjAlloc(width * height * tjPixelSize[pixelFormat]);
  int flags = 0;
#ifdef WIN32
  flags |= TJFLAG_BOTTOMUP;
#endif
  int tempWidth, tempHeight;

  if (m_useScaledImages) {
    int factorsNum;
    tjscalingfactor scalingFactor = {1, 1};
    tjscalingfactor *factor       = tjGetScalingFactors(&factorsNum);
    int intRatio                  = (float)width / (float)height * 100.0;
    int i                         = 0;

    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimension res = camera->getRes();

    // find the scaling factor that is at least as big as the current camera
    while (i < factorsNum) {
      scalingFactor = factor[i];
      tempWidth     = TJSCALED(width, scalingFactor);
      if (tempWidth < res.lx && i > 0) {
        scalingFactor = factor[i - 1];
        break;
      }
      i++;
    }
    // make sure the scaling factor has the right aspect ratio
    while (i >= 0) {
      tempWidth  = TJSCALED(width, scalingFactor);
      tempHeight = TJSCALED(height, scalingFactor);
      if ((int)((float)tempWidth / (float)tempHeight * 100.0) == intRatio) {
        break;
      }
      i--;
      scalingFactor = factor[i];
    }
  } else {
    tempWidth  = width;
    tempHeight = height;
  }

  if (m_useScaledImages || !getCurrentImageQuality().contains("Large")) {
    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimensionD size       = camera->getSize();
    m_proxyImageDimensions = TDimension(tempWidth, tempHeight);
    double minimumDpi      = std::min(m_proxyImageDimensions.lx / size.lx,
                                 m_proxyImageDimensions.ly / size.ly);
    m_proxyDpi             = TPointD(minimumDpi, minimumDpi);
  }

  tjDecompress2(tjInstance, data, mySize, imgBuf, tempWidth,
                tempWidth * tjPixelSize[pixelFormat], tempHeight, pixelFormat,
                flags);

  m_newImage = TRaster32P(tempWidth, tempHeight);
  m_newImage->lock();
  uchar *rawData = m_newImage->getRawData();
  memcpy(rawData, imgBuf, tempWidth * tempHeight * tjPixelSize[pixelFormat]);
  m_newImage->unlock();

  tjFree(imgBuf);
  imgBuf = NULL;
  tjDestroy(tjInstance);
  tjInstance = NULL;

  // end tj code

  if (m_useScaledImages) {
    QFile fullImage(m_tempFile);
    fullImage.open(QIODevice::WriteOnly);
    QDataStream dataStream(&fullImage);
    dataStream.writeRawData((const char *)data, mySize);
    fullImage.close();
  }

  EdsRelease(stream);
  stream = NULL;
  if (object) EdsRelease(object);

  if (err == EDS_ERR_OK) {
    emit(newImageReady());
  }

  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::takePicture() {
  EdsError err;
  err = EdsSendCommand(m_camera, kEdsCameraCommand_PressShutterButton,
                       kEdsCameraCommand_ShutterButton_Completely_NonAF);
  err = EdsSendCommand(m_camera, kEdsCameraCommand_PressShutterButton,
                       kEdsCameraCommand_ShutterButton_OFF);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::startLiveView() {
  if (m_camera && m_sessionOpen) {
    EdsError err = EDS_ERR_OK;
    // Get the output device for the live view image
    EdsUInt32 device;
    err = EdsGetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                             sizeof(device), &device);
    // PC live view starts by setting the PC as the output device for the live
    // view image.
    if (err == EDS_ERR_OK) {
      device |= kEdsEvfOutputDevice_PC;
      err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                               sizeof(device), &device);
    }
    if (err == EDS_ERR_OK) {
      m_liveViewStatus = LiveViewStarting;
    }
    // A property change event notification is issued from the camera if
    // property settings are made successfully. Start downloading of the live
    // view image once the property change notification arrives.
    return err;
  } else
    return EDS_ERR_DEVICE_NOT_FOUND;
}

//-----------------------------------------------------------------

EdsError StopMotion::endLiveView() {
  EdsError err = EDS_ERR_OK;
  // Get the output device for the live view image
  EdsUInt32 device;
  err = EdsGetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                           sizeof(device), &device);
  // PC live view ends if the PC is disconnected from the live view image output
  // device.
  if (err == EDS_ERR_OK) {
    device &= ~kEdsEvfOutputDevice_PC;
    err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_OutputDevice, 0,
                             sizeof(device), &device);
  }
  m_liveViewStatus = LiveViewClosed;
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::zoomLiveView() {
  if (!m_sessionOpen) return EDS_ERR_DEVICE_INVALID;
  EdsError err = EDS_ERR_OK;

  if (m_liveViewZoom == 1) {
    m_liveViewZoom = 5;
    m_zooming      = true;
  } else if (m_liveViewZoom == 5) {
    m_liveViewZoom = 1;
    m_zooming      = false;
  }

  err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_Zoom, 0,
                           sizeof(m_liveViewZoom), &m_liveViewZoom);
  if (m_liveViewZoom == 5) setZoomPoint();

  return err;
}

//-----------------------------------------------------------------

void StopMotion::makeZoomPoint(TPointD pos) {
  m_liveViewZoomPickPoint = pos;
  double maxFullWidth =
      (double)m_fullImageDimensions.lx / m_fullImageDpi.x * Stage::inch;
  double maxFullHeight =
      (double)m_fullImageDimensions.ly / m_fullImageDpi.y * Stage::inch;
  double newX             = m_liveViewZoomPickPoint.x + maxFullWidth / 2.0;
  double newY             = -m_liveViewZoomPickPoint.y + maxFullHeight / 2.0;
  m_calculatedZoomPoint.x = newX / Stage::inch * m_fullImageDpi.x;
  m_calculatedZoomPoint.y = newY / Stage::inch * m_fullImageDpi.x;
}

//-----------------------------------------------------------------

EdsError StopMotion::setZoomPoint() {
  EdsError err = EDS_ERR_OK;

  EdsPoint zoomPoint;
  if (m_liveViewZoomPickPoint == TPointD(0.0, 0.0)) {
    m_calculatedZoomPoint =
        TPoint(m_fullImageDimensions.lx / 2, m_fullImageDimensions.ly / 2);
    m_finalZoomPoint.x = m_calculatedZoomPoint.x - (m_zoomRect.x / 2);
    m_finalZoomPoint.y = m_calculatedZoomPoint.y - (m_zoomRect.y / 2);
  } else {
    m_finalZoomPoint.x = m_calculatedZoomPoint.x - (m_zoomRect.x / 2);
    m_finalZoomPoint.y = m_calculatedZoomPoint.y - (m_zoomRect.y / 2);
    if (m_finalZoomPoint.x < 0) m_finalZoomPoint.x = 0;
    if (m_finalZoomPoint.y < 0) m_finalZoomPoint.y = 0;
    if (m_finalZoomPoint.x > m_fullImageDimensions.lx - (m_zoomRect.x)) {
      m_finalZoomPoint.x = m_fullImageDimensions.lx - (m_zoomRect.x);
    }
    if (m_finalZoomPoint.y > m_fullImageDimensions.ly - (m_zoomRect.y)) {
      m_finalZoomPoint.y = m_fullImageDimensions.ly - (m_zoomRect.y);
    }
  }

  zoomPoint.x = m_finalZoomPoint.x;
  zoomPoint.y = m_finalZoomPoint.y;

  // make sure this is set AFTER starting zoom
  err = EdsSetPropertyData(m_camera, kEdsPropID_Evf_ZoomPosition, 0,
                           sizeof(zoomPoint), &zoomPoint);

  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::downloadEVFData() {
  EdsError err            = EDS_ERR_OK;
  EdsStreamRef stream     = NULL;
  EdsEvfImageRef evfImage = NULL;

  // Create memory stream.
  err = EdsCreateMemoryStream(0, &stream);
  // Create EvfImageRef.
  if (err == EDS_ERR_OK) {
    err = EdsCreateEvfImageRef(stream, &evfImage);
  }
  // Download live view image data.
  if (err == EDS_ERR_OK) {
    err = EdsDownloadEvfImage(m_camera, evfImage);
  }

  EdsDataType evfZoom;
  EdsDataType evfZoomPos;
  EdsDataType evfZoomRect;
  EdsUInt32 size;
  EdsUInt32 sizePos;
  EdsUInt32 sizeRect;
  EdsUInt32 sizeImagePos;
  EdsUInt32 sizeCoordSys;
  EdsUInt32 zoomAmount;
  EdsPoint zoomPos;
  EdsRect zoomRect;
  EdsPoint imagePos;
  EdsSize coordSys;

  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_Zoom, 0, &evfZoom, &size);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_Zoom, 0, sizeof(size),
                           &zoomAmount);

  // this is the top corner? of the zoomed image
  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_ZoomPosition, 0,
                           &evfZoomPos, &sizePos);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_ZoomPosition, 0, sizePos,
                           &zoomPos);
  // this is the top corner of the zoomed image and the size of the zoomed image
  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_ZoomRect, 0, &evfZoomRect,
                           &sizeRect);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_ZoomRect, 0, sizeRect,
                           &zoomRect);

  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_ImagePosition, 0,
                           &evfZoomRect, &sizeImagePos);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_ImagePosition, 0,
                           sizeImagePos, &imagePos);
  // this returns the size of the full image
  err = EdsGetPropertySize(evfImage, kEdsPropID_Evf_CoordinateSystem, 0,
                           &evfZoomRect, &sizeCoordSys);
  err = EdsGetPropertyData(evfImage, kEdsPropID_Evf_CoordinateSystem, 0,
                           sizeCoordSys, &coordSys);

  m_zoomRect = TPoint(zoomRect.size.width, zoomRect.size.height);
  if (zoomAmount == 5 && m_zoomRect == TPoint(0, 0)) {
    setZoomPoint();
  }

  // Get the incidental data of the image.
  if (err == EDS_ERR_OK) {
    JpgConverter *converter = new JpgConverter;
    converter->setStream(stream);
    converter->setScale(m_useScaledImages);

    connect(converter, SIGNAL(imageReady(bool)), this, SLOT(onImageReady(bool)),
            Qt::QueuedConnection);
    connect(converter, SIGNAL(finished()), this, SLOT(onFinished()),
            Qt::QueuedConnection);

    converter->start();

    while (!l_quitLoop)
      QCoreApplication::processEvents(QEventLoop::AllEvents |
                                      QEventLoop::WaitForMoreEvents);

    l_quitLoop         = false;
    m_liveViewImage    = converter->getImage();
    m_hasLiveViewImage = true;
    delete converter;
    if (!m_converterSucceeded) return EDS_ERR_UNEXPECTED_EXCEPTION;

    // make sure not to set to LiveViewOpen if it has been turned off
    if (m_liveViewStatus > 0) {
      m_liveViewStatus = LiveViewOpen;
    }
    emit(newLiveViewImageReady());

    if (m_hasLiveViewImage &&
        (m_liveViewDpi.x == 0.0 || m_liveViewImageDimensions.lx == 0)) {
      TCamera *camera =
          TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
      TDimensionD size = camera->getSize();
      m_liveViewImageDimensions =
          TDimension(m_liveViewImage->getLx(), m_liveViewImage->getLy());
      double minimumDpi = std::min(m_liveViewImageDimensions.lx / size.lx,
                                   m_liveViewImageDimensions.ly / size.ly);
      m_liveViewDpi     = TPointD(minimumDpi, minimumDpi);

      m_fullImageDimensions = TDimension(coordSys.width, coordSys.height);
      minimumDpi            = std::min(m_fullImageDimensions.lx / size.lx,
                            m_fullImageDimensions.ly / size.ly);
      m_fullImageDpi        = TPointD(minimumDpi, minimumDpi);

      emit(newDimensions());
    }
  }

  if (stream != NULL) {
    EdsRelease(stream);
  }
  stream = NULL;
  if (evfImage != NULL) {
    EdsRelease(evfImage);
    evfImage = NULL;
  }

  // calculate dpi data

  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::focusNear() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Near1);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::focusFar() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Far1);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::focusNear2() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Near2);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::focusFar2() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Far2);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::focusNear3() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Near3);
  return err;
}

//-----------------------------------------------------------------

EdsError StopMotion::focusFar3() {
  EdsError err = EDS_ERR_OK;
  err          = EdsSendCommand(m_camera, kEdsCameraCommand_DriveLensEvf,
                       kEdsEvfDriveLens_Far3);
  return err;
}

//-----------------------------------------------------------------

void StopMotion::onImageReady(const bool &status) {
  m_converterSucceeded = status;
}

//-----------------------------------------------------------------

void StopMotion::onFinished() { l_quitLoop = true; }

//-----------------------------------------------------------------
//-----------------------------------------------------------------

EdsError StopMotion::handleObjectEvent(EdsObjectEvent event, EdsBaseRef object,
                                       EdsVoid *context) {
  if (event == kEdsObjectEvent_DirItemRequestTransfer) {
    instance()->downloadImage(object);
  }

  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

EdsError StopMotion::handlePropertyEvent(EdsPropertyEvent event,
                                         EdsPropertyID property,
                                         EdsUInt32 param, EdsVoid *context) {
  if (property == kEdsPropID_Evf_OutputDevice &&
      event == kEdsPropertyEvent_PropertyChanged) {
    if (instance()->m_liveViewStatus == LiveViewStarting)
      instance()->m_liveViewStatus = LiveViewOpen;
  }
  if (property == kEdsPropID_AEMode &&
      event == kEdsPropertyEvent_PropertyChanged) {
    emit(instance()->modeChanged());
  }

  if (property == kEdsPropID_Av &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->apertureOptionsChanged());
  }

  if (property == kEdsPropID_Tv &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->shutterSpeedOptionsChanged());
  }
  if (property == kEdsPropID_ISOSpeed &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->isoOptionsChanged());
  }
  if (property == kEdsPropID_ExposureCompensation &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->exposureOptionsChanged());
  }
  if (property == kEdsPropID_WhiteBalance &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->whiteBalanceOptionsChanged());
  }
  if (property == kEdsPropID_PictureStyle &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->pictureStyleOptionsChanged());
  }
  if (property == kEdsPropID_ImageQuality &&
      event == kEdsPropertyEvent_PropertyDescChanged) {
    emit(instance()->imageQualityOptionsChanged());
  }

  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

EdsError StopMotion::handleStateEvent(EdsStateEvent event, EdsUInt32 parameter,
                                      EdsVoid *context) {
  if (event == kEdsStateEvent_Shutdown) {
    if (instance()->m_sessionOpen) {
      instance()->closeCameraSession();
      instance()->releaseCamera();
      instance()->m_liveViewStatus = LiveViewClosed;
      emit(instance()->cameraChanged());
    }
  }
  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

EdsError StopMotion::handleCameraAddedEvent(EdsVoid *context) {
  instance()->cameraAdded();
  return EDS_ERR_OK;
}

//-----------------------------------------------------------------

void StopMotion::buildAvMap() {
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x00, "00"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x08, "1"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x0B, "1.1"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x0C, "1.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x0D, "1.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x10, "1.4"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x13, "1.6"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x14, "1.8"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x15, "1.8"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x18, "2"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x1B, "2.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x1C, "2.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x1D, "2.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x20, "2.8"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x23, "3.2"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x24, "3.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x25, "3.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x28, "4"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x2B, "4.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x2C, "4.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x2D, "5.0"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x30, "5.6"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x33, "6.3"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x34, "6.7"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x35, "7.1"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x38, "8"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x3B, "9"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x3C, "9.5"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x3D, "10"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x40, "11"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x43, "13"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x44, "13"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x45, "14"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x48, "16"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x4B, "18"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x4C, "19"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x4D, "20"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x50, "22"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x53, "25"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x54, "27"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x55, "29"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x58, "32"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x5B, "36"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x5C, "38"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x5D, "40"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x60, "45"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x63, "51"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x64, "54"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x65, "57"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x68, "64"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x6B, "72"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x6C, "76"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x6D, "80"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0x70, "91"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0xFF, "Auto"));
  m_avMap.insert(std::pair<EdsUInt32, const char *>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void StopMotion::buildIsoMap() {
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x00, "Auto"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x28, "6"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x30, "12"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x38, "25"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x40, "50"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x48, "100"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x4b, "125"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x4d, "160"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x50, "200"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x53, "250"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x55, "320"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x58, "400"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x5b, "500"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x5d, "640"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x60, "800"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x63, "1000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x65, "1250"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x68, "1600"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x6b, "2000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x6d, "2500"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x70, "3200"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x73, "4000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x75, "5000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x78, "6400"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x7b, "8000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x7d, "10000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x80, "12800"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x83, "16000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x85, "20000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x88, "25600"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x8b, "32000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x8d, "40000"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x90, "51200"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0x98, "102400"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0xa0, "204800"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0xa8, "409600"));
  m_isoMap.insert(std::pair<EdsUInt32, const char *>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void StopMotion::buildTvMap() {
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x04, "Auto"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x0c, "Bulb"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x10, "30\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x13, "25\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x14, "20\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x15, "20\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x18, "15\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x1B, "13\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x1C, "10\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x1D, "10\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x20, "8\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x23, "6\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x24, "6\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x25, "5\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x28, "4\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x2B, "3\"2"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x2C, "3\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x2D, "2\"5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x30, "2\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x33, "1\"6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x34, "1\"5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x35, "1\"3"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x38, "1\""));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x3B, "0\"8"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x3C, "0\"7"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x3D, "0\"6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x40, "0\"5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x43, "0\"4"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x44, "0\"3"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x45, "0\"3"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x48, "1/4"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x4B, "1/5"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x4C, "1/6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x4D, "1/6"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x50, "1/8"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x53, "1/10"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x54, "1/10"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x55, "1/13"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x58, "1/15"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x5B, "1/20"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x5C, "1/20"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x5D, "1/25"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x60, "1/30"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x63, "1/40"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x64, "1/45"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x65, "1/50"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x68, "1/60"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x6B, "1/80"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x6C, "1/90"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x6D, "1/100"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x70, "1/125"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x73, "1/160"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x74, "1/180"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x75, "1/200"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x78, "1/250"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x7B, "1/320"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x7C, "1/350"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x7D, "1/400"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x80, "1/500"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x83, "1/640"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x84, "1/750"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x85, "1/800"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x88, "1/1000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x8B, "1/1250"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x8C, "1/1500"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x8D, "1/1600"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x90, "1/2000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x93, "1/2500"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x94, "1/3000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x95, "1/3200"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x98, "1/4000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x9B, "1/5000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x9C, "1/6000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0x9D, "1/6400"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0xA0, "1/8000"));
  m_tvMap.insert(std::pair<EdsUInt32, const char *>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void StopMotion::buildModeMap() {
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(0, "P"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(1, "Tv"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(2, "Av"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(3, "M"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(55, "FV"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(4, "Bulb"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(5, "A-DEP"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(6, "DEP"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(7, "C1"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(16, "C2"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(17, "C3"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(8, "Lock"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(9, "GreenMode"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(10, "Night Portrait"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(11, "Sports"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(13, "LandScape"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(14, "Close-Up"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(15, "No Strobo"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(12, "Portrait"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(19, "Creative Auto"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(20, "Movies"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(25, "SCN"));
  m_modeMap.insert(
      std::pair<EdsUInt32, const char *>(22, "Scene Intelligent Auto"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(29, "Creative filters"));
  m_modeMap.insert(std::pair<EdsUInt32, const char *>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void StopMotion::buildExposureMap() {
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x28, "+5"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x25, "+4 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x24, "+4 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x23, "+4 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x20, "+4"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x1D, "+3 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x1C, "+3 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x1B, "+3 1/3"));

  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x18, "+3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x15, "+2 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x14, "+2 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x13, "+2 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x10, "+2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x0d, "+1 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x0c, "+1 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x0b, "+1 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x08, "+1"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x05, "+2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x04, "+1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x03, "+1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0x00, "0"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xfd, "-1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xfc, "-1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xfb, "-2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xf8, "-1"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xf5, "-1 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xf4, "-1 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xf3, "-1 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xf0, "-2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xed, "-2 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xec, "-2 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xeb, "-2 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xe8, "-3"));

  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xE5, "-3 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xE4, "-3 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xE3, "-3 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xE0, "-4"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xDD, "-4 1/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xDC, "-4 1/2"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xDB, "-4 2/3"));
  m_exposureMap.insert(std::pair<EdsUInt32, const char *>(0xD8, "-5"));

  m_exposureMap.insert(
      std::pair<EdsUInt32, const char *>(0xffffffff, "unknown"));
}

//-----------------------------------------------------------------

void StopMotion::buildWhiteBalanceMap() {
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char *>(0, "Auto: Ambience Priority"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(1, "Daylight"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(2, "Cloudy"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(3, "Tungsten"));
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char *>(4, "Fluorescent"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(5, "Flash"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(6, "Manual"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(8, "Shade"));
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char *>(9, "Color Temperature"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(10, "Custom 1"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(11, "Custom 2"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(12, "Custom 3"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(15, "Manual 2"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(16, "Manual 3"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(18, "Manual 4"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(19, "Manual 5"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(20, "Custom 4"));
  m_whiteBalanceMap.insert(std::pair<EdsUInt32, const char *>(21, "Custom 5"));
  m_whiteBalanceMap.insert(
      std::pair<EdsUInt32, const char *>(23, "Auto: White Priority"));
}

//-----------------------------------------------------------------

void StopMotion::buildImageQualityMap() {
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_LR, "RAW"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRLJF, "RAW + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRMJF, "RAW + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRSJF, "RAW + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRLJN, "RAW + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRMJN, "RAW + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRSJN, "RAW + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRS1JF, "RAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRS1JN, "RAW + Small1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRS2JF, "RAW + Small2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRS3JF, "RAW + Small3 Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRLJ, "RAW + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRM1J, "RAW + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRM2J, "RAW + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LRSJ, "RAW + Small Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MR, "Middle Raw(Small RAW1)"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRLJF, "Middle Raw(Small RAW1) + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRMJF, "Middle Raw(Small RAW1) + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRSJF, "Middle Raw(Small RAW1) + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRLJN, "Middle Raw(Small RAW1) + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRMJN, "Middle Raw(Small RAW1) + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRSJN, "Middle Raw(Small RAW1) + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRS1JF, "Middle RAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRS1JN, "Middle RAW + Small1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRS2JF, "Middle RAW + Small2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRS3JF, "Middle RAW + Small3 Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRLJ, "Middle Raw + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRM1J, "Middle Raw + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRM2J, "Middle Raw + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MRSJ, "Middle Raw + Small Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SR, "Small RAW(Small RAW2)"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRLJF, "Small RAW(Small RAW2) + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRMJF, "Small RAW(Small RAW2) + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRSJF, "Small RAW(Small RAW2) + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRLJN, "Small RAW(Small RAW2) + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRMJN, "Small RAW(Small RAW2) + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRSJN, "Small RAW(Small RAW2) + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRS1JF, "Small RAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRS1JN, "Small RAW + Small1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRS2JF, "Small RAW + Small2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRS3JF, "Small RAW + Small3 Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRLJ, "Small RAW + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRM1J, "Small RAW + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRM2J, "Small RAW + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SRSJ, "Small RAW + Small Jpeg"));

  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_CR, "CRAW"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRLJF, "CRAW + Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRMJF, "CRAW + Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRM1JF, "CRAW + Middle1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRM2JF, "CRAW + Middle2 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRSJF, "CRAW + Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRS1JF, "CRAW + Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRS2JF, "CRAW + Small2 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRS3JF, "CRAW + Small3 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRLJN, "CRAW + Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRMJN, "CRAW + Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRM1JN, "CRAW + Middle1 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRM2JN, "CRAW + Middle2 Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRSJN, "CRAW + Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRS1JN, "CRAW + Small1 Normal Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRLJ, "CRAW + Large Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRM1J, "CRAW + Middle1 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRM2J, "CRAW + Middle2 Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_CRSJ, "CRAW + Small Jpeg"));

  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LJF, "Large Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_LJN, "Large Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MJF, "Middle Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_MJN, "Middle Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SJF, "Small Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_SJN, "Small Normal Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_S1JF, "Small1 Fine Jpeg"));
  m_imageQualityMap.insert(std::pair<EdsUInt32, const char *>(
      EdsImageQuality_S1JN, "Small1 Normal Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_S2JF, "Small2 Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_S3JF, "Small3 Jpeg"));

  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_LJ, "Large Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_M1J, "Middle1 Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_M2J, "Middle2 Jpeg"));
  m_imageQualityMap.insert(
      std::pair<EdsUInt32, const char *>(EdsImageQuality_SJ, "Small Jpeg"));
}

//-----------------------------------------------------------------

void StopMotion::buildPictureStyleMap() {
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char *>(
      kEdsPictureStyle_Standard, "Standard"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char *>(
      kEdsPictureStyle_Portrait, "Portrait"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char *>(
      kEdsPictureStyle_Landscape, "Landscape"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_Neutral, "Neutral"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char *>(
      kEdsPictureStyle_Faithful, "Faithful"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char *>(
      kEdsPictureStyle_Monochrome, "Monochrome"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_Auto, "Auto"));
  m_pictureStyleMap.insert(std::pair<EdsUInt32, const char *>(
      kEdsPictureStyle_FineDetail, "Fine Detail"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_User1, "User 1"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_User2, "User 2"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_User3, "User 3"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_PC1, "Computer 1"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_PC2, "Computer 2"));
  m_pictureStyleMap.insert(
      std::pair<EdsUInt32, const char *>(kEdsPictureStyle_PC3, "Computer 3"));
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

class StopMotionToggleZoomCommand : public MenuItemHandler {
public:
  StopMotionToggleZoomCommand() : MenuItemHandler(MI_StopMotionToggleZoom) {}
  void execute() {
    StopMotion *sm = StopMotion::instance();
    sm->zoomLiveView();
  }
} StopMotionToggleZoomCommand;

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