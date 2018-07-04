

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
#include "toonzqt/viewcommandids.h"

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
#include <QGestureEvent>

// definito - per ora - in tapp.cpp
extern QString updateToolEnableStatus(TTool *tool);

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void initToonzEvent(TMouseEvent &toonzEvent, QMouseEvent *event,
                    int widgetHeight, double pressure, int devPixRatio) {
  toonzEvent.m_pos = TPointD(event->pos().x() * devPixRatio,
                             widgetHeight - 1 - event->pos().y() * devPixRatio);
  toonzEvent.m_mousePos = event->pos();
  toonzEvent.m_pressure = 1.0;

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
  toonzEvent.m_pos = TPointD(
      event->posF().x() * (float)devPixRatio,
      (float)widgetHeight - 1.0f - event->posF().y() * (float)devPixRatio);
  toonzEvent.m_mousePos = event->posF();
  toonzEvent.m_pressure = pressure;

  toonzEvent.setModifiers(event->modifiers() & Qt::ShiftModifier,
                          event->modifiers() & Qt::AltModifier,
                          event->modifiers() & Qt::ControlModifier);
  toonzEvent.m_buttons  = event->buttons();
  toonzEvent.m_button   = event->button();
  toonzEvent.m_isTablet = true;
  // this delays autosave during stylus button press until after the next
  // brush stroke - this minimizes problems from interruptions to tablet input
  TApp::instance()->getCurrentTool()->setToolBusy(true);
}

//-----------------------------------------------------------------------------

void initToonzEvent(TMouseEvent &toonzEvent, QKeyEvent *event) {
  toonzEvent.m_pos      = TPointD();
  toonzEvent.m_mousePos = QPointF();
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
// when using tablet on OSX, use tabletEvent instead of all the mouse-related
// events
// on Windows or on other OS, handle only left-button case

void SceneViewer::tabletEvent(QTabletEvent *e) {
  if (m_freezedStatus != NO_FREEZED) return;

  m_tabletEvent = true;
  m_pressure    = e->pressure();
  m_tabletMove  = false;
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
#ifdef MACOSX
    // In OSX tablet action may cause only tabletEvent, not followed by
    // mousePressEvent.
    // So call onPress here in order to enable processing.
    if (e->button() == Qt::LeftButton) m_tabletState = Touched;
    TMouseEvent mouseEvent;
    initToonzEvent(mouseEvent, e, height(), m_pressure, getDevPixRatio());
    onPress(mouseEvent);

    // create context menu on right click here
    if (e->button() == Qt::RightButton) {
      m_mouseButton = Qt::NoButton;
      onContextMenu(e->pos(), e->globalPos());
    }
#else
    // for Windows, use tabletEvent only for the left Button
    // (regular pen touch), because
    // the current Qt seems to fail to catch the Wacom's button binding properly
    // with QTabletEvent->button() when pressing middle or right button.
    // So, in such case set m_tabletEvent = FALSE and let the mousePressEvent to
    // work.
    if (e->button() == Qt::LeftButton) {
      TMouseEvent mouseEvent;
      initToonzEvent(mouseEvent, e, height(), m_pressure, getDevPixRatio());
      m_tabletState = Touched;
      onPress(mouseEvent);
    } else
      m_tabletEvent = false;
#endif

  } break;
  case QEvent::TabletRelease: {
#ifdef MACOSX
    if (m_tabletState == StartStroke || m_tabletState == OnStroke)
      m_tabletState = Released;

    TMouseEvent mouseEvent;
    initToonzEvent(mouseEvent, e, height(), m_pressure, getDevPixRatio());
    onRelease(mouseEvent);

    if (TApp::instance()->getCurrentTool()->isToolBusy())
      TApp::instance()->getCurrentTool()->setToolBusy(false);
#else
    if (m_tabletState == StartStroke || m_tabletState == OnStroke) {
      m_tabletState = Released;
      TMouseEvent mouseEvent;
      initToonzEvent(mouseEvent, e, height(), m_pressure, getDevPixRatio());
      onRelease(mouseEvent);
    } else
      m_tabletEvent = false;
#endif
  } break;
  case QEvent::TabletMove: {
#ifdef MACOSX
    // for now OSX seems to fail to call enter/leaveEvent properly while
    // the tablet is floating
    bool isHoveringInsideViewer =
        !rect().marginsRemoved(QMargins(5, 5, 5, 5)).contains(e->pos());
    // call the fake enter event
    if (isHoveringInsideViewer) onEnter();
#else
    // for Windowsm, use tabletEvent only for the left Button
    if (m_tabletState != StartStroke && m_tabletState != OnStroke) {
      m_tabletEvent = false;
      break;
    }
#endif
    QPointF curPos = e->posF() * getDevPixRatio();
    // It seems that the tabletEvent is called more often than mouseMoveEvent.
    // So I fire the interval timer in order to limit the following process
    // to be called in 50fps in maximum.
    if (curPos != m_lastMousePos && !m_isBusyOnTabletMove) {
      m_isBusyOnTabletMove = true;
      TMouseEvent mouseEvent;
      initToonzEvent(mouseEvent, e, height(), m_pressure, getDevPixRatio());
      QTimer::singleShot(20, this, SLOT(releaseBusyOnTabletMove()));
      // cancel stroke to prevent drawing while floating
      // 23/1/2018 There is a case that the pressure becomes zero at the start
      // and the end of stroke. For such case, stroke should not be cancelled.
      // So, added a process to finish stroke here instead of cancelling.
      // This will be called only if the state is OnStroke so at the start
      // of the stroke this condition will be passed.
      if (m_tabletState == OnStroke && m_pressure == 0.0) {
        m_tabletState       = Released;
        mouseEvent.m_button = Qt::LeftButton;
        onRelease(mouseEvent);
      } else {
        m_tabletMove = true;
        onMove(mouseEvent);  // m_tabletState is set to OnStrole here
      }
    }
#ifdef MACOSX
    // call the fake leave event if the pen is hovering the viewer edge
    if (!isHoveringInsideViewer) onLeave();
#endif
  } break;
  default:
    break;
  }
  e->accept();
}

//-----------------------------------------------------------------------------

void SceneViewer::leaveEvent(QEvent *) { onLeave(); }

//-----------------------------------------------------------------------------

void SceneViewer::onLeave() {
  if (!m_isMouseEntered) return;

  m_isMouseEntered = false;

  if (m_freezedStatus != NO_FREEZED) return;
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) tool->onLeave();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::enterEvent(QEvent *) { onEnter(); }

//-----------------------------------------------------------------------------

void SceneViewer::onEnter() {
  if (m_isMouseEntered) return;

  m_isMouseEntered = true;

  TApp *app = TApp::instance();
  app->setActiveViewer(this);
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
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::mouseMoveEvent(QMouseEvent *event) {
  // if this is called just after tabletEvent, skip the execution
  if (m_tabletEvent) return;
  // and touchscreens but not touchpads...
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen) {
    return;
  }
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
  QPointF curPos  = event.mousePos() * devPixRatio;
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
    }
  } else if (m_mouseButton == Qt::NoButton || m_mouseButton == Qt::LeftButton) {
    if (is3DView()) {
      if (m_current3DDevice != NONE && m_mouseButton == Qt::LeftButton)
        return;
      else if (m_mouseButton == Qt::NoButton) {
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
    }

    // if the middle mouse button is pressed while dragging, then do panning
    if (event.buttons() & Qt::MidButton) {
      // panning
      QPointF p = curPos - m_pos;
      if (m_pos == QPointF() && p.manhattanLength() > 300) return;
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
    if (m_tabletEvent &&
        (m_tabletState == OnStroke || m_tabletState == StartStroke) &&
        m_tabletMove) {
      tool->leftButtonDrag(pos, event);
      m_tabletState = OnStroke;
    }

    else if (m_mouseButton == Qt::LeftButton) {
      // sometimes the mousePressedEvent is postponed to a wrong  mouse move
      // event!
      if (m_buttonClicked && !m_toolSwitched) tool->leftButtonDrag(pos, event);
    } else if (m_pressure == 0.0) {
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
  // source can be useful for detecting touch vs mouse,
  // but has reports of being unreliable on mac
  // so m_gestureActive is the marker for rejecting touch events
  int source = event->source();
  // if this is called just after tabletEvent, skip the execution
  if (m_tabletEvent) return;
  // and touchscreens but not touchpads...
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen) {
    return;
  }
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
  if (m_mouseButton != Qt::NoButton) {
    // if some button is pressed while another button being active,
    // finish the action for the previous button first.
    TMouseEvent preEvent = event;
    preEvent.m_button    = m_mouseButton;
    onRelease(preEvent);
  }

  // evita i press ripetuti
  if (m_buttonClicked) return;
  m_buttonClicked = true;
  m_toolSwitched  = false;
  if (m_freezedStatus != NO_FREEZED) return;

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
  if (m_tabletEvent && m_tabletState == Touched) {
    TApp::instance()->getCurrentTool()->setToolBusy(true);
    m_tabletState = StartStroke;
    tool->leftButtonDown(pos, event);
  } else if (m_mouseButton == Qt::LeftButton) {
    TApp::instance()->getCurrentTool()->setToolBusy(true);
    tool->leftButtonDown(pos, event);
  }
  if (m_mouseButton == Qt::RightButton) tool->rightButtonDown(pos, event);
}

//-----------------------------------------------------------------------------

void SceneViewer::mouseReleaseEvent(QMouseEvent *event) {
  // if this is called just after tabletEvent, skip the execution
  if (m_tabletEvent) {
    // mouseRelease should not clear flag if we are starting or in middle of
    // stroke
    // initiated by tableEvent. All other cases, it's ok to clear flag
    if (m_tabletState == Released || m_tabletState == None)
      m_tabletEvent = false;
    return;
  }
  // for touchscreens but not touchpads...
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen) {
    m_gestureActive = false;
    m_rotating      = false;
    m_zooming       = false;
    m_panning       = false;
    m_scaleFactor   = 0.0;
    m_rotationDelta = 0.0;
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

  m_pos = QPointF();
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

    if (m_mouseButton == Qt::LeftButton || m_tabletState == Released) {
      if (!m_toolSwitched) tool->leftButtonUp(pos, event);
      TApp::instance()->getCurrentTool()->setToolBusy(false);
    }
  }

quit:
  m_mouseButton = Qt::NoButton;
  // If m_tabletState is "Touched", we've been called by tabletPress event.
  // Don't clear it out table state so the tablePress event will process
  // correctly.
  if (m_tabletState != Touched) m_tabletState = None;
  m_tabletMove                                = false;
  m_pressure                                  = 0;
  // Leave m_tabletEvent as-is in order to check whether the onRelease is called
  // from tabletEvent or not in mouseReleaseEvent.
  if (m_tabletState == Released)  // only clear if tabletRelease event
    m_tabletEvent = false;
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
  m_mouseButton   = Qt::NoButton;
  m_tabletEvent   = false;
  m_tabletState   = None;
  m_tabletMove    = false;
  m_pressure      = 0;
  m_buttonClicked = false;
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
    }
    // Mouse wheel zoom interfered with touchpad panning (touch enabled)
    // Now if touch is enabled, touchpads ignore the mouse wheel zoom
    else if ((m_gestureActive == true &&
              m_touchDevice == QTouchDevice::TouchScreen) ||
             m_gestureActive == false) {
      zoomQt(event->pos() * getDevPixRatio(), exp(0.001 * delta));
    }
  }
  event->accept();
}

//-----------------------------------------------------------------------------

void SceneViewer::gestureEvent(QGestureEvent *e) {
  m_gestureActive = false;
  if (QGesture *swipe = e->gesture(Qt::SwipeGesture)) {
    m_gestureActive = true;
  } else if (QGesture *pan = e->gesture(Qt::PanGesture)) {
    m_gestureActive = true;
  }
  if (QGesture *pinch = e->gesture(Qt::PinchGesture)) {
    QPinchGesture *gesture = static_cast<QPinchGesture *>(pinch);
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    QPoint firstCenter                     = gesture->centerPoint().toPoint();

    if (gesture->state() == Qt::GestureStarted) {
      m_gestureActive = true;
    } else if (gesture->state() == Qt::GestureFinished) {
      m_gestureActive = false;
      m_rotating      = false;
      m_zooming       = false;
      m_scaleFactor   = 0.0;
      m_rotationDelta = 0.0;
    } else {
      if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        double scaleFactor = gesture->scaleFactor();
        // the scale factor makes for too sensitive scaling
        // divide the change in half
        if (scaleFactor > 1) {
          double decimalValue = scaleFactor - 1;
          decimalValue /= 1.5;
          scaleFactor = 1 + decimalValue;
        } else if (scaleFactor < 1) {
          double decimalValue = 1 - scaleFactor;
          decimalValue /= 1.5;
          scaleFactor = 1 - decimalValue;
        }
        if (!m_rotating && !m_zooming) {
          double delta = scaleFactor - 1;
          m_scaleFactor += delta;
          if (m_scaleFactor > .2 || m_scaleFactor < -.2) {
            m_zooming = true;
          }
        }
        if (m_zooming) {
          zoomQt(firstCenter * getDevPixRatio(), scaleFactor);
        }
        m_gestureActive = true;
      }
      if (changeFlags & QPinchGesture::RotationAngleChanged) {
        qreal rotationDelta =
            gesture->rotationAngle() - gesture->lastRotationAngle();
        TAffine aff    = getViewMatrix().inv();
        TPointD center = aff * TPointD(0, 0);
        if (!m_rotating && !m_zooming) {
          m_rotationDelta += rotationDelta;
          double absDelta = abs(m_rotationDelta);
          if (absDelta >= 10) {
            m_rotating = true;
          }
        }
        if (m_rotating) {
          rotate(center, -rotationDelta);
        }
      }

      if (changeFlags & QPinchGesture::CenterPointChanged) {
        QPointF centerDelta = (gesture->centerPoint() * getDevPixRatio()) -
                              (gesture->lastCenterPoint() * getDevPixRatio());
        if (centerDelta.manhattanLength() > 1) {
          // panQt(centerDelta.toPoint());
        }
        m_gestureActive = true;
      }
    }
  }
  e->accept();
}

void SceneViewer::touchEvent(QTouchEvent *e, int type) {
  if (type == QEvent::TouchBegin) {
    m_touchActive   = true;
    m_firstPanPoint = e->touchPoints().at(0).pos();
    m_undoPoint     = m_firstPanPoint;
    // obtain device type
    m_touchDevice = e->device()->type();
  } else if (m_touchActive) {
    // touchpads must have 2 finger panning for tools and navigation to be
    // functional
    // on other devices, 1 finger panning is preferred
    if ((e->touchPoints().count() == 2 &&
         m_touchDevice == QTouchDevice::TouchPad) ||
        (e->touchPoints().count() == 1 &&
         m_touchDevice == QTouchDevice::TouchScreen)) {
      QTouchEvent::TouchPoint panPoint = e->touchPoints().at(0);
      if (!m_panning) {
        QPointF deltaPoint = panPoint.pos() - m_firstPanPoint;
        // minimize accidental and jerky zooming/rotating during 2 finger
        // panning
        if ((deltaPoint.manhattanLength() > 100) && !m_zooming && !m_rotating) {
          m_panning = true;
        }
      }
      if (m_panning) {
        QPointF centerDelta = (panPoint.pos() * getDevPixRatio()) -
                              (panPoint.lastPos() * getDevPixRatio());
        panQt(centerDelta.toPoint());
      }
    } else if (e->touchPoints().count() == 3) {
      QPointF newPoint = e->touchPoints().at(0).pos();
      if (m_undoPoint.x() - newPoint.x() > 100) {
        CommandManager::instance()->execute("MI_Undo");
        m_undoPoint = newPoint;
      }
      if (m_undoPoint.x() - newPoint.x() < -100) {
        CommandManager::instance()->execute("MI_Redo");
        m_undoPoint = newPoint;
      }
    }
  }
  if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
    m_touchActive = false;
    m_panning     = false;
  }
  e->accept();
}

//-----------------------------------------------------------------------------

bool SceneViewer::event(QEvent *e) {
  if (CommandManager::instance()
          ->getAction(MI_TouchGestureControl)
          ->isChecked()) {
    if (e->type() == QEvent::Gesture) {
      gestureEvent(static_cast<QGestureEvent *>(e));
      return true;
    }
    if (e->type() == QEvent::TouchBegin || e->type() == QEvent::TouchEnd ||
        e->type() == QEvent::TouchCancel || e->type() == QEvent::TouchUpdate) {
      touchEvent(static_cast<QTouchEvent *>(e), e->type());
      m_gestureActive = true;
      return true;
    }
  }
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
    // for other tools, check if the pressed keys should be catched instead of
    // triggering the shortcut command actions
    else if (tool && tool->isEventAcceptable(e)) {
      e->accept();
    }
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
    if (clock.isValid() && clock.elapsed() < 10) {
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

  return QOpenGLWidget::event(e);
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

  bool setFlipX() override {
    static_cast<SceneViewer *>(getWidget())->flipX();
    return true;
  }

  bool setFlipY() override {
    static_cast<SceneViewer *>(getWidget())->flipY();
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

  // resolving priority and tool-specific key events in this lambda
  auto ret = [&]() -> bool {
    TTool *tool = TApp::instance()->getCurrentTool()->getTool();
    if (!tool) return false;

    bool isTextToolActive = tool->getName() == T_Type && tool->isActive();

    if (!isTextToolActive) {
      if (ViewerZoomer(this).exec(event)) return true;
      if (SceneViewerShortcutReceiver(this).exec(event)) return true;
      // If this object is child of Viewer or ComboViewer
      // (m_isStyleShortcutSelective = true),
      // then consider about shortcut for the current style selection.
      if (m_isStyleShortcutSwitchable &&
          Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
          (event->modifiers() == Qt::NoModifier ||
           event->modifiers() == Qt::KeypadModifier) &&
          ((Qt::Key_0 <= key && key <= Qt::Key_9) || key == Qt::Key_Tab ||
           key == Qt::Key_Backtab)) {
        event->ignore();
        return true;
      }
    }

    if (!tool->isEnabled()) return false;

    tool->setViewer(this);

    if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
        key == Qt::Key_AltGr) {
      // quando l'utente preme shift/ctrl ecc. alcuni tool (es. pinch) devono
      // cambiare subito la forma del cursore, senza aspettare il prossimo move
      TMouseEvent toonzEvent;
      initToonzEvent(toonzEvent, event);
      toonzEvent.m_pos = TPointD(m_lastMousePos.x(),
                                 (double)(height() - 1) - m_lastMousePos.y());

      TPointD pos = tool->getMatrix().inv() * winToWorld(m_lastMousePos);

      TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
      if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
        pos.x /= m_dpiScale.x;
        pos.y /= m_dpiScale.y;
      }

      tool->mouseMove(pos, toonzEvent);
      setToolCursor(this, tool->getCursorId());
    }

    if (key == Qt::Key_Menu || key == Qt::Key_Meta) return false;

    return tool->keyDown(event);
  }();

  if (!ret) {
    if (changeFrameSkippingHolds(event)) return;

    TFrameHandle *fh = TApp::instance()->getCurrentFrame();

    if (key == Qt::Key_Up || key == Qt::Key_Left)
      fh->prevFrame();
    else if (key == Qt::Key_Down || key == Qt::Key_Right) {
      // If on a level frame pass the frame id after the last frame to allow
      // creating a new frame with the down arrow key
      TFrameId newId = 0;
      if (Preferences::instance()->getDownArrowLevelStripNewFrame() &&
          fh->getFrameType() == TFrameHandle::LevelFrame) {
        TXshSimpleLevel *level =
            TApp::instance()->getCurrentLevel()->getLevel()->getSimpleLevel();
        if (level) {
          std::vector<TFrameId> fids;
          level->getFids(fids);
          if (!fids.empty()) {
            int frameCount = (int)fids.size();
            newId          = level->index2fid(frameCount);
          }
        }
      }
      fh->nextFrame(newId);
    } else if (key == Qt::Key_Home)
      fh->firstFrame();
    else if (key == Qt::Key_End)
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
    // quando l'utente preme shift/ctrl ecc. alcuni tool (es. pinch) devono
    // cambiare subito la forma del cursore, senza aspettare il prossimo move
    TMouseEvent toonzEvent;
    initToonzEvent(toonzEvent, event);
    toonzEvent.m_pos = TPointD(m_lastMousePos.x(),
                               (double)(height() - 1) - m_lastMousePos.y());

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
  if (m_gestureActive) {
    fitToCamera();
    m_gestureActive = false;
    return;
  }
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
  if (m_tabletEvent) return;
#ifndef _WIN32
  /* On windows the widget receive the release event before the menu
     is shown, on linux and osx the release event is lost, never
     received by the widget */
  QMouseEvent fakeRelease(QEvent::MouseButtonRelease, e->pos(), Qt::RightButton,
                          Qt::NoButton, Qt::NoModifier);

  QApplication::instance()->sendEvent(this, &fakeRelease);
#endif

  onContextMenu(e->pos(), e->globalPos());
}

//-----------------------------------------------------------------------------

void SceneViewer::onContextMenu(const QPoint &pos, const QPoint &globalPos) {
  if (m_freezedStatus != NO_FREEZED) return;
  if (m_isLocator) return;
  static bool menuVisible = false;
  if (menuVisible) return;
  menuVisible     = true;
  int devPixRatio = getDevPixRatio();
  TPoint winPos(pos.x() * devPixRatio, height() - pos.y() * devPixRatio);
  std::vector<int> columnIndices;
  // enable to select all the columns regardless of the click position
  for (int i = 0;
       i < TApp::instance()->getCurrentXsheet()->getXsheet()->getColumnCount();
       i++)
    columnIndices.push_back(i);

  SceneViewerContextMenu *menu = new SceneViewerContextMenu(this);

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  TPointD p   = ((tool) ? tool->getMatrix().inv() : TAffine()) *
              winToWorld(pos * devPixRatio);
  menu->addEnterGroupCommands(p);

  menu->addLevelCommands(columnIndices);

  ComboViewerPanel *cvp = qobject_cast<ComboViewerPanel *>(
      parentWidget()->parentWidget()->parentWidget());
  if (cvp) {
    menu->addSeparator();
    cvp->addShowHideContextMenu(menu);
  }

  menu->exec(globalPos);
  delete menu;
  menuVisible = false;
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
