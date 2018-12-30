

#include "toonzqt/schematicgroupeditor.h"
#include "toonzqt/fxschematicnode.h"
#include "toonzqt/fxschematicscene.h"
#include "toonzqt/stageschematicnode.h"
#include "toonzqt/stageschematicscene.h"
#include "toonzqt/schematicnode.h"
#include "toonzqt/gutil.h"
#include "toonz/fxcommand.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/tstageobject.h"
#include "tfxattributes.h"
#include "tmacrofx.h"
#include <QAction>
#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>

//=====================================================
//
// SchematicGroupEditor
//
//=====================================================

SchematicWindowEditor::SchematicWindowEditor(
    const QList<SchematicNode *> &groupedNode, SchematicScene *scene)
#if QT_VERSION >= 0x050000
    : QGraphicsItem()
#else
    : QGraphicsItem(0, scene)
#endif
    , m_groupedNode(groupedNode)
    , m_scene(scene)
    , m_lastPos()
    , m_button(Qt::NoButton)
    , m_isMacroEditor(false) {
#if QT_VERSION >= 0x050000
  scene->addItem(this);
#endif
  m_nameItem = new SchematicName(this, 67, 14);
  m_nameItem->setPos(-2, -2);
  m_nameItem->setZValue(1);
  m_nameItem->hide();
  connect(m_nameItem, SIGNAL(focusOut()), this, SLOT(onNameChanged()));
}

//---------------------------------------------------------------

SchematicWindowEditor::~SchematicWindowEditor() {}

//---------------------------------------------------------------

QRectF SchematicWindowEditor::boundingRect() const {
  QRectF rect = boundingSceneRect();
  rect.moveTopLeft(QPointF(0, 0));
  rect.adjust(-1, -1, 1, 1);
  return rect;
}

//---------------------------------------------------------------

void SchematicWindowEditor::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
  painter->setPen(QColor(0, 0, 0, 255));
  if (m_isMacroEditor)
    painter->setBrush(QColor(132, 86, 123, 255));
  else
    painter->setBrush(QColor(76, 148, 177, 255));
  QRectF bRect = boundingRect();
  QRectF rect(0, 0, bRect.width(), 15);
  painter->drawRect(rect);
  rect = QRectF(0, 15, bRect.width(), bRect.height() - 15);
  painter->setBrush(QColor(180, 180, 180, 125));
  painter->drawRect(rect);

  // draw the topRight cross
  rect = QRectF(0, 0, 11, 11);
  rect.moveTopLeft(QPointF(bRect.width() - 13, 2));
  painter->drawRoundedRect(rect, 2, 2);
  painter->setPen(Qt::black);
  painter->drawLine(QPointF(rect.left() + 2, rect.top() + 2),
                    QPointF(rect.right() - 2, rect.bottom() - 2));
  painter->drawLine(QPointF(rect.left() + 2, rect.bottom() - 2),
                    QPointF(rect.right() - 2, rect.top() + 2));

  if (!m_nameItem->isVisible()) {
    painter->setPen(Qt::white);
    QFont font("Verdana", 7);
    painter->setFont(font);
    QRectF rect        = QRectF(2, 1, bRect.width() - 15, 13);
    QString elidedName = elideText(m_groupName, font, rect.width());
    painter->drawText(rect, elidedName);
  }
}

//---------------------------------------------------------------

void SchematicWindowEditor::resizeNodes(bool maximizeNodes) {
  prepareGeometryChange();
  doResizeNodes(maximizeNodes);
}

//---------------------------------------------------------------

void SchematicWindowEditor::mousePressEvent(QGraphicsSceneMouseEvent *e) {
  QRectF bRect = boundingRect();
  QRectF rect  = QRectF(0, 0, 11, 11);
  rect.moveTopLeft(QPointF(bRect.width() - 13, 2));
  if (rect.contains(e->pos())) {
    closeEditor();
    return;
  }

  rect = QRectF(0, 0, bRect.width(), 15);
  if (rect.contains(e->pos())) {
    m_button  = e->button();
    m_lastPos = e->scenePos();
  }
}

//---------------------------------------------------------------

void SchematicWindowEditor::mouseMoveEvent(QGraphicsSceneMouseEvent *e) {
  if (m_button == Qt::LeftButton) {
    QPointF delta = e->scenePos() - m_lastPos;
    setPos(scenePos() + delta);
    m_lastPos = e->scenePos();
    int i;
    for (i = 0; i < m_groupedNode.size(); i++) {
      SchematicNode *node = m_groupedNode[i];
      node->setPosition(node->scenePos() + delta);
      node->setSchematicNodePos(node->scenePos());
      node->updateLinksGeometry();
    }
  } else
    e->ignore();
}

//---------------------------------------------------------------

void SchematicWindowEditor::mouseReleaseEvent(QGraphicsSceneMouseEvent *e) {
  m_button = Qt::NoButton;
}

//---------------------------------------------------------------

void SchematicWindowEditor::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) {
  QRectF bRect = boundingRect();
  QRectF rect  = QRectF(2, 0, bRect.width() - 15, 15);
  if (rect.contains(e->pos())) {
    m_nameItem->setPlainText(m_groupName);
    m_nameItem->show();
    m_nameItem->setFocus();
    setFlag(QGraphicsItem::ItemIsSelectable, false);
  }
}

//---------------------------------------------------------------

void SchematicWindowEditor::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *e) {
  QRectF bRect = boundingRect();
  QRectF rect(0, 0, bRect.width(), 15);
  if (!rect.contains(e->pos())) {
    e->ignore();
    return;
  }
  QMenu menu(scene()->views()[0]);
  QAction *close = new QAction(tr("&Close Editor"), &menu);
  connect(close, SIGNAL(triggered()), this, SLOT(closeEditor()));

  menu.addAction(close);
  menu.exec(e->screenPos());
}

//=====================================================
//
// FxSchematicGroupEditor
//
//=====================================================

FxSchematicGroupEditor::FxSchematicGroupEditor(
    int groupId, const QList<SchematicNode *> &groupedNode,
    SchematicScene *scene)
    : SchematicWindowEditor(groupedNode, scene), m_groupId(groupId) {
  initializeEditor();
  setPos(boundingSceneRect().topLeft());
  m_nameItem->setName(m_groupName);
}

//---------------------------------------------------------------

FxSchematicGroupEditor::~FxSchematicGroupEditor() {}

//---------------------------------------------------------------

void FxSchematicGroupEditor::initializeEditor() {
  FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(m_groupedNode[0]);
  assert(node);
  TFx *fx = node->getFx();
  assert(fx);
  assert(m_groupId == fx->getAttributes()->getEditingGroupId());
  m_groupName =
      QString::fromStdWString(fx->getAttributes()->getEditingGroupName());
}

//---------------------------------------------------------------

void FxSchematicGroupEditor::closeEditor() {
  // Ptrebbero esserci delle macro aperte per edit nel gruppo...
  // devo chiedere alla scena di chiuderle per me... da qui non posso farlo!
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  assert(fxScene);
  fxScene->closeInnerMacroEditor(m_groupId);
  int i;
  for (i = 0; i < m_groupedNode.size(); i++) {
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(m_groupedNode[i]);
    if (!node) continue;
    FxGroupNode *groupNode = dynamic_cast<FxGroupNode *>(node);
    if (groupNode) {
      QList<TFxP> fxs = groupNode->getGroupedFxs();
      assert(!fxs.isEmpty());
      int j;
      for (j = 0; j < fxs.size(); j++) {
        fxs[j]->getAttributes()->closeEditingGroup(m_groupId);
        TMacroFx *macro = dynamic_cast<TMacroFx *>(fxs[j].getPointer());
        if (macro) {
          std::vector<TFxP> macroFxs = macro->getFxs();
          int j;
          for (j = 0; j < (int)macroFxs.size(); j++)
            macroFxs[j]->getAttributes()->closeEditingGroup(m_groupId);
        }
      }
    } else {
      TFx *fx = node->getFx();
      assert(fx);
      fx->getAttributes()->closeEditingGroup(m_groupId);
      TMacroFx *macro = dynamic_cast<TMacroFx *>(fx);
      if (macro) {
        std::vector<TFxP> macroFxs = macro->getFxs();
        int j;
        for (j = 0; j < (int)macroFxs.size(); j++)
          macroFxs[j]->getAttributes()->closeEditingGroup(m_groupId);
      }
    }
  }
  m_scene->updateScene();
}

//---------------------------------------------------------------

void FxSchematicGroupEditor::onNameChanged() {
  int i;
  QList<TFxP> fxs;
  m_nameItem->hide();
  m_groupName = m_nameItem->toPlainText();
  for (i = 0; i < m_groupedNode.size(); i++) {
    FxGroupNode *groupNode = dynamic_cast<FxGroupNode *>(m_groupedNode[i]);
    FxSchematicNode *node  = dynamic_cast<FxSchematicNode *>(m_groupedNode[i]);
    if (groupNode)
      fxs.append(groupNode->getGroupedFxs());
    else if (node)
      fxs.append(node->getFx());
  }
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFxCommand::renameGroup(fxs.toStdList(), m_groupName.toStdWString(), true,
                          fxScene->getXsheetHandle());
  update();
}

//---------------------------------------------------------------

QRectF FxSchematicGroupEditor::boundingSceneRect() const {
  QRectF rect = m_groupedNode[0]->boundingRect();
  QPointF shiftPos(m_groupedNode[0]->scenePos().x() - rect.left(),
                   m_groupedNode[0]->scenePos().y() + rect.top() + 10);
  rect.moveTopLeft(shiftPos);
  int i;
  for (i = 0; i < m_groupedNode.size(); i++) {
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(m_groupedNode[i]);
    assert(node);
    TFx *fx = node->getFx();
    assert(fx);
    QRectF app = node->boundingRect();
    QPointF shiftAppPos(node->scenePos().x() - app.left(),
                        node->scenePos().y() + app.top() + 10);
    app.moveTopLeft(shiftAppPos);
    bool isASubgroupedNode =
        fx->getAttributes()->getEditingGroupId() != m_groupId;
    if (isASubgroupedNode) {
      QStack<int> idStack = fx->getAttributes()->getGroupIdStack();
      int start  = idStack.indexOf(fx->getAttributes()->getEditingGroupId());
      int k      = idStack.indexOf(m_groupId, start) + 1;
      int factor = k * 30;
      app.adjust(-factor, -factor, factor, factor);
    }
#if QT_VERSION >= 0x050000
    rect = rect.united(app);
#else
    rect = rect.unite(app);
#endif
  }
  rect.adjust(-20, -35, 0, 20);
  return rect;
}

//---------------------------------------------------------------

void FxSchematicGroupEditor::setGroupedNodeZValue(int zValue) {
  int i, size = m_groupedNode.size();
  for (i = 0; i < size; i++) {
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(m_groupedNode[i]);
    if (!node) continue;
    if (node->getFx()->getAttributes()->getEditingGroupId() == m_groupId)
      node->setZValue(zValue);
  }
}

//---------------------------------------------------------------

void FxSchematicGroupEditor::doResizeNodes(bool maximizeNodes) {}

//=====================================================
//
// FxSchematicMacroEditor
//
//=====================================================

FxSchematicMacroEditor::FxSchematicMacroEditor(
    TMacroFx *macro, const QList<SchematicNode *> &groupedNode,
    SchematicScene *scene)
    : SchematicWindowEditor(groupedNode, scene), m_macro(macro) {
  m_isMacroEditor = true;
  initializeEditor();
  setPos(boundingSceneRect().topLeft());
  m_nameItem->setName(m_groupName);
}

//---------------------------------------------------------------

FxSchematicMacroEditor::~FxSchematicMacroEditor() {}

//---------------------------------------------------------------

void FxSchematicMacroEditor::initializeEditor() {
  m_groupName = QString::fromStdWString(m_macro->getName());
}

//---------------------------------------------------------------

void FxSchematicMacroEditor::closeEditor() {
  m_macro->editMacro(false);
  m_scene->updateScene();
}

//---------------------------------------------------------------

void FxSchematicMacroEditor::onNameChanged() {
  QList<TFxP> fxs;
  m_nameItem->hide();
  m_groupName = m_nameItem->toPlainText();
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  FxSchematicScene *fxScene = dynamic_cast<FxSchematicScene *>(scene());
  if (!fxScene) return;
  TFxCommand::renameFx(m_macro, m_groupName.toStdWString(),
                       fxScene->getXsheetHandle());
  update();
}

//---------------------------------------------------------------

QRectF FxSchematicMacroEditor::boundingSceneRect() const {
  QRectF rect = m_groupedNode[0]->boundingRect();
  QPointF shiftPos(m_groupedNode[0]->scenePos().x() - rect.left(),
                   m_groupedNode[0]->scenePos().y() + rect.top() + 10);
  rect.moveTopLeft(shiftPos);
  int i;
  for (i = 0; i < m_groupedNode.size(); i++) {
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(m_groupedNode[i]);
    assert(node);
    TFx *fx = node->getFx();
    assert(fx);
    QRectF app = node->boundingRect();
    QPointF shiftAppPos(node->scenePos().x() - app.left(),
                        node->scenePos().y() + app.top() + 10);
    app.moveTopLeft(shiftAppPos);
#if QT_VERSION >= 0x050000
    rect = rect.united(app);
#else
    rect = rect.unite(app);
#endif
  }
  rect.adjust(-20, -35, 0, 20);
  return rect;
}

//---------------------------------------------------------------

void FxSchematicMacroEditor::setGroupedNodeZValue(int zValue) {
  int i, size = m_groupedNode.size();
  for (i = 0; i < size; i++) {
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(m_groupedNode[i]);
    if (!node) continue;
    node->setZValue(zValue);
  }
}

//---------------------------------------------------------------

void FxSchematicMacroEditor::mouseMoveEvent(QGraphicsSceneMouseEvent *e) {
  QPointF prevPos = pos();
  SchematicWindowEditor::mouseMoveEvent(e);
  if (m_button == Qt::LeftButton) {
    QPointF delta  = pos() - prevPos;
    TFx *root      = m_macro->getRoot();
    TPointD oldPos = m_macro->getAttributes()->getDagNodePos();
    m_macro->getAttributes()->setDagNodePos(oldPos +
                                            TPointD(delta.x(), delta.y()));
  }
}

//---------------------------------------------------------------

void FxSchematicMacroEditor::doResizeNodes(bool maximizeNodes) {
  m_macro->getAttributes()->setIsOpened(maximizeNodes);
}

//=====================================================
//
// StageSchematicGroupEditor
//
//=====================================================

StageSchematicGroupEditor::StageSchematicGroupEditor(
    int groupId, const QList<SchematicNode *> &groupedNode,
    SchematicScene *scene)
    : SchematicWindowEditor(groupedNode, scene), m_groupId(groupId) {
  initializeEditor();
  setPos(boundingSceneRect().topLeft());
  m_nameItem->setName(m_groupName);
}

//---------------------------------------------------------------

StageSchematicGroupEditor::~StageSchematicGroupEditor() {}

//---------------------------------------------------------------

QRectF StageSchematicGroupEditor::boundingSceneRect() const {
  QRectF rect = m_groupedNode[0]->boundingRect();
  QPointF shiftPos(m_groupedNode[0]->scenePos().x() - rect.left(),
                   m_groupedNode[0]->scenePos().y() + rect.top() + 10);
  rect.moveTopLeft(shiftPos);
  int i;
  for (i = 0; i < m_groupedNode.size(); i++) {
    StageSchematicNode *node =
        dynamic_cast<StageSchematicNode *>(m_groupedNode[i]);
    assert(node);
    TStageObject *obj = node->getStageObject();
    assert(obj);
    QRectF app = node->boundingRect();
    QPointF shiftAppPos(node->scenePos().x() - app.left(),
                        node->scenePos().y() + app.top() + 10);
    app.moveTopLeft(shiftAppPos);
    bool isASubgroupedNode = obj->getEditingGroupId() != m_groupId;
    if (isASubgroupedNode) app.adjust(-30, -30, 30, 30);
#if QT_VERSION >= 0x050000
    rect = rect.united(app);
#else
    rect = rect.unite(app);
#endif
  }
  rect.adjust(-20, -35, 0, 0);
  return rect;
}

//---------------------------------------------------------------

void StageSchematicGroupEditor::setGroupedNodeZValue(int zValue) {
  int i, size = m_groupedNode.size();
  for (i = 0; i < size; i++) {
    StageSchematicNode *node =
        dynamic_cast<StageSchematicNode *>(m_groupedNode[i]);
    if (!node) continue;
    if (node->getStageObject()->getEditingGroupId() == m_groupId)
      node->setZValue(zValue);
  }
}

//---------------------------------------------------------------

void StageSchematicGroupEditor::closeEditor() {
  int i;
  for (i = 0; i < m_groupedNode.size(); i++) {
    StageSchematicNode *node =
        dynamic_cast<StageSchematicNode *>(m_groupedNode[i]);
    if (!node) continue;
    StageSchematicGroupNode *groupNode =
        dynamic_cast<StageSchematicGroupNode *>(node);
    if (groupNode) {
      QList<TStageObject *> objs = groupNode->getGroupedObjects();
      assert(!objs.isEmpty());
      int j;
      for (j = 0; j < objs.size(); j++) objs[j]->closeEditingGroup(m_groupId);
    } else {
      TStageObject *obj = node->getStageObject();
      assert(obj);
      obj->closeEditingGroup(m_groupId);
    }
  }
  m_scene->updateScene();
}

//---------------------------------------------------------------

void StageSchematicGroupEditor::onNameChanged() {
  int i;
  QList<TStageObject *> objs;
  m_nameItem->hide();
  m_groupName = m_nameItem->toPlainText();
  for (i = 0; i < m_groupedNode.size(); i++) {
    StageSchematicGroupNode *groupNode =
        dynamic_cast<StageSchematicGroupNode *>(m_groupedNode[i]);
    StageSchematicNode *node =
        dynamic_cast<StageSchematicNode *>(m_groupedNode[i]);
    if (groupNode)
      objs.append(groupNode->getGroupedObjects());
    else if (node)
      objs.append(node->getStageObject());
  }
  setFlag(QGraphicsItem::ItemIsSelectable, true);
  StageSchematicScene *stageScene =
      dynamic_cast<StageSchematicScene *>(scene());
  if (!stageScene) return;
  TStageObjectCmd::renameGroup(objs, m_groupName.toStdWString(), true,
                               stageScene->getXsheetHandle());
  update();
}

//---------------------------------------------------------------

void StageSchematicGroupEditor::initializeEditor() {
  StageSchematicNode *node =
      dynamic_cast<StageSchematicNode *>(m_groupedNode[0]);
  assert(node);
  TStageObject *obj = node->getStageObject();
  assert(obj);
  assert(m_groupId == obj->getEditingGroupId());
  m_groupName = QString::fromStdWString(obj->getEditingGroupName());
}

//---------------------------------------------------------------

void StageSchematicGroupEditor::doResizeNodes(bool maximizeNodes) {
  int i;
  for (i = 0; i < m_groupedNode.size(); i++) {
    StageSchematicNode *node =
        dynamic_cast<StageSchematicNode *>(m_groupedNode[i]);
    assert(node);
    node->getStageObject()->setIsOpened(maximizeNodes);
  }
}
