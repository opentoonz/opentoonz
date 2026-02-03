

#include "tfxattributes.h"

#include "trenderer.h"
#include "tcacheresource.h"
#include "tcacheresourcepool.h"

#include "tpassivecachemanager.h"

//#define USE_SQLITE_HDPOOL

/* PRACTICAL EXPLANATION:

The TPassiveCacheManager's purpose is that of storing render results from a
specified set of fx nodes.

When an fx is passed to the manager to be cached, a structure of type FxData is
generated for the fx.
It is stored inside the manager and ASSOCIATED to the fx through an INDEX key -
this one being stored
inside the fx data.

In particular, the FxData instances contain a PERSISTANT identifier
(passiveCacheId) that is saved inside
the scene data, a flag specifying the cache status of the fx node (like, if it's
enabled or not), and
a description of the schematic dag below the associated fx.

Now, when a resource request is made to the TPassiveCacheManager, it checks the
FxData about the generating
fx - if it contains the flag specifying that it must be cached, a resource is
allocated (if not already
present), a reference to it is stored, and finally the resource is returned.

References to interesting resources are stored in a TABLE container, indexed by
"rendering context" and
fx. A rendering context is hereby intended as a SEQUENCE of render processes
where the next render of the
sequence is assumed to REPLACE the previous one.
We therefore have one context for the swatch viewer, one for File>Preview, one
for the Preview Fx of each
fx node, and one context for EACH FRAME of the sceneviewer preview (one frame is
not supposed to replace
another in that case).

The table is in practice a map of maps (a kind of 'comb' structure) - where the
primary map is associated
by render context and the secondary ones by fx. This mean that we are able to
address and iterate contexts
easier than fxs. Values of the table are set of resources.

RESOURCES MAINTENANCE POLICY:

As resources use a concrete amount of memory for storage, they should be kept in
the resources table only
for the time they are supposedly useful to the user. There are 2 ways of dealing
with this issue.

1) First, we can track scene changes and eliminate those resources that will no
longer be accessed due to the
applied change. This happens, for example, when a level is being drawn, fxs are
inserted, moved or removed.

Level changes are delivered immediately to the ::invalidateLevel(..) method,
which removes all resources
whose name contains the level's name. Unfortunately, this cannot be done at
frame precision (ie you cannot
invalidate a single frame of the level, but ALL the level).

Xsheet (schematic) changes are tracked through the schematic node description
stored inside the FxData structure.
Once one such change happens, all resources update their description - if that
changes, the associated resources
are released.

The same schematic description is used to track and release resources affected
by changes on a downstream fx's
parameters.

There are also scene changes that are inherently hard to track or intercept. For
example, pegbar affines changes,
fx nodes expressions referencing data not directly connected to the fx, and
possibly others. These are currently
NOT handled by direct tracking.

2) As there may be resources which escape the resource control by point (1), we
need some extra control
policies. Here they are:

We assume that the pool of resources accessed by render processes of a same
render context should be CONSTANT
IF NO SCENE CHANGE HAPPENS. In other words, we are guessing that only those
resources used in the last
rendering will be used in the next. Resources accessed in a render but NOT in
the next (of the same context)
are released.

However, it could be that the sequence of render processes from a context is
halted from a certain moment on.
In that case, it is a waste to keep the resources accessed by its last render if
no new render will ever take
place. We then assume further that a rendering context can be ENABLED or
DISABLED - when a render context is
enabled, it will most likely have a next render - and therefore can keep its
resources in memory.
Once a context is DISABLED, it moves the resources to a TEMPORARY context.

Resources in the temporary context are those 'on their way for release'. They
will be KEPT only if the
next rendering - INDEPENDENTLY FROM THE RENDER CONTEXT - requires them (in this
case, they will be adopted
by the new context). This is necessary since context disables typically happen
when a preview window closes.
It is not unfrequent that users close the preview window, modify the scene, and
then restore the preview.

CONSIDERATIONS:

* The File>Render generates NO CONTEXT - and therefore does NOT PASSIVELY CACHE
RESOURCES, since it cannot be
  stated whether it is in the 'enabled' or 'disabled' state. It could be
considered coherent with what tcomposer
  does, by the way...

* Our resources maintenance policy ensures that the memory usage should be
stable over time - that is, no
  useless resource is kept in memory.
  Of course, it is possibly more restrictive than what the user may desire. For
example, closing 2 preview
  windows marks their resources for deletion, but only one of the two restored
previews will keep its
  resources intact...

*/

//*****************************************************************************************
//    Preliminaries
//*****************************************************************************************

//  Local stuff - inlines
namespace {
inline QRect toQRect(const TRect &r) {
  return QRect(r.x0, r.y0, r.getLx(), r.getLy());
}
inline TRect toTRect(const QRect &r) {
  return TRect(r.left(), r.top(), r.right(), r.bottom());
}
}

//---------------------------------------------------------------------------

TFx *TPassiveCacheManager::getNotAllowingAncestor(TFx *fx) {
  // Trace all output ports
  int outputPortsCount = fx->getOutputConnectionCount();
  /*if(!outputPortsCount)   //We have no access to TApp here!!
{
//It could be a terminal fx. In that case, pass to the xsheet fx
FxDag* dag = TApp::instance()->getCurrentXsheet()->getXsheet()->getFxDag();
if(dag->getTerminalFxs()->containsFx(fx))
return getNotAllowingAncestor(dag->getXsheetFx());
}*/

  // Now, for common ports
  for (int i = 0; i < outputPortsCount; ++i) {
    // Find the output Fx and the port connected to our fx
    TFxPort *port    = fx->getOutputConnection(i);
    TRasterFx *outFx = static_cast<TRasterFx *>(port->getOwnerFx());

    int portIdx, portsCount = outFx->getInputPortCount();
    for (portIdx = 0; portIdx < portsCount; ++portIdx)
      if (outFx->getInputPort(portIdx) == port) break;
    assert(portIdx < portsCount);

    if (!outFx->allowUserCacheOnPort(portIdx)) return outFx;

    TFx *naAncestor = getNotAllowingAncestor(outFx);
    if (naAncestor) return naAncestor;
  }

  return 0;
}

//*****************************************************************************************
//    Resources Container Definition
//*****************************************************************************************

template <typename RowKey, typename ColKey, typename Val>
class Table {
public:
  typedef typename std::map<ColKey, Val> Row;

private:
  std::map<RowKey, Row> m_table;

  friend class Iterator;
  friend class ColIterator;

public:
  typedef typename std::map<RowKey, Row>::iterator RowsIterator;

  class Iterator {
  protected:
    Table *m_table;
    RowsIterator m_rowIt;
    typename Row::iterator m_it;

    friend class Table;
    Iterator(Table *table) : m_table(table) {}

    virtual void makeConsistent() {
      if (m_it == m_rowIt->second.end()) {
        if (++m_rowIt == m_table->m_table.end()) return;
        m_it = m_rowIt->second.begin();
      }
    }

  public:
    const RowKey &row() { return m_rowIt->first; }
    const ColKey &col() { return m_it->first; }

    virtual void operator++() {
      ++m_it;
      makeConsistent();
    }

    virtual operator bool() { return m_rowIt != m_table->m_table.end(); }

    Val &operator*() { return m_it->second; }
    Val *operator->() { return &m_it->second; }

    bool operator==(const Iterator &it) { return m_it == it.m_it; }

    bool operator!=(const Iterator &it) { return !operator==(it); }
  };

  class ColIterator final : public Iterator {
    ColKey m_colKey;

    friend class Table;
    ColIterator(Table *table, const ColKey &c) : Iterator(table), m_colKey(c) {}

    void makeConsistent() override {
      Iterator::m_rowIt = Iterator::m_rowIt;
      while (Iterator::m_rowIt != Iterator::m_table->m_table.end()) {
        Iterator::m_it = Iterator::m_rowIt->second.find(m_colKey);
        if (Iterator::m_it != Iterator::m_rowIt->second.end()) break;
        ++Iterator::m_rowIt;
      }
    }

  public:
    void operator++() override {
      ++Iterator::m_rowIt;
      makeConsistent();
    }
  };

  class RowIterator final : public Iterator {
    friend class Table;
    RowIterator(Table *table) : Iterator(table) {}

    void makeConsistent() override {}

  public:
    RowIterator(const RowsIterator rowIt) : Iterator(0) {
      Iterator::m_rowIt = rowIt;
      Iterator::m_it    = rowIt->second.begin();
    }

    void operator++() override { ++Iterator::m_it; }
    operator bool() override {
      return Iterator::m_it != Iterator::m_rowIt->second.end();
    }
  };

public:
  Table() {}
  ~Table() {}

  std::map<RowKey, Row> &rows() { return m_table; }

  Iterator begin() {
    Iterator result(this);
    result.m_rowIt = m_table.begin();
    if (result.m_rowIt != m_table.end())
      result.m_it = result.m_rowIt->second.begin();
    return result;
  }

  RowIterator rowBegin(const RowKey &row) {
    RowIterator result(this);
    result.m_rowIt = m_table.find(row);
    if (result.m_rowIt != m_table.end())
      result.m_it = result.m_rowIt->second.begin();
    return result;
  }

  ColIterator colBegin(const ColKey &col) {
    ColIterator result(this, col);
    result.m_rowIt = m_table.begin();
    result.makeConsistent();
    return result;
  }

  Val &value(const RowKey &r, const ColKey &c) { return m_table[r][c]; }

  Iterator insert(const RowKey &r, const ColKey &c, const Val &val) {
    Iterator result(this);
    result.m_rowIt = m_table.insert(std::make_pair(r, Row())).first;
    result.m_it = result.m_rowIt->second.insert(std::make_pair(c, val)).first;
    return result;
  }

  Iterator erase(const Iterator &it) {
    Iterator result(it);
    Row &row = it.m_rowIt->second;
    ++result.m_it;
    row.erase(it.m_it);
    if (result.m_it == row.end() && row.empty()) {
      result.makeConsistent();
      m_table.erase(it.m_rowIt);
      return result;
    }

    result.makeConsistent();
    return result;
  }

  void erase(const ColKey &c) {
    ColIterator it(colBegin(c));
    while (it) {
      RowsIterator rowIt = it.m_rowIt;
      rowIt->second.erase(it.m_it);
      ++it;
      if (rowIt->second.empty()) m_table.erase(rowIt);
    }
  }

  void erase(const RowKey &r) { m_table.erase(r); }

  void clear() { m_table.clear(); }
};

//--------------------------------------------------------------------------

struct LockedResourceP {
  TCacheResourceP m_resource;

  LockedResourceP(const TCacheResourceP &resource) : m_resource(resource) {
    m_resource->addLock();
  }

  LockedResourceP(const LockedResourceP &resource)
      : m_resource(resource.m_resource) {
    m_resource->addLock();
  }

  ~LockedResourceP() { m_resource->releaseLock(); }

  LockedResourceP &operator=(const LockedResourceP &src) {
    src.m_resource->addLock();
    if (m_resource) m_resource->releaseLock();
    m_resource = src.m_resource;
    return *this;
  }

  operator bool() const { return m_resource; }

  bool operator<(const LockedResourceP &resource) const {
    return m_resource < resource.m_resource;
  }

  TCacheResource *operator->() const { return m_resource.getPointer(); }
  TCacheResource &operator*() const { return *m_resource.getPointer(); }
};

typedef Table<std::string, int, std::set<LockedResourceP>> ResourcesTable;

//--------------------------------------------------------------------------

class TPassiveCacheManager::ResourcesContainer {
  ResourcesTable m_resources;

public:
  ResourcesContainer() {}
  ~ResourcesContainer() {}

  ResourcesTable &getTable() { return m_resources; }
};

//*****************************************************************************************
//    FxData implementation
//*****************************************************************************************

TPassiveCacheManager::FxData::FxData()
    : m_storageFlag(0), m_passiveCacheId(0) {}

//-------------------------------------------------------------------------

TPassiveCacheManager::FxData::~FxData() {}

//*****************************************************************************************
//    Manager generator
//*****************************************************************************************

//=======================================
//    TPassiveCacheManagerGenerator
//---------------------------------------

class TPassiveCacheManagerGenerator final
    : public TRenderResourceManagerGenerator {
  TRenderResourceManager *operator()(void) override {
    // return new TPassiveCacheManager;
    return TPassiveCacheManager::instance();
  }
};

MANAGER_FILESCOPE_DECLARATION_DEP(TPassiveCacheManager,
                                  TPassiveCacheManagerGenerator,
                                  TFxCacheManager::deps())

//*****************************************************************************************
//    Implementation
//*****************************************************************************************

TPassiveCacheManager::TPassiveCacheManager()
#ifdef USE_SQLITE_HDPOOL
    : m_currStorageFlag(ON_DISK)
#else
    : m_currStorageFlag(IN_MEMORY)
#endif
    , m_enabled(true)
    , m_descriptorCallback(0)
    , m_mutex(QMutex::Recursive)
    , m_resources(new ResourcesContainer) {
  reset();
}

//-------------------------------------------------------------------------

TPassiveCacheManager::~TPassiveCacheManager() { delete m_resources; }

//-------------------------------------------------------------------------

TPassiveCacheManager *TPassiveCacheManager::instance() {
  static TPassiveCacheManager theInstance;
  return &theInstance;
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::setContextName(unsigned long renderId,
                                          const std::string &name) {
  QMutexLocker locker(&m_mutex);

  // Retrieve the context data if already present
  std::map<std::string, UCHAR>::iterator it = m_contextNames.find(name);
  if (it == m_contextNames.end())
    it = m_contextNames.insert(std::make_pair(name, 0)).first;

  it->second = !it->second;
  m_contextNamesByRenderId.insert(
      std::make_pair(renderId, name + "%" + std::to_string(it->second)));
}

//-------------------------------------------------------------------------

std::string TPassiveCacheManager::getContextName() {
  QMutexLocker locker(&m_mutex);

  // First, search the context name
  std::map<unsigned long, std::string>::iterator it =
      m_contextNamesByRenderId.find(TRenderer::renderId());

  if (it == m_contextNamesByRenderId.end()) return "";

  return it->second;
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::setEnabled(bool enabled) { m_enabled = enabled; }

//-------------------------------------------------------------------------

bool TPassiveCacheManager::isEnabled() const { return m_enabled; }

//-------------------------------------------------------------------------

void TPassiveCacheManager::setStorageMode(StorageFlag mode) {
  m_currStorageFlag = mode;
}

//-------------------------------------------------------------------------

TPassiveCacheManager::StorageFlag TPassiveCacheManager::getStorageMode() const {
  return m_currStorageFlag;
}

//-----------------------------------------------------------------------------------

void TPassiveCacheManager::setTreeDescriptor(TreeDescriptor callback) {
  m_descriptorCallback = callback;
}

//-----------------------------------------------------------------------------------

int TPassiveCacheManager::getNewPassiveCacheId() {
  return ++m_currentPassiveCacheId;
}

//-----------------------------------------------------------------------------------

int TPassiveCacheManager::updatePassiveCacheId(int id) {
  if (m_updatingPassiveCacheIds)
    m_currentPassiveCacheId = std::max(m_currentPassiveCacheId, id);
  else
    id = getNewPassiveCacheId();

  return id;
}

//-----------------------------------------------------------------------------------

void TPassiveCacheManager::onSceneLoaded() {
  m_updatingPassiveCacheIds = false;

// Initialize the fxs tree description. This was not possible before, as the
// scene was yet incomplete (in loading state).
#ifndef USE_SQLITE_HDPOOL
  unsigned int count = m_fxDataVector.size();
  for (unsigned int i = 0; i < count; ++i) {
    FxData &data = m_fxDataVector[i];
    (*m_descriptorCallback)(data.m_treeDescription, data.m_fx);
  }
#endif
}

//-----------------------------------------------------------------------------------

void TPassiveCacheManager::touchFxData(int &idx) {
  if (idx >= 0) return;

  QMutexLocker locker(&m_mutex);

  m_fxDataVector.push_back(FxData());
  idx = m_fxDataVector.size() - 1;
}

//-----------------------------------------------------------------------------------

int TPassiveCacheManager::declareCached(TFx *fx, int passiveCacheId) {
  int &idx = fx->getAttributes()->passiveCacheDataIdx();
  touchFxData(idx);

  FxData &data          = m_fxDataVector[idx];
  data.m_fx             = fx;
  data.m_storageFlag    = m_currStorageFlag;
  data.m_passiveCacheId = updatePassiveCacheId(passiveCacheId);

  return idx;
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::reset() {
  m_updatingPassiveCacheIds = true;
  m_currentPassiveCacheId   = 0;
  m_fxDataVector.clear();
  m_resources->getTable().clear();
}

//-------------------------------------------------------------------------

bool TPassiveCacheManager::cacheEnabled(TFx *fx) {
  int idx = fx->getAttributes()->passiveCacheDataIdx();
  if (idx < 0) return false;

  assert(idx < (int)m_fxDataVector.size());

  QMutexLocker locker(&m_mutex);

  return m_fxDataVector[idx].m_storageFlag > 0;
}

//-------------------------------------------------------------------------

int TPassiveCacheManager::getPassiveCacheId(TFx *fx) {
  int idx = fx->getAttributes()->passiveCacheDataIdx();
  if (idx < 0) return 0;

  // This needs not be mutex locked

  assert(idx < (int)m_fxDataVector.size());
  return m_fxDataVector[idx].m_passiveCacheId;
}

//-------------------------------------------------------------------------

TPassiveCacheManager::StorageFlag TPassiveCacheManager::getStorageMode(
    TFx *fx) {
  int idx = fx->getAttributes()->passiveCacheDataIdx();
  if (idx < 0) return NONE;

  QMutexLocker locker(&m_mutex);

  return (StorageFlag)m_fxDataVector[idx].m_storageFlag;
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::enableCache(TFx *fx) {
  int &idx = fx->getAttributes()->passiveCacheDataIdx();
  touchFxData(idx);

  FxData &data = m_fxDataVector[idx];

  QMutexLocker locker(&m_mutex);

#ifdef DIAGNOSTICS
  DIAGNOSTICS_STR("#activity.txt | " + QTime::currentTime().toString() + " " +
                  QString("Enable Cache"));
#endif

  StorageFlag flag = getStorageMode();
  if (flag) {
    UCHAR &storedFlag = data.m_storageFlag;
    UCHAR oldFlag     = storedFlag;

    storedFlag |= flag;

    if (data.m_passiveCacheId == 0)
      data.m_passiveCacheId = getNewPassiveCacheId();

    if ((storedFlag & ON_DISK) && !(oldFlag & ON_DISK)) {
      ResourcesTable::ColIterator it =
          m_resources->getTable().colBegin(data.m_passiveCacheId);
      for (; it; ++it) {
        std::set<LockedResourceP> &resources = *it;

        std::set<LockedResourceP>::iterator jt;
        for (jt = resources.begin(); jt != resources.end(); ++jt)
          (*jt)->enableBackup();
      }
    }

#ifndef USE_SQLITE_HDPOOL
    if ((storedFlag & IN_MEMORY) && !(oldFlag & IN_MEMORY)) {
      data.m_fx = fx;
      (*m_descriptorCallback)(data.m_treeDescription, data.m_fx);
    }
#endif
  }
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::disableCache(TFx *fx) {
  int idx = fx->getAttributes()->passiveCacheDataIdx();
  if (idx < 0) return;

  FxData &data = m_fxDataVector[idx];

  QMutexLocker locker(&m_mutex);

#ifdef DIAGNOSTICS
  DIAGNOSTICS_STR("#activity.txt | " + QTime::currentTime().toString() + " " +
                  QString("Disable Cache"));
#endif

  StorageFlag flag = getStorageMode();
  if (flag) {
    UCHAR &storedFlag = data.m_storageFlag;
    UCHAR oldFlag     = storedFlag;

    storedFlag &= ~flag;

    if ((oldFlag & IN_MEMORY) && !(storedFlag & IN_MEMORY)) {
      m_resources->getTable().erase(data.m_passiveCacheId);

      data.m_fx              = TFxP();
      data.m_treeDescription = "";
    }

#ifdef USE_SQLITE_HDPOOL
    if ((oldFlag & ON_DISK) && !(storedFlag & ON_DISK))
      TCacheResourcePool::instance()->releaseReferences(
          "P" + QString::number(data.m_passiveCacheId));
#endif
  }
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::toggleCache(TFx *fx) {
  int &idx = fx->getAttributes()->passiveCacheDataIdx();
  touchFxData(idx);

  FxData &data = m_fxDataVector[idx];

  QMutexLocker locker(&m_mutex);

  StorageFlag flag = getStorageMode();
  if (flag) {
    UCHAR &storedFlag = data.m_storageFlag;
    UCHAR oldFlag     = storedFlag;

    storedFlag ^= flag;

#ifdef DIAGNOSTICS
    DIAGNOSTICS_STR("#activity.txt | " + QTime::currentTime().toString() + " " +
                    QString("Toggle Cache (now ") +
                    ((storedFlag & IN_MEMORY) ? "enabled)" : "disabled)"));
#endif

    if (data.m_passiveCacheId == 0)
      data.m_passiveCacheId = getNewPassiveCacheId();

    if ((storedFlag & ON_DISK) && !(oldFlag & ON_DISK)) {
      ResourcesTable::ColIterator it =
          m_resources->getTable().colBegin(data.m_passiveCacheId);
      for (; it; ++it) {
        std::set<LockedResourceP> &resources = *it;

        std::set<LockedResourceP>::iterator jt;
        for (jt = resources.begin(); jt != resources.end(); ++jt)
          (*jt)->enableBackup();
      }
    }

    // Implementa la versione contraria - eliminazione dell'fx nell'hdPool...
    // Metti anche questo in versione ritardata con flush... Il flush e' da
    // unificare...
    // e magari da spostare direttamente nell'hdPool

    if ((oldFlag & IN_MEMORY) && !(storedFlag & IN_MEMORY)) {
      m_resources->getTable().erase(data.m_passiveCacheId);

      data.m_fx              = TFxP();
      data.m_treeDescription = "";
    }

#ifndef USE_SQLITE_HDPOOL
    if ((storedFlag & IN_MEMORY) && !(oldFlag & IN_MEMORY)) {
      data.m_fx = fx;
      (*m_descriptorCallback)(data.m_treeDescription, data.m_fx);
    }
#endif

#ifdef USE_SQLITE_HDPOOL
    if ((oldFlag & ON_DISK) && !(storedFlag & ON_DISK))
      TCacheResourcePool::instance()->releaseReferences(
          "P" + QString::number(data.m_passiveCacheId));
#endif
  }
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::invalidateLevel(const std::string &levelName) {
  QMutexLocker locker(&m_mutex);

  // Traverse the managed resources for passed levelName.
  ResourcesTable &table       = m_resources->getTable();
  ResourcesTable::Iterator it = table.begin();
  while (it) {
    std::set<LockedResourceP> &resources = *it;
    std::set<LockedResourceP>::iterator jt, kt;
    for (jt = resources.begin(); jt != resources.end();) {
      if ((*jt)->getName().find(levelName) != std::string::npos) {
        kt = jt++;
        it->erase(kt);
      } else
        ++jt;
    }

    if (resources.empty())
      it = table.erase(it);
    else
      ++it;
  }

#ifdef USE_SQLITE_HDPOOL
  // Store the level name until the invalidation is forced
  m_invalidatedLevels.insert(levelName);
#endif
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::forceInvalidate() {
#ifdef USE_SQLITE_HDPOOL
  TCacheResourcePool *pool = TCacheResourcePool::instance();

  // Clear all invalidated levels from the resource pool
  std::set<std::string>::iterator it;
  for (it = m_invalidatedLevels.begin(); it != m_invalidatedLevels.end(); ++it)
    pool->clearKeyword(*it);

  m_invalidatedLevels.clear();
#endif
}

//-------------------------------------------------------------------------

// Generate the fx's tree description. If it is contained in one of those
// stored with cached fxs, release their associated resources.
void TPassiveCacheManager::onFxChanged(const TFxP &fx) {
#ifndef USE_SQLITE_HDPOOL

  std::string fxTreeDescription;
  (*m_descriptorCallback)(fxTreeDescription, fx);

  unsigned int count = m_fxDataVector.size();
  for (unsigned int i = 0; i < count; ++i) {
    FxData &data = m_fxDataVector[i];

    if (!data.m_fx) continue;

    if (data.m_treeDescription.find(fxTreeDescription) != std::string::npos)
      m_resources->getTable().erase(data.m_passiveCacheId);
  }

#endif
}

//-------------------------------------------------------------------------

// Regenerate the tree descriptions of cached fxs. If the new description does
// not match the previous one, release the associated resources.
void TPassiveCacheManager::onXsheetChanged() {
#ifndef USE_SQLITE_HDPOOL

#ifdef DIAGNOSTICS
  DIAGNOSTICS_STR("#activity.txt | " + QTime::currentTime().toString() +
                  " XSheet changed");
#endif

  unsigned int count = m_fxDataVector.size();
  for (unsigned int i = 0; i < count; ++i) {
    FxData &data = m_fxDataVector[i];

    if (!data.m_fx) continue;

    std::string newTreeDescription;
    (*m_descriptorCallback)(newTreeDescription, data.m_fx);

    if (data.m_treeDescription != newTreeDescription) {
      m_resources->getTable().erase(data.m_passiveCacheId);
      data.m_treeDescription = newTreeDescription;
    }
  }

#endif
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::getResource(TCacheResourceP &resource,
                                       const std::string &alias, const TFxP &fx,
                                       double frame, const TRenderSettings &rs,
                                       ResourceDeclaration *resData) {
  if (!(m_enabled && fx && rs.m_userCachable)) return;

  StorageFlag flag = getStorageMode(fx.getPointer());
  if (flag == NONE) return;

  std::string contextName(getContextName());
  if (contextName.empty()) return;

  // Build a resource if none was passed.
  if (!resource) resource = TCacheResourceP(alias, true);

#ifdef USE_SQLITE_HDPOOL
  if (flag & ON_DISK) {
    resource->enableBackup();

    int passiveCacheId =
        m_fxDataVector[fx->getAttributes()->passiveCacheDataIdx()]
            .m_passiveCacheId;
    TCacheResourcePool::instance()->addReference(
        resource, "P" + QString::number(passiveCacheId));
  }
#endif

  if (flag & IN_MEMORY) {
    QMutexLocker locker(&m_mutex);

    int passiveCacheId =
        m_fxDataVector[fx->getAttributes()->passiveCacheDataIdx()]
            .m_passiveCacheId;
    m_resources->getTable().value(contextName, passiveCacheId).insert(resource);
  }
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::releaseContextNamesWithPrefix(
    const std::string &prefix) {
  QMutexLocker locker(&m_mutex);

#ifdef DIAGNOSTICS
  DIAGNOSTICS_STR("#activity.txt | " + QTime::currentTime().toString() +
                  " Release Context Name (" + QString::fromStdString(prefix) +
                  ")");
#endif

  // Retrieve the context range
  std::string prefixPlus1 = prefix;
  prefixPlus1[prefix.size() - 1]++;

  {
    std::map<std::string, UCHAR>::iterator it, jt;
    it = m_contextNames.lower_bound(prefix);
    jt = m_contextNames.lower_bound(prefixPlus1);
    m_contextNames.erase(it, jt);
  }

  // Transfer to temporary
  ResourcesTable &table = m_resources->getTable();
  std::map<std::string, ResourcesTable::Row> &rows =
      m_resources->getTable().rows();

  std::map<std::string, ResourcesTable::Row>::iterator it, jt, kt;
  it = rows.lower_bound(prefix);
  jt = rows.lower_bound(prefixPlus1);

  std::string temporaryName("T");
  for (kt = it; kt != jt; ++kt) {
    ResourcesTable::RowIterator lt(kt);
    for (; lt; ++lt)
      table.value(temporaryName, lt.col()).insert(lt->begin(), lt->end());
  }
  rows.erase(it, jt == rows.end() ? rows.lower_bound(prefixPlus1) : jt);
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::releaseOldResources() {
  QMutexLocker locker(&m_mutex);

  // Release all the resources that were stored in the old render instance of
  // the
  // context, PLUS those of the temporary container.
  // Resources that were held by the TPassiveCacheManager::getResource()
  // procedure
  // are now duplicated in a proper row of the table - and will not be freed.
  std::string contextName(getContextName());
  if (contextName.empty()) return;

  char &lastChar = contextName[contextName.size() - 1];
  lastChar       = '0' + !(lastChar - '0');

  ResourcesTable &table = m_resources->getTable();
  table.erase(contextName);
  table.erase("T");
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::onRenderInstanceStart(unsigned long renderId) {
  TFxCacheManagerDelegate::onRenderInstanceStart(renderId);

#ifdef USE_SQLITE_HDPOOL
  // Force invalidation of levels before the render starts
  forceInvalidate();
#endif
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::onRenderInstanceEnd(unsigned long renderId) {
  QMutexLocker locker(&m_mutex);

  releaseOldResources();
  m_contextNamesByRenderId.erase(renderId);
}

//-------------------------------------------------------------------------

void TPassiveCacheManager::onRenderStatusEnd(int renderStatus) {
  if (renderStatus == TRenderer::TESTRUN) releaseOldResources();
}
