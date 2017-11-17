

// System-core includes
#include "tsystem.h"
#include "tthreadmessage.h"
#include "timagecache.h"
#include "tstopwatch.h"
#include "tfiletype.h"
#include "ttimer.h"

// Images includes
#include "trasterimage.h"
#include "trop.h"
#include "tiio.h"
#include "tpixelutils.h"
#include "tlevel_io.h"
#include "tcodec.h"

// Fx-related includes
#include "tfxutil.h"

// Cache management includes
#include "tpassivecachemanager.h"

// Toonz app (currents)
#include "tapp.h"
#include "toutputproperties.h"

// Toonz stage structures
#include "toonz/tobjecthandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/scenefx.h"
#include "toonz/toonzscene.h"
#include "toonz/txshlevel.h"
#include "toonz/txsheet.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "toonz/palettecontroller.h"

// Toonz-qt stuff
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "filebrowserpopup.h"
#include "filebrowsermodel.h"

// Toonz 6 FlipConsole's slider
#include "toonzqt/flipconsole.h"

// Qt stuff
#include <QTimer>
#include <QRegion>
#include <QMetaType>

#include "previewer.h"

//------------------------------------------------

/*! \class Previewer
The Previewer class is a singleton which deals with Preview renders of isolated
frames.
The main purpose of this class, as of Toonz 6.x, is that of managing preview
requests coming
from Toonz's Scene Viewers; however, it is designed to support requests from a
generic
\b Previewer::Listener class.
The Previewer is a singleton, meaning that it centralizes requests from many
possible listeners
in a single collection of previewed frames.
\n \n
The most important method of the class is \b getRaster(), which attempts
retrieval of a certain
frame and, if the frame was not calculated yet, makes the Previewer build it.
\n
The update() slot is provided to refresh the cached informations about the
entire collection
of currently stored frames. Updated frames are recalculated only upon a new
getRaster(), in case the previously
stored informations no more match the current frame description.
\n
The clear() methods make the Previewer erase all stored informations about one
or all frames,
so that the following getRaster() will forcibly recalculate the requested frame.
*/

//------------------------------------------------

//======================================================================================
//    Preliminary stuff
//======================================================================================

#define CACHEID "RenderCache"

namespace {
bool suspendedRendering        = false;
Previewer *previewerInstance   = 0;
Previewer *previewerInstanceSC = 0;

QTimer levelChangedTimer, fxChangedTimer, xsheetChangedTimer,
    objectChangedTimer;
const int notificationDelay = 300;

//-------------------------------------------------------------------------

void buildNodeTreeDescription(std::string &desc, const TFxP &root);

//-------------------------------------------------------------------------

// Qt's contains actually returns QRegion::intersected... I wonder why...
inline bool contains(const QRegion &region, const TRect &rect) {
  return QRegion(toQRect(rect)).subtracted(region).isEmpty();
}
}

//======================================================================================
//    Internal classes declaration
//======================================================================================

//=======================
//    Previewer::Imp
//-----------------------

class Previewer::Imp final : public TRenderPort {
public:
  // All useful infos about a frame under Previewer's management
  struct FrameInfo {
  public:
    std::string m_alias;       // The alias of m_fx
    unsigned long m_renderId;  // The render process Id - passed by TRenderer
    QRegion m_renderedRegion;  // The plane region already rendered for m_fx
    TRect m_rectUnderRender;   // Plane region currently under render

    FrameInfo() : m_renderId((unsigned long)-1) {}
  };

public:
  Previewer *m_owner;
  TThread::Mutex m_mutex;

  std::set<Previewer::Listener *> m_listeners;
  std::map<int, FrameInfo> m_frames;
  std::vector<UCHAR> m_pbStatus;
  int m_computingFrameCount;

  TRenderSettings m_renderSettings;

  std::string m_cachePrefix;

  TDimension m_cameraRes;
  TRectD m_renderArea;
  TPointD m_cameraPos;
  bool m_subcamera;

  TRect m_previewRect;

  TRenderer m_renderer;

  // Save command stuff
  TLevelWriterP m_lw;
  int m_currentFrameToSave;

public:
  Imp(Previewer *owner);
  ~Imp();

  void notifyStarted(int frame);
  void notifyCompleted(int frame);
  void notifyFailed(int frame);
  void notifyUpdate();

  TFxPair buildSceneFx(int frame);

  // Updater methods. These refresh the manager's status, but do not launch new
  // renders
  // on their own. The refreshFrame() method must be manually invoked if
  // necessary.
  void updateRenderSettings();

  void updateAliases();
  void updateAliasKeyword(const std::string &keyword);

  // There are dependencies among the following updaters. Invoke them in the
  // specified order.
  void updateFrameRange();
  void updateProgressBarStatus();

  void updateCamera();
  void updatePreviewRect();  // This is automatically invoked by refreshFrame()

  // Use this method to re-render the passed frame. Infos specified with the
  // update* methods
  // are assumed correct.
  void refreshFrame(int frame);

  // TRenderPort methods
  void onRenderRasterStarted(const RenderData &renderData) override;
  void onRenderRasterCompleted(const RenderData &renderData) override;
  void onRenderFailure(const RenderData &renderData, TException &e) override;

  // Main-thread executed code related to TRenderPort. Used to update
  // thread-vulnerable infos.
  void doOnRenderRasterStarted(const RenderData &renderData);
  void doOnRenderRasterCompleted(const RenderData &renderData);
  void doOnRenderRasterFailed(const RenderData &renderData);

  void remove(int frame);
  void remove();

public:
  void saveFrame();
  bool doSaveRenderedFrames(const TFilePath fp);
};

//======================================================================================
//    Code implementation
//======================================================================================

//============================
//    Previewer::Listener
//----------------------------

Previewer::Listener::Listener() { m_refreshTimer.setSingleShot(true); }

//-----------------------------------------------------------------------------

void Previewer::Listener::requestTimedRefresh() { m_refreshTimer.start(1000); }

//=======================
//    Previewer::Imp
//-----------------------

Previewer::Imp::Imp(Previewer *owner)
    : m_owner(owner)
    , m_cameraRes(0, 0)
    , m_renderer(TSystem::getProcessorCount())
    , m_computingFrameCount(0)
    , m_currentFrameToSave(0)
    , m_subcamera(false)
    , m_lw() {
  // Precomputing (ie predictive cache) is disabled in this case. This is due to
  // current TFxCacheManager's
  // implementation, which can't still handle multiple render processes from the
  // same TRenderer. This should
  // change in the near future...
  m_renderer.enablePrecomputing(false);
  m_renderer.addPort(this);

  updateRenderSettings();
  updateCamera();
  updateFrameRange();
}

//-----------------------------------------------------------------------------

Previewer::Imp::~Imp() {
  m_renderer.removePort(this);

  // for(std::map<int, FrameInfo*>::iterator it=m_frames.begin();
  // it!=m_frames.end(); ++it)
  //  delete it->second; //crash on exit! not a serious leak, Previewer is a
  //  singleton
}

//-----------------------------------------------------------------------------

TFxPair Previewer::Imp::buildSceneFx(int frame) {
  TFxPair fxPair;

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();
  if (m_renderSettings.m_stereoscopic) {
    scene->shiftCameraX(-m_renderSettings.m_stereoscopicShift / 2.0);
    fxPair.m_frameA = ::buildSceneFx(
        scene, xsh, frame, TOutputProperties::AllLevels,
        m_renderSettings.m_applyShrinkToViewer ? m_renderSettings.m_shrinkX : 1,
        false);

    scene->shiftCameraX(m_renderSettings.m_stereoscopicShift);
    fxPair.m_frameB = ::buildSceneFx(
        scene, xsh, frame, TOutputProperties::AllLevels,
        m_renderSettings.m_applyShrinkToViewer ? m_renderSettings.m_shrinkX : 1,
        false);

    scene->shiftCameraX(-m_renderSettings.m_stereoscopicShift / 2.0);
  } else
    fxPair.m_frameA = ::buildSceneFx(
        scene, xsh, frame, TOutputProperties::AllLevels,
        m_renderSettings.m_applyShrinkToViewer ? m_renderSettings.m_shrinkX : 1,
        false);

  return fxPair;
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updateCamera() {
  // Retrieve current camera
  TCamera *currCamera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TRect subCameraRect = currCamera->getInterestRect();
  TPointD cameraPos(-0.5 * currCamera->getRes().lx,
                    -0.5 * currCamera->getRes().ly);

  // Update the camera region and camera stage area
  TDimension cameraRes(0, 0);
  TRectD renderArea;

  if (m_subcamera && subCameraRect.getLx() > 0 && subCameraRect.getLy() > 0) {
    cameraRes  = TDimension(subCameraRect.getLx(), subCameraRect.getLy());
    renderArea = TRectD(subCameraRect.x0, subCameraRect.y0,
                        subCameraRect.x1 + 1, subCameraRect.y1 + 1) +
                 cameraPos;
  } else {
    cameraRes  = currCamera->getRes();
    renderArea = TRectD(cameraPos, TDimensionD(cameraRes.lx, cameraRes.ly));
  }

  // Add the shrink to camera res
  cameraRes.lx /= m_renderSettings.m_shrinkX;
  cameraRes.ly /= m_renderSettings.m_shrinkY;

  // Invalidate the old camera size
  if (m_cameraRes != cameraRes || m_renderArea != renderArea) {
    m_cameraRes  = cameraRes;
    m_renderArea = renderArea;
    m_cameraPos  = cameraPos;

    // All previously rendered frames must be erased
    std::map<int, FrameInfo>::iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); ++it)
      TImageCache::instance()->remove(m_cachePrefix +
                                      std::to_string(it->first));

    m_frames.clear();
  }
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updateRenderSettings() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TRenderSettings renderSettings =
      scene->getProperties()->getPreviewProperties()->getRenderSettings();

  if (renderSettings.m_applyShrinkToViewer)
    renderSettings.m_shrinkY = renderSettings.m_shrinkX;
  else
    renderSettings.m_shrinkY = renderSettings.m_shrinkX = 1;

  // In case the settings changee, erase all previously cached images
  if (renderSettings != m_renderSettings) {
    m_renderSettings = renderSettings;

    std::map<int, FrameInfo>::iterator it;
    for (it = m_frames.begin(); it != m_frames.end(); ++it)
      TImageCache::instance()->remove(m_cachePrefix +
                                      std::to_string(it->first));

    m_frames.clear();
  }
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updateFrameRange() {
  // Erase all rendered frames outside the new frame range
  int newFrameCount =
      TApp::instance()->getCurrentScene()->getScene()->getFrameCount();

  std::map<int, FrameInfo>::iterator it,
      jt = m_frames.lower_bound(newFrameCount);
  for (it = jt; it != m_frames.end(); ++it)
    // Release all associated cached images
    TImageCache::instance()->remove(m_cachePrefix + std::to_string(it->first));

  m_frames.erase(jt, m_frames.end());

  // Resize the progress bar status
  int i, oldSize = m_pbStatus.size();
  m_pbStatus.resize(newFrameCount);
  for (i          = oldSize; i < newFrameCount; ++i)
    m_pbStatus[i] = FlipSlider::PBFrameNotStarted;
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updateProgressBarStatus() {
  unsigned int i, pbSize = m_pbStatus.size();
  std::map<int, FrameInfo>::iterator it;
  for (i = 0; i < pbSize; ++i) {
    it = m_frames.find(i);
    m_pbStatus[i] =
        (it == m_frames.end())
            ? FlipSlider::PBFrameNotStarted
            : ::contains(it->second.m_renderedRegion, m_previewRect)
                  ? FlipSlider::PBFrameFinished
                  : it->second.m_rectUnderRender.contains(m_previewRect)
                        ? FlipSlider::PBFrameStarted
                        : FlipSlider::PBFrameNotStarted;
  }
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updatePreviewRect() {
  TRectD previewRectD;

  /*--
   * SubCameraPreviewではない場合、Viewerの表示エリアに関係なく全画面で計算を行う
   * --*/
  if (!m_subcamera)
    previewRectD = m_renderArea;
  else {
    // Retrieve the view rects from each listener. Their union will form the
    // rect to be rendered.
    std::set<Previewer::Listener *>::iterator it;
    for (it = m_listeners.begin(); it != m_listeners.end(); ++it) {
      // Retrieve the listener's viewRect and add it to the preview rect
      previewRectD += (*it)->getPreviewRect();
    }
  }

  previewRectD *= m_renderArea;

  int shrinkX = m_renderSettings.m_shrinkX;
  int shrinkY = m_renderSettings.m_shrinkY;

  // Ensure that rect has the same pixel geometry as the preview camera
  previewRectD -= m_cameraPos;
  previewRectD.x0 = previewRectD.x0 / shrinkX;
  previewRectD.y0 = previewRectD.y0 / shrinkY;
  previewRectD.x1 = previewRectD.x1 / shrinkX;
  previewRectD.y1 = previewRectD.y1 / shrinkY;

  // Now, pass to m_cameraRes-relative coordinates
  TPointD shrinkedRelPos((m_renderArea.x0 - m_cameraPos.x) / shrinkX,
                         (m_renderArea.y0 - m_cameraPos.y) / shrinkY);
  previewRectD -= shrinkedRelPos;

  previewRectD.x0 = tfloor(previewRectD.x0);
  previewRectD.y0 = tfloor(previewRectD.y0);
  previewRectD.x1 = tceil(previewRectD.x1);
  previewRectD.y1 = tceil(previewRectD.y1);

  m_previewRect = TRect(previewRectD.x0, previewRectD.y0, previewRectD.x1 - 1,
                        previewRectD.y1 - 1);

  previewRectD += m_cameraPos + shrinkedRelPos;

  setRenderArea(previewRectD);
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updateAliases() {
  std::map<int, FrameInfo>::iterator it;
  for (it = m_frames.begin(); it != m_frames.end(); ++it) {
    TFxPair fxPair = buildSceneFx(it->first);

    std::string newAlias =
        fxPair.m_frameA ? fxPair.m_frameA->getAlias(it->first, m_renderSettings)
                        : "";
    newAlias =
        newAlias + (fxPair.m_frameB
                        ? fxPair.m_frameB->getAlias(it->first, m_renderSettings)
                        : "");

    if (newAlias != it->second.m_alias) {
      // Clear the remaining frame infos
      it->second.m_renderedRegion = QRegion();
    }
  }
}

//-----------------------------------------------------------------------------

void Previewer::Imp::updateAliasKeyword(const std::string &keyword) {
  std::map<int, FrameInfo>::iterator it;
  for (it = m_frames.begin(); it != m_frames.end(); ++it) {
    if (it->second.m_alias.find(keyword) != std::string::npos) {
      TFxPair fxPair = buildSceneFx(it->first);
      it->second.m_alias =
          fxPair.m_frameA
              ? fxPair.m_frameA->getAlias(it->first, m_renderSettings)
              : "";
      it->second.m_alias =
          it->second.m_alias +
          (fxPair.m_frameB
               ? fxPair.m_frameB->getAlias(it->first, m_renderSettings)
               : "");

      // Clear the remaining frame infos
      it->second.m_renderedRegion = QRegion();

      // No need to release the cached image... eventually, clear it
      TRasterImageP ri = TImageCache::instance()->get(
          m_cachePrefix + std::to_string(it->first), true);
      if (ri) ri->getRaster()->clear();
    }
  }
}

//-----------------------------------------------------------------------------

//! Starts rendering the passed frame.
void Previewer::Imp::refreshFrame(int frame) {
  if (suspendedRendering) return;

  // Build the region to render
  updatePreviewRect();

  if (m_previewRect.getLx() <= 0 || m_previewRect.getLy() <= 0) return;

  // Retrieve the FrameInfo for passed frame
  std::map<int, FrameInfo>::iterator it = m_frames.find(frame);
  if (it != m_frames.end()) {
    // In case the rect we would render is contained in the frame's rendered
    // region, quit
    if (::contains(it->second.m_renderedRegion, m_previewRect)) return;

    // Then, check the m_previewRect against the frame's m_rectUnderRendering.
    // Ensure that we're not re-launching the very same render.
    if (it->second.m_rectUnderRender == m_previewRect) return;

    // Stop any frame's previously running render process
    m_renderer.abortRendering(it->second.m_renderId);
  } else {
    it = m_frames.insert(std::make_pair(frame, FrameInfo())).first;

    // In case the frame is not in the frame range, we add a temporary
    // supplement
    // to the progress bar.
    if (frame >= (int)m_pbStatus.size()) m_pbStatus.resize(frame + 1);
  }

  // Build the TFxPair to be passed to TRenderer
  TFxPair fxPair = buildSceneFx(frame);

  // Update the RenderInfos associated with frame
  it->second.m_rectUnderRender = m_previewRect;
  it->second.m_alias = fxPair.m_frameA->getAlias(frame, m_renderSettings);
  if (fxPair.m_frameB)
    it->second.m_alias =
        it->second.m_alias + fxPair.m_frameB->getAlias(frame, m_renderSettings);

  // Retrieve the renderId of the rendering instance
  it->second.m_renderId = m_renderer.nextRenderId();
  std::string contextName("P");
  contextName += m_subcamera ? "SC" : "FU";
  contextName += std::to_string(frame);
  TPassiveCacheManager::instance()->setContextName(it->second.m_renderId,
                                                   contextName);

  // Start the render
  m_renderer.startRendering(frame, m_renderSettings, fxPair);
}

//-----------------------------------------------------------------------------

void Previewer::Imp::remove(int frame) {
  // Search the frame among rendered ones
  std::map<int, FrameInfo>::iterator it = m_frames.find(frame);
  if (it != m_frames.end()) {
    m_renderer.abortRendering(it->second.m_renderId);
    m_frames.erase(frame);
  }

  // Remove the associated image from cache
  TImageCache::instance()->remove(m_cachePrefix + std::to_string(frame));
}

//-----------------------------------------------------------------------------

void Previewer::Imp::remove() {
  m_renderer.stopRendering(false);

  // Remove all cached images
  std::map<int, FrameInfo>::iterator it;
  for (it = m_frames.begin(); it != m_frames.end(); ++it)
    TImageCache::instance()->remove(m_cachePrefix + std::to_string(it->first));

  m_frames.clear();
}

//-----------------------------------------------------------------------------

inline void Previewer::Imp::notifyStarted(int frame) {
  std::set<Previewer::Listener *>::iterator it;
  for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onRenderStarted(frame);
}

//-----------------------------------------------------------------------------

inline void Previewer::Imp::notifyCompleted(int frame) {
  std::set<Previewer::Listener *>::iterator it;
  for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onRenderCompleted(frame);
}

//-----------------------------------------------------------------------------

inline void Previewer::Imp::notifyFailed(int frame) {
  std::set<Previewer::Listener *>::iterator it;
  for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onRenderFailed(frame);
}

//-----------------------------------------------------------------------------

inline void Previewer::Imp::notifyUpdate() {
  std::set<Previewer::Listener *>::iterator it;
  for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onPreviewUpdate();
}

//-----------------------------------------------------------------------------

//! Adds the renderized image to TImageCache; listeneres are advised too.
void Previewer::Imp::onRenderRasterStarted(const RenderData &renderData) {
  // Emit the started signal to execute code in the main thread
  m_owner->emitStartedFrame(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::Imp::doOnRenderRasterStarted(const RenderData &renderData) {
  int frame = renderData.m_frames[0];

  m_computingFrameCount++;

  // Update the progress bar status
  if (frame < m_pbStatus.size()) m_pbStatus[frame] = FlipSlider::PBFrameStarted;

  // Notify listeners
  notifyStarted(frame);
}

//-----------------------------------------------------------------------------

//! Adds the renderized image to TImageCache; listeneres are advised too.
void Previewer::Imp::onRenderRasterCompleted(const RenderData &renderData) {
  if (renderData.m_rasB) {
    RenderData _renderData = renderData;
    assert(m_renderSettings.m_stereoscopic);
    TRop::makeStereoRaster(_renderData.m_rasA, _renderData.m_rasB);
    if (_renderData.m_info.m_gamma != 1.0)
      TRop::gammaCorrect(_renderData.m_rasA, _renderData.m_info.m_gamma);

    _renderData.m_rasB = TRasterP();
    m_owner->emitRenderedFrame(_renderData);
    return;
  }

  // If required, correct gamma
  if (renderData.m_info.m_gamma != 1.0)
    TRop::gammaCorrect(renderData.m_rasA, renderData.m_info.m_gamma);

  // Emit the started signal to execute code in the main thread
  m_owner->emitRenderedFrame(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::Imp::doOnRenderRasterCompleted(const RenderData &renderData) {
  int renderId = renderData.m_renderId;
  int frame    = renderData.m_frames[0];

  if (renderData.m_rasB) {
    assert(m_renderSettings.m_stereoscopic);
    TRop::makeStereoRaster(renderData.m_rasA, renderData.m_rasB);
  }

  TRasterP ras(renderData.m_rasA);

  m_computingFrameCount--;

  // Find the render infos in the Previewer
  std::map<int, FrameInfo>::iterator it = m_frames.find(frame);
  if (it == m_frames.end()) return;

  // Ensure that the render process id is the same
  if (renderId != it->second.m_renderId) return;

  // Store the rendered image in the cache - this is done in the MAIN thread due
  // to the necessity of accessing it->second.m_rectUnderRender for raster
  // extraction.
  std::string str = m_cachePrefix + std::to_string(frame);

  TRasterImageP ri(TImageCache::instance()->get(str, true));
  TRasterP cachedRas(ri ? ri->getRaster() : TRasterP());

  if (!cachedRas || (cachedRas->getSize() != m_cameraRes)) {
    TImageCache::instance()->remove(str);

    // Create the raster at camera resolution
    cachedRas = ras->create(m_cameraRes.lx, m_cameraRes.ly);
    cachedRas->clear();
    ri = TRasterImageP(cachedRas);
  }

  // Finally, copy the rendered raster over the cached one
  TRect rectUnderRender(
      it->second
          .m_rectUnderRender);  // Extract may MODIFY IT! E.g. with shrinks..!
  cachedRas = cachedRas->extract(rectUnderRender);
  if (cachedRas) {
    cachedRas->copy(ras);
    TImageCache::instance()->add(str, ri);
  }

  // Update the FrameInfo
  it->second.m_renderedRegion += toQRect(it->second.m_rectUnderRender);
  it->second.m_rectUnderRender = TRect();

  // Update the progress bar status
  if (frame < m_pbStatus.size())
    m_pbStatus[frame] = FlipSlider::PBFrameFinished;

  // Notify listeners
  notifyCompleted(frame);
}

//-----------------------------------------------------------------------------

//! Removes the associated raster from TImageCache, and listeners are made
//! aware.
void Previewer::Imp::onRenderFailure(const RenderData &renderData,
                                     TException &e) {
  m_owner->emitFailedFrame(renderData);
}

//-----------------------------------------------------------------------------

//! Adds the renderized image to TImageCache; listeneres are advised too.
void Previewer::Imp::doOnRenderRasterFailed(const RenderData &renderData) {
  m_computingFrameCount--;

  int frame = (int)renderData.m_frames[0];

  std::map<int, FrameInfo>::iterator it = m_frames.find(frame);
  if (it == m_frames.end()) return;

  if (renderData.m_renderId != it->second.m_renderId) return;

  it->second.m_renderedRegion  = QRegion();
  it->second.m_rectUnderRender = TRect();

  // Update the progress bar status
  if (frame < m_pbStatus.size())
    m_pbStatus[frame] = FlipSlider::PBFrameNotStarted;

  notifyCompleted(frame);  // Completed!?
}

//-----------------------------------------------------------------------------

namespace {
enum { eBegin, eIncrement, eEnd };

static DVGui::ProgressDialog *Pd = 0;

//-----------------------------------------------------------------------------

class ProgressBarMessager final : public TThread::Message {
public:
  int m_choice;
  int m_val;
  QString m_str;
  ProgressBarMessager(int choice, int val, const QString &str = "")
      : m_choice(choice), m_val(val), m_str(str) {}
  void onDeliver() override {
    switch (m_choice) {
    case eBegin:
      if (!Pd)
        Pd = new DVGui::ProgressDialog(
            QObject::tr("Saving previewed frames...."), QObject::tr("Cancel"),
            0, m_val);
      else
        Pd->setMaximum(m_val);
      Pd->show();
      break;
    case eIncrement:
      if (Pd->wasCanceled()) {
        delete Pd;
        Pd = 0;
      } else {
        // if (m_val==Pd->maximum()) Pd->hide();
        Pd->setValue(m_val);
      }
      break;
    case eEnd: {
      DVGui::info(m_str);
      delete Pd;
      Pd = 0;
    } break;
    default:
      assert(false);
    }
  }

  TThread::Message *clone() const override {
    return new ProgressBarMessager(*this);
  }
};

}  // namespace

//-----------------------------------------------------------------------------

class SavePreviewedPopup final : public FileBrowserPopup {
  Previewer *m_p;

public:
  SavePreviewedPopup() : FileBrowserPopup(tr("Save Previewed Images")) {
    setOkText(tr("Save"));
  }

  void setPreview(Previewer *p) { m_p = p; }

  bool execute() override {
    if (m_selectedPaths.empty()) return false;

    return m_p->doSaveRenderedFrames(*m_selectedPaths.begin());
  }
};

//=============================================================================

bool Previewer::doSaveRenderedFrames(TFilePath fp) {
  return m_imp->doSaveRenderedFrames(fp);
}

//=============================================================================

using namespace DVGui;

bool Previewer::Imp::doSaveRenderedFrames(TFilePath fp) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TOutputProperties *outputSettings =
      scene->getProperties()->getOutputProperties();

  QStringList formats;
  TLevelWriter::getSupportedFormats(formats, true);
  Tiio::Writer::getSupportedFormats(formats, true);

  std::string ext = fp.getType();
  if (ext == "") {
    ext = outputSettings->getPath().getType();
    fp  = fp.withType(ext);
  }
  if (fp.getName() == "") {
    DVGui::warning(
        tr("The file name cannot be empty or contain any of the following "
           "characters:(new line)  \\ / : * ? \"  |"));
    return false;
  }

  if (!formats.contains(QString::fromStdString(ext))) {
    DVGui::warning("Unsopporter raster format, cannot save");
    return false;
  }

  if (fp.getWideName() == L"") fp = fp.withName(scene->getSceneName());
  if (TFileType::getInfo(fp) == TFileType::RASTER_IMAGE || ext == "pct" ||
      fp.getType() == "pic" || ext == "pict")  // pct e' un formato"livello" (ha
                                               // i settings di quicktime) ma
                                               // fatto di diversi frames
    fp = fp.withFrame(TFrameId::EMPTY_FRAME);

  fp          = scene->decodeFilePath(fp);
  bool exists = TFileStatus(fp.getParentDir()).doesExist();
  if (!exists) {
    try {
      TFilePath parent = fp.getParentDir();
      TSystem::mkDir(parent);
      DvDirModel::instance()->refreshFolder(parent.getParentDir());
    } catch (TException &e) {
      error("Cannot create " + toQString(fp.getParentDir()) + " : " +
            QString(::to_string(e.getMessage()).c_str()));
      return false;
    } catch (...) {
      error("Cannot create " + toQString(fp.getParentDir()));
      return false;
    }
  }

  if (TSystem::doesExistFileOrLevel(fp)) {
    QString question(tr("File %1 already exists.\nDo you want to overwrite it?")
                         .arg(toQString(fp)));
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2) return false;
  }

  try {
    m_lw = TLevelWriterP(fp,
                         outputSettings->getFileFormatProperties(fp.getType()));
  } catch (TImageException &e) {
    error(QString::fromStdString(::to_string(e.getMessage())));
    return false;
  }

  m_lw->setFrameRate(outputSettings->getFrameRate());

  m_currentFrameToSave = 1;

  ProgressBarMessager(
      eBegin,
      TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount())
      .sendBlocking();

  QTimer::singleShot(50, m_owner, SLOT(saveFrame()));
  return true;
}

//-----------------------------------------------------------------------------

void Previewer::Imp::saveFrame() {
  static int savedFrames = 0;

  int frameCount =
      TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount();

  assert(Pd);

  for (; m_currentFrameToSave <= frameCount; m_currentFrameToSave++) {
    ProgressBarMessager(eIncrement, m_currentFrameToSave).sendBlocking();
    if (!Pd) break;

    // Ensure that current frame actually have to be saved
    int currFrameToSave = m_currentFrameToSave - 1;
    std::map<int, FrameInfo>::iterator it = m_frames.find(currFrameToSave);
    if (it == m_frames.end()) continue;

    if (m_pbStatus.size() < currFrameToSave ||
        m_pbStatus[currFrameToSave] != FlipSlider::PBFrameFinished)
      continue;

    TImageP img = TImageCache::instance()->get(
        m_cachePrefix + std::to_string(currFrameToSave), false);
    if (!img) continue;

    // Save the frame
    TImageWriterP writer = m_lw->getFrameWriter(TFrameId(m_currentFrameToSave));
    bool failureOnSaving = false;
    if (!writer) continue;

    writer->save(img);
    savedFrames++;
    m_currentFrameToSave++;
    QTimer::singleShot(50, m_owner, SLOT(saveFrame()));
    return;
  }

  // Output the save result
  QString str = "Saved " + QString(std::to_string(savedFrames).c_str()) +
                " frames out of " +
                QString(std::to_string(frameCount).c_str()) + " in " +
                QString(::to_string(m_lw->getFilePath()).c_str());

  if (!Pd) str = "Canceled! " + str;

  ProgressBarMessager(eEnd, 0, str).send();

  m_currentFrameToSave = 0;
  m_lw                 = TLevelWriterP();
  savedFrames          = 0;
}

//=============================================================================
// Previewer
//-----------------------------------------------------------------------------

Previewer::Previewer(bool subcamera) : m_imp(new Imp(this)) {
  m_imp->m_subcamera   = subcamera;
  m_imp->m_cachePrefix = CACHEID + std::string(subcamera ? "SC" : "");

  TApp *app = TApp::instance();

  bool ret = true;

  // ret = ret && connect(app->getPaletteController()->getCurrentPalette(),
  // SIGNAL(colorStyleChangedOnMouseRelease()),SLOT(onLevelChanged()));
  // ret = ret && connect(app->getPaletteController()->getCurrentPalette(),
  // SIGNAL(paletteChanged()),   SLOT(onLevelChanged()));
  ret = ret && connect(app->getPaletteController()->getCurrentLevelPalette(),
                       SIGNAL(colorStyleChangedOnMouseRelease()),
                       SLOT(onLevelChanged()));
  ret = ret && connect(app->getPaletteController()->getCurrentLevelPalette(),
                       SIGNAL(paletteChanged()), SLOT(onLevelChanged()));

  levelChangedTimer.setInterval(notificationDelay);
  fxChangedTimer.setInterval(notificationDelay);
  xsheetChangedTimer.setInterval(notificationDelay);
  objectChangedTimer.setInterval(notificationDelay);
  levelChangedTimer.setSingleShot(true);
  fxChangedTimer.setSingleShot(true);
  xsheetChangedTimer.setSingleShot(true);
  objectChangedTimer.setSingleShot(true);

  ret = ret && connect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()),
                       &levelChangedTimer, SLOT(start()));
  ret = ret && connect(app->getCurrentFx(), SIGNAL(fxChanged()),
                       &fxChangedTimer, SLOT(start()));
  ret = ret && connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()),
                       &xsheetChangedTimer, SLOT(start()));
  ret = ret && connect(app->getCurrentObject(), SIGNAL(objectChanged(bool)),
                       &objectChangedTimer, SLOT(start()));

  ret = ret && connect(&levelChangedTimer, SIGNAL(timeout()), this,
                       SLOT(onLevelChanged()));
  ret = ret &&
        connect(&fxChangedTimer, SIGNAL(timeout()), this, SLOT(onFxChanged()));
  ret = ret && connect(&xsheetChangedTimer, SIGNAL(timeout()), this,
                       SLOT(onXsheetChanged()));
  ret = ret && connect(&objectChangedTimer, SIGNAL(timeout()), this,
                       SLOT(onObjectChanged()));

  qRegisterMetaType<TRenderPort::RenderData>("TRenderPort::RenderData");

  ret = ret && connect(this, SIGNAL(startedFrame(TRenderPort::RenderData)),
                       this, SLOT(onStartedFrame(TRenderPort::RenderData)));

  ret = ret && connect(this, SIGNAL(renderedFrame(TRenderPort::RenderData)),
                       this, SLOT(onRenderedFrame(TRenderPort::RenderData)));

  ret = ret && connect(this, SIGNAL(failedFrame(TRenderPort::RenderData)), this,
                       SLOT(onFailedFrame(TRenderPort::RenderData)));

  // As a result of performing the connections above in the Previewer
  // constructor, no instance()
  // of it can be requested before a first scene has been completely
  // initialized.
  // Inform a global variable of the fact that a first instantiation was made.
  if (subcamera)
    previewerInstanceSC = this;
  else
    previewerInstance = this;

  assert(ret);
}

//-----------------------------------------------------------------------------

Previewer::~Previewer() {}

//-----------------------------------------------------------------------------

//! Ritorna l'istanza del \b Previewer
Previewer *Previewer::instance(bool subcameraPreview) {
  static Previewer _instance(false);
  static Previewer _instanceSC(true);

  return subcameraPreview ? &_instanceSC : &_instance;
}

//-----------------------------------------------------------------------------

std::vector<UCHAR> &Previewer::getProgressBarStatus() const {
  return m_imp->m_pbStatus;
}

//-----------------------------------------------------------------------------

void Previewer::addListener(Listener *listener) {
  m_imp->m_listeners.insert(listener);
  connect(&listener->m_refreshTimer, SIGNAL(timeout()), this,
          SLOT(updateView()));
}

//-----------------------------------------------------------------------------

void Previewer::removeListener(Listener *listener) {
  m_imp->m_listeners.erase(listener);
  disconnect(&listener->m_refreshTimer, SIGNAL(timeout()), this,
             SLOT(updateView()));

  if (m_imp->m_listeners.empty()) {
    m_imp->m_renderer.stopRendering(false);

    // Release all used context names
    std::string prefix("P");
    TPassiveCacheManager::instance()->releaseContextNamesWithPrefix(
        prefix + (m_imp->m_subcamera ? "SC" : "FU"));
  }
}

//-----------------------------------------------------------------------------

void Previewer::saveFrame() { m_imp->saveFrame(); }

//-----------------------------------------------------------------------------

void Previewer::saveRenderedFrames() {
  if (TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount() == 0) {
    info("No frame to save!");
    return;
  }
  if (m_imp->m_currentFrameToSave != 0) {
    info("Already saving!");
    return;
  }

  static SavePreviewedPopup *savePopup = 0;
  if (!savePopup) savePopup            = new SavePreviewedPopup;

  savePopup->setPreview(this);

  TFilePath outpath = TApp::instance()
                          ->getCurrentScene()
                          ->getScene()
                          ->getProperties()
                          ->getOutputProperties()
                          ->getPath();
  savePopup->setFolder(outpath.getParentDir());
  TFilePath name = outpath.withoutParentDir();

  savePopup->setFilename(
      name.getName() == ""
          ? name.withName(
                TApp::instance()->getCurrentScene()->getScene()->getSceneName())
          : name);

  savePopup->show();
  savePopup->raise();
  savePopup->activateWindow();
}

//-----------------------------------------------------------------------------

/*! Restituisce un puntatore al raster randerizzato se il frame e' disponibile,
    altrimenti comincia a calcolarlo*/
TRasterP Previewer::getRaster(int frame, bool renderIfNeeded) const {
  if (frame < 0) return TRasterP();
  std::map<int, Imp::FrameInfo>::iterator it = m_imp->m_frames.find(frame);
  if (it != m_imp->m_frames.end()) {
    if (frame < m_imp->m_pbStatus.size()) {
      if (m_imp->m_pbStatus[frame] == FlipSlider::PBFrameFinished ||
          !renderIfNeeded) {
        std::string str = m_imp->m_cachePrefix + std::to_string(frame);
        TRasterImageP rimg =
            (TRasterImageP)TImageCache::instance()->get(str, false);
        if (rimg) {
          TRasterP ras = rimg->getRaster();
          assert((TRaster32P)ras || (TRaster64P)ras);
          return ras;
        } else
          // Weird case - the frame was declared rendered, but no raster is
          // available...
          return TRasterP();
      } else {
        // Calculate the frame if it was not yet started
        if (m_imp->m_pbStatus[frame] == FlipSlider::PBFrameNotStarted)
          m_imp->refreshFrame(frame);
      }
    }

    // Retrieve the cached image, if any
    std::string str = m_imp->m_cachePrefix + std::to_string(frame);
    TRasterImageP rimg =
        (TRasterImageP)TImageCache::instance()->get(str, false);
    if (rimg) {
      TRasterP ras = rimg->getRaster();
      assert((TRaster32P)ras || (TRaster64P)ras);
      return ras;
    } else
      return TRasterP();
  } else {
    // Just schedule the frame for calculation, and return a void raster ptr
    if (renderIfNeeded) m_imp->refreshFrame(frame);
    return TRasterP();
  }
}

//-----------------------------------------------------------------------------

//! Verifica se \b frame e' nella cache, cioe' se il frame e' disponibile
bool Previewer::isFrameReady(int frame) const {
  if (frame < 0 || frame >= (int)m_imp->m_pbStatus.size()) return false;

  return m_imp->m_pbStatus[frame] == FlipSlider::PBFrameFinished;
}

//-----------------------------------------------------------------------------

bool Previewer::isActive() const { return !m_imp->m_listeners.empty(); }

//-----------------------------------------------------------------------------

bool Previewer::isBusy() const {
  return !m_imp->m_listeners.empty() && m_imp->m_computingFrameCount > 0;
}

//-----------------------------------------------------------------------------

//! Richiama IMP::invalidateFrames(string aliasKeyframe) per aggiornare il frame
//! \b fid.
void Previewer::onImageChange(TXshLevel *xl, const TFrameId &fid) {
  TFilePath fp             = xl->getPath().withFrame(fid);
  std::string levelKeyword = ::to_string(fp);

  // Inform the cache managers of level invalidation
  if (!m_imp->m_subcamera)
    TPassiveCacheManager::instance()->invalidateLevel(levelKeyword);

  m_imp->updateAliasKeyword(levelKeyword);
}

//-----------------------------------------------------------------------------

void Previewer::clear(int frame) {
  m_imp->remove(frame);
  m_imp->updateProgressBarStatus();
}

//-----------------------------------------------------------------------------

void Previewer::clear() {
  m_imp->remove();
  m_imp->updateAliases();
  m_imp->updateFrameRange();
  m_imp->updateRenderSettings();
  m_imp->updateCamera();
  m_imp->updatePreviewRect();
  m_imp->updateProgressBarStatus();
}

//-----------------------------------------------------------------------------

void Previewer::clearAll() {
  Previewer::instance(false)->clear();
  Previewer::instance(true)->clear();
}

//-----------------------------------------------------------------------------

//! Refreshes all scene frames
void Previewer::update() {
  if (m_imp->m_listeners.empty()) return;

  m_imp->updateAliases();
  m_imp->updateFrameRange();
  m_imp->updateRenderSettings();
  m_imp->updateCamera();
  m_imp->updatePreviewRect();
  m_imp->updateProgressBarStatus();

  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

//! Limited version of update(), just refreshes the view area.
void Previewer::updateView() {
  if (m_imp->m_listeners.empty()) return;

  m_imp->updateFrameRange();
  m_imp->updateRenderSettings();
  m_imp->updateCamera();
  m_imp->updatePreviewRect();
  m_imp->updateProgressBarStatus();

  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

void Previewer::onLevelChange(TXshLevel *xl) {
  TFilePath fp             = xl->getPath();
  std::string levelKeyword = ::to_string(fp);

  // Inform the cache managers of level invalidation
  if (!m_imp->m_subcamera)
    TPassiveCacheManager::instance()->invalidateLevel(levelKeyword);

  m_imp->updateAliasKeyword(levelKeyword);
  m_imp->updateProgressBarStatus();

  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

void Previewer::onLevelChanged() {
  TXshLevel *xl = TApp::instance()->getCurrentLevel()->getLevel();
  if (!xl) return;

  std::string levelKeyword;
  TFilePath fp = xl->getPath();
  levelKeyword = ::to_string(fp.withType(""));

  // Inform the cache managers of level invalidation
  if (!m_imp->m_subcamera)
    TPassiveCacheManager::instance()->invalidateLevel(levelKeyword);

  m_imp->updateAliasKeyword(levelKeyword);
  m_imp->updateProgressBarStatus();

  // Seems that the scene viewer does not update in this case...
  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

void Previewer::onFxChanged() {
  m_imp->updateAliases();
  m_imp->updateProgressBarStatus();

  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

void Previewer::onXsheetChanged() {
  m_imp->updateRenderSettings();
  m_imp->updateCamera();
  m_imp->updateFrameRange();
  m_imp->updateAliases();
  m_imp->updateProgressBarStatus();

  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

void Previewer::onObjectChanged() {
  m_imp->updateAliases();
  m_imp->updateProgressBarStatus();

  m_imp->notifyUpdate();
}

//-----------------------------------------------------------------------------

void Previewer::onChange(const TFxChange &change) { onObjectChanged(); }

//-----------------------------------------------------------------------------

void Previewer::emitStartedFrame(const TRenderPort::RenderData &renderData) {
  emit startedFrame(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::emitRenderedFrame(const TRenderPort::RenderData &renderData) {
  emit renderedFrame(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::emitFailedFrame(const TRenderPort::RenderData &renderData) {
  emit failedFrame(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::onStartedFrame(TRenderPort::RenderData renderData) {
  // Invoke the corresponding function. This happens in the MAIN THREAD
  m_imp->doOnRenderRasterStarted(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::onRenderedFrame(TRenderPort::RenderData renderData) {
  m_imp->doOnRenderRasterCompleted(renderData);
}

//-----------------------------------------------------------------------------

void Previewer::onFailedFrame(TRenderPort::RenderData renderData) {
  m_imp->doOnRenderRasterFailed(renderData);
}

//-----------------------------------------------------------------------------

//! The suspendRendering method allows suspension of the previewer's rendering
//! activity for safety purposes, typically related to the fact that no
//! rendering
//! process should actually be performed as the underlying scene is about to
//! change
//! or being destroyed. Upon suspension, further rendering requests are silently
//! rejected - and currently active ones are canceled and waited until they are
//! no
//! longer active.
//! NOTE: This method is currently static declared, since the Previewer must be
//! be instanced only after a consistent scene has been initialized. This method
//! is allowed to bypass such limitation.
void Previewer::suspendRendering(bool suspend) {
  suspendedRendering = suspend;
  if (suspend && previewerInstance)
    previewerInstance->m_imp->m_renderer.stopRendering(true);
  if (suspend && previewerInstanceSC)
    previewerInstanceSC->m_imp->m_renderer.stopRendering(true);
}
