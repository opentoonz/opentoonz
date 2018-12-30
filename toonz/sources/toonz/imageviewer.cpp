

#include "imageviewer.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "flipbook.h"
#include "histogrampopup.h"
#include "sceneviewer.h"  // ToggleCommandHandler
#include "mainwindow.h"   //RecentFiles

// TnzTools includes
#include "tools/toolcommandids.h"
#include "tools/cursors.h"
#include "tools/cursormanager.h"
#include "tools/stylepicker.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/lutcalibrator.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tgl.h"

// Qt includes
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLFramebufferObject>

//===================================================================================

extern ToggleCommandHandler safeAreaToggle;
extern void getSafeAreaData(double &_smallSize, double &_largeSize);
// enable to choose safe area from a list, and enable to draw multiple lines
extern void getSafeAreaSizeList(QList<QList<double>> &_sizeList);

//===================================================================================

//==============================
//    Local namespace stuff
//------------------------------

namespace {
// enable to choose safe area from a list, and enable to draw multiple lines
void drawSafeArea(const TRectD box) {
  glPushMatrix();
  glLoadIdentity();

  tglColor(TPixel32::Red);
  glLineStipple(1, 0xCCCC);
  glEnable(GL_LINE_STIPPLE);

  tglDrawRect(box);

  QList<QList<double>> sizeList;

  getSafeAreaSizeList(sizeList);

  for (int i = 0; i < sizeList.size(); i++) {
    QList<double> curSize = sizeList.at(i);
    if (curSize.size() == 5)
      tglColor(
          TPixel((int)curSize.at(2), (int)curSize.at(3), (int)curSize.at(4)));
    else
      tglColor(TPixel32::Red);

    double facX = -0.5 * (1 - curSize.at(0) / 100.0);
    double facY = -0.5 * (1 - curSize.at(1) / 100.0);
    tglDrawRect(box.enlarge(facX * box.getLx(), facY * box.getLy()));
  }

  glDisable(GL_LINE_STIPPLE);
  glPopMatrix();
}

//-----------------------------------------------------------------------------

inline TRect getImageBounds(const TImageP &img) {
  if (TRasterImageP ri = img)
    return ri->getRaster()->getBounds();
  else if (TToonzImageP ti = img)
    return ti->getRaster()->getBounds();
  else {
    TVectorImageP vi = img;
    return convert(vi->getBBox());
  }
}

//-----------------------------------------------------------------------------

inline TRectD getImageBoundsD(const TImageP &img) {
  if (TRasterImageP ri = img)
    return TRectD(0, 0, ri->getRaster()->getLx(), ri->getRaster()->getLy());
  else if (TToonzImageP ti = img)
    return TRectD(0, 0, ti->getSize().lx, ti->getSize().ly);
  else {
    TVectorImageP vi = img;
    return vi->getBBox();
  }
}

//-----------------------------------------------------------------------------

class FlipZoomer final : public ImageUtils::ShortcutZoomer {
public:
  FlipZoomer(ImageViewer *parent) : ShortcutZoomer(parent) {}

  bool zoom(bool zoomin, bool resetZoom) override {
    static_cast<ImageViewer *>(getWidget())->zoomQt(zoomin, resetZoom);
    return true;
  }

  bool fit() override {
    static_cast<ImageViewer *>(getWidget())->fitView();
    return true;
  }

  bool toggleFullScreen(bool quit) override {
    if (ImageUtils::FullScreenWidget *fsWidget =
            dynamic_cast<ImageUtils::FullScreenWidget *>(
                getWidget()->parentWidget()))
      return fsWidget->toggleFullScreen(quit);

    return false;
  }
};

//-----------------------------------------------------------------------------

class ImageViewerShortcutReceiver {
  FlipBook *m_flipbook;

public:
  ImageViewerShortcutReceiver(FlipBook *flipbook) : m_flipbook(flipbook) {}
  ~ImageViewerShortcutReceiver() {}

  bool exec(QKeyEvent *event) {
    CommandManager *cManager = CommandManager::instance();

    if (event->key() == cManager->getKeyFromId(MI_FreezePreview)) {
      m_flipbook->isFreezed() ? m_flipbook->unfreezePreview()
                              : m_flipbook->freezePreview();
      return true;
    }

    if (event->key() == cManager->getKeyFromId(MI_ClonePreview)) {
      m_flipbook->clonePreview();
      return true;
    }

    if (event->key() == cManager->getKeyFromId(MI_RegeneratePreview)) {
      m_flipbook->regenerate();
      return true;
    }

    if (event->key() == cManager->getKeyFromId(MI_RegenerateFramePr)) {
      m_flipbook->regenerateFrame();
      return true;
    }

    return false;
  }
};
}  // namespace

//=============================================================================

//==========================
//    ImageViewer class
//--------------------------

/*! \class ImageViewer
                \brief The ImageViewer class provides to view an image.

                Inherits \b QOpenGLWidget.

                The object allows also to manage pan and zoom event. It's
   possible to set a
                color mask TRop::ColorMask to image view.
*/
/*! \fn void ImageViewer::setColorMask(UCHAR colorMask)
                Set current TRop::ColorMask color mask to \b colorMask.
*/
/*! \fn TImageP ImageViewer::getImage()
                Return current image viewer.
*/
/*! \fn TAffine ImageViewer::getViewAff()
                Return current viewer matrix trasformation.
*/

ImageViewer::ImageViewer(QWidget *parent, FlipBook *flipbook,
                         bool showHistogram)
    : GLWidgetForHighDpi(parent)
    , m_pressedMousePos(0, 0)
    , m_mouseButton(Qt::NoButton)
    , m_draggingZoomSelection(false)
    , m_image()
    , m_FPS(0)
    , m_viewAff()
    , m_pos(0, 0)
    , m_visualSettings()
    , m_compareSettings()
    , m_isHistogramEnable(showHistogram)
    , m_flipbook(flipbook)
    , m_isColorModel(false)
    , m_histogramPopup(0)
    , m_isRemakingPreviewFx(false)
    , m_rectRGBPick(false) {
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  m_visualSettings.m_drawExternalBG = true;
  setAttribute(Qt::WA_KeyCompression);
  setFocusPolicy(Qt::StrongFocus);

  setMouseTracking(true);

  if (m_isHistogramEnable)
    m_histogramPopup = new HistogramPopup(tr("Flipbook Histogram"));

  if (Preferences::instance()->isColorCalibrationEnabled())
    m_lutCalibrator = new LutCalibrator();
}

//-----------------------------------------------------------------------------

void ImageViewer::contextMenuEvent(QContextMenuEvent *event) {
  if (!m_flipbook) return;

  QAction *action;

  if (m_isColorModel) {
    event->ignore();
    return;
  }

  QMenu *menu = new QMenu(this);

  if (m_flipbook->getPreviewedFx()) {
    if (!(windowState() & Qt::WindowFullScreen)) {
      action = menu->addAction(tr("Clone Preview"));
      action->setShortcut(QKeySequence(
          CommandManager::instance()->getKeyFromId(MI_ClonePreview)));
      connect(action, SIGNAL(triggered()), m_flipbook, SLOT(clonePreview()));
    }

    if (m_flipbook->isFreezed()) {
      action = menu->addAction(tr("Unfreeze Preview"));
      action->setShortcut(QKeySequence(
          CommandManager::instance()->getKeyFromId(MI_FreezePreview)));
      connect(action, SIGNAL(triggered()), m_flipbook, SLOT(unfreezePreview()));
    } else {
      action = menu->addAction(tr("Freeze Preview"));
      action->setShortcut(QKeySequence(
          CommandManager::instance()->getKeyFromId(MI_FreezePreview)));
      connect(action, SIGNAL(triggered()), m_flipbook, SLOT(freezePreview()));
    }

    action = menu->addAction(tr("Regenerate Preview"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_RegeneratePreview)));
    connect(action, SIGNAL(triggered()), m_flipbook, SLOT(regenerate()));

    action = menu->addAction(tr("Regenerate Frame Preview"));
    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(MI_RegenerateFramePr)));
    connect(action, SIGNAL(triggered()), m_flipbook, SLOT(regenerateFrame()));

    menu->addSeparator();
  }

  action = menu->addAction(tr("Load / Append Images"));
  connect(action, SIGNAL(triggered()), m_flipbook, SLOT(loadImages()));

  // history of the loaded paths of flipbook
  action = CommandManager::instance()->getAction(MI_LoadRecentImage);
  menu->addAction(action);
  action->setParent(m_flipbook);

  if (m_flipbook->isSavable()) {
    action = menu->addAction(tr("Save Images"));
    connect(action, SIGNAL(triggered()), m_flipbook, SLOT(saveImages()));
  }
  menu->addSeparator();

  QAction *reset = menu->addAction(tr("Reset View"));
  reset->setShortcut(
      QKeySequence(CommandManager::instance()->getKeyFromId(V_ZoomReset)));
  connect(reset, SIGNAL(triggered()), SLOT(resetView()));

  QAction *fit = menu->addAction(tr("Fit To Window"));
  fit->setShortcut(
      QKeySequence(CommandManager::instance()->getKeyFromId(V_ZoomFit)));
  connect(fit, SIGNAL(triggered()), SLOT(fitView()));

#ifdef _WIN32

  if (ImageUtils::FullScreenWidget *fsWidget =
          dynamic_cast<ImageUtils::FullScreenWidget *>(parentWidget())) {
    bool isFullScreen = (fsWidget->windowState() & Qt::WindowFullScreen) != 0;

    action = menu->addAction(isFullScreen ? tr("Exit Full Screen Mode")
                                          : tr("Full Screen Mode"));

    action->setShortcut(QKeySequence(
        CommandManager::instance()->getKeyFromId(V_ShowHideFullScreen)));
    connect(action, SIGNAL(triggered()), fsWidget, SLOT(toggleFullScreen()));
  }

#endif

  bool addedSep = false;

  if (m_isHistogramEnable &&
      visibleRegion().contains(event->pos() * getDevPixRatio())) {
    menu->addSeparator();
    addedSep = true;
    action   = menu->addAction(tr("Show Histogram"));
    connect(action, SIGNAL(triggered()), SLOT(showHistogram()));
  }

  if (m_visualSettings.m_doCompare) {
    if (!addedSep) menu->addSeparator();
    action = menu->addAction(tr("Swap Compared Images"));
    connect(action, SIGNAL(triggered()), SLOT(swapCompared()));
  }

  menu->exec(event->globalPos());

  action = CommandManager::instance()->getAction(MI_LoadRecentImage);
  action->setParent(0);

  delete menu;
  update();
}

//-----------------------------------------------------------------------------

void ImageViewer::setVisual(const ImagePainter::VisualSettings &settings) {
  m_visualSettings = settings;
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  m_visualSettings.m_drawExternalBG = true;
}

//-----------------------------------------------------------------------------

ImageViewer::~ImageViewer() {
  // iwsw commented out temporarily
  /*
  if (m_ghibli3DLutUtil)
  {
          m_ghibli3DLutUtil->onEnd();
          delete m_ghibli3DLutUtil;
  }
  */
  if (m_fbo) delete m_fbo;
}

//-----------------------------------------------------------------------------
/*! Set current image to \b image and update. If Histogram is visible set its
 * image.
*/
void ImageViewer::setImage(TImageP image) {
  m_image = image;

  if (m_isHistogramEnable && m_histogramPopup->isVisible())
    m_histogramPopup->setImage(image);
  if (!isColorModel())
    repaint();
  else
    update();
}

//-------------------------------------------------------------------

void ImageViewer::setHistogramTitle(QString levelName) {
  if (m_isHistogramEnable && m_histogramPopup->isVisible()) {
    QString histogramTitle = QString("Histogram  ::  ") + levelName;
    m_histogramPopup->setTitle(histogramTitle);
  }
}
//-------------------------------------------------------------------

void ImageViewer::setHistogramEnable(bool enable) {
  m_isHistogramEnable = enable;
  if (m_isHistogramEnable && !m_histogramPopup)
    m_histogramPopup = new HistogramPopup(tr("Flipbook Histogram"));
}

//-----------------------------------------------------------------------------

void ImageViewer::hideHistogram() {
  if (!m_isHistogramEnable) return;
  m_histogramPopup->setImage(TImageP());
  if (m_histogramPopup->isVisible()) {
    m_histogramPopup->hide();
    m_histogramPopup->setTitle(tr("Flipbook Histogram"));
  }
}

//-------------------------------------------------------------------

void ImageViewer::initializeGL() {
  initializeOpenGLFunctions();

  // to be computed once through the software
  if (m_lutCalibrator) {
    m_lutCalibrator->initialize();
    connect(context(), SIGNAL(aboutToBeDestroyed()), this,
            SLOT(onContextAboutToBeDestroyed()));
  }

  // glClearColor(1.0,1.0,1.0,1);
  glClear(GL_COLOR_BUFFER_BIT);
}

//-----------------------------------------------------------------------------

void ImageViewer::resizeGL(int w, int h) {
  w *= getDevPixRatio();
  h *= getDevPixRatio();
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  double nearPlane   = 100;
  double farPlane    = 2500;
  double centerPlane = 1100;
  glOrtho(0, w, 0, h, -4000, 4000);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.375, 0.375, 0.0);
  glTranslated(w * 0.5, h * 0.5, 0);

  // remake fbo with new size
  if (m_lutCalibrator && m_lutCalibrator->isValid()) {
    if (m_fbo) delete m_fbo;
    m_fbo = new QOpenGLFramebufferObject(w, h);
  }
}

//-----------------------------------------------------------------------------

void ImageViewer::paintGL() {
  if (m_lutCalibrator && m_lutCalibrator->isValid()) m_fbo->bind();

  TDimension viewerSize(width(), height());
  TAffine aff = m_viewAff;

  // if (!m_visualSettings.m_defineLoadbox && m_flipbook &&
  // m_flipbook->getLoadbox()!=TRect())
  //  offs =
  //  convert(m_flipbook->getLoadbox().getP00())-TPointD(m_flipbook->getImageSize().lx/2.0,
  //  m_flipbook->getImageSize().ly/2.0);

  TDimension imageSize;
  TRect loadbox;

  if (m_flipbook) {
    QString title =
        (!m_image)
            ? m_flipbook->getTitle() + tr("  ::  Zoom : ") +
                  QString::number(tround(sqrt(m_viewAff.det()) * 100)) + " %"
            : m_flipbook->getLevelZoomTitle() + tr("  ::  Zoom : ") +
                  QString::number(tround(sqrt(m_viewAff.det()) * 100)) + " %";
    m_flipbook->parentWidget()->setWindowTitle(title);
    imageSize = m_flipbook->getImageSize();
    if (m_visualSettings.m_useLoadbox && m_flipbook->getLoadbox() != TRect())
      loadbox = m_flipbook->getLoadbox();
  }
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  // enable checks only in the color model
  m_visualSettings.m_useChecks = m_isColorModel;
  ImagePainter::paintImage(m_image, imageSize, viewerSize, aff,
                           m_visualSettings, m_compareSettings, loadbox);

  // when fx parameter is modified with showing the fx preview,
  // a flipbook shows a red border line before the rendered result is shown.
  if (m_isRemakingPreviewFx) {
    glPushMatrix();
    glLoadIdentity();
    glColor3d(1.0, 0.0, 0.0);

    glBegin(GL_LINE_LOOP);
    glVertex2d(5, 5);
    glVertex2d(5, height() - 5);
    glVertex2d(width() - 5, height() - 5);
    glVertex2d(width() - 5, 5);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glVertex2d(10, 10);
    glVertex2d(10, height() - 10);
    glVertex2d(width() - 10, height() - 10);
    glVertex2d(width() - 10, 10);
    glEnd();
    glPopMatrix();
  }

  if (!m_image) {
    if (m_lutCalibrator && m_lutCalibrator->isValid())
      m_lutCalibrator->onEndDraw(m_fbo);
    return;
  }

  if (safeAreaToggle.getStatus() && !m_isColorModel) {
    TRasterImageP rimg = (TRasterImageP)m_image;
    TVectorImageP vimg = (TVectorImageP)m_image;
    TToonzImageP timg  = (TToonzImageP)m_image;
    TRect bbox;

    TPointD centerD;
    TRect bounds;
    if (rimg) {
      centerD = rimg->getRaster()->getCenterD();
      bounds  = rimg->getRaster()->getBounds();
    } else if (timg) {
      centerD = timg->getRaster()->getCenterD();
      bounds  = timg->getRaster()->getBounds();
    }

    if (!vimg) {
      TAffine aff = TTranslation(viewerSize.lx * 0.5, viewerSize.ly * 0.5) *
                    m_viewAff * TTranslation(-centerD);
      TRectD bbox = aff * TRectD(0, 0, bounds.getLx() - 1, bounds.getLy() - 1);
      drawSafeArea(bbox);
    }
  }
  TPoint fromPos, toPos;

  if (m_visualSettings.m_defineLoadbox && m_flipbook) {
    TRect loadbox =
        convert(getImgToWidgetAffine() * convert(m_flipbook->getLoadbox()));
    if (loadbox != TRect()) {
      TPoint p00 = loadbox.getP00();
      TPoint p11 = loadbox.getP11();
      fromPos =
          TPoint(p00.x - width() * 0.5,
                 height() * 0.5 - p00.y);  // m_flipbook->getLoadbox().getP00();
      toPos =
          TPoint(p11.x - width() * 0.5,
                 height() * 0.5 - p11.y);  // m_flipbook->getLoadbox().getP11();
    }
  } else if (m_draggingZoomSelection || m_rectRGBPick) {
    fromPos = TPoint(m_pressedMousePos.x - width() * 0.5,
                     height() * 0.5 - m_pressedMousePos.y);
    toPos = TPoint(m_pos.x() - width() * 0.5, height() * 0.5 - m_pos.y());
  }
  if (fromPos != TPoint() || toPos != TPoint()) {
    if (m_rectRGBPick) {
      tglColor(TPixel32::Red);
      // TODO: glLineStipple is deprecated in the latest OpenGL. Need to be
      // replaced. (shun_iwasawa 2015/12/25)
      glLineStipple(1, 0x3F33);
      glEnable(GL_LINE_STIPPLE);

      glBegin(GL_LINE_STRIP);
      // do not draw the rect around the mouse cursor
      int margin = (fromPos.y < toPos.y) ? -3 : 3;
      glVertex2i(toPos.x, toPos.y + margin);
      glVertex2i(toPos.x, fromPos.y);
      glVertex2i(fromPos.x, fromPos.y);
      glVertex2i(fromPos.x, toPos.y);
      margin = (fromPos.x < toPos.x) ? -3 : 3;
      glVertex2i(toPos.x + margin, toPos.y);
      glEnd();
      glDisable(GL_LINE_STIPPLE);
    } else {
      tglColor(m_draggingZoomSelection ? TPixel32::Red : TPixel32::Blue);
      glBegin(GL_LINE_STRIP);
      glVertex2i(fromPos.x, fromPos.y);
      glVertex2i(fromPos.x, toPos.y);
      glVertex2i(toPos.x, toPos.y);
      glVertex2i(toPos.x, fromPos.y);
      glVertex2i(fromPos.x, fromPos.y);
      glEnd();
    }
  }

  if (m_lutCalibrator && m_lutCalibrator->isValid())
    m_lutCalibrator->onEndDraw(m_fbo);
}

//------------------------------------------------------------------------------
/*! Add to current trasformation matrix a \b delta traslation.
*/
void ImageViewer::panQt(const QPoint &delta) {
  if (delta == QPoint()) return;

  // stop panning when the image is at the edge of window
  QPoint delta_(delta.x(), delta.y());

  TToonzImageP timg  = (TToonzImageP)m_image;
  TRasterImageP rimg = (TRasterImageP)m_image;
  if (timg || rimg) {
    bool isXPlus = delta.x() > 0;
    bool isYPlus = delta.y() > 0;

    TDimension imgSize((timg) ? timg->getSize() : rimg->getRaster()->getSize());
    int subSampling = (timg) ? timg->getSubsampling() : rimg->getSubsampling();

    TPointD cornerPos = TPointD(imgSize.lx * ((isXPlus) ? -1 : 1),
                                imgSize.ly * ((isYPlus) ? 1 : -1)) *
                        (0.5 / (double)subSampling);
    cornerPos = m_viewAff * cornerPos;

    if ((cornerPos.x > 0) == isXPlus) delta_.setX(0);
    if ((cornerPos.y < 0) == isYPlus) delta_.setY(0);
  }

  setViewAff(TTranslation(delta_.x(), -delta_.y()) * m_viewAff);

  update();
}

//-----------------------------------------------------------------------------
/*! Add to current trasformation matrix a \b center traslation matched with a
                scale of factor \b factor. Apply a zoom of factor \b factor with
   center
                \b center.
*/
void ImageViewer::zoomQt(const QPoint &center, double factor) {
  if (factor == 1.0) return;
  TPointD delta(center.x(), center.y());
  double scale2 = fabs(m_viewAff.det());
  if ((scale2 < 100000 || factor < 1) && (scale2 > 0.001 * 0.05 || factor > 1))
    setViewAff(TTranslation(delta) * TScale(factor) * TTranslation(-delta) *
               m_viewAff);
  update();
}

//-----------------------------------------------------------------------------

void ImageViewer::zoomQt(bool forward, bool reset) {
  double scale2 = m_viewAff.det();
  if (reset)
    setViewAff(TAffine());
  else if (((scale2 < 100000 || !forward) &&
            (scale2 > 0.001 * 0.05 || forward))) {
    double oldZoomScale = sqrt(scale2);
    double zoomScale =
        reset ? 1 : ImageUtils::getQuantizedZoomFactor(oldZoomScale, forward);
    setViewAff(TScale(zoomScale / oldZoomScale) * m_viewAff);
  }
  update();
}

//-----------------------------------------------------------------------------

void ImageViewer::dragCompare(const QPoint &dp) {
  if (m_compareSettings.m_dragCompareX)
    m_compareSettings.m_compareX += ((double)dp.x()) / width();
  else if (m_compareSettings.m_dragCompareY)
    m_compareSettings.m_compareY -= ((double)dp.y()) / height();

  m_compareSettings.m_compareX = tcrop(m_compareSettings.m_compareX, 0.0, 1.0);
  m_compareSettings.m_compareY = tcrop(m_compareSettings.m_compareY, 0.0, 1.0);

  update();
}

//-----------------------------------------------------------------------------

void ImageViewer::updateLoadbox(const TPoint &curPos) {
  TAffine aff = getImgToWidgetAffine();
  TRect r =
      m_flipbook
          ->getLoadbox();  // convert(aff*convert(m_flipbook->getLoadbox()));
  if (m_dragType == eDrawRect)
    r = convert(aff.inv() *
                convert(TRect(m_pressedMousePos,
                              curPos)));  // TRect(m_pressedMousePos, curPos);
  else if (m_dragType == eMoveRect)
    r = r + (TPoint(curPos.x, -curPos.y) - TPoint(m_pos.x(), -m_pos.y()));
  else {
    double fac = sqrt(1.0 / fabs(aff.det()));

    if (m_dragType & eMoveLeft) r.x0 += fac * (curPos.x - m_pos.x());
    if (m_dragType & eMoveRight) r.x1 += fac * (curPos.x - m_pos.x());
    if (m_dragType & eMoveDown) r.y1 -= fac * (curPos.y - m_pos.y());
    if (m_dragType & eMoveUp) r.y0 -= fac * (curPos.y - m_pos.y());
  }
  m_flipbook->setLoadbox(r);  // convert(aff.inv() * convert(r)));
}

//--------------------------------------------------------
void ImageViewer::updateCursor(const TPoint &curPos) {
  int dragType = getDragType(
      curPos,
      convert(getImgToWidgetAffine() * convert(m_flipbook->getLoadbox())));

  switch (dragType) {
  case eMoveRect:
    setCursor(Qt::SizeAllCursor);
    break;
  case eMoveLeft:
  case eMoveRight:
    setCursor(Qt::SizeHorCursor);
    break;
  case eMoveDown:
  case eMoveUp:
    setCursor(Qt::SizeVerCursor);
    break;
  case eMoveLeft | eMoveUp:
  case eMoveRight | eMoveDown:
    setCursor(Qt::SizeBDiagCursor);
    break;
  case eMoveLeft | eMoveDown:
  case eMoveRight | eMoveUp:
    setCursor(Qt::SizeFDiagCursor);
    break;
  default:
    setCursor(Qt::ArrowCursor);
    break;
  }
}

//---------------------------------------------------------------------------------------------
/*! If middle button is pressed pan the image. Update current mouse position.
*/
void ImageViewer::mouseMoveEvent(QMouseEvent *event) {
  if (!m_image) return;
  QPoint curQPos = event->pos() * getDevPixRatio();

  TPoint curPos = TPoint(curQPos.x(), curQPos.y());

  if (m_visualSettings.m_defineLoadbox && m_flipbook) {
    if (m_mouseButton == Qt::LeftButton)
      updateLoadbox(curPos);
    else if (m_mouseButton == Qt::MidButton)
      panQt(curQPos - m_pos);
    else
      updateCursor(curPos);
    update();
    event->ignore();
    m_pos = curQPos;
    return;
  }

  // setting the cursors
  if (!m_isColorModel) {
    // when the histogram window is opened, switch to the RGB picker tool
    if (m_isHistogramEnable && m_histogramPopup->isVisible())
      setToolCursor(this, ToolCursor::PickerRGB);
    else
      setCursor(Qt::ArrowCursor);
  }

  if (m_visualSettings.m_doCompare && m_mouseButton == Qt::NoButton) {
    if (fabs(curPos.x - width() * m_compareSettings.m_compareX) < 20)
      setToolCursor(this, ToolCursor::ScaleHCursor);
    else if (fabs((height() - curPos.y) -
                  height() * m_compareSettings.m_compareY) < 20)
      setToolCursor(this, ToolCursor::ScaleVCursor);
  }

  if (m_compareSettings.m_dragCompareX || m_compareSettings.m_dragCompareY)
    dragCompare(curQPos - m_pos);
  else if (m_mouseButton == Qt::MidButton)
    panQt(curQPos - m_pos);

  m_pos = curQPos;

  // pick the color if the histogram popup is opened
  if (m_isHistogramEnable && m_histogramPopup->isVisible() && !m_isColorModel) {
    // Rect Pick
    if (m_mouseButton == Qt::LeftButton &&
        (event->modifiers() & Qt::ControlModifier)) {
      if (!m_rectRGBPick && !m_visualSettings.m_defineLoadbox &&
          (abs(m_pos.x() - m_pressedMousePos.x) > 10 ||
           abs(m_pos.y() - m_pressedMousePos.y) > 10) &&
          !(m_compareSettings.m_dragCompareX ||
            m_compareSettings.m_dragCompareY) &&
          m_flipbook)
        m_rectRGBPick = true;

      if (m_rectRGBPick) {
        update();
        rectPickColor();
      }
    }

    update();
    pickColor(event);
    return;
  }

  if (m_mouseButton == Qt::LeftButton && !m_isColorModel &&
      (event->modifiers() & Qt::AltModifier)) {
    if (!m_draggingZoomSelection && !m_visualSettings.m_defineLoadbox &&
        (abs(m_pos.x() - m_pressedMousePos.x) > 10 ||
         abs(m_pos.y() - m_pressedMousePos.y) > 10) &&
        !(m_compareSettings.m_dragCompareX ||
          m_compareSettings.m_dragCompareY) &&
        m_flipbook)
      m_draggingZoomSelection = true;

    if (m_draggingZoomSelection) update();
  } else
    m_draggingZoomSelection = false;

  if (m_isColorModel && m_mouseButton == Qt::LeftButton) {
    event->ignore();
  }
}

//---------------------------------------------------------------------------------------------
/*! notify the color picked by rgb picker to palette controller
*/
void ImageViewer::setPickedColorToStyleEditor(const TPixel32 &color) {
  // do not modify the style #0
  TPaletteHandle *ph =
      TApp::instance()->getPaletteController()->getCurrentPalette();
  if (ph->getStyleIndex() == 0) return;

  TApp::instance()->getPaletteController()->setColorSample(color);
}

//---------------------------------------------------------------------------------------------
/*! rgb picking
*/
void ImageViewer::pickColor(QMouseEvent *event, bool putValueToStyleEditor) {
  if (!m_isHistogramEnable) return;
  if (!m_histogramPopup->isVisible()) return;

  QPoint curPos = event->pos() * getDevPixRatio();

  // avoid to pick outside of the flip
  if ((!m_image) || !rect().contains(curPos)) {
    // throw transparent color
    m_histogramPopup->updateInfo(TPixel32::Transparent, TPointD(-1, -1));
    return;
  }

  StylePicker picker(m_image);

  TPoint mousePos = TPoint(curPos.x(), height() - 1 - curPos.y());
  TRectD area     = TRectD(mousePos.x, mousePos.y, mousePos.x, mousePos.y);

  if (m_lutCalibrator && m_lutCalibrator->isValid()) m_fbo->bind();

  const TPixel32 pix = picker.pickColor(area);

  if (m_lutCalibrator && m_lutCalibrator->isValid()) m_fbo->release();

  QPoint viewP = mapFrom(this, curPos);
  TPointD pos  = getViewAff().inv() *
                TPointD(viewP.x() - width() / 2, -viewP.y() + height() / 2);
  TPointD imagePos = TPointD(0.5 * m_image->getBBox().getLx() + pos.x,
                             0.5 * m_image->getBBox().getLy() + pos.y);
  if (m_image->getBBox().contains(imagePos)) {
    // throw the picked color to the histogram
    m_histogramPopup->updateInfo(pix, imagePos);
    // throw it to the style editor as well
    if (putValueToStyleEditor) setPickedColorToStyleEditor(pix);
  } else {
    // throw transparent color if picking outside of the image
    m_histogramPopup->updateInfo(TPixel32::Transparent, TPointD(-1, -1));
  }
}

//---------------------------------------------------------------------------------------------
/*! rectangular rgb picking. The picked color will be an average of pixels in
 * specified rectangle
*/
void ImageViewer::rectPickColor(bool putValueToStyleEditor) {
  if (!m_isHistogramEnable) return;
  if (!m_histogramPopup->isVisible()) return;

  StylePicker picker(m_image);

  TPoint startPos =
      TPoint(m_pressedMousePos.x, height() - 1 - m_pressedMousePos.y);
  TPoint endPos = TPoint(m_pos.x(), height() - 1 - m_pos.y());
  TRectD area   = TRectD(convert(startPos), convert(endPos));
  area          = area.enlarge(-1, -1);
  if (area.getLx() < 2 || area.getLy() < 2) {
    m_histogramPopup->updateAverageColor(TPixel32::Transparent);
    return;
  }

  if (m_lutCalibrator && m_lutCalibrator->isValid() && m_fbo) m_fbo->bind();

  const TPixel32 pix = picker.pickColor(area.enlarge(-1, -1));

  if (m_lutCalibrator && m_lutCalibrator->isValid() && m_fbo) m_fbo->release();

  // throw the picked color to the histogram
  m_histogramPopup->updateAverageColor(pix);
  // throw it to the style editor as well
  if (putValueToStyleEditor) setPickedColorToStyleEditor(pix);
}

//-----------------------------------------------------------------------------
/*! Set current position and current mouse button event.
                Ignore event, so access to parent mousePressEvent.
*/

int ImageViewer::getDragType(const TPoint &pos, const TRect &loadbox) {
  if (loadbox == TRect()) return eDrawRect;

  int ret = 0, dx = std::min(abs(loadbox.x0 - pos.x), abs(loadbox.x1 - pos.x)),
      dy = std::min(abs(loadbox.y0 - pos.y), abs(loadbox.y1 - pos.y));

  if (dx > 10 && dy > 10)
    return (loadbox.contains(pos)) ? eMoveRect : eDrawRect;

  if (dx <= 10 && pos.y >= loadbox.y0 - 10 && pos.y <= loadbox.y1 + 10) {
    if (dx == abs(loadbox.x0 - pos.x))
      ret = eMoveLeft;
    else if (dx == abs(loadbox.x1 - pos.x))
      ret = eMoveRight;
  } else if (dy <= 10 && pos.x >= loadbox.x0 - 10 && pos.x <= loadbox.x1 + 10) {
    if (dy == abs(loadbox.y0 - pos.y))
      return eMoveDown;
    else
      return eMoveUp;
  } else
    return eDrawRect;

  if (dy > 10) return ret;

  return ret | (dy == (abs(loadbox.y0 - pos.y)) ? eMoveDown : eMoveUp);
}

//-------------------------------------------------------------------------------
void ImageViewer::mouseDoubleClickEvent(QMouseEvent *event) {
  if (!m_image) return;
  if (m_visualSettings.m_defineLoadbox && m_flipbook) {
    m_flipbook->setLoadbox(TRect());
    update();
    event->ignore();
  }
}

//------------------------------------------------------------------------------

void ImageViewer::mousePressEvent(QMouseEvent *event) {
  if (!m_image) return;
  m_pos                   = event->pos() * getDevPixRatio();
  m_pressedMousePos       = TPoint(m_pos.x(), m_pos.y());
  m_mouseButton           = event->button();
  m_draggingZoomSelection = false;

  if (m_mouseButton != Qt::LeftButton) {
    event->ignore();
    return;
  }
  if (m_visualSettings.m_defineLoadbox && m_flipbook)
    m_dragType = getDragType(
        m_pressedMousePos,
        convert(getImgToWidgetAffine() * convert(m_flipbook->getLoadbox())));
  else if (m_visualSettings.m_doCompare) {
    if (fabs(m_pos.x() - width() * m_compareSettings.m_compareX) < 20) {
      m_compareSettings.m_dragCompareX = true;
      m_compareSettings.m_dragCompareY = false;
      m_compareSettings.m_compareY     = ImagePainter::DefaultCompareValue;
      update();
    } else if (fabs((height() - m_pos.y()) -
                    height() * m_compareSettings.m_compareY) < 20) {
      m_compareSettings.m_dragCompareY = true;
      m_compareSettings.m_dragCompareX = false;
      m_compareSettings.m_compareX     = ImagePainter::DefaultCompareValue;
      update();
    } else
      m_compareSettings.m_dragCompareX = m_compareSettings.m_dragCompareY =
          false;
  }

  // pick color and throw it to the style editor
  if (m_isHistogramEnable && m_histogramPopup->isVisible() && !m_isColorModel &&
      !(event->modifiers() & Qt::ControlModifier))  // if ctrl button is
                                                    // pressed, which means
                                                    // rectangular pick
  {
    update();
    pickColor(event, true);
  }

  event->ignore();
}

//-----------------------------------------------------------------------------
/*! Reset current mouse position and current mouse button event.
*/
void ImageViewer::mouseReleaseEvent(QMouseEvent *event) {
  if (!m_image) return;
  if (m_draggingZoomSelection && !m_visualSettings.m_defineLoadbox) {
    m_draggingZoomSelection = false;

    int left, right, top, bottom;
    if (m_pos.x() < m_pressedMousePos.x)
      left = m_pos.x(), right = m_pressedMousePos.x;
    else
      right = m_pos.x(), left = m_pressedMousePos.x;
    if (m_pos.y() < m_pressedMousePos.y)
      top = m_pos.y(), bottom = m_pressedMousePos.y;
    else
      bottom = m_pos.y(), top = m_pressedMousePos.y;

    adaptView(QRect(QPoint(left, top), QPoint(right, bottom)));
  }

  if (m_rectRGBPick) {
    // pick color in the rectangular region
    rectPickColor(true);
    // release the flag
    m_rectRGBPick = false;
    update();
  }

  m_pos                            = QPoint(0, 0);
  m_mouseButton                    = Qt::NoButton;
  m_compareSettings.m_dragCompareX = m_compareSettings.m_dragCompareY = false;

  event->ignore();
}

//-----------------------------------------------------------------------------
/*! Apply zoom.
*/
void ImageViewer::wheelEvent(QWheelEvent *event) {
  if (!m_image) return;
  if (event->orientation() == Qt::Horizontal) return;
  int delta = event->delta() > 0 ? 120 : -120;
  QPoint center(event->pos().x() * getDevPixRatio() - width() / 2,
                -event->pos().y() * getDevPixRatio() + height() / 2);
  zoomQt(center, exp(0.001 * delta));
}

//-----------------------------------------------------------------------------

void ImageViewer::swapCompared() {
  m_compareSettings.m_swapCompared = !m_compareSettings.m_swapCompared;
  update();
}

//-----------------------------------------------------------------------------

void ImageViewer::setViewAff(TAffine viewAff) {
  m_viewAff = viewAff;
  update();
  if (m_flipbook && m_flipbook->getPreviewedFx())
    // Update the observable fx region
    m_flipbook->schedulePreviewedFxUpdate();
}

//-----------------------------------------------------------------------------

void ImageViewer::resetView() { zoomQt(false, true); }

//-----------------------------------------------------------------------------

void ImageViewer::fitView() {
  if (!m_image) return;
  TRect imgBounds(getImageBounds(m_image));
  adaptView(imgBounds, imgBounds);
}

//-----------------------------------------------------------------------------
/*! Update image viewer, reset current matrix trasformation, current position
                and update view.
*/
void ImageViewer::updateImageViewer() {
  setViewAff(TAffine());
  m_pos = QPoint(0, 0);
  update();
}

//-----------------------------------------------------------------------------

TAffine ImageViewer::getImgToWidgetAffine() const {
  assert(m_image);
  return getImgToWidgetAffine(getImageBoundsD(m_image));
}

//-----------------------------------------------------------------------------

//! Returns the affine transform from image reference to widget's pixel one.
TAffine ImageViewer::getImgToWidgetAffine(const TRectD &geom) const {
  TPointD geomCenter((geom.x0 + geom.x1) * 0.5, (geom.y0 + geom.y1) * 0.5);

  QRect widGeom(rect());

  TPointD viewerCenter((widGeom.left() + widGeom.right() + 1) * 0.5,
                       (widGeom.top() + widGeom.bottom() + 1) * 0.5);

  return TAffine(TAffine(1.0, 0.0, 0.0, 0.0, -1.0, height()) *
                 TTranslation(viewerCenter) * m_viewAff *
                 TTranslation(-geomCenter));
}

//-----------------------------------------------------------------------------

//! Adapts image viewer's affine to display the passed image rect at maximized
//! ratio
void ImageViewer::adaptView(const TRect &imgRect, const TRect &viewRect) {
  QRect viewerRect(rect());

  double imageScale = std::min(viewerRect.width() / (double)viewRect.getLx(),
                               viewerRect.height() / (double)viewRect.getLy());

  TPointD viewRectCenter((viewRect.x0 + viewRect.x1 + 1) * 0.5,
                         (viewRect.y0 + viewRect.y1 + 1) * 0.5);
  TPointD imgRectCenter((imgRect.x0 + imgRect.x1 + 1) * 0.5,
                        (imgRect.y0 + imgRect.y1 + 1) * 0.5);

  TAffine newViewAff(TScale(imageScale, imageScale) *
                     TTranslation(imgRectCenter - viewRectCenter));
  setViewAff(newViewAff);
}

//-----------------------------------------------------------------------------

//! Adapts image viewer's affine to display the passed viewer rect at maximized
//! ratio
void ImageViewer::adaptView(const QRect &geomRect) {
  if (!m_image) return;

  // Retrieve the rect in image reference and call the associated adaptView
  TRect imgBounds(getImageBounds(m_image));
  TRectD imgBoundsD(imgBounds.x0, imgBounds.y0, imgBounds.x1 + 1,
                    imgBounds.y1 + 1);

  TRectD geomRectD(geomRect.left(), geomRect.top(), geomRect.right() + 1,
                   geomRect.bottom() + 1);
  TRectD viewRectD(getImgToWidgetAffine().inv() * geomRectD);
  TRect viewRect(tfloor(viewRectD.x0), tfloor(viewRectD.y0),
                 tceil(viewRectD.x1) - 1, tceil(viewRectD.y1) - 1);

  adaptView(imgBounds, viewRect);
}

void ImageViewer::doSwapBuffers() { glFlush(); }

void ImageViewer::changeSwapBehavior(bool enable) {
  // do nothing for now as setUpdateBehavior is not available with QGLWidget
  // setUpdateBehavior(enable ? PartialUpdate : NoPartialUpdate);
}

//-----------------------------------------------------------------------------

void ImageViewer::keyPressEvent(QKeyEvent *event) {
  if (FlipZoomer(this).exec(event)) return;

  ImageViewerShortcutReceiver(m_flipbook).exec(event);
}

//-----------------------------------------------------------------------------

void ImageViewer::onContextAboutToBeDestroyed() {
  if (!m_lutCalibrator) return;
  makeCurrent();
  m_lutCalibrator->cleanup();
  doneCurrent();
}

//-----------------------------------------------------------------------------
/*! load image from history
*/
class LoadRecentFlipbookImagesCommandHandler final : public MenuItemHandler {
public:
  LoadRecentFlipbookImagesCommandHandler()
      : MenuItemHandler(MI_LoadRecentImage) {}
  void execute() override {
    QAction *act = CommandManager::instance()->getAction(MI_LoadRecentImage);

    /*--- 右クリックで呼ばれないとここにWidgetが入らない ---*/
    FlipBook *flip = qobject_cast<FlipBook *>(act->parentWidget());
    if (!flip) return;

    DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
    int index          = menu->getTriggeredActionIndex();
    QString path =
        RecentFiles::instance()->getFilePath(index, RecentFiles::Flip);

    TFilePath fp(path.toStdWString());

    /*--- shrinkは1でロードする ---*/
    ::viewFile(fp, -1, -1, -1, 1, 0, flip, false);

    RecentFiles::instance()->moveFilePath(index, 0, RecentFiles::Flip);
  }
} loadRecentFlipbookImagesCommandHandler;

//-----------------------------------------------------------------------------
/*! clear the history
*/
class ClearRecentFlipbookImagesCommandHandler final : public MenuItemHandler {
public:
  ClearRecentFlipbookImagesCommandHandler()
      : MenuItemHandler(MI_ClearRecentImage) {}
  void execute() override {
    RecentFiles::instance()->clearRecentFilesList(RecentFiles::Flip);
  }
} clearRecentFlipbookImagesCommandHandler;
