
#include "toonzqt/schematicviewer.h"

// TnzQt includes
#include "toonzqt/fxtypes.h"
#include "toonzqt/schematicnode.h"
#include "toonzqt/fxschematicnode.h"
#include "toonzqt/schematicgroupeditor.h"
#include "toonzqt/stageschematicscene.h"
#include "toonzqt/fxschematicscene.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/dvscrollwidget.h"
#include "toonzqt/fxselection.h"
#include "stageobjectselection.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/preferences.h"
#include "toonz/fxdag.h"
#include "toonz/tapplication.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshleveltypes.h"

#include "../toonz/menubarcommandids.h"

#include "tools/cursormanager.h"
#include "tools/cursors.h"

// Qt includes
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QGraphicsItem>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QIcon>
#include <QAction>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QGestureEvent>

#include <QDebug>

// STD includes
#include "assert.h"
#include "math.h"

namespace {

class SchematicZoomer final : public ImageUtils::ShortcutZoomer {
public:
  SchematicZoomer(QWidget *parent) : ShortcutZoomer(parent) {}

  bool zoom(bool zoomin, bool resetView) override {
    static_cast<SchematicSceneViewer *>(getWidget())->zoomQt(zoomin, resetView);
    return true;
  }
  bool resetZoom() override {
    static_cast<SchematicSceneViewer *>(getWidget())->normalizeScene();
    return true;
  }
  bool fit() override {
    static_cast<SchematicSceneViewer *>(getWidget())->fitScene();
    return true;
  }
};

const int snapDistance = 15;
}  // namespace

int SchematicScene::snapHSpacing  = 50;
int SchematicScene::snapVInterval = 75;

//==================================================================
//
// SchematicScene
//
//==================================================================

SchematicScene::SchematicScene(QWidget *parent) : QGraphicsScene(parent) {
  setSceneRect(0, 0, 50000, 50000);
  setItemIndexMethod(NoIndex);
}

//------------------------------------------------------------------

SchematicScene::~SchematicScene() { clearAllItems(); }

//------------------------------------------------------------------

void SchematicScene::showEvent(QShowEvent *se) {
  TSelectionHandle *selHandle = TSelectionHandle::getCurrent();
  connect(selHandle, SIGNAL(selectionSwitched(TSelection *, TSelection *)),
          this, SLOT(onSelectionSwitched(TSelection *, TSelection *)));
  clearSelection();
}

//------------------------------------------------------------------

void SchematicScene::hideEvent(QHideEvent *se) {
  TSelectionHandle *selHandle = TSelectionHandle::getCurrent();
  disconnect(selHandle, SIGNAL(selectionSwitched(TSelection *, TSelection *)),
             this, SLOT(onSelectionSwitched(TSelection *, TSelection *)));
}

//------------------------------------------------------------------

/*! Removes and then deletes all item in the scene.
 */

void SchematicScene::clearAllItems() {
  clearSelection();
  m_highlightedLinks.clear();
  QList<SchematicWindowEditor *> editors;
  QList<SchematicNode *> nodes;
  QList<SchematicLink *> links;
  int i;
  QList<QGraphicsItem *> sceneItems = items();
  int size                          = sceneItems.size();
  // create nodes and links list
  for (i = 0; i < size; i++) {
    QGraphicsItem *item           = sceneItems.at(i);
    SchematicWindowEditor *editor = dynamic_cast<SchematicWindowEditor *>(item);
    SchematicNode *node           = dynamic_cast<SchematicNode *>(item);
    SchematicLink *link           = dynamic_cast<SchematicLink *>(item);
    if (editor) editors.append(editor);
    if (node) nodes.append(node);
    if (link) links.append(link);
  }
  while (links.size() > 0) {
    SchematicLink *link = links.back();
    removeItem(link);
    links.removeLast();
    SchematicPort *startPort = link->getStartPort();
    SchematicPort *endPort   = link->getEndPort();
    if (startPort) startPort->removeLink(link);
    if (endPort) endPort->removeLink(link);
    delete link;
  }
  while (editors.size() > 0) {
    SchematicWindowEditor *editor = editors.back();
    removeItem(editor);
    editors.removeLast();
    delete editor;
  }
  while (nodes.size() > 0) {
    SchematicNode *node = nodes.back();
    removeItem(node);
    nodes.removeLast();
    delete node;
  }
  assert(items().size() == 0);
}

//------------------------------------------------------------------
/*! check if any item exists in the rect
 */
bool SchematicScene::isAnEmptyZone(const QRectF &rect) {
  QList<QGraphicsItem *> allItems = items();
  for (auto const level : allItems) {
    SchematicNode *node = dynamic_cast<SchematicNode *>(level);
    if (!node) continue;
    FxSchematicNode *fxNode = dynamic_cast<FxSchematicNode *>(node);
    if (fxNode && fxNode->isA(eXSheetFx)) continue;
    if (node->boundingRect().translated(node->scenePos()).intersects(rect))
      return false;
  }
  return true;
}

//------------------------------------------------------------------

QVector<SchematicNode *> SchematicScene::getPlacedNode(SchematicNode *node) {
  QRectF rect = node->boundingRect().translated(node->scenePos());
  QList<QGraphicsItem *> allItems = items();
  QVector<SchematicNode *> nodes;
  for (auto const item : allItems) {
    SchematicNode *placedNode = dynamic_cast<SchematicNode *>(item);
    if (!placedNode || placedNode == node) continue;
    QRectF nodeRect =
        placedNode->boundingRect().translated(placedNode->scenePos());
    QRectF enlargedRect = rect.adjusted(-10, -10, 10, 10);
    if (enlargedRect.contains(nodeRect)) nodes.push_back(placedNode);
  }
  return nodes;
}

//------------------------------------------------------------------

void SchematicScene::addSnapTarget(const QPointF &pos, const QRectF &rect,
                                   const QPointF &theOtherEndPos,
                                   const QPointF &endPosOffset) {
  // reduce highlight margin
  QRectF nodeRect = rect.adjusted(5, 5, -5, -5);

  /*
  auto findSnapPos = [&](QPointF pos) {
    for (auto item : m_snapTargets) {
      if (item->scenePos() == pos) return true;
    }
    return false;
  };

  QList<QPointF> posList = {pos};

  for (int i = 1; i <= 10; i++) {
    QPointF tmp_pos =
        pos + QPointF(0, (double)(SchematicScene::snapVInterval * i));
    posList.append(tmp_pos);
    if (isAnEmptyZone(nodeRect.translated(tmp_pos))) break;
  }
  for (int i = -1; i >= -10; i--) {
    QPointF tmp_pos =
        pos + QPointF(0, (double)(SchematicScene::snapVInterval * i));
    posList.append(tmp_pos);
    if (isAnEmptyZone(nodeRect.translated(tmp_pos))) break;
  }

  for (auto p : posList) {
    if (findSnapPos(p)) continue;
    SnapTargetItem *item = new SnapTargetItem(p, nodeRect);
    addItem(item);
    m_snapTargets.append(item);
  }*/

  SnapTargetItem *item =
      new SnapTargetItem(pos, nodeRect, theOtherEndPos, endPosOffset);
  addItem(item);
  m_snapTargets.append(item);
}

//------------------------------------------------------------------

void SchematicScene::clearSnapTargets() {
  for (auto item : m_snapTargets) {
    removeItem(item);
    delete item;
  }
  m_snapTargets.clear();
}

//------------------------------------------------------------------
// snap to neighbor nodes on dragging
void SchematicScene::computeSnap(SchematicNode *node, QPointF &delta,
                                 bool enable) {
  if (m_snapTargets.isEmpty()) return;

  if (!enable) {
    // hide targets
    if (m_snapTargets[0]->isVisible()) {
      for (auto item : m_snapTargets) item->setVisible(false);
    }
    return;
  }

  if (!m_snapTargets[0]->isVisible()) {
    for (auto item : m_snapTargets) item->setVisible(true);
  }

  QPointF newScenePos = node->scenePos() + delta;
  QPointF newPos      = views()[0]->mapFromScene(newScenePos);

  for (auto target : m_snapTargets) {
    QPointF targetPos = target->scenePos();
    int snapIndex     = std::nearbyint((newScenePos.y() - targetPos.y()) /
                                       (double)SchematicScene::snapVInterval);
    targetPos.setY(targetPos.y() +
                   (double)(snapIndex * SchematicScene::snapVInterval));
    target->setPos(targetPos);
    if ((newPos - views()[0]->mapFromScene(targetPos)).manhattanLength() <
        snapDistance) {
      delta = targetPos - node->scenePos();
      return;
    }
  }
}

//==================================================================
//
// SchematicSceneViewer
//
//==================================================================

SchematicSceneViewer::SchematicSceneViewer(QWidget *parent)
    : QGraphicsView(parent)
    , m_buttonState(Qt::NoButton)
    , m_oldWinPos()
    , m_oldScenePos()
    , m_firstShowing(true) {
  setObjectName("SchematicSceneViewer");

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::NoAnchor);
  setRenderHint(QPainter::SmoothPixmapTransform);
  setRenderHint(QPainter::TextAntialiasing);
  setRenderHint(QPainter::Antialiasing);
  setInteractive(true);
  setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
  show();

  setAttribute(Qt::WA_AcceptTouchEvents);
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::PanGesture);
  grabGesture(Qt::PinchGesture);
}

//------------------------------------------------------------------

SchematicSceneViewer::~SchematicSceneViewer() {}

//------------------------------------------------------------------

/*! Reimplemets the QGraphicsView::mousePressEvent()
 */
void SchematicSceneViewer::mousePressEvent(QMouseEvent *me) {
  // qDebug() << "[mousePressEvent]";
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen &&
      !m_stylusUsed) {
    return;
  }

  m_buttonState = me->button();
  m_oldWinPos   = me->pos();
  m_oldScenePos = mapToScene(m_oldWinPos);

  if (m_buttonState == Qt::LeftButton) {
    if (m_cursorMode == CursorMode::Zoom) {
      m_zoomPoint = me->pos();
      m_zooming   = true;
      return;
    } else if (m_cursorMode == CursorMode::Hand) {
      m_mousePanPoint = m_touchDevice == QTouchDevice::TouchScreen
                            ? mapToScene(me->pos())
                            : me->pos() * getDevicePixelRatio(this);
      m_panning       = true;
      return;
    }
  } else if (m_buttonState == Qt::MiddleButton) {
    m_mousePanPoint = m_touchDevice == QTouchDevice::TouchScreen
                          ? mapToScene(me->pos())
                          : me->pos() * getDevicePixelRatio(this);
  }
  bool drawRect                       = true;
  QList<QGraphicsItem *> pointedItems = items(me->pos());
  int i;
  for (i = 0; i < pointedItems.size(); i++) {
    SchematicWindowEditor *editor =
        dynamic_cast<SchematicWindowEditor *>(pointedItems[i]);
    if (!editor) {
      drawRect = false;
      break;
    }
  }

  if (m_buttonState == Qt::LeftButton && drawRect)
    setDragMode(QGraphicsView::RubberBandDrag);
  QGraphicsView::mousePressEvent(me);
}

//------------------------------------------------------------------

/*! Reimplemets the QGraphicsView::mouseMoveEvent()
 */
void SchematicSceneViewer::mouseMoveEvent(QMouseEvent *me) {
  // qDebug() << "[mouseMoveEvent]";
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen &&
      !m_stylusUsed) {
    return;
  }

  QPoint currWinPos    = me->pos();
  QPointF currScenePos = mapToScene(currWinPos);
  if ((m_cursorMode == CursorMode::Hand && m_panning) ||
      m_buttonState == Qt::MiddleButton) {
    QPointF usePos     = m_touchDevice == QTouchDevice::TouchScreen
                             ? mapToScene(me->pos())
                             : me->pos() * getDevicePixelRatio(this);
    QPointF deltaPoint = usePos - m_mousePanPoint;
    panQt(deltaPoint);
    m_mousePanPoint = m_touchDevice == QTouchDevice::TouchScreen
                          ? mapToScene(me->pos())
                          : me->pos() * getDevicePixelRatio(this);
  } else {
    if (m_cursorMode == CursorMode::Zoom && m_zooming) {
      int deltaY     = (m_oldWinPos.y() - me->pos().y()) * 10;
      double factorY = exp(deltaY * 0.001);
      changeScale(m_zoomPoint, factorY);
      m_panning = false;
    }
    m_oldWinPos   = currWinPos;
    m_oldScenePos = currScenePos;
  }
  QGraphicsView::mouseMoveEvent(me);
}

//------------------------------------------------------------------

/*! Reimplemets the QGraphicsView::mouseReleaseEvent()
 */
void SchematicSceneViewer::mouseReleaseEvent(QMouseEvent *me) {
  // qDebug() << "[mouseReleaseEvent]";
  m_gestureActive = false;
  m_zooming       = false;
  m_panning       = false;
  m_stylusUsed    = false;
  m_scaleFactor   = 0.0;

  m_buttonState = Qt::NoButton;
  QGraphicsView::mouseReleaseEvent(me);
  setDragMode(QGraphicsView::NoDrag);
  // update();
}

//------------------------------------------------------------------

void SchematicSceneViewer::mouseDoubleClickEvent(QMouseEvent *event) {
  // qDebug() << "[mouseDoubleClickEvent]";
  if (m_gestureActive && !m_stylusUsed) {
    m_gestureActive = false;
    QGraphicsItem *item =
        scene()->itemAt(mapToScene(event->pos()), QTransform());
    if (!item) {
      fitScene();
      return;
    }

    mousePressEvent(event);
  }

  QGraphicsView::mouseDoubleClickEvent(event);
}

//------------------------------------------------------------------

void SchematicSceneViewer::keyPressEvent(QKeyEvent *ke) {
  ke->ignore();
  QGraphicsView::keyPressEvent(ke);
  if (!ke->isAccepted()) SchematicZoomer(this).exec(ke);
}

//------------------------------------------------------------------

/*! Reimplemets the QGraphicsView::wheelEvent()
 */
void SchematicSceneViewer::wheelEvent(QWheelEvent *me) {
  // qDebug() << "[wheelEvent]";

  int delta = 0;
  switch (me->source()) {
  case Qt::MouseEventNotSynthesized: {
    if (me->modifiers() & Qt::AltModifier)
      delta = me->angleDelta().x();
    else
      delta = me->angleDelta().y();
    break;
  }

  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = me->pixelDelta();
    QPoint numDegrees = me->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = me->pixelDelta().y();
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
    if ((m_gestureActive == true &&
         m_touchDevice == QTouchDevice::TouchScreen) ||
        m_gestureActive == false) {
      double factor = exp(delta * 0.001);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
      changeScale(me->position().toPoint(), factor);
#else
      changeScale(me->pos(), factor);
#endif
      m_panning = false;
    }
  }
  me->accept();
}

//------------------------------------------------------------------

void SchematicSceneViewer::zoomQt(bool zoomin, bool resetView) {
  if (resetView) {
    resetTransform();
    // resetting will set view to the center of items bounding
    centerOn(scene()->itemsBoundingRect().center());
    return;
  }

  double scale2 = transform().determinant();
  if ((scale2 < 100000 || !zoomin) && (scale2 > 0.001 * 0.05 || zoomin)) {
    double oldZoomScale = sqrt(scale2);
    double zoomScale =
        resetView ? 1
                  : ImageUtils::getQuantizedZoomFactor(oldZoomScale, zoomin);
    QTransform scale =
        QTransform().scale(zoomScale / oldZoomScale, zoomScale / oldZoomScale);

    // See QGraphicsView::mapToScene()'s doc for details
    QPointF sceneCenter(mapToScene(rect().center()));
    setTransform(scale, true);
    centerOn(sceneCenter);
  }
}

//------------------------------------------------------------------

/*! The view is scaled around the point \b winPos by \b scaleFactor;
 */
void SchematicSceneViewer::changeScale(const QPoint &winPos,
                                       qreal scaleFactor) {
  QPointF startScenePos = mapToScene(winPos);
  QTransform scale      = QTransform().scale(scaleFactor, scaleFactor);
  setTransform(scale, true);
  QPointF endScenePos = mapToScene(winPos);
  QPointF delta       = endScenePos - startScenePos;
  translate(delta.x(), delta.y());
}

//------------------------------------------------------------------

void SchematicSceneViewer::fitScene() {
  if (scene()) {
    QRectF rect = scene()->itemsBoundingRect();
    fitInView(rect, Qt::KeepAspectRatio);
  }
}

//------------------------------------------------------------------

void SchematicSceneViewer::centerOnCurrent() {
  SchematicScene *schematicScene = dynamic_cast<SchematicScene *>(scene());
  QGraphicsItem *node            = schematicScene->getCurrentNode();
  if (node) centerOn(node);
}

//------------------------------------------------------------------

void SchematicSceneViewer::reorderScene() {
  SchematicScene *schematicScene = dynamic_cast<SchematicScene *>(scene());
  schematicScene->reorderScene();
}

//------------------------------------------------------------------

void SchematicSceneViewer::normalizeScene() {
  // See QGraphicsView::mapToScene()'s doc for details
  QPointF sceneCenter(mapToScene(rect().center()));
  resetTransform();
#if defined(MACOSX)
  scale(1.32, 1.32);
#endif
  centerOn(sceneCenter);
}

//------------------------------------------------------------------
void SchematicSceneViewer::panQt(const QPointF &delta) {
  if (delta == QPointF()) return;
  setInteractive(false);
  // I need to disable QGraphicsView event handling to avoid the generation of
  // 'virtual' mouseMoveEvent
  translate(delta.x(), delta.y());
  // translate has changed the matrix affecting the mapToScene() method. I
  // have to recompute currScenePos
  setInteractive(true);
}

void SchematicSceneViewer::showEvent(QShowEvent *se) {
  QGraphicsView::showEvent(se);
  if (m_firstShowing) {
    m_firstShowing = false;
    QRectF rect    = scene()->itemsBoundingRect();
    resetTransform();
    centerOn(rect.center());
  }
}

//------------------------------------------------------------------

void SchematicSceneViewer::enterEvent(QEvent *e) {
  switch (m_cursorMode) {
  case CursorMode::Hand:
    setToolCursor(this, ToolCursor::PanCursor);
    break;
  case CursorMode::Zoom:
    setToolCursor(this, ToolCursor::ZoomCursor);
    break;
  default:
    setToolCursor(this, ToolCursor::StrokeSelectCursor);
    break;
  }
}

//------------------------------------------------------------------

void SchematicSceneViewer::leaveEvent(QEvent *e) { setCursor(Qt::ArrowCursor); }

//------------------------------------------------------------------

void SchematicSceneViewer::tabletEvent(QTabletEvent *e) {
  // qDebug() << "[tabletEvent]";
  if (e->type() == QTabletEvent::TabletPress) {
    m_stylusUsed = e->pointerType() ? true : false;
  } else if (e->type() == QTabletEvent::TabletRelease) {
    m_stylusUsed = false;
  }

  QGraphicsView::tabletEvent(e);
}

//------------------------------------------------------------------

void SchematicSceneViewer::gestureEvent(QGestureEvent *e) {
  // qDebug() << "[gestureEvent]";
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
    if (m_touchDevice == QTouchDevice::TouchScreen)
      firstCenter = mapFromGlobal(firstCenter);

    if (gesture->state() == Qt::GestureStarted) {
      m_gestureActive = true;
    } else if (gesture->state() == Qt::GestureFinished) {
      m_gestureActive = false;
      m_zooming       = false;
      m_scaleFactor   = 0.0;
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
        if (!m_zooming) {
          double delta = scaleFactor - 1;
          m_scaleFactor += delta;
          if (m_scaleFactor > .2 || m_scaleFactor < -.2) {
            m_zooming = true;
          }
        }
        if (m_zooming) {
          changeScale(firstCenter, scaleFactor);
          m_panning = false;
        }
        m_gestureActive = true;
      }

      if (changeFlags & QPinchGesture::CenterPointChanged) {
        QPointF centerDelta =
            (gesture->centerPoint() * getDevicePixelRatio(this)) -
            (gesture->lastCenterPoint() * getDevicePixelRatio(this));
        if (centerDelta.manhattanLength() > 1) {
          // panQt(centerDelta.toPoint());
        }
        m_gestureActive = true;
      }
    }
  }
  e->accept();
}

void SchematicSceneViewer::touchEvent(QTouchEvent *e, int type) {
  // qDebug() << "[touchEvent]";
  if (type == QEvent::TouchBegin) {
    m_touchActive   = true;
    m_firstPanPoint = e->touchPoints().at(0).pos();
    // obtain device type
    m_touchDevice = e->device()->type();
  } else if (m_touchActive) {
    // touchpads must have 2 finger panning for tools and navigation to be
    // functional on other devices, 1 finger panning is preferred
    if ((e->touchPoints().count() == 2 &&
         m_touchDevice == QTouchDevice::TouchPad) ||
        (e->touchPoints().count() == 1 &&
         m_touchDevice == QTouchDevice::TouchScreen)) {
      QTouchEvent::TouchPoint panPoint = e->touchPoints().at(0);
      if (!m_panning) {
        QPointF deltaPoint = panPoint.pos() - m_firstPanPoint;
        // minimize accidental and jerky zooming/rotating during 2 finger
        // panning
        if ((deltaPoint.manhattanLength() > 100) && !m_zooming) {
          m_panning = true;
        }
      }
      if (m_panning) {
        QPointF curPos      = m_touchDevice == QTouchDevice::TouchScreen
                                  ? mapToScene(panPoint.pos().toPoint())
                                  : mapToScene(panPoint.pos().toPoint()) *
                                   getDevicePixelRatio(this);
        QPointF lastPos     = m_touchDevice == QTouchDevice::TouchScreen
                                  ? mapToScene(panPoint.lastPos().toPoint())
                                  : mapToScene(panPoint.lastPos().toPoint()) *
                                    getDevicePixelRatio(this);
        QPointF centerDelta = curPos - lastPos;
        panQt(centerDelta);
      }
    }
  }
  if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
    m_touchActive = false;
    m_panning     = false;
  }
  e->accept();
}

bool SchematicSceneViewer::event(QEvent *e) {
  /*
  switch (e->type()) {
  case QEvent::TabletPress: {
    QTabletEvent *te = static_cast<QTabletEvent *>(e);
    qDebug() << "[event] TabletPress: pointerType(" << te->pointerType()
             << ") device(" << te->device() << ")";
  } break;
  case QEvent::TabletRelease:
    qDebug() << "[event] TabletRelease";
    break;
  case QEvent::TouchBegin:
    qDebug() << "[event] TouchBegin";
    break;
  case QEvent::TouchEnd:
    qDebug() << "[event] TouchEnd";
    break;
  case QEvent::TouchCancel:
    qDebug() << "[event] TouchCancel";
    break;
  case QEvent::MouseButtonPress:
    qDebug() << "[event] MouseButtonPress";
    break;
  case QEvent::MouseButtonDblClick:
    qDebug() << "[event] MouseButtonDblClick";
    break;
  case QEvent::MouseButtonRelease:
    qDebug() << "[event] MouseButtonRelease";
    break;
  case QEvent::Gesture:
    qDebug() << "[event] Gesture";
    break;
  default:
    break;
  }
  */

  if (e->type() == QEvent::Gesture && CommandManager::instance()
                                          ->getAction(MI_TouchGestureControl)
                                          ->isChecked()) {
    gestureEvent(static_cast<QGestureEvent *>(e));
    return true;
  }
  if ((e->type() == QEvent::TouchBegin || e->type() == QEvent::TouchEnd ||
       e->type() == QEvent::TouchCancel || e->type() == QEvent::TouchUpdate) &&
      CommandManager::instance()
          ->getAction(MI_TouchGestureControl)
          ->isChecked()) {
    touchEvent(static_cast<QTouchEvent *>(e), e->type());
    m_gestureActive = true;
    return true;
  }
  return QGraphicsView::event(e);
}

//------------------------------------------------------------------

void SchematicSceneViewer::setCursorMode(CursorMode cursorMode) {
  m_cursorMode = cursorMode;
}

//==================================================================
//
// SchematicViewer
//
//==================================================================

SchematicViewer::SchematicViewer(QWidget *parent)
    : QWidget(parent)
    , m_fullSchematic(true)
    , m_maximizedNode(false)
    , m_sceneHandle(0)
    , m_cursorMode(CursorMode::Select) {
  m_viewer     = new SchematicSceneViewer(this);
  m_stageScene = new StageSchematicScene(this);
  m_fxScene    = new FxSchematicScene(this);

  m_commonToolbar = new QToolBar(m_viewer);
  m_stageToolbar  = new QToolBar(m_viewer);
  m_fxToolbar     = new QToolBar(m_viewer);
  m_swapToolbar   = new QToolBar(m_viewer);

  m_commonToolbar->setObjectName("MediumPaddingToolBar");
  m_stageToolbar->setObjectName("MediumPaddingToolBar");
  m_fxToolbar->setObjectName("MediumPaddingToolBar");
  m_swapToolbar->setObjectName("MediumPaddingToolBar");

  createToolbars();
  createActions();

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  {
    mainLayout->addWidget(m_viewer, 1);

    QFrame *bottomFrame = new QFrame(this);
    bottomFrame->setObjectName("SchematicBottomFrame");
    QHBoxLayout *horizontalLayout = new QHBoxLayout();
    horizontalLayout->setMargin(0);
    horizontalLayout->setSpacing(0);
    {
      horizontalLayout->addWidget(m_commonToolbar);
      horizontalLayout->addStretch();
      horizontalLayout->addWidget(m_fxToolbar);
      horizontalLayout->addWidget(m_stageToolbar);
      horizontalLayout->addWidget(m_swapToolbar);
    }
    bottomFrame->setLayout(horizontalLayout);

    mainLayout->addWidget(bottomFrame, 0);
  }
  setLayout(mainLayout);

  connect(m_fxScene, SIGNAL(showPreview(TFxP)), this,
          SIGNAL(showPreview(TFxP)));
  connect(m_fxScene, SIGNAL(doCollapse(const QList<TFxP> &)), this,
          SIGNAL(doCollapse(const QList<TFxP> &)));
  connect(m_stageScene, SIGNAL(doCollapse(QList<TStageObjectId>)), this,
          SIGNAL(doCollapse(QList<TStageObjectId>)));
  connect(m_fxScene, SIGNAL(doExplodeChild(const QList<TFxP> &)), this,
          SIGNAL(doExplodeChild(const QList<TFxP> &)));
  connect(m_stageScene, SIGNAL(doExplodeChild(QList<TStageObjectId>)), this,
          SIGNAL(doExplodeChild(QList<TStageObjectId>)));
  connect(m_stageScene, SIGNAL(editObject()), this, SIGNAL(editObject()));
  connect(m_fxScene, SIGNAL(editObject()), this, SIGNAL(editObject()));

  connect(m_fxScene->getFxSelection(), SIGNAL(doDelete()), this,
          SLOT(deleteFxs()));
  connect(m_stageScene->getStageSelection(), SIGNAL(doDelete()), this,
          SLOT(deleteStageObjects()));

  connect(m_fxScene->getFxSelection(),
          SIGNAL(columnPasted(const QList<TXshColumnP> &)), this,
          SIGNAL(columnPasted(const QList<TXshColumnP> &)));
  connect(m_stageScene->getStageSelection(),
          SIGNAL(columnPasted(const QList<TXshColumnP> &)), this,
          SIGNAL(columnPasted(const QList<TXshColumnP> &)));

  m_viewer->setScene(m_stageScene);
  m_fxToolbar->hide();

  setFocusProxy(m_viewer);
}

//------------------------------------------------------------------

SchematicViewer::~SchematicViewer() {}

//------------------------------------------------------------------

void SchematicViewer::getNodeColor(int ltype, QColor &nodeColor) {
  switch (ltype) {
  case TZI_XSHLEVEL:
  case OVL_XSHLEVEL:
    nodeColor = getFullcolorColumnColor();
    break;
  case PLI_XSHLEVEL:
    nodeColor = getVectorColumnColor();
    break;
  case TZP_XSHLEVEL:
    nodeColor = getLevelColumnColor();
    break;
  case ZERARYFX_XSHLEVEL:
    nodeColor = getFxColumnColor();
    break;
  case CHILD_XSHLEVEL:
    nodeColor = getChildColumnColor();
    break;
  case MESH_XSHLEVEL:
    nodeColor = getMeshColumnColor();
    break;
  case PLT_XSHLEVEL:
    nodeColor = getPaletteColumnColor();
    break;
  case eNormalFx:
    nodeColor = getNormalFxColor();
    break;
  case eZeraryFx:
    nodeColor = getFxColumnColor();
    break;
  case eMacroFx:
    nodeColor = getMacroFxColor();
    break;
  case eGroupedFx:
    nodeColor = getGroupColor();
    break;
  case eNormalImageAdjustFx:
    nodeColor = getImageAdjustFxColor();
    break;
  case eNormalLayerBlendingFx:
    nodeColor = getLayerBlendingFxColor();
    break;
  case eNormalMatteFx:
    nodeColor = getMatteFxColor();
    break;
  default:
    nodeColor = grey210;
    break;
  }
}

//------------------------------------------------------------------

void SchematicViewer::setApplication(TApplication *app) {
  m_stageScene->setXsheetHandle(app->getCurrentXsheet());
  m_stageScene->setObjectHandle(app->getCurrentObject());
  m_stageScene->setFxHandle(app->getCurrentFx());
  m_stageScene->setColumnHandle(app->getCurrentColumn());
  m_stageScene->setSceneHandle(app->getCurrentScene());
  m_stageScene->setFrameHandle(app->getCurrentFrame());

  m_fxScene->setApplication(app);
}

//------------------------------------------------------------------

void SchematicViewer::updateSchematic() {
  m_stageScene->updateScene();
  m_fxScene->updateScene();
}

//------------------------------------------------------------------

void SchematicViewer::setSchematicScene(SchematicScene *scene) {
  if (scene) {
    m_viewer->setScene(scene);
    m_viewer->centerOn(scene->sceneRect().center());
  }
}

//------------------------------------------------------------------

void SchematicViewer::createToolbars() {
  // Initialize them
  m_stageToolbar->setMovable(false);
  m_stageToolbar->setIconSize(QSize(20, 20));
  m_stageToolbar->setLayoutDirection(Qt::RightToLeft);
  m_stageToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_commonToolbar->setMovable(false);
  m_commonToolbar->setIconSize(QSize(20, 20));
  m_commonToolbar->setLayoutDirection(Qt::RightToLeft);
  m_commonToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_fxToolbar->setMovable(false);
  m_fxToolbar->setIconSize(QSize(20, 20));
  m_fxToolbar->setLayoutDirection(Qt::RightToLeft);
  m_fxToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_swapToolbar->setMovable(false);
  m_swapToolbar->setIconSize(QSize(20, 20));
  m_swapToolbar->setLayoutDirection(Qt::RightToLeft);
  m_swapToolbar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

//------------------------------------------------------------------

void SchematicViewer::createActions() {
  // Create all actions
  QAction *addPegbar = 0, *addSpline = 0, *addCamera = 0, *insertFx = 0,
          *addOutputFx = 0, *switchPort = 0, *iconifyNodes = 0;
  {
    // Fit schematic
    QIcon fitSchematicIcon = createQIcon("fit_to_window");
    m_fitSchematic =
        new QAction(fitSchematicIcon, tr("&Fit to Window"), m_commonToolbar);
    connect(m_fitSchematic, SIGNAL(triggered()), m_viewer, SLOT(fitScene()));

    // Center On
    QIcon centerOnIcon = createQIcon("focus");
    m_centerOn =
        new QAction(centerOnIcon, tr("&Focus on Current"), m_commonToolbar);
    connect(m_centerOn, SIGNAL(triggered()), m_viewer, SLOT(centerOnCurrent()));

    // Reorder schematic
    QIcon reorderIcon = createQIcon("reorder", false);
    m_reorder = new QAction(reorderIcon, tr("&Reorder Nodes"), m_commonToolbar);
    connect(m_reorder, SIGNAL(triggered()), m_viewer, SLOT(reorderScene()));

    // Normalize schematic schematic
    QIcon normalizeIcon = createQIcon("actual_pixel_size");
    m_normalize =
        new QAction(normalizeIcon, tr("&Reset Size"), m_commonToolbar);
    connect(m_normalize, SIGNAL(triggered()), m_viewer, SLOT(normalizeScene()));

    QIcon nodeSizeIcon =
        createQIcon(m_maximizedNode ? "minimizenodes" : "maximizenodes");
    m_nodeSize = new QAction(
        nodeSizeIcon,
        m_maximizedNode ? tr("&Minimize Nodes") : tr("&Maximize Nodes"),
        m_commonToolbar);
    connect(m_nodeSize, SIGNAL(triggered()), this, SLOT(changeNodeSize()));

    QIcon selectModeIcon = createQIcon("selection_schematic");
    m_selectMode =
        new QAction(selectModeIcon, tr("&Selection Mode"), m_commonToolbar);
    m_selectMode->setCheckable(true);
    connect(m_selectMode, SIGNAL(triggered()), this, SLOT(selectModeEnabled()));

    QIcon zoomModeIcon = createQIcon("zoom_schematic");
    m_zoomMode = new QAction(zoomModeIcon, tr("&Zoom Mode"), m_commonToolbar);
    m_zoomMode->setCheckable(true);
    connect(m_zoomMode, SIGNAL(triggered()), this, SLOT(zoomModeEnabled()));

    QIcon handModeIcon = createQIcon("hand_schematic");
    m_handMode = new QAction(handModeIcon, tr("&Hand Mode"), m_commonToolbar);
    m_handMode->setCheckable(true);
    connect(m_handMode, SIGNAL(triggered()), this, SLOT(handModeEnabled()));

    setCursorMode(m_cursorMode);

    if (m_fullSchematic) {
      // AddPegbar
      addPegbar           = new QAction(tr("&New Pegbar"), m_stageToolbar);
      QIcon addPegbarIcon = createQIcon("pegbar");
      addPegbar->setIcon(addPegbarIcon);
      connect(addPegbar, SIGNAL(triggered()), m_stageScene,
              SLOT(onPegbarAdded()));

      // AddCamera
      addCamera           = new QAction(tr("&New Camera"), m_stageToolbar);
      QIcon addCameraIcon = createQIcon("camera");
      addCamera->setIcon(addCameraIcon);
      connect(addCamera, SIGNAL(triggered()), m_stageScene,
              SLOT(onCameraAdded()));

      // AddSpline
      addSpline           = new QAction(tr("&New Motion Path"), m_stageToolbar);
      QIcon addSplineIcon = createQIcon("motionpath");
      addSpline->setIcon(addSplineIcon);
      connect(addSpline, SIGNAL(triggered()), m_stageScene,
              SLOT(onSplineAdded()));

      // Switch display of stage schematic's output port
      switchPort =
          new QAction(tr("&Switch output port display mode"), m_stageToolbar);
      switchPort->setCheckable(true);
      switchPort->setChecked(m_stageScene->isShowLetterOnPortFlagEnabled());
      QIcon switchPortIcon = createQIcon("switchport");
      switchPort->setIcon(switchPortIcon);
      connect(switchPort, SIGNAL(toggled(bool)), m_stageScene,
              SLOT(onSwitchPortModeToggled(bool)));

      // InsertFx
      insertFx = CommandManager::instance()->getAction("MI_InsertFx");
      if (insertFx) {
        QIcon insertFxIcon = createQIcon("fx_logo");
        insertFx->setIcon(insertFxIcon);
      }

      // AddOutputFx
      addOutputFx = CommandManager::instance()->getAction("MI_NewOutputFx");

      // Iconify Fx nodes
      iconifyNodes = new QAction(tr("&Toggle node icons"), m_fxToolbar);
      iconifyNodes->setCheckable(true);
      iconifyNodes->setChecked(!m_fxScene->isNormalIconView());
      QIcon iconifyNodesIcon = createQIcon("iconifynodes");
      iconifyNodes->setIcon(iconifyNodesIcon);
      connect(iconifyNodes, SIGNAL(toggled(bool)), m_fxScene,
              SLOT(onIconifyNodesToggled(bool)));

      // Swap fx/stage schematic
      QIcon changeSchematicIcon = createQIcon("swap");
      m_changeScene =
          CommandManager::instance()->getAction("A_FxSchematicToggle", true);
      if (m_changeScene) {
        m_changeScene->setIcon(changeSchematicIcon);
        connect(m_changeScene, SIGNAL(triggered()), this,
                SLOT(onSceneChanged()));
      } else
        m_changeScene = 0;
    }
  }

  // Add actions to toolbars (in reverse)

  m_commonToolbar->addSeparator();
  m_commonToolbar->addAction(m_nodeSize);
  m_commonToolbar->addAction(m_normalize);
  m_commonToolbar->addAction(m_reorder);
  m_commonToolbar->addAction(m_centerOn);
  m_commonToolbar->addAction(m_fitSchematic);
  m_commonToolbar->addSeparator();
  m_commonToolbar->addAction(m_handMode);
  m_commonToolbar->addAction(m_zoomMode);
  m_commonToolbar->addAction(m_selectMode);

  if (m_fullSchematic) {
    m_stageToolbar->addSeparator();
    m_stageToolbar->addAction(switchPort);
    m_stageToolbar->addSeparator();
    m_stageToolbar->addAction(addSpline);
    m_stageToolbar->addAction(addCamera);
    m_stageToolbar->addAction(addPegbar);

    m_fxToolbar->addSeparator();
    m_fxToolbar->addAction(iconifyNodes);
    m_fxToolbar->addSeparator();
    m_fxToolbar->addAction(addOutputFx);
    m_fxToolbar->addAction(insertFx);

    if (m_changeScene) m_swapToolbar->addAction(m_changeScene);
  }
}

//------------------------------------------------------------------

void SchematicViewer::setStageSchematic() {
  if (m_viewer->scene() != m_stageScene) {
    m_viewer->setScene(m_stageScene);
    QRectF rect = m_stageScene->itemsBoundingRect();

    m_viewer->resetTransform();
    m_viewer->centerOn(rect.center());

    m_fxToolbar->hide();
    m_stageToolbar->show();

    m_viewer->update();
  }

  parentWidget()->setWindowTitle(QObject::tr("Stage Schematic"));
}

//------------------------------------------------------------------

void SchematicViewer::setFxSchematic() {
  if (m_viewer->scene() != m_fxScene) {
    m_viewer->setScene(m_fxScene);
    QRectF rect = m_fxScene->itemsBoundingRect();

    m_viewer->resetTransform();
    m_viewer->centerOn(rect.center());

    m_stageToolbar->hide();
    m_fxToolbar->show();

    // check if the fx scene was small scaled (icon view mode)
    if (!m_fxScene->isNormalIconView()) m_fxScene->updateScene();

    m_viewer->update();
  }

  parentWidget()->setWindowTitle(QObject::tr("FX Schematic"));
}

//------------------------------------------------------------------

void SchematicViewer::onSceneChanged() {
  if (!hasFocus()) return;

  QGraphicsScene *scene = m_viewer->scene();
  if (scene == m_fxScene)
    setStageSchematic();
  else if (scene == m_stageScene)
    setFxSchematic();
}

//------------------------------------------------------------------

void SchematicViewer::onSceneSwitched() {
  m_maximizedNode = m_fxScene->getXsheetHandle()
                        ->getXsheet()
                        ->getFxDag()
                        ->getDagGridDimension() == 0;
  QIcon nodeSizeIcon =
      createQIcon(m_maximizedNode ? "minimizenodes" : "maximizenodes");
  m_nodeSize->setIcon(nodeSizeIcon);
  QString label(m_maximizedNode ? tr("&Minimize Nodes")
                                : tr("&Maximize Nodes"));
  m_nodeSize->setText(label);

  // reset schematic
  m_viewer->resetTransform();
  m_viewer->centerOn(m_viewer->scene()->itemsBoundingRect().center());
  if (m_viewer->scene() == m_fxScene && !m_fxScene->isNormalIconView())
    m_fxScene->updateScene();
}

//------------------------------------------------------------------

bool SchematicViewer::isStageSchematicViewed() {
  QGraphicsScene *scene = m_viewer->scene();
  return scene == m_stageScene;
}

//------------------------------------------------------------------

void SchematicViewer::setStageSchematicViewed(bool isStageSchematic) {
  if (!m_fullSchematic) isStageSchematic = true;

  if (isStageSchematic == isStageSchematicViewed()) return;
  if (isStageSchematic)
    setStageSchematic();
  else
    setFxSchematic();
}

//------------------------------------------------------------------
// called when the signals xshLevelChanged or objectSwitched is emitted
void SchematicViewer::updateScenes() {
  TStageObjectId id = m_stageScene->getCurrentObject();
  if (id.isColumn()) {
    m_stageScene->update();
    TXsheet *xsh = m_stageScene->getXsheetHandle()->getXsheet();
    if (!xsh) return;
    TXshColumn *column = xsh->getColumn(id.getIndex());
    if (!column || !column->getZeraryFxColumn()) return;
    TFx *fx = column->getZeraryFxColumn()->getZeraryColumnFx();
    m_fxScene->getFxHandle()->setFx(fx);
    m_fxScene->update();
  }
}

//------------------------------------------------------------------

void SchematicViewer::changeNodeSize() {
  m_maximizedNode = !m_maximizedNode;
  // aggiono l'icona del pulsante;
  m_fxScene->resizeNodes(m_maximizedNode);
  m_stageScene->resizeNodes(m_maximizedNode);
  QIcon nodeSizeIcon =
      createQIcon(m_maximizedNode ? "minimizenodes" : "maximizenodes");
  m_nodeSize->setIcon(nodeSizeIcon);
  QString label(m_maximizedNode ? tr("&Minimize Nodes")
                                : tr("&Maximize Nodes"));
  m_nodeSize->setText(label);
}

//------------------------------------------------------------------

QColor SchematicViewer::getSelectedNodeTextColor() {
  // get colors
  TPixel currentColumnPixel;
  Preferences::instance()->getCurrentColumnData(currentColumnPixel);
  QColor currentColumnColor((int)currentColumnPixel.r,
                            (int)currentColumnPixel.g,
                            (int)currentColumnPixel.b, 255);
  return currentColumnColor;
}

//------------------------------------------------------------------

void SchematicViewer::setCursorMode(CursorMode cursorMode) {
  m_cursorMode = cursorMode;
  m_viewer->setCursorMode(m_cursorMode);

  m_selectMode->setChecked((m_cursorMode == CursorMode::Select));
  m_zoomMode->setChecked((m_cursorMode == CursorMode::Zoom));
  m_handMode->setChecked((m_cursorMode == CursorMode::Hand));
}

//------------------------------------------------------------------

void SchematicViewer::selectModeEnabled() { setCursorMode(CursorMode::Select); }

//------------------------------------------------------------------

void SchematicViewer::zoomModeEnabled() { setCursorMode(CursorMode::Zoom); }

//------------------------------------------------------------------

void SchematicViewer::handModeEnabled() { setCursorMode(CursorMode::Hand); }

//------------------------------------------------------------------

void SchematicViewer::deleteFxs() {
  emit doDeleteFxs(m_fxScene->getFxSelection());
}

//------------------------------------------------------------------

void SchematicViewer::deleteStageObjects() {
  emit doDeleteStageObjects(m_stageScene->getStageSelection());
}
