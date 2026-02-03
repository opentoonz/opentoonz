

// Qt includes
#include <QMap>
#include <QSettings>
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>

//#define USE_SQLITE_HDPOOL

#ifdef USE_SQLITE_HDPOOL
// SQLite include
#include "sqlite/sqlite3.h"
#endif

#include "tcacheresourcepool.h"

// Debug
//#define DIAGNOSTICS
//#include "diagnostics.h"

//******************************************************************************************
//    Cache Resource Pool BACKED ON DISK
//******************************************************************************************

// STILL UNDER DEVELOPMENT...
class THDCacheResourcePool {
public:
  THDCacheResourcePool() {}
  ~THDCacheResourcePool() {}
};

//*************************************************************************************
//    Cache resource pool methods involved with HD Pool management
//*************************************************************************************

inline bool TCacheResourcePool::isHDActive() {
#ifdef USE_SQLITE_HDPOOL
  return m_hdPool && m_hdPool->isActive();
#else
  return false;
#endif
}

//-----------------------------------------------------------------------------------

void TCacheResourcePool::reset() { setPath("", "", ""); }

//----------------------------------------------------------------------

// Prevents the resources in memory from backing to disk. Observe that
// the actual content of the resource is NOT invalidated - since resources
// are intended as 'reference-protected' material which is expected to last
// as long as references are held.
void TCacheResourcePool::invalidateAll() {
  QMutexLocker locker(&m_memMutex);

  MemResources::iterator it;
  for (it = m_memResources.begin(); it != m_memResources.end(); ++it)
    it->second->invalidate();
}

//----------------------------------------------------------------------

inline QString TCacheResourcePool::getPoolRoot(QString cacheRoot,
                                               QString projectName,
                                               QString sceneName) {
  return QString(cacheRoot + "/render/" + projectName + "/" + sceneName + "/");
}

//----------------------------------------------------------------------

//! Connects to the pool associated to the given project/scene pair.
//! \warning As this closes the current connection before opening a new one,
//! make sure that no pool access happens at this point. You should also
//! verify that no resource from the old pair still exists.
void TCacheResourcePool::setPath(QString cacheRoot, QString projectName,
                                 QString sceneName) {
  // There should be no resource in memory.
  assert(m_memResources.empty());

  // However, just in case, invalidate all resources so that no more resource
  // backing
  // operation take place for current resources, from now on.
  // No care is paid as to whether active transactions currently exist. You
  // have been warned by the way....
  invalidateAll();

  delete m_hdPool;
  m_hdPool = 0;
  m_path   = TFilePath();

#ifdef USE_SQLITE_HDPOOL

  if (!(cacheRoot.isEmpty() || projectName.isEmpty() || sceneName.isEmpty())) {
    QString hdPoolRoot(getPoolRoot(cacheRoot, projectName, sceneName));
    m_hdPool = new THDCacheResourcePool(hdPoolRoot);
    m_path   = m_hdPool->getResourcesFilePath();
  }

#endif
}

//-----------------------------------------------------------------------------------

void TCacheResourcePool::startBacking(TCacheResource *resource) {
  assert(isHDActive());
  if (!isHDActive()) return;

#ifdef USE_SQLITE_HDPOOL

  resource->m_backEnabled = true;

  m_hdPool->buildBackingPath(resource);

#endif
}

//******************************************************************************************
//    Cache resource pool implementation
//******************************************************************************************

TCacheResourcePool *TCacheResourcePool::instance() {
  static TCacheResourcePool theInstance;
  return &theInstance;
}

//----------------------------------------------------------------------

TCacheResourcePool::TCacheResourcePool()
    : m_memMutex(QMutex::Recursive)
    , m_searchCount(0)
    , m_foundIterator(false)
    , m_searchIterator(m_memResources.end())
    , m_hdPool(0)
    , m_path() {
  // Open the settings for cache retrieval
}

//----------------------------------------------------------------------

TCacheResourcePool::~TCacheResourcePool() {
  // Temporary
  // performAutomaticCleanup();

  delete m_hdPool;
}

//----------------------------------------------------------------------

const TFilePath &TCacheResourcePool::getPath() const { return m_path; }

//----------------------------------------------------------------------

//! Initializes an optimized search on the pool for a specific resource, caching
//! successive
//! results.
//! \note Pool searches are serialized, and calls to this method lock the pool's
//! mutex until a
//! corresponding number of endCachedSearch() methods are invoked.
void TCacheResourcePool::beginCachedSearch() {
  m_memMutex.lock();
  m_searchCount++;
}

//----------------------------------------------------------------------

//! The inverse to beginCachedSearch(). This method \b MUST be called in
//! correspondence to
//! beginCachedSearch() calls.
void TCacheResourcePool::endCachedSearch() {
  if (--m_searchCount <= 0) {
    m_foundIterator  = false;
    m_searchIterator = m_memResources.end();
  }
  m_memMutex.unlock();
}

//----------------------------------------------------------------------

//! Attempts retrieval of the resource with specified name, and eventually
//! creates it if
//! the createIfNone parameter is set.
TCacheResource *TCacheResourcePool::getResource(const std::string &name,
                                                bool createIfNone) {
  // DIAGNOSTICS_TIMER("#times.txt | getResource Overall time");
  // DIAGNOSTICS_MEANTIMER("#times.txt | getResource Mean time");

  TCacheResource *result = 0;

  // NOTA: Passa ad un oggetto lockatore. Quello e' in grado di gestire i casi
  // di eccezioni ecc..
  beginCachedSearch();

  // Search for an already allocated resource
  if (m_searchIterator == m_memResources.end()) {
    m_searchIterator = m_memResources.lower_bound(name);
    if (m_searchIterator != m_memResources.end()) {
      if (!(name < m_searchIterator->first)) {
        m_foundIterator = true;
      } else if (m_searchIterator != m_memResources.begin()) {
        m_searchIterator--;
      }
    }
  }

  if (m_foundIterator) {
    result = m_searchIterator->second;

    endCachedSearch();
    return result;
  }

  {
    QString resourcePath;
    QString resourceFlags;

    if (isHDActive()) {
#ifdef USE_SQLITE_HDPOOL

      // DIAGNOSTICS_TIMER("#times.txt | HDPOOL getResource Overall time");
      // DIAGNOSTICS_MEANTIMER("#times.txt | HDPOOL getResource Mean time");

      // Search in the HD pool
      ReadQuery query(m_hdPool);

      bool ret =
          query.prepare("SELECT Path, Flags FROM Resources WHERE Name = '" +
                        QString::fromStdString(name) + "';");

      // If an error occurred, assume the resource does not exist. Doing nothing
      // works fine.
      assert(ret);

      if (query.step()) {
        resourcePath  = query.value(0);
        resourceFlags = query.value(1);
      }

#endif
    }

    if (!resourcePath.isEmpty() || createIfNone) {
      TCacheResource *result = new TCacheResource;
      result->m_pos          = m_searchIterator =
          m_memResources.insert(m_searchIterator, std::make_pair(name, result));

// DIAGNOSTICS_STRSET("#resources.txt | RISORSE | " + QString::number((UINT)
// result) + " | Name",
// QString::fromStdString(name).left(70));

#ifdef USE_SQLITE_HDPOOL
      if (isHDActive()) m_hdPool->loadResourceInfos(result, resourcePath);
#endif

      m_foundIterator = true;
      endCachedSearch();

      return result;
    }
  }

  endCachedSearch();
  return 0;
}

//----------------------------------------------------------------------

void TCacheResourcePool::releaseResource(TCacheResource *resource) {
  QMutexLocker locker(&m_memMutex);

  // Re-check the resource's reference count. This is necessary since a
  // concurrent
  // thread may have locked the memMutex for resource retrieval BEFORE this one.
  // If that is the case, the resource's refCount has increased back above 0.
  if (resource->m_refCount > 0) return;

#ifdef USE_SQLITE_HDPOOL
  QMutexLocker flushLocker(isHDActive() ? &m_hdPool->m_flushMutex : 0);

  if (isHDActive()) {
    // Flush all resource updates as this resource is being destroyed
    m_hdPool->flushResources();

    // Save the resource infos
    m_hdPool->saveResourceInfos(resource);
  }
#endif

  m_memResources.erase(resource->m_pos);
  delete resource;
}
