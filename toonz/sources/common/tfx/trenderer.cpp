

#include "trenderer.h"
#include "trendererP.h"

// TnzCore includes
#include "tsystem.h"
#include "tthreadmessage.h"
#include "tthread.h"
#include "tconvert.h"
#include "tstream.h"
#include "trasterimage.h"
#include "trop.h"
#include "timagecache.h"
#include "tstopwatch.h"

// TnzBase includes
#include "trenderresourcemanager.h"
#include "tpredictivecachemanager.h"

// Qt includes
#include <QEventLoop>
#include <QCoreApplication>
#include <QReadWriteLock>
#include <QReadLocker>
#include <QWriteLocker>
#include <QThreadStorage>

// tcg includes
#include "tcg/tcg_deleter_types.h"

// Debug
//#define DIAGNOSTICS
//#include "diagnostics.h"

#include <queue>
#include <functional>

#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QOpenGLContext>
#include <QThread>
#include <QGuiApplication>

using namespace TThread;

std::vector<const TFx *> calculateSortedFxs(TRasterFxP rootFx);

//================================================================================
//    Preliminaries - Anonymous namespace
//================================================================================

namespace {
// TRenderer-thread association storage. It provides TRenderers the per-thread
// singleton status from inside a rendering process.
QThreadStorage<TRendererImp **> rendererStorage;

// Same for render process ids.
QThreadStorage<unsigned long *> renderIdsStorage;

//-------------------------------------------------------------------------------

// Interlacing functions for field-based rendering
inline void interlace(TRasterP f0, const TRasterP &f1, int field) {
  if (f0->getPixelSize() != f1->getPixelSize())
    throw TException("interlace: invalid raster combination");

  assert(f0->getBounds() == f1->getBounds());

  int outWrapBytes  = f0->getWrap() * f0->getPixelSize();
  int inWrapBytes   = f1->getWrap() * f1->getPixelSize();
  int outWrapBytes2 = outWrapBytes * 2;
  int inWrapBytes2  = inWrapBytes * 2;
  int rowSize       = f0->getLx() * f0->getPixelSize();

  f1->lock();
  f0->lock();
  UCHAR *rowIn  = f1->getRawData();
  UCHAR *rowOut = f0->getRawData();

  if (field) rowIn += inWrapBytes;

  int count = f0->getLy() / 2;
  while (--count) {
    memcpy(rowOut, rowIn, rowSize);
    rowIn += inWrapBytes2;
    rowOut += outWrapBytes2;
  }

  f1->unlock();
  f0->unlock();
}
}  // anonymous namespace

//================================================================================
//    Preliminaries - output rasters management
//================================================================================

//===================
//    RasterItem
//-------------------

//! The RasterItem class represents a reusable output raster for rendering
//! purposes.
//! RasterItems are used as TRenderer's output rasters in order to avoid the
//! overhead
//! of allocating one raster for each rendered frame.
//! Each frame-rendering task will lock a RasterItem as preallocated output
//! before starting the
//! render, therefore changing the \b RasterItem::m_busy attribute accordingly.
//! As each frame is rendered on a separate thread, the number of RasterItems
//! that
//! TRenderer will allocate depends on the number of rendering threads specified
//! to it.

//! \sa RasterPool, TRenderer classes

class RasterItem {
  std::string m_rasterId;

public:
  int m_bpp;
  bool m_busy;

  //---------------------------------------------------------

  RasterItem(const TDimension &size, int bpp, bool busyFlag)
      : m_rasterId(""), m_bpp(bpp), m_busy(busyFlag) {
    TRasterP raster;
    if (bpp == 32)
      raster = TRaster32P(size);
    else if (bpp == 64)
      raster = TRaster64P(size);
    else
      assert(false);

    m_rasterId = TImageCache::instance()->getUniqueId();
    TImageCache::instance()->add(m_rasterId, TRasterImageP(raster));
  }

  //---------------------------------------------------------

  ~RasterItem() { TImageCache::instance()->remove(m_rasterId); }

  //---------------------------------------------------------

  TRasterP getRaster() const {
    TRasterImageP rimg =
        (TRasterImageP)TImageCache::instance()->get(m_rasterId, true);
    return rimg ? rimg->getRaster() : TRasterP();
  }

private:
  // not implemented
  RasterItem();
  RasterItem(const TRaster &RasterItem);
};

//================================================================================

//===================
//    RasterPool
//-------------------

//! Stores a list of RasterItems under TRenderer's requests.
class RasterPool {
  TDimension m_size;
  int m_bpp;

  typedef std::list<RasterItem *> RasterRepository;
  RasterRepository m_rasterRepository;

  TThread::Mutex m_repositoryLock;

public:
  RasterPool() : m_size(-1, -1) {}
  ~RasterPool();

  void setRasterSpecs(const TDimension &size, int bpp);
  void getRasterSpecs(TDimension &size, int &bpp) {
    size = m_size;
    bpp  = m_bpp;
  }

  TRasterP getRaster();
  TRasterP getRaster(const TDimension &size, int bpp);

  void releaseRaster(const TRasterP &r);

  void clear();
};

//---------------------------------------------------------

void RasterPool::clear() {
  QMutexLocker sl(&m_repositoryLock);
  clearPointerContainer(m_rasterRepository);
}

//---------------------------------------------------------

void RasterPool::setRasterSpecs(const TDimension &size, int bpp) {
  if (size != m_size || bpp != m_bpp) {
    m_size = size;
    m_bpp  = bpp;
    clear();
  }
}

//---------------------------------------------------------

TRasterP RasterPool::getRaster(const TDimension &size, int bpp) {
  assert(size == m_size && bpp == m_bpp);
  return getRaster();
}

//---------------------------------------------------------

//! Returns for the first not-busy raster
TRasterP RasterPool::getRaster() {
  QMutexLocker sl(&m_repositoryLock);

  RasterRepository::iterator it = m_rasterRepository.begin();
  while (it != m_rasterRepository.end()) {
    RasterItem *rasItem = *it;
    if (rasItem->m_busy == false) {
      TRasterP raster = rasItem->getRaster();

      if (!raster) {
        delete rasItem;
        m_rasterRepository.erase(it++);
        continue;
      }

      rasItem->m_busy = true;
      raster->clear();
      return raster;
    }

    ++it;
  }

  RasterItem *rasItem = new RasterItem(m_size, m_bpp, true);
  m_rasterRepository.push_back(rasItem);

  return rasItem->getRaster();
}

//---------------------------------------------------------

//! Cerca il raster \b r in m_rasterRepository; se lo trova setta a \b false il
//! campo \b m_busy.
void RasterPool::releaseRaster(const TRasterP &r) {
  if (!r) return;

  QMutexLocker sl(&m_repositoryLock);
  for (RasterRepository::iterator it = m_rasterRepository.begin();
       it != m_rasterRepository.end(); ++it) {
    RasterItem *rasItem = *it;
    if (rasItem->getRaster()->getRawData() == r->getRawData()) {
      assert(rasItem->m_busy);
      rasItem->m_busy = false;
      return;
    }
  }
}

//---------------------------------------------------------

RasterPool::~RasterPool() {
  /*if (m_rasterRepository.size())
TSystem::outputDebug("~RasterPool: itemCount = " + toString
((int)m_rasterRepository.size())+" (should be 0)\n");*/

  // Release all raster items
  clear();
}

//================================================================================
//    Internal rendering classes declaration
//================================================================================

//=====================
//    TRendererImp
//---------------------

class TRendererImp final : public TSmartObject {
public:
  struct RenderInstanceInfos {
    int m_canceled;
    int m_activeTasks;
    int m_status;

    RenderInstanceInfos()
        : m_canceled(false), m_activeTasks(0), m_status(TRenderer::IDLE) {}
  };

public:
  typedef std::vector<TRenderPort *> PortContainer;
  typedef PortContainer::iterator PortContainerIterator;

  QReadWriteLock m_portsLock;
  PortContainer m_ports;

  QMutex m_renderInstancesMutex;
  std::map<unsigned long, RenderInstanceInfos> m_activeInstances;

  //! The renderer Id is a number uniquely associated with a TRenderer instance.
  static unsigned long m_rendererIdCounter;

  //! The render Id is a number uniquely associated with a render process
  //! started
  //! with the startRendering() method.
  static unsigned long m_renderIdCounter;

  unsigned long m_rendererId;

  Executor m_executor;

  bool m_precomputingEnabled;
  RasterPool m_rasterPool;

  std::vector<TRenderResourceManager *> m_managers;

  TAtomicVar m_undoneTasks;
  // std::vector<QEventLoop*> m_waitingLoops;
  std::vector<bool *> m_waitingLoops;

  TRasterFxP rootFx;

  //-------------------------------------------------------------

  TRendererImp(int nThreads);
  ~TRendererImp();

  void startRendering(unsigned long renderId,
                      const std::vector<TRenderer::RenderData> &renderDatas);

  void notifyRasterStarted(const TRenderPort::RenderData &rd);
  void notifyRasterCompleted(const TRenderPort::RenderData &rd);
  void notifyRasterFailure(const TRenderPort::RenderData &rd, TException &e);
  void notifyRenderFinished(bool isCanceled = false);

  void addPort(TRenderPort *port);
  void removePort(TRenderPort *port);

  void abortRendering(unsigned long renderId);
  void stopRendering(bool waitForCompleteStop);

  bool hasToDie(unsigned long renderId);
  int getRenderStatus(unsigned long renderId);

  void enablePrecomputing(bool on) { m_precomputingEnabled = on; }
  bool isPrecomputingEnabled() const { return m_precomputingEnabled; }

  void setThreadsCount(int nThreads) { m_executor.setMaxActiveTasks(nThreads); }

  inline void declareRenderStart(unsigned long renderId);
  inline void declareRenderEnd(unsigned long renderId);
  inline void declareFrameStart(double frame);
  inline void declareFrameEnd(double frame);
  inline void declareStatusStart(int renderStatus);
  inline void declareStatusEnd(int renderStatus);

  void quitWaitingLoops();
};

#ifdef _WIN32
template class DVAPI TSmartPointerT<TRendererImp>;
#endif

typedef TSmartPointerT<TRendererImp> TRendererImpP;

unsigned long TRendererImp::m_rendererIdCounter = 0;
unsigned long TRendererImp::m_renderIdCounter   = 0;

//================================================================================

//===================
//    RenderTask
//-------------------

class RenderTask final : public TThread::Runnable {
  std::vector<double> m_frames;

  unsigned long m_taskId;
  unsigned long m_renderId;

  TRendererImpP m_rendererImp;

  TFxPair m_fx;
  TPointD m_framePos;
  TDimension m_frameSize;
  TRenderSettings m_info;

  bool m_fieldRender, m_stereoscopic;

  Mutex m_rasterGuard;
  TTile m_tileA;  // in normal and field rendering, Rendered at given frame; in
                  // stereoscopic, rendered left frame
  TTile m_tileB;  // in  field rendering, rendered at frame + 0.5; in
                  // stereoscopic, rendered right frame

public:
  RenderTask(unsigned long renderId, unsigned long taskId, double frame,
             const TRenderSettings &ri, const TFxPair &fx,
             const TPointD &framePos, const TDimension &frameSize,
             const TRendererImpP &rendererImp);

  ~RenderTask() {}

  void addFrame(double frame) { m_frames.push_back(frame); }

  void buildTile(TTile &tile);
  void releaseTiles();

  void onFrameStarted();
  void onFrameCompleted();
  void onFrameFailed(TException &e);

  void preRun();
  void run() override;

  int taskLoad() override { return 100; }

  void onFinished(TThread::RunnableP) override;
};

//================================================================================
//    Implementations
//================================================================================

//==================
//    TRenderer
//------------------

TRenderer::TRenderer(int nThread) {
  m_imp = new TRendererImp(nThread);  // Already adds a ref
}

//---------------------------------------------------------

TRenderer::TRenderer(TRendererImp *imp) : m_imp(imp) {
  if (m_imp) m_imp->addRef();
}

//---------------------------------------------------------

TRenderer::TRenderer(const TRenderer &r) : m_imp(r.m_imp) {
  if (m_imp) m_imp->addRef();
}

//---------------------------------------------------------

void TRenderer::operator=(const TRenderer &r) {
  m_imp = r.m_imp;
  if (m_imp) m_imp->addRef();
}

//---------------------------------------------------------

TRenderer::~TRenderer() {
  if (m_imp) m_imp->release();
}

//---------------------------------------------------------

//! The static method is intended for use inside a rendering thread only. It
//! returns
//! a copy of the eventual TRenderer interface installed on the thread -
//! or an empty TRenderer if none happened to be. It can be set using the
//! install() and uninstall() methods.
TRenderer TRenderer::instance() {
  TRendererImp **lData = rendererStorage.localData();
  if (lData) return TRenderer(*lData);
  return 0;
}

//---------------------------------------------------------

//! Returns the renderer id.
unsigned long TRenderer::rendererId() { return m_imp->m_rendererId; }

//---------------------------------------------------------

//! Returns the rendering process id currently running on the invoking
//! thread.
unsigned long TRenderer::renderId() {
  unsigned long *lData = renderIdsStorage.localData();
  return lData ? *lData : -1;
}

//---------------------------------------------------------

//! Builds and returns an id for starting a new rendering process.
unsigned long TRenderer::buildRenderId() {
  return TRendererImp::m_renderIdCounter++;
}

//---------------------------------------------------------

//! Returns the renderId that will be used by the next startRendering() call.
//! This method can be used to retrieve the renderId of a rendering instance
//! before it is actually started - provided that no other render instance
//! is launched inbetween.
unsigned long TRenderer::nextRenderId() {
  return TRendererImp::m_renderIdCounter;
}

//---------------------------------------------------------

inline void TRendererImp::declareRenderStart(unsigned long renderId) {
  // Inform the resource managers
  for (unsigned int i = 0; i < m_managers.size(); ++i)
    m_managers[i]->onRenderInstanceStart(renderId);
}

//! Declares that the render process of passed renderId is about to start.
void TRenderer::declareRenderStart(unsigned long renderId) {
  m_imp->declareRenderStart(renderId);
}

//---------------------------------------------------------

inline void TRendererImp::declareRenderEnd(unsigned long renderId) {
  // Inform the resource managers
  for (int i = m_managers.size() - 1; i >= 0; --i)
    m_managers[i]->onRenderInstanceEnd(renderId);
}

//! Declares that the render process of passed renderId has ended.
void TRenderer::declareRenderEnd(unsigned long renderId) {
  m_imp->declareRenderEnd(renderId);
}

//---------------------------------------------------------

inline void TRendererImp::declareFrameStart(double frame) {
  // Inform the resource managers
  for (unsigned int i = 0; i < m_managers.size(); ++i)
    m_managers[i]->onRenderFrameStart(frame);
}

//! Declares that render of passed frame is about to start.
void TRenderer::declareFrameStart(double frame) {
  m_imp->declareFrameStart(frame);
}

//---------------------------------------------------------

inline void TRendererImp::declareFrameEnd(double frame) {
  // Inform the resource managers
  for (int i = m_managers.size() - 1; i >= 0; --i)
    m_managers[i]->onRenderFrameEnd(frame);
}

//! Declares that render of passed has ended.
void TRenderer::declareFrameEnd(double frame) { m_imp->declareFrameEnd(frame); }

//---------------------------------------------------------

inline void TRendererImp::declareStatusStart(int renderStatus) {
  // Inform the resource managers
  for (unsigned int i = 0; i < m_managers.size(); ++i)
    m_managers[i]->onRenderStatusStart(renderStatus);
}

//---------------------------------------------------------

inline void TRendererImp::declareStatusEnd(int renderStatus) {
  // Inform the resource managers
  for (int i = m_managers.size() - 1; i >= 0; --i)
    m_managers[i]->onRenderStatusEnd(renderStatus);
}

//---------------------------------------------------------

//! Installs the specified render process on the invoking thread.
void TRenderer::install(unsigned long renderId) {
  m_imp->addRef();
  rendererStorage.setLocalData(new (TRendererImp *)(m_imp));
  renderIdsStorage.setLocalData(new unsigned long(renderId));
}

//---------------------------------------------------------

//! Uninstalls any rendering process active on the invoking thread.
void TRenderer::uninstall() {
  rendererStorage.setLocalData(0);
  renderIdsStorage.setLocalData(0);
  m_imp->release();
}

//---------------------------------------------------------

TRenderResourceManager *TRenderer::getManager(unsigned int id) const {
  return m_imp->m_managers[id];
}

//---------------------------------------------------------

void TRenderer::enablePrecomputing(bool on) { m_imp->enablePrecomputing(on); }

//---------------------------------------------------------

bool TRenderer::isPrecomputingEnabled() const {
  return m_imp->isPrecomputingEnabled();
}

//---------------------------------------------------------

void TRenderer::setThreadsCount(int nThreads) {
  m_imp->setThreadsCount(nThreads);
}

//---------------------------------------------------------

void TRenderer::addPort(TRenderPort *port) { m_imp->addPort(port); }

//---------------------------------------------------------

void TRenderer::removePort(TRenderPort *port) { m_imp->removePort(port); }

//---------------------------------------------------------

unsigned long TRenderer::startRendering(double f, const TRenderSettings &info,
                                        const TFxPair &actualRoot) {
  assert(f >= 0);

  std::vector<RenderData> *rds = new std::vector<RenderData>;
  rds->push_back(RenderData(f, info, actualRoot));
  return startRendering(rds);
}

//---------------------------------------------------------

//! Queues a rendering event in the main event loop.
//! NOTE: The passed pointer is owned by the TRenderer after it is submitted for
//! rendering -
//! do not delete it later.
unsigned long TRenderer::startRendering(
    const std::vector<RenderData> *renderDatas) {
  if (renderDatas->empty()) {
    delete renderDatas;
    return -1;
  }

  // Build a new render Id
  unsigned long renderId = m_imp->m_renderIdCounter++;

  TRendererStartInvoker::StartInvokerRenderData srd;
  srd.m_renderId         = renderId;
  srd.m_renderDataVector = renderDatas;
  TRendererStartInvoker::instance()->emitStartRender(m_imp, srd);

  return renderId;
}

//---------------------------------------------------------

void TRenderer::abortRendering(unsigned long renderId) {
  m_imp->abortRendering(renderId);
}

//---------------------------------------------------------

void TRenderer::stopRendering(bool waitForCompleteStop) {
  m_imp->stopRendering(waitForCompleteStop);
}

//---------------------------------------------------------

bool TRenderer::isAborted(unsigned long renderId) const {
  return m_imp->hasToDie(renderId);
}

//---------------------------------------------------------

int TRenderer::getRenderStatus(unsigned long renderId) const {
  return m_imp->getRenderStatus(renderId);
}

//================================================================================

//=====================
//    TRendererImp
//---------------------

TRendererImp::TRendererImp(int nThreads)
    : m_executor()
    , m_undoneTasks()
    , m_rendererId(m_rendererIdCounter++)
    , m_precomputingEnabled(true) {
  m_executor.setMaxActiveTasks(nThreads);

  std::vector<TRenderResourceManagerGenerator *> &generators =
      TRenderResourceManagerGenerator::generators(false);

  // May be adopted by other TRenderers from now on.
  addRef();

  rendererStorage.setLocalData(new (TRendererImp *)(this));

  unsigned int i;
  for (i = 0; i < generators.size(); ++i) {
    TRenderResourceManager *manager = (*generators[i])();
    if (manager) m_managers.push_back(manager);
  }

  rendererStorage.setLocalData(0);
}

//---------------------------------------------------------

TRendererImp::~TRendererImp() {
  rendererStorage.setLocalData(new (TRendererImp *)(this));

  int i;
  for (i = m_managers.size() - 1; i >= 0; --i)
    if (m_managers[i]->renderHasOwnership()) delete m_managers[i];

  rendererStorage.setLocalData(0);
}

//---------------------------------------------------------

void TRendererImp::addPort(TRenderPort *port) {
  QWriteLocker sl(&m_portsLock);

  PortContainerIterator it = std::find(m_ports.begin(), m_ports.end(), port);
  if (it == m_ports.end()) m_ports.push_back(port);
}

//---------------------------------------------------------

void TRendererImp::removePort(TRenderPort *port) {
  QWriteLocker sl(&m_portsLock);

  PortContainerIterator it = std::find(m_ports.begin(), m_ports.end(), port);
  if (it != m_ports.end()) m_ports.erase(it);
}

//---------------------------------------------------------

bool TRendererImp::hasToDie(unsigned long renderId) {
  QMutexLocker sl(&m_renderInstancesMutex);

  std::map<unsigned long, RenderInstanceInfos>::iterator it =
      m_activeInstances.find(renderId);
  assert(it != m_activeInstances.end());
  return it == m_activeInstances.end() ? true : it->second.m_canceled;
}

//---------------------------------------------------------

int TRendererImp::getRenderStatus(unsigned long renderId) {
  QMutexLocker sl(&m_renderInstancesMutex);

  std::map<unsigned long, RenderInstanceInfos>::iterator it =
      m_activeInstances.find(renderId);
  assert(it != m_activeInstances.end());
  return it == m_activeInstances.end() ? true : it->second.m_status;
}

//---------------------------------------------------------

void TRendererImp::abortRendering(unsigned long renderId) {
  QMutexLocker sl(&m_renderInstancesMutex);

  std::map<unsigned long, RenderInstanceInfos>::iterator it =
      m_activeInstances.find(renderId);
  if (it != m_activeInstances.end()) it->second.m_canceled = true;
}

//---------------------------------------------------------

void TRendererImp::stopRendering(bool waitForCompleteStop) {
  QMutexLocker sl(&m_renderInstancesMutex);

  {
    // Tasks already stop rendering on their own when they don't find their
    // render ids here.
    std::map<unsigned long, RenderInstanceInfos>::iterator it;
    for (it = m_activeInstances.begin(); it != m_activeInstances.end(); ++it)
      it->second.m_canceled = true;
  }

  if (waitForCompleteStop && m_undoneTasks > 0) {
    // Sometimes, QEventLoop suddenly stops processing slots (especially those
    // notifications
    // from active rendering instances) - therefore resulting in a block of the
    // application.
    // I've not figured out why (#QTBUG-11649?) - but substituting with a plain
    // while
    // seems to do the trick...

    /*QEventLoop eventLoop;
m_waitingLoops.push_back(&eventLoop);
eventLoop.exec();*/

    bool loopQuit = false;
    m_waitingLoops.push_back(&loopQuit);

    sl.unlock();

    while (!loopQuit)
      QCoreApplication::processEvents(QEventLoop::AllEvents |
                                      QEventLoop::WaitForMoreEvents);
  }
}

//---------------------------------------------------------

void TRendererImp::quitWaitingLoops() {
  // Make the stopRendering waiting loops quit
  while (!m_waitingLoops.empty()) {
    // rendererImp->m_waitingLoops.back()->quit();
    *m_waitingLoops.back() = true;
    m_waitingLoops.pop_back();
  }
}

//---------------------------------------------------------

void TRendererImp::notifyRasterStarted(const TRenderPort::RenderData &rd) {
  // Since notifications may trigger port removals, we always work on a copy of
  // the ports
  // vector.
  TRendererImp::PortContainer portsCopy;
  {
    QReadLocker sl(&m_portsLock);
    portsCopy = m_ports;
  }

  for (PortContainerIterator it = portsCopy.begin(); it != portsCopy.end();
       ++it)
    (*it)->onRenderRasterStarted(rd);
}

//---------------------------------------------------------

void TRendererImp::notifyRasterCompleted(const TRenderPort::RenderData &rd) {
  TRendererImp::PortContainer portsCopy;
  {
    QReadLocker sl(&m_portsLock);
    portsCopy = m_ports;
  }

  assert(rd.m_rasA);

  for (PortContainerIterator it = portsCopy.begin(); it != portsCopy.end();
       ++it)
    (*it)->onRenderRasterCompleted(rd);
}

//---------------------------------------------------------

void TRendererImp::notifyRasterFailure(const TRenderPort::RenderData &rd,
                                       TException &e) {
  TRendererImp::PortContainer portsCopy;
  {
    QReadLocker sl(&m_portsLock);
    portsCopy = m_ports;
  }

  for (PortContainerIterator it = portsCopy.begin(); it != portsCopy.end();
       ++it)
    (*it)->onRenderFailure(rd, e);
}

//---------------------------------------------------------

void TRendererImp::notifyRenderFinished(bool isCanceled) {
  TRendererImp::PortContainer portsCopy;
  {
    QReadLocker sl(&m_portsLock);
    portsCopy = m_ports;
  }

  auto sortedFxs = calculateSortedFxs(rootFx);
  for (auto fx : sortedFxs) {
    if (fx) const_cast<TFx *>(fx)->callEndRenderHandler();
  }

  for (PortContainerIterator it = portsCopy.begin(); it != portsCopy.end();
       ++it)
    (*it)->onRenderFinished();
}

//================================================================================

//====================
//    TRenderPort
//--------------------

TRenderPort::TRenderPort() {}

//---------------------------------------------------------

TRenderPort::~TRenderPort() {}

//---------------------------------------------------------

//! Setta \b m_renderArea a \b area e pulisce l'istanza corrente di \b
//! RasterPool.
void TRenderPort::setRenderArea(const TRectD &area) { m_renderArea = area; }

//---------------------------------------------------------

//! Ritorna \b m_renderArea.
TRectD &TRenderPort::getRenderArea() { return m_renderArea; }

//================================================================================

//===================
//    RenderTask
//-------------------

RenderTask::RenderTask(unsigned long renderId, unsigned long taskId,
                       double frame, const TRenderSettings &ri,
                       const TFxPair &fx, const TPointD &framePos,
                       const TDimension &frameSize,
                       const TRendererImpP &rendererImp)
    : m_renderId(renderId)
    , m_taskId(taskId)
    , m_info(ri)
    , m_fx(fx)
    , m_frameSize(frameSize)
    , m_framePos(framePos)
    , m_rendererImp(rendererImp)
    , m_fieldRender(ri.m_fieldPrevalence != TRenderSettings::NoField)
    , m_stereoscopic(ri.m_stereoscopic) {
  m_frames.push_back(frame);

  // Connect the onFinished slot
  connect(this, SIGNAL(finished(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));
  connect(this, SIGNAL(exception(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));

  // The shrink info is currently reversed to the settings'affine. Shrink info
  // in the TRenderSettings
  // is no longer supported.
  m_info.m_shrinkX = m_info.m_shrinkY = 1;
}

//---------------------------------------------------------

void RenderTask::preRun() {
  TRectD geom(m_framePos, TDimensionD(m_frameSize.lx, m_frameSize.ly));

  if (m_fx.m_frameA) m_fx.m_frameA->dryCompute(geom, m_frames[0], m_info);

  if (m_fx.m_frameB)
    m_fx.m_frameB->dryCompute(
        geom, m_fieldRender ? m_frames[0] + 0.5 : m_frames[0], m_info);
}

//---------------------------------------------------------

void RenderTask::run() {
  // Retrieve the task's frame
  assert(!m_frames.empty());
  double t = m_frames[0];

  if (m_rendererImp->hasToDie(m_renderId)) {
    TException e("Render task aborted");
    onFrameFailed(e);
    return;
  }

  // Install the renderer in current thread
  rendererStorage.setLocalData(
      new (TRendererImp *)(m_rendererImp.getPointer()));
  renderIdsStorage.setLocalData(new unsigned long(m_renderId));

  // Inform the managers of frame start
  m_rendererImp->declareFrameStart(t);

  auto sortedFxs = calculateSortedFxs(m_fx.m_frameA);
  for (auto fx : sortedFxs) {
    if (fx) const_cast<TFx *>(fx)->callStartRenderFrameHandler(&m_info, t);
  }

  try {
    onFrameStarted();

    TStopWatch::global(8).start();

    if (!m_fieldRender && !m_stereoscopic) {
      // Common case - just build the first tile
      buildTile(m_tileA);
      /*-- 通常はここがFxのレンダリング処理 --*/
      m_fx.m_frameA->compute(m_tileA, t, m_info);
    } else {
      assert(!(m_stereoscopic && m_fieldRender));
      // Field rendering  or stereoscopic case
      if (m_stereoscopic) {
        buildTile(m_tileA);
        m_fx.m_frameA->compute(m_tileA, t, m_info);

        buildTile(m_tileB);
        m_fx.m_frameB->compute(m_tileB, t, m_info);
      }
      // if fieldPrevalence, Decide the rendering frames depending on field
      // prevalence
      else if (m_info.m_fieldPrevalence == TRenderSettings::EvenField) {
        buildTile(m_tileA);
        m_fx.m_frameA->compute(m_tileA, t, m_info);

        buildTile(m_tileB);
        m_fx.m_frameB->compute(m_tileB, t + 0.5, m_info);
      } else {
        buildTile(m_tileB);
        m_fx.m_frameA->compute(m_tileB, t, m_info);

        buildTile(m_tileA);
        m_fx.m_frameB->compute(m_tileA, t + 0.5, m_info);
      }
    }

    TStopWatch::global(8).stop();

    onFrameCompleted();
  } catch (TException &e) {
    onFrameFailed(e);
  } catch (...) {
    TException ex("Unknown render exception");
    onFrameFailed(ex);
  }

  // Inform the managers of frame end
  m_rendererImp->declareFrameEnd(t);

  // Uninstall the renderer from current thread
  rendererStorage.setLocalData(0);
  renderIdsStorage.setLocalData(0);

  for (auto fx : sortedFxs) {
    if (fx) const_cast<TFx *>(fx)->callEndRenderFrameHandler(&m_info, t);
  }
}

//---------------------------------------------------------

void RenderTask::buildTile(TTile &tile) {
  tile.m_pos = m_framePos;
  tile.setRaster(
      m_rendererImp->m_rasterPool.getRaster(m_frameSize, m_info.m_bpp));
}

//---------------------------------------------------------

void RenderTask::releaseTiles() {
  m_rendererImp->m_rasterPool.releaseRaster(m_tileA.getRaster());
  m_tileA.setRaster(TRasterP());
  if (m_fieldRender || m_stereoscopic) {
    m_rendererImp->m_rasterPool.releaseRaster(m_tileB.getRaster());
    m_tileB.setRaster(TRasterP());
  }
}

//---------------------------------------------------------

void RenderTask::onFrameStarted() {
  TRenderPort::RenderData rd(m_frames, m_info, 0, 0, m_renderId, m_taskId);
  m_rendererImp->notifyRasterStarted(rd);
}

//---------------------------------------------------------

void RenderTask::onFrameCompleted() {
  TRasterP rasA(m_tileA.getRaster());
  TRasterP rasB(m_tileB.getRaster());

  if (m_fieldRender) {
    assert(rasB);

    double t = m_frames[0];

    int f = (m_info.m_fieldPrevalence == TRenderSettings::EvenField) ? 0 : 1;
    interlace(rasA, rasB, f);
    rasB = TRasterP();
  }

  TRenderPort::RenderData rd(m_frames, m_info, rasA, rasB, m_renderId,
                             m_taskId);
  m_rendererImp->notifyRasterCompleted(rd);
}

//---------------------------------------------------------

void RenderTask::onFrameFailed(TException &e) {
  // TRasterP evenRas(m_evenTile.getRaster());

  TRenderPort::RenderData rd(m_frames, m_info, m_tileA.getRaster(),
                             m_tileB.getRaster(), m_renderId, m_taskId);
  m_rendererImp->notifyRasterFailure(rd, e);
}

//---------------------------------------------------------

void RenderTask::onFinished(TThread::RunnableP) {
  TRendererImp *rendererImp = m_rendererImp.getPointer();
  --rendererImp->m_undoneTasks;

  // Tiles release back to the Raster Pool happens in the main thread, after all
  // possible
  // signals emitted in the onFrameCompleted/Failed notifications have been
  // resolved, thus
  // ensuring that no other rendering thread owns the rasters before them.
  releaseTiles();

  // Update the render instance status
  bool instanceExpires = false;
  bool isCanceled      = false;
  {
    QMutexLocker sl(&rendererImp->m_renderInstancesMutex);
    std::map<unsigned long, TRendererImp::RenderInstanceInfos>::iterator it =
        rendererImp->m_activeInstances.find(m_renderId);

    if (it != rendererImp->m_activeInstances.end() &&
        (--it->second.m_activeTasks) <= 0) {
      instanceExpires = true;
      isCanceled      = (m_info.m_isCanceled && *m_info.m_isCanceled);
      rendererImp->m_activeInstances.erase(m_renderId);
      // m_info is freed, don't access further!
    }
  }

  // If the render instance has just expired
  if (instanceExpires) {
    /*-- キャンセルされた場合はm_overallRenderedRegionの更新をしない --*/

    // Inform the render ports
    rendererImp->notifyRenderFinished(isCanceled);

    // NOTE: This slot is currently invoked on the main thread. It could
    // eventually be
    // invoked directly on rendering threads, specifying the
    // Qt::DirectConnection option -
    // but probably there would be no real advantage in doing so...

    // Temporarily install the renderer in current thread
    rendererStorage.setLocalData(new (TRendererImp *)(rendererImp));
    renderIdsStorage.setLocalData(new unsigned long(m_renderId));

    // Inform the resource managers
    rendererImp->declareRenderEnd(m_renderId);

    // Uninstall the temporary
    rendererStorage.setLocalData(0);
    renderIdsStorage.setLocalData(0);

    rendererImp->m_rasterPool
        .clear();  // Isn't this misplaced? Should be in the block
  }                // below...

  // If no rendering task (of this or other render instances) is found...
  if (rendererImp->m_undoneTasks == 0) {
    QMutexLocker sl(&rendererImp->m_renderInstancesMutex);
    rendererImp->quitWaitingLoops();
  }
}

//================================================================================
//    Tough Stuff
//================================================================================

void TRendererStartInvoker::emitStartRender(TRendererImp *renderer,
                                            StartInvokerRenderData rd) {
  renderer->addRef();
  Q_EMIT startRender(renderer, rd);
}

//---------------------------------------------------------

void TRendererStartInvoker::doStartRender(TRendererImp *renderer,
                                          StartInvokerRenderData rd) {
  renderer->startRendering(rd.m_renderId, *rd.m_renderDataVector);
  renderer->release();
  delete rd.m_renderDataVector;
}

std::vector<const TFx *> calculateSortedFxs(TRasterFxP rootFx) {
  std::map<const TFx *, std::set<const TFx *>> E; /* 辺の情報 */
  std::set<const TFx *> Sources; /* 入次数0のノード群 */

  std::queue<const TFx *> Q;
  Q.push(rootFx.getPointer());

  E[rootFx.getPointer()] = std::set<const TFx *>();

  while (!Q.empty()) {
    const TFx *vptr = Q.front();
    Q.pop();
    if (!vptr) {
      continue;
    }

    /* 繋がっている入力ポートの先の Fx を訪問する
入力ポートが無ければ終了 */
    int portCount = vptr->getInputPortCount();
    if (portCount < 1) {
      Sources.insert(vptr);
      continue;
    }
    for (int i = 0; i < portCount; i++) {
      TFxPort *port = vptr->getInputPort(i);
      if (!port) {
        continue;
      }
      TFxP u          = port->getFx();
      const TFx *uptr = u.getPointer();
      if (E.count(uptr) == 0) {
        E[uptr] = std::set<const TFx *>();
      }
      if (E[uptr].count(vptr) == 0) {
        E[uptr].insert(vptr);
      }
      Q.push(uptr);
    }
  }

  /* トポロジカルソート */
  std::set<const TFx *> visited;
  std::vector<const TFx *> L;
  std::function<void(const TFx *)> visit = [&visit, &visited, &E,
                                            &L](const TFx *fx) {
    if (visited.count(fx)) return;
    visited.insert(fx);
    auto edge = E[fx];
    for (auto i = edge.cbegin(); i != edge.cend(); i++) {
      visit(*i);
    }
    L.insert(L.begin(), fx);
  };
  for (auto i = E.cbegin(); i != E.cend(); i++) {
    visit(i->first);
  }
  return L;
}

//---------------------------------------------------------

void TRendererImp::startRendering(
    unsigned long renderId,
    const std::vector<TRenderer::RenderData> &renderDatas) {
  rootFx = renderDatas.front().m_fxRoot.m_frameA;
  int T  = renderDatas.size();
  for (int z = 0; z < T; z++) {
    auto sortedFxs = calculateSortedFxs(renderDatas[z].m_fxRoot.m_frameA);
    if (z == 0) {
      for (auto fx : sortedFxs) {
        if (fx) const_cast<TFx *>(fx)->callStartRenderHandler();
      }
    }
  }

  struct locals {
    static inline void setStorage(TRendererImp *imp, unsigned long renderId) {
      rendererStorage.setLocalData(new (TRendererImp *)(imp));
      renderIdsStorage.setLocalData(new unsigned long(renderId));
    }

    static inline void clearStorage() {
      rendererStorage.setLocalData(0);
      renderIdsStorage.setLocalData(0);
    }

    static inline void declareStatusStart(TRendererImp *imp,
                                          TRenderer::RenderStatus status,
                                          RenderInstanceInfos *renderInfos) {
      renderInfos->m_status = status;
      imp->declareStatusStart(status);
    }

    //-----------------------------------------------------------------------

    struct InstanceDeclaration {
      TRendererImp *m_imp;
      unsigned long m_renderId;
      bool m_rollback;

      InstanceDeclaration(TRendererImp *imp, unsigned long renderId)
          : m_imp(imp), m_renderId(renderId), m_rollback(true) {}

      ~InstanceDeclaration() {
        if (m_rollback) {
          QMutexLocker locker(&m_imp->m_renderInstancesMutex);

          m_imp->m_activeInstances.erase(m_renderId);
          if (m_imp->m_undoneTasks == 0) m_imp->quitWaitingLoops();
        }
      }

      void commit() { m_rollback = false; }
    };

    struct StorageDeclaration {
      StorageDeclaration(TRendererImp *imp, unsigned long renderId) {
        setStorage(imp, renderId);
      }

      ~StorageDeclaration() { clearStorage(); }
    };

    struct RenderDeclaration {
      TRendererImp *m_imp;
      unsigned long m_renderId;
      bool m_rollback;

      RenderDeclaration(TRendererImp *imp, unsigned long renderId)
          : m_imp(imp), m_renderId(renderId), m_rollback(true) {
        imp->declareRenderStart(renderId);
      }

      ~RenderDeclaration() {
        if (m_rollback) m_imp->declareRenderEnd(m_renderId);
      }

      void commit() { m_rollback = false; }
    };

    struct StatusDeclaration {
      TRendererImp *m_imp;
      TRenderer::RenderStatus m_status;

      StatusDeclaration(TRendererImp *imp, TRenderer::RenderStatus status,
                        RenderInstanceInfos *renderInfos)
          : m_imp(imp), m_status(status) {
        declareStatusStart(imp, status, renderInfos);
      }

      ~StatusDeclaration() { m_imp->declareStatusEnd(m_status); }
    };
  };  // locals

  // DIAGNOSTICS_CLEAR;

  //----------------------------------------------------------------------
  //    Preliminary initializations
  //----------------------------------------------------------------------

  // Calculate the overall render area - sum of all render ports' areas
  TRectD renderArea;
  {
    QReadLocker sl(&m_portsLock);

    for (PortContainerIterator it = m_ports.begin(); it != m_ports.end(); ++it)
      renderArea += (*it)->getRenderArea();
  }

  const TRenderSettings &info(renderDatas[0].m_info);

  // Extract the render geometry
  TPointD pos(renderArea.getP00());
  TDimension frameSize(tceil(renderArea.getLx()), tceil(renderArea.getLy()));

  TRectD camBox(TPointD(pos.x / info.m_shrinkX, pos.y / info.m_shrinkY),
                TDimensionD(frameSize.lx, frameSize.ly));

  // Refresh the raster pool specs
  m_rasterPool.setRasterSpecs(frameSize, info.m_bpp);

  // Set a temporary active instance count - so that hasToDie(renderId) returns
  // false
  RenderInstanceInfos *renderInfos;
  {
    QMutexLocker locker(&m_renderInstancesMutex);
    renderInfos                = &m_activeInstances[renderId];
    renderInfos->m_activeTasks = 1;
  }

  locals::InstanceDeclaration instanceDecl(this, renderId);

  //----------------------------------------------------------------------
  //    Clustering - Render Tasks creation
  //----------------------------------------------------------------------

  std::vector<RenderTask *> tasksVector;

  struct TasksCleaner {
    std::vector<RenderTask *> &m_tasksVector;
    ~TasksCleaner() {
      std::for_each(m_tasksVector.begin(), m_tasksVector.end(),
                    tcg::deleter<RenderTask>());
    }
  } tasksCleaner = {tasksVector};

  unsigned long tasksIdCounter = 0;

  std::map<std::string, RenderTask *> clusters;
  std::vector<TRenderer::RenderData>::const_iterator it;
  std::map<std::string, RenderTask *>::iterator jt;

  for (it = renderDatas.begin(); it != renderDatas.end(); ++it) {
    // Check for user cancels
    if (hasToDie(renderId)) return;

    // Build the frame's description alias
    const TRenderer::RenderData &renderData = *it;

    /*--- カメラサイズ (LevelAutoやノイズで使用する) ---*/
    TRenderSettings rs = renderData.m_info;
    rs.m_cameraBox     = camBox;
    /*--- 途中でPreview計算がキャンセルされたときのフラグ ---*/
    rs.m_isCanceled = &renderInfos->m_canceled;

    TRasterFxP fx = renderData.m_fxRoot.m_frameA;
    assert(fx);

    double frame = renderData.m_frame;

    std::string alias = fx->getAlias(frame, renderData.m_info);
    if (renderData.m_fxRoot.m_frameB)
      alias = alias +
              renderData.m_fxRoot.m_frameB->getAlias(frame, renderData.m_info);

    // If the render contains offscreen render, then prepare the
    // QOffscreenSurface
    // in main (GUI) thread. For now it is used only in the plasticDeformerFx.
    if (alias.find("plasticDeformerFx") != std::string::npos &&
        QThread::currentThread() == qGuiApp->thread()) {
      rs.m_offScreenSurface.reset(new QOffscreenSurface());
      rs.m_offScreenSurface->setFormat(QSurfaceFormat::defaultFormat());
      rs.m_offScreenSurface->create();
    }

    // Search the alias among stored clusters - and store the frame
    jt = clusters.find(alias);

    if (jt == clusters.end()) {
      RenderTask *newTask =
          new RenderTask(renderId, tasksIdCounter++, renderData.m_frame, rs,
                         renderData.m_fxRoot, pos, frameSize, this);

      tasksVector.push_back(newTask);
      clusters.insert(std::make_pair(alias, newTask));
    } else
      jt->second->addFrame(renderData.m_frame);

    // Call processEvents to make the GUI reactive.
    QCoreApplication::instance()->processEvents();
  }

  // Release the clusters - we'll just need the tasks vector from now on
  clusters.clear();

  std::vector<RenderTask *>::iterator kt, kEnd = tasksVector.end();
  {
    // Install TRenderer on current thread before proceeding
    locals::StorageDeclaration storageDecl(this, renderId);

    // Inform the resource managers
    locals::RenderDeclaration renderDecl(this, renderId);

    //----------------------------------------------------------------------
    //    Precomputing
    //----------------------------------------------------------------------

    if (m_precomputingEnabled) {
      // Set current maxTileSize for cache manager precomputation
      const TRenderSettings &rs = renderDatas[0].m_info;
      TPredictiveCacheManager::instance()->setMaxTileSize(rs.m_maxTileSize);
      TPredictiveCacheManager::instance()->setBPP(rs.m_bpp);

      // Perform the first precomputing run - fx usages declaration
      {
        locals::StatusDeclaration firstrunDecl(this, TRenderer::FIRSTRUN,
                                               renderInfos);

        for (kt = tasksVector.begin(); kt != kEnd; ++kt) {
          if (hasToDie(renderId)) return;

          (*kt)->preRun();

          // NOTE: Thread-specific data must be temporarily uninstalled before
          // processing events (which may redefine the thread data).
          locals::clearStorage();
          QCoreApplication::instance()->processEvents();
          locals::setStorage(this, renderId);
        }
      }

      // Pass to the TESTRUN status - this one should faithfully reproduce
      // the actual COMPUTING status
      {
        locals::StatusDeclaration testrunDecl(this, TRenderer::TESTRUN,
                                              renderInfos);

        for (kt = tasksVector.begin(); kt != kEnd; ++kt) {
          if (hasToDie(renderId)) return;

          (*kt)->preRun();

          // NOTE: Thread-specific data must be temporarily uninstalled before
          // processing events (which may redefine the thread data).
          locals::clearStorage();
          QCoreApplication::instance()->processEvents();
          locals::setStorage(this, renderId);
        }
      }
    }

    //----------------------------------------------------------------------
    //    Render
    //----------------------------------------------------------------------

    locals::declareStatusStart(this, TRenderer::COMPUTING, renderInfos);

    // Update the tasks counts
    m_undoneTasks += tasksVector.size();
    {
      QMutexLocker locker(&m_renderInstancesMutex);
      renderInfos->m_activeTasks = tasksVector.size();
    }

    renderDecl.commit();  // Declarations are taken over by render tasks
  }

  instanceDecl.commit();  // Same here

  // Launch the render
  for (kt = tasksVector.begin(); kt != tasksVector.end(); ++kt)
    m_executor.addTask(*kt);

  tasksVector.clear();  // Prevent tasks destruction by TasksCleaner
}

void TRenderer::initialize() { TRendererStartInvoker::instance(); }
