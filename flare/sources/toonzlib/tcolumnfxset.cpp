

#include "toonz/tcolumnfxset.h"

// TnzLib includes
#include "toonz/tcolumnfx.h"

// TnzBase includes
#include "tfx.h"

// TnzCore includes
#include "tstream.h"
#include "texception.h"

TFxSet::TFxSet()
//: m_xsheetFx(0)
{}

TFxSet::~TFxSet() {
  // delete m_xsheetFx;
  clear();
}

void TFxSet::addFx(TFx *fx) {
  std::set<TFx *>::iterator it = m_fxs.find(fx);
  if (it == m_fxs.end()) {
    fx->addRef();
    m_fxs.insert(fx);
    fx->setNewIdentifier();
  }
}

void TFxSet::getFxs(std::set<TFx *> &fxs) {
  fxs.insert(m_fxs.begin(), m_fxs.end());
}

void TFxSet::saveData(TOStream &os, int occupiedColumnCount) {
  std::set<TFx *>::iterator it;
  for (it = m_fxs.begin(); it != m_fxs.end(); ++it) {
    TRasterFx *rasterFx = dynamic_cast<TRasterFx *>(*it);
    if (rasterFx) {
      TColumnFx *columnFx = dynamic_cast<TColumnFx *>(rasterFx);
      if (columnFx && (columnFx->getColumnIndex() == -1 ||
                       columnFx->getColumnIndex() >= occupiedColumnCount))
        continue;
    }
    os.openChild("fxnode");
    os << (*it);
    os.closeChild();
  }
}

int TFxSet::getFxCount() const { return m_fxs.size(); }

bool TFxSet::removeFx(TFx *fx) {
  std::set<TFx *>::iterator it = m_fxs.find(fx);
  if (it != m_fxs.end()) {
    TFx *fx = *it;
    fx->release();
    m_fxs.erase(fx);
    return true;
  } else
    return false;
}

TFx *TFxSet::getFx(int index) const {
  assert(0 <= index && index < getFxCount());
  std::set<TFx *>::const_iterator it = m_fxs.begin();
  std::advance(it, index);
  return *it;
}

TFx *TFxSet::getFx(const std::string &id) const {
  std::set<TFx *>::const_iterator it;
  for (it = m_fxs.begin(); it != m_fxs.end(); ++it) {
    //     TFx *fx = *it;
    // if (fx->getId() == id)
    //  return fx;
  }
  return 0;
}

void TFxSet::clear() {
  std::set<TFx *>::iterator it;
  for (it = m_fxs.begin(); it != m_fxs.end(); ++it) {
    TFx *fx = *it;
    fx->release();
  }

  m_fxs.clear();
}

void TFxSet::loadData(TIStream &is) {
  clear();

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "fxnode") {
      TPersist *p = 0;
      is >> p;

      // NOTE: In current implementation p is sharedly owned by is - it's
      // automatically
      //       released upon stream destruction if the below assignment fails

      if (TFx *fx = dynamic_cast<TFx *>(p)) {
        addFx(fx);
      }
    } else {
      throw TException("TFxSet, unknown tag: " + tagName);
    }
    is.closeChild();
  }
}

static TFx *getActualFx(TFx *fx) {
  // Zerary fxs and zerary COLUMN fxs are separate, and fx port connections
  // are stored in the actual zerary fx.
  // It's sad - couldn't we do something about it?

  TZeraryColumnFx *zeraryColumnFx = dynamic_cast<TZeraryColumnFx *>(fx);
  return zeraryColumnFx ? zeraryColumnFx->getZeraryFx() : fx;
}

TFx *searchFx(const std::map<TFx *, TFx *> &table, TFx *fx) {
  std::map<TFx *, TFx *>::const_iterator it = table.find(fx);
  return it == table.end() ? 0 : it->second;
}

void updateFxLinks(const std::map<TFx *, TFx *> &table) {
  // The input table is made of (original, cloned) couples.
  // The purpose of this function is that of replicating the hierarchycal
  // structure from the first fxs to the seconds.

  std::map<TFx *, TFx *>::const_iterator it;
  for (it = table.begin(); it != table.end(); ++it) {
    TFx *fx = getActualFx(it->first), *fx2 = getActualFx(it->second);
    if (!fx || !fx2) continue;

    for (int i = 0; i < fx->getInputPortCount(); ++i) {
      TFx *inputFx  = fx->getInputPort(i)->getFx();
      TFx *inputFx2 = 0;

      if (inputFx) {
        inputFx2 = searchFx(table, inputFx);
        inputFx  = getActualFx(inputFx);

        // Normally, the above should be enough. However, it seems that
        // the possibility of the input table being 'incomplete' must be dealt
        // with
        // (why is it not asserted >_< !)

        while (inputFx && !inputFx2 && (inputFx->getInputPortCount() > 0)) {
          // So, well... this block tries to delve deeper in the hierarchy
          // until a suitable mapped children is found... Yeah, sure...

          TFxPort *port = inputFx->getInputPort(0);
          inputFx       = port->getFx();

          inputFx2 = searchFx(table, inputFx);
          inputFx  = getActualFx(inputFx);
        }
      }

      if (inputFx2) fx2->getInputPort(i)->setFx(inputFx2);
    }
  }
}
