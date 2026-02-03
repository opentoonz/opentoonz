

// TnzLib includes
#include "toonz/tstageobjectcmd.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/fxdag.h"

// TnzBase includes
#include "tdoublekeyframe.h"
#include "tfx.h"

// TnzCore includes
#include "tundo.h"
#include "tconvert.h"

#include "historytypes.h"

// Qt includes
#include <QMap>
#include <QString>

namespace {

bool canRemoveFx(const std::set<TFx *> &leaves, TFx *fx) {
  for (int i = 0; i < fx->getInputPortCount(); i++) {
    TFx *inputFx = fx->getInputPort(i)->getFx();
    if (!inputFx) continue;
    if (leaves.count(inputFx) > 0) continue;
    if (!canRemoveFx(leaves, inputFx)) return false;
  }
  return fx->getInputPortCount() > 0;
}

//=========================================================
//
// NewCameraUndo
//
//---------------------------------------------------------

class NewCameraUndo final : public TUndo {
  TStageObjectId m_cameraId, m_oldCurrentId;
  TStageObject *m_stageObject;
  TXsheetHandle *m_xshHandle;
  TObjectHandle *m_objHandle;

public:
  NewCameraUndo(const TStageObjectId &cameraId, TXsheetHandle *xshHandle,
                TObjectHandle *objHandle)
      : m_cameraId(cameraId)
      , m_stageObject(0)
      , m_xshHandle(xshHandle)
      , m_objHandle(objHandle) {
    assert(cameraId.isCamera());
    TXsheet *xsh  = m_xshHandle->getXsheet();
    m_stageObject = xsh->getStageObject(m_cameraId);
    m_stageObject->addRef();
    m_oldCurrentId = m_objHandle->getObjectId();
  }
  ~NewCameraUndo() { m_stageObject->release(); }
  void undo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    if (m_cameraId == m_objHandle->getObjectId())
      m_objHandle->setObjectId(m_oldCurrentId);
    xsh->getStageObjectTree()->removeStageObject(m_cameraId);
    m_xshHandle->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    xsh->getStageObjectTree()->insertStageObject(m_stageObject);
    m_objHandle->setObjectId(m_cameraId);
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("New Camera  %1")
        .arg(QString::fromStdString(m_cameraId.toString()));
  }
  int getHistoryType() override { return HistoryType::Schematic; }

private:
  // not implemented
  NewCameraUndo(const NewCameraUndo &);
  NewCameraUndo &operator=(const NewCameraUndo &);
};

//=========================================================
//
// NewPegbarUndo
//
//---------------------------------------------------------

class NewPegbarUndo final : public TUndo {
  TStageObjectId m_id, m_oldCurrentId;
  TStageObject *m_stageObject;
  TXsheetHandle *m_xshHandle;
  TObjectHandle *m_objHandle;

public:
  NewPegbarUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                TObjectHandle *objHandle)
      : m_id(id)
      , m_stageObject(0)
      , m_xshHandle(xshHandle)
      , m_objHandle(objHandle) {
    assert(!id.isTable());
    TXsheet *xsh  = m_xshHandle->getXsheet();
    m_stageObject = xsh->getStageObject(m_id);
    m_stageObject->addRef();
    m_oldCurrentId = m_objHandle->getObjectId();
  }

  ~NewPegbarUndo() { m_stageObject->release(); }

  void undo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    if (m_id == m_objHandle->getObjectId())
      m_objHandle->setObjectId(m_oldCurrentId);
    xsh->getStageObjectTree()->removeStageObject(m_id);
    m_xshHandle->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    xsh->getStageObjectTree()->insertStageObject(m_stageObject);
    m_objHandle->setObjectId(m_id);
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("New Pegbar  %1")
        .arg(QString::fromStdString(m_id.toString()));
  }

  int getHistoryType() override { return HistoryType::Schematic; }

private:
  // not implemented
  NewPegbarUndo(const NewPegbarUndo &);
  NewPegbarUndo &operator=(const NewPegbarUndo &);
};

//===================================================================
//
// SetActiveCameraUndo
//
//-------------------------------------------------------------------

class SetActiveCameraUndo final : public TUndo {
  TStageObjectId m_oldCameraId, m_newCameraId;
  TXsheetHandle *m_xshHandle;

public:
  SetActiveCameraUndo(const TStageObjectId &oldCameraId,
                      const TStageObjectId &newCameraId,
                      TXsheetHandle *xshHandle)
      : m_oldCameraId(oldCameraId)
      , m_newCameraId(newCameraId)
      , m_xshHandle(xshHandle) {}

  void undo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    xsh->getStageObjectTree()->setCurrentCameraId(m_oldCameraId);
    // make the preview camera same as the final camera
    xsh->getStageObjectTree()->setCurrentPreviewCameraId(m_oldCameraId);
    m_xshHandle->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    xsh->getStageObjectTree()->setCurrentCameraId(m_newCameraId);
    // make the preview camera same as the final camera
    xsh->getStageObjectTree()->setCurrentPreviewCameraId(m_newCameraId);
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Set Active Camera  %1 > %2")
        .arg(QString::fromStdString(m_oldCameraId.toString()))
        .arg(QString::fromStdString(m_newCameraId.toString()));
  }

  int getHistoryType() override { return HistoryType::Schematic; }
};

//===================================================================
//
// RemoveSplineUndo
//
//-------------------------------------------------------------------

class RemoveSplineUndo final : public TUndo {
  TStageObjectId m_id;
  TStageObjectSpline *m_spline;
  std::vector<TStageObjectId> m_ids;
  TXsheetHandle *m_xshHandle;

public:
  RemoveSplineUndo(TStageObjectSpline *spline, TXsheetHandle *xshHandle)
      : m_spline(spline), m_xshHandle(xshHandle) {
    m_spline->addRef();
    TStageObjectTree *pegbarTree =
        m_xshHandle->getXsheet()->getStageObjectTree();
    for (int i = 0; i < pegbarTree->getStageObjectCount(); i++) {
      TStageObject *pegbar = pegbarTree->getStageObject(i);
      if (pegbar->getSpline() == m_spline) m_ids.push_back(pegbar->getId());
    }
  }
  ~RemoveSplineUndo() { m_spline->release(); }
  void undo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    xsh->getStageObjectTree()->insertSpline(m_spline);
    for (int i = 0; i < (int)m_ids.size(); i++) {
      TStageObject *pegbar = xsh->getStageObject(m_ids[i]);
      assert(pegbar);
      pegbar->setSpline(m_spline);
    }
    m_xshHandle->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    for (int i = 0; i < (int)m_ids.size(); i++) {
      TStageObject *pegbar = xsh->getStageObject(m_ids[i]);
      assert(pegbar);
      pegbar->setSpline(0);
    }
    xsh->getStageObjectTree()->removeSpline(m_spline);
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override {
    return sizeof *this + sizeof(TStageObjectSpline) +
           sizeof(TStageObjectId) * m_ids.size();
  }

  QString getHistoryString() override {
    return QObject::tr("Remove Spline  %1")
        .arg(QString::fromStdString(m_id.toString()));
  }

  int getHistoryType() override { return HistoryType::Schematic; }
};

//===================================================================
//
// NewSplineUndo
//
//-------------------------------------------------------------------

class NewSplineUndo final : public TUndo {
  TStageObjectId m_id;
  TStageObjectSpline *m_spline;
  TXsheetHandle *m_xshHandle;

public:
  NewSplineUndo(const TStageObjectId &id, TStageObjectSpline *spline,
                TXsheetHandle *xshHandle)
      : m_id(id), m_spline(spline), m_xshHandle(xshHandle) {
    m_spline->addRef();
  }
  ~NewSplineUndo() { m_spline->release(); }
  void undo() const override {
    TXsheet *xsh         = m_xshHandle->getXsheet();
    TStageObject *pegbar = xsh->getStageObject(m_id);
    assert(pegbar);
    pegbar->setSpline(0);
    xsh->getStageObjectTree()->removeSpline(m_spline);
    m_xshHandle->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = m_xshHandle->getXsheet();
    xsh->getStageObjectTree()->insertSpline(m_spline);
    TStageObject *pegbar = xsh->getStageObject(m_id);
    assert(pegbar);
    pegbar->setSpline(m_spline);
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override {
    return sizeof *this + sizeof(TStageObjectSpline);
  }

  QString getHistoryString() override {
    return QObject::tr("New Motion Path  %1")
        .arg(QString::fromStdString(m_spline->getName()));
  }
  int getHistoryType() override { return HistoryType::Schematic; }
};

//===================================================================
//
// SplineLinkUndo
//
//-------------------------------------------------------------------

class SplineLinkUndo final : public TUndo {
  TStageObjectId m_id;
  TStageObjectSpline *m_spline;
  TXsheetHandle *m_xshHandle;

public:
  SplineLinkUndo(const TStageObjectId &id, TStageObjectSpline *spline,
                 TXsheetHandle *xshHandle)
      : m_id(id), m_spline(spline), m_xshHandle(xshHandle) {
    m_spline->addRef();
  }

  ~SplineLinkUndo() { m_spline->release(); }

  void undo() const override {
    TStageObject *object = m_xshHandle->getXsheet()->getStageObject(m_id);
    object->setSpline(0);
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    TStageObject *object = m_xshHandle->getXsheet()->getStageObject(m_id);
    object->setSpline(m_spline);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof *this + sizeof(TStageObjectSpline);
  }

  QString getHistoryString() override {
    return QObject::tr("Link Motion Path  %1 > %2")
        .arg(QString::fromStdString(m_spline->getName()))
        .arg(QString::fromStdString(m_id.toString()));
  }
  int getHistoryType() override { return HistoryType::Schematic; }
};

//===================================================================
//
// DeleteSplineLinkUndo
//
//-------------------------------------------------------------------

class RemoveSplineLinkUndo final : public TUndo {
  TStageObjectId m_id;
  TStageObjectSpline *m_spline;
  TXsheetHandle *m_xshHandle;
  TObjectHandle *m_objHandle;

public:
  RemoveSplineLinkUndo(const TStageObjectId &id, TStageObjectSpline *spline,
                       TXsheetHandle *xshHandle, TObjectHandle *objHandle)
      : m_id(id)
      , m_spline(spline)
      , m_xshHandle(xshHandle)
      , m_objHandle(objHandle) {
    m_spline->addRef();
  }

  ~RemoveSplineLinkUndo() { m_spline->release(); }

  void undo() const override {
    TXsheet *xsh              = m_xshHandle->getXsheet();
    TStageObjectTree *objTree = xsh->getStageObjectTree();
    TStageObject *object      = objTree->getStageObject(m_id, false);
    if (!object) return;
    object->setSpline(m_spline);
    if (m_objHandle->getObjectId() == m_id) m_objHandle->setIsSpline(true);
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    TXsheet *xsh              = m_xshHandle->getXsheet();
    TStageObjectTree *objTree = xsh->getStageObjectTree();
    TStageObject *object      = objTree->getStageObject(m_id, false);
    if (!object) return;
    object->setSpline(0);
    if (m_objHandle->getObjectId() == m_id) m_objHandle->setIsSpline(false);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof *this + sizeof(TStageObjectSpline);
  }
};

//===================================================================
//
// RemovePegbarNodeUndo
//
//-------------------------------------------------------------------

class RemovePegbarNodeUndo final : public TUndo {
  TStageObjectId m_objId;
  TXshColumnP m_column;
  TStageObjectParams *m_params;
  QList<TStageObjectId> m_linkedObj;
  TXsheetHandle *m_xshHandle;

public:
  RemovePegbarNodeUndo(TStageObjectId id, TXsheetHandle *xshHandle)
      : TUndo(), m_objId(id), m_xshHandle(xshHandle), m_column(0) {
    TXsheet *xsh      = xshHandle->getXsheet();
    TStageObject *obj = xsh->getStageObject(id);
    assert(obj);
    m_params = obj->getParams();
    if (id.isColumn()) m_column = xsh->getColumn(id.getIndex());
  }

  ~RemovePegbarNodeUndo() { delete m_params; }

  void setLinkedObjects(const QList<TStageObjectId> &linkedObj) {
    m_linkedObj = linkedObj;
  }

  void undo() const override {
    // reinsert Object
    TXsheet *xsh = m_xshHandle->getXsheet();
    if (m_objId.isColumn() && m_column)
      xsh->insertColumn(m_objId.getIndex(), m_column.getPointer());
    TStageObject *obj = xsh->getStageObject(m_objId);
    obj->assignParams(m_params);
    obj->setParent(m_params->m_parentId);

    int i, linkCount = m_linkedObj.size();
    for (i = 0; i < linkCount; i++) {
      TStageObject *linkedObj = xsh->getStageObject(m_linkedObj[i]);
      assert(linkedObj);
      linkedObj->setParent(m_objId);
    }
    m_xshHandle->notifyXsheetChanged();
    xsh->notifyStageObjectAdded(m_objId);
  }

  void redo() const override {
    TXsheet *xsh     = m_xshHandle->getXsheet();
    int pegbarsCount = xsh->getStageObjectTree()->getStageObjectCount();
    int i;
    for (i = 0; i < pegbarsCount; ++i) {
      TStageObject *other = xsh->getStageObjectTree()->getStageObject(i);
      if (other->getId() == m_objId) continue;
      if (other->getParent() == m_objId)
        other->setParent(xsh->getStageObjectParent(m_objId));
    }
    if (m_objId.isColumn())
      xsh->removeColumn(m_objId.getIndex());
    else
      xsh->getStageObjectTree()->removeStageObject(m_objId);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof *this + sizeof(TStageObjectParams) + sizeof(m_xshHandle);
  }

  QString getHistoryString() override {
    return QObject::tr("Remove Object  %1")
        .arg(QString::fromStdString(m_objId.toString()));
  }
  int getHistoryType() override { return HistoryType::Schematic; }
};

//===================================================================
//
// RemoveColumnsUndo
//
//-------------------------------------------------------------------

class RemoveColumnsUndo final : public TUndo {
  std::vector<TFx *> m_deletedFx;
  std::vector<TFx *> m_terminalFx;
  QMap<TStageObjectId, QList<TFxPort *>> m_columnFxConnections;
  QList<TFx *> m_notTerminalColumns;
  TXsheetHandle *m_xshHandle;

public:
  RemoveColumnsUndo(
      const std::vector<TFx *> &deletedFx, const std::vector<TFx *> &terminalFx,
      const QMap<TStageObjectId, QList<TFxPort *>> columnFxConnections,
      const QList<TFx *> &notTerminalColumns, TXsheetHandle *xshHandle)
      : TUndo()
      , m_deletedFx(deletedFx)
      , m_terminalFx(terminalFx)
      , m_columnFxConnections(columnFxConnections)
      , m_notTerminalColumns(notTerminalColumns)
      , m_xshHandle(xshHandle) {
    int i;
    for (i = 0; i < (int)m_deletedFx.size(); i++) m_deletedFx[i]->addRef();
  }

  ~RemoveColumnsUndo() {
    int i;
    for (i = 0; i < (int)m_deletedFx.size(); i++) m_deletedFx[i]->release();
  }

  void undo() const override {
    TXsheet *xsh        = m_xshHandle->getXsheet();
    TFxSet *terminalFxs = xsh->getFxDag()->getTerminalFxs();
    TFxSet *internalFxs = xsh->getFxDag()->getInternalFxs();
    int i;
    for (i = 0; i < (int)m_deletedFx.size(); i++)
      internalFxs->addFx(m_deletedFx[i]);
    for (i = 0; i < (int)m_terminalFx.size(); i++)
      terminalFxs->addFx(m_terminalFx[i]);
    QMap<TStageObjectId, QList<TFxPort *>>::const_iterator it;
    for (it = m_columnFxConnections.begin(); it != m_columnFxConnections.end();
         it++) {
      TStageObjectId id      = it.key();
      QList<TFxPort *> ports = it.value();
      TXshColumnP column     = xsh->getColumn(id.getIndex());
      assert(column);
      int j;
      for (j = 0; j < ports.size(); j++) ports[j]->setFx(column->getFx());
    }
    for (i = 0; i < m_notTerminalColumns.size(); i++)
      terminalFxs->removeFx(m_notTerminalColumns[i]);

    m_xshHandle->notifyXsheetChanged();
    xsh->notifyFxAdded(m_deletedFx);
  }

  void redo() const override {
    TXsheet *xsh        = m_xshHandle->getXsheet();
    TFxSet *terminalFxs = xsh->getFxDag()->getTerminalFxs();
    TFxSet *internalFxs = xsh->getFxDag()->getInternalFxs();
    int i;
    for (i = 0; i < (int)m_deletedFx.size(); i++)
      internalFxs->removeFx(m_deletedFx[i]);
    for (i = 0; i < (int)m_terminalFx.size(); i++)
      terminalFxs->removeFx(m_terminalFx[i]);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof *this + m_deletedFx.size() * sizeof(TFx) +
           m_terminalFx.size() * sizeof(TFx) +
           m_columnFxConnections.size() *
               (sizeof(TStageObjectId) + 10 * sizeof(TFxPort)) +
           m_notTerminalColumns.size() * sizeof(TFx) + sizeof(TXsheetHandle);
  }

  QString getHistoryString() override {
    QString str = QObject::tr("Remove Column  ");
    QMap<TStageObjectId, QList<TFxPort *>>::const_iterator it;
    for (it = m_columnFxConnections.begin(); it != m_columnFxConnections.end();
         it++) {
      TStageObjectId id = it.key();
      if (it != m_columnFxConnections.begin())
        str += QString::fromStdString(", ");
      str += QString::fromStdString(id.toString());
    }
    return str;
  }
  int getHistoryType() override { return HistoryType::Schematic; }
};

//===================================================================
//
// UndoGroup
//
//-------------------------------------------------------------------

class UndoGroup final : public TUndo {
  QList<TStageObjectId> m_ids;
  int m_groupId;
  QList<int> m_positions;
  TXsheetHandle *m_xshHandle;

public:
  UndoGroup(const QList<TStageObjectId> &ids, int groupId,
            const QList<int> &positions, TXsheetHandle *xshHandle)
      : m_ids(ids)
      , m_groupId(groupId)
      , m_positions(positions)
      , m_xshHandle(xshHandle) {}

  ~UndoGroup() {}

  void undo() const override {
    assert(m_ids.size() == m_positions.size());
    TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
    int i;
    for (i = 0; i < m_ids.size(); i++) {
      TStageObject *obj = pegTree->getStageObject(m_ids[i], false);
      if (obj) {
        obj->removeGroupName(m_positions[i]);
        obj->removeGroupId(m_positions[i]);
      }
    }
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    assert(m_ids.size() == m_positions.size());
    TStageObjectTree *pegTree = m_xshHandle->getXsheet()->getStageObjectTree();
    int i;
    for (i = 0; i < m_ids.size(); i++) {
      TStageObject *obj = pegTree->getStageObject(m_ids[i], false);
      if (obj) {
        obj->setGroupId(m_groupId, m_positions[i]);
        obj->setGroupName(L"Group " + std::to_wstring(m_groupId),
                          m_positions[i]);
      }
    }
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
};

//===================================================================
//
// UndoUngroup
//
//-------------------------------------------------------------------

class UndoUngroup final : public TUndo {
  QList<TStageObjectId> m_objsId;
  QList<int> m_positions;
  int m_groupId;
  std::wstring m_groupName;
  TXsheetHandle *m_xshHandle;

public:
  UndoUngroup(const QList<TStageObject *> &objs, TXsheetHandle *xshHandle)
      : m_xshHandle(xshHandle) {
    assert(objs.size() > 0);
    int i;
    for (i = 0; i < objs.size(); i++) {
      m_objsId.append(objs[i]->getId());
      if (i == 0) {
        m_groupId   = objs[i]->getGroupId();
        m_groupName = objs[i]->getGroupName(false);
      }
    }
  }

  ~UndoUngroup() {}

  void setStackPositions(const QList<int> &positions) {
    m_positions = positions;
  }

  void undo() const override {
    assert(m_objsId.size() == m_positions.size());
    TStageObjectTree *objTree = m_xshHandle->getXsheet()->getStageObjectTree();
    if (!objTree) return;
    int i;
    for (i = 0; i < m_objsId.size(); i++) {
      TStageObject *obj = objTree->getStageObject(m_objsId[i], false);
      if (!obj) continue;
      obj->setGroupId(m_groupId, m_positions[i]);
      obj->setGroupName(m_groupName, m_positions[i]);
    }
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    assert(m_objsId.size() == m_positions.size());
    TStageObjectTree *objTree = m_xshHandle->getXsheet()->getStageObjectTree();
    if (!objTree) return;
    int i;
    for (i = 0; i < m_objsId.size(); i++) {
      TStageObject *obj = objTree->getStageObject(m_objsId[i], false);
      if (!obj) continue;
      obj->removeGroupName(m_positions[i]);
      obj->removeGroupId(m_positions[i]);
    }
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
};

//===================================================================
//
// UndoRenameGroup
//
//-------------------------------------------------------------------

class UndoRenameGroup final : public TUndo {
  QList<TStageObject *> m_objs;
  QList<int> m_positions;
  std::wstring m_oldGroupName;
  std::wstring m_newGroupName;
  TXsheetHandle *m_xshHandle;

public:
  UndoRenameGroup(const QList<TStageObject *> &objs,
                  const QList<int> &positions, const std::wstring &newName,
                  const std::wstring &oldName, TXsheetHandle *xshHandle)
      : m_objs(objs)
      , m_newGroupName(newName)
      , m_oldGroupName(oldName)
      , m_xshHandle(xshHandle)
      , m_positions(positions) {
    assert(objs.size() > 0);
    int i;
    for (i = 0; i < m_objs.size(); i++) m_objs[i]->addRef();
  }

  ~UndoRenameGroup() {
    int i;
    for (i = 0; i < m_objs.size(); i++) m_objs[i]->release();
  }

  void undo() const override {
    assert(m_objs.size() == m_positions.size());
    int i;
    for (i = 0; i < m_objs.size(); i++) {
      m_objs[i]->removeGroupName(m_positions[i]);
      m_objs[i]->setGroupName(m_oldGroupName, m_positions[i]);
    }
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    assert(m_objs.size() == m_positions.size());
    int i;
    for (i = 0; i < m_objs.size(); i++) {
      m_objs[i]->removeGroupName(m_positions[i]);
      m_objs[i]->setGroupName(m_newGroupName, m_positions[i]);
    }
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
};

//===================================================================
//
// UndoStatusChange
//
//-------------------------------------------------------------------

class UndoStatusChange final : public TUndo {
  TStageObject *m_obj;
  TStageObject::Status m_oldStatus, m_newStatus;
  TXsheetHandle *m_xshHandle;

public:
  UndoStatusChange(TStageObject *obj, TXsheetHandle *xshHandle)
      : m_obj(obj), m_xshHandle(xshHandle) {
    m_obj->addRef();
    // devo fare addref della spline altimenti crasha in uscita
    // m_obj non fa addref della spline a lui associata, e quindi crasha perche'
    // la spline viene distrutta
    // prima di m_obj... sarebbe piu' corretto fare addref della spline quando
    // viene settata all'oggetto
    // piuttosto che farla qui?
    TStageObjectSpline *spline = m_obj->getSpline();
    if (spline) spline->addRef();
    m_oldStatus = m_obj->getStatus();
  }

  ~UndoStatusChange() {
    TStageObjectSpline *spline = m_obj->getSpline();
    m_obj->release();
    if (spline) spline->release();
  }

  void onAdd() override { m_newStatus = m_obj->getStatus(); }

  void undo() const override {
    m_obj->setStatus(m_oldStatus);
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    m_obj->setStatus(m_newStatus);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
};

//===================================================================
//
// removePegbarNode
//
//-------------------------------------------------------------------

void removeStageObjectNode(const TStageObjectId &id, TXsheetHandle *xshHandle,
                           TObjectHandle *objHandle, TFxHandle *fxHandle,
                           bool doUndo = true) {
  TXsheet *xsh         = xshHandle->getXsheet();
  TStageObject *pegbar = xsh->getStageObject(id);

  // Lacamera corrente e il tavolo non si devono rimuovere
  if (id.isTable() ||
      (id.isCamera() && xsh->getStageObjectTree()->getCurrentCameraId() == id))
    return;

  if (id.isCamera() && xsh->getCameraColumnIndex() == id.getIndex())
    xsh->setCameraColumnIndex(
        xsh->getStageObjectTree()->getCurrentCameraId().getIndex());

  // stacco tutti i figli e li attacco al padre
  QList<TStageObjectId> linkedObjects;
  int pegbarsCount = xsh->getStageObjectTree()->getStageObjectCount();
  int i;
  for (i = 0; i < pegbarsCount; ++i) {
    TStageObject *other = xsh->getStageObjectTree()->getStageObject(i);
    if (other == pegbar) continue;
    if (other->getParent() == id) {
      other->setParent(pegbar->getParent());
      linkedObjects.push_back(other->getId());
    }
  }
  if (id == objHandle->getObjectId())
    objHandle->setObjectId(TStageObjectId::TableId);

  RemovePegbarNodeUndo *undo = new RemovePegbarNodeUndo(id, xshHandle);
  undo->setLinkedObjects(linkedObjects);
  if (id.isColumn())
    xsh->removeColumn(id.getIndex());
  else
    xsh->getStageObjectTree()->removeStageObject(id);
  if (doUndo)
    TUndoManager::manager()->add(undo);
  else
    delete undo;
}

//===================================================================
//
// removeColumns
//
//-------------------------------------------------------------------

void removeColumns(const QVector<int> &columnIndexes, TXsheetHandle *xshHandle,
                  TObjectHandle *objHandle, TFxHandle *fxHandle,
                  bool doUndo = true) {
  TXsheet *xsh = xshHandle->getXsheet();
  int i;
  QMap<TStageObjectId, QList<TFxPort *>> columnFxConnection;
  std::set<TFx *> leafesFx;
  QList<TFx *> notTerminalColumns;
  for (i = columnIndexes.size() - 1; i >= 0; i--) {
    TStageObjectId id  = TStageObjectId::ColumnId(columnIndexes[i]);
    TXshColumnP column = xsh->getColumn(id.getIndex());
    if (!column) continue;

    TFx *columnFx = column->getFx();
    if (!columnFx) continue;

    int j;
    for (j = 0; j < columnFx->getOutputConnectionCount(); j++)
      columnFxConnection[id].append(columnFx->getOutputConnection(j));
    leafesFx.insert(columnFx);
    if (!xsh->getFxDag()->getTerminalFxs()->containsFx(columnFx))
      notTerminalColumns.append(columnFx);
  }

  std::vector<TFx *> fxsToKill;
  std::vector<TFx *> terminalFxsToKill;
  TFxSet *fxSet = xsh->getFxDag()->getInternalFxs();
  for (i = 0; i < fxSet->getFxCount(); i++) {
    TFx *fx = fxSet->getFx(i);
    if (canRemoveFx(leafesFx, fx)) {
      fxsToKill.push_back(fx);
      if (xsh->getFxDag()->getTerminalFxs()->containsFx(fx))
        terminalFxsToKill.push_back(fx);
    }
  }

  if (doUndo) {
    RemoveColumnsUndo *undo =
        new RemoveColumnsUndo(fxsToKill, terminalFxsToKill, columnFxConnection,
                              notTerminalColumns, xshHandle);
    TUndoManager::manager()->add(undo);
  }

  for (i = 0; i < (int)fxsToKill.size(); i++) {
    TFx *fx = fxsToKill[i];
    if (fx == fxHandle->getFx()) fxHandle->setFx(0);
    if (fx->getLinkedFx() != fx) fx->unlinkParams();
    int j, outputPortCount = fx->getOutputConnectionCount();
    for (j = outputPortCount - 1; j >= 0; j--) {
      TFxPort *port = fx->getOutputConnection(j);
      std::vector<TFx *>::iterator it =
          std::find(fxsToKill.begin(), fxsToKill.end(), port->getOwnerFx());
      std::set<TFx *>::iterator it2 =
          std::find(leafesFx.begin(), leafesFx.end(), port->getFx());
      if (it == fxsToKill.end() && it2 == leafesFx.end()) port->setFx(0);
    }
    fxSet->removeFx(fx);
    xsh->getFxDag()->getTerminalFxs()->removeFx(fx);
  }

  for (i = columnIndexes.size() - 1; i >= 0; i--)
    removeStageObjectNode(TStageObjectId::ColumnId(columnIndexes[i]), xshHandle,
                          objHandle, fxHandle, doUndo);
}

//===================================================================
//
// removeSpline
//
//-------------------------------------------------------------------

void removeSpline(TStageObjectSpline *spline, TXsheetHandle *xshHandle,
                  TObjectHandle *objHandle, bool doUndo = true) {
  if (doUndo)
    TUndoManager::manager()->add(new RemoveSplineUndo(spline, xshHandle));
  TStageObjectTree *pegbarTree = xshHandle->getXsheet()->getStageObjectTree();
  for (int i = 0; i < pegbarTree->getStageObjectCount(); i++) {
    TStageObject *pegbar = pegbarTree->getStageObject(i);
    if (pegbar->getSpline() == spline) {
      pegbar->setSpline(0);
      if (pegbar->getId() == objHandle->getObjectId())
        objHandle->setIsSpline(false);
    }
  }

  pegbarTree->removeSpline(spline);
  // xshHandle->notifyXsheetChanged();
}

void removeLink(const QPair<TStageObjectId, TStageObjectId> &link,
                TXsheetHandle *xshHandle, TObjectHandle *objHandle,
                bool doUndo = true) {
  TStageObjectTree *objTree = xshHandle->getXsheet()->getStageObjectTree();
  if (link.first ==
      link.second) {  // is a link connecting a spline with an object
    TStageObject *object = objTree->getStageObject(link.first, false);
    if (!object) return;
    TStageObjectSpline *spline = object->getSpline();
    assert(spline);
    object->setSpline(0);
    if (objHandle->getObjectId() == link.first) objHandle->setIsSpline(false);
    if (doUndo) {
      TUndo *undo =
          new RemoveSplineLinkUndo(link.first, spline, xshHandle, objHandle);
      TUndoManager::manager()->add(undo);
    }
  } else {
    TStageObject *object       = objTree->getStageObject(link.first, false);
    TStageObject *parentObject = objTree->getStageObject(link.second, false);
    if (!object || !parentObject || object->isGrouped() ||
        parentObject->isGrouped())
      return;
    assert(object->getParent() == parentObject->getId());
    TStageObjectCmd::setParent(object->getId(), TStageObjectId::NoneId, "",
                               xshHandle, doUndo);
  }
}

}  // namespace

//===================================================================
//
// SetAttributeUndo & sons
//
//-------------------------------------------------------------------

namespace {

//-------------------------------------------------------------------

template <class T>
class SetAttributeUndo : public TUndo {
  TStageObjectId m_id;
  T m_oldValue, m_newValue;
  TXsheetHandle *m_xshHandle;

public:
  SetAttributeUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                   T oldValue, T newValue)
      : m_id(id)
      , m_xshHandle(xshHandle)
      , m_oldValue(oldValue)
      , m_newValue(newValue) {}

  TStageObjectId getId() const { return m_id; }
  TStageObject *getStageObject() const {
    TStageObject *pegbar = m_xshHandle->getXsheet()->getStageObject(m_id);
    assert(pegbar);
    return pegbar;
  }
  virtual void setAttribute(TStageObject *pegbar, T value) const = 0;

  void setAttribute(T value) const {
    TStageObject *pegbar = getStageObject();
    if (pegbar) setAttribute(pegbar, value);
  }
  void undo() const override {
    setAttribute(m_oldValue);
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    setAttribute(m_newValue);
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }

  int getHistoryType() override { return HistoryType::Unidentified; }
  QString getHistoryString() override {
    return QString("%1 %2 : %3 -> %4")
        .arg(getActionName())
        .arg(QString::fromStdString(getId().toString()))
        .arg(getStringFromValue(m_oldValue))
        .arg(getStringFromValue(m_newValue));
  }
  virtual QString getActionName() { return QString(); }
  virtual QString getStringFromValue(T value) { return QString(); }
};

//-------------------------------------------------------------------

class StageObjectRenameUndo final : public SetAttributeUndo<std::string> {
public:
  StageObjectRenameUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                        std::string oldName, std::string newName)
      : SetAttributeUndo<std::string>(id, xshHandle, oldName, newName) {}
  void setAttribute(TStageObject *pegbar, std::string name) const override {
    pegbar->setName(name);
  }
  QString getActionName() override { return QString("Rename Object"); }
  QString getStringFromValue(std::string value) override {
    return QString::fromStdString(value);
  }
};

//-------------------------------------------------------------------

class ResetOffsetUndo final : public SetAttributeUndo<TPointD> {
public:
  ResetOffsetUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                  const TPointD &oldOffset)
      : SetAttributeUndo<TPointD>(id, xshHandle, oldOffset, TPointD()) {}
  void setAttribute(TStageObject *pegbar, TPointD offset) const override {
    pegbar->setOffset(offset);
  }
  QString getActionName() override { return QString("Reset Center"); }
  QString getStringFromValue(TPointD value) override {
    return QString("(%1,%2)")
        .arg(QString::number(value.x))
        .arg(QString::number(value.y));
  }
};

//-------------------------------------------------------------------

class ResetCenterAndOffsetUndo final : public SetAttributeUndo<TPointD> {
public:
  ResetCenterAndOffsetUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                           const TPointD &oldOffset)
      : SetAttributeUndo<TPointD>(id, xshHandle, oldOffset, TPointD()) {}
  void setAttribute(TStageObject *pegbar, TPointD offset) const override {
    pegbar->setCenterAndOffset(offset, offset);
  }
  QString getActionName() override { return QString("Reset Center"); }
  QString getStringFromValue(TPointD value) override {
    return QString("(%1,%2)")
        .arg(QString::number(value.x))
        .arg(QString::number(value.y));
  }
};

//-------------------------------------------------------------------

class SetHandleUndo final : public SetAttributeUndo<std::string> {
  TPointD m_center, m_offset;
  TXsheetHandle *m_xshHandle;

public:
  SetHandleUndo(const TStageObjectId &id, std::string oldHandle,
                std::string newHandle, TXsheetHandle *xshHandle)
      : SetAttributeUndo<std::string>(id, xshHandle, oldHandle, newHandle)
      , m_xshHandle(xshHandle) {
    TStageObject *pegbar = getStageObject();
    if (pegbar) pegbar->getCenterAndOffset(m_center, m_offset);
  }
  void setAttribute(TStageObject *pegbar, std::string handle) const override {
    pegbar->setHandle(handle);
  }
  void undo() const override {
    SetAttributeUndo<std::string>::undo();
    TStageObject *pegbar = getStageObject();
    if (pegbar) pegbar->setCenterAndOffset(m_center, m_offset);
    m_xshHandle->notifyXsheetChanged();
  }
  QString getActionName() override { return QString("Set Handle"); }
  QString getStringFromValue(std::string value) override {
    return QString::fromStdString(value);
  }
};

//-------------------------------------------------------------------

class SetParentHandleUndo final : public SetAttributeUndo<std::string> {
public:
  SetParentHandleUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                      std::string oldHandle, std::string newHandle)
      : SetAttributeUndo<std::string>(id, xshHandle, oldHandle, newHandle) {}
  void setAttribute(TStageObject *pegbar, std::string handle) const override {
    pegbar->setParentHandle(handle);
  }
  QString getActionName() override { return QString("Set Parent Handle"); }
  QString getStringFromValue(std::string value) override {
    return QString::fromStdString(value);
  }
};

//-------------------------------------------------------------------

typedef std::pair<TStageObjectId, std::string> ParentIdAndHandle;

class SetParentUndo final : public SetAttributeUndo<ParentIdAndHandle> {
public:
  SetParentUndo(const TStageObjectId &id, TXsheetHandle *xshHandle,
                TStageObjectId oldParentId, std::string oldParentHandle,
                TStageObjectId newParentId, std::string newParentHandle)
      : SetAttributeUndo<ParentIdAndHandle>(
            id, xshHandle, ParentIdAndHandle(oldParentId, oldParentHandle),
            ParentIdAndHandle(newParentId, newParentHandle)) {}
  void setAttribute(TStageObject *pegbar,
                    ParentIdAndHandle parent) const override {
    pegbar->setParent(parent.first);
    pegbar->setParentHandle(parent.second);
  }
  QString getActionName() override { return QString("Set Parent Handle"); }
  QString getStringFromValue(ParentIdAndHandle value) override {
    return QString("(%1,%2)")
        .arg(QString::fromStdString(value.first.toString()))
        .arg(QString::fromStdString(value.second));
  }
};

//-------------------------------------------------------------------

class ResetPositionUndo final : public TUndo {
  TXsheetHandle *m_xshHandle;
  TStageObjectId m_id;
  TPointD m_center, m_offset;
  std::vector<TDoubleKeyframe> m_xKeyframes, m_yKeyframes;

  void saveKeyframes(std::vector<TDoubleKeyframe> &keyframes,
                     const TDoubleParam *param) {
    int n = param->getKeyframeCount();
    if (n == 0)
      keyframes.clear();
    else {
      keyframes.resize(n);
      for (int i = 0; i < n; i++) keyframes[i] = param->getKeyframe(i);
    }
  }

  void deleteAllKeyframes(TDoubleParam *param) const {
    while (param->getKeyframeCount() > 0)
      param->deleteKeyframe(param->keyframeIndexToFrame(0));
  }
  void restoreKeyframes(TDoubleParam *param,
                        const std::vector<TDoubleKeyframe> &keyframes) const {
    deleteAllKeyframes(param);
    for (int i = 0; i < (int)keyframes.size(); i++)
      param->setKeyframe(keyframes[i]);
  }

  TStageObject *getStageObject() const {
    return m_xshHandle->getXsheet()->getStageObject(m_id);
  }

public:
  ResetPositionUndo(const TStageObjectId &id, TXsheetHandle *xshHandle)
      : m_xshHandle(xshHandle), m_id(id) {
    TStageObject *stageObject = getStageObject();
    if (stageObject) {
      stageObject->getCenterAndOffset(m_center, m_offset);
      saveKeyframes(m_xKeyframes, stageObject->getParam(TStageObject::T_X));
      saveKeyframes(m_yKeyframes, stageObject->getParam(TStageObject::T_Y));
    }
  }

  void undo() const override {
    TStageObject *stageObject = getStageObject();
    if (!stageObject) return;
    stageObject->setCenterAndOffset(m_center, m_offset);
    restoreKeyframes(stageObject->getParam(TStageObject::T_X), m_xKeyframes);
    restoreKeyframes(stageObject->getParam(TStageObject::T_Y), m_yKeyframes);
    m_xshHandle->notifyXsheetChanged();
  }
  void redo() const override {
    TStageObject *stageObject = getStageObject();
    if (!stageObject) return;
    stageObject->setCenterAndOffset(TPointD(0, 0), TPointD(0, 0));
    deleteAllKeyframes(stageObject->getParam(TStageObject::T_X));
    deleteAllKeyframes(stageObject->getParam(TStageObject::T_Y));
    m_xshHandle->notifyXsheetChanged();
  }
  int getSize() const override {
    return sizeof(*this) + sizeof(TDoubleKeyframe) *
                               (m_xKeyframes.size() + m_yKeyframes.size());
  }
};

//-------------------------------------------------------------------

}  // namespace

//===================================================================
//
// pegbar rename
//
//-------------------------------------------------------------------

void TStageObjectCmd::rename(const TStageObjectId &id, std::string name,
                             TXsheetHandle *xshHandle) {
  TStageObject *pegbar = xshHandle->getXsheet()->getStageObject(id);
  if (!pegbar) return;
  std::string oldName = pegbar->getName();
  if (oldName == name) return;
  pegbar->setName(name);
  TUndoManager::manager()->add(
      new StageObjectRenameUndo(id, xshHandle, oldName, name));
}

//===================================================================
//
// resetOffset
//
//-------------------------------------------------------------------

void TStageObjectCmd::resetOffset(const TStageObjectId &id,
                                  TXsheetHandle *xshHandle) {
  TStageObject *peg = xshHandle->getXsheet()->getStageObject(id);
  if (!peg) return;
  TPointD oldOffset = peg->getOffset();
  peg->setOffset(TPointD());
  TUndoManager::manager()->add(new ResetOffsetUndo(id, xshHandle, oldOffset));
  xshHandle->notifyXsheetChanged();
  // TNotifier::instance()->notify(TStageChange());
}

//===================================================================
//
// resetCenterAndOffset
//
//-------------------------------------------------------------------

void TStageObjectCmd::resetCenterAndOffset(const TStageObjectId &id,
                                           TXsheetHandle *xshHandle) {
  TStageObject *peg = xshHandle->getXsheet()->getStageObject(id);
  if (!peg) return;
  TPointD oldOffset = peg->getOffset();
  peg->setCenterAndOffset(TPointD(), TPointD());
  TUndoManager::manager()->add(
      new ResetCenterAndOffsetUndo(id, xshHandle, oldOffset));
  xshHandle->notifyXsheetChanged();
}

//===================================================================
//
// resetPosition
//
//-------------------------------------------------------------------

void TStageObjectCmd::resetPosition(const TStageObjectId &id,
                                    TXsheetHandle *xshHandle) {
  TStageObject *obj = xshHandle->getXsheet()->getStageObject(id);
  if (!obj) return;
  TUndo *undo = new ResetPositionUndo(id, xshHandle);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

//===================================================================
//
// setHandle
//
//-------------------------------------------------------------------

void TStageObjectCmd::setHandle(const TStageObjectId &id, std::string handle,
                                TXsheetHandle *xshHandle) {
  TStageObject *peg = xshHandle->getXsheet()->getStageObject(id);
  if (!peg) return;
  std::string oldHandle = peg->getHandle();
  TUndoManager::manager()->add(
      new SetHandleUndo(id, oldHandle, handle, xshHandle));
  peg->setHandle(handle);
}

//===================================================================
//
// setParentHandle
//
//-------------------------------------------------------------------

void TStageObjectCmd::setParentHandle(const std::vector<TStageObjectId> &ids,
                                      std::string handle,
                                      TXsheetHandle *xshHandle) {
  for (int i = 0; i < (int)ids.size(); i++) {
    TStageObjectId id = ids[i];
    TStageObject *peg = xshHandle->getXsheet()->getStageObject(id);
    if (!peg) continue;
    std::string oldHandle = peg->getParentHandle();
    peg->setParentHandle(handle);
    TUndoManager::manager()->add(
        new SetParentHandleUndo(id, xshHandle, oldHandle, handle));
  }
}

//===================================================================
//
// setParent
//
//-------------------------------------------------------------------

void TStageObjectCmd::setParent(const TStageObjectId &id,
                                TStageObjectId parentId,
                                std::string parentHandle,
                                TXsheetHandle *xshHandle, bool doUndo) {
  if (parentId == TStageObjectId::NoneId) {
    if (id.isColumn() || id.isPegbar()) {
      parentId     = TStageObjectId::TableId;
      parentHandle = "B";
    }
  }

  TStageObject *stageObject = xshHandle->getXsheet()->getStageObject(id);
  if (!stageObject) return;
  TStageObjectId oldParentId = stageObject->getParent();
  std::string oldParentHandle;
  if (oldParentId != TStageObjectId::NoneId)
    oldParentHandle = stageObject->getParentHandle();

  stageObject->setParent(parentId);
  stageObject->setParentHandle(parentHandle);
  if (doUndo) {
    TUndoManager *undoManager = TUndoManager::manager();
    TUndoManager::manager()->add(new SetParentUndo(
        id, xshHandle, oldParentId, oldParentHandle, parentId, parentHandle));
  }
}

//===================================================================
//
// setSplineParent
//
//-------------------------------------------------------------------

void TStageObjectCmd::setSplineParent(TStageObjectSpline *spline,
                                      TStageObject *parentObj,
                                      TXsheetHandle *xshHandle) {
  TUndoManager::manager()->add(
      new SplineLinkUndo(parentObj->getId(), spline, xshHandle));
  parentObj->setSpline(spline);
}

//===================================================================
//
// addNewCamera
//
//-------------------------------------------------------------------

void TStageObjectCmd::addNewCamera(TXsheetHandle *xshHandle,
                                   TObjectHandle *objHandle,
                                   QPointF initialPos) {
  TXsheet *xsh           = xshHandle->getXsheet();
  int cameraIndex        = 0;
  TStageObjectTree *tree = xsh->getStageObjectTree();
  TStageObjectId cameraId;
  for (;;) {
    cameraId = TStageObjectId::CameraId(cameraIndex);
    if (tree->getStageObject(cameraId, false) != 0) {
      cameraIndex++;
      continue;
    }
    break;
  }
  // crea la nuova camera
  TStageObject *newCameraPegbar = xsh->getStageObject(cameraId);

  // make the new peg at the cursor position
  if (!initialPos.isNull())
    newCameraPegbar->setDagNodePos(TPointD(initialPos.x(), initialPos.y()));

  // settings uguali a quelli della camera corrente
  TCamera *currentCamera        = tree->getCurrentCamera();
  *newCameraPegbar->getCamera() = *currentCamera;

  TUndoManager::manager()->add(
      new NewCameraUndo(cameraId, xshHandle, objHandle));
  xshHandle->notifyXsheetChanged();
}

//===================================================================
//
// addNewPegbar
//
//-------------------------------------------------------------------

void TStageObjectCmd::addNewPegbar(TXsheetHandle *xshHandle,
                                   TObjectHandle *objHandle,
                                   QPointF initialPos) {
  TXsheet *xsh = xshHandle->getXsheet();
  // crea la nuova pegbar
  TStageObjectTree *pTree = xsh->getStageObjectTree();
  int pegbarIndex         = 0;
  while (pTree->getStageObject(TStageObjectId::PegbarId(pegbarIndex), false))
    pegbarIndex++;
  TStageObjectId id = TStageObjectId::PegbarId(pegbarIndex);

  TStageObject *obj = pTree->getStageObject(id, true);
  if (!initialPos.isNull())
    obj->setDagNodePos(TPointD(initialPos.x(), initialPos.y()));

  TUndoManager::manager()->add(new NewPegbarUndo(id, xshHandle, objHandle));
  xshHandle->notifyXsheetChanged();
}

//===================================================================
//
// setAsActiveCamera
//
//-------------------------------------------------------------------

void TStageObjectCmd::setAsActiveCamera(TXsheetHandle *xshHandle,
                                        TObjectHandle *objHandle) {
  TXsheet *xsh                   = xshHandle->getXsheet();
  TStageObjectId currentPegbarId = objHandle->getObjectId();
  assert(currentPegbarId.isCamera());
  TStageObjectId newCameraId = currentPegbarId;
  TStageObjectId oldCameraId = xsh->getStageObjectTree()->getCurrentCameraId();

  xsh->getStageObjectTree()->setCurrentCameraId(newCameraId);
  // make the preview camera same as the final render camera
  xsh->getStageObjectTree()->setCurrentPreviewCameraId(newCameraId);

  TUndoManager::manager()->add(
      new SetActiveCameraUndo(oldCameraId, newCameraId, xshHandle));

  xshHandle->notifyXsheetChanged();
}

//===================================================================
//
// addNewSpline
//
//-------------------------------------------------------------------

void TStageObjectCmd::addNewSpline(TXsheetHandle *xshHandle,
                                   TObjectHandle *objHandle,
                                   TColumnHandle *colHandle,
                                   QPointF initialPos) {
  TXsheet *xsh               = xshHandle->getXsheet();
  TStageObjectSpline *spline = xsh->getStageObjectTree()->createSpline();

  if (!initialPos.isNull())
    spline->setDagNodePos(TPointD(initialPos.x(), initialPos.y()));

  TStageObjectId objId = objHandle->getObjectId();
  if (objId == TStageObjectId::NoneId) {
    int col = colHandle->getColumnIndex();
    if (col >= 0) objId = TStageObjectId::ColumnId(col);
  }
  if (objId != TStageObjectId::NoneId) {
    TStageObject *pegbar = xsh->getStageObject(objId);
    assert(pegbar);
    pegbar->setSpline(spline);
    TUndoManager::manager()->add(new NewSplineUndo(objId, spline, xshHandle));
  }
  xshHandle->notifyXsheetChanged();
}

//===================================================================
//
// deleteSelection
//
//-------------------------------------------------------------------

void TStageObjectCmd::deleteSelection(
    const std::vector<TStageObjectId> &objIds,
    const std::list<QPair<TStageObjectId, TStageObjectId>> &links,
    const std::list<int> &splineIds, TXsheetHandle *xshHandle,
    TObjectHandle *objHandle, TFxHandle *fxHandle, bool doUndo) {
  if (doUndo) TUndoManager::manager()->beginBlock();

  QVector<int> columnIndexes;
  QVector<int> pegbarIndexes;
  QVector<int> cameraIndexes;

  std::vector<TStageObjectId>::const_iterator it2;
  for (it2 = objIds.begin(); it2 != objIds.end(); it2++) {
    if (it2->isColumn()) columnIndexes.append(it2->getIndex());
    if (it2->isPegbar()) pegbarIndexes.append(it2->getIndex());
    if (it2->isCamera()) cameraIndexes.append(it2->getIndex());
  }
  if (!columnIndexes.isEmpty()) {
    std::sort(columnIndexes.begin(), columnIndexes.end());
  }
  if (!pegbarIndexes.isEmpty()) {
    std::sort(pegbarIndexes.begin(), pegbarIndexes.end());
  }
  if (!cameraIndexes.isEmpty()) {
    std::sort(cameraIndexes.begin(), cameraIndexes.end());
  }

  // remove all selected objects
  removeColumns(columnIndexes, xshHandle, objHandle, fxHandle, doUndo);
  int i;
  for (i = pegbarIndexes.size() - 1; i >= 0; i--)
    removeStageObjectNode(TStageObjectId::PegbarId(pegbarIndexes[i]), xshHandle,
                          objHandle, fxHandle, doUndo);
  for (i = cameraIndexes.size() - 1; i >= 0; i--)
    removeStageObjectNode(TStageObjectId::CameraId(cameraIndexes[i]), xshHandle,
                          objHandle, fxHandle, doUndo);

  std::list<QPair<TStageObjectId, TStageObjectId>>::const_iterator it1;
  for (it1 = links.begin(); it1 != links.end() && objIds.empty(); it1++)
    removeLink(*it1, xshHandle, objHandle, doUndo);

  std::list<int>::const_iterator it3;
  TStageObjectTree *objTree = xshHandle->getXsheet()->getStageObjectTree();
  for (it3 = splineIds.begin(); it3 != splineIds.end(); it3++) {
    int splineCount = objTree->getSplineCount();
    int i;
    for (i = 0; i < splineCount; i++) {
      TStageObjectSpline *spline = objTree->getSpline(i);
      if (spline->getId() == *it3) {
        removeSpline(spline, xshHandle, objHandle, doUndo);
        break;
      }
    }
  }
  if (doUndo) TUndoManager::manager()->endBlock();
  xshHandle->notifyXsheetChanged();
}

//===================================================================
//
// group
//
//-------------------------------------------------------------------

void TStageObjectCmd::group(const QList<TStageObjectId> ids,
                            TXsheetHandle *xshHandle) {
  TStageObjectTree *pegTree = xshHandle->getXsheet()->getStageObjectTree();
  int groupId               = pegTree->getNewGroupId();
  int i;
  QList<int> positions;
  for (i = 0; i < ids.size(); i++) {
    TStageObject *obj = pegTree->getStageObject(ids[i], false);
    if (obj) {
      int position = obj->setGroupId(groupId);
      obj->setGroupName(L"Group " + std::to_wstring(groupId));
      positions.append(position);
    }
  }
  TUndoManager::manager()->add(
      new UndoGroup(ids, groupId, positions, xshHandle));
}

//===================================================================
//
// ungroup
//
//-------------------------------------------------------------------

void TStageObjectCmd::ungroup(int groupId, TXsheetHandle *xshHandle) {
  TStageObjectTree *objTree = xshHandle->getXsheet()->getStageObjectTree();
  if (!objTree) return;
  QList<TStageObject *> objs;
  int i;
  for (i = 0; i < objTree->getStageObjectCount(); i++) {
    TStageObject *obj = objTree->getStageObject(i);
    if (!obj) continue;
    if (obj->getGroupId() == groupId) objs.push_back(obj);
  }
  QList<int> positions;
  UndoUngroup *undo = new UndoUngroup(objs, xshHandle);
  TUndoManager::manager()->add(undo);
  for (i = 0; i < objs.size(); i++) {
    TStageObject *obj = objs[i];
    if (obj) {
      obj->removeGroupName();
      int position = obj->removeGroupId();
      positions.append(position);
    }
  }
  undo->setStackPositions(positions);
}

//===================================================================
//
// renameGroup
//
//-------------------------------------------------------------------

void TStageObjectCmd::renameGroup(const QList<TStageObject *> objs,
                                  const std::wstring &name, bool fromEditor,
                                  TXsheetHandle *xshHandle) {
  std::wstring oldName;
  TStageObjectTree *pegTree = xshHandle->getXsheet()->getStageObjectTree();
  QList<int> positions;
  int i;
  for (i = 0; i < objs.size(); i++) {
    if (i == 0) oldName = objs[i]->getGroupName(fromEditor);
    int position = objs[i]->removeGroupName(fromEditor);
    objs[i]->setGroupName(name, position);
    positions.push_back(position);
  }
  TUndoManager::manager()->add(
      new UndoRenameGroup(objs, positions, name, oldName, xshHandle));
}

//===================================================================
//
// renameGroup
//
//-------------------------------------------------------------------

void TStageObjectCmd::duplicateObject(const QList<TStageObjectId> ids,
                                      TXsheetHandle *xshHandle) {
  TXsheet *xsh              = xshHandle->getXsheet();
  TStageObjectTree *objTree = xsh->getStageObjectTree();
  int i, objCount = ids.size();
  for (i = 0; i < objCount; i++) {
    TStageObjectId id = ids[i];
    TStageObject *obj = objTree->getStageObject(id, false);
    assert(obj);
    TStageObject *duplicatedObj = 0;
    if (id.isPegbar() || id.isCamera()) {
      int index = 0;
      TStageObjectId newId;
      for (;;) {
        newId = id.isPegbar() ? TStageObjectId::PegbarId(index)
                              : TStageObjectId::CameraId(index);
        if (objTree->getStageObject(newId, false)) {
          index++;
          continue;
        }
        break;
      }
      duplicatedObj              = xsh->getStageObject(newId);
      TStageObjectParams *params = obj->getParams();
      duplicatedObj->assignParams(params);
      delete params;
      if (id.isCamera()) *(duplicatedObj->getCamera()) = *(obj->getCamera());
    }
  }
  xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------------------

void TStageObjectCmd::enableSplineAim(TStageObject *obj, int state,
                                      TXsheetHandle *xshHandle) {
  UndoStatusChange *undo = new UndoStatusChange(obj, xshHandle);
  obj->enableAim(state != 2);
  TUndoManager::manager()->add(undo);
}

//-------------------------------------------------------------------

void TStageObjectCmd::enableSplineUppk(TStageObject *obj, bool toggled,
                                       TXsheetHandle *xshHandle) {
  UndoStatusChange *undo = new UndoStatusChange(obj, xshHandle);
  obj->enableUppk(toggled);
  TUndoManager::manager()->add(undo);
}
