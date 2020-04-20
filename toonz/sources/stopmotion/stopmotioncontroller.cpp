#include "stopmotioncontroller.h"

// TnzLib includes
#include "toonz/levelset.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tstageobjecttree.h"

// TnzCore includes
#include "filebrowsermodel.h"
#include "formatsettingspopups.h"
#include "tapp.h"
#include "tenv.h"
#include "tlevel_io.h"
#include "toutputproperties.h"
#include "tsystem.h"

// TnzQt includes
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QAction>
#include <QApplication>
#include <QCameraInfo>
#include <QCheckBox>
#include <QComboBox>
#include <QCommonStyle>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>

#ifdef _WIN32
#include <dshow.h>
#endif

namespace {

//-----------------------------------------------------------------------------

#ifdef _WIN32
void openCaptureFilterSettings(const QWidget *parent,
                               const QString &cameraName) {
  HRESULT hr;

  ICreateDevEnum *createDevEnum = NULL;
  IEnumMoniker *enumMoniker     = NULL;
  IMoniker *moniker             = NULL;

  IBaseFilter *deviceFilter;

  ISpecifyPropertyPages *specifyPropertyPages;
  CAUUID cauuid;
  // set parent's window handle in order to make the dialog modal
  HWND ghwndApp = (HWND)(parent->winId());

  // initialize COM
  CoInitialize(NULL);

  // get device list
  CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                   IID_ICreateDevEnum, (PVOID *)&createDevEnum);

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
    IPropertyBag *pPropertyBag;
    moniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropertyBag);
    VARIANT var;
    var.vt = VT_BSTR;
    VariantInit(&var);

    pPropertyBag->Read(L"FriendlyName", &var, 0);

    QString deviceName = QString::fromWCharArray(var.bstrVal);

    VariantClear(&var);

    if (deviceName == cameraName) {
      // bind monkier to the filter
      moniker->BindToObject(0, 0, IID_IBaseFilter, (void **)&deviceFilter);

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
                                    (void **)&specifyPropertyPages);
  if (hr == S_OK) {
    hr = specifyPropertyPages->GetPages(&cauuid);

    hr = OleCreatePropertyFrame(ghwndApp, 30, 30, NULL, 1,
                                (IUnknown **)&deviceFilter, cauuid.cElems,
                                (GUID *)cauuid.pElems, 0, 0, NULL);

    CoTaskMemFree(cauuid.pElems);
    specifyPropertyPages->Release();
  }
}
#endif

//-----------------------------------------------------------------------------

QScrollArea *makeChooserPage(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

//-----------------------------------------------------------------------------

QScrollArea *makeChooserPageWithoutScrollBar(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

}  // namespace

//*****************************************************************************
//    StopMotionController  implementation
//*****************************************************************************

StopMotionController::StopMotionController(QWidget *parent) : QWidget(parent) {
  m_stopMotion = StopMotion::instance();
  m_tabBar     = new DVGui::TabBar(this);
  m_tabBar->setDrawBase(false);
  m_tabBar->setObjectName("StopMotionTabBar");
  m_tabBar->addSimpleTab(tr("Controls"));
  m_tabBar->addSimpleTab(tr("Settings"));
  m_tabBar->addSimpleTab(tr("Options"));
  m_tabBarContainer    = new TabBarContainter(this);
  m_mainControlsPage   = new QFrame();
  m_cameraSettingsPage = new QFrame();
  m_optionsPage        = new QFrame();

  // **********************
  // Make Control Page
  // **********************

  m_saveInFolderPopup = new PencilTestSaveInFolderPopup(this);
  m_cameraListCombo   = new QComboBox(this);
  m_resolutionCombo   = new QComboBox(this);
  m_resolutionCombo->setFixedWidth(fontMetrics().width("0000 x 0000") + 25);
  m_resolutionLabel                 = new QLabel(tr("Resolution: "), this);
  QPushButton *refreshCamListButton = new QPushButton(tr("Refresh"), this);
  refreshCamListButton->setFixedHeight(28);
  refreshCamListButton->setStyleSheet("padding: 0 2;");
  QGroupBox *fileFrame = new QGroupBox(tr("File"), this);
  m_levelNameEdit      = new LevelNameLineEdit(this);

#ifdef _WIN32
  m_captureFilterSettingsBtn = new QPushButton(this);
#else
  m_captureFilterSettingsBtn = 0;
#endif
  if (m_captureFilterSettingsBtn) {
    m_captureFilterSettingsBtn->setObjectName("GearButton");
    m_captureFilterSettingsBtn->setFixedSize(28, 28);
    m_captureFilterSettingsBtn->setIconSize(QSize(15, 15));
    m_captureFilterSettingsBtn->setToolTip(tr("Webcam Settings..."));
  }

  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  int startFrame =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled() ? 10 : 1;
  m_frameNumberEdit        = new FrameNumberLineEdit(this, startFrame);
  m_frameInfoLabel         = new QLabel("", this);
  m_fileTypeCombo          = new QComboBox(this);
  m_fileFormatOptionButton = new QPushButton(tr("Options"), this);
  m_fileFormatOptionButton->setFixedHeight(28);
  m_fileFormatOptionButton->setStyleSheet("padding: 0 2;");
  m_xSheetFrameNumberEdit = new DVGui::IntLineEdit(this, 1, 1);
  m_saveInFileFld =
      new DVGui::FileField(this, m_saveInFolderPopup->getParentPath());
  QToolButton *nextLevelButton       = new QToolButton(this);
  m_previousLevelButton              = new QToolButton(this);
  QPushButton *nextOpenLevelButton   = new QPushButton(this);
  QToolButton *nextFrameButton       = new QToolButton(this);
  m_previousFrameButton              = new QToolButton(this);
  QPushButton *lastFrameButton       = new QPushButton(this);
  QToolButton *nextXSheetFrameButton = new QToolButton(this);
  m_previousXSheetFrameButton        = new QToolButton(this);
  m_onionOpacityFld                  = new DVGui::IntField(this);
  m_captureButton                    = new QPushButton(tr("Capture"), this);
  // QPushButton *subfolderButton = new QPushButton(tr("Subfolder"), this);
  m_fileTypeCombo->addItems({"jpg", "png", "tga", "tif"});
  m_fileTypeCombo->setCurrentIndex(0);

  fileFrame->setObjectName("CleanupSettingsFrame");
  m_frameNumberEdit->setObjectName("LargeSizedText");
  m_frameInfoLabel->setAlignment(Qt::AlignRight);
  nextLevelButton->setFixedSize(24, 24);
  nextLevelButton->setArrowType(Qt::RightArrow);
  nextLevelButton->setToolTip(tr("Next Level"));
  nextOpenLevelButton->setText(tr("Next New"));
  nextOpenLevelButton->setFixedHeight(28);
  nextOpenLevelButton->setStyleSheet("padding: 0 2;");
  nextOpenLevelButton->setSizePolicy(QSizePolicy::Maximum,
                                     QSizePolicy::Maximum);
  m_previousLevelButton->setFixedSize(24, 24);
  m_previousLevelButton->setArrowType(Qt::LeftArrow);
  m_previousLevelButton->setToolTip(tr("Previous Level"));
  nextFrameButton->setFixedSize(24, 24);
  nextFrameButton->setArrowType(Qt::RightArrow);
  nextFrameButton->setToolTip(tr("Next Frame"));
  lastFrameButton->setText(tr("Last Frame"));
  lastFrameButton->setFixedHeight(28);
  lastFrameButton->setStyleSheet("padding: 0 2;");
  lastFrameButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_previousFrameButton->setFixedSize(24, 24);
  m_previousFrameButton->setArrowType(Qt::LeftArrow);
  m_previousFrameButton->setToolTip(tr("Previous Frame"));

  m_xSheetFrameNumberEdit->setObjectName("LargeSizedText");
  nextXSheetFrameButton->setFixedSize(24, 24);
  nextXSheetFrameButton->setArrowType(Qt::RightArrow);
  nextXSheetFrameButton->setToolTip(tr("Next XSheet Frame"));
  m_previousXSheetFrameButton->setFixedSize(24, 24);
  m_previousXSheetFrameButton->setArrowType(Qt::LeftArrow);
  m_previousXSheetFrameButton->setToolTip(tr("Previous XSheet Frame"));

  m_setToCurrentXSheetFrameButton = new QPushButton(this);
  m_setToCurrentXSheetFrameButton->setText(tr("Current Frame"));
  m_setToCurrentXSheetFrameButton->setFixedHeight(28);
  m_setToCurrentXSheetFrameButton->setSizePolicy(QSizePolicy::Maximum,
                                                 QSizePolicy::Maximum);
  m_setToCurrentXSheetFrameButton->setStyleSheet("padding: 2px;");
  m_setToCurrentXSheetFrameButton->setToolTip(
      tr("Set to the Current Playhead Location"));

  m_onionOpacityFld->setRange(0, 100);
  m_onionOpacityFld->setValue(100);
  m_onionOpacityFld->setDisabled(false);
  m_toggleLiveViewButton = new QPushButton(tr("Start Live View"));
  m_toggleLiveViewButton->setObjectName("LargeSizedText");
  m_toggleLiveViewButton->setFixedHeight(35);
  m_captureButton->setObjectName("LargeSizedText");
  m_captureButton->setFixedHeight(35);
  QCommonStyle style;
  m_captureButton->setIcon(style.standardIcon(QStyle::SP_DialogOkButton));
  m_captureButton->setIconSize(QSize(20, 20));

  // subfolderButton->setObjectName("SubfolderButton");
  // subfolderButton->setIconSize(QSize(15, 15));
  m_saveInFileFld->setMaximumWidth(380);
  m_levelNameEdit->setMaximumWidth(380);

  m_saveInFolderPopup->hide();
  m_zoomButton = new QPushButton(tr("Zoom"), this);
  m_zoomButton->setFixedHeight(28);
  m_zoomButton->setStyleSheet("padding: 0 2;");
  m_zoomButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_pickZoomButton = new QPushButton(tr("Pick Zoom"), this);
  m_pickZoomButton->setStyleSheet("padding: 0 2;");
  m_pickZoomButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_pickZoomButton->setFixedHeight(28);
  m_focusNearButton = new QPushButton(tr("<"), this);
  m_focusNearButton->setFixedSize(32, 28);
  m_focusFarButton = new QPushButton(tr(">"), this);
  m_focusFarButton->setFixedSize(32, 28);
  m_focusNear2Button = new QPushButton(tr("<<"), this);
  m_focusNear2Button->setFixedSize(32, 28);
  m_focusFar2Button = new QPushButton(tr(">>"), this);
  m_focusFar2Button->setFixedSize(32, 28);
  m_focusNear3Button = new QPushButton(tr("<<<"), this);
  m_focusNear3Button->setFixedSize(32, 28);
  m_focusFar3Button = new QPushButton(tr(">>>"), this);
  m_focusFar3Button->setFixedSize(32, 28);
  //*****//****

  QVBoxLayout *controlLayout = new QVBoxLayout();
  controlLayout->setSpacing(0);
  controlLayout->setMargin(5);

  {
    {
      QGridLayout *camLay = new QGridLayout();
      camLay->setMargin(0);
      camLay->setSpacing(3);
      {
        camLay->addWidget(new QLabel(tr("Camera:"), this), 0, 0,
                          Qt::AlignRight);
        camLay->addWidget(m_cameraListCombo, 0, 1, Qt::AlignLeft);
        camLay->addWidget(refreshCamListButton, 0, 2, Qt::AlignLeft);
        if (m_captureFilterSettingsBtn) {
          camLay->addWidget(m_captureFilterSettingsBtn, 0, 3, Qt::AlignLeft);
          camLay->addWidget(m_resolutionLabel, 1, 0, Qt::AlignRight);
          camLay->addWidget(m_resolutionCombo, 1, 1, 1, 3, Qt::AlignLeft);
          camLay->setColumnStretch(3, 30);
        } else {
          camLay->addWidget(m_resolutionLabel, 1, 0, Qt::AlignRight);
          camLay->addWidget(m_resolutionCombo, 1, 1, 1, 2, Qt::AlignLeft);
          camLay->setColumnStretch(2, 30);
        }
      }
      controlLayout->addLayout(camLay, 0);

      QVBoxLayout *fileLay = new QVBoxLayout();
      fileLay->setMargin(8);
      fileLay->setSpacing(5);
      {
        QGridLayout *levelLay = new QGridLayout();
        levelLay->setMargin(0);
        levelLay->setHorizontalSpacing(3);
        levelLay->setVerticalSpacing(5);
        {
          levelLay->addWidget(new QLabel(tr("Name:"), this), 0, 0,
                              Qt::AlignRight);
          QHBoxLayout *nameLay = new QHBoxLayout();
          nameLay->setMargin(0);
          nameLay->setSpacing(2);
          {
            nameLay->addWidget(m_previousLevelButton, 0);
            nameLay->addWidget(m_levelNameEdit, 1);
            nameLay->addWidget(nextLevelButton, 0);
            nameLay->addWidget(nextOpenLevelButton, 0);
          }
          levelLay->addLayout(nameLay, 0, 1);

          levelLay->addWidget(new QLabel(tr("Frame:"), this), 1, 0,
                              Qt::AlignRight);

          QHBoxLayout *frameLay = new QHBoxLayout();
          frameLay->setMargin(0);
          frameLay->setSpacing(2);
          {
            frameLay->addWidget(m_previousFrameButton, 0);
            frameLay->addWidget(m_frameNumberEdit, 1);
            frameLay->addWidget(nextFrameButton, 0);
            frameLay->addWidget(lastFrameButton, 0);
            frameLay->addWidget(m_frameInfoLabel, 1, Qt::AlignVCenter);
          }
          levelLay->addLayout(frameLay, 1, 1);
        }
        levelLay->setColumnStretch(0, 0);
        levelLay->setColumnStretch(1, 1);
        fileLay->addLayout(levelLay, 0);

        QHBoxLayout *fileTypeLay = new QHBoxLayout();
        fileTypeLay->setMargin(0);
        fileTypeLay->setSpacing(3);
        {
          fileTypeLay->addWidget(new QLabel(tr("File Type:"), this), 0);
          fileTypeLay->addWidget(m_fileTypeCombo, 1);
          fileTypeLay->addSpacing(10);
          fileTypeLay->addWidget(m_fileFormatOptionButton);
        }
        fileLay->addLayout(fileTypeLay, 0);

        QHBoxLayout *saveInLay = new QHBoxLayout();
        saveInLay->setMargin(0);
        saveInLay->setSpacing(3);
        {
          saveInLay->addWidget(new QLabel(tr("Save In:"), this), 0);
          saveInLay->addWidget(m_saveInFileFld, 1);
        }
        fileLay->addLayout(saveInLay, 0);
        // fileLay->addWidget(subfolderButton, 0);
      }
      fileFrame->setLayout(fileLay);
      controlLayout->addWidget(fileFrame, 0);

      QGridLayout *displayLay = new QGridLayout();
      displayLay->setMargin(8);
      displayLay->setHorizontalSpacing(3);
      displayLay->setVerticalSpacing(5);
      {
        displayLay->addWidget(new QLabel(tr("XSheet Frame:"), this), 0, 0,
                              Qt::AlignRight);
        QHBoxLayout *xsheetLay = new QHBoxLayout();
        xsheetLay->setMargin(0);
        xsheetLay->setSpacing(2);
        {
          xsheetLay->addWidget(m_previousXSheetFrameButton, Qt::AlignLeft);
          xsheetLay->addWidget(m_xSheetFrameNumberEdit, Qt::AlignLeft);
          xsheetLay->addWidget(nextXSheetFrameButton, Qt::AlignLeft);
          xsheetLay->addWidget(m_setToCurrentXSheetFrameButton,
                               Qt::AlignCenter);
          xsheetLay->addStretch(50);
        }
        displayLay->addLayout(xsheetLay, 0, 1);
      }
      displayLay->setColumnStretch(0, 0);
      displayLay->setColumnStretch(1, 1);
      controlLayout->addLayout(displayLay, 0);
      controlLayout->addStretch(1);
      controlLayout->addSpacing(5);
      controlLayout->addStretch(1);
    }

    m_mainControlsPage->setLayout(controlLayout);

    // Make Settings Page

    m_isoCombo            = new QComboBox(this);
    m_shutterSpeedCombo   = new QComboBox(this);
    m_exposureCombo       = new QComboBox(this);
    m_apertureCombo       = new QComboBox(this);
    m_whiteBalanceCombo   = new QComboBox(this);
    m_kelvinCombo         = new QComboBox(this);
    m_imageQualityCombo   = new QComboBox(this);
    m_pictureStyleCombo   = new QComboBox(this);
    m_cameraSettingsLabel = new QLabel(tr("Camera Model"), this);
    m_cameraModeLabel     = new QLabel(tr("Camera Mode"), this);
    m_kelvinLabel         = new QLabel(tr("Temperature: "), this);
    m_isoCombo->setFixedWidth(fontMetrics().width("000000") + 25);
    m_shutterSpeedCombo->setFixedWidth(fontMetrics().width("000000") + 25);
    m_apertureCombo->setFixedWidth(fontMetrics().width("000000") + 25);
    m_exposureCombo->setFixedWidth(fontMetrics().width("000000") + 25);
    QVBoxLayout *settingsLayout = new QVBoxLayout;
    settingsLayout->setSpacing(0);
    settingsLayout->setMargin(5);

    QGridLayout *settingsGridLayout = new QGridLayout;
    {
      settingsGridLayout->setMargin(0);
      settingsGridLayout->setSpacing(3);
      settingsGridLayout->addWidget(m_cameraSettingsLabel, 0, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_cameraModeLabel, 1, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(tr("Shutter Speed: ")), 2, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_shutterSpeedCombo, 2, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Iso: ")), 3, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_isoCombo, 3, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Aperture: ")), 4, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_apertureCombo, 4, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Exposure: ")), 5, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_exposureCombo, 5, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Image Quality: ")), 6, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_imageQualityCombo, 6, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Picture Style: ")), 7, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_pictureStyleCombo, 7, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("White Balance: ")), 8, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_whiteBalanceCombo, 8, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(m_kelvinLabel, 9, 0, Qt::AlignRight);
      settingsGridLayout->addWidget(m_kelvinCombo, 9, 1, Qt::AlignLeft);

      settingsGridLayout->setColumnStretch(1, 30);
    }
    settingsLayout->addLayout(settingsGridLayout, 0);
    m_focusAndZoomLayout = new QHBoxLayout;
    m_focusAndZoomLayout->addStretch();
    m_focusAndZoomLayout->addWidget(m_focusNear3Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusNear2Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusNearButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_zoomButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_pickZoomButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusFarButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusFar2Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusFar3Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addStretch();
    // settingsLayout->addStretch();
    settingsLayout->addLayout(m_focusAndZoomLayout);
    settingsLayout->addStretch();
    m_cameraSettingsPage->setLayout(settingsLayout);

    // Make Options Page
    QGroupBox *webcamBox   = new QGroupBox(tr("Webcam Options"), this);
    QGroupBox *dslrBox     = new QGroupBox(tr("DSLR Options"), this);
    m_postCaptureReviewFld = new DVGui::IntField(this);
    m_postCaptureReviewFld->setRange(0, 10);

    m_subsamplingFld = new DVGui::IntField(this);
    m_subsamplingFld->setRange(1, 30);
    m_subsamplingFld->setDisabled(true);
    m_blackScreenForCapture = new QCheckBox(this);

    m_placeOnXSheetCB = new QCheckBox(this);
    m_placeOnXSheetCB->setToolTip(tr("Place the frame in the XSheet"));

    m_useScaledFullSizeImages = new QCheckBox(this);
    m_directShowLabel = new QLabel(tr("Use Direct Show Webcam Drivers"), this);
    m_directShowCB    = new QCheckBox(this);
    m_useMjpgCB       = new QCheckBox(this);
    m_useNumpadCB     = new QCheckBox(this);

    m_liveViewOnAllFramesCB           = new QCheckBox(this);
    QVBoxLayout *optionsOutsideLayout = new QVBoxLayout;
    QGridLayout *optionsLayout        = new QGridLayout;
    optionsLayout->setSpacing(3);
    optionsLayout->setMargin(5);
    QGridLayout *webcamLayout   = new QGridLayout;
    QGridLayout *dslrLayout     = new QGridLayout;
    QGridLayout *checkboxLayout = new QGridLayout;

    dslrLayout->addWidget(m_blackScreenForCapture, 0, 0, Qt::AlignRight);
    dslrLayout->addWidget(new QLabel(tr("Black Screen for Capture")), 0, 1,
                          Qt::AlignLeft);
    dslrLayout->addWidget(m_useScaledFullSizeImages, 1, 0, Qt::AlignRight);
    dslrLayout->addWidget(new QLabel(tr("Use Reduced Resolution Images")), 1, 1,
                          Qt::AlignLeft);
    dslrLayout->setColumnStretch(1, 30);
    dslrBox->setLayout(dslrLayout);
    dslrBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    optionsOutsideLayout->addWidget(dslrBox, Qt::AlignCenter);

    webcamLayout->addWidget(m_directShowCB, 0, 0, Qt::AlignRight);
    webcamLayout->addWidget(m_directShowLabel, 0, 1, Qt::AlignLeft);
    webcamLayout->addWidget(m_useMjpgCB, 1, 0, Qt::AlignRight);
    webcamLayout->addWidget(new QLabel(tr("Use MJPG with Webcam")), 1, 1,
                            Qt::AlignLeft);
    webcamLayout->setColumnStretch(1, 30);
    webcamBox->setLayout(webcamLayout);
    webcamBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    optionsOutsideLayout->addWidget(webcamBox, Qt::AlignCenter);

    checkboxLayout->addWidget(m_placeOnXSheetCB, 0, 0, 1, 1, Qt::AlignRight);
    checkboxLayout->addWidget(new QLabel(tr("Place on XSheet")), 0, 1,
                              Qt::AlignLeft);
    checkboxLayout->addWidget(m_useNumpadCB, 1, 0, Qt::AlignRight);
    checkboxLayout->addWidget(
        new QLabel(tr("Use Numpad Shortcuts When Active")), 1, 1,
        Qt::AlignLeft);
    checkboxLayout->addWidget(m_liveViewOnAllFramesCB, 2, 0, Qt::AlignRight);
    checkboxLayout->addWidget(new QLabel(tr("Show Live View on All Frames")), 2,
                              1, Qt::AlignLeft);

    checkboxLayout->setColumnStretch(1, 30);
    optionsOutsideLayout->addLayout(checkboxLayout, Qt::AlignLeft);

    optionsLayout->addWidget(new QLabel(tr("Capture Review Time: ")), 0, 0,
                             Qt::AlignRight);
    optionsLayout->addWidget(m_postCaptureReviewFld, 0, 1);
    optionsLayout->addWidget(new QLabel(tr("Level Subsampling: ")), 1, 0,
                             Qt::AlignRight);
    optionsLayout->addWidget(m_subsamplingFld, 1, 1);
    optionsLayout->setColumnStretch(1, 30);
    optionsLayout->setRowStretch(2, 30);
    optionsOutsideLayout->addLayout(optionsLayout, Qt::AlignLeft);
    optionsOutsideLayout->addStretch();

    m_optionsPage->setLayout(optionsOutsideLayout);

    QScrollArea *mainArea = makeChooserPageWithoutScrollBar(m_mainControlsPage);
    QScrollArea *settingsArea =
        makeChooserPageWithoutScrollBar(m_cameraSettingsPage);
    QScrollArea *optionsArea = makeChooserPageWithoutScrollBar(m_optionsPage);

    m_stackedChooser = new QStackedWidget(this);
    m_stackedChooser->addWidget(mainArea);
    m_stackedChooser->addWidget(settingsArea);
    m_stackedChooser->addWidget(optionsArea);
    m_stackedChooser->setFocusPolicy(Qt::NoFocus);

    QFrame *opacityFrame    = new QFrame();
    QHBoxLayout *opacityLay = new QHBoxLayout();
    opacityLay->addWidget(new QLabel(tr("Opacity:"), this), 0);
    opacityLay->addWidget(m_onionOpacityFld, 1);
    opacityFrame->setLayout(opacityLay);
    opacityFrame->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);

    QFrame *controlButtonFrame    = new QFrame();
    QHBoxLayout *controlButtonLay = new QHBoxLayout();
    controlButtonLay->addWidget(m_captureButton, 0);
    controlButtonLay->addWidget(m_toggleLiveViewButton, 0);
    controlButtonFrame->setLayout(controlButtonLay);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    {
      QHBoxLayout *hLayout = new QHBoxLayout;
      hLayout->setMargin(0);
      {
        hLayout->addSpacing(4);
        hLayout->addWidget(m_tabBar);
        hLayout->addStretch();
      }
      m_tabBarContainer->setLayout(hLayout);

      mainLayout->addWidget(m_tabBarContainer, 0, 0);
      mainLayout->addWidget(m_stackedChooser, 1, 0);
      mainLayout->addWidget(opacityFrame, 0);
      mainLayout->addWidget(controlButtonFrame, 0);
      setLayout(mainLayout);
      m_tabBarContainer->layout()->update();
    }
  }

  TSceneHandle *sceneHandle   = TApp::instance()->getCurrentScene();
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();

  bool ret = true;

  // Outside Connections
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));
  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetSwitched()), this, SLOT(update()));

  // UI SIGNALS
  ret = ret && connect(m_tabBar, SIGNAL(currentChanged(int)), this,
                       SLOT(setPage(int)));

  // Control Page
  ret = ret && connect(refreshCamListButton, SIGNAL(pressed()), this,
                       SLOT(refreshCameraListCalled()));
  ret = ret && connect(m_cameraListCombo, SIGNAL(activated(int)), this,
                       SLOT(onCameraListComboActivated(int)));
  ret = ret && connect(m_resolutionCombo, SIGNAL(activated(const QString &)),
                       this, SLOT(onResolutionComboActivated(const QString &)));
  if (m_captureFilterSettingsBtn)
    ret = ret && connect(m_captureFilterSettingsBtn, SIGNAL(pressed()), this,
                         SLOT(onCaptureFilterSettingsBtnPressed()));
  ret = ret && connect(m_fileFormatOptionButton, SIGNAL(pressed()), this,
                       SLOT(onFileFormatOptionButtonPressed()));
  ret = ret && connect(m_levelNameEdit, SIGNAL(levelNameEdited()), this,
                       SLOT(onLevelNameEdited()));
  ret = ret &&
        connect(nextLevelButton, SIGNAL(pressed()), this, SLOT(onNextName()));
  ret = ret && connect(m_previousLevelButton, SIGNAL(pressed()), this,
                       SLOT(onPreviousName()));
  ret = ret && connect(nextOpenLevelButton, SIGNAL(pressed()), this,
                       SLOT(onNextNewLevel()));
  ret = ret &&
        connect(nextFrameButton, SIGNAL(pressed()), this, SLOT(onNextFrame()));
  ret = ret &&
        connect(lastFrameButton, SIGNAL(pressed()), this, SLOT(onLastFrame()));
  ret = ret && connect(m_previousFrameButton, SIGNAL(pressed()), this,
                       SLOT(onPreviousFrame()));
  ret = ret && connect(nextXSheetFrameButton, SIGNAL(pressed()), this,
                       SLOT(onNextXSheetFrame()));
  ret = ret && connect(m_previousXSheetFrameButton, SIGNAL(pressed()), this,
                       SLOT(onPreviousXSheetFrame()));
  ret = ret && connect(m_setToCurrentXSheetFrameButton, SIGNAL(pressed()), this,
                       SLOT(setToCurrentXSheetFrame()));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onOnionOpacityFldEdited()));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onOnionOpacitySliderChanged(bool)));
  ret = ret && connect(m_captureButton, SIGNAL(clicked(bool)), this,
                       SLOT(onCaptureButtonClicked(bool)));
  // ret = ret && connect(subfolderButton, SIGNAL(clicked(bool)), this,
  //                     SLOT(openSaveInFolderPopup()));
  ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onSaveInPathEdited()));
  ret = ret && connect(m_fileTypeCombo, SIGNAL(activated(int)), this,
                       SLOT(onFileTypeActivated()));
  ret = ret && connect(m_frameNumberEdit, SIGNAL(editingFinished()), this,
                       SLOT(onFrameNumberChanged()));
  ret = ret && connect(m_xSheetFrameNumberEdit, SIGNAL(editingFinished()), this,
                       SLOT(onXSheetFrameNumberChanged()));
  ret = ret && connect(m_toggleLiveViewButton, SIGNAL(clicked()), this,
                       SLOT(onLiveViewToggleClicked()));
  ret = ret && connect(m_stopMotion, SIGNAL(filePathChanged(QString)), this,
                       SLOT(onFilePathChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(levelNameChanged(QString)), this,
                       SLOT(onLevelNameChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(fileTypeChanged(QString)), this,
                       SLOT(onFileTypeChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(frameInfoTextChanged(QString)),
                       this, SLOT(onFrameInfoTextChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(xSheetFrameNumberChanged(int)),
                       this, SLOT(onXSheetFrameNumberChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(frameNumberChanged(int)), this,
                       SLOT(onFrameNumberChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(opacityChanged(int)), this,
                       SLOT(onOpacityChanged(int)));

  // Options Page
  ret = ret && connect(m_useScaledFullSizeImages, SIGNAL(stateChanged(int)),
                       this, SLOT(onScaleFullSizeImagesChanged(int)));
  ret = ret && connect(m_liveViewOnAllFramesCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onLiveViewOnAllFramesChanged(int)));
  ret = ret && connect(m_blackScreenForCapture, SIGNAL(stateChanged(int)), this,
                       SLOT(onBlackScreenForCaptureChanged(int)));
  ret = ret && connect(m_placeOnXSheetCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onPlaceOnXSheetChanged(int)));
  ret = ret && connect(m_directShowCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseDirectShowChanged(int)));
  ret = ret && connect(m_useMjpgCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseMjpgChanged(int)));
  ret = ret && connect(m_useNumpadCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseNumpadChanged(int)));
  ret = ret && connect(m_postCaptureReviewFld, SIGNAL(valueEditedByHand()),
                       this, SLOT(onCaptureReviewFldEdited()));
  ret = ret && connect(m_postCaptureReviewFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onCaptureReviewSliderChanged(bool)));
  ret = ret && connect(m_subsamplingFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onSubsamplingFldEdited()));
  ret = ret && connect(m_subsamplingFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onSubsamplingSliderChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(subsamplingChanged(int)), this,
                       SLOT(onSubsamplingChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(scaleFullSizeImagesSignal(bool)),
                       this, SLOT(onScaleFullSizeImagesSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(blackCaptureSignal(bool)), this,
                       SLOT(onBlackCaptureSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(liveViewOnAllFramesSignal(bool)),
                       this, SLOT(onLiveViewOnAllFramesSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(placeOnXSheetSignal(bool)), this,
                       SLOT(onPlaceOnXSheetSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(useDirectShowSignal(bool)), this,
                       SLOT(onUseDirectShowSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(useMjpgSignal(bool)), this,
                       SLOT(onUseMjpgSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(useNumpadSignal(bool)), this,
                       SLOT(onUseNumpadSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(reviewTimeChangedSignal(int)), this,
                       SLOT(onReviewTimeChangedSignal(int)));

  // From Stop Motion Main
  ret = ret && connect(m_stopMotion, SIGNAL(newDimensions()), this,
                       SLOT(updateDimensions()));
  ret = ret && connect(m_stopMotion, SIGNAL(updateCameraList()), this,
                       SLOT(refreshCameraList()));
  ret = ret && connect(m_stopMotion, SIGNAL(liveViewChanged(bool)), this,
                       SLOT(onLiveViewChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(newCameraSelected(int, bool)), this,
                       SLOT(onNewCameraSelected(int, bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(cameraChanged()), this,
                       SLOT(refreshCameraList()));
  ret = ret && connect(m_stopMotion, SIGNAL(optionsChanged()), this,
                       SLOT(refreshOptionsLists()));

  // EOS Connections
  ret = ret &&
        connect(m_zoomButton, SIGNAL(pressed()), this, SLOT(onZoomPressed()));
  ret = ret && connect(m_pickZoomButton, SIGNAL(pressed()), this,
                       SLOT(onPickZoomPressed()));
  ret = ret && connect(m_focusNearButton, SIGNAL(pressed()), this,
                       SLOT(onFocusNear()));
  ret = ret &&
        connect(m_focusFarButton, SIGNAL(pressed()), this, SLOT(onFocusFar()));
  ret = ret && connect(m_focusNear2Button, SIGNAL(pressed()), this,
                       SLOT(onFocusNear2()));
  ret = ret && connect(m_focusFar2Button, SIGNAL(pressed()), this,
                       SLOT(onFocusFar2()));
  ret = ret && connect(m_focusNear3Button, SIGNAL(pressed()), this,
                       SLOT(onFocusNear3()));
  ret = ret && connect(m_focusFar3Button, SIGNAL(pressed()), this,
                       SLOT(onFocusFar3()));
  ret = ret && connect(m_stopMotion, SIGNAL(apertureChangedSignal(QString)),
                       this, SLOT(onApertureChangedSignal(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(isoChangedSignal(QString)), this,
                       SLOT(onIsoChangedSignal(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(shutterSpeedChangedSignal(QString)),
                       this, SLOT(onShutterSpeedChangedSignal(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(exposureChangedSignal(QString)),
                       this, SLOT(onExposureChangedSignal(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(whiteBalanceChangedSignal(QString)),
                       this, SLOT(onWhiteBalanceChangedSignal(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(imageQualityChangedSignal(QString)),
                       this, SLOT(onImageQualityChangedSignal(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(pictureStyleChangedSignal(QString)),
                       this, SLOT(onPictureStyleChangedSignal(QString)));
  ret = ret &&
        connect(m_stopMotion, SIGNAL(colorTemperatureChangedSignal(QString)),
                this, SLOT(onColorTemperatureChangedSignal(QString)));
  ret = ret && connect(m_apertureCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onApertureChanged(int)));
  ret = ret && connect(m_shutterSpeedCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onShutterSpeedChanged(int)));
  ret = ret && connect(m_isoCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onIsoChanged(int)));
  ret = ret && connect(m_exposureCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onExposureChanged(int)));
  ret = ret && connect(m_whiteBalanceCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onWhiteBalanceChanged(int)));
  ret = ret && connect(m_kelvinCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onColorTemperatureChanged(int)));
  ret = ret && connect(m_imageQualityCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onImageQualityChanged(int)));
  ret = ret && connect(m_pictureStyleCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onPictureStyleChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(apertureOptionsChanged()), this,
                       SLOT(refreshApertureList()));
  ret = ret && connect(m_stopMotion, SIGNAL(shutterSpeedOptionsChanged()), this,
                       SLOT(refreshShutterSpeedList()));
  ret = ret && connect(m_stopMotion, SIGNAL(isoOptionsChanged()), this,
                       SLOT(refreshIsoList()));
  ret = ret && connect(m_stopMotion, SIGNAL(exposureOptionsChanged()), this,
                       SLOT(refreshExposureList()));
  ret = ret && connect(m_stopMotion, SIGNAL(whiteBalanceOptionsChanged()), this,
                       SLOT(refreshWhiteBalanceList()));
  ret = ret && connect(m_stopMotion, SIGNAL(imageQualityOptionsChanged()), this,
                       SLOT(refreshImageQualityList()));
  ret = ret && connect(m_stopMotion, SIGNAL(pictureStyleOptionsChanged()), this,
                       SLOT(refreshPictureStyleList()));
  ret = ret &&
        connect(m_stopMotion, SIGNAL(modeChanged()), this, SLOT(refreshMode()));

  // Webcam Specific Connections
  ret = ret && connect(m_stopMotion, SIGNAL(webcamResolutionsChanged()), this,
                       SLOT(onWebcamResolutionsChanged()));
  ret = ret && connect(m_stopMotion, SIGNAL(newWebcamResolutionSelected(int)),
                       this, SLOT(onNewWebcamResolutionSelected(int)));

  assert(ret);

  m_placeOnXSheetCB->setChecked(
      m_stopMotion->getPlaceOnXSheet() == true ? true : false);
  m_useScaledFullSizeImages->setChecked(m_stopMotion->m_useScaledImages);
  m_onionOpacityFld->setValue(double(100 * m_stopMotion->getOpacity()) / 255.0);
  m_directShowCB->setChecked(m_stopMotion->getUseDirectShow());
  m_useMjpgCB->setChecked(m_stopMotion->getUseMjpg());
  m_useNumpadCB->setChecked(m_stopMotion->getUseNumpadShortcuts());
  m_liveViewOnAllFramesCB->setChecked(m_stopMotion->getAlwaysLiveView());
  m_blackScreenForCapture->setChecked(
      m_stopMotion->getBlackCapture() == true ? true : false);
  m_postCaptureReviewFld->setValue(m_stopMotion->getReviewTime());

  refreshCameraList();
  onSceneSwitched();
  m_stopMotion->setToNextNewLevel();
  m_saveInFileFld->setPath(m_stopMotion->getFilePath());
}

//-----------------------------------------------------------------------------

StopMotionController::~StopMotionController() {}

//-----------------------------------------------------------------------------

void StopMotionController::setPage(int index) {
  if (m_stopMotion->m_usingWebcam && index > 0) index += 1;
  m_stackedChooser->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScaleFullSizeImagesChanged(int checked) {
  m_stopMotion->setUseScaledImages(checked > 0 ? true : false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScaleFullSizeImagesSignal(bool on) {
  m_useScaledFullSizeImages->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewOnAllFramesChanged(int checked) {
  m_stopMotion->setAlwaysLiveView(checked > 0 ? true : false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewOnAllFramesSignal(bool on) {
  m_liveViewOnAllFramesCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onBlackScreenForCaptureChanged(int checked) {
  m_stopMotion->setBlackCapture(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onBlackCaptureSignal(bool on) {
  m_blackScreenForCapture->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlaceOnXSheetChanged(int checked) {
  m_stopMotion->setPlaceOnXSheet(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlaceOnXSheetSignal(bool on) {
  m_placeOnXSheetCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseDirectShowChanged(int checked) {
  m_stopMotion->setUseDirectShow(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseDirectShowSignal(bool on) {
  m_directShowCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseMjpgChanged(int checked) {
  m_stopMotion->setUseMjpg(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseMjpgSignal(bool on) {
  m_useMjpgCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseNumpadChanged(int checked) {
  m_stopMotion->setUseNumpadShortcuts(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseNumpadSignal(bool on) {
  m_useNumpadCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureReviewFldEdited() {
  m_stopMotion->setReviewTime(m_postCaptureReviewFld->getValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureReviewSliderChanged(bool ignore) {
  m_stopMotion->setReviewTime(m_postCaptureReviewFld->getValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onReviewTimeChangedSignal(int time) {
  m_postCaptureReviewFld->setValue(time);
}

//-----------------------------------------------------------------------------

void StopMotionController::onSubsamplingChanged(int subsampling) {
  if (subsampling < 1) {
    m_subsamplingFld->setValue(1);
    m_subsamplingFld->setDisabled(true);
  } else {
    m_subsamplingFld->setValue(subsampling);
    m_subsamplingFld->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameNumberChanged(int frameNumber) {
  m_frameNumberEdit->setValue(frameNumber);
  m_previousFrameButton->setDisabled(frameNumber == 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::onXSheetFrameNumberChanged(int frameNumber) {
  m_xSheetFrameNumberEdit->setValue(frameNumber);
}

//-----------------------------------------------------------------------------

void StopMotionController::onFilePathChanged(QString filePath) {
  m_saveInFileFld->setPath(filePath);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLevelNameChanged(QString levelName) {
  m_levelNameEdit->setText(levelName);
}

//-----------------------------------------------------------------------------

void StopMotionController::onFileTypeChanged(QString fileType) {
  m_fileTypeCombo->setCurrentText(fileType);
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameInfoTextChanged(QString infoText) {
  m_frameInfoLabel->setText(infoText);
  m_frameInfoLabel->setStyleSheet(QString("QLabel{color: %1;}\
                                          QLabel QWidget{ color: black;}")
                                      .arg(m_stopMotion->getInfoColorName()));
  m_frameInfoLabel->setToolTip(m_stopMotion->getFrameInfoToolTip());
}

//-----------------------------------------------------------------------------

void StopMotionController::onSubsamplingFldEdited() {
  m_stopMotion->setSubsamplingValue(m_subsamplingFld->getValue());
  m_stopMotion->setSubsampling();
}

//-----------------------------------------------------------------------------

void StopMotionController::onSubsamplingSliderChanged(bool ignore) {
  m_stopMotion->setSubsamplingValue(m_subsamplingFld->getValue());
  m_stopMotion->setSubsampling();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshCameraListCalled() {
  m_stopMotion->refreshCameraList();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshCameraList() {
  m_cameraListCombo->clear();

  QList<QCameraInfo> webcams = m_stopMotion->getWebcams();

  int count = m_stopMotion->getCameraCount() + webcams.count();
  if (count < 1) {
    m_cameraListCombo->addItem(tr("No camera detected."));
    m_cameraSettingsLabel->setText(tr("No camera detected"));
    m_cameraModeLabel->setText("");
    m_cameraListCombo->setDisabled(true);
    m_captureButton->setDisabled(true);
    m_toggleLiveViewButton->setDisabled(true);
    m_toggleLiveViewButton->setText(tr("Start Live View"));
  } else {
    int maxTextLength = 0;
    m_cameraListCombo->addItem(tr("- Select camera -"));
    if (webcams.count() > 0) {
      for (int c = 0; c < webcams.size(); c++) {
        QString camDesc  = webcams.at(c).description();
        m_cameraListCombo->addItem(camDesc);
        maxTextLength = std::max(maxTextLength, fontMetrics().width(camDesc));
      }
    }
    if (m_stopMotion->getCameraCount() > 0) {
      QString name;
      m_stopMotion->getCamera(0);
      m_stopMotion->openCameraSession();
      name = QString::fromStdString(m_stopMotion->getCameraName());
      m_stopMotion->closeCameraSession();
      m_cameraSettingsLabel->setText(name);
      m_cameraListCombo->addItem(name);
      maxTextLength = std::max(maxTextLength, fontMetrics().width(name));
    }
    m_cameraListCombo->setMaximumWidth(maxTextLength + 25);
    m_cameraListCombo->setEnabled(true);
    m_cameraListCombo->setCurrentIndex(0);
    m_captureButton->setEnabled(true);
    m_toggleLiveViewButton->setEnabled(true);
  }
  m_stopMotion->updateLevelNameAndFrame(m_levelNameEdit->text().toStdWString());
  refreshOptionsLists();
  refreshMode();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshOptionsLists() {
  m_apertureCombo->blockSignals(true);
  m_isoCombo->blockSignals(true);
  m_shutterSpeedCombo->blockSignals(true);
  m_exposureCombo->blockSignals(true);
  m_whiteBalanceCombo->blockSignals(true);
  m_kelvinCombo->blockSignals(true);
  m_imageQualityCombo->blockSignals(true);
  m_pictureStyleCombo->blockSignals(true);

  m_isoCombo->clear();
  m_shutterSpeedCombo->clear();
  m_apertureCombo->clear();
  m_exposureCombo->clear();

  if (m_stopMotion->getCameraCount() == 0) {
    m_resolutionCombo->setDisabled(true);
    m_shutterSpeedCombo->setDisabled(true);
    m_isoCombo->setDisabled(true);
    m_apertureCombo->setDisabled(true);
    m_exposureCombo->setDisabled(true);
    m_whiteBalanceCombo->setDisabled(true);
    m_kelvinCombo->setDisabled(true);
    m_imageQualityCombo->setDisabled(true);
    m_pictureStyleCombo->setDisabled(true);
    return;
  }

  refreshApertureList();
  refreshShutterSpeedList();
  refreshIsoList();
  refreshExposureList();
  refreshWhiteBalanceList();
  refreshImageQualityList();
  refreshPictureStyleList();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshMode() {
  if (m_stopMotion->getCameraCount() == 0) {
    m_cameraModeLabel->setText("");
    return;
  }
  QString mode = m_stopMotion->getMode();
  m_cameraModeLabel->setText(tr("Mode: ") + mode);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshApertureList() {
  m_apertureCombo->blockSignals(true);
  m_apertureCombo->clear();
  m_stopMotion->getAvailableApertures();
  m_apertureCombo->addItems(m_stopMotion->getApertureOptions());

  if (m_apertureCombo->count() == 0) {
    m_apertureCombo->addItem(tr("Auto"));
    m_apertureCombo->setDisabled(true);
  } else {
    m_apertureCombo->setEnabled(true);
    m_apertureCombo->setCurrentText(m_stopMotion->getCurrentAperture());
  }
  m_apertureCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshShutterSpeedList() {
  m_shutterSpeedCombo->blockSignals(true);
  m_shutterSpeedCombo->clear();
  m_stopMotion->getAvailableShutterSpeeds();
  m_shutterSpeedCombo->addItems(m_stopMotion->getShutterSpeedOptions());

  if (m_shutterSpeedCombo->count() == 0) {
    m_shutterSpeedCombo->addItem(tr("Auto"));
    m_shutterSpeedCombo->setDisabled(true);
  } else {
    m_shutterSpeedCombo->setEnabled(true);
    m_shutterSpeedCombo->setCurrentText(m_stopMotion->getCurrentShutterSpeed());
  }
  m_shutterSpeedCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshIsoList() {
  m_isoCombo->blockSignals(true);
  m_isoCombo->clear();
  m_stopMotion->getAvailableIso();
  m_isoCombo->addItems(m_stopMotion->getIsoOptions());

  if (m_isoCombo->count() == 0) {
    m_isoCombo->addItem(tr("Auto"));
    m_isoCombo->setDisabled(true);
  } else {
    m_isoCombo->setEnabled(true);
    m_isoCombo->setCurrentText(m_stopMotion->getCurrentIso());
  }
  m_isoCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshExposureList() {
  m_exposureCombo->blockSignals(true);
  m_exposureCombo->clear();
  m_stopMotion->getAvailableExposureCompensations();
  m_exposureCombo->addItems(m_stopMotion->getExposureOptions());

  if (m_exposureCombo->count() == 0) {
    m_exposureCombo->addItem(tr("Disabled"));
    m_exposureCombo->setDisabled(true);
  } else {
    m_exposureCombo->setEnabled(true);
    m_exposureCombo->setCurrentText(
        m_stopMotion->getCurrentExposureCompensation());
  }
  m_exposureCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshWhiteBalanceList() {
  m_whiteBalanceCombo->blockSignals(true);
  m_whiteBalanceCombo->clear();
  m_stopMotion->getAvailableWhiteBalances();
  m_whiteBalanceCombo->addItems(m_stopMotion->getWhiteBalanceOptions());

  if (m_whiteBalanceCombo->count() == 0) {
    m_whiteBalanceCombo->addItem(tr("Disabled"));
    m_whiteBalanceCombo->setDisabled(true);
  } else {
    m_whiteBalanceCombo->setEnabled(true);
    m_whiteBalanceCombo->setCurrentText(m_stopMotion->getCurrentWhiteBalance());
  }
  m_whiteBalanceCombo->blockSignals(false);
  refreshColorTemperatureList();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshColorTemperatureList() {
  m_kelvinCombo->blockSignals(true);
  m_kelvinCombo->clear();
  m_kelvinCombo->addItems(m_stopMotion->getColorTemperatureOptions());
  if (m_kelvinCombo->count() == 0 ||
      m_stopMotion->getCurrentWhiteBalance() != "Color Temperature") {
    // m_kelvinCombo->addItem(tr("Disabled"));
    m_kelvinCombo->setDisabled(true);
    m_kelvinCombo->hide();
    m_kelvinLabel->hide();
  } else {
    m_kelvinCombo->show();
    m_kelvinLabel->show();
    m_kelvinCombo->setEnabled(true);
    m_kelvinCombo->setCurrentText(m_stopMotion->getCurrentColorTemperature());
  }
  m_kelvinCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshImageQualityList() {
  m_imageQualityCombo->blockSignals(true);
  m_imageQualityCombo->clear();
  m_stopMotion->getAvailableImageQualities();
  m_imageQualityCombo->addItems(m_stopMotion->getImageQualityOptions());

  if (m_imageQualityCombo->count() == 0) {
    m_imageQualityCombo->addItem(tr("Disabled"));
    m_imageQualityCombo->setDisabled(true);
  } else {
    m_imageQualityCombo->setEnabled(true);
    m_imageQualityCombo->setCurrentText(m_stopMotion->getCurrentImageQuality());
  }
  m_imageQualityCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshPictureStyleList() {
  m_pictureStyleCombo->blockSignals(true);
  m_pictureStyleCombo->clear();
  m_stopMotion->getAvailablePictureStyles();
  m_pictureStyleCombo->addItems(m_stopMotion->getPictureStyleOptions());

  if (m_pictureStyleCombo->count() == 0) {
    m_pictureStyleCombo->addItem(tr("Disabled"));
    m_pictureStyleCombo->setDisabled(true);
  } else {
    m_pictureStyleCombo->setEnabled(true);
    m_pictureStyleCombo->setCurrentText(m_stopMotion->getCurrentPictureStyle());
  }
  m_pictureStyleCombo->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCameraListComboActivated(int comboIndex) {
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  if (cameras.size() + m_stopMotion->getCameraCount() !=
      m_cameraListCombo->count() - 1)
    return;

  m_stopMotion->changeCameras(comboIndex);
}

//-----------------------------------------------------------------------------

void StopMotionController::onNewCameraSelected(int index, bool useWebcam) {
  if (index < m_cameraListCombo->count())
    m_cameraListCombo->setCurrentIndex(index);
  if (index == 0) {
    m_cameraListCombo->setCurrentIndex(index);
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
  }
  if (useWebcam) {
    if (m_tabBar->tabText(1) == tr("Settings")) {
      m_tabBar->removeTab(1);
    }
    m_resolutionCombo->show();
    m_resolutionCombo->setEnabled(true);
    m_resolutionLabel->show();
    m_captureFilterSettingsBtn->show();
  } else {
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
    m_captureFilterSettingsBtn->hide();
    if (m_tabBar->tabText(1) == tr("Options")) {
      m_tabBar->insertTab(1, tr("Settings"));
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamResolutionsChanged() {
  m_resolutionCombo->clear();
  QList<QSize> resolutions = m_stopMotion->getWebcamResolutions();
  for (int s = 0; s < resolutions.size(); s++) {
    m_resolutionCombo->addItem(QString("%1 x %2")
                                   .arg(resolutions.at(s).width())
                                   .arg(resolutions.at(s).height()));
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onNewWebcamResolutionSelected(int index) {
  m_resolutionCombo->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onResolutionComboActivated(const QString &itemText) {
  m_stopMotion->setWebcamResolution(itemText);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureFilterSettingsBtnPressed() {
  if (!m_stopMotion->getWebcam() || m_stopMotion->m_webcamDeviceName.isNull())
    return;

  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  for (int c = 0; c < cameras.size(); c++) {
    if (cameras.at(c).deviceName() == m_stopMotion->m_webcamDeviceName) {
#ifdef _WIN32
      openCaptureFilterSettings(this, cameras.at(c).description());
#endif
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onFileFormatOptionButtonPressed() {
  if (m_fileTypeCombo->currentIndex() == 0) return;
  // Tentatively use the preview output settings
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties *prop = scene->getProperties()->getPreviewProperties();
  std::string ext         = m_fileTypeCombo->currentText().toStdString();
  openFormatSettingsPopup(this, ext, prop->getFileFormatProperties(ext));
}

//-----------------------------------------------------------------------------

void StopMotionController::onLevelNameEdited() {
  m_stopMotion->updateLevelNameAndFrame(m_levelNameEdit->text().toStdWString());
}

//-----------------------------------------------------------------------------
void StopMotionController::onNextName() { m_stopMotion->nextName(); }

//-----------------------------------------------------------------------------

void StopMotionController::onNextNewLevel() {
  m_stopMotion->setToNextNewLevel();
}

//-----------------------------------------------------------------------------

void StopMotionController::onPreviousName() { m_stopMotion->previousName(); }

//-----------------------------------------------------------------------------

void StopMotionController::onNextFrame() { m_stopMotion->nextFrame(); }

//-----------------------------------------------------------------------------

void StopMotionController::onLastFrame() {}

//-----------------------------------------------------------------------------

void StopMotionController::onPreviousFrame() { m_stopMotion->previousFrame(); }

//-----------------------------------------------------------------------------

void StopMotionController::onNextXSheetFrame() {
  m_stopMotion->setXSheetFrameNumber(m_stopMotion->getXSheetFrameNumber() + 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPreviousXSheetFrame() {
  m_stopMotion->setXSheetFrameNumber(m_stopMotion->getXSheetFrameNumber() - 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::setToCurrentXSheetFrame() {
  int frameNumber = TApp::instance()->getCurrentFrame()->getFrame() + 1;
  m_stopMotion->setXSheetFrameNumber(frameNumber);
}

//-----------------------------------------------------------------------------

void StopMotionController::updateDimensions() {
  m_stopMotion->refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameCaptured(QImage &image) {}

//-----------------------------------------------------------------------------

void StopMotionController::onApertureChanged(int index) {
  m_stopMotion->setAperture(m_apertureCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onApertureChangedSignal(QString text) {
  m_apertureCombo->setCurrentText(m_stopMotion->getCurrentAperture());
}

//-----------------------------------------------------------------------------

void StopMotionController::onShutterSpeedChanged(int index) {
  m_stopMotion->setShutterSpeed(m_shutterSpeedCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onShutterSpeedChangedSignal(QString text) {
  m_shutterSpeedCombo->setCurrentText(m_stopMotion->getCurrentShutterSpeed());
}

//-----------------------------------------------------------------------------

void StopMotionController::onIsoChanged(int index) {
  m_stopMotion->setIso(m_isoCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onIsoChangedSignal(QString text) {
  m_isoCombo->setCurrentText(m_stopMotion->getCurrentIso());
}

//-----------------------------------------------------------------------------

void StopMotionController::onExposureChanged(int index) {
  m_stopMotion->setExposureCompensation(m_exposureCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onExposureChangedSignal(QString text) {
  m_exposureCombo->setCurrentText(
      m_stopMotion->getCurrentExposureCompensation());
}

//-----------------------------------------------------------------------------

void StopMotionController::onWhiteBalanceChanged(int index) {
  m_stopMotion->setWhiteBalance(m_whiteBalanceCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onWhiteBalanceChangedSignal(QString text) {
  m_whiteBalanceCombo->setCurrentText(m_stopMotion->getCurrentWhiteBalance());
  refreshColorTemperatureList();
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTemperatureChanged(int index) {
  m_stopMotion->setColorTemperature(m_kelvinCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTemperatureChangedSignal(QString text) {
  m_kelvinCombo->setCurrentText(m_stopMotion->getCurrentColorTemperature());
}

//-----------------------------------------------------------------------------

void StopMotionController::onImageQualityChanged(int index) {
  m_stopMotion->setImageQuality(m_imageQualityCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onImageQualityChangedSignal(QString text) {
  m_imageQualityCombo->setCurrentText(m_stopMotion->getCurrentImageQuality());
}

//-----------------------------------------------------------------------------

void StopMotionController::onPictureStyleChanged(int index) {
  m_stopMotion->setPictureStyle(m_pictureStyleCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onPictureStyleChangedSignal(QString text) {
  m_pictureStyleCombo->setCurrentText(m_stopMotion->getCurrentPictureStyle());
}

//-----------------------------------------------------------------------------

void StopMotionController::onZoomPressed() { m_stopMotion->zoomLiveView(); }

//-----------------------------------------------------------------------------

void StopMotionController::onPickZoomPressed() {
  m_stopMotion->m_pickLiveViewZoom = true;
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear() { m_stopMotion->focusNear(); }

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar() { m_stopMotion->focusFar(); }

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear2() { m_stopMotion->focusNear2(); }

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar2() { m_stopMotion->focusFar2(); }

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear3() { m_stopMotion->focusNear3(); }

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar3() { m_stopMotion->focusFar3(); }

//-----------------------------------------------------------------------------

void StopMotionController::showEvent(QShowEvent *event) {}

//-----------------------------------------------------------------------------

void StopMotionController::hideEvent(QHideEvent *event) {
  // disconnect(m_apertureCombo, SIGNAL(currentIndexChanged(int)), this,
  //           SLOT(onApertureChanged(int)));
  // disconnect(m_shutterSpeedCombo, SIGNAL(currentIndexChanged(int)), this,
  //           SLOT(onShutterSpeedChanged(int)));
  // disconnect(m_isoCombo, SIGNAL(currentIndexChanged(int)), this,
  //           SLOT(onIsoChanged(int)));
  // disconnect(m_exposureCombo, SIGNAL(currentIndexChanged(int)), this,
  //           SLOT(onExposureChanged(int)));

  // disconnect(m_stopMotion, SIGNAL(cameraChanged()), this,
  //           SLOT(refreshCameraList()));
  // disconnect(m_stopMotion, SIGNAL(optionsChanged()), this,
  //           SLOT(refreshOptionsLists()));
  // disconnect(m_stopMotion, SIGNAL(apertureOptionsChanged()), this,
  //           SLOT(refreshApertureList()));
  // disconnect(m_stopMotion, SIGNAL(shutterSpeedOptionsChanged()), this,
  //           SLOT(refreshShutterSpeedList()));
  // disconnect(m_stopMotion, SIGNAL(isoOptionsChanged()), this,
  //           SLOT(refreshIsoList()));
  // disconnect(m_stopMotion, SIGNAL(exposureOptionsChanged()), this,
  //           SLOT(refreshExposureList()));
  // disconnect(m_stopMotion, SIGNAL(modeChanged()), this, SLOT(refreshMode()));
}

////-----------------------------------------------------------------------------
//
// void StopMotionController::keyPressEvent(QKeyEvent *event) {
//  // override return (or enter) key as shortcut key for capturing
//  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
//    // show button-clicking animation followed by calling
//    // onCaptureButtonClicked()
//    m_captureButton->animateClick();
//    event->accept();
//  } else
//    event->ignore();
//}
//
////-----------------------------------------------------------------------------
//
// void StopMotionController::mousePressEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewToggleClicked() {
  m_stopMotion->toggleLiveView();
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewChanged(bool on) {
  if (on) {
    m_toggleLiveViewButton->setText(tr("Stop Live View"));

  } else
    m_toggleLiveViewButton->setText(tr("Start Live View"));
}

//-----------------------------------------------------------------------------

void StopMotionController::onOnionOpacityFldEdited() {
  int value = (int)(255.0f * (float)m_onionOpacityFld->getValue() / 100.0f);
  m_stopMotion->setOpacity(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onOnionOpacitySliderChanged(bool ignore) {
  int value = (int)(255.0f * (float)m_onionOpacityFld->getValue() / 100.0f);
  m_stopMotion->setOpacity(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onOpacityChanged(int opacity) {
  m_onionOpacityFld->setValue(double(100 * opacity) / 255.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureButtonClicked(bool on) {
  m_stopMotion->captureImage();
}

//-----------------------------------------------------------------------------

void StopMotionController::openSaveInFolderPopup() {
  if (m_saveInFolderPopup->exec()) {
    QString oldPath = m_saveInFileFld->getPath();
    m_saveInFileFld->setPath(m_saveInFolderPopup->getPath());
    if (oldPath == m_saveInFileFld->getPath())
      m_stopMotion->setToNextNewLevel();
    else {
      onSaveInPathEdited();
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onFileTypeActivated() {
  m_stopMotion->setFileType(m_fileTypeCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameNumberChanged() {
  m_stopMotion->setFrameNumber(m_frameNumberEdit->getValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onXSheetFrameNumberChanged() {
  m_stopMotion->setXSheetFrameNumber(m_xSheetFrameNumberEdit->getValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onSaveInPathEdited() {
  m_stopMotion->setFilePath(m_saveInFileFld->getPath());
}

//-----------------------------------------------------------------------------

void StopMotionController::onSceneSwitched() {
  // m_saveInFolderPopup->updateParentFolder();
  // m_saveInFileFld->setPath(m_saveInFolderPopup->getParentPath());
  // m_stopMotion->refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void StopMotionController::updateStopMotion() {}
