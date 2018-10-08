

#include "toonzqt/stageschematicscene.h"

// TnzQt includes
#include "toonzqt/stageschematicnode.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/schematicgroupeditor.h"
#include "stageobjectselection.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshcolumn.h"
#include "toonz/toonzscene.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tcamera.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/tproject.h"
#include "toonz/childstack.h"

// TnzCore includes
#include "tconst.h"
#include "tsystem.h"
#include "tstream.h"
#include "tstroke.h"
#include "tenv.h"

// Qt includes
#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QFileDialog>

TEnv::IntVar ShowLetterOnOutputPortOfStageNode(
    "ShowLetterOnOutputPortOfStageNode", 0);

namespace {

//! Define the method to sort a TreeStageNode container.
//! The sorting follows these rules:
//! \li TableNode < CameraNode < PegbarNode < ColumnNode
//! \li if two nodes are of the same type, they are odered using indexes
class CompareNodes {
public:
  bool operator()(TreeStageNode *node1, TreeStageNode *node2) {
    TStageObjectId id1 = node1->getNode()->getStageObject()->getId();
    TStageObjectId id2 = node2->getNode()->getStageObject()->getId();

    if (id1.isTable() ||
        (id1.isCamera() && !id2.isTable() && !id2.isCamera()) ||
        (id1.isPegbar() && !id2.isTable() && !id2.isCamera() &&
         !id2.isPegbar()))
      return true;
    if ((id1.isCamera() && id2.isCamera()) ||
        (id1.isTable() && id2.isTable()) ||
        (id1.isPegbar() && id2.isPegbar()) ||
        (id1.isColumn() && id2.isColumn()))
      return id1.getIndex() < id2.getIndex();
    return false;
  }
};

//------------------------------------------------------------------

TStageObject *getRoot(const QList<TStageObject *> &objs,
                      TStageObjectTree *pegTree) {
  int i;
  for (i = 0; i < objs.size(); i++) {
    TStageObject *parent = pegTree->getStageObject(objs[i]->getParent(), false);
    if (objs.contains(parent))
      continue;
    else
      return objs[i];
  }
  return 0;
}

//------------------------------------------------------------------

void keepSubgroup(QMap<int, QList<SchematicNode *>> &editedGroup) {
  QMap<int, QList<SchematicNode *>>::iterator it;
  for (it = editedGroup.begin(); it != editedGroup.end(); it++) {
    QList<SchematicNode *> groupedNodes = it.value();
    int i;
    for (i = 0; i < groupedNodes.size(); i++) {
      StageSchematicNode *node =
          dynamic_cast<StageSchematicNode *>(groupedNodes[i]);
      if (!node) continue;
      TStageObject *obj = node->getStageObject();
      assert(obj);
      QMap<int, QList<SchematicNode *>>::iterator it2;
      for (it2 = editedGroup.begin(); it2 != editedGroup.end(); it2++) {
        if (obj->isContainedInGroup(it2.key()) &&
            !editedGroup[it2.key()].contains(node))
          editedGroup[it2.key()].append(node);
      }
    }
  }
}

}  // namespace

//==================================================================
//
// TreeStageNode
//
//==================================================================

void TreeStageNode::sortChildren(int startIndex, int lastIndex) {
  if (startIndex == lastIndex) return;
  std::vector<TreeStageNode *>::iterator begin, end;
  begin = m_cildren.begin() + startIndex;
  end   = m_cildren.begin() + lastIndex;
  std::sort(begin, end, CompareNodes());
}

//==================================================================
//
// StageSchematicScene
//
//==================================================================

StageSchematicScene::StageSchematicScene(QWidget *parent)
    : SchematicScene(parent)
    , m_nextNodePos(0, 0)
    , m_xshHandle(0)
    , m_objHandle(0)
    , m_colHandle(0)
    , m_sceneHandle(0)
    , m_frameHandle(0)
    , m_gridDimension(eSmall)
    , m_showLetterOnPortFlag(ShowLetterOnOutputPortOfStageNode != 0)
    , m_viewer() {
  m_viewer = (SchematicViewer *)parent;

  QPointF sceneCenter = sceneRect().center();
  m_firstPos          = TPointD(sceneCenter.x(), sceneCenter.y());

  m_selection = new StageObjectSelection();
  connect(m_selection, SIGNAL(doCollapse(QList<TStageObjectId>)), this,
          SLOT(onCollapse(QList<TStageObjectId>)));
  connect(m_selection, SIGNAL(doExplodeChild(QList<TStageObjectId>)), this,
          SIGNAL(doExplodeChild(QList<TStageObjectId>)));
  connect(this, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));
  m_highlightedLinks.clear();
}

//------------------------------------------------------------------

StageSchematicScene::~StageSchematicScene() {}

//------------------------------------------------------------------

void StageSchematicScene::setXsheetHandle(TXsheetHandle *xshHandle) {
  m_xshHandle = xshHandle;
  m_selection->setXsheetHandle(xshHandle);
}

//------------------------------------------------------------------

void StageSchematicScene::setObjectHandle(TObjectHandle *objHandle) {
  m_objHandle = objHandle;
  m_selection->setObjectHandle(objHandle);
}

//------------------------------------------------------------------

void StageSchematicScene::setColumnHandle(TColumnHandle *colHandle) {
  m_colHandle = colHandle;
  m_selection->setColumnHandle(colHandle);
}

//------------------------------------------------------------------

void StageSchematicScene::setFxHandle(TFxHandle *fxHandle) {
  m_selection->setFxHandle(fxHandle);
}

//------------------------------------------------------------------

void StageSchematicScene::onSelectionSwitched(TSelection *oldSel,
                                              TSelection *newSel) {
  if (m_selection == oldSel && m_selection != newSel) clearSelection();
}

//------------------------------------------------------------------

void StageSchematicScene::updateScene() {
  clearAllItems();

  QPointF firstPos = sceneRect().center();
  m_nextNodePos    = TPointD(firstPos.x(), firstPos.y());

  TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();

  m_nodeTable.clear();
  m_GroupTable.clear();
  m_groupEditorTable.clear();
  m_gridDimension = pegTree->getDagGridDimension();

  QMap<int, QList<TStageObject *>> groupedObj;
  QMap<int, QList<SchematicNode *>> editedGroup;
  // in order to draw the position-specified nodes first
  QList<int> modifiedNodeIds;
  for (int i = 0; i < pegTree->getStageObjectCount(); i++) {
    TStageObject *pegbar = pegTree->getStageObject(i);
    if (pegbar->getDagNodePos() == TConst::nowhere)
      modifiedNodeIds.push_back(i);
    else
      modifiedNodeIds.push_front(i);
  }

  for (int i = 0; i < modifiedNodeIds.size(); i++) {
    TStageObject *pegbar = pegTree->getStageObject(modifiedNodeIds.at(i));
    if (pegbar->isGrouped() && !pegbar->isGroupEditing()) {
      groupedObj[pegbar->getGroupId()].push_back(pegbar);
      continue;
    }
    StageSchematicNode *node = addStageSchematicNode(pegbar);
    if (node != 0) {
      m_nodeTable[pegbar->getId()] = node;
      if (pegbar->isGrouped())
        editedGroup[pegbar->getEditingGroupId()].append(node);
    }
  }

  // Motion Path
  m_splineTable.clear();
  for (int i = 0; i < pegTree->getSplineCount(); i++) {
    TStageObjectSpline *spline     = pegTree->getSpline(i);
    StageSchematicSplineNode *node = addSchematicSplineNode(spline);
    if (node != 0) {
      m_splineTable[spline] = node;
      connect(node, SIGNAL(currentObjectChanged(const TStageObjectId &, bool)),
              this, SLOT(onCurrentObjectChanged(const TStageObjectId &, bool)));
    }
  }

  QMap<int, QList<TStageObject *>>::iterator it;
  for (it = groupedObj.begin(); it != groupedObj.end(); it++) {
    StageSchematicGroupNode *node = addStageGroupNode(it.value());
    m_GroupTable[it.key()]        = node;
    int editingGroupId            = node->getStageObject()->getEditingGroupId();
    if (editingGroupId != -1) editedGroup[editingGroupId].append(node);
  }

  keepSubgroup(editedGroup);
  updateEditedGroups(editedGroup);

  // update stage links
  for (int i = 0; i < pegTree->getStageObjectCount(); i++)

  {
    TStageObject *pegbar    = pegTree->getStageObject(i);
    TStageObjectId parentId = pegbar->getParent();
    // update link; find StageSchematicNode representing pegbar
    QMap<TStageObjectId, StageSchematicNode *>::iterator it;

    if (!pegbar->isGrouped() || pegbar->isGroupEditing()) {
      it = m_nodeTable.find(pegbar->getId());

      //! note: not every stage object is represented in the scene (e.g. empty
      //! columns are not)
      if (it == m_nodeTable.end()) continue;
      StageSchematicNode *node = it.value();
      // get pegbar parent
      if (parentId != TStageObjectId::NoneId) {
        // find StageSchematicNode representing parentId
        it = m_nodeTable.find(parentId);
        if (it != m_nodeTable.end())  // at least we avoid crash
        {
          StageSchematicNode *parentNode = it.value();
          if (parentNode) {
            StageSchematicNodePort *port1 = parentNode->makeChildPort(
                QString::fromStdString(pegbar->getParentHandle()));
            StageSchematicNodePort *port2 = node->makeParentPort(
                QString::fromStdString(pegbar->getHandle()));
            port2->makeLink(port1);
          }
        }
        TStageObject *parent = pegTree->getStageObject(parentId, false);
        if (parent && parent->isGrouped()) {
          int groupId = parent->getGroupId();
          if (m_GroupTable.contains(groupId)) {
            StageSchematicGroupNode *groupeNode = m_GroupTable[groupId];
            StageSchematicNodePort *port1       = groupeNode->getChildPort(0);
            StageSchematicNodePort *port2       = node->makeParentPort(
                QString::fromStdString(pegbar->getHandle()));
            port2->makeLink(port1);
          }
        }
      }

      // link to motion path
      TStageObjectSpline *spline = pegbar->getSpline();
      if (spline) {
        QMap<TStageObjectSpline *, StageSchematicSplineNode *>::iterator
            splineIt;
        splineIt = m_splineTable.find(spline);
        if (splineIt != m_splineTable.end()) {
          SchematicPort *port1 = splineIt.value()->getPort(-1);
          SchematicPort *port2 = node->getPort(-1);
          port2->makeLink(port1);
        }
      }
    } else {
      int groupId                         = pegbar->getGroupId();
      StageSchematicGroupNode *groupeNode = m_GroupTable[groupId];
      TStageObject *parent = pegTree->getStageObject(parentId, false);
      if (parent && (!parent->isGrouped() || parent->isGroupEditing())) {
        it = m_nodeTable.find(parentId);
        if (it == m_nodeTable.end()) continue;
        StageSchematicNode *parentNode = it.value();
        if (parentNode) {
          StageSchematicNodePort *port1 = parentNode->makeChildPort(
              QString::fromStdString(pegbar->getParentHandle()));
          StageSchematicNodePort *port2 = groupeNode->getParentPort();
          port2->makeLink(port1);
        }
      }
      if (parent && parent->isGrouped()) {
        int parentGroupId = parent->getGroupId();
        if (m_GroupTable.contains(parentGroupId)) {
          StageSchematicGroupNode *groupeParentNode =
              m_GroupTable[parentGroupId];
          StageSchematicNodePort *port1 = groupeParentNode->getChildPort(0);
          StageSchematicNodePort *port2 = groupeNode->getParentPort();
          if (!port1->isLinkedTo(port2) && groupeParentNode != groupeNode)
            port2->makeLink(port1);
        }
      }
      TStageObjectSpline *spline = pegbar->getSpline();
      if (spline) {
        QMap<TStageObjectSpline *, StageSchematicSplineNode *>::iterator
            splineIt;
        splineIt = m_splineTable.find(spline);
        if (splineIt != m_splineTable.end()) {
          SchematicPort *port1 = splineIt.value()->getPort(-1);
          SchematicPort *port2 = groupeNode->getPort(-1);
          port2->makeLink(port1);
        }
      }
    }
  }
  m_nodesToPlace.clear();
}

//------------------------------------------------------------------

StageSchematicNode *StageSchematicScene::addStageSchematicNode(
    TStageObject *pegbar) {
  // create the node according to the type of StageObject
  StageSchematicNode *node = createStageSchematicNode(this, pegbar);
  if (!node) return 0;
  connect(node, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));
  connect(node, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
  connect(node, SIGNAL(currentObjectChanged(const TStageObjectId &, bool)),
          this, SLOT(onCurrentObjectChanged(const TStageObjectId &, bool)));
  connect(node, SIGNAL(currentColumnChanged(int)), this,
          SLOT(onCurrentColumnChanged(int)));
  connect(node, SIGNAL(editObject()), this, SIGNAL(editObject()));

  // specify the node position
  if (pegbar->getDagNodePos() == TConst::nowhere) {
    if (pegbar->getId().isColumn()) {
      StageSchematicColumnNode *cNode =
          dynamic_cast<StageSchematicColumnNode *>(node);
      assert(node);
      cNode->resize(m_gridDimension == 0);
    }
    placeNode(node);
  } else
    updatePosition(node, pegbar->getDagNodePos());
  return node;
}

//------------------------------------------------------------------

StageSchematicGroupNode *StageSchematicScene::addStageGroupNode(
    QList<TStageObject *> objs) {
  if (objs.isEmpty()) return 0;
  TStageObjectTree *objTree = m_xshHandle->getXsheet()->getStageObjectTree();
  TStageObject *root        = getRoot(objs, objTree);
  StageSchematicGroupNode *node = new StageSchematicGroupNode(this, root, objs);
  if (!node) return 0;
  connect(node, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));
  connect(node, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
  connect(node, SIGNAL(currentObjectChanged(const TStageObjectId &, bool)),
          this, SLOT(onCurrentObjectChanged(const TStageObjectId &, bool)));
  connect(node, SIGNAL(currentColumnChanged(int)), this,
          SLOT(onCurrentColumnChanged(int)));
  connect(node, SIGNAL(editObject()), this, SIGNAL(editObject()));
  if (root->getDagNodePos() == TConst::nowhere) {
    placeNode(node);
  } else
    updatePosition(node, root->getDagNodePos());
  return node;
}

//------------------------------------------------------------------

StageSchematicGroupEditor *
StageSchematicScene::addEditedGroupedStageSchematicNode(
    int groupId, const QList<SchematicNode *> &groupedObjs) {
  StageSchematicGroupEditor *editorGroup =
      new StageSchematicGroupEditor(groupId, groupedObjs, this);
  m_groupEditorTable[groupId] = editorGroup;
  return editorGroup;
}

//------------------------------------------------------------------

void StageSchematicScene::updatePosition(StageSchematicNode *node,
                                         const TPointD &pos) {
  node->setPos(QPointF(pos.x, pos.y));
  node->getStageObject()->setDagNodePos(pos);
  // this operation may abort
  /*
QVector<SchematicNode*> placedNodes = getPlacedNode(node);
TPointD offset(15,15);
int i;
for(i=0; i<placedNodes.size(); i++)
{
StageSchematicNode *placedNode =
dynamic_cast<StageSchematicNode*>(placedNodes[i]);
assert(placedNode);
TPointD newPos = placedNode->getStageObject()->getDagNodePos()+offset;
updatePosition(placedNode,newPos);
}
*/
}

//------------------------------------------------------------------

void StageSchematicScene::highlightLinks(StageSchematicNode *node, bool value) {
  int i, portCount = node->getChildCount();
  for (i = 0; i < portCount; i++) {
    StageSchematicNodePort *port = node->getChildPort(i);
    int j, linkCount = port->getLinkCount();
    for (j = 0; j < linkCount; j++) {
      SchematicLink *link = port->getLink(j);
      if (!link) continue;
      link->setHighlighted(value);
      link->update();
      m_highlightedLinks.push_back(link);
    }
  }

  StageSchematicNodePort *port = node->getParentPort();
  if (port) {
    int linkCount = port->getLinkCount();
    for (i = 0; i < linkCount; i++) {
      SchematicLink *link = port->getLink(i);
      if (link) {
        link->setHighlighted(value);
        link->update();
        m_highlightedLinks.push_back(link);
      }
    }
  }
}

//------------------------------------------------------------------

void StageSchematicScene::updateEditedGroups(
    const QMap<int, QList<SchematicNode *>> &editedGroup) {
  QMap<int, QList<SchematicNode *>>::const_iterator it;
  for (it = editedGroup.begin(); it != editedGroup.end(); it++) {
    int zValue = 2;
    QMap<int, QList<SchematicNode *>>::const_iterator it2 = editedGroup.begin();
    while (it2 != editedGroup.end()) {
      StageSchematicNode *placedObj =
          dynamic_cast<StageSchematicNode *>(it2.value()[0]);
      StageSchematicNode *obj =
          dynamic_cast<StageSchematicNode *>(it.value()[0]);
      if (!placedObj || !obj) {
        it2++;
        continue;
      }
      int placedGroupedId = placedObj->getStageObject()->getEditingGroupId();
      if (obj->getStageObject()->isContainedInGroup(placedGroupedId) &&
          obj->getStageObject()->getEditingGroupId() != it2.key())
        zValue += 2;
      it2++;
    }
    StageSchematicGroupEditor *node =
        addEditedGroupedStageSchematicNode(it.key(), it.value());
    node->setZValue(zValue);
    node->setGroupedNodeZValue(zValue + 1);
  }
}

//------------------------------------------------------------------

void StageSchematicScene::updateNestedGroupEditors(StageSchematicNode *node,
                                                   const QPointF &newPos) {
  if (!node) return;
  QStack<int> groupIdStack = node->getStageObject()->getGroupIdStack();
  if (groupIdStack.isEmpty()) return;
  int i;
  QRectF rect;
  for (i = 0; i < groupIdStack.size(); i++) {
    if (m_groupEditorTable.contains(groupIdStack[i])) {
      QRectF app = m_groupEditorTable[groupIdStack[i]]->sceneBoundingRect();
      if (rect.isEmpty())
        rect = app;
      else
#if QT_VERSION >= 0x050000
        rect = rect.united(app);
#else
        rect = rect.unite(app);
#endif
    }
  }
  node->setPos(newPos);
  for (i = 0; i < groupIdStack.size(); i++) {
    if (!m_groupEditorTable.contains(groupIdStack[i])) continue;
#if QT_VERSION >= 0x050000
    rect =
        rect.united(m_groupEditorTable[groupIdStack[i]]->sceneBoundingRect());
#else
    rect = rect.unite(m_groupEditorTable[groupIdStack[i]]->sceneBoundingRect());
#endif
    QRectF app = m_groupEditorTable[groupIdStack[i]]->boundingSceneRect();
    if (m_groupEditorTable[groupIdStack[i]]->scenePos() != app.topLeft())
      m_groupEditorTable[groupIdStack[i]]->setPos(app.topLeft());
  }
  update(rect);
}

//------------------------------------------------------------------

void StageSchematicScene::resizeNodes(bool maximizedNode) {
  m_gridDimension           = maximizedNode ? eLarge : eSmall;
  TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
  pegTree->setDagGridDimension(m_gridDimension);
  int i, objCount = pegTree->getStageObjectCount();
  for (i = 0; i < objCount; i++) {
    TStageObject *obj = pegTree->getStageObject(i);
    if (obj && obj->getId().isColumn()) {
      if (!m_nodeTable.contains(obj->getId())) continue;
      StageSchematicColumnNode *node =
          dynamic_cast<StageSchematicColumnNode *>(m_nodeTable[obj->getId()]);
      assert(node);
      node->resize(maximizedNode);
    }
    if (obj && !obj->getId().isCamera())
      updatePositionOnResize(obj, maximizedNode);
  }
  int splineCount = pegTree->getSplineCount();
  for (i = 0; i < splineCount; i++) {
    TStageObjectSpline *spl = pegTree->getSpline(i);
    if (spl) {
      if (!m_splineTable.contains(spl)) continue;
      StageSchematicSplineNode *node =
          dynamic_cast<StageSchematicSplineNode *>(m_splineTable[spl]);
      assert(node);
      node->resize(maximizedNode);
      updateSplinePositionOnResize(spl, maximizedNode);
    }
  }
  QMap<int, StageSchematicGroupNode *>::iterator it;
  for (it = m_GroupTable.begin(); it != m_GroupTable.end(); it++) {
    it.value()->resize(maximizedNode);
    QList<TStageObject *> groupedObjs = it.value()->getGroupedObjects();
    for (int i = 0; i < groupedObjs.size(); i++)
      updatePositionOnResize(groupedObjs[i], maximizedNode);
  }
  QMap<int, StageSchematicGroupEditor *>::iterator it2;
  for (it2 = m_groupEditorTable.begin(); it2 != m_groupEditorTable.end(); it2++)
    it2.value()->resizeNodes(maximizedNode);
  updateScene();
}

//------------------------------------------------------------------

void StageSchematicScene::updatePositionOnResize(TStageObject *obj,
                                                 bool maximizedNode) {
  TPointD oldPos = obj->getDagNodePos();
  double oldPosY = oldPos.y - 25500;
  double newPosY = maximizedNode ? oldPosY * 2 : oldPosY * 0.5;
  obj->setDagNodePos(TPointD(oldPos.x, newPosY + 25500));
}

//------------------------------------------------------------------

void StageSchematicScene::updateSplinePositionOnResize(TStageObjectSpline *spl,
                                                       bool maximizedNode) {
  TPointD oldPos = spl->getDagNodePos();
  double oldPosY = oldPos.y - 25500;
  double newPosY = maximizedNode ? oldPosY * 2 : oldPosY * 0.5;
  spl->setDagNodePos(TPointD(oldPos.x, newPosY + 25500));
}

//------------------------------------------------------------------

StageSchematicSplineNode *StageSchematicScene::addSchematicSplineNode(
    TStageObjectSpline *spline) {
  StageSchematicSplineNode *node = new StageSchematicSplineNode(this, spline);
  connect(node, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));
  connect(node, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
  if (!node) return 0;
  if (spline->getDagNodePos() == TConst::nowhere) {
    node->resize(m_gridDimension == 0);
    placeSplineNode(node);
  } else
    node->setPos(QPointF(spline->getDagNodePos().x, spline->getDagNodePos().y));
  return node;
}

//------------------------------------------------------------------
/*! create node according to the type of StageObject
*/
StageSchematicNode *StageSchematicScene::createStageSchematicNode(
    StageSchematicScene *scene, TStageObject *pegbar) {
  TStageObjectId id = pegbar->getId();
  if (id.isColumn()) {
    int columnIndex = id.getIndex();
    if (m_xshHandle->getXsheet()->isColumnEmpty(columnIndex)) {
      return 0;
    } else {
      TXshColumn *column = m_xshHandle->getXsheet()->getColumn(columnIndex);
      if (!column || column->getSoundColumn() || column->getSoundTextColumn())
        return 0;
    }
  }
  if (!pegbar || !scene) return 0;
  if (id.isTable())
    return new StageSchematicTableNode(scene, pegbar);
  else if (id.isCamera())
    return new StageSchematicCameraNode(scene, pegbar);
  else if (id.isPegbar())
    return new StageSchematicPegbarNode(scene, pegbar);
  else if (id.isColumn())
    return new StageSchematicColumnNode(scene, pegbar);
  else
    return 0;
}

//------------------------------------------------------------------

QGraphicsItem *StageSchematicScene::getCurrentNode() {
  QList<QGraphicsItem *> allItems = items();

  for (auto const item : allItems) {
    StageSchematicNode *node = dynamic_cast<StageSchematicNode *>(item);
    if (node && node->getStageObject()->getId() == m_objHandle->getObjectId())
      return node;
  }
  return 0;
}

//------------------------------------------------------------------

void StageSchematicScene::reorderScene() { placeNodes(); }

//------------------------------------------------------------------

void StageSchematicScene::placeNodes() {
  // search all possible roots
  std::vector<TreeStageNode *> roots;
  findRoots(roots);

  // sorts the roots container. Sortijg rules are specified by CompareNodes
  // class
  std::sort(roots.begin(), roots.end(), CompareNodes());

  double xFirstPos = m_firstPos.x - 500;
  double yFirstPos = m_firstPos.y + 500;
  double xPos      = xFirstPos;
  double yPos      = yFirstPos;
  double maxXPos;
  double maxYPos = yFirstPos;
  int step       = m_gridDimension == eLarge ? 100 : 50;

  // places the first roots and its children. The first root is always a table!
  assert(roots[0]->getNode()->getStageObject()->getId().isTable());
  roots[0]->getNode()->getStageObject()->setDagNodePos(
      TPointD(xFirstPos, yFirstPos));
  placeChildren(roots[0], xPos, yPos);
  maxXPos = xPos;

  int i;
  // places other roots and root's children.
  for (i = 1; i < (int)roots.size(); i++) {
    TStageObject *pegbar = roots[i]->getNode()->getStageObject();
    xPos                 = xFirstPos;
    yPos                 = maxYPos + (pegbar->getId().isCamera() ? 100 : step);
    pegbar->setDagNodePos(TPointD(xPos, yPos));
    placeChildren(roots[i], xPos, yPos);
    maxXPos = std::max(xPos, maxXPos);
    maxYPos = std::max(yPos, maxYPos);
  }

  // places all spline nodes.
  TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
  for (i = 0; i < pegTree->getSplineCount(); i++) {
    TStageObjectSpline *spline = pegTree->getSpline(i);
    spline->setDagNodePos(TPointD(maxXPos, yFirstPos + step));
    maxXPos += (m_showLetterOnPortFlag) ? 150 : 120;
  }

  // delete the tree
  for (i = 0; i < (int)roots.size(); i++) delete roots[i];
  roots.clear();

  updateScene();
}

//------------------------------------------------------------------

void StageSchematicScene::findRoots(std::vector<TreeStageNode *> &roots) {
  TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
  int i;
  for (i = 0; i < pegTree->getStageObjectCount(); i++) {
    TStageObject *pegbar = pegTree->getStageObject(i);
    // only cameras and pegbars can be roots
    if (pegbar->getId().isCamera() || pegbar->getId().isTable()) {
      StageSchematicNode *node = m_nodeTable[pegbar->getId()];
      TreeStageNode *treeNode  = new TreeStageNode(node);
      roots.push_back(treeNode);
      // when a root is found, the tree is build.
      makeTree(treeNode);
    }
  }
}

//------------------------------------------------------------------

void StageSchematicScene::makeTree(TreeStageNode *treeNode) {
  int i, portCount = treeNode->getNode()->getChildCount();
  for (i = 0; i < portCount; i++) {
    StageSchematicNodePort *port = treeNode->getNode()->getChildPort(i);
    int firstChild               = treeNode->getChildrenCount();
    int j, linkCount = port->getLinkCount();
    for (j = 0; j < linkCount; j++) {
      // the linked node is taken and the method is iterated for this node
      StageSchematicNode *schematicNode =
          dynamic_cast<StageSchematicNode *>(port->getLinkedNode(j));
      TreeStageNode *treeChild = new TreeStageNode(schematicNode);
      treeNode->addChild(treeChild);
      makeTree(treeChild);
    }
    int lastChild = treeNode->getChildrenCount();
    treeNode->sortChildren(firstChild, lastChild);
  }
}

//------------------------------------------------------------------

void StageSchematicScene::placeChildren(TreeStageNode *treeNode, double &xPos,
                                        double &yPos, bool isCameraTree) {
  int i;
  xPos += (m_showLetterOnPortFlag) ? 150 : 120;
  double xChildPos = xPos;
  double xRefPos   = xPos;
  bool firstChild  = true;
  bool startFromCamera =
      isCameraTree || treeNode->getNode()->getStageObject()->getId().isCamera();
  int step       = m_gridDimension == eLarge ? 100 : 50;
  double yOffset = startFromCamera ? step : -step;
  if (startFromCamera) treeNode->reverseChildren();
  for (i = 0; i < treeNode->getChildrenCount(); i++) {
    TreeStageNode *childNode  = treeNode->getChildTreeNode(i);
    TStageObject *childPegbar = childNode->getNode()->getStageObject();
    if (childPegbar->getId().isCamera()) continue;
    xChildPos = xRefPos;
    // first child must have same yPos of father.
    yPos += firstChild ? 0 : yOffset;
    firstChild = false;
    childPegbar->setDagNodePos(TPointD(xChildPos, yPos));
    placeChildren(childNode, xChildPos, yPos, startFromCamera);
    xPos = std::max(xPos, xChildPos);
  }
}

//------------------------------------------------------------------
/*! define the position of nodes which is not specified manually
*/
void StageSchematicScene::placeNode(StageSchematicNode *node) {
  double xFirstPos = m_firstPos.x - 500;
  double yFirstPos = m_firstPos.y + 500;
  double xPos      = xFirstPos;
  double yPos      = yFirstPos;
  int step         = m_gridDimension == eLarge ? 100 : 50;
  int hStep        = (m_showLetterOnPortFlag) ? 150 : 120;

  TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
  QRectF nodeRect           = node->boundingRect();
  TStageObject *pegbar      = node->getStageObject();

  TStageObjectId parentObjId = pegbar->getParent();
  TStageObject *parentObj    = pegTree->getStageObject(parentObjId, false);

  if (pegbar->getId().isCamera())
    yPos = yFirstPos + step;
  else if (pegbar->getId().isPegbar()) {
    if (parentObj) {
      if (parentObj->getDagNodePos() != TConst::nowhere) {
        TPointD pos = parentObj->getDagNodePos();
        yPos        = pos.y;
        xPos        = pos.x + hStep;
      } else {
        m_nodesToPlace[parentObjId].append(node);
        return;
      }

    } else {
      yPos = yFirstPos;
      xPos = xFirstPos + hStep;
    }
  } else if (pegbar->getId().isColumn()) {
    if (parentObj) {
      if (parentObj->getDagNodePos() != TConst::nowhere) {
        TPointD pos = parentObj->getDagNodePos();
        yPos        = pos.y;
        xPos        = pos.x + hStep;
      } else {
        m_nodesToPlace[parentObjId].append(node);
        return;
      }
    } else {
      yPos = yFirstPos;
      xPos = xFirstPos + (hStep * 2);
    }
  }
  QPointF initPos(xPos, yPos);
  nodeRect.moveTopLeft(initPos);

  bool found = false;
  // try to place inside of the visible area
  if (!views().isEmpty() && pegbar->getId().isPegbar()) {
    QGraphicsView *view = views().at(0);
    QRectF visibleRect =
        view->mapToScene(0, 0, view->width(), view->height()).boundingRect();
    while (visibleRect.left() > nodeRect.left()) {
      nodeRect.translate(hStep, 0);
      xPos += hStep;
    }
    while (visibleRect.bottom() < nodeRect.bottom()) {
      nodeRect.translate(0, -step);
      yPos -= step;
    }
    QPointF tmpPos(xPos, yPos);
    while (visibleRect.right() > nodeRect.right()) {
      while (!isAnEmptyZone(nodeRect)) {
        nodeRect.translate(0, -step);
        yPos -= step;
      }
      // if failed to place this column, then move the candidate position to
      // right
      if (visibleRect.contains(nodeRect)) {
        found = true;
        break;
      } else {
        xPos += hStep;
        yPos = tmpPos.y();
        nodeRect.moveTopLeft(QPointF(xPos, yPos));
      }
    }
  }
  // place the node outside of the visible area
  if (!found) {
    nodeRect.moveTopLeft(initPos);
    xPos = initPos.x();
    yPos = initPos.y();
    nodeRect.moveTopLeft(initPos);

    while (!isAnEmptyZone(nodeRect)) {
      if (pegbar->getId().isCamera()) {
        nodeRect.translate(0, step);
        yPos += step;
      } else if (pegbar->getId().isColumn() || pegbar->getId().isPegbar()) {
        nodeRect.translate(0, -step);
        yPos -= step;
      }
    }
  }
  pegbar->setDagNodePos(TPointD(xPos, yPos));
  node->setPos(xPos, yPos);
  if (m_nodesToPlace.contains(pegbar->getId())) {
    QList<StageSchematicNode *> nodes = m_nodesToPlace[pegbar->getId()];
    int i;
    for (i = 0; i < nodes.size(); i++) placeNode(nodes[i]);
  }
}

//------------------------------------------------------------------

void StageSchematicScene::placeSplineNode(
    StageSchematicSplineNode *splineNode) {
  double xFirstPos           = m_firstPos.x - 500;
  double yFirstPos           = m_firstPos.y + 500;
  int hStep                  = (m_showLetterOnPortFlag) ? 150 : 120;
  double xPos                = xFirstPos + (hStep * 2);
  int step                   = m_gridDimension == eLarge ? 100 : 50;
  double yPos                = yFirstPos + step;
  QRectF nodeRect            = splineNode->boundingRect();
  TStageObjectSpline *spline = splineNode->getSpline();
  nodeRect.translate(QPointF(xPos, yPos));

  while (!isAnEmptyZone(nodeRect)) {
    nodeRect.translate(hStep, 0);
    xPos += hStep;
  }
  spline->setDagNodePos(TPointD(xPos, yPos));
  splineNode->setPos(xPos, yPos);
}

//------------------------------------------------------------------

void StageSchematicScene::onPegbarAdded() {
  // if this command called from the context menu, place the node at the
  // cllicked position
  // clickedPos will be null(0,0) if it called from the toolbar
  QPointF clickedPos = qobject_cast<QAction *>(sender())->data().toPointF();

  TStageObjectCmd::addNewPegbar(m_xshHandle, m_objHandle, clickedPos);
}

//------------------------------------------------------------------

void StageSchematicScene::onSplineAdded() {
  // if this command called from the context menu, place the node at the
  // cllicked position
  // clickedPos will be null(0,0) if it called from the toolbar
  QPointF clickedPos = qobject_cast<QAction *>(sender())->data().toPointF();

  TStageObjectCmd::addNewSpline(m_xshHandle, m_objHandle, m_colHandle,
                                clickedPos);
}

//------------------------------------------------------------------

void StageSchematicScene::onCameraAdded() {
  // if this command called from the context menu, place the node at the
  // cllicked position
  // clickedPos will be null(0,0) if it called from the toolbar
  QPointF clickedPos = qobject_cast<QAction *>(sender())->data().toPointF();

  TStageObjectCmd::addNewCamera(m_xshHandle, m_objHandle, clickedPos);
}

//------------------------------------------------------------------

void StageSchematicScene::onSwitchPortModeToggled(bool withLetter) {
  m_showLetterOnPortFlag            = withLetter;
  ShowLetterOnOutputPortOfStageNode = (withLetter) ? 1 : 0;
  updateScene();
}

//------------------------------------------------------------------

void StageSchematicScene::onResetCenter() {
  TStageObjectId id = m_objHandle->getObjectId();
  TStageObjectCmd::resetCenterAndOffset(id, m_xshHandle);
}

//------------------------------------------------------------------

void StageSchematicScene::onCameraActivate() {
  TStageObjectCmd::setAsActiveCamera(m_xshHandle, m_objHandle);
}

//------------------------------------------------------------------

void StageSchematicScene::onRemoveSpline() { m_selection->deleteSelection(); }

//------------------------------------------------------------------

void StageSchematicScene::onSaveSpline() {
  TFilePath projectFolder =
      m_sceneHandle->getScene()->getProject()->getProjectFolder();
  QString fileNameStr = QFileDialog::getSaveFileName(
      this->views()[0], QObject::tr("Save Motion Path"),
      QString::fromStdWString(projectFolder.getWideString()),
      QObject::tr("Motion Path files (*.mpath)"));
  if (fileNameStr == "") return;
  TFilePath fp(fileNameStr.toStdWString());
  if (fp.getType() == "") fp = fp.withType("mpath");
  try {
    assert(m_objHandle->isSpline());
    TStageObjectId id        = m_objHandle->getObjectId();
    TXsheet *xsh             = m_xshHandle->getXsheet();
    TStageObject *currentObj = xsh->getStageObjectTree()->getStageObject(id);

    if (currentObj == 0) throw "no currentObj";
    TStageObjectSpline *spline = currentObj->getSpline();
    if (spline == 0) throw "no spline";
    TOStream os(fp);

    // Only points are saved
    const TStroke *stroke = spline->getStroke();
    int n                 = stroke ? stroke->getControlPointCount() : 0;
    for (int i = 0; i < n; i++) {
      TThickPoint p = stroke->getControlPoint(i);
      os << p.x << p.y << p.thick;
    }
  } catch (...) {
    DVGui::warning(QObject::tr("It is not possible to save the motion path."));
  }
}

//------------------------------------------------------------------

void StageSchematicScene::onLoadSpline() {
  TFilePath projectFolder =
      m_sceneHandle->getScene()->getProject()->getProjectFolder();
  QString fileNameStr = QFileDialog::getOpenFileName(
      this->views()[0], QObject::tr("Load Motion Path"),
      QString::fromStdWString(projectFolder.getWideString()),
      QObject::tr("Motion Path files (*.mpath)"));
  if (fileNameStr == "") return;
  TFilePath fp(fileNameStr.toStdWString());
  if (fp.getType() == "") fp = fp.withType("mpath");
  try {
    if (!TFileStatus(fp).doesExist()) {
      QString msg;
      msg = "Motion path " + toQString(fp) + " doesn't exists.";
      DVGui::info(msg);
      return;
    }
    assert(m_objHandle->isSpline());
    // ObjectID of the parent node to which the spline is connected
    TStageObjectId id = m_objHandle->getObjectId();
    TXsheet *xsh      = m_xshHandle->getXsheet();

    TStageObjectSpline *spline =
        xsh->getStageObjectTree()->getStageObject(id, false)->getSpline();
    if (!spline) return;

    TIStream is(fp);
    if (is) {
      spline->loadData(is);
      m_objHandle->setSplineObject(spline);
      m_objHandle->commitSplineChanges();
      IconGenerator::instance()->invalidate(spline);
    }
  } catch (...) {
    DVGui::warning(QObject::tr("It is not possible to load the motion path."));
  }
}

//--------------------------------------------------------

void StageSchematicScene::onPathToggled(int state) {
  TStageObjectId objId = m_objHandle->getObjectId();
  TStageObject *obj    = m_xshHandle->getXsheet()->getStageObject(objId);
  TStageObjectCmd::enableSplineAim(obj, state, m_xshHandle);
  onSceneChanged();
  update();
}

//--------------------------------------------------------

void StageSchematicScene::onCpToggled(bool toggled) {
  TStageObjectId objId = m_objHandle->getObjectId();
  TStageObject *obj    = m_xshHandle->getXsheet()->getStageObject(objId);
  TStageObjectCmd::enableSplineUppk(obj, toggled, m_xshHandle);
  update();
}

//------------------------------------------------------------------

TXsheet *StageSchematicScene::getXsheet() { return m_xshHandle->getXsheet(); }

//------------------------------------------------------------------

void StageSchematicScene::onXsheetChanged() {
  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------------------

void StageSchematicScene::onSceneChanged() {
  m_sceneHandle->notifySceneChanged();
}

//------------------------------------------------------------------

void StageSchematicScene::onCurrentObjectChanged(const TStageObjectId &id,
                                                 bool isSpline) {
  // revert the pre-selected node to normal text color
  QMap<TStageObjectId, StageSchematicNode *>::iterator it;
  it = m_nodeTable.find(getCurrentObject());
  if (it != m_nodeTable.end()) {
    it.value()->update();
  }

  m_objHandle->setObjectId(id);
  if (m_frameHandle->isEditingLevel()) return;
  m_objHandle->setIsSpline(isSpline);
}

//------------------------------------------------------------------

void StageSchematicScene::onCurrentColumnChanged(int index) {
  m_colHandle->setColumnIndex(index);
}

//------------------------------------------------------------------

void StageSchematicScene::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *cme) {
  QPointF scenePos                = cme->scenePos();
  QList<QGraphicsItem *> itemList = items(scenePos);
  if (!itemList.isEmpty()) {
    SchematicScene::contextMenuEvent(cme);
    return;
  }

  QMenu menu(views()[0]);

  // AddPegbar
  QAction *addPegbar = new QAction(tr("&New Pegbar"), &menu);
  connect(addPegbar, SIGNAL(triggered()), this, SLOT(onPegbarAdded()));

  // AddSpline
  QAction *addSpline = new QAction(tr("&New Motion Path"), &menu);
  connect(addSpline, SIGNAL(triggered()), this, SLOT(onSplineAdded()));

  // AddCamera
  QAction *addCamera = new QAction(tr("&New Camera"), &menu);
  connect(addCamera, SIGNAL(triggered()), this, SLOT(onCameraAdded()));

  QAction *paste = CommandManager::instance()->getAction("MI_Paste");

  // this is to place the node at the clicked position
  addPegbar->setData(cme->scenePos());
  addSpline->setData(cme->scenePos());
  addCamera->setData(cme->scenePos());

  menu.addAction(addPegbar);
  menu.addAction(addCamera);
  menu.addAction(addSpline);

  // Close sub xsheet and move to parent sheet
  ToonzScene *scene      = m_sceneHandle->getScene();
  ChildStack *childStack = scene->getChildStack();
  if (childStack && childStack->getAncestorCount() > 0) {
    menu.addSeparator();
    menu.addAction(CommandManager::instance()->getAction("MI_CloseChild"));
  }

  menu.addSeparator();
  menu.addAction(paste);
  m_selection->setPastePosition(TPointD(scenePos.x(), scenePos.y()));
  menu.exec(cme->screenPos());
}

//------------------------------------------------------------------

void StageSchematicScene::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  QList<QGraphicsItem *> items = selectedItems();
  SchematicScene::mousePressEvent(me);
  if (me->button() == Qt::MidButton) {
    int i;
    for (i = 0; i < items.size(); i++) items[i]->setSelected(true);
  }
}

//------------------------------------------------------------------

SchematicNode *StageSchematicScene::getNodeFromPosition(const QPointF &pos) {
  QList<QGraphicsItem *> pickedItems = items(pos);
  for (int i = 0; i < pickedItems.size(); i++) {
    SchematicNode *node = dynamic_cast<SchematicNode *>(pickedItems.at(i));
    if (node) return node;
  }
  return 0;
}

//------------------------------------------------------------------

void StageSchematicScene::onSelectionChanged() {
  m_selection->selectNone();
  int i, size = m_highlightedLinks.size();
  for (i = 0; i < size; i++) {
    SchematicLink *link = m_highlightedLinks[i];
    link->setHighlighted(false);
    link->update();
  }
  m_highlightedLinks.clear();
  QList<QGraphicsItem *> slcItems = selectedItems();
  QList<QGraphicsItem *>::iterator it;
  for (it = slcItems.begin(); it != slcItems.end(); it++) {
    StageSchematicGroupNode *groupNode =
        dynamic_cast<StageSchematicGroupNode *>(*it);
    StageSchematicNode *node = dynamic_cast<StageSchematicNode *>(*it);
    if (groupNode) {
      QList<TStageObject *> objs = groupNode->getGroupedObjects();
      for (i = 0; i < objs.size(); i++) m_selection->select(objs[i]->getId());
      highlightLinks(groupNode, true);
    } else if (node) {
      m_selection->select(node->getStageObject()->getId());
      highlightLinks(node, true);
    }
    StageSchematicSplineNode *spline =
        dynamic_cast<StageSchematicSplineNode *>(*it);
    if (spline) {
      m_selection->select(spline->getSpline()->getId());
      StageSchematicSplinePort *port = spline->getParentPort();
      if (port) {
        int i, linkCount = port->getLinkCount();
        for (i = 0; i < linkCount; i++) {
          SchematicLink *link = port->getLink(i);
          link->setHighlighted(true);
          m_highlightedLinks.push_back(link);
        }
      }
    }

    SchematicLink *link = dynamic_cast<SchematicLink *>(*it);
    if (link) m_selection->select(link);
  }
  if (!m_selection->isEmpty()) m_selection->makeCurrent();
}

//------------------------------------------------------------------

void StageSchematicScene::onCollapse(QList<TStageObjectId> objects) {
  emit doCollapse(objects);
}

//------------------------------------------------------------------

void StageSchematicScene::onEditGroup() {
  if (m_selection->isEmpty()) return;
  TStageObjectTree *pegTree  = m_xshHandle->getXsheet()->getStageObjectTree();
  QList<TStageObjectId> objs = m_selection->getObjects();
  int i;
  for (i = 0; i < objs.size(); i++) {
    TStageObject *obj = pegTree->getStageObject(objs[i], false);
    if (obj && obj->isGrouped() && !obj->isGroupEditing()) obj->editGroup();
  }
  updateScene();
}

//------------------------------------------------------------------

TStageObjectId StageSchematicScene::getCurrentObject() {
  return m_objHandle->getObjectId();
}
