

// TnzCore includes
#include "tconst.h"
#include "tundo.h"

// TnzBase includes
#include "tfxattributes.h"

// TnzLib includes
#include "toonz/tcolumnfx.h"
#include "toonz/fxcommand.h"
#include "toonz/fxdag.h"
#include "toonz/txsheet.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txsheethandle.h"
#include "toonzqt/fxschematicscene.h"

// TnzQt includes
#include "toonzqt/schematicnode.h"
#include "toonzqt/fxschematicnode.h"
#include "toonzqt/selectioncommandids.h"
#include "fxdata.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

#include "toonzqt/fxselection.h"

namespace {
bool canGroup(TFx *fx) {
  TXsheetFx *xfx = dynamic_cast<TXsheetFx *>(fx);
  TOutputFx *ofx = dynamic_cast<TOutputFx *>(fx);
  return (!xfx && !ofx);
}
}  // namespace

//=========================================================
//
// FxSelection
//
//=========================================================

FxSelection::FxSelection()
    : m_xshHandle(0)
    , m_fxHandle(0)
    , m_pastePosition(TConst::nowhere)
    , m_schematicScene(0) {}

//---------------------------------------------------------

FxSelection::FxSelection(const FxSelection &src)
    : m_selectedFxs(src.m_selectedFxs)
    , m_selectedLinks(src.m_selectedLinks)
    , m_xshHandle(src.m_xshHandle)
    , m_fxHandle(src.m_fxHandle)
    , m_selectedColIndexes(src.m_selectedColIndexes)
    , m_pastePosition(TConst::nowhere)
    , m_schematicScene(src.m_schematicScene) {}

//---------------------------------------------------------

FxSelection::~FxSelection() {}

//---------------------------------------------------------

void FxSelection::enableCommands() {
  enableCommand(this, MI_Clear, &FxSelection::deleteSelection);
  enableCommand(this, MI_Cut, &FxSelection::cutSelection);
  enableCommand(this, MI_Copy, &FxSelection::copySelection);
  enableCommand(this, MI_Paste, &FxSelection::pasteSelection);
  enableCommand(this, MI_Group, &FxSelection::groupSelection);
  enableCommand(this, MI_Ungroup, &FxSelection::ungroupSelection);
  enableCommand(this, MI_Collapse, &FxSelection::collapseSelection);
  enableCommand(this, MI_ExplodeChild, &FxSelection::explodeChild);
}

//---------------------------------------------------------

TSelection *FxSelection::clone() const {
  assert(0);
  return new FxSelection(*this);
}

//---------------------------------------------------------

void FxSelection::select(TFxP fx) { m_selectedFxs.append(fx); }

//---------------------------------------------------------

void FxSelection::select(int colIndex) {
  m_selectedColIndexes.append(colIndex);
  std::sort(m_selectedColIndexes.begin(), m_selectedColIndexes.end());
}

//---------------------------------------------------------

void FxSelection::unselect(int colIndex) {
  m_selectedColIndexes.removeOne(colIndex);
}

//---------------------------------------------------------

void FxSelection::unselect(TFxP fx) {
  int index = m_selectedFxs.indexOf(fx);
  if (index >= 0) m_selectedFxs.removeAt(index);
}

//---------------------------------------------------------

void FxSelection::select(SchematicLink *link) {
  if (link->isLineShaped()) return;
  Link boundingFxs = getBoundingFxs(link);
  if (boundingFxs == Link()) return;
  m_selectedLinks.append(boundingFxs);
}

//---------------------------------------------------------

void FxSelection::unselect(SchematicLink *link) {
  Link boundingFxs = getBoundingFxs(link);
  int index        = m_selectedLinks.indexOf(boundingFxs);
  if (index >= 0) m_selectedLinks.removeAt(index);
}

//---------------------------------------------------------

bool FxSelection::isSelected(TFxP fx) const {
  int i;
  for (i = 0; i < m_selectedFxs.size(); i++) {
    TFx *selectedFx      = m_selectedFxs[i].getPointer();
    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(selectedFx);
    if (zfx &&
        (fx.getPointer() == zfx || fx.getPointer() == zfx->getZeraryFx()))
      return true;
    if (fx.getPointer() == selectedFx) return true;
  }
  return false;
}

//---------------------------------------------------------

bool FxSelection::isSelected(int columnIndex) const {
  return m_selectedColIndexes.contains(columnIndex);
}

//---------------------------------------------------------

bool FxSelection::isSelected(SchematicLink *link) {
  Link boundingFxs = getBoundingFxs(link);
  return m_selectedLinks.contains(boundingFxs);
}

//---------------------------------------------------------

void FxSelection::deleteSelection() {
  emit doDelete();
  // std::list<TFxP, std::allocator<TFxP>> fxList = m_selectedFxs.toStdList();
  // TFxCommand::deleteSelection(fxList, m_selectedLinks.toStdList(),
  //                            m_selectedColIndexes.toStdList(), m_xshHandle,
  //                            m_fxHandle);
}

//---------------------------------------------------------

void FxSelection::copySelection() {
  QClipboard *clipboard = QApplication::clipboard();
  FxsData *fxsData      = new FxsData();
  fxsData->setFxs(m_selectedFxs, m_selectedLinks, m_selectedColIndexes,
                  m_xshHandle->getXsheet());
  clipboard->setMimeData(fxsData);
}

//---------------------------------------------------------

void FxSelection::cutSelection() {
  copySelection();
  deleteSelection();
}

//---------------------------------------------------------

void FxSelection::pasteSelection() {
  /*--- Fxノードを１つだけ選択していた場合、Replace paste ---*/
  if (m_selectedFxs.size() >= 1 && m_selectedLinks.size() == 0 &&
      m_selectedColIndexes.isEmpty())
    replacePasteSelection();
  /*--- Linkを１つだけ選択していた場合、Insert paste ---*/
  else if (m_selectedFxs.size() == 0 && m_selectedLinks.size() >= 1 &&
           m_selectedColIndexes.isEmpty())
    insertPasteSelection();
  else {
    QClipboard *clipboard = QApplication::clipboard();
    const FxsData *fxsData =
        dynamic_cast<const FxsData *>(clipboard->mimeData());
    if (!fxsData) return;
    QList<TFxP> fxs;
    QMap<TFx *, int> zeraryFxColumnSize;
    QList<TXshColumnP> columns;
    fxsData->getFxs(fxs, zeraryFxColumnSize, columns);
    if (fxs.isEmpty() && columns.isEmpty()) return;

    // in case of the paste command triggered from short cut key
    if (m_pastePosition == TConst::nowhere && m_schematicScene) {
      SchematicSceneViewer *ssv =
          dynamic_cast<SchematicSceneViewer *>(m_schematicScene->views().at(0));
      if (ssv)
        m_pastePosition =
            TPointD(ssv->getOldScenePos().x(), ssv->getOldScenePos().y());
    }

    if (!columns.isEmpty()) {
      // make sure that the levels contained in the pasted column nodes are
      // registered in the scene cast it may rename the level if there is
      // another level with the same name
      TUndoManager::manager()->beginBlock();
      emit columnPasted(columns);
    }

    TFxCommand::pasteFxs(std::list<TFxP>(fxs.begin(), fxs.end()),
                         zeraryFxColumnSize.toStdMap(),
                         std::list<TXshColumnP>(columns.begin(), columns.end()),
                         m_pastePosition, m_xshHandle, m_fxHandle);

    if (!columns.isEmpty()) {
      TUndoManager::manager()->endBlock();
    }

    if (m_schematicScene) {
      selectNone();
      for (int i = 0; i < (int)fxs.size(); i++) select(fxs[i]);
      m_schematicScene->selectNodes(fxs);
    }
  }
  m_pastePosition = TConst::nowhere;
}

//---------------------------------------------------------

bool FxSelection::insertPasteSelection() {
  QClipboard *clipboard  = QApplication::clipboard();
  const FxsData *fxsData = dynamic_cast<const FxsData *>(clipboard->mimeData());

  m_pastePosition = TConst::nowhere;

  if (!fxsData || !fxsData->isConnected()) return false;

  if (m_selectedLinks.isEmpty()) return true;

  // Start an undo block and ensure it is appropriately destroyed
  struct Auto {
    bool m_destruct;
    ~Auto() {
      if (m_destruct) TUndoManager::manager()->endBlock();
    }
  } auto_ = {false};

  // Need to make a temporary copy of selected links. It's necessary since the
  // selection will
  // be updated (cleared) after each insertion.
  QList<TFxCommand::Link> selectedLinks(m_selectedLinks);

  int i, size = selectedLinks.size();
  for (i = 0; i < size; ++i) {
    // Clone the fxs to be inserted
    QList<TFxP> fxs;
    QMap<TFx *, int> zeraryFxColumnSize;
    QList<TXshColumnP> columns;

    fxsData->getFxs(fxs, zeraryFxColumnSize, columns);
    if (fxs.empty() && columns.empty()) return true;

    // auto ends the undo block in the destructor.
    if (!auto_.m_destruct) {
      auto_.m_destruct = true;
      TUndoManager::manager()->beginBlock();
      // make sure that the levels contained in the pasted column nodes are
      // registered in the scene cast it may rename the level if there is
      // another level with the same name
      emit columnPasted(columns);
    }

    TFxCommand::insertPasteFxs(
        selectedLinks[i], std::list<TFxP>(fxs.begin(), fxs.end()),
        zeraryFxColumnSize.toStdMap(),
        std::list<TXshColumnP>(columns.begin(), columns.end()), m_xshHandle,
        m_fxHandle);
  }

  return true;
}

//---------------------------------------------------------

bool FxSelection::addPasteSelection() {
  QClipboard *clipboard  = QApplication::clipboard();
  const FxsData *fxsData = dynamic_cast<const FxsData *>(clipboard->mimeData());

  m_pastePosition = TConst::nowhere;

  if (!fxsData || !fxsData->isConnected()) return false;

  if (m_selectedFxs.isEmpty()) return true;

  struct Auto {
    bool m_destruct;
    ~Auto() {
      if (m_destruct) TUndoManager::manager()->endBlock();
    }
  } auto_ = {false};

  QList<TFxP> selectedFxs(m_selectedFxs);

  int i, size = selectedFxs.size();
  for (i = 0; i < size; ++i) {
    // Clone the fxs to be inserted
    QList<TFxP> fxs;
    QMap<TFx *, int> zeraryFxColumnSize;
    QList<TXshColumnP> columns;

    fxsData->getFxs(fxs, zeraryFxColumnSize, columns);
    if (fxs.empty() && columns.empty()) return true;

    // auto ends the undo block in its destructor
    if (!auto_.m_destruct)
      auto_.m_destruct = true, TUndoManager::manager()->beginBlock();

    TFx *inFx = selectedFxs[i].getPointer();
    TFxCommand::addPasteFxs(
        inFx, std::list<TFxP>(fxs.begin(), fxs.end()),
        zeraryFxColumnSize.toStdMap(),
        std::list<TXshColumnP>(columns.begin(), columns.end()), m_xshHandle,
        m_fxHandle);
  }

  return true;
}

//--------------------------------------------------------

bool FxSelection::replacePasteSelection() {
  QClipboard *clipboard  = QApplication::clipboard();
  const FxsData *fxsData = dynamic_cast<const FxsData *>(clipboard->mimeData());

  m_pastePosition = TConst::nowhere;

  if (!fxsData || !fxsData->isConnected()) return false;

  if (m_selectedFxs.isEmpty()) return true;

  struct Auto {
    bool m_destruct;
    ~Auto() {
      if (m_destruct) TUndoManager::manager()->endBlock();
    }
  } auto_ = {false};

  QList<TFxP> selectedFxs(m_selectedFxs);

  int i, size = selectedFxs.size();
  for (i = 0; i < size; ++i) {
    // Clone the fxs to be inserted
    QList<TFxP> fxs;
    QMap<TFx *, int> zeraryFxColumnSize;
    QList<TXshColumnP> columns;

    fxsData->getFxs(fxs, zeraryFxColumnSize, columns);
    if (fxs.empty() && columns.empty()) return true;

    // auto ends the undo block in its destructor
    if (!auto_.m_destruct) {
      auto_.m_destruct = true, TUndoManager::manager()->beginBlock();
      // make sure that the levels contained in the pasted column nodes are
      // registered in the scene cast it may rename the level if there is
      // another level with the same name
      emit columnPasted(columns);
    }

    TFx *inFx = m_selectedFxs[i].getPointer();
    TFxCommand::replacePasteFxs(
        inFx, std::list<TFxP>(fxs.begin(), fxs.end()),
        zeraryFxColumnSize.toStdMap(),
        std::list<TXshColumnP>(columns.begin(), columns.end()), m_xshHandle,
        m_fxHandle);
  }

  return true;
}

//---------------------------------------------------------

void FxSelection::groupSelection() {
  if (m_selectedFxs.size() <= 1) return;
  TFxCommand::groupFxs(
      std::list<TFxP>(m_selectedFxs.begin(), m_selectedFxs.end()), m_xshHandle);
  selectNone();
  m_xshHandle->notifyXsheetChanged();
}

//---------------------------------------------------------

void FxSelection::ungroupSelection() {
  if (isEmpty()) return;
  QSet<int> idSet;
  int i;
  for (i = 0; i < m_selectedFxs.size(); i++) {
    int groupId = m_selectedFxs[i]->getAttributes()->getGroupId();
    if (groupId > 0) idSet.insert(groupId);
  }

  TUndoManager::manager()->beginBlock();
  QSet<int>::iterator it;
  for (it = idSet.begin(); it != idSet.end(); it++) {
    TFxCommand::ungroupFxs(*it, m_xshHandle);
  }
  TUndoManager::manager()->endBlock();
  selectNone();
  m_xshHandle->notifyXsheetChanged();
}

//---------------------------------------------------------

void FxSelection::collapseSelection() {
  if (!m_selectedFxs.isEmpty()) emit doCollapse(m_selectedFxs);
}

//---------------------------------------------------------

void FxSelection::explodeChild() {
  if (!m_selectedFxs.isEmpty()) emit doExplodeChild(m_selectedFxs);
}

//---------------------------------------------------------

Link FxSelection::getBoundingFxs(SchematicLink *link) {
  Link boundingFxs;
  if (link) {
    SchematicPort *port = link->getStartPort();
    if (!port) return boundingFxs;
    if (port->getType() == 201 || port->getType() == 202 ||
        port->getType() == 203)
      boundingFxs = getBoundingFxs(port, link->getOtherPort(port));
    else if (port->getType() == 200 || port->getType() == 204)
      boundingFxs = getBoundingFxs(link->getOtherPort(port), port);
  }
  return boundingFxs;
}

//---------------------------------------------------------

Link FxSelection::getBoundingFxs(SchematicPort *inputPort,
                                 SchematicPort *outputPort) {
  Link boundingFxs;
  if (!inputPort || !outputPort) return boundingFxs;
  FxSchematicNode *inputNode =
      dynamic_cast<FxSchematicNode *>(outputPort->getNode());
  FxSchematicNode *outputNode =
      dynamic_cast<FxSchematicNode *>(inputPort->getNode());
  FxGroupNode *groupNode = dynamic_cast<FxGroupNode *>(inputNode);

  if (!inputNode || !outputNode ||
      (groupNode && groupNode->getOutputConnectionsCount() != 1))
    return boundingFxs;
  if (dynamic_cast<TXsheetFx *>(outputNode->getFx())) {
    if (!groupNode)
      boundingFxs.m_inputFx = inputNode->getFx();
    else {
      TFxSet *terminals =
          m_xshHandle->getXsheet()->getFxDag()->getTerminalFxs();
      QList<TFxP> roots = groupNode->getRootFxs();
      int i;
      for (i = 0; i < roots.size(); i++)
        if (terminals->containsFx(roots[i].getPointer())) {
          boundingFxs.m_inputFx = roots[i];
          break;
        }
    }
    boundingFxs.m_outputFx = outputNode->getFx();
    return boundingFxs;
  }

  if (outputNode->isA(eGroupedFx)) {
    // devo prima trovare l'effetto interno al gruppo al quale inputNode e'
    // linkato.
    FxGroupNode *groupNode = dynamic_cast<FxGroupNode *>(outputNode);
    assert(groupNode);
    QList<TFx *> fxs;
    TFx *inputFx = inputNode->getFx();
    int i;
    for (i = 0; i < inputFx->getOutputConnectionCount(); i++) {
      TFx *outputFx = inputFx->getOutputConnection(i)->getOwnerFx();
      if (!outputFx) continue;
      if (groupNode->contains(outputFx)) fxs.push_back(outputFx);
    }
    if (fxs.size() != 1)  // un nodo esterno al gruppo puo' essere linkato a
                          // piu' nodi interni al gruppo
      return boundingFxs;

    TFx *outputFx = fxs[0];
    // ho tovato l'effetto, ora devo trovare l'indice della porta a cui e'
    // linkato l'effetto in input
    for (i = 0; i < outputFx->getInputPortCount(); i++) {
      TFxPort *inputPort = outputFx->getInputPort(i);
      TFx *fx            = inputPort->getFx();
      if (fx == inputFx) break;
    }
    if (i >= outputFx->getInputPortCount()) return boundingFxs;
    boundingFxs.m_inputFx  = inputFx;
    boundingFxs.m_outputFx = outputFx;
    boundingFxs.m_index    = i;
    return boundingFxs;
  } else {
    bool found = false;
    int i, index = -1;
    for (i = 0; i < outputNode->getInputPortCount() && !found; i++) {
      FxSchematicPort *inputAppPort = outputNode->getInputPort(i);
      int j;
      for (j = 0; j < inputAppPort->getLinkCount(); j++) {
        FxSchematicNode *outputAppNode =
            dynamic_cast<FxSchematicNode *>(inputAppPort->getLinkedNode(j));
        if (!outputAppNode) continue;
        FxSchematicPort *outputAppPort = outputAppNode->getOutputPort();
        if (inputAppPort == inputPort && outputPort == outputAppPort) {
          found = true;
          index = i;
          break;
        }
      }
    }
    if (index == -1) return boundingFxs;
    TFx *inputFx           = inputNode->getFx();
    TFx *outputFx          = outputNode->getFx();
    boundingFxs.m_inputFx  = inputFx;
    boundingFxs.m_outputFx = outputFx;
    boundingFxs.m_index    = index;
    return boundingFxs;
  }
}

//---------------------------------------------------------

bool FxSelection::isConnected() {
  if (m_selectedFxs.isEmpty()) return false;
  QList<TFx *> visitedFxs;
  visitFx(m_selectedFxs.at(0).getPointer(), visitedFxs);
  bool connected = true;
  QList<TFxP>::const_iterator it;
  TXsheet *xsh        = m_xshHandle->getXsheet();
  TFxSet *internalFxs = xsh->getFxDag()->getInternalFxs();
  for (it = m_selectedFxs.begin(); it != m_selectedFxs.end(); it++) {
    TFx *selectedFx = it->getPointer();
    TColumnFx *cfx  = dynamic_cast<TColumnFx *>(selectedFx);
    if (!cfx && !internalFxs->containsFx(selectedFx)) return false;
    TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(selectedFx);
    if (zfx) selectedFx = zfx->getZeraryFx();
    connected = connected && visitedFxs.contains(selectedFx);
  }
  return connected;
}

//-------------------------------------------------------

void FxSelection::visitFx(TFx *fx, QList<TFx *> &visitedFxs) {
  if (visitedFxs.contains(fx)) return;
  TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx);
  if (zfx) fx = zfx->getZeraryFx();
  if (!canGroup(fx)) return;
  visitedFxs.append(fx);
  int i;
  for (i = 0; i < fx->getInputPortCount(); i++) {
    TFx *inputFx              = fx->getInputPort(i)->getFx();
    TZeraryColumnFx *onputZFx = dynamic_cast<TZeraryColumnFx *>(inputFx);
    if (onputZFx) inputFx = onputZFx->getZeraryFx();
    if (!inputFx) continue;
    bool canBeGrouped = !inputFx->getAttributes()->isGrouped() ||
                        (inputFx->getAttributes()->getEditingGroupId() ==
                         fx->getAttributes()->getEditingGroupId());
    if (!visitedFxs.contains(inputFx) && isSelected(inputFx) && canBeGrouped)
      visitFx(inputFx, visitedFxs);
  }
  if (zfx) fx = zfx;
  if (fx->isZerary() && !zfx) {
    TXsheet *xsh    = m_xshHandle->getXsheet();
    int columnCount = xsh->getColumnCount();
    int j;
    for (j = 0; j < columnCount; j++) {
      TZeraryColumnFx *zerary =
          dynamic_cast<TZeraryColumnFx *>(xsh->getColumn(j)->getFx());
      if (zerary && zerary->getZeraryFx() == fx) {
        fx = zerary;
        break;
      }
    }
  }
  for (i = 0; i < fx->getOutputConnectionCount(); i++) {
    TFx *outputFx = fx->getOutputConnection(i)->getOwnerFx();
    if (!outputFx) continue;
    bool canBeGrouped = !outputFx->getAttributes()->isGrouped() ||
                        (outputFx->getAttributes()->getEditingGroupId() ==
                         fx->getAttributes()->getEditingGroupId());
    if (!visitedFxs.contains(outputFx) && isSelected(outputFx) && canBeGrouped)
      visitFx(outputFx, visitedFxs);
  }
}

//-------------------------------------------------------

bool FxSelection::areLinked(TFx *outFx, TFx *inFx) {
  int i;
  for (i = 0; i < outFx->getInputPortCount(); i++) {
    TFx *inputFx = outFx->getInputPort(i)->getFx();
    if (inFx == inputFx) return true;
  }
  return false;
}
