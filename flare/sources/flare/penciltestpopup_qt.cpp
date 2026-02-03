#include "penciltestpopup_qt.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "filebrowsermodel.h"
#include "cellselection.h"
#include "toonzqt/tselectionhandle.h"
#include "cameracapturelevelcontrol_qt.h"
#include "iocommand.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/gutil.h"

// Tnzlib includes
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toutputproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/levelset.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzfolders.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/levelproperties.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "toonz/filepathproperties.h"

// TnzCore includes
#include "tsystem.h"
#include "tpixelutils.h"
#include "tenv.h"
#include "tlevel_io.h"

#include <algorithm>

// Qt includes
#include <QMainWindow>
#include <QCameraInfo>
#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraViewfinderSettings>
#ifdef MACOSX
#include <QCameraViewfinder>
#endif

#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QRadioButton>
#include <QSlider>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QToolButton>
#include <QDateTime>
#include <QMultimedia>
#include <QPainter>
#include <QKeyEvent>
#include <QCommonStyle>
#include <QTimer>
#include <QIntValidator>
#include <QRegExpValidator>

#include <QVideoSurfaceFormat>
#include <QThreadPool>

#ifdef _WIN32
#include <dshow.h>
#endif

using namespace DVGui;

// Connected camera
TEnv::StringVar CamCapCameraName("CamCapCameraName", "");
// Camera resolution
TEnv::StringVar CamCapCameraResolution("CamCapCameraResolution", "");
// Whether to open save-in popup on launch
TEnv::IntVar CamCapOpenSaveInPopupOnLaunch("CamCapOpenSaveInPopupOnLaunch", 0);
// SaveInFolderPopup settings
TEnv::StringVar CamCapSaveInParentFolder("CamCapSaveInParentFolder", "");
TEnv::IntVar CamCapSaveInPopupSubFolder("CamCapSaveInPopupSubFolder", 0);
TEnv::StringVar CamCapSaveInPopupProject("CamCapSaveInPopupProject", "");
TEnv::StringVar CamCapSaveInPopupEpisode("CamCapSaveInPopupEpisode", "1");
TEnv::StringVar CamCapSaveInPopupSequence("CamCapSaveInPopupSequence", "1");
TEnv::StringVar CamCapSaveInPopupScene("CamCapSaveInPopupScene", "1");
TEnv::IntVar CamCapSaveInPopupAutoSubName("CamCapSaveInPopupAutoSubName", 1);
TEnv::IntVar CamCapSaveInPopupCreateSceneInFolder(
    "CamCapSaveInPopupCreateSceneInFolder", 0);

namespace {

void convertImageToRaster(TRaster32P dstRas, const QImage& srcImg) {
  dstRas->lock();
  int lx = dstRas->getLx();
  int ly = dstRas->getLy();
  assert(lx == srcImg.width() && ly == srcImg.height());
  for (int j = 0; j < ly; j++) {
    TPixel32* dstPix = dstRas->pixels(j);
    for (int i = 0; i < lx; i++, dstPix++) {
      QRgb srcPix = srcImg.pixel(lx - 1 - i, j);
      dstPix->r   = qRed(srcPix);
      dstPix->g   = qGreen(srcPix);
      dstPix->b   = qBlue(srcPix);
      dstPix->m   = TPixel32::maxChannelValue;
    }
  }
  dstRas->unlock();
}

void bgReduction(QImage& srcImg, QImage& bgImg, int reduction) {
  float reductionRatio = (float)reduction / 100.0f;
  // first, make the reduction table
  std::vector<int> reductionAmount(256);
  for (int i = 0; i < reductionAmount.size(); i++) {
    reductionAmount[i] = (int)(std::floor((float)(255 - i) * reductionRatio));
  }
  // then, compute for all pixels
  int lx = srcImg.width();
  int ly = srcImg.height();
  for (int j = 0; j < ly; j++) {
    // TPixel32 * pix = ras->pixels(j);
    QRgb* pix   = (QRgb*)srcImg.scanLine(j);
    QRgb* bgPix = (QRgb*)bgImg.scanLine(j);
    for (int i = 0; i < lx; i++, pix++, bgPix++) {
      *pix = qRgb(std::min(255, qRed(*pix) + reductionAmount[qRed(*bgPix)]),
                  std::min(255, qGreen(*pix) + reductionAmount[qGreen(*bgPix)]),
                  std::min(255, qBlue(*pix) + reductionAmount[qBlue(*bgPix)]));
    }
  }
}

void my_compute_lut(int black, int white, float gamma, std::vector<int>& lut) {
  const int maxChannelValue         = lut.size() - 1;
  const float half_maxChannelValueF = 0.5f * maxChannelValue;
  const float maxChannelValueF      = maxChannelValue;

  float value;

  int lutSize = lut.size();
  for (int i = 0; i < lutSize; i++) {
    if (i <= black)
      value = 0.0f;
    else if (i >= white)
      value = 1.0f;
    else {
      value = (float)(i - black) / (float)(white - black);
      value = std::pow(value, 1.0f / gamma);
    }

    lut[i] = (int)std::floor(value * maxChannelValueF);
  }
}

//-----------------------------------------------------------------------------

inline void doPixGray(QRgb* pix, const std::vector<int>& lut) {
  int gray = lut[qGray(*pix)];
  *pix     = qRgb(gray, gray, gray);
}

//-----------------------------------------------------------------------------

inline void doPixBinary(QRgb* pix, int threshold) {
  int gray = qGray(*pix);
  if (gray >= threshold)
    gray = 255;
  else
    gray = 0;
  *pix = qRgb(gray, gray, gray);
}

//-----------------------------------------------------------------------------

inline void doPix(QRgb* pix, const std::vector<int>& lut) {
  // The captured image MUST be full opaque!
  *pix = qRgb(lut[qRed(*pix)], lut[qGreen(*pix)], lut[qBlue(*pix)]);
}

//-----------------------------------------------------------------------------

void onChange(QImage& img, int black, int white, float gamma, bool doGray) {
  std::vector<int> lut(TPixel32::maxChannelValue + 1);
  my_compute_lut(black, white, gamma, lut);

  int ly = img.height();
  // compute in multi thread
  int threadCount =
      std::max(1, QThreadPool::globalInstance()->maxThreadCount() / 2);
  int tmpStart = 0;
  for (int t = 0; t < threadCount; t++) {
    int tmpEnd = (int)std::round((float)(ly * (t + 1)) / (float)threadCount);

    QRunnable* task;
    if (doGray)
      task = new ApplyGrayLutTask(tmpStart, tmpEnd, img, lut);
    else
      task = new ApplyLutTask(tmpStart, tmpEnd, img, lut);

    QThreadPool::globalInstance()->start(task);

    tmpStart = tmpEnd;
  }

  QThreadPool::globalInstance()->waitForDone();
}

//-----------------------------------------------------------------------------

void onChangeBW(QImage& img, int threshold) {
  int lx = img.width(), y, ly = img.height();
  for (y = 0; y < ly; ++y) {
    QRgb *pix = (QRgb*)img.scanLine(y), *endPix = (QRgb*)(pix + lx);
    while (pix < endPix) {
      doPixBinary(pix, threshold);
      ++pix;
    }
  }
}

//-----------------------------------------------------------------------------

TPointD getCurrentCameraDpi() {
  TCamera* camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TDimensionD size = camera->getSize();
  TDimension res   = camera->getRes();
  return TPointD(res.lx / size.lx, res.ly / size.ly);
}

//-----------------------------------------------------------------------------

QChar numToLetter(int letterNum) {
  switch (letterNum) {
  case 0:
    return QChar();
    break;
  case 1:
    return 'A';
    break;
  case 2:
    return 'B';
    break;
  case 3:
    return 'C';
    break;
  case 4:
    return 'D';
    break;
  case 5:
    return 'E';
    break;
  case 6:
    return 'F';
    break;
  case 7:
    return 'G';
    break;
  case 8:
    return 'H';
    break;
  case 9:
    return 'I';
    break;
  default:
    return QChar();
    break;
  }
}

int letterToNum(QChar appendix) {
  if (appendix == QChar('A') || appendix == QChar('a'))
    return 1;
  else if (appendix == QChar('B') || appendix == QChar('b'))
    return 2;
  else if (appendix == QChar('C') || appendix == QChar('c'))
    return 3;
  else if (appendix == QChar('D') || appendix == QChar('d'))
    return 4;
  else if (appendix == QChar('E') || appendix == QChar('e'))
    return 5;
  else if (appendix == QChar('F') || appendix == QChar('f'))
    return 6;
  else if (appendix == QChar('G') || appendix == QChar('g'))
    return 7;
  else if (appendix == QChar('H') || appendix == QChar('h'))
    return 8;
  else if (appendix == QChar('I') || appendix == QChar('i'))
    return 9;
  else
    return 0;
}

#ifdef _WIN32
void openCaptureFilterSettings(const QWidget* parent,
                               const QString& cameraName) {
  HRESULT hr;

  ICreateDevEnum* createDevEnum = NULL;
  IEnumMoniker* enumMoniker     = NULL;
  IMoniker* moniker             = NULL;

  IBaseFilter* deviceFilter;

  ISpecifyPropertyPages* specifyPropertyPages;
  CAUUID cauuid;
  // set parent's window handle in order to make the dialog modal
  HWND ghwndApp = (HWND)(parent->winId());

  // initialize COM
  CoInitialize(NULL);

  // get device list
  CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                   IID_ICreateDevEnum, (PVOID*)&createDevEnum);

  // create EnumMoniker
  createDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       &enumMoniker, 0);
  if (enumMoniker == NULL) {
    // if no connected devices found
    return;
  }

  // reset EnumMoniker
  enumMoniker->Reset();

  // find target camera
  ULONG fetched      = 0;
  bool isCameraFound = false;
  while (hr = enumMoniker->Next(1, &moniker, &fetched), hr == S_OK) {
    // get friendly name (= device name) of the camera
    IPropertyBag* pPropertyBag;
    moniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropertyBag);
    VARIANT var;
    var.vt = VT_BSTR;
    VariantInit(&var);

    pPropertyBag->Read(L"FriendlyName", &var, 0);

    QString deviceName = QString::fromWCharArray(var.bstrVal);

    VariantClear(&var);

    if (deviceName == cameraName) {
      // bind monkier to the filter
      moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&deviceFilter);

      // release moniker etc.
      moniker->Release();
      enumMoniker->Release();
      createDevEnum->Release();

      isCameraFound = true;
      break;
    }
  }

  // if no matching camera found
  if (!isCameraFound) return;

  // open capture filter popup
  hr = deviceFilter->QueryInterface(IID_ISpecifyPropertyPages,
                                    (void**)&specifyPropertyPages);
  if (hr == S_OK) {
    hr = specifyPropertyPages->GetPages(&cauuid);

    hr = OleCreatePropertyFrame(ghwndApp, 30, 30, NULL, 1,
                                (IUnknown**)&deviceFilter, cauuid.cElems,
                                (GUID*)cauuid.pElems, 0, 0, NULL);

    CoTaskMemFree(cauuid.pElems);
    specifyPropertyPages->Release();
  }
}
#endif

QString convertToFrameWithLetter(int value, int length = -1) {
  QString str;
  str.setNum((int)(value / 10));
  while (str.length() < length) str.push_front("0");
  QChar letter = numToLetter(value % 10);
  if (!letter.isNull()) str.append(letter);
  return str;
}

QString fidsToString(const std::vector<TFrameId>& fids,
                     bool letterOptionEnabled) {
  if (fids.empty()) return PencilTestPopup::tr("No", "frame id");
  QString retStr("");
  if (letterOptionEnabled) {
    bool beginBlock = true;
    for (int f = 0; f < fids.size() - 1; f++) {
      int num      = fids[f].getNumber();
      int next_num = fids[f + 1].getNumber();

      if (num % 10 == 0 && num + 10 == next_num) {
        if (beginBlock) {
          retStr += convertToFrameWithLetter(num) + " - ";
          beginBlock = false;
        }
      } else {
        retStr += convertToFrameWithLetter(num) + ", ";
        beginBlock = true;
      }
    }
    retStr += convertToFrameWithLetter(fids.back().getNumber());
  } else {
    bool beginBlock = true;
    for (int f = 0; f < fids.size() - 1; f++) {
      int num      = fids[f].getNumber();
      int next_num = fids[f + 1].getNumber();
      if (num + 1 == next_num) {
        if (beginBlock) {
          retStr += QString::number(num) + " - ";
          beginBlock = false;
        }
      } else {
        retStr += QString::number(num) + ", ";
        beginBlock = true;
      }
    }
    retStr += QString::number(fids.back().getNumber());
  }
  return retStr;
}

bool findCell(TXsheet* xsh, int col, const TXshCell& targetCell,
              int& bottomRowWithTheSameLevel) {
  bottomRowWithTheSameLevel = -1;
  TXshColumnP column        = const_cast<TXsheet*>(xsh)->getColumn(col);
  if (!column) return false;

  TXshCellColumn* cellColumn = column->getCellColumn();
  if (!cellColumn) return false;

  int r0, r1;
  if (!cellColumn->getRange(r0, r1)) return false;

  for (int r = r0; r <= r1; r++) {
    TXshCell cell = cellColumn->getCell(r);
    if (cell == targetCell) return true;
    if (cell.m_level == targetCell.m_level) bottomRowWithTheSameLevel = r;
  }

  return false;
}

bool getRasterLevelSize(TXshLevel* level, TDimension& dim) {
  std::vector<TFrameId> fids;
  level->getFids(fids);
  if (fids.empty()) return false;
  TXshSimpleLevel* simpleLevel = level->getSimpleLevel();
  if (!simpleLevel) return false;
  TRasterImageP rimg = (TRasterImageP)simpleLevel->getFrame(fids[0], false);
  if (!rimg || rimg->isEmpty()) return false;

  dim = rimg->getRaster()->getSize();
  return true;
}

}  // namespace

//=============================================================================

void ApplyLutTask::run() {
  int lx = m_img.width();
  for (int y = m_fromY; y < m_toY; ++y) {
    QRgb *pix = (QRgb*)m_img.scanLine(y), *endPix = (QRgb*)(pix + lx);
    while (pix < endPix) {
      doPix(pix, m_lut);
      ++pix;
    }
  }
}

void ApplyGrayLutTask::run() {
  int lx = m_img.width();
  for (int y = m_fromY; y < m_toY; ++y) {
    QRgb *pix = (QRgb*)m_img.scanLine(y), *endPix = (QRgb*)(pix + lx);
    while (pix < endPix) {
      doPixGray(pix, m_lut);
      ++pix;
    }
  }
}

//=============================================================================

MyVideoSurface::MyVideoSurface(QWidget* widget, QObject* parent)
    : QAbstractVideoSurface(parent)
    , m_widget(widget)
    , m_imageFormat(QImage::Format_Invalid) {}

QList<QVideoFrame::PixelFormat> MyVideoSurface::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const {
  if (handleType == QAbstractVideoBuffer::NoHandle) {
    return QList<QVideoFrame::PixelFormat>()
           << QVideoFrame::Format_RGB32 << QVideoFrame::Format_ARGB32
           << QVideoFrame::Format_ARGB32_Premultiplied
           << QVideoFrame::Format_RGB565 << QVideoFrame::Format_RGB555;
  } else {
    return QList<QVideoFrame::PixelFormat>();
  }
}

bool MyVideoSurface::isFormatSupported(const QVideoSurfaceFormat& format,
                                       QVideoSurfaceFormat* similar) const {
  Q_UNUSED(similar);

  const QImage::Format imageFormat =
      QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
  const QSize size = format.frameSize();

  return imageFormat != QImage::Format_Invalid && !size.isEmpty() &&
         format.handleType() == QAbstractVideoBuffer::NoHandle;
}

bool MyVideoSurface::start(const QVideoSurfaceFormat& format) {
  const QImage::Format imageFormat =
      QVideoFrame::imageFormatFromPixelFormat(format.pixelFormat());
  const QSize size = format.frameSize();

  if (imageFormat != QImage::Format_Invalid && !size.isEmpty()) {
    m_imageFormat = imageFormat;
    m_imageSize   = size;
    m_sourceRect  = format.viewport();

    QAbstractVideoSurface::start(format);

    m_widget->updateGeometry();
    updateVideoRect();

    return true;
  } else {
    return false;
  }
}

void MyVideoSurface::updateVideoRect() {
  QSize size = surfaceFormat().sizeHint();
  size.scale(m_widget->size(), Qt::KeepAspectRatio);

  m_targetRect = QRect(QPoint(0, 0), size);
  m_targetRect.moveCenter(m_widget->rect().center());

  double scale =
      (double)m_targetRect.width() / (double)surfaceFormat().sizeHint().width();
  m_S2V_Transform =
      QTransform::fromTranslate(m_targetRect.left(), m_targetRect.top())
          .scale(scale, scale);
}

bool MyVideoSurface::present(const QVideoFrame& frame) {
  if (surfaceFormat().pixelFormat() != frame.pixelFormat() ||
      surfaceFormat().frameSize() != frame.size()) {
    setError(IncorrectFormatError);
    stop();
    return false;
  } else {
    m_currentFrame = frame;

    if (m_currentFrame.map(QAbstractVideoBuffer::ReadOnly)) {
      QImage image = QImage(m_currentFrame.bits(), m_currentFrame.width(),
                            m_currentFrame.height(),
                            m_currentFrame.bytesPerLine(), m_imageFormat);
      m_currentFrame.unmap();
      if (!image.isNull()) emit frameCaptured(image);
    }

    return true;
  }
}

void MyVideoSurface::stop() {
  m_currentFrame = QVideoFrame();
  m_targetRect   = QRect();

  QAbstractVideoSurface::stop();

  m_widget->update();
}

//=============================================================================

MyVideoWidget::MyVideoWidget(QWidget* parent)
    : QWidget(parent)
    , m_previousImage(QImage())
    , m_surface(0)
    , m_showOnionSkin(false)
    , m_onionOpacity(128)
    , m_upsideDown(false)
    , m_countDownTime(0)
    , m_subCameraRect(QRect()) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  m_surface = new MyVideoSurface(this);
  setMouseTracking(true);
}

MyVideoWidget::~MyVideoWidget() { delete m_surface; }

QSize MyVideoWidget::sizeHint() const {
  return m_surface->surfaceFormat().sizeHint();
}

void MyVideoWidget::paintEvent(QPaintEvent* event) {
  QPainter p(this);

  p.fillRect(rect(), Qt::black);

  if (m_surface->isActive()) {
    const QRect videoRect         = m_surface->videoRect();
    const QTransform oldTransform = p.transform();

    if (m_upsideDown) {
      p.translate(videoRect.center());
      p.rotate(180);
      p.translate(-videoRect.center());
    }
    if (m_surface->surfaceFormat().scanLineDirection() ==
        QVideoSurfaceFormat::BottomToTop) {
      p.scale(1, -1);
      p.translate(0, -height());
    }

    p.drawImage(videoRect, m_image, m_surface->sourceRect());

    p.setTransform(oldTransform);

    if (m_showOnionSkin && m_onionOpacity > 0.0f && !m_previousImage.isNull() &&
        m_previousImage.size() == m_image.size()) {
      p.setOpacity((qreal)m_onionOpacity / 255.0);
      p.drawImage(videoRect, m_previousImage, m_surface->sourceRect());
      p.setOpacity(1.0);
    }

    // draw subcamera
    if (m_subCameraRect.isValid()) drawSubCamera(p);

    // draw countdown text
    if (m_countDownTime > 0) {
      QString str =
          QTime::fromMSecsSinceStartOfDay(m_countDownTime).toString("s.zzz");
      p.setPen(Qt::yellow);
      QFont font = p.font();
      font.setPixelSize(50);
      p.setFont(font);
      p.drawText(rect(), Qt::AlignRight | Qt::AlignBottom, str);
    }
  } else {
    p.setPen(Qt::white);
    QFont font = p.font();
    font.setPixelSize(30);
    p.setFont(font);
    p.drawText(rect(), Qt::AlignCenter, tr("Camera is not available"));
  }
}

void MyVideoWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);

  m_surface->updateVideoRect();
}

void MyVideoWidget::setSubCameraSize(QSize size) {
  QSize frameSize = m_surface->surfaceFormat().frameSize();
  assert(frameSize == size.expandedTo(frameSize));

  m_subCameraRect.setSize(size);
  // make sure the sub camera is inside of the frame
  if (!QRect(QPoint(0, 0), frameSize).contains(m_subCameraRect))
    m_subCameraRect.moveCenter(QRect(QPoint(0, 0), frameSize).center());

  update();
}

void MyVideoWidget::drawSubCamera(QPainter& p) {
  auto drawSubFrameLine = [&](SUBHANDLE handle, QPoint from, QPoint to) {
    p.setPen(QPen(handle == m_activeSubHandle ? Qt::green : Qt::magenta, 2));
    p.drawLine(from, to);
  };

  auto drawHandle = [&](SUBHANDLE handle, QPoint pos) {
    p.setPen(handle == m_activeSubHandle ? Qt::green : Qt::magenta);
    QRect handleRect(0, 0, 11, 11);
    handleRect.moveCenter(pos);
    p.drawRect(handleRect);
  };

  QRect vidSubRect = m_surface->transform().mapRect(m_subCameraRect);
  p.setBrush(Qt::NoBrush);
  drawSubFrameLine(HandleLeft, vidSubRect.topLeft(), vidSubRect.bottomLeft());
  drawSubFrameLine(HandleTop, vidSubRect.topLeft(), vidSubRect.topRight());
  drawSubFrameLine(HandleRight, vidSubRect.topRight(),
                   vidSubRect.bottomRight());
  drawSubFrameLine(HandleBottom, vidSubRect.bottomLeft(),
                   vidSubRect.bottomRight());

  // draw handles
  drawHandle(HandleTopLeft, vidSubRect.topLeft());
  drawHandle(HandleTopRight, vidSubRect.topRight());
  drawHandle(HandleBottomLeft, vidSubRect.bottomLeft());
  drawHandle(HandleBottomRight, vidSubRect.bottomRight());

  // draw cross mark at subcamera center when the cursor is in the frame
  if (m_activeSubHandle != HandleNone) {
    p.setPen(QPen(Qt::magenta, 1, Qt::DashLine));
    QPoint crossP(vidSubRect.width() / 40, vidSubRect.height() / 40);
    p.drawLine(vidSubRect.center() - crossP, vidSubRect.center() + crossP);
    crossP.setX(-crossP.x());
    p.drawLine(vidSubRect.center() - crossP, vidSubRect.center() + crossP);
  }
}

void MyVideoWidget::mouseMoveEvent(QMouseEvent* event) {
  int d = 10;

  auto isNearBy = [&](QPoint handlePos) -> bool {
    return (handlePos - event->pos()).manhattanLength() <= d * 2;
  };

  auto isNearEdge = [&](int handlePos, int mousePos) -> bool {
    return std::abs(handlePos - mousePos) <= d;
  };

  // if the sub camera is not active, do nothing and return
  if (!m_surface->isActive() || m_subCameraRect.isNull()) return;

  // with no mouse button, update the active handles
  if (event->buttons() == Qt::NoButton) {
    QRect vidSubRect    = m_surface->transform().mapRect(m_subCameraRect);
    SUBHANDLE preHandle = m_activeSubHandle;
    if (!vidSubRect.adjusted(-d, -d, d, d).contains(event->pos()))
      m_activeSubHandle = HandleNone;
    else if (vidSubRect.adjusted(d, d, -d, -d).contains(event->pos()))
      m_activeSubHandle = HandleFrame;
    else if (isNearBy(vidSubRect.topLeft()))
      m_activeSubHandle = HandleTopLeft;
    else if (isNearBy(vidSubRect.topRight()))
      m_activeSubHandle = HandleTopRight;
    else if (isNearBy(vidSubRect.bottomLeft()))
      m_activeSubHandle = HandleBottomLeft;
    else if (isNearBy(vidSubRect.bottomRight()))
      m_activeSubHandle = HandleBottomRight;
    else if (isNearEdge(vidSubRect.left(), event->pos().x()))
      m_activeSubHandle = HandleLeft;
    else if (isNearEdge(vidSubRect.top(), event->pos().y()))
      m_activeSubHandle = HandleTop;
    else if (isNearEdge(vidSubRect.right(), event->pos().x()))
      m_activeSubHandle = HandleRight;
    else if (isNearEdge(vidSubRect.bottom(), event->pos().y()))
      m_activeSubHandle = HandleBottom;
    else
      m_activeSubHandle = HandleNone;
    if (preHandle != m_activeSubHandle) {
      Qt::CursorShape cursor;
      if (m_activeSubHandle == HandleNone)
        cursor = Qt::ArrowCursor;
      else if (m_activeSubHandle == HandleFrame)
        cursor = Qt::SizeAllCursor;
      else if (m_activeSubHandle == HandleTopLeft ||
               m_activeSubHandle == HandleBottomRight)
        cursor = Qt::SizeFDiagCursor;
      else if (m_activeSubHandle == HandleTopRight ||
               m_activeSubHandle == HandleBottomLeft)
        cursor = Qt::SizeBDiagCursor;
      else if (m_activeSubHandle == HandleLeft ||
               m_activeSubHandle == HandleRight)
        cursor = Qt::SplitHCursor;
      else  // if (m_activeSubHandle == HandleTop || m_activeSubHandle ==
            // HandleBottom)
        cursor = Qt::SplitVCursor;

      setCursor(cursor);
      update();
    }
  }
  // if left button is pressed and some handle is active, transform the
  // subcamera
  else if (event->buttons() & Qt::LeftButton &&
           m_activeSubHandle != HandleNone && m_preSubCameraRect.isValid()) {
    auto clampVal = [&](int& val, int min, int max) {
      if (val < min)
        val = min;
      else if (val > max)
        val = max;
    };
    auto clampPoint = [&](QPoint& pos, int xmin, int xmax, int ymin, int ymax) {
      clampVal(pos.rx(), xmin, xmax);
      clampVal(pos.ry(), ymin, ymax);
    };

    int minimumSize = 100;

    QPoint offset =
        m_surface->transform().inverted().map(event->pos()) - m_dragStartPos;
    if (m_activeSubHandle >= HandleTopLeft &&
        m_activeSubHandle <= HandleBottomRight) {
      QSize offsetSize = m_preSubCameraRect.size();
      if (m_activeSubHandle == HandleBottomLeft ||
          m_activeSubHandle == HandleTopRight)
        offset.rx() *= -1;
      offsetSize.scale(offset.x(), offset.y(), Qt::KeepAspectRatioByExpanding);
      offset = QPoint(offsetSize.width(), offsetSize.height());
      if (m_activeSubHandle == HandleBottomLeft ||
          m_activeSubHandle == HandleTopRight)
        offset.rx() *= -1;
    }
    QSize camSize = m_surface->surfaceFormat().sizeHint();

    if (m_activeSubHandle == HandleFrame) {
      clampPoint(offset, -m_preSubCameraRect.left(),
                 camSize.width() - m_preSubCameraRect.right(),
                 -m_preSubCameraRect.top(),
                 camSize.height() - m_preSubCameraRect.bottom());
      m_subCameraRect = m_preSubCameraRect.translated(offset);
    } else {
      if (m_activeSubHandle == HandleTopLeft ||
          m_activeSubHandle == HandleBottomLeft ||
          m_activeSubHandle == HandleLeft) {
        clampVal(offset.rx(), -m_preSubCameraRect.left(),
                 m_preSubCameraRect.width() - minimumSize);
        m_subCameraRect.setLeft(m_preSubCameraRect.left() + offset.x());
      } else if (m_activeSubHandle == HandleTopRight ||
                 m_activeSubHandle == HandleBottomRight ||
                 m_activeSubHandle == HandleRight) {
        clampVal(offset.rx(), -m_preSubCameraRect.width() + minimumSize,
                 camSize.width() - m_preSubCameraRect.right());
        m_subCameraRect.setRight(m_preSubCameraRect.right() + offset.x());
      }

      if (m_activeSubHandle == HandleTopLeft ||
          m_activeSubHandle == HandleTopRight ||
          m_activeSubHandle == HandleTop) {
        clampVal(offset.ry(), -m_preSubCameraRect.top(),
                 m_preSubCameraRect.height() - minimumSize);
        m_subCameraRect.setTop(m_preSubCameraRect.top() + offset.y());
      } else if (m_activeSubHandle == HandleBottomRight ||
                 m_activeSubHandle == HandleBottomLeft ||
                 m_activeSubHandle == HandleBottom) {
        clampVal(offset.ry(), -m_preSubCameraRect.height() + minimumSize,
                 camSize.height() - m_preSubCameraRect.bottom());
        m_subCameraRect.setBottom(m_preSubCameraRect.bottom() + offset.y());
      }
      // if the sub camera size is changed, notify the parent for updating the
      // fields
      emit subCameraResized(true);
    }
    update();
  }
}

void MyVideoWidget::mousePressEvent(QMouseEvent* event) {
  // if the sub camera is not active, do nothing and return
  // use left button only and some handle must be active
  if (!m_surface->isActive() || m_subCameraRect.isNull() ||
      event->button() != Qt::LeftButton || m_activeSubHandle == HandleNone)
    return;

  // record the original sub camera size
  m_preSubCameraRect = m_subCameraRect;
  m_dragStartPos     = m_surface->transform().inverted().map(event->pos());

  // temporary stop the camera
  emit stopCamera();
}

void MyVideoWidget::mouseReleaseEvent(QMouseEvent* event) {
  // if the sub camera is not active, do nothing and return
  // use left button only and some handle must be active
  if (!m_surface->isActive() || m_subCameraRect.isNull() ||
      event->button() != Qt::LeftButton || m_activeSubHandle == HandleNone)
    return;

  m_preSubCameraRect = QRect();
  if (m_activeSubHandle != HandleFrame) emit subCameraResized(false);

  // restart the camera
  emit startCamera();
}

//=============================================================================

FrameNumberLineEdit::FrameNumberLineEdit(QWidget* parent, TFrameId fId,
                                         bool acceptLetter)
    : LineEdit(parent) {
  if (acceptLetter) {
    QString regExpStr   = QString("^%1$").arg(TFilePath::fidRegExpStr());
    m_regexpValidator   = new QRegExpValidator(QRegExp(regExpStr), this);
    TProjectManager* pm = TProjectManager::instance();
    pm->addListener(this);
  } else
    m_regexpValidator = new QRegExpValidator(QRegExp("^\\d{1,4}$"), this);

  m_regexpValidator_alt =
      new QRegExpValidator(QRegExp("^\\d{1,3}[A-Ia-i]?$"), this);

  updateValidator();
  updateSize();

  setValue(fId);
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::updateValidator() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
    setValidator(m_regexpValidator_alt);
  else
    setValidator(m_regexpValidator);
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::updateSize() {
  FilePathProperties* fpProp =
      TProjectManager::instance()->getCurrentProject()->getFilePathProperties();
  bool useStandard = fpProp->useStandard();
  int letterCount  = fpProp->letterCountForSuffix();
  if (useStandard)
    setFixedWidth(60);
  else {
    // 4 digits + letters reserve 12 px each
    int lc = (letterCount == 0) ? 9 : letterCount + 4;
    setFixedWidth(12 * lc);
  }
  updateGeometry();
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::setValue(TFrameId fId) {
  QString str;
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    if (!fId.getLetter().isEmpty()) {
      // need some warning?
    }
    str = convertToFrameWithLetter(fId.getNumber(), 3);
  } else {
    str = QString::fromStdString(fId.expand());
  }
  setText(str);
  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

TFrameId FrameNumberLineEdit::getValue() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    QString str = text();
    int f;
    // if no letters added
    if (str.at(str.size() - 1).isDigit())
      f = str.toInt() * 10;
    else {
      f = str.left(str.size() - 1).toInt() * 10 +
          letterToNum(str.at(str.size() - 1));
    }
    return TFrameId(f);
  } else {
    QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
    QRegExp rx(regExpStr);
    int pos = rx.indexIn(text());
    if (pos < 0) return TFrameId();
    if (rx.cap(2).isEmpty())
      return TFrameId(rx.cap(1).toInt());
    else
      return TFrameId(rx.cap(1).toInt(), rx.cap(2));
  }
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::onProjectSwitched() {
  QRegExpValidator* oldValidator = m_regexpValidator;
  QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
  m_regexpValidator = new QRegExpValidator(QRegExp(regExpStr), this);
  updateValidator();
  if (oldValidator) delete oldValidator;
}

void FrameNumberLineEdit::onProjectChanged() { onProjectSwitched(); }

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::focusInEvent(QFocusEvent* e) {
  m_textOnFocusIn = text();
}

void FrameNumberLineEdit::focusOutEvent(QFocusEvent* e) {
  // if the field is empty, then revert the last input
  if (text().isEmpty()) setText(m_textOnFocusIn);

  LineEdit::focusOutEvent(e);
}

//=============================================================================

LevelNameLineEdit::LevelNameLineEdit(QWidget* parent)
    : QLineEdit(parent), m_textOnFocusIn("") {
  // Exclude all character which cannot fit in a filepath (Win).
  // Dots are also prohibited since they are internally managed by Toonz.
  QRegExp rx("[^\\\\/:?*.\"<>|]+");
  setValidator(new QRegExpValidator(rx, this));
  setObjectName("LargeSizedText");

  connect(this, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));
}

void LevelNameLineEdit::focusInEvent(QFocusEvent* e) {
  m_textOnFocusIn = text();
}

void LevelNameLineEdit::onEditingFinished() {
  // if the content is not changed, do nothing.
  if (text() == m_textOnFocusIn) return;

  emit levelNameEdited();
}

//=============================================================================

std::wstring FlexibleNameCreator::getPrevious() {
  if (m_s.empty() || (m_s[0] == 0 && m_s.size() == 1)) {
    m_s.push_back('Z' - 'A');
    m_s.push_back('Z' - 'A');
    return L"ZZ";
  }
  int i = 0;
  int n = m_s.size();
  while (i < n) {
    m_s[i]--;
    if (m_s[i] >= 0) break;
    m_s[i] = 'Z' - 'A';
    i++;
  }
  if (i >= n) {
    n--;
    m_s.pop_back();
  }
  std::wstring s;
  for (i = n - 1; i >= 0; i--) s.append(1, (wchar_t)(L'A' + m_s[i]));
  return s;
}

//-------------------------------------------------------------------

bool FlexibleNameCreator::setCurrent(std::wstring name) {
  if (name.empty() || name.size() > 2) return false;
  std::vector<int> newNameBuf;
  for (std::wstring::iterator it = name.begin(); it != name.end(); ++it) {
    int s = (int)((*it) - L'A');
    if (s < 0 || s > 'Z' - 'A') return false;
    newNameBuf.push_back(s);
  }
  m_s.clear();
  for (int i = newNameBuf.size() - 1; i >= 0; i--) m_s.push_back(newNameBuf[i]);
  return true;
}

//=============================================================================

PencilTestSaveInFolderPopup::PencilTestSaveInFolderPopup(QWidget* parent)
    : Dialog(parent, true, false, "PencilTestSaveInFolder") {
  setWindowTitle(tr("Create the Destination Subfolder to Save"));

  m_parentFolderField = new FileField(this);

  QPushButton* setAsDefaultBtn = new QPushButton(tr("Set As Default"), this);
  setAsDefaultBtn->setToolTip(
      tr("Set the current \"Save In\" path as the default."));

  m_subFolderCB = new QCheckBox(tr("Create Subfolder"), this);

  QFrame* subFolderFrame = new QFrame(this);

  QGroupBox* infoGroupBox    = new QGroupBox(tr("Information"), this);
  QGroupBox* subNameGroupBox = new QGroupBox(tr("Subfolder Name"), this);

  m_projectField  = new QLineEdit(this);
  m_episodeField  = new QLineEdit(this);
  m_sequenceField = new QLineEdit(this);
  m_sceneField    = new QLineEdit(this);

  m_autoSubNameCB      = new QCheckBox(tr("Auto Format:"), this);
  m_subNameFormatCombo = new QComboBox(this);
  m_subFolderNameField = new QLineEdit(this);

  QCheckBox* showPopupOnLaunchCB =
      new QCheckBox(tr("Show This on Launch of the Camera Capture"), this);
  m_createSceneInFolderCB = new QCheckBox(tr("Save Scene in Subfolder"), this);

  QPushButton* okBtn     = new QPushButton(tr("OK"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  //---- properties

  m_subFolderCB->setChecked(CamCapSaveInPopupSubFolder != 0);
  subFolderFrame->setEnabled(CamCapSaveInPopupSubFolder != 0);

  // project name
  QString prjName = QString::fromStdString(CamCapSaveInPopupProject.getValue());
  if (prjName.isEmpty()) {
    prjName = TProjectManager::instance()
                  ->getCurrentProject()
                  ->getName()
                  .getQString();
  }
  m_projectField->setText(prjName);

  m_episodeField->setText(
      QString::fromStdString(CamCapSaveInPopupEpisode.getValue()));
  m_sequenceField->setText(
      QString::fromStdString(CamCapSaveInPopupSequence.getValue()));
  m_sceneField->setText(
      QString::fromStdString(CamCapSaveInPopupScene.getValue()));

  m_autoSubNameCB->setChecked(CamCapSaveInPopupAutoSubName != 0);
  m_subNameFormatCombo->setEnabled(CamCapSaveInPopupAutoSubName != 0);
  QStringList items;
  items << tr("C- + Sequence + Scene") << tr("Sequence + Scene")
        << tr("Episode + Sequence + Scene")
        << tr("Project + Episode + Sequence + Scene");
  m_subNameFormatCombo->addItems(items);
  m_subNameFormatCombo->setCurrentIndex(CamCapSaveInPopupAutoSubName - 1);

  showPopupOnLaunchCB->setChecked(CamCapOpenSaveInPopupOnLaunch != 0);
  m_createSceneInFolderCB->setChecked(CamCapSaveInPopupCreateSceneInFolder !=
                                      0);
  m_createSceneInFolderCB->setToolTip(
      tr("Save the current scene in the subfolder.\nSet the output folder path "
         "to the subfolder as well."));

  addButtonBarWidget(okBtn, cancelBtn);

  //---- layout
  m_topLayout->setMargin(10);
  m_topLayout->setSpacing(10);
  {
    QGridLayout* saveInLay = new QGridLayout();
    saveInLay->setMargin(0);
    saveInLay->setHorizontalSpacing(3);
    saveInLay->setVerticalSpacing(0);
    {
      saveInLay->addWidget(new QLabel(tr("Save In:"), this), 0, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      saveInLay->addWidget(m_parentFolderField, 0, 1);
      saveInLay->addWidget(setAsDefaultBtn, 1, 1);
    }
    saveInLay->setColumnStretch(0, 0);
    saveInLay->setColumnStretch(1, 1);
    m_topLayout->addLayout(saveInLay);

    m_topLayout->addWidget(m_subFolderCB, 0, Qt::AlignLeft);

    QVBoxLayout* subFolderLay = new QVBoxLayout();
    subFolderLay->setMargin(0);
    subFolderLay->setSpacing(10);
    {
      QGridLayout* infoLay = new QGridLayout();
      infoLay->setMargin(10);
      infoLay->setHorizontalSpacing(3);
      infoLay->setVerticalSpacing(10);
      {
        infoLay->addWidget(new QLabel(tr("Project:"), this), 0, 0);
        infoLay->addWidget(m_projectField, 0, 1);

        infoLay->addWidget(new QLabel(tr("Episode:"), this), 1, 0);
        infoLay->addWidget(m_episodeField, 1, 1);

        infoLay->addWidget(new QLabel(tr("Sequence:"), this), 2, 0);
        infoLay->addWidget(m_sequenceField, 2, 1);

        infoLay->addWidget(new QLabel(tr("Scene:"), this), 3, 0);
        infoLay->addWidget(m_sceneField, 3, 1);
      }
      infoLay->setColumnStretch(0, 0);
      infoLay->setColumnStretch(1, 1);
      infoGroupBox->setLayout(infoLay);
      subFolderLay->addWidget(infoGroupBox, 0);

      QGridLayout* subNameLay = new QGridLayout();
      subNameLay->setMargin(10);
      subNameLay->setHorizontalSpacing(3);
      subNameLay->setVerticalSpacing(10);
      {
        subNameLay->addWidget(m_autoSubNameCB, 0, 0);
        subNameLay->addWidget(m_subNameFormatCombo, 0, 1);

        subNameLay->addWidget(new QLabel(tr("Subfolder Name:"), this), 1, 0);
        subNameLay->addWidget(m_subFolderNameField, 1, 1);
      }
      subNameLay->setColumnStretch(0, 0);
      subNameLay->setColumnStretch(1, 1);
      subNameGroupBox->setLayout(subNameLay);
      subFolderLay->addWidget(subNameGroupBox, 0);

      subFolderLay->addWidget(m_createSceneInFolderCB, 0, Qt::AlignLeft);
    }
    subFolderFrame->setLayout(subFolderLay);
    m_topLayout->addWidget(subFolderFrame);

    m_topLayout->addWidget(showPopupOnLaunchCB, 0, Qt::AlignLeft);

    m_topLayout->addStretch(1);
  }

  resize(300, 440);

  //---- signal-slot connection
  bool ret = true;

  ret = ret && connect(m_subFolderCB, SIGNAL(clicked(bool)), subFolderFrame,
                       SLOT(setEnabled(bool)));
  ret = ret && connect(m_projectField, SIGNAL(textEdited(const QString&)), this,
                       SLOT(updateSubFolderName()));
  ret = ret && connect(m_episodeField, SIGNAL(textEdited(const QString&)), this,
                       SLOT(updateSubFolderName()));
  ret = ret && connect(m_sequenceField, SIGNAL(textEdited(const QString&)),
                       this, SLOT(updateSubFolderName()));
  ret = ret && connect(m_sceneField, SIGNAL(textEdited(const QString&)), this,
                       SLOT(updateSubFolderName()));
  ret = ret && connect(m_autoSubNameCB, SIGNAL(clicked(bool)), this,
                       SLOT(onAutoSubNameCBClicked(bool)));
  ret = ret && connect(m_subNameFormatCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(updateSubFolderName()));

  ret = ret && connect(showPopupOnLaunchCB, SIGNAL(clicked(bool)), this,
                       SLOT(onShowPopupOnLaunchCBClicked(bool)));
  ret = ret && connect(m_createSceneInFolderCB, SIGNAL(clicked(bool)), this,
                       SLOT(onCreateSceneInFolderCBClicked(bool)));
  ret = ret && connect(setAsDefaultBtn, SIGNAL(pressed()), this,
                       SLOT(onSetAsDefaultBtnPressed()));

  ret = ret && connect(okBtn, SIGNAL(clicked(bool)), this, SLOT(onOkPressed()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked(bool)), this, SLOT(reject()));
  assert(ret);

  updateSubFolderName();
}

//-----------------------------------------------------------------------------

QString PencilTestSaveInFolderPopup::getPath() {
  if (!m_subFolderCB->isChecked()) return m_parentFolderField->getPath();

  // re-code filepath
  TFilePath path(m_parentFolderField->getPath() + "\\" +
                 m_subFolderNameField->text());
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene) {
    path = scene->decodeFilePath(path);
    path = scene->codeFilePath(path);
  }
  return path.getQString();
}

//-----------------------------------------------------------------------------

QString PencilTestSaveInFolderPopup::getParentPath() {
  return m_parentFolderField->getPath();
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::showEvent(QShowEvent* event) {
  // Show "Save the scene" check box only when the scene is untitled
  bool isUntitled =
      TApp::instance()->getCurrentScene()->getScene()->isUntitled();
  m_createSceneInFolderCB->setVisible(isUntitled);
}

//-----------------------------------------------------------------------------
namespace {
QString formatString(QString inStr, int charNum) {
  if (inStr.isEmpty()) return QString("0").rightJustified(charNum, '0');

  QString numStr, postStr;
  // find the first non-digit character
  int index = inStr.indexOf(QRegExp("[^0-9]"), 0);

  if (index == -1)  // only digits
    numStr = inStr;
  else if (index == 0)  // only post strings
    return inStr;
  else {  // contains both
    numStr  = inStr.left(index);
    postStr = inStr.right(inStr.length() - index);
  }
  return numStr.rightJustified(charNum, '0') + postStr;
}
};  // namespace

void PencilTestSaveInFolderPopup::updateSubFolderName() {
  if (!m_autoSubNameCB->isChecked()) return;

  QString episodeStr  = formatString(m_episodeField->text(), 3);
  QString sequenceStr = formatString(m_sequenceField->text(), 3);
  QString sceneStr    = formatString(m_sceneField->text(), 4);

  QString str;

  switch (m_subNameFormatCombo->currentIndex()) {
  case 0:  // C- + Sequence + Scene
    str = QString("C-%1-%2").arg(sequenceStr).arg(sceneStr);
    break;
  case 1:  // Sequence + Scene
    str = QString("%1-%2").arg(sequenceStr).arg(sceneStr);
    break;
  case 2:  // Episode + Sequence + Scene
    str = QString("%1-%2-%3").arg(episodeStr).arg(sequenceStr).arg(sceneStr);
    break;
  case 3:  // Project + Episode + Sequence + Scene
    str = QString("%1-%2-%3-%4")
              .arg(m_projectField->text())
              .arg(episodeStr)
              .arg(sequenceStr)
              .arg(sceneStr);
    break;
  default:
    return;
  }
  m_subFolderNameField->setText(str);
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::onAutoSubNameCBClicked(bool on) {
  m_subNameFormatCombo->setEnabled(on);
  updateSubFolderName();
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::onShowPopupOnLaunchCBClicked(bool on) {
  CamCapOpenSaveInPopupOnLaunch = (on) ? 1 : 0;
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::onCreateSceneInFolderCBClicked(bool on) {
  CamCapSaveInPopupCreateSceneInFolder = (on) ? 1 : 0;
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::onSetAsDefaultBtnPressed() {
  CamCapSaveInParentFolder = m_parentFolderField->getPath().toStdString();
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::onOkPressed() {
  if (!m_subFolderCB->isChecked()) {
    accept();
    return;
  }

  // check the subFolder value
  QString subFolderName = m_subFolderNameField->text();
  if (subFolderName.isEmpty()) {
    DVGui::MsgBox(WARNING, tr("Subfolder name should not be empty."));
    return;
  }

  int index = subFolderName.indexOf(QRegExp("[\\]:;|=,\\[\\*\\.\"/\\\\]"), 0);
  if (index >= 0) {
    DVGui::MsgBox(WARNING, tr("Subfolder name should not contain following "
                              "characters:  * . \" / \\ [ ] : ; | = , "));
    return;
  }

  TFilePath fp(m_parentFolderField->getPath());
  fp += TFilePath(subFolderName);
  TFilePath actualFp =
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(fp);

  if (QFileInfo::exists(actualFp.getQString())) {
    DVGui::MsgBox(WARNING,
                  tr("Folder %1 already exists.").arg(actualFp.getQString()));
    return;
  }

  // save the current properties to env data
  CamCapSaveInPopupSubFolder   = (m_subFolderCB->isChecked()) ? 1 : 0;
  CamCapSaveInPopupProject     = m_projectField->text().toStdString();
  CamCapSaveInPopupEpisode     = m_episodeField->text().toStdString();
  CamCapSaveInPopupSequence    = m_sequenceField->text().toStdString();
  CamCapSaveInPopupScene       = m_sceneField->text().toStdString();
  CamCapSaveInPopupAutoSubName = (!m_autoSubNameCB->isChecked())
                                     ? 0
                                     : m_subNameFormatCombo->currentIndex() + 1;

  // create folder
  try {
    TSystem::mkDir(actualFp);
  } catch (...) {
    MsgBox(CRITICAL, tr("It is not possible to create the %1 folder.")
                         .arg(toQString(actualFp)));
    return;
  }

  createSceneInFolder();
  accept();
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::createSceneInFolder() {
  // make sure that the check box is displayed (= the scene is untitled) and is
  // checked.
  if (m_createSceneInFolderCB->isHidden() ||
      !m_createSceneInFolderCB->isChecked())
    return;
  // just in case
  if (!m_subFolderCB->isChecked()) return;

  // set the output folder
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  TFilePath fp(getPath().toStdWString());

  // for the scene folder mode, output destination must be already set to
  // $scenefolder or its subfolder. See TSceneProperties::onInitialize()
  if (Preferences::instance()->getPathAliasPriority() !=
      Preferences::SceneFolderAlias) {
    TOutputProperties* prop = scene->getProperties()->getOutputProperties();
    prop->setPath(prop->getPath().withParentDir(fp));
  }

  // save the scene
  TFilePath sceneFp =
      scene->decodeFilePath(fp) +
      TFilePath(m_subFolderNameField->text().toStdWString()).withType("tnz");
  IoCmd::saveScene(sceneFp, 0);
}

//-----------------------------------------------------------------------------

void PencilTestSaveInFolderPopup::updateParentFolder() {
  // If the parent folder is saved in the scene, use it
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  QString parentFolder =
      scene->getProperties()->cameraCaptureSaveInPath().getQString();
  if (parentFolder.isEmpty()) {
    // else then, if the user-env stores the parent folder value, use it
    parentFolder = QString::fromStdString(CamCapSaveInParentFolder);
    // else, use "+extras" project folder
    if (parentFolder.isEmpty())
      parentFolder =
          QString("+%1").arg(QString::fromStdString(TProject::Extras));
  }

  m_parentFolderField->setPath(parentFolder);
}

//=============================================================================

PencilTestPopup::PencilTestPopup()
    // set the parent 0 in order to enable the popup behind the main window
    : Dialog(0, false, false, "PencilTest")
    , m_currentCamera(NULL)
    , m_captureWhiteBGCue(false)
    , m_captureCue(false) {
  setWindowTitle(tr("Camera Capture"));

  // add maximize button to the dialog
  setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

  layout()->setSizeConstraint(QLayout::SetNoConstraint);

  m_saveInFolderPopup = new PencilTestSaveInFolderPopup(this);

  m_videoWidget = new MyVideoWidget(this);

  m_cameraListCombo                 = new QComboBox(this);
  QPushButton* refreshCamListButton = new QPushButton(tr("Refresh"), this);
  m_resolutionCombo                 = new QComboBox(this);

  QGroupBox* fileFrame = new QGroupBox(tr("File"), this);
  m_levelNameEdit      = new LevelNameLineEdit(this);
  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  int startFrame =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled() ? 10 : 1;
  m_frameNumberEdit        = new FrameNumberLineEdit(this, startFrame);
  m_frameInfoLabel         = new QLabel("", this);
  m_fileTypeCombo          = new QComboBox(this);
  m_fileFormatOptionButton = new QPushButton(tr("Options"), this);

  m_saveInFileFld = new FileField(this, m_saveInFolderPopup->getParentPath());

  QToolButton* nextLevelButton = new QToolButton(this);
  m_previousLevelButton        = new QToolButton(this);

  m_saveOnCaptureCB =
      new QCheckBox(tr("Save images as they are captured"), this);

  QGroupBox* imageFrame = new QGroupBox(tr("Image adjust"), this);
  m_colorTypeCombo      = new QComboBox(this);

  m_camCapLevelControl = new CameraCaptureLevelControl(this);
  m_upsideDownCB       = new QCheckBox(tr("Upside down"), this);

  m_bgReductionFld       = new IntField(this);
  m_captureWhiteBGButton = new QPushButton(tr("Capture white BG"), this);

  QGroupBox* displayFrame = new QGroupBox(tr("Display"), this);
  m_onionSkinCB           = new QCheckBox(tr("Show onion skin"), this);
  m_loadImageButton       = new QPushButton(tr("Load Selected Image"), this);
  m_onionOpacityFld       = new IntField(this);

  QGroupBox* timerFrame = new QGroupBox(tr("Interval timer"), this);
  m_timerCB             = new QCheckBox(tr("Use interval timer"), this);
  m_timerIntervalFld    = new IntField(this);
  m_captureTimer        = new QTimer(this);
  m_countdownTimer      = new QTimer(this);

  m_captureButton          = new QPushButton(tr("Capture\n[Return key]"), this);
  QPushButton* closeButton = new QPushButton(tr("Close"), this);

#ifdef _WIN32
  m_captureFilterSettingsBtn = new QPushButton(this);
#else
  m_captureFilterSettingsBtn = 0;
#endif

  QPushButton* subfolderButton = new QPushButton(tr("Subfolder"), this);

  // subcamera
  m_subcameraButton     = new QPushButton(tr("Subcamera"), this);
  m_subWidthFld         = new IntLineEdit(this);
  m_subHeightFld        = new IntLineEdit(this);
  QWidget* subCamWidget = new QWidget(this);

  //----

  m_resolutionCombo->setMaximumWidth(fontMetrics().width("0000 x 0000") + 25);
  m_fileTypeCombo->addItems({"jpg", "png", "tga", "tif"});
  m_fileTypeCombo->setCurrentIndex(0);

  fileFrame->setObjectName("CleanupSettingsFrame");
  m_frameNumberEdit->setObjectName("LargeSizedText");
  m_frameInfoLabel->setAlignment(Qt::AlignRight);
  nextLevelButton->setFixedSize(24, 24);
  nextLevelButton->setArrowType(Qt::RightArrow);
  nextLevelButton->setToolTip(tr("Next Level"));
  m_previousLevelButton->setFixedSize(24, 24);
  m_previousLevelButton->setArrowType(Qt::LeftArrow);
  m_previousLevelButton->setToolTip(tr("Previous Level"));
  m_saveOnCaptureCB->setChecked(true);

  imageFrame->setObjectName("CleanupSettingsFrame");
  m_colorTypeCombo->addItems(
      {tr("Color"), tr("Grayscale"), tr("Black & White")});
  m_colorTypeCombo->setCurrentIndex(0);
  m_upsideDownCB->setChecked(false);

  m_bgReductionFld->setRange(0, 100);
  m_bgReductionFld->setValue(0);
  m_bgReductionFld->setDisabled(true);

  displayFrame->setObjectName("CleanupSettingsFrame");
  m_onionSkinCB->setChecked(false);
  m_onionOpacityFld->setRange(1, 100);
  m_onionOpacityFld->setValue(50);
  m_onionOpacityFld->setDisabled(true);

  timerFrame->setObjectName("CleanupSettingsFrame");
  m_timerCB->setChecked(false);
  m_timerIntervalFld->setRange(0, 60);
  m_timerIntervalFld->setValue(10);
  m_timerIntervalFld->setDisabled(true);
  // Make the interval timer single-shot. When the capture finished, restart
  // timer for next frame.
  // This is because capturing and saving the image needs some time.
  m_captureTimer->setSingleShot(true);

  m_captureButton->setObjectName("LargeSizedText");
  m_captureButton->setFixedHeight(75);
  QCommonStyle style;
  m_captureButton->setIcon(style.standardIcon(QStyle::SP_DialogOkButton));
  m_captureButton->setIconSize(QSize(30, 30));

  if (m_captureFilterSettingsBtn) {
    m_captureFilterSettingsBtn->setObjectName("GearButton");
    m_captureFilterSettingsBtn->setFixedSize(24, 24);
    m_captureFilterSettingsBtn->setIconSize(QSize(16, 16));
    m_captureFilterSettingsBtn->setIcon(createQIcon("gear"));
    m_captureFilterSettingsBtn->setToolTip(
        tr("Video Capture Filter Settings..."));
  }

  subfolderButton->setObjectName("SubfolderButton");
  subfolderButton->setIconSize(QSize(16, 16));
  subfolderButton->setIcon(createQIcon("folder_new"));
  m_saveInFileFld->setMaximumWidth(380);

  m_saveInFolderPopup->hide();

  m_subcameraButton->setObjectName("SubcameraButton");
  m_subcameraButton->setIconSize(QSize(16, 16));
  m_subcameraButton->setIcon(createQIcon("subcamera"));
  m_subcameraButton->setCheckable(true);
  m_subcameraButton->setChecked(false);
  subCamWidget->setHidden(true);

  //---- layout ----
  m_topLayout->setMargin(10);
  m_topLayout->setSpacing(10);
  {
    QHBoxLayout* camLay = new QHBoxLayout();
    camLay->setMargin(0);
    camLay->setSpacing(3);
    {
      camLay->addWidget(new QLabel(tr("Camera:"), this), 0);
      camLay->addWidget(m_cameraListCombo, 1);
      camLay->addWidget(refreshCamListButton, 0);
      camLay->addSpacing(10);
      camLay->addWidget(new QLabel(tr("Resolution:"), this), 0);
      camLay->addWidget(m_resolutionCombo, 1);

      if (m_captureFilterSettingsBtn) {
        camLay->addSpacing(10);
        camLay->addWidget(m_captureFilterSettingsBtn);
      }

      camLay->addSpacing(10);
      camLay->addWidget(m_subcameraButton, 0);
      QHBoxLayout* subCamLay = new QHBoxLayout();
      subCamLay->setMargin(0);
      subCamLay->setSpacing(3);
      {
        subCamLay->addWidget(m_subWidthFld, 0);
        subCamLay->addWidget(new QLabel("x", this), 0);
        subCamLay->addWidget(m_subHeightFld, 0);
        subCamLay->addStretch(0);
      }
      subCamWidget->setLayout(subCamLay);
      camLay->addWidget(subCamWidget, 0);

      camLay->addStretch(0);
      camLay->addSpacing(15);
      camLay->addWidget(new QLabel(tr("Save In:"), this), 0);
      camLay->addWidget(m_saveInFileFld, 1);

      camLay->addSpacing(10);
      camLay->addWidget(subfolderButton, 0);
    }
    m_topLayout->addLayout(camLay, 0);

    QHBoxLayout* bottomLay = new QHBoxLayout();
    bottomLay->setMargin(0);
    bottomLay->setSpacing(10);
    {
      bottomLay->addWidget(m_videoWidget, 1);

      QVBoxLayout* rightLay = new QVBoxLayout();
      rightLay->setMargin(0);
      rightLay->setSpacing(5);
      {
        QVBoxLayout* fileLay = new QVBoxLayout();
        fileLay->setMargin(8);
        fileLay->setSpacing(5);
        {
          QGridLayout* levelLay = new QGridLayout();
          levelLay->setMargin(0);
          levelLay->setHorizontalSpacing(3);
          levelLay->setVerticalSpacing(5);
          {
            levelLay->addWidget(new QLabel(tr("Name:"), this), 0, 0,
                                Qt::AlignRight);
            QHBoxLayout* nameLay = new QHBoxLayout();
            nameLay->setMargin(0);
            nameLay->setSpacing(2);
            {
              nameLay->addWidget(m_previousLevelButton, 0);
              nameLay->addWidget(m_levelNameEdit, 1);
              nameLay->addWidget(nextLevelButton, 0);
            }
            levelLay->addLayout(nameLay, 0, 1);

            levelLay->addWidget(new QLabel(tr("Frame:"), this), 1, 0,
                                Qt::AlignRight);

            QHBoxLayout* frameLay = new QHBoxLayout();
            frameLay->setMargin(0);
            frameLay->setSpacing(2);
            {
              frameLay->addWidget(m_frameNumberEdit, 1);
              frameLay->addWidget(m_frameInfoLabel, 1, Qt::AlignVCenter);
            }
            levelLay->addLayout(frameLay, 1, 1);
          }
          levelLay->setColumnStretch(0, 0);
          levelLay->setColumnStretch(1, 1);
          fileLay->addLayout(levelLay, 0);

          QHBoxLayout* fileTypeLay = new QHBoxLayout();
          fileTypeLay->setMargin(0);
          fileTypeLay->setSpacing(3);
          {
            fileTypeLay->addWidget(new QLabel(tr("File Type:"), this), 0);
            fileTypeLay->addWidget(m_fileTypeCombo, 1);
            fileTypeLay->addSpacing(10);
            fileTypeLay->addWidget(m_fileFormatOptionButton);
          }
          fileLay->addLayout(fileTypeLay, 0);

          fileLay->addWidget(m_saveOnCaptureCB, 0);
        }
        fileFrame->setLayout(fileLay);
        rightLay->addWidget(fileFrame, 0);

        QGridLayout* imageLay = new QGridLayout();
        imageLay->setMargin(8);
        imageLay->setHorizontalSpacing(3);
        imageLay->setVerticalSpacing(5);
        {
          imageLay->addWidget(new QLabel(tr("Color type:"), this), 0, 0,
                              Qt::AlignRight);
          imageLay->addWidget(m_colorTypeCombo, 0, 1);

          imageLay->addWidget(m_camCapLevelControl, 1, 0, 1, 3);

          imageLay->addWidget(m_upsideDownCB, 2, 0, 1, 3, Qt::AlignLeft);

          imageLay->addWidget(new QLabel(tr("BG reduction:"), this), 3, 0,
                              Qt::AlignRight);
          imageLay->addWidget(m_bgReductionFld, 3, 1, 1, 2);

          imageLay->addWidget(m_captureWhiteBGButton, 4, 0, 1, 3);
        }
        imageLay->setColumnStretch(0, 0);
        imageLay->setColumnStretch(1, 0);
        imageLay->setColumnStretch(2, 1);
        imageFrame->setLayout(imageLay);
        rightLay->addWidget(imageFrame, 0);

        QGridLayout* displayLay = new QGridLayout();
        displayLay->setMargin(8);
        displayLay->setHorizontalSpacing(3);
        displayLay->setVerticalSpacing(5);
        {
          displayLay->addWidget(m_onionSkinCB, 0, 0, 1, 2);

          displayLay->addWidget(new QLabel(tr("Opacity(%):"), this), 1, 0,
                                Qt::AlignRight);
          displayLay->addWidget(m_onionOpacityFld, 1, 1);
          displayLay->addWidget(m_loadImageButton, 2, 0, 1, 2);
        }
        displayLay->setColumnStretch(0, 0);
        displayLay->setColumnStretch(1, 1);
        displayFrame->setLayout(displayLay);
        rightLay->addWidget(displayFrame);

        QGridLayout* timerLay = new QGridLayout();
        timerLay->setMargin(8);
        timerLay->setHorizontalSpacing(3);
        timerLay->setVerticalSpacing(5);
        {
          timerLay->addWidget(m_timerCB, 0, 0, 1, 2);

          timerLay->addWidget(new QLabel(tr("Interval(sec):"), this), 1, 0,
                              Qt::AlignRight);
          timerLay->addWidget(m_timerIntervalFld, 1, 1);
        }
        timerLay->setColumnStretch(0, 0);
        timerLay->setColumnStretch(1, 1);
        timerFrame->setLayout(timerLay);
        rightLay->addWidget(timerFrame);

        rightLay->addStretch(1);

        rightLay->addWidget(m_captureButton, 0);
        rightLay->addSpacing(10);
        rightLay->addWidget(closeButton, 0);
        rightLay->addSpacing(5);
      }
      bottomLay->addLayout(rightLay, 0);
    }
    m_topLayout->addLayout(bottomLay, 1);
  }

  //---- signal-slot connections ----
  bool ret = true;
  ret      = ret && connect(refreshCamListButton, SIGNAL(pressed()), this,
                            SLOT(refreshCameraList()));
  ret      = ret && connect(m_cameraListCombo, SIGNAL(activated(int)), this,
                            SLOT(onCameraListComboActivated(int)));
  ret = ret && connect(m_resolutionCombo, SIGNAL(activated(const QString&)),
                       this, SLOT(onResolutionComboActivated(const QString&)));
  ret = ret && connect(m_fileFormatOptionButton, SIGNAL(pressed()), this,
                       SLOT(onFileFormatOptionButtonPressed()));
  ret = ret && connect(m_levelNameEdit, SIGNAL(levelNameEdited()), this,
                       SLOT(onLevelNameEdited()));
  ret = ret &&
        connect(nextLevelButton, SIGNAL(pressed()), this, SLOT(onNextName()));
  ret = ret && connect(m_previousLevelButton, SIGNAL(pressed()), this,
                       SLOT(onPreviousName()));
  ret = ret && connect(m_colorTypeCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onColorTypeComboChanged(int)));
  ret = ret && connect(m_captureWhiteBGButton, SIGNAL(pressed()), this,
                       SLOT(onCaptureWhiteBGButtonPressed()));
  ret = ret && connect(m_onionSkinCB, SIGNAL(toggled(bool)), this,
                       SLOT(onOnionCBToggled(bool)));
  ret = ret && connect(m_loadImageButton, SIGNAL(pressed()), this,
                       SLOT(onLoadImageButtonPressed()));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onOnionOpacityFldEdited()));
  ret = ret && connect(m_upsideDownCB, SIGNAL(toggled(bool)), m_videoWidget,
                       SLOT(onUpsideDownChecked(bool)));
  ret = ret && connect(m_timerCB, SIGNAL(toggled(bool)), this,
                       SLOT(onTimerCBToggled(bool)));
  ret = ret && connect(m_captureTimer, SIGNAL(timeout()), this,
                       SLOT(onCaptureTimerTimeout()));
  ret = ret &&
        connect(m_countdownTimer, SIGNAL(timeout()), this, SLOT(onCountDown()));

  ret = ret && connect(closeButton, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret && connect(m_captureButton, SIGNAL(clicked(bool)), this,
                       SLOT(onCaptureButtonClicked(bool)));
  if (m_captureFilterSettingsBtn)
    ret = ret && connect(m_captureFilterSettingsBtn, SIGNAL(pressed()), this,
                         SLOT(onCaptureFilterSettingsBtnPressed()));
  ret = ret && connect(subfolderButton, SIGNAL(clicked(bool)), this,
                       SLOT(openSaveInFolderPopup()));
  ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onSaveInPathEdited()));
  ret = ret && connect(m_fileTypeCombo, SIGNAL(activated(int)), this,
                       SLOT(refreshFrameInfo()));
  ret = ret && connect(m_frameNumberEdit, SIGNAL(editingFinished()), this,
                       SLOT(refreshFrameInfo()));

  // sub camera
  ret = ret && connect(m_subcameraButton, SIGNAL(toggled(bool)), this,
                       SLOT(onSubCameraToggled(bool)));
  ret = ret && connect(m_subcameraButton, SIGNAL(toggled(bool)), subCamWidget,
                       SLOT(setVisible(bool)));
  ret = ret && connect(m_subWidthFld, SIGNAL(editingFinished()), this,
                       SLOT(onSubCameraSizeEdited()));
  ret = ret && connect(m_subHeightFld, SIGNAL(editingFinished()), this,
                       SLOT(onSubCameraSizeEdited()));
  ret = ret && connect(m_videoWidget, &MyVideoWidget::startCamera, [&]() {
          if (m_currentCamera) m_currentCamera->start();
        });
  ret = ret && connect(m_videoWidget, &MyVideoWidget::stopCamera, [&]() {
          if (m_currentCamera) m_currentCamera->stop();
        });
  ret = ret && connect(m_videoWidget, SIGNAL(subCameraResized(bool)), this,
                       SLOT(onSubCameraResized(bool)));

  assert(ret);

  refreshCameraList();

  int startupCamIndex = m_cameraListCombo->findText(
      QString::fromStdString(CamCapCameraName.getValue()));
  if (startupCamIndex > 0) {
    m_cameraListCombo->setCurrentIndex(startupCamIndex);
    onCameraListComboActivated(startupCamIndex);
  }

  QString resStr = QString::fromStdString(CamCapCameraResolution.getValue());
  if (m_currentCamera && !resStr.isEmpty()) {
    int startupResolutionIndex = m_resolutionCombo->findText(resStr);
    if (startupResolutionIndex >= 0) {
      m_resolutionCombo->setCurrentIndex(startupResolutionIndex);
      onResolutionComboActivated(resStr);
    }
  }

  setToNextNewLevel();
}

//-----------------------------------------------------------------------------

PencilTestPopup::~PencilTestPopup() {
  if (m_currentCamera) {
    if (m_currentCamera->state() == QCamera::ActiveState)
      m_currentCamera->stop();
    if (m_currentCamera->state() == QCamera::LoadedState)
      m_currentCamera->unload();
    delete m_currentCamera;
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::refreshCameraList() {
  m_cameraListCombo->clear();

  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if (cameras.empty()) {
    m_cameraListCombo->addItem(tr("No camera found"));
    m_cameraListCombo->setMaximumWidth(250);
    m_cameraListCombo->setDisabled(true);
  }

  int maxTextLength = 0;
  // add non-connected state as default
  m_cameraListCombo->addItem(tr("- Select camera -"));
  for (int c = 0; c < cameras.size(); c++) {
    QString camDesc = cameras.at(c).description();
    m_cameraListCombo->addItem(camDesc);
    maxTextLength = std::max(maxTextLength, fontMetrics().width(camDesc));
  }
  m_cameraListCombo->setMaximumWidth(maxTextLength + 25);
  m_cameraListCombo->setEnabled(true);
  m_cameraListCombo->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCameraListComboActivated(int comboIndex) {
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if (cameras.size() != m_cameraListCombo->count() - 1) return;

  // if selected the non-connected state, then disconnect the current camera
  if (comboIndex == 0) {
    m_videoWidget->videoSurface()->stop();
    if (m_currentCamera) {
      if (m_currentCamera->state() == QCamera::ActiveState)
        m_currentCamera->stop();
      if (m_currentCamera->state() == QCamera::LoadedState)
        m_currentCamera->unload();
    }
    m_deviceName = QString();
    m_videoWidget->setImage(QImage());
    // update env
    CamCapCameraName = "";
    return;
  }

  int index = comboIndex - 1;
  // in case the camera is not changed (just click the combobox)
  if (cameras.at(index).deviceName() == m_deviceName) return;

  QCamera* oldCamera = m_currentCamera;
  if (oldCamera) m_videoWidget->videoSurface()->stop();

  m_currentCamera = new QCamera(cameras.at(index), this);
  m_deviceName    = cameras.at(index).deviceName();

  // loading new camera
  m_currentCamera->load();

  // refresh resolution
  m_resolutionCombo->clear();
  QList<QSize> sizes = m_currentCamera->supportedViewfinderResolutions();

  for (int s = 0; s < sizes.size(); s++) {
    m_resolutionCombo->addItem(
        QString("%1 x %2").arg(sizes.at(s).width()).arg(sizes.at(s).height()));
  }
  if (!sizes.isEmpty()) {
    // select the largest available resolution
    m_resolutionCombo->setCurrentIndex(m_resolutionCombo->count() - 1);
    QCameraViewfinderSettings settings = m_currentCamera->viewfinderSettings();
    settings.setResolution(sizes.last());
    m_currentCamera->setViewfinderSettings(settings);
  }
  m_currentCamera->setViewfinder(m_videoWidget->videoSurface());
  m_videoWidget->videoSurface()->start(
      m_videoWidget->videoSurface()->surfaceFormat());

  // deleting old camera
  if (oldCamera) {
    if (oldCamera->state() == QCamera::ActiveState) oldCamera->stop();
    delete oldCamera;
  }
  // start new camera
  m_currentCamera->start();
  m_videoWidget->setImage(QImage());

  // update env
  CamCapCameraName = m_cameraListCombo->itemText(comboIndex).toStdString();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onResolutionComboActivated(const QString& itemText) {
  // resolution is written in the itemText with the format "<width> x <height>"
  // (e.g. "800 x 600")
  QStringList texts = itemText.split(' ');
  // the splited text must be "<width>" "x" and "<height>"
  if (texts.size() != 3) return;

#ifndef MACOSX
  m_currentCamera->stop();
  m_currentCamera->unload();
#endif

  QCameraViewfinderSettings settings = m_currentCamera->viewfinderSettings();
  QSize newResolution(texts[0].toInt(), texts[2].toInt());
  settings.setResolution(newResolution);
  m_currentCamera->setViewfinderSettings(settings);

  // reset white bg
  m_whiteBGImg = QImage();
  m_bgReductionFld->setDisabled(true);
#ifndef MACOSX
  m_currentCamera->start();
#endif
  m_videoWidget->setImage(QImage());

  // update env
  CamCapCameraResolution = itemText.toStdString();

  refreshFrameInfo();

  // reset subcamera info
  m_subcameraButton->setChecked(false);  // this will hide the size fields
  m_subWidthFld->setRange(10, newResolution.width());
  m_subHeightFld->setRange(10, newResolution.height());
  // if there is no existing level or its size is larger than the current camera
  if (!m_allowedCameraSize.isValid() ||
      m_allowedCameraSize.width() > newResolution.width() ||
      m_allowedCameraSize.height() > newResolution.height()) {
    // make the initial subcamera size to be with the same aspect ratio as the
    // current camera
    TCamera* camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimension camres = camera->getRes();
    newResolution =
        QSize(camres.lx, camres.ly).scaled(newResolution, Qt::KeepAspectRatio);
    // newResolution.scale(QSize(res.lx, res.ly), Qt::KeepAspectRatio);
    m_subWidthFld->setValue(newResolution.width());
    m_subHeightFld->setValue(newResolution.height());
  } else {
    m_subWidthFld->setValue(m_allowedCameraSize.width());
    m_subHeightFld->setValue(m_allowedCameraSize.height());
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onFileFormatOptionButtonPressed() {
  // Tentatively use the preview output settings
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* prop = scene->getProperties()->getPreviewProperties();
  std::string ext         = m_fileTypeCombo->currentText().toStdString();
  TFrameId oldTmplFId     = scene->getProperties()->formatTemplateFIdForInput();
  openFormatSettingsPopup(this, ext, prop->getFileFormatProperties(ext),
                          &scene->getProperties()->formatTemplateFIdForInput());

  TFrameId newTmplFId = scene->getProperties()->formatTemplateFIdForInput();
  if (oldTmplFId.getZeroPadding() != newTmplFId.getZeroPadding() ||
      oldTmplFId.getStartSeqInd() != newTmplFId.getStartSeqInd())
    refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onLevelNameEdited() {
  updateLevelNameAndFrame(m_levelNameEdit->text().toStdWString());
}

//-----------------------------------------------------------------------------
void PencilTestPopup::onNextName() {
  std::unique_ptr<FlexibleNameCreator> nameCreator(new FlexibleNameCreator());
  if (!nameCreator->setCurrent(m_levelNameEdit->text().toStdWString())) {
    setToNextNewLevel();
    return;
  }

  std::wstring levelName = nameCreator->getNext();

  updateLevelNameAndFrame(levelName);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onPreviousName() {
  std::unique_ptr<FlexibleNameCreator> nameCreator(new FlexibleNameCreator());

  std::wstring levelName;

  // if the current level name is non-sequential, then try to switch the last
  // sequential level in the scene.
  if (!nameCreator->setCurrent(m_levelNameEdit->text().toStdWString())) {
    TLevelSet* levelSet =
        TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
    nameCreator->setCurrent(L"ZZ");
    for (;;) {
      levelName = nameCreator->getPrevious();
      if (levelSet->getLevel(levelName) != 0) break;
      if (levelName == L"A") {
        setToNextNewLevel();
        return;
      }
    }
  } else
    levelName = nameCreator->getPrevious();

  updateLevelNameAndFrame(levelName);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::setToNextNewLevel() {
  const std::unique_ptr<NameBuilder> nameBuilder(NameBuilder::getBuilder(L""));

  TLevelSet* levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  ToonzScene* scene      = TApp::instance()->getCurrentScene()->getScene();
  std::wstring levelName = L"";

  // Select a different unique level name in case it already exists (either in
  // scene or on disk)
  TFilePath fp;
  TFilePath actualFp;
  for (;;) {
    levelName = nameBuilder->getNext();

    if (levelSet->getLevel(levelName) != 0) continue;

    fp = TFilePath(m_saveInFileFld->getPath()) +
         TFilePath(levelName + L".." +
                   m_fileTypeCombo->currentText().toStdWString());
    actualFp = scene->decodeFilePath(fp);

    if (TSystem::doesExistFileOrLevel(actualFp)) {
      continue;
    }

    break;
  }

  updateLevelNameAndFrame(levelName);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::updateLevelNameAndFrame(std::wstring levelName) {
  if (levelName != m_levelNameEdit->text().toStdWString())
    m_levelNameEdit->setText(QString::fromStdWString(levelName));
  m_previousLevelButton->setDisabled(levelName == L"A");

  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  bool withLetter =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled();

  TLevelSet* levelSet =
      TApp::instance()->getCurrentScene()->getScene()->getLevelSet();
  TXshLevel* level_p = levelSet->getLevel(levelName);
  int startFrame;
  if (!level_p) {
    startFrame = withLetter ? 10 : 1;
  } else {
    std::vector<TFrameId> fids;
    level_p->getFids(fids);
    if (fids.empty()) {
      startFrame = withLetter ? 10 : 1;
    } else {
      int lastNum = fids.back().getNumber();
      startFrame  = withLetter ? ((int)(lastNum / 10) + 1) * 10 : lastNum + 1;
    }
  }
  m_frameNumberEdit->setValue(startFrame);

  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onColorTypeComboChanged(int index) {
  m_camCapLevelControl->setMode(index != 2);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onFrameCaptured(QImage& image) {
  if (!m_videoWidget || image.isNull()) return;
  // capture the white BG
  if (m_captureWhiteBGCue) {
    m_whiteBGImg        = image.copy();
    m_captureWhiteBGCue = false;
    m_bgReductionFld->setEnabled(true);
  }

  processImage(image);
  m_videoWidget->setImage(image.copy());

  if (m_captureCue) {
#ifndef MACOSX
    m_currentCamera->stop();
#endif
    m_captureCue = false;

    bool scanBtoT =
        m_videoWidget->videoSurface()->surfaceFormat().scanLineDirection() ==
        QVideoSurfaceFormat::BottomToTop;
    bool upsideDown = m_upsideDownCB->isChecked();

    image = image.mirrored(upsideDown, upsideDown != scanBtoT);

    if (importImage(image)) {
      m_videoWidget->setPreviousImage(image.copy());
      if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
        TFrameId fId = m_frameNumberEdit->getValue();
        int f        = fId.getNumber();
        if (f % 10 == 0)  // next number
          m_frameNumberEdit->setValue(TFrameId(((int)(f / 10) + 1) * 10));
        else  // next alphabet
          m_frameNumberEdit->setValue(TFrameId(f + 1));
      } else {
        TFrameId fId = m_frameNumberEdit->getValue();

        if (fId.getLetter() == '\0' || fId.getLetter() == 'Z' ||
            fId.getLetter() == 'z')  // next number
          m_frameNumberEdit->setValue(TFrameId(fId.getNumber() + 1));
        else {  // next alphabet
          QByteArray byteArray = fId.getLetter().toUtf8();
          // return incrementing the last letter
          byteArray.data()[byteArray.size() - 1]++;
          m_frameNumberEdit->setValue(
              TFrameId(fId.getNumber(), QString::fromUtf8(byteArray)));
        }
      }

      /* notify */
      TApp::instance()->getCurrentScene()->notifySceneChanged();
      TApp::instance()->getCurrentScene()->notifyCastChange();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();

      // restart interval timer for capturing next frame (it is single shot)
      if (m_timerCB->isChecked() && m_captureButton->isChecked()) {
        m_captureTimer->start(m_timerIntervalFld->getValue() * 1000);
        // restart the count down as well (for aligning the timing. It is not
        // single shot)
        if (m_timerIntervalFld->getValue() != 0) m_countdownTimer->start(100);
      }
    }
    // if capture was failed, stop interval capturing
    else if (m_timerCB->isChecked()) {
      m_captureButton->setChecked(false);
      onCaptureButtonClicked(false);
    }
#ifndef MACOSX
    m_currentCamera->start();
#endif
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::showEvent(QShowEvent* event) {
  // m_timerId = startTimer(10);

  // if there is another action of which "return" key is assigned as short cut
  // key,
  // then release the shortcut key temporary while the popup opens
  QAction* action = CommandManager::instance()->getActionFromShortcut("Return");
  if (action) action->setShortcut(QKeySequence(""));

  connect(m_videoWidget->videoSurface(), SIGNAL(frameCaptured(QImage&)), this,
          SLOT(onFrameCaptured(QImage&)));

  // reload camera
  if (m_currentCamera) {
    if (m_currentCamera->state() == QCamera::UnloadedState)
      m_currentCamera->load();
    if (m_currentCamera->state() == QCamera::LoadedState)
      m_currentCamera->start();
  }

  TSceneHandle* sceneHandle = TApp::instance()->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(onSceneSwitched()));
  connect(sceneHandle, SIGNAL(castChanged()), this, SLOT(refreshFrameInfo()));

  bool tmp_alwaysOverwrite = m_alwaysOverwrite;
  onSceneSwitched();
  m_alwaysOverwrite = tmp_alwaysOverwrite;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::hideEvent(QHideEvent* event) {
  // set back the "return" short cut key
  QAction* action = CommandManager::instance()->getActionFromShortcut("Return");
  if (action) action->setShortcut(QKeySequence("Return"));

  disconnect(m_videoWidget->videoSurface(), SIGNAL(frameCaptured(QImage&)),
             this, SLOT(onFrameCaptured(QImage&)));

  // stop interval timer if it is active
  if (m_timerCB->isChecked() && m_captureButton->isChecked()) {
    m_captureButton->setChecked(false);
    onCaptureButtonClicked(false);
  }

  // release camera
  if (m_currentCamera) {
    if (m_currentCamera->state() == QCamera::ActiveState)
      m_currentCamera->stop();
    if (m_currentCamera->state() == QCamera::LoadedState)
      m_currentCamera->unload();
  }
  Dialog::hideEvent(event);

  TSceneHandle* sceneHandle = TApp::instance()->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
             SLOT(refreshFrameInfo()));
  disconnect(sceneHandle, SIGNAL(castChanged()), this,
             SLOT(refreshFrameInfo()));
}

//-----------------------------------------------------------------------------

void PencilTestPopup::keyPressEvent(QKeyEvent* event) {
  // override return (or enter) key as shortcut key for capturing
  int key = event->key();
  if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    // show button-clicking animation followed by calling
    // onCaptureButtonClicked()
    m_captureButton->animateClick();
    event->accept();
  } else
    event->ignore();
}

//-----------------------------------------------------------------------------

bool PencilTestPopup::event(QEvent* event) {
  if (event->type() == QEvent::ShortcutOverride) {
    QKeyEvent* ke = static_cast<QKeyEvent*>(event);
    int key       = ke->key();
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
      if (!m_frameNumberEdit->hasFocus()) {
        m_frameNumberEdit->setFocus();
        m_frameNumberEdit->clear();
      }
      event->accept();
      return true;
    }
  }
  return DVGui::Dialog::event(event);
}
//-----------------------------------------------------------------------------

void PencilTestPopup::processImage(QImage& image) {
  /* "upside down" is not executed here. It will be done when capturing the
   * image */
  // white bg reduction
  if (!m_whiteBGImg.isNull() && m_bgReductionFld->getValue() != 0) {
    bgReduction(image, m_whiteBGImg, m_bgReductionFld->getValue());
  }
  // obtain histogram AFTER bg reduction
  m_camCapLevelControl->updateHistogram(image);

  // color and grayscale mode
  if (m_colorTypeCombo->currentIndex() != 2) {
    int black, white;
    float gamma;
    m_camCapLevelControl->getValues(black, white, gamma);
    if (black != 0 || white != 255 || gamma != 1.0)
      onChange(image, black, white, gamma,
               m_colorTypeCombo->currentIndex() != 0);
  } else {
    onChangeBW(image, m_camCapLevelControl->getThreshold());
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureWhiteBGButtonPressed() {
  m_captureWhiteBGCue = true;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onOnionCBToggled(bool on) {
  m_videoWidget->setShowOnionSkin(on);
  m_onionOpacityFld->setEnabled(on);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onLoadImageButtonPressed() {
  TCellSelection* cellSelection = dynamic_cast<TCellSelection*>(
      TApp::instance()->getCurrentSelection()->getSelection());
  if (cellSelection) {
    int c0;
    int r0;
    TCellSelection::Range range = cellSelection->getSelectedCells();
    if (range.isEmpty()) {
      error(tr("No image selected.  Please select an image in the Xsheet."));
      return;
    }
    c0 = range.m_c0;
    r0 = range.m_r0;
    TXshCell cell =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getCell(r0, c0);
    if (cell.getSimpleLevel() == nullptr) {
      error(tr("No image selected.  Please select an image in the Xsheet."));
      return;
    }
    TXshSimpleLevel* level = cell.getSimpleLevel();
    int type               = level->getType();
    if (type != TXshLevelType::OVL_XSHLEVEL) {
      error(tr("The selected image is not in a raster level."));
      return;
    }
    TImageP origImage = cell.getImage(false);
    TRasterImageP tempImage(origImage);
    TRasterImage* image = (TRasterImage*)tempImage->cloneImage();

    int m_lx          = image->getRaster()->getLx();
    int m_ly          = image->getRaster()->getLy();
    QString res       = m_resolutionCombo->currentText();
    QStringList texts = res.split(' ');
    if (m_lx != texts[0].toInt() || m_ly != texts[2].toInt()) {
      error(
          tr("The selected image size does not match the current camera "
             "settings."));
      return;
    }
    int m_bpp      = image->getRaster()->getPixelSize();
    int totalBytes = m_lx * m_ly * m_bpp;
    image->getRaster()->yMirror();

    // lock raster to get data
    image->getRaster()->lock();
    void* buffin = image->getRaster()->getRawData();
    assert(buffin);
    void* buffer = malloc(totalBytes);
    memcpy(buffer, buffin, totalBytes);

    image->getRaster()->unlock();

    QImage qi = QImage((uint8_t*)buffer, m_lx, m_ly, QImage::Format_ARGB32);
    QImage qi2(qi.size(), QImage::Format_ARGB32);
    qi2.fill(QColor(Qt::white).rgb());
    QPainter painter(&qi2);
    painter.drawImage(0, 0, qi);
    m_videoWidget->setPreviousImage(qi2);
    m_onionSkinCB->setChecked(true);
    free(buffer);
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onOnionOpacityFldEdited() {
  int value = (int)(255.0f * (float)m_onionOpacityFld->getValue() / 100.0f);
  m_videoWidget->setOnionOpacity(value);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onTimerCBToggled(bool on) {
  m_timerIntervalFld->setEnabled(on);
  m_captureButton->setCheckable(on);
  if (on)
    m_captureButton->setText(tr("Start Capturing\n[Return key]"));
  else
    m_captureButton->setText(tr("Capture\n[Return key]"));
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureButtonClicked(bool on) {
  if (m_timerCB->isChecked()) {
    m_timerCB->setDisabled(on);
    m_timerIntervalFld->setDisabled(on);
    // start interval capturing
    if (on) {
      m_captureButton->setText(tr("Stop Capturing\n[Return key]"));
      m_captureTimer->start(m_timerIntervalFld->getValue() * 1000);
      if (m_timerIntervalFld->getValue() != 0) m_countdownTimer->start(100);
    }
    // stop interval capturing
    else {
      m_captureButton->setText(tr("Start Capturing\n[Return key]"));
      m_captureTimer->stop();
      m_countdownTimer->stop();
      // hide the count down text
      m_videoWidget->showCountDownTime(0);
    }
  }
  // capture immediately
  else
    m_captureCue = true;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureTimerTimeout() { m_captureCue = true; }

//-----------------------------------------------------------------------------

void PencilTestPopup::onCountDown() {
  m_videoWidget->showCountDownTime(
      m_captureTimer->isActive() ? m_captureTimer->remainingTime() : 0);
}

//-----------------------------------------------------------------------------
/*! referenced from LevelCreatePopup::apply()
 */
bool PencilTestPopup::importImage(QImage image) {
  TApp* app         = TApp::instance();
  ToonzScene* scene = app->getCurrentScene()->getScene();
  TXsheet* xsh      = scene->getXsheet();

  std::wstring levelName = m_levelNameEdit->text().toStdWString();
  if (levelName.empty()) {
    error(tr("No level name specified: please choose a valid level name"));
    return false;
  }

  TFrameId fId = m_frameNumberEdit->getValue();

  /* create parent directory if it does not exist */
  TFilePath parentDir =
      scene->decodeFilePath(TFilePath(m_saveInFileFld->getPath()));
  if (!TFileStatus(parentDir).doesExist()) {
    QString question;
    question = tr("Folder %1 doesn't exist.\nDo you want to create it?")
                   .arg(toQString(parentDir));
    int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
    if (ret == 0 || ret == 2) return false;
    try {
      TSystem::mkDir(parentDir);
      DvDirModel::instance()->refreshFolder(parentDir.getParentDir());
    } catch (...) {
      error(tr("Unable to create") + toQString(parentDir));
      return false;
    }
  }

  TFrameId tmplFId = scene->getProperties()->formatTemplateFIdForInput();

  TFilePath levelFp = TFilePath(m_saveInFileFld->getPath()) +
                      TFilePath(levelName + L"." +
                                m_fileTypeCombo->currentText().toStdWString())
                          .withFrame(tmplFId);
  TFilePath actualLevelFp = scene->decodeFilePath(levelFp);

  TXshSimpleLevel* sl = 0;

  TXshLevel* level = scene->getLevelSet()->getLevel(levelName);
  enum State { NEWLEVEL = 0, ADDFRAME, OVERWRITE } state;

  // retrieve subcamera image
  if (m_subcameraButton->isChecked() &&
      m_videoWidget->subCameraRect().isValid())
    image = image.copy(m_videoWidget->subCameraRect());

  /* if the level already exists in the scene cast */
  if (level) {
    /* if the existing level is not a raster level, then return */
    if (level->getType() != OVL_XSHLEVEL) {
      error(
          tr("The level name specified is already used: please choose a "
             "different level name."));
      return false;
    }
    /* if the existing level does not match file path and pixel size, then
     * return */
    sl = level->getSimpleLevel();
    if (scene->decodeFilePath(sl->getPath()) != actualLevelFp) {
      error(
          tr("The save in path specified does not match with the existing "
             "level."));
      return false;
    }
    if (sl->getProperties()->getImageRes() !=
        TDimension(image.width(), image.height())) {
      error(tr(
          "The captured image size does not match with the existing level."));
      return false;
    }

    // if the level already has a frame, use the same zero padding regardless of
    // the frame format setting
    sl->formatFId(fId, tmplFId);

    /* if the level already have the same frame, then ask if overwrite it */
    TFilePath frameFp(actualLevelFp.withFrame(fId));
    if (TFileStatus(frameFp).doesExist()) {
      if (!m_alwaysOverwrite) {
        QString question =
            tr("File %1 does exist.\nDo you want to overwrite it?")
                .arg(toQString(frameFp));
        int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                                QObject::tr("Always Overwrite in This Scene"),
                                QObject::tr("Cancel"));
        if (ret == 0 || ret == 3) return false;
        if (ret == 2) m_alwaysOverwrite = true;
      }
      state = OVERWRITE;
    } else
      state = ADDFRAME;
  }
  /* if the level does not exist in the scene cast */
  else {
    /* if the file does exist, load it first */
    if (TSystem::doesExistFileOrLevel(actualLevelFp)) {
      level = scene->loadLevel(actualLevelFp);
      if (!level) {
        error(tr("Failed to load %1.").arg(toQString(actualLevelFp)));
        return false;
      }

      /* if the loaded level does not match in pixel size, then return */
      sl = level->getSimpleLevel();
      if (!sl || sl->getProperties()->getImageRes() !=
                     TDimension(image.width(), image.height())) {
        error(tr(
            "The captured image size does not match with the existing level."));
        return false;
      }
      // if the level already has a frame, use the same zero padding regardless
      // of the frame format setting
      sl->formatFId(fId, tmplFId);

      /* confirm overwrite */
      TFilePath frameFp(actualLevelFp.withFrame(fId));
      if (TFileStatus(frameFp).doesExist()) {
        if (!m_alwaysOverwrite) {
          QString question =
              tr("File %1 does exist.\nDo you want to overwrite it?")
                  .arg(toQString(frameFp));
          int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                                  QObject::tr("Always Overwrite in This Scene"),
                                  QObject::tr("Cancel"));
          if (ret == 0 || ret == 3) return false;
          if (ret == 2) m_alwaysOverwrite = true;
        }
      }
    }
    /* if the file does not exist, then create a new level */
    else {
      TXshLevel* level = scene->createNewLevel(OVL_XSHLEVEL, levelName,
                                               TDimension(), 0, levelFp);
      sl               = level->getSimpleLevel();
      sl->setPath(levelFp, true);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
      TPointD dpi;
      // if the subcamera is not active or the pixel unit is used, apply the
      // current camera dpi
      if (!m_subcameraButton->isChecked() ||
          !m_videoWidget->subCameraRect().isValid() ||
          Preferences::instance()->getPixelsOnly())
        dpi = getCurrentCameraDpi();
      // if the subcamera is active, compute the dpi so that the image will fit
      // to the camera frame
      else {
        TCamera* camera =
            TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
        TDimensionD size = camera->getSize();
        double minimumDpi =
            std::min(image.width() / size.lx, image.height() / size.ly);
        dpi = TPointD(minimumDpi, minimumDpi);
      }
      sl->getProperties()->setDpi(dpi.x);
      sl->getProperties()->setImageDpi(dpi);
      sl->getProperties()->setImageRes(
          TDimension(image.width(), image.height()));
      sl->formatFId(fId, tmplFId);
    }

    state = NEWLEVEL;
  }

  TPointD levelDpi = sl->getDpi();
  /* create the raster */
  TRaster32P raster(image.width(), image.height());
  convertImageToRaster(raster, image.mirrored(true, true));

  TRasterImageP ri(raster);
  ri->setDpi(levelDpi.x, levelDpi.y);
  /* setting the frame */
  sl->setFrame(fId, ri);

  /* set dirty flag */
  sl->getProperties()->setDirtyFlag(true);

  if (m_saveOnCaptureCB->isChecked()) sl->save();

  /* placement in xsheet */

  int row = app->getCurrentFrame()->getFrame();
  int col = app->getCurrentColumn()->getColumnIndex();

  // if the level is newly created or imported, then insert a new column
  if (state == NEWLEVEL) {
    if (col < 0 || !xsh->isColumnEmpty(col)) {
      col += 1;
      xsh->insertColumn(col);
    }
    xsh->setCell(row, col, TXshCell(sl, fId));
    app->getCurrentColumn()->setColumnIndex(col);
    return true;
  }

  // state == OVERWRITE, ADDFRAME

  // if the same cell is already in the column, then just replace the content
  // and do not set a new cell
  int foundCol, foundRow = -1;
  // most possibly, it's in the current column
  int rowCheck;
  if (findCell(xsh, col, TXshCell(sl, fId), rowCheck)) return true;
  if (rowCheck >= 0) {
    foundRow = rowCheck;
    foundCol = col;
  }
  // search entire xsheet
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    if (c == col) continue;
    if (findCell(xsh, c, TXshCell(sl, fId), rowCheck)) return true;
    if (rowCheck >= 0) {
      foundRow = rowCheck;
      foundCol = c;
    }
  }
  // if there is a column containing the same level
  if (foundRow >= 0) {
    // put the cell at the bottom
    int tmpRow = foundRow + 1;
    while (1) {
      if (xsh->getCell(tmpRow, foundCol).isEmpty()) {
        xsh->setCell(tmpRow, foundCol, TXshCell(sl, fId));
        app->getCurrentColumn()->setColumnIndex(foundCol);
        break;
      }
      tmpRow++;
    }
  }
  // if the level is registered in the scene, but is not placed in the xsheet,
  // then insert a new column
  else {
    if (!xsh->isColumnEmpty(col)) {
      col += 1;
      xsh->insertColumn(col);
    }
    xsh->setCell(row, col, TXshCell(sl, fId));
    app->getCurrentColumn()->setColumnIndex(col);
  }

  return true;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureFilterSettingsBtnPressed() {
  if (!m_currentCamera || m_deviceName.isNull()) return;

  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  for (int c = 0; c < cameras.size(); c++) {
    if (cameras.at(c).deviceName() == m_deviceName) {
#ifdef _WIN32
      openCaptureFilterSettings(this, cameras.at(c).description());
#endif
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::openSaveInFolderPopup() {
  if (m_saveInFolderPopup->exec()) {
    QString oldPath = m_saveInFileFld->getPath();
    m_saveInFileFld->setPath(m_saveInFolderPopup->getPath());
    if (oldPath == m_saveInFileFld->getPath())
      setToNextNewLevel();
    else {
      onSaveInPathEdited();
    }
  }
}

//-----------------------------------------------------------------------------

// Refresh information that how many & which frames are saved for the current
// level
void PencilTestPopup::refreshFrameInfo() {
  if (!m_currentCamera || m_deviceName.isNull()) {
    m_frameInfoLabel->setText("");
    return;
  }

  QString tooltipStr, labelStr;
  enum InfoType { NEW = 0, ADD, OVERWRITE, WARNING } infoType(WARNING);

  static QColor infoColors[4] = {Qt::cyan, Qt::green, Qt::yellow, Qt::red};

  ToonzScene* currentScene = TApp::instance()->getCurrentScene()->getScene();
  TLevelSet* levelSet      = currentScene->getLevelSet();

  std::wstring levelName = m_levelNameEdit->text().toStdWString();
  TFrameId fId           = m_frameNumberEdit->getValue();

  TDimension camRes;
  if (m_subcameraButton->isChecked())
    camRes = TDimension(m_subWidthFld->getValue(), m_subHeightFld->getValue());
  else {
    QStringList texts = m_resolutionCombo->currentText().split(' ');
    if (texts.size() != 3) return;
    camRes = TDimension(texts[0].toInt(), texts[2].toInt());
  }

  bool letterOptionEnabled =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled();

  // level with the same name
  TXshLevel* level_sameName = levelSet->getLevel(levelName);

  TOutputProperties* prop =
      currentScene->getProperties()->getPreviewProperties();
  TFrameId tmplFId = prop->formatTemplateFId();

  TFilePath levelFp = TFilePath(m_saveInFileFld->getPath()) +
                      TFilePath(levelName + L"." +
                                m_fileTypeCombo->currentText().toStdWString())
                          .withFrame(tmplFId);

  // level with the same path
  TXshLevel* level_samePath = levelSet->getLevel(*(currentScene), levelFp);

  TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);

  // level existence
  bool levelExist = TSystem::doesExistFileOrLevel(actualLevelFp);

  // frame existence
  TFilePath frameFp(actualLevelFp.withFrame(fId));
  bool frameExist = false;
  if (levelExist) frameExist = TFileStatus(frameFp).doesExist();

  // reset acceptable camera size
  m_allowedCameraSize = QSize();

  // ### CASE 1 ###
  // If there is no same level registered in the scene cast
  if (!level_sameName && !level_samePath) {
    // If there is a level in the file system
    if (levelExist) {
      TLevelReaderP lr;
      TLevelP level_p;
      try {
        lr = TLevelReaderP(actualLevelFp);
      } catch (...) {
        // TODO: output something
        m_frameInfoLabel->setText(tr("UNDEFINED WARNING"));
        return;
      }
      if (!lr) {
        // TODO: output something
        m_frameInfoLabel->setText(tr("UNDEFINED WARNING"));
        return;
      }
      try {
        level_p = lr->loadInfo();
      } catch (...) {
        // TODO: output something
        m_frameInfoLabel->setText(tr("UNDEFINED WARNING"));
        return;
      }
      if (!level_p) {
        // TODO: output something
        m_frameInfoLabel->setText(tr("UNDEFINED WARNING"));
        return;
      }
      int frameCount      = level_p->getFrameCount();
      TLevel::Iterator it = level_p->begin();
      std::vector<TFrameId> fids;
      for (int i = 0; it != level_p->end(); ++it, ++i) {
        fids.push_back(it->first);
        // in case fId with different format
        if (!frameExist && fId == it->first) frameExist = true;
      }

      tooltipStr +=
          tr("The level is not registered in the scene, but exists in the file "
             "system.");

      // check resolution
      const TImageInfo* ii;
      try {
        ii = lr->getImageInfo(fids[0]);
      } catch (...) {
        // TODO: output something
        m_frameInfoLabel->setText(tr("UNDEFINED WARNING"));
        return;
      }
      TDimension dim(ii->m_lx, ii->m_ly);
      // if the saved images has not the same resolution as the current camera
      // resolution
      if (camRes != dim) {
        tooltipStr += tr("\nWARNING : Image size mismatch. The saved image "
                         "size is %1 x %2.")
                          .arg(dim.lx)
                          .arg(dim.ly);
        labelStr += tr("WARNING");
        infoType = WARNING;
      }
      // if the resolutions are matched
      else {
        if (frameCount == 1)
          tooltipStr += tr("\nFrame %1 exists.")
                            .arg(fidsToString(fids, letterOptionEnabled));
        else
          tooltipStr += tr("\nFrames %1 exist.")
                            .arg(fidsToString(fids, letterOptionEnabled));
        // if the frame exists, then it will be overwritten
        if (frameExist) {
          labelStr += tr("OVERWRITE 1 of");
          infoType = OVERWRITE;
        } else {
          labelStr += tr("ADD to");
          infoType = ADD;
        }
        if (frameCount == 1)
          labelStr += tr(" %1 frame").arg(frameCount);
        else
          labelStr += tr(" %1 frames").arg(frameCount);
      }
      m_allowedCameraSize = QSize(dim.lx, dim.ly);
    }
    // If no level exists in the file system, then it will be a new level
    else {
      tooltipStr += tr("The level will be newly created.");
      labelStr += tr("NEW");
      infoType = NEW;
    }
  }
  // ### CASE 2 ###
  // If there is already the level registered in the scene cast
  else if (level_sameName && level_samePath &&
           level_sameName == level_samePath) {
    tooltipStr += tr("The level is already registered in the scene.");
    if (!levelExist) tooltipStr += tr("\nNOTE : The level is not saved.");

    std::vector<TFrameId> fids;
    level_sameName->getFids(fids);

    // check resolution
    TDimension dim;
    bool ret = getRasterLevelSize(level_sameName, dim);
    if (!ret) {
      tooltipStr +=
          tr("\nWARNING : Failed to get image size of the existing level %1.")
              .arg(QString::fromStdWString(levelName));
      labelStr += tr("WARNING");
      infoType = WARNING;
    }
    // if the saved images has not the same resolution as the current camera
    // resolution
    else if (camRes != dim) {
      tooltipStr += tr("\nWARNING : Image size mismatch. The existing level "
                       "size is %1 x %2.")
                        .arg(dim.lx)
                        .arg(dim.ly);
      labelStr += tr("WARNING");
      infoType = WARNING;
    }
    // if the resolutions are matched
    else {
      int frameCount = fids.size();
      if (fids.size() == 1)
        tooltipStr += tr("\nFrame %1 exists.")
                          .arg(fidsToString(fids, letterOptionEnabled));
      else
        tooltipStr += tr("\nFrames %1 exist.")
                          .arg(fidsToString(fids, letterOptionEnabled));
      // Check if the target frame already exist in the level
      bool hasFrame = false;
      for (int f = 0; f < frameCount; f++) {
        if (fids.at(f) == fId) {
          hasFrame = true;
          break;
        }
      }
      // If there is already the frame then it will be overwritten
      if (hasFrame) {
        labelStr += tr("OVERWRITE 1 of");
        infoType = OVERWRITE;
      }
      // Or, the frame will be added to the level
      else {
        labelStr += tr("ADD to");
        infoType = ADD;
      }
      if (frameCount == 1)
        labelStr += tr(" %1 frame").arg(frameCount);
      else
        labelStr += tr(" %1 frames").arg(frameCount);
    }
    m_allowedCameraSize = QSize(dim.lx, dim.ly);
  }
  // ### CASE 3 ###
  // If there are some conflicts with the existing level.
  else {
    if (level_sameName) {
      TFilePath anotherPath = level_sameName->getPath();
      tooltipStr +=
          tr("WARNING : Level name conflicts. There already is a level %1 in the scene with the path\
                        \n          %2.")
              .arg(QString::fromStdWString(levelName))
              .arg(toQString(anotherPath));
      // check resolution
      TDimension dim;
      bool ret = getRasterLevelSize(level_sameName, dim);
      if (ret && camRes != dim)
        tooltipStr += tr("\nWARNING : Image size mismatch. The size of level "
                         "with the same name is is %1 x %2.")
                          .arg(dim.lx)
                          .arg(dim.ly);
      m_allowedCameraSize = QSize(dim.lx, dim.ly);
    }
    if (level_samePath) {
      std::wstring anotherName = level_samePath->getName();
      if (!tooltipStr.isEmpty()) tooltipStr += QString("\n");
      tooltipStr +=
          tr("WARNING : Level path conflicts. There already is a level with the path %1\
                        \n          in the scene with the name %2.")
              .arg(toQString(levelFp))
              .arg(QString::fromStdWString(anotherName));
      // check resolution
      TDimension dim;
      bool ret = getRasterLevelSize(level_samePath, dim);
      if (ret && camRes != dim)
        tooltipStr += tr("\nWARNING : Image size mismatch. The size of level "
                         "with the same path is %1 x %2.")
                          .arg(dim.lx)
                          .arg(dim.ly);
      m_allowedCameraSize = QSize(dim.lx, dim.ly);
    }
    labelStr += tr("WARNING");
    infoType = WARNING;
  }

  QColor infoColor = infoColors[(int)infoType];
  m_frameInfoLabel->setStyleSheet(QString("QLabel{color: %1;}\
                                          QLabel QWidget{ color: black;}")
                                      .arg(infoColor.name()));
  m_frameInfoLabel->setText(labelStr);
  m_frameInfoLabel->setToolTip(tooltipStr);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSaveInPathEdited() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath saveInPath(m_saveInFileFld->getPath().toStdWString());
  scene->getProperties()->setCameraCaptureSaveInPath(saveInPath);
  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSceneSwitched() {
  m_saveInFolderPopup->updateParentFolder();
  m_saveInFileFld->setPath(m_saveInFolderPopup->getParentPath());
  refreshFrameInfo();
  m_alwaysOverwrite = false;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSubCameraToggled(bool on) {
  m_videoWidget->setSubCameraSize(
      on ? QSize(m_subWidthFld->getValue(), m_subHeightFld->getValue())
         : QSize());
  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSubCameraResized(bool isDragging) {
  QSize subSize = m_videoWidget->subCameraRect().size();
  assert(subSize.isValid());
  m_subWidthFld->setValue(subSize.width());
  m_subHeightFld->setValue(subSize.height());
  if (!isDragging) refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSubCameraSizeEdited() {
  m_videoWidget->setSubCameraSize(
      QSize(m_subWidthFld->getValue(), m_subHeightFld->getValue()));
  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<PencilTestPopup> openPencilTestPopup(MI_PencilTest);

// specialized in order to call openSaveInFolderPopup()
template <>
void OpenPopupCommandHandler<PencilTestPopup>::execute() {
  if (!m_popup) m_popup = new PencilTestPopup();
  m_popup->show();
  m_popup->raise();
  m_popup->activateWindow();
  if (CamCapOpenSaveInPopupOnLaunch != 0) m_popup->openSaveInFolderPopup();
}
