

#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tstageobject.h"
#include "tgrammar.h"
#include "ttokenizer.h"
#include "tconvert.h"
#include "tunit.h"
#include "tstream.h"

#include "toonz/txsheet.h"
#include "toonz/txshcell.h"

#include "toonz/txsheetexpr.h"

using namespace TSyntax;

//=============================================================================
//! The struct TStageObjectTree::TStageObjectTreeImp contains all element
//! necessary to define a pegbar tree.

struct TStageObjectTree::TStageObjectTreeImp {
  enum DagGridDimension { eLarge = 0, eSmall = 1 };

  //! The map contains generic pegbar of pegbar tree.
  std::map<TStageObjectId, TStageObject *> m_pegbarTable;

  //! Define pegbar tree current camera .
  TStageObjectId m_currentCameraId;
  TStageObjectId m_currentPreviewCameraId;

  //! Allows to manager pegbar handle.
  HandleManager *m_handleManager;

  //! The vector contains all spline \b TStageObjectSpline of pegbar tree.
  std::map<int, TStageObjectSpline *> m_splines;

  int m_cameraCount;
  int m_groupIdCount;
  int m_splineCount;
  int m_dagGridDimension;

  Grammar *m_grammar;

  /*!
Constructs a TStageObjectTreeImp with default value.
*/
  TStageObjectTreeImp();
  /*!
Destroys the TStageObjectTreeImp object.
*/
  ~TStageObjectTreeImp();

  // not implemented
  TStageObjectTreeImp(const TStageObjectTreeImp &);
  TStageObjectTreeImp &operator=(const TStageObjectTreeImp &);
};

//-----------------------------------------------------------------------------

TStageObjectTree::TStageObjectTreeImp::TStageObjectTreeImp()
    : m_currentCameraId(TStageObjectId::CameraId(0))
    , m_currentPreviewCameraId(TStageObjectId::CameraId(0))
    , m_handleManager(0)
    , m_cameraCount(0)
    , m_groupIdCount(0)
    , m_splineCount(0)
    , m_grammar(0)
    , m_dagGridDimension(eSmall) {}

//-----------------------------------------------------------------------------

TStageObjectTree::TStageObjectTreeImp::~TStageObjectTreeImp() {
  std::map<TStageObjectId, TStageObject *>::iterator i;
  for (i = m_pegbarTable.begin(); i != m_pegbarTable.end(); ++i)
    i->second->release();
  std::map<int, TStageObjectSpline *>::iterator it;
  for (it = m_splines.begin(); it != m_splines.end(); it++)
    it->second->release();
  delete m_grammar;
}

//=============================================================================
// TStageObjectTree

TStageObjectTree::TStageObjectTree() : m_imp(new TStageObjectTreeImp) {
  getStageObject(TStageObjectId::CameraId(0), true);
  getStageObject(TStageObjectId::TableId, true);

/*
  static bool firstTime = true;
  if(firstTime)
  {
    firstTime = false;
    Grammar*grammar = ExprGrammar::instance();
    grammar->addPattern("leaf", new ParamReferencePattern());
  }
*/
#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------

TStageObjectTree::~TStageObjectTree() {}

//-----------------------------------------------------------------------------

void TStageObjectTree::checkIntegrity() {
  std::map<TStageObjectId, TStageObject *> &pegbars     = m_imp->m_pegbarTable;
  std::map<TStageObjectId, TStageObject *>::iterator it = pegbars.begin();
  int minColumnIndex = 0;
  int maxColumnIndex = -1;
  std::set<int> columnIndexTable;

  int cameraCount = 0;
  for (; it != pegbars.end(); ++it) {
    TStageObjectId id = it->first;
    TStageObject *imp = it->second;
    assert(imp->getId() == id);
    if (id.isColumn()) {
      int index = id.getIndex();
      if (minColumnIndex > maxColumnIndex)
        minColumnIndex = maxColumnIndex = index;
      else {
        if (index < minColumnIndex) minColumnIndex = index;
        if (index > maxColumnIndex) maxColumnIndex = index;
      }
      assert(columnIndexTable.find(index) == columnIndexTable.end());
      columnIndexTable.insert(index);
    } else if (id.isPegbar()) {
      assert(imp->getParent() != TStageObjectId());
      assert(imp->getParent().isPegbar() || imp->getParent().isTable());
    } else if (id.isTable())
      assert(imp->getParent() == TStageObjectId());
    else if (id.isCamera())  // la camera puo' essere attaccata dovunque
      cameraCount++;
    else
      assert(0);
  }
  if (minColumnIndex > maxColumnIndex) {
    assert(columnIndexTable.size() == 0);
  } else {
    assert(minColumnIndex == 0);
    int count = maxColumnIndex - minColumnIndex + 1;
    int m     = columnIndexTable.size();
    assert(m == count);
    int k = minColumnIndex;
    for (std::set<int>::iterator it = columnIndexTable.begin();
         it != columnIndexTable.end(); ++it) {
      assert(*it == k);
      k++;
    }
  }
  assert(m_imp->m_cameraCount == cameraCount);
}

//-----------------------------------------------------------------------------

TStageObject *TStageObjectTree::getStageObject(const TStageObjectId &id,
                                               bool create) {
  std::map<TStageObjectId, TStageObject *> &pegbars     = m_imp->m_pegbarTable;
  std::map<TStageObjectId, TStageObject *>::iterator it = pegbars.find(id);
  if (it != pegbars.end()) {
    TStageObject *pegbar = it->second;
    return pegbar;
  } else if (create) {
    TStageObject *pegbar = new TStageObject(this, id);
    if (id.isColumn()) {
      int index = id.getIndex();
      // mi assicuro che esistano tutte le colonne con indici inferiori
      if (index > 0) getStageObject(TStageObjectId::ColumnId(index - 1));
      // assegno il parent (per una colonna dev'essere la table)
      TStageObjectId parentId = TStageObjectId::TableId;
      pegbar->setParent(parentId);
      // mi assicuro che il padre esista
      getStageObject(parentId);
    } else if (id.isPegbar()) {
      pegbar->setParent(TStageObjectId::TableId);
    } else if (id.isCamera())
      m_imp->m_cameraCount++;
    pegbars[id] = pegbar;
    pegbar->addRef();

#ifndef NDEBUG
    checkIntegrity();
#endif

    return pegbar;
  } else
    return 0L;
}

//-----------------------------------------------------------------------------

void TStageObjectTree::insertColumn(int index) {
  assert(0 <= index);
#ifndef NDEBUG
  checkIntegrity();
#endif

  TStageObjectId id       = TStageObjectId::ColumnId(index);
  TStageObject *pegbar    = new TStageObject(this, id);
  TStageObjectId parentId = TStageObjectId::TableId;
  pegbar->setParent(parentId);
  // mi assicuro che il padre esista
  getStageObject(parentId);

  // mi assicuro che esistano tutte le colonne di indice inferiore
  for (int k = 0; k < index; k++) getStageObject(TStageObjectId::ColumnId(k));

  std::map<TStageObjectId, TStageObject *> &pegbars = m_imp->m_pegbarTable;

  std::vector<std::pair<TStageObjectId, TStageObject *>> tmp(pegbars.begin(),
                                                             pegbars.end());
  std::vector<std::pair<TStageObjectId, TStageObject *>>::iterator it;

  for (it = tmp.begin(); it != tmp.end(); ++it) {
    TStageObjectId j = it->first;
    if (j.isColumn() && j.getIndex() >= index) {
      it->first = TStageObjectId::ColumnId(j.getIndex() + 1);
      it->second->setId(it->first);
    }
  }

  pegbars.clear();
  pegbars.insert(tmp.begin(), tmp.end());

  pegbars[id] = pegbar;
  pegbar->addRef();

#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------

void TStageObjectTree::removeColumn(int index) {
  assert(0 <= index);

  TStageObjectId id = TStageObjectId::ColumnId(index);
  std::map<TStageObjectId, TStageObject *> &pegbars = m_imp->m_pegbarTable;
  std::map<TStageObjectId, TStageObject *>::iterator pit;
  pit                           = pegbars.find(id);
  TStageObject *imp             = 0;
  if (pit != pegbars.end()) imp = pit->second;

  if (imp) {
    // i figli della colonna rimossa si attaccano al "nonno"
    TStageObjectId parentId = imp->getParent();
    imp->detachFromParent();
    /* while(! m_children.empty())
{
TStageObject*son = *imp->m_children.begin();
son->setParent(parentId);
}*/
    imp->attachChildrenToParent(parentId);
    imp->release();
  }
  imp = 0;

  pegbars.erase(id);
  std::vector<std::pair<TStageObjectId, TStageObject *>> tmp(pegbars.begin(),
                                                             pegbars.end());
  std::vector<std::pair<TStageObjectId, TStageObject *>>::iterator it;
  for (it = tmp.begin(); it != tmp.end(); ++it) {
    TStageObjectId j = it->first;
    if (j.isColumn() && j.getIndex() > index) {
      it->first = TStageObjectId::ColumnId(j.getIndex() - 1);
      it->second->setId(it->first);
    }
  }

  pegbars.clear();
  pegbars.insert(tmp.begin(), tmp.end());

#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------

void TStageObjectTree::removeStageObject(const TStageObjectId &id) {
  std::map<TStageObjectId, TStageObject *> &pegbars = m_imp->m_pegbarTable;
  assert(pegbars.count(id) > 0);
  TStageObject *obj = pegbars[id];
  obj->attachChildrenToParent(obj->getParent());
  obj->detachFromParent();
  obj->release();
  pegbars.erase(id);
  if (id.isCamera()) m_imp->m_cameraCount--;
#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------

void TStageObjectTree::insertStageObject(TStageObject *pegbar) {
  TStageObject *imp = dynamic_cast<TStageObject *>(pegbar);
  assert(imp);
  TStageObjectId id = imp->getId();
  std::map<TStageObjectId, TStageObject *> &pegbars = m_imp->m_pegbarTable;
  assert(pegbars.count(id) == 0);
  pegbars[id] = imp;
  imp->addRef();
  imp->setParent(imp->getParent());
  if (id.isCamera()) m_imp->m_cameraCount++;
#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------
namespace {  // Utility Function
//-----------------------------------------------------------------------------
bool checkId(TStageObjectTree *tree, const TStageObjectId &id) {
  TStageObject *pegbar = tree->getStageObject(id, false);
  return !pegbar || pegbar->getId() == id;
}
//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

void TStageObjectTree::swapColumns(int i, int j) {
  if (i == j) return;
  if (i > j) std::swap(i, j);
  std::map<TStageObjectId, TStageObject *> &pegbars = m_imp->m_pegbarTable;
  std::map<TStageObjectId, TStageObject *>::iterator iti, itj;
  std::map<TStageObjectId, TStageObject *>::const_iterator end = pegbars.end();
  TStageObjectId idi = TStageObjectId::ColumnId(i);
  TStageObjectId idj = TStageObjectId::ColumnId(j);
  assert(checkId(this, idi));
  assert(checkId(this, idj));

  iti = pegbars.find(idi);
  itj = pegbars.find(idj);

  if (iti == end && itj == end) {
  } else if (iti != end && itj != end) {
    assert(iti->second);
    assert(itj->second);
    std::swap(iti->second, itj->second);
    iti->second->setId(iti->first);
    itj->second->setId(itj->first);
  } else if (iti == end) {
    // iti e' il minore. Se manca iti deve mancare anche itj
    assert(0);
  } else {
    // manca itj, ma iti c'e'. Devo crearne uno per evitare i buchi
    assert(iti != end);
    assert(iti->second);
    assert(itj == end);
    pegbars[idj] = iti->second;
    iti->second->setId(idj);
    pegbars.erase(iti);

    // creo
    getStageObject(idi, true);
  }
  assert(checkId(this, idi));
  assert(checkId(this, idj));

#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------

void TStageObjectTree::loadData(TIStream &is) {
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "splines") {
      while (!is.eos()) {
        TPersist *p = 0;
        is >> p;
        TStageObjectSpline *spline = dynamic_cast<TStageObjectSpline *>(p);
        insertSpline(spline);
      }
      is.matchEndTag();
    } else if (tagName == "pegbar") {
      std::string idStr = is.getTagAttribute("id");
      if (idStr == "")  // vecchio formato
      {
        is >> idStr;
      }
      TStageObjectId id = toStageObjectId(idStr);
      if (id.isCamera() && is.getTagAttribute("active") == "yes")
        m_imp->m_currentCameraId = id;
      else if (id.isCamera() && is.getTagAttribute("activepreview") == "yes")
        m_imp->m_currentPreviewCameraId = id;
      else if (id.isCamera() && is.getTagAttribute("activeboth") == "yes")
        m_imp->m_currentPreviewCameraId = m_imp->m_currentCameraId = id;

      TStageObject *pegbar =
          dynamic_cast<TStageObject *>(getStageObject(id, true));
      if (!pegbar)
        throw TException("TStageObjectTree::loadData. can't create the pegbar");
      pegbar->loadData(is);
      if (pegbar->isGrouped() && m_imp->m_groupIdCount < pegbar->getGroupId())
        m_imp->m_groupIdCount = pegbar->getGroupId();
      is.matchEndTag();

      std::string name = pegbar->getName();
    } else if (tagName == "grid_dimension") {
      is >> m_imp->m_dagGridDimension;
      is.matchEndTag();
    } else
      throw TException("pegbartree: unexpected tag : " + tagName);
  }

#ifndef NDEBUG
  checkIntegrity();
#endif
}

//-----------------------------------------------------------------------------

void TStageObjectTree::saveData(TOStream &os, int occupiedColumnCount) {
  std::map<TStageObjectId, TStageObject *>::iterator it;
  std::map<TStageObjectId, TStageObject *> &pegbars = m_imp->m_pegbarTable;
  if (!m_imp->m_splines.empty()) {
    os.openChild("splines");
    std::map<int, TStageObjectSpline *>::iterator it;
    for (it = m_imp->m_splines.begin(); it != m_imp->m_splines.end(); it++) {
      os << it->second;
    }
    os.closeChild();
  }
  for (it = pegbars.begin(); it != pegbars.end(); ++it) {
    TStageObjectId objectId = it->first;
    TStageObject *pegbar    = it->second;

    if (objectId.isColumn() && objectId.getIndex() >= occupiedColumnCount)
      continue;

    std::map<std::string, std::string> attr;
    attr["id"] = objectId.toString();
    if (objectId == m_imp->m_currentCameraId &&
        objectId == m_imp->m_currentPreviewCameraId)
      attr["activeboth"] = "yes";
    else if (objectId == m_imp->m_currentCameraId)
      attr["active"] = "yes";
    else if (objectId == m_imp->m_currentPreviewCameraId)
      attr["activepreview"] = "yes";

    os.openChild("pegbar", attr);

    pegbar->saveData(os);
    os.closeChild();
  }
  os.child("grid_dimension") << (int)m_imp->m_dagGridDimension;
}

//-----------------------------------------------------------------------------

int TStageObjectTree::getStageObjectCount() const {
  return m_imp->m_pegbarTable.size();
}

//-----------------------------------------------------------------------------

int TStageObjectTree::getCameraCount() const { return m_imp->m_cameraCount; }

//-----------------------------------------------------------------------------

int TStageObjectTree::getNewGroupId() { return ++m_imp->m_groupIdCount; }

//-----------------------------------------------------------------------------

TStageObject *TStageObjectTree::getStageObject(int index) const {
  assert(0 <= index && index < getStageObjectCount());
  std::map<TStageObjectId, TStageObject *>::const_iterator it;
  int i = 0;
  for (it = m_imp->m_pegbarTable.begin();
       it != m_imp->m_pegbarTable.end() && i < index; ++it, ++i) {
  }
  assert(it != m_imp->m_pegbarTable.end());
  return it->second;
}

//-----------------------------------------------------------------------------

TStageObjectId TStageObjectTree::getCurrentCameraId() const {
  return m_imp->m_currentCameraId;
}

//-----------------------------------------------------------------------------

TStageObjectId TStageObjectTree::getCurrentPreviewCameraId() const {
  return m_imp->m_currentPreviewCameraId;
}

//-----------------------------------------------------------------------------

void TStageObjectTree::setCurrentCameraId(const TStageObjectId &id) {
  assert(id.isCamera());
  m_imp->m_currentCameraId = id;
}

//-----------------------------------------------------------------------------

void TStageObjectTree::setCurrentPreviewCameraId(const TStageObjectId &id) {
  assert(id.isCamera());
  m_imp->m_currentPreviewCameraId = id;
}

//-----------------------------------------------------------------------------

void TStageObjectTree::invalidateAll() {
  std::map<TStageObjectId, TStageObject *>::iterator it;
  for (it = m_imp->m_pegbarTable.begin(); it != m_imp->m_pegbarTable.end();
       ++it) {
    it->second->invalidate();
  }
}

//-----------------------------------------------------------------------------

void TStageObjectTree::setHandleManager(HandleManager *hm) {
  m_imp->m_handleManager = hm;
}

//-----------------------------------------------------------------------------

TPointD TStageObjectTree::getHandlePos(const TStageObjectId &id,
                                       std::string handle, int row) const {
  if (m_imp->m_handleManager)
    return m_imp->m_handleManager->getHandlePos(id, handle, row);
  else
    return TPointD();
}

//-----------------------------------------------------------------------------

int TStageObjectTree::getSplineCount() const { return m_imp->m_splines.size(); }

//-----------------------------------------------------------------------------

TStageObjectSpline *TStageObjectTree::getSpline(int index) const {
  assert(0 <= index && index < getSplineCount());
  std::map<int, TStageObjectSpline *>::iterator it = m_imp->m_splines.begin();
  for (int i = 0; i < index; i++) it++;
  return it->second;
}

//-----------------------------------------------------------------------------

TStageObjectSpline *TStageObjectTree::getSplineById(int splineId) const {
  std::map<int, TStageObjectSpline *>::iterator it =
      m_imp->m_splines.find(splineId);
  if (it != m_imp->m_splines.end()) return it->second;
  return 0;
}

//-----------------------------------------------------------------------------

TStageObjectSpline *TStageObjectTree::createSpline() {
  TStageObjectSpline *spline = new TStageObjectSpline();
  spline->setId(m_imp->m_splineCount++);
  m_imp->m_splines[spline->getId()] = spline;
  spline->addRef();
  return spline;
}

//-----------------------------------------------------------------------------

void TStageObjectTree::assignUniqueSplineId(TStageObjectSpline *spline) {
  if (!spline) return;
  spline->setId(m_imp->m_splineCount++);
}

//-----------------------------------------------------------------------------

void TStageObjectTree::removeSpline(TStageObjectSpline *spline) {
  assert(spline->getId() >= 0);
  std::map<int, TStageObjectSpline *> &splines = m_imp->m_splines;
  std::map<int, TStageObjectSpline *>::iterator it;
  it = splines.find(spline->getId());
  if (it == splines.end()) return;
  splines.erase(it);
  assert(!containsSpline(spline));
  spline->release();
}

//-----------------------------------------------------------------------------

bool TStageObjectTree::containsSpline(TStageObjectSpline *s) const {
  assert(s->getId() >= 0);
  std::map<int, TStageObjectSpline *> &splines     = m_imp->m_splines;
  std::map<int, TStageObjectSpline *>::iterator it = splines.find(s->getId());
  return it != splines.end() && s == it->second;
}

//-----------------------------------------------------------------------------

void TStageObjectTree::insertSpline(TStageObjectSpline *s) {
  assert(s->getId() >= 0);
  std::map<int, TStageObjectSpline *> &splines = m_imp->m_splines;
  if (containsSpline(s)) return;
  splines[s->getId()]  = s;
  m_imp->m_splineCount = std::max(m_imp->m_splineCount, s->getId() + 1);
  s->addRef();
}

//-----------------------------------------------------------------------------

void TStageObjectTree::createGrammar(TXsheet *xsh) {
  assert(!m_imp->m_grammar);
  m_imp->m_grammar = createXsheetGrammar(xsh);

  std::map<TStageObjectId, TStageObject *>::iterator it;
  for (it = m_imp->m_pegbarTable.begin(); it != m_imp->m_pegbarTable.end();
       ++it) {
    TStageObject *obj = it->second;

    int c, cCount = TStageObject::T_ChannelCount;
    for (c = 0; c != cCount; ++c)
      obj->getParam((TStageObject::Channel)c)->setGrammar(m_imp->m_grammar);

    if (const PlasticSkeletonDeformationP &sd =
            obj->getPlasticSkeletonDeformation())
      sd->setGrammar(m_imp->m_grammar);
  }
}

//-----------------------------------------------------------------------------

void TStageObjectTree::setGrammar(const TDoubleParamP &param) {
  if (m_imp->m_grammar) param->setGrammar(m_imp->m_grammar);
}

//-----------------------------------------------------------------------------

TSyntax::Grammar *TStageObjectTree::getGrammar() const {
  return m_imp->m_grammar;
}

//-----------------------------------------------------------------------------
/*
void TStageObjectTree::setToonzBuilder(const TDoubleParamP &param)
{
//  param->setBuilder(new ToonzCalcNodeBuilder(this));
}
*/

//-----------------------------------------------------------------------------

TCamera *TStageObjectTree::getCamera(const TStageObjectId &cameraId) {
  assert(cameraId.isCamera());
  TStageObject *cameraPegbar = getStageObject(cameraId);
  assert(cameraPegbar);
  return cameraPegbar->getCamera();
}

//-----------------------------------------------------------------------------

void TStageObjectTree::setDagGridDimension(int dim) {
  m_imp->m_dagGridDimension = dim;
}

//-----------------------------------------------------------------------------

int TStageObjectTree::getDagGridDimension() const {
  return m_imp->m_dagGridDimension;
}

//-----------------------------------------------------------------------------

PERSIST_IDENTIFIER(TStageObjectTree, "PegbarTree");
