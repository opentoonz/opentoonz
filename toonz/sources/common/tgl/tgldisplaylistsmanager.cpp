

// TnzCore includes
#include "tsmartpointer.h"

// tcg includes
#include "tcg/tcg_list.h"

#include "tgldisplaylistsmanager.h"

//***********************************************************************************************
//    Local namespace - declarations
//***********************************************************************************************

namespace {

struct ProxyReference {
  TGLDisplayListsProxy *m_proxy;
  int m_refCount;

  ProxyReference(TGLDisplayListsProxy *proxy) : m_proxy(proxy), m_refCount() {}
};

}  // namespace

//***********************************************************************************************
//    Local namespace - globals
//***********************************************************************************************

namespace {

tcg::list<ProxyReference> m_proxies;
std::map<TGlContext, int> m_proxyIdsByContext;

}  // namespace

//***********************************************************************************************
//    TGLDisplayListsManager  implementation
//***********************************************************************************************

TGLDisplayListsManager *TGLDisplayListsManager::instance() {
  static TGLDisplayListsManager theInstance;
  return &theInstance;
}

//-----------------------------------------------------------------------------------

int TGLDisplayListsManager::storeProxy(TGLDisplayListsProxy *proxy) {
  return m_proxies.push_back(ProxyReference(proxy));
}

//-----------------------------------------------------------------------------------

void TGLDisplayListsManager::attachContext(int dlSpaceId, TGlContext context) {
  m_proxyIdsByContext.insert(std::make_pair(context, dlSpaceId));
  ++m_proxies[dlSpaceId].m_refCount;
}

//-----------------------------------------------------------------------------------

void TGLDisplayListsManager::releaseContext(TGlContext context) {
  std::map<TGlContext, int>::iterator it = m_proxyIdsByContext.find(context);

  assert(it != m_proxyIdsByContext.end());
  if (it == m_proxyIdsByContext.end()) return;

  int dlSpaceId = it->second;
  if (--m_proxies[dlSpaceId].m_refCount <= 0) {
    // Notify observers first
    observers_container::const_iterator ot, oEnd = observers().end();
    for (ot = observers().begin(); ot != oEnd; ++ot)
      static_cast<Observer *>(*ot)->onDisplayListDestroyed(dlSpaceId);

    // Then, destroy stuff
    delete m_proxies[dlSpaceId].m_proxy;
    m_proxies.erase(dlSpaceId);
  }

  m_proxyIdsByContext.erase(it);
}

//-----------------------------------------------------------------------------------

int TGLDisplayListsManager::displayListsSpaceId(TGlContext context) {
  std::map<TGlContext, int>::iterator it = m_proxyIdsByContext.find(context);
  return (it == m_proxyIdsByContext.end()) ? -1 : it->second;
}

//-----------------------------------------------------------------------------------

TGLDisplayListsProxy *TGLDisplayListsManager::dlProxy(int dlSpaceId) {
  return m_proxies[dlSpaceId].m_proxy;
}
