#include "penciltestpopup.h"

#ifdef WIN32
#include <Windows.h>
#include <mfobjects.h>
#include <mfapi.h>
#include <mfidl.h>
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "filebrowsermodel.h"
#include "cellselection.h"
#include "toonzqt/tselectionhandle.h"
#include "cameracapturelevelcontrol.h"
#include "iocommand.h"
#include "filebrowser.h"

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
#include <QHostInfo>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QMessageBox>

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
TEnv::IntVar CamCapUseMjpg("CamCapUseMjpg", 1);
#ifdef _WIN32
TEnv::IntVar CamCapUseDirectShow("CamCapUseDirectShow", 1);
#endif
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
TEnv::IntVar CamCapDoCalibration("CamCapDoCalibration", 0);

TEnv::RectVar CamCapSubCameraRect("CamCapSubCameraRect", TRect());
TEnv::IntVar CamCapDoAutoDpi("CamCapDoAutoDpi", 1);
TEnv::DoubleVar CamCapCustomDpi("CamCapDpiForNewLevel", 120.0);
TEnv::StringVar CamCapFileType("CamCapFileType", "jpg");

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

void bgReduction(cv::Mat& srcImg, cv::Mat& bgImg, int reduction) {
  // void bgReduction(QImage& srcImg, QImage& bgImg, int reduction) {
  if (srcImg.cols != bgImg.cols || srcImg.rows != bgImg.rows) return;
  float reductionRatio = (float)reduction / 100.0f;
  // first, make the reduction table
  std::vector<int> reductionAmount(256);
  for (int i = 0; i < reductionAmount.size(); i++) {
    reductionAmount[i] = (int)(std::floor((float)(255 - i) * reductionRatio));
  }
  // then, compute for all pixels
  int lx = srcImg.cols;
  int ly = srcImg.rows;
  for (int j = 0; j < srcImg.rows; j++) {
    cv::Vec3b* pix   = srcImg.ptr<cv::Vec3b>(j);
    cv::Vec3b* bgPix = bgImg.ptr<cv::Vec3b>(j);
    for (int i = 0; i < srcImg.cols; i++, pix++, bgPix++) {
      *pix = cv::Vec3b(std::min(255, (*pix)[0] + reductionAmount[(*bgPix)[0]]),
                       std::min(255, (*pix)[1] + reductionAmount[(*bgPix)[1]]),
                       std::min(255, (*pix)[2] + reductionAmount[(*bgPix)[2]]));
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
      int num             = fids[f].getNumber();
      QString letter      = fids[f].getLetter();
      int next_num        = fids[f + 1].getNumber();
      QString next_letter = fids[f + 1].getLetter();

      if (num + 1 == next_num && letter.isEmpty() && next_letter.isEmpty()) {
        if (beginBlock) {
          retStr += QString::number(num) + " - ";
          beginBlock = false;
        }
      } else {
        retStr += QString::number(num);
        if (!letter.isEmpty()) retStr += letter;
        retStr += ", ";
        beginBlock = true;
      }
    }
    retStr += QString::number(fids.back().getNumber());
    if (!fids.back().getLetter().isEmpty()) retStr += fids.back().getLetter();
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

class IconView : public QWidget {
  QIcon m_icon;

public:
  IconView(const QString& iconName, const QString& toolTipStr = "",
           QWidget* parent = 0)
      : QWidget(parent), m_icon(createQIcon(iconName.toUtf8())) {
    setMinimumSize(18, 18);
    setToolTip(toolTipStr);
  }

protected:
  void paintEvent(QPaintEvent* e) {
    QPainter p(this);
    p.drawPixmap(QRect(0, 2, 18, 18), m_icon.pixmap(18, 18));
  }
};

// https://stackoverflow.com/a/50848100/6622587
cv::Mat QImageToMat(const QImage& image) {
  cv::Mat out;
  switch (image.format()) {
  case QImage::Format_Invalid: {
    cv::Mat empty;
    empty.copyTo(out);
    break;
  }
  case QImage::Format_RGB32: {
    cv::Mat view(image.height(), image.width(), CV_8UC4,
                 (void*)image.constBits(), image.bytesPerLine());
    view.copyTo(out);
    break;
  }
  case QImage::Format_RGB888: {
    cv::Mat view(image.height(), image.width(), CV_8UC3,
                 (void*)image.constBits(), image.bytesPerLine());
    cv::cvtColor(view, out, cv::COLOR_RGB2BGR);
    break;
  }
  default: {
    QImage conv = image.convertToFormat(QImage::Format_ARGB32);
    cv::Mat view(conv.height(), conv.width(), CV_8UC4, (void*)conv.constBits(),
                 conv.bytesPerLine());
    view.copyTo(out);
    break;
  }
  }
  return out;
}

}  // namespace

//=============================================================================

MyVideoWidget::MyVideoWidget(QWidget* parent)
    : QWidget(parent)
    , m_previousImage(QImage())
    , m_showOnionSkin(false)
    , m_onionOpacity(128)
    , m_upsideDown(false)
    , m_countDownTime(0)
    , m_subCameraRect(QRect()) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

  setMouseTracking(true);
}

void MyVideoWidget::paintEvent(QPaintEvent* event) {
  QPainter p(this);

  p.fillRect(rect(), Qt::black);

  if (!m_image.isNull()) {
    p.drawImage(m_targetRect, m_image);

    if (m_showOnionSkin && m_onionOpacity > 0.0f && !m_previousImage.isNull() &&
        m_previousImage.size() == m_image.size()) {
      p.setOpacity((qreal)m_onionOpacity / 255.0);
      p.drawImage(m_targetRect, m_previousImage);
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
  computeTransform(m_image.size());
}

void MyVideoWidget::computeTransform(QSize imgSize) {
  QSize adjustedSize = imgSize;
  adjustedSize.scale(size(), Qt::KeepAspectRatio);
  m_targetRect = QRect(QPoint(), adjustedSize);
  m_targetRect.moveCenter(rect().center());

  double scale = (double)m_targetRect.width() / (double)imgSize.width();
  m_S2V_Transform =
      QTransform::fromTranslate(m_targetRect.left(), m_targetRect.top())
          .scale(scale, scale);
}

void MyVideoWidget::setSubCameraRect(QRect rect) {
  QSize frameSize = m_image.size();
  assert(frameSize == rect.size().expandedTo(frameSize));

  m_subCameraRect = rect;
  // make sure the sub camera is inside of the frame
  if (rect.isValid() &&
      !QRect(QPoint(0, 0), frameSize).contains(m_subCameraRect)) {
    m_subCameraRect.moveCenter(QRect(QPoint(0, 0), frameSize).center());
    emit subCameraChanged(false);
  }

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

  QRect vidSubRect = m_S2V_Transform.mapRect(m_subCameraRect);
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
  if (m_image.isNull() || m_subCameraRect.isNull()) return;

  // with no mouse button, update the active handles
  if (event->buttons() == Qt::NoButton) {
    QRect vidSubRect    = m_S2V_Transform.mapRect(m_subCameraRect);
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
        m_S2V_Transform.inverted().map(event->pos()) - m_dragStartPos;
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
    QSize camSize = m_image.size();

    if (m_activeSubHandle == HandleFrame) {
      clampPoint(offset, -m_preSubCameraRect.left(),
                 camSize.width() - m_preSubCameraRect.right() - 1,
                 -m_preSubCameraRect.top(),
                 camSize.height() - m_preSubCameraRect.bottom() - 1);
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
                 camSize.width() - m_preSubCameraRect.right() - 1);
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
                 camSize.height() - m_preSubCameraRect.bottom() - 1);
        m_subCameraRect.setBottom(m_preSubCameraRect.bottom() + offset.y());
      }
    }
    // if the sub camera size is changed, notify the parent for updating the
    // fields
    emit subCameraChanged(true);
    update();
  }
}

void MyVideoWidget::mousePressEvent(QMouseEvent* event) {
  // if the sub camera is not active, do nothing and return
  // use left button only and some handle must be active
  if (m_image.isNull() || m_subCameraRect.isNull() ||
      event->button() != Qt::LeftButton || m_activeSubHandle == HandleNone)
    return;

  // record the original sub camera size
  m_preSubCameraRect = m_subCameraRect;
  m_dragStartPos     = m_S2V_Transform.inverted().map(event->pos());

  // temporary stop the camera
  emit stopCamera();
}

void MyVideoWidget::mouseReleaseEvent(QMouseEvent* event) {
  // if the sub camera is not active, do nothing and return
  // use left button only and some handle must be active
  if (m_image.isNull() || m_subCameraRect.isNull() ||
      event->button() != Qt::LeftButton || m_activeSubHandle == HandleNone)
    return;

  m_preSubCameraRect = QRect();

  emit subCameraChanged(false);

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
  updateSize();
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
namespace {

bool strToSubCamera(const QString& str, QRect& subCamera, double& dpi) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  QStringList values = str.split(',', Qt::SkipEmptyParts);
#else
  QStringList values         = str.split(',', QString::SkipEmptyParts);
#endif
  if (values.count() != 4 && values.count() != 5) return false;
  subCamera = QRect(values[0].toInt(), values[1].toInt(), values[2].toInt(),
                    values[3].toInt());
  dpi       = (values.count() == 5) ? values[4].toDouble() : -1.;
  return true;
}

const QString subCameraToStr(const QRect& subCamera, const double dpi = -1.) {
  QString ret = QString("%1,%2,%3,%4")
                    .arg(subCamera.left())
                    .arg(subCamera.top())
                    .arg(subCamera.width())
                    .arg(subCamera.height());
  if (dpi > 0.) ret += QString(",%1").arg(dpi);
  return ret;
}
}  // namespace

SubCameraButton::SubCameraButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent), m_currentDpi(-1.) {
  setObjectName("SubcameraButton");
  setIconSize(QSize(16, 16));
  setIcon(createQIcon("subcamera"));
  setCheckable(true);

  // load preference file
  TFilePath layoutDir = ToonzFolder::getMyModuleDir();
  TFilePath prefPath  = layoutDir + TFilePath("camera_capture_subcamera.ini");
  // In case the personal settings is not exist (for new users)
  if (!TFileStatus(prefPath).doesExist()) {
    TFilePath templatePath = ToonzFolder::getTemplateModuleDir() +
                             TFilePath("camera_capture_subcamera.ini");
    // If there is the template, copy it to the personal one
    if (TFileStatus(templatePath).doesExist())
      TSystem::copyFile(prefPath, templatePath);
  }
  m_settings.reset(new QSettings(
      QString::fromStdWString(prefPath.getWideString()), QSettings::IniFormat));
}

void SubCameraButton::contextMenuEvent(QContextMenuEvent* event) {
  // return if the current camera is not valid
  if (!m_curResolution.isValid()) return;
  // load presets for the current resolution
  QString groupName = QString("%1x%2")
                          .arg(m_curResolution.width())
                          .arg(m_curResolution.height());
  m_settings->beginGroup(groupName);

  bool hasPresets = !m_settings->childKeys().isEmpty();

  if (!m_curSubCamera.isValid() && !hasPresets) {
    m_settings->endGroup();
    return;
  }

  QMenu menu(this);
  menu.setToolTipsVisible(true);

  for (auto key : m_settings->childKeys()) {
    QAction* scAct = menu.addAction(key);
    scAct->setData(m_settings->value(key));
    QRect rect;
    double dpi;
    if (!strToSubCamera(m_settings->value(key).toString(), rect, dpi)) continue;
    if (m_curSubCamera == rect && m_currentDpi == dpi) {
      scAct->setCheckable(true);
      scAct->setChecked(true);
      scAct->setEnabled(false);
    } else {
      QString toolTip = QString("%1 x %2, X%3 Y%4")
                            .arg(rect.width())
                            .arg(rect.height())
                            .arg(rect.x())
                            .arg(rect.y());
      if (dpi > 0.) toolTip += QString(", %1DPI").arg(dpi);
      scAct->setToolTip(toolTip);
      connect(scAct, SIGNAL(triggered()), this, SLOT(onSubCameraAct()));
    }
  }

  if (hasPresets) menu.addSeparator();

  // save preset (visible if the subcamera is active)
  if (m_curSubCamera.isValid()) {
    QAction* saveAct = menu.addAction(tr("Save Current Subcamera"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(onSaveSubCamera()));
  }

  // delete preset (visible if there is any)
  if (hasPresets) {
    QMenu* delMenu = menu.addMenu(tr("Delete Preset"));
    for (auto key : m_settings->childKeys()) {
      QAction* delAct = delMenu->addAction(tr("Delete %1").arg(key));
      delAct->setData(key);
      connect(delAct, SIGNAL(triggered()), this, SLOT(onDeletePreset()));
    }
  }

  m_settings->endGroup();

  menu.exec(event->globalPos());
}

void SubCameraButton::onSubCameraAct() {
  QRect subCameraRect;
  double dpi;
  if (strToSubCamera(qobject_cast<QAction*>(sender())->data().toString(),
                     subCameraRect, dpi))
    emit subCameraPresetSelected(subCameraRect, dpi);
}

void SubCameraButton::onSaveSubCamera() {
  auto initDialog = [&](QLineEdit** lineEdit) {
    QDialog* ret = new QDialog();
    *lineEdit    = new QLineEdit(ret);
    QDialogButtonBox* buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QVBoxLayout* lay = new QVBoxLayout();
    lay->setMargin(5);
    lay->setSpacing(10);
    lay->addWidget(*lineEdit);
    lay->addWidget(buttonBox);
    ret->setLayout(lay);
    connect(buttonBox, &QDialogButtonBox::accepted, ret, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, ret, &QDialog::reject);
    return ret;
  };

  static QDialog* nameDialog = nullptr;
  static QLineEdit* lineEdit = nullptr;
  if (!nameDialog) nameDialog = initDialog(&lineEdit);

  // ask name
  QString oldName;
  QString groupName = QString("%1x%2")
                          .arg(m_curResolution.width())
                          .arg(m_curResolution.height());
  m_settings->beginGroup(groupName);
  for (auto key : m_settings->childKeys()) {
    QRect rect;
    double dpi;
    if (!strToSubCamera(m_settings->value(key).toString(), rect, dpi)) continue;
    if (m_curSubCamera == rect && m_currentDpi == dpi) {
      oldName = key;
    }
  }
  lineEdit->setText(oldName);
  lineEdit->selectAll();

  if (nameDialog->exec() == QDialog::Rejected) {
    m_settings->endGroup();
    return;
  }

  QString newName = lineEdit->text();

  if (newName.isEmpty()) {
    m_settings->endGroup();
    return;
  }

  // ask if there are the same name / data in the existing entries
  if (m_settings->contains(newName) || !oldName.isEmpty()) {
    QString txt =
        tr("Overwriting the existing subcamera preset. Are you sure?");
    if (QMessageBox::Yes != QMessageBox::question(this, tr("Question"), txt))
      return;
    if (!oldName.isEmpty()) m_settings->remove(oldName);
  }

  // register
  m_settings->setValue(newName, subCameraToStr(m_curSubCamera, m_currentDpi));
  m_settings->endGroup();
}

void SubCameraButton::onDeletePreset() {
  QString key       = qobject_cast<QAction*>(sender())->data().toString();
  QString groupName = QString("%1x%2")
                          .arg(m_curResolution.width())
                          .arg(m_curResolution.height());
  m_settings->beginGroup(groupName);
  if (!m_settings->contains(key)) {
    m_settings->endGroup();
    return;
  }

  QString txt = tr("Deleting the subcamera preset %1. Are you sure?").arg(key);
  if (QMessageBox::Yes != QMessageBox::question(this, tr("Question"), txt)) {
    m_settings->endGroup();
    return;
  }

  m_settings->remove(key);
  m_settings->endGroup();
}

//=============================================================================

PencilTestPopup::PencilTestPopup()
    // set the parent 0 in order to enable the popup behind the main window
    : Dialog(0, false, false, "PencilTest")
    , m_currentCamera(NULL)
    , m_captureWhiteBGCue(false)
    , m_captureCue(false)
    , m_useMjpg(CamCapUseMjpg != 0)
#ifdef _WIN32
    , m_useDirectShow(CamCapUseDirectShow != 0)
#endif
    , m_doAutoDpi(CamCapDoAutoDpi != 0)
    , m_customDpi(CamCapCustomDpi) {
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

  QGroupBox* imageFrame        = new QGroupBox(tr("Image adjust"), this);
  m_colorTypeCombo             = new QComboBox(this);
  m_saveImgAdjustDefaultButton = new QPushButton(this);

  m_camCapLevelControl = new CameraCaptureLevelControl(this);
  m_upsideDownCB       = new QCheckBox(tr("Upside down"), this);

  m_bgReductionFld       = new IntField(this);
  m_captureWhiteBGButton = new QPushButton(tr("Capture white BG"), this);

  m_onionSkinGBox   = new QGroupBox(tr("Onion skin"), this);
  m_loadImageButton = new QPushButton(tr("Load Selected Image"), this);
  m_onionOpacityFld = new IntField(this);

  m_timerGBox        = new QGroupBox(tr("Interval timer"), this);
  m_timerIntervalFld = new IntField(this);
  m_captureTimer     = new QTimer(this);
  m_countdownTimer   = new QTimer(this);
  m_timer            = new QTimer(this);

  m_captureButton          = new QPushButton(tr("Capture\n[Return key]"), this);
  QPushButton* closeButton = new QPushButton(tr("Close"), this);

#ifdef WIN32
  m_captureFilterSettingsBtn = new QPushButton(this);
#else
  m_captureFilterSettingsBtn = nullptr;
#endif

  QPushButton* subfolderButton = new QPushButton(tr("Subfolder"), this);

  // subcamera
  m_subcameraButton     = new SubCameraButton(tr("Subcamera"), this);
  m_subWidthFld         = new IntLineEdit(this);
  m_subHeightFld        = new IntLineEdit(this);
  m_subXPosFld          = new IntLineEdit(this);
  m_subYPosFld          = new IntLineEdit(this);
  QWidget* subCamWidget = new QWidget(this);

  // Calibration
  m_calibration.groupBox  = new QGroupBox(tr("Calibration"), this);
  m_calibration.capBtn    = new QPushButton(tr("Capture"), this);
  m_calibration.cancelBtn = new QPushButton(tr("Cancel"), this);
  m_calibration.newBtn    = new QPushButton(tr("Start calibration"), this);
  m_calibration.loadBtn   = new QPushButton(tr("Load"), this);
  m_calibration.exportBtn = new QPushButton(tr("Export"), this);
  m_calibration.label     = new QLabel(this);

  // dpi settings
  m_dpiBtn        = new QPushButton(tr("DPI:Auto"), this);
  m_dpiMenuWidget = createDpiMenuWidget();
  //----

  m_resolutionCombo->setMaximumWidth(fontMetrics().width("0000 x 0000") + 25);
  m_fileTypeCombo->addItems({"jpg", "png", "tga", "tif"});
  m_fileTypeCombo->setCurrentText(QString::fromStdString(CamCapFileType));

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
  m_saveImgAdjustDefaultButton->setFixedSize(24, 24);
  m_saveImgAdjustDefaultButton->setIconSize(QSize(16, 16));
  m_saveImgAdjustDefaultButton->setIcon(createQIcon("gear_check"));
  m_saveImgAdjustDefaultButton->setToolTip(
      tr("Save Current Image Adjust Parameters As Default"));
  m_upsideDownCB->setChecked(false);

  m_bgReductionFld->setRange(0, 100);
  m_bgReductionFld->setValue(0);
  m_bgReductionFld->setDisabled(true);

  m_onionSkinGBox->setObjectName("CleanupSettingsFrame");
  m_onionSkinGBox->setCheckable(true);
  m_onionSkinGBox->setChecked(false);
  m_onionOpacityFld->setRange(1, 100);
  m_onionOpacityFld->setValue(50);
  m_onionOpacityFld->setDisabled(true);

  m_timerGBox->setObjectName("CleanupSettingsFrame");
  m_timerGBox->setCheckable(true);
  m_timerGBox->setChecked(false);
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
    m_captureFilterSettingsBtn->setToolTip(tr("Options"));
    m_captureFilterSettingsBtn->setMenu(createOptionsMenu());
  }

  subfolderButton->setObjectName("SubfolderButton");
  subfolderButton->setIconSize(QSize(16, 16));
  subfolderButton->setIcon(createQIcon("folder_new"));
  m_saveInFileFld->setMaximumWidth(380);

  m_saveInFolderPopup->hide();

  m_subcameraButton->setChecked(false);
  m_subcameraButton->setEnabled(false);
  subCamWidget->setHidden(true);

  // Calibration
  m_calibration.groupBox->setCheckable(true);
  m_calibration.groupBox->setChecked(CamCapDoCalibration);
  QAction* calibrationHelp =
      new QAction(tr("Open Readme.txt for Camera calibration..."));
  m_calibration.groupBox->addAction(calibrationHelp);
  m_calibration.groupBox->setContextMenuPolicy(Qt::ActionsContextMenu);
  m_calibration.capBtn->hide();
  m_calibration.cancelBtn->hide();
  m_calibration.label->hide();
  m_calibration.exportBtn->setEnabled(false);

  int subCameraFieldWidth = fontMetrics().width("00000") + 5;
  m_subWidthFld->setFixedWidth(subCameraFieldWidth);
  m_subHeightFld->setFixedWidth(subCameraFieldWidth);
  m_subXPosFld->setFixedWidth(subCameraFieldWidth);
  m_subYPosFld->setFixedWidth(subCameraFieldWidth);

  m_dpiBtn->setObjectName("SubcameraButton");

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
        subCamLay->addWidget(new IconView("edit_scale", tr("Size"), this), 0);
        subCamLay->addWidget(m_subWidthFld, 0);
        subCamLay->addWidget(new QLabel("x", this), 0);
        subCamLay->addWidget(m_subHeightFld, 0);

        subCamLay->addSpacing(3);
        subCamLay->addWidget(
            new IconView("edit_position", tr("Position"), this), 0);
        subCamLay->addWidget(new QLabel("X:", this), 0);
        subCamLay->addWidget(m_subXPosFld, 0);
        subCamLay->addWidget(new QLabel("Y:", this), 0);
        subCamLay->addWidget(m_subYPosFld, 0);

        subCamLay->addStretch(0);
      }
      subCamWidget->setLayout(subCamLay);
      camLay->addWidget(subCamWidget, 0);

      camLay->addSpacing(10);
      camLay->addWidget(m_dpiBtn, 0);

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
          imageLay->addWidget(m_saveImgAdjustDefaultButton, 0, 2,
                              Qt::AlignRight);

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

        // Calibration
        QGridLayout* calibLay = new QGridLayout();
        calibLay->setMargin(8);
        calibLay->setHorizontalSpacing(3);
        calibLay->setVerticalSpacing(5);
        {
          calibLay->addWidget(m_calibration.newBtn, 0, 0);
          calibLay->addWidget(m_calibration.loadBtn, 0, 1);
          calibLay->addWidget(m_calibration.exportBtn, 0, 2);
          QHBoxLayout* lay = new QHBoxLayout();
          lay->setMargin(0);
          lay->setSpacing(5);
          lay->addWidget(m_calibration.capBtn, 1);
          lay->addWidget(m_calibration.label, 0);
          lay->addWidget(m_calibration.cancelBtn, 1);
          calibLay->addLayout(lay, 1, 0, 1, 3);
        }
        calibLay->setColumnStretch(0, 1);
        m_calibration.groupBox->setLayout(calibLay);
        rightLay->addWidget(m_calibration.groupBox, 0);

        QGridLayout* onionSkinLay = new QGridLayout();
        onionSkinLay->setMargin(8);
        onionSkinLay->setHorizontalSpacing(3);
        onionSkinLay->setVerticalSpacing(5);
        {
          onionSkinLay->addWidget(new QLabel(tr("Opacity(%):"), this), 0, 0,
                                  Qt::AlignRight);
          onionSkinLay->addWidget(m_onionOpacityFld, 0, 1);
          onionSkinLay->addWidget(m_loadImageButton, 1, 0, 1, 2);
        }
        onionSkinLay->setColumnStretch(0, 0);
        onionSkinLay->setColumnStretch(1, 1);
        m_onionSkinGBox->setLayout(onionSkinLay);
        rightLay->addWidget(m_onionSkinGBox);

        QHBoxLayout* timerLay = new QHBoxLayout();
        timerLay->setMargin(8);
        timerLay->setSpacing(3);
        {
          timerLay->addWidget(new QLabel(tr("Interval(sec):"), this), 0,
                              Qt::AlignRight);
          timerLay->addWidget(m_timerIntervalFld, 1);
        }
        m_timerGBox->setLayout(timerLay);
        rightLay->addWidget(m_timerGBox);

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
  ret      = ret && connect(m_resolutionCombo, SIGNAL(activated(int)), this,
                            SLOT(onResolutionComboActivated()));
  ret      = ret && connect(m_fileFormatOptionButton, SIGNAL(pressed()), this,
                            SLOT(onFileFormatOptionButtonPressed()));
  ret      = ret && connect(m_levelNameEdit, SIGNAL(levelNameEdited()), this,
                            SLOT(onLevelNameEdited()));
  ret      = ret &&
        connect(nextLevelButton, SIGNAL(pressed()), this, SLOT(onNextName()));
  ret = ret && connect(m_previousLevelButton, SIGNAL(pressed()), this,
                       SLOT(onPreviousName()));
  ret = ret && connect(m_colorTypeCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onColorTypeComboChanged(int)));
  ret = ret && connect(m_saveImgAdjustDefaultButton, SIGNAL(pressed()), this,
                       SLOT(saveImageAdjustDefault()));
  ret = ret && connect(m_captureWhiteBGButton, SIGNAL(pressed()), this,
                       SLOT(onCaptureWhiteBGButtonPressed()));
  ret = ret && connect(m_onionSkinGBox, SIGNAL(toggled(bool)), this,
                       SLOT(onOnionCBToggled(bool)));
  ret = ret && connect(m_loadImageButton, SIGNAL(pressed()), this,
                       SLOT(onLoadImageButtonPressed()));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onOnionOpacityFldEdited()));
  ret = ret && connect(m_upsideDownCB, SIGNAL(toggled(bool)), m_videoWidget,
                       SLOT(onUpsideDownChecked(bool)));
  ret = ret && connect(m_timerGBox, SIGNAL(toggled(bool)), this,
                       SLOT(onTimerCBToggled(bool)));
  ret = ret && connect(m_captureTimer, SIGNAL(timeout()), this,
                       SLOT(onCaptureTimerTimeout()));
  ret = ret &&
        connect(m_countdownTimer, SIGNAL(timeout()), this, SLOT(onCountDown()));

  ret = ret && connect(closeButton, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret && connect(m_captureButton, SIGNAL(clicked(bool)), this,
                       SLOT(onCaptureButtonClicked(bool)));
  ret = ret && connect(subfolderButton, SIGNAL(clicked(bool)), this,
                       SLOT(openSaveInFolderPopup()));
  ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onSaveInPathEdited()));
  ret = ret &&
        connect(m_fileTypeCombo, QOverload<int>::of(&QComboBox::activated),
                [&](int index) {
                  CamCapFileType = m_fileTypeCombo->currentText().toStdString();
                  refreshFrameInfo();
                });
  ret = ret && connect(m_frameNumberEdit, SIGNAL(editingFinished()), this,
                       SLOT(refreshFrameInfo()));

  // sub camera
  ret = ret && connect(m_subcameraButton, SIGNAL(toggled(bool)), this,
                       SLOT(onSubCameraToggled(bool)));
  ret = ret && connect(m_subcameraButton, SIGNAL(toggled(bool)), subCamWidget,
                       SLOT(setVisible(bool)));
  ret =
      ret &&
      connect(m_subcameraButton,
              SIGNAL(subCameraPresetSelected(const QRect&, const double)), this,
              SLOT(onSubCameraPresetSelected(const QRect&, const double)));
  ret = ret && connect(m_subWidthFld, SIGNAL(editingFinished()), this,
                       SLOT(onSubCameraRectEdited()));
  ret = ret && connect(m_subHeightFld, SIGNAL(editingFinished()), this,
                       SLOT(onSubCameraRectEdited()));
  ret = ret && connect(m_subXPosFld, SIGNAL(editingFinished()), this,
                       SLOT(onSubCameraRectEdited()));
  ret = ret && connect(m_subYPosFld, SIGNAL(editingFinished()), this,
                       SLOT(onSubCameraRectEdited()));
  ret = ret && connect(m_videoWidget, SIGNAL(subCameraChanged(bool)), this,
                       SLOT(onSubCameraChanged(bool)));

  ret = ret && connect(m_timer, SIGNAL(timeout()), this, SLOT(onTimeout()));

  // Calibration
  ret = ret &&
        connect(m_calibration.groupBox, &QGroupBox::toggled, [&](bool checked) {
          CamCapDoCalibration = checked;
          resetCalibSettingsFromFile();
        });
  // ret = ret && connect(m_calibration.groupBox, SIGNAL(toggled(bool)), this,
  // SLOT(resetCalibSettingsFromFile()));
  ret = ret && connect(m_calibration.capBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibCapBtnClicked()));
  ret = ret && connect(m_calibration.newBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibNewBtnClicked()));
  ret = ret && connect(m_calibration.cancelBtn, SIGNAL(clicked()), this,
                       SLOT(resetCalibSettingsFromFile()));
  ret = ret && connect(m_calibration.loadBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibLoadBtnClicked()));
  ret = ret && connect(m_calibration.exportBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibExportBtnClicked()));
  ret = ret && connect(calibrationHelp, SIGNAL(triggered()), this,
                       SLOT(onCalibReadme()));

  ret =
      ret && connect(m_dpiBtn, &QPushButton::clicked, [&](bool) {
        m_dpiMenuWidget->move(m_dpiBtn->mapToGlobal(m_dpiBtn->rect().center()));
        m_dpiMenuWidget->show();
        m_dpiMenuWidget->updateGeometry();
      });

  assert(ret);

  refreshCameraList();

  int startupCamIndex = m_cameraListCombo->findText(
      QString::fromStdString(CamCapCameraName.getValue()));
  // if previous camera is not found, then try to activate the connected default
  // camera
  if (startupCamIndex <= 0 && !QCameraInfo::defaultCamera().isNull()) {
    startupCamIndex =
        m_cameraListCombo->findText(QCameraInfo::defaultCamera().description());
  }
  if (startupCamIndex > 0) {
    m_cameraListCombo->setCurrentIndex(startupCamIndex);
    onCameraListComboActivated(startupCamIndex);
  }
  // just in case, try to activate any connected camera
  else if (m_cameraListCombo->count() >= 2) {
    m_cameraListCombo->setCurrentIndex(1);
    onCameraListComboActivated(1);
  }

  QString resStr = QString::fromStdString(CamCapCameraResolution.getValue());
  if (m_currentCamera && !resStr.isEmpty()) {
    int startupResolutionIndex = m_resolutionCombo->findText(resStr);
    if (startupResolutionIndex >= 0) {
      m_resolutionCombo->setCurrentIndex(startupResolutionIndex);
      onResolutionComboActivated();

      // if the saved resolution was reproduced, then reproduce the subcamera
      TRect subCamRect = CamCapSubCameraRect;
      if (!subCamRect.isEmpty()) {
        m_subWidthFld->setValue(subCamRect.getLx());
        m_subHeightFld->setValue(subCamRect.getLy());
        m_subXPosFld->setValue(subCamRect.x0);
        m_subYPosFld->setValue(subCamRect.y0);
        // need to store the dummy image before starting the camera
        QImage dummyImg(m_resolution, QImage::Format_RGB32);
        dummyImg.fill(Qt::transparent);
        m_videoWidget->setImage(dummyImg);
        m_subcameraButton->setChecked(true);
      }

      // try loading the default parameter
      loadImageAdjustDefault();
    }
  }

  setToNextNewLevel();
}

//-----------------------------------------------------------------------------

PencilTestPopup::~PencilTestPopup() { m_cvWebcam.release(); }

//-----------------------------------------------------------------------------

QMenu* PencilTestPopup::createOptionsMenu() {
  QMenu* menu = new QMenu();
  bool ret    = true;
#ifdef _WIN32
  QAction* settingsAct =
      menu->addAction(tr("Video Capture Filter Settings..."));
  ret = ret && connect(settingsAct, SIGNAL(triggered()), this,
                       SLOT(onCaptureFilterSettingsBtnPressed()));
  settingsAct->setIcon(QIcon(":Resources/preferences.svg"));

  menu->addSeparator();

  QAction* useDShowAct = menu->addAction(tr("Use Direct Show Webcam Drivers"));
  useDShowAct->setCheckable(true);
  useDShowAct->setChecked(m_useDirectShow);
  ret = ret && connect(useDShowAct, &QAction::toggled, [&](bool checked) {
          m_cvWebcam.release();
          if (m_timer->isActive()) m_timer->stop();
          m_useDirectShow     = checked;
          CamCapUseDirectShow = checked;
          m_timer->start(40);
        });
#endif
  QAction* useMjpgAct = menu->addAction(tr("Use MJPG with Webcam"));
  useMjpgAct->setCheckable(true);
  useMjpgAct->setChecked(m_useMjpg);
  ret = ret && connect(useMjpgAct, &QAction::toggled, [&](bool checked) {
          m_cvWebcam.release();
          if (m_timer->isActive()) m_timer->stop();
          m_useMjpg     = checked;
          CamCapUseMjpg = checked;
          m_timer->start(40);
        });

  return menu;
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

  m_cvWebcam.release();
  if (m_timer->isActive()) m_timer->stop();

  // if selected the non-connected state, then disconnect the current camera
  if (comboIndex == 0) {
    m_deviceName = QString();
    m_videoWidget->setImage(QImage());
    // update env
    CamCapCameraName = "";
    m_subcameraButton->setEnabled(false);
    return;
  }

  int index = comboIndex - 1;
  // in case the camera is not changed (just click the combobox)
  if (cameras.at(index).deviceName() == m_deviceName) return;

  m_currentCamera = new QCamera(cameras.at(index), this);
  m_deviceName    = cameras.at(index).deviceName();

  // loading new camera
  m_currentCamera->load();

  // refresh resolution
  m_resolutionCombo->clear();
  QList<QSize> sizes = m_currentCamera->supportedViewfinderResolutions();

  m_currentCamera->unload();
  for (const QSize size : sizes) {
    m_resolutionCombo->addItem(
        QString("%1 x %2").arg(size.width()).arg(size.height()), size);
  }
  if (!sizes.isEmpty()) {
    // select the largest available resolution
    m_resolutionCombo->setCurrentIndex(m_resolutionCombo->count() - 1);
  }

  m_videoWidget->setImage(QImage());
  m_timer->start(40);
  // update env
  CamCapCameraName = m_cameraListCombo->itemText(comboIndex).toStdString();
  m_subcameraButton->setEnabled(true);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onResolutionComboActivated() {
  m_cvWebcam.release();
  if (m_timer->isActive()) m_timer->stop();

  QSize newResolution = m_resolutionCombo->currentData().toSize();

  if (!newResolution.isValid()) return;
  if (newResolution == m_resolution) {
    m_timer->start(40);
    return;
  }

  m_resolution = newResolution;

  // reset white bg
  m_whiteBGImg = cv::Mat();
  m_bgReductionFld->setDisabled(true);
  m_videoWidget->setImage(QImage());
  m_videoWidget->computeTransform(m_resolution);

  // update env
  CamCapCameraResolution = m_resolutionCombo->currentText().toStdString();

  refreshFrameInfo();

  // reset subcamera info
  m_subcameraButton->setChecked(false);  // this will hide the size fields
  m_subcameraButton->setCurResolution(m_resolution);
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
    m_subWidthFld->setValue(newResolution.width());
    m_subHeightFld->setValue(newResolution.height());
  } else {
    m_subWidthFld->setValue(m_allowedCameraSize.width());
    m_subHeightFld->setValue(m_allowedCameraSize.height());
  }

  m_calibration.isValid = false;
  m_calibration.exportBtn->setEnabled(false);
  resetCalibSettingsFromFile();

  m_timer->start(40);
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

void PencilTestPopup::onTimeout() { getWebcamImage(); }

//-----------------------------------------------------------------------------

int PencilTestPopup::translateIndex(int camIndex) {
#ifdef WIN32
  // We are using Qt to get the camera info and supported resolutions, but
  // we are using OpenCV to actually get the images.
  // The camera index from OpenCV and from Qt don't always agree,
  // So this checks the name against the correct index.

  // Thanks to:
  // https://elcharolin.wordpress.com/2017/08/28/webcam-capture-with-the-media-foundation-sdk/
  // for the webcam enumeration here

  std::wstring desc = m_cameraListCombo->currentText().toStdWString();

#define CLEAN_ATTRIBUTES()                                                     \
  if (attributes) {                                                            \
    attributes->Release();                                                     \
    attributes = NULL;                                                         \
  }                                                                            \
  for (DWORD i = 0; i < count; i++) {                                          \
    if (&devices[i]) {                                                         \
      devices[i]->Release();                                                   \
      devices[i] = NULL;                                                       \
    }                                                                          \
  }                                                                            \
  CoTaskMemFree(devices);                                                      \
  return camIndex;

  HRESULT hr = S_OK;

  // this is important!!
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  UINT32 count              = 0;
  IMFAttributes* attributes = NULL;
  IMFActivate** devices     = NULL;

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Create an attribute store to specify enumeration parameters.
  hr = MFCreateAttributes(&attributes, 1);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }

  // The attribute to be requested is devices that can capture video
  hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Enummerate the video capture devices
  hr = MFEnumDeviceSources(attributes, &devices, &count);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // if there are any available devices
  if (count > 0) {
    WCHAR* nameString = NULL;
    // Get the human-friendly name of the device
    UINT32 cchName;

    for (int i = 0; i < count; i++) {
      hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                          &nameString, &cchName);
      if (nameString == desc) {
        return i;
      }
      // devices[0]->ShutdownObject();
    }

    CoTaskMemFree(nameString);
  }
  // clean
  CLEAN_ATTRIBUTES()
#endif  // WIN32
  return camIndex;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::getWebcamImage() {
  bool error = false;
  cv::Mat imgOriginal;
  cv::Mat imgCorrected;

  if (m_cvWebcam.isOpened() == false) {
    if (m_cameraListCombo->currentIndex() <= 0) return;
    int camIndex = m_cameraListCombo->currentIndex() - 1;
#ifdef WIN32
    if (!m_useDirectShow) {
      // the webcam order obtained from Qt isn't always the same order as
      // the one obtained from OpenCV without DirectShow
      m_cvWebcam.open(translateIndex(camIndex));
    } else {
      m_cvWebcam.open(camIndex, cv::CAP_DSHOW);
    }
#else
    m_cvWebcam.open(translateIndex(camIndex));
#endif
    // mjpg is used by many webcams
    // opencv runs very slow on some webcams without it.
    if (m_useMjpg) {
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('m', 'j', 'p', 'g'));
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }
    m_cvWebcam.set(cv::CAP_PROP_FRAME_WIDTH, m_resolution.width());
    m_cvWebcam.set(cv::CAP_PROP_FRAME_HEIGHT, m_resolution.height());
    if (!m_cvWebcam.isOpened()) error = true;
  }

  bool blnFrameReadSuccessfully =
      m_cvWebcam.read(imgOriginal);  // get next frame

  if (!blnFrameReadSuccessfully ||
      imgOriginal.empty()) {  // if frame not read successfully
    std::cout << "error: frame not read from webcam\n";
    error = true;  // print error message to std out
  }

  if (!error) {
    cv::cvtColor(imgOriginal, imgCorrected, cv::COLOR_BGR2RGB);
    onFrameCaptured(imgCorrected);
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onFrameCaptured(cv::Mat& image) {
  if (!m_videoWidget) return;
  // capture the white BG
  if (m_captureWhiteBGCue) {
    ;
    m_whiteBGImg        = image.clone();
    m_captureWhiteBGCue = false;
    m_bgReductionFld->setEnabled(true);
  }

  // capture calibration reference
  if (m_calibration.captureCue) {
    m_calibration.captureCue = false;
    captureCalibrationRefImage(image);
    return;
  }

  processImage(image);

  QImage::Format format = (m_colorTypeCombo->currentIndex() == 0)
                              ? QImage::Format_RGB888
                              : QImage::Format_Grayscale8;
  QImage qimg(image.data, image.cols, image.rows, format);
  m_videoWidget->setImage(qimg.copy());

  if (m_captureCue) {
    m_captureCue = false;

    if (importImage(qimg)) {
      m_videoWidget->setPreviousImage(qimg.copy());
      if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
        TFrameId fId = m_frameNumberEdit->getValue();
        int f        = fId.getNumber();
        if (f % 10 == 0)  // next number
          m_frameNumberEdit->setValue(TFrameId(((int)(f / 10) + 1) * 10));
        else  // next alphabet
          m_frameNumberEdit->setValue(TFrameId(f + 1));
      } else {
        TFrameId fId = m_frameNumberEdit->getValue();

        if (fId.getLetter().isEmpty() || fId.getLetter() == "Z" ||
            fId.getLetter() == "z")  // next number
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
      if (m_timerGBox->isChecked() && m_captureButton->isChecked()) {
        m_captureTimer->start(m_timerIntervalFld->getValue() * 1000);
        // restart the count down as well (for aligning the timing. It is not
        // single shot)
        if (m_timerIntervalFld->getValue() != 0) m_countdownTimer->start(100);
      }
    }
    // if capture was failed, stop interval capturing
    else if (m_timerGBox->isChecked()) {
      m_captureButton->setChecked(false);
      onCaptureButtonClicked(false);
    }
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::captureCalibrationRefImage(cv::Mat& image) {
  cv::cvtColor(image, image, cv::COLOR_RGB2GRAY);
  std::vector<cv::Point2f> corners;
  bool found = cv::findChessboardCorners(
      image, m_calibration.boardSize, corners,
      cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FILTER_QUADS);
  if (found) {
    // compute corners in detail
    cv::cornerSubPix(
        image, corners, cv::Size(11, 11), cv::Size(-1, -1),
        cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30,
                         0.1));
    // count up
    m_calibration.refCaptured++;
    // register corners
    m_calibration.image_points.push_back(corners);
    // register 3d points in real world space
    std::vector<cv::Point3f> obj;
    for (int i = 0;
         i < m_calibration.boardSize.width * m_calibration.boardSize.height;
         i++)
      obj.push_back(cv::Point3f(i / m_calibration.boardSize.width,
                                i % m_calibration.boardSize.width, 0.0f));
    m_calibration.obj_points.push_back(obj);

    // needs 10 references
    if (m_calibration.refCaptured < 10) {
      // update label
      m_calibration.label->setText(
          QString("%1/%2").arg(m_calibration.refCaptured).arg(10));
    } else {
      // swap UIs
      m_calibration.label->hide();
      m_calibration.capBtn->hide();
      m_calibration.cancelBtn->hide();
      m_calibration.newBtn->show();
      m_calibration.loadBtn->show();
      m_calibration.exportBtn->show();

      cv::Mat intrinsic          = cv::Mat(3, 3, CV_32FC1);
      intrinsic.ptr<float>(0)[0] = 1.f;
      intrinsic.ptr<float>(1)[1] = 1.f;
      cv::Mat distCoeffs;
      std::vector<cv::Mat> rvecs;
      std::vector<cv::Mat> tvecs;
      cv::calibrateCamera(m_calibration.obj_points, m_calibration.image_points,
                          image.size(), intrinsic, distCoeffs, rvecs, tvecs);

      cv::Mat mapR          = cv::Mat::eye(3, 3, CV_64F);
      cv::Mat new_intrinsic = cv::getOptimalNewCameraMatrix(
          intrinsic, distCoeffs, image.size(),
          0.0);  // setting the last argument to 1.0 will include all source
                 // pixels in the frame
      cv::initUndistortRectifyMap(intrinsic, distCoeffs, mapR, new_intrinsic,
                                  image.size(), CV_32FC1, m_calibration.mapX,
                                  m_calibration.mapY);

      // save calibration settings
      QString calibFp = getCurrentCalibFilePath();
      cv::FileStorage fs(calibFp.toStdString(), cv::FileStorage::WRITE);
      if (!fs.isOpened()) {
        DVGui::warning(tr("Failed to save calibration settings."));
        return;
      }
      fs << "identifier"
         << "OpenToonzCameraCalibrationSettings";
      fs << "resolution"
         << cv::Size(m_resolution.width(), m_resolution.height());
      fs << "instrinsic" << intrinsic;
      fs << "distCoeffs" << distCoeffs;
      fs << "new_intrinsic" << new_intrinsic;
      fs.release();

      m_calibration.isValid = true;
      m_calibration.exportBtn->setEnabled(true);
    }
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::showEvent(QShowEvent* event) {
  // if there is another action of which "return" key is assigned as short cut
  // key,
  // then release the shortcut key temporary while the popup opens
  QAction* action = CommandManager::instance()->getActionFromShortcut("Return");
  if (action) action->setShortcut(QKeySequence(""));

  TSceneHandle* sceneHandle = TApp::instance()->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(onSceneSwitched()));
  connect(sceneHandle, SIGNAL(castChanged()), this, SLOT(refreshFrameInfo()));
  connect(sceneHandle, SIGNAL(preferenceChanged(const QString&)), this,
          SLOT(onPreferenceChanged(const QString&)));

  bool tmp_alwaysOverwrite = m_alwaysOverwrite;
  onSceneSwitched();
  m_alwaysOverwrite = tmp_alwaysOverwrite;

  onResolutionComboActivated();
  m_videoWidget->computeTransform(m_resolution);

  m_dpiBtn->setHidden(Preferences::instance()->getPixelsOnly());
}

//-----------------------------------------------------------------------------

void PencilTestPopup::hideEvent(QHideEvent* event) {
  // set back the "return" short cut key
  QAction* action = CommandManager::instance()->getActionFromShortcut("Return");
  if (action) action->setShortcut(QKeySequence("Return"));

  // stop interval timer if it is active
  if (m_timerGBox->isChecked() && m_captureButton->isChecked()) {
    m_captureButton->setChecked(false);
    onCaptureButtonClicked(false);
  }

  m_cvWebcam.release();
  if (m_timer->isActive()) m_timer->stop();

  Dialog::hideEvent(event);

  TSceneHandle* sceneHandle = TApp::instance()->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
             SLOT(refreshFrameInfo()));
  disconnect(sceneHandle, SIGNAL(castChanged()), this,
             SLOT(refreshFrameInfo()));
  disconnect(sceneHandle, SIGNAL(preferenceChanged(const QString&)), this,
             SLOT(onPreferenceChanged(const QString&)));

  // save the current subcamera to env
  if (m_subcameraButton->isChecked()) {
    CamCapSubCameraRect = TRect(
        TPoint(m_subXPosFld->getValue(), m_subYPosFld->getValue()),
        TDimension(m_subWidthFld->getValue(), m_subHeightFld->getValue()));
  } else
    CamCapSubCameraRect = TRect();
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

void PencilTestPopup::processImage(cv::Mat& image) {
  // perform calibration
  if (m_calibration.groupBox->isChecked() && m_calibration.isValid) {
    cv::remap(image, image, m_calibration.mapX, m_calibration.mapY,
              cv::INTER_LINEAR);
  }

  // white bg reduction
  if (!m_whiteBGImg.empty() && m_bgReductionFld->getValue() != 0)
    bgReduction(image, m_whiteBGImg, m_bgReductionFld->getValue());
  //  void PencilTestPopup::processImage(QImage& image) {
  if (m_upsideDownCB->isChecked())
    cv::flip(image, image, -1);  // flip in both directions

  // obtain histogram AFTER bg reduction
  m_camCapLevelControl->updateHistogram(image);

  // change channel
  if (m_colorTypeCombo->currentIndex() != 0)
    cv::cvtColor(image, image, cv::COLOR_RGB2GRAY);

  // color and grayscale mode
  if (m_colorTypeCombo->currentIndex() != 2)
    m_camCapLevelControl->adjustLevel(image);
  else
    m_camCapLevelControl->binarize(image);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureWhiteBGButtonPressed() {
  m_captureWhiteBGCue = true;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onOnionCBToggled(bool on) {
  m_videoWidget->setShowOnionSkin(on);
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
    m_onionSkinGBox->setChecked(true);
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
  m_captureButton->setCheckable(on);
  if (on)
    m_captureButton->setText(tr("Start Capturing\n[Return key]"));
  else
    m_captureButton->setText(tr("Capture\n[Return key]"));
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureButtonClicked(bool on) {
  if (m_timerGBox->isChecked()) {
    m_timerGBox->setDisabled(on);
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

      // if the pixel unit is used, always apply the current camera dpi
      if (Preferences::instance()->getPixelsOnly()) {
        dpi = getCurrentCameraDpi();
      } else if (m_doAutoDpi) {
        // if the subcamera is not active, apply the current camera dpi
        if (!m_subcameraButton->isChecked() ||
            !m_videoWidget->subCameraRect().isValid())
          dpi = getCurrentCameraDpi();
        // if the subcamera is active, compute the dpi so that the image will
        // fit to the camera frame
        else {
          TCamera* camera = TApp::instance()
                                ->getCurrentScene()
                                ->getScene()
                                ->getCurrentCamera();
          TDimensionD size = camera->getSize();
          double minimumDpi =
              std::min(image.width() / size.lx, image.height() / size.ly);
          dpi = TPointD(minimumDpi, minimumDpi);
        }
      } else {  // apply the custom dpi value
        dpi = TPointD(m_customDpi, m_customDpi);
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

  TFrameId tmplFId = currentScene->getProperties()->formatTemplateFIdForInput();

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

void PencilTestPopup::onFileTypeChanged() {
  CamCapFileType = m_fileTypeCombo->currentText().toStdString();
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
  QRect subCamera =
      on ? QRect(m_subXPosFld->getValue(), m_subYPosFld->getValue(),
                 m_subWidthFld->getValue(), m_subHeightFld->getValue())
         : QRect();
  m_videoWidget->setSubCameraRect(subCamera);
  m_subcameraButton->setCurSubCamera(subCamera);
  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSubCameraChanged(bool isDragging) {
  QRect subCameraRect = m_videoWidget->subCameraRect();
  assert(subCameraRect.isValid());
  m_subWidthFld->setValue(subCameraRect.width());
  m_subHeightFld->setValue(subCameraRect.height());
  m_subXPosFld->setValue(subCameraRect.x());
  m_subYPosFld->setValue(subCameraRect.y());

  if (!isDragging) {
    refreshFrameInfo();
    m_subcameraButton->setCurSubCamera(subCameraRect);
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSubCameraRectEdited() {
  m_videoWidget->setSubCameraRect(
      QRect(m_subXPosFld->getValue(), m_subYPosFld->getValue(),
            m_subWidthFld->getValue(), m_subHeightFld->getValue()));
  refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onSubCameraPresetSelected(const QRect& subCameraRect,
                                                const double dpi) {
  assert(subCameraRect.isValid());
  m_subWidthFld->setValue(subCameraRect.width());
  m_subHeightFld->setValue(subCameraRect.height());
  m_subXPosFld->setValue(subCameraRect.x());
  m_subYPosFld->setValue(subCameraRect.y());

  if (!m_subcameraButton->isChecked())
    // calling onSubCameraToggled() accordingly
    m_subcameraButton->setChecked(true);
  else
    onSubCameraToggled(true);

  if (m_dpiMenuWidget) {
    m_autoDpiRadioBtn->setChecked(true);
    if (dpi > 0.) {
      m_customDpi     = dpi;
      CamCapCustomDpi = dpi;
      m_customDpiField->setText(QString::number(dpi));
      m_dpiBtn->setText(tr("DPI:%1").arg(QString::number(dpi)));
      m_customDpiRadioBtn->setChecked(true);
    }
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCalibCapBtnClicked() {
  m_calibration.captureCue = true;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCalibNewBtnClicked() {
  if (m_calibration.isValid) {
    QString question = tr("Do you want to restart camera calibration?");
    int ret =
        DVGui::MsgBox(question, QObject::tr("Restart"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }
  // initialize calibration parameter
  m_calibration.captureCue  = false;
  m_calibration.refCaptured = 0;
  m_calibration.obj_points.clear();
  m_calibration.image_points.clear();
  m_calibration.isValid = false;

  // initialize label
  m_calibration.label->setText(
      QString("%1/%2").arg(m_calibration.refCaptured).arg(10));
  // swap UIs
  m_calibration.newBtn->hide();
  m_calibration.loadBtn->hide();
  m_calibration.exportBtn->hide();
  m_calibration.label->show();
  m_calibration.capBtn->show();
  m_calibration.cancelBtn->show();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::resetCalibSettingsFromFile() {
  if (m_calibration.capBtn->isVisible()) {
    // swap UIs
    m_calibration.label->hide();
    m_calibration.capBtn->hide();
    m_calibration.cancelBtn->hide();
    m_calibration.newBtn->show();
    m_calibration.loadBtn->show();
    m_calibration.exportBtn->show();
  }
  if (m_calibration.groupBox->isChecked() && !m_calibration.isValid) {
    QString calibFp = getCurrentCalibFilePath();
    std::cout << calibFp.toStdString() << std::endl;
    if (!calibFp.isEmpty() && QFileInfo(calibFp).exists()) {
      cv::Mat intrinsic, distCoeffs, new_intrinsic;
      cv::FileStorage fs(calibFp.toStdString(), cv::FileStorage::READ);
      if (!fs.isOpened()) return;
      std::string identifierStr;
      fs["identifier"] >> identifierStr;
      if (identifierStr != "OpenToonzCameraCalibrationSettings") return;
      cv::Size resolution;
      fs["resolution"] >> resolution;
      if (m_resolution != QSize(resolution.width, resolution.height)) return;
      fs["instrinsic"] >> intrinsic;
      fs["distCoeffs"] >> distCoeffs;
      fs["new_intrinsic"] >> new_intrinsic;
      fs.release();

      cv::Mat mapR = cv::Mat::eye(3, 3, CV_64F);
      cv::initUndistortRectifyMap(
          intrinsic, distCoeffs, mapR, new_intrinsic,
          cv::Size(m_resolution.width(), m_resolution.height()), CV_32FC1,
          m_calibration.mapX, m_calibration.mapY);

      m_calibration.isValid = true;
      m_calibration.exportBtn->setEnabled(true);
    }
  }
}

//-----------------------------------------------------------------------------

QString PencilTestPopup::getCameraConfigurationPath(const QString& folderName,
                                                    const QString& ext) {
  QString cameraName = m_cameraListCombo->currentText();
  if (cameraName.isEmpty()) return QString();
  QString resolution   = m_resolutionCombo->currentText();
  QString hostName     = QHostInfo::localHostName();
  TFilePath folderPath = ToonzFolder::getLibraryFolder();
  return folderPath.getQString() + "\\" + folderName + "\\" + hostName + "_" +
         cameraName + "_" + resolution + "." + ext;
}

//-----------------------------------------------------------------------------

QString PencilTestPopup::getCurrentCalibFilePath() {
  return getCameraConfigurationPath("camera calibration", "xml");
}

//-----------------------------------------------------------------------------

QString PencilTestPopup::getImageAdjustSettingsPath() {
  return getCameraConfigurationPath("camera image adjust", "ini");
}

//-----------------------------------------------------------------------------

QString PencilTestPopup::getImageAdjustBgImgPath() {
  return getCameraConfigurationPath("camera image adjust", "jpg");
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCalibLoadBtnClicked() {
  LoadCalibrationFilePopup popup(this);

  QString fp = popup.getPath().getQString();
  if (fp.isEmpty()) return;
  try {
    cv::FileStorage fs(fp.toStdString(), cv::FileStorage::READ);
    if (!fs.isOpened())
      throw TException(fp.toStdWString() + L": Can't open file");

    std::string identifierStr;
    fs["identifier"] >> identifierStr;
    if (identifierStr != "OpenToonzCameraCalibrationSettings")
      throw TException(fp.toStdWString() + L": Identifier does not match");
    cv::Size resolution;
    fs["resolution"] >> resolution;
    if (m_resolution != QSize(resolution.width, resolution.height))
      throw TException(fp.toStdWString() + L": Resolution does not match");
  } catch (const TException& se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  } catch (...) {
    DVGui::error(tr("Couldn't load %1").arg(fp));
    return;
  }

  if (m_calibration.isValid) {
    QString question = tr("Overwriting the current calibration. Are you sure?");
    int ret = DVGui::MsgBox(question, QObject::tr("OK"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
    m_calibration.isValid = false;
  }

  TSystem::copyFile(TFilePath(getCurrentCalibFilePath()), TFilePath(fp), true);
  resetCalibSettingsFromFile();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCalibExportBtnClicked() {
  // just in case
  if (!m_calibration.isValid) return;
  if (!QFileInfo(getCurrentCalibFilePath()).exists()) return;

  ExportCalibrationFilePopup popup(this);

  QString fp = popup.getPath().getQString();
  if (fp.isEmpty()) return;

  try {
    {
      QFileInfo fs(fp);
      if (fs.exists() && !fs.isWritable()) {
        throw TSystemException(
            TFilePath(fp),
            L"The file cannot be saved: it is a read only file.");
      }
    }
    TSystem::copyFile(TFilePath(fp), TFilePath(getCurrentCalibFilePath()),
                      true);
  } catch (const TSystemException& se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
  } catch (...) {
    DVGui::error(tr("Couldn't save %1").arg(fp));
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCalibReadme() {
  TFilePath readmeFp =
      ToonzFolder::getLibraryFolder() + "camera calibration" + "readme.txt";
  if (!TFileStatus(readmeFp).doesExist()) return;
  if (TSystem::isUNC(readmeFp))
    QDesktopServices::openUrl(QUrl(readmeFp.getQString()));
  else
    QDesktopServices::openUrl(QUrl::fromLocalFile(readmeFp.getQString()));
}
//-----------------------------------------------------------------------------

QWidget* PencilTestPopup::createDpiMenuWidget() {
  QWidget* widget = new QWidget(this, Qt::Popup);
  widget->hide();

  widget->setToolTip(
      tr("This option specifies DPI for newly created levels.\n"
         "Adding frames to the existing level won't refer to this value.\n"
         "Auto: If the subcamera is not active, apply the current camera dpi.\n"
         "      If the subcamera is active, compute the dpi so that\n"
         "      the image will fit to the camera frame.\n"
         "Custom : Always use the custom dpi specified here."));

  m_autoDpiRadioBtn   = new QRadioButton(tr("Auto"), this);
  m_customDpiRadioBtn = new QRadioButton(tr("Custom"), this);
  // check the opposite option so that the slot will be called when setting the
  // initial state
  if (m_doAutoDpi)
    m_customDpiRadioBtn->setChecked(true);
  else
    m_autoDpiRadioBtn->setChecked(true);
  m_customDpiField = new QLineEdit(this);

  m_customDpiField->setValidator(new QDoubleValidator(1.0, 2000.0, 6));

  QGridLayout* layout = new QGridLayout();
  layout->setMargin(10);
  layout->setHorizontalSpacing(5);
  layout->setVerticalSpacing(10);
  {
    layout->addWidget(m_autoDpiRadioBtn, 0, 0, 1, 2,
                      Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(m_customDpiRadioBtn, 1, 0,
                      Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(m_customDpiField, 1, 1);
  }
  widget->setLayout(layout);

  bool ret = true;
  ret      = ret &&
        connect(m_autoDpiRadioBtn, &QRadioButton::toggled, [&](bool checked) {
          m_doAutoDpi     = checked;
          CamCapDoAutoDpi = checked;
          m_customDpiField->setDisabled(checked);
          if (checked) {
            m_dpiBtn->setText(tr("DPI:Auto"));
            m_subcameraButton->setCurDpi(-1.);
          } else {
            m_dpiBtn->setText(tr("DPI:%1").arg(QString::number(m_customDpi)));
            m_subcameraButton->setCurDpi(m_customDpi);
          }
        });

  ret = ret && connect(m_customDpiField, &QLineEdit::editingFinished, [&]() {
          m_customDpi     = m_customDpiField->text().toDouble();
          CamCapCustomDpi = m_customDpi;
          m_dpiBtn->setText(tr("DPI:%1").arg(QString::number(m_customDpi)));
          m_subcameraButton->setCurDpi(m_customDpi);
          m_dpiMenuWidget->hide();
        });
  assert(ret);

  m_customDpiField->setText(QString::number(m_customDpi));
  if (m_doAutoDpi)
    m_autoDpiRadioBtn->setChecked(true);
  else
    m_customDpiRadioBtn->setChecked(true);
  // m_customDpiField->setDisabled(m_doAutoDpi);

  return widget;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onPreferenceChanged(const QString& prefName) {
  // react only when the related preference is changed
  if (prefName != "pixelsOnly") return;

  m_dpiBtn->setHidden(Preferences::instance()->getPixelsOnly());
}

//-----------------------------------------------------------------------------
// called in ctor
void PencilTestPopup::loadImageAdjustDefault() {
  // find the configuration file
  QString settingsPath = getImageAdjustSettingsPath();
  if (!TFileStatus(TFilePath(settingsPath)).doesExist()) return;

  QSettings imgAdjSettings(settingsPath, QSettings::IniFormat);
  int colorType   = imgAdjSettings.value("ColorType", 0).toInt();
  int black       = imgAdjSettings.value("BlackLevel", 0).toInt();
  int white       = imgAdjSettings.value("WhiteLevel", 255).toInt();
  double gamma    = imgAdjSettings.value("Gamma", 1.0).toDouble();
  int threshold   = imgAdjSettings.value("Threshold", 128).toInt();
  bool upsideDown = imgAdjSettings.value("UpsideDown", false).toBool();
  int bgReduction = imgAdjSettings.value("BgReduction", 0).toInt();

  m_colorTypeCombo->setCurrentIndex(colorType);
  m_camCapLevelControl->setValues(black, white, threshold, gamma,
                                  colorType != 2);
  m_upsideDownCB->setChecked(upsideDown);
  m_bgReductionFld->setValue(bgReduction);

  QString bgPath = getImageAdjustBgImgPath();
  if (!TFileStatus(TFilePath(bgPath)).doesExist()) {
    if (bgReduction > 0)
      warning(
          tr("BG Reduction is set but the White BG image is missing. Please "
             "capture the White BG again."));

    m_whiteBGImg = cv::Mat();
    m_bgReductionFld->setDisabled(true);
    return;
  }

  QImage bgImg(bgPath);
  if (bgImg.isNull()) {
    m_whiteBGImg = cv::Mat();
    m_bgReductionFld->setDisabled(true);
    return;
  }
  cv::Mat tmpMat = QImageToMat(bgImg);
  cv::cvtColor(tmpMat, m_whiteBGImg, cv::COLOR_RGBA2RGB);
  m_bgReductionFld->setEnabled(true);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::saveImageAdjustDefault() {
  QString question =
      tr("Do you want to save the current parameters as the default values?");
  int ret = DVGui::MsgBox(question, QObject::tr("Save"), QObject::tr("Cancel"));
  if (ret == 0 || ret == 2) return;

  QString settingsPath = getImageAdjustSettingsPath();
  QSettings imgAdjSettings(settingsPath, QSettings::IniFormat);

  imgAdjSettings.setValue("ColorType", m_colorTypeCombo->currentIndex());
  int black, white, threshold;
  double gamma;
  m_camCapLevelControl->getValues(black, white, threshold, gamma);
  imgAdjSettings.setValue("BlackLevel", black);
  imgAdjSettings.setValue("WhiteLevel", white);
  imgAdjSettings.setValue("Gamma", gamma);
  imgAdjSettings.setValue("Threshold", threshold);
  imgAdjSettings.setValue("UpsideDown", m_upsideDownCB->isChecked());
  imgAdjSettings.setValue("BgReduction", m_bgReductionFld->getValue());

  if (!m_whiteBGImg.empty()) {
    QImage qimg(m_whiteBGImg.data, m_whiteBGImg.cols, m_whiteBGImg.rows,
                QImage::Format_RGB888);
    qimg.save(getImageAdjustBgImgPath());
  }
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

//=============================================================================

ExportCalibrationFilePopup::ExportCalibrationFilePopup(QWidget* parent)
    : GenericSaveFilePopup(tr("Export Camera Calibration Settings")) {
  Qt::WindowFlags flags = windowFlags();
  setParent(parent);
  setWindowFlags(flags);
  m_browser->enableGlobalSelection(false);
  setFilterTypes(QStringList("xml"));
}

void ExportCalibrationFilePopup::showEvent(QShowEvent* e) {
  FileBrowserPopup::showEvent(e);
  setFolder(ToonzFolder::getLibraryFolder() + "camera calibration");
}

//=============================================================================

LoadCalibrationFilePopup::LoadCalibrationFilePopup(QWidget* parent)
    : GenericLoadFilePopup(tr("Load Camera Calibration Settings")) {
  Qt::WindowFlags flags = windowFlags();
  setParent(parent);
  setWindowFlags(flags);
  m_browser->enableGlobalSelection(false);
  setFilterTypes(QStringList("xml"));
}

void LoadCalibrationFilePopup::showEvent(QShowEvent* e) {
  FileBrowserPopup::showEvent(e);
  setFolder(ToonzFolder::getLibraryFolder() + "camera calibration");
}