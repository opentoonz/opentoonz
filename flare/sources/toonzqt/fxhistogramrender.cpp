

#include "toonzqt/fxhistogramrender.h"
#include "toonzqt/histogram.h"
#include "tsystem.h"
#include "timagecache.h"
#include "trasterfx.h"
#include "trasterimage.h"
#include "toutputproperties.h"
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/scenefx.h"
#include "toonz/tcamera.h"
#include "toonz/txshlevel.h"

#include <QMetaType>

//*******************************************************************************************************
// FxHistogramRenderPort
//*******************************************************************************************************

FxHistogramRenderPort::FxHistogramRenderPort() {}

//------------------------------------------------------------------------------------------------------

FxHistogramRenderPort::~FxHistogramRenderPort() {}

//------------------------------------------------------------------------------------------------------

void FxHistogramRenderPort::onRenderRasterCompleted(
    const RenderData &renderData) {
  // It is necessary to clone the raster. The raster is the same for each render
  // and
  // each new render modify the images in the filpBooks showing old previews.
  emit renderCompleted(renderData.m_rasA->clone(), renderData.m_renderId);
}

//*******************************************************************************************************
// PreviewFxManager
//*******************************************************************************************************

FxHistogramRender::FxHistogramRender()
    : QObject()
    , m_renderer(TSystem::getProcessorCount())
    , m_lastFrameInfo()
    , m_scene()
    , m_histograms()
    , m_isCameraViewMode(false) {
  m_renderPort = new FxHistogramRenderPort();
  m_renderer.enablePrecomputing(false);
  m_renderer.addPort(m_renderPort);
  m_abortedRendering.clear();

  qRegisterMetaType<TRasterP>("TRasterP");
  qRegisterMetaType<UINT>("UINT");
  connect(m_renderPort, SIGNAL(renderCompleted(const TRasterP &, UINT)), this,
          SLOT(onRenderCompleted(const TRasterP &, UINT)));
}

//-----------------------------------------------------------------------------

FxHistogramRender::~FxHistogramRender() {
  if (m_renderPort) delete m_renderPort;
}

//-----------------------------------------------------------------------------

void FxHistogramRender::setScene(ToonzScene *scene) { m_scene = scene; }

//-----------------------------------------------------------------------------

void FxHistogramRender::setHistograms(Histograms *histograms) {
  m_histograms = histograms;
}

//-----------------------------------------------------------------------------

void FxHistogramRender::computeHistogram(TFxP fx, int frame) {
  if (!m_histograms || !m_scene) return;
  if (!fx.getPointer()) {
    m_histograms->setRaster(0);
    return;
  }

  TSceneProperties *sceneProperties = m_scene->getProperties();
  if (!sceneProperties) return;
  TOutputProperties *outputProperties = sceneProperties->getPreviewProperties();
  if (!outputProperties) return;
  TRenderSettings rs = outputProperties->getRenderSettings();

  TFxP buildedFx;
  if (m_isCameraViewMode)
    buildedFx =
        buildPartialSceneFx(m_scene, (double)frame, fx, rs.m_shrinkX, true);
  else
    buildedFx = buildSceneFx(m_scene, frame, fx, true);
  if (!buildedFx.getPointer()) return;
  TRasterFxP rasterFx(buildedFx);
  if (!rasterFx) return;
  std::string alias = rasterFx->getAlias(frame, rs);
  if (!TImageCache::instance()->isCached(alias + ".noext" +
                                         std::to_string(frame))) {
    TDimension size = m_scene->getCurrentCamera()->getRes();
    TRectD area(TPointD(-0.5 * size.lx, -0.5 * size.ly),
                TDimensionD(size.lx, size.ly));
    m_renderPort->setRenderArea(area);
    TFxPair fxPair;
    fxPair.m_frameA = buildedFx;
    m_lastFrameInfo.m_renderId =
        m_renderer.startRendering((double)frame, rs, fxPair);
    if (m_lastFrameInfo.m_renderId == (UINT)-1) return;

    m_lastFrameInfo.m_frame   = frame;
    m_lastFrameInfo.m_fx      = fx;
    m_lastFrameInfo.m_fxAlias = alias;
  } else {
    std::string id =
        std::to_string(fx->getIdentifier()) + ".noext" + std::to_string(frame);
    TRasterImageP img = TImageCache::instance()->get(id, false);
    m_histograms->setRaster(img->getRaster());
  }
}

//-----------------------------------------------------------------------------

void FxHistogramRender::invalidateFrame(int frame) {
  if (!m_histograms || !m_scene) return;
  QMutexLocker sl(&m_mutex);
  updateRenderer(frame);
}

//-----------------------------------------------------------------------------

void FxHistogramRender::updateRenderer(int frame) {
  if (!m_histograms || !m_scene) return;
  if (!m_lastFrameInfo.m_fx.getPointer()) {
    m_histograms->setRaster(0);
    return;
  }
  // abort old render
  UINT renderId = m_lastFrameInfo.m_renderId;
  m_renderer.abortRendering(renderId);
  m_abortedRendering.append(renderId);

  int i;
  for (i = 0; i < m_scene->getFrameCount(); i++) {
    std::string id = std::to_string(m_lastFrameInfo.m_fx->getIdentifier()) +
                     ".noext" + std::to_string(i);
    TImageCache::instance()->remove(id);
  }
  m_lastFrameInfo.m_frame = frame;
  remakeRender();
}

//-----------------------------------------------------------------------------

void FxHistogramRender::remakeRender() {
  if (!m_histograms || !m_scene) return;
  if (!m_lastFrameInfo.m_fx.getPointer()) {
    m_histograms->setRaster(0);
    return;
  }
  TDimension size = m_scene->getCurrentCamera()->getRes();
  TRectD area(TPointD(-0.5 * size.lx, -0.5 * size.ly),
              TDimensionD(size.lx, size.ly));
  m_renderPort->setRenderArea(area);
  TRenderSettings rs =
      m_scene->getProperties()->getPreviewProperties()->getRenderSettings();

  TFxP buildedFx =
      buildPartialSceneFx(m_scene, (double)m_lastFrameInfo.m_frame,
                          m_lastFrameInfo.m_fx, rs.m_shrinkX, true);
  TRasterFxP rasterFx(buildedFx);
  if (!rasterFx) return;
  std::string alias = rasterFx->getAlias(m_lastFrameInfo.m_frame, rs);
  TFxPair fxPair;
  fxPair.m_frameA = buildedFx;
  m_lastFrameInfo.m_renderId =
      m_renderer.startRendering((double)m_lastFrameInfo.m_frame, rs, fxPair);
  if (m_lastFrameInfo.m_renderId == (UINT)-1) return;

  m_lastFrameInfo.m_fxAlias = alias;
}

//-----------------------------------------------------------------------------

void FxHistogramRender::onRenderCompleted(const TRasterP &raster,
                                          UINT renderId) {
  if (m_abortedRendering.contains(renderId)) {
    m_abortedRendering.removeAll(renderId);
    return;
  }

  QMutexLocker sl(&m_mutex);
  TRasterImageP img(raster);
  std::string id = std::to_string(m_lastFrameInfo.m_fx->getIdentifier()) +
                   ".noext" + std::to_string(m_lastFrameInfo.m_frame);
  TImageCache::instance()->add(id, img, true);

  m_histograms->setRaster(raster);
}
