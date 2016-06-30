

#include <string>

#include "trenderer.h"
#include "tcacheresourcepool.h"

#include "tfxcachemanager.h"

// Debug
//#define DIAGNOSTICS
#ifdef DIAGNOSTICS

#include "diagnostics.h"

//#define WRITESTACK
//#define WRITESUBRECTS
//#define WRITEGENERAL

namespace {
QString traduce(const TRectD &rect) {
  return "[" + QString::number(rect.x0) + " " + QString::number(rect.y0) + " " +
         QString::number(rect.x1) + " " + QString::number(rect.y1) + "]";
}

QString traduce(const TTile &tile) {
  TDimension dim(tile.getRaster()->getSize());
  TRectD tileRect(tile.m_pos, TDimensionD(dim.lx, dim.ly));
  return traduce(tileRect);
}

QString prefixInfo("#info.txt | ");
QString prefixWarn("#warning.txt | ");
QString prefixErr("#error.txt | ");
QString prefixTest("#TestRun.txt | ");
QString prefixComp("#Computing.txt | ");
QString prefixSubTiles("#SubTiles.txt | ");
}

#endif  // DIAGNOSTICS

//****************************************************************************************************
//    Explanation
//****************************************************************************************************

/*
This file contains most of the code that deals with smart caching during a Toonz
render process.

The paradigms on which the 'smart caching' takes place are:

  - Only calculations resulting in an image positioned in a plane are dealt.
These results are
    called 'tiles' - and are modeled by a TTile instance.
    These images MUST BE wrapped to INTEGER POSITIONS on the reference, and are
intended so that
    a PIXEL corresponds to a UNIT SQUARE.
  - Given one such calculation procedure, it MUST BE SIMULABLE, so that children
calculations
    are invoked 'faithfully' with respect to the actual calculation.
    This is necessary to predict results in advance in force of which we can
efficiently store
    results in the cache, releasing them when they will no longer be required.

Now, the principal classes dealing with this job are:

 - One TFxCacheManager per render instance, used to store predictive
informations.
 - A set of TFxCacheManagerDelegate per render instance, used to store
references to cache resources
   and providing reasons for caching results.
 - The ResourceBuilder interface class, used by users to access the smart
caching ensemble. This class
   implements the actual resources build and simulation code.

*/

//****************************************************************************************************
//    Preliminaries
//****************************************************************************************************

namespace {
// Global variables

//-----------------------------------------------------------------------------------------

// Utility functions

inline QRect toQRect(const TRect &r) {
  return QRect(r.x0, r.y0, r.getLx(), r.getLy());
}
inline TRect toTRect(const QRect &r) {
  return TRect(r.left(), r.top(), r.right(), r.bottom());
}
inline QPoint toQPoint(const TPoint &p) { return QPoint(p.x, p.y); }

inline bool isEmpty(const TRectD &rect) {
  return rect.x0 >= rect.x1 || rect.y0 >= rect.y1;
}
inline void enlargeToI(TRectD &r) {
  TRectD temp(tfloor(r.x0), tfloor(r.y0), tceil(r.x1), tceil(r.y1));
  if (!isEmpty(temp))
    r = temp;  // Since r could have TConsts::infiniteRectD-like coordinates...
}

// Qt's contains actually returns QRegion::intersected... I wonder why...
inline bool contains(const QRegion &region, const TRect &rect) {
  return QRegion(toQRect(rect)).subtracted(region).isEmpty();
}

bool getTilesToBuild(
    const ResourceData &data, const TRectD &rect,
    std::vector<ResourceDeclaration::TileData *> &rectsToCalculate);
}

//****************************************************************************************************
//    TFxCacheManager Generator
//****************************************************************************************************

class TFxCacheManagerGenerator final : public TRenderResourceManagerGenerator {
public:
  TFxCacheManagerGenerator() : TRenderResourceManagerGenerator(true) {}

  TRenderResourceManager *operator()() override { return new TFxCacheManager; }
};

MANAGER_FILESCOPE_DECLARATION(TFxCacheManager, TFxCacheManagerGenerator);

//****************************************************************************************************
//    TFxCacheManager implementation
//****************************************************************************************************

class TFxCacheManager::Imp {
public:
  typedef std::map<std::string, ResourceDeclaration> ResourceInstanceDataMap;

  ResourceInstanceDataMap m_resourcesData;
  std::map<ResourceDeclaration *, ResourceDeclaration::RawData> m_rawData;
  int m_renderStatus;

  QMutex m_mutex;

public:
  void prepareTilesToCalculate(ResourceDeclaration &data);
  inline void subdivideIntoSmallerTiles(const TRectD &rect,
                                        std::vector<TRectD> &tileSet);
  void recursiveRectSubdivide(
      std::vector<ResourceDeclaration::TileData> &results, TRasterFx *fx,
      const TRectD &rect, double frame, const TRenderSettings &info,
      int dropTol = (std::numeric_limits<int>::max)());
};

//****************************************************************************************************
//    Methods implementation
//****************************************************************************************************

//========================
//    TFxCacheManager
//------------------------

TFxCacheManager::TFxCacheManager() : m_imp(new Imp) {}

//-----------------------------------------------------------------------------------

TFxCacheManager::~TFxCacheManager() {
  // Release all the static-cached images
  std::set<std::string>::iterator it;
  for (it = m_staticCacheIds.begin(); it != m_staticCacheIds.end(); ++it)
    TImageCache::instance()->remove(*it);
}

//-----------------------------------------------------------------------------------

TFxCacheManager *TFxCacheManager::instance() {
  return static_cast<TFxCacheManager *>(
      TFxCacheManager::gen()->getManager(TRenderer::renderId()));
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::add(const std::string &cacheId, TImageP img) {
  TImageCache::instance()->add(cacheId, img);

  QMutexLocker locker(&m_imp->m_mutex);
  m_staticCacheIds.insert(cacheId);
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::remove(const std::string &cacheId) {
  TImageCache::instance()->remove(cacheId);

  QMutexLocker locker(&m_imp->m_mutex);
  m_staticCacheIds.erase(cacheId);
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::install(TFxCacheManagerDelegate *managerDelegate) {
  m_delegates.insert(managerDelegate);
}

//-----------------------------------------------------------------------------------

/*void TFxCacheManager::install(TFxCacheManagerListener* listener)
{
  m_listeners.insert(listener);
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::notifyResourceUpload(const TCacheResourceP& resource,
const TRect& rect)
{
  std::set<TFxCacheManagerListener*>::iterator it;
  for(it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onResourceUpload(resource, rect);
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::notifyResourceDownload(const TCacheResourceP& resource,
const TRect& rect)
{
  std::set<TFxCacheManagerListener*>::iterator it;
  for(it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onResourceDownload(resource, rect);
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::notifyPredictedRelease(const TCacheResourceP& resource)
{
  std::set<TFxCacheManagerListener*>::iterator it;
  for(it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onPredictedRelease(resource);
}*/

//****************************************************************************************************
//    Resources dealing
//****************************************************************************************************

void TFxCacheManager::declareResource(const std::string &alias, const TFxP &fx,
                                      const TRectD &rect, double frame,
                                      const TRenderSettings &rs,
                                      bool subtileable) {
  Imp::ResourceInstanceDataMap::iterator it;
  it = m_imp->m_resourcesData
           .insert(std::make_pair(alias, ResourceDeclaration()))
           .first;
  it->second.m_rawData =
      &m_imp->m_rawData
           .insert(std::make_pair(&it->second, ResourceDeclaration::RawData()))
           .first->second;

  ResourceDeclaration::RawData &rawData = *it->second.m_rawData;

  // Assign the sim data
  rawData.m_fx = fx;
  rawData.m_tiles.push_back(rect);
  rawData.m_rs    = rs;
  rawData.m_frame = frame;
  // rawData.m_bbox = bbox;
  rawData.m_subtileable = subtileable;
}

//-----------------------------------------------------------------------------------

ResourceData TFxCacheManager::getResource(const std::string &alias,
                                          const TFxP &fx, double frame,
                                          const TRenderSettings &rs) {
  TCacheResourceP result, temp;

  // Seek the associated infos
  Imp::ResourceInstanceDataMap::iterator jt =
      m_imp->m_resourcesData.find(alias);
  ResourceDeclaration *decl =
      (jt == m_imp->m_resourcesData.end()) ? 0 : &jt->second;

  // Search the resource in cached mode.
  // TCacheResourcePool* pool = TCacheResourcePool::instance();
  // pool->beginCachedSearch();

  // Ask every installed delegate if it's managing - or want to manage
  // the passed resource specs.
  std::set<TFxCacheManagerDelegate *>::iterator it;
  for (it = m_delegates.begin(); it != m_delegates.end(); ++it) {
    (*it)->getResource(temp, alias, fx, frame, rs, decl);
    if (!result && temp) result = temp;
  }

  // pool->endCachedSearch();

  return ResourceData(decl, result);
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::onRenderStatusStart(int renderStatus) {
  // Store current render status
  m_imp->m_renderStatus = renderStatus;

#ifdef WRITESTACK
  if (renderStatus == TRenderer::TESTRUN) {
    DIAGNOSTICS_GLOSTRSET("status", "test");
    DIAGNOSTICS_GLOSET("testInst", DIAGNOSTICS_GLOGET("compInst") + 1);
    DIAGNOSTICS_GLOSTRSET("instVar",
                          QString::number(DIAGNOSTICS_GLOGET("testInst")));
    DIAGNOSTICS_GLOSTRSET(
        "testRenderStr",
        "Render #" + QString::number(DIAGNOSTICS_GLOGET("testInst")) + " | ");
  } else if (renderStatus == TRenderer::COMPUTING) {
    DIAGNOSTICS_GLOSTRSET("status", "comp");
    DIAGNOSTICS_GLOSTRSET("instVar",
                          QString::number(DIAGNOSTICS_GLOGET("compInst")));
    DIAGNOSTICS_GLOSTRSET(
        "compRenderStr",
        "Render #" + QString::number(DIAGNOSTICS_GLOGET("compInst")) + " | ");
    DIAGNOSTICS_GLOSET(DIAGNOSTICS_THRSTRGET("stackVar"), 0);
  }
#endif
}

//-----------------------------------------------------------------------------------

void TFxCacheManager::onRenderStatusEnd(int renderStatus) {
  if (renderStatus == TRenderer::FIRSTRUN) {
    Imp::ResourceInstanceDataMap &resMap = m_imp->m_resourcesData;

    Imp::ResourceInstanceDataMap::iterator it;
    for (it = resMap.begin(); it != resMap.end();) {
      m_imp->prepareTilesToCalculate(it->second);

      // Cannot be done. The resource could still be feasible to caching,
      // due to external requests.

      // Erase all resource datas which have been declared and prepared only
      // once
      /*if(it->second.m_tiles.size() == 1 &&
it->second.m_simData->m_tiles.size() == 1)
{
it = resMap.erase(it);
continue;
}*/

      ++it;
    }
  } else if (renderStatus == TRenderer::TESTRUN) {
    Imp::ResourceInstanceDataMap &resMap = m_imp->m_resourcesData;
    Imp::ResourceInstanceDataMap::iterator it;
    for (it = resMap.begin(); it != resMap.end();) {
      // Release all resource declarations which are declared to be used only
      // once.
      if (it->second.m_tiles.size() == 1 &&
          it->second.m_tiles[0].m_refCount == 1) {
        Imp::ResourceInstanceDataMap::iterator jt = it++;
        resMap.erase(jt);
        continue;
      }

      // In any case, release all simulation datas - they are no longer useful.
      // An associated cache resource avoids deletion only in case some manager
      // retained it.
      it->second.m_rawData = 0;

      ++it;
    }

#ifdef WRITEGENERAL
    DIAGNOSTICS_SET("Declarations used more than once", resMap.size());
#endif

    m_imp->m_rawData.clear();
  }
#ifdef WRITEGENERAL
  else {
    // Print the number of not depleted declarations
    Imp::ResourceInstanceDataMap &resMap = m_imp->m_resourcesData;
    Imp::ResourceInstanceDataMap::iterator it;

    DIAGNOSTICS_ADD(
        prefixErr + "Computing | Declarations survived after Test Run",
        resMap.size());
    if (resMap.size() > 0) {
      for (it = resMap.begin(); it != resMap.end(); ++it) {
        DIAGNOSTICS_STR(prefixErr + "Survived Declarations | " +
                        QString::fromStdString(it->first).left(40));
      }
    }
  }
#endif
}

//****************************************************************************************************
//    Tiles to calculate - methods
//****************************************************************************************************

void TFxCacheManager::Imp::prepareTilesToCalculate(ResourceDeclaration &data) {
  // First, build the total sum of declared tiles
  TRectD sum;
  int tilesCount = data.m_rawData->m_tiles.size();

  for (int i = 0; i < tilesCount; ++i) sum += data.m_rawData->m_tiles[i];

  // Intersect the sum with bbox and ensure integer geometry
  // sum *= data.m_rawData->m_bbox;
  enlargeToI(sum);

  if (!data.m_rawData->m_subtileable) {
    data.m_tiles.push_back(sum);
    return;
  }

  TRasterFx *fx = dynamic_cast<TRasterFx *>(data.m_rawData->m_fx.getPointer());

  // Now, subdivide the sum
  recursiveRectSubdivide(data.m_tiles, fx, sum, data.m_rawData->m_frame,
                         data.m_rawData->m_rs);
}

//---------------------------------------------------------------------------

//! Calculates at max 4 smaller subrects of passed one. Returns true or false
//! whether the subdivision was
//! successfully applied.
inline void TFxCacheManager::Imp::subdivideIntoSmallerTiles(
    const TRectD &rect, std::vector<TRectD> &tileSet) {
  // Find the greater rect edge
  TRectD subTile1, subTile2;
  if (rect.getLx() > rect.getLy()) {
    int sep  = rect.x0 + tceil(0.5 * rect.getLx());
    subTile1 = TRectD(rect.x0, rect.y0, sep, rect.y1);
    subTile2 = TRectD(sep, rect.y0, rect.x1, rect.y1);
  } else {
    int sep  = rect.y0 + tceil(0.5 * rect.getLy());
    subTile1 = TRectD(rect.x0, rect.y0, rect.x1, sep);
    subTile2 = TRectD(rect.x0, sep, rect.x1, rect.y1);
  }

  tileSet.push_back(subTile1);
  tileSet.push_back(subTile2);
}

//---------------------------------------------------------------------------

void TFxCacheManager::Imp::recursiveRectSubdivide(
    std::vector<ResourceDeclaration::TileData> &results, TRasterFx *fx,
    const TRectD &rect, double frame, const TRenderSettings &info,
    int dropTol) {
  // Here is the subdivision strategy:
  //  - First, since cache tiles are newly ALLOCATED, we impose the raster
  //    size restriction on them directly
  //  - Then, we check that the memory requirement for the fx (max raster size
  //    that it will allocate) is little.
  //  - As an exception to previous point, if the memory requirement stagnates
  //    near the same memory size, quit
  // NOTE: Level images pass here, but haven't any fx. So they are currently not
  // subdivided.

  // Retrieve the memory requirement for this input.
  int memReq = fx ? fx->getMemoryRequirement(rect, frame, info) : 0;

  // In case memReq < 0, we assume a strong subdivision denial, just as if the
  // usage was
  // explicitly set as unsubdividable.
  if (memReq < 0) {
    results.push_back(rect);
    return;
  }

  if ((memReq > info.m_maxTileSize && memReq < dropTol) ||
      TRasterFx::memorySize(rect, info.m_bpp) > info.m_maxTileSize) {
    std::vector<TRectD> subTileRects;
    subdivideIntoSmallerTiles(rect, subTileRects);

    while (!subTileRects.empty()) {
      TRectD subTileRect(subTileRects.back());
      subTileRects.pop_back();

      // Pass subdivision below, with updated drop-tolerance
      recursiveRectSubdivide(results, fx, subTileRect, frame, info,
                             memReq - (memReq >> 2));

      // The newly required memory must be under 3/4 of the previous.
      // This is required in order to make it worth subdividing.
    }

    return;
  }

  results.push_back(ResourceDeclaration::TileData(rect));
}

//****************************************************************************************************
//    ResourceBuilder
//****************************************************************************************************

ResourceBuilder::ResourceBuilder(const std::string &resourceName,
                                 const TFxP &fx, double frame,
                                 const TRenderSettings &rs)
    : m_cacheManager(TFxCacheManager::instance())
    , m_data(m_cacheManager->getResource(resourceName, fx, frame, rs)) {
#ifdef WRITESTACK
  DIAGNOSTICS_THRSET("frame", frame);
  DIAGNOSTICS_THRSTRSET(
      "frameStr",
      "Frame " + QString::number(frame).rightJustified(4, ' ') + " | ");
  DIAGNOSTICS_THRSTRSET("stackVar", DIAGNOSTICS_GLOSTRGET("status") + "sv" +
                                        DIAGNOSTICS_GLOSTRGET("instVar") +
                                        "fr" + QString::number(frame));
  DIAGNOSTICS_THRSTRSET("ResourceName",
                        QString::fromStdString(resourceName).left(35));
#endif
}

//-----------------------------------------------------------------------------------

void ResourceBuilder::declareResource(const std::string &alias, const TFxP &fx,
                                      const TRectD &rect, double frame,
                                      const TRenderSettings &rs,
                                      bool subtileable) {
  TFxCacheManager::instance()->declareResource(alias, fx, rect, frame, rs,
                                               subtileable);
}

//-----------------------------------------------------------------------------------

namespace {
// Retrieves all interesting tiles with respect to the build procedure.
// Explicitly, this refers to tiles intersecting the required rect, that are
// either
// not in the resource, or supposed to be not.
// Returns true if the required tile is contained in the sum of the predicted
// ones
// (which should definitely happen if the predictive step is coherent with the
// computation one).
bool getTilesToBuild(
    const ResourceData &data, const TRectD &rect,
    std::vector<ResourceDeclaration::TileData *> &rectsToCalculate) {
  assert(data.first);   // The declaration must DEFINITELY be present
  assert(data.second);  // The resource should be present here

  // Now, fill in with all prepared rects which intersect input rect and are not
  // already in the resource

  std::vector<ResourceDeclaration::TileData> &preparedRects =
      data.first->m_tiles;
  std::vector<ResourceDeclaration::TileData>::iterator jt;

  TRectD sum;
  for (jt = preparedRects.begin(); jt != preparedRects.end(); ++jt) {
    sum += jt->m_rect;

    if (!(isEmpty(rect * jt->m_rect) || jt->m_calculated))
      rectsToCalculate.push_back(&(*jt));
  }

  return sum.contains(rect);
}
}  // namespace

//-----------------------------------------------------------------------------------

void ResourceBuilder::simBuild(const TRectD &rect) {
  // Retrieve the render status
  int renderStatus = m_cacheManager->m_imp->m_renderStatus;

  // In the initial precomputing stage, just retrieve all the declarations
  // without efforts.
  if (renderStatus == TRenderer::FIRSTRUN) {
    simCompute(rect);
    return;
  }

  // Perform the test run
  if (renderStatus == TRenderer::TESTRUN) {
    if (!(m_data.first && m_data.second)) return;

#ifdef WRITESTACK
    QString resName(DIAGNOSTICS_THRSTRGET("ResourceName"));

    QString renderStr(DIAGNOSTICS_GLOSTRGET("testRenderStr"));
    QString frameStr(DIAGNOSTICS_THRSTRGET("frameStr"));
    DIAGNOSTICS_NUMBEREDSTRSET(
        prefixTest + renderStr + frameStr, DIAGNOSTICS_THRSTRGET("stackVar"),
        DIAGNOSTICS_STACKGET("parentResource") + " " + resName, ::traduce(rect),
        4);
    DIAGNOSTICS_PUSHAUTO(
        "parentResource",
        QString::number(DIAGNOSTICS_GLOGET(DIAGNOSTICS_THRSTRGET("stackVar"))),
        bla);
#endif

    // Retrieve the tiles to build
    std::vector<ResourceDeclaration::TileData> &tiles = m_data.first->m_tiles;

    // For every tile intersecting rect
    std::vector<ResourceDeclaration::TileData>::iterator it;
    for (it = tiles.begin(); it != tiles.end(); ++it) {
      ResourceDeclaration::TileData &tileData = *it;

      if (!isEmpty(tileData.m_rect * rect)) {
        // If the tile ref count == 0, assume that this tile has not yet been
        // simComputed.
        // Do so, then; further, add 1 to the number of predicted actively
        // accessed tiles.
        if (tileData.m_refCount == 0) {
#ifdef WRITESUBRECTS
          DIAGNOSTICS_NUMBEREDSTRSET(
              prefixTest + renderStr + frameStr,
              DIAGNOSTICS_THRSTRGET("stackVar"),
              DIAGNOSTICS_STACKGET("parentResource") + "   " + resName,
              ::traduce(tileData.m_rect), 4);
          DIAGNOSTICS_PUSHAUTO("parentResource",
                               QString::number(DIAGNOSTICS_GLOGET(
                                   DIAGNOSTICS_THRSTRGET("stackVar"))),
                               bla2);
#endif

          simCompute(tileData.m_rect);
          ++m_data.first->m_tilesCount;
        }

        // Add a reference to this tile
        ++tileData.m_refCount;

        if (m_data.second) {
          QMutexLocker locker(m_data.second->getMutex());

          TRect tileRectI(tileData.m_rect.x0, tileData.m_rect.y0,
                          tileData.m_rect.x1 - 1, tileData.m_rect.y1 - 1);

          m_data.second->addRef2(tileRectI);
        }
      }
    }

    return;
  }

  // In this case, the simulation is used to declare that this rect will NOT be
  // calculated.
  // So, this is the behaviour: 1 refCount is depleted by all cells intersecting
  // the rect
  // - if the refCount is still >0, then another request will occur that will
  // make the
  // tile calculated. If it is == 0, and the tile has not yet been calculated,
  // then the tile
  // will supposedly be NO MORE calculated, so the simCompute has to be launched
  // on that tile, too.

  if (renderStatus == TRenderer::COMPUTING) {
    if (!(m_data.first && m_data.second)) return;

    QMutexLocker locker(m_data.second->getMutex());

    // Retrieve the tiles to build
    std::vector<ResourceDeclaration::TileData> &tiles = m_data.first->m_tiles;

    // Please note that the request should definitely be fitting the predicted
    // results,
    // since the original rect which generated these simCompute calls must have
    // been fitting.

    // For every tile to build
    std::vector<ResourceDeclaration::TileData>::iterator it;
    for (it = tiles.begin(); it != tiles.end(); ++it) {
      ResourceDeclaration::TileData &tileData = *it;

      if (!isEmpty(tileData.m_rect * rect)) {
        if (tileData.m_refCount <= 0) continue;

        if (--tileData.m_refCount == 0 && !tileData.m_calculated) {
          --m_data.first->m_tilesCount;
          simCompute(tileData.m_rect);
        }

        if (m_data.second) {
          TRect tileRectI(tileData.m_rect.x0, tileData.m_rect.y0,
                          tileData.m_rect.x1 - 1, tileData.m_rect.y1 - 1);

          m_data.second->release2(tileRectI);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------------

void ResourceBuilder::build(const TRectD &tileRect) {
#ifdef WRITESTACK
  QString resName(DIAGNOSTICS_THRSTRGET("ResourceName"));

  QString renderStr(DIAGNOSTICS_GLOSTRGET("compRenderStr"));
  QString frameStr(DIAGNOSTICS_THRSTRGET("frameStr"));
  DIAGNOSTICS_NUMBEREDSTRSET(
      prefixComp + renderStr + frameStr, DIAGNOSTICS_THRSTRGET("stackVar"),
      DIAGNOSTICS_STACKGET("parentResource") + " " + resName,
      ::traduce(tileRect), 4);
  DIAGNOSTICS_PUSHAUTO(
      "parentResource",
      QString::number(DIAGNOSTICS_GLOGET(DIAGNOSTICS_THRSTRGET("stackVar"))),
      bla);
#endif

  // If there is no resource, just compute the tile directly.
  if (!m_data.second) {
#ifdef WRITEGENERAL
    if (m_data.first)
      if (m_data.first->m_tilesCount > 0)
        DIAGNOSTICS_ADD(
            prefixErr +
                "Computing | No-resource build, active decl, tilesCount > 0",
            1);
      else
        DIAGNOSTICS_ADD(
            prefixErr +
                "Computing | No-resource build, active decl, tilesCount <= 0",
            1);
#endif

    // assert(!m_data.first);  //Should have been erased before the COMPUTING
    // run.
    compute(tileRect);
    return;
  }

  // Since this function must be thread-safe, use the appropriate
  // synchronization tool.
  QMutexLocker locker(m_data.second->getMutex());

  // Without declaration, you can just deal with the required tile.
  if (!(m_data.first && m_data.first->m_tilesCount > 0)) {
#ifdef WRITEGENERAL
    if (!m_data.first)
      DIAGNOSTICS_ADD("#error.txt | Resources without declaration", 1);
    else
      DIAGNOSTICS_ADD("#error.txt | Resources with declaration, tilesCount <=0",
                      1);
#endif

    if (download(m_data.second)) return;

    compute(tileRect);

    // Since there is an associated resource, the calculated content is
    // supposedly
    // an interesting one. Upload it.
    upload(m_data.second);

    return;
  }

  // Now, both the declaration and the resource exist.

  // Retrieve the predicted tile that must be built in place of tile.
  TDimension dim(tileRect.getLx(), tileRect.getLy());

  std::vector<ResourceDeclaration::TileData *> tiles;
  bool fittingPrediction = getTilesToBuild(m_data, tileRect, tiles);

  if (!fittingPrediction) {
#ifdef WRITEGENERAL
    DIAGNOSTICS_ADD(prefixErr + "Computing | Not fitting tiles", 1);
#endif

    // If the required tile is not fitting the prediction, we assume it is a
    // full un-predicted one - so no reference count will be updated (this would
    // comply
    // with the simCompute() method, in case we assume that mis-predicted
    // computes are always prudently built IN EXCESS).

    // For now, just calculate it and stop.
    locker.unlock();

    compute(tileRect);
    return;
  }

  // If necessary, calculate something
  if (tiles.size() > 0) {
    // For every tile to build
    std::vector<ResourceDeclaration::TileData *>::iterator it;
    for (it = tiles.begin(); it != tiles.end(); ++it) {
      ResourceDeclaration::TileData &tileData = **it;

      // If the tile can be downloaded from the resource, it's because it has
      // actually
      // been calculated by another render process - either a concurrent one, or
      // any
      // which has written this resource part on disk storage.

      // Since reference counts built in the simCompute assume that tiles
      // downloaded
      // from the resource have been calculated in THIS very render process,
      // therefore having tileData.m_calculated == true, in this case
      // heir refCounts must be updated since no computing will happen on them
      // due to the predicted node builds of this resource.
      TRect tileRectI(tileData.m_rect.x0, tileData.m_rect.y0,
                      tileData.m_rect.x1 - 1, tileData.m_rect.y1 - 1);

      if (m_data.second->canDownloadAll(tileRectI)) {
        if (!tileData.m_calculated && tileData.m_refCount > 0) {
          /*#ifdef WRITESTACK
QString renderStr(DIAGNOSTICS_GLOSTRGET("compRenderStr"));
QString frameStr(DIAGNOSTICS_THRSTRGET("frameStr"));
DIAGNOSTICS_NUMBEREDSTRSET(prefixComp + renderStr + frameStr,
DIAGNOSTICS_THRSTRGET("stackVar"), "$ > " + resName,
::traduce(tileData.m_rect), 4);
#endif*/

          // Deplete children refsCount - rely on the simulated procedure
          simCompute(tileData.m_rect);
        }
      } else {
#ifdef WRITESUBRECTS
        DIAGNOSTICS_NUMBEREDSTRSET(
            prefixComp + renderStr + frameStr,
            DIAGNOSTICS_THRSTRGET("stackVar"),
            DIAGNOSTICS_STACKGET("parentResource") + "   " + resName,
            ::traduce(tileData.m_rect), 4);
        DIAGNOSTICS_PUSHAUTO("parentResource",
                             QString::number(DIAGNOSTICS_GLOGET(
                                 DIAGNOSTICS_THRSTRGET("stackVar"))),
                             bla2);
#endif

        // Compute the tile to be calculated
        compute(tileData.m_rect);
        if (tileData.m_refCount > 0) tileData.m_calculated = true;

        // Upload the tile into the resource - do so even if the tile
        // was unpredicted. In this case, we rely on the resource refCount to
        // provide the deallocations... Should be so?
        upload(m_data.second);
      }
    }
  }

  // Finally, download the built resource in the required tile
  bool ret = download(m_data.second);
  assert(ret);

#ifdef WRITESTACK
  if (!ret)
    DIAGNOSTICS_STRSET(
        prefixErr + "Download falliti | " +
            DIAGNOSTICS_GLOSTRGET("compRenderStr") +
            DIAGNOSTICS_THRSTRGET("frameStr") +
            QString::number(
                DIAGNOSTICS_GLOGET(DIAGNOSTICS_THRSTRGET("stackVar"))),
        "CROP #" + QString::number(DIAGNOSTICS_GLOGET("crStack")));
#endif

  // Deplete a usage for all tiles intersecting the downloaded one. Fully
  // depleted
  // tiles become unpredicted from now on.
  std::vector<ResourceDeclaration::TileData> &resTiles = m_data.first->m_tiles;
  std::vector<ResourceDeclaration::TileData>::iterator it;
  for (it = resTiles.begin(); it != resTiles.end(); ++it) {
    ResourceDeclaration::TileData &tileData = *it;

    if (!isEmpty(tileData.m_rect * tileRect)) {
      if (tileData.m_refCount <= 0) {
#ifdef WRITEGENERAL
        DIAGNOSTICS_ADD(prefixErr + "Computing | Over-used subtiles", 1);
#endif

        continue;
      }

      if (--tileData.m_refCount == 0) {
        tileData.m_calculated = false;
        --m_data.first->m_tilesCount;
      }

      TRect tileRectI(tileData.m_rect.x0, tileData.m_rect.y0,
                      tileData.m_rect.x1 - 1, tileData.m_rect.y1 - 1);

      m_data.second->release2(tileRectI);
    }
  }

  // If the declaration has been completely used up, destroy it.
  // NOTE: Keeping the declarations is useful for diagnostic purposes. The
  // following code
  // could be reactivated - but declarations tend to be lightweight now...
  // NOTE: If re-enabled, competing mutexes must be set where the resourcesData
  // map is used...
  /*if(m_data.first->m_tilesCount <= 0)
{
QMutexLocker locker(&m_cacheManager->m_imp->m_mutex);
m_cacheManager->m_imp->m_resourcesData.erase(m_data.second->getName());
}*/
}
