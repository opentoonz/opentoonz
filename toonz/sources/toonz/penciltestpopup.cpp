#include "penciltestpopup.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "formatsettingspopups.h"
#include "filebrowsermodel.h"
#include "cellselection.h"
#include "toonzqt/tselectionhandle.h"
#include "cameracapturelevelcontrol.h"
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
#include <QGroupBox>
#include <QDateTime>
#include <QMultimedia>
#include <QPainter>
#include <QKeyEvent>
#include <QCommonStyle>
#include <QTimer>
#include <QIntValidator>
#include <QRegExpValidator>
#include <QPushButton>

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
  *pix   = qRgb(gray, gray, gray);
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

  int lx = img.width(), y, ly = img.height();

  if (doGray) {
    for (y = 0; y < ly; ++y) {
      QRgb *pix = (QRgb *)img.scanLine(y), *endPix = (QRgb *)(pix + lx);
      while (pix < endPix) {
        doPixGray(pix, lut);
        ++pix;
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

void onChangeBW(QImage& img, int threshold) {
  int lx = img.width(), y, ly = img.height();
  for (y = 0; y < ly; ++y) {
    QRgb *pix = (QRgb *)img.scanLine(y), *endPix = (QRgb *)(pix + lx);
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

void MyViewFinder::updateSize() {
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

void MyViewFinder::resizeEvent(QResizeEvent* event) { updateSize(); }

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
    str = convertToFrameWithLetter(value, 3);
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

void FrameNumberLineEdit::focusOutEvent(QFocusEvent* e) {
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
  setWindowTitle("Create the Destination Subfolder to Save");

  m_parentFolderField = new FileField(this);

  QPushButton* setAsDefaultBtn = new QPushButton(tr("Set As Default"), this);
  setAsDefaultBtn->setToolTip(
      tr("Set the current \"Save In\" path as the default."));

  m_subFolderCB = new QCheckBox(tr("Create Subfolder"), this);

  QFrame* subFolderFrame = new QFrame(this);

  QGroupBox* infoGroupBox    = new QGroupBox(tr("Infomation"), this);
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

  return m_parentFolderField->getPath() + "\\" + m_subFolderNameField->text();
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
};

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
  TOutputProperties* prop = scene->getProperties()->getOutputProperties();
  TFilePath outFp         = prop->getPath().withParentDir(fp);

  prop->setPath(outFp);

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
    : Dialog(0, false, false, "PencilTest"),
      m_currentCamera(NULL),
      m_cameraImageCapture(NULL),
      m_captureWhiteBGCue(false),
      m_captureCue(false) {
  setWindowTitle(tr("Camera Capture"));

  // add maximize button to the dialog
  setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);

  layout()->setSizeConstraint(QLayout::SetNoConstraint);

  std::wstring dateTime =
      QDateTime::currentDateTime().toString("yyMMddhhmmss").toStdWString();
  TFilePath cacheImageFp = ToonzFolder::getCacheRootFolder() +
                           TFilePath(L"penciltest" + dateTime + L".jpg");
  m_cacheImagePath = cacheImageFp.getQString();

  m_saveInFolderPopup = new PencilTestSaveInFolderPopup(this);

  m_cameraViewfinder = new MyViewFinder(this);
  // CameraViewfinderContainer* cvfContainer = new
  // CameraViewfinderContainer(m_cameraViewfinder, this);

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

#ifdef MACOSX
  m_dummyViewFinder = new QCameraViewfinder(this);
  m_dummyViewFinder->hide();
#endif
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
    m_captureFilterSettingsBtn->setFixedSize(23, 23);
    m_captureFilterSettingsBtn->setIconSize(QSize(15, 15));
    m_captureFilterSettingsBtn->setToolTip(
        tr("Video Capture Filter Settings..."));
  }

  subfolderButton->setObjectName("SubfolderButton");
  subfolderButton->setIconSize(QSize(15, 15));
  m_saveInFileFld->setMaximumWidth(380);

  m_saveInFolderPopup->hide();

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
      bottomLay->addWidget(m_cameraViewfinder, 1);

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
        // displayLay->setColumnStretch(2, 1);
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
  ret = ret && connect(m_cameraListCombo, SIGNAL(activated(int)), this,
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
    if (startupResolutionIndex > 0) {
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
    m_cameraViewfinder->setCamera(NULL);
    if (m_cameraImageCapture) {
      disconnect(m_cameraImageCapture,
                 SIGNAL(imageCaptured(int, const QImage&)), this,
                 SLOT(onImageCaptured(int, const QImage&)));
      delete m_cameraImageCapture;
      m_cameraImageCapture = NULL;
    }
    if (m_currentCamera) {
      if (m_currentCamera->state() == QCamera::ActiveState)
        m_currentCamera->stop();
      if (m_currentCamera->state() == QCamera::LoadedState)
        m_currentCamera->unload();
    }
    m_deviceName = QString();
    m_cameraViewfinder->setImage(QImage());
    // update env
    CamCapCameraName = "";
    return;
  }

  int index = comboIndex - 1;
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

#ifdef MACOSX
  // this line is needed only in macosx
  m_currentCamera->setViewfinder(m_dummyViewFinder);
#endif

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
    imageEncoderSettings.setCodec("image/jpeg");
    imageEncoderSettings.setQuality(QMultimedia::NormalQuality);
    imageEncoderSettings.setResolution(sizes.last());
    m_cameraImageCapture->setEncodingSettings(imageEncoderSettings);
  }
  m_cameraViewfinder->setCamera(m_currentCamera);
  m_cameraViewfinder->updateSize();

  // deleting old camera
  if (oldCamera) {
    if (oldCamera->state() == QCamera::ActiveState) oldCamera->stop();
    delete oldCamera;
  }
  // start new camera
  m_currentCamera->start();
  m_cameraViewfinder->setImage(QImage());

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
  m_cameraViewfinder->updateSize();

#ifdef MACOSX
  m_dummyViewFinder->resize(newResolution);
#endif

  // reset white bg
  m_whiteBGImg = QImage();
  m_bgReductionFld->setDisabled(true);

  m_currentCamera->start();
  m_cameraViewfinder->setImage(QImage());

  // update env
  CamCapCameraResolution = itemText.toStdString();

  refreshFrameInfo();
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

  // if the current level name is non-sequencial, then try to switch the last
  // sequencial level in the scene.
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

void PencilTestPopup::onImageCaptured(int id, const QImage& image) {
  if (!m_cameraViewfinder) return;
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
        if (f % 10 == 0)  // next number
          m_frameNumberEdit->setValue(((int)(f / 10) + 1) * 10);
        else  // next alphabet
          m_frameNumberEdit->setValue(f + 1);
      } else
        m_frameNumberEdit->setValue(m_frameNumberEdit->getValue() + 1);

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
  onSceneSwitched();
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
  // obtain histogram AFTER bg reduction
  m_camCapLevelControl->updateHistogram(image);

  // color and grayscale mode
  if (m_colorTypeCombo->currentIndex() != 2) {
    int black, white;
    float gamma;
    m_camCapLevelControl->getValues(black, white, gamma);
    onChange(image, black, white, gamma, m_colorTypeCombo->currentIndex() != 0);
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
  m_cameraViewfinder->setShowOnionSkin(on);
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
    if (m_upsideDownCB->isChecked()) {
      painter.translate(m_lx / 2, m_ly / 2);
      painter.rotate(180);
      painter.translate(-m_lx / 2, -m_ly / 2);
    }
    painter.drawImage(0, 0, qi);
    m_cameraViewfinder->setPreviousImage(qi2);
    m_onionSkinCB->setChecked(true);
    free(buffer);
  }
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
  enum State { NEWLEVEL = 0, ADDFRAME, OVERWRITE } state;

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
      if (!sl ||
          sl->getProperties()->getImageRes() !=
              TDimension(image.width(), image.height())) {
        error(tr(
            "The captured image size does not match with the existing level."));
        return false;
      }

      /* confirm overwrite */
      TFilePath frameFp(actualLevelFp.withFrame(frameNumber));
      if (TFileStatus(frameFp).doesExist()) {
        QString question =
            tr("File %1 does exist.\nDo you want to overwrite it?")
                .arg(toQString(frameFp));
        int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                                QObject::tr("Cancel"));
        if (ret == 0 || ret == 2) return false;
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

    state = NEWLEVEL;
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

  // if the level is newly created or imported, then insert a new column
  if (state == NEWLEVEL) {
    if (!xsh->isColumnEmpty(col)) {
      col += 1;
      xsh->insertColumn(col);
    }
    xsh->setCell(row, col, TXshCell(sl, fid));
    app->getCurrentColumn()->setColumnIndex(col);
    return true;
  }

  // state == OVERWRITE, ADDFRAME

  // if the same cell is already in the column, then just replace the content
  // and do not set a new cell
  int foundCol, foundRow = -1;
  // most possibly, it's in the current column
  int rowCheck;
  if (findCell(xsh, col, TXshCell(sl, fid), rowCheck)) return true;
  if (rowCheck >= 0) {
    foundRow = rowCheck;
    foundCol = col;
  }
  // search entire xsheet
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    if (c == col) continue;
    if (findCell(xsh, c, TXshCell(sl, fid), rowCheck)) return true;
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
        xsh->setCell(tmpRow, foundCol, TXshCell(sl, fid));
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
    xsh->setCell(row, col, TXshCell(sl, fid));
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
  int frameNumber        = m_frameNumberEdit->getValue();

  QStringList texts = m_resolutionCombo->currentText().split(' ');
  if (texts.size() != 3) return;
  TDimension camRes(texts[0].toInt(), texts[2].toInt());

  bool letterOptionEnabled =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled();

  // level with the same name
  TXshLevel* level_sameName = levelSet->getLevel(levelName);

  TFilePath levelFp = TFilePath(m_saveInFileFld->getPath()) +
                      TFilePath(levelName + L".." +
                                m_fileTypeCombo->currentText().toStdWString());

  // level with the same path
  TXshLevel* level_samePath = levelSet->getLevel(*(currentScene), levelFp);

  TFilePath actualLevelFp = currentScene->decodeFilePath(levelFp);

  // level existence
  bool levelExist = TSystem::doesExistFileOrLevel(actualLevelFp);

  // frame existence
  TFilePath frameFp(actualLevelFp.withFrame(frameNumber));
  bool frameExist            = false;
  if (levelExist) frameExist = TFileStatus(frameFp).doesExist();

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
      for (int i = 0; it != level_p->end(); ++it, ++i)
        fids.push_back(it->first);

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
        if (fids.at(f).getNumber() == frameNumber) {
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
