

#include "columncommand.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "columnselection.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/stageobjectsdata.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobject.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/childstack.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/fxcommand.h"

// TnzBase includes
#include "tfx.h"
#include "tfxattributes.h"

// TnzCore includes
#include "tstroke.h"
#include "tundo.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

// tcg includes
#include "tcg/tcg_function_types.h"

#include <memory>

//*************************************************************************
//    Local Namespace  stuff
//*************************************************************************

namespace {

bool filterNegatives(int c) { return (c < 0); }
typedef tcg::function<bool (*)(int), filterNegatives> filterNegatives_fun;

//-----------------------------------------------------------------------------

bool canRemoveFx(const std::set<TFx *> &leaves, TFx *fx) {
  bool removeFx = false;
  for (int i = 0; i < fx->getInputPortCount(); i++) {
    TFx *inputFx = fx->getInputPort(i)->getFx();
    if (!inputFx) continue;
    if (leaves.count(inputFx) > 0) {
      removeFx = true;
      continue;
    }
    if (!canRemoveFx(leaves, inputFx)) return false;
    removeFx = true;
  }
  return removeFx;
}

//-----------------------------------------------------------------------------

void getColumnLinkedFxs(TFx *startFx, TFx *newStartFx,
                        QMap<TFx *, TFx *> &fxs) {
  int i, outputNodeCount = 0;
  for (i = 0; i < startFx->getOutputConnectionCount(); i++) {
    TFx *outputFx = startFx->getOutputConnection(i)->getOwnerFx();
    if (fxs.contains(outputFx)) continue;
    if (dynamic_cast<TOutputFx *>(outputFx)) {
      outputNodeCount++;
      continue;
    }
    TFxPort *port = newStartFx->getOutputConnection(i - outputNodeCount);
    if (!port) continue;
    TFx *newOutputFx = port->getOwnerFx();
    fxs[outputFx]    = newOutputFx;
    getColumnLinkedFxs(outputFx, newOutputFx, fxs);
  }
}

//-----------------------------------------------------------------------------

void cloneNotColumnLinkedFxsAndOutputsFx(TXsheet *xsh, TXsheet *newXsh) {
  int columnCount = xsh->getColumnCount();
  assert(newXsh->getColumnCount() == columnCount);

  // Riempio un mapping (fx del vecchio xsheet -> fx del nuovo xsheet)
  QMap<TFx *, TFx *> clonedFxs;
  int i;
  for (i = 0; i < columnCount; i++) {
    TXshColumn *xshColumn = xsh->getColumn(i);

    TFx *columnFx    = xshColumn->getFx();
    TFx *newColumnFx = newXsh->getColumn(i)->getFx();

    if (columnFx && newColumnFx)
      getColumnLinkedFxs(columnFx, newColumnFx, clonedFxs);
  }

  FxDag *fxDag    = xsh->getFxDag();
  FxDag *newFxDag = newXsh->getFxDag();

  // aggiungo nel mapping tutti gli effetti che non sono connessi da un cammino
  // con una colonna
  std::vector<TFx *> fxs, newFxs;
  fxDag->getFxs(fxs);
  newFxDag->getFxs(newFxs);
  QList<TFx *> notColumnLinkedClonedFxs;
  if (fxs.size() > newFxs.size()) {
    for (i = 0; i < fxs.size(); i++) {
      TFx *fx = fxs[i];
      if (clonedFxs.contains(fx)) continue;
      TFx *newFx = fx->clone(false);
      newFxDag->getInternalFxs()->addFx(newFx);
      if (fxDag->getTerminalFxs()->containsFx(fx))
        newFxDag->getTerminalFxs()->addFx(newFx);
      newFxDag->assignUniqueId(newFx);
      clonedFxs[fx] = newFx;
      notColumnLinkedClonedFxs.append(newFx);
    }
  }

  // Aggiungo tutti gli outputFx mancanti
  for (i = 0; i < fxDag->getOutputFxCount(); i++) {
    if (i >= newFxDag->getOutputFxCount()) newFxDag->addOutputFx();
    newFxDag->getOutputFx(i)->getAttributes()->setDagNodePos(
        fxDag->getOutputFx(i)->getAttributes()->getDagNodePos());
  }

  // connetto tutti i nuovi effetti aggiunti
  for (i = 0; i < notColumnLinkedClonedFxs.size(); i++) {
    TFx *newFx = notColumnLinkedClonedFxs[i];
    TFx *fx    = clonedFxs.key(newFx);
    assert(fx);
    int j;
    for (j = 0; j < fx->getInputPortCount(); j++) {
      TFx *inputFx = fx->getInputPort(j)->getFx();
      if (!inputFx) continue;
      if (dynamic_cast<TXsheetFx *>(inputFx))
        newFx->getInputPort(j)->setFx(newFxDag->getXsheetFx());
      else {
        TFx *newInputFx = clonedFxs[inputFx];
        if (!newInputFx) continue;
        newFx->getInputPort(j)->setFx(newInputFx);
      }
    }
    for (j = 0; j < fx->getOutputConnectionCount(); j++) {
      TFxPort *outputPort = fx->getOutputConnection(j);
      TFx *outputFx       = outputPort->getOwnerFx();
      int k, index = 0;
      if (TOutputFx *outFx = dynamic_cast<TOutputFx *>(outputFx)) continue;

      index = 0;
      for (k = 0; k < outputFx->getInputPortCount(); k++) {
        if (outputFx->getInputPort(k) == outputPort) index = k;
      }
      TFx *newOutputFx = clonedFxs[outputFx];
      newOutputFx->getInputPort(index)->setFx(newFx);
    }
  }
  // Connetto tutti gli output
  for (i = 0; i < fxDag->getOutputFxCount(); i++) {
    TOutputFx *outputFx    = fxDag->getOutputFx(i);
    TOutputFx *newOutputFx = newFxDag->getOutputFx(i);
    TFx *inputFx           = outputFx->getInputPort(0)->getFx();
    if (!inputFx) continue;
    if (dynamic_cast<TXsheetFx *>(inputFx))
      newOutputFx->getInputPort(0)->setFx(newFxDag->getXsheetFx());
    else if (TLevelColumnFx *levelFx =
                 dynamic_cast<TLevelColumnFx *>(inputFx)) {
      int index       = levelFx->getColumnIndex();
      TFx *newInputFx = newXsh->getColumn(index)->getFx();
      newOutputFx->getInputPort(0)->setFx(newInputFx);
    } else if (TZeraryColumnFx *zeraryFx =
                   dynamic_cast<TZeraryColumnFx *>(inputFx)) {
      int index       = zeraryFx->getColumnIndex();
      TFx *newInputFx = newXsh->getColumn(index)->getFx();
      newOutputFx->getInputPort(0)->setFx(newInputFx);
    } else {
      TFx *newInputFx = clonedFxs[inputFx];
      newOutputFx->getInputPort(0)->setFx(newInputFx);
    }
  }
}

//-----------------------------------------------------------------------------

void cloneXsheetTStageObjectTree(TXsheet *xsh, TXsheet *newXsh) {
  std::set<TStageObjectId> pegbarIds;
  TStageObjectTree *tree    = xsh->getStageObjectTree();
  TStageObjectTree *newTree = newXsh->getStageObjectTree();
  // Ricostruisco l'intero albero
  int i;
  for (i = 0; i < tree->getStageObjectCount(); i++) {
    TStageObject *stageObject    = tree->getStageObject(i);
    TStageObjectId id            = stageObject->getId();
    TStageObject *newStageObject = newXsh->getStageObject(id);
    if (id.isCamera()) {
      TCamera *camera              = stageObject->getCamera();
      *newStageObject->getCamera() = *camera;
    }
    // Gestisco le spline delle colonne in modo differente perche' sono state
    // gia' settate.
    TStageObjectSpline *spline = newStageObject->getSpline();
    TStageObjectParams *data   = stageObject->getParams();
    newStageObject->assignParams(data);
    delete data;
    newStageObject->setParent(xsh->getStageObjectParent(id));
    if (id.isColumn() && spline) newStageObject->setSpline(spline);

    // Gestisco le spline che non sono di colonna (spline di camera)
    TStageObjectSpline *oldSpline = stageObject->getSpline();
    if (oldSpline && !id.isColumn()) {
      TStageObjectSpline *newSpline = newTree->createSpline();
      newSpline->addRef();
      newSpline->setStroke(new TStroke(*oldSpline->getStroke()));
      newStageObject->setSpline(newSpline);
    }
  }
}

//-----------------------------------------------------------------------------

bool pasteColumnsWithoutUndo(std::set<int> *indices, bool doClone,
                             const StageObjectsData *data) {
  if (!data)
    data = dynamic_cast<const StageObjectsData *>(
        QApplication::clipboard()->mimeData());

  if (!data) return false;

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  // Check Circular References
  if (data->checkCircularReferences(xsh)) {
    DVGui::error(
        QObject::tr("It is not possible to paste the columns: there is a "
                    "circular reference."));
    return false;
  }

  std::list<int> restoredSplineIds;
  data->restoreObjects(*indices, restoredSplineIds, xsh,
                       doClone ? StageObjectsData::eDoClone : 0,
                       TConst::nowhere);
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentObject()->notifyObjectIdSwitched();
  return true;
}

//-----------------------------------------------------------------------------

void deleteColumnsWithoutUndo(std::set<int> *indices,
                              bool onlyColumns = false) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  // non posso buttare la colonna di camera
  indices->erase(-1);

  // cerco le fx da eliminare. prima trovo le foglie
  /*-- leavesに消されるカラムのColumnFxを格納していく --*/
  std::set<TFx *> leaves;
  std::set<int>::iterator it2;
  for (it2 = indices->begin(); it2 != indices->end(); ++it2) {
    int index = *it2;
    if (index < 0) continue;
    TXshColumn *column = xsh->getColumn(index);
    if (!column) continue;
    TFx *fx = column->getFx();
    if (fx) leaves.insert(fx);
  }

  // e poi ...
  /*-- カラムを消した時、一緒に消してもよいFxを格納していく --*/
  std::vector<TFx *> fxsToKill;
  TFxSet *fxSet = xsh->getFxDag()->getInternalFxs();
  int i;
  for (i = 0; i < fxSet->getFxCount(); i++) {
    TFx *fx = fxSet->getFx(i);
    if (canRemoveFx(leaves, fx)) fxsToKill.push_back(fx);
  }

  bool soundColumnRemoved = false;
  int firstIndex          = *indices->begin();
  std::set<int>::reverse_iterator it;
  for (it = indices->rbegin(); it != indices->rend(); ++it) {
    TXshColumn *column = xsh->getColumn(*it);
    if (column && column->getSoundColumn()) soundColumnRemoved = true;
    if (column && column->getFx()) app->getCurrentFx()->setFx(0);
    xsh->removeColumn(*it);
  }

  for (i = 0; i < (int)fxsToKill.size() && !onlyColumns; i++) {
    TFx *fx = fxsToKill[i];

    if (fx == app->getCurrentFx()->getFx()) app->getCurrentFx()->setFx(0);
    if (fx->getLinkedFx() != fx) fx->unlinkParams();

    int j, outputPortCount = fx->getOutputConnectionCount();
    for (j = outputPortCount - 1; j >= 0; j--) {
      TFxPort *port = fx->getOutputConnection(j);

      TFx *outFx = port->getOwnerFx();
      if (TZeraryFx *zeraryFx = dynamic_cast<TZeraryFx *>(outFx))
        outFx = zeraryFx->getColumnFx();

      std::vector<TFx *>::iterator it =
          std::find(fxsToKill.begin(), fxsToKill.end(), outFx);
      std::set<TFx *>::iterator it2 =
          std::find(leaves.begin(), leaves.end(), outFx);

      if (it == fxsToKill.end() && it2 == leaves.end()) port->setFx(0);
    }

    fxSet->removeFx(fx);
    xsh->getFxDag()->getTerminalFxs()->removeFx(fx);
  }

  xsh->updateFrameCount();

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentObject()->notifyObjectIdSwitched();
  if (soundColumnRemoved) {
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentXsheet()->notifyXsheetSoundChanged();
  }
  indices->clear();
}

//-----------------------------------------------------------------------------

void resetColumns(
    const QMimeData *mimeData, std::set<int> *indices,
    const QMap<TFxPort *, TFx *> &columnFxLinks,
    const QMap<TStageObjectId, TStageObjectId> &columnObjParents,
    const QMap<TStageObjectId, QList<TStageObjectId>> &columnObjChildren) {
  const StageObjectsData *data =
      dynamic_cast<const StageObjectsData *>(mimeData);
  if (!data) return;
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  std::list<int> restoredSplineIds;
  data->restoreObjects(*indices, restoredSplineIds, xsh, 0);
  QMap<TFxPort *, TFx *>::const_iterator it;
  for (it = columnFxLinks.begin(); it != columnFxLinks.end(); it++)
    it.key()->setFx(it.value());

  // Devo rimettere le stesse connessioni tra gli stage object
  QMap<TStageObjectId, TStageObjectId>::const_iterator it2;
  for (it2 = columnObjParents.begin(); it2 != columnObjParents.end();
       it2++) {  // Parents
    TStageObject *obj = xsh->getStageObject(it2.key());
    if (obj) {
      obj->setParent(it2.value());
    }
  }

  QMap<TStageObjectId, QList<TStageObjectId>>::const_iterator it3;
  for (it3 = columnObjChildren.begin(); it3 != columnObjChildren.end();
       it3++) {  // Children
    QList<TStageObjectId> children = it3.value();
    int i;
    for (i = 0; i < children.size(); i++) {
      TStageObject *child = xsh->getStageObject(children[i]);
      if (child) {
        child->setParent(it3.key());
      }
    }
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentObject()->notifyObjectIdSwitched();
}

//-----------------------------------------------------------------------------

// Clones the TXshChildLevel, but NOT any further nested TXshChildLevel inside
// it.
TXshChildLevel *cloneChildLevel(TXshChildLevel *cl) {
  TXshChildLevel *newLevel = new TXshChildLevel(cl->getName());
  newLevel->setScene(cl->getScene());

  TXsheet *childXsh = cl->getXsheet(), *newChildXsh = newLevel->getXsheet();

  std::set<int> indices;
  for (int i = 0; i < childXsh->getColumnCount(); i++) indices.insert(i);

  StageObjectsData *data = new StageObjectsData();
  data->storeColumns(indices, childXsh, 0);
  data->storeColumnFxs(indices, childXsh, 0);
  std::list<int> restoredSplineIds;
  data->restoreObjects(indices, restoredSplineIds, newChildXsh,
                       StageObjectsData::eDoClone);
  delete data;

  cloneNotColumnLinkedFxsAndOutputsFx(childXsh, newChildXsh);
  cloneXsheetTStageObjectTree(childXsh, newChildXsh);

  return newLevel;
}

//-----------------------------------------------------------------------------

void cloneSubXsheets(TXsheet *xsh) {
  std::map<TXsheet *, TXshChildLevel *> visited;
  std::set<TXsheet *> toVisit;

  toVisit.insert(xsh);

  while (!toVisit.empty()) {
    xsh = *toVisit.begin();
    toVisit.erase(xsh);

    for (int i = 0; i < xsh->getColumnCount(); ++i) {
      TXshColumn *column = xsh->getColumn(i);
      if (!column) continue;

      if (TXshCellColumn *cc = column->getCellColumn()) {
        int r0 = 0, r1 = -1;
        cc->getRange(r0, r1);
        if (!cc->isEmpty() && r0 <= r1)
          for (int r = r0; r <= r1; ++r) {
            TXshCell cell = cc->getCell(r);
            if (cell.m_level && cell.m_level->getChildLevel()) {
              TXsheet *subxsh = cell.m_level->getChildLevel()->getXsheet();

              std::map<TXsheet *, TXshChildLevel *>::iterator it =
                  visited.find(subxsh);
              if (it == visited.end()) {
                it = visited
                         .insert(std::make_pair(
                             subxsh,
                             cloneChildLevel(cell.m_level->getChildLevel())))
                         .first;
                toVisit.insert(subxsh);
              }
              assert(it != visited.end());

              cell.m_level = it->second;
              cc->setCell(r, cell);
            }
          }
      }
    }
  }
}

//=============================================================================
//  PasteColumnsUndo
//-----------------------------------------------------------------------------

class PasteColumnsUndo final : public TUndo {
  std::set<int> m_indices;
  StageObjectsData *m_data;
  QMap<TFxPort *, TFx *> m_columnLinks;

public:
  PasteColumnsUndo(std::set<int> indices) : m_indices(indices) {
    TApp *app    = TApp::instance();
    m_data       = new StageObjectsData();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    m_data->storeColumns(indices, xsh, 0);
    m_data->storeColumnFxs(indices, xsh, 0);
    std::set<int>::iterator it;
    for (it = m_indices.begin(); it != m_indices.end(); it++) {
      TXshColumn *column = xsh->getColumn(*it);
      if (!column || !column->getFx()) continue;
      TFx *fx = column->getFx();
      int i;
      for (i = 0; i < fx->getOutputConnectionCount(); i++)
        m_columnLinks[fx->getOutputConnection(i)] = fx;
    }
  }

  ~PasteColumnsUndo() { delete m_data; }

  void undo() const override {
    std::set<int> indices;
    std::set<int>::const_iterator indicesIt = m_indices.begin();
    while (indicesIt != m_indices.end()) indices.insert(*indicesIt++);
    deleteColumnsWithoutUndo(&indices);
  }

  void redo() const override {
    std::set<int> indices;
    std::set<int>::const_iterator indicesIt = m_indices.begin();
    while (indicesIt != m_indices.end()) indices.insert(*indicesIt++);
    resetColumns(m_data, &indices, m_columnLinks,
                 QMap<TStageObjectId, TStageObjectId>(),
                 QMap<TStageObjectId, QList<TStageObjectId>>());
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Paste Column :  ");

    std::set<int>::iterator it;
    for (it = m_indices.begin(); it != m_indices.end(); it++) {
      if (it != m_indices.begin()) str += QString(", ");
      str += QString("Col%1").arg(QString::number((*it) + 1));
    }
    return str;
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
//  DeleteColumnsUndo
//-----------------------------------------------------------------------------

class DeleteColumnsUndo final : public TUndo {
  std::set<int> m_indices;

  QMap<TFxPort *, TFx *> m_columnFxLinks;
  QMap<TStageObjectId, QList<TStageObjectId>> m_columnObjChildren;
  QMap<TStageObjectId, TStageObjectId> m_columnObjParents;

  mutable std::unique_ptr<StageObjectsData> m_data;

public:
  DeleteColumnsUndo(const std::set<int> &indices)
      : m_indices(indices), m_data(new StageObjectsData) {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

    m_data->storeColumns(m_indices, xsh, 0);
    m_data->storeColumnFxs(m_indices, xsh, 0);

    std::set<int>::iterator it;
    for (it = m_indices.begin(); it != m_indices.end(); ++it) {
      TXshColumn *column = xsh->getColumn(*it);
      if (!column) continue;

      // Store output links
      if (TFx *fx = column->getFx()) {
        for (int i = 0; i < fx->getOutputConnectionCount(); ++i) {
          // Only links whose output-connected fx is in the xsheet (ie not
          // temporary)
          // are stored. Temporaries may be erased without an undo
          // notification...

          TFx *outFx = fx->getOutputConnection(i)->getOwnerFx();
          if (xsh->getFxDag()->getInternalFxs()->containsFx(outFx))
            m_columnFxLinks[fx->getOutputConnection(i)] = fx;
        }
      }

      // Store TStageObject children
      int pegbarsCount     = xsh->getStageObjectTree()->getStageObjectCount();
      TStageObjectId id    = TStageObjectId::ColumnId(*it);
      TStageObject *pegbar = xsh->getStageObject(id);
      for (int i = 0; i < pegbarsCount; ++i) {
        TStageObject *other = xsh->getStageObjectTree()->getStageObject(i);
        if (other == pegbar) continue;

        if (other->getParent() == id) {
          // other->setParent(pegbar->getParent());
          m_columnObjChildren[id].append(other->getId());
        }
      }

      // Mi salvo il parent
      m_columnObjParents[id] = pegbar->getParent();
    }
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    assert(!m_data.get());
    m_data.reset(new StageObjectsData);

    m_data->storeColumns(m_indices, xsh, 0);
    m_data->storeColumnFxs(m_indices, xsh, 0);

    std::set<int> indices = m_indices;
    deleteColumnsWithoutUndo(&indices);
  }

  void undo() const override {
    std::set<int> indices = m_indices;
    resetColumns(m_data.get(), &indices, m_columnFxLinks, m_columnObjParents,
                 m_columnObjChildren);

    m_data.reset();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Delete Column :  ");

    std::set<int>::iterator it;
    for (it = m_indices.begin(); it != m_indices.end(); it++) {
      if (it != m_indices.begin()) str += QString(", ");
      str += QString("Col%1").arg(QString::number((*it) + 1));
    }
    return str;
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

//*************************************************************************
//    Column Command  Undo
//*************************************************************************

class ColumnCommandUndo : public TUndo {
public:
  virtual ~ColumnCommandUndo() {}
  virtual bool isConsistent() const = 0;

protected:
};

//*************************************************************************
//    Insert Empty Columns  Command
//*************************************************************************

class InsertEmptyColumnsUndo final : public ColumnCommandUndo {
  std::vector<std::pair<int, int>> m_columnBlocks;

public:
  InsertEmptyColumnsUndo(const std::vector<int> &indices, bool insertAfter) {
    initialize(indices, insertAfter);
  }

  bool isConsistent() const override { return true; }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Insert Column :  ");

    std::vector<std::pair<int, int>>::iterator it;
    for (it = m_columnBlocks.begin(); it != m_columnBlocks.end(); it++) {
      if (it != m_columnBlocks.begin()) str += QString(", ");
      str += QString("Col%1").arg(QString::number((*it).first + 1));
    }
    return str;
  }

  int getHistoryType() override { return HistoryType::Xsheet; }

private:
  void initialize(const std::vector<int> &indices, bool insertAfter = false);
};

//------------------------------------------------------

void InsertEmptyColumnsUndo::initialize(const std::vector<int> &indices,
                                        bool insertAfter) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  std::vector<int>::const_iterator cb, ce, cEnd = indices.end();

  for (cb = indices.begin(); cb != cEnd; cb = ce)  // As long as block end is ok
  {
    int c = *cb;  // Find a corresponding block start
    for (ce = cb, ++ce, ++c;
         (ce != cEnd) && (*ce == c);)  // by iterating as long as the next
      ++ce, ++c;                       // column index is the previous one + 1

    int insertAt = (insertAfter ? c : *cb);
    m_columnBlocks.push_back(std::make_pair(insertAt, c - *cb));
  }

  assert(!m_columnBlocks.empty());
}

//------------------------------------------------------

void InsertEmptyColumnsUndo::redo() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  // If this is the very first column, add one now since there is always
  // 1 visible on the screen but its not actually there yet.
  if (!xsh->getColumnCount()) xsh->insertColumn(0);

  std::vector<std::pair<int, int>>::const_reverse_iterator bt,
      bEnd = m_columnBlocks.rend();
  for (bt = m_columnBlocks.rbegin(); bt != bEnd; ++bt)
    for (int n = 0; n != bt->second; ++n) xsh->insertColumn(bt->first);

  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentObject()->notifyObjectIdSwitched();
}

//------------------------------------------------------

void InsertEmptyColumnsUndo::undo() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  std::vector<std::pair<int, int>>::const_iterator bt,
      bEnd = m_columnBlocks.end();
  for (bt = m_columnBlocks.begin(); bt != bEnd; ++bt)
    for (int n = 0; n != bt->second; ++n) xsh->removeColumn(bt->first);

  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentObject()->notifyObjectIdSwitched();
}

//======================================================

void ColumnCmd::insertEmptyColumns(const std::set<int> &indices,
                                   bool insertAfter) {
  // Filter out all less than 0 indices (in particular, the 'camera' column
  // in the Toonz derivative product "Tab")
  std::vector<int> positiveIndices(indices.lower_bound(0), indices.end());
  if (positiveIndices.empty()) return;

  std::unique_ptr<ColumnCommandUndo> undo(
      new InsertEmptyColumnsUndo(positiveIndices, insertAfter));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//======================================================

void ColumnCmd::insertEmptyColumn(int index) {
  std::set<int> ii;
  ii.insert(index);
  ColumnCmd::insertEmptyColumns(ii);
}

//*************************************************************************
//    Copy Columns  Command
//*************************************************************************

static void copyColumns_internal(const std::set<int> &indices) {
  assert(!indices.empty());

  StageObjectsData *data = new StageObjectsData;

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  data->storeColumns(indices, xsh, StageObjectsData::eDoClone |
                                       StageObjectsData::eResetFxDagPositions);
  data->storeColumnFxs(
      indices, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);

  QApplication::clipboard()->setMimeData(data);
}

//------------------------------------------------------

void ColumnCmd::copyColumns(const std::set<int> &indices) {
  if (indices.empty()) return;

  copyColumns_internal(indices);
}

//*************************************************************************
//    Paste Columns  Command
//*************************************************************************

void ColumnCmd::pasteColumns(std::set<int> &indices,
                             const StageObjectsData *data) {
  // indices will be updated here by inserted column ids after pasting
  bool isPaste = pasteColumnsWithoutUndo(&indices, true, data);
  if (!isPaste) return;
  TUndoManager::manager()->add(new PasteColumnsUndo(indices));
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//*************************************************************************
//    Delete Columns  Command
//*************************************************************************

void ColumnCmd::deleteColumns(std::set<int> &indices, bool onlyColumns,
                              bool withoutUndo) {
  if (indices.empty()) return;

  if (!withoutUndo && !onlyColumns)
    TUndoManager::manager()->add(new DeleteColumnsUndo(indices));

  deleteColumnsWithoutUndo(&indices, onlyColumns);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================
// deleteColumn
//=============================================================================

void ColumnCmd::deleteColumn(int index) {
  std::set<int> ii;
  ii.insert(index);
  ColumnCmd::deleteColumns(ii, false, false);
}

//=============================================================================
// cutColumns
//-----------------------------------------------------------------------------

static void cutColumnsWithoutUndo(std::set<int> *indices) {
  copyColumns_internal(*indices);
  deleteColumnsWithoutUndo(indices);
}

void ColumnCmd::cutColumns(std::set<int> &indices) {
  if (indices.empty()) return;

  TUndoManager::manager()->add(new DeleteColumnsUndo(indices));

  cutColumnsWithoutUndo(&indices);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================
// Undo Resequence
//=============================================================================

class ResequenceUndo final : public TUndo {
  int m_index;
  int m_r0;
  std::vector<TFrameId> m_oldFrames;
  int m_newFramesCount;

public:
  ResequenceUndo(int col, int count)
      : m_index(col), m_r0(0), m_newFramesCount(count) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int r0, r1;
    xsh->getCellRange(col, r0, r1);
    m_r0 = r0;
    assert(r0 <= r1);
    if (r0 > r1) return;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      assert(cell.isEmpty() || cell.m_level->getChildLevel());
      m_oldFrames.push_back(cell.m_frameId);
    }
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int r0, r1;
    xsh->getCellRange(m_index, r0, r1);
    assert(r0 <= r1);
    if (r0 > r1) return;
    TXshCell cell = xsh->getCell(r0, m_index);
    assert(!cell.isEmpty());
    assert(cell.m_level->getChildLevel());
    xsh->clearCells(r0, m_index, r1 - r0 + 1);
    for (int i = 0; i < (int)m_oldFrames.size(); i++) {
      TFrameId fid = m_oldFrames[i];
      if (fid != TFrameId::EMPTY_FRAME) {
        cell.m_frameId = fid;
        xsh->setCell(m_r0 + i, m_index, cell);
      }
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int r0, r1;
    xsh->getCellRange(m_index, r0, r1);
    assert(r0 <= r1);
    if (r0 > r1) return;
    TXshCell cell = xsh->getCell(r0, m_index);
    assert(!cell.isEmpty());
    assert(cell.m_level->getChildLevel());
    xsh->clearCells(r0, m_index, r1 - r0 + 1);
    for (int i = 0; i < m_newFramesCount; i++) {
      cell.m_frameId = TFrameId(i + 1);
      xsh->setCell(m_r0 + i, m_index, cell);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    return sizeof(*this) + m_oldFrames.size() * sizeof(TFrameId);
  }

  QString getHistoryString() override {
    return QObject::tr("Resequence :  Col%1").arg(QString::number(m_index + 1));
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// Resequence
//=============================================================================

bool ColumnCmd::canResequence(int index) {
  TXsheet *xsh       = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(index);
  if (!column) return false;
  TXshLevelColumn *lcolumn = column->getLevelColumn();
  if (!lcolumn) return false;
  int r0 = 0, r1 = -1;
  if (lcolumn->getRange(r0, r1) == 0) return false;
  assert(r0 <= r1);
  TXshCell cell = lcolumn->getCell(r0);
  assert(!cell.isEmpty());
  TXshLevel *xl = cell.m_level->getChildLevel();
  if (!xl) return false;
  for (int r = r0 + 1; r <= r1; r++) {
    cell = lcolumn->getCell(r);
    if (cell.isEmpty()) continue;
    if (cell.m_level.getPointer() != xl) return false;
  }
  return true;
}

//-----------------------------------------------------------------------------

void ColumnCmd::resequence(int index) {
  if (!canResequence(index)) return;
  TXsheet *xsh       = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(index);
  assert(column);
  TXshLevelColumn *lcolumn = column->getLevelColumn();
  assert(lcolumn);
  int r0 = 0, r1 = -1;
  lcolumn->getRange(r0, r1);
  assert(r0 <= r1);
  TXshCell cell = lcolumn->getCell(r0);
  assert(!cell.isEmpty());
  TXshChildLevel *xl = cell.m_level->getChildLevel();
  assert(xl);
  TXsheet *childXsh              = xl->getXsheet();
  int frameCount                 = childXsh->getFrameCount();
  if (frameCount < 1) frameCount = 1;

  TUndoManager::manager()->add(new ResequenceUndo(index, frameCount));

  lcolumn->clearCells(r0, r1 - r0 + 1);
  for (int i = 0; i < frameCount; i++) {
    cell.m_frameId = TFrameId(i + 1);
    lcolumn->setCell(r0 + i, cell);
  }

  xsh->updateFrameCount();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================================
// Undo cloneChild
//=============================================================================

class CloneChildUndo final : public TUndo {
  TXshChildLevelP m_childLevel;
  int m_columnIndex;

public:
  CloneChildUndo(TXshChildLevel *childLevel, int columnIndex)
      : m_childLevel(childLevel), m_columnIndex(columnIndex) {}

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    xsh->removeColumn(m_columnIndex);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    xsh->insertColumn(m_columnIndex);
    int frameCount                 = m_childLevel->getXsheet()->getFrameCount();
    if (frameCount < 1) frameCount = 1;
    for (int r = 0; r < frameCount; r++)
      xsh->setCell(r, m_columnIndex,
                   TXshCell(m_childLevel.getPointer(), TFrameId(r + 1)));
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override {
    // bisognerebbe tener conto della dimensione del sottoxsheet
    return sizeof(*this) + 100;
  }

  QString getHistoryString() override {
    return QObject::tr("Clone Sub-xsheet :  Col%1")
        .arg(QString::number(m_columnIndex + 1));
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// cloneChild
//=============================================================================

void ColumnCmd::cloneChild(int index) {
  if (!canResequence(index)) return;

  /*-- カラムを取得 --*/
  TXsheet *xsh       = TApp::instance()->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(index);
  assert(column);

  // get the subxsheet to clone (childLevel, childXsh)
  /*-- SubXsheetレベルを取得 --*/
  TXshLevelColumn *lcolumn = column->getLevelColumn();
  assert(lcolumn);
  int r0 = 0, r1 = -1;
  lcolumn->getRange(r0, r1);
  assert(r0 <= r1);
  /*-- SubXsheetの一番頭のセル --*/
  TXshCell cell = lcolumn->getCell(r0);
  assert(!cell.isEmpty());
  /*- cell内に格納されているLevelを取得 -*/
  TXshChildLevel *childLevel = cell.m_level->getChildLevel();
  assert(childLevel);
  /*- SubXsheetのXsheetを取得 -*/
  TXsheet *childXsh = childLevel->getXsheet();

  // insert a new empty column
  /*- 隣に空きColumnをInsertしてCloneに備える -*/
  int newColumnIndex = index + 1;
  xsh->insertColumn(newColumnIndex);

  // create a subxsheet (newChildLevel, newChildXsh)
  ToonzScene *scene      = TApp::instance()->getCurrentScene()->getScene();
  ChildStack *childStack = scene->getChildStack();
  TXshChildLevel *newChildLevel = childStack->createChild(0, newColumnIndex);
  TXsheet *newChildXsh          = newChildLevel->getXsheet();

  // copy columns.
  std::set<int> indices;
  for (int i = 0; i < childXsh->getColumnCount(); i++) indices.insert(i);
  StageObjectsData *data = new StageObjectsData();
  data->storeColumns(indices, childXsh, 0);
  data->storeColumnFxs(indices, childXsh, 0);
  std::list<int> restoredSplineIds;
  data->restoreObjects(indices, restoredSplineIds, newChildXsh,
                       StageObjectsData::eDoClone);
  delete data;

  cloneNotColumnLinkedFxsAndOutputsFx(childXsh, newChildXsh);
  cloneXsheetTStageObjectTree(childXsh, newChildXsh);
  /*--以下は、Clone SubXsheet
  するときに、SubXsheet内にある子SubXsheetをクローンする関数
  クローンされた中にある子SubXsheetは、同じもので良いので、スキップする --*/
  // cloneSubXsheets(newChildXsh);

  /*-- XSheetノードのFxSchematicでのDagNodePosを再現
  FxやColumnノードの位置の再現は上のsetColumnで行っている
--*/
  newChildXsh->getFxDag()->getXsheetFx()->getAttributes()->setDagNodePos(
      childXsh->getFxDag()->getXsheetFx()->getAttributes()->getDagNodePos());

  newChildXsh->updateFrameCount();

  /*-- TXshChildLevel作成時にsetCellした1つ目のセルを消去 --*/
  xsh->removeCells(0, newColumnIndex);
  /*-- CloneしたColumnのセル番号順を再現 --*/
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = lcolumn->getCell(r);
    if (cell.isEmpty()) continue;

    cell.m_level = newChildLevel;
    xsh->setCell(r, newColumnIndex, cell);
  }

  TStageObjectId currentObjectId =
      TApp::instance()->getCurrentObject()->getObjectId();
  xsh->getStageObject(TStageObjectId::ColumnId(newColumnIndex))
      ->setParent(xsh->getStageObjectParent(currentObjectId));

  xsh->updateFrameCount();
  TUndoManager::manager()->add(
      new CloneChildUndo(newChildLevel, newColumnIndex));

  // notify changes
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================================
// copyColumn
//=============================================================================

void ColumnCmd::copyColumn(int dstIndex, int srcIndex) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  // columns[srcIndex] => data
  StageObjectsData *data = new StageObjectsData();
  std::set<int> ii;
  ii.insert(srcIndex);
  data->storeColumns(ii, xsh, StageObjectsData::eDoClone);
  data->storeColumnFxs(ii, xsh, StageObjectsData::eDoClone);

  // data => columns[dstIndex]
  ii.clear();
  ii.insert(dstIndex);
  ColumnCmd::pasteColumns(ii, data);

  delete data;
}

//=============================================================================
// Undo Clear Cells
//=============================================================================

class ClearColumnCellsUndo final : public TUndo {
  int m_col;
  int m_r0;
  std::vector<TXshCell> m_oldFrames;

public:
  ClearColumnCellsUndo(int col) : m_col(col), m_r0(0) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int r0, r1;
    xsh->getCellRange(col, r0, r1);
    m_r0 = r0;
    if (r0 > r1) return;
    for (int r = r0; r <= r1; r++) {
      m_oldFrames.push_back(xsh->getCell(r, col));
    }
  }

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (int i = 0; i < (int)m_oldFrames.size(); i++) {
      xsh->setCell(m_r0 + i, m_col, m_oldFrames[i]);
    }
    xsh->updateFrameCount();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int r0, r1;
    xsh->getCellRange(m_col, r0, r1);
    if (r0 <= r1) {
      xsh->clearCells(r0, m_col, r1 - r0 + 1);
      xsh->updateFrameCount();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + m_oldFrames.size() * sizeof(m_oldFrames[0]);
  }

  QString getHistoryString() override {
    return QObject::tr("Clear Cells :  Col%1").arg(QString::number(m_col + 1));
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// clearCells
//=============================================================================

void ColumnCmd::clearCells(int index) {
  ClearColumnCellsUndo *undo = new ClearColumnCellsUndo(index);
  undo->redo();
  TUndoManager::manager()->add(undo);
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//=============================================================================

namespace {

enum {
  CMD_LOCK        = 0x0001,
  CMD_UNLOCK      = 0x0002,
  CMD_TOGGLE_LOCK = 0x0004,

  CMD_ENABLE_PREVIEW  = 0x0008,
  CMD_DISABLE_PREVIEW = 0x0010,
  CMD_TOGGLE_PREVIEW  = 0x0020,

  CMD_ENABLE_CAMSTAND  = 0x0040,
  CMD_DISABLE_CAMSTAND = 0x0080,
  CMD_TOGGLE_CAMSTAND  = 0x0100
};

enum {
  TARGET_ALL,
  TARGET_SELECTED,
  TARGET_CURRENT,
  TARGET_OTHER,
  TARGET_UPPER /*-- カレントカラムより右側にあるカラムを非表示にするコマンド
                  --*/
};

}  // namespace

class ColumnsStatusCommand final : public MenuItemHandler {
  int m_cmd, m_target;

public:
  ColumnsStatusCommand(CommandId id, int cmd, int target)
      : MenuItemHandler(id), m_cmd(cmd), m_target(target) {}

  void execute() override {
    TColumnSelection *selection = dynamic_cast<TColumnSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    TXsheet *xsh       = TApp::instance()->getCurrentXsheet()->getXsheet();
    int cc             = TApp::instance()->getCurrentColumn()->getColumnIndex();
    bool sound_changed = false;
    for (int i = 0; i < xsh->getColumnCount(); i++) {
      /*- 空のカラムの場合は飛ばす -*/
      if (xsh->isColumnEmpty(i)) continue;
      /*- カラムが取得できなかったら飛ばす -*/
      TXshColumn *column = xsh->getColumn(i);
      if (!column) continue;
      /*- ターゲットが選択カラムのモードで、選択されていなかった場合は飛ばす -*/
      bool isSelected = selection && selection->isColumnSelected(i);
      if (m_target == TARGET_SELECTED && !isSelected) continue;

      /*-
       * ターゲットが「カレントカラムより右側」のモードで、iがカレントカラムより左の場合は飛ばす
       * -*/
      if (m_target == TARGET_UPPER && i < cc) continue;

      bool negate = m_target == TARGET_CURRENT && cc != i ||
                    m_target == TARGET_OTHER && cc == i ||
                    m_target == TARGET_UPPER && cc == i;

      int cmd = m_cmd;

      if (cmd & (CMD_LOCK | CMD_UNLOCK | CMD_TOGGLE_LOCK)) {
        if (cmd & CMD_LOCK)
          column->lock(!negate);
        else if (cmd & CMD_UNLOCK)
          column->lock(negate);
        else
          column->lock(!column->isLocked());
      }
      if (cmd &
          (CMD_ENABLE_PREVIEW | CMD_DISABLE_PREVIEW | CMD_TOGGLE_PREVIEW)) {
        if (cmd & CMD_ENABLE_PREVIEW)
          column->setPreviewVisible(!negate);
        else if (cmd & CMD_DISABLE_PREVIEW)
          column->setPreviewVisible(negate);
        else
          column->setPreviewVisible(!column->isPreviewVisible());
      }
      if (cmd &
          (CMD_ENABLE_CAMSTAND | CMD_DISABLE_CAMSTAND | CMD_TOGGLE_CAMSTAND)) {
        if (cmd & CMD_ENABLE_CAMSTAND)
          column->setCamstandVisible(!negate);
        else if (cmd & CMD_DISABLE_CAMSTAND)
          column->setCamstandVisible(negate);
        else
          column->setCamstandVisible(!column->isCamstandVisible());
        if (column->getSoundColumn()) sound_changed = true;
      }
      /*TAB
if(cmd & (CMD_ENABLE_PREVIEW|CMD_DISABLE_PREVIEW|CMD_TOGGLE_PREVIEW))
{ //In Tab preview e cameraStand vanno settati entrambi
if(cmd&CMD_ENABLE_PREVIEW)
{
column->setPreviewVisible(!negate);
column->setCamstandVisible(!negate);
}
else if(cmd&CMD_DISABLE_PREVIEW)
{
column->setPreviewVisible(negate);
column->setCamstandVisible(negate);
}
else
{
column->setPreviewVisible(!column->isPreviewVisible());
column->setCamstandVisible(!column->isCamstandVisible());
}
}
      */
    }
    if (sound_changed)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }
};

namespace {
ColumnsStatusCommand

    c00(MI_ActivateAllColumns, CMD_ENABLE_CAMSTAND, TARGET_ALL),
    c01(MI_DeactivateAllColumns, CMD_DISABLE_CAMSTAND, TARGET_ALL),
    c02(MI_ActivateThisColumnOnly, CMD_ENABLE_CAMSTAND, TARGET_CURRENT),
    c03(MI_ToggleColumnsActivation, CMD_TOGGLE_CAMSTAND, TARGET_ALL),
    c04(MI_ActivateSelectedColumns, CMD_ENABLE_CAMSTAND, TARGET_SELECTED),
    c05(MI_DeactivateSelectedColumns, CMD_DISABLE_CAMSTAND, TARGET_SELECTED),
    c18(MI_DeactivateUpperColumns, CMD_DISABLE_CAMSTAND, TARGET_UPPER),

    c06(MI_EnableAllColumns, CMD_ENABLE_PREVIEW, TARGET_ALL),
    c07(MI_DisableAllColumns, CMD_DISABLE_PREVIEW, TARGET_ALL),
    c08(MI_EnableThisColumnOnly, CMD_ENABLE_PREVIEW, TARGET_CURRENT),
    c09(MI_SwapEnabledColumns, CMD_TOGGLE_PREVIEW, TARGET_ALL),
    c10(MI_EnableSelectedColumns, CMD_ENABLE_PREVIEW, TARGET_SELECTED),
    c11(MI_DisableSelectedColumns, CMD_DISABLE_PREVIEW, TARGET_SELECTED),

    c12(MI_LockAllColumns, CMD_LOCK, TARGET_ALL),
    c13(MI_UnlockAllColumns, CMD_UNLOCK, TARGET_ALL),
    c14(MI_LockThisColumnOnly, CMD_LOCK, TARGET_CURRENT),
    c15(MI_ToggleColumnLocks, CMD_TOGGLE_LOCK, TARGET_ALL),
    c16(MI_LockSelectedColumns, CMD_LOCK, TARGET_SELECTED),
    c17(MI_UnlockSelectedColumns, CMD_UNLOCK, TARGET_SELECTED);
}  // namespace
