

// TnzCore includes
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "drawutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/stage.h"

// Tnz6 includes
#include "tapp.h"

// Qt includes
#include <QHBoxLayout>
#include <QList>
#include <QEvent>

#include "vectorizerswatch.h"

//*****************************************************************************
//    Local namespace stuff
//*****************************************************************************

namespace {

inline void getCurrentXsheetPosition(int &row, int &col) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  row = app->getCurrentFrame()->getFrame();
  col = app->getCurrentColumn()->getColumnIndex();
}

//-----------------------------------------------------------------------------

inline TImageP getXsheetImage(int row, int col) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  const TXshCell &cell = xsh->getCell(row, col);
  TXshSimpleLevel *sl  = cell.getSimpleLevel();

  return sl
             ? sl->getFullsampledFrame(cell.getFrameId(),
                                       ImageManager::dontPutInCache)
             : TImageP();
}

//-----------------------------------------------------------------------------

std::unique_ptr<VectorizerConfiguration> getCurrentVectorizerConfiguration(
    int row, int col) {
  typedef std::unique_ptr<VectorizerConfiguration> result_type;

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  const ToonzScene *scene = app->getCurrentScene()->getScene();
  assert(scene);

  const VectorizerParameters *vParams =
      scene->getProperties()->getVectorizerParameters();
  assert(vParams);

  const TXshCell &cell = xsh->getCell(row, col);
  TXshSimpleLevel *sl  = cell.getSimpleLevel();

  if (!sl) return result_type();

  int framesCount = sl->getFrameCount();
  double vFrame =
      (framesCount <= 1)
          ? 0.0
          : tcrop((cell.getFrameId().getNumber() - 1) / double(framesCount - 1),
                  0.0, 1.0);

  return result_type(vParams->getCurrentConfiguration(vFrame));
}

}  // Local namespace

//*****************************************************************************
//    VectorizationSwatchData declaration
//*****************************************************************************

namespace {

/*!
  VectorizationSwatchData is the 'server' singleton data class used to
  vectorize within swatch areas.
*/
struct VectorizationBuilder final : public TThread::Executor {
  int m_row, m_col;
  bool m_done, m_submitted;

  TImageP m_image;

  QList<VectorizerSwatchArea *> m_listeners;

  // Toonz Raster Level may have palette including MyPaint styles,
  // which cannot be rendered in vector levels.
  // In such case prepare an alternative palette in which MyPaint styles
  // are converted to solid color styles.
  TPaletteP m_substitutePalette;
  void prepareSupstitutePaletteIfNeeded(int row, int col);

public:
  VectorizationBuilder()
      : m_row(-1), m_col(-1), m_image(), m_done(false), m_submitted(false) {
    setMaxActiveTasks(1);
  }

  static VectorizationBuilder *instance() {
    static VectorizationBuilder theInstance;
    return &theInstance;
  }

  void addListener(VectorizerSwatchArea *listener);
  void removeListener(VectorizerSwatchArea *listener);

  bool update(TImageP &img);
  bool invalidate(TImageP &img);

  void notifyDone(TImageP img);
};

}  // Local namespace

//*****************************************************************************
//    VectorizationBuilder implementation
//*****************************************************************************

void VectorizationBuilder::addListener(VectorizerSwatchArea *listener) {
  m_listeners.push_back(listener);
}

//-----------------------------------------------------------------------------

void VectorizationBuilder::removeListener(VectorizerSwatchArea *listener) {
  m_listeners.removeOne(listener);
  if (m_listeners.empty()) {
    m_image = TImageP();
    m_row = m_col = -1;
    m_done        = false;
  }
}

//-----------------------------------------------------------------------------

bool VectorizationBuilder::update(TImageP &img) {
  int row, col;
  getCurrentXsheetPosition(row, col);

  bool computing = false;
  if (row != m_row || col != m_col) {
    m_row = row, m_col = col;
    m_done = false;
    prepareSupstitutePaletteIfNeeded(row, col);
    addTask(new VectorizationSwatchTask(row, col, m_substitutePalette));
    computing = true;
  }

  img = m_image;
  return computing;
}

//-----------------------------------------------------------------------------

bool VectorizationBuilder::invalidate(TImageP &img) {
  m_row = m_col       = -1;
  m_done              = false;
  m_substitutePalette = TPaletteP();
  return update(img);
}

//-----------------------------------------------------------------------------

void VectorizationBuilder::notifyDone(TImageP img) {
  m_image = img;

  QList<VectorizerSwatchArea *>::iterator it;
  for (it = m_listeners.begin(); it != m_listeners.end(); ++it) {
    VectorizerSwatchArea::Swatch *swatch = (*it)->rightSwatch();

    swatch->image() = img;
    swatch->setDrawInProgress(false);
    swatch->update();
  }
}

//-----------------------------------------------------------------------------

void VectorizationBuilder::prepareSupstitutePaletteIfNeeded(int row, int col) {
  // Retrieve the image to be vectorized
  TToonzImageP ti(getXsheetImage(row, col));
  if (!ti) return;
  m_substitutePalette = ti->getPalette()->clone();
  bool found          = false;
  for (int s = 0; s < m_substitutePalette->getStyleCount(); s++) {
    TColorStyle *style = m_substitutePalette->getStyle(s);
    if (style->getTagId() == 4001) {  // TMyPaintBrushStyle
      found = true;
      m_substitutePalette->setStyle(s, style->getMainColor());
    }
  }
  if (!found) m_substitutePalette = TPaletteP();
}

//*****************************************************************************
//    VectorizationSwatchTask implementation
//*****************************************************************************

VectorizationSwatchTask::VectorizationSwatchTask(int row, int col,
                                                 TPaletteP substitutePalette)
    : m_row(row), m_col(col), m_substitutePalette(substitutePalette) {
  // Establish connections to default slots; the started one must be blocking,
  // so that run() awaits until it has been performed.

  connect(this, SIGNAL(myStarted(TThread::RunnableP)),
          SLOT(onStarted(TThread::RunnableP)), Qt::BlockingQueuedConnection);
  connect(this, SIGNAL(finished(TThread::RunnableP)),
          SLOT(onFinished(TThread::RunnableP)));
}

//-----------------------------------------------------------------------------

void VectorizationSwatchTask::onStarted(TThread::RunnableP task) {
  VectorizationBuilder *builder = VectorizationBuilder::instance();

  if (builder->m_done || builder->m_row != m_row || builder->m_col != m_col)
    return;

  // Retrieve the image to be vectorized
  m_image = getXsheetImage(m_row, m_col);

  // Build the vectorization configuration
  m_config = getCurrentVectorizerConfiguration(m_row, m_col);
}

//-----------------------------------------------------------------------------

void VectorizationSwatchTask::run() {
  emit myStarted(this);

  if (!m_image || !m_config.get()) return;

  // The task must be performed - retrieve and prepare configuration data
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  VectorizerConfiguration *c = m_config.get();

  // Build additional configuration data
  TPaletteP palette;
  TPointD center, dpi;

  if (TToonzImageP ti = m_image) {
    palette = (m_substitutePalette) ? m_substitutePalette : ti->getPalette();
    center  = ti->getRaster()->getCenterD();
    ti->getDpi(dpi.x, dpi.y);
  } else if (TRasterImageP ri = m_image) {
    palette = new TPalette;
    center  = ri->getRaster()->getCenterD();
    ri->getDpi(dpi.x, dpi.y);
  }

  // In case the image has no dpi, use the standard dpi (same one the
  // PlaneViewer will show
  // the raster image with)
  if (dpi.x == 0.0 || dpi.y == 0.0) dpi.x = dpi.y = Stage::inch;

  // The vectorized image must feel the same visualization transform as m_image.
  // We're transforming the image itself, rather than having it transformed at
  // every paint event.

  // NOTE: It would be best if the transform was the same found from the
  // VectorizerPopup...
  // Unfortunately, this would mean accessing the level's dpi value, thus having
  // to rewrite quite
  // some code around here...
  c->m_affine =
      TScale(Stage::inch / dpi.x, Stage::inch / dpi.y) * TTranslation(-center);
  c->m_thickScale = Stage::inch / dpi.x;

  // Vectorize the image
  VectorizerCore vectorizer;
  TVectorImageP vi = vectorizer.vectorize(m_image, *c, palette.getPointer());
  vi->setPalette(palette.getPointer());

  m_image = vi;
}

//-----------------------------------------------------------------------------

void VectorizationSwatchTask::onFinished(TThread::RunnableP task) {
  if (m_image) {
    VectorizationBuilder *builder = VectorizationBuilder::instance();

    if (builder->m_row == m_row && builder->m_col == m_col) {
      builder->m_done = true;
      builder->notifyDone(m_image);
    }
  }
}

//*****************************************************************************
//    VectorizerPopup::Swatch implementation
//*****************************************************************************

VectorizerSwatchArea::Swatch::Swatch(VectorizerSwatchArea *area)
    : PlaneViewer(area)
    , m_area(area)
    , m_drawVectors(false)
    , m_drawInProgress(false) {
  setBgColor(TPixel32::White, TPixel32::White);
}

//-----------------------------------------------------------------------------

bool VectorizerSwatchArea::Swatch::event(QEvent *e) {
  bool ret           = PlaneViewer::event(e);
  const TAffine &aff = (e->type() == QEvent::Resize)
                           ? m_area->leftSwatch()->viewAff()
                           : viewAff();
  m_area->updateView(aff);
  return ret;
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::Swatch::paintGL() {
  if (isEnabled()) {
    drawBackground();

    if (m_img) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

      pushGLWorldCoordinates();
      draw(m_img);
      if (m_drawVectors) drawVectors();
      popGLCoordinates();

      glDisable(GL_BLEND);
    }
    if (m_drawInProgress) drawInProgress();
  } else {
    glClearColor(0.6, 0.6, 0.6, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::Swatch::drawVectors() {
  TVectorImageP vi(m_img);
  if (vi) {
    glColor3d(1.0, 0.0, 0.0);

    double pixelSize = sqrt(tglGetPixelSize2());
    for (int i = 0; i < (int)vi->getStrokeCount(); ++i) {
      TStroke *stroke = vi->getStroke(i);
      drawStrokeCenterline(*stroke, pixelSize);
    }
  }
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::Swatch::drawInProgress() {
  glColor3d(1.0, 0.0, 0.0);
  glLineWidth(3.0);

  pushGLWinCoordinates();

  glBegin(GL_LINE_STRIP);

  glVertex2d(0.5, 0.5);
  glVertex2d(width() - 0.5, 0.5);
  glVertex2d(width() - 0.5, height() - 0.5);
  glVertex2d(0.5, height() - 0.5);
  glVertex2d(0.5, 0.5);

  glEnd();

  popGLCoordinates();

  glLineWidth(1.0);
}

//*****************************************************************************
//    VectorizerPopup::SwatchArea implementation
//*****************************************************************************

VectorizerSwatchArea::VectorizerSwatchArea(QWidget *parent) {
  m_leftSwatch  = new Swatch(this);
  m_rightSwatch = new Swatch(this);

  m_leftSwatch->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                          QSizePolicy::MinimumExpanding));
  m_rightSwatch->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding,
                                           QSizePolicy::MinimumExpanding));

  QHBoxLayout *lay = new QHBoxLayout;
  setLayout(lay);
  lay->addWidget(m_leftSwatch);
  lay->addWidget(m_rightSwatch);
  lay->setMargin(0);

  setMinimumHeight(150);

  // The followings help in re-establishing focus for wheel events
  m_leftSwatch->setFocusPolicy(Qt::WheelFocus);
  m_rightSwatch->setFocusPolicy(Qt::WheelFocus);
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::updateView(const TAffine &aff) {
  if (m_leftSwatch->viewAff() != aff)
    m_leftSwatch->viewAff() = aff, m_leftSwatch->update();
  if (m_rightSwatch->viewAff() != aff)
    m_rightSwatch->viewAff() = aff, m_rightSwatch->update();
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::resetView() {
  m_leftSwatch->resetView();
  m_rightSwatch->resetView();
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::updateContents() {
  if (!isEnabled() || !isVisible()) return;

  // Retrieve current image
  int row, col;
  getCurrentXsheetPosition(row, col);
  TImageP img = getXsheetImage(row, col);

  // Update the image to be vectorized
  if ((!img) || TVectorImageP(img)) {
    m_leftSwatch->image()  = TImageP();
    m_rightSwatch->image() = TImageP();
  } else {
    m_leftSwatch->image() = img;
    if (VectorizationBuilder::instance()->update(m_rightSwatch->image()))
      m_rightSwatch->setDrawInProgress(true);
  }

  m_leftSwatch->update();
  m_rightSwatch->update();
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::invalidateContents() {
  if (!isEnabled() || !isVisible()) return;

  // Retrieve current image
  int row, col;
  getCurrentXsheetPosition(row, col);
  TImageP img = getXsheetImage(row, col);

  // Update the image to be vectorized
  if ((!img) || TVectorImageP(img)) {
    m_leftSwatch->image()  = TImageP();
    m_rightSwatch->image() = TImageP();
  } else {
    m_leftSwatch->image() = img;
    if (VectorizationBuilder::instance()->invalidate(m_rightSwatch->image()))
      m_rightSwatch->setDrawInProgress(true);
  }

  m_leftSwatch->update();
  m_rightSwatch->update();
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::enablePreview(bool enable) {
  setEnabled(enable);

  if (!isVisible()) return;

  if (enable) {
    connectUpdates();
    updateContents();
  } else
    disconnectUpdates();
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::enableDrawCenterlines(bool enable) {
  m_rightSwatch->setDrawVectors(enable);
  m_rightSwatch->update();
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::connectUpdates() {
  TFrameHandle *frameHandle    = TApp::instance()->getCurrentFrame();
  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();

  connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(updateContents()));
  connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(updateContents()));
  connect(levelHandle, SIGNAL(xshLevelChanged()), this,
          SLOT(invalidateContents()));

  VectorizationBuilder::instance()->addListener(this);
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::disconnectUpdates() {
  TFrameHandle *frameHandle    = TApp::instance()->getCurrentFrame();
  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();

  disconnect(frameHandle, 0, this, 0);
  disconnect(levelHandle, 0, this, 0);

  VectorizationBuilder::instance()->removeListener(this);
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::showEvent(QShowEvent *se) {
  if (isEnabled()) {
    connectUpdates();
    updateContents();
  }
}

//-----------------------------------------------------------------------------

void VectorizerSwatchArea::hideEvent(QHideEvent *he) {
  m_leftSwatch->image()  = TImageP();
  m_rightSwatch->image() = TImageP();

  if (isEnabled()) disconnectUpdates();
}
