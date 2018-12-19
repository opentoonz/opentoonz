

#include "toonz/fxdag.h"

// TnzLib includes
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"
#include "tw/stringtable.h"

// TnzBase includes
#include "tmacrofx.h"
#include "tfxattributes.h"

// TnzCore includes
#include "tstream.h"

// STD includes
//#include <cwctypes>

//===================================================================

FxDag::FxDag()
    : m_internalFxs(new TFxSet())
    , m_terminalFxs(new TFxSet())
    , m_groupIdCount(0)
    , m_dagGridDimension(eSmall) {
  TXsheetFx *xsheetFx = new TXsheetFx;
  xsheetFx->setFxDag(this);
  m_xsheetFx = xsheetFx;
  m_xsheetFx->addRef();
  m_xsheetFx->setNewIdentifier();
  addOutputFx();
  m_outputFxs[0]->getInputPort(0)->setFx(m_xsheetFx);
}

//-------------------------------------------------------------------

FxDag::~FxDag() {
  delete m_internalFxs;
  delete m_terminalFxs;
  m_xsheetFx->release();
  int i;
  for (i = 0; i < (int)m_outputFxs.size(); i++) m_outputFxs[i]->release();
}

//-------------------------------------------------------------------

TOutputFx *FxDag::addOutputFx(TOutputFx *outputFx) {
  if (!outputFx) outputFx = new TOutputFx();
  outputFx->addRef();
  m_xsheetFx->setNewIdentifier();
  assert(outputFx->getInputPortCount() == 1);
  m_outputFxs.push_back(outputFx);
  return outputFx;
}

//-------------------------------------------------------------------

void FxDag::removeOutputFx(TOutputFx *fx) {
  assert(fx);
  if (m_outputFxs.size() == 1) return;
  if (std::find(m_outputFxs.begin(), m_outputFxs.end(), fx) ==
      m_outputFxs.end())
    return;
  m_outputFxs.erase(std::remove(m_outputFxs.begin(), m_outputFxs.end(), fx),
                    m_outputFxs.end());
  fx->release();
}

//-------------------------------------------------------------------

TFxSet *FxDag::getInternalFxs() const { return m_internalFxs; }

//-------------------------------------------------------------------

void FxDag::setCurrentOutputFx(TOutputFx *fx) {
  std::vector<TOutputFx *>::iterator it;
  it = std::find(m_outputFxs.begin(), m_outputFxs.end(), fx);
  if (it == m_outputFxs.end()) return;
  if (it == m_outputFxs.begin()) return;
  std::swap(*it, *m_outputFxs.begin());
}

TOutputFx *FxDag::getCurrentOutputFx() const {
  assert(!m_outputFxs.empty());
  return m_outputFxs[0];
}

//-------------------------------------------------------------------

bool FxDag::checkLoop(TFx *inputFx, TFx *fx) {
  if (inputFx == fx) return true;
  if (dynamic_cast<TXsheetFx *>(inputFx)) {
    TFxSet *terminals = getTerminalFxs();
    for (int i = 0; i < terminals->getFxCount(); i++) {
      TFx *tfx = terminals->getFx(i);
      if (tfx && checkLoop(tfx, fx)) return true;
    }
  } else {
    if (TZeraryColumnFx *zerary = dynamic_cast<TZeraryColumnFx *>(inputFx))
      inputFx = zerary->getZeraryFx();
    for (int i = 0; i < inputFx->getInputPortCount(); i++) {
      TFx *ifx = inputFx->getInputPort(i)->getFx();
      if (ifx && checkLoop(ifx, fx)) return true;
    }
  }
  return false;
}

//-------------------------------------------------------------------

TFxSet *FxDag::getTerminalFxs() const { return m_terminalFxs; }

//-------------------------------------------------------------------

void FxDag::addToXsheet(TFx *fx) { m_terminalFxs->addFx(fx); }

//-------------------------------------------------------------------

void FxDag::removeFromXsheet(TFx *fx) { m_terminalFxs->removeFx(fx); }

//-------------------------------------------------------------------

namespace {
struct NotAlnum {
  bool operator()(wint_t val) const { return !iswalnum(val); }
};
}  // namespace

void FxDag::assignUniqueId(TFx *fx) {
  struct locals {
    static void eraseNonAlnums(std::wstring &str) {
      str.erase(std::remove_if(str.begin(), str.end(), NotAlnum()), str.end());
    }
  };  // locals

  std::string type = fx->getFxType();
  int count        = ++m_typeTable[type];

  fx->getAttributes()->setId(count);

  std::wstring name = TStringTable::translate(type);
  locals::eraseNonAlnums(
      name);  // fx ids are used as XML tag names - thus, we'll restrict
              // the char set to alnums. Specifically, '/' must be ruled out.
              // E.g.: "Erode/Dilate 1" must become "ErodeDilate1"
  name = name + QString::number(count).rightJustified(2, '0').toStdWString();

  if (fx->getName() == L"") fx->setName(name);
  fx->setFxId(name);
  m_idTable[toLower(name)] = fx;
}

//-------------------------------------------------------------------

TFx *FxDag::getFxById(std::wstring id) const {
  std::map<std::wstring, TFx *>::const_iterator it = m_idTable.find(id);
  if (it == m_idTable.end())
    return 0;
  else
    return it->second;
}
//-------------------------------------------------------------------

void FxDag::updateFxTypeTable(TFx *fx, int value) {
  std::string type  = fx->getFxType();
  m_typeTable[type] = value;
}

//-------------------------------------------------------------------

void FxDag::updateFxIdTable(TFx *fx) { m_idTable[toLower(fx->getFxId())] = fx; }

//-------------------------------------------------------------------

int FxDag::getFxTypeCount(TFx *fx) {
  std::string type = fx->getFxType();
  std::map<std::string, int>::iterator it = m_typeTable.find(type);
  if (it == m_typeTable.end()) return 0;
  return it->second;
}

//-------------------------------------------------------------------

void FxDag::getFxs(std::vector<TFx *> &fxs) const {
  std::set<TFx *> fxSet;
  getInternalFxs()->getFxs(fxSet);
  fxs.insert(fxs.end(), fxSet.begin(), fxSet.end());
}

//-------------------------------------------------------------------

bool FxDag::isRendered(TFx *fx) const {
  if (m_terminalFxs->containsFx(fx)) return true;
  if (dynamic_cast<TOutputFx *>(fx)) return true;
  int i;
  for (i = 0; i < fx->getOutputConnectionCount(); i++) {
    TFx *outFx = fx->getOutputConnection(i)->getOwnerFx();
    if (outFx && isRendered(outFx)) return true;
  }
  return false;
}

//-------------------------------------------------------------------

bool FxDag::isControl(TFx *fx) const {
  if (m_terminalFxs->containsFx(fx)) return false;
  if (dynamic_cast<TOutputFx *>(fx)) return false;
  int i;
  for (i = 0; i < fx->getOutputConnectionCount(); i++) {
    TFxPort *port = fx->getOutputConnection(i);
    TFx *outFx    = port->getOwnerFx();
    if (outFx) {
      if (outFx->getInputPort(0) != port) return true;
      if (isControl(outFx)) return true;
    }
  }
  return false;
}

//-------------------------------------------------------------------

namespace {
TFx *search(const std::map<TFx *, TFx *> &table, TFx *fx) {
  std::map<TFx *, TFx *>::const_iterator it = table.find(fx);
  return it == table.end() ? 0 : it->second;
}
}

//-------------------------------------------------------------------

void FxDag::saveData(TOStream &os, int occupiedColumnCount) {
  if (getInternalFxs()->getFxCount() > 0) {
    os.openChild("internal");
    getInternalFxs()->saveData(os, occupiedColumnCount);
    os.closeChild();
  }
  if (getTerminalFxs()->getFxCount() > 0) {
    os.openChild("terminal");
    getTerminalFxs()->saveData(os, occupiedColumnCount);
    os.closeChild();
  }
  os.child("xsheet") << m_xsheetFx;
  int k;
  for (k = 0; k < (int)m_outputFxs.size(); k++)
    os.child("output") << m_outputFxs[k];
  os.child("grid_dimension") << (int)m_dagGridDimension;
}

//-------------------------------------------------------------------

void FxDag::loadData(TIStream &is) {
  VersionNumber tnzVersion = is.getVersion();
  int k;
  for (k = 0; k < (int)m_outputFxs.size(); k++) m_outputFxs[k]->release();
  m_outputFxs.clear();
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "terminal") {
      TFxSet *fxSet = getTerminalFxs();
      fxSet->loadData(is);
      int i;
      for (i = 0; i < fxSet->getFxCount(); i++) {
        TFx *fx = fxSet->getFx(i);
        if (fx->getAttributes()->isGrouped() &&
            m_groupIdCount < fx->getAttributes()->getGroupId())
          m_groupIdCount = fx->getAttributes()->getGroupId();
        if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx))
          fx = zfx->getZeraryFx();
        if (tnzVersion < VersionNumber(1, 16)) {
          std::wstring app = fx->getName();
          assignUniqueId(fx);
          fx->setName(app);
          continue;
        }
        int fxTypeCount = getFxTypeCount(fx);
        int maxFxTypeId = std::max(fxTypeCount, fx->getAttributes()->getId());
        updateFxTypeTable(fx, maxFxTypeId);
        TMacroFx *macroFx = dynamic_cast<TMacroFx *>(fx);
        if (macroFx) {
          std::vector<TFxP> fxs = macroFx->getFxs();
          int j;
          for (j = 0; j < (int)fxs.size(); j++) {
            TFxP inMacroFx = fxs[j];
            fxTypeCount    = getFxTypeCount(inMacroFx.getPointer());
            maxFxTypeId =
                std::max(fxTypeCount, inMacroFx->getAttributes()->getId());
            updateFxTypeTable(inMacroFx.getPointer(), maxFxTypeId);
            m_idTable[toLower(inMacroFx->getFxId())] = inMacroFx.getPointer();
          }
        }
      }
    } else if (tagName == "internal") {
      TFxSet *fxSet = getInternalFxs();
      fxSet->loadData(is);
      int i;
      for (i = 0; i < fxSet->getFxCount(); i++) {
        TFx *fx = fxSet->getFx(i);
        if (fx->getAttributes()->isGrouped() &&
            m_groupIdCount < fx->getAttributes()->getGroupId())
          m_groupIdCount = fx->getAttributes()->getGroupId();
        if (TZeraryColumnFx *zfx = dynamic_cast<TZeraryColumnFx *>(fx))
          fx = zfx->getZeraryFx();
        if (tnzVersion < VersionNumber(1, 16)) {
          std::wstring app = fx->getName();
          assignUniqueId(fx);
          fx->setName(app);
          continue;
        }
        int fxTypeCount = getFxTypeCount(fx);
        int maxFxTypeId = std::max(fxTypeCount, fx->getAttributes()->getId());
        updateFxTypeTable(fx, maxFxTypeId);
        m_idTable[toLower(fx->getFxId())] = fx;
      }
    } else if (tagName == "xsheet" || tagName == "output") {
      TPersist *p = 0;
      is >> p;
      TFx *fx = dynamic_cast<TFx *>(p);
      if (!fx) throw TException("FxDag. unexpeced Fx");
      fx->addRef();
      fx->setNewIdentifier();
      if (tagName == "xsheet") {
        m_xsheetFx->release();
        m_xsheetFx = fx;

        TXsheetFx *xsheetFx = dynamic_cast<TXsheetFx *>(fx);
        if (xsheetFx) xsheetFx->setFxDag(this);
      } else {
        TOutputFx *outputFx = dynamic_cast<TOutputFx *>(fx);
        if (outputFx) m_outputFxs.push_back(outputFx);
      }
    } else if (tagName == "grid_dimension") {
      is >> m_dagGridDimension;
    } else
      throw TException("FxDag. unexpeced tag: " + tagName);
    is.closeChild();
  }
}
