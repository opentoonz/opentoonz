

#include "linetestcapturepane.h"
#ifdef LINETEST
#include "tapp.h"
#include "menubarcommandids.h"
#include "mainwindow.h"
#include "tsystem.h"
#include "tenv.h"
#include "viewerdraw.h"
#include "trasterimage.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tgl.h"
#include "formatsettingspopups.h"
#include "sceneviewer.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/namebuilder.h"
#include "toonz/levelset.h"
#include "toonz/tcamera.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/captureparameters.h"
#include "toonz/sceneproperties.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/stage2.h"
#include "toonzqt/gutil.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/imageutils.h"
#include "tools/toolcommandids.h"

#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QListWidget>
#include <QAction>
#include <QFileDialog>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSound>
#include <QSplitter>

//-----------------------------------------------------------------------------
namespace {

class ViewerZoomer final : public ImageUtils::ShortcutZoomer {
public:
  ViewerZoomer(QWidget *parent) : ShortcutZoomer(parent) {}
  void zoom(bool zoomin, bool resetZoom) {
    if (resetZoom)
      ((LineTestImageViewer *)getWidget())->resetView();
    else
      ((LineTestImageViewer *)getWidget())->zoomQt(zoomin, resetZoom);
  }
  void fit() {
    //((LineTestViewer*)getWidget())->fitToCamera();
  }
  void setActualPixelSize() {
    //((LineTestViewer*)getWidget())->setActualPixelSize();
  }
};

}  // namespace
//-----------------------------------------------------------------------------

ToggleCommandHandler capturePanelFieldGuideToggle(MI_CapturePanelFieldGuide,
                                                  false);

//=============================================================================
// CaptureCommand
//-----------------------------------------------------------------------------

class CaptureCommand final : public MenuItemHandler {
public:
  CaptureCommand() : MenuItemHandler(MI_Capture) {}
  void execute() {}
} CaptureCommand;

//=============================================================================
// LineTestImageViewer
//-----------------------------------------------------------------------------

LineTestImageViewer::LineTestImageViewer(QWidget *parent)
    : QWidget(parent)
    , m_pos()
    , m_mouseButton()
    , m_viewAffine()
    , m_raster()
    , m_isPainting(false)
    , m_isOnionSkinActive(false)
    , m_isViewFrameActive(false) {
  m_viewAffine = getNormalZoomScale();
  bool ret     = connect(this, SIGNAL(rasterChanged()), this, SLOT(update()));
  Q_ASSERT(ret);
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::setImage(TRasterP ras) {
  if (m_isPainting) return;
  m_raster = TRaster32P();
  m_raster = ras;
  emit rasterChanged();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::resetView() {
  m_viewAffine = getNormalZoomScale();
  update();
  emit onZoomChanged();
}

//-----------------------------------------------------------------------------

TRasterP LineTestImageViewer::getCurrentImage() const {
  TApp *app  = TApp::instance();
  int row    = app->getCurrentFrame()->getFrameIndex();
  int column = app->getCurrentColumn()->getColumnIndex();

  TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(row, column);
  if (cell.isEmpty()) return TRasterP();

  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (!sl) return TRasterP();

  TRasterImageP rasImage = sl->getFrame(cell.m_frameId, false);
  if (!rasImage) return TRasterP();

  return rasImage->getRaster();
}

//-----------------------------------------------------------------------------

TDimension LineTestImageViewer::getImageSize() const {
  ToonzScene *scene    = TApp::instance()->getCurrentScene()->getScene();
  CaptureParameters *p = scene->getProperties()->getCaptureParameters();
  return p->getResolution();
}

//-----------------------------------------------------------------------------

TAffine LineTestImageViewer::getNormalZoomScale() {
  TApp *app         = TApp::instance();
  const double inch = Stage::inch;
  TCamera *camera   = app->getCurrentScene()->getScene()->getCurrentCamera();
  double size;
  int res;
  if (camera->isXPrevalence()) {
    size = camera->getSize().lx;
    res  = camera->getRes().lx;
  } else {
    size = camera->getSize().ly;
    res  = camera->getRes().ly;
  }

  TScale cameraScale(inch * size / res);
  return cameraScale.inv();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::mouseMoveEvent(QMouseEvent *event) {
  QPoint curPos = event->pos();
  if (m_mouseButton == Qt::MidButton || m_mouseButton == Qt::LeftButton) {
    // panning
    panQt(curPos - m_pos);
    m_pos = curPos;
    return;
  }
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::mousePressEvent(QMouseEvent *event) {
  m_pos         = event->pos();
  m_mouseButton = event->button();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::mouseReleaseEvent(QMouseEvent *) {
  m_pos         = QPoint(0, 0);
  m_mouseButton = Qt::NoButton;
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::enterEvent(QEvent *) { setFocus(); }

//-----------------------------------------------------------------------------

void LineTestImageViewer::keyPressEvent(QKeyEvent *event) {
  ViewerZoomer(this).exec(event);
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu  = new QMenu(this);
  QAction *act = menu->addAction("Reset View");
  act->setShortcut(
      QKeySequence(CommandManager::instance()->getKeyFromId(T_ZoomReset)));
  connect(act, SIGNAL(triggered()), this, SLOT(resetView()));
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::wheelEvent(QWheelEvent *e) {
  if (e->orientation() == Qt::Horizontal) return;
  int delta = e->delta() > 0 ? 120 : -120;
  zoomQt(e->pos(), exp(0.001 * delta));

  emit onZoomChanged();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::panQt(const QPoint &delta) {
  if (delta == QPoint()) return;
  m_viewAffine = TTranslation(delta.x(), -delta.y()) * m_viewAffine;
  update();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::zoomQt(const QPoint &center, double factor) {
  if (factor == 1.0) return;
  TPointD delta(center.x() - width() * 0.5, -center.y() + height() * 0.5);
  double scale2 = fabs(m_viewAffine.det());
  if ((scale2 < 100000 || factor < 1) && (scale2 > 0.001 * 0.05 || factor > 1))
    m_viewAffine = TTranslation(delta) * TScale(factor) * TTranslation(-delta) *
                   m_viewAffine;
  update();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::zoomQt(bool forward, bool reset) {
  int i;
  double normalZoom = sqrt(getNormalZoomScale().det());
  double scale2     = m_viewAffine.det();
  if (reset ||
      ((scale2 < 256 || !forward) && (scale2 > 0.001 * 0.05 || forward))) {
    double oldZoomScale = sqrt(scale2);
    double zoomScale    = reset ? 1 : ImageUtils::getQuantizedZoomFactor(
                                       oldZoomScale / normalZoom, forward);
    m_viewAffine = TScale(zoomScale * normalZoom / oldZoomScale) * m_viewAffine;
  }
  update();
  emit onZoomChanged();
}

//-----------------------------------------------------------------------------

void LineTestImageViewer::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  p.save();
  p.setRenderHint(QPainter::SmoothPixmapTransform, false);
  QRect rect = visibleRegion().boundingRect();
  p.fillRect(rect, QBrush(Qt::white));
  if (!m_raster && !m_isViewFrameActive) return;

  m_isPainting = true;

  int w = width();
  int h = height();

  TAffine viewAffine = TTranslation(w * 0.5, h * 0.5) * m_viewAffine;
  QMatrix matrix(viewAffine.a11, viewAffine.a21, viewAffine.a12, viewAffine.a22,
                 viewAffine.a13, viewAffine.a23);

  QMatrix flip(1, 0, 0, -1, 0, h);
  matrix *= flip;

  if (!m_isViewFrameActive && m_raster) {
    m_raster->lock();
    QImage image = rasterToQImage(m_raster, true, false);

    QPainter::CompositionMode mode = p.compositionMode();
    if (m_isOnionSkinActive) {
      p.setMatrix(matrix, true);
      // Devo mostrare l'immagine corrente
      TRasterP currentRas = getCurrentImage();
      if (currentRas) {
        QImage currentImage = rasterToQImage(currentRas, true, false);
        p.drawImage(-currentRas->getLx() * 0.5, -currentRas->getLy() * 0.5,
                    currentImage);

        QImage mask(1, 1, QImage::Format_ARGB32_Premultiplied);
        mask.bits()[0] = 255;
        mask.bits()[1] = 0;
        mask.bits()[2] = 0;
        mask.bits()[3] = 255;

        p.setCompositionMode(QPainter::CompositionMode_Plus);
        p.drawImage(
            QRect(-currentRas->getLx() * 0.5, -currentRas->getLy() * 0.5,
                  currentRas->getLx(), currentRas->getLy()),
            mask);
        p.setCompositionMode(QPainter::CompositionMode_Multiply);
      }
      currentRas = TRasterP();
    }
    int lx = m_raster->getLx();
    int ly = m_raster->getLy();

    TDimension dim       = getImageSize();
    double xScaleFactor  = (double)dim.lx / (double)lx;
    double yScaleFactor  = (double)dim.ly / (double)ly;
    QMatrix scaledMatrix = matrix;
    p.setMatrix(scaledMatrix.scale(xScaleFactor, yScaleFactor));

    if (m_isOnionSkinActive) {
      QImage mask(1, 1, QImage::Format_ARGB32_Premultiplied);
      mask.bits()[0] = 0;
      mask.bits()[1] = 0;
      mask.bits()[2] = 255;
      mask.bits()[3] = 255;

      QPainter q(&image);

      q.setCompositionMode(QPainter::CompositionMode_SourceIn);
      q.drawImage(QRect(0, 0, lx, ly), mask);
    }

    p.drawImage(-lx * 0.5, -ly * 0.5, image);

    p.setCompositionMode(mode);

    m_raster->unlock();
  } else {
    p.setMatrix(matrix, true);
    // Devo mostrare l'immagine corrente
    TRasterP currentRas = getCurrentImage();
    if (currentRas) {
      QImage currentImage = rasterToQImage(currentRas, true, false);
      p.drawImage(-currentRas->getLx() * 0.5, -currentRas->getLy() * 0.5,
                  currentImage);
    }
    currentRas = TRasterP();
  }
  m_isPainting = false;

  // Draw Camera and Field Guide
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TDimension res = camera->getRes();
  QRectF cameraRect(-res.lx * 0.5, -res.ly * 0.5, res.lx, res.ly);
  p.setMatrix(matrix);

  if (capturePanelFieldGuideToggle.getStatus()) {
    TSceneProperties *sprop =
        TApp::instance()->getCurrentScene()->getScene()->getProperties();
    double f              = 1;
    int n                 = sprop->getFieldGuideSize();
    if (n < 4) n          = 4;
    double ar             = sprop->getFieldGuideAspectRatio();
    double cameraAr       = camera->getAspectRatio();
    TDimensionD cameraDpi = camera->getSize();
    double ux             = (double)res.lx / cameraDpi.lx * 0.5;
    double uy             = ux / ar;
    p.setPen(QColor(102, 102, 102));
    int i;
    for (i = -n; i <= n; i++) {
      QLineF hLine(QPointF(i * ux, -n * uy), QPointF(i * ux, n * uy));
      p.drawLine(hLine);
      QLineF vLine(QPointF(-n * ux, i * uy), QPointF(n * ux, i * uy));
      p.drawLine(vLine);
    }
    QLineF hLine(QPointF(-n * ux, -n * uy), QPointF(n * ux, n * uy));
    p.drawLine(hLine);
    QLineF vLine(QPointF(-n * ux, n * uy), QPointF(n * ux, -n * uy));
    p.drawLine(vLine);

    QMatrix m = matrix;
    p.setMatrix(m.scale(1, -1));
    for (i = 1; i <= n; i++) {
      QString s     = QString::number(i);
      QPointF delta = 0.03 * QPointF(ux, uy);
      QPointF point = QPointF(0, i * uy) + delta;
      p.drawText(point, s);
      point = QPointF(0, -i * uy) + delta;
      p.drawText(point, s);
      point = QPointF(-i * ux, 0) + delta;
      p.drawText(point, s);
      point = QPointF(i * ux, 0) + delta;
      p.drawText(point, s);
    }
    p.setMatrix(matrix);
  }

  if (!TApp::instance()->getCurrentFrame()->isEditingLevel()) {
    p.setPen(QPen(Qt::red));
    p.drawRect(cameraRect);
  }

  if (TnzCamera::instance()->isFreezed()) {
    QMatrix m = matrix;
    p.setMatrix(m.scale(1, -1));
    p.setPen(QPen(Qt::red));
    p.drawText(QPointF(-21, 4.5), QString("FROZEN"));
  }
  p.restore();
}

//===================================================================
// ChooseCameraDialog

ChooseCameraDialog::ChooseCameraDialog(QList<QString> cameras)
    : Dialog(TApp::instance()->getMainWindow(), true, false)
    , m_cameraIndex(-1) {
  setWindowTitle("LineTest");
  bool ret = true;
  beginVLayout();

  QListWidget *listView = new QListWidget(this);
  int i;
  for (i = 0; i < cameras.size(); i++) listView->addItem(cameras.at(i));
  if (cameras.size() > 0) m_cameraIndex = 0;
  ret = ret && connect(listView, SIGNAL(currentRowChanged(int)), this,
                       SLOT(onCurrentRowChanged(int)));

  addWidget(listView);

  endVLayout();

  QPushButton *okButton = new QPushButton(tr("Ok"), this);
  ret                   = ret &&
        connect(okButton, SIGNAL(clicked()), this, SLOT(onOkButtonPressed()));
  QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
  ret = ret && connect(cancelButton, SIGNAL(clicked()), this,
                       SLOT(onCancelButtonPressed()));

  addButtonBarWidget(okButton, cancelButton);

  Q_ASSERT(ret);
}

//-------------------------------------------------------------------

void ChooseCameraDialog::onCurrentRowChanged(int currentRow) {
  m_cameraIndex = currentRow;
}

//-------------------------------------------------------------------

void ChooseCameraDialog::onOkButtonPressed() { close(); }

//-------------------------------------------------------------------

void ChooseCameraDialog::onCancelButtonPressed() {
  m_cameraIndex = -1;
  close();
}

//===================================================================
// CaptureSettingsPopup

CaptureSettingsPopup::CaptureSettingsPopup()
    : Dialog(TApp::instance()->getMainWindow())
    , m_defineDeviceButton(0)
    , m_useWhiteImage(0)
    , m_keepWhiteImage(0)
    , m_imageWidthLineEdit(0)
    , m_imageHeightLineEdit(0)
    , m_contrastField(0)
    , m_brightnessField(0) {
  setWindowTitle("Capture Settings");
  CaptureParameters *parameters = getCaptureParameters();
  bool ret                      = true;
  beginVLayout();

  m_defineDeviceButton = new QPushButton(tr("Define Device"), this);
  m_defineDeviceButton->setFixedHeight(19);
  ret = ret && connect(m_defineDeviceButton, SIGNAL(clicked()), this,
                       SLOT(defineDevice()));
  addWidget(QString(tr("")), m_defineDeviceButton);

  QWidget *resolutionWidget = new QWidget(this);
  resolutionWidget->setMaximumHeight(DVGui::WidgetHeight);
  QHBoxLayout *resolutionLayout = new QHBoxLayout(resolutionWidget);
  resolutionLayout->setMargin(0);
  m_imageWidthLineEdit = new DVGui::IntLineEdit(
      resolutionWidget, parameters->getResolution().lx, 0, 10000);
  m_imageWidthLineEdit->setMaximumHeight(DVGui::WidgetHeight);
  ret = ret && connect(m_imageWidthLineEdit, SIGNAL(editingFinished()), this,
                       SLOT(onImageWidthEditingFinished()));
  resolutionLayout->addWidget(m_imageWidthLineEdit);
  QLabel *vResLabel = new QLabel(tr("V Resolution"), resolutionWidget);
  vResLabel->setMaximumHeight(DVGui::WidgetHeight);
  resolutionLayout->addWidget(vResLabel);
  m_imageHeightLineEdit = new DVGui::IntLineEdit(
      resolutionWidget, parameters->getResolution().ly, 0, 10000);
  m_imageHeightLineEdit->setMaximumHeight(DVGui::WidgetHeight);
  ret = ret && connect(m_imageHeightLineEdit, SIGNAL(editingFinished()), this,
                       SLOT(onImageHeightEditingFinished()));
  resolutionLayout->addWidget(m_imageHeightLineEdit);
  resolutionWidget->setLayout(resolutionLayout);
  addWidget(QString(tr("H Resolution")), resolutionWidget);

  QWidget *whiteImageWidget = new QWidget(this);
  whiteImageWidget->setMaximumHeight(DVGui::WidgetHeight);
  QHBoxLayout *whiteImageLayout = new QHBoxLayout(whiteImageWidget);
  whiteImageLayout->setMargin(0);
  m_useWhiteImage =
      new DVGui::CheckBox(tr("White Calibration"), whiteImageWidget);
  m_useWhiteImage->setMaximumHeight(DVGui::WidgetHeight);
  m_useWhiteImage->setChecked(parameters->isUseWhiteImage());
  ret = ret && connect(m_useWhiteImage, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseWhiteImageStateChanged(int)));
  whiteImageLayout->addWidget(m_useWhiteImage);
  m_keepWhiteImage = new QPushButton(tr("Capture"), m_useWhiteImage);
  ret              = ret && connect(m_keepWhiteImage, SIGNAL(clicked()), this,
                       SLOT(onKeepWhiteImage()));
  whiteImageLayout->addWidget(m_keepWhiteImage);
  whiteImageWidget->setLayout(whiteImageLayout);
  addWidget(QString(tr("")), whiteImageWidget);

  m_brightnessField = new DVGui::IntField(this);
  m_brightnessField->setRange(0, 100);
  m_brightnessField->setValue(parameters->getBrightness());
  m_brightnessField->setFixedHeight(DVGui::WidgetHeight);
  ret = ret && connect(m_brightnessField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onBrightnessChanged(bool)));
  addWidget(QString(tr("Brightness:")), m_brightnessField);

  m_contrastField = new DVGui::IntField(this);
  m_contrastField->setRange(-100, 100);
  m_contrastField->setValue(parameters->getContranst());
  m_contrastField->setFixedHeight(DVGui::WidgetHeight);
  ret = ret && connect(m_contrastField, SIGNAL(valueChanged(bool)), this,
                       SLOT(onContrastChanged(bool)));
  addWidget(QString(tr("Contrast:")), m_contrastField);

  // UsideDown Field
  m_upsideDown = new DVGui::CheckBox(tr(" Upside-down"), this);
  m_upsideDown->setMaximumSize(100, 25);
  m_upsideDown->setChecked(parameters->isUpsideDown());
  ret = ret && connect(m_upsideDown, SIGNAL(stateChanged(int)), this,
                       SLOT(onUpsideDownStateChanged(int)));
  addWidget(QString(tr("")), m_upsideDown);

  endVLayout();

  Q_ASSERT(ret);
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::showEvent(QShowEvent *) {
  TnzCamera *camera = TnzCamera::instance();
  bool ret          = true;
  ret               = ret && connect(camera, SIGNAL(imageSizeUpdated()), this,
                       SLOT(updateWidgets()));
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::hideEvent(QHideEvent *) {
  TnzCamera *camera = TnzCamera::instance();
  disconnect(camera, SIGNAL(imageSizeUpdated()), this, SLOT(updateWidgets()));
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::updateWidgets() {
  CaptureParameters *parameters = getCaptureParameters();

  m_imageWidthLineEdit->setValue(parameters->getResolution().lx);
  m_imageHeightLineEdit->setValue(parameters->getResolution().ly);

  m_useWhiteImage->setChecked(parameters->isUseWhiteImage());

  m_brightnessField->setValue(parameters->getBrightness());
  m_contrastField->setValue(parameters->getContranst());

  m_upsideDown->setChecked(parameters->isUpsideDown());
}

//-------------------------------------------------------------------

CaptureParameters *CaptureSettingsPopup::getCaptureParameters() {
  TSceneProperties *sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  Q_ASSERT(sceneProperties);
  return sceneProperties->getCaptureParameters();
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::defineDevice() {
  if (TnzCamera::instance()->isCameraConnected()) {
    DVGui::warning(tr("A Device is Connected."));
    return;
  }
  QList<QString> cameras;
  bool ret = TnzCamera::instance()->findConnectedCameras(cameras);
  if (!ret) {
    DVGui::warning(tr("No cameras found."));
    return;
  }

  ChooseCameraDialog *dialog = new ChooseCameraDialog(cameras);
  dialog->exec();

  int cameraIndex = dialog->getCurrentCameraIndex();
  if (cameraIndex == -1) return;

  CaptureParameters *parameters = getCaptureParameters();
  parameters->setDeviceName(cameras.at(cameraIndex).toStdWString());
  parameters->setResolution(TDimension(0, 0));

  TnzCamera::instance()->removeWhiteImage();

  updateWidgets();

  emit newDeviceDefined();
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onUseWhiteImageStateChanged(int state) {
  bool value = state == 2;
  m_keepWhiteImage->setEnabled(value);
  getCaptureParameters()->useWhiteImage(value);
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onKeepWhiteImage() {
  TnzCamera *camera = TnzCamera::instance();
  if (!camera->isCameraConnected()) {
    DVGui::warning(tr("Device Disconnected."));
    return;
  }
  TFilePath whiteImagePath =
      TEnv::getConfigDir() + TFilePath("whiteReferenceImage.0001.tif");
  if (TSystem::doesExistFileOrLevel(whiteImagePath)) {
    QString question;
    question = "The White Image already exists.\nDo you want to overwrite it?";
    int ret  = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return;
  }
  camera->keepWhiteImage();
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onImageWidthEditingFinished() {
  CaptureParameters *parameters = getCaptureParameters();
  if (parameters->getDeviceName().empty()) {
    DVGui::warning(tr("No Device Defined."));
    m_imageWidthLineEdit->setValue(parameters->getResolution().lx);
    return;
  }
  TnzCamera *camera           = TnzCamera::instance();
  TDimension deviceResolution = camera->getDeviceResolution();
  double aspectRatio =
      (double)deviceResolution.ly / (double)deviceResolution.lx;

  int newWidth  = m_imageWidthLineEdit->getValue();
  int newHeight = tround(newWidth * aspectRatio);
  m_imageHeightLineEdit->setValue(newHeight);

  camera->setCurrentImageSize(newWidth, newHeight);
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onImageHeightEditingFinished() {
  CaptureParameters *parameters = getCaptureParameters();
  if (parameters->getDeviceName().empty()) {
    DVGui::warning(tr("No Device Defined."));
    m_imageHeightLineEdit->setValue(parameters->getResolution().ly);
    return;
  }
  TnzCamera *camera           = TnzCamera::instance();
  TDimension deviceResolution = camera->getDeviceResolution();
  double aspectRatio =
      (double)deviceResolution.lx / (double)deviceResolution.ly;

  int newHeight = m_imageHeightLineEdit->getValue();
  int newWidth  = tround(newHeight * aspectRatio);
  m_imageWidthLineEdit->setValue(newWidth);

  camera->setCurrentImageSize(newWidth, newHeight);
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onContrastChanged(bool) {
  getCaptureParameters()->setContranst(m_contrastField->getValue());
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onBrightnessChanged(bool) {
  getCaptureParameters()->setBrightness(m_brightnessField->getValue());
}

//-------------------------------------------------------------------

void CaptureSettingsPopup::onUpsideDownStateChanged(int state) {
  getCaptureParameters()->upsideDown(state != 0);
}

//===================================================================
// FileSettingsPopup

FileSettingsPopup::FileSettingsPopup()
    : Dialog(TApp::instance()->getMainWindow())
    , m_pathField(0)
    , m_fileFormat(0) {
  setWindowTitle("File Settings");
  CaptureParameters *parameters = getCaptureParameters();
  bool ret                      = true;
  beginVLayout();

  m_pathField =
      new DVGui::FileField(this, toQString(parameters->getFilePath()));
  m_pathField->setMaximumHeight(19);
  ret = ret && connect(m_pathField, SIGNAL(pathChanged()), this,
                       SLOT(onPathChanged()));
  addWidget(QString(tr("Save in:")), m_pathField);

  QWidget *fileFormatWidget = new QWidget(this);
  fileFormatWidget->setMaximumHeight(DVGui::WidgetHeight);
  QHBoxLayout *fileFormatLayout = new QHBoxLayout(fileFormatWidget);
  fileFormatLayout->setMargin(0);

  m_fileFormat = new QComboBox(fileFormatWidget);
  QStringList formats;
  TImageWriter::getSupportedFormats(formats, true);
  //  TLevelWriter::getSupportedFormats(formats, true);
  Tiio::Writer::getSupportedFormats(formats, true);
  formats.sort();

  m_fileFormat->addItems(formats);
  m_fileFormat->setMaximumHeight(WidgetHeight);
  std::string ext = parameters->getFileFormat();
  m_fileFormat->setCurrentIndex(
      m_fileFormat->findText(QString::fromStdString(ext)));
  ret =
      ret && connect(m_fileFormat, SIGNAL(currentIndexChanged(const QString &)),
                     SLOT(onFormatChanged(const QString &)));
  fileFormatLayout->addWidget(m_fileFormat);

  QPushButton *fileFormatButton =
      new QPushButton(QString("Options"), fileFormatWidget);
  fileFormatButton->setFixedHeight(19);
  fileFormatButton->setFixedSize(60, 19);
  ret = ret && connect(fileFormatButton, SIGNAL(pressed()), this,
                       SLOT(openSettingsPopup()));
  fileFormatLayout->addWidget(fileFormatButton);

  addWidget(QString(tr("File Format:")), fileFormatWidget);

  endVLayout();
  Q_ASSERT(ret);
}

//-------------------------------------------------------------------

void FileSettingsPopup::updateWidgets() {
  CaptureParameters *parameters = getCaptureParameters();

  m_pathField->setPath(toQString(parameters->getFilePath()));

  std::string ext = parameters->getFileFormat();
  m_fileFormat->setCurrentIndex(
      m_fileFormat->findText(QString::fromStdString(ext)));
}

//-------------------------------------------------------------------

CaptureParameters *FileSettingsPopup::getCaptureParameters() {
  TSceneProperties *sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  Q_ASSERT(sceneProperties);
  return sceneProperties->getCaptureParameters();
}

//-------------------------------------------------------------------

void FileSettingsPopup::onPathChanged() {
  getCaptureParameters()->setFilePath(
      TFilePath(m_pathField->getPath().toStdWString()));
}

//-------------------------------------------------------------------

void FileSettingsPopup::onFormatChanged(const QString &str) {
  getCaptureParameters()->setFileFormat(str.toStdString());
}

//-----------------------------------------------------------------------------
/*! Open a popup output format settings popup to set current output format
    settings.
*/
void FileSettingsPopup::openSettingsPopup() {
  CaptureParameters *parameters = getCaptureParameters();
  std::string ext               = parameters->getFileFormat();
  openFormatSettingsPopup(this, ext, parameters->getFileFormatProperties(ext));
}

//=============================================================================
// LineTestCapturePane

LineTestCapturePane::LineTestCapturePane(QWidget *parent)
    : TPanel(parent)
    , m_imageView(0)
    , m_nameField(0)
    , m_frameField(0)
    , m_saveMode(0)
    , m_onionSkin(0)
    , m_incrementField(0)
    , m_stepField(0)
    , m_connectionCheckBox(0)
    , m_lineColorField(0)
    , m_captureButton(0)
    , m_captureSettingsPopup(0)
    , m_fileSettingsPopup(0)
    , m_canCapture(true)
    , m_timerId(0) {
  bool ret                      = true;
  CaptureParameters *parameters = getCaptureParameters();

  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->setObjectName("OnePixelMarginFrame");

  // First splitter widget top widget
  QWidget *topWidget     = new QWidget(splitter);
  QVBoxLayout *topLayout = new QVBoxLayout(topWidget);
  topLayout->setMargin(0);
  topLayout->setSpacing(0);

  // Add to top widget image view
  m_imageView = new LineTestImageViewer(topWidget);
  topLayout->addWidget(m_imageView, 10);
  ret = ret && connect(m_imageView, SIGNAL(onZoomChanged()),
                       SLOT(changeWindowTitle()));

  // Add to top widget separator
  DVGui::Separator *horizSeparator = new DVGui::Separator(QString(), topWidget);
  horizSeparator->setFixedHeight(1);
  topLayout->addWidget(horizSeparator);

  // Add to top widget capture widget
  QWidget *captureWidget = new QWidget(topWidget);
  captureWidget->setMaximumHeight(45);
  QHBoxLayout *captureLayout = new QHBoxLayout(captureWidget);
  captureLayout->setMargin(0);
  captureLayout->setSpacing(0);

  QWidget *optionsWidget = new QWidget(captureWidget);
  optionsWidget->setMaximumHeight(45);
  QGridLayout *optionsLayout = new QGridLayout(optionsWidget);
  optionsLayout->setMargin(0);
  optionsLayout->setSpacing(0);
  // Name field
  QLabel *nameLabel = new QLabel(tr("Name:"), optionsWidget);
  nameLabel->setMaximumHeight(19);
  optionsLayout->addWidget(nameLabel, 0, 0, 1, 1,
                           Qt::AlignRight | Qt::AlignVCenter);
  m_nameField = new DVGui::LineEdit(tr(""), optionsWidget, true);
  m_nameField->setMaximumSize(110, 19);
  optionsLayout->addWidget(m_nameField, 0, 1, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  // Frame Field
  QLabel *frameLabel = new QLabel(tr("Frame:"), optionsWidget);
  frameLabel->setMaximumHeight(DVGui::WidgetHeight);
  optionsLayout->addWidget(frameLabel, 0, 2, 1, 1,
                           Qt::AlignRight | Qt::AlignVCenter);
  m_frameField = new DVGui::IntLineEdit(optionsWidget, 1, 1, 9999, 4);
  m_frameField->setFixedSize(35, 19);
  optionsLayout->addWidget(m_frameField, 0, 3, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  // Step Field
  QLabel *incrementLabel = new QLabel(tr("Increment:"), optionsWidget);
  incrementLabel->setMaximumHeight(DVGui::WidgetHeight);
  optionsLayout->addWidget(incrementLabel, 0, 4, 1, 1,
                           Qt::AlignRight | Qt::AlignVCenter);
  m_incrementField =
      new DVGui::IntLineEdit(optionsWidget, parameters->getIncrement(), 1, 100);
  m_incrementField->setFixedSize(35, 19);
  optionsLayout->addWidget(m_incrementField, 0, 5, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  ret = ret && connect(m_incrementField, SIGNAL(editingFinished()),
                       SLOT(onIncrementFieldEditFinished()));
  // Step Widget
  QLabel *stepLabel = new QLabel(tr("Step:"), optionsWidget);
  stepLabel->setMaximumHeight(DVGui::WidgetHeight);
  optionsLayout->addWidget(stepLabel, 0, 6, 1, 1,
                           Qt::AlignRight | Qt::AlignVCenter);
  m_stepField =
      new DVGui::IntLineEdit(optionsWidget, parameters->getStep(), 1, 100);
  m_stepField->setFixedSize(35, 19);
  optionsLayout->addWidget(m_stepField, 0, 7, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  ret = ret && connect(m_stepField, SIGNAL(editingFinished()),
                       SLOT(onStepFieldEditFinished()));
  // Mode Field
  QLabel *modeLabel = new QLabel(tr("Mode:"), optionsWidget);
  modeLabel->setMaximumHeight(DVGui::WidgetHeight);
  optionsLayout->addWidget(modeLabel, 1, 0, 1, 1,
                           Qt::AlignRight | Qt::AlignVCenter);
  m_saveMode = new QComboBox(optionsWidget);
  m_saveMode->setMaximumSize(110, DVGui::WidgetHeight);
  QStringList saveMode;
  saveMode << tr("New     ") << tr("Overwite     ") << tr("Insert");
  m_saveMode->addItems(saveMode);
  ret = ret && connect(m_saveMode, SIGNAL(currentIndexChanged(int)),
                       SLOT(onSaveModeChanged(int)));
  optionsLayout->addWidget(m_saveMode, 1, 1, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  // Onion Skin Field
  m_onionSkin = new DVGui::CheckBox(tr(" Onion Skin  "), optionsWidget);
  m_onionSkin->setMaximumSize(100, 25);
  ret = ret && connect(m_onionSkin, SIGNAL(stateChanged(int)), this,
                       SLOT(onOnionSkinStateChanged(int)));
  optionsLayout->addWidget(m_onionSkin, 1, 3, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  // Onion View Frame
  m_viewFrame = new DVGui::CheckBox(tr(" View Frame"), optionsWidget);
  m_viewFrame->setMaximumSize(100, 25);
  ret = ret && connect(m_viewFrame, SIGNAL(stateChanged(int)), this,
                       SLOT(onViewFrameStateChanged(int)));
  optionsLayout->addWidget(m_viewFrame, 1, 4, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);
  // Line Colo Field
  QLabel *fadeLabel = new QLabel(tr("Fade:"), optionsWidget);
  fadeLabel->setMaximumHeight(DVGui::WidgetHeight);
  optionsLayout->addWidget(fadeLabel, 1, 5, 1, 1,
                           Qt::AlignRight | Qt::AlignVCenter);
  m_lineColorField =
      new DVGui::ColorField(optionsWidget, false, TPixel32(), 18);
  m_lineColorField->setFixedHeight(18);
  m_lineColorField->hideChannelsFields(true);
  ret = ret &&
        connect(m_lineColorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(onLinesColorChanged(const TPixel32 &, bool)));
  optionsLayout->addWidget(m_lineColorField, 1, 6, 1, 1,
                           Qt::AlignLeft | Qt::AlignVCenter);

  optionsWidget->setLayout(optionsLayout);
  captureLayout->addWidget(optionsWidget, Qt::AlignCenter);
  captureLayout->setStretchFactor(optionsWidget, 20);

  DVGui::Separator *separator = new DVGui::Separator(QString(), captureWidget);
  separator->setFixedSize(1, 80);
  separator->setOrientation(false);
  captureLayout->addWidget(separator);

  QWidget *captureButtonsWidget     = new QWidget(captureWidget);
  QVBoxLayout *captureButtonsLayout = new QVBoxLayout(captureButtonsWidget);
  captureButtonsLayout->setMargin(2);
  captureButtonsLayout->setSpacing(0);
  // Connection Button
  m_connectionCheckBox =
      new DVGui::CheckBox(tr(" Connection"), captureButtonsWidget);
  m_connectionCheckBox->setMaximumSize(100, 19);
  ret = ret && connect(m_connectionCheckBox, SIGNAL(stateChanged(int)), this,
                       SLOT(onConnectCheckboxStateChanged(int)));
  captureButtonsLayout->addWidget(m_connectionCheckBox, Qt::AlignCenter);
  // Capture Button
  m_captureButton =
      new QPushButton(tr("       Capture       "), captureButtonsWidget);
  m_captureButton->setMaximumSize(100, 19);
  ret = ret && connect(m_captureButton, SIGNAL(clicked()), this,
                       SLOT(captureButton()));
  captureButtonsLayout->addWidget(m_captureButton, Qt::AlignCenter);

  captureButtonsWidget->setLayout(captureButtonsLayout);
  captureLayout->addWidget(captureButtonsWidget, Qt::AlignCenter);
  captureLayout->setStretchFactor(captureButtonsWidget, 1);

  captureWidget->setLayout(captureLayout);
  topLayout->addWidget(captureWidget);

  topWidget->setLayout(topLayout);

  splitter->addWidget(topWidget);

  // Second splitter widget
  QWidget *bottomWidget = new QWidget(splitter);
  bottomWidget->setFixedHeight(23);
  QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);
  bottomLayout->setMargin(0);
  bottomLayout->setSpacing(0);
  // Capture Settings Button
  QPushButton *captureSettingsButton =
      new QPushButton(tr("Capture Settings"), bottomWidget);
  captureSettingsButton->setMaximumSize(100, 19);
  ret = ret && connect(captureSettingsButton, SIGNAL(clicked()), this,
                       SLOT(showCaptureSettings()));
  bottomLayout->addWidget(captureSettingsButton, Qt::AlignHCenter);
  // File Settings Button
  QPushButton *fileSettingsButton =
      new QPushButton(tr("   File Settings    "), bottomWidget);
  fileSettingsButton->setMaximumSize(100, 19);
  ret = ret && connect(fileSettingsButton, SIGNAL(clicked()), this,
                       SLOT(showFileSettings()));
  bottomLayout->addWidget(fileSettingsButton, Qt::AlignHCenter);

  bottomWidget->setLayout(bottomLayout);
  splitter->addWidget(bottomWidget);

  setWidget(splitter);

  initializeTitleBar(getTitleBar());

  // Lo sceneSwitched() va aggiornato anche quando il popup non e' visibile
  // altrimenti ci si trova in una situazione inconsistente
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(sceneSwitched()), this, SLOT(onSceneSwitched()));

  changeWindowTitle();

  Q_ASSERT(ret);
}

//-------------------------------------------------------------------

CaptureParameters *LineTestCapturePane::getCaptureParameters() {
  TSceneProperties *sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  Q_ASSERT(sceneProperties);
  return sceneProperties->getCaptureParameters();
}

//-------------------------------------------------------------------

void LineTestCapturePane::closeEvent(QCloseEvent *) {
  TnzCamera::instance()->cameraDisconnect();
}

//-------------------------------------------------------------------

void LineTestCapturePane::showEvent(QShowEvent *) {
  updateFileField();
  TApp *app = TApp::instance();

  TXsheetHandle *xshHandle = app->getCurrentXsheet();
  bool ret = connect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()),
                     this, SLOT(updateFileField()));
  ret = ret && connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
                       SLOT(updateFileField()));
  ret = ret && connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
                       SLOT(updateFileField()));
  ret = ret && connect(TnzCamera::instance(), SIGNAL(captureFinished()), this,
                       SLOT(onCaptureFinished()));

  QAction *act = CommandManager::instance()->getAction(MI_Capture);
  ret          = ret &&
        connect(act, SIGNAL(triggered()), m_captureButton, SIGNAL(clicked()));

  TnzCamera *camera = TnzCamera::instance();
  camera->setScene(app->getCurrentScene()->getScene());

  assert(ret);
}

//-------------------------------------------------------------------

void LineTestCapturePane::hideEvent(QHideEvent *) {
  TApp *app                = TApp::instance();
  TXsheetHandle *xshHandle = app->getCurrentXsheet();
  disconnect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
             SLOT(updateFileField()));
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
             SLOT(updateFileField()));
  disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(updateFileField()));
  disconnect(TnzCamera::instance(), SIGNAL(captureFinished()), this,
             SLOT(onCaptureFinished()));

  QAction *act = CommandManager::instance()->getAction(MI_Capture);
  disconnect(act, SIGNAL(triggered()), m_captureButton, SIGNAL(clicked()));
}

//-------------------------------------------------------------------

void LineTestCapturePane::timerEvent(QTimerEvent *e) {
  if (m_timerId <= 0) return;
  killTimer(m_timerId);
  m_timerId    = 0;
  m_canCapture = true;
}

//-------------------------------------------------------------------

bool LineTestCapturePane::capture() {
  TApp *app          = TApp::instance();
  int row            = app->getCurrentFrame()->getFrameIndex();
  int col            = app->getCurrentColumn()->getColumnIndex();
  TXsheet *xsheet    = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsheet->getColumn(col);
  TXshCell cell      = xsheet->getCell(row, col);
  if ((column && !column->getLevelColumn()) ||
      (!cell.isEmpty() && cell.getChildLevel() != 0)) {
    DVGui::error(tr("Bad Selection."));
    return false;
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  bool ret = TnzCamera::instance()->onRelease(
      TFrameId(m_frameField->text().toInt()),
      m_nameField->text().toStdWString(), row, col);
  QApplication::restoreOverrideCursor();
  return ret;
}

//-------------------------------------------------------------------

void LineTestCapturePane::setFrameField(TFrameId fid) {
  int number = fid.getNumber();
  QString newStr;
  if (number < 10)
    newStr = QString("000");
  else if (number < 100)
    newStr = QString("00");
  else if (number < 1000)
    newStr = QString("0");
  m_frameField->setText(newStr + QString::number(number));
}

//-------------------------------------------------------------------

void LineTestCapturePane::initializeTitleBar(TPanelTitleBar *titleBar) {
  bool ret = true;

  int x         = -50;
  int iconWidth = 20;
  TPanelTitleBarButton *button;
  button = new TPanelTitleBarButton(titleBar, ":Resources/pane_freeze_off.svg",
                                    ":Resources/pane_freeze_over.svg",
                                    ":Resources/pane_freeze_on.svg");
  button->setToolTip("Freeze");
  titleBar->add(QPoint(x, 0), button);
  ret = ret && connect(button, SIGNAL(toggled(bool)), this, SLOT(freeze(bool)));

  assert(ret);
}

//-------------------------------------------------------------------

void LineTestCapturePane::showCaptureSettings() {
  if (!m_captureSettingsPopup) {
    m_captureSettingsPopup = new CaptureSettingsPopup();
    bool ret = connect(m_captureSettingsPopup, SIGNAL(newDeviceDefined()),
                       m_connectionCheckBox, SLOT(toggle()));
    assert(ret);
  }
  m_captureSettingsPopup->show();
  m_captureSettingsPopup->raise();
}

//-------------------------------------------------------------------

void LineTestCapturePane::showFileSettings() {
  if (!m_fileSettingsPopup) m_fileSettingsPopup = new FileSettingsPopup();
  m_fileSettingsPopup->show();
  m_fileSettingsPopup->raise();
}

//-------------------------------------------------------------------

void LineTestCapturePane::onSaveModeChanged(int value) {
  TnzCamera::instance()->setSaveType(value);
}

//-------------------------------------------------------------------

void LineTestCapturePane::onIncrementFieldEditFinished() {
  getCaptureParameters()->setIncrement(m_incrementField->getValue());
}

//-------------------------------------------------------------------

void LineTestCapturePane::onStepFieldEditFinished() {
  getCaptureParameters()->setStep(m_stepField->getValue());
}

//-------------------------------------------------------------------

void LineTestCapturePane::onOnionSkinStateChanged(int state) {
  bool checked = state != Qt::Unchecked;
  if (checked) m_viewFrame->setCheckState(Qt::Unchecked);
  m_imageView->setOnionSkinActive(checked);
}

//-------------------------------------------------------------------

void LineTestCapturePane::onViewFrameStateChanged(int state) {
  bool checked = state != Qt::Unchecked;
  if (checked) m_onionSkin->setCheckState(Qt::Unchecked);
  m_imageView->setViewFrameActive(checked);
  if (!m_connectionCheckBox->isChecked()) m_imageView->repaint();
}

//-------------------------------------------------------------------

void LineTestCapturePane::onLinesColorChanged(const TPixel32 &color, bool) {
  TnzCamera::instance()->setLinesColor(color);
}

//-------------------------------------------------------------------

void LineTestCapturePane::onConnectCheckboxStateChanged(int state) {
  if (state) {
    TSceneProperties *sceneProperties =
        TApp::instance()->getCurrentScene()->getScene()->getProperties();
    Q_ASSERT(sceneProperties);
    std::wstring deviceName =
        sceneProperties->getCaptureParameters()->getDeviceName();

    if (deviceName.empty()) {
      DVGui::warning(tr("No Device Defined."));
      m_connectionCheckBox->setChecked(false);
      return;
    }
    bool ret = TnzCamera::instance()->cameraConnect(deviceName);
    if (!ret) {
      DVGui::warning(tr("Cannot connect Camera"));
      m_connectionCheckBox->setChecked(false);
    } else
      TnzCamera::instance()->onViewfinder(m_imageView);
  } else {
    TnzCamera::instance()->cameraDisconnect();
  }
}

//-------------------------------------------------------------------

void LineTestCapturePane::captureButton() {
  // Se il bottone di capture e' stato cliccato da meno di 500 ms non consento
  // una nuova acquisizione.
  if (!m_canCapture) return;

  if (!TnzCamera::instance()->isCameraConnected()) {
    DVGui::warning(tr("Device Disconnected."));
    return;
  }
  QString clickPath =
      QString::fromStdString(TEnv::getApplicationName()) + QString(" ") +
      QString::fromStdString(TEnv::getApplicationVersion()) + QString(".app") +
      QString("/Contents/Resources/click.wav");
  QSound::play(clickPath);

  bool isDummyLevel = false;
  TApp *app         = TApp::instance();
  int row           = app->getCurrentFrame()->getFrameIndex();
  int col           = app->getCurrentColumn()->getColumnIndex();
  TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
  TXshCell cell     = xsh->getCell(row, col);
  if (!cell.isEmpty()) {
    TXshSimpleLevel *sl                                = cell.getSimpleLevel();
    if (sl && !sl->isFid(cell.m_frameId)) isDummyLevel = true;
  }
  bool ret = capture();
  if (!ret) return;

  app->getCurrentSelection()->setSelection(0);
  disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(updateFileField()));
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
             SLOT(updateFileField()));

  int step = getCaptureParameters()->getStep();
  if (isDummyLevel) {
    step              = 0;
    int frameIdNumber = cell.m_frameId.getNumber();
    while (frameIdNumber == cell.m_frameId.getNumber()) {
      ++step;
      cell = xsh->getCell(row + step, col);
    }
  }
  TApp::instance()->getCurrentFrame()->setFrameIndex(row + step);

  QString str = m_frameField->text();
  int number  = str.toInt();
  number += getCaptureParameters()->getIncrement();
  TFrameId fid(number);

  // controllo se esiste gia' un livello con frame number diversi.
  // Nel qual ccaso devo utilizzare il FrameId presente nella cella.
  row                = TApp::instance()->getCurrentFrame()->getFrameIndex();
  TXshColumn *column = xsh->getColumn(col);
  if (column) {
    TXshCell cell            = column->getCellColumn()->getCell(row);
    if (!cell.isEmpty()) fid = cell.getFrameId();
  }

  setFrameField(fid);

  bool connectionRet = connect(app->getCurrentFrame(), SIGNAL(frameSwitched()),
                               this, SLOT(updateFileField()));
  connectionRet =
      connectionRet && connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()),
                               this, SLOT(updateFileField()));
  assert(connectionRet);

  m_canCapture = false;
  m_timerId    = startTimer(500);
}

//-------------------------------------------------------------------

void LineTestCapturePane::updateFileField() {
  QString parentDir;
  QString fileName;
  TFrameId fid;
  TApp *app          = TApp::instance();
  TXsheet *xsheet    = app->getCurrentXsheet()->getXsheet();
  int col            = app->getCurrentColumn()->getColumnIndex();
  TXshColumn *column = xsheet->getColumn(col);
  if (!column || column->getLevelColumn()) {
    int row           = app->getCurrentFrame()->getFrameIndex();
    TXshCell cell     = xsheet->getCell(row, col);
    TXshCell precCell = row > 0 ? xsheet->getCell(row - 1, col) : TXshCell();
    if (!cell.isEmpty()) {
      TXshSimpleLevel *sl = cell.getSimpleLevel();
      if (sl) {
        parentDir = toQString(sl->getPath().getParentDir());
        fileName  = QString::fromStdWString(sl->getName());
        fid       = cell.getFrameId();
      }
    } else if (!precCell.isEmpty() && precCell.getSimpleLevel()) {
      TXshSimpleLevel *sl = precCell.getSimpleLevel();
      parentDir           = toQString(sl->getPath().getParentDir());
      fileName            = QString::fromStdWString(sl->getName());
      fid                 = TFrameId(precCell.getFrameId().getNumber() + 1);
    } else {
      ToonzScene *scene   = xsheet->getScene();
      TLevelSet *levelSet = scene->getLevelSet();
      std::wstring levelName;
      NameBuilder *nameBuilder = NameBuilder::getBuilder(levelName);
      TFilePath fp;
      for (;;) {
        levelName = nameBuilder->getNext();
        if (levelSet->getLevel(levelName) != 0) continue;
        fp = scene->getDefaultLevelPath(OVL_XSHLEVEL, levelName);
        TFilePath actualFp = scene->decodeFilePath(fp);
        break;
      }

      delete nameBuilder;
      nameBuilder = 0;

      fileName = QString::fromStdWString(levelName);
      TFilePath defaultPath =
          scene->getDefaultLevelPath(OVL_XSHLEVEL).getParentDir();
      parentDir = toQString(defaultPath);
      fid       = TFrameId(1);
    }
  }

  m_nameField->setText(fileName);
  setFrameField(fid);
  if (!m_connectionCheckBox->isChecked()) m_imageView->repaint();
}

//-------------------------------------------------------------------

void LineTestCapturePane::onCaptureFinished() {
  TApp *app = TApp::instance();
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
             SLOT(updateFileField()));

  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentLevel()->notifyLevelChange();

  bool ret = connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
                     SLOT(updateFileField()));
  assert(ret);

  TnzCamera *camera = TnzCamera::instance();
  IconGenerator::instance()->invalidate(camera->getCurrentLevel(),
                                        camera->getCurrentFid());
}

//-------------------------------------------------------------------

void LineTestCapturePane::onSceneSwitched() {
  if (m_captureSettingsPopup) m_captureSettingsPopup->updateWidgets();
  if (m_fileSettingsPopup) m_fileSettingsPopup->updateWidgets();

  TnzCamera *camera = TnzCamera::instance();
  if (camera->isCameraConnected()) camera->cameraDisconnect();

  updateFileField();

  m_saveMode->setCurrentIndex(0);
  camera->setSaveType(0);

  m_incrementField->setValue(getCaptureParameters()->getIncrement());
  m_stepField->setValue(getCaptureParameters()->getStep());

  m_lineColorField->setColor(TPixel32());
  camera->setLinesColor(TPixel32());

  m_connectionCheckBox->setChecked(false);

  m_onionSkin->setChecked(false);
  m_imageView->setOnionSkinActive(false);

  m_imageView->resetView();

  camera->setScene(TApp::instance()->getCurrentScene()->getScene());
  changeWindowTitle();
}

//-------------------------------------------------------------------

void LineTestCapturePane::changeWindowTitle() {
  QString name = tr("LineTest Capture");
  TAffine aff =
      m_imageView->getViewMatrix() * m_imageView->getNormalZoomScale().inv();
  name = name + "  Zoom : " + QString::number(tround(100.0 * sqrt(aff.det()))) +
         "%";
  setWindowTitle(name);
}

//-------------------------------------------------------------------

void LineTestCapturePane::freeze(bool value) {
  TnzCamera::instance()->freeze(value);
  m_imageView->update();
}

#endif
