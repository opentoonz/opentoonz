

#include "toonzqt/schematicnode.h"
#include "toonzqt/stageschematicscene.h"

#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QKeyEvent>
#include <algorithm>
#include <QApplication>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QMenuBar>
#include <QPolygonF>
#include <QDesktopWidget>
#include "tundo.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

//========================================================
//
// StageSchematicName
//
//========================================================

SchematicName::SchematicName(QGraphicsItem *parent, double width, double height)
    : QGraphicsTextItem("", parent), m_width(width), m_height(height) {
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);
  setTextInteractionFlags(Qt::TextEditorInteraction);

  connect(document(), SIGNAL(contentsChanged()), this,
          SLOT(onContentsChanged()));
}

//--------------------------------------------------------

SchematicName::~SchematicName() {}

//--------------------------------------------------------

void SchematicName::setName(const QString &name) { setPlainText(name); }

//--------------------------------------------------------

void SchematicName::onContentsChanged() {
  QString text       = document()->toPlainText();
  QTextCursor cursor = textCursor();
  int position       = cursor.position();
  if (position > 0 && text.at(position - 1) == '\n') {
    text.remove("\n");
    setPlainText(text);
    ;
    emit focusOut();
  }
}

//--------------------------------------------------------

void SchematicName::focusOutEvent(QFocusEvent *fe) {
  qApp->removeEventFilter(this);
  if (fe->reason() == Qt::MouseFocusReason) emit focusOut();
}

//--------------------------------------------------------

void SchematicName::keyPressEvent(QKeyEvent *ke) {
  if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right) {
    QTextCursor cursor = textCursor();
    int currentPos     = cursor.position();
    if (ke->key() == Qt::Key_Left)
      cursor.setPosition(currentPos - 1);
    else
      cursor.setPosition(currentPos + 1);
    setTextCursor(cursor);
  } else
    QGraphicsTextItem::keyPressEvent(ke);
}

//--------------------------------------------------------

bool SchematicName::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::Shortcut ||
      event->type() == QEvent::ShortcutOverride) {
    if (!object->inherits("QGraphicsView")) {
      event->accept();
      return true;
    }
  }
  return false;
}

//--------------------------------------------------------

void SchematicName::focusInEvent(QFocusEvent *fe) {
  QGraphicsTextItem::focusInEvent(fe);
  qApp->installEventFilter(this);
  QTextDocument *doc = document();
  QTextCursor cursor(doc->begin());
  cursor.select(QTextCursor::Document);
  setTextCursor(cursor);
}

//========================================================
//
// class SchematicThumbnailToggle
//
//========================================================

SchematicThumbnailToggle::SchematicThumbnailToggle(SchematicNode *parent,
                                                   bool isOpened)
    : QGraphicsItem(parent), m_isDown(!isOpened) {}

//--------------------------------------------------------

SchematicThumbnailToggle::~SchematicThumbnailToggle() {}

//--------------------------------------------------------

QRectF SchematicThumbnailToggle::boundingRect() const {
  return QRectF(0, 0, 14, 14);
}

//--------------------------------------------------------

void SchematicThumbnailToggle::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *option,
                                     QWidget *widget) {
  QRect rect(3, 3, 8, 8);
  QRect sourceRect = scene()->views()[0]->matrix().mapRect(rect);
  static QIcon onIcon(":Resources/schematic_thumbtoggle_on.svg");
  static QIcon offIcon(":Resources/schematic_thumbtoggle_off.svg");
  QPixmap pixmap;
  if (m_isDown)
    pixmap = offIcon.pixmap(sourceRect.size());
  else
    pixmap   = onIcon.pixmap(sourceRect.size());
  sourceRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                     sourceRect.height() * getDevPixRatio());
  painter->drawPixmap(rect, pixmap, sourceRect);
}

//--------------------------------------------------------

void SchematicThumbnailToggle::setIsDown(bool value) {
  m_isDown = value;
  emit(toggled(!m_isDown));
}

//--------------------------------------------------------

void SchematicThumbnailToggle::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  m_isDown = !m_isDown;
  emit(toggled(!m_isDown));
}

//========================================================
//
// SchematicToggle
//
//========================================================

SchematicToggle::SchematicToggle(SchematicNode *parent, const QIcon &imageOn,
                                 QColor colorOn, int flags,
                                 bool isNormalIconView)
    : QGraphicsItem(parent)
    , m_imageOn(imageOn)
    , m_imageOn2()
    , m_imageOff()
    , m_state(0)
    , m_flags(flags)
    , m_width(isNormalIconView ? 18 : 30)
    , m_height(isNormalIconView ? 7 : 5)
    , m_colorOn(colorOn)
    , m_colorOff(QColor(0, 0, 0, 0)) {}

//--------------------------------------------------------

SchematicToggle::SchematicToggle(SchematicNode *parent, const QIcon &imageOn,
                                 QColor colorOn, const QIcon &imageOff,
                                 QColor colorOff, int flags,
                                 bool isNormalIconView)
    : QGraphicsItem(parent)
    , m_imageOn(imageOn)
    , m_imageOn2()
    , m_imageOff(imageOff)
    , m_state(0)
    , m_flags(flags)
    , m_width(isNormalIconView ? 18 : 30)
    , m_height(isNormalIconView ? 7 : 5)
    , m_colorOn(colorOn)
    , m_colorOff(colorOff) {}

//--------------------------------------------------------

SchematicToggle::SchematicToggle(SchematicNode *parent, const QIcon &imageOn,
                                 const QIcon &imageOn2, QColor colorOn,
                                 int flags, bool isNormalIconView)
    : QGraphicsItem(parent)
    , m_imageOn(imageOn)
    , m_imageOn2(imageOn2)
    , m_imageOff()
    , m_state(0)
    , m_flags(flags)
    , m_width(isNormalIconView ? 18 : 30)
    , m_height(isNormalIconView ? 7 : 5)
    , m_colorOn(colorOn)
    , m_colorOff(QColor(0, 0, 0, 0)) {}

//--------------------------------------------------------

SchematicToggle::SchematicToggle(SchematicNode *parent, const QIcon &imageOn,
                                 const QIcon &imageOn2, QColor colorOn,
                                 const QIcon &imageOff, QColor colorOff,
                                 int flags, bool isNormalIconView)
    : QGraphicsItem(parent)
    , m_imageOn(imageOn)
    , m_imageOn2(imageOn2)
    , m_imageOff(imageOff)
    , m_state(0)
    , m_flags(flags)
    , m_width(isNormalIconView ? 18 : 30)
    , m_height(isNormalIconView ? 7 : 5)
    , m_colorOn(colorOn)
    , m_colorOff(colorOff) {}

//--------------------------------------------------------

SchematicToggle::~SchematicToggle() {}

//--------------------------------------------------------

QRectF SchematicToggle::boundingRect() const {
  return QRectF(0, 0, m_width, m_height);
}

//--------------------------------------------------------

void SchematicToggle::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *option,
                            QWidget *widget) {
  int rectHeight = boundingRect().height();
  int rectWidth  = boundingRect().width();
  int rectX      = boundingRect().left();
  int rectY      = boundingRect().top();

  QRect rect =
      QRect(0, 0, rectHeight, rectHeight)
          .translated(rectX + (rectWidth / 2) - (rectHeight / 2), rectY);

  if (m_state != 0) {
    QIcon &pix =
        (m_state == 2 && !m_imageOn2.isNull()) ? m_imageOn2 : m_imageOn;
    painter->fillRect(boundingRect().toRect(), m_colorOn);
    QRect sourceRect =
        scene()->views()[0]->matrix().mapRect(QRect(0, 0, 18, 17));
    QPixmap redPm = pix.pixmap(sourceRect.size());
    QRect newRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                          sourceRect.height() * getDevPixRatio());
    painter->drawPixmap(rect, redPm, newRect);
  } else if (!m_imageOff.isNull()) {
    QPen pen(m_colorOn);
    pen.setWidthF(0.5);
    painter->setPen(pen);
    painter->setBrush(m_colorOff);
    double d = pen.widthF() / 2.0;
    painter->drawRect(boundingRect().adjusted(d, d, -d, -d));
    QRect sourceRect =
        scene()->views()[0]->matrix().mapRect(QRect(0, 0, 18, 17));
    QPixmap redPm = m_imageOff.pixmap(sourceRect.size());
    QRect newRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                          sourceRect.height() * getDevPixRatio());
    painter->drawPixmap(rect, redPm, newRect);
  }
}

//--------------------------------------------------------

void SchematicToggle::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  if (me->button() == Qt::LeftButton) {
    if (m_imageOn2.isNull()) {
      m_state = 1 - m_state;
      emit(toggled(m_state != 0));
    } else if (m_flags & eEnableNullState) {
      m_state = (m_state + 1) % 3;
      emit(stateChanged(m_state));
    } else {
      m_state = 3 - m_state;
      emit(stateChanged(m_state));
    }
  }
  if (me->button() == Qt::RightButton) {
    SchematicNode *parent = dynamic_cast<SchematicNode *>(this->parentItem());
    if (parent) parent->onClicked();
  }
}
//--------------------------------------------------------

void SchematicToggle::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  if (!(m_flags & eIsParentColumn)) return;
  if (m_imageOn2.isNull()) {
    QMenu *menu                = new QMenu(0);
    CommandManager *cmdManager = CommandManager::instance();
    menu->addAction(cmdManager->getAction("MI_EnableThisColumnOnly"));
    menu->addAction(cmdManager->getAction("MI_EnableSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_EnableAllColumns"));
    menu->addAction(cmdManager->getAction("MI_DisableAllColumns"));
    menu->addAction(cmdManager->getAction("MI_DisableSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_SwapEnabledColumns"));
    QAction *action = menu->exec(cme->screenPos());
  } else {
    QMenu *menu                = new QMenu(0);
    CommandManager *cmdManager = CommandManager::instance();
    menu->addAction(cmdManager->getAction("MI_ActivateThisColumnOnly"));
    menu->addAction(cmdManager->getAction("MI_ActivateSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_ActivateAllColumns"));
    menu->addAction(cmdManager->getAction("MI_DeactivateAllColumns"));
    menu->addAction(cmdManager->getAction("MI_DeactivateSelectedColumns"));
    menu->addAction(cmdManager->getAction("MI_ToggleColumnsActivation"));
    QAction *action = menu->exec(cme->screenPos());
  }
}
//========================================================

//--------------------------------------------------------
/*! for Spline Aim and CP toggles
*/
void SchematicToggle_SplineOptions::paint(
    QPainter *painter, const QStyleOptionGraphicsItem *option,
    QWidget *widget) {
  QRectF rect = boundingRect();
  painter->fillRect(rect, Qt::white);
  if (m_state != 0) {
    QIcon &pix =
        (m_state == 2 && !m_imageOn2.isNull()) ? m_imageOn2 : m_imageOn;
    QRect sourceRect =
        scene()->views()[0]->matrix().mapRect(QRect(0, 0, 18, 17));
    QPixmap redPm = pix.pixmap(sourceRect.size());
    QRect newRect = QRect(0, 0, sourceRect.width() * getDevPixRatio(),
                          sourceRect.height() * getDevPixRatio());
    painter->drawPixmap(rect, redPm, newRect);
  }
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QColor(180, 180, 180, 255));
  painter->drawRect(rect);
}

//--------------------------------------------------------
/*! for Spline Aim and CP toggles
*/
void SchematicToggle_SplineOptions::mousePressEvent(
    QGraphicsSceneMouseEvent *me) {
  SchematicToggle::mousePressEvent(me);
  update();
}

//========================================================
//
// SchematicHandleSpinBox
//
//========================================================

SchematicHandleSpinBox::SchematicHandleSpinBox(QGraphicsItem *parent)
    : QGraphicsItem(parent), m_buttonState(Qt::NoButton), m_delta(0) {
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  m_pixmap = QPixmap(":Resources/schematic_spin_arrows.svg");
}

//--------------------------------------------------------

SchematicHandleSpinBox::~SchematicHandleSpinBox() {}

//--------------------------------------------------------

QRectF SchematicHandleSpinBox::boundingRect() const {
  return QRectF(0, 0, 10, 10);
}

//--------------------------------------------------------

void SchematicHandleSpinBox::paint(QPainter *painter,
                                   const QStyleOptionGraphicsItem *option,
                                   QWidget *widget) {
  QRectF rect = boundingRect();
  painter->drawPixmap(rect.toRect(), m_pixmap);
  painter->setBrush(QColor(0, 0, 0, 0));
  painter->setPen(QColor(128, 128, 128, 255));
  painter->drawRect(rect);
}

//--------------------------------------------------------

void SchematicHandleSpinBox::mouseMoveEvent(QGraphicsSceneMouseEvent *me) {
  if (m_buttonState == Qt::LeftButton) {
    bool increase           = false;
    int delta               = me->screenPos().y() - me->lastScreenPos().y();
    if (delta < 0) increase = true;
    m_delta += abs(delta);
    if (m_delta > 5) {
      if (increase)
        emit(modifyHandle(1));
      else
        emit(modifyHandle(-1));
      m_delta = 0;
      emit sceneChanged();
    }
  }
}

//--------------------------------------------------------

void SchematicHandleSpinBox::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  m_buttonState = me->button();
  TUndoManager::manager()->beginBlock();
}

//--------------------------------------------------------

void SchematicHandleSpinBox::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  m_buttonState = Qt::NoButton;
  m_delta       = 0;
  TUndoManager::manager()->endBlock();
  emit handleReleased();
}

//========================================================
//
// class SchematicLink
//
//========================================================

SchematicLink::SchematicLink(QGraphicsItem *parent, QGraphicsScene *scene)
#if QT_VERSION >= 0x050000
    : QGraphicsItem(parent)
#else
    : QGraphicsItem(parent, scene)
#endif
    , m_startPort(0)
    , m_endPort(0)
    , m_path()
    , m_hitPath()
    , m_lineShaped(false)
    , m_highlighted(false) {
#if QT_VERSION >= 0x050000
  scene->addItem(this);
#endif
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  setZValue(0.0);
}

//--------------------------------------------------------

SchematicLink::~SchematicLink() { m_startPort = m_endPort = 0; }

//--------------------------------------------------------

QRectF SchematicLink::boundingRect() const {
  return m_hitPath.boundingRect().adjusted(-5, -5, 5, 5);
}

//--------------------------------------------------------

QPainterPath SchematicLink::shape() const { return m_hitPath; }

//--------------------------------------------------------

void SchematicLink::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  if (getStartPort() && (getStartPort()->getType() == 100  // eStageSplinePort
                         || getStartPort()->getType() == 202)) {  // eFxLinkPort
    if (isSelected() || isHighlighted())
      painter->setPen(QColor(255, 255, 10));
    else
      painter->setPen(QColor(50, 255, 50, 128));
  } else if (isSelected() || isHighlighted())
    painter->setPen(QPen(Qt::cyan));

  else if (!m_lineShaped)
    painter->setPen(QPen(Qt::white));
  else
    painter->setPen(QPen(QColor(170, 170, 10), 0, Qt::DashLine));

  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->drawPath(m_path);
}

//--------------------------------------------------------

void SchematicLink::updatePath(const QPointF &startPos, const QPointF &endPos) {
  prepareGeometryChange();
  setPos(startPos);
  if (!m_lineShaped) {
    QPointF p0((endPos.x() - startPos.x()) * 0.5, 0);
    QPointF p1(p0.x(), endPos.y() - startPos.y());
    QPointF p2(endPos - startPos);

    m_path = QPainterPath(QPointF(0, 0));
    m_path.cubicTo(p0, p1, p2);

    QPointF h(0, 5);
    QPointF p = h;
    if (p2.y() > 0)
      p.setX(p2.x() > 0 ? -5 : 5);
    else
      p.setX(p2.x() > 0 ? 5 : -5);
    m_hitPath = QPainterPath(QPointF(0, 0));
    m_hitPath.lineTo(h);
    m_hitPath.cubicTo(p0 + p, p1 + p, p2 + h);
    m_hitPath.lineTo(p2 - h);
    m_hitPath.cubicTo(p1 - p, p0 - p, -h);
    m_hitPath.lineTo(0, 0);
  } else {
    m_path = QPainterPath(QPointF(0, 0));
    m_path.lineTo(endPos - startPos);

    m_hitPath = QPainterPath(QPointF(0, 0));
    m_hitPath.lineTo(endPos - startPos);
  }
}

//--------------------------------------------------------

void SchematicLink::updatePath(SchematicPort *startPort,
                               SchematicPort *endPort) {
  updatePath(startPort->getLinkEndPoint(), endPort->getLinkEndPoint());
}

//--------------------------------------------------------

void SchematicLink::updateEndPos(const QPointF &endPos) {
  if (m_startPort) updatePath(m_startPort->getLinkEndPoint(), endPos);
}

//--------------------------------------------------------

SchematicPort *SchematicLink::getOtherPort(const SchematicPort *port) const {
  if (port == m_startPort)
    return m_endPort;
  else if (port == m_endPort)
    return m_startPort;
  else
    return 0;
}

//--------------------------------------------------------

SchematicNode *SchematicLink::getOtherNode(const SchematicNode *node) const {
  if (node == m_startPort->getNode())
    return m_endPort->getNode();
  else if (node == m_endPort->getNode())
    return m_startPort->getNode();
  else
    return 0;
}

//--------------------------------------------------------

void SchematicLink::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  QPointF pos              = me->scenePos();
  SchematicPort *startPort = getStartPort();
  SchematicPort *endPort   = getEndPort();
  if (startPort && endPort) {
    QRectF startRect = startPort->boundingRect();
    startRect.moveTopLeft(startPort->scenePos());
    QRectF endRect = endPort->boundingRect();
    endRect.moveTopLeft(endPort->scenePos());
    if (startRect.contains(pos) || endRect.contains(pos)) {
      me->ignore();
      return;
    }
  }

  QMatrix matrix = scene()->views()[0]->matrix();
#if QT_VERSION >= 0x050000
  double scaleFactor = sqrt(matrix.determinant());
#else
  double scaleFactor = sqrt(matrix.det());
#endif

  QPointF startPos = getStartPort()->getLinkEndPoint();
  QPointF endPos   = getEndPort()->getLinkEndPoint();
  QPointF p0((endPos.x() - startPos.x()) * 0.5, 0);
  QPointF p1(p0.x(), endPos.y() - startPos.y());
  QPointF p2(endPos - startPos);
  double sensitivity = 5 / scaleFactor;
  QPointF h(0, sensitivity);
  QPointF p = h;
  if (p2.y() > 0)
    p.setX(p2.x() > 0 ? -sensitivity : sensitivity);
  else
    p.setX(p2.x() > 0 ? sensitivity : -sensitivity);
  QPainterPath path(QPointF(0, 0));
  path.lineTo(h);
  path.cubicTo(p0 + p, p1 + p, p2 + h);
  path.lineTo(p2 - h);
  path.cubicTo(p1 - p, p0 - p, -h);
  path.lineTo(0, 0);
  if (!path.contains(me->scenePos() - scenePos())) {
    me->ignore();
    return;
  }

  if (!isSelected()) {
    if (me->modifiers() != Qt::ControlModifier) scene()->clearSelection();
    if (me->button() == Qt::LeftButton || me->button() == Qt::RightButton)
      setSelected(true);
  } else {
    if (me->modifiers() == Qt::ControlModifier &&
        me->button() == Qt::LeftButton)
      setSelected(false);
  }
}

//--------------------------------------------------------

void SchematicLink::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  if (me->modifiers() != Qt::ControlModifier && me->button() != Qt::RightButton)
    QGraphicsItem::mouseReleaseEvent(me);
}

//========================================================
//
// class SchematicPort
//
//========================================================

SchematicPort::SchematicPort(QGraphicsItem *parent, SchematicNode *node,
                             int type)
    : QGraphicsItem(parent)
    , m_node(node)
    , m_buttonState(Qt::NoButton)
    , m_highlighted(false)
    , m_linkingTo(0)
    , m_hook(0, 0)
    , m_type(type) {
#if QT_VERSION >= 0x050000
  setAcceptHoverEvents(false);
#else
  setAcceptsHoverEvents(false);
#endif
  setFlag(QGraphicsItem::ItemIsSelectable, false);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
}

//--------------------------------------------------------

SchematicPort::~SchematicPort() { m_links.clear(); }

//--------------------------------------------------------

void SchematicPort::mouseMoveEvent(QGraphicsSceneMouseEvent *me) {
  if (m_buttonState != Qt::LeftButton) return;
  if (m_ghostLinks.isEmpty()) return;

  // Snapping
  SchematicPort *linkingTo = searchPort(me->scenePos());
  if (!linkingTo) {
    for (SchematicLink *ghostLink : m_ghostLinks) {
      ghostLink->updateEndPos(me->scenePos());
      ghostLink->getStartPort()->showSnappedLinks(m_linkingTo);
    }
    if (m_linkingTo) {
      m_linkingTo->highLight(false);
      m_linkingTo->update();
      m_linkingTo = nullptr;
    }
  }
  // if to be connected something
  else if (linkingTo != this) {
    m_linkingTo = linkingTo;
    for (SchematicLink *ghostLink : m_ghostLinks) {
      ghostLink->updatePath(ghostLink->getStartPort(), linkingTo);
      ghostLink->getStartPort()->hideSnappedLinks(m_linkingTo);
    }
  }
  // autopan
  QGraphicsView *viewer = scene()->views()[0];
  viewer->setInteractive(false);
  viewer->ensureVisible(QRectF(me->scenePos(), QSizeF(1, 1)), 5, 5);
  viewer->setInteractive(true);
}

//--------------------------------------------------------

void SchematicPort::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  if (!isSelected()) {
    if (me->modifiers() != Qt::ControlModifier) scene()->clearSelection();
    if (me->button() == Qt::LeftButton || me->button() == Qt::RightButton)
      getNode()->setSelected(true);
  } else {
    if (me->modifiers() == Qt::ControlModifier &&
        me->button() == Qt::LeftButton)
      getNode()->setSelected(false);
  }
  getNode()->onClicked();
  if (me->button() == Qt::LeftButton && getType() != 202  // eFxLinkPort
      && getType() != 203                                 // eFxGroupedInPort
      && getType() != 204                                 // eFxGroupedOutPort
      && getType() != 103   // eStageSplineGroupPort
      && getType() != 104   // eStageParentGroupPort
      && getType() != 105)  // eStageChildGroupPort
  {
    m_buttonState = Qt::LeftButton;
    QPointF endPos(me->pos());

    // Enable to connect multiple links from all selected nodes
    // only when ( Ctrl + ) dragging from the eStageParentPort.
    if (getType() == 101) {  // eStageParentPort
      QList<QGraphicsItem *> items = scene()->selectedItems();
      if (items.empty()) return;
      for (auto const &item : items) {
        SchematicNode *node = dynamic_cast<SchematicNode *>(item);
        if (node) {
          SchematicPort *parentPort =
              node->getPort(0);  // id 0 is for parent port.
          if (parentPort) {
            SchematicLink *ghostLink = new SchematicLink(0, scene());
            ghostLink->setStartPort(parentPort);
            ghostLink->setZValue(3.0);
            ghostLink->updateEndPos(me->scenePos());
            m_ghostLinks.push_back(ghostLink);
          }
        }
      }
    } else {
      SchematicLink *ghostLink = new SchematicLink(0, scene());
      ghostLink->setStartPort(this);
      ghostLink->setZValue(3.0);
      ghostLink->updateEndPos(me->scenePos());
      m_ghostLinks.push_back(ghostLink);
    }
    emit(isClicked());
  }
}

//--------------------------------------------------------

void SchematicPort::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  if (me->modifiers() != Qt::ControlModifier && me->button() != Qt::RightButton)
    QGraphicsItem::mouseReleaseEvent(me);

  if (m_buttonState == Qt::LeftButton) emit(isReleased(me->scenePos()));

  // The link is added to the scene only if the user released the left mouse
  // button over
  // a SchematicPort different from SchematicPort of the parent node.
  bool somethingChanged = false;
  if (m_buttonState == Qt::LeftButton && m_linkingTo) {
    TUndoManager::manager()->beginBlock();
    for (SchematicLink *ghostLink : m_ghostLinks) {
      SchematicPort *port = ghostLink->getStartPort();
      if (!port) continue;
      if (!port->isLinkedTo(m_linkingTo) && port->linkTo(m_linkingTo, true)) {
        port->linkTo(m_linkingTo);
        somethingChanged = true;
      } else
        port->showSnappedLinks(m_linkingTo);
    }
    m_buttonState = Qt::NoButton;
    m_linkingTo   = 0;
    TUndoManager::manager()->endBlock();
  }

  if (!m_ghostLinks.isEmpty()) {
    for (SchematicLink *ghostLink : m_ghostLinks)
      scene()->removeItem(ghostLink);
    qDeleteAll(m_ghostLinks.begin(), m_ghostLinks.end());
    m_ghostLinks.clear();
  }

  if (somethingChanged) {
    emit sceneChanged();
    emit xsheetChanged();
  }
}

//--------------------------------------------------------

void SchematicPort::removeLink(SchematicLink *link) { m_links.removeAll(link); }

//--------------------------------------------------------

void SchematicPort::eraseLink(SchematicLink *link) {
  SchematicPort *otherPort = link->getOtherPort(this);
  if (otherPort) otherPort->removeLink(link);
  removeLink(link);
  if (link->scene()) link->scene()->removeItem(link);
  delete link;
}

//--------------------------------------------------------

void SchematicPort::eraseAllLinks() {
  while (!m_links.empty()) eraseLink(m_links[0]);
}

//--------------------------------------------------------

SchematicLink *SchematicPort::makeLink(SchematicPort *port) {
  if (isLinkedTo(port) || !port) return 0;
  SchematicLink *link = new SchematicLink(0, scene());
  if (getType() == 202 && port->getType() == 202) link->setLineShaped(true);

  link->setStartPort(this);
  link->setEndPort(port);
  addLink(link);
  port->addLink(link);
  link->updatePath();
  return link;
}

//--------------------------------------------------------

bool SchematicPort::isLinkedTo(SchematicPort *port) const {
  if (m_links.size() == 0) return false;
  int i;
  for (i = 0; i < m_links.size(); i++) {
    SchematicLink *link = m_links[i];
    if (((link->getStartPort() == this && link->getEndPort() == port) ||
         (link->getEndPort() == this && link->getStartPort() == port)) &&
        link->isVisible())
      return true;
  }
  return false;
}

//--------------------------------------------------------

void SchematicPort::updateLinksGeometry() {
  int linkCount = getLinkCount();
  int i;
  for (i = 0; i < linkCount; i++) {
    SchematicLink *link      = getLink(i);
    SchematicPort *startPort = link->getStartPort();
    SchematicPort *endPort   = link->getEndPort();
    if (startPort && endPort) {
      link->updatePath(startPort, endPort);
      link->setPos(startPort->getLinkEndPoint());
    }
  }
}

//--------------------------------------------------------

QPointF SchematicPort::getLinkEndPoint() const { return scenePos() + m_hook; }

//========================================================
//
// class SchematicNode
//
//========================================================

/*! \class SchematicNode schematicnode.h "../inlcude/toonzqt/schematicnode.h"
        \brief The class provides methods to draw and handle a node item in the
   SchematicScene.
*/

/*! \fn SchematicPort *SchematicNode::getInputPort() const
        Returns the input SchematicPort of the node.
*/

/*! \fn SchematicPort *SchematicNode::getOutputPort() const
        Returns the output SchematicPort of the node.
*/

/*! \fn SchematicScene* SchematicNode::getScene() const
        Returns the scene where the node is placed.
*/

SchematicNode::SchematicNode(SchematicScene *scene)
#if QT_VERSION >= 0x050000
    : QGraphicsItem(0)
#else
    : QGraphicsItem(0, scene)
#endif
    , m_scene(scene)
    , m_width(0)
    , m_height(0)
    , m_buttonState(Qt::NoButton) {
#if QT_VERSION >= 0x050000
  scene->addItem(this);
#endif
  setFlag(QGraphicsItem::ItemIsMovable, false);
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, false);
  setZValue(1.0);
}

//--------------------------------------------------------

SchematicNode::~SchematicNode() {}

//--------------------------------------------------------

/*!Reimplements the pure virtual QGraphicsItem::boundingRect() method.
*/
QRectF SchematicNode::boundingRect() const { return QRectF(0, 0, 1, 1); }

//--------------------------------------------------------

/*! Reimplements the pure virtual QGraphicsItem::paint() method.
*/
void SchematicNode::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
  QPen pen;
  if (isSelected()) {
    painter->setBrush(QColor(0, 0, 0, 0));
    pen.setColor(QColor(255, 255, 255, 255));

    pen.setWidth(4.0);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->drawRect(-2, -2, m_width + 4, m_height + 4);
  }
  pen.setColor(QColor(0, 0, 0, 255));
  pen.setWidth(0);
  painter->setPen(pen);
}

//--------------------------------------------------------

/*! Reimplements the QGraphicsItem::mouseMoveEvent() method.
*/
void SchematicNode::mouseMoveEvent(QGraphicsSceneMouseEvent *me) {
  QList<QGraphicsItem *> items = scene()->selectedItems();
  if (items.empty()) return;
  QPointF delta         = me->scenePos() - me->lastScenePos();
  QGraphicsView *viewer = scene()->views()[0];
  for (auto const &item : items) {
    SchematicNode *node = dynamic_cast<SchematicNode *>(item);
    if (node) {
      node->setPosition(node->scenePos() + delta);
      node->setSchematicNodePos(node->scenePos());
      node->updateLinksGeometry();
    }
  }
  viewer->setInteractive(false);
  viewer->ensureVisible(QRectF(me->scenePos(), QSizeF(1, 1)), 5, 5);
  viewer->setInteractive(true);
}

//--------------------------------------------------------

void SchematicNode::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  if (!isSelected()) {
    if (me->modifiers() != Qt::ControlModifier) scene()->clearSelection();
    if (me->button() == Qt::LeftButton || me->button() == Qt::RightButton)
      setSelected(true);
  } else {
    if (me->modifiers() == Qt::ControlModifier &&
        me->button() == Qt::LeftButton)
      setSelected(false);
  }
  onClicked();
}

//--------------------------------------------------------

void SchematicNode::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  if (me->modifiers() != Qt::ControlModifier && me->button() != Qt::RightButton)
    QGraphicsItem::mouseReleaseEvent(me);
}

//--------------------------------------------------------
/* Add a pair (portId, SchematicPort*port) in the mapping
*/
SchematicPort *SchematicNode::addPort(int portId, SchematicPort *port) {
  QMap<int, SchematicPort *>::iterator it;
  it = m_ports.find(portId);
  if (it != m_ports.end() && m_ports[portId] != port) {
    SchematicPort *oldPort = m_ports[portId];
    m_ports.erase(it);
    scene()->removeItem(oldPort);
    delete oldPort;
  }
  m_ports[portId] = port;
  return port;
}

//--------------------------------------------------------

/*!Erase the pair (portId, SchematicPort*port) from the mapping*/
void SchematicNode::erasePort(int portId) {
  QMap<int, SchematicPort *>::iterator it = m_ports.find(portId);
  if (it != m_ports.end()) {
    delete it.value();
    m_ports.erase(it);
  }
}

//--------------------------------------------------------

/*! Returns a pointer to the SchematicPort mapped from \b portId.\n
    Returns 0 if \b portId doesn't map no SchematicPort.
*/
SchematicPort *SchematicNode::getPort(int portId) const {
  QMap<int, SchematicPort *>::const_iterator it = m_ports.find(portId);
  if (it != m_ports.end()) return it.value();
  return 0;
}

//--------------------------------------------------------

/*! Returns a list of all node connected by links to a SchematicPort identified
 * by \b portId.
*/
QList<SchematicNode *> SchematicNode::getLinkedNodes(int portId) const {
  QList<SchematicNode *> list;
  SchematicPort *port = getPort(portId);
  if (port) {
    int linkCount = port->getLinkCount();
    int i;
    for (i = 0; i < linkCount; i++) list.push_back(port->getLinkedNode(i));
  }
  return list;
}

//--------------------------------------------------------

void SchematicNode::updateLinksGeometry() {
  QMap<int, SchematicPort *>::iterator it;
  for (it = m_ports.begin(); it != m_ports.end(); ++it)
    it.value()->updateLinksGeometry();
}
