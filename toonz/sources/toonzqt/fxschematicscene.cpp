

#include "toonzqt/fxschematicscene.h"

// TnzQt includes
#include "toonzqt/fxschematicnode.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/fxselection.h"
#include "toonzqt/schematicgroupeditor.h"
#include "toonzqt/swatchviewer.h"
#include "toonzqt/tselectionhandle.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/toonzscene.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/fxdag.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/fxcommand.h"
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"

// TnzBase includes
#include "tmacrofx.h"
#include "fxdata.h"
#include "tpassivecachemanager.h"
#include "tfxattributes.h"

// TnzCore includes
#include "tconst.h"

// Qt includes
#include <QMenu>
#include <QApplication>
#include <QGraphicsSceneContextMenuEvent>

namespace {

class MatchesFx {
  TFxP m_fx;

public:
  MatchesFx(const TFxP &fx) : m_fx(fx) {}

  bool operator()(const TFxP &fx) {
    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx.getPointer());
    return (m_fx.getPointer() == fx.getPointer()) ||
           (zfx && m_fx.getPointer() == zfx->getZeraryFx());
  }
};

//-----------------------------------------------------------

void keepSubgroup(QMap<int, QList<SchematicNode *>> &editedGroup) {
  QMap<int, QList<SchematicNode *>>::iterator it;
  for (it = editedGroup.begin(); it != editedGroup.end(); it++) {
    QList<SchematicNode *> groupedNodes = it.value();
    int i;
    for (i = 0; i < groupedNodes.size(); i++) {
      FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(groupedNodes[i]);
      if (!node) continue;
      TFx *fx = node->getFx();
      assert(fx);
      QMap<int, QList<SchematicNode *>>::iterator it2;
      for (it2 = editedGroup.begin(); it2 != editedGroup.end(); it2++) {
        if (fx->getAttributes()->isContainedInGroup(it2.key()) &&
            !editedGroup[it2.key()].contains(node))
          editedGroup[it2.key()].append(node);
      }
    }
  }
}

//-----------------------------------------------------------

//! Find the input and the output fx contained in \b visitedFxs.
//! \b visitedFxs must be a connected fx selection. In \b outputFx is put the
//! root of the connected fx selection.
//! In \b inputFx is put a leaf of the connected fx selection.
void findBoundariesFxs(TFx *&inputFx, TFx *&outputFx,
                       QMap<TFx *, bool> &visitedFxs, TFx *currentFx = 0) {
  if (visitedFxs.isEmpty()) return;
  if (!currentFx) currentFx = visitedFxs.begin().key();
  int inputPortCount        = currentFx->getInputPortCount();
  int outputConnectionCount = currentFx->getOutputConnectionCount();

  if (inputPortCount > 0 && !visitedFxs[currentFx]) {
    visitedFxs[currentFx] = true;
    TFxPort *fxPort       = currentFx->getInputPort(0);
    TFx *fx               = fxPort->getFx();
    if (fx && visitedFxs.count(fx) == 1) {
      if (!visitedFxs[fx]) findBoundariesFxs(inputFx, outputFx, visitedFxs, fx);
    } else
      inputFx = currentFx;
  } else
    inputFx = currentFx;
  if (!outputFx) {
    if (outputConnectionCount > 0) {
      TFx *fx = currentFx->getOutputConnection(0)->getOwnerFx();
      if (fx && visitedFxs.count(fx) == 1) {
        if (!visitedFxs[fx])
          findBoundariesFxs(inputFx, outputFx, visitedFxs, fx);
      } else
        outputFx = currentFx;
    } else
      outputFx = currentFx;
  }
}

//-----------------------------------------------------------

bool canDisconnectSelection(FxSelection *selection) {
  QList<TFxP> selectedFx = selection->getFxs();
  int i;
  for (i = 0; i < selectedFx.size(); i++)  // .....
  {
    TFx *fx = selectedFx[i].getPointer();

    TLevelColumnFx *lcFx  = dynamic_cast<TLevelColumnFx *>(fx);
    TPaletteColumnFx *pfx = dynamic_cast<TPaletteColumnFx *>(fx);
    TXsheetFx *xfx        = dynamic_cast<TXsheetFx *>(fx);
    TOutputFx *ofx        = dynamic_cast<TOutputFx *>(fx);
    TZeraryColumnFx *zfx  = dynamic_cast<TZeraryColumnFx *>(fx);

    return (!lcFx && !pfx && !xfx && !ofx &&
            (!zfx || zfx->getInputPortCount() > 0));  // !!!!!
  }
  return false;
}

//------------------------------------------------------------------

QList<TFxP> getRoots(const QList<TFxP> &fxs, TFxSet *terminals) {
  QList<TFxP> roots;
  int i;
  for (i = 0; i < fxs.size(); i++) {
    TFx *fx = fxs[i].getPointer();
    int j;
    bool isRoot = true;
    for (j = 0; j < fx->getOutputConnectionCount(); j++) {
      TFx *outFx = fx->getOutputConnection(j)->getOwnerFx();
      if (outFx &&
          std::find_if(fxs.begin(), fxs.end(), MatchesFx(outFx)) != fxs.end() &&
          !terminals->containsFx(fx))
        isRoot = false;
    }
    if (isRoot) roots.append(fx);
  }
  return roots;
}

}  // namespace

//==================================================================
//
// FxSchematicScene::SupportLinks
//
//==================================================================

void FxSchematicScene::SupportLinks::addBridgeLink(SchematicLink *link) {
  if (link && !m_bridges.contains(link)) m_bridges.push_back(link);
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::addOutputLink(SchematicLink *link) {
  if (link && !m_outputs.contains(link)) m_outputs.push_back(link);
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::addInputLink(SchematicLink *link) {
  if (link && !m_inputs.contains(link)) m_inputs.push_back(link);
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::hideBridgeLinks() {
  int i;
  for (i = 0; i < m_bridges.size(); i++) m_bridges[i]->hide();
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::hideInputLinks() {
  int i;
  for (i = 0; i < m_inputs.size(); i++) m_inputs[i]->hide();
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::hideOutputLinks() {
  int i;
  for (i = 0; i < m_outputs.size(); i++) m_outputs[i]->hide();
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::showBridgeLinks() {
  int i;
  for (i = 0; i < m_bridges.size(); i++) m_bridges[i]->show();
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::showInputLinks() {
  int i;
  for (i = 0; i < m_inputs.size(); i++) m_inputs[i]->show();
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::showOutputLinks() {
  int i;
  for (i = 0; i < m_outputs.size(); i++) m_outputs[i]->show();
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::removeBridgeLinks(bool deleteLink) {
  int i;
  for (i = 0; i < m_bridges.size(); i++) {
    SchematicLink *link = m_bridges[i];
    m_bridges.removeAt(i);
    if (deleteLink) {
      link->getStartPort()->removeLink(link);
      link->getEndPort()->removeLink(link);
      delete link;
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::removeInputLinks(bool deleteLink) {
  int i;
  for (i = 0; i < m_inputs.size(); i++) {
    SchematicLink *link = m_inputs[i];
    m_inputs.removeAt(i);
    if (deleteLink) {
      link->getStartPort()->removeLink(link);
      link->getEndPort()->removeLink(link);
      delete link;
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::removeOutputLinks(bool deleteLink) {
  int i;
  for (i = 0; i < m_outputs.size(); i++) {
    SchematicLink *link = m_outputs[i];
    m_outputs.removeAt(i);
    if (deleteLink) {
      link->getStartPort()->removeLink(link);
      link->getEndPort()->removeLink(link);
      delete link;
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::SupportLinks::clearAll() {
  m_bridges.clear();
  m_inputs.clear();
  m_outputs.clear();
}

//------------------------------------------------------------------

int FxSchematicScene::SupportLinks::size() {
  return m_bridges.size() + m_inputs.size() + m_outputs.size();
}

//==================================================================
//
// FxSchematicScene
//
//==================================================================

FxSchematicScene::FxSchematicScene(QWidget *parent)
    : SchematicScene(parent)
    , m_firstPoint(sceneRect().center())
    , m_xshHandle(0)
    , m_fxHandle(0)
    , m_addFxContextMenu()
    , m_disconnectionLinks()
    , m_connectionLinks()
    , m_isConnected(false)
    , m_linkUnlinkSimulation(false)
    , m_altPressed(false)
    , m_lastPos(0, 0)
    , m_currentFxNode(0)
    , m_gridDimension(eSmall)
    , m_isLargeScaled(true) {
  m_selection = new FxSelection();
  m_selection->setFxSchematicScene(this);

  connect(m_selection, SIGNAL(doCollapse(const QList<TFxP> &)), this,
          SLOT(onCollapse(const QList<TFxP> &)));
  connect(m_selection, SIGNAL(doExplodeChild(const QList<TFxP> &)), this,
          SIGNAL(doExplodeChild(const QList<TFxP> &)));
  connect(this, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged()));

  m_addFxContextMenu.setSelection(m_selection);
  m_highlightedLinks.clear();
}

//------------------------------------------------------------------

FxSchematicScene::~FxSchematicScene() {
  if (m_selection) delete m_selection;
}

//------------------------------------------------------------------

void FxSchematicScene::setApplication(TApplication *app) {
  m_app = app;

  m_xshHandle    = app->getCurrentXsheet();
  m_fxHandle     = app->getCurrentFx();
  m_frameHandle  = app->getCurrentFrame();
  m_columnHandle = app->getCurrentColumn();

  if (m_fxHandle)
    connect(m_fxHandle, SIGNAL(fxSwitched()), this,
            SLOT(onCurrentFxSwitched()));

  m_addFxContextMenu.setApplication(app);

  m_selection->setXsheetHandle(m_xshHandle);
  m_selection->setFxHandle(m_fxHandle);
}

//------------------------------------------------------------------

void FxSchematicScene::updateScene() {
  if (!views().empty())
#if QT_VERSION >= 0x050000
    m_isLargeScaled = views().at(0)->matrix().determinant() >= 1.0;
#else
    m_isLargeScaled = views().at(0)->matrix().det() >= 1.0;
#endif
  m_disconnectionLinks.clearAll();
  m_connectionLinks.clearAll();
  m_selectionOldPos.clear();

  clearSelection();
  clearAllItems();

  m_table.clear();
  m_groupedTable.clear();
  m_groupEditorTable.clear();
  m_macroEditorTable.clear();

  m_currentFxNode = 0;

  // GroupId->Fx
  QMap<int, QList<TFxP>> groupedFxs;
  QMap<int, QList<SchematicNode *>> editedGroup;
  QMap<TMacroFx *, QList<SchematicNode *>> editedMacro;

  TXsheet *xsh    = m_xshHandle->getXsheet();
  m_gridDimension = xsh->getFxDag()->getDagGridDimension();
  TFxSet *fxSet   = xsh->getFxDag()->getInternalFxs();
  int i;

  FxDag *fxDag = xsh->getFxDag();
  // Add XSheetFX node
  addFxSchematicNode(fxDag->getXsheetFx());

  // Add outputFx nodes
  int k;
  for (k = 0; k < fxDag->getOutputFxCount(); k++) {
    TOutputFx *fx = fxDag->getOutputFx(k);
    if (fx->getAttributes()->isGrouped() &&
        !fx->getAttributes()->isGroupEditing()) {
      groupedFxs[fx->getAttributes()->getGroupId()].push_back(fx);
      continue;
    }
    SchematicNode *node = addFxSchematicNode(fx);
    if (fx->getAttributes()->isGrouped())
      editedGroup[fx->getAttributes()->getEditingGroupId()].append(node);
  }

  // Add columnFx and zeraryFx nodes
  for (i = 0; i < xsh->getColumnCount(); i++) {
    TXshColumn *column = xsh->getColumn(i);
    TFx *fx            = 0;
    if (TXshLevelColumn *lc = column->getLevelColumn())
      fx = lc->getLevelColumnFx();
    else if (TXshPaletteColumn *pc = dynamic_cast<TXshPaletteColumn *>(column))
      fx = pc->getPaletteColumnFx();
    else if (TXshZeraryFxColumn *zc =
                 dynamic_cast<TXshZeraryFxColumn *>(column))
      fx = zc->getZeraryColumnFx();
    if (!fx) continue;

    if (fx->getAttributes()->isGrouped() &&
        !fx->getAttributes()->isGroupEditing()) {
      groupedFxs[fx->getAttributes()->getGroupId()].push_back(fx);
      continue;
    }
    if (column->isEmpty() && fx && fx->getOutputConnectionCount() == 0)
      continue;
    SchematicNode *node = addFxSchematicNode(fx);
    if (fx->getAttributes()->isGrouped())
      editedGroup[fx->getAttributes()->getEditingGroupId()].append(node);
  }

  // Add normalFx
  for (i = 0; i < fxSet->getFxCount(); i++) {
    TFx *fx         = fxSet->getFx(i);
    TMacroFx *macro = dynamic_cast<TMacroFx *>(fx);
    if (fx->getAttributes()->isGrouped() &&
        !fx->getAttributes()->isGroupEditing()) {
      groupedFxs[fx->getAttributes()->getGroupId()].push_back(fx);
      continue;
    } else if (macro && macro->isEditing()) {
      std::vector<TFxP> fxs = macro->getFxs();
      int j;
      for (j = 0; j < (int)fxs.size(); j++) {
        SchematicNode *node = addFxSchematicNode(fxs[j].getPointer());
        editedMacro[macro].append(node);
        if (fxs[j]->getAttributes()->isGrouped() &&
            macro->getAttributes()->isGroupEditing())
          editedGroup[fx->getAttributes()->getEditingGroupId()].append(node);
      }
      continue;
    }
    SchematicNode *node = addFxSchematicNode(fx);
    if (fx->getAttributes()->isGrouped())
      editedGroup[fx->getAttributes()->getEditingGroupId()].append(node);
  }

  // grouped node
  QMap<int, QList<TFxP>>::const_iterator it;
  for (it = groupedFxs.begin(); it != groupedFxs.end(); it++) {
    FxSchematicNode *node = addGroupedFxSchematicNode(it.key(), it.value());
    TFx *fx               = node->getFx();
    assert(fx);
    int editingGroupId = fx->getAttributes()->getEditingGroupId();
    if (editingGroupId != -1) editedGroup[editingGroupId].append(node);
  }

  keepSubgroup(editedGroup);
  updateEditedGroups(editedGroup);
  updateEditedMacros(editedMacro);
  updateLink();
  m_nodesToPlace.clear();
}

//------------------------------------------------------------------

void FxSchematicScene::updateEditedGroups(
    const QMap<int, QList<SchematicNode *>> &editedGroup) {
  QMap<int, QList<SchematicNode *>>::const_iterator it;
  for (it = editedGroup.begin(); it != editedGroup.end(); it++) {
    int zValue = 2;
    QMap<int, QList<SchematicNode *>>::const_iterator it2 = editedGroup.begin();
    while (it2 != editedGroup.end()) {
      FxSchematicNode *placedFxNode =
          dynamic_cast<FxSchematicNode *>(it2.value()[0]);
      FxSchematicNode *fxNode = dynamic_cast<FxSchematicNode *>(it.value()[0]);
      if (!placedFxNode || !fxNode) {
        it2++;
        continue;
      }
      int placedGroupedId =
          placedFxNode->getFx()->getAttributes()->getEditingGroupId();
      if (fxNode->getFx()->getAttributes()->isContainedInGroup(
              placedGroupedId) &&
          fxNode->getFx()->getAttributes()->getEditingGroupId() != it2.key())
        zValue += 2;
      it2++;
    }
    FxSchematicGroupEditor *node =
        addEditedGroupedFxSchematicNode(it.key(), it.value());
    node->setZValue(zValue);
    node->setGroupedNodeZValue(zValue + 1);
  }
}

//------------------------------------------------------------------

void FxSchematicScene::updateEditedMacros(
    const QMap<TMacroFx *, QList<SchematicNode *>> &editedMacro) {
  QMap<TMacroFx *, QList<SchematicNode *>>::const_iterator it;
  for (it = editedMacro.begin(); it != editedMacro.end(); it++) {
    TMacroFx *macro = it.key();
    int zValue      = 2;
    if (macro->getAttributes()->isGrouped()) {
      FxSchematicGroupEditor *containingGroup =
          m_groupEditorTable[macro->getAttributes()->getEditingGroupId()];
      assert(containingGroup);
      zValue = containingGroup->zValue() + 2;
    }
    FxSchematicMacroEditor *node =
        addEditedMacroFxSchematicNode(it.key(), it.value());
    node->setZValue(zValue);
    node->setGroupedNodeZValue(zValue + 1);
  }
}

//------------------------------------------------------------------

FxSchematicNode *FxSchematicScene::addFxSchematicNode(TFx *fx) {
  FxSchematicNode *node = createFxSchematicNode(fx);
  if (!node) return 0;
  connect(node, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));
  connect(node, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
  connect(node, SIGNAL(switchCurrentFx(TFx *)), this,
          SLOT(onSwitchCurrentFx(TFx *)));
  connect(node, SIGNAL(currentColumnChanged(int)), this,
          SLOT(onCurrentColumnChanged(int)));

  connect(node, SIGNAL(fxNodeDoubleClicked()), this,
          SLOT(onFxNodeDoubleClicked()));
  if (fx->getAttributes()->getDagNodePos() == TConst::nowhere) {
    node->resize(m_gridDimension == 0);
    placeNode(node);
  } else
    updatePosition(node, fx->getAttributes()->getDagNodePos());
  m_table[fx] = node;
  return node;
}

//------------------------------------------------------------------

FxSchematicNode *FxSchematicScene::addGroupedFxSchematicNode(
    int groupId, const QList<TFxP> &groupedFxs) {
  TFxSet *terminals = getXsheet()->getFxDag()->getTerminalFxs();
  QList<TFxP> roots = getRoots(groupedFxs, terminals);
  if (roots.isEmpty()) return 0;
  std::wstring name = roots[0]->getAttributes()->getGroupName(false);
  FxGroupNode *node = new FxGroupNode(this, groupedFxs, roots, groupId, name);
  if (!node) return 0;
  connect(node, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));
  connect(node, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));
  connect(node, SIGNAL(switchCurrentFx(TFx *)), this,
          SLOT(onSwitchCurrentFx(TFx *)));
  connect(node, SIGNAL(currentColumnChanged(int)), this,
          SLOT(onCurrentColumnChanged(int)));
  m_groupedTable[groupId] = node;
  return node;
}

//------------------------------------------------------------------

FxSchematicGroupEditor *FxSchematicScene::addEditedGroupedFxSchematicNode(
    int groupId, const QList<SchematicNode *> &groupedFxs) {
  FxSchematicGroupEditor *editorGroup =
      new FxSchematicGroupEditor(groupId, groupedFxs, this);
  m_groupEditorTable[groupId] = editorGroup;
  return editorGroup;
}

//------------------------------------------------------------------

FxSchematicMacroEditor *FxSchematicScene::addEditedMacroFxSchematicNode(
    TMacroFx *macro, const QList<SchematicNode *> &groupedFxs) {
  FxSchematicMacroEditor *editorMacro =
      new FxSchematicMacroEditor(macro, groupedFxs, this);
  m_macroEditorTable[macro] = editorMacro;
  return editorMacro;
}

//------------------------------------------------------------------

void FxSchematicScene::updatePosition(FxSchematicNode *node,
                                      const TPointD &pos) {
  node->setPos(QPointF(pos.x, pos.y));
  node->getFx()->getAttributes()->setDagNodePos(pos);
  QVector<SchematicNode *> placedNodes = getPlacedNode(node);
  int step                             = m_gridDimension == eLarge ? 100 : 50;
  TPointD offset(0, -step);
  int i;
  for (i = 0; i < placedNodes.size(); i++) {
    FxSchematicNode *placedNode =
        dynamic_cast<FxSchematicNode *>(placedNodes[i]);
    assert(placedNode);
    TPointD newPos =
        placedNode->getFx()->getAttributes()->getDagNodePos() + offset;
    updatePosition(placedNode, newPos);
  }
}

//------------------------------------------------------------------
/*! create node depends on the fx type
*/
FxSchematicNode *FxSchematicScene::createFxSchematicNode(TFx *fx) {
  if (TLevelColumnFx *lcFx = dynamic_cast<TLevelColumnFx *>(fx))
    return new FxSchematicColumnNode(this, lcFx);
  else if (TPaletteColumnFx *pfx = dynamic_cast<TPaletteColumnFx *>(fx))
    return new FxSchematicPaletteNode(this, pfx);
  else if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx))
    return new FxSchematicZeraryNode(this, zfx);
  else if (TXsheetFx *xfx = dynamic_cast<TXsheetFx *>(fx))
    return new FxSchematicXSheetNode(this, xfx);
  else if (TOutputFx *ofx = dynamic_cast<TOutputFx *>(fx))
    return new FxSchematicOutputNode(this, ofx);
  else
    return new FxSchematicNormalFxNode(this, fx);
}

//------------------------------------------------------------------
/*! place nodes of which positions are not specified manually
*/
void FxSchematicScene::placeNode(FxSchematicNode *node) {
  if (!node) return;
  int step        = m_gridDimension == eLarge ? 100 : 50;
  FxDag *fxDag    = m_xshHandle->getXsheet()->getFxDag();
  QRectF nodeRect = node->boundingRect();
  if (node->isA(eOutpuFx)) {
    // I'm placing an output node
    TFx *xsheetFx        = fxDag->getXsheetFx();
    TFxPort *outPort     = xsheetFx->getOutputConnection(0);
    TFx *connectedOutput = outPort ? outPort->getOwnerFx() : 0;
    if (connectedOutput && connectedOutput == node->getFx()) {
      // The output node is connected to the xsheet node
      TPointD pos = xsheetFx->getAttributes()->getDagNodePos();
      if (pos != TConst::nowhere)
        nodeRect.translate(pos.x + 120, pos.y);
      else
        nodeRect.translate(sceneRect().center());
      while (!isAnEmptyZone(nodeRect)) nodeRect.translate(0, step);
    } else {
      // The output node is not connected to the xsheet node
      TFx *fx       = node->getFx();
      TFxPort *port = fx->getInputPort(0);
      TFx *inputFx  = port->getFx();
      if (inputFx) {
        // The output node is connected to another node
        TPointD pos = inputFx->getAttributes()->getDagNodePos();
        if (pos != TConst::nowhere)
          nodeRect.translate(pos.x + 120, pos.y);
        else {
          m_nodesToPlace[inputFx].append(node);
          return;
        }
      } else {
        // The output node is not connected
        QPointF pos = sceneRect().center();
        nodeRect.translate(pos);
      }
    }
    while (!isAnEmptyZone(nodeRect)) nodeRect.translate(0, step);
    QPointF newPos = nodeRect.topLeft();
    node->getFx()->getAttributes()->setDagNodePos(
        TPointD(newPos.x(), newPos.y()));
    node->setPos(newPos);
    return;
  } else if (node->isA(eXSheetFx)) {
    // I'm placing the xsheet node
    TFxSet *terminalFxs = fxDag->getTerminalFxs();
    int i;
    double maxX = m_firstPoint.x();
    for (i = 0; i < terminalFxs->getFxCount(); i++) {
      TFx *terminalFx = terminalFxs->getFx(i);
      if (terminalFx->getAttributes()->getDagNodePos() == TConst::nowhere)
        continue;
      maxX = std::max(maxX, terminalFx->getAttributes()->getDagNodePos().x);
    }
    TPointD oldPos = node->getFx()->getAttributes()->getDagNodePos();
    QPointF pos;
    if (oldPos == TConst::nowhere)
      pos = QPointF(maxX + 120, m_firstPoint.y());
    else
      pos = QPointF(maxX + 120 > oldPos.x ? maxX + 120 : oldPos.x, oldPos.y);
    node->getFx()->getAttributes()->setDagNodePos(TPointD(pos.x(), pos.y()));
    node->setPos(pos);
    return;
  } else if (node->isA(eMacroFx) || node->isA(eNormalFx) ||
             node->isA(eNormalLayerBlendingFx) || node->isA(eNormalMatteFx) ||
             node->isA(eNormalImageAdjustFx)) {
    // I'm placing an effect or a macro
    TFx *inputFx = node->getFx()->getInputPort(0)->getFx();
    QPointF pos;
    if (inputFx) {
      if (inputFx->getAttributes()->getDagNodePos() != TConst::nowhere) {
        TPointD dagPos =
            inputFx->getAttributes()->getDagNodePos() + TPointD(150, 0);
        pos = QPointF(dagPos.x, dagPos.y);
        nodeRect.moveTopLeft(pos);
        while (!isAnEmptyZone(nodeRect)) nodeRect.translate(0, -step);
        pos = nodeRect.topLeft();
      } else {
        m_nodesToPlace[inputFx].append(node);
        return;
      }
    } else {
      pos = sceneRect().center();
      nodeRect.moveTopLeft(pos);
      while (!isAnEmptyZone(nodeRect)) nodeRect.translate(0, -step);
      pos = nodeRect.topLeft();
    }
    node->getFx()->getAttributes()->setDagNodePos(TPointD(pos.x(), pos.y()));
    node->setPos(QPointF(pos));
    if (m_nodesToPlace.contains(node->getFx())) {
      QList<FxSchematicNode *> nodes = m_nodesToPlace[node->getFx()];
      int i;
      for (i = 0; i < nodes.size(); i++) placeNode(nodes[i]);
    }
    return;
  } else if (node->isA(eZeraryFx) || node->isA(eColumnFx) ||
             node->isA(eGroupedFx)) {
    // I'm placing a column
    nodeRect.translate(m_firstPoint);
    nodeRect.translate(10, 10);
    while (!isAnEmptyZone(nodeRect)) nodeRect.translate(0, -step);
    QPointF newPos = nodeRect.topLeft();
    node->getFx()->getAttributes()->setDagNodePos(
        TPointD(newPos.x(), newPos.y()));
    node->setPos(QPointF(newPos));
    return;
  }
}

//------------------------------------------------------------------

void FxSchematicScene::updateLink() {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Iterate the fxs table
  QMap<TFx *, FxSchematicNode *>::iterator it;
  for (it = m_table.begin(); it != m_table.end(); ++it) {
    FxSchematicNode *node = it.value();
    if (!node) continue;  // Should be asserted? Is it legal?

    TFx *fx           = it.key();
    TFx *inputPortsFx = fx;

    if (TZeraryColumnFx *fx2 = dynamic_cast<TZeraryColumnFx *>(fx)) {
      inputPortsFx = fx2->getZeraryFx();
      if (!inputPortsFx)
        return;  // Should really never happen. Should be asserted...
    }

    for (int i = 0; i != inputPortsFx->getInputPortCount(); ++i) {
      TFxPort *port = inputPortsFx->getInputPort(i);

      if (TFx *linkedFx = port->getFx()) {
        if (!linkedFx->getAttributes()->isGrouped() ||
            linkedFx->getAttributes()->isGroupEditing()) {
          // Not in a group / open group case
          assert(m_table.contains(linkedFx));

          if (m_table.contains(linkedFx)) {
            FxSchematicNode *linkedNode = m_table[linkedFx];
            SchematicPort *p0           = linkedNode->getOutputPort();
            SchematicPort *p1           = node->getInputPort(i);
            if (p0 && p1) p0->makeLink(p1);
          }
        } else {
          assert(
              m_groupedTable.contains(linkedFx->getAttributes()->getGroupId()));

          if (m_groupedTable.contains(
                  linkedFx->getAttributes()->getGroupId())) {
            FxSchematicNode *linkedNode =
                m_groupedTable[linkedFx->getAttributes()->getGroupId()];
            SchematicPort *p0 = linkedNode->getOutputPort();
            SchematicPort *p1 = node->getInputPort(i);
            if (p0 && p1) p0->makeLink(p1);
          }
        }
      }
    }
    if (xsh->getFxDag()->getTerminalFxs()->containsFx(fx)) {
      SchematicPort *p0 = node->getOutputPort();
      SchematicPort *p1 =
          m_table[xsh->getFxDag()->getXsheetFx()]->getInputPort(0);
      p0->makeLink(p1);
    }
  }
  QMap<int, FxGroupNode *>::iterator it2;
  for (it2 = m_groupedTable.begin(); it2 != m_groupedTable.end(); it2++) {
    FxGroupNode *node = it2.value();
    if (!node) continue;
    int i, fxCount = node->getFxCount();
    bool xsheetConnected = false;
    for (i = 0; i < fxCount; i++) {
      TFx *fx = node->getFx(i);
      if (xsh->getFxDag()->getTerminalFxs()->containsFx(fx) &&
          !xsheetConnected) {
        SchematicPort *p0 = node->getOutputPort();
        SchematicPort *p1 =
            m_table[xsh->getFxDag()->getXsheetFx()]->getInputPort(0);
        p0->makeLink(p1);
        xsheetConnected = true;
      }

      TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx);
      if (zfx) fx          = zfx->getZeraryFx();
      if (fx) {
        int j;
        for (j = 0; j < fx->getInputPortCount(); j++) {
          TFx *linkedFx = fx->getInputPort(j)->getFx();
          if (!linkedFx) continue;
          if (!linkedFx->getAttributes()->isGrouped() ||
              linkedFx->getAttributes()->isGroupEditing()) {
            assert(m_table.contains(linkedFx));
            if (m_table.contains(linkedFx)) {
              FxSchematicNode *linkedNode = m_table[linkedFx];
              SchematicPort *p0           = linkedNode->getOutputPort();
              SchematicPort *p1           = node->getInputPort(0);
              if (p0 && p1) p0->makeLink(p1);
            }
          } else {
            int linkedGroupId = linkedFx->getAttributes()->getGroupId();
            assert(m_groupedTable.contains(linkedGroupId));
            if (m_groupedTable.contains(linkedGroupId)) {
              FxGroupNode *linkedNode = m_groupedTable[linkedGroupId];
              if (linkedNode == node) continue;
              SchematicPort *p0 = linkedNode->getOutputPort();
              SchematicPort *p1 = node->getInputPort(0);
              if (p0 && p1 && !p0->isLinkedTo(p1)) p0->makeLink(p1);
            }
          }
        }
      }
    }
  }

  // to solve an edit macro problem: create a dummy link
  QMap<TMacroFx *, FxSchematicMacroEditor *>::iterator it3;
  for (it3 = m_macroEditorTable.begin(); it3 != m_macroEditorTable.end();
       it3++) {
    TMacroFx *macro = it3.key();
    int i;
    FxSchematicNode *root = m_table[macro->getRoot()];
    SchematicPort *p0     = root->getOutputPort();
    for (i = 0; i < macro->getOutputConnectionCount(); i++) {
      TFxPort *outConnection = macro->getOutputConnection(i);
      TFx *outFx             = outConnection->getOwnerFx();
      TMacroFx *outMacroFx   = dynamic_cast<TMacroFx *>(outFx);
      if (outMacroFx && outMacroFx->isEditing()) {
        std::vector<TFxP> fxs = outMacroFx->getFxs();
        int k;
        for (k = 0; k < (int)fxs.size(); k++) {
          TFx *fx = fxs[k].getPointer();
          int j;
          for (j = 0; j < fx->getInputPortCount(); j++)
            if (outConnection == fx->getInputPort(j)) {
              outFx = fx;
              break;
            }
          if (outFx != outMacroFx) break;
        }
      }

      int j;
      for (j = 0; j < outFx->getInputPortCount(); j++)
        if (outFx->getInputPort(j)->getFx() == macro) {
          SchematicPort *p1 = m_table[outFx]->getInputPort(j);
          p0->makeLink(p1);
          break;
        }
    }
    if (xsh->getFxDag()->getTerminalFxs()->containsFx(macro)) {
      assert(root);
      if (!root) continue;
      SchematicPort *p1 =
          m_table[xsh->getFxDag()->getXsheetFx()]->getInputPort(0);
      p0->makeLink(p1);
    }
  }
  updateDuplcatedNodesLink();
}

//------------------------------------------------------------------

void FxSchematicScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *cme) {
  QPointF scenePos                = cme->scenePos();
  QList<QGraphicsItem *> itemList = items(scenePos);
  if (!itemList.isEmpty()) {
    SchematicScene::contextMenuEvent(cme);
    return;
  }

  QMenu menu(views()[0]);

  if (cme->modifiers() & Qt::ControlModifier) {
    menu.addAction(m_addFxContextMenu.getAgainCommand(AddFxContextMenu::Add));
    if (!menu.actions().isEmpty()) {
      menu.exec(cme->screenPos());
      return;
    }
  }

  QAction *addOutputFx =
      CommandManager::instance()->getAction("MI_NewOutputFx");
  QAction *copy  = CommandManager::instance()->getAction("MI_Copy");
  QAction *cut   = CommandManager::instance()->getAction("MI_Cut");
  QAction *paste = CommandManager::instance()->getAction("MI_Paste");

  m_addFxContextMenu.setCurrentCursorScenePos(cme->scenePos());

  menu.addMenu(m_addFxContextMenu.getAddMenu());
  if (addOutputFx) menu.addAction(addOutputFx);
  menu.addSeparator();
  menu.addAction(copy);
  menu.addAction(cut);
  menu.addAction(paste);
  m_selection->setPastePosition(TPointD(scenePos.x(), scenePos.y()));
  menu.exec(cme->screenPos());
  m_selection->setPastePosition(TConst::nowhere);
}

//------------------------------------------------------------------

QPointF FxSchematicScene::nearestPoint(const QPointF &point) {
  QRectF rect(0, 0, 0.1, 0.1);
  rect.moveCenter(point);
  QList<QGraphicsItem *> itemList = items(rect);
  while (itemList.isEmpty()) {
    rect.adjust(-0.1, -0.1, 0.1, 0.1);
    itemList = items(rect);
  }
#if QT_VERSION >= 0x050000
  /*
  FIXME: QTransform() のデフォルトは Qt4.8 の itemAt() と比べて equivant
  だろうか？
*/
  QGraphicsItem *item = itemAt(rect.bottomLeft(), QTransform());
  if (item) return rect.bottomLeft();
  item = itemAt(rect.bottomRight(), QTransform());
  if (item) return rect.bottomRight();
  item = itemAt(rect.topLeft(), QTransform());
  if (item) return rect.topLeft();
  item = itemAt(rect.topRight(), QTransform());
#else
  QGraphicsItem *item = itemAt(rect.bottomLeft());
  if (item) return rect.bottomLeft();
  item = itemAt(rect.bottomRight());
  if (item) return rect.bottomRight();
  item = itemAt(rect.topLeft());
  if (item) return rect.topLeft();
  item                    = itemAt(rect.topRight());
#endif
  if (item) return rect.topRight();
  return QPointF();
}

//------------------------------------------------------------------

FxSchematicNode *FxSchematicScene::getFxNodeFromPosition(const QPointF &pos) {
  QList<QGraphicsItem *> pickedItems = items(pos);
  for (int i = 0; i < pickedItems.size(); i++) {
    FxSchematicNode *fxNode =
        dynamic_cast<FxSchematicNode *>(pickedItems.at(i));
    if (fxNode) return fxNode;
    FxSchematicPort *fxPort =
        dynamic_cast<FxSchematicPort *>(pickedItems.at(i));
    if (fxPort) return fxPort->getDock()->getNode();
  }
  return 0;
}

//------------------------------------------------------------------

void FxSchematicScene::updateDuplcatedNodesLink() {
  QMap<TFx *, FxSchematicNode *>::iterator it;

  // fx2node contains only duplicated nodes
  // and zerary duplicated node s
  QMap<TFx *, FxSchematicNode *> fx2node;
  for (it = m_table.begin(); it != m_table.end(); ++it) {
    TFx *fx               = it.key();
    FxSchematicNode *node = it.value();
    if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx)) {
      fx = zcfx->getZeraryFx();
      if (!fx) return;
    }
    assert(fx2node.count(fx) == 0);
    if (fx->getLinkedFx() == fx) continue;
    fx2node[fx] = node;
  }

  // trovo i link
  std::set<TFx *> visited;
  for (it = fx2node.begin(); it != fx2node.end(); ++it) {
    TFx *fx               = it.key();
    FxSchematicNode *node = it.value();
    assert(fx->getLinkedFx() != fx);
    if (visited.count(fx) > 0) continue;
    visited.insert(fx);
    FxSchematicNode *lastNode = node;
    assert(lastNode);
    FxSchematicPort *lastPort = lastNode->getLinkPort();
    assert(lastPort);
    for (fx = fx->getLinkedFx(); fx != it.key(); fx = fx->getLinkedFx()) {
      assert(visited.count(fx) == 0);
      if (visited.count(fx) > 0) break;
      visited.insert(fx);
      QMap<TFx *, FxSchematicNode *>::iterator h;
      h = fx2node.find(fx);
      if (h == fx2node.end()) continue;

      assert(h != fx2node.end());
      FxSchematicNode *node = h.value();
      assert(node);
      FxSchematicPort *port = node->getLinkPort();
      assert(port);
      if (port && lastPort) port->makeLink(lastPort);
      lastNode = node;
      lastPort = port;
    }
  }
}

//------------------------------------------------------------------

QGraphicsItem *FxSchematicScene::getCurrentNode() {
  QList<QGraphicsItem *> allItems = items();

  for (auto const item : allItems) {
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(item);
    if (node && node->getFx() == m_fxHandle->getFx()) return node;
  }
  return 0;
}

//------------------------------------------------------------------

void FxSchematicScene::onSelectionSwitched(TSelection *oldSel,
                                           TSelection *newSel) {
  if (m_selection == oldSel && m_selection != newSel) clearSelection();
}

//------------------------------------------------------------------

void FxSchematicScene::onSelectionChanged() {
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
    FxSchematicNode *node = dynamic_cast<FxSchematicNode *>(*it);
    if (node) {
      if (!node->isA(eGroupedFx)) {
        if (node->isA(eXSheetFx)) continue;
        m_selection->select(node->getFx());
        if (node->isA(eColumnFx)) {
          FxSchematicColumnNode *columnNode =
              dynamic_cast<FxSchematicColumnNode *>(node);
          if (columnNode)
            m_selection->select(columnNode->getColumnIndex());
          else {
            FxSchematicPaletteNode *paletteNode =
                dynamic_cast<FxSchematicPaletteNode *>(node);
            if (paletteNode) m_selection->select(paletteNode->getColumnIndex());
          }
        }
      } else {
        FxGroupNode *groupNode = dynamic_cast<FxGroupNode *>(node);
        assert(groupNode);
        QList<TFxP> fxs = groupNode->getGroupedFxs();
        for (i = 0; i < fxs.size(); i++) {
          m_selection->select(fxs[i].getPointer());
          TLevelColumnFx *colFx =
              dynamic_cast<TLevelColumnFx *>(fxs[i].getPointer());
          if (colFx) {
            if (TXshLevelColumn *column = colFx->getColumn()) {
              int colIndex = column->getIndex();
              m_selection->select(colIndex);
            }
          }
        }
      }
      highlightLinks(node, true);
    }
    SchematicLink *link = dynamic_cast<SchematicLink *>(*it);
    if (link) m_selection->select(link);
  }
  m_selection->makeCurrent();
  TSelectionHandle *selHandle = TSelectionHandle::getCurrent();
  selHandle->notifySelectionChanged();
}

//------------------------------------------------------------------

void FxSchematicScene::reorderScene() {
  int step = m_gridDimension == eLarge ? 100 : 50;
  m_placedFxs.clear();
  QPointF sceneCenter = sceneRect().center();
  double minY         = sceneCenter.y();
  double maxX         = sceneCenter.x();
  double y            = minY;
  double x            = maxX;

  TXsheet *xsh = m_xshHandle->getXsheet();
  int i        = 0;
  for (i = 0; i < xsh->getColumnCount(); i++) {
    TXshColumn *column = xsh->getColumn(i);
    TFx *fx            = column->getFx();

    if (column->isEmpty() || !fx) continue;

    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx);
    if (zfx && (zfx->getZeraryFx()->getInputPortCount() > 0)) {
      TFxPort *port = zfx->getZeraryFx()->getInputPort(0);
      if (port && port->getFx()) continue;
    }

    if (zfx && m_placedFxs.contains(zfx->getZeraryFx())) continue;

    x = sceneCenter.x();
    placeNodeAndParents(fx, x, maxX, minY);
    y -= step;
    minY = std::min(y, minY);
  }

  // remove retrolink
  for (i = 0; i < xsh->getColumnCount(); i++) {
    TXshColumn *column = xsh->getColumn(i);
    TFx *fx            = column->getFx();
    if (column->isEmpty() || !fx) continue;

    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx);
    if (zfx && m_placedFxs.contains(zfx->getZeraryFx())) continue;

    if (zfx && (zfx->getZeraryFx()->getInputPortCount() > 0)) {
      TFxPort *port = zfx->getZeraryFx()->getInputPort(0);
      if (port && port->getFx()) continue;
    }

    for (int j = 0; j < fx->getOutputConnectionCount(); j++) {
      TFx *outFx = fx->getOutputConnection(j)->getOwnerFx();
      removeRetroLinks(outFx, maxX);
    }
  }

  double middleY = (sceneCenter.y() + minY + step) * 0.5;
  placeNodeAndParents(xsh->getFxDag()->getXsheetFx(), maxX, maxX, middleY);

  FxDag *fxDag  = xsh->getFxDag();
  TFxSet *fxSet = fxDag->getInternalFxs();
  for (i = 0; i < fxSet->getFxCount(); i++) {
    TFx *fx = fxSet->getFx(i);
    if (m_placedFxs.contains(fx)) continue;

    fx->getAttributes()->setDagNodePos(TPointD(sceneCenter.x() + 120, minY));
    minY -= step;
  }
  updateScene();
}

//------------------------------------------------------------------

void FxSchematicScene::removeRetroLinks(TFx *fx, double &maxX) {
  if (!fx) return;
  for (int i = 0; i < fx->getInputPortCount(); i++) {
    TFx *inFx = fx->getInputPort(i)->getFx();
    if (!inFx) continue;
    TPointD inFxPos = inFx->getAttributes()->getDagNodePos();
    TPointD fxPos   = fx->getAttributes()->getDagNodePos();
    if (fxPos.x <= inFxPos.x) {
      while (fxPos.x <= inFxPos.x) fxPos.x += 150;
      maxX = std::max(fxPos.x + 150, maxX);
      fx->getAttributes()->setDagNodePos(fxPos);
      for (int j = 0; j < fx->getOutputConnectionCount(); j++) {
        TFx *outFx = fx->getOutputConnection(j)->getOwnerFx();
        removeRetroLinks(outFx, maxX);
      }
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::placeNodeAndParents(TFx *fx, double x, double &maxX,
                                           double &minY) {
  int step = m_gridDimension == eLarge ? 100 : 50;
  if (!fx) return;
  m_placedFxs.append(fx);
  if (fx->getFxType() == "STD_particlesFx" ||
      fx->getFxType() == "STD_Iwa_ParticlesFx") {
    TXsheet *xsh = m_xshHandle->getXsheet();
    int i        = 0;
    for (i = 0; i < xsh->getColumnCount(); i++) {
      TFx *columnFx        = xsh->getColumn(i)->getFx();
      TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(columnFx);
      if (zfx && zfx->getZeraryFx() == fx) {
        fx = zfx;
        break;
      }
    }
  }
  double y = minY;
  fx->getAttributes()->setDagNodePos(TPointD(x, y));
  if (fx->getOutputConnectionCount() == 0) minY -= step;
  x += 120;
  maxX = std::max(maxX, x);
  int i;
  for (i = 0; i < fx->getOutputConnectionCount(); i++) {
    TFx *outputFx = fx->getOutputConnection(i)->getOwnerFx();
    // controllo se e' una porta sorgente
    TFxPort *port = outputFx->getInputPort(0);
    if (port && port->getFx() != fx) continue;
    if (!m_placedFxs.contains(outputFx) ||
        outputFx->getAttributes()->getDagNodePos().x < x) {
      placeNodeAndParents(outputFx, x, maxX, minY);
      y -= step;
      minY = std::min(y, minY);
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::onDisconnectFromXSheet() {
  std::list<TFxP, std::allocator<TFxP>> list =
      m_selection->getFxs().toStdList();
  TFxCommand::disconnectNodesFromXsheet(list, m_xshHandle);
}

//------------------------------------------------------------------

void FxSchematicScene::onConnectToXSheet() {
  std::list<TFxP, std::allocator<TFxP>> list =
      m_selection->getFxs().toStdList();
  TFxCommand::connectNodesToXsheet(list, m_xshHandle);
}

//------------------------------------------------------------------

void FxSchematicScene::onDeleteFx() {
  std::list<TFxP, std::allocator<TFxP>> fxList =
      m_selection->getFxs().toStdList();
  std::list<TFxCommand::Link> linkList = m_selection->getLinks().toStdList();
  std::list<int> columnIndexList = m_selection->getColumnIndexes().toStdList();
  TFxCommand::deleteSelection(fxList, linkList, columnIndexList, m_xshHandle,
                              m_fxHandle);
}

//------------------------------------------------------------------

void FxSchematicScene::onDuplicateFx() {
  QList<TFxP> fxs = m_selection->getFxs();
  if (fxs.empty()) return;

  TUndoManager::manager()->beginBlock();

  int i, size = fxs.size();
  for (i = 0; i != size; ++i)
    TFxCommand::duplicateFx(fxs[i].getPointer(), m_xshHandle, m_fxHandle);

  TUndoManager::manager()->endBlock();
}

//------------------------------------------------------------------

void FxSchematicScene::onUnlinkFx() {
  QList<TFxP> fxs = m_selection->getFxs();
  if (fxs.empty()) return;

  TUndoManager::manager()->beginBlock();

  int i, size = fxs.size();
  for (i = 0; i != size; ++i)
    TFxCommand::unlinkFx(fxs[i].getPointer(), m_fxHandle, m_xshHandle);

  TUndoManager::manager()->endBlock();
}

//------------------------------------------------------------------

void FxSchematicScene::onMacroFx() {
  TFxCommand::makeMacroFx(m_selection->getFxs().toVector().toStdVector(),
                          m_app);
}

//------------------------------------------------------------------

void FxSchematicScene::onExplodeMacroFx() {
  if (TMacroFx *macroFx = dynamic_cast<TMacroFx *>(m_fxHandle->getFx()))
    TFxCommand::explodeMacroFx(macroFx, m_app);
}

//------------------------------------------------------------------

void FxSchematicScene::onOpenMacroFx() {
  if (TMacroFx *macroFx = dynamic_cast<TMacroFx *>(m_fxHandle->getFx())) {
    macroFx->editMacro(true);
    updateScene();
  }
}

//------------------------------------------------------------------

void FxSchematicScene::onSavePresetFx() {
  CommandManager::instance()->getAction("MI_SavePreset")->trigger();
}

//------------------------------------------------------------------

void FxSchematicScene::onRemoveOutput() {
  TFxCommand::removeOutputFx(m_fxHandle->getFx(), m_xshHandle, m_fxHandle);
}

//------------------------------------------------------------------

void FxSchematicScene::onActivateOutput() {
  TFxCommand::makeOutputFxCurrent(m_fxHandle->getFx(), m_xshHandle);
}

//------------------------------------------------------------------

void FxSchematicScene::onPreview() { emit showPreview(m_fxHandle->getFx()); }

//------------------------------------------------------------------

void FxSchematicScene::onCacheFx() { setEnableCache(true); }

//------------------------------------------------------------------

void FxSchematicScene::onUncacheFx() { setEnableCache(false); }

//------------------------------------------------------------------

void FxSchematicScene::setEnableCache(bool toggle) {
  QList<TFxP> selectedFxs = m_selection->getFxs();
  int i;
  for (i = 0; i < selectedFxs.size(); i++) {
    TFx *fx               = selectedFxs[i].getPointer();
    TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx);
    if (zcfx) fx          = zcfx->getZeraryFx();
    TFxAttributes *attr   = fx->getAttributes();
    if (!attr->isGrouped() || attr->isGroupEditing())
      if (toggle)
        TPassiveCacheManager::instance()->enableCache(fx);
      else
        TPassiveCacheManager::instance()->disableCache(fx);
    else {
      QMap<int, FxGroupNode *>::iterator it;
      for (it = m_groupedTable.begin(); it != m_groupedTable.end(); it++) {
        FxGroupNode *group = it.value();
        QList<TFxP> roots  = group->getRootFxs();
        int j;
        for (j = 0; j < roots.size(); j++)
          if (fx == roots[j].getPointer())
            if (toggle)
              TPassiveCacheManager::instance()->enableCache(fx);
            else
              TPassiveCacheManager::instance()->disableCache(fx);
        group->update();
      }
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::onCollapse(const QList<TFxP> &fxs) {
  emit doCollapse(fxs);
}

//------------------------------------------------------------------

void FxSchematicScene::onOpenSubxsheet() {
  CommandManager *cm = CommandManager::instance();
  cm->execute("MI_OpenChild");
}

//------------------------------------------------------------------

TXsheet *FxSchematicScene::getXsheet() { return m_xshHandle->getXsheet(); }

//------------------------------------------------------------------

void FxSchematicScene::onXsheetChanged() { m_xshHandle->notifyXsheetChanged(); }

//------------------------------------------------------------------

void FxSchematicScene::onSceneChanged() {
  m_app->getCurrentScene()->notifySceneChanged();
}

//------------------------------------------------------------------

void FxSchematicScene::onSwitchCurrentFx(TFx *fx) {
  if (m_fxHandle->getFx() == fx) return;
  if (fx) {
    // Forbid update of the swatch upon column switch. This could generate
    // a further useless render...
    SwatchViewer::suspendRendering(true, false);

    int columnIndex = fx->getReferenceColumnIndex();
    if (columnIndex >= 0) {
      m_columnHandle->setColumnIndex(columnIndex);
      m_app->getCurrentObject()->setObjectId(
          TStageObjectId::ColumnId(columnIndex));
    }

    SwatchViewer::suspendRendering(false);

    m_fxHandle->setFx(fx, false);  // Setting the fx updates the swatch

    emit editObject();
  } else {
    m_fxHandle->setFx(0, false);
  }
}

//------------------------------------------------------------------

void FxSchematicScene::onFxNodeDoubleClicked() {
  // emitting fxSettingsShouldBeSwitched
  m_fxHandle->onFxNodeDoubleClicked();
}

//------------------------------------------------------------------

void FxSchematicScene::onCurrentFxSwitched() {
  if (m_currentFxNode) m_currentFxNode->setIsCurrentFxLinked(false, 0);
  if (m_table.contains(m_fxHandle->getFx())) {
    m_currentFxNode = m_table[m_fxHandle->getFx()];
    m_currentFxNode->setIsCurrentFxLinked(true, 0);
  } else
    m_currentFxNode = 0;
}

//------------------------------------------------------------------

void FxSchematicScene::onCurrentColumnChanged(int index) {
  m_app->getCurrentColumn()->setColumnIndex(index);
  m_app->getCurrentObject()->setObjectId(TStageObjectId::ColumnId(index));
}

//------------------------------------------------------------------

TFx *FxSchematicScene::getCurrentFx() { return m_fxHandle->getFx(); }

//------------------------------------------------------------------

void FxSchematicScene::mousePressEvent(QGraphicsSceneMouseEvent *me) {
  QList<QGraphicsItem *> items = selectedItems();
#if QT_VERSION >= 0x050000
  QGraphicsItem *item = itemAt(me->scenePos(), QTransform());
#else
  QGraphicsItem *item     = itemAt(me->scenePos());
#endif
  FxSchematicPort *port = dynamic_cast<FxSchematicPort *>(item);
  FxSchematicLink *link = dynamic_cast<FxSchematicLink *>(item);
  SchematicScene::mousePressEvent(me);
  onSelectionChanged();
  if (me->button() == Qt::MidButton) {
    int i;
    for (i = 0; i < items.size(); i++) items[i]->setSelected(true);
  }
  /*
  m_selection may not be updated here, so I use QGraphicsScene::selectedItems()
  instead of m_selection->isEmpty() to check whether any node is selected or
  not.
  */
  if (selectedItems().isEmpty()) {
    if (me->button() != Qt::MidButton && !item) m_fxHandle->setFx(0, false);
    return;
  }
  m_isConnected = false;
  if (!canDisconnectSelection(m_selection)) return;
  m_selectionOldPos.clear();
  QList<TFxP> selectedFxs = m_selection->getFxs();
  int i;
  for (i = 0; i < selectedFxs.size(); i++) {
    TFxP selectedFx = selectedFxs[i];
    TPointD pos     = selectedFx->getAttributes()->getDagNodePos();
    m_selectionOldPos.append(QPair<TFxP, TPointD>(selectedFx, pos));
  }
  FxsData fxsData;
  fxsData.setFxs(m_selection->getFxs(), m_selection->getLinks(),
                 m_selection->getColumnIndexes(), m_xshHandle->getXsheet());
  if (fxsData.isConnected() && me->button() == Qt::LeftButton && !port && !link)
    m_isConnected = true;
}

//------------------------------------------------------------------

void FxSchematicScene::mouseMoveEvent(QGraphicsSceneMouseEvent *me) {
  SchematicScene::mouseMoveEvent(me);

  m_lastPos = me->scenePos();

  bool leftButton = (QApplication::mouseButtons() == Qt::LeftButton);
  bool altButton  = (QApplication::keyboardModifiers() == Qt::AltModifier);

  if (leftButton && m_isConnected && altButton) {
    m_linkUnlinkSimulation = true;

    simulateDisconnectSelection(true);
    m_connectionLinks.showBridgeLinks();

#if QT_VERSION >= 0x050000
    SchematicLink *link =
        dynamic_cast<SchematicLink *>(itemAt(m_lastPos, QTransform()));
#else
    SchematicLink *link   = dynamic_cast<SchematicLink *>(itemAt(m_lastPos));
#endif
    if (link && (link->getEndPort() && link->getStartPort())) {
      TFxCommand::Link fxLink = m_selection->getBoundingFxs(link);
      if (fxLink == TFxCommand::Link()) return;

      TFx *inputFx  = fxLink.m_inputFx.getPointer();
      TFx *outputFx = fxLink.m_outputFx.getPointer();

      TFxSet *internalFxs = getXsheet()->getFxDag()->getInternalFxs();
      if (!internalFxs->containsFx(inputFx) &&
          !dynamic_cast<TColumnFx *>(inputFx) &&
          !dynamic_cast<TXsheetFx *>(inputFx) &&
          !dynamic_cast<TOutputFx *>(inputFx))
        return;
      if (!internalFxs->containsFx(outputFx) &&
          !dynamic_cast<TColumnFx *>(outputFx) &&
          !dynamic_cast<TXsheetFx *>(outputFx) &&
          !dynamic_cast<TOutputFx *>(outputFx))
        return;
    }

    m_connectionLinks.hideBridgeLinks();
    simulateInsertSelection(link, altButton && !!link);
  }
}

//------------------------------------------------------------------

void FxSchematicScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *me) {
  SchematicScene::mouseReleaseEvent(me);

  m_linkUnlinkSimulation = false;

  if ((m_disconnectionLinks.size() == 0) && (m_connectionLinks.size() == 0))
    return;

  TUndoManager::manager()->beginBlock();

  bool altButton = QApplication::keyboardModifiers() == Qt::AltModifier;
  if (altButton && m_isConnected) {
    if (m_connectionLinks.size() > 0) {
      const QList<SchematicLink *> &bridgeLinks =
          m_connectionLinks.getBridgeLinks();
      assert(bridgeLinks.size() <= 1);

      SchematicLink *link = bridgeLinks[0];

      if (link) {
        FxSchematicNode *outputNode =
            dynamic_cast<FxSchematicNode *>(link->getEndPort()->getNode());
        FxSchematicNode *inputNode =
            dynamic_cast<FxSchematicNode *>(link->getStartPort()->getNode());

        if (inputNode && outputNode) {
          SchematicPort *port = link->getStartPort();
          if (port->getType() == 200 || port->getType() == 204)
            port = link->getOtherPort(port);

          int i;
          for (i = 0; i < outputNode->getInputPortCount(); i++)
            if (port == outputNode->getInputPort(i)) break;

          TFxCommand::Link fxLink;
          fxLink.m_outputFx                               = outputNode->getFx();
          fxLink.m_inputFx                                = inputNode->getFx();
          if (!outputNode->isA(eXSheetFx)) fxLink.m_index = i;

          TFxCommand::connectFxs(fxLink, m_selection->getFxs().toStdList(),
                                 m_xshHandle, m_selectionOldPos);
        }
      }
    } else if (m_disconnectionLinks.size() > 0) {
      QList<TFxP> fxs = m_selection->getFxs();
      TFxCommand::disconnectFxs(fxs.toStdList(), m_xshHandle,
                                m_selectionOldPos);
      m_selectionOldPos.clear();
    }
  }

  TUndoManager::manager()->endBlock();

  m_isConnected = false;
}

//------------------------------------------------------------------

bool FxSchematicScene::event(QEvent *e) {
  bool ret        = SchematicScene::event(e);
  bool altPressed = QApplication::keyboardModifiers() == Qt::AltModifier;
  if (m_linkUnlinkSimulation && m_altPressed != altPressed)
    onAltModifierChanged(altPressed);
  m_altPressed = altPressed;
  return ret;
}

//------------------------------------------------------------------

void FxSchematicScene::onInsertPaste() {
  if (!m_selection->insertPasteSelection())
    DVGui::error(
        tr("Cannot Paste Insert a selection of unconnected FX nodes.\nSelect "
           "FX nodes and related links before copying or cutting the selection "
           "you want to paste."));
}

//------------------------------------------------------------------

void FxSchematicScene::onAddPaste() {
  if (!m_selection->addPasteSelection())
    DVGui::error(
        tr("Cannot Paste Add a selection of unconnected FX nodes.\nSelect FX "
           "nodes and related links before copying or cutting the selection "
           "you want to paste."));
}

//------------------------------------------------------------------

void FxSchematicScene::onReplacePaste() {
  if (!m_selection->replacePasteSelection())
    DVGui::error(
        tr("Cannot Paste Replace a selection of unconnected FX nodes.\nSelect "
           "FX nodes and related links before copying or cutting the selection "
           "you want to paste."));
}

//------------------------------------------------------------------

void FxSchematicScene::onAltModifierChanged(bool altPressed) {
  if (altPressed) {
    if (m_disconnectionLinks.size() == 0 && m_linkUnlinkSimulation)
      simulateDisconnectSelection(altPressed);
    if (m_connectionLinks.size() == 0 && m_linkUnlinkSimulation) {
      m_connectionLinks.showBridgeLinks();
#if QT_VERSION >= 0x050000
      SchematicLink *link =
          dynamic_cast<SchematicLink *>(itemAt(m_lastPos, QTransform()));
#else
      SchematicLink *link = dynamic_cast<SchematicLink *>(itemAt(m_lastPos));
#endif
      if (link && (!link->getEndPort() || !link->getStartPort())) return;
      m_connectionLinks.hideBridgeLinks();
      simulateInsertSelection(link, altPressed && !!link);
    }
  } else {
    if (m_disconnectionLinks.size() > 0 && m_linkUnlinkSimulation)
      simulateDisconnectSelection(altPressed);
    if (m_connectionLinks.size() > 0 && m_linkUnlinkSimulation) {
      m_connectionLinks.showBridgeLinks();
#if QT_VERSION >= 0x050000
      SchematicLink *link =
          dynamic_cast<SchematicLink *>(itemAt(m_lastPos, QTransform()));
#else
      SchematicLink *link = dynamic_cast<SchematicLink *>(itemAt(m_lastPos));
#endif
      if (link && (!link->getEndPort() || !link->getStartPort())) return;
      m_connectionLinks.hideBridgeLinks();
      simulateInsertSelection(link, altPressed && !!link);
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::onEditGroup() {
  if (m_selection->isEmpty()) return;
  QList<TFxP> fxs = m_selection->getFxs();
  int i;
  for (i = 0; i < fxs.size(); i++) {
    if (fxs[i]->getAttributes()->isGrouped() &&
        !fxs[i]->getAttributes()->isGroupEditing()) {
      fxs[i]->getAttributes()->editGroup();
      TMacroFx *macro = dynamic_cast<TMacroFx *>(fxs[i].getPointer());
      if (macro) {
        std::vector<TFxP> macroFxs = macro->getFxs();
        int j;
        for (j = 0; j < (int)macroFxs.size(); j++)
          macroFxs[j]->getAttributes()->editGroup();
      }
    }
  }
  updateScene();
}

//------------------------------------------------------------------

void FxSchematicScene::highlightLinks(FxSchematicNode *node, bool value) {
  int i, portCount = node->getInputPortCount();
  // SchematicLink* ghostLink = m_supportLinks.getDisconnectionLink(eGhost);
  for (i = 0; i < portCount; i++) {
    FxSchematicPort *port = node->getInputPort(i);
    int j, linkCount = port->getLinkCount();
    for (j = 0; j < linkCount; j++) {
      SchematicLink *link = port->getLink(j);
      if (!link) continue;
      if (m_disconnectionLinks.isABridgeLink(link)) continue;
      link->setHighlighted(value);
      link->update();
      m_highlightedLinks.push_back(link);
    }
  }

  FxSchematicPort *port = node->getOutputPort();
  if (port) {
    int linkCount = port->getLinkCount();
    for (i = 0; i < linkCount; i++) {
      SchematicLink *link = port->getLink(i);
      if (!link) continue;
      if (m_disconnectionLinks.isABridgeLink(link)) continue;
      link->setHighlighted(value);
      link->update();
      m_highlightedLinks.push_back(link);
    }
  }
  port = node->getLinkPort();
  if (port) {
    SchematicLink *link = port->getLink(0);
    if (link && !m_disconnectionLinks.isABridgeLink(link)) {
      link->setHighlighted(value);
      link->update();
      m_highlightedLinks.push_back(link);
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::simulateDisconnectSelection(bool disconnect) {
  if (m_selection->isEmpty()) return;
  QList<TFxP> selectedFxs = m_selection->getFxs();
  if (selectedFxs.isEmpty()) return;
  QMap<TFx *, bool> visitedFxs;
  int i;
  for (i                                    = 0; i < selectedFxs.size(); i++)
    visitedFxs[selectedFxs[i].getPointer()] = false;

  TFx *inputFx = 0, *outputFx = 0;
  findBoundariesFxs(inputFx, outputFx, visitedFxs);
  FxSchematicNode *inputNode  = m_table[inputFx];
  FxSchematicNode *outputNode = m_table[outputFx];
  assert(inputNode && outputNode);

  FxSchematicPort *inputPort = 0, *outputPort = 0;
  SchematicPort *otherInputPort = 0;
  QList<SchematicPort *> otherOutputPorts;
  if (inputNode->getInputPortCount() > 0) {
    inputPort = inputNode->getInputPort(0);
    if (inputPort) {
      SchematicLink *inputLink = inputPort->getLink(0);
      if (inputLink && !m_connectionLinks.isAnInputLink(inputLink)) {
        if (!m_disconnectionLinks.isAnInputLink(inputLink))
          m_disconnectionLinks.addInputLink(inputLink);
        otherInputPort = inputLink->getOtherPort(inputPort);
      }
    }
  }
  outputPort = outputNode->getOutputPort();
  if (outputPort) {
    for (i = 0; i < outputPort->getLinkCount(); i++) {
      SchematicLink *outputLink = outputPort->getLink(i);
      if (outputLink && !m_connectionLinks.isAnOutputLink(outputLink)) {
        if (!m_disconnectionLinks.isAnOutputLink(outputLink))
          m_disconnectionLinks.addOutputLink(outputLink);
        otherOutputPorts.push_back(outputLink->getOtherPort(outputPort));
      }
    }
  }
  if (disconnect) {
    m_disconnectionLinks.hideInputLinks();
    m_disconnectionLinks.hideOutputLinks();
  } else {
    m_disconnectionLinks.showInputLinks();
    m_disconnectionLinks.showOutputLinks();
    m_disconnectionLinks.removeInputLinks();
    m_disconnectionLinks.removeOutputLinks();
  }

  if (otherInputPort) {
    if (disconnect) {
      for (i = 0; i < otherOutputPorts.size(); i++)
        m_disconnectionLinks.addBridgeLink(
            otherOutputPorts[i]->makeLink(otherInputPort));
    } else
      m_disconnectionLinks.removeBridgeLinks(true);
  }
}

//------------------------------------------------------------------

void FxSchematicScene::simulateInsertSelection(SchematicLink *link,
                                               bool connect) {
  if (!link) {
    m_connectionLinks.showBridgeLinks();
    m_connectionLinks.hideInputLinks();
    m_connectionLinks.hideOutputLinks();
    m_connectionLinks.removeBridgeLinks();
    m_connectionLinks.removeInputLinks(true);
    m_connectionLinks.removeOutputLinks(true);
  } else {
    if (m_disconnectionLinks.isABridgeLink(link) || m_selection->isEmpty())
      return;

    if (connect) {
      m_connectionLinks.addBridgeLink(link);
      m_connectionLinks.hideBridgeLinks();
    } else {
      m_connectionLinks.showBridgeLinks();
      m_connectionLinks.removeBridgeLinks();
    }

    SchematicPort *inputPort = 0, *outputPort = 0;
    if (link) {
      if (link->getStartPort()->getType() == eFxInputPort) {
        inputPort  = link->getStartPort();
        outputPort = link->getEndPort();
      } else {
        inputPort  = link->getEndPort();
        outputPort = link->getStartPort();
      }
    }

    QMap<TFx *, bool> visitedFxs;
    QList<TFxP> selectedFxs = m_selection->getFxs();
    if (selectedFxs.isEmpty()) return;
    int i;
    for (i                                    = 0; i < selectedFxs.size(); i++)
      visitedFxs[selectedFxs[i].getPointer()] = false;

    TFx *inputFx = 0, *outputFx = 0;
    findBoundariesFxs(inputFx, outputFx, visitedFxs);
    FxSchematicNode *inputNode  = m_table[inputFx];
    FxSchematicNode *outputNode = m_table[outputFx];
    assert(inputNode && outputNode);

    if (inputNode->getInputPortCount() > 0) {
      SchematicPort *inputNodePort = inputNode->getInputPort(0);
      if (inputNodePort && outputPort)
        m_connectionLinks.addInputLink(inputNodePort->makeLink(outputPort));
    }

    SchematicPort *outputNodePort = outputNode->getOutputPort();
    if (outputNodePort && inputPort)
      m_connectionLinks.addOutputLink(inputPort->makeLink(outputNodePort));

    if (connect) {
      m_connectionLinks.showInputLinks();
      m_connectionLinks.showOutputLinks();
    } else {
      m_connectionLinks.hideInputLinks();
      m_connectionLinks.hideOutputLinks();
      m_connectionLinks.removeInputLinks(true);
      m_connectionLinks.removeOutputLinks(true);
    }
  }
}
//------------------------------------------------------------
/*! in order to select nods after pasting the copied fx nodes from FxSelection
*/
void FxSchematicScene::selectNodes(QList<TFxP> &fxs) {
  clearSelection();
  for (int i = 0; i < (int)fxs.size(); i++) {
    TFx *tempFx = fxs[i].getPointer();

    QMap<TFx *, FxSchematicNode *>::iterator it;
    it = m_table.find(tempFx);
    if (it == m_table.end()) continue;

    it.value()->setSelected(true);
  }
  update();
}

//------------------------------------------------------------------

void FxSchematicScene::updateNestedGroupEditors(FxSchematicNode *node,
                                                const QPointF &newPos) {
  if (!node) return;
  QStack<int> groupIdStack = node->getFx()->getAttributes()->getGroupIdStack();
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
        rect              = rect.unite(app);
#endif
    }
  }
  QMap<TMacroFx *, FxSchematicMacroEditor *>::iterator it;
  for (it = m_macroEditorTable.begin(); it != m_macroEditorTable.end(); it++) {
    if (it.value()->contains(node)) {
      QRectF app = it.value()->sceneBoundingRect();
      if (rect.isEmpty())
        rect = app;
      else
#if QT_VERSION >= 0x050000
        rect = rect.united(app);
#else
        rect              = rect.unite(app);
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
    rect                  = rect.unite(m_groupEditorTable[groupIdStack[i]]->sceneBoundingRect());
#endif
    QRectF app = m_groupEditorTable[groupIdStack[i]]->boundingSceneRect();
    if (m_groupEditorTable[groupIdStack[i]]->scenePos() != app.topLeft())
      m_groupEditorTable[groupIdStack[i]]->setPos(app.topLeft());
  }
  for (it = m_macroEditorTable.begin(); it != m_macroEditorTable.end(); it++) {
    FxSchematicMacroEditor *editor = it.value();
    if (editor->contains(node)) {
      QRectF app = editor->sceneBoundingRect();
#if QT_VERSION >= 0x050000
      rect = rect.united(app);
#else
      rect                = rect.unite(app);
#endif
      app = editor->boundingSceneRect();
      if (editor->scenePos() != app.topLeft()) editor->setPos(app.topLeft());
    }
  }
  update(rect);
}

//------------------------------------------------------------------

void FxSchematicScene::closeInnerMacroEditor(int groupId) {
  assert(m_groupEditorTable.contains(groupId));
  QMap<TMacroFx *, FxSchematicMacroEditor *>::iterator it;
  for (it = m_macroEditorTable.begin(); it != m_macroEditorTable.end(); it++) {
    TMacroFx *macro = it.key();
    assert(macro);
    if (macro->getAttributes()->isContainedInGroup(groupId)) {
      macro->editMacro(false);
      macro->getAttributes()->closeEditingGroup(groupId);
    }
  }
}

//------------------------------------------------------------------

void FxSchematicScene::resizeNodes(bool maximizedNode) {
  // resize nodes
  m_gridDimension = maximizedNode ? eLarge : eSmall;
  m_xshHandle->getXsheet()->getFxDag()->setDagGridDimension(m_gridDimension);
  QMap<TFx *, FxSchematicNode *>::iterator it1;
  for (it1 = m_table.begin(); it1 != m_table.end(); it1++) {
    if (!it1.value()) continue;
    it1.value()->resize(maximizedNode);
    TFx *fx = it1.value()->getFx();
    updatePositionOnResize(fx, maximizedNode);
  }
  QMap<int, FxGroupNode *>::iterator it2;
  for (it2 = m_groupedTable.begin(); it2 != m_groupedTable.end(); it2++) {
    if (!it2.value()) continue;
    it2.value()->resize(maximizedNode);
    QList<TFxP> groupedFxs = it2.value()->getGroupedFxs();
    for (int i = 0; i < groupedFxs.size(); i++)
      updatePositionOnResize(groupedFxs[i].getPointer(), maximizedNode);
  }

  QMap<TMacroFx *, FxSchematicMacroEditor *>::iterator it3;
  for (it3 = m_macroEditorTable.begin(); it3 != m_macroEditorTable.end();
       it3++) {
    if (!it3.value()) continue;
    it3.value()->resizeNodes(maximizedNode);
  }
  updateScene();
}

//------------------------------------------------------------------

void FxSchematicScene::updatePositionOnResize(TFx *fx, bool maximizedNode) {
  TPointD oldPos = fx->getAttributes()->getDagNodePos();
  double oldPosY = oldPos.y - 25000;
  double newPosY = maximizedNode ? oldPosY * 2 : oldPosY * 0.5;
  fx->getAttributes()->setDagNodePos(TPointD(oldPos.x, newPosY + 25000));
}
