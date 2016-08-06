#include "penciltestpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "filebrowsermodel.h"

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
#include "toonz/namebuilder.h"
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

// TnzCore includes
#include "tsystem.h"
#include "tpixelutils.h"

#include <algorithm>

// Qt includes
#include <QMainWindow>
#include <QCameraInfo>
#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraViewfinderSettings>

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
#include <QGroupBox>
#include <QDateTime>
#include <QMultimedia>
#include <QPainter>
#include <QKeyEvent>
#include <QCommonStyle>
#include <QTimer>
#include <QIntValidator>
#include <QRegExpValidator>

using namespace DVGui;

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

// referenced from brightnessandcontrastpopup.cpp
void my_compute_lut(double contrast, double brightness, std::vector<int>& lut) {
  const int maxChannelValue          = lut.size() - 1;
  const double half_maxChannelValueD = 0.5 * maxChannelValue;
  const double maxChannelValueD      = maxChannelValue;

  int i;
  double value, nvalue, power;

  int lutSize = lut.size();
  for (i = 0; i < lutSize; i++) {
    value = i / maxChannelValueD;

    // brightness
    if (brightness < 0.0)
      value = value * (1.0 + brightness);
    else
      value = value + ((1.0 - value) * brightness);

    // contrast
    if (contrast < 0.0) {
      if (value > 0.5)
        nvalue = 1.0 - value;
      else
        nvalue                 = value;
      if (nvalue < 0.0) nvalue = 0.0;
      nvalue = 0.5 * pow(nvalue * 2.0, (double)(1.0 + contrast));
      if (value > 0.5)
        value = 1.0 - nvalue;
      else
        value = nvalue;
    } else {
      if (value > 0.5)
        nvalue = 1.0 - value;
      else
        nvalue                 = value;
      if (nvalue < 0.0) nvalue = 0.0;
      power =
          (contrast == 1.0) ? half_maxChannelValueD : 1.0 / (1.0 - contrast);
      nvalue = 0.5 * pow(2.0 * nvalue, power);
      if (value > 0.5)
        value = 1.0 - nvalue;
      else
        value = nvalue;
    }

    lut[i] = value * maxChannelValueD;
  }
}

//-----------------------------------------------------------------------------

inline void doPixGray(QRgb* pix, const std::vector<int>& lut) {
  int gray = qGray(qRgb(lut[qRed(*pix)], lut[qGreen(*pix)], lut[qBlue(*pix)]));
  *pix     = qRgb(gray, gray, gray);
}

//-----------------------------------------------------------------------------

inline void doPixBinary(QRgb* pix, const std::vector<int>& lut,
                        unsigned char threshold) {
  int gray = qGray(qRgb(lut[qRed(*pix)], lut[qGreen(*pix)], lut[qBlue(*pix)]));
  if ((unsigned char)gray >= threshold)
    gray = 255;
  else
    gray = 0;
  *pix   = qRgb(gray, gray, gray);
}

//-----------------------------------------------------------------------------

inline void doPix(QRgb* pix, const std::vector<int>& lut) {
  // The captured image MUST be full opaque!
  *pix = qRgb(lut[qRed(*pix)], lut[qGreen(*pix)], lut[qBlue(*pix)]);
}

//-----------------------------------------------------------------------------

void onChange(QImage& img, int contrast, int brightness, bool doGray,
              unsigned char threshold = 0) {
  double b      = brightness / 127.0;
  double c      = contrast / 127.0;
  if (c > 1) c  = 1;
  if (c < -1) c = -1;

  std::vector<int> lut(TPixel32::maxChannelValue + 1);
  my_compute_lut(c, b, lut);

  int lx = img.width(), y, ly = img.height();

  if (doGray) {
    if (threshold == 0) {  // Grayscale
      for (y = 0; y < ly; ++y) {
        QRgb *pix = (QRgb *)img.scanLine(y), *endPix = (QRgb *)(pix + lx);
        while (pix < endPix) {
          doPixGray(pix, lut);
          ++pix;
        }
      }
    } else {  // Binary
      for (y = 0; y < ly; ++y) {
        QRgb *pix = (QRgb *)img.scanLine(y), *endPix = (QRgb *)(pix + lx);
        while (pix < endPix) {
          doPixBinary(pix, lut, threshold);
          ++pix;
        }
      }
    }
  } else {  // color
    for (y = 0; y < ly; ++y) {
      QRgb *pix = (QRgb *)img.scanLine(y), *endPix = (QRgb *)(pix + lx);
      while (pix < endPix) {
        doPix(pix, lut);
        ++pix;
      }
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

}  // namespace

//=============================================================================

MyViewFinder::MyViewFinder(QWidget* parent)
    : QFrame(parent)
    , m_image(QImage())
    , m_camera(0)
    , m_showOnionSkin(false)
    , m_onionOpacity(128)
    , m_upsideDown(false)
    , m_countDownTime(0) {}

void MyViewFinder::paintEvent(QPaintEvent* event) {
  QPainter p(this);

  p.fillRect(rect(), Qt::black);

  if (m_image.isNull()) {
    p.setPen(Qt::white);
    QFont font = p.font();
    font.setPixelSize(30);
    p.setFont(font);
    p.drawText(rect(), Qt::AlignCenter, tr("Camera is not available"));
    return;
  }

  p.save();

  if (m_upsideDown) {
    p.translate(m_imageRect.center());
    p.rotate(180);
    p.translate(-m_imageRect.center());
  }

  p.drawImage(m_imageRect, m_image);

  if (m_showOnionSkin && m_onionOpacity > 0.0f && !m_previousImage.isNull() &&
      m_previousImage.size() == m_image.size()) {
    p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(QColor(255, 255, 255, 255 - m_onionOpacity)));
    p.drawRect(m_imageRect);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    p.drawImage(m_imageRect, m_previousImage);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  }

  p.restore();

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
}

void MyViewFinder::resizeEvent(QResizeEvent* event) {
  if (!m_camera) return;
  QSize cameraReso = m_camera->viewfinderSettings().resolution();
  double cameraAR  = (double)cameraReso.width() / (double)cameraReso.height();
  // in case the camera aspect is wider than this widget
  if (cameraAR >= (double)width() / (double)height()) {
    m_imageRect.setWidth(width());
    m_imageRect.setHeight((int)((double)width() / cameraAR));
    m_imageRect.moveTo(0, (height() - m_imageRect.height()) / 2);
  }
  // in case the camera aspect is thinner than this widget
  else {
    m_imageRect.setHeight(height());
    m_imageRect.setWidth((int)((double)height() * cameraAR));
    m_imageRect.moveTo((width() - m_imageRect.width()) / 2, 0);
  }
}

//=============================================================================

FrameNumberLineEdit::FrameNumberLineEdit(QWidget* parent, int value)
    : LineEdit(parent) {
  setFixedWidth(54);
  m_intValidator = new QIntValidator(this);
  setValue(value);
  m_intValidator->setRange(1, 9999);

  QRegExp rx("^[0-9]{1,4}[A-Ia-i]?$");
  m_regexpValidator = new QRegExpValidator(rx, this);

  updateValidator();
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::updateValidator() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
    setValidator(m_regexpValidator);
  else
    setValidator(m_intValidator);
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::setValue(int value) {
  if (value <= 0)
    value = 1;
  else if (value > 9999)
    value = 9999;

  QString str;
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    str.setNum((int)(value / 10));
    while (str.length() < 3) str.push_front("0");
    QChar letter = numToLetter(value % 10);
    if (!letter.isNull()) str.append(letter);
  } else {
    str.setNum(value);
    while (str.length() < 4) str.push_front("0");
  }
  setText(str);
  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

int FrameNumberLineEdit::getValue() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    QString str = text();
    // if no letters added
    if (str.at(str.size() - 1).isDigit())
      return str.toInt() * 10;
    else {
      return str.left(str.size() - 1).toInt() * 10 +
             letterToNum(str.at(str.size() - 1));
    }
  } else
    return text().toInt();
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::focusOutEvent(QFocusEvent*) {}

//=============================================================================

PencilTestPopup::PencilTestPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, false, "PencilTest")
    , m_currentCamera(0)
    , m_cameraImageCapture(0)
    , m_captureWhiteBGCue(false)
    , m_captureCue(false) {
  setWindowTitle(tr("Camera Capture"));

  // add maximize button to the dialog
  Qt::WindowFlags flags = windowFlags();
  flags |= Qt::WindowMaximizeButtonHint;
  setWindowFlags(flags);

  layout()->setSizeConstraint(QLayout::SetNoConstraint);

  std::wstring dateTime =
      QDateTime::currentDateTime().toString("yyMMddhhmmss").toStdWString();
  TFilePath cacheImageFp = ToonzFolder::getCacheRootFolder() +
                           TFilePath(L"penciltest" + dateTime + L".jpg");
  m_cacheImagePath = cacheImageFp.getQString();

  m_cameraViewfinder = new MyViewFinder(this);
  // CameraViewfinderContainer* cvfContainer = new
  // CameraViewfinderContainer(m_cameraViewfinder, this);

  m_cameraListCombo                 = new QComboBox(this);
  QPushButton* refreshCamListButton = new QPushButton(tr("Refresh"), this);
  m_resolutionCombo                 = new QComboBox(this);

  QGroupBox* fileFrame = new QGroupBox(tr("File"), this);
  m_levelNameEdit      = new QLineEdit(this);
  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  int startFrame =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled() ? 10 : 1;
  m_frameNumberEdit        = new FrameNumberLineEdit(this, startFrame);
  m_fileTypeCombo          = new QComboBox(this);
  m_fileFormatOptionButton = new QPushButton(tr("Options"), this);
  m_saveInFileFld          = new FileField(
      0, QString("+%1").arg(QString::fromStdString(TProject::Extras)));
  QToolButton* nextLevelButton = new QToolButton(this);
  m_saveOnCaptureCB =
      new QCheckBox(tr("Save images as they are captured"), this);

  QGroupBox* imageFrame = new QGroupBox(tr("Image adjust"), this);
  m_colorTypeCombo      = new QComboBox(this);

  m_thresholdFld  = new IntField(this);
  m_contrastFld   = new IntField(this);
  m_brightnessFld = new IntField(this);
  m_upsideDownCB  = new QCheckBox(tr("Upside down"), this);

  m_bgReductionFld       = new IntField(this);
  m_captureWhiteBGButton = new QPushButton(tr("Capture white BG"), this);

  QGroupBox* displayFrame = new QGroupBox(tr("Display"), this);
  m_onionSkinCB           = new QCheckBox(tr("Show onion skin"), this);
  m_onionOpacityFld       = new IntField(this);

  QGroupBox* timerFrame = new QGroupBox(tr("Interval timer"), this);
  m_timerCB             = new QCheckBox(tr("Use interval timer"), this);
  m_timerIntervalFld    = new IntField(this);
  m_captureTimer        = new QTimer(this);
  m_countdownTimer      = new QTimer(this);

  m_captureButton          = new QPushButton(tr("Capture\n[Return key]"), this);
  QPushButton* closeButton = new QPushButton(tr("Close"), this);
  //----

  m_resolutionCombo->setMaximumWidth(fontMetrics().width("0000 x 0000") + 25);
  m_fileTypeCombo->addItems({"jpg", "png", "tga", "tif"});
  m_fileTypeCombo->setCurrentIndex(0);

  fileFrame->setObjectName("CleanupSettingsFrame");
  // Exclude all character which cannot fit in a filepath (Win).
  // Dots are also prohibited since they are internally managed by Toonz.
  QRegExp rx("[^\\\\/:?*.\"<>|]+");
  m_levelNameEdit->setValidator(new QRegExpValidator(rx, this));
  m_levelNameEdit->setObjectName("LargeSizedText");
  m_frameNumberEdit->setObjectName("LargeSizedText");
  nextLevelButton->setFixedSize(24, 24);
  nextLevelButton->setArrowType(Qt::RightArrow);
  nextLevelButton->setToolTip(tr("Next Level"));
  m_saveOnCaptureCB->setChecked(true);

  imageFrame->setObjectName("CleanupSettingsFrame");
  m_colorTypeCombo->addItems({"Color", "Grayscale", "Black & White"});
  m_colorTypeCombo->setCurrentIndex(0);
  m_thresholdFld->setRange(1, 255);
  m_thresholdFld->setValue(128);
  m_thresholdFld->setDisabled(true);
  m_contrastFld->setRange(-127, 127);
  m_contrastFld->setValue(0);
  m_brightnessFld->setRange(-127, 127);
  m_brightnessFld->setValue(0);
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
  m_captureButton->setFixedHeight(80);
  QCommonStyle style;
  m_captureButton->setIcon(style.standardIcon(QStyle::SP_DialogOkButton));
  m_captureButton->setIconSize(QSize(30, 30));

  //---- layout ----
  QHBoxLayout* mainLay = new QHBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(10);
  {
    QVBoxLayout* leftLay = new QVBoxLayout();
    leftLay->setMargin(5);
    leftLay->setSpacing(10);
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
        camLay->addStretch(0);
      }
      leftLay->addLayout(camLay, 0);
      leftLay->addWidget(m_cameraViewfinder, 1);
    }
    mainLay->addLayout(leftLay, 1);

    QVBoxLayout* rightLay = new QVBoxLayout();
    rightLay->setMargin(0);
    rightLay->setSpacing(5);
    {
      QVBoxLayout* fileLay = new QVBoxLayout();
      fileLay->setMargin(10);
      fileLay->setSpacing(10);
      {
        QGridLayout* levelLay = new QGridLayout();
        levelLay->setMargin(0);
        levelLay->setHorizontalSpacing(3);
        levelLay->setVerticalSpacing(10);
        {
          levelLay->addWidget(new QLabel(tr("Name:"), this), 0, 0,
                              Qt::AlignRight);
          levelLay->addWidget(m_levelNameEdit, 0, 1);
          levelLay->addWidget(nextLevelButton, 0, 2);

          levelLay->addWidget(new QLabel(tr("Frame:"), this), 1, 0,
                              Qt::AlignRight);
          levelLay->addWidget(m_frameNumberEdit, 1, 1);
        }
        levelLay->setColumnStretch(0, 0);
        levelLay->setColumnStretch(1, 1);
        levelLay->setColumnStretch(2, 0);
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

        QHBoxLayout* saveInLay = new QHBoxLayout();
        saveInLay->setMargin(0);
        saveInLay->setSpacing(3);
        {
          saveInLay->addWidget(new QLabel(tr("Save In:"), this), 0);
          saveInLay->addWidget(m_saveInFileFld, 1);
        }
        fileLay->addLayout(saveInLay, 0);

        fileLay->addWidget(m_saveOnCaptureCB, 0);
      }
      fileFrame->setLayout(fileLay);
      rightLay->addWidget(fileFrame, 0);

      QGridLayout* imageLay = new QGridLayout();
      imageLay->setMargin(10);
      imageLay->setHorizontalSpacing(3);
      imageLay->setVerticalSpacing(10);
      {
        imageLay->addWidget(new QLabel(tr("Color type:"), this), 0, 0,
                            Qt::AlignRight);
        imageLay->addWidget(m_colorTypeCombo, 0, 1);

        imageLay->addWidget(new QLabel(tr("Threshold:"), this), 1, 0,
                            Qt::AlignRight);
        imageLay->addWidget(m_thresholdFld, 1, 1, 1, 2);

        imageLay->addWidget(new QLabel(tr("Contrast:"), this), 2, 0,
                            Qt::AlignRight);
        imageLay->addWidget(m_contrastFld, 2, 1, 1, 2);

        imageLay->addWidget(new QLabel(tr("Brightness:"), this), 3, 0,
                            Qt::AlignRight);
        imageLay->addWidget(m_brightnessFld, 3, 1, 1, 2);

        imageLay->addWidget(m_upsideDownCB, 4, 0, 1, 3, Qt::AlignLeft);

        imageLay->addWidget(new QLabel(tr("BG reduction:"), this), 5, 0,
                            Qt::AlignRight);
        imageLay->addWidget(m_bgReductionFld, 5, 1, 1, 2);

        imageLay->addWidget(m_captureWhiteBGButton, 6, 0, 1, 3);
      }
      imageLay->setColumnStretch(0, 0);
      imageLay->setColumnStretch(1, 0);
      imageLay->setColumnStretch(2, 1);
      imageFrame->setLayout(imageLay);
      rightLay->addWidget(imageFrame, 0);

      QGridLayout* displayLay = new QGridLayout();
      displayLay->setMargin(10);
      displayLay->setHorizontalSpacing(3);
      displayLay->setVerticalSpacing(10);
      {
        displayLay->addWidget(m_onionSkinCB, 0, 0, 1, 2);

        displayLay->addWidget(new QLabel(tr("Opacity(%):"), this), 1, 0,
                              Qt::AlignRight);
        displayLay->addWidget(m_onionOpacityFld, 1, 1);
      }
      displayLay->setColumnStretch(0, 0);
      displayLay->setColumnStretch(1, 1);
      // displayLay->setColumnStretch(2, 1);
      displayFrame->setLayout(displayLay);
      rightLay->addWidget(displayFrame);

      QGridLayout* timerLay = new QGridLayout();
      timerLay->setMargin(10);
      timerLay->setHorizontalSpacing(3);
      timerLay->setVerticalSpacing(10);
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
      rightLay->addSpacing(20);
      rightLay->addWidget(closeButton, 0);
    }
    mainLay->addLayout(rightLay, 0);
  }
  m_topLayout->addLayout(mainLay);

  //---- signal-slot connections ----
  bool ret = true;
  ret      = ret && connect(refreshCamListButton, SIGNAL(pressed()), this,
                       SLOT(refreshCameraList()));
  ret = ret && connect(m_cameraListCombo, SIGNAL(activated(int)), this,
                       SLOT(onCameraListComboActivated(int)));
  ret = ret && connect(m_resolutionCombo, SIGNAL(activated(const QString&)),
                       this, SLOT(onResolutionComboActivated(const QString&)));
  ret = ret && connect(m_fileFormatOptionButton, SIGNAL(pressed()), this,
                       SLOT(onFileFormatOptionButtonPressed()));
  ret = ret &&
        connect(nextLevelButton, SIGNAL(pressed()), this, SLOT(onNextName()));
  ret = ret && connect(m_colorTypeCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onColorTypeComboChanged(int)));
  ret = ret && connect(m_captureWhiteBGButton, SIGNAL(pressed()), this,
                       SLOT(onCaptureWhiteBGButtonPressed()));
  ret = ret && connect(m_onionSkinCB, SIGNAL(toggled(bool)), this,
                       SLOT(onOnionCBToggled(bool)));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onOnionOpacityFldEdited()));
  ret = ret && connect(m_upsideDownCB, SIGNAL(toggled(bool)),
                       m_cameraViewfinder, SLOT(onUpsideDownChecked(bool)));
  ret = ret && connect(m_timerCB, SIGNAL(toggled(bool)), this,
                       SLOT(onTimerCBToggled(bool)));
  ret = ret && connect(m_captureTimer, SIGNAL(timeout()), this,
                       SLOT(onCaptureTimerTimeout()));
  ret = ret &&
        connect(m_countdownTimer, SIGNAL(timeout()), this, SLOT(onCountDown()));

  ret = ret && connect(closeButton, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret && connect(m_captureButton, SIGNAL(clicked(bool)), this,
                       SLOT(onCaptureButtonClicked(bool)));
  assert(ret);

  refreshCameraList();
  onNextName();
}

//-----------------------------------------------------------------------------

PencilTestPopup::~PencilTestPopup() {
  if (m_currentCamera && m_currentCamera->state() == QCamera::ActiveState)
    m_currentCamera->stop();
  // remove the cache image, if it exists
  TFilePath fp(m_cacheImagePath);
  if (TFileStatus(fp).doesExist()) TSystem::deleteFile(fp);
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
  int defaultIndex;
  for (int c = 0; c < cameras.size(); c++) {
    QString camDesc = cameras.at(c).description();
    m_cameraListCombo->addItem(camDesc);
    maxTextLength = std::max(maxTextLength, fontMetrics().width(camDesc));
    if (cameras.at(c).deviceName() == QCameraInfo::defaultCamera().deviceName())
      defaultIndex = c;
  }
  m_cameraListCombo->setMaximumWidth(maxTextLength + 25);
  m_cameraListCombo->setEnabled(true);
  m_cameraListCombo->setCurrentIndex(defaultIndex);

  onCameraListComboActivated(defaultIndex);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCameraListComboActivated(int index) {
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if (cameras.size() != m_cameraListCombo->count()) return;

  // in case the camera is not changed (just click the combobox)
  if (cameras.at(index).deviceName() == m_deviceName) return;

  QCamera* oldCamera = m_currentCamera;
  m_currentCamera    = new QCamera(cameras.at(index), this);
  m_deviceName       = cameras.at(index).deviceName();
  if (m_cameraImageCapture) {
    disconnect(m_cameraImageCapture, SIGNAL(imageCaptured(int, const QImage&)),
               this, SLOT(onImageCaptured(int, const QImage&)));
    delete m_cameraImageCapture;
  }
  m_cameraImageCapture = new QCameraImageCapture(m_currentCamera, this);
  /* Capturing to buffer currently seems not to be supported on Windows */
  // if
  // (!m_cameraImageCapture->isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer))
  //  std::cout << "it does not support CaptureToBuffer" << std::endl;
  m_cameraImageCapture->setCaptureDestination(
      QCameraImageCapture::CaptureToBuffer);
  connect(m_cameraImageCapture, SIGNAL(imageCaptured(int, const QImage&)), this,
          SLOT(onImageCaptured(int, const QImage&)));

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
    QImageEncoderSettings imageEncoderSettings;
    imageEncoderSettings.setCodec("PNG");
    imageEncoderSettings.setResolution(sizes.last());
    m_cameraImageCapture->setEncodingSettings(imageEncoderSettings);
  }
  m_cameraViewfinder->setCamera(m_currentCamera);

  // deleting old camera
  if (oldCamera) {
    if (oldCamera->state() == QCamera::ActiveState) oldCamera->stop();
    delete oldCamera;
  }
  // start new camera
  m_currentCamera->start();
  m_cameraViewfinder->setImage(QImage());
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onResolutionComboActivated(const QString& itemText) {
  // resolution is written in the itemText with the format "<width> x <height>"
  // (e.g. "800 x 600")
  QStringList texts = itemText.split(' ');
  // the splited text must be "<width>" "x" and "<height>"
  if (texts.size() != 3) return;

  m_currentCamera->stop();
  m_currentCamera->unload();
  QCameraViewfinderSettings settings = m_currentCamera->viewfinderSettings();
  QSize newResolution(texts[0].toInt(), texts[2].toInt());
  settings.setResolution(newResolution);
  m_currentCamera->setViewfinderSettings(settings);
  QImageEncoderSettings imageEncoderSettings;
  imageEncoderSettings.setCodec("image/jpeg");
  imageEncoderSettings.setQuality(QMultimedia::NormalQuality);
  imageEncoderSettings.setResolution(newResolution);
  m_cameraImageCapture->setEncodingSettings(imageEncoderSettings);

  // reset white bg
  m_whiteBGImg = QImage();
  m_bgReductionFld->setDisabled(true);

  m_currentCamera->start();
  m_cameraViewfinder->setImage(QImage());
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onFileFormatOptionButtonPressed() {
  if (m_fileTypeCombo->currentIndex() == 0) return;
  // Tentatively use the preview output settings
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties* prop = scene->getProperties()->getPreviewProperties();
  std::string ext         = m_fileTypeCombo->currentText().toStdString();
  openFormatSettingsPopup(this, ext, prop->getFileFormatProperties(ext));
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onNextName() {
  const std::auto_ptr<NameBuilder> nameBuilder(NameBuilder::getBuilder(L""));

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

  m_levelNameEdit->setText(QString::fromStdWString(levelName));
  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  int startFrame =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled() ? 10 : 1;
  m_frameNumberEdit->setValue(startFrame);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onColorTypeComboChanged(int index) {
  m_thresholdFld->setEnabled(index == 2);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onImageCaptured(int id, const QImage& image) {
  // capture the white BG
  if (m_captureWhiteBGCue) {
    m_whiteBGImg        = image.copy();
    m_captureWhiteBGCue = false;
    m_bgReductionFld->setEnabled(true);
  }

  QImage procImg = image.copy();
  processImage(procImg);
  m_cameraViewfinder->setImage(procImg);

  if (m_captureCue) {
    m_captureCue = false;
    if (importImage(procImg)) {
      m_cameraViewfinder->setPreviousImage(procImg);
      if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
        int f = m_frameNumberEdit->getValue();
        m_frameNumberEdit->setValue(((int)(f / 10) + 1) * 10);
      } else
        m_frameNumberEdit->setValue(m_frameNumberEdit->getValue() + 1);

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
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::timerEvent(QTimerEvent* event) {
  if (!m_currentCamera || !m_cameraImageCapture ||
      !m_cameraImageCapture->isAvailable() ||
      !m_cameraImageCapture->isReadyForCapture())
    return;

  m_currentCamera->setCaptureMode(QCamera::CaptureStillImage);
  m_currentCamera->start();
  m_currentCamera->searchAndLock();
  m_cameraImageCapture->capture(m_cacheImagePath);
  m_currentCamera->unlock();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::showEvent(QShowEvent* event) {
  m_timerId = startTimer(10);

  // if there is another action of which "return" key is assigned as short cut
  // key,
  // then release the shortcut key temporary while the popup opens
  QAction* action = CommandManager::instance()->getActionFromShortcut("Return");
  if (action) action->setShortcut(QKeySequence(""));
}

//-----------------------------------------------------------------------------

void PencilTestPopup::hideEvent(QHideEvent* event) {
  killTimer(m_timerId);

  // set back the "return" short cut key
  QAction* action = CommandManager::instance()->getActionFromShortcut("Return");
  if (action) action->setShortcut(QKeySequence("Return"));

  // stop interval timer if it is active
  if (m_timerCB->isChecked() && m_captureButton->isChecked()) {
    m_captureButton->setChecked(false);
    onCaptureButtonClicked(false);
  }
}

//-----------------------------------------------------------------------------

void PencilTestPopup::keyPressEvent(QKeyEvent* event) {
  // override return (or enter) key as shortcut key for capturing
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    // show button-clicking animation followed by calling
    // onCaptureButtonClicked()
    m_captureButton->animateClick();
    event->accept();
  } else
    event->ignore();
}

//-----------------------------------------------------------------------------

void PencilTestPopup::processImage(QImage& image) {
  /* "upside down" is not executed here. It will be done when capturing the
   * image */

  // white bg reduction
  if (!m_whiteBGImg.isNull() && m_bgReductionFld->getValue() != 0) {
    bgReduction(image, m_whiteBGImg, m_bgReductionFld->getValue());
  }

  int threshold =
      (m_colorTypeCombo->currentIndex() != 2) ? 0 : m_thresholdFld->getValue();
  onChange(image, m_contrastFld->getValue(), m_brightnessFld->getValue(),
           m_colorTypeCombo->currentIndex() != 0, threshold);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onCaptureWhiteBGButtonPressed() {
  m_captureWhiteBGCue = true;
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onOnionCBToggled(bool on) {
  m_cameraViewfinder->setShowOnionSkin(on);
  m_onionOpacityFld->setEnabled(on);
}

//-----------------------------------------------------------------------------

void PencilTestPopup::onOnionOpacityFldEdited() {
  int value = (int)(255.0f * (float)m_onionOpacityFld->getValue() / 100.0f);
  m_cameraViewfinder->setOnionOpacity(value);
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
      m_cameraViewfinder->showCountDownTime(0);
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
  m_cameraViewfinder->showCountDownTime(
      m_captureTimer->isActive() ? m_captureTimer->remainingTime() : 0);
}

//-----------------------------------------------------------------------------
/*! referenced from LevelCreatePopup::apply()
*/
bool PencilTestPopup::importImage(QImage& image) {
  TApp* app         = TApp::instance();
  ToonzScene* scene = app->getCurrentScene()->getScene();
  TXsheet* xsh      = scene->getXsheet();

  std::wstring levelName = m_levelNameEdit->text().toStdWString();
  if (levelName.empty()) {
    error(tr("No level name specified: please choose a valid level name"));
    return false;
  }

  int frameNumber = m_frameNumberEdit->getValue();

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

  TFilePath levelFp = TFilePath(m_saveInFileFld->getPath()) +
                      TFilePath(levelName + L".." +
                                m_fileTypeCombo->currentText().toStdWString());
  TFilePath actualLevelFp = scene->decodeFilePath(levelFp);

  TXshSimpleLevel* sl = 0;

  TXshLevel* level = scene->getLevelSet()->getLevel(levelName);
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
    /* if the level already have the same frame, then ask if overwrite it */
    TFilePath frameFp(actualLevelFp.withFrame(frameNumber));
    if (TFileStatus(frameFp).doesExist()) {
      QString question = tr("File %1 does exist.\nDo you want to overwrite it?")
                             .arg(toQString(frameFp));
      int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                              QObject::tr("Cancel"));
      if (ret == 0 || ret == 2) return false;
    }
  }
  /* if the level does not exist in the scene cast */
  else {
    /* if the file does exist, load it first */
    if (TFileStatus(actualLevelFp).doesExist()) {
      level = scene->loadLevel(actualLevelFp);
      if (!level) {
        error(tr("Failed to load %1.").arg(toQString(actualLevelFp)));
        return false;
      }

      /* if the loaded level does not match in pixel size, then return */
      sl = level->getSimpleLevel();
      if (!sl ||
          sl->getProperties()->getImageRes() !=
              TDimension(image.width(), image.height())) {
        error(tr(
            "The captured image size does not match with the existing level."));
        return false;
      }
    }
    /* if the file does not exist, then create a new level */
    else {
      TXshLevel* level = scene->createNewLevel(OVL_XSHLEVEL, levelName,
                                               TDimension(), 0, levelFp);
      sl = level->getSimpleLevel();
      sl->setPath(levelFp, true);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
      TPointD currentCamDpi = getCurrentCameraDpi();
      sl->getProperties()->setDpi(currentCamDpi.x);
      sl->getProperties()->setImageDpi(currentCamDpi);
      sl->getProperties()->setImageRes(
          TDimension(image.width(), image.height()));
    }
  }

  TFrameId fid(frameNumber);
  TPointD levelDpi = sl->getDpi();
  /* create the raster */
  TRaster32P raster(image.width(), image.height());
  convertImageToRaster(raster, (m_upsideDownCB->isChecked())
                                   ? image
                                   : image.mirrored(true, true));
  TRasterImageP ri(raster);
  ri->setDpi(levelDpi.x, levelDpi.y);
  /* setting the frame */
  sl->setFrame(fid, ri);

  /* set dirty flag */
  sl->getProperties()->setDirtyFlag(true);

  if (m_saveOnCaptureCB->isChecked()) sl->save();

  /* placement in xsheet */
  int row = app->getCurrentFrame()->getFrame();
  int col = app->getCurrentColumn()->getColumnIndex();

  /* try to find the vacant cell */
  int tmpRow            = row;
  bool isFoundEmptyCell = false;
  while (1) {
    if (xsh->getCell(tmpRow, col).isEmpty()) {
      isFoundEmptyCell = true;
      break;
    }
    if (xsh->getCell(tmpRow, col).m_level->getSimpleLevel() != sl) break;
    tmpRow++;
  }

  /* in case setting the same level as the the current column */
  if (isFoundEmptyCell) {
    xsh->setCell(tmpRow, col, TXshCell(sl, fid));
  }
  /* in case the level is different from the current column, then insert a new
     column */
  else {
    col += 1;
    xsh->insertColumn(col);
    xsh->setCell(row, col, TXshCell(sl, fid));
    app->getCurrentColumn()->setColumnIndex(col);
  }
  /* notify */
  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentScene()->notifyCastChange();
  app->getCurrentXsheet()->notifyXsheetChanged();

  return true;
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<PencilTestPopup> openPencilTestPopup(MI_PencilTest);