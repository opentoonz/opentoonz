

// System-core includes
#include "tsystem.h"  //Processors count
#include "timagecache.h"
#include "tw/stringtable.h"

// Toonz scene-stage structures
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/tframehandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tcamera.h"
#include "toonz/palettecontroller.h"
#include "tapp.h"  //Toonz current objects

// Images stuff
#include "trasterimage.h"
#include "trop.h"

// Fxs stuff
#include "toutputproperties.h"
#include "trasterfx.h"
#include "toonz/scenefx.h"  //Fxs tree build-up
#include "toonz/tcolumnfx.h"

// Cache management
#include "tpassivecachemanager.h"

// Flipbook
#include "flipbook.h"
#include "toonzqt/flipconsole.h"

// Qt stuff
#include <QMetaType>
#include <QRegion>
#include "toonzqt/gutil.h"  //For conversions between TRects and QRects

// Preferences
#include "toonz/preferences.h"

#include "previewfxmanager.h"

//=======================================================================================================

// Resume: The 'Preview Fx' command shows a flipbook associated with given fx,
// containing the appearance
//         of the fx under current rendering status (including current camera,
//         preview settings, schematic tree).
//
// There are some considerations to be aware of:
//   1. A Preview Fx flipbook must hold the render of the whole Preview settings
//   range. Plus, many Preview Fx
//      could live altogether. It could be that more than one Preview Fx window
//      is active for the same fx.
//   2. The flipbook associated with a 'Preview Fx' command should react to
//   updates of the rendering status.
//      This should happen as long as the fx actually has a meaning in the
//      rendering context - that is, if
//      the user enters sub- or super-xsheets, the flipbook should continue
//      rendering with
//      the old data. Possibly, if modifying a sub-xsheet, an 'upper' Preview Fx
//      should react accordingly.
//   3. Fx Subtree aliases retrieved through TRasterFx::getAlias() may be used
//   to decide if some frame has to
//      be rebuilt.

//=======================================================================================================

//  Forward declarations
class PreviewFxInstance;

//=======================================================================================================

//==============================
//    Preliminary functions
//------------------------------

namespace {
bool suspendedRendering             = false;
PreviewFxManager *previewerInstance = 0;

// Timer used to deliver scene changes notifications in an 'overridden' fashion.
// In practice, only the last (up to a fixed time granularity) of these
// notifications
// is actually received by the manager's associated slots.
QTimer levelChangedTimer, fxChangedTimer, xsheetChangedTimer,
    objectChangedTimer;
const int notificationDelay = 300;

//----------------------------------------------------------------------------

inline std::string getCacheId(const TFxP &fx, int frame) {
  return std::to_string(fx->getIdentifier()) + ".noext" + std::to_string(frame);
}

//----------------------------------------------------------------------------

// NOTE: This method will not currently trespass xsheet level boundaries. It
// will not
// recognize descendants in sub-xsheets....
bool areAncestorAndDescendant(const TFxP &ancestor, const TFxP &descendant) {
  if (ancestor.getPointer() == descendant.getPointer()) return true;

  int i;
  for (i = 0; i < ancestor->getInputPortCount(); ++i)
    if (areAncestorAndDescendant(ancestor->getInputPort(i)->getFx(),
                                 descendant))
      return true;

  return false;
}

//----------------------------------------------------------------------------

// Qt's contains actually returns QRegion::intersected... I wonder why...
inline bool contains(const QRegion &region, const TRect &rect) {
  return QRegion(toQRect(rect)).subtracted(region).isEmpty();
}

//----------------------------------------------------------------------------

inline void adaptView(FlipBook *flipbook, TDimension cameraSize) {
  TRect imgRect(cameraSize);
  flipbook->getImageViewer()->adaptView(imgRect, imgRect);
}
};  // namespace

//=======================================================================================================

//==================================
//    PreviewFxRenderPort class
//----------------------------------

//! This class receives and handles notifications from a TRenderer executing the
//! preview fx
//! rendering job.
class PreviewFxRenderPort final : public QObject, public TRenderPort {
  PreviewFxInstance *m_owner;

public:
  PreviewFxRenderPort(PreviewFxInstance *owner);
  ~PreviewFxRenderPort();

  void onRenderRasterStarted(
      const TRenderPort::RenderData &renderData) override;
  void onRenderRasterCompleted(const RenderData &renderData) override;
  void onRenderFailure(const RenderData &renderData, TException &e) override;
  void onRenderFinished(bool inCanceled = false) override;
};

//----------------------------------------------------------------------------------------

PreviewFxRenderPort::PreviewFxRenderPort(PreviewFxInstance *owner)
    : m_owner(owner) {}

//----------------------------------------------------------------------------------------

PreviewFxRenderPort::~PreviewFxRenderPort() {}

//=======================================================================================================

//==========================
//    PreviewFxInstance
//--------------------------

class PreviewFxInstance {
public:
  struct FrameInfo {
    std::string m_alias;
    QRegion m_renderedRegion;

    FrameInfo(const std::string &alias) : m_alias(alias) {}
  };

public:
  TXsheetP m_xsheet;
  TRasterFxP m_fx;
  TRenderer m_renderer;
  PreviewFxRenderPort m_renderPort;

  std::set<FlipBook *> m_flipbooks;
  std::set<FlipBook *> m_frozenFlips;  // Used externally by PreviewFxManager

  int m_start, m_end, m_step, m_initFrame;
  std::map<int, FrameInfo> m_frameInfos;  // Used to resume fx tree structures
  std::vector<UCHAR> m_pbStatus;

  TRenderSettings m_renderSettings;
  TDimension m_cameraRes;
  TRectD m_renderArea;
  TPointD m_cameraPos;
  TPointD m_subcameraDisplacement;
  bool m_subcamera;

  QRegion m_overallRenderedRegion;
  TRect m_rectUnderRender;
  bool m_renderFailed;

  TLevelP m_level;
  TSoundTrackP m_snd;

public:
  PreviewFxInstance(TFxP fx, TXsheet *xsh);
  ~PreviewFxInstance();

  void addFlipbook(FlipBook *&flipbook);
  void detachFlipbook(FlipBook *flipbook);
  void updateFlipbooks();

  void refreshViewRects(bool rebuild = false);

  void reset();
  void reset(int frame);

  // Updater methods. These refresh the manager's status, but do not launch new
  // renders
  // on their own. The refreshViewRects method must be invoked to trigger it.
  // Observe that there may exist dependencies among them - invoking in the
  // following
  // declaration order is safe.
  void updateFrameRange();
  void updateInitialFrame();
  void updateRenderSettings();
  void updateCamera();
  void updateFlipbookTitles();
  void updatePreviewRect();

  void updateAliases();
  void updateAliasKeyword(const std::string &keyword);

  void updateProgressBarStatus();

  void onRenderRasterStarted(const TRenderPort::RenderData &renderData);
  void onRenderRasterCompleted(const TRenderPort::RenderData &renderData);
  void onRenderFailure(const TRenderPort::RenderData &renderData,
                       TException &e);
  void onRenderFinished(bool isCanceled = false);

  void doOnRenderRasterCompleted(const TRenderPort::RenderData &renderData);
  void doOnRenderRasterStarted(const TRenderPort::RenderData &renderData);

  bool isSubCameraActive() { return m_subcamera; }

private:
  void cropAndStep(int &frame);

  bool isFullPreview();
  TFxP buildSceneFx(int frame);

  void addRenderData(std::vector<TRenderer::RenderData> &datas,
                     ToonzScene *scene, int frame, bool rebuild);
  void startRender(bool rebuild = false);
};

//------------------------------------------------------------------

inline bool PreviewFxInstance::isFullPreview() {
  return dynamic_cast<TOutputFx *>((TFx *)m_fx.getPointer());
}

//------------------------------------------------------------------

inline void PreviewFxInstance::cropAndStep(int &frame) {
  frame = frame < m_start ? m_start : frame;
  frame = (frame > m_end && m_end > -1) ? m_end : frame;

  // If a step was specified, ensure that frame is on step multiples.
  int framePos = (frame - m_start) / m_step;
  frame        = m_start + (framePos * m_step);
}

//------------------------------------------------------------------

inline TFxP PreviewFxInstance::buildSceneFx(int frame) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  if (isFullPreview())
    return ::buildSceneFx(scene, m_xsheet.getPointer(), frame,
                          m_renderSettings.m_shrinkX, true);
  else
    return ::buildPartialSceneFx(scene, m_xsheet.getPointer(), frame, m_fx,
                                 m_renderSettings.m_shrinkX, true);
}

//------------------------------------------------------------------

void PreviewFxInstance::addRenderData(std::vector<TRenderer::RenderData> &datas,
                                      ToonzScene *scene, int frame,
                                      bool rebuild) {
  // Seek the image associated to the render data in the cache.
  std::map<int, FrameInfo>::iterator it;
  it = m_frameInfos.find(frame);

  TRasterFxP builtFx =
      buildSceneFx(frame);  // when stereoscopic, i use this only for the alias
  TRasterFxP builtFxA, builtFxB;

  if (m_renderSettings.m_stereoscopic) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    scene->shiftCameraX(-m_renderSettings.m_stereoscopicShift / 2);
    builtFxA = buildSceneFx(frame);
    scene->shiftCameraX(m_renderSettings.m_stereoscopicShift);
    builtFxB = buildSceneFx(frame);
    scene->shiftCameraX(-m_renderSettings.m_stereoscopicShift / 2);
  } else
    builtFxA = builtFx;

  if (it == m_frameInfos.end()) {
    // Should not be - however, in this case build an associated Frame info
    it = m_frameInfos.insert(std::make_pair(frame, FrameInfo(std::string())))
             .first;
    it->second.m_alias =
        builtFx ? builtFx->getAlias(frame, m_renderSettings) : "";
  }

  bool isCalculated =
      (!builtFx) || ((!rebuild) && ::contains(it->second.m_renderedRegion,
                                              m_rectUnderRender));

  m_pbStatus[(frame - m_start) / m_step] = isCalculated
                                               ? FlipSlider::PBFrameFinished
                                               : FlipSlider::PBFrameNotStarted;

  // If it is already present, return
  if (isCalculated) return;

  datas.push_back(TRenderer::RenderData(frame, m_renderSettings,
                                        TFxPair(builtFxA, builtFxB)));
}

//-----------------------------------------------------------------------------------------

PreviewFxInstance::PreviewFxInstance(TFxP fx, TXsheet *xsh)
    : m_renderer(TSystem::getProcessorCount())
    , m_renderPort(this)
    , m_fx(fx)
    , m_cameraRes(0, 0)
    , m_start(0)
    , m_end(-1)
    , m_initFrame(0)
    , m_xsheet(xsh) {
  // Install the render port on the instance renderer
  m_renderer.addPort(&m_renderPort);

  updateRenderSettings();
  updateCamera();
  updateFrameRange();
  updateAliases();
}

//----------------------------------------------------------------------------------------

PreviewFxInstance::~PreviewFxInstance() {
  // Stop the render - there is no need to wait for a complete (and blocking)
  // render stop.
  m_renderer.removePort(
      &m_renderPort);  // No more images to be stored in the cache!
  m_renderer.stopRendering();

  // Release the user cache about this instance
  std::string contextName("PFX");
  contextName += std::to_string(m_fx->getIdentifier());
  TPassiveCacheManager::instance()->releaseContextNamesWithPrefix(contextName);

  // Clear the cached images
  int i;
  for (i = m_start; i <= m_end; i += m_step)
    TImageCache::instance()->remove(getCacheId(m_fx, i));
}

//-----------------------------------------------------------------------------------------

//! Clears the preview instance information about passed frame, including any
//! cached image.
//! Information needed to preview again are NOT rebuilt.
void PreviewFxInstance::reset(int frame) {
  TImageCache::instance()->remove(getCacheId(m_fx, frame));
  std::map<int, FrameInfo>::iterator it = m_frameInfos.find(frame);
  if (it != m_frameInfos.end()) {
    TRasterFxP builtFx = buildSceneFx(frame);
    it->second.m_alias =
        builtFx ? builtFx->getAlias(frame, m_renderSettings) : "";
    it->second.m_renderedRegion = QRegion();
    m_overallRenderedRegion     = QRegion();
  }
}

//-----------------------------------------------------------------------------------------

//! Clears the preview instance information, including cached images.
//! Information needed
//! to preview again are rebuilt.
void PreviewFxInstance::reset() {
  int i;
  for (i = m_start; i <= m_end; i += m_step)
    TImageCache::instance()->remove(getCacheId(m_fx, i));

  m_frameInfos.clear();

  updateFrameRange();
  updateRenderSettings();
  updateCamera();
  updateFlipbookTitles();
  updateAliases();
}

//-----------------------------------------------------------------------------------------

void PreviewFxInstance::addFlipbook(FlipBook *&flipbook) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TOutputProperties *properties =
      scene->getProperties()->getPreviewProperties();

  int currFrame = flipbook ? flipbook->getCurrentFrame() - 1
                           : app->getCurrentFrame()->getFrame();
  cropAndStep(currFrame);

  if (!flipbook) {
    /*-- 使用可能なFlipbookを取り出す。Poolに無い場合は新たに作る --*/
    flipbook = FlipBookPool::instance()->pop();

    // In case this is a subcamera preview, fit the flipbook's view. This is
    // done *before* associating
    // m_fx to the flipbook (using Flipbook::setLevel) - so there is no
    // duplicate view refresh.
    /*-- Preview Settingsで"Use Sub Camera"
     * がONのとき、サブカメラ枠の外は計算しないようになる --*/
    if (m_subcamera) {
      // result->adaptGeometry(TRect(previewInstance->m_cameraRes));   //This
      // one fits the panel, too.
      adaptView(flipbook, m_cameraRes);  // This one adapts the view. If has
                                         // associated fx, calls refresh...
    } else {
      // Retrieve the eventual sub-camera
      TCamera *currCamera = TApp::instance()
                                ->getCurrentScene()
                                ->getScene()
                                ->getCurrentPreviewCamera();
      TRect subcameraRect(currCamera->getInterestRect());
      /*-- Viewer上でサブカメラが指定されている状態でFxPreviewをした場合 --*/
      if (subcameraRect.getLx() > 0 && subcameraRect.getLy() > 0) {
        /*-- サブカメラ枠のShrink --*/
        if (m_renderSettings.m_shrinkX > 1 || m_renderSettings.m_shrinkY > 1) {
          subcameraRect.x0 /= m_renderSettings.m_shrinkX;
          subcameraRect.y0 /= m_renderSettings.m_shrinkY;
          subcameraRect.x1 =
              (subcameraRect.x1 + 1) / m_renderSettings.m_shrinkX - 1;
          subcameraRect.y1 =
              (subcameraRect.y1 + 1) / m_renderSettings.m_shrinkY - 1;
        }

        // Fit & pan the panel to cover the sub-camera
        flipbook->adaptGeometry(subcameraRect, TRect(TPoint(), m_cameraRes));
      }
    }

    /*-- フリーズボタンの表示 --*/
    flipbook->addFreezeButtonToTitleBar();
  }

  m_flipbooks.insert(flipbook);

  /*-- タイトルの設定。Previewコマンドから呼ばれた場合はisFullPreviewがON --*/

  // Build the fx string description - Should really be moved in a better
  // function...
  std::wstring fxId;
  TLevelColumnFx *columnFx = dynamic_cast<TLevelColumnFx *>(m_fx.getPointer());
  TZeraryColumnFx *sfx     = dynamic_cast<TZeraryColumnFx *>(m_fx.getPointer());
  if (columnFx)
    fxId =
        L"Col" + QString::number(columnFx->getColumnIndex() + 1).toStdWString();
  else if (sfx)
    fxId = sfx->getZeraryFx()->getFxId();
  else {
    fxId = m_fx->getFxId();
    if (fxId.empty()) fxId = m_fx->getName();
  }

  // Adjust the flipbook appropriately

  // Decorate the description for the flipbook
  if (isFullPreview()) {
    flipbook->setTitle(
        /*"Rendered Frames  ::  From " + QString::number(m_start+1) +
            " To " + QString::number(m_end+1) +
     "  ::  Step " + QString::number(m_step)*/
        QObject::tr("Rendered Frames  ::  From %1 To %2  ::  Step %3")
            .arg(QString::number(m_start + 1))
            .arg(QString::number(m_end + 1))
            .arg(QString::number(m_step)));
    TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
    prop->m_frameRate              = properties->getFrameRate();
    m_snd                          = m_xsheet->makeSound(prop);
    if (Preferences::instance()->fitToFlipbookEnabled())
      flipbook->getImageViewer()->adaptView(TRect(m_cameraRes),
                                            TRect(m_cameraRes));

  } else
    flipbook->setTitle(
        QObject::tr("Preview FX :: %1 ").arg(QString::fromStdWString(fxId)));

  // In case the render is a full preview, add the soundtrack

  // Associate the rendered level to flipbook
  flipbook->setLevel(m_fx.getPointer(), m_xsheet.getPointer(),
                     m_level.getPointer(), 0, m_start + 1, m_end + 1, m_step,
                     currFrame + 1, m_snd.getPointer());

  // Add the progress bar status pointer
  flipbook->setProgressBarStatus(&m_pbStatus);
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::detachFlipbook(FlipBook *flipbook) {
  // Just remove the flipbook from the flipbooks container
  std::set<FlipBook *>::iterator it = m_flipbooks.find(flipbook);
  if (it == m_flipbooks.end()) return;

  m_flipbooks.erase(it);

  // If the flipbook set associated with the render is now empty, stop the
  // render
  if (m_flipbooks.empty()) m_renderer.stopRendering();
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateFlipbooks() {
  std::set<FlipBook *>::iterator it;
  for (it = m_flipbooks.begin(); it != m_flipbooks.end(); ++it) (*it)->update();
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateFrameRange() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TOutputProperties *properties =
      scene->getProperties()->getPreviewProperties();

  int frameCount = m_xsheet->getFrameCount();

  // Initialize the render starting from current frame. If not in the preview
  // range,
  // start from the closest range extreme.
  properties->getRange(m_start, m_end, m_step);
  if (m_end < 0) m_end = frameCount - 1;

  // Intersect with the fx active frame range
  TRasterFxP rasterFx(m_fx);
  TFxTimeRegion timeRegion(rasterFx->getTimeRegion());
  m_start = std::max(timeRegion.getFirstFrame(), m_start);
  m_end   = std::min(timeRegion.getLastFrame(), m_end);

  // Release all images not in the new frame range
  std::map<int, FrameInfo>::iterator it, jt;
  for (it = m_frameInfos.begin(); it != m_frameInfos.end();) {
    if (it->first < m_start || it->first > m_end ||
        ((it->first - m_start) % m_step)) {
      TImageCache::instance()->remove(getCacheId(m_fx, it->first));
      jt = it++;
      m_frameInfos.erase(jt);
    } else
      ++it;
  }

  // Build a level to associate the flipbook with the rendered output
  m_level->setName(std::to_string(m_fx->getIdentifier()) + ".noext");
  int i;
  for (i = 0; i < frameCount; i++) m_level->setFrame(TFrameId(i), 0);

  // Resize and update internal containers
  if (m_start > m_end) {
    m_frameInfos.clear();
    m_pbStatus.clear();
  } else {
    // Build the new frame-alias range
    for (i = m_start; i <= m_end; i += m_step)
      if (m_frameInfos.find(i) == m_frameInfos.end()) {
        // Clear the overall rendered region and build the frame info
        m_overallRenderedRegion = QRegion();
        m_frameInfos.insert(std::make_pair(i, std::string()));
      }

    // Resize the progress bar
    m_pbStatus.resize((m_end - m_start) / m_step + 1);
  }

  // Reset the flipbooks' frame range
  std::set<FlipBook *>::iterator kt;
  int currFrame;
  bool fullPreview = isFullPreview();
  for (kt = m_flipbooks.begin(); kt != m_flipbooks.end(); ++kt) {
    currFrame = (*kt)->getCurrentFrame() - 1;
    cropAndStep(currFrame);
    (*kt)->setLevel(m_fx.getPointer(), m_xsheet.getPointer(),
                    m_level.getPointer(), 0, m_start + 1, m_end + 1, m_step,
                    currFrame + 1, m_snd.getPointer());
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateInitialFrame() {
  // Search all flipbooks and take the minimum of each's current
  std::set<FlipBook *>::iterator kt;
  m_initFrame = (std::numeric_limits<int>::max)();
  for (kt = m_flipbooks.begin(); kt != m_flipbooks.end(); ++kt)
    m_initFrame = std::min(m_initFrame, (*kt)->getCurrentFrame() - 1);

  cropAndStep(m_initFrame);
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateFlipbookTitles() {
  if (isFullPreview() && m_start <= m_end) {
    int start = m_start + 1;
    int end   = m_end + 1;

    std::set<FlipBook *>::iterator kt;
    for (kt = m_flipbooks.begin(); kt != m_flipbooks.end(); ++kt) {
      // In the full preview case, the title must display the frame range
      // information
      (*kt)->setTitle(
          /*"Rendered Frames  ::  From " + QString::number(start) +
        " To " + QString::number(end) +
 "  ::  Step " + QString::number(m_step)*/
          QObject::tr("Rendered Frames  ::  From %1 To %2  ::  Step %3")
              .arg(QString::number(start))
              .arg(QString::number(end))
              .arg(QString::number(m_step)));
    }
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateAliases() {
  if (m_start > m_end) return;

  std::string newAlias;

  // Build and compare the new aliases with the stored ones
  std::map<int, FrameInfo>::iterator it;
  for (it = m_frameInfos.begin(); it != m_frameInfos.end(); ++it) {
    TRasterFxP builtFx = buildSceneFx(it->first);
    newAlias = builtFx ? builtFx->getAlias(it->first, m_renderSettings) : "";
    if (newAlias != it->second.m_alias) {
      // Clear the overall and frame-specific rendered regions
      m_overallRenderedRegion     = QRegion();
      it->second.m_renderedRegion = QRegion();

      it->second.m_alias = newAlias;
    }
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateAliasKeyword(const std::string &keyword) {
  if (m_start > m_end) return;

  // Remove the rendered image whose alias contains keyword
  std::map<int, FrameInfo>::iterator it;
  for (it = m_frameInfos.begin(); it != m_frameInfos.end(); ++it) {
    if (it->second.m_alias.find(keyword) != std::string::npos) {
      // Clear the overall and frame-specific rendered regions
      m_overallRenderedRegion     = QRegion();
      it->second.m_renderedRegion = QRegion();

      // Clear the cached image
      TRasterImageP ri =
          TImageCache::instance()->get(getCacheId(m_fx, it->first), true);
      if (ri) ri->getRaster()->clear();
    }
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateProgressBarStatus() {
  int i;
  unsigned int j;
  std::map<int, FrameInfo>::iterator it;
  for (i = m_start, j = 0; i <= m_end; i += m_step, ++j) {
    it            = m_frameInfos.find(i);
    m_pbStatus[j] = ::contains(it->second.m_renderedRegion, m_rectUnderRender)
                        ? FlipSlider::PBFrameFinished
                        : FlipSlider::PBFrameNotStarted;
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateRenderSettings() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TOutputProperties *properties =
      scene->getProperties()->getPreviewProperties();

  m_subcamera = properties->isSubcameraPreview();

  TRenderSettings renderSettings = properties->getRenderSettings();

  if (m_renderSettings != renderSettings) {
    m_renderSettings = renderSettings;

    // Erase all previuosly previewed images
    int i;
    for (i = m_start; i <= m_end; i += m_step)
      TImageCache::instance()->remove(getCacheId(m_fx, i));

    // Clear all frame-specific rendered regions
    std::map<int, FrameInfo>::iterator it;
    for (it = m_frameInfos.begin(); it != m_frameInfos.end(); ++it)
      it->second.m_renderedRegion = QRegion();
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updateCamera() {
  // Clear the overall rendered region
  m_overallRenderedRegion = QRegion();

  // Retrieve the preview camera
  TCamera *currCamera = TApp::instance()
                            ->getCurrentScene()
                            ->getScene()
                            ->getCurrentPreviewCamera();
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

  cameraRes.lx /= m_renderSettings.m_shrinkX;
  cameraRes.ly /= m_renderSettings.m_shrinkY;

  if (m_cameraRes != cameraRes || m_renderArea != renderArea) {
    m_cameraRes  = cameraRes;
    m_renderArea = renderArea;
    m_cameraPos  = cameraPos;

    // Build the displacement needed when extracting the flipbooks' views
    m_subcameraDisplacement =
        TPointD(0.5 * (m_renderArea.x0 + m_renderArea.x1),
                0.5 * (m_renderArea.y0 + m_renderArea.y1));

    // Erase all previuosly previewed images
    int i;
    for (i = m_start; i <= m_end; i += m_step)
      TImageCache::instance()->remove(getCacheId(m_fx, i));

    // Clear all frame-specific rendered regions
    std::map<int, FrameInfo>::iterator it;
    for (it = m_frameInfos.begin(); it != m_frameInfos.end(); ++it)
      it->second.m_renderedRegion = QRegion();
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::updatePreviewRect() {
  bool isFullRender = false;
  if (!m_subcamera) {
    // Retrieve the eventual sub-camera
    TCamera *currCamera = TApp::instance()
                              ->getCurrentScene()
                              ->getScene()
                              ->getCurrentPreviewCamera();
    TRect subcameraRect(currCamera->getInterestRect());

    /*-- Viewer上でサブカメラが指定されていない状態でFxPreviewをした場合 --*/
    if (subcameraRect.getLx() == 0 || subcameraRect.getLy() == 0)
      isFullRender = true;
  }

  // Build all the viewRects to be calculated. They will be computed on
  // consecutive
  // render operations.
  // NOTE: For now, we'll perform a simplicistic solution - coalesce all the
  // flipbooks'
  // viewrects and launch just one render.
  TRectD previewRectD;
  m_rectUnderRender = TRect();

  int shrinkX = m_renderSettings.m_shrinkX;
  int shrinkY = m_renderSettings.m_shrinkY;

  if (!isFullRender) {
    // For each opened flipbook
    /*-- 開いているFlipbookの表示範囲を足しこんでいく --*/
    std::set<FlipBook *>::iterator it;
    for (it = m_flipbooks.begin(); it != m_flipbooks.end(); ++it) {
      // Only visible flipbooks are considered
      if ((*it)->isVisible())
        // Retrieve the flipbook's viewRect. Observe that this image geometry
        // is intended in shrinked image reference, and assumes that the camera
        // center
        // lies at coords (0.0, 0.0).
        previewRectD += (*it)->getPreviewedImageGeometry();
    }

    // Pass from shrinked to standard image geometry
    /*-- いったんShrinkを元に戻す --*/
    previewRectD.x0 *= shrinkX;
    previewRectD.y0 *= shrinkY;
    previewRectD.x1 *= shrinkX;
    previewRectD.y1 *= shrinkY;

    // Now, the viewer will center the subcamera's raster instead than camera's.
    // So, we have
    // to correct the previewRectD by the stored displacement.
    previewRectD += m_subcameraDisplacement;

    /*-- 表示範囲と計算範囲の共通部分を得る --*/
    previewRectD *= m_renderArea;
  } else
    previewRectD = m_renderArea;

  // Ensure that rect has the same pixel geometry as the preview camera
  /*-- 再度Shrink --*/
  previewRectD -= m_cameraPos;
  previewRectD.x0 = previewRectD.x0 / shrinkX;
  previewRectD.y0 = previewRectD.y0 / shrinkY;
  previewRectD.x1 = previewRectD.x1 / shrinkX;
  previewRectD.y1 = previewRectD.y1 / shrinkY;

  // Now, pass to m_cameraRes-relative coordinates
  /*-- 計算エリア基準の座標 → カメラ基準の座標 --*/
  TPointD shrinkedRelPos((m_renderArea.x0 - m_cameraPos.x) / shrinkX,
                         (m_renderArea.y0 - m_cameraPos.y) / shrinkY);
  previewRectD -= shrinkedRelPos;

  previewRectD.x0 = tfloor(previewRectD.x0);
  previewRectD.y0 = tfloor(previewRectD.y0);
  previewRectD.x1 = tceil(previewRectD.x1);
  previewRectD.y1 = tceil(previewRectD.y1);

  /*-- 表示しなくてはいけないRect --*/
  QRect qViewRect(previewRectD.x0, previewRectD.y0, previewRectD.getLx(),
                  previewRectD.getLy());
  /*-- 表示しなくてはいけないRectから、既に計算済みの範囲を引く ＝
   * 新たに計算が必要な領域 --*/
  QRegion viewRectRegionToRender(
      QRegion(qViewRect).subtracted(m_overallRenderedRegion));

  // If the rect to render has already been calculated, continue.
  /*-- 新たに計算が必要な領域が無ければReturn --*/
  if (viewRectRegionToRender.isEmpty()) return;

  // Retrieve the minimal box containing the region yet to be rendered
  /*-- 新たに計算が必要な領域を含む最小のRectを得る --*/
  QRect boxRectToRender(viewRectRegionToRender.boundingRect());

  /*-- 計算中のRectに登録する --*/
  m_rectUnderRender = toTRect(boxRectToRender);
  /*-- カメラ基準の座標 → 計算エリア基準の座標 --*/
  previewRectD = toTRectD(boxRectToRender) + m_cameraPos + shrinkedRelPos;
  /*-- RenderAreaをセット --*/
  m_renderPort.setRenderArea(previewRectD);
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::refreshViewRects(bool rebuild) {
  if (suspendedRendering) return;

  if (m_flipbooks.empty()) return;

  // Stop any currently running render process. It *should not* be necessary to
  // wait for complete stop.
  // WARNING: This requires that different rendering instances are
  // simultaneously
  // supported in a single TRenderer...!! We're not currently supporting this...
  {
    // NOTE: stopRendering(true) LOOPS and may trigger events which delete this
    // very
    // render instance. So we have to watch inside the manager to see if this is
    // still
    // alive... The following should be removed using stopRendering(false)...
    // NOTE: The same problem imposes that refreshViewRects() is not invoked
    // directly
    // when iterating the previewInstances map - we've used a signal-slot
    // connection for this.
    unsigned long fxId = m_fx->getIdentifier();

    m_renderer.stopRendering(true);  // Wait until we've finished

    QMap<unsigned long, PreviewFxInstance *> &previewInstances =
        PreviewFxManager::instance()->m_previewInstances;
    if (previewInstances.find(fxId) == previewInstances.end()) return;
  }

  if (suspendedRendering) return;

  updatePreviewRect();
  updateProgressBarStatus();
  updateFlipbooks();

  startRender(rebuild);
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::startRender(bool rebuild) {
  if (m_start > m_end) return;

  // Build the rendering initial frame
  /*-- m_initialFrameに最初に計算するフレーム番号を格納 --*/
  updateInitialFrame();

  m_renderFailed = false;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  // Fill the production-specific infos (threads count and tile size)
  TOutputProperties *properties =
      scene->getProperties()->getPreviewProperties();

  // Update the threads number
  const int procCount       = TSystem::getProcessorCount();
  const int threadCounts[3] = {1, procCount / 2, procCount};

  int index = properties->getThreadIndex();
  m_renderer.setThreadsCount(threadCounts[index]);

  // Build raster granularity size
  index = properties->getMaxTileSizeIndex();

  const int maxTileSizes[4] = {
      (std::numeric_limits<int>::max)(), TOutputProperties::LargeVal,
      TOutputProperties::MediumVal, TOutputProperties::SmallVal};

  int oldMaxTileSize             = m_renderSettings.m_maxTileSize;
  m_renderSettings.m_maxTileSize = maxTileSizes[index];

  // Initialize the vector of TRenderer::RenderData to be rendered. The 'frame'
  // data
  // should be inserted first.
  RenderDataVector *renderDatas = new RenderDataVector;
  int i;
  for (i = m_initFrame; i <= m_end; i += m_step)
    addRenderData(*renderDatas, scene, i, rebuild);
  for (i = m_start; i < m_initFrame; i += m_step)
    addRenderData(*renderDatas, scene, i, rebuild);

  // Restore the original max tile size
  m_renderSettings.m_maxTileSize = oldMaxTileSize;

  // Retrieve the renderId
  unsigned long renderId = m_renderer.nextRenderId();
  std::string contextName("PFX");
  contextName += std::to_string(m_fx->getIdentifier());
  TPassiveCacheManager::instance()->setContextName(renderId, contextName);

  // Finally, start rendering all frames which were not found in cache
  m_renderer.startRendering(renderDatas);
}

//----------------------------------------------------------------------------------------

void PreviewFxRenderPort::onRenderRasterStarted(
    const TRenderPort::RenderData &renderData) {
  m_owner->onRenderRasterStarted(renderData);
}

//----------------------------------------------------------------------------------------

void PreviewFxRenderPort::onRenderRasterCompleted(
    const RenderData &renderData) {
  /*-- Do not show the result if canceled while rendering --*/
  if (renderData.m_info.m_isCanceled && *renderData.m_info.m_isCanceled) {
    // set m_renderFailed to true in order to prevent updating
    // m_overallRenderedRegion at PreviewFxInstance::onRenderFinished().
    m_owner->m_renderFailed = true;
    return;
  }

  m_owner->onRenderRasterCompleted(renderData);
}

//----------------------------------------------------------------------------------------

void PreviewFxRenderPort::onRenderFailure(const RenderData &renderData,
                                          TException &e) {
  m_owner->onRenderFailure(renderData, e);
}

//----------------------------------------------------------------------------------------

void PreviewFxRenderPort::onRenderFinished(bool isCanceled) {
  m_owner->onRenderFinished(isCanceled);
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::onRenderRasterStarted(
    const TRenderPort::RenderData &renderData) {
  PreviewFxManager::instance()->emitStartedFrame(m_fx->getIdentifier(),
                                                 renderData);
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::onRenderRasterCompleted(
    const TRenderPort::RenderData &renderData) {
  PreviewFxManager::instance()->emitRenderedFrame(m_fx->getIdentifier(),
                                                  renderData);
}

//----------------------------------------------------------------------------------------

// Update the progress bar status to show the frame has started
void PreviewFxInstance::doOnRenderRasterStarted(
    const TRenderPort::RenderData &renderData) {
  unsigned int i, size = renderData.m_frames.size();
  for (i = 0; i < size; ++i)
    // Update the pb status for each cluster's frame
    m_pbStatus[(renderData.m_frames[i] - m_start) / m_step] =
        FlipSlider::PBFrameStarted;

  /*-- 計算中の赤枠を表示する --*/
  std::set<FlipBook *>::iterator it;
  for (it = m_flipbooks.begin(); it != m_flipbooks.end(); ++it)
    (*it)->setIsRemakingPreviewFx(true);

  updateFlipbooks();
}

//----------------------------------------------------------------------------------------

// Show the rendered frame if it is some flipbook's current
void PreviewFxInstance::doOnRenderRasterCompleted(
    const TRenderPort::RenderData &renderData) {
  std::string cacheId(getCacheId(m_fx, renderData.m_frames[0]));

  TRasterImageP ri(TImageCache::instance()->get(cacheId, true));
  TRasterP ras;
  if (ri)
    ras = ri->getRaster();
  else
    ras = 0;

  TRasterP rasA = renderData.m_rasA;
  TRasterP rasB = renderData.m_rasB;

  // Linear Color Space -> sRGB
  if (rasA->isLinear()) {
    TRop::tosRGB(rasA, m_renderSettings.m_colorSpaceGamma);
    if (m_renderSettings.m_stereoscopic)
      TRop::tosRGB(rasB, m_renderSettings.m_colorSpaceGamma);
  }

  /*-- 16bpcで計算された場合、結果をDitheringする --*/
  // dither the 16bpc image IF the "30bit display" preference option is OFF
  if ((rasA->getPixelSize() == 8 || rasA->getPixelSize() == 16) &&
      !Preferences::instance()->is30bitDisplayEnabled())  // render in 64 bits
  {
    TRaster32P auxA(rasA->getLx(), rasA->getLy());
    TRop::convert(auxA, rasA);  // dithering
    rasA = auxA;
    if (m_renderSettings.m_stereoscopic) {
      assert(rasB);
      TRaster32P auxB(rasB->getLx(), rasB->getLy());
      TRop::convert(auxB, rasB);  // dithering
      rasB = auxB;
    }
  }

  if (!ras || (ras->getSize() != m_cameraRes)) {
    TImageCache::instance()->remove(cacheId);

    // Create the raster at camera resolution
    ras = rasA->create(m_cameraRes.lx, m_cameraRes.ly);
    ras->clear();
    ri = TRasterImageP(ras);
  }

  // Finally, copy the rendered raster over the cached one
  TRect rectUnderRender(
      m_rectUnderRender);  // Extract may MODIFY IT! E.g. with shrinks..!
  ras = ras->extract(rectUnderRender);
  if (ras) {
    if (m_renderSettings.m_stereoscopic) {
      assert(rasB);
      TRop::makeStereoRaster(rasA, rasB);
    }
    ras->copy(rasA);
  }
  // Submit the image to the cache, for all cluster's frames
  unsigned int i, size = renderData.m_frames.size();
  for (i = 0; i < size; ++i) {
    int frame = renderData.m_frames[i];
    TImageCache::instance()->add(getCacheId(m_fx, frame), ri);

    // Update the pb status
    int pbIndex = (frame - m_start) / m_step;
    if (pbIndex >= 0 && pbIndex < (int)m_pbStatus.size())
      m_pbStatus[pbIndex] = FlipSlider::PBFrameFinished;

    // Update the frame-specific rendered region
    std::map<int, FrameInfo>::iterator jt = m_frameInfos.find(frame);
    assert(jt != m_frameInfos.end());
    jt->second.m_renderedRegion += toQRect(m_rectUnderRender);

    std::set<FlipBook *>::iterator it;
    int renderedFrame = frame + 1;
    for (it = m_flipbooks.begin(); it != m_flipbooks.end(); ++it)
      if ((*it)->getCurrentFrame() == renderedFrame)
        (*it)->showFrame(renderedFrame);
  }

  updateFlipbooks();
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::onRenderFailure(
    const TRenderPort::RenderData &renderData, TException &e) {
  m_renderFailed = true;

  // Update each frame status
  unsigned int i, size = renderData.m_frames.size();
  for (i = 0; i < size; ++i) {
    int frame = renderData.m_frames[i];

    // Update the pb status
    int pbIndex = (frame - m_start) / m_step;
    if (pbIndex >= 0 && pbIndex < (int)m_pbStatus.size())
      m_pbStatus[pbIndex] = FlipSlider::PBFrameNotStarted;
  }
}

//----------------------------------------------------------------------------------------

void PreviewFxInstance::onRenderFinished(bool isCanceled) {
  // Update the rendered region
  if (!m_renderFailed && !isCanceled)
    m_overallRenderedRegion += toQRect(m_rectUnderRender);

  /*-- 計算中の赤枠の表示を消す --*/
  std::set<FlipBook *>::iterator it;
  for (it = m_flipbooks.begin(); it != m_flipbooks.end(); ++it)
    (*it)->setIsRemakingPreviewFx(false);
}

//=======================================================================================================

//=========================
//    PreviewFxManager
//-------------------------

PreviewFxManager::PreviewFxManager() : QObject() {
  TApp *app = TApp::instance();
  qRegisterMetaType<unsigned long>("unsigned long");
  qRegisterMetaType<TRenderPort::RenderData>("TRenderPort::RenderData");

  /*-- Rendering終了時、各RenderPortからEmit → Flipbookの更新を行う --*/
  connect(this, SIGNAL(renderedFrame(unsigned long, TRenderPort::RenderData)),
          this, SLOT(onRenderedFrame(unsigned long, TRenderPort::RenderData)));
  /*-- Rendering開始時、各RenderPortからEmit →
   * Flipbookのプログレスバーのステータスを「計算中」にする --*/
  connect(this, SIGNAL(startedFrame(unsigned long, TRenderPort::RenderData)),
          this, SLOT(onStartedFrame(unsigned long, TRenderPort::RenderData)));

  // connect(app->getPaletteController()->getCurrentPalette(),
  // SIGNAL(colorStyleChangedOnMouseRelease()),SLOT(onLevelChanged()));
  // connect(app->getPaletteController()->getCurrentPalette(),
  // SIGNAL(paletteChanged()),   SLOT(onLevelChanged()));
  connect(app->getPaletteController()->getCurrentLevelPalette(),
          SIGNAL(colorStyleChangedOnMouseRelease()), SLOT(onLevelChanged()));
  connect(app->getPaletteController()->getCurrentLevelPalette(),
          SIGNAL(paletteChanged()), SLOT(onLevelChanged()));

  connect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()), this,
          SLOT(onLevelChanged()));
  connect(app->getCurrentFx(), SIGNAL(fxChanged()), this, SLOT(onFxChanged()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
          SLOT(onXsheetChanged()));
  connect(app->getCurrentObject(), SIGNAL(objectChanged(bool)), this,
          SLOT(onObjectChanged(bool)));

  /*-- 上記の on○○Changed() は、全て refreshViewRects をEmitしている。
          → これまでの計算を止め、新たにstartRenderをする。
          （Qt::QueuedConnection
  は、イベントループの手が空いた時に初めてSLOTを呼ぶ、ということ）
  --*/
  // Due to current implementation of PreviewFxInstance::refreshViewRects().
  connect(this, SIGNAL(refreshViewRects(unsigned long)), this,
          SLOT(onRefreshViewRects(unsigned long)), Qt::QueuedConnection);

  previewerInstance = this;
}

//-----------------------------------------------------------------------------

PreviewFxManager::~PreviewFxManager() {}

//-----------------------------------------------------------------------------

PreviewFxManager *PreviewFxManager::instance() {
  static PreviewFxManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

FlipBook *PreviewFxManager::showNewPreview(TFxP fx, bool forceFlipbook) {
  if (!fx) return 0;

  /*-- fxIdは、Fxの作成ごとに１つずつインクリメントして割り振られる数字 --*/
  unsigned long fxId                 = fx->getIdentifier();
  PreviewFxInstance *previewInstance = 0;

  /*-- PreviewFxInstanceをFxごとに作成する --*/
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it == m_previewInstances.end()) {
    TXsheet *xsh    = TApp::instance()->getCurrentXsheet()->getXsheet();
    previewInstance = new PreviewFxInstance(fx, xsh);
    m_previewInstances.insert(fxId, previewInstance);
  } else {
    previewInstance = it.value();
    /*-- 以前PreviewしたことのあるFxを、再度Previewしたとき --*/
    if (!forceFlipbook &&
        !Preferences::instance()->previewAlwaysOpenNewFlipEnabled()) {
      // Search the first visible flipbook to be raised. If not found, add a new
      // one.
      /*--
       * そのFxに関連付けられたFlipbookがあり、かつVisibleな場合は、reset()で再計算
       * --*/
      std::set<FlipBook *> &flipbooks = previewInstance->m_flipbooks;
      std::set<FlipBook *>::iterator jt;
      for (jt = flipbooks.begin(); jt != flipbooks.end(); ++jt)
        if ((*jt)->isVisible()) {
          reset(fx);  // Also recalculate the preview
          (*jt)->parentWidget()->raise();
          return 0;
        }
    }
  }

  FlipBook *result = 0;
  /*-- resultに必要なFlipbookを格納し、setLevelをする --*/
  previewInstance->addFlipbook(result);

  /*-- Flipbookのクローン時以外は forceFlipbookがfalse --*/
  if (!forceFlipbook) /*-- startRenderを実行 --*/
    previewInstance->refreshViewRects();

  return result;
}

//-----------------------------------------------------------------------------
/*! return true if the preview fx instance for specified fx is with sub-camera
 * activated
 */

bool PreviewFxManager::isSubCameraActive(TFxP fx) {
  if (!fx) return false;

  unsigned long fxId = fx->getIdentifier();
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it == m_previewInstances.end()) return false;

  return it.value()->isSubCameraActive();
}

//-----------------------------------------------------------------------------

void PreviewFxManager::refreshView(TFxP fx) {
  if (!fx) return;

  unsigned long fxId = fx->getIdentifier();
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it == m_previewInstances.end()) return;

  it.value()->refreshViewRects();
}

//-----------------------------------------------------------------------------

//! This slot is necessary to prevent problems with current implementation of
//! the
//! event-looping PreviewFxInstance::refreshViewRects function.
void PreviewFxManager::onRefreshViewRects(unsigned long id) {
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(id);
  if (it != m_previewInstances.end()) it.value()->refreshViewRects();
}

//-----------------------------------------------------------------------------

void PreviewFxManager::unfreeze(FlipBook *flipbook) {
  TFxP fx(flipbook->getPreviewedFx());
  if (!fx) return;

  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fx->getIdentifier());
  if (it == m_previewInstances.end()) return;

  PreviewFxInstance *previewInstance = it.value();
  std::set<FlipBook *> &frozenFlips  = previewInstance->m_frozenFlips;

  std::set<FlipBook *>::iterator jt = frozenFlips.find(flipbook);
  if (jt == frozenFlips.end()) return;

  // Re-attach to the preview instance
  {
    frozenFlips.erase(jt);

    // Before attaching, remove any old flipbook level from the cache
    flipbook->clearCache();

    // Also any associated pb status
    delete flipbook->getProgressBarStatus();
    flipbook->setProgressBarStatus(NULL);

    previewInstance->addFlipbook(flipbook);

    // recompute frames, if necessary (call the same process as
    // PreviewFxManager::onXsheetChanged())
    previewInstance->updateRenderSettings();
    previewInstance->updateCamera();
    previewInstance->updateFrameRange();
    previewInstance->updateFlipbookTitles();
    previewInstance->updateAliases();

    previewInstance->refreshViewRects();
  }
}

//-----------------------------------------------------------------------------

//! Observe that detached flipbooks which maintain the previewed images also
//! maintain the internal reference
//! to the previewed fx returned by the FlipBook::getPreviewedFx() method, so
//! the flipbook may be re-attached
//! (ie un-freezed) to the same preview fx.
void PreviewFxManager::freeze(FlipBook *flipbook) {
  // Retrieve its previewed fx
  TFxP fx(flipbook->getPreviewedFx());
  if (!fx) return;

  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fx->getIdentifier());
  if (it == m_previewInstances.end()) return;

  PreviewFxInstance *previewInstance = it.value();

  // First off, detach the flipbook from the preview instance and
  // insert it among the instance's frozen ones
  previewInstance->detachFlipbook(flipbook);
  previewInstance->m_frozenFlips.insert(flipbook);

  // Then, perform the level copy
  {
    std::string levelName("freezed" + std::to_string(flipbook->getPoolIndex()) +
                          ".noext");
    int i;

    // Clone the preview images
    for (i = previewInstance->m_start; i <= previewInstance->m_end;
         i += previewInstance->m_step) {
      TImageP cachedImage =
          TImageCache::instance()->get(getCacheId(fx, i), false);
      if (cachedImage)
        TImageCache::instance()->add(levelName + std::to_string(i),
                                     cachedImage->cloneImage());
    }

    // Associate a level with the cached images
    TLevelP freezedLevel;
    freezedLevel->setName(levelName);
    for (i = 0; i < previewInstance->m_level->getFrameCount(); ++i)
      freezedLevel->setFrame(TFrameId(i), 0);
    flipbook->setLevel(fx.getPointer(), previewInstance->m_xsheet.getPointer(),
                       freezedLevel.getPointer(), 0,
                       previewInstance->m_start + 1, previewInstance->m_end + 1,
                       previewInstance->m_step, flipbook->getCurrentFrame(),
                       previewInstance->m_snd.getPointer());

    // Also, the associated PB must be cloned
    std::vector<UCHAR> *newPBStatuses = new std::vector<UCHAR>;
    *newPBStatuses                    = previewInstance->m_pbStatus;
    flipbook->setProgressBarStatus(newPBStatuses);

    // Traverse the PB: frames under rendering must be signed as uncompleted
    std::vector<UCHAR>::iterator it;
    for (it = newPBStatuses->begin(); it != newPBStatuses->end(); ++it)
      if (*it == FlipSlider::PBFrameStarted)
        *it = FlipSlider::PBFrameNotStarted;
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::detach(FlipBook *flipbook) {
  // Retrieve its previewed fx
  TFxP fx(flipbook->getPreviewedFx());
  if (!fx) return;

  // Search the flip among attached ones
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fx->getIdentifier());
  if (it == m_previewInstances.end()) return;

  PreviewFxInstance *previewInstance = it.value();

  // Detach the flipbook (does nothing if flipbook is frozen)
  previewInstance->detachFlipbook(flipbook);

  // Eventually, it could be in frozens; detach it from there too
  std::set<FlipBook *> &frozenFlips = previewInstance->m_frozenFlips;

  std::set<FlipBook *>::iterator jt = frozenFlips.find(flipbook);
  if (jt != frozenFlips.end()) {
    flipbook->clearCache();
    delete flipbook->getProgressBarStatus();
    frozenFlips.erase(jt);
  }

  // Finally, delete the preview instance if no flipbook (active or frozen)
  // remain
  if (previewInstance->m_flipbooks.empty() &&
      previewInstance->m_frozenFlips.empty()) {
    delete it.value();
    m_previewInstances.erase(it);
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::onLevelChanged() {
  if (m_previewInstances.size()) {
    // Build the level name as an alias keyword. All cache images associated
    // with an alias containing the level name will be updated.
    TXshLevel *xl = TApp::instance()->getCurrentLevel()->getLevel();
    if (!xl) return;
    std::string aliasKeyword;
    TFilePath fp = xl->getPath();
    aliasKeyword = ::to_string(fp.withType(""));

    QMap<unsigned long, PreviewFxInstance *>::iterator it;
    for (it = m_previewInstances.begin(); it != m_previewInstances.end();
         ++it) {
      it.value()->updateAliasKeyword(aliasKeyword);
      emit refreshViewRects(it.key());
      // it.value()->refreshViewRects();
    }
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::onFxChanged() {
  // Examinate all RenderInstances for ancestors of current fx
  if (m_previewInstances.size()) {
    TFxP fx = TApp::instance()->getCurrentFx()->getFx();

    QMap<unsigned long, PreviewFxInstance *>::iterator it;
    for (it = m_previewInstances.begin(); it != m_previewInstances.end(); ++it)
    // if(areAncestorAndDescendant(it.value()->m_fx, fx))  //Currently not
    // trespassing sub-xsheet boundaries
    {
      // in case the flipbook is frozen
      if (it.value()->m_flipbooks.empty()) continue;
      it.value()->updateAliases();
      emit refreshViewRects(it.key());
      // it.value()->refreshViewRects();
    }
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::onXsheetChanged() {
  // Update all rendered frames, if necessary
  if (m_previewInstances.size()) {
    QMap<unsigned long, PreviewFxInstance *>::iterator it;
    for (it = m_previewInstances.begin(); it != m_previewInstances.end();
         ++it) {
      // in case the flipbook is frozen
      if (it.value()->m_flipbooks.empty()) continue;
      it.value()->updateRenderSettings();
      it.value()->updateCamera();
      it.value()->updateFrameRange();
      it.value()->updateFlipbookTitles();
      it.value()->updateAliases();
      emit refreshViewRects(it.key());
      // it.value()->refreshViewRects();
    }
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::onObjectChanged(bool isDragging) {
  if (isDragging) return;
  // Update all rendered frames, if necessary
  if (m_previewInstances.size()) {
    QMap<unsigned long, PreviewFxInstance *>::iterator it;
    for (it = m_previewInstances.begin(); it != m_previewInstances.end();
         ++it) {
      // in case the flipbook is frozen
      if (it.value()->m_flipbooks.empty()) continue;
      it.value()->updateFrameRange();
      it.value()->updateFlipbookTitles();
      it.value()->updateAliases();
      emit refreshViewRects(it.key());
      // it.value()->refreshViewRects();
    }
  }
}

//-----------------------------------------------------------------------------
/*--
 * 既にPreviewしたことがあり、Flipbookが開いているFxを再度Previewしたときに実行される
 * --*/
void PreviewFxManager::reset(TFxP fx) {
  if (!fx) return;

  unsigned long fxId = fx->getIdentifier();
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it == m_previewInstances.end()) return;

  it.value()->m_renderer.stopRendering(true);

  // stopRendering(true) LOOPS and may destroy the preview instance. Recheck for
  // its presence
  it = m_previewInstances.find(fxId);
  if (it != m_previewInstances.end()) {
    it.value()->reset();
    it.value()->refreshViewRects();
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::reset(TFxP fx, int frame) {
  if (!fx) return;

  unsigned long fxId = fx->getIdentifier();
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it == m_previewInstances.end()) return;

  if (it.value()->m_start > it.value()->m_end) return;

  it.value()->m_renderer.stopRendering(true);

  // stopRendering(true) LOOPS and may destroy the preview instance. Recheck for
  // its presence
  it = m_previewInstances.find(fxId);
  if (it != m_previewInstances.end()) {
    it.value()->reset(frame);
    it.value()->refreshViewRects();
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::reset(bool detachFlipbooks) {
  // Hard copy the instances pointers
  QMap<unsigned long, PreviewFxInstance *> previewInstances =
      m_previewInstances;

  QMap<unsigned long, PreviewFxInstance *>::iterator it;
  for (it = previewInstances.begin(); it != previewInstances.end(); ++it) {
    // Just like the above, stopRendering(true) event-LOOPS...
    it.value()->m_renderer.stopRendering(true);

    if (m_previewInstances.find(it.key()) == m_previewInstances.end()) continue;

    if (detachFlipbooks) {
      // Reset all associated flipbooks
      PreviewFxInstance *previewInstance = it.value();

      // Hard copy, since detach manipulates the original
      std::set<FlipBook *> flipbooks = previewInstance->m_flipbooks;

      std::set<FlipBook *>::iterator jt;
      for (jt = flipbooks.begin(); jt != flipbooks.end(); ++jt) {
        // Detach and reset the flipbook
        (*jt)->reset();  // The detachment happens in here
      }

      /*- Frozen
       * Flipがひとつも無い場合には、この時点でpreviewInstanceが除外されている
       * -*/
      if (m_previewInstances.find(it.key()) == m_previewInstances.end())
        continue;

      // Same for frozen ones

      if (!previewInstance->m_frozenFlips.empty() &&
          previewInstance->m_frozenFlips.size() < 20) {
        std::set<FlipBook *> frozenFlips = previewInstance->m_frozenFlips;
        for (jt = frozenFlips.begin(); jt != frozenFlips.end(); ++jt) {
          // Detach and reset the flipbook
          (*jt)->reset();  // The detachment happens in here
        }
      }

      // The Preview instance should have been deleted at this point (due to
      // flipbook detachments)
      assert(m_previewInstances.find(it.key()) == m_previewInstances.end());
    } else {
      it.value()->reset();
      it.value()->refreshViewRects();
    }
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::emitStartedFrame(
    unsigned long fxId, const TRenderPort::RenderData &renderData) {
  emit startedFrame(fxId, renderData);
}

//-----------------------------------------------------------------------------

void PreviewFxManager::emitRenderedFrame(
    unsigned long fxId, const TRenderPort::RenderData &renderData) {
  emit renderedFrame(fxId, renderData);
}

//-----------------------------------------------------------------------------

void PreviewFxManager::onStartedFrame(unsigned long fxId,
                                      TRenderPort::RenderData renderData) {
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it != m_previewInstances.end()) {
    // Invoke the corresponding function. This happens in the MAIN THREAD
    it.value()->doOnRenderRasterStarted(renderData);
  }
}

//-----------------------------------------------------------------------------

void PreviewFxManager::onRenderedFrame(unsigned long fxId,
                                       TRenderPort::RenderData renderData) {
  QMap<unsigned long, PreviewFxInstance *>::iterator it =
      m_previewInstances.find(fxId);
  if (it != m_previewInstances.end()) {
    // Invoke the corresponding function. This happens in the MAIN THREAD
    it.value()->doOnRenderRasterCompleted(renderData);
  }
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
void PreviewFxManager::suspendRendering(bool suspend) {
  suspendedRendering = suspend;
  if (suspend && previewerInstance) {
    QMap<unsigned long, PreviewFxInstance *>::iterator it;
    for (it = previewerInstance->m_previewInstances.begin();
         it != previewerInstance->m_previewInstances.end(); ++it) {
      it.value()->m_renderer.stopRendering(true);
    }
  }
}
