#pragma once

#ifndef TFXCACHEMANAGER_H
#define TFXCACHEMANAGER_H

#include <memory>

#include "trenderresourcemanager.h"
#include "tcacheresource.h"

//=======================================================================

//  Forward declarations
struct ResourceDeclaration;
typedef std::pair<ResourceDeclaration *, TCacheResourceP> ResourceData;

class TFxCacheManager;
class TFxCacheManagerDelegate;
// class TFxCacheManagerListener;

//=======================================================================

//=====================================
//    ResourceBuilder class
//-------------------------------------

/*!
The ResourceBuilder class is the preferential base interface that must be used
to
by eventual users to interact with the Toonz cache during render processes.

Caching can be used to specify that some intermediate rendering result is
useful,
and efforts should be taken to ensure that it is stored in memory rather than
repeatedly rebuilt, when it is sufficiently convenient.
\n
These kind of operations are automatically performed by Toonz on the overall
results of
fx nodes - users should definitely implement hidden fx nodes rather than a
specialized
ResourceBuilder class. However, ResourceBuilder provides a more simple and
generic API
than TRasterFx's, and does not force code splitting into several fxs.

This class already works out most of the implementation details necessary
to deal with cache management and predictability issues.
\n \n
It just requires the implementation of two essential functions:

<li> The compute() method, which performs the effective calculation of the
passed tile.
<li> The simCompute() method, which is a dummy simulation of the above
compute(),
used by the builder for predictive purposes.

Please refer to their descriptions for implementation details.
\n \n
Once the necessary compute() and simCompute() functions have been supplied, the
ResourceBuilder can be used to obtain the resource data associated with passed
TTile
simply by invoking the build() method. The build() invocation performs all the
cache resource availability checkings, retrieval, and the eventual resource
calculation
on its own.
\n \n
The simBuild() method is the dummy counterpart for build(), which must be used
in the
simCompute() simulations whenever some father resource requires that a child
resource
is built first.
*/

class DVAPI ResourceBuilder {
  TFxCacheManager *m_cacheManager;
  ResourceData m_data;

protected:
  virtual void simCompute(const TRectD &rect) = 0;
  virtual void compute(const TRectD &rect)    = 0;

  virtual void upload(TCacheResourceP &resource)   = 0;
  virtual bool download(TCacheResourceP &resource) = 0;

public:
  ResourceBuilder(const std::string &resourceName, const TFxP &fx, double frame,
                  const TRenderSettings &rs);
  virtual ~ResourceBuilder() {}

  static void declareResource(const std::string &alias, const TFxP &fx,
                              const TRectD &rect, double frame,
                              const TRenderSettings &rs,
                              bool subtileable = true);

  void simBuild(const TRectD &tile);
  void build(const TRectD &tile);
};

//************************************************************************************************
//    Cache Management internals
//************************************************************************************************

//! The ResourceDeclaration structure contains the informations retrieved in the
//! predictive processes performed before a render process. It can be retrieved
//! via the TFxCacheManager::getResource() method or passed directly by it to
//! TFxCacheManagerDelegate instances.

struct ResourceDeclaration {
  //! ResourceDeclaration's Raw data is gathered in the first predictive run for
  //! a render process. It is destroyed before the actual process begins due to
  //! its considerable size.
  struct RawData {
    // Requested infos for calculation

    //! Fx who generated the data
    TFxP m_fx;
    //! Frame at which the data was generated
    double m_frame;
    //! Render settings associated with the data
    TRenderSettings m_rs;
    //! Tiles declared in the prediction process.
    //! May differ from those actually calculated by the renderer.
    std::vector<TRectD> m_tiles;

    // Useful infos

    //! Bounding box associated with the considered fx calculation
    // TRectD m_bbox;

    //! Whether the resource may be subdivided for calculation
    bool m_subtileable;
  };

  //! The ResourceDeclaration's Raw data is processed into a vector of
  //! TileDatas, representing the actual tiles to be computed in the render
  //! process.
  struct TileData {
    TRectD m_rect;
    int m_refCount;
    bool m_calculated;

    TileData(const TRectD &rect)
        : m_rect(rect), m_refCount(0), m_calculated(false) {}
  };

public:
  RawData *m_rawData;
  std::vector<TileData> m_tiles;
  int m_tilesCount;

  ResourceDeclaration() : m_rawData(0), m_tilesCount(0) {}
  ~ResourceDeclaration() {}
};

//=======================================================================

typedef std::pair<ResourceDeclaration *, TCacheResourceP> ResourceData;

//=======================================================================

//======================
//    Cache Manager
//----------------------

/*!
The TFxCacheManager is the main resource manager that deals with fx nodes
caching
inside Toonz rendering processes. During a render process, a considerable amount
of time
can be saved if any intermediate rendered result used more than once is saved in
cache
and reused multiple times rather than rebuilding it every time. In some cases,
the user may even
know that some specific intermediate result is useful for rendering purposes,
and could
explicitly require that it needs to be cached.
\n \n
In Toonz's rendering workflow, these operations are managed by the
TFxCacheManager class,
which delegates specific caching procedures to a group of
TFxCacheManagerDelegate instances.
\n \n
Access to a cache resource is performed through the getResource() method, which
requires
a description of the resource (as specified in the TCacheResource class)
and some informations - namely, the fx, render settings and frame - about the
circumstances
of the request. If the resource is managed by some manager delegate, it will be
returned.
\n \n
Resource uploads and downloads must be notified through the apposite methods to
inform the
manager delegates appropriately. These notifications can be invoked without need
that
the associated method is actually invoked for the resource - this is especially
required
when the TRenderer instance is simulating the actual render process for
predictive purposes.

\sa TCacheResource, TPassiveCacheManager, TPredictiveCacheManager and TRenderer
classes
*/

class DVAPI TFxCacheManager final : public TRenderResourceManager {
  T_RENDER_RESOURCE_MANAGER

private:
  // std::set<TFxCacheManagerListener*> m_listeners;
  std::set<TFxCacheManagerDelegate *> m_delegates;

  std::set<std::string> m_staticCacheIds;

  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  TFxCacheManager();
  ~TFxCacheManager();

  static TFxCacheManager *instance();

  void add(const std::string &cacheId, TImageP img);
  void remove(const std::string &cacheId);

  void onRenderStatusStart(int renderStatus) override;
  void onRenderStatusEnd(int renderStatus) override;

  // void install(TFxCacheManagerListener* listener);
  // void uninstall(TFxCacheManagerListener* listener);

private:
  friend class ResourceBuilder;

  void declareResource(const std::string &alias, const TFxP &fx,
                       const TRectD &rect, double frame,
                       const TRenderSettings &rs, bool subtileable);

  ResourceData getResource(const std::string &resourceName, const TFxP &fx,
                           double frame, const TRenderSettings &rs);

  /*void notifyResourceUpload(const TCacheResourceP& resource, const TRect&
rect);
void notifyResourceDownload(const TCacheResourceP& resource, const TRect& rect);
void notifyPredictedRelease(const TCacheResourceP& resource);*/

private:
  friend class TFxCacheManagerDelegate;

  void install(TFxCacheManagerDelegate *managerDelegate);
};

//=======================================================================

//===============================
//    Cache Manager Delegate
//-------------------------------

/*!
TFxCacheManagerDelegate is the base class to implement cache managers used
in Toonz render processes.

A TFxCacheManagerDelegate instance automatically receives access notifications
through the TFxCacheManager associated with a render process.
These may either concern a request of a certain cache resource, or the storage
proposal of a calculated node result.

\warning Delegates are expected to be dependent on TFxCacheManager - use the
MANAGER_FILESCOPE_DECLARATION_DEP macro to achieve that.

\sa TCacheResource and TFxCacheManager classes
*/

class TFxCacheManagerDelegate : public TRenderResourceManager {
public:
  TFxCacheManagerDelegate() {}

  virtual void getResource(TCacheResourceP &resource, const std::string &alias,
                           const TFxP &fx, double frame,
                           const TRenderSettings &rs,
                           ResourceDeclaration *resData) = 0;

  void onRenderInstanceStart(unsigned long renderId) override {
    assert(TFxCacheManager::instance());
    TFxCacheManager::instance()->install(this);
  }
};

//=======================================================================

//===============================
//    Cache Manager Listener
//-------------------------------

/*class TFxCacheManagerListener
{
public:

  TFxCacheManagerListener() {}

  virtual void onResourceUpload(const TCacheResourceP& resource, const TRect&
rect) {}
  virtual void onResourceDownload(const TCacheResourceP& resource, const TRect&
rect) {}
  virtual void onPredictedRelease(const TCacheResourceP& resource) {}
};*/

#endif  // TFXCACHEMANAGER_H
