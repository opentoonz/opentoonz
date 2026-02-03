

// Rendering components
#include "toonz/multimediarenderer.h"
#include "toonz/movierenderer.h"
#include "trenderer.h"

// Scene structures
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"

// Fxs tree decomposition
#include "toonz/scenefx.h"
#include "toonz/tcolumnfx.h"

// Idle processing
#include <QEventLoop>

//----------------------------------------------------------------------------

//  Local stuff

namespace {
std::wstring removeSpaces(const std::wstring &str) {
  std::wstring result;
  std::wstring::size_type a = 0, b;
  while ((b = str.find_first_of(L" ", a)) != std::wstring::npos) {
    result += str.substr(a, b - a);
    a = b + 1;
  }
  result += str.substr(a, std::wstring::npos);
  return result;
}
}

//=========================================================
//
//    MultimediaRenderer::Imp   class
//
//---------------------------------------------------------

class MultimediaRenderer::Imp final : public MovieRenderer::Listener,
                                      public TSmartObject {
public:
  ToonzScene *m_scene;
  TFilePath m_fp;
  int m_threadCount;
  bool m_cacheResults;

  // Output movie frames. May be remapped from rendered ones.
  double m_xDpi, m_yDpi;

  TRenderSettings m_renderSettings;

  std::vector<MultimediaRenderer::Listener *> m_listeners;

  bool m_precomputingEnabled;
  bool m_canceled;

  int m_currentFx;
  std::set<double>::iterator m_currentFrame;
  TRenderer *m_currentTRenderer;

  TFxSet m_fxsToRender;
  std::set<double> m_framesToRender;

  QEventLoop m_eventLoop;

  int m_multimediaMode;

  Imp(ToonzScene *scene, const TFilePath &moviePath, int multimediaMode,
      int threadCount, bool cacheResults);

  ~Imp();

  void scanSceneForRenderNodes();
  void scanSceneForColumns();
  void scanSceneForLayers();
  bool scanColsRecursive(TFx *fx);
  TColumnFx *searchColumn(TFxP fx);
  TFxP addPostProcessing(TFxP fx, TFxP postProc);
  void addPostProcessingRecursive(TFxP fx, TFxP postProc);

  void start();

  bool onFrameCompleted(int frame) override;
  bool onFrameFailed(int frame, TException &e) override;
  void onSequenceCompleted(const TFilePath &fp) override;
  void onRenderCompleted();
};

//---------------------------------------------------------

MultimediaRenderer::Imp::Imp(ToonzScene *scene, const TFilePath &moviePath,
                             int multimediaMode, int threadCount,
                             bool cacheResults)
    : m_scene(scene)
    , m_fp(moviePath)
    , m_threadCount(threadCount)
    , m_cacheResults(cacheResults)
    , m_xDpi(72)
    , m_yDpi(72)
    , m_renderSettings()
    , m_listeners()
    , m_precomputingEnabled(true)
    , m_canceled(false)
    , m_currentFx(0)
    , m_currentFrame()
    , m_multimediaMode(multimediaMode) {
  // Retrieve all fx nodes to be rendered in this process.
  scanSceneForRenderNodes();
}

//---------------------------------------------------------

MultimediaRenderer::Imp::~Imp() {}

//---------------------------------------------------------

void MultimediaRenderer::Imp::scanSceneForRenderNodes() {
  switch (m_multimediaMode) {
  case COLUMNS:
    scanSceneForColumns();
    break;
  case LAYERS:
    scanSceneForLayers();
    break;
  default:
    assert(0);
  }
}

//---------------------------------------------------------

//! Retrieve the first level of scene columns, climb their paths
//! along unary fxs, and then build the scene fxs.
void MultimediaRenderer::Imp::scanSceneForColumns() {
  // Retrieve the terminal fxs (ie, fxs which are implicitly
  // connected to the xsheet one)
  TXsheet *xsh = m_scene->getXsheet();
  TFxSet *fxs  = xsh->getFxDag()->getTerminalFxs();

  // Launch the recursive column climber procedure for each
  for (int i = 0; i < fxs->getFxCount(); ++i) {
    TFx *fx = fxs->getFx(i);
    if (!fx) continue;
    bool isItARenderFx = scanColsRecursive(fx);
    if (isItARenderFx) m_fxsToRender.addFx(fx);
  }
}

//---------------------------------------------------------

//! Returns true if the passed fx is a valid column representant
bool MultimediaRenderer::Imp::scanColsRecursive(TFx *fx) {
  // Columns should return true - they are column representant
  if (dynamic_cast<TColumnFx *>(fx)) return true;

  // Search columns in every port
  bool isChildAnFxRepres;
  for (int i = 0; i < fx->getInputPortCount(); ++i) {
    TFx *childFx = fx->getInputPort(i)->getFx();
    if (!childFx) continue;
    isChildAnFxRepres = scanColsRecursive(childFx);
    if (isChildAnFxRepres && fx->getInputPortCount() > 1)
      m_fxsToRender.addFx(childFx);
  }

  if (isChildAnFxRepres && fx->getInputPortCount() == 1) return true;
  return false;
}

//---------------------------------------------------------

//! Build the scene fx for each node below the xsheet one.
//! Remember that left xsheet ports must not be expanded.
void MultimediaRenderer::Imp::scanSceneForLayers() {
  // Retrieve the terminal fxs (ie, fxs which are implicitly
  // connected to the xsheet one)
  TXsheet *xsh = m_scene->getXsheet();
  TFxSet *fxs  = xsh->getFxDag()->getTerminalFxs();

  // Examine all of them and - eventually - expand left xsheet
  // ports (ie fx nodes who allow implicit overlaying)
  for (int i = 0; i < fxs->getFxCount(); ++i) {
    TFx *fx = fxs->getFx(i);
    TFxPort *leftXSheetPort;

  retry:

    if (!fx) continue;
    leftXSheetPort = fx->getXsheetPort();

    if (!leftXSheetPort) {
      m_fxsToRender.addFx(fx);
      continue;
    }

    // If the leftXSheetPort is not connected, retry on port 0
    if (leftXSheetPort->isConnected())
      m_fxsToRender.addFx(fx);
    else {
      fx = fx->getInputPort(0)->getFx();
      goto retry;
    }
  }
}

//---------------------------------------------------------

TColumnFx *MultimediaRenderer::Imp::searchColumn(TFxP fx) {
  // Descend ports 0 until a TColumnFx is found. Then, output its number.
  TFx *currFx      = fx.getPointer();
  TColumnFx *colFx = dynamic_cast<TColumnFx *>(currFx);
  while (!colFx) {
    if (fx->getInputPortCount() <= 0) break;

    currFx = currFx->getInputPort(0)->getFx();

    if (!currFx) break;
    colFx = dynamic_cast<TColumnFx *>(currFx);
  }

  return colFx;
}

//---------------------------------------------------------

TFxP MultimediaRenderer::Imp::addPostProcessing(TFxP fx, TFxP postProc) {
  if (dynamic_cast<TXsheetFx *>(postProc.getPointer())) return fx;

  // Clone the postProcessing tree and substitute recursively
  postProc = postProc->clone(true);
  addPostProcessingRecursive(fx, postProc);

  return postProc;
}

//---------------------------------------------------------

void MultimediaRenderer::Imp::addPostProcessingRecursive(TFxP fx,
                                                         TFxP postProc) {
  if (!postProc) return;

  int i, count = postProc->getInputPortCount();
  for (i = 0; i < count; ++i) {
    TFxPort *port = postProc->getInputPort(i);
    TFx *childFx  = port->getFx();

    if (dynamic_cast<TXsheetFx *>(childFx))
      port->setFx(fx.getPointer());
    else
      addPostProcessingRecursive(fx, childFx);
  }
}

//---------------------------------------------------------

void MultimediaRenderer::Imp::start() {
  // Retrieve some useful infos
  double stretchTo   = m_renderSettings.m_timeStretchTo;
  double stretchFrom = m_renderSettings.m_timeStretchFrom;

  double timeStretchFactor = stretchFrom / stretchTo;
  bool fieldRendering =
      m_renderSettings.m_fieldPrevalence != TRenderSettings::NoField;

  std::wstring modeStr;
  switch (m_multimediaMode) {
  case COLUMNS:
    modeStr = L"_col";
    break;
  case LAYERS:
    modeStr = L"_lay";
    break;
  default:
    assert(0);
  }

  // Build the post processing fxs tree
  std::vector<TFxP> postFxs(m_framesToRender.size());

  std::set<double>::iterator jt;
  int j;
  for (j = 0, jt = m_framesToRender.begin(); jt != m_framesToRender.end();
       ++j, ++jt)
    postFxs[j] =
        buildPostSceneFx(m_scene, *jt, m_renderSettings.m_shrinkX,
                         false);  // Adds camera and camera dpi transforms

  // For each node to be rendered
  int i, count = m_fxsToRender.getFxCount();
  for (i = 0; i < count; ++i) {
    // In case the process has been canceled, return
    if (m_canceled) return;

    // Then, build the scene fx for each frame to be rendered
    std::vector<std::pair<double, TFxPair>> pairsToBeRendered;

    // NOTE: The above pairs should not be directly added to a previously
    // declared movierenderer,
    // since an output level would be created even before knowing if any cell to
    // be rendered is
    // actually available.
    const BSFX_Transforms_Enum transforms =    // Do NOT add camera and
        BSFX_Transforms_Enum(BSFX_COLUMN_TR);  // camera dpi transforms

    int j;
    for (j = 0, jt = m_framesToRender.begin(); jt != m_framesToRender.end();
         ++j, ++jt) {
      TFxPair fx;

      if (m_renderSettings.m_stereoscopic)
        m_scene->shiftCameraX(-m_renderSettings.m_stereoscopicShift / 2);

      fx.m_frameA =
          buildSceneFx(m_scene, *jt, 0, m_fxsToRender.getFx(i), transforms);

      if (m_renderSettings.m_stereoscopic) {
        m_scene->shiftCameraX(m_renderSettings.m_stereoscopicShift);
        fx.m_frameB =
            buildSceneFx(m_scene, *jt, 0, m_fxsToRender.getFx(i), transforms);
        m_scene->shiftCameraX(-m_renderSettings.m_stereoscopicShift / 2);
      } else if (fieldRendering)
        fx.m_frameB = buildSceneFx(m_scene, *jt + 0.5 * timeStretchFactor, 0,
                                   m_fxsToRender.getFx(i), transforms);
      else
        fx.m_frameB = TRasterFxP();

      // Substitute with the post-process...
      if (fx.m_frameA) fx.m_frameA = addPostProcessing(fx.m_frameA, postFxs[j]);
      if (fx.m_frameB) fx.m_frameB = addPostProcessing(fx.m_frameB, postFxs[j]);

      if (fx.m_frameA)  // Could be 0, if the corresponding cell is void / not
                        // rendered
        pairsToBeRendered.push_back(std::pair<double, TFxPair>(*jt, fx));
    }

    if (pairsToBeRendered.size() == 0)
      continue;  // Could be, if no cell for this column was in the frame range.

    // Build the output name
    TColumnFx *colFx = searchColumn(m_fxsToRender.getFx(i));
    TFx *currFx      = m_fxsToRender.getFx(i);
    assert(colFx);
    if (!colFx) continue;

    int columnIndex = colFx->getColumnIndex();
    std::wstring columnName(colFx->getColumnName());
    std::wstring columnId(colFx->getColumnId());
    std::wstring fxName(currFx->getName());
    std::wstring fxNameNoSpaces(::removeSpaces(fxName));
    std::wstring fxId(currFx->getFxId());

    std::wstring fpName =
        m_fp.getWideName() + L"_" + columnName +
        (columnId == columnName ? L"" : L"(" + columnId + L")") +
        (fxId.empty()
             ? L""
             : L"_" + fxName +
                   (fxId == fxNameNoSpaces ? L"" : L"(" + fxId + L")"));
    TFilePath movieFp(m_fp.withName(fpName));

    // Initialize a MovieRenderer with our infos
    MovieRenderer movieRenderer(m_scene, movieFp, m_threadCount, false);
    movieRenderer.setRenderSettings(m_renderSettings);
    movieRenderer.setDpi(m_xDpi, m_yDpi);
    movieRenderer.enablePrecomputing(m_precomputingEnabled);
    movieRenderer.addListener(this);

    for (unsigned int j = 0; j < pairsToBeRendered.size(); ++j) {
      std::pair<double, TFxPair> &currentPair = pairsToBeRendered[j];
      movieRenderer.addFrame(currentPair.first, currentPair.second);
    }

    // Finally, start rendering currently initialized MovieRenderer.
    m_currentFx        = i;
    m_currentFrame     = m_framesToRender.begin();
    m_currentTRenderer = movieRenderer.getTRenderer();

    movieRenderer.start();

    // I don't recall Toonz currently supports different, simultaneous rendering
    // processes.
    // However, one rendering process is supposed to be exhausting the machine's
    // resources
    // on its own - so, we'll just set up an idle (but GUI-reactive) loop here.
    // It will quit looping whenever the "onSequenceCompleted" notification gets
    // called.
    m_eventLoop.exec();

    //---------------------- Loops here --------------------------
  }

  // Lastly, inform everyone that rendering stopped
  onRenderCompleted();
}

//---------------------------------------------------------

bool MultimediaRenderer::Imp::onFrameCompleted(int frame) {
  // Inform all listeners
  for (unsigned int i = 0; i < m_listeners.size(); ++i)
    m_listeners[i]->onFrameCompleted(*m_currentFrame, m_currentFx);

  m_currentFrame++;
  return !m_canceled;
}

//---------------------------------------------------------

bool MultimediaRenderer::Imp::onFrameFailed(int frame, TException &e) {
  // Inform all listeners
  for (unsigned int i = 0; i < m_listeners.size(); ++i)
    m_listeners[i]->onFrameFailed(*m_currentFrame, m_currentFx, e);

  m_currentFrame++;
  return !m_canceled;
}

//---------------------------------------------------------

void MultimediaRenderer::Imp::onSequenceCompleted(const TFilePath &fp) {
  // Inform all listeners
  m_currentTRenderer = 0;

  for (unsigned int i = 0; i < m_listeners.size(); ++i)
    m_listeners[i]->onSequenceCompleted(m_currentFx);

  m_eventLoop.quit();
}

//---------------------------------------------------------

void MultimediaRenderer::Imp::onRenderCompleted() {
  for (unsigned int i = 0; i < m_listeners.size(); ++i)
    m_listeners[i]->onRenderCompleted();
}

//=============================================
//
//    MultimediaRenderer   class
//
//---------------------------------------------

MultimediaRenderer::MultimediaRenderer(ToonzScene *scene,
                                       const TFilePath &moviePath,
                                       int multimediaMode, int threadCount,
                                       bool cacheResults)
    : m_imp(new Imp(scene, moviePath, multimediaMode, threadCount,
                    cacheResults)) {
  m_imp->addRef();
}

//---------------------------------------------------------

MultimediaRenderer::~MultimediaRenderer() { m_imp->release(); }

//---------------------------------------------------------

const TFilePath &MultimediaRenderer::getFilePath() { return m_imp->m_fp; }

//---------------------------------------------------------

int MultimediaRenderer::getFrameCount() {
  return m_imp->m_framesToRender.size();
}

//---------------------------------------------------------

int MultimediaRenderer::getColumnsCount() {
  return m_imp->m_fxsToRender.getFxCount();
}

//---------------------------------------------------------

int MultimediaRenderer::getMultimediaMode() const {
  return m_imp->m_multimediaMode;
}

//---------------------------------------------------------

void MultimediaRenderer::setRenderSettings(
    const TRenderSettings &renderSettings) {
  // assert(m_imp->m_framesOnRendering.empty());
  m_imp->m_renderSettings = renderSettings;
}

//---------------------------------------------------------

void MultimediaRenderer::setDpi(double xDpi, double yDpi) {
  m_imp->m_xDpi = xDpi;
  m_imp->m_yDpi = yDpi;
}

//---------------------------------------------------------

void MultimediaRenderer::addListener(Listener *listener) {
  m_imp->m_listeners.push_back(listener);
}

//---------------------------------------------------------

void MultimediaRenderer::addFrame(double frame) {
  m_imp->m_framesToRender.insert(frame);
}

//---------------------------------------------------------

void MultimediaRenderer::enablePrecomputing(bool on) {
  m_imp->m_precomputingEnabled = on;
}

//---------------------------------------------------------

//!\b NOTE: Such render may vary from time to time, and even be 0 if no renderer
//! is currently
//! active, for example due to preprocessing states.
TRenderer *MultimediaRenderer::getTRenderer() {
  return m_imp->m_currentTRenderer;
}

//---------------------------------------------------------

bool MultimediaRenderer::isPrecomputingEnabled() const {
  return m_imp->m_precomputingEnabled;
}

//---------------------------------------------------------

void MultimediaRenderer::start() { m_imp->start(); }

//---------------------------------------------------------

void MultimediaRenderer::onCanceled() { m_imp->m_canceled = true; }

//---------------------------------------------------------

/*
bool MultimediaRenderer::done() const
{
  return true;
}
*/
