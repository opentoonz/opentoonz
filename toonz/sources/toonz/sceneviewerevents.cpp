

#include <QtGlobal>

#include "sceneviewercontextmenu.h"

// Tnz6 includes
#include "sceneviewer.h"
#include "sceneviewerevents.h"
#include "viewerpane.h"
#include "tapp.h"
#include "iocommand.h"
#include "menubarcommandids.h"
#include "onionskinmaskgui.h"
#include "ruler.h"
#include "comboviewerpane.h"
#include "locatorpopup.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"

// TnzTools includes
#include "tools/cursors.h"
#include "tools/toolhandle.h"
#include "tools/cursormanager.h"
#include "tools/toolcommandids.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/palettecontroller.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/dpiscale.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/selection.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "subcameramanager.h"

#include "tw/keycodes.h"

// TnzCore includes
#include "timagecache.h"
#include "trasterimage.h"
#include "tvectorimage.h"
#include "tpalette.h"

// Qt includes
#include <QUrl>
#include <QApplication>
#include <QDebug>
#include <QMimeData>

// definito - per ora - in tapp.cpp
extern QString updateToolEnableStatus(TTool *tool);

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

// ahime': baco di Qt. il tablet event non conosce i modificatori correnti
int modifiers = 0;

//-----------------------------------------------------------------------------

void initToonzEvent(TMouseEvent &toonzEvent, QMouseEvent *event,
                    int widgetHeight, double pressure, int devPixRatio) {
  toonzEvent.m_pos = TPoint(event->pos().x() * devPixRatio,
                            widgetHeight - 1 - event->pos().y() * devPixRatio);
  toonzEvent.m_mousePos = event->pos();
  toonzEvent.m_pressure = 255;

  toonzEvent.setModifiers(event->modifiers() & Qt::ShiftModifier,
                          event->modifiers() & Qt::AltModifier,
                          event->modifiers() & Qt::ControlModifier);

  toonzEvent.m_buttons  = event->buttons();
  toonzEvent.m_button   = event->button();
  toonzEvent.m_isTablet = false;
}

//-----------------------------------------------------------------------------

void initToonzEvent(TMouseEvent &toonzEvent, QTabletEvent *event,
                    int widgetHeight, double pressure, int devPixRatio) {
  toonzEvent.m_pos = TPoint(event->pos().x() * devPixRatio,
                            widgetHeight - 1 - event->pos().y() * devPixRatio);
  toonzEvent.m_mousePos = event->pos();
  toonzEvent.m_pressure = int(255 * pressure);

  toonzEvent.setModifiers(event->modifiers() & Qt::ShiftModifier,
                          event->modifiers() & Qt::AltModifier,
                          event->modifiers() & Qt::ControlModifier);

  toonzEvent.m_buttons  = event->buttons();
  toonzEvent.m_button   = event->button();
  toonzEvent.m_isTablet = true;
}

//-----------------------------------------------------------------------------

void initToonzEvent(TMouseEvent &toonzEvent, QKeyEvent *event) {
  toonzEvent.m_pos      = TPoint();
  toonzEvent.m_mousePos = QPoint();
  toonzEvent.m_pressure = 0;

  toonzEvent.setModifiers(event->modifiers() & Qt::ShiftModifier,
                          event->modifiers() & Qt::AltModifier,
                          event->modifiers() & Qt::ControlModifier);
  toonzEvent.m_buttons  = Qt::NoButton;
  toonzEvent.m_button   = Qt::NoButton;
  toonzEvent.m_isTablet = false;
}

//-----------------------------------------------------------------------------

//====================================
//    SceneViewerShortcutReceiver
//------------------------------------

class SceneViewerShortcutReceiver {
  SceneViewer *m_viewer;

public:
  SceneViewerShortcutReceiver(SceneViewer *viewer) : m_viewer(viewer) {}
  ~SceneViewerShortcutReceiver() {}

  bool exec(QKeyEvent *event) {
    CommandManager *cManager = CommandManager::instance();

    if (event->key() == cManager->getKeyFromId(MI_RegeneratePreview)) {
      m_viewer->regeneratePreview();
      return true;
    }

    if (event->key() == cManager->getKeyFromId(MI_RegenerateFramePr)) {
      m_viewer->regeneratePreviewFrame();
      return true;
    }

    if (event->key() == cManager->getKeyFromId(MI_SavePreviewedFrames)) {
      Previewer::instance(m_viewer->getPreviewMode() ==
                          SceneViewer::SUBCAMERA_PREVIEW)
          ->saveRenderedFrames();
      return true;
    }

    return false;
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

using namespace DVGui;

void SceneViewer::onButtonPressed(FlipConsole::EGadget button) {
  if (m_freezedStatus != NO_FREEZED) return;
  switch (button) {
  case FlipConsole::eSaveImg: {
    if (m_previewMode == NO_PREVIEW) {
      DVGui::warning(QObject::tr(
          "It is not possible to save images in camera stand view."));
      return;
    }
    TApp *app = TApp::instance();
    int row   = app->getCurrentFrame()->getFrame();

    Previewer *previewer =
        Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW);
    if (!previewer->isFrameReady(row)) {
      DVGui::warning(QObject::tr("The preview images are not ready yet."));
      return;
    }

    TRasterP ras =
        previewer->getRaster(row, m_visualSettings.m_recomputeIfNeeded);

    TImageCache::instance()->add(QString("TnzCompareImg"),
                                 TRasterImageP(ras->clone()));

    break;
  }

  case FlipConsole::eSave:
    Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW)
        ->saveRenderedFrames();
    break;

  case FlipConsole::eHisto: {
    QAction *action = CommandManager::instance()->getAction(MI_Histogram);
    action->trigger();
    break;
  }

  case FlipConsole::eDefineSubCamera:
    m_editPreviewSubCamera = !m_editPreviewSubCamera;
    update();
    break;

  // open locator. Create one for the first time
  case FlipConsole::eLocator:
    if (!m_locator) m_locator = new LocatorPopup(this);
    m_locator->show();
    m_locator->raise();
    m_locator->activateWindow();
    break;
  }
}

//-----------------------------------------------------------------------------

void SceneViewer::tabletEvent(QTabletEvent *e) {
  if (m_freezedStatus != NO_FREEZED) return;

  m_tabletEvent    = true;
  m_pressure       = e->pressure();
  m_tabletPressed  = false;
  m_tabletReleased = false;
  m_tabletMove     = false;
  // Management of the Eraser pointer
  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  if (e->pointerType() == QTabletEvent::Eraser) {
    if (!m_eraserPointerOn) {
      if (toolHandle->getTool()->getName() != T_Eraser) {
        // Save the current tool
        m_backupTool = QString::fromStdString(toolHandle->getTool()->getName());
        // Switch to the Eraser tool
        toolHandle->setTool(T_Eraser);
        m_eraserPointerOn = true;
      }
    }
  } else {
    if (m_eraserPointerOn) {
      // Restore the previous tool
      toolHandle->setTool(m_backupTool);
      m_eraserPointerOn = false;
    }
  }
  switch (e->type()) {
  case QEvent::TabletPress: {
    // In OSX tablet action may cause only tabletEvent, not followed by
    // mousePressEvent.
    // So call onPress here in order to enable processing.
    // This separation done is only for the left Button (regular pen touch),
    // because
    // the current Qt seems to fail to catch the Wacom's button binding properly
    // with QTabletEvent->button() when pressing middle or right button.
    // So, in such case set m_tabletEvent = FALSE and let the mousePressEvent to
    // work.
    if (e->button() == Qt::LeftButton) {
      TMouseEvent mouseEvent;
      initToonzEvent(mouseEvent, e, height(), e->pressure(), getDevPixRatio());
      m_tabletPressed = true;
      onPress(mouseEvent);
    } else
      m_tabletEvent = false;
  } break;
  case QEvent::TabletRelease: {
    if (m_tabletActive) {
      m_tabletReleased = true;
      TMouseEvent mouseEvent;
      initToonzEvent(mouseEvent, e, height(), e->pressure(), getDevPixRatio());
      onRelease(mouseEvent);
    } else
      m_tabletEvent = false;
  } break;
  // for now "TabletMove" will be called only when with some button.
  // setTabletTracking(bool) will be introduced in Qt5.9
  case QEvent::TabletMove: {
    if (m_tabletActive) {
      QPoint curPos = e->pos() * getDevPixRatio();
      // It seems that the tabletEvent is called more often than mouseMoveEvent.
      // So I fire the interval timer in order to limit the following process
      // to be called in 50fps in maximum.
      if (curPos != m_lastMousePos && !m_isBusyOnTabletMove) {
        m_isBusyOnTabletMove = true;
        TMouseEvent mouseEvent;
        initToonzEvent(mouseEvent, e, height(), e->pressure(),
                       getDevPixRatio());
        m_tabletMove = true;
        QTimer::singleShot(20, this, SLOT(releaseBusyOnTabletMove()));
        onMove(mouseEvent);
      }
    } else
      m_tabletEvent = false;
  } break;
  default:
    break;
  }
  e->accept();
}

//-----------------------------------------------------------------------------

void SceneViewer::leaveEvent(QEvent *) {
  if (!m_isMouseEntered) return;

  m_isMouseEntered = false;

  if (m_freezedStatus != NO_FREEZED) return;
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) tool->onLeave();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::enterEvent(QEvent *) {
  if (m_isMouseEntered) return;

  m_isMouseEntered = true;

  TApp *app        = TApp::instance();
  modifiers        = 0;
  TTool *tool      = app->getCurrentTool()->getTool();
  TXshLevel *level = app->getCurrentLevel()->getLevel();
  if (level && level->getSimpleLevel())
    m_dpiScale =
        getCurrentDpiScale(level->getSimpleLevel(), tool->getCurrentFid());
  else
    m_dpiScale = TPointD(1, 1);

  if (m_freezedStatus != NO_FREEZED) return;

  invalidateToolStatus();
  if (tool && tool->isEnabled()) {
    tool->setViewer(this);
    tool->updateMatrix();
    tool->onEnter();
  }

  setFocus();
  updateGL();
}

//-----------------------------------------------------------------------------

void SceneViewer::mouseMoveEvent(QMouseEvent *event) {
  // if this is called just after tabletEvent, skip the execution
  if (m_tabletEvent) return;

  // there are three cases to come here :
  // 1. on mouse is moved (no tablet is used)
  // 2. on tablet is moved, with middle or right button is pressed
  // 3. on tablet is moved, BUT tabletEvent was not called
  // 4. on tablet is moved without pen touching (i.e. floating move)
  // the case 3 sometimes occurs on OSX. (reporteed in QTBUG-26532 for Qt4, but
  // not confirmed in Qt5)
  // the case 4 can be removed once start using Qt5.9 and call
  // setTabletTracking(true).

  TMouseEvent mouseEvent;
  initToonzEvent(mouseEvent, event, height(), 1.0, getDevPixRatio());
  onMove(mouseEvent);
}

//-----------------------------------------------------------------------------

void SceneViewer::onMove(const TMouseEvent &event) {
  if (m_freezedStatus != NO_FREEZED) return;

  int devPixRatio = getDevPixRatio();
  QPoint curPos   = event.mousePos() * devPixRatio;
  bool cursorSet  = false;
  m_lastMousePos  = curPos;

  if (m_editPreviewSubCamera) {
    if (!PreviewSubCameraManager::instance()->mouseMoveEvent(this, event))
      return;
  }

  // if the "compare with snapshot" mode is activated, change the mouse cursor
  // on the border handle
  if (m_visualSettings.m_doCompare) {
    if (abs(curPos.x() - width() * m_compareSettings.m_compareX) < 20) {
      cursorSet = true;
      setToolCursor(this, ToolCursor::ScaleHCursor);
    } else if (abs((height() - curPos.y()) -
                   height() * m_compareSettings.m_compareY) < 20) {
      cursorSet = true;
      setToolCursor(this, ToolCursor::ScaleVCursor);
    }
  }

  // control of the border handle when the "compare with snapshot" mode is
  // activated
  if (m_compareSettings.m_dragCompareX || m_compareSettings.m_dragCompareY) {
    if (m_compareSettings.m_dragCompareX)
      m_compareSettings.m_compareX +=
          ((double)(curPos.x() - m_pos.x())) / width();
    else if (m_compareSettings.m_dragCompareY)
      m_compareSettings.m_compareY -=
          ((double)(curPos.y() - m_pos.y())) / height();
    m_compareSettings.m_compareX =
        tcrop(m_compareSettings.m_compareX, 0.0, 1.0);
    m_compareSettings.m_compareY =
        tcrop(m_compareSettings.m_compareY, 0.0, 1.0);

    update();

    m_pos = curPos;
  } else if (m_mouseButton == Qt::NoButton || m_mouseButton == Qt::LeftButton) {
    if (is3DView() && m_current3DDevice != NONE &&
        m_mouseButton == Qt::LeftButton)
      return;
    else if (is3DView() && m_mouseButton == Qt::NoButton) {
      TRectD rect = TRectD(TPointD(m_topRasterPos.x, m_topRasterPos.y),
                           TDimensionD(20 * devPixRatio, 20 * devPixRatio));
      if (rect.contains(TPointD(curPos.x(), curPos.y())))
        m_current3DDevice = TOP_3D;
      else {
        rect = TRectD(TPointD(m_sideRasterPos.x, m_sideRasterPos.y),
                      TDimensionD(20 * devPixRatio, 20 * devPixRatio));
        if (rect.contains(TPointD(curPos.x(), curPos.y()))) {
          if (m_phi3D > 0)
            m_current3DDevice = SIDE_RIGHT_3D;
          else
            m_current3DDevice = SIDE_LEFT_3D;
        } else
          m_current3DDevice = NONE;
      }
      if (m_current3DDevice != NONE) {
        cursorSet = true;
        setToolCursor(this, ToolCursor::CURSOR_ARROW);
      }
    }

    // if the middle mouse button is pressed while dragging, then do panning
    if (event.buttons() & Qt::MidButton) {
      // panning
      QPoint p = curPos - m_pos;
      if (m_pos == QPoint(0, 0) && p.manhattanLength() > 300) return;
      panQt(curPos - m_pos);
      m_pos = curPos;
      return;
    }

    TTool *tool = TApp::instance()->getCurrentTool()->getTool();
    if (!tool || !tool->isEnabled()) {
      m_tabletEvent = false;
      return;
    }
    tool->setViewer(this);
    TPointD worldPos = winToWorld(curPos);
    TPointD pos      = tool->getMatrix().inv() * worldPos;

    if (m_locator) {
      m_locator->onChangeViewAff(worldPos);
    }

    TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
    if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
      pos.x /= m_dpiScale.x;
      pos.y /= m_dpiScale.y;
    }

    // qDebug() << "mouseMoveEvent. "  << (m_tabletEvent?"tablet":"mouse")
    //         << " pressure=" << m_pressure << " mouseButton=" << m_mouseButton
    //         << " buttonClicked=" << m_buttonClicked;

    // separate tablet events from mouse events
    // don't perform a drag event if tablet not active
    if (m_tabletActive && !m_tabletMove) return;
    if (m_tabletEvent && m_tabletActive && m_tabletMove) {
      tool->leftButtonDrag(pos, event);
    }

    else if (m_mouseButton == Qt::LeftButton) {
      // sometimes the mousePressedEvent is postponed to a wrong  mouse move
      // event!
      if (m_buttonClicked && !m_toolSwitched) tool->leftButtonDrag(pos, event);
    } else if (m_pressure == 0) {
      tool->mouseMove(pos, event);
    }
    if (!cursorSet) setToolCursor(this, tool->getCursorId());
    m_pos        = curPos;
    m_tabletMove = false;
  } else if (m_mouseButton == Qt::MidButton) {
    if ((event.buttons() & Qt::MidButton) == 0) m_mouseButton = Qt::NoButton;
    // scrub with shift and middle click
    else if (event.isShiftPressed() && event.isCtrlPressed()) {
      if (curPos.x() > m_pos.x()) {
        CommandManager::instance()->execute("MI_NextFrame");
      } else if (curPos.x() < m_pos.x()) {
        CommandManager::instance()->execute("MI_PrevFrame");
      }
    } else {
      // panning
      panQt(curPos - m_pos);
    }
    m_pos = curPos;

    return;
  }
}

//-----------------------------------------------------------------------------

void SceneViewer::mousePressEvent(QMouseEvent *event) {
  // if this is called just after tabletEvent, skip the execution
  if (m_tabletEvent) return;

  // For now OSX has a critical problem that mousePressEvent is called just
  // after releasing the stylus, which causes the irregular stroke while
  // float-moving.
  // In such case resetTabletStatus() will be called on
  // QEvent::TabletLeaveProximity
  // and will cancel the onPress operation called here.

  TMouseEvent mouseEvent;
  initToonzEvent(mouseEvent, event, height(), 1.0, getDevPixRatio());
  onPress(mouseEvent);
}

//-----------------------------------------------------------------------------

void SceneViewer::onPress(const TMouseEvent &event) {
  // evita i press ripetuti
  if (m_buttonClicked) return;
  m_buttonClicked = true;
  m_toolSwitched  = false;
  if (m_freezedStatus != NO_FREEZED) return;

  if (m_mouseButton != Qt::NoButton) return;

  m_pos         = event.mousePos() * getDevPixRatio();
  m_mouseButton = event.button();

  // when using tablet, avoid unexpected drawing behavior occurs when
  // middle-click
  if (m_tabletEvent && m_mouseButton == Qt::MidButton &&
      event.isLeftButtonPressed()) {
    return;
  }

  if (is3DView() && m_current3DDevice != NONE &&
      m_mouseButton == Qt::LeftButton)
    return;
  else if (m_mouseButton == Qt::LeftButton && m_editPreviewSubCamera) {
    if (!PreviewSubCameraManager::instance()->mousePressEvent(this, event))
      return;
  } else if (m_mouseButton == Qt::LeftButton && m_visualSettings.m_doCompare) {
    if (abs(m_pos.x() - width() * m_compareSettings.m_compareX) < 20) {
      m_compareSettings.m_dragCompareX = true;
      m_compareSettings.m_dragCompareY = false;
      m_compareSettings.m_compareY     = ImagePainter::DefaultCompareValue;
      update();
      m_tabletEvent = false;
      return;
    } else if (abs((height() - m_pos.y()) -
                   height() * m_compareSettings.m_compareY) < 20) {
      m_compareSettings.m_dragCompareY = true;
      m_compareSettings.m_dragCompareX = false;
      m_compareSettings.m_compareX     = ImagePainter::DefaultCompareValue;
      update();
      m_tabletEvent = false;
      return;
    } else
      m_compareSettings.m_dragCompareX = m_compareSettings.m_dragCompareY =
          false;
  }

  if (m_freezedStatus != NO_FREEZED) return;

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool || !tool->isEnabled()) {
    m_tabletEvent = false;
    return;
  }
  tool->setViewer(this);
  if (m_mouseButton == Qt::LeftButton && tool->preLeftButtonDown()) {
    tool = TApp::instance()->getCurrentTool()->getTool();
    if (!tool || !tool->isEnabled()) {
      m_tabletEvent = false;
      return;
    }
    tool->setViewer(this);
  }

  // if(!m_tabletEvent) qDebug() << "-----------------MOUSE PRESS 'PURO'.
  // POSSIBILE EMBOLO";
  TPointD pos = tool->getMatrix().inv() * winToWorld(m_pos);

  TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
  if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
    pos.x /= m_dpiScale.x;
    pos.y /= m_dpiScale.y;
  }
  // separate tablet and mouse events
  if (m_tabletEvent && m_tabletPressed) {
    m_tabletActive = true;
    tool->leftButtonDown(pos, event);
  } else if (m_mouseButton == Qt::LeftButton) {
    TApp::instance()->getCurrentTool()->setToolBusy(true);
    tool->leftButtonDown(pos, event);
  }
  if (m_mouseButton == Qt::RightButton) tool->rightButtonDown(pos, event);
  m_tabletPressed = false;
}

//-----------------------------------------------------------------------------

void SceneViewer::mouseReleaseEvent(QMouseEvent *event) {
  // if this is called just after tabletEvent, skip the execution
  if (m_tabletEvent) {
    m_tabletEvent = false;
    return;
  }

  TMouseEvent mouseEvent;
  initToonzEvent(mouseEvent, event, height(), 1.0, getDevPixRatio());
  onRelease(mouseEvent);
}
//-----------------------------------------------------------------------------

void SceneViewer::onRelease(const TMouseEvent &event) {
  // evita i release ripetuti
  if (!m_buttonClicked) return;
  m_buttonClicked = false;

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool || !tool->isEnabled()) {
    if (!m_toolDisableReason.isEmpty() && m_mouseButton == Qt::LeftButton &&
        !m_editPreviewSubCamera)
      DVGui::warning(m_toolDisableReason);
  }

  if (m_freezedStatus != NO_FREEZED) return;

  if (m_mouseButton != event.button()) return;

  // reject if tablet was active and the up button is not actually the pen.
  if (m_tabletActive && !m_tabletReleased) return;
  if (m_current3DDevice != NONE) {
    m_mouseButton = Qt::NoButton;
    m_tabletEvent = false;
    m_pressure    = 0;
    if (m_current3DDevice == SIDE_LEFT_3D)
      set3DLeftSideView();
    else if (m_current3DDevice == SIDE_RIGHT_3D)
      set3DRightSideView();
    else if (m_current3DDevice == TOP_3D)
      set3DTopView();
    return;
  }

  if (m_mouseButton == Qt::LeftButton && m_editPreviewSubCamera) {
    if (!PreviewSubCameraManager::instance()->mouseReleaseEvent(this))
      goto quit;
  }

  if (m_compareSettings.m_dragCompareX || m_compareSettings.m_dragCompareY) {
    m_compareSettings.m_dragCompareX = m_compareSettings.m_dragCompareY = false;
    goto quit;
  }

  m_pos = QPoint(0, 0);
  if (!tool || !tool->isEnabled()) goto quit;

  tool->setViewer(this);

  {
    TPointD pos = tool->getMatrix().inv() *
                  winToWorld(event.mousePos() * getDevPixRatio());

    TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
    if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
      pos.x /= m_dpiScale.x;
      pos.y /= m_dpiScale.y;
    }

    if (m_mouseButton == Qt::LeftButton || m_tabletReleased) {
      if (!m_toolSwitched) tool->leftButtonUp(pos, event);
      TApp::instance()->getCurrentTool()->setToolBusy(false);
    }
  }

quit:
  m_mouseButton    = Qt::NoButton;
  m_tabletPressed  = false;
  m_tabletActive   = false;
  m_tabletReleased = false;
  m_tabletMove     = false;
  m_pressure       = 0;
  // Leave m_tabletEvent as-is in order to check whether the onRelease is called
  // from tabletEvent or not in mouseReleaseEvent.
}

//-----------------------------------------------------------------------------
// on OSX, there is a critical bug that SceneViewer::mousePressEvent is called
// when leaving the stylus and it causes unwanted stroke drawn while
// hover-moving of the pen.
// When QEvent::TabletLeaveProximity is detected, call this function
// in order to force initializing such irregular mouse press.
// NOTE: For now QEvent::TabletLeaveProximity is NOT detected on Windows. See
// QTBUG-53628.
void SceneViewer::resetTabletStatus() {
  if (!m_buttonClicked) return;
  m_mouseButton    = Qt::NoButton;
  m_tabletEvent    = false;
  m_tabletPressed  = false;
  m_tabletActive   = false;
  m_tabletReleased = false;
  m_tabletMove     = false;
  m_pressure       = 0;
  m_buttonClicked  = false;
  if (TApp::instance()->getCurrentTool()->isToolBusy())
    TApp::instance()->getCurrentTool()->setToolBusy(false);
}

//-----------------------------------------------------------------------------

void SceneViewer::wheelEvent(QWheelEvent *event) {
  if (m_freezedStatus != NO_FREEZED) return;

  int delta = 0;
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    if (event->modifiers() & Qt::AltModifier)
      delta = event->angleDelta().x();
    else
      delta = event->angleDelta().y();
    break;
  }

  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = event->pixelDelta().y();
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      delta           = numSteps.y();
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
    {
      std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                   "Qt::MouseEventSynthesizedByApplication"
                << std::endl;
      break;
    }

  }  // end switch

  if (abs(delta) > 0) {
    // scrub with mouse wheel
    if ((event->modifiers() & Qt::AltModifier) &&
        (event->modifiers() & Qt::ShiftModifier) &&
        (event->modifiers() & Qt::ControlModifier)) {
      if (delta < 0) {
        CommandManager::instance()->execute("MI_NextStep");
      } else if (delta > 0) {
        CommandManager::instance()->execute("MI_PrevStep");
      }
    } else if ((event->modifiers() & Qt::ControlModifier) &&
               (event->modifiers() & Qt::ShiftModifier)) {
      if (delta < 0) {
        CommandManager::instance()->execute("MI_NextFrame");
      } else if (delta > 0) {
        CommandManager::instance()->execute("MI_PrevFrame");
      }
    } else if ((event->modifiers() & Qt::ShiftModifier) &&
               (event->modifiers() & Qt::AltModifier)) {
      if (delta < 0) {
        CommandManager::instance()->execute("MI_NextDrawing");
      } else if (delta > 0) {
        CommandManager::instance()->execute("MI_PrevDrawing");
      }
    } else {
      zoomQt(event->pos() * getDevPixRatio(), exp(0.001 * delta));
    }
  }
  event->accept();
}

//-----------------------------------------------------------------------------

bool SceneViewer::event(QEvent *e) {
  if (e->type() == QEvent::ShortcutOverride || e->type() == QEvent::KeyPress) {
    if (!((QKeyEvent *)e)->isAutoRepeat()) {
      TApp::instance()->getCurrentTool()->storeTool();
    }
  }
  if (e->type() == QEvent::ShortcutOverride) {
    TTool *tool = TApp::instance()->getCurrentTool()->getTool();
    if (tool && tool->isEnabled() && tool->getName() == T_Type &&
        tool->isActive())
      e->accept();
    return true;
  }
  if (e->type() == QEvent::KeyRelease) {
    if (!((QKeyEvent *)e)->isAutoRepeat()) {
      QWidget *focusWidget = QApplication::focusWidget();
      if (focusWidget == 0 ||
          QString(focusWidget->metaObject()->className()) == "SceneViewer")
        TApp::instance()->getCurrentTool()->restoreTool();
    }
  }

  // discard too frequent move events
  static QTime clock;
  if (e->type() == QEvent::MouseButtonPress)
    clock.start();
  else if (e->type() == QEvent::MouseMove) {
    if (clock.elapsed() < 10) {
      e->accept();
      return true;
    }
    clock.start();
  }

  /*
  switch(e->type())
  {
  case QEvent::Enter:
  qDebug() << "************************** Enter";
  break;
  case QEvent::Leave:
  qDebug() << "************************** Leave";
  break;

  case QEvent::TabletPress:
  qDebug() << "************************** TabletPress"  << m_pressure;
  break;
  case QEvent::TabletMove:
  qDebug() << "************************** TabletMove";
  break;
  case QEvent::TabletRelease:
  qDebug() << "************************** TabletRelease";
  break;


  case QEvent::MouseButtonPress:
  qDebug() << "**************************MouseButtonPress"  << m_pressure << " "
  << m_tabletEvent;
  break;
  case QEvent::MouseMove:
  qDebug() << "**************************MouseMove" <<  m_pressure;
  break;
  case QEvent::MouseButtonRelease:
  qDebug() << "**************************MouseButtonRelease";
  break;

  case QEvent::MouseButtonDblClick:
  qDebug() << "============================== MouseButtonDblClick";
  break;
  }
  */

  return QGLWidget::event(e);
}

//-----------------------------------------------------------------------------

class ViewerZoomer final : public ImageUtils::ShortcutZoomer {
public:
  ViewerZoomer(SceneViewer *parent) : ShortcutZoomer(parent) {}

  bool zoom(bool zoomin, bool resetZoom) override {
    SceneViewer *sceneViewer = static_cast<SceneViewer *>(getWidget());

    resetZoom ? sceneViewer->resetSceneViewer()
              : sceneViewer->zoomQt(zoomin, resetZoom);

    return true;
  }

  bool fit() override {
    static_cast<SceneViewer *>(getWidget())->fitToCamera();
    return true;
  }

  bool setActualPixelSize() override {
    static_cast<SceneViewer *>(getWidget())->setActualPixelSize();
    return true;
  }

  bool toggleFullScreen(bool quit) override {
    if (ImageUtils::FullScreenWidget *fsWidget =
            dynamic_cast<ImageUtils::FullScreenWidget *>(
                getWidget()->parentWidget()))
      return fsWidget->toggleFullScreen(quit);

    return false;
  }
};

//-----------------------------------------------------------------------------

bool changeFrameSkippingHolds(QKeyEvent *e) {
  if ((e->modifiers() & Qt::ShiftModifier) == 0 ||
      e->key() != Qt::Key_Down && e->key() != Qt::Key_Up)
    return false;
  TApp *app        = TApp::instance();
  TFrameHandle *fh = app->getCurrentFrame();
  if (!fh->isEditingScene()) return false;
  int row       = fh->getFrame();
  int col       = app->getCurrentColumn()->getColumnIndex();
  TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
  TXshCell cell = xsh->getCell(row, col);
  if (e->key() == Qt::Key_Down) {
    if (cell.isEmpty()) {
      int r0, r1;
      if (xsh->getCellRange(col, r0, r1)) {
        while (row <= r1 && xsh->getCell(row, col).isEmpty()) row++;
        if (xsh->getCell(row, col).isEmpty()) return false;
      } else
        return false;
    } else {
      while (xsh->getCell(row, col) == cell) row++;
    }
  } else {
    // Key_Up
    while (row >= 0 && xsh->getCell(row, col) == cell) row--;
    if (row < 0) return false;
    cell = xsh->getCell(row, col);
    while (row > 0 && xsh->getCell(row - 1, col) == cell) row--;
  }
  fh->setFrame(row);
  return true;
}

//-----------------------------------------------------------------------------

void SceneViewer::keyPressEvent(QKeyEvent *event) {
  if (m_freezedStatus != NO_FREEZED) return;

  int key = event->key();

  if (changeFrameSkippingHolds(event)) {
    return;
  }
  if ((event->modifiers() & Qt::ShiftModifier) &&
      (key == Qt::Key_Down || key == Qt::Key_Up)) {
  }

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool) return;

  bool isTextToolActive = tool->getName() == T_Type && tool->isActive();

  if (!isTextToolActive && ViewerZoomer(this).exec(event)) return;

  if (!isTextToolActive && SceneViewerShortcutReceiver(this).exec(event))
    return;

  if (!tool->isEnabled()) return;

  tool->setViewer(this);

  // If this object is child of Viewer or ComboViewer
  // (m_isStyleShortcutSelective = true),
  // then consider about shortcut for the current style selection.
  if (m_isStyleShortcutSwitchable &&
      Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
      (!isTextToolActive) && (event->modifiers() == Qt::NoModifier ||
                              event->modifiers() == Qt::KeypadModifier) &&
      ((Qt::Key_0 <= key && key <= Qt::Key_9) || key == Qt::Key_Tab ||
       key == Qt::Key_Backtab)) {
    event->ignore();
    return;
  }

  if (key == Qt::Key_Shift)
    modifiers |= Qt::SHIFT;
  else if (key == Qt::Key_Control)
    modifiers |= Qt::CTRL;
  else if (key == Qt::Key_Alt || key == Qt::Key_AltGr)
    modifiers |= Qt::ALT;

  if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
      key == Qt::Key_AltGr) {
    // quando l'utente preme shift/ctrl ecc. alcuni tool (es. pinch) devono
    // cambiare subito la forma del cursore, senza aspettare il prossimo move
    TMouseEvent toonzEvent;
    initToonzEvent(toonzEvent, event);
    toonzEvent.m_pos =
        TPoint(m_lastMousePos.x(), height() - 1 - m_lastMousePos.y());

    TPointD pos = tool->getMatrix().inv() * winToWorld(m_lastMousePos);

    TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
    if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
      pos.x /= m_dpiScale.x;
      pos.y /= m_dpiScale.y;
    }

    tool->mouseMove(pos, toonzEvent);
    setToolCursor(this, tool->getCursorId());
  }

  bool shiftButton = QApplication::keyboardModifiers() == Qt::ShiftModifier;

  TUINT32 flags = 0;
  TPoint pos;
  if (key == Qt::Key_Shift)
    flags = TwConsts::TK_ShiftPressed;
  else if (key == Qt::Key_Control)
    flags = TwConsts::TK_CtrlPressed;
  else if (key == Qt::Key_Alt)
    flags = TwConsts::TK_AltPressed;
  else if (key == Qt::Key_CapsLock)
    flags = TwConsts::TK_CapsLock;
  else if (key == Qt::Key_Backspace)
    key = TwConsts::TK_Backspace;
  else if (key == Qt::Key_Return || key == Qt::Key_Enter)
    key = TwConsts::TK_Return;
  else if (key == Qt::Key_Left && !shiftButton)
    key = TwConsts::TK_LeftArrow;
  else if (key == Qt::Key_Right && !shiftButton)
    key = TwConsts::TK_RightArrow;
  else if (key == Qt::Key_Up && !shiftButton)
    key = TwConsts::TK_UpArrow;
  else if (key == Qt::Key_Down && !shiftButton)
    key = TwConsts::TK_DownArrow;
  else if (key == Qt::Key_Left && shiftButton)
    key = TwConsts::TK_ShiftLeftArrow;
  else if (key == Qt::Key_Right && shiftButton)
    key = TwConsts::TK_ShiftRightArrow;
  else if (key == Qt::Key_Up && shiftButton)
    key = TwConsts::TK_ShiftUpArrow;
  else if (key == Qt::Key_Down && shiftButton)
    key = TwConsts::TK_ShiftDownArrow;
  else if (key == Qt::Key_Home)
    key = TwConsts::TK_Home;
  else if (key == Qt::Key_End)
    key = TwConsts::TK_End;
  else if (key == Qt::Key_PageUp)
    key = TwConsts::TK_PageUp;
  else if (key == Qt::Key_PageDown)
    key = TwConsts::TK_PageDown;
  else if (key == Qt::Key_Insert)
    key = TwConsts::TK_Insert;
  else if (key == Qt::Key_Delete)
    key = TwConsts::TK_Delete;
  else if (key == Qt::Key_Escape)
    key = TwConsts::TK_Esc;
  else if (key == Qt::Key_F1)
    key = TwConsts::TK_F1;
  else if (key == Qt::Key_F2)
    key = TwConsts::TK_F2;
  else if (key == Qt::Key_F3)
    key = TwConsts::TK_F3;
  else if (key == Qt::Key_F4)
    key = TwConsts::TK_F4;
  else if (key == Qt::Key_F5)
    key = TwConsts::TK_F5;
  else if (key == Qt::Key_F6)
    key = TwConsts::TK_F6;
  else if (key == Qt::Key_F7)
    key = TwConsts::TK_F7;
  else if (key == Qt::Key_F8)
    key = TwConsts::TK_F8;
  else if (key == Qt::Key_F9)
    key = TwConsts::TK_F9;
  else if (key == Qt::Key_F10)
    key = TwConsts::TK_F10;
  else if (key == Qt::Key_F11)
    key = TwConsts::TK_F11;
  else if (key == Qt::Key_F12)
    key = TwConsts::TK_F12;
  else if (key == Qt::Key_Menu || key == Qt::Key_Meta)
    return;

  bool ret = false;
  if (tool)  // && m_toolEnabled)
  {
    QString text = event->text();
    if ((event->modifiers() & Qt::ShiftModifier)) text.toUpper();
    std::wstring unicodeChar = text.toStdWString();
    ret = tool->keyDown(key, unicodeChar, flags, pos);  // per il textTool
    if (ret) {
      update();
      return;
    }
    ret = tool->keyDown(key, flags, pos);  // per gli altri tool
  }
  if (!ret) {
    TFrameHandle *fh = TApp::instance()->getCurrentFrame();

    if (key == TwConsts::TK_UpArrow || key == TwConsts::TK_LeftArrow)
      fh->prevFrame();
    else if (key == TwConsts::TK_DownArrow || key == TwConsts::TK_RightArrow)
      fh->nextFrame();
    else if (key == TwConsts::TK_Home)
      fh->firstFrame();
    else if (key == TwConsts::TK_End)
      fh->lastFrame();
  }
  update();
  // TODO: devo accettare l'evento?
}

//-----------------------------------------------------------------------------

void SceneViewer::keyReleaseEvent(QKeyEvent *event) {
  if (m_freezedStatus != NO_FREEZED) return;

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool || !tool->isEnabled()) return;
  tool->setViewer(this);

  int key = event->key();

  if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
      key == Qt::Key_AltGr) {
    if (key == Qt::Key_Shift)
      modifiers &= ~Qt::ShiftModifier;
    else if (key == Qt::Key_Control)
      modifiers &= ~Qt::ControlModifier;
    else if (key == Qt::Key_Alt || key == Qt::Key_AltGr)
      modifiers &= ~Qt::AltModifier;

    // quando l'utente preme shift/ctrl ecc. alcuni tool (es. pinch) devono
    // cambiare subito la forma del cursore, senza aspettare il prossimo move
    TMouseEvent toonzEvent;
    initToonzEvent(toonzEvent, event);
    toonzEvent.m_pos =
        TPoint(m_lastMousePos.x(), height() - 1 - m_lastMousePos.y());

    TPointD pos = tool->getMatrix().inv() * winToWorld(m_lastMousePos);

    TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
    if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
      pos.x /= m_dpiScale.x;
      pos.y /= m_dpiScale.y;
    }

    tool->mouseMove(pos, toonzEvent);
    setToolCursor(this, tool->getCursorId());
  }

  if (tool->getName() == T_Type)
    event->accept();
  else
    event->ignore();
}

//-----------------------------------------------------------------------------

void SceneViewer::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_freezedStatus != NO_FREEZED) return;

  int frame = TApp::instance()->getCurrentFrame()->getFrame();
  if (frame == -1) return;

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool || !tool->isEnabled()) return;
  TMouseEvent toonzEvent;
  initToonzEvent(toonzEvent, event, height(), 1.0, getDevPixRatio());
  TPointD pos =
      tool->getMatrix().inv() * winToWorld(event->pos() * getDevPixRatio());
  TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
  if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
    pos.x /= m_dpiScale.x;
    pos.y /= m_dpiScale.y;
  }

  if (event->button() == Qt::LeftButton)
    tool->leftButtonDoubleClick(pos, toonzEvent);
}

//-----------------------------------------------------------------------------

using namespace ImageUtils;

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void SceneViewer::contextMenuEvent(QContextMenuEvent *e) {
#ifndef _WIN32
  /* On windows the widget receive the release event before the menu
     is shown, on linux and osx the release event is lost, never
     received by the widget */
  QMouseEvent fakeRelease(QEvent::MouseButtonRelease, e->pos(), Qt::RightButton,
                          Qt::NoButton, Qt::NoModifier);

  QApplication::instance()->sendEvent(this, &fakeRelease);
#endif

  if (m_freezedStatus != NO_FREEZED) return;
  if (m_isLocator) return;

  int devPixRatio = getDevPixRatio();
  TPoint winPos(e->pos().x() * devPixRatio,
                height() - e->pos().y() * devPixRatio);
  std::vector<int> columnIndices;
  // enable to select all the columns regardless of the click position
  for (int i = 0;
       i < TApp::instance()->getCurrentXsheet()->getXsheet()->getColumnCount();
       i++)
    columnIndices.push_back(i);

  SceneViewerContextMenu *menu = new SceneViewerContextMenu(this);

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  TPointD pos = ((tool) ? tool->getMatrix().inv() : TAffine()) *
                winToWorld(e->pos() * devPixRatio);
  menu->addEnterGroupCommands(pos);

  menu->addLevelCommands(columnIndices);

  ComboViewerPanel *cvp = qobject_cast<ComboViewerPanel *>(
      parentWidget()->parentWidget()->parentWidget());
  if (cvp) {
    menu->addSeparator();
    cvp->addShowHideContextMenu(menu);
  }

  menu->exec(e->globalPos());
  delete menu;
}

//-----------------------------------------------------------------------------

void SceneViewer::inputMethodEvent(QInputMethodEvent *e) {
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) {
    tool->onInputText(e->preeditString().toStdWString(),
                      e->commitString().toStdWString(), e->replacementStart(),
                      e->replacementLength());
  }
}
//-----------------------------------------------------------------------------

void SceneViewer::dragEnterEvent(QDragEnterEvent *event) {
  if (m_freezedStatus != NO_FREEZED) return;

  const QMimeData *mimeData = event->mimeData();

  if (acceptResourceOrFolderDrop(mimeData->urls()))
    event->acceptProposedAction();
  else
    event->ignore();
}

//-----------------------------------------------------------------------------

void SceneViewer::dropEvent(QDropEvent *e) {
  if (m_freezedStatus != NO_FREEZED) return;

  const QMimeData *mimeData = e->mimeData();
  if (mimeData->hasUrls()) {
    IoCmd::LoadResourceArguments args;

    foreach (const QUrl &url, mimeData->urls()) {
      TFilePath fp(url.toLocalFile().toStdWString());
      args.resourceDatas.push_back(fp);
    }

    IoCmd::loadResources(args);
  }
  e->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void SceneViewer::onToolSwitched() {
  m_forceGlFlush = true;
  m_toolSwitched = true;
  invalidateToolStatus();

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool) tool->updateMatrix();

  onLevelChanged();
  update();
}
