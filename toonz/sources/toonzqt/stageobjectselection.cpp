

#include "stageobjectselection.h"
#include "tundo.h"
#include "tconst.h"
#include "toonzqt/schematicnode.h"
#include "toonzqt/stageschematicnode.h"
#include "toonzqt/stageobjectsdata.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"

#include "toonzqt/selectioncommandids.h"
#include "historytypes.h"

#include <QSet>
#include <QApplication>
#include <QClipboard>

namespace {
class TPasteSelectionUndo final : public TUndo {
  StageObjectsData *m_objData;
  int m_index;
  std::vector<TStageObjectId> m_pastedId;
  std::list<int> m_pastedSplineIds;
  QMap<TStageObjectId, QList<TFxPort *>> m_columnFxConnections;
  TXsheetHandle *m_xshHandle;
  TObjectHandle *m_objHandle;
  TFxHandle *m_fxHandle;
  TPointD m_pastedPos;

public:
  TPasteSelectionUndo(StageObjectsData *objData, int index,
                      const std::vector<TStageObjectId> &pastedId,
                      const std::list<int> pastedSplineIds,
                      const TPointD &pastedPos, TXsheetHandle *xshHandle,
                      TObjectHandle *objHandle, TFxHandle *fxHandle)
      : TUndo()
      , m_objData(objData)
      , m_index(index)
      , m_pastedSplineIds(pastedSplineIds)
      , m_pastedId(pastedId)
      , m_xshHandle(xshHandle)
      , m_objHandle(objHandle)
      , m_fxHandle(fxHandle)
      , m_pastedPos(pastedPos) {
    std::vector<TStageObjectId>::const_iterator it;
    for (it = m_pastedId.begin(); it != m_pastedId.end(); it++) {
      if (it->isColumn()) {
        TXsheet *xsh       = m_xshHandle->getXsheet();
        TXshColumnP column = xsh->getColumn(it->getIndex());
        assert(column);
        TFx *columnFx = column->getFx();
        if (!columnFx) continue;
        int i;
        for (i = 0; i < columnFx->getOutputConnectionCount(); i++)
          m_columnFxConnections[*it].append(columnFx->getOutputConnection(i));
      }
    }
  }

  ~TPasteSelectionUndo() {}

  void undo() const override {
    m_xshHandle->blockSignals(true);
    TStageObjectCmd::deleteSelection(
        m_pastedId, std::list<QPair<TStageObjectId, TStageObjectId>>(),
        m_pastedSplineIds, m_xshHandle, m_objHandle, m_fxHandle, false);
    m_xshHandle->blockSignals(false);
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    std::set<int> indexes;
    indexes.insert(m_index);
    std::list<int> splineIds;
    m_objData->restoreObjects(indexes, splineIds, m_xshHandle->getXsheet(), 0,
                              m_pastedPos);
    QMap<TStageObjectId, QList<TFxPort *>>::const_iterator it;
    TXsheet *xsh = m_xshHandle->getXsheet();
    for (it = m_columnFxConnections.begin(); it != m_columnFxConnections.end();
         it++) {
      TXshColumnP column = xsh->getColumn(it.key().getIndex());
      assert(column);
      TFx *columnFx          = column->getFx();
      QList<TFxPort *> ports = it.value();
      int i;
      for (i = 0; i < ports.size(); i++) ports[i]->setFx(columnFx);
    }
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof(*this) + sizeof(StageObjectsData);
  }

  QString getHistoryString() override {
    QString str                              = QObject::tr("Paste Object  ");
    std::vector<TStageObjectId>::iterator it = m_pastedId.begin();
    for (; it != m_pastedId.end(); it++) {
      if (it != m_pastedId.begin()) str += QString::fromStdString(", ");
      str += QString::fromStdString((*it).toString());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::Schematic; }
};
}

//======================================================================
//
// StageObjectSelection
//
//======================================================================

StageObjectSelection::StageObjectSelection()
    : m_xshHandle(0)
    , m_objHandle(0)
    , m_fxHandle(0)
    , m_pastePosition(TConst::nowhere) {}

//-------------------------------------------------------

StageObjectSelection::StageObjectSelection(const StageObjectSelection &src)
    : m_selectedObjects(src.m_selectedObjects)
    , m_selectedLinks(src.m_selectedLinks)
    , m_selectedSplines(src.m_selectedSplines)
    , m_xshHandle(src.m_xshHandle)
    , m_objHandle(src.m_objHandle)
    , m_fxHandle(src.m_fxHandle)
    , m_pastePosition(TConst::nowhere) {}

//-------------------------------------------------------

StageObjectSelection::~StageObjectSelection() {}

//-------------------------------------------------------

void StageObjectSelection::enableCommands() {
  enableCommand(this, MI_Clear, &StageObjectSelection::deleteSelection);
  enableCommand(this, MI_Group, &StageObjectSelection::groupSelection);
  enableCommand(this, MI_Ungroup, &StageObjectSelection::ungroupSelection);
  enableCommand(this, MI_Collapse, &StageObjectSelection::collapseSelection);
  enableCommand(this, MI_ExplodeChild, &StageObjectSelection::explodeChild);
  enableCommand(this, MI_Copy, &StageObjectSelection::copySelection);
  enableCommand(this, MI_Paste, &StageObjectSelection::pasteSelection);
  enableCommand(this, MI_Cut, &StageObjectSelection::cutSelection);
}

//-------------------------------------------------------

void StageObjectSelection::deleteSelection() {
  TStageObjectCmd::deleteSelection(
      m_selectedObjects.toVector().toStdVector(), m_selectedLinks.toStdList(),
      m_selectedSplines.toStdList(), m_xshHandle, m_objHandle, m_fxHandle);
}

//-------------------------------------------------------

void StageObjectSelection::select(const TStageObjectId &id) {
  m_selectedObjects.append(id);
}

//-------------------------------------------------------

void StageObjectSelection::unselect(const TStageObjectId &id) {
  int index = m_selectedObjects.indexOf(id);
  if (index >= 0) m_selectedObjects.removeAt(index);
}

//-------------------------------------------------------

void StageObjectSelection::select(int id) { m_selectedSplines.append(id); }

//-------------------------------------------------------

void StageObjectSelection::unselect(int id) {
  int index = m_selectedSplines.indexOf(id);
  if (index >= 0) m_selectedSplines.removeAt(index);
}

//-------------------------------------------------------

void StageObjectSelection::select(SchematicLink *link) {
  QPair<TStageObjectId, TStageObjectId> boundingObjects =
      getBoundingObjects(link);
  m_selectedLinks.append(boundingObjects);
}

//-------------------------------------------------------

void StageObjectSelection::unselect(SchematicLink *link) {
  QPair<TStageObjectId, TStageObjectId> boundingObjects =
      getBoundingObjects(link);
  int index = m_selectedLinks.indexOf(boundingObjects);
  if (index >= 0) m_selectedLinks.removeAt(index);
}

//-------------------------------------------------------

bool StageObjectSelection::isSelected(const TStageObjectId &id) const {
  return m_selectedObjects.contains(id);
}

//-------------------------------------------------------

bool StageObjectSelection::isSelected(SchematicLink *link) {
  QPair<TStageObjectId, TStageObjectId> boundingObjects =
      getBoundingObjects(link);
  return m_selectedLinks.contains(boundingObjects);
}

//-------------------------------------------------------

QPair<TStageObjectId, TStageObjectId> StageObjectSelection::getBoundingObjects(
    SchematicLink *link) {
  QPair<TStageObjectId, TStageObjectId> boundingObjects;
  if (link) {
    SchematicPort *port = link->getStartPort();
    if (port->getType() == 100 && link->getEndPort()->getType() == 100) {
      StageSchematicNode *node =
          dynamic_cast<StageSchematicNode *>(port->getNode());
      if (node)
        boundingObjects = getBoundingObjects(port, port);
      else
        boundingObjects =
            getBoundingObjects(link->getEndPort(), link->getEndPort());
    } else if (port->getType() == 101)
      boundingObjects = getBoundingObjects(port, link->getOtherPort(port));
    else if (port->getType() == 102)
      boundingObjects = getBoundingObjects(link->getOtherPort(port), port);
  }
  return boundingObjects;
}

//-------------------------------------------------------

QPair<TStageObjectId, TStageObjectId> StageObjectSelection::getBoundingObjects(
    SchematicPort *inputPort, SchematicPort *outputPort) {
  QPair<TStageObjectId, TStageObjectId> boundingObjects;
  StageSchematicNode *inputNode =
      dynamic_cast<StageSchematicNode *>(inputPort->getNode());
  StageSchematicNode *outputNode =
      dynamic_cast<StageSchematicNode *>(outputPort->getNode());
  if (!inputNode || !outputNode) return boundingObjects;
  TStageObjectId inputId  = inputNode->getStageObject()->getId();
  TStageObjectId outputId = outputNode->getStageObject()->getId();
  boundingObjects.first   = inputId;
  boundingObjects.second  = outputId;
  return boundingObjects;
}

//-------------------------------------------------------

bool StageObjectSelection::isConnected() const {
  int count = 0;
  int i;
  TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
  bool canBeGrouped         = true;
  for (i = 0; i < m_selectedObjects.size(); i++) {
    TStageObjectId id = m_selectedObjects[i];
    TStageObject *obj = pegTree->getStageObject(id, false);
    if (!m_selectedObjects.contains(obj->getParent())) {
      count++;
      continue;
    }
    TStageObject *parentObj = pegTree->getStageObject(obj->getParent(), false);
    canBeGrouped            = canBeGrouped &&
                   obj->getEditingGroupId() == parentObj->getEditingGroupId();
  }
  return count == 1 && canBeGrouped;
}

//-------------------------------------------------------

void StageObjectSelection::groupSelection() {
  if (m_selectedObjects.size() <= 1 || !isConnected()) return;
  TStageObjectCmd::group(m_selectedObjects, m_xshHandle);
  selectNone();
  m_xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------

void StageObjectSelection::ungroupSelection() {
  if (isEmpty()) return;
  TStageObjectTree *objTree = m_xshHandle->getXsheet()->getStageObjectTree();
  if (!objTree) return;
  QSet<int> idSet;
  int i;
  for (i = 0; i < m_selectedObjects.size(); i++) {
    int groupId =
        objTree->getStageObject(m_selectedObjects[i], false)->getGroupId();
    if (groupId > 0)
      idSet.insert(
          objTree->getStageObject(m_selectedObjects[i], false)->getGroupId());
  }

  TUndoManager::manager()->beginBlock();
  QSet<int>::iterator it;
  for (it = idSet.begin(); it != idSet.end(); it++)
    TStageObjectCmd::ungroup(*it, m_xshHandle);
  TUndoManager::manager()->endBlock();
  selectNone();
  m_xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------

void StageObjectSelection::collapseSelection() {
  if (isEmpty()) return;
  QList<TStageObjectId> objects = getObjects();
  if (!objects.isEmpty()) emit doCollapse(objects);
}

//-------------------------------------------------------

void StageObjectSelection::explodeChild() {
  if (isEmpty()) return;
  QList<TStageObjectId> objects = getObjects();
  if (!objects.isEmpty()) emit doExplodeChild(objects);
}

//-------------------------------------------------------

void StageObjectSelection::copySelection() {
  StageObjectsData *data = new StageObjectsData();
  bool pegSelected       = false;
  data->storeObjects(
      m_selectedObjects.toVector().toStdVector(), m_xshHandle->getXsheet(),
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
  std::set<int> columnIndexes;
  int i;
  for (i = 0; i < m_selectedObjects.size(); i++)
    if (m_selectedObjects[i].isColumn())
      columnIndexes.insert(m_selectedObjects[i].getIndex());
  data->storeColumnFxs(
      columnIndexes, m_xshHandle->getXsheet(),
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
  data->storeSplines(
      m_selectedSplines.toStdList(), m_xshHandle->getXsheet(),
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);

  if (!data->isEmpty())
    QApplication::clipboard()->setMimeData(data);
  else
    delete data;
}

//-------------------------------------------------------

void StageObjectSelection::pasteSelection() {
  int index             = m_xshHandle->getXsheet()->getColumnCount();
  const QMimeData *data = QApplication::clipboard()->mimeData();
  const StageObjectsData *objData =
      dynamic_cast<const StageObjectsData *>(data);
  if (!objData) return;
  std::set<int> indexes;
  indexes.insert(index);
  std::list<int> restoredSplineIds;
  std::vector<TStageObjectId> ids = objData->restoreObjects(
      indexes, restoredSplineIds, m_xshHandle->getXsheet(),
      StageObjectsData::eDoClone, m_pastePosition);
  StageObjectsData *undoData = new StageObjectsData();
  undoData->storeObjects(ids, m_xshHandle->getXsheet(), 0);
  undoData->storeColumnFxs(indexes, m_xshHandle->getXsheet(), 0);
  undoData->storeSplines(restoredSplineIds, m_xshHandle->getXsheet(), 0);
  TUndoManager::manager()->add(new TPasteSelectionUndo(
      undoData, index, ids, restoredSplineIds, m_pastePosition, m_xshHandle,
      m_objHandle, m_fxHandle));
  m_xshHandle->notifyXsheetChanged();
  m_pastePosition = TConst::nowhere;
}

//-------------------------------------------------------

void StageObjectSelection::cutSelection() {
  copySelection();
  deleteSelection();
}
