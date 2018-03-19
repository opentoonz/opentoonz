

// TnzCore includes
#include "tfx.h"
#include "tfxattributes.h"
#include "tstroke.h"
#include "tconst.h"

// TnzBase includes
#include "tbasefx.h"

// TnzLib includes
#include "toonz/tstageobject.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/fxdag.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tcolumnfx.h"
#include "toonz/tstageobjectspline.h"

#include "toonzqt/stageobjectsdata.h"

// TODO: Method StageObjectsData::storeFxs() has no well-defined behaviour in
// cases
//      of 'jumpy' fx selections.

//********************************************************************************
//    Local namespace
//********************************************************************************

namespace {

enum FxCanGenerateState {
  NoInput = 0,  // This fx has input port, but no connected fx - state will not
                // transmit to parents and leave the decision up to another
                // input fx
  CanGenerate,  // This fx can generate only from fxsSet input
  CanNotGenerate  // This fx contains input fx other than fxsSet - state will
                  // transmit to parents
};

//! Returns whether ALL input chains from fx pass through fxsSet - ie if
//! fxsSet can completely generate fx's input.
//! \note Columns are assumed to be generated only if they are in fxsSet.
FxCanGenerateState canGenerate(const std::set<TFx *> &fxsSet, TFx *fx) {
  assert(fx);

  if (fxsSet.count(fx) > 0) return CanGenerate;

  int p, pCount = fx->getInputPortCount();

  if (pCount == 0) return CanNotGenerate;

  bool found = false;
  for (p = 0; p != pCount; ++p) {
    TFx *inputFx = fx->getInputPort(p)->getFx();
    if (!inputFx) continue;

    FxCanGenerateState tmpState = canGenerate(fxsSet, inputFx);
    // Inability to generate transmits to parents
    if (tmpState == CanNotGenerate)
      return CanNotGenerate;
    else if (tmpState == CanGenerate)
      found = true;
  }

  return (found) ? CanGenerate
                 : NoInput;  // Columns return false, the others checked ports
}

//------------------------------------------------------------------------------

//! Returns whether the specified fx is a downstream node of the xsheet node.
bool hasTerminalUpstream(TFx *fx, TFxSet *terminalFxs) {
  if (TZeraryFx *zfx = dynamic_cast<TZeraryFx *>(fx))
    return hasTerminalUpstream(zfx->getColumnFx(), terminalFxs);

  if (terminalFxs->containsFx(fx)) return true;

  // If the fx does not have output connections, check if it's terminal
  int outputConnectionsCount = fx->getOutputConnectionCount();
  if (!outputConnectionsCount) return terminalFxs->containsFx(fx);

  // Search output connections
  for (int i = 0; i < outputConnectionsCount; ++i) {
    TFx *parentFx = fx->getOutputConnection(i)->getOwnerFx();
    if (parentFx && hasTerminalUpstream(parentFx, terminalFxs)) return true;
  }

  return false;
}

//------------------------------------------------------------------------------

//! Returns whether the fx is terminal or has a not-generable, terminal-upstream
//! parent.
bool isColumnSelectionTerminalFx(TFx *fx, TFxSet *terminalFxs,
                                 const std::set<TFx *> &columnFxs) {
  assert(canGenerate(columnFxs, fx) == CanGenerate);

  if (terminalFxs->containsFx(fx)) return true;

  int oc, ocCount = fx->getOutputConnectionCount();
  for (oc = 0; oc != ocCount; ++oc) {
    TFx *parentFx = fx->getOutputConnection(oc)->getOwnerFx();

    if (TZeraryFx *zfx = dynamic_cast<TZeraryFx *>(parentFx))
      parentFx = zfx->getColumnFx();

    if (parentFx && hasTerminalUpstream(parentFx, terminalFxs) &&
        canGenerate(columnFxs, parentFx) != CanGenerate)
      return true;
  }

  return false;
}

}  // namespace

//********************************************************************************
//    TStageObjectDataElement  definition
//********************************************************************************

//! Base class used to clone a stage object's relational data inside the stage
//! schematic.
class TStageObjectDataElement {
protected:
  friend class StageObjectsData;

  TStageObjectParams *m_params;  //!< Stage object parameters (owned)
  TPointD m_dagPos;              //!< Stage object position in the viewer

public:
  TStageObjectDataElement();
  virtual ~TStageObjectDataElement();

  virtual TStageObjectDataElement *clone() const;

  //! Stores the object data strictly regarding the object being part of
  //! the schematic stage view (TStageObjectDataElement <- TStageObjectId).
  void storeObject(const TStageObjectId &objectId, TXsheet *xsh);

  //! Restores the object data in the xsheet (TStageObjectDataElement ->
  //! TXsheet) and
  //! returns the associated new stage object identifier.
  TStageObjectId restoreObject(TXsheet *xsh, bool copyPosition) const;
};

//------------------------------------------------------------------------------

TStageObjectDataElement::TStageObjectDataElement()
    : m_params(0), m_dagPos(TConst::nowhere) {}

//------------------------------------------------------------------------------

TStageObjectDataElement::~TStageObjectDataElement() { delete m_params; }

//------------------------------------------------------------------------------

TStageObjectDataElement *TStageObjectDataElement::clone() const {
  TStageObjectDataElement *data = new TStageObjectDataElement();

  data->m_params = m_params->clone();
  data->m_dagPos = m_dagPos;

  return data;
}

//------------------------------------------------------------------------------

void TStageObjectDataElement::storeObject(const TStageObjectId &objId,
                                          TXsheet *xsh) {
  // Search the object in the xsheet (false = don't create a new one)
  TStageObject *obj = xsh->getStageObjectTree()->getStageObject(objId, false);
  assert(obj);

  m_params = obj->getParams();
  m_dagPos = obj->getDagNodePos();
}

//------------------------------------------------------------------------------

TStageObjectId TStageObjectDataElement::restoreObject(TXsheet *xsh,
                                                      bool copyPosition) const {
  int index = 2;  // Skip the table and camera 1 (I guess)

  // Search the first unused common (pegbar) id
  TStageObjectTree *tree = xsh->getStageObjectTree();
  while (tree->getStageObject(TStageObjectId::PegbarId(index), false)) ++index;

  // Create the new object to be inserted
  TStageObject *newObj =
      tree->getStageObject(TStageObjectId::PegbarId(index), true);
  newObj->setParent(m_params->m_parentId);
  newObj->assignParams(m_params);

  // If specified, copy the stored position in the viewer
  if (copyPosition) newObj->setDagNodePos(m_dagPos);

  return newObj->getId();
}

//********************************************************************************
//    TColumnDataElement  definition
//********************************************************************************

//! Class used to clone a column stage object's relational data inside the stage
//! schematic.
class TColumnDataElement final : public TStageObjectDataElement {
  friend class StageObjectsData;

  TXshColumnP m_column;  //!< Column associated with the stage object

public:
  enum FxFlags { eDoClone = 0x1, eResetFxDagPositions = 0x2 };

public:
  TColumnDataElement();
  ~TColumnDataElement();

  //! Clones the stord data.
  //! \warning Clones the stored TXshColumnP, too.
  TColumnDataElement *clone() const override;

  //! Stores the stage object and column data of the specified column index in
  //! the supplied xsheet.
  //! Specifically, the TXshColumnP associated with the desired column index is
  //! stored.
  //! Supported additional flags include performing a clone of the associated
  //! TXshColumnP rather than
  //! storing the original.
  void storeColumn(TXsheet *xsheet, int index, int fxFlags);

  //! Inserts the stored column in the supplied xsheet with the specified column
  //! index, returning
  //! the new associated stage object id.
  //! Supported additional flags include performing a clone of the associated
  //! TXshColumnP rather than
  //! restoring the stored column directly.
  TStageObjectId restoreColumn(TXsheet *xsh, int index, int fxFlags,
                               bool copyPosition) const;
};

//------------------------------------------------------------------------------

TColumnDataElement::TColumnDataElement()
    : TStageObjectDataElement(), m_column(0) {}

//------------------------------------------------------------------------------

TColumnDataElement::~TColumnDataElement() {}

//------------------------------------------------------------------------------

TColumnDataElement *TColumnDataElement::clone() const {
  TColumnDataElement *element = new TColumnDataElement();

  element->m_params = m_params->clone();
  element->m_dagPos = m_dagPos;
  element->m_column = m_column;

  if (element->m_column) element->m_column = element->m_column->clone();

  return element;
}

//------------------------------------------------------------------------------

void TColumnDataElement::storeColumn(TXsheet *xsh, int index, int fxFlags) {
  if (index < 0) return;

  bool doClone             = (fxFlags & eDoClone);
  bool resetFxDagPositions = (fxFlags & eResetFxDagPositions);

  // Fetch the specified column (if none, return)
  TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(index));
  assert(obj);

  TXshColumn *column = xsh->getColumn(index);
  if (!column) return;

  TFx *fx = column->getFx();
  TPointD dagPos;

  if (fx) dagPos = fx->getAttributes()->getDagNodePos();
  if (doClone)
    column = column->clone();  // Zerary fxs clone the associated fx (drawn
                               // levels do not)
  if (fx && !resetFxDagPositions)
    column->getFx()->getAttributes()->setDagNodePos(dagPos);

  m_column = column;
  storeObject(obj->getId(), xsh);
}

//------------------------------------------------------------------------------

TStageObjectId TColumnDataElement::restoreColumn(TXsheet *xsh, int index,
                                                 int fxFlags,
                                                 bool copyPosition) const {
  bool doClone             = (fxFlags & eDoClone);
  bool resetFxDagPositions = (fxFlags & eResetFxDagPositions);

  TXshColumn *column = m_column.getPointer();

  // The xsheet 'changes' if a new column is inserted. If it was already there,
  // no.
  bool xsheetChange = false;
  if (column && column->getXsheet() && column->getXsheet() != xsh)
    xsheetChange = true;

  // Insert a column at the specified index. If a column was stored, insert that
  // one.
  TPointD dagPos = TConst::nowhere;
  if (column) {
    if (column->getFx())
      dagPos            = column->getFx()->getAttributes()->getDagNodePos();
    if (doClone) column = column->clone();
    xsh->insertColumn(index, column);
  } else
    xsh->insertColumn(index);  // Create a new one otherwise

  if (!resetFxDagPositions && dagPos != TConst::nowhere) {
    // Don't accept the default position (fx object)
    TXshColumn *restoredColumn = xsh->getColumn(index);
    restoredColumn->getFx()->getAttributes()->setDagNodePos(dagPos);
  }

  // Retrieve the newly inserted column stage object
  TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(index));
  assert(obj);
  obj->assignParams(m_params, doClone);  // Assign the stored params

  if (copyPosition) obj->setDagNodePos(m_dagPos);

  // Clone the associated curve if any
  if (xsheetChange && obj->getSpline()) {
    TStageObjectSpline *srcSpl = obj->getSpline();
    TStageObjectSpline *dstSpl = xsh->getStageObjectTree()->createSpline();
    dstSpl->addRef();
    dstSpl->setStroke(new TStroke(*srcSpl->getStroke()));
    obj->setSpline(dstSpl);
  }

  int gridType = xsh->getStageObjectTree()->getDagGridDimension();
  obj->setIsOpened(gridType ==
                   0);  // gridType is 0 if the node is opened, 1 if closed
                        // see StageSchematicScene::GridDimension
                        // TODO: Should be made PUBLIC!!

  xsh->updateFrameCount();
  return obj->getId();
}

//********************************************************************************
//    TCameraDataElement  definition
//********************************************************************************

//! Class used to clone a camera stage object's relational data inside the stage
//! schematic.
class TCameraDataElement final : public TStageObjectDataElement {
  TCamera m_camera;  //!< The object's camera

public:
  TCameraDataElement();
  ~TCameraDataElement();

  TCameraDataElement *clone() const override;

  //! Stores the specified camera object.
  void storeCamera(const TStageObjectId &selectedObj, TXsheet *xsh);

  //! Restores the stored camera object.
  TStageObjectId restoreCamera(TXsheet *xsh, bool copyPosition) const;
};

//------------------------------------------------------------------------------

TCameraDataElement::TCameraDataElement() : TStageObjectDataElement() {}

//------------------------------------------------------------------------------

TCameraDataElement::~TCameraDataElement() {}

//------------------------------------------------------------------------------

TCameraDataElement *TCameraDataElement::clone() const {
  TCameraDataElement *data = new TCameraDataElement();

  data->m_params = m_params->clone();
  data->m_camera = m_camera;
  data->m_dagPos = m_dagPos;

  return data;
}

//------------------------------------------------------------------------------

void TCameraDataElement::storeCamera(const TStageObjectId &selectedObj,
                                     TXsheet *xsh) {
  TStageObject *obj =
      xsh->getStageObjectTree()->getStageObject(selectedObj, false);
  assert(obj);

  m_params = obj->getParams();
  m_camera = *(obj->getCamera());
  m_dagPos = obj->getDagNodePos();
}

//------------------------------------------------------------------------------

TStageObjectId TCameraDataElement::restoreCamera(TXsheet *xsh,
                                                 bool copyPosition) const {
  TStageObjectTree *tree = xsh->getStageObjectTree();

  // Search the first unused camera id in the xsheet
  int index = 0;
  while (tree->getStageObject(TStageObjectId::CameraId(index), false)) ++index;

  // Create the new camera object and assign stored data
  TStageObject *newCamera =
      tree->getStageObject(TStageObjectId::CameraId(index), true);
  newCamera->assignParams(m_params);
  *(newCamera->getCamera()) = m_camera;

  if (copyPosition) newCamera->setDagNodePos(m_dagPos);

  return newCamera->getId();
}

//********************************************************************************
//    TSplineDataElement definition
//********************************************************************************

class TSplineDataElement {
  enum Flags { eDoClone = 0x1, eResetFxDagPositions = 0x2 };

  TStageObjectSpline *m_spline;

public:
  TSplineDataElement();
  ~TSplineDataElement();

  TSplineDataElement *clone();
  void storeSpline(TStageObjectSpline *spline, int flag);
  TStageObjectSpline *restoreSpline(int flag) const;
};

//********************************************************************************
//    TSplineDataElement implementation
//********************************************************************************

TSplineDataElement::TSplineDataElement() : m_spline(0) {}

//------------------------------------------------------------------------------

TSplineDataElement::~TSplineDataElement() {
  if (m_spline) m_spline->release();
}

//------------------------------------------------------------------------------

TSplineDataElement *TSplineDataElement::clone() {
  TSplineDataElement *data = new TSplineDataElement();
  if (m_spline) {
    data->m_spline = m_spline->clone();
    data->m_spline->addRef();
  }
  return data;
}

//------------------------------------------------------------------------------

void TSplineDataElement::storeSpline(TStageObjectSpline *spline, int flag) {
  if (!spline) return;

  m_spline = (flag & eDoClone) ? spline->clone() : spline;
  m_spline->addRef();
  if (flag & eResetFxDagPositions) m_spline->setDagNodePos(TConst::nowhere);
}

//------------------------------------------------------------------------------

TStageObjectSpline *TSplineDataElement::restoreSpline(int flag) const {
  TStageObjectSpline *spline = (flag & eDoClone) ? m_spline->clone() : m_spline;
  if (flag & eResetFxDagPositions) spline->setDagNodePos(TConst::nowhere);

  return spline;
}

//********************************************************************************
//    StageObjectsData  implementation
//********************************************************************************

StageObjectsData::StageObjectsData() : DvMimeData() {}

//------------------------------------------------------------------------------

StageObjectsData::~StageObjectsData() {
  int i, elementsCount = m_elements.size();
  for (i = 0; i < elementsCount; ++i) delete m_elements[i];

  for (i = 0; i < m_splines.size(); ++i) delete m_splines[i];

  std::set<TFx *>::iterator it;
  for (it = m_fxs.begin(); it != m_fxs.end(); ++it) {
    TFx *fx = *it;
    if (fx) fx->release();
  }

  for (it = m_terminalFxs.begin(); it != m_terminalFxs.end(); ++it) {
    TFx *fx = *it;
    if (fx) fx->release();
  }
}

//------------------------------------------------------------------------------

bool StageObjectsData::checkCircularReferences(TXsheet *xsh) const {
  int i, elementsCount = m_elements.size();
  for (i = 0; i < elementsCount; ++i) {
    TColumnDataElement *columnElement =
        dynamic_cast<TColumnDataElement *>(m_elements[i]);
    if (!columnElement) continue;

    if (xsh->checkCircularReferences(columnElement->m_column.getPointer()))
      return true;
  }

  return false;
}

//------------------------------------------------------------------------------

StageObjectsData *StageObjectsData::clone() const {
  StageObjectsData *data = new StageObjectsData();

  // Clone each element (the new data gets ownership)
  int i, elementsCount = m_elements.size();
  for (i = 0; i < elementsCount; ++i)
    data->m_elements.append(m_elements[i]->clone());

  // Clone each spline (the new data gets ownership)
  for (i = 0; i < m_splines.size(); ++i)
    data->m_splines.append(m_splines[i]->clone());

  // Same for internal fxs
  std::map<TFx *, TFx *> fxTable;  // And trace the pairings with the originals
  std::set<TFx *>::const_iterator it;
  for (it = m_fxs.begin(); it != m_fxs.end(); ++it) {
    TFx *fxOrig = *it;
    assert(fxOrig);
    assert(fxTable.count(fxOrig) == 0);

    TFx *fx = fxOrig->clone(false);
    fx->getAttributes()->setId(fxOrig->getAttributes()->getId());
    fx->getAttributes()->passiveCacheDataIdx() = -1;
    fx->setName(fxOrig->getName());
    fx->setFxId(fxOrig->getFxId());

    fxTable[fxOrig] = fx;
    fx->addRef();
    data->m_fxs.insert(fx);
  }

  // Same with terminals
  for (it = m_terminalFxs.begin(); it != m_terminalFxs.end(); ++it) {
    TFx *fxOrig = *it;
    assert(fxOrig);

    // If the fx was not already cloned above, do it now
    TFx *fx = searchFx(fxTable, fxOrig);
    if (!fx) {
      fx = fxOrig->clone(false);
      fx->getAttributes()->setId(fxOrig->getAttributes()->getId());
      fx->getAttributes()->passiveCacheDataIdx() = -1;
      fx->setName(fxOrig->getName());
      fx->setFxId(fxOrig->getFxId());
      fxTable[fxOrig] = fx;
    }

    fx->addRef();
    data->m_terminalFxs.insert(fx);
  }

  if (!fxTable.empty())
    updateFxLinks(
        fxTable);  // Applies the traced map pairings to every fx descendant
                   // of each fx stored in the map.

  // WARNING: m_fxsTable is NOT COPIED / CLONED !!

  return data;
}

//------------------------------------------------------------------------------

void StageObjectsData::storeObjects(const std::vector<TStageObjectId> &ids,
                                    TXsheet *xsh, int fxFlags) {
  assert(
      m_fxTable.empty());  // Should be enforced OUTSIDE. Track implicit uses.
  m_fxTable.clear();       // TO BE REMOVED

  int i, objCount = ids.size();

  // Discriminate sensible stage object types (ie cameras and columns from the
  // rest).
  // Store them in a map, ordered by object index.

  std::map<int, TStageObjectId> cameraIds, columnIds, pegbarIds;
  for (i = 0; i < objCount; ++i) {
    TStageObjectId id = ids[i];
    if (id.isColumn())
      columnIds[id.getIndex()] = id;
    else if (id.isPegbar())
      pegbarIds[id.getIndex()] = id;
    else if (id.isCamera())
      cameraIds[id.getIndex()] = id;
  }

  // Store a suitable object for each
  std::map<int, TStageObjectId>::iterator it;
  for (it = cameraIds.begin(); it != cameraIds.end(); ++it) {
    // Cameras
    TCameraDataElement *cameraElement = new TCameraDataElement();
    cameraElement->storeCamera(it->second, xsh);
    m_elements.append(cameraElement);
  }

  for (it = pegbarIds.begin(); it != pegbarIds.end(); ++it) {
    // Pegbars (includes curves)
    TStageObjectDataElement *objElement = new TStageObjectDataElement();
    objElement->storeObject(it->second, xsh);
    m_elements.append(objElement);
  }

  for (it = columnIds.begin(); it != columnIds.end(); ++it) {
    // Columns
    int colIndex = it->second.getIndex();

    TXshColumn *column = xsh->getColumn(colIndex);
    if (!column) continue;

    TColumnDataElement *columnElement = new TColumnDataElement();
    columnElement->storeColumn(xsh, colIndex, fxFlags);
    m_elements.append(columnElement);

    TXshColumn *copiedColumn = columnElement->m_column.getPointer();
    if (column->getFx() && copiedColumn->getFx()) {
      // Store column fx pairings (even if the originals are not cloned)
      m_fxTable[column->getFx()] = copiedColumn->getFx();
      m_originalColumnFxs.insert(column->getFx());
    }
  }

  // Insert terminal fxs
  set<TFx *>::iterator jt;
  for (jt = m_originalColumnFxs.begin(); jt != m_originalColumnFxs.end();
       ++jt) {
    if (isColumnSelectionTerminalFx(*jt, xsh->getFxDag()->getTerminalFxs(),
                                    m_originalColumnFxs)) {
      TFx *fx = m_fxTable[*jt];

      fx->addRef();
      m_terminalFxs.insert(fx);
    }
  }
}

//------------------------------------------------------------------------------

void StageObjectsData::storeColumns(const std::set<int> &columnIndexes,
                                    TXsheet *xsh, int fxFlags) {
  std::vector<TStageObjectId> ids;

  std::set<int>::const_iterator it;
  for (it = columnIndexes.begin(); it != columnIndexes.end(); ++it)
    ids.push_back(TStageObjectId::ColumnId(*it));

  storeObjects(ids, xsh, fxFlags);
}

//------------------------------------------------------------------------------

void StageObjectsData::storeFxs(const std::set<TFx *> &fxs, TXsheet *xsh,
                                int fxFlags) {
  bool doClone             = (fxFlags & eDoClone);
  bool resetFxDagPositions = (fxFlags & eResetFxDagPositions);

  TFxSet *terminalFxs = xsh->getFxDag()->getTerminalFxs();

  // Traverse specified fxs
  std::set<TFx *>::const_iterator it;
  for (it = fxs.begin(); it != fxs.end(); ++it) {
    TFx *fxOrig = *it, *fx = fxOrig;

    if (doClone) {
      // If required, clone them
      fx = fxOrig->clone(false);

      fx->setName(fxOrig->getName());
      fx->getAttributes()->setId(fxOrig->getAttributes()->getId());
      fx->getAttributes()->passiveCacheDataIdx() = -1;

      if (resetFxDagPositions)
        fx->getAttributes()->setDagNodePos(TConst::nowhere);
    }

    // Store them (and the original/clone pairing even if not cloning)
    m_fxTable[fxOrig] = fx;
    fx->addRef();
    m_fxs.insert(fx);

    // Find out if the fx is a terminal one in the selection. If so, store it
    // there too.
    bool isTerminal = true;
    if (!terminalFxs->containsFx(
            fxOrig))  // If it's terminal in the xsheet, no doubt
    {
      // Otherwise, check terminality with respect to the selection
      int i, outputConnectionsCount = fxOrig->getOutputConnectionCount();
      for (i = 0; i < outputConnectionsCount; ++i) {
        TFx *outputFx = fxOrig->getOutputConnection(i)->getOwnerFx();
        if (outputFx && fxs.count(outputFx) > 0) {
          isTerminal = false;
          break;
        }
      }
    }

    // Well, excluding true TOutputFxs...
    TOutputFx *outFx = dynamic_cast<TOutputFx *>(fx);
    if (isTerminal && !outFx) {
      fx->addRef();
      m_terminalFxs.insert(fx);
    }
  }

  // Updating terminality of the column fxs too!
  // WARNING: This requires that storeObjects() is invoked BEFORE this !
  for (it = m_originalColumnFxs.begin(); it != m_originalColumnFxs.end();
       ++it) {
    TFx *fxOrig = *it;

    bool isTerminal = true;
    if (!terminalFxs->containsFx(fxOrig)) {
      int i, outputConnectionsCount = fxOrig->getOutputConnectionCount();
      for (i = 0; i < outputConnectionsCount; ++i) {
        TFx *outputFx = fxOrig->getOutputConnection(i)->getOwnerFx();
        if (outputFx && fxs.count(outputFx) > 0) {
          isTerminal = false;
          break;
        }
      }
    }

    if (isTerminal) {
      TFx *fx = m_fxTable[fxOrig];

      fx->addRef();
      m_terminalFxs.insert(fx);
    }
  }

  if (!m_fxTable.empty() && doClone)
    updateFxLinks(m_fxTable);  // Apply original/clone pairings
                               // to fx relatives
}

//------------------------------------------------------------------------------

void StageObjectsData::storeColumnFxs(const std::set<int> &columnIndexes,
                                      TXsheet *xsh, int fxFlags) {
  bool doClone             = (fxFlags & eDoClone);
  bool resetFxDagPositions = (fxFlags & eResetFxDagPositions);

  std::set<TFx *> internalFxs;
  xsh->getFxDag()->getInternalFxs()->getFxs(internalFxs);

  // Iterate internal fxs (note: columns NOT included)
  // NOTE: Could this be too heavy ? Shouldn't we travel upstream from given
  // column fxs?
  std::set<TFx *>::iterator it;
  for (it = internalFxs.begin(); it != internalFxs.end(); ++it) {
    TFx *fxOrig = *it, *fx = fxOrig;

    if (m_fxTable.find(fx) != m_fxTable.end())  // If already treated
      continue;

    if (canGenerate(m_originalColumnFxs,
                    fx) != CanGenerate)  // If not completely in the upstream
      continue;

    if (doClone) {
      // Clone the fx if required
      fx = fxOrig->clone(false);

      fx->setName(fxOrig->getName());
      fx->getAttributes()->setId(fxOrig->getAttributes()->getId());
      fx->getAttributes()->passiveCacheDataIdx() = -1;

      if (resetFxDagPositions)
        fx->getAttributes()->setDagNodePos(TConst::nowhere);
    }

    m_fxTable[fxOrig] = fx;
    fx->addRef();
    m_fxs.insert(fx);

    if (isColumnSelectionTerminalFx(fxOrig, xsh->getFxDag()->getTerminalFxs(),
                                    m_originalColumnFxs)) {
      fx->addRef();
      m_terminalFxs.insert(fx);
    }

    if (fxOrig->getLinkedFx() != fxOrig)  // Linked fx
    {
      if (canGenerate(m_originalColumnFxs, fxOrig->getLinkedFx()) !=
          CanGenerate)
        fx->linkParams(fxOrig->getLinkedFx());
      else {
        // Insert the linked fx directly here
        TFx *linkedFx, *oldLinkedFx = fxOrig->getLinkedFx();
        if (doClone) {
          linkedFx = fx->clone(false);  // Not oldLinkedFx->clone() ?
          linkedFx->linkParams(fx);

          linkedFx->setName(oldLinkedFx->getName());
          linkedFx->getAttributes()->setId(
              oldLinkedFx->getAttributes()->getId());
          linkedFx->getAttributes()->passiveCacheDataIdx() = -1;

          if (resetFxDagPositions)
            fx->getAttributes()->setDagNodePos(TConst::nowhere);  // Here too ?

          xsh->getFxDag()->assignUniqueId(linkedFx);
        } else
          linkedFx = oldLinkedFx;

        m_fxTable[oldLinkedFx] = linkedFx;
        linkedFx->addRef();
        m_fxs.insert(linkedFx);

        if (xsh->getFxDag()->getTerminalFxs()->containsFx(
                fx->getLinkedFx()))  // Here too - isATerminal ?
        {
          linkedFx->addRef();
          m_terminalFxs.insert(linkedFx);
        }
      }
    }
  }

  // Like in the functions above, update links
  if (!m_fxTable.empty() && doClone) updateFxLinks(m_fxTable);
}

//------------------------------------------------------------------------------

void StageObjectsData::storeSplines(const std::list<int> &splineIds,
                                    TXsheet *xsh, int fxFlags) {
  std::list<int>::const_iterator it;
  for (it = splineIds.begin(); it != splineIds.end(); ++it) {
    TStageObjectSpline *spline = xsh->getStageObjectTree()->getSplineById(*it);
    int j;
    bool skipSpline = false;
    for (j = 0; j < m_elements.size(); j++) {
      TStageObjectDataElement *element = m_elements[j];
      if (spline == element->m_params->m_spline) {
        skipSpline = true;
        break;
      }
    }
    if (skipSpline) continue;
    TSplineDataElement *splineElement = new TSplineDataElement();
    splineElement->storeSpline(spline, fxFlags);
    m_splines.append(splineElement);
  }
}

//------------------------------------------------------------------------------

std::vector<TStageObjectId> StageObjectsData::restoreObjects(
    std::set<int> &columnIndices, std::list<int> &restoredSpline, TXsheet *xsh,
    int fxFlags, const TPointD &pos) const {
  bool doClone             = (fxFlags & eDoClone);
  bool resetFxDagPositions = (fxFlags & eResetFxDagPositions);

  QMap<TStageObjectId, TStageObjectId>
      idTable;                     // Trace stored/restored id pairings
  std::map<TFx *, TFx *> fxTable;  // Same for fxs here
  std::vector<TStageObjectId> restoredIds;

  std::set<int>::iterator idxt = columnIndices.begin();
  int index                    = -1;  // The actual column insertion index

  int i, elementsCount = m_elements.size();
  for (i = 0; i < elementsCount; ++i) {
    TStageObjectDataElement *element = m_elements[i];

    TCameraDataElement *cameraElement =
        dynamic_cast<TCameraDataElement *>(element);
    TColumnDataElement *columnElement =
        dynamic_cast<TColumnDataElement *>(element);

    // Restore the object depending on its specific type
    TStageObjectId restoredId = TStageObjectId::NoneId;

    if (!cameraElement && !columnElement)
      restoredId = element->restoreObject(xsh, pos != TConst::nowhere);
    else if (cameraElement)
      restoredId = cameraElement->restoreCamera(xsh, pos != TConst::nowhere);
    else if (columnElement) {
      // Build the column insertion index
      if (idxt != columnIndices.end())
        index = *idxt++;
      else {
        ++index;
        columnIndices.insert(index);
      }

      // Restore the column element
      restoredId = columnElement->restoreColumn(xsh, index, fxFlags,
                                                pos != TConst::nowhere);

      FxDag *fxDag = xsh->getFxDag();

      TXshColumn *column       = columnElement->m_column.getPointer();
      TXshColumn *pastedColumn = xsh->getColumn(index);

      TFx *fx       = column->getFx();
      TFx *pastedFx = pastedColumn->getFx();

      if (fx && pastedFx) fxTable[fx] = pastedFx;

      // Enforce the correct terminality. Added columns are terminal by default.
      bool terminal = (fx && (m_terminalFxs.count(fx) > 0));
      if (!terminal) fxDag->getTerminalFxs()->removeFx(pastedFx);

      // In case we've cloned a zerary fx column, update the actual fx's data
      if (TXshZeraryFxColumn *zc =
              dynamic_cast<TXshZeraryFxColumn *>(pastedColumn)) {
        TZeraryColumnFx *zfx = zc->getZeraryColumnFx();
        TFx *zeraryFx        = zfx->getZeraryFx();
        if (zeraryFx && doClone) {
          std::wstring app = zeraryFx->getName();
          fxDag->assignUniqueId(zeraryFx);
          zeraryFx->setName(app);
        }
      }
    }

    // Remember stored/restored stage object pairings
    idTable[element->m_params->m_id] = restoredId;
    restoredIds.push_back(restoredId);
  }

  // Apply stage object-parental relationships
  for (i = 0; i < elementsCount; ++i) {
    TStageObjectDataElement *element = m_elements[i];

    TStageObjectId id       = element->m_params->m_id;
    TStageObjectId parentId = element->m_params->m_parentId;

    TStageObjectId pastedId       = idTable[id];
    TStageObjectId pastedParentId = parentId;

    if (parentId.isColumn())  // Why discriminating for columns only ?
    {
      // Columns are redirected to table ids. If no redirected parent exists,
      // store
      // a void value that will be avoided later
      QMap<TStageObjectId, TStageObjectId>::iterator it =
          idTable.find(parentId);
      pastedParentId =
          (it == idTable.end()) ? TStageObjectId::NoneId : it.value();
    }

    if (pastedParentId != TStageObjectId::NoneId) {
      xsh->setStageObjectParent(pastedId, pastedParentId);
      TStageObject *pastedObj = xsh->getStageObject(pastedId);

      // Shouldn't these be done outside ?
      pastedObj->setHandle(element->m_params->m_handle);
      pastedObj->setParentHandle(element->m_params->m_parentHandle);
    }
  }

  // Iterate stored fxs
  std::set<TFx *>::const_iterator fxt, end = m_fxs.end();
  for (fxt = m_fxs.begin(); fxt != end; ++fxt) {
    TFx *fxOrig = *fxt, *fx = fxOrig;

    // Only NOT COLUMN fxs - ie internal fxs
    if (fxTable.find(fxOrig) != fxTable.end()) continue;

    // Internal fxs

    if (doClone) {
      fx = fxOrig->clone(false);

      fx->setName(fxOrig->getName());
      fx->getAttributes()->setId(fxOrig->getAttributes()->getId());
      fx->getAttributes()->passiveCacheDataIdx() = -1;

      if (resetFxDagPositions)
        fx->getAttributes()->setDagNodePos(TConst::nowhere);

      xsh->getFxDag()->assignUniqueId(
          fx);  // For now, dragging columns always increment fxid.
                // Could it be more adaptive, avoiding increment
                // if it is not necessary?
    }

    fxTable[fxOrig] = fx;

    // Insert the passed fx in the xsheet
    TOutputFx *outFx = dynamic_cast<TOutputFx *>(fx);
    if (!outFx)
      xsh->getFxDag()->getInternalFxs()->addFx(fx);
    else
      xsh->getFxDag()->addOutputFx(outFx);

    if (m_terminalFxs.count(fxOrig) > 0)
      xsh->getFxDag()->getTerminalFxs()->addFx(fx);

    if (!doClone) {
      // Err.... don't remember. Inquire further? :|
      int fxTypeCount = xsh->getFxDag()->getFxTypeCount(fx);

      int maxFxTypeId = std::max(fxTypeCount, fx->getAttributes()->getId());
      xsh->getFxDag()->updateFxTypeTable(fx, maxFxTypeId);
      xsh->getFxDag()->updateFxIdTable(fx);
    }

    bool isLinked = (fxOrig->getLinkedFx() != fxOrig);
    if (isLinked) {
      if (m_fxs.find(fxOrig->getLinkedFx()) == m_fxs.end())
        fx->linkParams(fxOrig->getLinkedFx());
      else {
        TFx *linkedFx, *oldLinkedFx = fxOrig->getLinkedFx();
        if (doClone) {
          // Clone the linked fx too
          linkedFx = fx->clone(false);
          linkedFx->linkParams(fx);

          linkedFx->setName(oldLinkedFx->getName());
          linkedFx->getAttributes()->setId(
              oldLinkedFx->getAttributes()->getId());
          linkedFx->getAttributes()->passiveCacheDataIdx() = -1;

          if (resetFxDagPositions)
            linkedFx->getAttributes()->setDagNodePos(TConst::nowhere);
          else
            linkedFx->getAttributes()->setDagNodePos(
                oldLinkedFx->getAttributes()->getDagNodePos());

          xsh->getFxDag()->assignUniqueId(linkedFx);
        } else
          linkedFx = oldLinkedFx;

        fxTable[oldLinkedFx] = linkedFx;

        xsh->getFxDag()->getInternalFxs()->addFx(linkedFx);
        if (m_terminalFxs.count(oldLinkedFx) > 0)
          xsh->getFxDag()->getTerminalFxs()->addFx(linkedFx);

        if (!doClone) {
          int fxTypeCount = xsh->getFxDag()->getFxTypeCount(linkedFx);
          int maxFxTypeId =
              std::max(fxTypeCount, linkedFx->getAttributes()->getId());
          xsh->getFxDag()->updateFxTypeTable(linkedFx, maxFxTypeId);
          xsh->getFxDag()->updateFxIdTable(linkedFx);
        }
      }
    }
  }

  // Update the link, like in functions above
  if (!fxTable.empty() && doClone) updateFxLinks(fxTable);

  // Paste any associated spline (not stored im m_splines)
  std::map<TStageObjectSpline *, TStageObjectSpline *> splines;
  for (i = 0; i < (int)restoredIds.size(); ++i) {
    TStageObjectId id = restoredIds[i];
    TStageObject *obj = xsh->getStageObject(id);

    TStageObjectSpline *spline = obj->getSpline();
    if (!spline) continue;

    TStageObjectTree *objTree = xsh->getStageObjectTree();
    if (objTree->containsSpline(
            spline))  // No need to add it if it's already there
      continue;

    std::map<TStageObjectSpline *, TStageObjectSpline *>::iterator it =
        splines.find(spline);
    if (it != splines.end()) {
      // Seems that multiple objects can have the same spline...
      // BTW, shouldn't this case stop at the continue before ?
      obj->setSpline(it->second);
      continue;
    }

    // The spline was not found. Clone and add it to the xsheet
    TStageObjectSpline *newSpline = spline->clone();  // Not checking doClone ?
    objTree->assignUniqueSplineId(newSpline);
    objTree->insertSpline(newSpline);
    obj->setSpline(newSpline);

    splines[spline] = newSpline;
  }

  // paste stored path
  QList<TSplineDataElement *>::const_iterator splinIt;
  for (splinIt = m_splines.begin(); splinIt != m_splines.end(); ++splinIt) {
    TStageObjectTree *objTree    = xsh->getStageObjectTree();
    TSplineDataElement *splineEl = *splinIt;
    TStageObjectSpline *spline   = splineEl->restoreSpline(fxFlags);
    if (doClone) objTree->assignUniqueSplineId(spline);
    objTree->insertSpline(spline);
    restoredSpline.push_back(spline->getId());
  }

  xsh->updateFrameCount();

  if (pos != TConst::nowhere) {
    // Update objects positions depending on the externally supplied pos

    TPointD middlePos;
    int count = 0;

    for (i = 0; i < (int)restoredIds.size(); ++i) {
      TStageObjectId id = restoredIds[i];
      TStageObject *obj = xsh->getStageObject(id);

      TPointD oldPos = obj->getDagNodePos();
      if (oldPos == TConst::nowhere) continue;

      middlePos += oldPos;
      ++count;
    }

    middlePos      = TPointD(middlePos.x / count, middlePos.y / count);
    TPointD offset = pos - middlePos;

    for (i = 0; i < (int)restoredIds.size(); ++i) {
      TStageObjectId id = restoredIds[i];
      TStageObject *obj = xsh->getStageObject(id);

      TPointD oldPos = obj->getDagNodePos();
      if (oldPos == TConst::nowhere) continue;

      obj->setDagNodePos(oldPos + offset);
    }
  }

  return restoredIds;
}
