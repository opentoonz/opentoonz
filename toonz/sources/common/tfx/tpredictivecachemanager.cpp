

#include "trenderer.h"
#include "trasterfx.h"

#include <algorithm>

#include "tpredictivecachemanager.h"

//************************************************************************************************
//    Preliminaries
//************************************************************************************************

class TPredictiveCacheManagerGenerator final
    : public TRenderResourceManagerGenerator {
public:
  TPredictiveCacheManagerGenerator() : TRenderResourceManagerGenerator(true) {}

  TRenderResourceManager *operator()(void) override {
    return new TPredictiveCacheManager;
  }
};

MANAGER_FILESCOPE_DECLARATION_DEP(TPredictiveCacheManager,
                                  TPredictiveCacheManagerGenerator,
                                  TFxCacheManager::deps())

//-------------------------------------------------------------------------

TPredictiveCacheManager *TPredictiveCacheManager::instance() {
  return static_cast<TPredictiveCacheManager *>(
      // TPredictiveCacheManager::gen()->getManager(TRenderer::instance())
      TPredictiveCacheManager::gen()->getManager(TRenderer::renderId()));
}

//************************************************************************************************
//    TPredictiveCacheManager::Imp definition
//************************************************************************************************

//=======================
//    PredictionData
//-----------------------

struct PredictionData {
  const ResourceDeclaration *m_decl;
  int m_usageCount;

  PredictionData(const ResourceDeclaration *declaration)
      : m_decl(declaration), m_usageCount(1) {}
};

//============================================================================================

//=====================================
//    TPredictiveCacheManager::Imp
//-------------------------------------

class TPredictiveCacheManager::Imp {
public:
  int m_renderStatus;
  bool m_enabled;

  std::map<TCacheResourceP, PredictionData> m_resources;
  QMutex m_mutex;

public:
  Imp()
      : m_renderStatus(TRenderer::IDLE)
      , m_enabled(TRenderer::instance().isPrecomputingEnabled()) {}

  void run(TCacheResourceP &resource, const std::string &alias, const TFxP &fx,
           double frame, const TRenderSettings &rs,
           ResourceDeclaration *resData) {
    switch (m_renderStatus) {
    case TRenderer::IDLE:
    case TRenderer::COMPUTING:
      getResourceComputing(resource, alias, fx, frame, rs, resData);
      break;
    case TRenderer::TESTRUN:
      getResourceTestRun(resource, alias, fx, frame, rs, resData);
      break;
    }
  }

private:
  void getResourceTestRun(TCacheResourceP &resource, const std::string &alias,
                          const TFxP &fx, double frame,
                          const TRenderSettings &rs,
                          ResourceDeclaration *resData);

  void getResourceComputing(TCacheResourceP &resource, const std::string &alias,
                            const TFxP &fx, double frame,
                            const TRenderSettings &rs,
                            ResourceDeclaration *resData);
};

//************************************************************************************************
//    TPredictiveCacheManager methods
//************************************************************************************************

TPredictiveCacheManager::TPredictiveCacheManager()
    : m_imp(new TPredictiveCacheManager::Imp()) {}

//---------------------------------------------------------------------------

TPredictiveCacheManager::~TPredictiveCacheManager() {}

//---------------------------------------------------------------------------

void TPredictiveCacheManager::setMaxTileSize(int maxTileSize) {}

//---------------------------------------------------------------------------

void TPredictiveCacheManager::setBPP(int bpp) {}

//---------------------------------------------------------------------------

void TPredictiveCacheManager::getResource(TCacheResourceP &resource,
                                          const std::string &alias,
                                          const TFxP &fx, double frame,
                                          const TRenderSettings &rs,
                                          ResourceDeclaration *resData) {
  if (!m_imp->m_enabled) return;

  m_imp->run(resource, alias, fx, frame, rs, resData);
}

//************************************************************************************************
//    Notification-related functions
//************************************************************************************************

void TPredictiveCacheManager::Imp::getResourceTestRun(
    TCacheResourceP &resource, const std::string &alias, const TFxP &fx,
    double frame, const TRenderSettings &rs, ResourceDeclaration *resData) {
  assert(resData && resData->m_rawData);
  if (!(resData && resData->m_rawData))
    // This is a very rare case. I've seen it happen once in a 'pathologic' case
    // which involved affines truncation while building aliases.
    // The rendering system didn't expect the truncated part 'resurface' in a
    // downstream fx with a slightly different affine alias.

    // TODO: Affines should be coded completely in the aliases... in a compact
    // way though.
    return;

  if (!resource) resource = TCacheResourceP(alias, true);

  // Lock against concurrent threads
  // QMutexLocker locker(&m_mutex);    //preComputing is currently
  // single-threaded

  std::map<TCacheResourceP, PredictionData>::iterator it =
      m_resources.find(resource);

  if (it != m_resources.end())
    it->second.m_usageCount++;
  else {
    // Already initializes usageCount at 1
    m_resources.insert(std::make_pair(resource, PredictionData(resData)));
  }
}

//---------------------------------------------------------------------------

void TPredictiveCacheManager::Imp::getResourceComputing(
    TCacheResourceP &resource, const std::string &alias, const TFxP &fx,
    double frame, const TRenderSettings &rs, ResourceDeclaration *resData) {
  // If there is no declaration data, either the request can be resolved in one
  // computation code (therefore it is uninteresting for us), or it was never
  // declared.
  // Anyway, return.
  if (!resData) return;

  // NO! The refCount is dynamically depleted - could become 0 from n...
  // assert(!(resData->m_tiles.size() == 1 && resData->m_tiles[0].m_refCount ==
  // 1));

  if (!resource) resource = TCacheResourceP(alias);

  if (!resource) return;

  // Lock against concurrent threads
  QMutexLocker locker(&m_mutex);

  std::map<TCacheResourceP, PredictionData>::iterator it =
      m_resources.find(resource);

  if (it == m_resources.end()) return;

  if (--it->second.m_usageCount <= 0) m_resources.erase(it);
}

//---------------------------------------------------------------------------

void TPredictiveCacheManager::onRenderStatusStart(int renderStatus) {
  m_imp->m_renderStatus = renderStatus;
}

//---------------------------------------------------------------------------

void TPredictiveCacheManager::onRenderStatusEnd(int renderStatus) {
  switch (renderStatus) {
  case TRenderer::TESTRUN:

    // All resources which have just 1 computation tile, which is also
    // referenced
    // only once, are released.

    std::map<TCacheResourceP, PredictionData>::iterator it;
    for (it = m_imp->m_resources.begin(); it != m_imp->m_resources.end();) {
      const ResourceDeclaration *decl = it->second.m_decl;

      if (decl->m_tiles.size() == 1 && decl->m_tiles[0].m_refCount == 1) {
        std::map<TCacheResourceP, PredictionData>::iterator jt = it++;
        m_imp->m_resources.erase(jt);
      } else
        it++;
    }
  }
}
