

// TnzCore includes
#include "tconst.h"
#include "tundo.h"

// TnzBase includes
#include "tfx.h"
#include "tfxattributes.h"
#include "tparamcontainer.h"
#include "tparamset.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobject.h"
#include "toonz/tcolumnfx.h"
#include "toonz/fxcommand.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/fxdag.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tcamera.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/stageobjectsdata.h"
#include "historytypes.h"

// Toonz includes
#include "columncommand.h"
#include "menubarcommandids.h"
#include "celldata.h"
#include "tapp.h"
#include "columnselection.h"
#include "cellselection.h"

#include "subscenecommand.h"

//*****************************************************************************
//    Local namespace
//*****************************************************************************

namespace {

struct GroupData {
public:
  QStack<int> m_groupIds;
  QStack<std::wstring> m_groupNames;
  int m_editingGroup;

  GroupData(const QStack<int> &groupIds, const QStack<std::wstring> &groupNames,
            int editingGroup)
      : m_groupIds(groupIds)
      , m_groupNames(groupNames)
      , m_editingGroup(editingGroup) {}
};

//-----------------------------------------------------------------------------

// Zerary fxs and zerary COLUMN fxs are separate, and fx port connections
// are stored in the actual zerary fx.
TFx *getActualFx(TFx *fx) {
  TZeraryColumnFx *zeraryColumnFx = dynamic_cast<TZeraryColumnFx *>(fx);
  return zeraryColumnFx ? zeraryColumnFx->getZeraryFx() : fx;
}

//-----------------------------------------------------------------------------

void setFxParamToCurrentScene(TFx *fx, TXsheet *xsh) {
  for (int i = 0; i < fx->getParams()->getParamCount(); i++) {
    TParam *param = fx->getParams()->getParam(i);
    if (TDoubleParam *dp = dynamic_cast<TDoubleParam *>(param))
      xsh->getStageObjectTree()->setGrammar(dp);
    else if (dynamic_cast<TPointParam *>(param) ||
             dynamic_cast<TRangeParam *>(param) ||
             dynamic_cast<TPixelParam *>(param)) {
      TParamSet *paramSet = dynamic_cast<TParamSet *>(param);
      assert(paramSet);
      int f;
      for (f = 0; f < paramSet->getParamCount(); f++) {
        TDoubleParam *dp =
            dynamic_cast<TDoubleParam *>(paramSet->getParam(f).getPointer());
        if (!dp) continue;
        xsh->getStageObjectTree()->setGrammar(dp);
      }
    }
  }
}

//-----------------------------------------------------------------------------

std::vector<TStageObjectId> getRoots(const QList<TStageObjectId> &objIds,
                                     TXsheetHandle *xshHandle) {
  std::vector<TStageObjectId> roots;
  std::map<TStageObjectId, std::string> parentHandles;
  TStageObjectTree *pegTree = xshHandle->getXsheet()->getStageObjectTree();
  for (int i = 0; i < objIds.size(); i++) {
    TStageObject *obj       = pegTree->getStageObject(objIds.at(i), false);
    TStageObjectId parentId = obj->getParent();
    bool parentIsColumn     = parentId.isColumn() && !objIds.contains(parentId);
    std::string parentHandle = obj->getParentHandle();
    if (!parentIsColumn && !objIds.contains(parentId) &&
        (parentHandles.count(parentId) == 0 ||
         parentHandles[parentId] != parentHandle)) {
      parentHandles[parentId] = parentHandle;
      roots.push_back(parentId);
    }
  }
  return roots;
}

//-----------------------------------------------------------------------------

std::vector<TStageObjectId> isConnected(
    const std::set<int> &indices, const std::set<TStageObjectId> &pegbarIds,
    TXsheetHandle *xshHandle) {
  std::vector<TStageObjectId> roots;
  std::map<TStageObjectId, std::string> parentHandles;
  TStageObjectTree *pegTree = xshHandle->getXsheet()->getStageObjectTree();
  std::set<int>::const_iterator it;
  for (it = indices.begin(); it != indices.end(); it++) {
    TStageObjectId id        = TStageObjectId::ColumnId(*it);
    TStageObject *obj        = pegTree->getStageObject(id, false);
    TStageObjectId parentId  = obj->getParent();
    std::string parentHandle = obj->getParentHandle();
    bool parentIsColumn      = parentId.isColumn() &&
                          indices.find(parentId.getIndex()) != indices.end();
    if (!parentIsColumn && pegbarIds.find(parentId) == pegbarIds.end() &&
        (parentHandles.count(parentId) == 0 ||
         parentHandles[parentId] != parentHandle)) {
      parentHandles[parentId] = parentHandle;
      roots.push_back(parentId);
    }
  }
  std::set<TStageObjectId>::const_iterator it2;
  for (it2 = pegbarIds.begin(); it2 != pegbarIds.end(); it2++) {
    TStageObject *obj       = pegTree->getStageObject(*it2, false);
    TStageObjectId parentId = obj->getParent();
    bool parentIsColumn     = parentId.isColumn() &&
                          indices.find(parentId.getIndex()) != indices.end();
    std::string parentHandle = obj->getParentHandle();
    if (!parentIsColumn && pegbarIds.find(parentId) == pegbarIds.end() &&
        (parentHandles.count(parentId) == 0 ||
         parentHandles[parentId] != parentHandle)) {
      parentHandles[parentId] = parentHandle;
      roots.push_back(parentId);
    }
  }
  return roots;
}

//-----------------------------------------------------------------------------

std::map<TFx *, std::vector<TFxPort *>> isConnected(
    const std::set<int> &indices, const std::set<TFx *> &internalFxs,
    TXsheetHandle *xshHandle) {
  TXsheet *xsh = xshHandle->getXsheet();
  std::set<int>::const_iterator it;
  std::map<TFx *, std::vector<TFxPort *>> roots;
  for (it = indices.begin(); it != indices.end(); it++) {
    TFx *fx = xsh->getColumn(*it)->getFx();
    int i, outputCount = fx->getOutputConnectionCount();
    for (i = 0; i < outputCount; i++) {
      TFx *outFx = fx->getOutputConnection(i)->getOwnerFx();
      if (internalFxs.find(outFx) == internalFxs.end())
        roots[fx].push_back(fx->getOutputConnection(i));
    }
  }
  std::set<TFx *>::const_iterator it2;
  for (it2 = internalFxs.begin(); it2 != internalFxs.end(); it2++) {
    int i, outputCount = (*it2)->getOutputConnectionCount();
    for (i = 0; i < outputCount; i++) {
      TFx *outFx = (*it2)->getOutputConnection(i)->getOwnerFx();
      if (internalFxs.find(outFx) == internalFxs.end())
        roots[*it2].push_back((*it2)->getOutputConnection(i));
    }
  }
  return roots;
}

//-----------------------------------------------------------------------------

// returns true if the column indexed with col contains only the childLevel.
// if not, false is returned and in from and to is put the frame range contained
// the frame indexed with row.
bool mustRemoveColumn(int &from, int &to, TXshChildLevel *childLevel,
                      TXsheet *xsh, int col, int row) {
  bool removeColumn = true;
  bool rangeFound   = false;
  from              = -1;
  to                = -1;
  int i, r0, r1;
  xsh->getColumn(col)->getRange(r0, r1);
  for (i = r0; i <= r1; i++) {
    TXshCell cell       = xsh->getCell(i, col);
    TXshChildLevel *app = cell.getChildLevel();
    if (app != childLevel) {
      removeColumn = false;
      if (from != -1 && to != -1) {
        rangeFound            = from <= row && row <= to;
        if (!rangeFound) from = to = -1;
      }
      continue;
    }
    if (from == -1 && !rangeFound) {
      from = to = i;
    } else if (from != -1 && !rangeFound) {
      to = i;
    }
  }
  return removeColumn;
}

//-----------------------------------------------------------------------------

class FxConnections {
  bool m_isTerminal;
  QMap<int, TFx *> m_inputLinks;
  QMap<TFx *, int> m_outputLinks;
  QList<TFx *> m_notTerminalInputFxs;

public:
  FxConnections() {}
  ~FxConnections() {}

  void setIsTerminal(bool isTerminal) { m_isTerminal = isTerminal; }
  void setInputLink(int portIndex, TFx *inputFx) {
    m_inputLinks[portIndex] = inputFx;
  }
  void setOutputLink(TFx *outputFx, int portIndex) {
    m_outputLinks[outputFx] = portIndex;
  }
  void addNotTerminalInputFx(TFx *fx) { m_notTerminalInputFxs.append(fx); }
  QMap<int, TFx *> getInputLinks() { return m_inputLinks; }
  QMap<TFx *, int> getOutputLinks() { return m_outputLinks; }
  QList<TFx *> getNotTerminalInputFxs() { return m_notTerminalInputFxs; }
  bool isTerminal() { return m_isTerminal; }
};

//-----------------------------------------------------------------------------

void getFxConnections(QMap<TFx *, FxConnections> &fxConnetcions,
                      const set<TFx *> &fxs, TXsheet *xsh) {
  TFxSet *terminalFxs = xsh->getFxDag()->getTerminalFxs();
  set<TFx *>::const_iterator it;
  for (it = fxs.begin(); it != fxs.end(); it++) {
    TFx *fx = (*it);
    FxConnections connections;
    connections.setIsTerminal(terminalFxs->containsFx(fx));
    int i;
    for (i = 0; i < fx->getInputPortCount(); i++) {
      TFx *inputFx = fx->getInputPort(i)->getFx();
      connections.setInputLink(i, inputFx);
      if (connections.isTerminal()) connections.addNotTerminalInputFx(inputFx);
    }
    for (i = 0; i < fx->getOutputConnectionCount(); i++) {
      TFx *outputFx = fx->getOutputConnection(i)->getOwnerFx();
      int j, inputCount = outputFx->getInputPortCount();
      if (inputCount == 0) continue;
      for (j = 0; j < inputCount; j++) {
        TFx *inputFx = outputFx->getInputPort(j)->getFx();
        if (inputFx == fx) break;
      }
      connections.setOutputLink(outputFx, j);
    }
    fxConnetcions[fx] = connections;
  }
}

//-----------------------------------------------------------------------------

void changeSaveSubXsheetAsCommand() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  bool isSubxsheet  = scene->getChildStack()->getAncestorCount() > 0;
  CommandManager::instance()->enable(MI_SaveSubxsheetAs, isSubxsheet);
}

//-----------------------------------------------------------------------------

void getColumnOutputConnections(
    const std::set<int> &indices,
    QMap<TFx *, QList<TFxPort *>> &columnOutputConnections) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::set<int>::const_iterator it;
  for (it = indices.begin(); it != indices.end(); it++) {
    int i              = *it;
    TXshColumn *column = xsh->getColumn(i);
    if (!column) continue;
    TFx *columnFx = column->getFx();
    if (!columnFx) continue;
    QList<TFxPort *> ports;
    int j;
    for (j = 0; j < columnFx->getOutputConnectionCount(); j++)
      ports.append(columnFx->getOutputConnection(j));
    columnOutputConnections[columnFx] = ports;
  }
}

//-----------------------------------------------------------------------------

void getChildren(const std::set<int> &indices,
                 QMap<TStageObjectId, QList<TStageObjectId>> &children) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::set<int>::const_iterator it;
  for (it = indices.begin(); it != indices.end(); it++) {
    TStageObjectId id = TStageObjectId::ColumnId(*it);
    TStageObject *obj = xsh->getStageObjectTree()->getStageObject(id, false);
    assert(obj);
    if (obj && !obj->getChildren().empty()) {
      std::list<TStageObject *> childrenObj = obj->getChildren();
      std::list<TStageObject *>::iterator it2;
      for (it2 = childrenObj.begin(); it2 != childrenObj.end(); it2++) {
        TStageObjectId childId = (*it2)->getId();
        children[id].append(childId);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void getParents(const std::set<int> &indices,
                QMap<TStageObjectId, TStageObjectId> &parents) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::set<int>::const_iterator it;
  for (it = indices.begin(); it != indices.end(); it++) {
    TStageObjectId id = TStageObjectId::ColumnId(*it);
    TStageObject *obj = xsh->getStageObjectTree()->getStageObject(id, false);
    assert(obj);
    if (obj) parents[id] = obj->getParent();
  }
}

//-----------------------------------------------------------------------------

void setColumnOutputConnections(
    const QMap<TFx *, QList<TFxPort *>> &columnOutputConnections) {
  QMap<TFx *, QList<TFxPort *>>::const_iterator it;
  for (it = columnOutputConnections.begin();
       it != columnOutputConnections.end(); it++) {
    TFx *columnFx          = it.key();
    QList<TFxPort *> ports = it.value();
    int i;
    for (i = 0; i < ports.size(); i++) ports.at(i)->setFx(columnFx);
  }
}

//-----------------------------------------------------------------------------

void setChildren(const QMap<TStageObjectId, QList<TStageObjectId>> &children) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  QMap<TStageObjectId, QList<TStageObjectId>>::const_iterator it;
  for (it = children.begin(); it != children.end(); it++) {
    TStageObjectId id                 = it.key();
    QList<TStageObjectId> childrenIds = it.value();
    QList<TStageObjectId>::iterator it2;
    for (it2 = childrenIds.begin(); it2 != childrenIds.end(); it2++)
      xsh->setStageObjectParent(*it2, id);
  }
}

//-----------------------------------------------------------------------------

void setParents(const QMap<TStageObjectId, TStageObjectId> &parents) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  QMap<TStageObjectId, TStageObjectId>::const_iterator it;
  for (it = parents.begin(); it != parents.end(); it++)
    xsh->setStageObjectParent(it.key(), it.value());
}
//-----------------------------------------------------------------------------

bool isConnectedToXsheet(TFx *fx) {
  if (!fx) return false;
  int i, count = fx->getInputPortCount();
  bool xsheetConnected = false;
  for (i = 0; i < count; i++) {
    TFx *inputFx = fx->getInputPort(i)->getFx();
    if (dynamic_cast<TXsheetFx *>(inputFx)) return true;
    xsheetConnected = xsheetConnected || isConnectedToXsheet(inputFx);
  }
  return xsheetConnected;
}

//-----------------------------------------------------------------------------

// clones in outerDag fx and all effects contained in the subtree with root in
// fx
void bringFxOut(TFx *fx, QMap<TFx *, QPair<TFx *, int>> &fxs, FxDag *outerDag,
                const GroupData &fxGroupData) {
  if (!fx) return;

  TFx *actualFx = getActualFx(fx);
  if (fx != actualFx) {
    // Zerary Column case
    TFx *outerFx = getActualFx(fxs[fx].first);

    int i, inputPortsCount = actualFx->getInputPortCount();
    for (i = 0; i < inputPortsCount; ++i) {
      TFx *inputFx = actualFx->getInputPort(i)->getFx();
      if (!inputFx) continue;

      bringFxOut(inputFx, fxs, outerDag, fxGroupData);
      outerFx->getInputPort(i)->setFx(fxs[inputFx].first);
    }

    return;
  }

  // Common case
  if (fxs.contains(fx)) return;

  TFx *outerFx     = fx->clone(false);
  TOutputFx *outFx = dynamic_cast<TOutputFx *>(outerFx);
  if (!outFx) {
    outerDag->getInternalFxs()->addFx(outerFx);
    outerDag->assignUniqueId(outerFx);
  } else
    outerDag->addOutputFx(outFx);

  TFxAttributes *attr = outerFx->getAttributes();
  attr->setDagNodePos(fx->getAttributes()->getDagNodePos());

  // Put in the right Fx group if needed
  attr->removeFromAllGroup();
  if (!fxGroupData.m_groupIds.empty()) {
    int i;
    for (i = 0; i < fxGroupData.m_groupIds.size(); i++) {
      attr->setGroupId(fxGroupData.m_groupIds[i]);
      attr->setGroupName(fxGroupData.m_groupNames[i]);
    }
    for (i = 0;
         i < fxGroupData.m_groupIds.size() && fxGroupData.m_editingGroup >= 0;
         i++)
      attr->editGroup();
  }

  int columnIndex = -1;
  bool firstIndex = true;

  int i, inputPortsCount = fx->getInputPortCount();
  for (i = 0; i < inputPortsCount; ++i) {
    TFx *inputFx = fx->getInputPort(i)->getFx();
    if (!inputFx) continue;

    bringFxOut(inputFx, fxs, outerDag, fxGroupData);
    outerFx->getInputPort(i)->setFx(fxs[inputFx].first);

    if (firstIndex) {
      columnIndex = fxs[inputFx].second;
      firstIndex  = false;
    }
  }

  fxs[fx] = QPair<TFx *, int>(outerFx, columnIndex);
}

//-----------------------------------------------------------------------------

TFx *explodeFxSubTree(TFx *innerFx, QMap<TFx *, QPair<TFx *, int>> &fxs,
                      FxDag *outerDag, TXsheet *outerXsheet, FxDag *innerDag,
                      const GroupData &fxGroupData,
                      const std::vector<TFxPort *> &outPorts) {
  TXsheetFx *xsheetFx = dynamic_cast<TXsheetFx *>(innerFx);
  if (!xsheetFx) {
    if (innerDag->getCurrentOutputFx() == innerFx)
      innerFx = innerFx->getInputPort(0)->getFx();
    if (!innerFx) return 0;
    bringFxOut(innerFx, fxs, outerDag, fxGroupData);
    TOutputFx *outFx = dynamic_cast<TOutputFx *>(innerFx);
    if (outFx)
      return fxs[outFx->getInputPort(0)->getFx()].first;
    else
      return fxs[innerFx].first;
  } else {
    TFxSet *innerTerminals = innerDag->getTerminalFxs();
    int i, terminalCount = innerTerminals->getFxCount();
    if (!terminalCount) return 0;
    QMultiMap<int, TFx *> sortedFx;
    for (i = 0; i < terminalCount; i++) {
      TFx *terminalFx = innerTerminals->getFx(i);
      bringFxOut(terminalFx, fxs, outerDag, fxGroupData);
      sortedFx.insert(fxs[terminalFx].second, fxs[terminalFx].first);
    }
    if (outPorts.empty()) return 0;
    TFx *root = sortedFx.begin().value();
    QMultiMap<int, TFx *>::iterator it = sortedFx.begin();
    outerDag->removeFromXsheet(it.value());
    for (++it; it != sortedFx.end(); ++it) {
      TFx *fx = it.value();
      assert(fx);
      TFx *overFx = TFx::create("overFx");
      outerDag->assignUniqueId(overFx);
      outerDag->getInternalFxs()->addFx(overFx);
      setFxParamToCurrentScene(overFx, outerXsheet);
      overFx->getInputPort(0)->setFx(fx);
      overFx->getInputPort(1)->setFx(root);
      outerDag->removeFromXsheet(fx);
      TPointD pos = root->getAttributes()->getDagNodePos();
      overFx->getAttributes()->setDagNodePos((pos == TConst::nowhere)
                                                 ? TConst::nowhere
                                                 : TPointD(pos.x + 150, pos.y));
      root = overFx;
      // e' brutto... mi serve solo per mettere gli over dentro il gruppo
      fxs[overFx] = QPair<TFx *, int>(overFx, -1);
    }
    return root;
  }
}

//-----------------------------------------------------------------------------

// brings in xsh obj and all objects contained in the subtree with root in obj
void bringObjectOut(TStageObject *obj, TXsheet *xsh,
                    QMap<TStageObjectId, TStageObjectId> &ids,
                    QMap<TStageObjectSpline *, TStageObjectSpline *> &splines,
                    QList<TStageObject *> &pegObjects, int &pegbarIndex,
                    const GroupData &objGroupData, int groupId) {
  if (!obj->hasChildren()) return;
  std::list<TStageObject *> children = obj->getChildren();
  std::list<TStageObject *>::iterator it;
  for (it = children.begin(); it != children.end(); it++) {
    TStageObjectId id = (*it)->getId();
    if (id.isColumn()) continue;
    assert(id.isPegbar());
    pegbarIndex++;
    TStageObjectId outerId = TStageObjectId::PegbarId(pegbarIndex);
    TStageObject *outerObj =
        xsh->getStageObjectTree()->getStageObject(outerId, true);
    outerObj->setDagNodePos((*it)->getDagNodePos());
    ids[id] = outerId;
    pegObjects.append(outerObj);
    outerObj->addRef();  // undo make release!!!
    TStageObjectParams *params = (*it)->getParams();
    if (params->m_spline) {
      if (splines.contains(params->m_spline))
        params->m_spline = splines[params->m_spline];
      else {
        TStageObjectSpline *spline = params->m_spline->clone();
        splines[params->m_spline]  = spline;
        xsh->getStageObjectTree()->assignUniqueSplineId(spline);
        xsh->getStageObjectTree()->insertSpline(spline);
        params->m_spline = spline;
      }
    }
    outerObj->assignParams(params);
    delete params;
    outerObj->setParent(ids[obj->getId()]);
    outerObj->removeFromAllGroup();
    if (groupId != -1) {
      outerObj->setGroupId(groupId);
      outerObj->setGroupName(L"Group " + std::to_wstring(groupId));
    }
    if (!objGroupData.m_groupIds.empty()) {
      int i;
      for (i = 0; i < objGroupData.m_groupIds.size(); i++) {
        outerObj->setGroupId(objGroupData.m_groupIds[i]);
        outerObj->setGroupName(objGroupData.m_groupNames[i]);
      }
      for (i = 0; i < objGroupData.m_groupIds.size() &&
                  objGroupData.m_editingGroup >= 0;
           i++)
        outerObj->editGroup();
    }
    bringObjectOut(*it, xsh, ids, splines, pegObjects, pegbarIndex,
                   objGroupData, groupId);
  }
}

//-----------------------------------------------------------------------------

set<int> explodeStageObjects(
    TXsheet *xsh, TXsheet *subXsh, int index, const TStageObjectId &parentId,
    const GroupData &objGroupData, const TPointD &subPos,
    const GroupData &fxGroupData, QList<TStageObject *> &pegObjects,
    QMap<TFx *, QPair<TFx *, int>> &fxs,
    QMap<TStageObjectSpline *, TStageObjectSpline *> &splines,
    bool onlyColumn) {
  /*- SubXsheet, 親Xsheet両方のツリーを取得 -*/
  TStageObjectTree *innerTree = subXsh->getStageObjectTree();
  TStageObjectTree *outerTree = xsh->getStageObjectTree();
  // inner id->outer id
  QMap<TStageObjectId, TStageObjectId> ids;
  // innerSpline->outerSpline
  int groupId = -1;  // outerTree->getNewGroupId();
  /*- Pegbarも持ち出す場合 -*/
  if (!onlyColumn) {
    // add a pegbar to represent the table
    TStageObject *table = subXsh->getStageObject(TStageObjectId::TableId);
    /*- 空いてるIndexまでpegbarIndexを進める -*/
    int pegbarIndex = 2;
    while (
        outerTree->getStageObject(TStageObjectId::PegbarId(pegbarIndex), false))
      pegbarIndex++;
    /*- 空いてるIndexのPegbarに、SubXsheetのTableを対応させる -*/
    TStageObjectId id = TStageObjectId::PegbarId(pegbarIndex);
    TStageObject *obj = outerTree->getStageObject(id, true);
    /*- 対応表に追加 -*/
    obj->setDagNodePos(table->getDagNodePos());
    ids[TStageObjectId::TableId] = id;
    pegObjects.append(obj);
    obj->addRef();  // undo make release!!!!
    /*- SubのTableの情報を、今作ったPegbarにコピーする -*/
    TStageObjectParams *params = table->getParams();
    if (params->m_spline) {
      if (splines.contains(params->m_spline))
        params->m_spline = splines[params->m_spline];
      else {
        TStageObjectSpline *spline = params->m_spline->clone();
        splines[params->m_spline]  = spline;
        outerTree->assignUniqueSplineId(spline);
        outerTree->insertSpline(spline);
        params->m_spline = spline;
      }
    }
    obj->assignParams(params);
    delete params;
    // a pegbar cannot be a child of column
    if (parentId.isColumn())
      obj->setParent(TStageObjectId::TableId);
    else
      obj->setParent(parentId);

    // Put in the right StageObject group if needed
    obj->removeFromAllGroup();
    groupId = outerTree->getNewGroupId();
    obj->setGroupId(groupId);
    obj->setGroupName(L"Group " + std::to_wstring(groupId));
    if (!objGroupData.m_groupIds.empty()) {
      int i;
      for (i = 0; i < objGroupData.m_groupIds.size(); i++) {
        obj->setGroupId(objGroupData.m_groupIds[i]);
        obj->setGroupName(objGroupData.m_groupNames[i]);
      }
      for (i = 0; i < objGroupData.m_groupIds.size() &&
                  objGroupData.m_editingGroup >= 0;
           i++)
        obj->editGroup();
    }
    // add all pegbar
    bringObjectOut(table, xsh, ids, splines, pegObjects, pegbarIndex,
                   objGroupData, groupId);
  }

  // add colums;
  FxDag *innerDag            = subXsh->getFxDag();
  FxDag *outerDag            = xsh->getFxDag();
  TStageObjectId tmpParentId = parentId;
  set<int> indexes;
  int i;
  for (i = 0; i < subXsh->getColumnCount(); i++) {
    TXshColumn *innerColumn = subXsh->getColumn(i);
    TXshColumn *outerColumn = innerColumn->clone();

    TFx *innerFx = innerColumn->getFx();
    TFx *outerFx = outerColumn->getFx();

    xsh->insertColumn(index, outerColumn);
    // the above insertion operation may increment the parentId, in case that
    // 1, the parent object is column, and
    // 2, the parent column is placed on the right side of the inserted column
    //    ( i.e. index of the parent column is equal to or higher than "index")
    if (onlyColumn && tmpParentId.isColumn() && tmpParentId.getIndex() >= index)
      tmpParentId = TStageObjectId::ColumnId(tmpParentId.getIndex() + 1);

    if (innerFx && outerFx) {
      outerFx->getAttributes()->setDagNodePos(
          innerFx->getAttributes()->getDagNodePos());
      fxs[innerColumn->getFx()] =
          QPair<TFx *, int>(outerColumn->getFx(), outerColumn->getIndex());
      if (!innerDag->getTerminalFxs()->containsFx(innerColumn->getFx()))
        outerDag->getTerminalFxs()->removeFx(outerColumn->getFx());
    }

    TStageObjectId innerId     = TStageObjectId::ColumnId(i);
    TStageObjectId outerId     = TStageObjectId::ColumnId(index);
    TStageObject *innerCol     = innerTree->getStageObject(innerId, false);
    TStageObject *outerCol     = outerTree->getStageObject(outerId, false);
    TStageObjectParams *params = innerCol->getParams();
    if (params->m_spline) {
      if (splines.contains(params->m_spline))
        params->m_spline = splines[params->m_spline];
      else {
        TStageObjectSpline *spline = params->m_spline->clone();
        splines[params->m_spline]  = spline;
        outerTree->assignUniqueSplineId(spline);
        outerTree->insertSpline(spline);
        params->m_spline = spline;
      }
    }
    outerCol->assignParams(params);
    outerCol->setDagNodePos(innerCol->getDagNodePos());
    delete params;
    assert(outerCol && innerCol);
    ids[innerId] = outerId;
    outerCol->removeFromAllGroup();
    if (groupId != -1) {
      outerCol->setGroupId(groupId);
      outerCol->setGroupName(L"Group " + std::to_wstring(groupId));
    }

    if (onlyColumn) outerCol->setParent(tmpParentId);

    // Put in the right StageObject group if needed
    if (!objGroupData.m_groupIds.empty()) {
      int j;
      for (j = 0; j < objGroupData.m_groupIds.size(); j++) {
        outerCol->setGroupId(objGroupData.m_groupIds[j]);
        outerCol->setGroupName(objGroupData.m_groupNames[j]);
      }
      for (j = 0; j < objGroupData.m_groupIds.size() &&
                  objGroupData.m_editingGroup >= 0;
           j++)
        outerCol->editGroup();
    }

    // Put in the right Fx group if needed
    if (outerFx && !fxGroupData.m_groupIds.empty()) {
      int j;
      for (j = 0; j < fxGroupData.m_groupIds.size(); j++) {
        outerColumn->getFx()->getAttributes()->setGroupId(
            fxGroupData.m_groupIds[j]);
        outerColumn->getFx()->getAttributes()->setGroupName(
            fxGroupData.m_groupNames[j]);
      }
      for (j = 0;
           j < fxGroupData.m_groupIds.size() && fxGroupData.m_editingGroup >= 0;
           j++)
        outerColumn->getFx()->getAttributes()->editGroup();
    }
    indexes.insert(index);
    index++;
  }

  // setting column parents
  for (i = 0; i < subXsh->getColumnCount() && !onlyColumn; i++) {
    TStageObjectId innerId = TStageObjectId::ColumnId(i);
    TStageObject *innerCol = innerTree->getStageObject(innerId, false);
    xsh->setStageObjectParent(ids[innerId], ids[innerCol->getParent()]);
  }

  TPointD middlePoint;
  int objCount = 0;
  QMap<TStageObjectId, TStageObjectId>::iterator it;
  for (it = ids.begin(); it != ids.end(); it++) {
    TStageObject *innerObj = innerTree->getStageObject(it.key(), false);
    if (!innerObj) continue;

    const TPointD &pos = innerObj->getDagNodePos();
    if (pos == TConst::nowhere) continue;

    middlePoint = middlePoint + pos;
    ++objCount;
  }
  middlePoint = TPointD(middlePoint.x / objCount, middlePoint.y / objCount);

  // faccio in modo che tutti i nodi estratti siano centrati in middlePoint
  // Li metto poi in un gruppo
  TPointD offset = middlePoint - subPos;
  for (it = ids.begin(); it != ids.end(); it++) {
    TStageObject *outerObj = outerTree->getStageObject(it.value(), false);
    if (!outerObj) continue;
    /*outerObj->setGroupId(groupId);
outerObj->setGroupName(L"Group "+toWideString(groupId));*/
    TPointD outerPos = outerObj->getDagNodePos();
    if (outerPos != TConst::nowhere) outerObj->setDagNodePos(outerPos - offset);
  }

  return indexes;
}

//-----------------------------------------------------------------------------

void explodeFxs(TXsheet *xsh, TXsheet *subXsh, const GroupData &fxGroupData,
                QMap<TFx *, QPair<TFx *, int>> &fxs, const TPointD &subPos,
                const std::vector<TFxPort *> &outPorts, bool linkToXsheet) {
  FxDag *innerDag      = subXsh->getFxDag();
  FxDag *outerDag      = xsh->getFxDag();
  bool explosionLinked = false;

  // porto fuori tutti gli effetti che partono da un nodo di output (escluso
  // quello attaccato all'Xsheet
  // o che ha tra i padri il nodo xsheet)
  int i;
  for (i = 0; i < innerDag->getOutputFxCount(); i++) {
    TOutputFx *outFx = innerDag->getOutputFx(i);
    if (isConnectedToXsheet(outFx)) continue;

    TFx *root = explodeFxSubTree(outFx, fxs, outerDag, xsh, innerDag,
                                 fxGroupData, outPorts);
    if (!root) continue;
    if (outFx == innerDag->getCurrentOutputFx()) {
      if (outPorts.empty() || linkToXsheet)
        outerDag->addToXsheet(root);
      else
        for (int j    = 0; j < outPorts.size(); j++) outPorts[j]->setFx(root);
      explosionLinked = true;
    }
  }

  // porto fuori tutti gli effetti che partono dall'xsheet
  TFx *root = explodeFxSubTree(innerDag->getXsheetFx(), fxs, outerDag, xsh,
                               innerDag, fxGroupData, outPorts);
  if (!explosionLinked) {
    if (outPorts.empty()) {
      assert(root == 0);
      TFxSet *internals = innerDag->getTerminalFxs();
      for (int j = 0; j < internals->getFxCount(); j++) {
        TFx *fx = internals->getFx(j);
        outerDag->addToXsheet(fxs[fx].first);
      }
    } else if (!linkToXsheet) {
      for (int j = 0; j < outPorts.size(); j++) outPorts[j]->setFx(root);
    } else
      outerDag->addToXsheet(root);
  }

  // Porto fuori tutti gli altri effetti!
  TFxSet *innerInternals = innerDag->getInternalFxs();
  for (i = 0; i < innerInternals->getFxCount(); i++) {
    TFx *fx = innerInternals->getFx(i);
    if (fxs.contains(fx) || isConnectedToXsheet(fx)) continue;
    explodeFxSubTree(fx, fxs, outerDag, xsh, innerDag, fxGroupData, outPorts);
  }

  // cerco il punto medio tra tutti i nodi
  TPointD middlePoint(0.0, 0.0);
  int fxsCount = 0;

  QMap<TFx *, QPair<TFx *, int>>::iterator it;
  for (it = fxs.begin(); it != fxs.end(); it++) {
    TFx *innerFx = it.key();
    if (!innerFx) continue;

    assert(innerFx->getAttributes());
    const TPointD &pos = innerFx->getAttributes()->getDagNodePos();
    if (pos == TConst::nowhere) continue;

    middlePoint = middlePoint + pos;
    ++fxsCount;
  }
  if (fxsCount > 0)
    middlePoint = TPointD(middlePoint.x / fxsCount, middlePoint.y / fxsCount);
  else
    middlePoint = TPointD(25000, 25000);  // center of the scene

  // faccio in modo che tutti i nodi estratti siano centrati in middlePoint
  // Li metto poi in un gruppo
  TPointD offset = middlePoint - subPos;
  int groupId    = outerDag->getNewGroupId();
  for (it = fxs.begin(); it != fxs.end(); it++) {
    QPair<TFx *, int> pair = it.value();
    TFx *outerFx = pair.first;
    outerFx->getAttributes()->setGroupId(groupId);
    outerFx->getAttributes()->setGroupName(L"Group " +
                                           std::to_wstring(groupId));
    TPointD outerFxPos = outerFx->getAttributes()->getDagNodePos();
    if (outerFxPos != TConst::nowhere)
      outerFx->getAttributes()->setDagNodePos(outerFxPos - offset);
  }
}

//-----------------------------------------------------------------------------

set<int> explode(TXsheet *xsh, TXsheet *subXsh, int index,
                 const TStageObjectId &parentId, const GroupData &objGroupData,
                 const TPointD &stageSubPos, const GroupData &fxGroupData,
                 const TPointD &fxSubPos, QList<TStageObject *> &pegObjects,
                 QMap<TStageObjectSpline *, TStageObjectSpline *> &splines,
                 const std::vector<TFxPort *> &outPorts, bool onlyColumn,
                 bool linkToXsheet) {
  // innerFx->outerFxs
  QMap<TFx *, QPair<TFx *, int>> fxs;
  set<int> indexes = explodeStageObjects(xsh, subXsh, index, parentId,
                                         objGroupData, stageSubPos, fxGroupData,
                                         pegObjects, fxs, splines, onlyColumn);
  explodeFxs(xsh, subXsh, fxGroupData, fxs, fxSubPos, outPorts, linkToXsheet);
  return indexes;
}

//=============================================================================
// OpenChildUndo
//-----------------------------------------------------------------------------

class OpenChildUndo final : public TUndo {
  int m_row, m_col;

public:
  OpenChildUndo() {
    TApp *app     = TApp::instance();
    m_row         = app->getCurrentFrame()->getFrame();
    m_col         = app->getCurrentColumn()->getColumnIndex();
    TXsheet *xsh  = app->getCurrentXsheet()->getXsheet();
    TXshCell cell = xsh->getCell(m_row, m_col);
  }

  void undo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    int row, col;
    scene->getChildStack()->closeChild(row, col);
    app->getCurrentXsheet()->setXsheet(scene->getXsheet());
    changeSaveSubXsheetAsCommand();
  }

  void redo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    scene->getChildStack()->openChild(m_row, m_col);
    app->getCurrentXsheet()->setXsheet(scene->getXsheet());
    changeSaveSubXsheetAsCommand();
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// CloseChildUndo
//-----------------------------------------------------------------------------

class CloseChildUndo final : public TUndo {
  std::vector<std::pair<int, int>> m_cells;

public:
  CloseChildUndo(const std::vector<std::pair<int, int>> &cells)
      : m_cells(cells) {}

  void undo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    for (int i = m_cells.size() - 1; i >= 0; i--) {
      std::pair<int, int> rowCol = m_cells[i];
      scene->getChildStack()->openChild(rowCol.first, rowCol.second);
    }
    app->getCurrentXsheet()->setXsheet(scene->getXsheet());
    changeSaveSubXsheetAsCommand();
  }

  void redo() const override {
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    for (int i = 0; i < (int)m_cells.size(); i++) {
      int row, col;
      scene->getChildStack()->closeChild(row, col);
    }
    app->getCurrentXsheet()->setXsheet(scene->getXsheet());
    changeSaveSubXsheetAsCommand();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Close SubXsheet"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

void openSubXsheet() {
  TApp *app = TApp::instance();
  /*- Enter only when ChildLevel exists in selected cell or selected column -*/
  TCellSelection *cellSelection =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());
  TColumnSelection *columnSelection =
      dynamic_cast<TColumnSelection *>(TSelection::getCurrent());

  bool ret               = false;
  ToonzScene *scene      = app->getCurrentScene()->getScene();
  int row                = app->getCurrentFrame()->getFrame();
  int col                = app->getCurrentColumn()->getColumnIndex();
  TXsheet *currentXsheet = app->getCurrentXsheet()->getXsheet();
  TXshCell targetCell;

  /*- For column selection -*/
  if (columnSelection && !columnSelection->isEmpty()) {
    int sceneLength = currentXsheet->getFrameCount();

    std::set<int> columnIndices = columnSelection->getIndices();
    std::set<int>::iterator it;
    /*- Try openChild on each cell for each Column -*/
    for (it = columnIndices.begin(); it != columnIndices.end(); ++it) {
      int c = *it;
      // See if the current row indicator is on an exposed sub-xsheet frame
      // If so, use that.
      targetCell = currentXsheet->getCell(row, c);
      if (!targetCell.isEmpty() &&
          (ret = scene->getChildStack()->openChild(row, c)))
        break;

      /*- For each Cell in the Column, if contents are found break -*/
      for (int r = 0; r < sceneLength; r++) {
        ret = scene->getChildStack()->openChild(r, c);
        if (ret) {
          targetCell = currentXsheet->getCell(r, c);
          break;
        }
      }
      if (ret) break;
    }
  }

  /*- In other cases (cell selection or other) -*/
  else {
    TRect selectedArea;
    /*- If it is not cell selection, see current frame / column -*/
    if (!cellSelection || cellSelection->isEmpty()) {
      /*- When it is not cell selection, 1 × 1 selection range -*/
      selectedArea = TRect(col, row, col, row);
    }
    /*- In case of cell selection -*/
    else {
      int r0, c0, r1, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);
      selectedArea = TRect(c0, r0, c1, r1);
    }
    /*- Try openChild on each cell in Rect -*/
    for (int c = selectedArea.x0; c <= selectedArea.x1; c++) {
      for (int r = selectedArea.y0; r <= selectedArea.y1; r++) {
        ret = scene->getChildStack()->openChild(r, c);
        if (ret) {
          // When opening based on cell selection use the 1st
          // exposed frame in the sub-xsheet it finds
          targetCell = currentXsheet->getCell(r, c);
          break;
        }
      }
      if (ret) break;
    }
  }

  /*- When subXsheet Level is found -*/
  if (ret) {
    int subXsheetFrame = 0;

    if (!targetCell.isEmpty())
      subXsheetFrame = targetCell.getFrameId().getNumber() - 1;

    if (TSelection::getCurrent()) TSelection::getCurrent()->selectNone();

    TUndoManager::manager()->add(new OpenChildUndo());
    app->getCurrentXsheet()->setXsheet(scene->getXsheet());
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentColumn()->setColumnIndex(0);
    app->getCurrentFrame()->setFrameIndex(subXsheetFrame);
    changeSaveSubXsheetAsCommand();
  } else
    DVGui::error(QObject::tr("Select a sub-xsheet cell."));
}

//=============================================================================

void closeSubXsheet(int dlevel) {
  if (dlevel < 1) return;
  TApp *app = TApp::instance();
  TSelection *selection =
      TApp::instance()->getCurrentSelection()->getSelection();
  if (selection) selection->selectNone();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  int ancestorCount = scene->getChildStack()->getAncestorCount();
  if (ancestorCount == 0) return;
  if (dlevel > ancestorCount) dlevel = ancestorCount;
  std::vector<std::pair<int, int>> cells;
  for (int i = 0; i < dlevel; i++) {
    std::pair<int, int> rowCol;
    scene->getChildStack()->closeChild(rowCol.first, rowCol.second);
    TXsheet *xsh = scene->getXsheet();
    IconGenerator::instance()->invalidate(
        xsh->getCell(rowCol.first, rowCol.second).m_level.getPointer(),
        TFrameId(1));
    cells.push_back(rowCol);
  }
  if (cells.empty()) return;
  TUndoManager::manager()->add(new CloseChildUndo(cells));
  app->getCurrentXsheet()->setXsheet(scene->getXsheet());
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentColumn()->setColumnIndex(cells[0].second);
  app->getCurrentFrame()->setFrameIndex(cells[0].first);
  changeSaveSubXsheetAsCommand();
}

//=============================================================================

void bringPegbarsInsideChildXsheet(TXsheet *xsh, TXsheet *childXsh) {
  // retrieve all pegbars used from copied columns
  std::set<TStageObjectId> pegbarIds;
  int i;
  for (i = 0; i < childXsh->getColumnCount(); i++) {
    TStageObjectId columnId = TStageObjectId::ColumnId(i);
    TStageObjectId id       = childXsh->getStageObjectParent(columnId);
    /*- Columnの上流のPegbar/Cameraを格納していく -*/
    while (id.isPegbar() || id.isCamera()) {
      pegbarIds.insert(id);
      id = xsh->getStageObjectParent(id);
    }
  }

  std::set<TStageObjectId>::iterator pegbarIt;
  for (pegbarIt = pegbarIds.begin(); pegbarIt != pegbarIds.end(); ++pegbarIt) {
    TStageObjectId id        = *pegbarIt;
    TStageObjectParams *data = xsh->getStageObject(id)->getParams();
    childXsh->getStageObject(id)->assignParams(data);
    delete data;
    childXsh->getStageObject(id)->setParent(xsh->getStageObjectParent(id));
  }
}

//-----------------------------------------------------------------------------

void removeFx(TXsheet *xsh, TFx *fx) {
  TOutputFx *outFx = dynamic_cast<TOutputFx *>(fx);
  if (outFx) {
    xsh->getFxDag()->removeOutputFx(outFx);
    return;
  }

  TFxSet *internalFx = xsh->getFxDag()->getInternalFxs();
  TFxSet *terminalFx = xsh->getFxDag()->getTerminalFxs();

  int j;
  for (j = 0; j < fx->getInputPortCount(); j++) {
    TFxPort *inputPort = fx->getInputPort(j);
    TFx *inputFx       = inputPort->getFx();
    if (inputFx && j == 0) {
      int k;
      for (k = fx->getOutputConnectionCount() - 1; k >= 0; k--) {
        TFxPort *outputPort = fx->getOutputConnection(k);
        outputPort->setFx(inputFx);
      }
      if (terminalFx->containsFx(fx)) {
        terminalFx->removeFx(fx);
        terminalFx->addFx(inputFx);
      }
    }
    int i;
    for (i = fx->getOutputConnectionCount() - 1; i >= 0; i--)
      fx->getOutputConnection(i)->setFx(inputPort->getFx());
    inputPort->setFx(0);
  }
  internalFx->removeFx(fx);
}

//-----------------------------------------------------------------------------

TXsheet *collapseColumns(std::set<int> indices) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  int index = *indices.begin();

  StageObjectsData *data = new StageObjectsData();
  /*- StageObjectsData内にXshのデータを格納 -*/
  data->storeColumns(indices, xsh, StageObjectsData::eDoClone);
  data->storeColumnFxs(indices, xsh, StageObjectsData::eDoClone);

  app->getCurrentXsheet()->blockSignals(true);
  app->getCurrentObject()->blockSignals(true);
  /*- 親Sheetのカラムを消す -*/
  ColumnCmd::deleteColumns(indices, false, true);
  app->getCurrentXsheet()->blockSignals(false);
  app->getCurrentObject()->blockSignals(false);

  /*- 消したColumnの最初のIndexに、SubXsheetLevelを作る -*/
  xsh->insertColumn(index);

  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXshLevel *xl     = scene->createNewLevel(CHILD_XSHLEVEL);
  assert(xl);

  TXshChildLevel *childLevel = xl->getChildLevel();
  assert(childLevel);

  TXsheet *childXsh = childLevel->getXsheet();

  std::set<int> newIndices;
  std::list<int> restoredSplineIds;
  /*- 先ほどのColumnDataをSubXsheetの中に格納 -*/
  data->restoreObjects(newIndices, restoredSplineIds, childXsh, 0);
  childXsh->updateFrameCount();

  /*- SubXsheet Levelの動画番号を親Sheetに記入 -*/
  int r, rowCount = childXsh->getFrameCount();
  for (r = 0; r < rowCount; ++r)
    xsh->setCell(r, index, TXshCell(xl, TFrameId(r + 1)));

  return childXsh;
}

//-----------------------------------------------------------------------------

void collapseColumns(std::set<int> indices, bool columnsOnly) {
  /*- 選択カラムが無ければreturn -*/
  if (indices.empty()) return;

  int index    = *indices.begin();
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  TXsheet *childXsh = collapseColumns(indices);

  /*- Pegbar を持ち込む場合 -*/
  if (!columnsOnly) bringPegbarsInsideChildXsheet(xsh, childXsh);

  /*- 現状では、Pegbarを持ち込むかどうかに関わらず、subXsheetはTableに繋がる -*/
  xsh->getStageObject(TStageObjectId::ColumnId(index))
      ->setParent(TStageObjectId::TableId);
  xsh->updateFrameCount();

  /*-- カメラ情報のコピー --*/
  // xsh -> childXsh
  TStageObjectTree *parentTree = xsh->getStageObjectTree();
  TStageObjectTree *childTree  = childXsh->getStageObjectTree();

  int tmpCamId = 0;
  for (int cam = 0; cam < parentTree->getCameraCount();) {
    TStageObject *parentCamera =
        parentTree->getStageObject(TStageObjectId::CameraId(tmpCamId), false);
    /*- DeleteされたCameraはコピーしない -*/
    if (!parentCamera) {
      tmpCamId++;
      continue;
    }

    /*- Deleteされていない場合 -*/
    if (parentCamera->getCamera()) {
      /*- SubXsheetの対応するCameraを取得。なければ作る -*/
      TCamera *childCamera =
          childTree->getStageObject(TStageObjectId::CameraId(tmpCamId))
              ->getCamera();
      if (parentCamera && childCamera) {
        childCamera->setRes(parentCamera->getCamera()->getRes());
        childCamera->setSize(parentCamera->getCamera()->getSize());
      }
    }
    tmpCamId++;
    cam++;
  }
  /*- カレントカメラを同期させる -*/
  childTree->setCurrentCameraId(parentTree->getCurrentCameraId());

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentObject()->notifyObjectIdSwitched();
}

//-----------------------------------------------------------------------------

void collapseColumns(std::set<int> indices,
                     const QList<TStageObjectId> &objIds) {
  if (indices.empty()) return;

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  int index = *indices.begin();

  std::vector<TStageObjectId> roots = getRoots(objIds, app->getCurrentXsheet());
  TStageObject *rootObj             = 0;
  if (roots.size() == 1) {
    rootObj = xsh->getStageObjectTree()->getStageObject(roots[0], false);
    assert(rootObj);
  }

  StageObjectsData *data = new StageObjectsData();
  data->storeObjects(objIds.toVector().toStdVector(), xsh,
                     StageObjectsData::eDoClone);
  data->storeColumnFxs(indices, xsh, StageObjectsData::eDoClone);

  app->getCurrentXsheet()->blockSignals(true);
  app->getCurrentObject()->blockSignals(true);
  ColumnCmd::deleteColumns(indices, false, true);
  app->getCurrentXsheet()->blockSignals(false);
  app->getCurrentObject()->blockSignals(false);

  xsh->insertColumn(index);

  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXshLevel *xl     = scene->createNewLevel(CHILD_XSHLEVEL);
  assert(xl);

  TXshChildLevel *childLevel = xl->getChildLevel();
  assert(childLevel);

  TXsheet *childXsh = childLevel->getXsheet();

  std::set<int> newIndices;
  std::list<int> restoredSplineIds;
  data->restoreObjects(newIndices, restoredSplineIds, childXsh, 0);
  childXsh->updateFrameCount();

  int r, rowCount = childXsh->getFrameCount();
  for (r = 0; r < rowCount; r++)
    xsh->setCell(r, index, TXshCell(xl, TFrameId(r + 1)));

  if (roots.size() == 1 && rootObj)
    xsh->getStageObject(TStageObjectId::ColumnId(index))
        ->setParent(rootObj->getId());
  else
    xsh->getStageObject(TStageObjectId::ColumnId(index))
        ->setParent(TStageObjectId::TableId);

  xsh->updateFrameCount();

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentObject()->notifyObjectIdSwitched();
}

//-----------------------------------------------------------------------------

void collapseColumns(std::set<int> indices, const std::set<TFx *> &fxs,
                     bool columnsOnly) {
  if (indices.empty()) return;
  int index    = *indices.begin();
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  //++++++++++++++++++++++++++++++

  StageObjectsData *data = new StageObjectsData();
  data->storeColumns(indices, xsh, StageObjectsData::eDoClone);
  data->storeFxs(fxs, xsh, StageObjectsData::eDoClone);

  std::map<TFx *, std::vector<TFxPort *>> roots =
      isConnected(indices, fxs, app->getCurrentXsheet());
  app->getCurrentXsheet()->blockSignals(true);
  app->getCurrentObject()->blockSignals(true);
  ColumnCmd::deleteColumns(indices, true, true);
  app->getCurrentXsheet()->blockSignals(false);
  app->getCurrentObject()->blockSignals(false);
  xsh->insertColumn(index);

  std::set<TFx *>::const_iterator it;
  for (it = fxs.begin(); it != fxs.end(); it++) {
    TOutputFx *output = dynamic_cast<TOutputFx *>(*it);
    if (output) xsh->getFxDag()->removeOutputFx(output);
  }

  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXshLevel *xl     = scene->createNewLevel(CHILD_XSHLEVEL);
  assert(xl);
  TXshChildLevel *childLevel = xl->getChildLevel();
  assert(childLevel);
  TXsheet *childXsh = childLevel->getXsheet();

  std::set<int> newIndices;
  std::list<int> restoredSplineIds;
  data->restoreObjects(newIndices, restoredSplineIds, childXsh, 0);
  childXsh->updateFrameCount();

  int rowCount = childXsh->getFrameCount();
  int r;
  for (r = 0; r < rowCount; r++)
    xsh->setCell(r, index, TXshCell(xl, TFrameId(r + 1)));

  //++++++++++++++++++++++++++++++

  // Rimuovo gli effetti che sono in fxs dall'xsheet
  std::set<TFx *>::const_iterator it2;
  for (it2 = fxs.begin(); it2 != fxs.end(); it2++) removeFx(xsh, *it2);
  if (!columnsOnly) bringPegbarsInsideChildXsheet(xsh, childXsh);

  xsh->getStageObject(TStageObjectId::ColumnId(index))
      ->setParent(TStageObjectId::TableId);
  if (roots.size() == 1) {
    TFx *fx                          = xsh->getColumn(index)->getFx();
    std::vector<TFxPort *> rootPorts = roots.begin()->second;
    int i;
    for (i = 0; i < rootPorts.size(); i++) rootPorts[i]->setFx(fx);
    xsh->getFxDag()->getTerminalFxs()->removeFx(fx);
  }

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentObject()->notifyObjectIdSwitched();
}

//-----------------------------------------------------------------------------

void getColumnIndexes(const QList<TStageObjectId> &objects,
                      std::set<int> &indeces) {
  int i;
  for (i = 0; i < objects.size(); i++) {
    if (objects[i].isColumn()) indeces.insert(objects[i].getIndex());
  }
}

//-----------------------------------------------------------------------------

void getColumnIndexesAndPegbarIds(const QList<TStageObjectId> &objects,
                                  std::set<int> &indeces,
                                  std::set<TStageObjectId> &pegbarIds) {
  int i;
  for (i = 0; i < objects.size(); i++) {
    if (objects[i].isColumn()) indeces.insert(objects[i].getIndex());
    if (objects[i].isPegbar()) pegbarIds.insert(objects[i]);
  }
}

//-----------------------------------------------------------------------------

void getColumnIndexesAndInternalFxs(const QList<TFxP> &fxs,
                                    std::set<int> &indices,
                                    std::set<TFx *> &internalFx) {
  int i;
  for (i = 0; i < fxs.size(); i++) {
    TFx *fx        = fxs[i].getPointer();
    TColumnFx *cFx = dynamic_cast<TColumnFx *>(fx);
    if (cFx)
      indices.insert(cFx->getColumnIndex());
    else {
      TXsheetFx *xshFx = dynamic_cast<TXsheetFx *>(fx);
      TOutputFx *outFx = dynamic_cast<TOutputFx *>(fx);
      if (xshFx) continue;
      if (outFx) {
        TXsheetFx *xshFx =
            dynamic_cast<TXsheetFx *>(outFx->getInputPort(0)->getFx());
        if (xshFx) continue;
      }
      internalFx.insert(fx);
      fx->addRef();
    }
  }
}

//=============================================================================
// CollapseUndo
//-----------------------------------------------------------------------------

class CollapseUndo : public TUndo {
protected:
  std::set<int> m_indices;
  StageObjectsData *m_data;
  StageObjectsData *m_newData;
  int m_columnIndex;
  QMap<TFx *, QList<TFxPort *>> m_columnOutputConnections;
  QMap<TStageObjectId, QList<TStageObjectId>> m_children;
  // id->parentId
  QMap<TStageObjectId, TStageObjectId> m_parents;

  void doUndo() const {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    xsh->removeColumn(m_columnIndex);
    std::set<int> indices = m_indices;
    std::list<int> restoredSplineIds;
    m_data->restoreObjects(indices, restoredSplineIds, xsh, 0);
    setColumnOutputConnections(m_columnOutputConnections);
    setChildren(m_children);
    setParents(m_parents);

    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      selection->selectNone();
      std::set<int> selectIndices             = m_indices;
      std::set<int>::const_iterator indicesIt = selectIndices.begin();
      while (indicesIt != selectIndices.end())
        selection->selectColumn(*indicesIt++);
    }
  }

  void doRedo(bool deleteOnlyColumns) const {
    TApp *app                     = TApp::instance();
    TXsheet *xsh                  = app->getCurrentXsheet()->getXsheet();
    std::set<int> indicesToRemove = m_indices;
    QMap<TFx *, QList<TFxPort *>> columnOutputConnections;
    getColumnOutputConnections(m_indices, columnOutputConnections);
    app->getCurrentXsheet()->blockSignals(true);
    app->getCurrentObject()->blockSignals(true);
    ColumnCmd::deleteColumns(indicesToRemove, deleteOnlyColumns, true);
    app->getCurrentXsheet()->blockSignals(false);
    app->getCurrentObject()->blockSignals(false);
    setColumnOutputConnections(columnOutputConnections);
    std::set<int> indices;
    indices.insert(m_columnIndex);
    std::list<int> restoredSplineIds;
    m_newData->restoreObjects(indices, restoredSplineIds, xsh, 0);
    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      selection->selectNone();
      selection->selectColumn(m_columnIndex);
    }
  }

public:
  CollapseUndo(const std::set<int> indices, int c0, StageObjectsData *data,
               StageObjectsData *newData,
               const QMap<TFx *, QList<TFxPort *>> &columnOutputConnections,
               const QMap<TStageObjectId, QList<TStageObjectId>> &children,
               const QMap<TStageObjectId, TStageObjectId> &parents)
      : m_indices(indices)
      , m_columnIndex(c0)
      , m_data(data)
      , m_newData(newData)
      , m_columnOutputConnections(columnOutputConnections)
      , m_children(children)
      , m_parents(parents) {}

  ~CollapseUndo() {
    delete m_data;
    delete m_newData;
  }

  void undo() const override {
    doUndo();
    TApp *app = TApp::instance();
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentObject()->notifyObjectIdSwitched();
    changeSaveSubXsheetAsCommand();
  }

  void redo() const override {
    doRedo(false);
    TApp *app = TApp::instance();
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentObject()->notifyObjectIdSwitched();
    changeSaveSubXsheetAsCommand();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Collapse"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// CollapseFxUndo
//-----------------------------------------------------------------------------

class CollapseFxUndo final : public CollapseUndo {
  set<TFx *> m_fxs;
  QMap<TFx *, FxConnections> m_fxConnections;

public:
  CollapseFxUndo(const std::set<int> indices, int c0, StageObjectsData *data,
                 StageObjectsData *newData,
                 const QMap<TFx *, QList<TFxPort *>> &columnOutputConnections,
                 const QMap<TStageObjectId, QList<TStageObjectId>> children,
                 const QMap<TStageObjectId, TStageObjectId> &parents,
                 const set<TFx *> &fxs,
                 const QMap<TFx *, FxConnections> fxConnections)
      : CollapseUndo(indices, c0, data, newData, columnOutputConnections,
                     children, parents)
      , m_fxs(fxs)
      , m_fxConnections(fxConnections) {}

  ~CollapseFxUndo() {
    set<TFx *>::const_iterator it;
    for (it = m_fxs.begin(); it != m_fxs.end(); it++) (*it)->release();
  }

  void undo() const override {
    doUndo();
    TApp *app           = TApp::instance();
    TXsheet *xsh        = app->getCurrentXsheet()->getXsheet();
    TFxSet *internalFxs = xsh->getFxDag()->getInternalFxs();
    TFxSet *terminalFxs = xsh->getFxDag()->getTerminalFxs();
    set<TFx *>::const_iterator it;
    for (it = m_fxs.begin(); it != m_fxs.end(); it++)
      if (!internalFxs->containsFx((*it))) {
        TOutputFx *outFx = dynamic_cast<TOutputFx *>(*it);
        if (outFx)
          xsh->getFxDag()->addOutputFx(outFx);
        else
          internalFxs->addFx((*it));
      }
    QMap<TFx *, FxConnections>::const_iterator it2;
    for (it2 = m_fxConnections.begin(); it2 != m_fxConnections.end(); it2++) {
      TFx *fx                   = it2.key();
      FxConnections connections = it2.value();
      QMap<int, TFx *> inputLinks = connections.getInputLinks();
      QMap<int, TFx *>::const_iterator it3;
      for (it3 = inputLinks.begin(); it3 != inputLinks.end(); it3++)
        fx->getInputPort(it3.key())->setFx(it3.value());
      if (connections.isTerminal()) {
        terminalFxs->addFx(fx);
        QList<TFx *> noTerminalInputFxs = connections.getNotTerminalInputFxs();
        int i;
        for (i = 0; i < noTerminalInputFxs.size(); i++)
          if (terminalFxs->containsFx(noTerminalInputFxs[i]))
            terminalFxs->removeFx(noTerminalInputFxs[i]);
      }
      QMap<TFx *, int> outputLinks = connections.getOutputLinks();
      QMap<TFx *, int>::const_iterator it4;
      for (it4 = outputLinks.begin(); it4 != outputLinks.end(); it4++)
        it4.key()->getInputPort(it4.value())->setFx(fx);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentObject()->notifyObjectIdSwitched();
    changeSaveSubXsheetAsCommand();
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    std::map<TFx *, std::vector<TFxPort *>> roots =
        isConnected(m_indices, m_fxs, app->getCurrentXsheet());
    doRedo(true);
    std::set<TFx *>::const_iterator it2;
    for (it2 = m_fxs.begin(); it2 != m_fxs.end(); it2++) removeFx(xsh, *it2);
    if (roots.size() == 1) {
      TFx *fx                          = xsh->getColumn(m_columnIndex)->getFx();
      std::vector<TFxPort *> rootPorts = roots.begin()->second;
      int i;
      for (i = 0; i < rootPorts.size(); i++) rootPorts[i]->setFx(fx);
      xsh->getFxDag()->getTerminalFxs()->removeFx(fx);
    }

    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentObject()->notifyObjectIdSwitched();
    changeSaveSubXsheetAsCommand();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Collapse (Fx)"); }
};

//=============================================================================
// ExplodeChildUndoRemovingColumn
//-----------------------------------------------------------------------------

class ExplodeChildUndoRemovingColumn final : public TUndo {
  std::set<int> m_newIndexs;
  int m_index;
  StageObjectsData *m_oldData;
  StageObjectsData *m_newData;
  QMap<TFx *, QList<TFxPort *>> m_oldColumnOutputConnections;
  QMap<TFx *, QList<TFxPort *>> m_newColumnOutputConnections;
  // objId->parentObjId
  QMap<TStageObjectId, TStageObjectId> m_parentIds;
  QList<TStageObject *> m_pegObjects;
  QMap<TStageObjectSpline *, TStageObjectSpline *> m_splines;
  TFx *m_root;
  std::set<TFx *> m_oldInternalFxs;
  std::set<TOutputFx *> m_oldOutFxs;
  std::set<TOutputFx *> m_newOutFxs;

  // to handle grouping for the subxsheet
  QStack<int> m_objGroupIds;
  QStack<std::wstring> m_objGroupNames;

public:
  ExplodeChildUndoRemovingColumn(
      const std::set<int> &newIndexs, int index, StageObjectsData *oldData,
      StageObjectsData *newData,
      const QMap<TFx *, QList<TFxPort *>> &columnOutputConnections,
      const QList<TStageObject *> &pegObjects,
      const QMap<TStageObjectSpline *, TStageObjectSpline *> &splines,
      const std::set<TFx *> &oldInternalFxs,
      const std::set<TOutputFx *> oldOutFxs, TFx *root,
      const QStack<int> &objGroupIds, const QStack<std::wstring> &objGroupNames)
      : m_newIndexs(newIndexs)
      , m_index(index)
      , m_oldData(oldData)
      , m_newData(newData)
      , m_oldColumnOutputConnections(columnOutputConnections)
      , m_pegObjects(pegObjects)
      , m_splines(splines)
      , m_root(root)
      , m_oldInternalFxs(oldInternalFxs)
      , m_oldOutFxs(oldOutFxs)
      , m_objGroupIds(objGroupIds)
      , m_objGroupNames(objGroupNames) {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    std::set<int>::iterator it;
    for (it = m_newIndexs.begin(); it != m_newIndexs.end(); it++) {
      TXshColumn *column   = xsh->getColumn(*it);
      TStageObjectId colId = TStageObjectId::ColumnId(*it);
      m_parentIds[colId]   = xsh->getStageObjectParent(colId);

      TFx *columnFx = column->getFx();
      if (!columnFx) continue;

      QList<TFxPort *> outputConnections;
      int i;
      for (i = 0; i < columnFx->getOutputConnectionCount(); i++)
        outputConnections.append(columnFx->getOutputConnection(i));
      m_newColumnOutputConnections[columnFx] = outputConnections;
    }

    std::set<TOutputFx *>::iterator it2;
    for (it2 = m_oldOutFxs.begin(); it2 != m_oldOutFxs.end(); it2++)
      (*it2)->addRef();
    int i, outFxCount = xsh->getFxDag()->getOutputFxCount();
    for (i = 0; i < outFxCount; i++) {
      TOutputFx *outFx = xsh->getFxDag()->getOutputFx(i);
      m_newOutFxs.insert(outFx);
      outFx->addRef();
    }

    for (int i                              = 0; i < m_pegObjects.size(); i++)
      m_parentIds[m_pegObjects[i]->getId()] = m_pegObjects[i]->getParent();

    QMap<TStageObjectSpline *, TStageObjectSpline *>::iterator it3;
    for (it3 = m_splines.begin(); it3 != m_splines.end(); it3++)
      it3.value()->addRef();
  }

  ~ExplodeChildUndoRemovingColumn() {
    delete m_oldData;
    delete m_newData;
    int i;
    for (i = m_pegObjects.size() - 1; i >= 0; i--) m_pegObjects[i]->release();
    std::set<TOutputFx *>::iterator it2;
    for (it2 = m_oldOutFxs.begin(); it2 != m_oldOutFxs.end(); it2++)
      (*it2)->release();
    for (it2 = m_newOutFxs.begin(); it2 != m_newOutFxs.end(); it2++)
      (*it2)->release();
    QMap<TStageObjectSpline *, TStageObjectSpline *>::iterator it3;
    for (it3 = m_splines.begin(); it3 != m_splines.end(); it3++)
      it3.value()->release();
  }

  void setEditingFxGroup(TFx *fx, int editingGroup,
                         const QStack<int> &fxGroupIds) const {
    fx->getAttributes()->closeEditingGroup(fxGroupIds.top());
    while (fx->getAttributes()->getEditingGroupId() != editingGroup)
      fx->getAttributes()->editGroup();
    for (int i = 0; i < fx->getInputPortCount(); i++) {
      TFx *inputFx = fx->getInputPort(i)->getFx();
      if (inputFx) setEditingFxGroup(inputFx, editingGroup, fxGroupIds);
    }
  }

  void setEditingObjGroup(TStageObject *obj, int editingGroup,
                          const QStack<int> &objGroupIds) const {
    obj->closeEditingGroup(objGroupIds.top());
    while (obj->getEditingGroupId() != editingGroup) obj->editGroup();
    std::list<TStageObject *> children = obj->getChildren();
    std::list<TStageObject *>::iterator it;
    for (it = children.begin(); it != children.end(); it++) {
      TStageObject *childeObj = *it;
      if (childeObj) setEditingObjGroup(childeObj, editingGroup, objGroupIds);
    }
  }

  void undo() const override {
    TApp *app               = TApp::instance();
    TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
    int editingGroup        = -1;
    TStageObjectId parentId = TStageObjectId::NoneId;
    if (m_root && m_root->getOutputConnectionCount() > 0)
      editingGroup = m_root->getOutputConnection(0)
                         ->getOwnerFx()
                         ->getAttributes()
                         ->getEditingGroupId();

    std::set<int> indexesToRemove = m_newIndexs;
    app->getCurrentXsheet()->blockSignals(true);
    app->getCurrentObject()->blockSignals(true);
    ColumnCmd::deleteColumns(indexesToRemove, false, true);
    app->getCurrentXsheet()->blockSignals(false);
    app->getCurrentObject()->blockSignals(false);
    int i;
    for (i = m_pegObjects.size() - 1; i >= 0; i--) {
      TStageObjectId pegObjectId = m_pegObjects[i]->getId();
      TStageObjectId _parentId   = xsh->getStageObjectParent(pegObjectId);
      if (!m_pegObjects.contains(xsh->getStageObject(_parentId)))
        parentId = _parentId;
      if (app->getCurrentObject()->getObjectId() == pegObjectId)
        app->getCurrentObject()->setObjectId(TStageObjectId::TableId);
      xsh->getStageObjectTree()->removeStageObject(pegObjectId);
    }
    QMap<TStageObjectSpline *, TStageObjectSpline *>::const_iterator it;
    for (it = m_splines.begin(); it != m_splines.end(); it++) {
      TStageObjectSpline *spline = it.value();
      xsh->getStageObjectTree()->removeSpline(spline);
    }
    std::set<int> indexes;
    indexes.insert(m_index);
    std::list<int> restoredSplineIds;
    m_oldData->restoreObjects(indexes, restoredSplineIds, xsh, 0);
    setColumnOutputConnections(m_oldColumnOutputConnections);
    TFxSet *internals = xsh->getFxDag()->getInternalFxs();
    for (i = internals->getFxCount() - 1; i >= 0; i--) {
      TFx *fx = internals->getFx(i);
      if (m_oldInternalFxs.find(fx) == m_oldInternalFxs.end())
        internals->removeFx(fx);
    }
    std::set<TOutputFx *>::const_iterator it2;
    for (it2 = m_newOutFxs.begin(); it2 != m_newOutFxs.end(); it2++) {
      if (m_oldOutFxs.find(*it2) == m_oldOutFxs.end())
        xsh->getFxDag()->removeOutputFx(*it2);
    }
    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      selection->selectNone();
      selection->selectColumn(m_index);
    }
    // reinsert in groups
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(m_index));
    if (parentId != TStageObjectId::NoneId) obj->setParent(parentId);
    if (!m_objGroupIds.empty()) {
      TStageObjectId parentId = obj->getParent();
      TStageObject *parentObj = xsh->getStageObject(parentId);
      int i;
      for (i = 0; i < m_objGroupIds.size(); i++) {
        obj->setGroupId(m_objGroupIds[i]);
        obj->setGroupName(m_objGroupNames[i]);
      }
      for (i = 0;
           i < m_objGroupIds.size() && parentObj->getEditingGroupId() >= 0; i++)
        obj->editGroup();
    }
    QStack<int> fxGroupIds;
    if (m_root) fxGroupIds = m_root->getAttributes()->getGroupIdStack();
    if (!fxGroupIds.empty()) {
      // recupero l'id del gruppo che si sta editando!
      TFx *colFx = xsh->getColumn(m_index)->getFx();
      assert(colFx);
      colFx->getAttributes()->closeEditingGroup(fxGroupIds.top());
      while (colFx->getAttributes()->getEditingGroupId() != editingGroup)
        colFx->getAttributes()->editGroup();
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(m_index));
    TStageObjectId parentId = obj->getParent();
    TStageObject *parentObj = xsh->getStageObject(parentId);

    int objEditingGroup = -1;
    if (parentObj->isGrouped())
      objEditingGroup = parentObj->getEditingGroupId();

    TXshColumn *column = xsh->getColumn(m_index);
    assert(column);
    TFx *columnFx = column->getFx();
    assert(columnFx);
    int i;
    std::vector<TFxPort *> outPorts;
    for (i = 0; i < columnFx->getOutputConnectionCount(); i++)
      outPorts.push_back(columnFx->getOutputConnection(i));
    xsh->removeColumn(m_index);
    set<int> indexes = m_newIndexs;
    for (i = m_pegObjects.size() - 1; i >= 0; i--)
      xsh->getStageObjectTree()->insertStageObject(m_pegObjects[i]);
    QMap<TStageObjectSpline *, TStageObjectSpline *>::const_iterator it3;
    for (it3 = m_splines.begin(); it3 != m_splines.end(); it3++)
      xsh->getStageObjectTree()->insertSpline(it3.value());
    std::list<int> restoredSplineIds;
    m_newData->restoreObjects(indexes, restoredSplineIds, xsh, 0);
    for (i = 0; i < m_pegObjects.size(); i++)
      xsh->setStageObjectParent(m_pegObjects[i]->getId(),
                                m_parentIds[m_pegObjects[i]->getId()]);
    std::set<int>::const_iterator it;
    for (it = m_newIndexs.begin(); it != m_newIndexs.end(); it++) {
      TStageObjectId colId    = TStageObjectId::ColumnId(*it);
      TStageObjectId parentId = m_parentIds[colId];
      xsh->setStageObjectParent(colId, m_parentIds[colId]);
      TStageObject *obj       = xsh->getStageObject(colId);
      TStageObject *parentObj = xsh->getStageObject(parentId);
      if (parentObj->isGrouped()) {
        QStack<int> idStack             = parentObj->getGroupIdStack();
        QStack<std::wstring> groupstack = parentObj->getGroupNameStack();
        for (int i = 0; i < idStack.size(); i++) {
          obj->setGroupId(idStack[i]);
          obj->setGroupName(groupstack[i]);
        }
        int editedGroup = parentObj->getEditingGroupId();
        while (editedGroup != -1 && obj->getEditingGroupId() != editedGroup)
          obj->editGroup();
      }
    }
    setColumnOutputConnections(m_newColumnOutputConnections);
    for (i = 0; i < outPorts.size() && m_root; i++) outPorts[i]->setFx(m_root);
    std::set<TOutputFx *>::const_iterator it2;
    for (it2 = m_newOutFxs.begin(); it2 != m_newOutFxs.end(); it2++) {
      if (m_oldOutFxs.find(*it2) == m_oldOutFxs.end())
        xsh->getFxDag()->addOutputFx(*it2);
    }
    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      selection->selectNone();
      std::set<int> selectIndices             = m_newIndexs;
      std::set<int>::const_iterator indicesIt = selectIndices.begin();
      while (indicesIt != selectIndices.end())
        selection->selectColumn(*indicesIt++);
    }
    QStack<int> fxGroupIds;
    if (m_root) fxGroupIds = m_root->getAttributes()->getGroupIdStack();
    if (!fxGroupIds.empty()) {
      // recupero l'id del gruppo che si sta editando!
      int editingGroup = -1;
      if (m_root->getOutputConnectionCount() > 0)
        editingGroup = m_root->getOutputConnection(0)
                           ->getOwnerFx()
                           ->getAttributes()
                           ->getEditingGroupId();
      setEditingFxGroup(m_root, editingGroup, fxGroupIds);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Explode"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// ExplodeChildUndo
//-----------------------------------------------------------------------------

class ExplodeChildUndoWithoutRemovingColumn final : public TUndo {
  std::set<int> m_newIndexs;
  int m_index, m_from, m_to;

  TCellData *m_cellData;
  StageObjectsData *m_newData;
  QMap<TFx *, QList<TFxPort *>> m_newColumnOutputConnections;
  QList<TStageObject *> m_pegObjects;
  QMap<TStageObjectSpline *, TStageObjectSpline *> m_splines;
  std::set<TFx *> m_oldInternalFxs;
  std::set<TOutputFx *> m_oldOutFxs;
  std::set<TOutputFx *> m_newOutFxs;

  // to handle grouping for the subxsheet
  QStack<int> m_objGroupIds;
  QStack<std::wstring> m_objGroupNames;

public:
  ExplodeChildUndoWithoutRemovingColumn(
      const std::set<int> &newIndexs, int index, int from, int to,
      TCellData *cellData, StageObjectsData *newData,
      QList<TStageObject *> pegObjects,
      const QMap<TStageObjectSpline *, TStageObjectSpline *> &splines,
      const std::set<TFx *> &oldInternalFxs,
      const std::set<TOutputFx *> oldOutFxs, const QStack<int> &objGroupIds,
      const QStack<std::wstring> &objGroupNames)
      : m_newIndexs(newIndexs)
      , m_index(index)
      , m_from(from)
      , m_to(to)
      , m_cellData(cellData)
      , m_newData(newData)
      , m_pegObjects(pegObjects)
      , m_splines(splines)
      , m_oldInternalFxs(oldInternalFxs)
      , m_oldOutFxs(oldOutFxs)
      , m_objGroupIds(objGroupIds)
      , m_objGroupNames(objGroupNames) {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    std::set<int>::iterator it;
    for (it = m_newIndexs.begin(); it != m_newIndexs.end(); it++) {
      TXshColumn *column = xsh->getColumn(*it);
      TFx *columnFx      = column->getFx();
      QList<TFxPort *> outputConnections;
      int i;
      for (i = 0; i < columnFx->getOutputConnectionCount(); i++)
        outputConnections.append(columnFx->getOutputConnection(i));
      m_newColumnOutputConnections[columnFx] = outputConnections;
    }
    std::set<TOutputFx *>::iterator it2;
    for (it2 = m_oldOutFxs.begin(); it2 != m_oldOutFxs.end(); it2++)
      (*it2)->addRef();
    int i, outFxCount = xsh->getFxDag()->getOutputFxCount();
    for (i = 0; i < outFxCount; i++) {
      TOutputFx *outFx = xsh->getFxDag()->getOutputFx(i);
      m_newOutFxs.insert(outFx);
      outFx->addRef();
    }
  }

  ~ExplodeChildUndoWithoutRemovingColumn() {
    delete m_cellData;
    delete m_newData;
    int i;
    for (i = m_pegObjects.size() - 1; i >= 0; i--) m_pegObjects[i]->release();
    std::set<TOutputFx *>::iterator it2;
    for (it2 = m_oldOutFxs.begin(); it2 != m_oldOutFxs.end(); it2++)
      (*it2)->release();
    for (it2 = m_newOutFxs.begin(); it2 != m_newOutFxs.end(); it2++)
      (*it2)->release();
  }

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    set<int> indexesToRemove = m_newIndexs;
    app->getCurrentXsheet()->blockSignals(true);
    app->getCurrentObject()->blockSignals(true);
    ColumnCmd::deleteColumns(indexesToRemove, false, true);
    app->getCurrentXsheet()->blockSignals(false);
    app->getCurrentObject()->blockSignals(false);
    int i;
    for (i = m_pegObjects.size() - 1; i >= 0; i--)
      xsh->getStageObjectTree()->removeStageObject(m_pegObjects[i]->getId());
    set<int> indexes;
    indexes.insert(m_index);
    int to    = m_to;
    int index = m_index;
    m_cellData->getCells(xsh, m_from, m_index, to, index, false, false);
    TFxSet *internals = xsh->getFxDag()->getInternalFxs();
    for (i = internals->getFxCount() - 1; i >= 0; i--) {
      TFx *fx = internals->getFx(i);
      if (m_oldInternalFxs.find(fx) == m_oldInternalFxs.end())
        internals->removeFx(fx);
    }
    std::set<TOutputFx *>::const_iterator it;
    for (it = m_newOutFxs.begin(); it != m_newOutFxs.end(); it++) {
      if (m_oldOutFxs.find(*it) == m_oldOutFxs.end())
        xsh->getFxDag()->removeOutputFx(*it);
    }
    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      selection->selectNone();
      selection->selectColumn(m_index);
    }
    // reinsert in groups
    if (!m_objGroupIds.empty()) {
      TStageObject *obj =
          xsh->getStageObject(TStageObjectId::ColumnId(m_index));
      TStageObjectId parentId = obj->getParent();
      TStageObject *parentObj = xsh->getStageObject(parentId);
      int i;
      for (i = 0; i < m_objGroupIds.size(); i++) {
        obj->setGroupId(m_objGroupIds[i]);
        obj->setGroupName(m_objGroupNames[i]);
      }
      for (i = 0;
           i < m_objGroupIds.size() && parentObj->getEditingGroupId() >= 0; i++)
        obj->editGroup();
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    xsh->clearCells(m_from, m_index, m_to - m_from + 1);
    set<int> indexes = m_newIndexs;
    int i;
    for (i = m_pegObjects.size() - 1; i >= 0; i--)
      xsh->getStageObjectTree()->insertStageObject(m_pegObjects[i]);
    std::list<int> restoredSplineIds;
    m_newData->restoreObjects(indexes, restoredSplineIds, xsh, 0);
    setColumnOutputConnections(m_newColumnOutputConnections);
    std::set<TOutputFx *>::const_iterator it;
    for (it = m_newOutFxs.begin(); it != m_newOutFxs.end(); it++) {
      if (m_oldOutFxs.find(*it) == m_oldOutFxs.end())
        xsh->getFxDag()->addOutputFx(*it);
    }
    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        app->getCurrentSelection()->getSelection());
    if (selection) {
      selection->selectNone();
      std::set<int> selectIndices             = m_newIndexs;
      std::set<int>::const_iterator indicesIt = selectIndices.begin();
      while (indicesIt != selectIndices.end())
        selection->selectColumn(*indicesIt++);
    }
    // reinsert in groups
    if (!m_objGroupIds.empty()) {
      set<int>::iterator it;
      for (it = indexes.begin(); it != indexes.end(); it++) {
        TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(*it));
        TStageObjectId parentId = obj->getParent();
        TStageObject *parentObj = xsh->getStageObject(parentId);
        int i;
        for (i = 0; i < m_objGroupIds.size(); i++) {
          obj->setGroupId(m_objGroupIds[i]);
          obj->setGroupName(m_objGroupNames[i]);
        }
        for (i = 0;
             i < m_objGroupIds.size() && parentObj->getEditingGroupId() >= 0;
             i++)
          obj->editGroup();
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Explode"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

//=============================================================================
// OpenChildCommand
//-----------------------------------------------------------------------------

class OpenChildCommand final : public MenuItemHandler {
public:
  OpenChildCommand() : MenuItemHandler(MI_OpenChild) {}
  void execute() override { openSubXsheet(); }
} openChildCommand;

//=============================================================================
// CloseChildCommand
//-----------------------------------------------------------------------------

class CloseChildCommand final : public MenuItemHandler {
public:
  CloseChildCommand() : MenuItemHandler(MI_CloseChild) {}
  void execute() override { closeSubXsheet(1); }
} closeChildCommand;

//=============================================================================
// collapseColumns
//-----------------------------------------------------------------------------

//! Collapses the specified column indices in current XSheet.
void SubsceneCmd::collapse(std::set<int> &indices) {
  if (indices.empty()) return;

#ifndef LINETEST

  // User must decide if pegbars must be collapsed too
  QString question(QObject::tr("Collapsing columns: what you want to do?"));

  QList<QString> list;
  list.append(
      QObject::tr("Include relevant pegbars in the sub-xsheet as well."));
  list.append(QObject::tr("Include only selected columns in the sub-xsheet."));

  int ret = DVGui::RadioButtonMsgBox(DVGui::WARNING, question, list);
  if (ret == 0) return;

#else
  int ret = 1;
#endif

  std::set<int> oldIndices = indices;
  int index                = *indices.begin();

  // Retrieve current status to backup it in the UNDO
  StageObjectsData *oldData = new StageObjectsData();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  oldData->storeColumns(indices, xsh, 0);
  oldData->storeColumnFxs(indices, xsh, 0);

  QMap<TFx *, QList<TFxPort *>> columnOutputConnections;
  getColumnOutputConnections(indices, columnOutputConnections);

  QMap<TStageObjectId, QList<TStageObjectId>> children;
  getChildren(indices, children);

  QMap<TStageObjectId, TStageObjectId> parents;
  getParents(indices, parents);

  // Perform the collapse
  collapseColumns(indices, ret == 2);
  setColumnOutputConnections(columnOutputConnections);

  // Retrieve current status to backup it in the REDO
  indices.clear();
  indices.insert(index);

  StageObjectsData *newData = new StageObjectsData();
  newData->storeColumns(indices, xsh, 0);
  newData->storeColumnFxs(indices, xsh, 0);

  // Build the undo
  CollapseUndo *undo =
      new CollapseUndo(oldIndices, index, oldData, newData,
                       columnOutputConnections, children, parents);
  TUndoManager::manager()->add(undo);

  changeSaveSubXsheetAsCommand();
}

//-----------------------------------------------------------------------------

void SubsceneCmd::collapse(const QList<TStageObjectId> &objects) {
  if (objects.isEmpty()) return;

  std::set<int> indices;
  getColumnIndexes(objects, indices);

  std::set<int> oldIndices = indices;
  int index                = *indices.begin();

  StageObjectsData *oldData = new StageObjectsData();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  oldData->storeColumns(indices, xsh, 0);
  oldData->storeColumnFxs(indices, xsh, 0);

  QMap<TFx *, QList<TFxPort *>> columnOutputConnections;
  getColumnOutputConnections(indices, columnOutputConnections);

  QMap<TStageObjectId, QList<TStageObjectId>> children;
  getChildren(indices, children);

  QMap<TStageObjectId, TStageObjectId> parents;
  getParents(indices, parents);

  collapseColumns(indices, objects);
  setColumnOutputConnections(columnOutputConnections);

  indices.clear();
  indices.insert(index);

  StageObjectsData *newData = new StageObjectsData();
  newData->storeColumns(indices, xsh, 0);
  newData->storeColumnFxs(indices, xsh, 0);

  CollapseUndo *undo =
      new CollapseUndo(oldIndices, index, oldData, newData,
                       columnOutputConnections, children, parents);
  TUndoManager::manager()->add(undo);

  changeSaveSubXsheetAsCommand();
}

//-----------------------------------------------------------------------------

void SubsceneCmd::collapse(const QList<TFxP> &fxs) {
  if (fxs.isEmpty()) return;

#ifndef LINETEST

  QString question(QObject::tr("Collapsing columns: what you want to do?"));
  QList<QString> list;
  list.append(
      QObject::tr("Include relevant pegbars in the sub-xsheet as well."));
  list.append(QObject::tr("Include only selected columns in the sub-xsheet."));
  int ret = DVGui::RadioButtonMsgBox(DVGui::WARNING, question, list);
  if (ret == 0) return;

#else
  int ret = 1;
#endif

  std::set<int> indices;
  std::set<TFx *> internalFx;
  getColumnIndexesAndInternalFxs(fxs, indices, internalFx);

  std::set<int> oldIndices = indices;
  int index                = *indices.begin();

  StageObjectsData *oldData = new StageObjectsData();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  oldData->storeColumns(indices, xsh, 0);
  oldData->storeColumnFxs(indices, xsh, 0);

  QMap<TFx *, QList<TFxPort *>> columnOutputConnections;
  getColumnOutputConnections(indices, columnOutputConnections);

  QMap<TStageObjectId, QList<TStageObjectId>> children;
  getChildren(indices, children);

  QMap<TStageObjectId, TStageObjectId> parents;
  getParents(indices, parents);

  QMap<TFx *, FxConnections> fxConnections;
  getFxConnections(fxConnections, internalFx, xsh);

  collapseColumns(indices, internalFx, ret == 2);

  indices.clear();
  indices.insert(index);

  StageObjectsData *newData = new StageObjectsData();
  newData->storeColumns(indices, xsh, 0);
  newData->storeColumnFxs(indices, xsh, 0);

  CollapseFxUndo *undo = new CollapseFxUndo(oldIndices, index, oldData, newData,
                                            columnOutputConnections, children,
                                            parents, internalFx, fxConnections);
  TUndoManager::manager()->add(undo);

  changeSaveSubXsheetAsCommand();
}

//-----------------------------------------------------------------------------

void SubsceneCmd::explode(int index) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  TFrameHandle *frameHandle = app->getCurrentFrame();
  assert(frameHandle->getFrameType() == TFrameHandle::SceneFrame);
  int frameIndex = app->getCurrentFrame()->getFrame();

  /*- これからExplodeするセルを取得 -*/
  TXshCell cell = xsh->getCell(frameIndex, index);

  TXshChildLevel *childLevel = cell.getChildLevel();
  if (!childLevel) return;

#ifndef LINETEST

  /*- Pegbarを親Sheetに持って出るか？の質問ダイアログ -*/
  QString question(QObject::tr("Exploding Sub-xsheet: what you want to do?"));
  QList<QString> list;
  list.append(QObject::tr("Bring relevant pegbars in the main xsheet."));
  list.append(QObject::tr("Bring only columns in the main xsheet."));
  int ret = DVGui::RadioButtonMsgBox(DVGui::WARNING, question, list);
  if (ret == 0) return;

#else
  int ret = 2;
#endif

  // Collect column stage object informations
  TStageObjectId colId    = TStageObjectId::ColumnId(index);
  TStageObjectId parentId = xsh->getStageObjectParent(colId);
  TStageObject *obj = xsh->getStageObjectTree()->getStageObject(colId, false);
  assert(obj);

  // Collect StageObjects group informations
  QStack<int> objGroupIds;
  QStack<std::wstring> objGroupNames;
  int objEditingGroup = obj->getEditingGroupId();
  if (obj->isGrouped()) {
    int i         = 0;
    objGroupIds   = obj->getGroupIdStack();
    objGroupNames = obj->getGroupNameStack();
    while (objGroupIds.empty() && objEditingGroup >= 0 &&
           objGroupIds[i] != objEditingGroup) {
      objGroupIds.remove(i);
      objGroupNames.remove(i);
      i++;
    }
  }

  GroupData objGroupData(objGroupIds, objGroupNames, objEditingGroup);

  // Collect column fx information
  /*- FxのGroupの管理 -*/
  TXshColumn *column  = xsh->getColumn(index);
  TFx *columnFx       = column->getFx();
  TFxAttributes *attr = columnFx->getAttributes();

  // Collect Fx group informations
  QStack<int> fxGroupIds;
  QStack<std::wstring> fxGroupNames;
  int fxEditingGroup = attr->getEditingGroupId();
  if (attr->isGrouped()) {
    int i        = 0;
    fxGroupIds   = attr->getGroupIdStack();
    fxGroupNames = attr->getGroupNameStack();
    while (!fxGroupIds.empty() && fxEditingGroup >= 0 &&
           fxGroupIds[i] != fxEditingGroup) {
      fxGroupIds.remove(i);
      fxGroupNames.remove(i);
      i++;
    }
  }

  GroupData fxGroupData(fxGroupIds, fxGroupNames, fxEditingGroup);

  /*- Explode前のOutputFxのリストを取得 (oldOutFxs) -*/
  set<TOutputFx *> oldOutFxs;
  int i, outFxCount = xsh->getFxDag()->getOutputFxCount();
  for (i = 0; i < outFxCount; i++)
    oldOutFxs.insert(xsh->getFxDag()->getOutputFx(i));

  /*- SubXsheetカラムノードから繋がっているFxPortのリストを取得 (outPorts) -*/
  std::vector<TFxPort *> outPorts;
  for (i = 0; i < columnFx->getOutputConnectionCount(); i++)
    outPorts.push_back(columnFx->getOutputConnection(i));

  // Cannot remove the column if it contains frames of a TXshSimpleLevel.
  int from, to;
  /*--
  このカラムがsubXsheetLevelしか入っていない場合は、カラムを消去できるのでremoveColumnはtrue
          何か別のLevelが入っていた場合は、カラムを消去しないので、removeColumnはfalse
  --*/
  bool removeColumn =
      mustRemoveColumn(from, to, childLevel, xsh, index, frameIndex);
  QList<TStageObject *> pegObjects;
  QMap<TStageObjectSpline *, TStageObjectSpline *> splines;

  TPointD fxSubPos    = attr->getDagNodePos();
  TPointD stageSubPos = obj->getDagNodePos();

  if (removeColumn) {
    // Collect data for undo
    std::set<int> indexes;
    indexes.insert(index);

    /*- Undoのためのデータの取得 -*/
    StageObjectsData *oldData = new StageObjectsData();
    oldData->storeColumns(indexes, xsh, 0);
    oldData->storeColumnFxs(indexes, xsh, 0);

    QMap<TFx *, QList<TFxPort *>> columnOutputConnections;
    getColumnOutputConnections(indexes, columnOutputConnections);

    TFxSet *internals = xsh->getFxDag()->getInternalFxs();
    std::set<TFx *> oldInternalFxs;
    internals->getFxs(oldInternalFxs);

    // Remove column
    xsh->removeColumn(index);
    // The above removing operation may decrement the parentId, in case that
    // 1, the parent object is column, and
    // 2, the parent column is placed on the right side of the removed column
    //    ( i.e. index of the parent column is higher than "index")
    if (parentId.isColumn() && parentId.getIndex() > index)
      parentId = TStageObjectId::ColumnId(parentId.getIndex() - 1);

    // Explode
    set<int> newIndexes =
        ::explode(xsh, childLevel->getXsheet(), index, parentId, objGroupData,
                  stageSubPos, fxGroupData, fxSubPos, pegObjects, splines,
                  outPorts, ret == 2, false);

    /*- Redoのためのデータの取得 -*/
    StageObjectsData *newData = new StageObjectsData();
    newData->storeColumns(newIndexes, xsh, 0);
    newData->storeColumnFxs(newIndexes, xsh, 0);

    TFx *root = 0;
    assert(!columnOutputConnections.empty());
    QList<TFxPort *> ports   = columnOutputConnections.begin().value();
    if (!ports.empty()) root = (*ports.begin())->getFx();

    ExplodeChildUndoRemovingColumn *undo = new ExplodeChildUndoRemovingColumn(
        newIndexes, index, oldData, newData, columnOutputConnections,
        pegObjects, splines, oldInternalFxs, oldOutFxs, root, objGroupIds,
        objGroupNames);
    TUndoManager::manager()->add(undo);
  } else {
    // Collect information for undo
    TCellData *cellData = new TCellData();
    cellData->setCells(xsh, from, index, to, index);

    TFxSet *internals = xsh->getFxDag()->getInternalFxs();
    std::set<TFx *> oldInternalFxs;
    internals->getFxs(oldInternalFxs);

    // Remove cells
    xsh->clearCells(from, index, to - from + 1);

    // Explode
    set<int> newIndexes = ::explode(
        xsh, childLevel->getXsheet(), index + 1, parentId, objGroupData,
        stageSubPos + TPointD(10, 10), fxGroupData, fxSubPos + TPointD(10, 10),
        pegObjects, splines, outPorts, ret == 2, true);

    StageObjectsData *newData = new StageObjectsData();
    newData->storeColumns(newIndexes, xsh, 0);
    newData->storeColumnFxs(newIndexes, xsh, 0);

    ExplodeChildUndoWithoutRemovingColumn *undo =
        new ExplodeChildUndoWithoutRemovingColumn(
            newIndexes, index, from, to, cellData, newData, pegObjects, splines,
            oldInternalFxs, oldOutFxs, objGroupIds, objGroupNames);
    TUndoManager::manager()->add(undo);
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
}
