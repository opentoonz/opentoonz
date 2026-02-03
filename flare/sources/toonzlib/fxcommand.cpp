

#include "toonz/fxcommand.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/tcolumnfx.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txshzeraryfxcolumn.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "historytypes.h"

// TnzBase includes
#include "tparamcontainer.h"
#include "tparamset.h"
#include "tfxattributes.h"
#include "tmacrofx.h"
#include "tpassivecachemanager.h"

// TnzCore includes
#include "tundo.h"
#include "tconst.h"

// Qt includes
#include <QMap>

// tcg includes
#include "tcg/tcg_macros.h"
#include "tcg/tcg_base.h"

#include <memory>

/*
  Toonz currently has THREE different APIs to deal with scene objects commands:

    1. From the xsheet, see columncommand.cpp
    2. From the stage schematic
    3. From the fx schematic

  *This* is the one version that should 'rule them all' - we still have to
  unify them, though.

TODO:

  - Associated Stage Object copies when copying columns
  - Stage schematic currently ignored (eg delete columns undo)
  - Double-check macro fxs behavior
  - Double-check group behavior
  - Enforce dynamic link groups consistency
*/

//**********************************************************************
//    Local Namespace  stuff
//**********************************************************************

namespace {

//======================================================

inline TFx *getActualIn(TFx *fx) {
  TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx);
  return zcfx ? (assert(zcfx->getZeraryFx()), zcfx->getZeraryFx()) : fx;
}

//------------------------------------------------------

inline TFx *getActualOut(TFx *fx) {
  TZeraryFx *zfx = dynamic_cast<TZeraryFx *>(fx);
  return (zfx && zfx->getColumnFx()) ? zfx->getColumnFx() : fx;
}

//------------------------------------------------------

inline int inputPortIndex(TFx *fx, TFxPort *port) {
  int p, pCount = fx->getInputPortCount();
  for (p = 0; p != pCount; ++p)
    if (fx->getInputPort(p) == port) break;
  return p;
}

//------------------------------------------------------

/*!
  Returns whether the specified fx is internal to a macro fx. Fxs
  inside a macro should not be affected by most editing commands - the
  macro is required to be exploded first.
*/
bool isInsideAMacroFx(TFx *fx, TXsheet *xsh) {
  if (!fx) return false;

  TColumnFx *cfx = dynamic_cast<TColumnFx *>(fx);
  TXsheetFx *xfx = dynamic_cast<TXsheetFx *>(fx);
  TOutputFx *ofx = dynamic_cast<TOutputFx *>(fx);

  return !cfx && !xfx && !ofx &&
         !(xsh->getFxDag()->getInternalFxs()->containsFx(fx));
}

//------------------------------------------------------

template <typename ParamCont>
void setParamsToCurrentScene(TXsheet *xsh, const ParamCont *cont) {
  for (int p = 0; p != cont->getParamCount(); ++p) {
    TParam &param = *cont->getParam(p);

    if (TDoubleParam *dp = dynamic_cast<TDoubleParam *>(&param))
      xsh->getStageObjectTree()->setGrammar(dp);
    else if (TParamSet *paramSet = dynamic_cast<TParamSet *>(&param))
      setParamsToCurrentScene(xsh, paramSet);
  }
}

//------------------------------------------------------

inline void setFxParamToCurrentScene(TFx *fx, TXsheet *xsh) {
  setParamsToCurrentScene(xsh, fx->getParams());
}

//------------------------------------------------------

void initializeFx(TXsheet *xsh, TFx *fx) {
  if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx))
    fx = zcfx->getZeraryFx();
  // if the fx has not unique name then let assignUniqueId() set the default
  // name
  if (fx->getName() != L"" && fx->getName() == fx->getFxId()) fx->setName(L"");

  xsh->getFxDag()->assignUniqueId(fx);
  setFxParamToCurrentScene(fx, xsh);
}

//------------------------------------------------------

void showFx(TXsheet *xsh, TFx *fx) {
  fx->getAttributes()->setIsOpened(xsh->getFxDag()->getDagGridDimension() == 0);

  if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx))
    fx = zcfx->getZeraryFx();
  fx->getAttributes()->passiveCacheDataIdx() = -1;
}

//------------------------------------------------------

void hideFx(TXsheet *xsh, TFx *fx) {
  if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx))
    fx = zcfx->getZeraryFx();
  TPassiveCacheManager::instance()->disableCache(fx);
}

//------------------------------------------------------

void addFxToCurrentScene(TFx *fx, TXsheet *xsh, bool isNewFx = true) {
  if (isNewFx) initializeFx(xsh, fx);

  xsh->getFxDag()->getInternalFxs()->addFx(fx);

  showFx(xsh, fx);
}

//------------------------------------------------------

void removeFxFromCurrentScene(TFx *fx, TXsheet *xsh) {
  xsh->getFxDag()->getInternalFxs()->removeFx(fx);
  xsh->getFxDag()->getTerminalFxs()->removeFx(fx);

  hideFx(xsh, fx);
}

}  // namespace

//**********************************************************************
//    Filter Functors  definition
//**********************************************************************

namespace {

struct FilterInsideAMacro {
  TXsheet *m_xsh;
  inline bool operator()(const TFxP &fx) {
    return ::isInsideAMacroFx(fx.getPointer(), m_xsh);
  }

  inline bool operator()(const TFxCommand::Link &link) {
    return ::isInsideAMacroFx(link.m_inputFx.getPointer(), m_xsh) ||
           ::isInsideAMacroFx(link.m_outputFx.getPointer(), m_xsh);
  }
};

struct FilterNonTerminalFxs {
  TXsheet *xsh;
  inline bool operator()(const TFxP &fx) {
    return !xsh->getFxDag()->getTerminalFxs()->containsFx(fx.getPointer());
  }
};

struct FilterTerminalFxs {
  TXsheet *xsh;
  inline bool operator()(const TFxP &fx) {
    return xsh->getFxDag()->getTerminalFxs()->containsFx(fx.getPointer());
  }
};

struct FilterColumnFxs {
  inline bool operator()(const TFxP &fx) {
    return dynamic_cast<TLevelColumnFx *>(fx.getPointer());
  }
};

}  // namespace

//**********************************************************************
//    CloneFxFunctor  definition
//**********************************************************************

namespace {

struct CloneFxFunctor {
  TFxP m_src;
  bool m_ownsSrc;
  TFx *operator()() {
    if (m_ownsSrc)
      m_ownsSrc = false;  // Transfer m_src and ownership if it
    else                  // was surrendered
    {
      assert(m_src->getRefCount() >
             1);  // We'll be linking params to the cloned
                  // fx - so it MUST NOT be destroyed on release
      TFx *src = m_src.getPointer();
      m_src    = m_src->clone(false);  // Duplicate and link parameters all
      m_src->linkParams(src);          // the following times
    }

    return m_src.getPointer();
  }
};

}  // namespace

//**********************************************************************
//    FxCommandUndo  definition
//**********************************************************************

class FxCommandUndo : public TUndo {
public:
  virtual ~FxCommandUndo() {}

  virtual bool isConsistent() const = 0;

public:
  template <typename Pred>
  static TFx *leftmostConnectedFx(TFx *fx, Pred pred);
  template <typename Pred>
  static TFx *rightmostConnectedFx(TFx *fx, Pred pred);

  static TFx *leftmostConnectedFx(TFx *fx);
  static TFx *rightmostConnectedFx(TFx *fx);

  static std::vector<TFxCommand::Link> inputLinks(TXsheet *xsh, TFx *fx);
  static std::vector<TFxCommand::Link> outputLinks(TXsheet *xsh, TFx *fx);

  int getHistoryType() override { return HistoryType::Schematic; }

protected:
  static TXshZeraryFxColumn *createZeraryFxColumn(TXsheet *xsh, TFx *zfx,
                                                  int row = 0);
  static void cloneGroupStack(const QStack<int> &groupIds,
                              const QStack<std::wstring> &groupNames,
                              TFx *toFx);
  static void cloneGroupStack(TFx *fromFx, TFx *toFx);
  static void copyGroupEditLevel(int editGroupId, TFx *toFx);
  static void copyGroupEditLevel(TFx *fromFx, TFx *toFx);
  static void copyDagPosition(TFx *fromFx, TFx *toFx);
  static void attach(TXsheet *xsh, TFx *inputFx, TFx *outputFx, int port,
                     bool copyGroupData);
  static void attach(TXsheet *xsh, const TFxCommand::Link &link,
                     bool copyGroupData);
  static void attachOutputs(TXsheet *xsh, TFx *insertedFx, TFx *inputFx);
  static void detachFxs(TXsheet *xsh, TFx *fxLeft, TFx *fxRight,
                        bool detachLeft = true);
  static void insertFxs(TXsheet *xsh, const TFxCommand::Link &link, TFx *fxLeft,
                        TFx *fxRight);
  static void insertColumn(TXsheet *xsh, TXshColumn *column, int colIdx,
                           bool removeHole = false, bool autoTerminal = false);
  static void removeFxOrColumn(TXsheet *xsh, TFx *fx, int colIdx,
                               bool insertHole   = false,
                               bool unlinkParams = true);
  static void linkParams(TFx *fx, TFx *linkedFx);
  static void unlinkParams(TFx *fx);
  static void makeNotCurrent(TFxHandle *fxHandle, TFx *fx);

private:
  static void removeColumn(TXsheet *xsh, int colIdx, bool insertHole);
  static void removeNormalFx(TXsheet *xsh, TFx *fx);
  static void removeOutputFx(TXsheet *xsh, TOutputFx *outputFx);
};

//------------------------------------------------------

TXshZeraryFxColumn *FxCommandUndo::createZeraryFxColumn(TXsheet *xsh, TFx *zfx,
                                                        int row) {
  int frameCount = xsh->getScene()->getFrameCount() - row;

  TXshZeraryFxColumn *column =
      new TXshZeraryFxColumn(frameCount > 0 ? frameCount : 100);
  column->getZeraryColumnFx()->setZeraryFx(zfx);
  column->insertEmptyCells(0, row);

  return column;
}

//------------------------------------------------------

void FxCommandUndo::cloneGroupStack(const QStack<int> &groupIds,
                                    const QStack<std::wstring> &groupNames,
                                    TFx *toFx) {
  toFx->getAttributes()->removeFromAllGroup();

  for (int i = 0; i < groupIds.size(); ++i) {
    toFx->getAttributes()->setGroupId(groupIds[i]);
    toFx->getAttributes()->setGroupName(groupNames[i]);
  }
}

//------------------------------------------------------

void FxCommandUndo::cloneGroupStack(TFx *fromFx, TFx *toFx) {
  if (fromFx->getAttributes()->isGrouped()) {
    cloneGroupStack(fromFx->getAttributes()->getGroupIdStack(),
                    fromFx->getAttributes()->getGroupNameStack(), toFx);
  }
}

//------------------------------------------------------

void FxCommandUndo::copyGroupEditLevel(int editGroupId, TFx *toFx) {
  toFx->getAttributes()->closeAllGroups();
  while (editGroupId != toFx->getAttributes()->getEditingGroupId() &&
         toFx->getAttributes()->editGroup())
    ;

  assert(editGroupId == toFx->getAttributes()->getEditingGroupId());
}

//------------------------------------------------------

void FxCommandUndo::copyGroupEditLevel(TFx *fromFx, TFx *toFx) {
  assert(toFx);
  if (fromFx && fromFx->getAttributes()->isGrouped())
    copyGroupEditLevel(fromFx->getAttributes()->getEditingGroupId(), toFx);
}

//------------------------------------------------------

void FxCommandUndo::copyDagPosition(TFx *fromFx, TFx *toFx) {
  assert(toFx);
  if (fromFx)
    toFx->getAttributes()->setDagNodePos(
        fromFx->getAttributes()->getDagNodePos());
}

//------------------------------------------------------

void FxCommandUndo::attach(TXsheet *xsh, TFx *inputFx, TFx *outputFx, int link,
                           bool copyGroupData) {
  if (outputFx) {
    FxDag *fxDag = xsh->getFxDag();

    inputFx  = ::getActualOut(inputFx);
    outputFx = ::getActualIn(outputFx);

    if (inputFx && link < 0) {
      assert(dynamic_cast<TXsheetFx *>(outputFx));
      fxDag->addToXsheet(inputFx);
    } else {
      int ipCount = outputFx->getInputPortCount();
      if (ipCount > 0 && link < ipCount)
        outputFx->getInputPort(link)->setFx(inputFx);

      if (copyGroupData) copyGroupEditLevel(inputFx, outputFx);
    }
  }
}

//------------------------------------------------------

void FxCommandUndo::attach(TXsheet *xsh, const TFxCommand::Link &link,
                           bool copyGroupData) {
  attach(xsh, link.m_inputFx.getPointer(), link.m_outputFx.getPointer(),
         link.m_index, copyGroupData);
}

//------------------------------------------------------

void FxCommandUndo::attachOutputs(TXsheet *xsh, TFx *insertedFx, TFx *inputFx) {
  TCG_ASSERT(inputFx, return);

  FxDag *fxDag = xsh->getFxDag();

  insertedFx = ::getActualOut(insertedFx);
  inputFx    = ::getActualOut(inputFx);

  int p, pCount = inputFx->getOutputConnectionCount();
  for (p = pCount - 1; p >= 0;
       --p)  // Backward iteration on output connections -
  {          // it's necessary since TFxPort::setFx() REMOVES
    TFxPort *port = inputFx->getOutputConnection(
        p);  // the corresponding port int the output connections
    port->setFx(
        insertedFx);  // container - thus, it's better to start from the end
  }

  if (fxDag->getTerminalFxs()->containsFx(inputFx)) {
    fxDag->removeFromXsheet(inputFx);
    fxDag->addToXsheet(insertedFx);
  }
}

//------------------------------------------------------

void FxCommandUndo::detachFxs(TXsheet *xsh, TFx *fxLeft, TFx *fxRight,
                              bool detachLeft) {
  assert(fxLeft && fxRight);

  fxLeft  = ::getActualIn(fxLeft);
  fxRight = ::getActualOut(fxRight);

  int ipCount = fxLeft->getInputPortCount();

  // Redirect input/output ports
  TFx *inputFx0 = (ipCount > 0) ? fxLeft->getInputPort(0)->getFx() : 0;

  int p, opCount = fxRight->getOutputConnectionCount();
  for (p = opCount - 1; p >= 0;
       --p)  // Backward iteration due to TFxPort::setFx()
  {
    TFxPort *outPort = fxRight->getOutputConnection(p);
    assert(outPort && outPort->getFx() == fxRight);

    outPort->setFx(inputFx0);
  }

  // Xsheet links redirection
  FxDag *fxDag = xsh->getFxDag();
  if (fxDag->getTerminalFxs()->containsFx(fxRight)) {
    fxDag->removeFromXsheet(fxRight);

    for (int p = 0; p != ipCount; ++p)
      if (TFx *inputFx = fxLeft->getInputPort(p)->getFx())
        fxDag->addToXsheet(inputFx);
  }

  if (detachLeft) fxLeft->disconnectAll();
}

//------------------------------------------------------

void FxCommandUndo::insertFxs(TXsheet *xsh, const TFxCommand::Link &link,
                              TFx *fxLeft, TFx *fxRight) {
  assert(fxLeft && fxRight);

  if (link.m_inputFx && link.m_outputFx) {
    FxCommandUndo::attach(xsh, link.m_inputFx.getPointer(), fxLeft, 0, false);
    FxCommandUndo::attach(xsh, fxRight, link.m_outputFx.getPointer(),
                          link.m_index, false);

    if (link.m_index < 0)
      xsh->getFxDag()->removeFromXsheet(
          ::getActualOut(link.m_inputFx.getPointer()));
  }
}

//------------------------------------------------------

void FxCommandUndo::insertColumn(TXsheet *xsh, TXshColumn *column, int col,
                                 bool removeHole, bool autoTerminal) {
  FxDag *fxDag  = xsh->getFxDag();
  TFx *fx       = column->getFx();
  bool terminal = false;

  if (fx) {
    ::showFx(xsh, fx);
    terminal = fxDag->getTerminalFxs()->containsFx(fx);
  }

  if (removeHole) xsh->removeColumn(col);

  xsh->insertColumn(col, column);  // Attaches the fx to the xsheet, too -
                                   // but not if the column is a palette one.
  if (!autoTerminal) {
    // Preserve the initial terminal state.
    // This lets fxs to be linked to the xsheet while still hidden.

    fxDag->removeFromXsheet(fx);
    if (terminal) fxDag->addToXsheet(fx);
  }

  xsh->updateFrameCount();
}

//------------------------------------------------------

void FxCommandUndo::removeColumn(TXsheet *xsh, int col, bool insertHole) {
  if (TFx *colFx = xsh->getColumn(col)->getFx()) {
    detachFxs(xsh, colFx, colFx);
    ::hideFx(xsh, colFx);
  }

  xsh->removeColumn(col);    // Already detaches any fx in output,
  if (insertHole)            // including the terminal case
    xsh->insertColumn(col);  // Note that fxs in output are not
                             // removed - just detached.
  xsh->updateFrameCount();
}

//------------------------------------------------------

void FxCommandUndo::removeNormalFx(TXsheet *xsh, TFx *fx) {
  detachFxs(xsh, fx, fx);
  ::removeFxFromCurrentScene(fx, xsh);  // Already hideFx()s
}

//------------------------------------------------------

void FxCommandUndo::removeOutputFx(TXsheet *xsh, TOutputFx *outputFx) {
  detachFxs(xsh, outputFx, outputFx);
  xsh->getFxDag()->removeOutputFx(outputFx);
}

//------------------------------------------------------

void FxCommandUndo::linkParams(TFx *fx, TFx *linkedFx) {
  if (linkedFx) ::getActualIn(fx)->linkParams(::getActualIn(linkedFx));
}

//------------------------------------------------------

void FxCommandUndo::unlinkParams(TFx *fx) {
  if (fx = ::getActualIn(fx), fx->getLinkedFx()) fx->unlinkParams();
}

//------------------------------------------------------

void FxCommandUndo::makeNotCurrent(TFxHandle *fxHandle, TFx *fx) {
  if (fx = ::getActualOut(fx), fx == fxHandle->getFx()) fxHandle->setFx(0);
}

//------------------------------------------------------

void FxCommandUndo::removeFxOrColumn(TXsheet *xsh, TFx *fx, int colIdx,
                                     bool insertHole, bool unlinkParams) {
  assert(fx || colIdx >= 0);

  if (!fx)
    fx = xsh->getColumn(colIdx)->getFx();
  else if (TColumnFx *colFx = dynamic_cast<TColumnFx *>(fx))
    colIdx = colFx->getColumnIndex();
  else if (TZeraryFx *zfx = dynamic_cast<TZeraryFx *>(fx)) {
    if (zfx->getColumnFx())
      fx     = zfx->getColumnFx(),
      colIdx = static_cast<TColumnFx *>(fx)->getColumnIndex();
  }

  if (fx) {
    // Discriminate special fx types
    if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx)) {
      // Removed as a column
      fx = zcfx->getZeraryFx();
    } else if (TOutputFx *outputFx = dynamic_cast<TOutputFx *>(fx)) {
      assert(xsh->getFxDag()->getOutputFxCount() > 1);
      FxCommandUndo::removeOutputFx(xsh, outputFx);
    } else if (colIdx < 0)
      FxCommandUndo::removeNormalFx(xsh, fx);

    if (unlinkParams) FxCommandUndo::unlinkParams(fx);
  }

  if (colIdx >= 0) FxCommandUndo::removeColumn(xsh, colIdx, insertHole);
}

//------------------------------------------------------

template <typename Pred>
TFx *FxCommandUndo::leftmostConnectedFx(TFx *fx, Pred pred) {
  assert(fx);

  fx = rightmostConnectedFx(fx, pred);  // The rightmost fx should be discovered
                                        // first, then, we'll descend from that
  do {
    fx = ::getActualIn(fx);

    if (!((fx->getInputPortCount() > 0) && fx->getInputPort(0)->isConnected() &&
          pred(fx->getInputPort(0)->getFx())))
      break;

    fx = fx->getInputPort(0)->getFx();
  } while (true);

  return fx;
}

//------------------------------------------------------

template <typename Pred>
TFx *FxCommandUndo::rightmostConnectedFx(TFx *fx, Pred pred) {
  assert(fx);

  do {
    fx = ::getActualOut(fx);

    if (!(fx->getOutputConnectionCount() > 0 &&
          pred(fx->getOutputConnection(0)->getOwnerFx())))
      break;

    fx = fx->getOutputConnection(0)->getOwnerFx();
  } while (true);

  return fx;
}

//------------------------------------------------------

namespace {
struct True_pred {
  bool operator()(TFx *fx) { return true; }
};
}  // namespace

TFx *FxCommandUndo::leftmostConnectedFx(TFx *fx) {
  return leftmostConnectedFx(fx, ::True_pred());
}

//------------------------------------------------------

TFx *FxCommandUndo::rightmostConnectedFx(TFx *fx) {
  return rightmostConnectedFx(fx, ::True_pred());
}

//------------------------------------------------------

std::vector<TFxCommand::Link> FxCommandUndo::inputLinks(TXsheet *xsh, TFx *fx) {
  std::vector<TFxCommand::Link> result;

  fx = ::getActualIn(fx);

  int il, ilCount = fx->getInputPortCount();
  for (il = 0; il != ilCount; ++il) {
    TFxPort *port = fx->getInputPort(il);

    assert(port);
    if (port->isConnected())
      result.push_back(TFxCommand::Link(port->getFx(), fx, il));
  }

  return result;
}

//------------------------------------------------------

std::vector<TFxCommand::Link> FxCommandUndo::outputLinks(TXsheet *xsh,
                                                         TFx *fx) {
  std::vector<TFxCommand::Link> result;

  fx = ::getActualOut(fx);

  int ol, olCount = fx->getOutputConnectionCount();
  for (ol = 0; ol != olCount; ++ol) {
    TFxPort *port = fx->getOutputConnection(ol);
    TFx *ownerFx  = port->getOwnerFx();
    int portIndex = ::inputPortIndex(ownerFx, port);

    result.push_back(TFxCommand::Link(fx, ownerFx, portIndex));
  }

  FxDag *fxDag = xsh->getFxDag();
  if (fxDag->getTerminalFxs()->containsFx(fx))
    result.push_back(TFxCommand::Link(fx, fxDag->getXsheetFx(), -1));

  return result;
}

//**********************************************************************
//    Insert Fx  command
//**********************************************************************

class InsertFxUndo final : public FxCommandUndo {
  QList<TFxP> m_selectedFxs;
  QList<TFxCommand::Link> m_selectedLinks;

  TApplication *m_app;

  QList<TFxP> m_insertedFxs;
  TXshZeraryFxColumnP m_insertedColumn;
  int m_colIdx;
  bool m_columnReplacesHole;
  bool m_attachOutputs;

public:
  InsertFxUndo(const TFxP &fx, int row, int col, const QList<TFxP> &selectedFxs,
               QList<TFxCommand::Link> selectedLinks, TApplication *app,
               bool attachOutputs = true)
      : m_selectedFxs(selectedFxs)
      , m_selectedLinks(selectedLinks)
      , m_insertedColumn(0)
      , m_app(app)
      , m_colIdx(col)
      , m_columnReplacesHole(false)
      , m_attachOutputs(attachOutputs) {
    initialize(fx, row, col);
  }

  bool isConsistent() const override { return !m_insertedFxs.isEmpty(); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override;

private:
  void initialize(const TFxP &newFx, int row, int col);
};

//------------------------------------------------------

inline bool has_fx_column(TFx *fx) {
  if (TPluginInterface *plgif = dynamic_cast<TPluginInterface *>(fx))
    return plgif->isPluginZerary();
  else if (TZeraryFx *zfx = dynamic_cast<TZeraryFx *>(fx))
    return zfx->isZerary();
  return false;
}

namespace {

bool containsInputFx(const QList<TFxP> &fxs, const TFxCommand::Link &link) {
  return fxs.contains(link.m_inputFx);
}

}  // namespace

void InsertFxUndo::initialize(const TFxP &newFx, int row, int col) {
  struct Locals {
    InsertFxUndo *m_this;
    inline void storeFx(TXsheet *xsh, TFx *fx) {
      ::initializeFx(xsh, fx);
      m_this->m_insertedFxs.push_back(fx);
    }

  } locals = {this};

  TXsheet *xsh = m_app->getCurrentXsheet()->getXsheet();
  TFx *fx      = newFx.getPointer();

  assert(!dynamic_cast<TZeraryColumnFx *>(fx));

  TZeraryFx *zfx = dynamic_cast<TZeraryFx *>(fx);
  if (has_fx_column(fx)) {
    m_insertedColumn = InsertFxUndo::createZeraryFxColumn(xsh, fx, row);

    locals.storeFx(xsh, fx);

    if (xsh->getColumn(col) && xsh->getColumn(col)->isEmpty())
      m_columnReplacesHole = true;
  } else {
    if (m_selectedFxs.isEmpty() && m_selectedLinks.isEmpty()) {
      // Attempt retrieval of current Fx from the fxHandle
      if (TFx *currentFx = m_app->getCurrentFx()->getFx())
        m_selectedFxs.push_back(currentFx);
      else {
        // Isolated case
        locals.storeFx(xsh, fx);
        return;
      }
    }

    // Remove all unacceptable input fxs
    ::FilterInsideAMacro filterInMacroFxs = {xsh};
    m_selectedFxs.erase(std::remove_if(m_selectedFxs.begin(),
                                       m_selectedFxs.end(), filterInMacroFxs),
                        m_selectedFxs.end());

    // Remove all unacceptable links or links whose input fx was already
    // selected
    m_selectedLinks.erase(
        std::remove_if(m_selectedLinks.begin(), m_selectedLinks.end(),
                       filterInMacroFxs),
        m_selectedLinks.end());
    m_selectedLinks.erase(
        std::remove_if(m_selectedLinks.begin(), m_selectedLinks.end(),
                       [this](const TFxCommand::Link &link) {
                         return containsInputFx(m_selectedFxs, link);
                       }),
        m_selectedLinks.end());

    // Build an fx for each of the specified inputs
    ::CloneFxFunctor cloneFx = {fx, true};

    int f, fCount = m_selectedFxs.size();
    for (f = 0; f != fCount; ++f) {
      TFx *fx = cloneFx();
      FxCommandUndo::cloneGroupStack(m_selectedFxs[f].getPointer(), fx);
      locals.storeFx(xsh, fx);
    }

    fCount = m_selectedLinks.size();
    for (f = 0; f != fCount; ++f) {
      TFx *fx = cloneFx();
      FxCommandUndo::cloneGroupStack(m_selectedLinks[f].m_inputFx.getPointer(),
                                     fx);
      locals.storeFx(xsh, fx);
    }
  }
}

//------------------------------------------------------

void InsertFxUndo::redo() const {
  struct OnExit {
    const InsertFxUndo *m_this;
    ~OnExit() {
      m_this->m_app->getCurrentFx()->setFx(
          m_this->m_insertedFxs.back().getPointer());
      m_this->m_app->getCurrentXsheet()->notifyXsheetChanged();
      m_this->m_app->getCurrentScene()->setDirtyFlag(true);
    }
  } onExit = {this};

  TXsheet *xsh = m_app->getCurrentXsheet()->getXsheet();

  // Zerary case
  if (m_insertedColumn) {
    FxCommandUndo::insertColumn(xsh, m_insertedColumn.getPointer(), m_colIdx,
                                m_columnReplacesHole, true);
    return;
  }

  // Isolated Fx case
  if (m_selectedLinks.isEmpty() && m_selectedFxs.isEmpty()) {
    assert(m_insertedFxs.size() == 1);
    ::addFxToCurrentScene(m_insertedFxs.back().getPointer(), xsh,
                          false);  // Already showFx()s
  } else {
    // Selected links
    int i;
    for (i = 0; i < m_selectedLinks.size(); ++i) {
      const TFxCommand::Link &link = m_selectedLinks[i];
      TFx *insertedFx              = m_insertedFxs[i].getPointer();

      ::addFxToCurrentScene(insertedFx, xsh, false);
      FxCommandUndo::insertFxs(xsh, link, insertedFx, insertedFx);
      FxCommandUndo::copyGroupEditLevel(link.m_inputFx.getPointer(),
                                        insertedFx);
    }

    // Selected fxs
    int j, t;
    for (j = 0, t = 0; j < m_selectedFxs.size(); j++) {
      TFx *fx = m_selectedFxs[j].getPointer();
      assert(fx);

      TFx *insertedFx = m_insertedFxs[i + t].getPointer();
      t++;

      assert(insertedFx);
      ::addFxToCurrentScene(insertedFx, xsh, false);

      if (m_attachOutputs) FxCommandUndo::attachOutputs(xsh, insertedFx, fx);

      FxCommandUndo::attach(xsh, fx, insertedFx, 0, true);
    }
  }
}

//------------------------------------------------------

void InsertFxUndo::undo() const {
  TXsheet *xsh = m_app->getCurrentXsheet()->getXsheet();

  int i, iCount = m_insertedFxs.size();
  for (i = 0; i != iCount; ++i) {
    TFx *insertedFx = m_insertedFxs[i].getPointer();

    FxCommandUndo::removeFxOrColumn(xsh, insertedFx, -1, m_columnReplacesHole,
                                    false);  // Skip parameter links removal
    FxCommandUndo::makeNotCurrent(m_app->getCurrentFx(), insertedFx);
  }

  m_app->getCurrentFx()->setFx(0);
  m_app->getCurrentXsheet()->notifyXsheetChanged();
  m_app->getCurrentScene()->setDirtyFlag(true);
}

//------------------------------------------------------

QString InsertFxUndo::getHistoryString() {
  QString str = (m_selectedLinks.isEmpty()) ? QObject::tr("Add Fx  : ")
                                            : QObject::tr("Insert Fx  : ");
  QList<TFxP>::iterator it;
  for (it = m_insertedFxs.begin(); it != m_insertedFxs.end(); it++) {
    if (it != m_insertedFxs.begin()) str += QString(", ");

    str += QString::fromStdWString((*it)->getFxId());
  }
  return str;
}

//=============================================================

void TFxCommand::insertFx(TFx *newFx, const QList<TFxP> &fxs,
                          const QList<Link> &links, TApplication *app, int col,
                          int row) {
  if (!newFx) return;

  if (col < 0)
    col = 0;  // Normally insert before. In case of camera, insert after

  std::unique_ptr<FxCommandUndo> undo(
      new InsertFxUndo(newFx, row, col, fxs, links, app));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Add Fx  command
//**********************************************************************

void TFxCommand::addFx(TFx *newFx, const QList<TFxP> &fxs, TApplication *app,
                       int col, int row) {
  if (!newFx) return;

  std::unique_ptr<FxCommandUndo> undo(
      new InsertFxUndo(newFx, row, col, fxs, QList<Link>(), app, false));
  if (!undo->isConsistent()) return;

  undo->redo();
  TUndoManager::manager()->add(undo.release());
}

//**********************************************************************
//    Duplicate Fx  command
//**********************************************************************

class DuplicateFxUndo final : public FxCommandUndo {
  TFxP m_fx, m_dupFx;
  TXshColumnP m_column;
  int m_colIdx;

  TXsheetHandle *m_xshHandle;
  TFxHandle *m_fxHandle;

public:
  DuplicateFxUndo(const TFxP &originalFx, TXsheetHandle *xshHandle,
                  TFxHandle *fxHandle)
      : m_fx(originalFx)
      , m_colIdx(-1)
      , m_xshHandle(xshHandle)
      , m_fxHandle(fxHandle) {
    initialize();
  }

  bool isConsistent() const override { return bool(m_dupFx); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override;

private:
  void initialize();
};

//-------------------------------------------------------------

void DuplicateFxUndo::initialize() {
  TXsheet *xsh = m_xshHandle->getXsheet();
  TFx *fx      = m_fx.getPointer();

  fx = ::getActualOut(fx);

  if (isInsideAMacroFx(fx, xsh) || dynamic_cast<TXsheetFx *>(fx) ||
      dynamic_cast<TOutputFx *>(fx) ||
      (dynamic_cast<TColumnFx *>(fx) && !dynamic_cast<TZeraryColumnFx *>(fx)))
    return;

  if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx)) {
    m_column = new TXshZeraryFxColumn(*zcfx->getColumn());
    m_colIdx = xsh->getFirstFreeColumnIndex();

    TZeraryColumnFx *dupZcfx =
        static_cast<TZeraryColumnFx *>(m_column->getFx());
    ::initializeFx(xsh, dupZcfx->getZeraryFx());

    FxCommandUndo::cloneGroupStack(zcfx, dupZcfx);

    m_dupFx = dupZcfx;
  } else {
    fx = fx->clone(false);
    ::initializeFx(xsh, fx);

    FxCommandUndo::cloneGroupStack(m_fx.getPointer(), fx);

    m_dupFx = fx;
  }

  // place duplicated nodes lower-right position from the original ones
  if (fx->getAttributes()->getDagNodePos() != TConst::nowhere) {
    TPointD dupFxPos = fx->getAttributes()->getDagNodePos() + TPointD(50, 50);
    m_dupFx->getAttributes()->setDagNodePos(dupFxPos);
  }
}

//-------------------------------------------------------------

void DuplicateFxUndo::redo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  if (m_column) {
    // Zerary Fx case
    TZeraryColumnFx *zcfx = static_cast<TZeraryColumnFx *>(m_fx.getPointer());
    TZeraryColumnFx *dupZcfx =
        static_cast<TZeraryColumnFx *>(m_dupFx.getPointer());

    FxCommandUndo::insertColumn(xsh, m_column.getPointer(), m_colIdx, true,
                                true);
    FxCommandUndo::copyGroupEditLevel(zcfx, dupZcfx);

    dupZcfx->getZeraryFx()->linkParams(zcfx->getZeraryFx());
  } else {
    // Normal Fx case
    addFxToCurrentScene(m_dupFx.getPointer(), m_xshHandle->getXsheet(), false);
    FxCommandUndo::copyGroupEditLevel(m_fx.getPointer(), m_dupFx.getPointer());

    m_dupFx->linkParams(m_fx.getPointer());
  }

  m_fxHandle->setFx(m_dupFx.getPointer());
  m_xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------------

void DuplicateFxUndo::undo() const {
  FxCommandUndo::removeFxOrColumn(m_xshHandle->getXsheet(),
                                  m_dupFx.getPointer(), -1, true, true);

  m_fxHandle->setFx(0);
  m_xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------------

QString DuplicateFxUndo::getHistoryString() {
  if (TZeraryColumnFx *zDup =
          dynamic_cast<TZeraryColumnFx *>(m_dupFx.getPointer()))
    return QObject::tr("Create Linked Fx  : %1")
        .arg(QString::fromStdWString(zDup->getZeraryFx()->getFxId()));

  return QObject::tr("Create Linked Fx  : %1")
      .arg(QString::fromStdWString(m_dupFx->getFxId()));
}

//=============================================================

void TFxCommand::duplicateFx(TFx *src, TXsheetHandle *xshHandle,
                             TFxHandle *fxHandle) {
  std::unique_ptr<FxCommandUndo> undo(
      new DuplicateFxUndo(src, xshHandle, fxHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Replace Fx  command
//**********************************************************************

class ReplaceFxUndo final : public FxCommandUndo {
  TFxP m_fx, m_repFx, m_linkedFx;
  TXshColumnP m_column, m_repColumn;
  int m_colIdx, m_repColIdx;

  std::vector<std::pair<int, TFx *>> m_inputLinks;

  TXsheetHandle *m_xshHandle;
  TFxHandle *m_fxHandle;

public:
  ReplaceFxUndo(const TFxP &replacementFx, const TFxP &replacedFx,
                TXsheetHandle *xshHandle, TFxHandle *fxHandle)
      : m_fx(replacedFx)
      , m_repFx(replacementFx)
      , m_xshHandle(xshHandle)
      , m_fxHandle(fxHandle)
      , m_colIdx(-1)
      , m_repColIdx(-1) {
    initialize();
  }

  bool isConsistent() const override { return bool(m_repFx); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override;

private:
  void initialize();
  static void replace(TXsheet *xsh, TFx *fx, TFx *repFx, TXshColumn *column,
                      TXshColumn *repColumn, int colIdx, int repColIdx);
};

//-------------------------------------------------------------

void ReplaceFxUndo::initialize() {
  TXsheet *xsh = m_xshHandle->getXsheet();

  TFx *fx    = m_fx.getPointer();
  TFx *repFx = m_repFx.getPointer();

  fx = ::getActualOut(fx);

  if (isInsideAMacroFx(fx, xsh) || dynamic_cast<TXsheetFx *>(fx) ||
      dynamic_cast<TOutputFx *>(fx) ||
      (dynamic_cast<TColumnFx *>(fx) && !dynamic_cast<TZeraryColumnFx *>(fx))) {
    m_repFx = TFxP();
    return;
  }

  if (dynamic_cast<TXsheetFx *>(repFx) || dynamic_cast<TOutputFx *>(repFx) ||
      dynamic_cast<TColumnFx *>(repFx)) {
    m_repFx = TFxP();
    return;
  }

  ::initializeFx(xsh, repFx);

  TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(fx);
  if (zcfx) {
    TXshZeraryFxColumn *zfColumn = zcfx->getColumn();

    m_column = zfColumn;
    m_colIdx = zfColumn->getIndex();

    fx = zcfx->getZeraryFx();
  }

  TZeraryColumnFx *zcrepfx = dynamic_cast<TZeraryColumnFx *>(repFx);
  if (zcrepfx) repFx = zcrepfx->getZeraryFx();

  bool fxHasCol    = has_fx_column(fx);
  bool repfxHasCol = has_fx_column(repFx);

  if (fxHasCol && repfxHasCol) {
    if (zcfx) {
      // Build a column with the same source cells pattern
      m_repColumn = new TXshZeraryFxColumn(*zcfx->getColumn());
      m_repColIdx = m_colIdx;

      // Substitute the column's zerary fx with the substitute one
      TZeraryColumnFx *repZcfx =
          static_cast<TZeraryColumnFx *>(m_repColumn->getFx());
      repZcfx->setZeraryFx(repFx);

      FxCommandUndo::cloneGroupStack(zcfx, repZcfx);
      m_repFx = repZcfx;
    } else {
      m_repColumn = FxCommandUndo::createZeraryFxColumn(xsh, repFx);
      m_repColIdx = xsh->getFirstFreeColumnIndex();
      m_repFx     = static_cast<TZeraryColumnFx *>(m_repColumn->getFx());
    }
  } else if (!fxHasCol && repfxHasCol) {
    m_repColumn = FxCommandUndo::createZeraryFxColumn(xsh, repFx);
    m_repColIdx = xsh->getFirstFreeColumnIndex();
    m_repFx     = static_cast<TZeraryColumnFx *>(m_repColumn->getFx());
  }

  FxCommandUndo::cloneGroupStack(fx, m_repFx.getPointer());

  // Store fx's input links (depending on m_repFx, they could be not matched
  // in the replacement)
  int p, ipCount = fx->getInputPortCount();
  for (p = 0; p != ipCount; ++p) {
    TFxPort *port = fx->getInputPort(p);
    if (TFx *inputFx = port->getFx())
      m_inputLinks.push_back(std::make_pair(p, inputFx));
  }

  // Store the fx's linked fx
  m_linkedFx = fx->getLinkedFx();
}

//-------------------------------------------------------------

void ReplaceFxUndo::replace(TXsheet *xsh, TFx *fx, TFx *repFx,
                            TXshColumn *column, TXshColumn *repColumn,
                            int colIdx, int repColIdx) {
  FxDag *fxDag = xsh->getFxDag();

  TZeraryColumnFx *zcfx = column ? static_cast<TZeraryColumnFx *>(fx) : 0;
  TZeraryColumnFx *repZcfx =
      repColumn ? static_cast<TZeraryColumnFx *>(repFx) : 0;

  TFx *ifx    = zcfx ? zcfx->getZeraryFx() : fx;
  TFx *irepFx = repZcfx ? repZcfx->getZeraryFx() : repFx;

  // Copy links first
  int p, ipCount = ifx->getInputPortCount(),
         ripCount = irepFx->getInputPortCount();
  for (p = 0; p != ipCount && p != ripCount; ++p) {
    TFxPort *ifxPort    = ifx->getInputPort(p);
    TFxPort *irepFxPort = irepFx->getInputPort(p);

    FxCommandUndo::attach(xsh, ifxPort->getFx(), irepFx, p, true);
  }

  int opCount = fx->getOutputConnectionCount();
  for (p = opCount - 1; p >= 0; --p) {
    TFxPort *port = fx->getOutputConnection(p);
    port->setFx(repFx);
  }

  if (fxDag->getTerminalFxs()->containsFx(fx)) {
    fxDag->removeFromXsheet(fx);
    fxDag->addToXsheet(repFx);
  }

  // Remove fx/column
  FxCommandUndo::removeFxOrColumn(xsh, fx, colIdx, bool(repColumn), false);

  // Insert the new fx/column
  if (repColumn)
    FxCommandUndo::insertColumn(xsh, repColumn, repColIdx,
                                column);  // Not attached to the xsheet
  else
    ::addFxToCurrentScene(repFx, xsh, false);

  FxCommandUndo::copyGroupEditLevel(fx, repFx);
  FxCommandUndo::copyDagPosition(fx, repFx);
}

//-------------------------------------------------------------

void ReplaceFxUndo::redo() const {
  ReplaceFxUndo::replace(m_xshHandle->getXsheet(), m_fx.getPointer(),
                         m_repFx.getPointer(), m_column.getPointer(),
                         m_repColumn.getPointer(), m_colIdx, m_repColIdx);
  FxCommandUndo::unlinkParams(m_fx.getPointer());

  m_fxHandle->setFx(0);
  m_xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------------

void ReplaceFxUndo::undo() const {
  ReplaceFxUndo::replace(m_xshHandle->getXsheet(), m_repFx.getPointer(),
                         m_fx.getPointer(), m_repColumn.getPointer(),
                         m_column.getPointer(), m_repColIdx, m_colIdx);

  // Repair original input links for m_fx
  m_fx->disconnectAll();

  size_t l, lCount = m_inputLinks.size();
  for (l = 0; l != lCount; ++l)
    m_fx->getInputPort(m_inputLinks[l].first)->setFx(m_inputLinks[l].second);

  // Repair parameter links
  FxCommandUndo::linkParams(m_fx.getPointer(), m_linkedFx.getPointer());

  m_fxHandle->setFx(0);
  m_xshHandle->notifyXsheetChanged();
}

//-------------------------------------------------------------

QString ReplaceFxUndo::getHistoryString() {
  QString str = QObject::tr("Replace Fx  : ");
  str += QString("%1 > %2")
             .arg(QString::fromStdWString(m_fx->getFxId()))
             .arg(QString::fromStdWString(m_repFx->getFxId()));

  return str;
}

//=============================================================

void TFxCommand::replaceFx(TFx *newFx, const QList<TFxP> &fxs,
                           TXsheetHandle *xshHandle, TFxHandle *fxHandle) {
  if (!newFx) return;

  TUndoManager *undoManager = TUndoManager::manager();
  ::CloneFxFunctor cloneFx  = {newFx, true};

  undoManager->beginBlock();

  TFxP clonedFx;

  int f, fCount = fxs.size();
  for (f = 0; f != fCount; ++f) {
    if (!clonedFx) clonedFx = cloneFx();

    std::unique_ptr<FxCommandUndo> undo(
        new ReplaceFxUndo(clonedFx, fxs[f], xshHandle, fxHandle));
    if (undo->isConsistent()) {
      undo->redo();
      undoManager->add(undo.release());

      clonedFx = TFxP();
    }
  }

  undoManager->endBlock();
}

//**********************************************************************
//    Unlink Fx  command
//**********************************************************************

class UnlinkFxUndo final : public FxCommandUndo {
  TFxP m_fx, m_linkedFx;

  TXsheetHandle *m_xshHandle;

public:
  UnlinkFxUndo(const TFxP &fx, TXsheetHandle *xshHandle)
      : m_fx(fx), m_linkedFx(fx->getLinkedFx()), m_xshHandle(xshHandle) {}

  bool isConsistent() const override { return bool(m_linkedFx); }

  void undo() const override {
    FxCommandUndo::linkParams(m_fx.getPointer(), m_linkedFx.getPointer());
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    FxCommandUndo::unlinkParams(m_fx.getPointer());
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Unlink Fx  : %1 - - %2")
        .arg(QString::fromStdWString(m_fx->getFxId()))
        .arg(QString::fromStdWString(m_linkedFx->getFxId()));
  }
};

//=============================================================

void TFxCommand::unlinkFx(TFx *fx, TFxHandle *fxHandle,
                          TXsheetHandle *xshHandle) {
  if (!fx) return;

  std::unique_ptr<FxCommandUndo> undo(new UnlinkFxUndo(fx, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Make Macro Fx  command
//**********************************************************************

class MakeMacroUndo : public FxCommandUndo {
protected:
  TFxP m_macroFx;
  TApplication *m_app;

public:
  MakeMacroUndo(const std::vector<TFxP> &fxs, TApplication *app) : m_app(app) {
    initialize(fxs);
  }

  bool isConsistent() const override { return bool(m_macroFx); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Make Macro Fx  : %1")
        .arg(QString::fromStdWString(m_macroFx->getFxId()));
  }

private:
  void initialize(const std::vector<TFxP> &fxs);

protected:
  MakeMacroUndo(TMacroFx *macroFx, TApplication *app)
      : m_macroFx(macroFx), m_app(app) {}
};

//-------------------------------------------------------------

void MakeMacroUndo::initialize(const std::vector<TFxP> &fxs) {
  TXsheet *xsh = m_app->getCurrentXsheet()->getXsheet();

  size_t f, fCount = fxs.size();
  for (f = 0; f != fCount; ++f) {
    // Only normal Fxs can be added in a macro
    TFx *fx = fxs[f].getPointer();

    if (isInsideAMacroFx(fx, xsh) || fx->isZerary() ||
        dynamic_cast<TZeraryColumnFx *>(fx) || dynamic_cast<TMacroFx *>(fx) ||
        dynamic_cast<TLevelColumnFx *>(fx) ||
        dynamic_cast<TPaletteColumnFx *>(fx) || dynamic_cast<TXsheetFx *>(fx) ||
        dynamic_cast<TOutputFx *>(fx))
      return;
  }

  TMacroFx *macroFx = TMacroFx::create(fxs);
  if (!macroFx) return;

  ::initializeFx(xsh, macroFx);
  m_macroFx = TFxP(macroFx);

  // An old comment suggested there may be trouble in case the fx editor popup
  // is opened.
  // In any case, the new macro fx will be selected at the end - so, let's
  // disable it right now
  m_app->getCurrentFx()->setFx(0);
}

//-------------------------------------------------------------

void MakeMacroUndo::redo() const {
  TXsheet *xsh        = m_app->getCurrentXsheet()->getXsheet();
  FxDag *fxDag        = xsh->getFxDag();
  TFxSet *terminalFxs = fxDag->getTerminalFxs();
  TMacroFx *macroFx   = static_cast<TMacroFx *>(m_macroFx.getPointer());

  ::addFxToCurrentScene(macroFx, xsh, false);

  // Replace the macro's root and deal with output links
  TFx *rootFx = macroFx->getRoot();
  if (terminalFxs->containsFx(rootFx)) fxDag->addToXsheet(macroFx);

  int p, opCount = rootFx->getOutputConnectionCount();
  for (p = opCount - 1; p >= 0; --p)
    rootFx->getOutputConnection(p)->setFx(macroFx);

  // Remove the macro's internal fxs from the scene
  const std::vector<TFxP> &fxs = macroFx->getFxs();

  size_t f, fCount = fxs.size();
  for (f = 0; f != fCount; ++f)
    ::removeFxFromCurrentScene(fxs[f].getPointer(), xsh);

  // Hijack their ports (no actual redirection) - resetting the port ownership.
  // NOTE: Is this even legal? Not gonna touch it, but...   o_o!
  int ipCount = macroFx->getInputPortCount();
  for (p = 0; p != ipCount; ++p) macroFx->getInputPort(p)->setOwnerFx(macroFx);

  m_app->getCurrentFx()->setFx(macroFx);
  m_app->getCurrentXsheet()->notifyXsheetChanged();
}

//-------------------------------------------------------------

void MakeMacroUndo::undo() const {
  TXsheet *xsh        = m_app->getCurrentXsheet()->getXsheet();
  FxDag *fxDag        = xsh->getFxDag();
  TFxSet *terminalFxs = fxDag->getTerminalFxs();
  TMacroFx *macroFx   = static_cast<TMacroFx *>(m_macroFx.getPointer());

  // Reattach the macro's root to the xsheet if necessary
  TFx *rootFx = macroFx->getRoot();
  if (terminalFxs->containsFx(macroFx)) fxDag->addToXsheet(rootFx);

  // Restore the root's output connections
  int p, opCount = macroFx->getOutputConnectionCount();
  for (p = opCount - 1; p >= 0; --p)
    macroFx->getOutputConnection(p)->setFx(rootFx);

  // Remove the macro
  ::removeFxFromCurrentScene(macroFx, xsh);

  // Re-insert the macro's internal fxs and restore ports ownership
  const std::vector<TFxP> &fxs = macroFx->getFxs();

  size_t f, fCount = fxs.size();
  for (f = 0; f != fCount; ++f) {
    TFx *fx = fxs[f].getPointer();

    ::addFxToCurrentScene(fx, xsh, false);

    int p, ipCount = fx->getInputPortCount();
    for (p = 0; p != ipCount; ++p) fx->getInputPort(p)->setOwnerFx(fx);
  }

  m_app->getCurrentFx()->setFx(0);
  m_app->getCurrentXsheet()->notifyXsheetChanged();
}

//=============================================================

void TFxCommand::makeMacroFx(const std::vector<TFxP> &fxs, TApplication *app) {
  if (fxs.empty()) return;

  std::unique_ptr<FxCommandUndo> undo(new MakeMacroUndo(fxs, app));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Explode Macro Fx  command
//**********************************************************************

class ExplodeMacroUndo final : public MakeMacroUndo {
public:
  ExplodeMacroUndo(TMacroFx *macro, TApplication *app)
      : MakeMacroUndo(macro, app) {
    initialize();
  }

  void redo() const override { MakeMacroUndo::undo(); }
  void undo() const override { MakeMacroUndo::redo(); }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Explode Macro Fx  : %1")
        .arg(QString::fromStdWString(m_macroFx->getFxId()));
  }

private:
  void initialize();
};

//------------------------------------------------------

void ExplodeMacroUndo::initialize() {
  if (!static_cast<TMacroFx *>(m_macroFx.getPointer())->getRoot())
    m_macroFx = TFxP();
}

//=============================================================

void TFxCommand::explodeMacroFx(TMacroFx *macroFx, TApplication *app) {
  if (!macroFx) return;

  std::unique_ptr<FxCommandUndo> undo(new ExplodeMacroUndo(macroFx, app));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Create Output Fx  command
//**********************************************************************

class CreateOutputFxUndo final : public FxCommandUndo {
  TFxP m_outputFx;
  TXsheetHandle *m_xshHandle;

public:
  CreateOutputFxUndo(TFx *fx, TXsheetHandle *xshHandle)
      : m_outputFx(new TOutputFx), m_xshHandle(xshHandle) {
    initialize(fx);
  }

  bool isConsistent() const override { return true; }

  void redo() const override {
    FxDag *fxDag        = m_xshHandle->getXsheet()->getFxDag();
    TOutputFx *outputFx = static_cast<TOutputFx *>(m_outputFx.getPointer());

    fxDag->addOutputFx(outputFx);
    fxDag->setCurrentOutputFx(outputFx);

    m_xshHandle->notifyXsheetChanged();
  }

  void undo() const override {
    TOutputFx *outputFx = static_cast<TOutputFx *>(m_outputFx.getPointer());

    m_xshHandle->getXsheet()->getFxDag()->removeOutputFx(outputFx);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Create Output Fx");
  }

private:
  void initialize(TFx *fx) {
    TXsheet *xsh        = m_xshHandle->getXsheet();
    TOutputFx *outputFx = static_cast<TOutputFx *>(m_outputFx.getPointer());

    if (fx && !dynamic_cast<TOutputFx *>(fx))
      outputFx->getInputPort(0)->setFx(fx);
    else {
      TOutputFx *currentOutputFx = xsh->getFxDag()->getCurrentOutputFx();
      const TPointD &pos = currentOutputFx->getAttributes()->getDagNodePos();
      if (pos != TConst::nowhere)
        outputFx->getAttributes()->setDagNodePos(pos + TPointD(20, 20));
    }
  }
};

//=============================================================

void TFxCommand::createOutputFx(TXsheetHandle *xshHandle, TFx *currentFx) {
  TUndo *undo = new CreateOutputFxUndo(currentFx, xshHandle);

  undo->redo();
  TUndoManager::manager()->add(undo);
}

//**********************************************************************
//    Make Output Fx Current  command
//**********************************************************************

void TFxCommand::makeOutputFxCurrent(TFx *fx, TXsheetHandle *xshHandle) {
  TOutputFx *outputFx = dynamic_cast<TOutputFx *>(fx);
  if (!outputFx) return;

  TXsheet *xsh = xshHandle->getXsheet();
  if (xsh->getFxDag()->getCurrentOutputFx() == outputFx) return;

  xsh->getFxDag()->setCurrentOutputFx(outputFx);
  xshHandle->notifyXsheetChanged();
}

//**********************************************************************
//    Connect Nodes To Xsheet  command
//**********************************************************************

class ConnectNodesToXsheetUndo : public FxCommandUndo {
protected:
  std::vector<TFxP> m_fxs;
  TXsheetHandle *m_xshHandle;

public:
  ConnectNodesToXsheetUndo(const std::list<TFxP> &fxs, TXsheetHandle *xshHandle)
      : m_fxs(fxs.begin(), fxs.end()), m_xshHandle(xshHandle) {
    initialize();
  }

  bool isConsistent() const override { return !m_fxs.empty(); }

  void redo() const override {
    /*
Due to compatibility issues from *schematicnode.cpp files, the "do" operation
must be
accessible without scene change notifications (see TFxCommand::setParent())
*/

    redo_();
    m_xshHandle->notifyXsheetChanged();
  }

  void redo_() const {
    FxDag *fxDag = m_xshHandle->getXsheet()->getFxDag();

    size_t f, fCount = m_fxs.size();
    for (f = 0; f != fCount; ++f) fxDag->addToXsheet(m_fxs[f].getPointer());
  }

  void undo() const override {
    FxDag *fxDag = m_xshHandle->getXsheet()->getFxDag();

    size_t f, fCount = m_fxs.size();
    for (f = 0; f != fCount; ++f)
      fxDag->removeFromXsheet(m_fxs[f].getPointer());

    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    QString str = QObject::tr("Connect to Xsheet  : ");
    std::vector<TFxP>::iterator it;
    for (it = m_fxs.begin(); it != m_fxs.end(); it++) {
      if (it != m_fxs.begin()) str += QString(", ");
      str += QString::fromStdWString((*it)->getFxId());
    }
    return str;
  }

protected:
  ConnectNodesToXsheetUndo(const std::list<TFxP> &fxs, TXsheetHandle *xshHandle,
                           bool)
      : m_fxs(fxs.begin(), fxs.end()), m_xshHandle(xshHandle) {}

private:
  void initialize();
};

//------------------------------------------------------

void ConnectNodesToXsheetUndo::initialize() {
  TXsheet *xsh = m_xshHandle->getXsheet();

  ::FilterInsideAMacro filterInMacro = {xsh};
  m_fxs.erase(std::remove_if(m_fxs.begin(), m_fxs.end(), filterInMacro),
              m_fxs.end());

  ::FilterTerminalFxs filterTerminalFxs = {xsh};
  m_fxs.erase(std::remove_if(m_fxs.begin(), m_fxs.end(), filterTerminalFxs),
              m_fxs.end());
}

//=============================================================

void TFxCommand::connectNodesToXsheet(const std::list<TFxP> &fxs,
                                      TXsheetHandle *xshHandle) {
  std::unique_ptr<FxCommandUndo> undo(
      new ConnectNodesToXsheetUndo(fxs, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Disconnect Nodes From Xsheet  command
//**********************************************************************

class DisconnectNodesFromXsheetUndo final : public ConnectNodesToXsheetUndo {
public:
  DisconnectNodesFromXsheetUndo(const std::list<TFxP> &fxs,
                                TXsheetHandle *xshHandle)
      : ConnectNodesToXsheetUndo(fxs, xshHandle, true) {
    initialize();
  }

  void redo() const override { ConnectNodesToXsheetUndo::undo(); }
  void undo() const override { ConnectNodesToXsheetUndo::redo(); }

  QString getHistoryString() override {
    QString str = QObject::tr("Disconnect from Xsheet  : ");
    std::vector<TFxP>::iterator it;
    for (it = m_fxs.begin(); it != m_fxs.end(); it++) {
      if (it != m_fxs.begin()) str += QString(", ");
      str += QString::fromStdWString((*it)->getFxId());
    }
    return str;
  }

private:
  void initialize();
};

//------------------------------------------------------

void DisconnectNodesFromXsheetUndo::initialize() {
  TXsheet *xsh = m_xshHandle->getXsheet();

  ::FilterInsideAMacro filterInMacro = {xsh};
  m_fxs.erase(std::remove_if(m_fxs.begin(), m_fxs.end(), filterInMacro),
              m_fxs.end());

  ::FilterNonTerminalFxs filterNonTerminalFxs = {xsh};
  m_fxs.erase(std::remove_if(m_fxs.begin(), m_fxs.end(), filterNonTerminalFxs),
              m_fxs.end());
}

//=============================================================

void TFxCommand::disconnectNodesFromXsheet(const std::list<TFxP> &fxs,
                                           TXsheetHandle *xshHandle) {
  std::unique_ptr<FxCommandUndo> undo(
      new DisconnectNodesFromXsheetUndo(fxs, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Delete Link  command
//**********************************************************************

class DeleteLinksUndo : public FxCommandUndo {
  struct DynamicLink {
    int m_groupIndex;
    std::string m_portName;
    TFx *m_inputFx;
  };

  typedef std::vector<DynamicLink> DynamicLinksVector;

protected:
  std::list<TFxCommand::Link> m_links;  //!< The input links to remove

private:
  std::list<TFxCommand::Link>
      m_normalLinks;               //!< Actual *common* links from m_links
  std::list<TFx *> m_terminalFxs;  //!< Fxs connected to the xsheet (represents
                                   //! xsheet input links)
  //   Why SMART pointers? No fx is deleted with this command... hmm...
  std::map<TFx *, DynamicLinksVector>
      m_dynamicLinks;  //!< Complete dynamic links configuration, per fx.

  TXsheetHandle *m_xshHandle;

public:
  DeleteLinksUndo(const std::list<TFxCommand::Link> &links,
                  TXsheetHandle *xshHandle)
      : m_links(links), m_xshHandle(xshHandle) {
    initialize();
  }

  bool isConsistent() const override { return !m_links.empty(); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return 10 << 10; }  // Say, around 10 kB

  QString getHistoryString() override;

protected:
  DeleteLinksUndo(TXsheetHandle *xshHandle) : m_xshHandle(xshHandle) {}

  void initialize();
};

//------------------------------------------------------

void DeleteLinksUndo::initialize() {
  struct locals {
    static bool isInvalid(FxDag *fxDag, const TFxCommand::Link &link) {
      if (link.m_index < 0)
        return !fxDag->getTerminalFxs()->containsFx(
            link.m_inputFx.getPointer());

      TFx *inFx  = ::getActualOut(link.m_inputFx.getPointer());
      TFx *outFx = ::getActualIn(link.m_outputFx.getPointer());

      return (link.m_index >= outFx->getInputPortCount())
                 ? true
                 : (outFx->getInputPort(link.m_index)->getFx() != inFx);
    }
  };

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Forget links dealing with an open macro Fx. Note that this INCLUDES
  // inside/outside links.
  ::FilterInsideAMacro filterInMacro = {xsh};
  m_links.erase(std::remove_if(m_links.begin(), m_links.end(), filterInMacro),
                m_links.end());

  // Remove invalid links
  m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                               [fxDag](const TFxCommand::Link &link) {
                                 return locals::isInvalid(fxDag, link);
                               }),
                m_links.end());

  std::list<TFxCommand::Link>::iterator lt, lEnd(m_links.end());
  for (lt = m_links.begin(); lt != lEnd; ++lt) {
    const TFxCommand::Link &link = *lt;

    if (TXsheetFx *xsheetFx =
            dynamic_cast<TXsheetFx *>(link.m_outputFx.getPointer())) {
      // The input fx is connected to an xsheet node - ie it's terminal
      m_terminalFxs.push_back(link.m_inputFx.getPointer());
      continue;
    }

    TFx *outputFx = link.m_outputFx.getPointer();

    // Zerary columns wrap the actual zerary fx - that is the fx holding input
    // ports
    if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(outputFx))
      outputFx = zfx->getZeraryFx();

    TFxPort *port = outputFx->getInputPort(link.m_index);

    int portGroup = port->getGroupIndex();
    if (portGroup < 0) m_normalLinks.push_back(link);

    if (outputFx->hasDynamicPortGroups())
      m_dynamicLinks.insert(std::make_pair(outputFx, DynamicLinksVector()));
  }

  m_normalLinks.sort();  // Really necessary?

  // Store the complete configuration of dynamic groups - not just the ones
  // touched by
  // link editing, ALL of them.
  std::map<TFx *, DynamicLinksVector>::iterator dlt,
      dlEnd(m_dynamicLinks.end());
  for (dlt = m_dynamicLinks.begin(); dlt != dlEnd; ++dlt) {
    TFx *outputFx                = dlt->first;
    DynamicLinksVector &dynLinks = dlt->second;

    int p, pCount = outputFx->getInputPortCount();
    for (p = 0; p != pCount; ++p) {
      TFxPort *port = outputFx->getInputPort(p);

      int g = port->getGroupIndex();
      if (g >= 0) {
        DynamicLink dLink = {g, outputFx->getInputPortName(p), port->getFx()};
        dynLinks.push_back(dLink);
      }
    }
  }
}

//------------------------------------------------------

void DeleteLinksUndo::redo() const {
  FxDag *fxDag = m_xshHandle->getXsheet()->getFxDag();

  // Perform unlinking
  std::list<TFxCommand::Link>::const_iterator lt, lEnd(m_links.end());
  for (lt = m_links.begin(); lt != lEnd; ++lt) {
    const TFxCommand::Link &link = *lt;

    TFx *outputFx = lt->m_outputFx.getPointer();

    if (TXsheetFx *xsheetFx = dynamic_cast<TXsheetFx *>(outputFx)) {
      // Terminal fx link case
      fxDag->removeFromXsheet(link.m_inputFx.getPointer());
      continue;
    }

    // Actual link case
    if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(outputFx))
      outputFx = zcfx->getZeraryFx();

    int index = lt->m_index;

    assert(index < outputFx->getInputPortCount());
    if (index < outputFx->getInputPortCount())
      outputFx->getInputPort(index)->setFx(0);
  }

  if (m_isLastInRedoBlock) m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void DeleteLinksUndo::undo() const {
  FxDag *fxDag = m_xshHandle->getXsheet()->getFxDag();

  // Re-attach terminal fxs
  std::list<TFx *>::const_iterator ft;
  for (ft = m_terminalFxs.begin(); ft != m_terminalFxs.end(); ++ft) {
    if (fxDag->checkLoop(*ft, fxDag->getXsheetFx())) {
      assert(fxDag->checkLoop(*ft, fxDag->getXsheetFx()));
      continue;
    }

    fxDag->addToXsheet(*ft);
  }

  // Restore common links
  std::list<TFxCommand::Link>::const_iterator lt, lEnd(m_normalLinks.end());
  for (lt = m_normalLinks.begin(); lt != lEnd; ++lt) {
    const TFxCommand::Link &link = *lt;

    int index     = link.m_index;
    TFx *inputFx  = link.m_inputFx.getPointer();
    TFx *outputFx = link.m_outputFx.getPointer();

    if (TZeraryColumnFx *zcfx = dynamic_cast<TZeraryColumnFx *>(outputFx))
      outputFx = zcfx->getZeraryFx();

    if (fxDag->checkLoop(inputFx, outputFx)) {
      assert(fxDag->checkLoop(inputFx, outputFx));
      continue;
    }

    assert(index < outputFx->getInputPortCount());

    if (index < outputFx->getInputPortCount())
      outputFx->getInputPort(index)->setFx(inputFx);
  }

  // Restore complete dynamic port groups configuration
  std::map<TFx *, DynamicLinksVector>::const_iterator dlt,
      dlEnd(m_dynamicLinks.end());
  for (dlt = m_dynamicLinks.begin(); dlt != dlEnd; ++dlt) {
    TFx *outputFx                      = dlt->first;
    const DynamicLinksVector &dynLinks = dlt->second;

    {
      int g, gCount = outputFx->dynamicPortGroupsCount();
      for (g = 0; g != gCount; ++g) outputFx->clearDynamicPortGroup(g);
    }

    size_t d, dCount = dynLinks.size();
    for (d = 0; d != dCount; ++d) {
      const DynamicLink &link = dynLinks[d];

      TFxPort *port = new TRasterFxPort;  // isAControlPort... semi-obsolete
      port->setFx(link.m_inputFx);

      outputFx->addInputPort(link.m_portName, port, link.m_groupIndex);
    }
  }

  if (m_isLastInBlock) m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

QString DeleteLinksUndo::getHistoryString() {
  QString str = QObject::tr("Delete Link");
  if (!m_normalLinks.empty()) {
    str += QString("  :  ");
    std::list<TFxCommand::Link>::const_iterator it;
    for (it = m_normalLinks.begin(); it != m_normalLinks.end(); it++) {
      if (it != m_normalLinks.begin()) str += QString(",  ");
      TFxCommand::Link boundingFxs = *it;
      str +=
          QString("%1- -%2")
              .arg(QString::fromStdWString(boundingFxs.m_inputFx->getName()))
              .arg(QString::fromStdWString(boundingFxs.m_outputFx->getName()));
    }
  }

  if (!m_terminalFxs.empty()) {
    str += QString("  :  ");
    std::list<TFx *>::const_iterator ft;
    for (ft = m_terminalFxs.begin(); ft != m_terminalFxs.end(); ++ft) {
      if (ft != m_terminalFxs.begin()) str += QString(",  ");
      str +=
          QString("%1- -Xsheet").arg(QString::fromStdWString((*ft)->getName()));
    }
  }

  return str;
}

//=============================================================

static void deleteLinks(const std::list<TFxCommand::Link> &links,
                        TXsheetHandle *xshHandle) {
  std::unique_ptr<FxCommandUndo> undo(new DeleteLinksUndo(links, xshHandle));
  if (undo->isConsistent()) {
    undo->m_isLastInRedoBlock = false;
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//******************************************************
//    Delete Fx  command
//******************************************************

class DeleteFxOrColumnUndo final : public DeleteLinksUndo {
protected:
  TFxP m_fx;
  TXshColumnP m_column;
  int m_colIdx;

  TFxP m_linkedFx;
  std::vector<TFx *> m_nonTerminalInputs;

  mutable std::unique_ptr<TStageObjectParams> m_columnData;

  TXsheetHandle *m_xshHandle;
  TFxHandle *m_fxHandle;

  using DeleteLinksUndo::m_links;

public:
  DeleteFxOrColumnUndo(const TFxP &fx, TXsheetHandle *xshHandle,
                       TFxHandle *fxHandle);
  DeleteFxOrColumnUndo(int colIdx, TXsheetHandle *xshHandle,
                       TFxHandle *fxHandle);

  bool isConsistent() const override;

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override;

private:
  void initialize();
};

//-------------------------------------------------------------

DeleteFxOrColumnUndo::DeleteFxOrColumnUndo(const TFxP &fx,
                                           TXsheetHandle *xshHandle,
                                           TFxHandle *fxHandle)
    : DeleteLinksUndo(xshHandle)
    , m_fx(fx)
    , m_colIdx(-1)
    , m_xshHandle(xshHandle)
    , m_fxHandle(fxHandle) {
  initialize();
}

//-------------------------------------------------------------

DeleteFxOrColumnUndo::DeleteFxOrColumnUndo(int colIdx, TXsheetHandle *xshHandle,
                                           TFxHandle *fxHandle)
    : DeleteLinksUndo(xshHandle)
    , m_colIdx(colIdx)
    , m_xshHandle(xshHandle)
    , m_fxHandle(fxHandle) {
  initialize();
}

//-------------------------------------------------------------

void DeleteFxOrColumnUndo::initialize() {
  struct {
    DeleteFxOrColumnUndo *m_this;
    inline void getActualData(TXsheet *xsh, TFx *&ifx, TFx *&ofx) {
      TFx *fx = m_this->m_fx.getPointer();

      if (!fx) fx = xsh->getColumn(m_this->m_colIdx)->getFx();

      if (fx) {
        ifx = ::getActualIn(fx);
        ofx = ::getActualOut(fx);

        if (TColumnFx *colFx = dynamic_cast<TColumnFx *>(ofx))
          m_this->m_colIdx = colFx->getColumnIndex();
      }

      m_this->m_fx = ofx;
    }
  } locals = {this};

  assert(m_fx || m_colIdx >= 0);

  TXsheet *xsh = m_xshHandle->getXsheet();
  TFx *ifx = 0, *ofx = 0;

  locals.getActualData(xsh, ifx, ofx);

  if (ofx && isInsideAMacroFx(ofx, xsh))  // Macros must be exploded first
  {
    m_fx = TFxP(), m_colIdx = -1;
    return;
  }

  // Assume shared ownership of the associated column, if any
  if (m_colIdx >= 0) {
    m_column = xsh->getColumn(m_colIdx);
    assert(m_column);

    // Currently disputed, but in the previous implementation there was code
    // suggesting
    // that the column could have already been removed from the xsheet.
    // Preventing that case...
    if (!m_column->inColumnsSet()) {
      m_fx = TFxP(), m_colIdx = -1;  // Bail out as inconsistent op
      return;                        //
    }
  } else if (TOutputFx *outputFx = dynamic_cast<TOutputFx *>(ofx)) {
    if (xsh->getFxDag()->getOutputFxCount() <= 1) {
      // Cannot delete the last output fx
      m_fx = TFxP();
      assert(m_colIdx < 0);
      return;
    }
  }

  // Store links to be re-established in the undo
  FxDag *fxDag = xsh->getFxDag();

  if (ofx) {
    // Store the terminal output link, if any
    if (fxDag->getTerminalFxs()->containsFx(ofx))
      m_links.push_back(TFxCommand::Link(ofx, fxDag->getXsheetFx(), -1));

    // Store output links
    int p, opCount = ofx->getOutputConnectionCount();
    for (p = 0; p != opCount; ++p) {
      if (TFx *outFx = ofx->getOutputConnection(p)->getOwnerFx()) {
        int ip, ipCount = outFx->getInputPortCount();
        for (ip = 0; ip != ipCount; ++ip)
          if (outFx->getInputPort(ip)->getFx() == ofx) break;

        assert(ip < ipCount);
        if (ip < ipCount) m_links.push_back(TFxCommand::Link(m_fx, outFx, ip));
      }
    }
  }

  if (ifx) {
    m_linkedFx = ifx->getLinkedFx();

    // Store input links
    int p, ipCount = ifx->getInputPortCount();
    for (p = 0; p != ipCount; ++p) {
      if (TFx *inputFx = ifx->getInputPort(p)->getFx()) {
        m_links.push_back(TFxCommand::Link(inputFx, m_fx, p));
        if (!fxDag->getTerminalFxs()->containsFx(
                inputFx))  // Store input fxs which DID NOT have an
          m_nonTerminalInputs.push_back(
              inputFx);  // xsheet link before the deletion
      }
    }
  }

  DeleteLinksUndo::initialize();
}

//-------------------------------------------------------------

bool DeleteFxOrColumnUndo::isConsistent() const {
  return (bool(m_fx) || (m_colIdx >= 0));

  // NOTE: DeleteLinksUndo::isConsistent() is not checked.
  //       This is because there could be no link to remove, and yet
  //       the operation IS consistent.
}

//-------------------------------------------------------------

void DeleteFxOrColumnUndo::redo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Store data to be restored in the undo
  if (m_colIdx >= 0) {
    assert(!m_columnData.get());

    m_columnData.reset(
        xsh->getStageObject(TStageObjectId::ColumnId(
                                m_colIdx))  // Cloned, ownership acquired
            ->getParams());  // However, params stored there are NOT cloned.
  }                          // This is fine since we're deleting the column...

  // Perform operation
  FxCommandUndo::removeFxOrColumn(xsh, m_fx.getPointer(), m_colIdx);

  if (m_isLastInRedoBlock)
    m_xshHandle->notifyXsheetChanged();  // Add the rest...
}

//-------------------------------------------------------------

void DeleteFxOrColumnUndo::undo() const {
  struct Locals {
    const DeleteFxOrColumnUndo *m_this;

    void insertColumnIn(TXsheet *xsh) {
      m_this->insertColumn(xsh, m_this->m_column.getPointer(),
                           m_this->m_colIdx);

      // Restore column data
      TStageObject *sObj =
          xsh->getStageObject(TStageObjectId::ColumnId(m_this->m_colIdx));
      assert(sObj);

      sObj->assignParams(m_this->m_columnData.get(), false);
      m_this->m_columnData.reset();
    }

  } locals = {this};

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Re-add the fx/column to the xsheet
  TFx *fx = m_fx.getPointer();

  if (m_column)
    locals.insertColumnIn(xsh);
  else if (TOutputFx *outFx = dynamic_cast<TOutputFx *>(fx))
    xsh->getFxDag()->addOutputFx(outFx);
  else
    addFxToCurrentScene(fx, xsh, false);

  if (fx) {
    // Remove xsheet connections that became terminal due to the fx
    // removal
    size_t ti, tiCount = m_nonTerminalInputs.size();
    for (ti = 0; ti != tiCount; ++ti)
      fxDag->removeFromXsheet(m_nonTerminalInputs[ti]);

    // Re-link parameters if necessary
    TFx *ifx = ::getActualIn(fx);

    if (m_linkedFx) ifx->linkParams(m_linkedFx.getPointer());

    // Re-establish fx links
    DeleteLinksUndo::undo();
  } else if (m_isLastInBlock)  // Already covered by DeleteLinksUndo::undo()
    m_xshHandle->notifyXsheetChanged();  // in the other branch
}

//-------------------------------------------------------------

QString DeleteFxOrColumnUndo::getHistoryString() {
  return QObject::tr("Delete Fx Node : %1")
      .arg(QString::fromStdWString(m_fx->getFxId()));
}

//=============================================================

static void deleteFxs(const std::list<TFxP> &fxs, TXsheetHandle *xshHandle,
                      TFxHandle *fxHandle) {
  TUndoManager *undoManager = TUndoManager::manager();
  TXsheet *xsh              = xshHandle->getXsheet();

  undoManager->beginBlock();

  std::list<TFxP>::const_iterator ft, fEnd = fxs.end();
  for (ft = fxs.begin(); ft != fEnd; ++ft) {
    // Skip levels, as they are effectively supplied in here, AND in the
    // deleteColumns() branch.
    // This should NOT be performed here, though. TO BE MOVED TO deleteSelection
    // or ABOVE, if any.
    if (dynamic_cast<TLevelColumnFx *>(ft->getPointer())) continue;

    std::unique_ptr<FxCommandUndo> undo(
        new DeleteFxOrColumnUndo(*ft, xshHandle, fxHandle));
    if (undo->isConsistent()) {
      // prevent emitting xsheetChanged signal for every undos which will cause
      // multiple triggers of preview rendering
      undo->m_isLastInRedoBlock = false;
      undo->redo();
      TUndoManager::manager()->add(undo.release());
    }
  }

  undoManager->endBlock();
}

//**********************************************************************
//    Delete Columns  command
//**********************************************************************

static void deleteColumns(const std::list<int> &columns,
                          TXsheetHandle *xshHandle, TFxHandle *fxHandle) {
  TUndoManager *undoManager = TUndoManager::manager();

  undoManager->beginBlock();

  // As columns are deleted, their index changes. So, the easiest workaround is
  // to address the
  // columns directly, and then their (updated) column index.
  TXsheet *xsh = xshHandle->getXsheet();

  std::vector<TXshColumn *> cols;
  for (auto const &c : columns) {
    cols.push_back(xsh->getColumn(c));
  }

  size_t c, cCount = cols.size();
  for (c = 0; c != cCount; ++c) {
    std::unique_ptr<FxCommandUndo> undo(
        new DeleteFxOrColumnUndo(cols[c]->getIndex(), xshHandle, fxHandle));
    if (undo->isConsistent()) {
      // prevent emitting xsheetChanged signal for every undos which will cause
      // multiple triggers of preview rendering
      undo->m_isLastInRedoBlock = false;
      undo->redo();
      undoManager->add(undo.release());
    }
  }

  undoManager->endBlock();
}

//**********************************************************************
//    Delete Selection  command
//**********************************************************************

void TFxCommand::deleteSelection(const std::list<TFxP> &fxs,
                                 const std::list<Link> &links,
                                 const std::list<int> &columns,
                                 TXsheetHandle *xshHandle,
                                 TFxHandle *fxHandle) {
  // Prepare selected fxs - column fxs would be done twice if the corresponding
  // columns have been supplied for deletion too
  ::FilterColumnFxs filterColumnFxs;

  std::list<TFxP> filteredFxs(fxs);
  filteredFxs.erase(
      std::remove_if(filteredFxs.begin(), filteredFxs.end(), filterColumnFxs),
      filteredFxs.end());

  // Perform deletions
  TUndoManager::manager()->beginBlock();

  if (!columns.empty()) deleteColumns(columns, xshHandle, fxHandle);
  if (!filteredFxs.empty()) deleteFxs(filteredFxs, xshHandle, fxHandle);
  if (!links.empty()) deleteLinks(links, xshHandle);

  TUndoManager::manager()->endBlock();
  // emit xsheetChanged once here
  xshHandle->notifyXsheetChanged();
}

//**********************************************************************
//    Paste Fxs  command
//**********************************************************************

/*
  NOTE: Zerary fxs should not be in the fxs list - but rather in the columns
  list.
  This would allow us to forget about the zeraryColumnSize map.

  This requires changing the *selection*, though. To be done, in a later step.
*/

class UndoPasteFxs : public FxCommandUndo {
protected:
  std::list<TFxP> m_fxs;             //!< Fxs to be pasted
  std::list<TXshColumnP> m_columns;  //!< Columns to be pasted
  std::vector<TFxCommand::Link>
      m_links;  //!< Links to be re-established at the redo

  TXsheetHandle *m_xshHandle;
  TFxHandle *m_fxHandle;

public:
  UndoPasteFxs(const std::list<TFxP> &fxs,
               const std::map<TFx *, int> &zeraryFxColumnSize,
               const std::list<TXshColumnP> &columns, const TPointD &pos,
               TXsheetHandle *xshHandle, TFxHandle *fxHandle,
               bool addOffset = true)
      : m_fxs(fxs)
      , m_columns(columns)
      , m_xshHandle(xshHandle)
      , m_fxHandle(fxHandle) {
    initialize(zeraryFxColumnSize, pos, addOffset);
  }

  bool isConsistent() const override {
    return !(m_fxs.empty() && m_columns.empty());
  }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override;

protected:
  template <typename Func>
  void for_each_fx(Func func) const;

private:
  void initialize(const std::map<TFx *, int> &zeraryFxColumnSize,
                  const TPointD &pos, bool addOffset);
};

//------------------------------------------------------

void UndoPasteFxs::initialize(const std::map<TFx *, int> &zeraryFxColumnSize,
                              const TPointD &pos, bool addOffset) {
  struct locals {
    static void buildDagPos(TFx *fromFx, TFx *toFx, bool copyPos,
                            bool addOffset) {
      TPointD dagPos = TConst::nowhere;
      if (copyPos) {
        static const TPointD offset(30, 30);

        dagPos = fromFx->getAttributes()->getDagNodePos();
        if (addOffset &&
            (dagPos != TConst::nowhere))  // Shift may only happen if the copied
          dagPos += offset;               // position is well-formed.
      }

      toFx->getAttributes()->setDagNodePos(dagPos);
    }

    static void renamePort(TFx *fx, int p, const std::wstring &oldId,
                           const std::wstring &newId) {
      const QString &qOldId = QString::fromStdWString(oldId);
      const QString &qNewId = QString::fromStdWString(newId);
      const QString &qPortName =
          QString::fromStdString(fx->getInputPortName(p));

      if (qPortName.endsWith(qOldId)) {
        QString qNewPortName = qPortName;
        qNewPortName.replace(qOldId, qNewId);
        fx->renamePort(qPortName.toStdString(), qNewPortName.toStdString());
      }
    }
  };

  TXsheet *xsh    = m_xshHandle->getXsheet();
  bool copyDagPos = (pos != TConst::nowhere);

  // Initialize fxs
  std::list<TFxP>::iterator ft, fEnd = m_fxs.end();
  for (ft = m_fxs.begin(); ft != fEnd;) {
    TFx *fx = ft->getPointer();

    assert(!dynamic_cast<TZeraryColumnFx *>(fx));

    if (has_fx_column(fx)) {
      // Zerary case

      // Since we have no column available (WHICH IS WRONG), we'll build up a
      // column with
      // the specified column size
      std::map<TFx *, int>::const_iterator it = zeraryFxColumnSize.find(fx);
      int rows = (it == zeraryFxColumnSize.end()) ? 100 : it->second;

      TXshZeraryFxColumn *column = new TXshZeraryFxColumn(rows);
      TZeraryColumnFx *zcfx      = column->getZeraryColumnFx();

      zcfx->setZeraryFx(fx);

      int op, opCount = fx->getOutputConnectionCount();
      for (op = opCount - 1; op >= 0;
           --op)  // Output links in the zerary case were
      {           // assigned to the *actual* zerary
        TFxPort *outPort =
            fx->getOutputConnection(op);  // and not to a related zerary column.
        outPort->setFx(zcfx);  // This is an FxsData fault... BUGFIX REQUEST!
      }

      zcfx->getAttributes()->setDagNodePos(
          fx->getAttributes()->getDagNodePos());
      m_columns.push_front(column);

      ft = m_fxs.erase(ft);
      continue;
    }

    // Macro case
    if (TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx)) {
      const std::vector<TFxP> &inMacroFxs = macroFx->getFxs();

      size_t f, fCount = inMacroFxs.size();
      for (f = 0; f != fCount; ++f) {
        TFx *inFx                   = inMacroFxs[f].getPointer();
        const std::wstring &oldFxId = inFx->getFxId();

        // Since we're pasting a macro, we're actually pasting all of its inner
        // fxs,
        // which must have a different name (id) from the originals. However,
        // such ids
        // are actually copied in the macro's *port names* - which must
        // therefore be
        // redirected to the new names.

        ::initializeFx(xsh, inFx);
        const std::wstring &newFxId = inFx->getFxId();

        int ip, ipCount = macroFx->getInputPortCount();
        for (ip = 0; ip != ipCount; ++ip)
          locals::renamePort(macroFx, ip, oldFxId, newFxId);
        // node position of the macrofx is defined by dag-pos of inner fxs.
        // so we need to reset them here or pasted node will be at the same
        // position as the copied one.
        locals::buildDagPos(inFx, inFx, copyDagPos, addOffset);
      }
    }

    ::initializeFx(xsh, fx);
    locals::buildDagPos(fx, fx, copyDagPos, addOffset);

    ++ft;
  }

  // Filter columns
  auto const circularSubxsheet = [xsh](const TXshColumnP &col) -> bool {
    return xsh->checkCircularReferences(col.getPointer());
  };
  m_columns.erase(
      std::remove_if(m_columns.begin(), m_columns.end(), circularSubxsheet),
      m_columns.end());

  // Initialize columns
  std::list<TXshColumnP>::const_iterator ct, cEnd(m_columns.end());
  for (ct = m_columns.begin(); ct != cEnd; ++ct) {
    if (TFx *cfx = (*ct)->getFx()) {
      ::initializeFx(xsh, cfx);
      locals::buildDagPos(cfx, cfx, copyDagPos, addOffset);
    }
  }

  // Now, let's make a temporary container of all the stored fxs, both
  // normal and column-based
  std::vector<TFx *> fxs;
  fxs.reserve(m_fxs.size() + m_columns.size());

  for_each_fx([&fxs](TFx *fx) { fxs.push_back(fx); });

  // We need to store input links for these fxs
  size_t f, fCount = fxs.size();
  for (f = 0; f != fCount; ++f) {
    TFx *fx = fxs[f];

    TFx *ofx = ::getActualIn(fx);
    fx       = ::getActualOut(fx);

    int il, ilCount = ofx->getInputPortCount();
    for (il = 0; il != ilCount; ++il) {
      if (TFx *ifx = ofx->getInputPort(il)->getFx())
        m_links.push_back(TFxCommand::Link(ifx, ofx, il));
    }
  }

  // Apply the required position, if any
  if (pos != TConst::nowhere) {
    // Then, we'll take the mean difference from pos and add it to
    // each fx
    TPointD middlePos;
    int fxsCount = 0;

    std::vector<TFx *>::const_iterator ft, fEnd = fxs.end();
    for (ft = fxs.begin(); ft != fEnd; ++ft) {
      TFx *fx = *ft;

      const TPointD &fxPos = fx->getAttributes()->getDagNodePos();
      if (fxPos != TConst::nowhere) {
        middlePos += fxPos;
        ++fxsCount;
      }
    }

    if (fxsCount > 0) {
      middlePos = TPointD(middlePos.x / fxsCount, middlePos.y / fxsCount);
      const TPointD &offset = pos - middlePos;

      for (ft = fxs.begin(); ft != fEnd; ++ft) {
        TFx *fx = *ft;

        const TPointD &fxPos = fx->getAttributes()->getDagNodePos();
        if (fxPos != TConst::nowhere)
          fx->getAttributes()->setDagNodePos(fxPos + offset);
      }
    }
  }
}

//------------------------------------------------------

void UndoPasteFxs::redo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Iterate all normal fxs
  std::list<TFxP>::const_iterator ft, fEnd = m_fxs.end();
  for (ft = m_fxs.begin(); ft != fEnd; ++ft) {
    // Re-insert them in the scene
    addFxToCurrentScene(ft->getPointer(), xsh, false);
  }

  // Iterate columns
  std::list<TXshColumnP>::const_iterator ct, cEnd = m_columns.end();
  for (ct = m_columns.begin(); ct != cEnd; ++ct) {
    // Insert them
    FxCommandUndo::insertColumn(xsh, ct->getPointer(),
                                xsh->getFirstFreeColumnIndex(), true, false);
  }

  // Restore links
  size_t l, lCount = m_links.size();
  for (l = 0; l != lCount; ++l) FxCommandUndo::attach(xsh, m_links[l], false);

  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void UndoPasteFxs::undo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  std::list<TFxP>::const_iterator ft, fEnd = m_fxs.end();
  for (ft = m_fxs.begin(); ft != fEnd; ++ft) {
    TFx *fx = ft->getPointer();

    FxCommandUndo::removeFxOrColumn(xsh, fx, -1, true,
                                    false);  // Skip parameter links removal
    FxCommandUndo::makeNotCurrent(m_fxHandle, fx);
  }

  std::list<TXshColumnP>::const_iterator ct, cEnd = m_columns.end();
  for (ct = m_columns.begin(); ct != cEnd; ++ct) {
    FxCommandUndo::removeFxOrColumn(xsh, 0, (*ct)->getIndex(), true,
                                    false);  // Skip parameter links removal
    FxCommandUndo::makeNotCurrent(m_fxHandle, (*ct)->getFx());
  }

  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

template <typename Func>
void UndoPasteFxs::for_each_fx(Func func) const {
  std::list<TFxP>::const_iterator ft, fEnd = m_fxs.end();
  for (ft = m_fxs.begin(); ft != fEnd; ++ft) func(ft->getPointer());

  std::list<TXshColumnP>::const_iterator ct, cEnd = m_columns.end();
  for (ct = m_columns.begin(); ct != cEnd; ++ct)
    if (TFx *cfx = (*ct)->getFx()) func(cfx);
}

//------------------------------------------------------

QString UndoPasteFxs::getHistoryString() {
  QString str = QObject::tr("Paste Fx  :  ");
  std::list<TFxP>::const_iterator it;
  for (it = m_fxs.begin(); it != m_fxs.end(); it++) {
    if (it != m_fxs.begin()) str += QString(",  ");
    str += QString("%1").arg(QString::fromStdWString((*it)->getName()));
  }
  return str;
}

//=============================================================

void TFxCommand::pasteFxs(const std::list<TFxP> &fxs,
                          const std::map<TFx *, int> &zeraryFxColumnSize,
                          const std::list<TXshColumnP> &columns,
                          const TPointD &pos, TXsheetHandle *xshHandle,
                          TFxHandle *fxHandle) {
  std::unique_ptr<FxCommandUndo> undo(new UndoPasteFxs(
      fxs, zeraryFxColumnSize, columns, pos, xshHandle, fxHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Add Paste Fxs  command
//**********************************************************************

class UndoAddPasteFxs : public UndoPasteFxs {
protected:
  TFxCommand::Link m_linkIn;  //!< Input link to be re-established on redo

public:
  UndoAddPasteFxs(TFx *inFx, const std::list<TFxP> &fxs,
                  const std::map<TFx *, int> &zeraryFxColumnSize,
                  const std::list<TXshColumnP> &columns,
                  TXsheetHandle *xshHandle, TFxHandle *fxHandle)
      : UndoPasteFxs(fxs, zeraryFxColumnSize, columns, TConst::nowhere,
                     xshHandle, fxHandle) {
    initialize(inFx);
  }

  void redo() const override;

  int getSize() const override { return sizeof(*this); }

private:
  void initialize(TFx *inFx);
};

//------------------------------------------------------

void UndoAddPasteFxs::initialize(TFx *inFx) {
  if (!(inFx && UndoPasteFxs::isConsistent())) return;

  // NOTE: Any zerary (shouldn't be there anyway) has already been
  //       moved to columns at this point

  TXsheet *xsh = m_xshHandle->getXsheet();

  // Ensure that inFx and outFx are not in a macro Fx
  if (::isInsideAMacroFx(inFx, xsh)) {
    m_fxs.clear(), m_columns.clear();
    return;
  }

  // Get the first fx to be inserted, and follow links down
  // (until an empty input port at index 0 is found)
  TFx *ifx = FxCommandUndo::leftmostConnectedFx(m_fxs.front().getPointer());

  // Then, we have the link to be established
  m_linkIn = TFxCommand::Link(inFx, ifx, 0);

  // Furthermore, clone the group stack from inFx into each inserted fx
  auto const clone_fun =
      static_cast<void (*)(TFx *, TFx *)>(FxCommandUndo::cloneGroupStack);
  for_each_fx([inFx, clone_fun](TFx *toFx) { clone_fun(inFx, toFx); });
}

//------------------------------------------------------

void UndoAddPasteFxs::redo() const {
  if (m_linkIn.m_inputFx) {
    TXsheet *xsh = m_xshHandle->getXsheet();

    // Further apply the stored link
    FxCommandUndo::attach(xsh, m_linkIn, false);

    // Copiare l'indice di gruppo dell'fx di input
    auto const copy_fun =
        static_cast<void (*)(TFx *, TFx *)>(FxCommandUndo::copyGroupEditLevel);
    for_each_fx([this, copy_fun](TFx *toFx) {
      copy_fun(m_linkIn.m_inputFx.getPointer(), toFx);
    });
  }

  UndoPasteFxs::redo();
}

//=============================================================

void TFxCommand::addPasteFxs(TFx *inFx, const std::list<TFxP> &fxs,
                             const std::map<TFx *, int> &zeraryFxColumnSize,
                             const std::list<TXshColumnP> &columns,
                             TXsheetHandle *xshHandle, TFxHandle *fxHandle) {
  std::unique_ptr<FxCommandUndo> undo(new UndoAddPasteFxs(
      inFx, fxs, zeraryFxColumnSize, columns, xshHandle, fxHandle));
  if (undo->isConsistent()) {
    // NOTE: (inFx == 0) is considered consistent, as long as
    // UndoPasteFxs::isConsistent()
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Insert Paste Fxs  command
//**********************************************************************

class UndoInsertPasteFxs final : public UndoAddPasteFxs {
  TFxCommand::Link m_linkOut;  //!< Output link to be re-established
                               //!< on redo
public:
  UndoInsertPasteFxs(const TFxCommand::Link &link, const std::list<TFxP> &fxs,
                     const std::map<TFx *, int> &zeraryFxColumnSize,
                     const std::list<TXshColumnP> &columns,
                     TXsheetHandle *xshHandle, TFxHandle *fxHandle)
      : UndoAddPasteFxs(link.m_inputFx.getPointer(), fxs, zeraryFxColumnSize,
                        columns, xshHandle, fxHandle) {
    initialize(link);
  }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

private:
  void initialize(const TFxCommand::Link &link);
};

//------------------------------------------------------

void UndoInsertPasteFxs::initialize(const TFxCommand::Link &link) {
  if (!UndoPasteFxs::isConsistent()) return;

  TXsheet *xsh = m_xshHandle->getXsheet();
  TFx *inFx    = link.m_inputFx.getPointer();
  TFx *outFx   = link.m_outputFx.getPointer();

  // Ensure consistency
  if (!(inFx && outFx) || ::isInsideAMacroFx(outFx, xsh)) {
    m_fxs.clear(), m_columns.clear();
    return;
  }

  // Get the first fx to be inserted, and follow links up
  // (to a no output fx)
  TFx *ofx = FxCommandUndo::rightmostConnectedFx(m_fxs.front().getPointer());

  // Now, store the appropriate output link
  m_linkOut = TFxCommand::Link(ofx, outFx, link.m_index);
}

//------------------------------------------------------

void UndoInsertPasteFxs::redo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Further apply the stored link pair
  FxCommandUndo::attach(xsh, m_linkOut, false);

  if (m_linkOut.m_index < 0)
    xsh->getFxDag()->removeFromXsheet(m_linkIn.m_inputFx.getPointer());

  UndoAddPasteFxs::redo();
}

//------------------------------------------------------

void UndoInsertPasteFxs::undo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Reattach the original configuration
  TFxCommand::Link orig(m_linkIn.m_inputFx, m_linkOut.m_outputFx,
                        m_linkOut.m_index);
  FxCommandUndo::attach(xsh, orig, false);

  UndoAddPasteFxs::undo();
}

//=============================================================

void TFxCommand::insertPasteFxs(const Link &link, const std::list<TFxP> &fxs,
                                const std::map<TFx *, int> &zeraryFxColumnSize,
                                const std::list<TXshColumnP> &columns,
                                TXsheetHandle *xshHandle, TFxHandle *fxHandle) {
  std::unique_ptr<FxCommandUndo> undo(new UndoInsertPasteFxs(
      link, fxs, zeraryFxColumnSize, columns, xshHandle, fxHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Replace Paste Fxs  command
//**********************************************************************

class UndoReplacePasteFxs final : public UndoAddPasteFxs {
  std::unique_ptr<DeleteFxOrColumnUndo> m_deleteFxUndo;

  TFx *m_fx, *m_rightmostFx;

public:
  UndoReplacePasteFxs(TFx *fx, const std::list<TFxP> &fxs,
                      const std::map<TFx *, int> &zeraryFxColumnSize,
                      const std::list<TXshColumnP> &columns,
                      TXsheetHandle *xshHandle, TFxHandle *fxHandle)
      : UndoAddPasteFxs(inFx(fx), fxs, zeraryFxColumnSize, columns, xshHandle,
                        fxHandle)
      , m_deleteFxUndo(new DeleteFxOrColumnUndo(fx, xshHandle, fxHandle))
      , m_fx(fx)
      , m_rightmostFx() {
    initialize();
  }

  bool isConsistent() const override {
    return UndoAddPasteFxs::isConsistent() && m_deleteFxUndo->isConsistent();
  }
  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

private:
  static TFx *inFx(const TFx *fx) {
    return (fx && fx->getInputPortCount() > 0) ? fx->getInputPort(0)->getFx()
                                               : 0;
  }

  void initialize();
};

//------------------------------------------------------

void UndoReplacePasteFxs::initialize() {
  if (m_fxs.empty()) return;

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Get the first fx to be inserted, and follow links up
  // (to a no output fx)
  m_rightmostFx =
      FxCommandUndo::rightmostConnectedFx(this->m_fxs.front().getPointer());

  // Then, add to the list of links to be inserted upon redo
  int ol, olCount = m_fx->getOutputConnectionCount();
  for (ol = 0; ol != olCount; ++ol) {
    TFxPort *port = m_fx->getOutputConnection(ol);
    TFx *ownerFx  = port->getOwnerFx();

    TCG_ASSERT(port && ownerFx, continue);

    int p = ::inputPortIndex(ownerFx, port);
    TCG_ASSERT(p < ownerFx->getInputPortCount(), continue);

    this->m_links.push_back(TFxCommand::Link(m_rightmostFx, ownerFx, p));
  }

  if (fxDag->getTerminalFxs()->containsFx(m_fx))
    this->m_links.push_back(
        TFxCommand::Link(m_rightmostFx, fxDag->getXsheetFx(), -1));
}

//------------------------------------------------------

void UndoReplacePasteFxs::redo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Deleting m_fx would attach its input to the xsheet, if m_fx is terminal.
  // In our case, however, it needs to be attached to the replacement fx - so,
  // let's detach m_fx from the xsheet
  fxDag->removeFromXsheet(m_fx);

  // Then, delete the fx and insert the replacement. Note that this order is
  // required to ensure the correct dag positions
  m_deleteFxUndo->redo();
  UndoAddPasteFxs::redo();
}

//------------------------------------------------------

void UndoReplacePasteFxs::undo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Remove m_lastFx's output connections - UndoAddPasteFxs would try to
  // redirect them to the replaced fx's input (due to the 'blind' detach
  // command)
  if (m_rightmostFx) {
    int ol, olCount = m_rightmostFx->getOutputConnectionCount();
    for (ol = olCount - 1; ol >= 0; --ol)
      if (TFxPort *port = m_rightmostFx->getOutputConnection(ol))
        port->setFx(0);

    fxDag->removeFromXsheet(m_rightmostFx);
  }

  // Reverse the applied commands. Again, the order prevents 'bumped' dag
  // positions

  UndoAddPasteFxs::undo();  // This would bridge the inserted fxs' inputs with
                            // their outputs
  m_deleteFxUndo->undo();  // This also re-establishes the original output links
}

//=============================================================

void TFxCommand::replacePasteFxs(TFx *inFx, const std::list<TFxP> &fxs,
                                 const std::map<TFx *, int> &zeraryFxColumnSize,
                                 const std::list<TXshColumnP> &columns,
                                 TXsheetHandle *xshHandle,
                                 TFxHandle *fxHandle) {
  std::unique_ptr<FxCommandUndo> undo(new UndoReplacePasteFxs(
      inFx, fxs, zeraryFxColumnSize, columns, xshHandle, fxHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Disconnect Fxs  command
//**********************************************************************

class UndoDisconnectFxs : public FxCommandUndo {
protected:
  std::list<TFxP> m_fxs;
  TFx *m_leftFx, *m_rightFx;

  // NOTE: Although we'll detach only 1 input link, fxs with dynamic ports still
  // require us
  // to store the whole ports configuration
  std::vector<TFxCommand::Link> m_undoLinksIn, m_undoLinksOut,
      m_undoTerminalLinks;
  std::vector<QPair<TFxP, TPointD>> m_undoDagPos, m_redoDagPos;

  TXsheetHandle *m_xshHandle;

public:
  UndoDisconnectFxs(const std::list<TFxP> &fxs,
                    const QList<QPair<TFxP, TPointD>> &oldFxPos,
                    TXsheetHandle *xshHandle)
      : m_fxs(fxs)
      , m_xshHandle(xshHandle)
      , m_undoDagPos(oldFxPos.begin(), oldFxPos.end()) {
    initialize();
  }

  bool isConsistent() const override { return !m_fxs.empty(); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Disconnect Fx"); }

private:
  void initialize();

  static void applyPos(const QPair<TFxP, TPointD> &pair) {
    pair.first->getAttributes()->setDagNodePos(pair.second);
  }

  static void attach(TXsheet *xsh, const TFxCommand::Link &link) {
    FxCommandUndo::attach(xsh, link, false);
  }
  static void detachXsh(TXsheet *xsh, const TFxCommand::Link &link) {
    xsh->getFxDag()->removeFromXsheet(link.m_inputFx.getPointer());
  }
};

//======================================================

void UndoDisconnectFxs::initialize() {
  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Don't deal with fxs inside a macro fx
  ::FilterInsideAMacro insideAMacro_fun = {xsh};
  if (std::find_if(m_fxs.begin(), m_fxs.end(), insideAMacro_fun) != m_fxs.end())
    m_fxs.clear();

  if (m_fxs.empty()) return;

  // Build fxs data
  auto const contains = [this](TFx const *fx) -> bool {
    return std::count_if(this->m_fxs.begin(), this->m_fxs.end(),
                         [fx](TFxP &f) { return f.getPointer() == fx; }) > 0;
  };

  m_leftFx =
      FxCommandUndo::leftmostConnectedFx(m_fxs.front().getPointer(), contains);
  m_rightFx =
      FxCommandUndo::rightmostConnectedFx(m_fxs.front().getPointer(), contains);

  // Store sensible original data for the undo
  m_undoLinksIn  = FxCommandUndo::inputLinks(xsh, m_leftFx);
  m_undoLinksOut = FxCommandUndo::outputLinks(xsh, m_rightFx);

  std::vector<TFxCommand::Link>::const_iterator lt, lEnd = m_undoLinksIn.end();
  for (lt = m_undoLinksIn.begin(); lt != lEnd; ++lt)
    if (fxDag->getTerminalFxs()->containsFx(lt->m_inputFx.getPointer()))
      m_undoTerminalLinks.push_back(TFxCommand::Link(lt->m_inputFx.getPointer(),
                                                     fxDag->getXsheetFx(), -1));

  std::vector<QPair<TFxP, TPointD>> v;
  for (auto const &e : m_undoDagPos) {
    v.emplace_back(e.first, e.first->getAttributes()->getDagNodePos());
  }
  m_redoDagPos = std::move(v);
  m_redoDagPos.shrink_to_fit();
}

//------------------------------------------------------

void UndoDisconnectFxs::redo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Detach the first port only - I'm not sure it should really be like this,
  // but it's
  // legacy, and altering it would require to revise the 'simulation' procedures
  // in fxschematicscene.cpp...  (any command simulation should be done here,
  // btw)
  FxCommandUndo::detachFxs(xsh, m_leftFx, m_rightFx, false);
  if (m_leftFx->getInputPortCount() > 0) m_leftFx->getInputPort(0)->setFx(0);

  // This is also convenient, since fxs with dynamic groups will maintain all
  // BUT 1
  // port - thus preventing us from dealing with that, since 1 port is always
  // available
  // on fxs with dynamic ports...

  std::for_each(m_redoDagPos.begin(), m_redoDagPos.end(), applyPos);

  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void UndoDisconnectFxs::undo() const {
  typedef void (*LinkFun)(TXsheet * xsh, const TFxCommand::Link &);

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Restore the old links
  auto const attacher = [xsh](const TFxCommand::Link &link) {
    return UndoDisconnectFxs::attach(xsh, link);
  };
  auto const xshDetacher = [xsh](const TFxCommand::Link &link) {
    return UndoDisconnectFxs::detachXsh(xsh, link);
  };

  std::for_each(m_undoLinksIn.begin(), m_undoLinksIn.end(), attacher);
  std::for_each(m_undoLinksOut.begin(), m_undoLinksOut.end(), attacher);

  std::for_each(m_undoLinksIn.begin(), m_undoLinksIn.end(), xshDetacher);
  std::for_each(m_undoTerminalLinks.begin(), m_undoTerminalLinks.end(),
                attacher);

  // Restore old positions
  std::for_each(m_undoDagPos.begin(), m_undoDagPos.end(), applyPos);

  m_xshHandle->notifyXsheetChanged();
}

//======================================================

void TFxCommand::disconnectFxs(const std::list<TFxP> &fxs,
                               TXsheetHandle *xshHandle,
                               const QList<QPair<TFxP, TPointD>> &fxPos) {
  std::unique_ptr<FxCommandUndo> undo(
      new UndoDisconnectFxs(fxs, fxPos, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Connect Fxs  command
//**********************************************************************

class UndoConnectFxs final : public UndoDisconnectFxs {
  struct GroupData;

private:
  TFxCommand::Link m_link;
  std::vector<GroupData> m_undoGroupDatas;

public:
  UndoConnectFxs(const TFxCommand::Link &link, const std::list<TFxP> &fxs,
                 const QList<QPair<TFxP, TPointD>> &fxPos,
                 TXsheetHandle *xshHandle)
      : UndoDisconnectFxs(fxs, fxPos, xshHandle), m_link(link) {
    initialize();
  }

  bool isConsistent() const override { return !m_fxs.empty(); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override;

private:
  void initialize();

  static void applyPos(const QPair<TFxP, TPointD> &pair) {
    pair.first->getAttributes()->setDagNodePos(pair.second);
  }
};

//======================================================

struct UndoConnectFxs::GroupData {
  TFx *m_fx;
  QStack<int> m_groupIds;
  QStack<std::wstring> m_groupNames;
  int m_editingGroup;

public:
  GroupData(TFx *fx);
  void restore() const;
};

//------------------------------------------------------

UndoConnectFxs::GroupData::GroupData(TFx *fx)
    : m_fx(fx)
    , m_groupIds(fx->getAttributes()->getGroupIdStack())
    , m_groupNames(fx->getAttributes()->getGroupNameStack())
    , m_editingGroup(fx->getAttributes()->getEditingGroupId()) {}

//------------------------------------------------------

void UndoConnectFxs::GroupData::restore() const {
  assert(!m_groupIds.empty());

  FxCommandUndo::cloneGroupStack(m_groupIds, m_groupNames, m_fx);
  FxCommandUndo::copyGroupEditLevel(m_editingGroup, m_fx);
}

//======================================================

void UndoConnectFxs::initialize() {
  if (!UndoDisconnectFxs::isConsistent()) return;

  TCG_ASSERT(m_link.m_inputFx && m_link.m_outputFx, m_fxs.clear(); return);

  // Store sensible original data for the undo
  m_undoGroupDatas.reserve(m_fxs.size());

  std::list<TFxP>::const_iterator ft, fEnd = m_fxs.end();
  for (ft = m_fxs.begin(); ft != fEnd; ++ft) {
    if ((*ft)->getAttributes()->isGrouped())
      m_undoGroupDatas.push_back(GroupData((*ft).getPointer()));
  }
}

//------------------------------------------------------

void UndoConnectFxs::redo() const {
  UndoDisconnectFxs::redo();

  TXsheet *xsh = m_xshHandle->getXsheet();

  // Apply the new links
  FxCommandUndo::insertFxs(xsh, m_link, m_leftFx, m_rightFx);

  // Copy the input fx's group data
  TFx *inFx = m_link.m_inputFx.getPointer();

  std::list<TFxP>::const_iterator ft, fEnd = m_fxs.end();
  for (ft = m_fxs.begin(); ft != fEnd; ++ft) {
    TFx *fx = (*ft).getPointer();

    FxCommandUndo::cloneGroupStack(inFx, fx);
    FxCommandUndo::copyGroupEditLevel(inFx, fx);
  }

  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void UndoConnectFxs::undo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  // Undo the insert
  FxCommandUndo::detachFxs(xsh, m_leftFx, m_rightFx);
  FxCommandUndo::attach(xsh, m_link, false);

  // Restore the old fxs' group data
  for (auto const &groupData : m_undoGroupDatas) {
    groupData.restore();
  }

  UndoDisconnectFxs::undo();
}

//------------------------------------------------------

QString UndoConnectFxs::getHistoryString() {
  return QObject::tr("Connect Fx : %1 - %2")
      .arg(QString::fromStdWString(m_leftFx->getName()))
      .arg(QString::fromStdWString(m_rightFx->getName()));
}

//======================================================

void TFxCommand::connectFxs(const Link &link, const std::list<TFxP> &fxs,
                            TXsheetHandle *xshHandle,
                            const QList<QPair<TFxP, TPointD>> &fxPos) {
  std::unique_ptr<FxCommandUndo> undo(
      new UndoConnectFxs(link, fxs, fxPos, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Set Parent  command
//**********************************************************************

class SetParentUndo final : public FxCommandUndo {
  TFxP m_oldFx, m_newFx, m_parentFx;
  int m_parentPort;

  bool m_removeFromXsheet;

  TXsheetHandle *m_xshHandle;

public:
  SetParentUndo(TFx *fx, TFx *parentFx, int parentFxPort,
                TXsheetHandle *xshHandle)
      : m_newFx(fx)
      , m_parentFx(parentFx)
      , m_parentPort(parentFxPort)
      , m_xshHandle(xshHandle) {
    initialize();
  }

  bool isConsistent() const override { return m_parentFx; }

  void redo() const override;
  void redo_() const;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

private:
  void initialize();
};

//------------------------------------------------------

void SetParentUndo::initialize() {
  if (!m_parentFx) return;

  // NOTE: We cannot store this directly, since it's the actual out that owns
  // the actual in, not vice versa
  TFx *parentFx = ::getActualIn(m_parentFx.getPointer());

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  assert(m_parentPort < parentFx->getInputPortCount());
  assert(m_parentPort >= 0);

  m_oldFx = parentFx->getInputPort(m_parentPort)->getFx();

  m_removeFromXsheet =  // This is a bit odd. The legacy behavior of the
      (m_newFx &&       // setParent() (ie connect 2 fxs with a link, I know
       (m_newFx->getOutputConnectionCount() ==
        0) &&  // the name is bad) command is that of *removing terminal
       fxDag->getTerminalFxs()->containsFx(
           m_newFx.getPointer()) &&  // links* on the input fx (m_newFx in our
                                     // case).
       m_parentFx != fxDag->getXsheetFx());  // I've retained this behavior
                                             // since it can be handy
  // for users, but only if the xsheet link is the SOLE output
  if (::isInsideAMacroFx(m_parentFx.getPointer(), xsh) ||  // link.
      ::isInsideAMacroFx(m_oldFx.getPointer(), xsh) ||
      ::isInsideAMacroFx(m_newFx.getPointer(), xsh))
    m_parentFx = TFxP();
}

//------------------------------------------------------

void SetParentUndo::redo() const {
  /*
Due to compatibility issues from *schematicnode.cpp files, the "do" operation
cannot
signal changes to the scene/xsheet... but the *re*do must.
*/

  redo_();
  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void SetParentUndo::redo_() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  TFx *parentFx = ::getActualIn(m_parentFx.getPointer());
  FxCommandUndo::attach(xsh, m_newFx.getPointer(), parentFx, m_parentPort,
                        false);

  if (m_removeFromXsheet)
    xsh->getFxDag()->removeFromXsheet(m_newFx.getPointer());
}

//------------------------------------------------------

void SetParentUndo::undo() const {
  TXsheet *xsh = m_xshHandle->getXsheet();

  TFx *parentFx = ::getActualIn(m_parentFx.getPointer());
  FxCommandUndo::attach(xsh, m_oldFx.getPointer(), parentFx, m_parentPort,
                        false);

  if (m_removeFromXsheet) xsh->getFxDag()->addToXsheet(m_newFx.getPointer());

  m_xshHandle->notifyXsheetChanged();
}

//======================================================

void TFxCommand::setParent(TFx *fx, TFx *parentFx, int parentFxPort,
                           TXsheetHandle *xshHandle) {
  if (dynamic_cast<TXsheetFx *>(parentFx) || parentFxPort < 0) {
    std::unique_ptr<ConnectNodesToXsheetUndo> undo(
        new ConnectNodesToXsheetUndo(std::list<TFxP>(1, fx), xshHandle));
    if (undo->isConsistent()) {
      undo->redo_();
      TUndoManager::manager()->add(undo.release());
    }
  } else {
    std::unique_ptr<SetParentUndo> undo(
        new SetParentUndo(fx, parentFx, parentFxPort, xshHandle));
    if (undo->isConsistent()) {
      undo->redo_();
      TUndoManager::manager()->add(undo.release());
    }
  }
}

//**********************************************************************
//    Rename Fx  command
//**********************************************************************

class UndoRenameFx final : public FxCommandUndo {
  TFxP m_fx;
  std::wstring m_newName, m_oldName;

  TXsheetHandle *m_xshHandle;

public:
  UndoRenameFx(TFx *fx, const std::wstring &newName, TXsheetHandle *xshHandle)
      : m_fx(fx)
      , m_newName(newName)
      , m_oldName(::getActualIn(fx)->getName())
      , m_xshHandle(xshHandle) {
    assert(fx);
  }

  bool isConsistent() const override { return true; }

  void redo() const override {
    redo_();
    m_xshHandle->notifyXsheetChanged();
  }

  void redo_() const { ::getActualIn(m_fx.getPointer())->setName(m_newName); }

  void undo() const override {
    ::getActualIn(m_fx.getPointer())->setName(m_oldName);
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Rename Fx : %1 > %2")
        .arg(QString::fromStdWString(m_oldName))
        .arg(QString::fromStdWString(m_newName));
  }
};

//======================================================

void TFxCommand::renameFx(TFx *fx, const std::wstring &newName,
                          TXsheetHandle *xshHandle) {
  if (!fx) return;

  std::unique_ptr<UndoRenameFx> undo(new UndoRenameFx(fx, newName, xshHandle));
  if (undo->isConsistent()) {
    undo->redo_();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Group Fxs  command
//**********************************************************************

class UndoGroupFxs : public FxCommandUndo {
public:
  struct GroupData {
    TFxP m_fx;
    mutable int m_groupIndex;  //! AKA group \a position (not \a id).

    GroupData(const TFxP &fx, int groupIdx = -1)
        : m_fx(fx), m_groupIndex(groupIdx) {}
  };

protected:
  std::vector<GroupData> m_groupData;
  int m_groupId;

  TXsheetHandle *m_xshHandle;

public:
  UndoGroupFxs(const std::list<TFxP> &fxs, TXsheetHandle *xshHandle)
      : m_groupData(fxs.begin(), fxs.end()), m_xshHandle(xshHandle) {
    initialize();
  }

  bool isConsistent() const override { return !m_groupData.empty(); }

  void redo() const override;
  void undo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Group Fx"); }

protected:
  UndoGroupFxs(int groupId, TXsheetHandle *xshHandle)
      : m_groupId(groupId), m_xshHandle(xshHandle) {}

private:
  void initialize();
};

//------------------------------------------------------

void UndoGroupFxs::initialize() {
  struct locals {
    inline static bool isXsheetFx(const GroupData &gd) {
      return dynamic_cast<TXsheet *>(gd.m_fx.getPointer());
    }
  };

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // Build a group id for the new group
  m_groupId = fxDag->getNewGroupId();

  // The xsheet fx must never be grouped
  m_groupData.erase(std::remove_if(m_groupData.begin(), m_groupData.end(),
                                   &locals::isXsheetFx),
                    m_groupData.end());

  // Scan for macro fxs. A macro's internal fxs must be added to the group data,
  // too
  // Yep, this is one of the few fx commands that do not require macro
  // explosion.
  size_t g, gCount = m_groupData.size();
  for (g = 0; g != gCount; ++g) {
    if (TMacroFx *macro =
            dynamic_cast<TMacroFx *>(m_groupData[g].m_fx.getPointer())) {
      const std::vector<TFxP> &internalFxs = macro->getFxs();

      std::vector<TFxP>::const_iterator ft, fEnd = internalFxs.end();
      for (ft = internalFxs.begin(); ft != fEnd; ++ft)
        m_groupData.push_back(*ft);
    }
  }
}

//------------------------------------------------------

void UndoGroupFxs::redo() const {
  const std::wstring groupName = L"Group " + std::to_wstring(m_groupId);

  std::vector<GroupData>::const_iterator gt, gEnd = m_groupData.end();
  for (gt = m_groupData.begin(); gt != gEnd; ++gt) {
    // Insert the group id in the fx
    gt->m_groupIndex = gt->m_fx->getAttributes()->setGroupId(m_groupId);
    gt->m_fx->getAttributes()->setGroupName(groupName);
  }

  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void UndoGroupFxs::undo() const {
  std::vector<GroupData>::const_iterator gt, gEnd = m_groupData.end();
  for (gt = m_groupData.begin(); gt != gEnd; ++gt) {
    TCG_ASSERT(gt->m_groupIndex >= 0, continue);

    // Insert the group id in the fx
    gt->m_fx->getAttributes()->removeGroupId(gt->m_groupIndex);
    gt->m_fx->getAttributes()->removeGroupName(gt->m_groupIndex);

    gt->m_groupIndex = -1;
  }

  m_xshHandle->notifyXsheetChanged();
}

//======================================================

void TFxCommand::groupFxs(const std::list<TFxP> &fxs,
                          TXsheetHandle *xshHandle) {
  std::unique_ptr<FxCommandUndo> undo(new UndoGroupFxs(fxs, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Ungroup Fxs  command
//**********************************************************************

class UndoUngroupFxs final : public UndoGroupFxs {
public:
  UndoUngroupFxs(int groupId, TXsheetHandle *xshHandle)
      : UndoGroupFxs(groupId, xshHandle) {
    initialize();
  }

  void redo() const override { UndoGroupFxs::undo(); }
  void undo() const override { UndoGroupFxs::redo(); }

  QString getHistoryString() override { return QObject::tr("Ungroup Fx"); }

private:
  void initialize();
};

//------------------------------------------------------

void UndoUngroupFxs::initialize() {
  struct {
    UndoUngroupFxs *m_this;

    void scanFxForGroup(TFx *fx) {
      if (fx) {
        const QStack<int> &groupStack = fx->getAttributes()->getGroupIdStack();

        int groupIdx =
            groupStack.indexOf(m_this->m_groupId);  // Returns -1 if not found
        if (groupIdx >= 0)
          m_this->m_groupData.push_back(GroupData(fx, groupIdx));
      }
    }

  } locals = {this};

  TXsheet *xsh = m_xshHandle->getXsheet();
  FxDag *fxDag = xsh->getFxDag();

  // We must iterate the xsheet's fxs pool and look for fxs with the specified
  // group id

  // Search column fxs
  int c, cCount = xsh->getColumnCount();
  for (c = 0; c != cCount; ++c) {
    TXshColumn *column = xsh->getColumn(c);
    assert(column);

    locals.scanFxForGroup(column->getFx());
  }

  // Search normal fxs (not column ones)
  TFxSet *internalFxs = fxDag->getInternalFxs();

  int f, fCount = internalFxs->getFxCount();
  for (f = 0; f != fCount; ++f) {
    TFx *fx = internalFxs->getFx(f);
    locals.scanFxForGroup(fx);

    if (TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx)) {
      // Search internal macro fxs
      const std::vector<TFxP> &fxs = macroFx->getFxs();

      std::vector<TFxP>::const_iterator ft, fEnd = fxs.end();
      for (ft = fxs.begin(); ft != fEnd; ++ft)
        locals.scanFxForGroup(ft->getPointer());
    }
  }

  // Search output fxs
  int o, oCount = fxDag->getOutputFxCount();
  for (o = 0; o != oCount; ++o) locals.scanFxForGroup(fxDag->getOutputFx(o));
}

//======================================================

void TFxCommand::ungroupFxs(int groupId, TXsheetHandle *xshHandle) {
  std::unique_ptr<FxCommandUndo> undo(new UndoUngroupFxs(groupId, xshHandle));
  if (undo->isConsistent()) {
    undo->redo();
    TUndoManager::manager()->add(undo.release());
  }
}

//**********************************************************************
//    Rename Group  command
//**********************************************************************

class UndoRenameGroup final : public FxCommandUndo {
  std::vector<UndoGroupFxs::GroupData> m_groupData;
  std::wstring m_oldGroupName, m_newGroupName;

  TXsheetHandle *m_xshHandle;

public:
  UndoRenameGroup(const std::list<TFxP> &fxs, const std::wstring &newGroupName,
                  bool fromEditor, TXsheetHandle *xshHandle)
      : m_groupData(fxs.begin(), fxs.end())
      , m_newGroupName(newGroupName)
      , m_xshHandle(xshHandle) {
    initialize(fromEditor);
  }

  bool isConsistent() const override { return !m_groupData.empty(); }

  void redo() const override;
  void undo() const override;

  void redo_() const;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Rename Group  : %1 > %2")
        .arg(QString::fromStdWString(m_oldGroupName))
        .arg(QString::fromStdWString(m_newGroupName));
  }

private:
  void initialize(bool fromEditor);
};

//------------------------------------------------------

void UndoRenameGroup::initialize(bool fromEditor) {
  struct locals {
    inline static bool isInvalid(const UndoGroupFxs::GroupData &gd) {
      return (gd.m_groupIndex < 0);
    }
  };

  if (!m_groupData.empty()) {
    m_oldGroupName =
        m_groupData.front().m_fx->getAttributes()->getGroupName(fromEditor);

    // Extract group indices
    std::vector<UndoGroupFxs::GroupData>::const_iterator gt,
        gEnd = m_groupData.end();
    for (gt = m_groupData.begin(); gt != gEnd; ++gt) {
      const QStack<std::wstring> &groupNamesStack =
          gt->m_fx->getAttributes()->getGroupNameStack();

      gt->m_groupIndex =
          groupNamesStack.indexOf(m_oldGroupName);  // Returns -1 if not found
      assert(gt->m_groupIndex >= 0);
    }
  }

  m_groupData.erase(std::remove_if(m_groupData.begin(), m_groupData.end(),
                                   &locals::isInvalid),
                    m_groupData.end());
}

//------------------------------------------------------

void UndoRenameGroup::redo() const {
  redo_();
  m_xshHandle->notifyXsheetChanged();
}

//------------------------------------------------------

void UndoRenameGroup::redo_() const {
  std::vector<UndoGroupFxs::GroupData>::const_iterator gt,
      gEnd = m_groupData.end();
  for (gt = m_groupData.begin(); gt != gEnd; ++gt) {
    gt->m_fx->getAttributes()->removeGroupName(gt->m_groupIndex);
    gt->m_fx->getAttributes()->setGroupName(m_newGroupName, gt->m_groupIndex);
  }
}

//------------------------------------------------------

void UndoRenameGroup::undo() const {
  std::vector<UndoGroupFxs::GroupData>::const_iterator gt,
      gEnd = m_groupData.end();
  for (gt = m_groupData.begin(); gt != gEnd; ++gt) {
    gt->m_fx->getAttributes()->removeGroupName(gt->m_groupIndex);
    gt->m_fx->getAttributes()->setGroupName(m_oldGroupName, gt->m_groupIndex);
  }

  m_xshHandle->notifyXsheetChanged();
}

//======================================================

void TFxCommand::renameGroup(const std::list<TFxP> &fxs,
                             const std::wstring &name, bool fromEditor,
                             TXsheetHandle *xshHandle) {
  std::unique_ptr<UndoRenameGroup> undo(
      new UndoRenameGroup(fxs, name, fromEditor, xshHandle));
  if (undo->isConsistent()) {
    undo->redo_();  // Same schematic nodes problem as above...   :(
    TUndoManager::manager()->add(undo.release());
  }
}
