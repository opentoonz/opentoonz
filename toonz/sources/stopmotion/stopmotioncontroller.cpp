#include "stopmotioncontroller.h"
#include "webcam.h"

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
#include "toonzqt/doublefield.h"
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
#include <QSerialPort>

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
  m_tabBar->addSimpleTab(tr("Light"));
  m_tabBar->addSimpleTab(tr("Motion"));
  m_tabBarContainer    = new TabBarContainter(this);
  m_mainControlsPage   = new QFrame(this);
  m_cameraSettingsPage = new QFrame(this);
  m_optionsPage        = new QFrame(this);
  m_motionPage         = new QFrame(this);
  m_lightPage          = new QFrame(this);

  // **********************
  // Make Control Page
  // **********************

  m_saveInFolderPopup = new PencilTestSaveInFolderPopup(this);
  m_cameraListCombo   = new QComboBox(this);
  m_resolutionCombo   = new QComboBox(this);
  m_resolutionCombo->setFixedWidth(fontMetrics().width("0000 x 0000") + 25);
  m_resolutionLabel                 = new QLabel(tr("Resolution: "), this);
  m_cameraStatusLabel               = new QLabel(tr("Camera Status"), this);
  QPushButton *refreshCamListButton = new QPushButton(tr("Refresh"), this);
  refreshCamListButton->setFixedHeight(28);
  refreshCamListButton->setStyleSheet("padding: 0 2;");
  QGroupBox *fileFrame = new QGroupBox(tr("File"), this);
  m_levelNameEdit      = new LevelNameLineEdit(this);

  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  int startFrame =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled() ? 10 : 1;
  m_frameNumberEdit = new FrameNumberLineEdit(this, startFrame);
  m_frameInfoLabel  = new QLabel("", this);

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

  // should choosing the file type is disabled for simplicity
  // too many options can be a bad thing
  m_fileTypeCombo          = new QComboBox(this);
  m_fileFormatOptionButton = new QPushButton(tr("Options"), this);
  m_fileFormatOptionButton->setFixedHeight(28);
  m_fileFormatOptionButton->setStyleSheet("padding: 0 2;");
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
  m_captureButton = new QPushButton(tr("Capture"), this);
  m_captureButton->setObjectName("LargeSizedText");
  m_captureButton->setFixedHeight(35);
  QCommonStyle style;
  m_captureButton->setIcon(style.standardIcon(QStyle::SP_DialogOkButton));
  m_captureButton->setIconSize(QSize(20, 20));
  m_alwaysUseLiveViewImagesButton = new QPushButton();
  // m_alwaysUseLiveViewImagesButton->setObjectName("LargeSizedText");
  m_alwaysUseLiveViewImagesButton->setObjectName("LiveViewButton");
  m_alwaysUseLiveViewImagesButton->setFixedHeight(35);
  m_alwaysUseLiveViewImagesButton->setFixedWidth(35);
  m_alwaysUseLiveViewImagesButton->setCheckable(true);
  m_alwaysUseLiveViewImagesButton->setIconSize(QSize(25, 25));
  m_alwaysUseLiveViewImagesButton->setToolTip(
      tr("Show original live view images in timeline"));

  // subfolderButton->setObjectName("SubfolderButton");
  // subfolderButton->setIconSize(QSize(15, 15));
  m_saveInFileFld->setMaximumWidth(380);
  m_levelNameEdit->setMaximumWidth(380);

  m_saveInFolderPopup->hide();
  m_zoomButton = new QPushButton(tr("Check"), this);
  m_zoomButton->setFixedHeight(28);
  m_zoomButton->setStyleSheet("padding: 5 2;");
  m_zoomButton->setMaximumWidth(100);
  m_zoomButton->setToolTip(tr("Zoom in to check focus"));
  m_zoomButton->setCheckable(true);
  m_pickZoomButton = new QPushButton(tr("Pick"), this);
  m_pickZoomButton->setStyleSheet("padding: 5 2;");
  m_pickZoomButton->setMaximumWidth(100);
  m_pickZoomButton->setFixedHeight(28);
  m_pickZoomButton->setToolTip(tr("Set focus check location"));
  m_pickZoomButton->setCheckable(true);
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
        // if (m_captureFilterSettingsBtn) {
        //  camLay->addWidget(m_captureFilterSettingsBtn, 0, 3, Qt::AlignLeft);
        //  camLay->addWidget(m_resolutionLabel, 1, 0, Qt::AlignRight);
        //  camLay->addWidget(m_resolutionCombo, 1, 1, 1, 3, Qt::AlignLeft);
        //  camLay->setColumnStretch(3, 30);
        //} else {
        //}
        camLay->addWidget(m_resolutionLabel, 1, 0, Qt::AlignRight);
        camLay->addWidget(m_resolutionCombo, 1, 1, 1, 2, Qt::AlignLeft);
        camLay->setColumnStretch(2, 30);
        camLay->addWidget(m_cameraStatusLabel, 2, 1, 1, 2, Qt::AlignLeft);
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
    QVBoxLayout *innerSettingsLayout = new QVBoxLayout;
    m_noCameraFrame                  = new QFrame();
    QHBoxLayout *noCameraLayout      = new QHBoxLayout();
    noCameraLayout->addStretch();
    noCameraLayout->addWidget(
        new QLabel(tr("Select a camera to change settings.")));
    noCameraLayout->addStretch();
    m_noCameraFrame->setLayout(noCameraLayout);
    innerSettingsLayout->addWidget(m_noCameraFrame);

    m_apertureLabel  = new QLabel(tr(""), this);
    m_apertureSlider = new QSlider(Qt::Horizontal, this);
    m_apertureSlider->setRange(0, 10);
    m_apertureSlider->setTickInterval(1);
    m_apertureSlider->setFixedWidth(260);
    m_isoLabel  = new QLabel(tr(""), this);
    m_isoSlider = new QSlider(Qt::Horizontal, this);
    m_isoSlider->setRange(0, 10);
    m_isoSlider->setTickInterval(1);
    m_isoSlider->setFixedWidth(260);
    m_shutterSpeedLabel  = new QLabel(tr(""), this);
    m_shutterSpeedSlider = new QSlider(Qt::Horizontal, this);
    m_shutterSpeedSlider->setRange(0, 10);
    m_shutterSpeedSlider->setTickInterval(1);
    m_shutterSpeedSlider->setFixedWidth(260);
    m_kelvinValueLabel = new QLabel(tr("Temperature: "), this);
    m_kelvinSlider     = new QSlider(Qt::Horizontal, this);
    m_kelvinSlider->setRange(0, 10);
    m_kelvinSlider->setTickInterval(1);
    m_kelvinSlider->setFixedWidth(260);
    m_exposureCombo       = new QComboBox(this);
    m_whiteBalanceCombo   = new QComboBox(this);
    m_imageQualityCombo   = new QComboBox(this);
    m_pictureStyleCombo   = new QComboBox(this);
    m_cameraSettingsLabel = new QLabel(tr("Camera Model"), this);
    m_cameraModeLabel     = new QLabel(tr("Camera Mode"), this);
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
      settingsGridLayout->addWidget(new QLabel(" ", this), 2, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_shutterSpeedLabel, 3, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_shutterSpeedSlider, 4, 0, 1, 2,
                                    Qt::AlignCenter);

      settingsGridLayout->addWidget(m_apertureLabel, 5, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_apertureSlider, 6, 0, 1, 2,
                                    Qt::AlignCenter);

      settingsGridLayout->addWidget(m_isoLabel, 7, 0, 1, 2, Qt::AlignCenter);
      settingsGridLayout->addWidget(m_isoSlider, 8, 0, 1, 2, Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 9, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(tr("White Balance: ")), 10, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_whiteBalanceCombo, 10, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(m_kelvinValueLabel, 11, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_kelvinSlider, 12, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 13, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(tr("Picture Style: ")), 14, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_pictureStyleCombo, 14, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Image Quality: ")), 15, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_imageQualityCombo, 15, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Exposure: ")), 16, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_exposureCombo, 16, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(" ", this), 17, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 19, 0, 1, 2,
                                    Qt::AlignCenter);

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
    m_dslrFrame = new QFrame();
    m_dslrFrame->setLayout(settingsLayout);
    innerSettingsLayout->addWidget(m_dslrFrame);
    m_dslrFrame->hide();

    QVBoxLayout *webcamSettingsLayout = new QVBoxLayout;
    webcamSettingsLayout->setSpacing(0);
    webcamSettingsLayout->setMargin(5);
    QHBoxLayout *webcamLabelLayout = new QHBoxLayout();
    m_webcamLabel = new QLabel(tr("insert webcam name here"), this);
    webcamLabelLayout->addStretch();
    webcamLabelLayout->addWidget(m_webcamLabel);
    webcamLabelLayout->addStretch();
    webcamSettingsLayout->addLayout(webcamLabelLayout);
    webcamSettingsLayout->addSpacing(10);

    // webcam focus
    m_webcamAutoFocusGB = new QGroupBox(tr("Manual Focus"), this);
    m_webcamAutoFocusGB->setCheckable(true);

    m_webcamFocusSlider = new QSlider(Qt::Horizontal, this);
    m_webcamFocusSlider->setRange(0, 255);
    m_webcamFocusSlider->setTickInterval(5);

    QHBoxLayout *webcamFocusLay = new QHBoxLayout();
    webcamFocusLay->addWidget(new QLabel(tr("Focus: "), this), 0);
    webcamFocusLay->addWidget(m_webcamFocusSlider, 1);
    m_webcamAutoFocusGB->setLayout(webcamFocusLay);
    webcamSettingsLayout->addWidget(m_webcamAutoFocusGB);
    webcamSettingsLayout->addSpacing(5);

    QGridLayout *webcamGridLay = new QGridLayout();
    webcamGridLay->setMargin(0);
    webcamGridLay->setSpacing(3);
    webcamGridLay->setColumnStretch(0, 0);
    webcamGridLay->setColumnStretch(1, 1);

    // webcam exposure
    m_webcamExposureSlider = new QSlider(Qt::Horizontal, this);
    m_webcamExposureSlider->setRange(-13, -1);
    m_webcamExposureSlider->setTickInterval(1);

    QHBoxLayout *webcamExposureLay = new QHBoxLayout();
    webcamExposureLay->addWidget(m_webcamExposureSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Exposure: "), this), 0, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamExposureLay, 0, 1, 1, 1);

    // webcam brightness
    m_webcamBrightnessSlider = new QSlider(Qt::Horizontal, this);
    m_webcamBrightnessSlider->setRange(0, 255);

    QHBoxLayout *webcamBrightnessLay = new QHBoxLayout();
    webcamBrightnessLay->addWidget(m_webcamBrightnessSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Brightness: "), this), 1, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamBrightnessLay, 1, 1, 1, 1);

    // webcam contrast
    m_webcamContrastSlider = new QSlider(Qt::Horizontal, this);
    m_webcamContrastSlider->setRange(0, 255);

    QHBoxLayout *webcamContrastLay = new QHBoxLayout();
    webcamContrastLay->addWidget(m_webcamContrastSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Contrast: "), this), 2, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamContrastLay, 2, 1, 1, 1);

    // webcam gain
    m_webcamGainSlider = new QSlider(Qt::Horizontal, this);
    m_webcamGainSlider->setRange(0, 255);

    QHBoxLayout *webcamGainLay = new QHBoxLayout();
    webcamGainLay->addWidget(m_webcamGainSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Gain: "), this), 3, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamGainLay, 3, 1, 1, 1);

    // webcam saturation
    m_webcamSaturationSlider = new QSlider(Qt::Horizontal, this);
    m_webcamSaturationSlider->setRange(0, 255);

    QHBoxLayout *webcamSaturationLay = new QHBoxLayout();
    webcamSaturationLay->addWidget(m_webcamSaturationSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Saturation: "), this), 4, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamSaturationLay, 4, 1, 1, 1);

#ifdef _WIN32
    m_captureFilterSettingsBtn = new QPushButton(this);
#else
    m_captureFilterSettingsBtn = 0;
#endif
    if (m_captureFilterSettingsBtn) {
      m_captureFilterSettingsBtn->setObjectName("GearButton");
      m_captureFilterSettingsBtn->setFixedSize(128, 28);
      m_captureFilterSettingsBtn->setText(tr("More"));
      m_captureFilterSettingsBtn->setIconSize(QSize(15, 15));
      m_captureFilterSettingsBtn->setToolTip(tr("Webcam Settings..."));
      webcamGridLay->addWidget(m_captureFilterSettingsBtn, 5, 0, 1, 2,
                               Qt::AlignCenter);
    }

    webcamSettingsLayout->addLayout(webcamGridLay);

    webcamSettingsLayout->addStretch();
    m_webcamFrame = new QFrame();
    m_webcamFrame->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Expanding);
    m_webcamFrame->setLayout(webcamSettingsLayout);
    innerSettingsLayout->addWidget(m_webcamFrame);
    m_webcamFrame->hide();
    innerSettingsLayout->addStretch();
    m_cameraSettingsPage->setLayout(innerSettingsLayout);

    // Make Options Page
    QGroupBox *webcamBox  = new QGroupBox(tr("Webcam Options"), this);
    QGroupBox *dslrBox    = new QGroupBox(tr("DSLR Options"), this);
    m_timerCB             = new QGroupBox(tr("Use Time Lapse"), this);
    m_timerIntervalFld    = new DVGui::DoubleField(this, true, 1);
    m_timerCB->setCheckable(true);
    m_timerCB->setObjectName("CleanupSettingsFrame");
    m_timerCB->setChecked(false);
    m_timerIntervalFld->setRange(0.0, 60.0);
    m_timerIntervalFld->setValue(0.0);

    m_postCaptureReviewFld = new DVGui::DoubleField(this, true, 1);
    m_postCaptureReviewFld->setRange(0.0, 10.0);
    m_postCaptureReviewFld->setValue(0.0);

    m_subsamplingFld = new DVGui::IntField(this);
    m_subsamplingFld->setRange(1, 30);
    m_subsamplingFld->setDisabled(true);

    m_placeOnXSheetCB = new QCheckBox(tr("Place on XSheet"), this);
    m_placeOnXSheetCB->setToolTip(tr("Place the frame in the XSheet"));

    m_useScaledFullSizeImages =
        new QCheckBox(tr("Use Reduced Resolution Images"), this);
    m_directShowCB = new QCheckBox(tr("Use Direct Show Webcam Drivers"), this);
    m_useMjpgCB    = new QCheckBox(tr("Use MJPG with Webcam"), this);
    m_useNumpadCB = new QCheckBox(tr("Use Numpad Shortcuts When Active"), this);
    m_useNumpadCB->setToolTip(
        tr("Requires restarting camera when toggled\n"
           "NP 1 = Previous Frame\n"
           "NP 2 = Next Frame\n"
           "NP 3 = Jump To Camera\n"
           "NP 5 = Toggle Live View\n"
           "NP 6 = Short Play\n"
           "NP 8 = Loop\n"
           "NP 0 = Play\n"
           "Period = Use Live View Images\n"
           "Plus = Raise Opacity\n"
           "Minus = Lower Opacity\n"
           "Enter = Capture\n"
           "BackSpace = Remove Frame\n"
           "Multiply = Toggle Zoom\n"
           "Divide = Focus Check"));
    m_drawBeneathCB = new QCheckBox(tr("Show Camera Below Other Levels"), this);
    m_liveViewOnAllFramesCB =
        new QCheckBox(tr("Show Live View on All Frames"), this);
    m_playSound = new QCheckBox(tr("Play Sound on Capture"), this);
    m_playSound->setToolTip(tr("Make a click sound on each capture"));

    QVBoxLayout *optionsOutsideLayout = new QVBoxLayout;
    QGridLayout *optionsLayout        = new QGridLayout;
    optionsLayout->setSpacing(3);
    optionsLayout->setMargin(5);
    QGridLayout *webcamLayout   = new QGridLayout;
    QGridLayout *dslrLayout     = new QGridLayout;
    QGridLayout *checkboxLayout = new QGridLayout;

    dslrLayout->addWidget(m_useScaledFullSizeImages, 1, 0, 1, 2);
    dslrLayout->setColumnStretch(1, 30);
    dslrBox->setLayout(dslrLayout);
    dslrBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    optionsOutsideLayout->addWidget(dslrBox, Qt::AlignCenter);
    dslrBox->hide();

    webcamLayout->addWidget(m_directShowCB, 0, 0, 1, 2);
    webcamLayout->addWidget(m_useMjpgCB, 1, 0, 1, 2);
    webcamLayout->setColumnStretch(1, 30);
    webcamBox->setLayout(webcamLayout);
    webcamBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    optionsOutsideLayout->addWidget(webcamBox, Qt::AlignCenter);
    webcamBox->hide();

    QGridLayout *timerLay = new QGridLayout();
    timerLay->setMargin(8);
    timerLay->setHorizontalSpacing(3);
    timerLay->setVerticalSpacing(5);
    {
      timerLay->addWidget(new QLabel(tr("Interval(sec):"), this), 1, 0,
                          Qt::AlignRight);
      timerLay->addWidget(m_timerIntervalFld, 1, 1);
    }
    timerLay->setColumnStretch(0, 0);
    timerLay->setColumnStretch(1, 1);
    m_timerCB->setLayout(timerLay);
    optionsOutsideLayout->addWidget(m_timerCB);

    checkboxLayout->addWidget(m_placeOnXSheetCB, 0, 0, 1, 2);
    m_placeOnXSheetCB->hide();
    checkboxLayout->addWidget(m_drawBeneathCB, 1, 0, 1, 2);
    m_drawBeneathCB->hide();

    checkboxLayout->addWidget(m_useNumpadCB, 2, 0, 1, 2);
    checkboxLayout->addWidget(m_liveViewOnAllFramesCB, 3, 0, 1, 2);
    m_liveViewOnAllFramesCB->hide();
    checkboxLayout->addWidget(m_playSound, 4, 0, 1, 2);

    checkboxLayout->setColumnStretch(1, 30);
    optionsOutsideLayout->addLayout(checkboxLayout, Qt::AlignLeft);

    optionsLayout->addWidget(new QLabel(tr("Capture Review Time: ")), 0, 0,
                             Qt::AlignRight);
    optionsLayout->addWidget(m_postCaptureReviewFld, 0, 1);
    // optionsLayout->addWidget(new QLabel(tr("Level Subsampling: ")), 1, 0,
    //                         Qt::AlignRight);
    optionsLayout->addWidget(m_subsamplingFld, 1, 1);
    m_subsamplingFld->hide();
    optionsLayout->setColumnStretch(1, 30);
    optionsLayout->setRowStretch(2, 30);
    optionsOutsideLayout->addLayout(optionsLayout, Qt::AlignLeft);
    optionsOutsideLayout->addStretch();

    m_optionsPage->setLayout(optionsOutsideLayout);

    m_blackScreenForCapture = new QCheckBox(tr("Blackout all Screens"), this);
    QVBoxLayout *lightOutsideLayout = new QVBoxLayout;
    m_testLightsButton              = new QPushButton(tr("Test"), this);
    m_testLightsButton->setMaximumWidth(150);
    m_testLightsButton->setFixedHeight(28);
    m_testLightsButton->setSizePolicy(QSizePolicy::Maximum,
                                      QSizePolicy::Maximum);
    m_testLightsButton->setStyleSheet("padding: 2px;");
    m_lightTestTimer = new QTimer(this);
    m_lightTestTimer->setSingleShot(true);
    m_screen1ColorFld = new DVGui::ColorField(
        this, false, TPixel32(0, 0, 0, 255), 40, true, 60);

    m_screen2ColorFld = new DVGui::ColorField(
        this, false, TPixel32(0, 0, 0, 255), 40, true, 60);

    m_screen3ColorFld = new DVGui::ColorField(
        this, false, TPixel32(0, 0, 0, 255), 40, true, 60);

    QGridLayout *lightTopLayout = new QGridLayout;
    lightTopLayout->addWidget(m_blackScreenForCapture, 0, 0, Qt::AlignRight);
    lightTopLayout->setColumnStretch(1, 30);
    lightOutsideLayout->addLayout(lightTopLayout);

    m_screen1Box = new QGroupBox(tr("Screen 1"), this);
    m_screen1Box->setCheckable(true);
    m_screen1Box->setChecked(false);
    QGridLayout *screen1Layout = new QGridLayout;
    screen1Layout->addWidget(m_screen1ColorFld, 0, 0, 1, 2, Qt::AlignLeft);
    screen1Layout->setColumnStretch(1, 30);
    m_screen1Box->setLayout(screen1Layout);
    m_screen1Box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    lightOutsideLayout->addWidget(m_screen1Box, Qt::AlignCenter);

    m_screen2Box = new QGroupBox(tr("Screen 2"), this);
    m_screen2Box->setCheckable(true);
    m_screen2Box->setChecked(false);
    QGridLayout *screen2Layout = new QGridLayout;
    screen2Layout->addWidget(m_screen2ColorFld, 0, 0, 1, 2, Qt::AlignLeft);
    screen2Layout->setColumnStretch(1, 30);
    m_screen2Box->setLayout(screen2Layout);
    m_screen2Box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    lightOutsideLayout->addWidget(m_screen2Box, Qt::AlignCenter);

    m_screen3Box = new QGroupBox(tr("Screen 3"), this);
    m_screen3Box->setCheckable(true);
    m_screen3Box->setChecked(false);
    QGridLayout *screen3Layout = new QGridLayout;
    screen3Layout->addWidget(m_screen3ColorFld, 0, 0, 1, 2, Qt::AlignLeft);
    screen3Layout->setColumnStretch(1, 30);
    m_screen3Box->setLayout(screen3Layout);
    m_screen3Box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    lightOutsideLayout->addWidget(m_screen3Box, Qt::AlignCenter);

    QHBoxLayout *testLayout = new QHBoxLayout;
    testLayout->addWidget(m_testLightsButton, Qt::AlignHCenter);
    lightOutsideLayout->addLayout(testLayout);
    lightOutsideLayout->addStretch();
    m_lightPage->setLayout(lightOutsideLayout);

    if (m_stopMotion->m_light->m_screenCount < 3) m_screen3Box->hide();
    if (m_stopMotion->m_light->m_screenCount < 2) m_screen2Box->hide();

    QVBoxLayout *motionOutsideLayout = new QVBoxLayout;
    // QGridLayout* motionInsideLayout = new QGridLayout;
    m_controlDeviceCombo = new QComboBox(this);
    m_controlDeviceCombo->addItems(
        m_stopMotion->m_serial->getAvailableSerialPorts());

    QGroupBox *motionBox      = new QGroupBox(tr("Motion Control"), this);
    QGridLayout *motionLayout = new QGridLayout;
    motionLayout->addWidget(new QLabel(tr("Port: ")), 0, 0, Qt::AlignRight);
    motionLayout->addWidget(m_controlDeviceCombo, 0, 1, Qt::AlignLeft);
    motionLayout->setColumnStretch(1, 30);
    motionBox->setLayout(motionLayout);
    motionBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    motionOutsideLayout->addWidget(motionBox, Qt::AlignCenter);
    motionOutsideLayout->addStretch();

    // motionOutsideLayout->addLayout(motionInsideLayout);
    m_motionPage->setLayout(motionOutsideLayout);

    QScrollArea *mainArea = makeChooserPageWithoutScrollBar(m_mainControlsPage);
    QScrollArea *settingsArea =
        makeChooserPageWithoutScrollBar(m_cameraSettingsPage);
    QScrollArea *optionsArea = makeChooserPageWithoutScrollBar(m_optionsPage);
    QScrollArea *lightArea   = makeChooserPageWithoutScrollBar(m_lightPage);
    QScrollArea *motionArea  = makeChooserPageWithoutScrollBar(m_motionPage);

    mainArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    settingsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    optionsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    lightArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    motionArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_stackedChooser = new QStackedWidget(this);
    m_stackedChooser->addWidget(mainArea);
    m_stackedChooser->addWidget(settingsArea);
    m_stackedChooser->addWidget(optionsArea);
    m_stackedChooser->addWidget(lightArea);
    m_stackedChooser->addWidget(motionArea);
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
    controlButtonLay->addWidget(m_alwaysUseLiveViewImagesButton, 0);
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

      mainLayout->addWidget(m_tabBarContainer, 0);
      mainLayout->addWidget(m_stackedChooser, 1);
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
  ret = ret && connect(refreshCamListButton, SIGNAL(clicked()), this,
                       SLOT(refreshCameraListCalled()));
  ret = ret && connect(m_cameraListCombo, SIGNAL(activated(int)), this,
                       SLOT(onCameraListComboActivated(int)));
  ret = ret && connect(m_resolutionCombo, SIGNAL(activated(const QString &)),
                       this, SLOT(onResolutionComboActivated(const QString &)));
  if (m_captureFilterSettingsBtn)
    ret = ret && connect(m_captureFilterSettingsBtn, SIGNAL(clicked()), this,
                         SLOT(onCaptureFilterSettingsBtnPressed()));
  ret = ret && connect(m_fileFormatOptionButton, SIGNAL(clicked()), this,
                       SLOT(onFileFormatOptionButtonPressed()));
  ret = ret && connect(m_levelNameEdit, SIGNAL(levelNameEdited()), this,
                       SLOT(onLevelNameEdited()));
  ret = ret &&
        connect(nextLevelButton, SIGNAL(clicked()), this, SLOT(onNextName()));
  ret = ret && connect(m_previousLevelButton, SIGNAL(clicked()), this,
                       SLOT(onPreviousName()));
  ret = ret && connect(nextOpenLevelButton, SIGNAL(clicked()), this,
                       SLOT(onNextNewLevel()));
  ret = ret &&
        connect(nextFrameButton, SIGNAL(clicked()), this, SLOT(onNextFrame()));
  ret = ret &&
        connect(lastFrameButton, SIGNAL(clicked()), this, SLOT(onLastFrame()));
  ret = ret && connect(m_previousFrameButton, SIGNAL(clicked()), this,
                       SLOT(onPreviousFrame()));
  ret = ret && connect(nextXSheetFrameButton, SIGNAL(clicked()), this,
                       SLOT(onNextXSheetFrame()));
  ret = ret && connect(m_previousXSheetFrameButton, SIGNAL(clicked()), this,
                       SLOT(onPreviousXSheetFrame()));
  ret = ret && connect(m_setToCurrentXSheetFrameButton, SIGNAL(clicked()), this,
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
  ret = ret && connect(m_alwaysUseLiveViewImagesButton, SIGNAL(clicked()), this,
                       SLOT(onAlwaysUseLiveViewImagesButtonClicked()));
  ret =
      ret && connect(m_stopMotion, SIGNAL(alwaysUseLiveViewImagesToggled(bool)),
                     this, SLOT(onAlwaysUseLiveViewImagesToggled(bool)));
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
  ret = ret && connect(m_playSound, SIGNAL(toggled(bool)), this,
                       SLOT(onPlaySoundToggled(bool)));
  ret = ret && connect(m_placeOnXSheetCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onPlaceOnXSheetChanged(int)));
  ret = ret && connect(m_directShowCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseDirectShowChanged(int)));
  ret = ret && connect(m_useMjpgCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseMjpgChanged(int)));
  ret = ret && connect(m_useNumpadCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseNumpadChanged(int)));
  ret = ret && connect(m_drawBeneathCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onDrawBeneathChanged(int)));
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
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(scaleFullSizeImagesSignal(bool)),
                this, SLOT(onScaleFullSizeImagesSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(liveViewOnAllFramesSignal(bool)),
                       this, SLOT(onLiveViewOnAllFramesSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(placeOnXSheetSignal(bool)), this,
                       SLOT(onPlaceOnXSheetSignal(bool)));
  ret =
      ret && connect(m_stopMotion->m_webcam, SIGNAL(useDirectShowSignal(bool)),
                     this, SLOT(onUseDirectShowSignal(bool)));
  ret = ret && connect(m_stopMotion->m_webcam, SIGNAL(useMjpgSignal(bool)),
                       this, SLOT(onUseMjpgSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(useNumpadSignal(bool)), this,
                       SLOT(onUseNumpadSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(drawBeneathLevelsSignal(bool)),
                       this, SLOT(onDrawBeneathSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(reviewTimeChangedSignal(int)), this,
                       SLOT(onReviewTimeChangedSignal(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(playCaptureSignal(bool)), this,
                       SLOT(onPlayCaptureSignal(bool)));

  // From Stop Motion Main
  ret = ret && connect(m_stopMotion, SIGNAL(newDimensions()), this,
                       SLOT(updateDimensions()));
  ret = ret && connect(m_stopMotion, SIGNAL(updateCameraList(QString)), this,
                       SLOT(refreshCameraList(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(liveViewChanged(bool)), this,
                       SLOT(onLiveViewChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(newCameraSelected(int, bool)), this,
                       SLOT(onNewCameraSelected(int, bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(cameraChanged(QString)), this,
                       SLOT(refreshCameraList(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(optionsChanged()), this,
                       SLOT(refreshOptionsLists()));

  // EOS Connections
  ret = ret &&
        connect(m_zoomButton, SIGNAL(clicked()), this, SLOT(onZoomPressed()));
  ret = ret && connect(m_pickZoomButton, SIGNAL(clicked()), this,
                       SLOT(onPickZoomPressed()));
  ret = ret && connect(m_focusNearButton, SIGNAL(clicked()), this,
                       SLOT(onFocusNear()));
  ret = ret &&
        connect(m_focusFarButton, SIGNAL(clicked()), this, SLOT(onFocusFar()));
  ret = ret && connect(m_focusNear2Button, SIGNAL(clicked()), this,
                       SLOT(onFocusNear2()));
  ret = ret && connect(m_focusFar2Button, SIGNAL(clicked()), this,
                       SLOT(onFocusFar2()));
  ret = ret && connect(m_focusNear3Button, SIGNAL(clicked()), this,
                       SLOT(onFocusNear3()));
  ret = ret && connect(m_focusFar3Button, SIGNAL(clicked()), this,
                       SLOT(onFocusFar3()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(apertureChangedSignal(QString)),
                this, SLOT(onApertureChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(isoChangedSignal(QString)),
                       this, SLOT(onIsoChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(shutterSpeedChangedSignal(QString)), this,
                       SLOT(onShutterSpeedChangedSignal(QString)));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(exposureChangedSignal(QString)),
                this, SLOT(onExposureChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(whiteBalanceChangedSignal(QString)), this,
                       SLOT(onWhiteBalanceChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(imageQualityChangedSignal(QString)), this,
                       SLOT(onImageQualityChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(pictureStyleChangedSignal(QString)), this,
                       SLOT(onPictureStyleChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(colorTemperatureChangedSignal(QString)), this,
                       SLOT(onColorTemperatureChangedSignal(QString)));
  ret = ret && connect(m_apertureSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onApertureChanged(int)));
  ret = ret && connect(m_shutterSpeedSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onShutterSpeedChanged(int)));
  ret = ret && connect(m_isoSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onIsoChanged(int)));
  ret = ret && connect(m_exposureCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onExposureChanged(int)));
  ret = ret && connect(m_whiteBalanceCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onWhiteBalanceChanged(int)));
  ret = ret && connect(m_kelvinSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onColorTemperatureChanged(int)));
  ret = ret && connect(m_imageQualityCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onImageQualityChanged(int)));
  ret = ret && connect(m_pictureStyleCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onPictureStyleChanged(int)));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(apertureOptionsChanged()),
                       this, SLOT(refreshApertureList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(shutterSpeedOptionsChanged()),
                this, SLOT(refreshShutterSpeedList()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(isoOptionsChanged()), this,
                       SLOT(refreshIsoList()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(exposureOptionsChanged()),
                       this, SLOT(refreshExposureList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(whiteBalanceOptionsChanged()),
                this, SLOT(refreshWhiteBalanceList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(imageQualityOptionsChanged()),
                this, SLOT(refreshImageQualityList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(pictureStyleOptionsChanged()),
                this, SLOT(refreshPictureStyleList()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(modeChanged()), this,
                       SLOT(refreshMode()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(focusCheckToggled(bool)),
                       this, SLOT(onFocusCheckToggled(bool)));
  ret =
      ret && connect(m_stopMotion->m_canon, SIGNAL(pickFocusCheckToggled(bool)),
                     this, SLOT(onPickFocusCheckToggled(bool)));

  // Webcam Specific Connections
  ret = ret && connect(m_stopMotion, SIGNAL(webcamResolutionsChanged()), this,
                       SLOT(onWebcamResolutionsChanged()));
  ret = ret && connect(m_stopMotion, SIGNAL(newWebcamResolutionSelected(int)),
                       this, SLOT(onNewWebcamResolutionSelected(int)));
  ret = ret && connect(m_webcamFocusSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamFocusSliderChanged(int)));
  ret = ret && connect(m_webcamAutoFocusGB, SIGNAL(toggled(bool)), this,
                       SLOT(onWebcamAutofocusToggled(bool)));
  ret = ret && connect(m_webcamExposureSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamExposureSliderChanged(int)));
  ret = ret && connect(m_webcamBrightnessSlider, SIGNAL(valueChanged(int)),
                       this, SLOT(onWebcamBrightnessSliderChanged(int)));
  ret = ret && connect(m_webcamContrastSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamContrastSliderChanged(int)));
  ret = ret && connect(m_webcamGainSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamGainSliderChanged(int)));
  ret = ret && connect(m_webcamSaturationSlider, SIGNAL(valueChanged(int)),
                       this, SLOT(onWebcamSaturationSliderChanged(int)));

  // Lighting Connections
  ret = ret &&
        connect(m_screen1ColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(setScreen1Color(const TPixel32 &, bool)));
  ret = ret &&
        connect(m_screen2ColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(setScreen2Color(const TPixel32 &, bool)));
  ret = ret &&
        connect(m_screen3ColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(setScreen3Color(const TPixel32 &, bool)));
  ret = ret && connect(m_screen1Box, SIGNAL(toggled(bool)), this,
                       SLOT(onScreen1OverlayToggled(bool)));
  ret = ret && connect(m_screen2Box, SIGNAL(toggled(bool)), this,
                       SLOT(onScreen2OverlayToggled(bool)));
  ret = ret && connect(m_screen3Box, SIGNAL(toggled(bool)), this,
                       SLOT(onScreen3OverlayToggled(bool)));
  ret = ret && connect(m_lightTestTimer, SIGNAL(timeout()), this,
                       SLOT(onTestLightsTimeout()));
  ret = ret && connect(m_testLightsButton, SIGNAL(clicked()), this,
                       SLOT(onTestLightsPressed()));
  ret = ret &&
        connect(m_stopMotion->m_light, SIGNAL(screen1ColorChanged(TPixel32)),
                this, SLOT(onScreen1ColorChanged(TPixel32)));
  ret = ret &&
        connect(m_stopMotion->m_light, SIGNAL(screen2ColorChanged(TPixel32)),
                this, SLOT(onScreen2ColorChanged(TPixel32)));
  ret = ret &&
        connect(m_stopMotion->m_light, SIGNAL(screen3ColorChanged(TPixel32)),
                this, SLOT(onScreen3ColorChanged(TPixel32)));
  ret =
      ret && connect(m_stopMotion->m_light, SIGNAL(screen1OverlayChanged(bool)),
                     this, SLOT(onScreen1OverlayChanged(bool)));
  ret =
      ret && connect(m_stopMotion->m_light, SIGNAL(screen2OverlayChanged(bool)),
                     this, SLOT(onScreen2OverlayChanged(bool)));
  ret =
      ret && connect(m_stopMotion->m_light, SIGNAL(screen3OverlayChanged(bool)),
                     this, SLOT(onScreen3OverlayChanged(bool)));
  ret = ret && connect(m_blackScreenForCapture, SIGNAL(stateChanged(int)), this,
                       SLOT(onBlackScreenForCaptureChanged(int)));
  ret = ret && connect(m_stopMotion->m_light, SIGNAL(blackCaptureSignal(bool)),
                       this, SLOT(onBlackCaptureSignal(bool)));

  // Serial Port Connections
  ret = ret && connect(m_controlDeviceCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(serialPortChanged(int)));

  // Time Lapse
  ret = ret && connect(m_timerCB, SIGNAL(toggled(bool)), this,
                       SLOT(onIntervalTimerCBToggled(bool)));
  ret = ret && connect(m_timerIntervalFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onIntervalSliderValueChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalAmountChanged(int)), this,
                       SLOT(onIntervalAmountChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalToggled(bool)), this,
                       SLOT(onIntervalToggled(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalStarted()), this,
                       SLOT(onIntervalStarted()));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalStopped()), this,
                       SLOT(onIntervalStopped()));
  ret = ret && connect(m_stopMotion->m_intervalTimer, SIGNAL(timeout()), this,
                       SLOT(onIntervalCaptureTimerTimeout()));
  ret = ret && connect(m_stopMotion->m_countdownTimer, SIGNAL(timeout()), this,
                       SLOT(onIntervalCountDownTimeout()));

  assert(ret);

  m_placeOnXSheetCB->setChecked(
      m_stopMotion->getPlaceOnXSheet() == true ? true : false);
  m_useScaledFullSizeImages->setChecked(
      m_stopMotion->m_canon->m_useScaledImages);
  m_onionOpacityFld->setValue(double(100 * m_stopMotion->getOpacity()) / 255.0);
  m_directShowCB->setChecked(m_stopMotion->m_webcam->getUseDirectShow());
  m_useMjpgCB->setChecked(m_stopMotion->m_webcam->getUseMjpg());
  m_useNumpadCB->setChecked(m_stopMotion->getUseNumpadShortcuts());
  m_drawBeneathCB->setChecked(m_stopMotion->m_drawBeneathLevels);
  m_liveViewOnAllFramesCB->setChecked(m_stopMotion->getAlwaysLiveView());
  m_playSound->setChecked(m_stopMotion->getPlayCaptureSound());
  m_blackScreenForCapture->setChecked(
      m_stopMotion->m_light->getBlackCapture() == true ? true : false);
  if (m_stopMotion->m_light->getBlackCapture()) {
    m_screen1Box->setDisabled(true);
    m_screen2Box->setDisabled(true);
    m_screen3Box->setDisabled(true);
  }
  m_postCaptureReviewFld->setValue(m_stopMotion->getReviewTimeDSec() / 10.0);
  m_timerIntervalFld->setValue(m_stopMotion->getIntervalDSec() / 10.0);

  refreshCameraList(QString(""));
  onSceneSwitched();
  m_stopMotion->setToNextNewLevel();
  m_saveInFileFld->setPath(m_stopMotion->getFilePath());

#ifndef _WIN32
  m_directShowCB->hide();
#endif
}

//-----------------------------------------------------------------------------

StopMotionController::~StopMotionController() {}

//-----------------------------------------------------------------------------

void StopMotionController::setPage(int index) {
  if (index > 0 && m_tabBar->tabText(1) != tr("Settings")) {
    index += 1;
  }
  m_stackedChooser->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScaleFullSizeImagesChanged(int checked) {
#ifdef WITH_CANON
  m_stopMotion->m_canon->setUseScaledImages(checked > 0 ? true : false);
#endif
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
  m_stopMotion->m_light->setBlackCapture(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onBlackCaptureSignal(bool on) {
  m_blackScreenForCapture->blockSignals(true);
  m_blackScreenForCapture->setChecked(on);
  m_screen1Box->setDisabled(on);
  m_screen2Box->setDisabled(on);
  m_screen3Box->setDisabled(on);
  m_blackScreenForCapture->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::setScreen1Color(const TPixel32 &bgColor,
                                           bool isDragging) {
  m_stopMotion->m_light->setScreen1Color(bgColor);
}

//-----------------------------------------------------------------------------

void StopMotionController::setScreen2Color(const TPixel32 &bgColor,
                                           bool isDragging) {
  m_stopMotion->m_light->setScreen2Color(bgColor);
}

//-----------------------------------------------------------------------------

void StopMotionController::setScreen3Color(const TPixel32 &bgColor,
                                           bool isDragging) {
  m_stopMotion->m_light->setScreen3Color(bgColor);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen1OverlayToggled(bool on) {
  m_stopMotion->m_light->setScreen1UseOverlay(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen2OverlayToggled(bool on) {
  m_stopMotion->m_light->setScreen2UseOverlay(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen3OverlayToggled(bool on) {
  m_stopMotion->m_light->setScreen3UseOverlay(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen1ColorChanged(TPixel32 color) {
  m_screen1ColorFld->blockSignals(true);
  m_screen1ColorFld->setColor(color);
  m_screen1ColorFld->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen2ColorChanged(TPixel32 color) {
  m_screen2ColorFld->blockSignals(true);
  m_screen2ColorFld->setColor(color);
  m_screen2ColorFld->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen3ColorChanged(TPixel32 color) {
  m_screen3ColorFld->blockSignals(true);
  m_screen3ColorFld->setColor(color);
  m_screen3ColorFld->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen1OverlayChanged(bool on) {
  m_screen1Box->blockSignals(true);
  m_screen1Box->setChecked(on);
  m_screen1Box->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen2OverlayChanged(bool on) {
  m_screen2Box->blockSignals(true);
  m_screen2Box->setChecked(on);
  m_screen2Box->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen3OverlayChanged(bool on) {
  m_screen3Box->blockSignals(true);
  m_screen3Box->setChecked(on);
  m_screen3Box->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onTestLightsPressed() {
  m_stopMotion->m_light->showOverlays();
  m_lightTestTimer->start(2000);
}

//-----------------------------------------------------------------------------

void StopMotionController::onTestLightsTimeout() {
  m_stopMotion->m_light->hideOverlays();
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
  m_stopMotion->m_webcam->setUseDirectShow(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseDirectShowSignal(bool on) {
  m_directShowCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseMjpgChanged(int checked) {
  m_stopMotion->m_webcam->setUseMjpg(checked);
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

void StopMotionController::onDrawBeneathChanged(int checked) {
  m_stopMotion->setDrawBeneathLevels(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onDrawBeneathSignal(bool on) {
  m_drawBeneathCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureReviewFldEdited() {
  m_stopMotion->setReviewTimeDSec(m_postCaptureReviewFld->getValue() * 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureReviewSliderChanged(bool ignore) {
  m_stopMotion->setReviewTimeDSec(m_postCaptureReviewFld->getValue() * 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onReviewTimeChangedSignal(int time) {
  m_postCaptureReviewFld->setValue(time / 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlayCaptureSignal(bool on) {
  m_playSound->setChecked(on);
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

void StopMotionController::refreshCameraList(QString activeCamera) {
  m_cameraListCombo->blockSignals(true);
  m_cameraListCombo->clear();
  m_cameraStatusLabel->hide();
  QList<QCameraInfo> webcams = m_stopMotion->m_webcam->getWebcams();
  int count                  = webcams.count();
#ifdef WITH_CANON
  count += m_stopMotion->m_canon->getCameraCount();
#endif
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
        std::string name = webcams.at(c).deviceName().toStdString();
        QString camDesc  = webcams.at(c).description();
        m_cameraListCombo->addItem(camDesc);
        maxTextLength = std::max(maxTextLength, fontMetrics().width(camDesc));
      }
    }
#ifdef WITH_CANON
    if (m_stopMotion->m_canon->getCameraCount() > 0) {
      QString name;
      m_stopMotion->m_canon->getCamera(0);
      bool open = m_stopMotion->m_canon->m_sessionOpen;
      if (!open) m_stopMotion->m_canon->openCameraSession();
      name = QString::fromStdString(m_stopMotion->m_canon->getCameraName());
      if (!open) m_stopMotion->m_canon->closeCameraSession();
      m_cameraSettingsLabel->setText(name);
      m_cameraListCombo->addItem(name);
      maxTextLength = std::max(maxTextLength, fontMetrics().width(name));
    }
#endif
    m_cameraListCombo->setMaximumWidth(maxTextLength + 25);
    m_cameraListCombo->setEnabled(true);
    m_cameraListCombo->setCurrentIndex(0);
    m_captureButton->setEnabled(true);
    m_toggleLiveViewButton->setEnabled(true);
  }
  if (activeCamera != "") m_cameraListCombo->setCurrentText(activeCamera);
  m_cameraListCombo->blockSignals(false);
  m_stopMotion->updateLevelNameAndFrame(m_levelNameEdit->text().toStdWString());
  refreshOptionsLists();
#ifdef WITH_CANON
  refreshMode();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshOptionsLists() {
  m_apertureSlider->blockSignals(true);
  m_isoSlider->blockSignals(true);
  m_shutterSpeedSlider->blockSignals(true);
  m_exposureCombo->blockSignals(true);
  m_whiteBalanceCombo->blockSignals(true);
  m_kelvinSlider->blockSignals(true);
  m_imageQualityCombo->blockSignals(true);
  m_pictureStyleCombo->blockSignals(true);

  // m_isoCombo->clear();
  // m_shutterSpeedCombo->clear();
  // m_apertureSlider->clear();
  m_exposureCombo->clear();
#ifdef WITH_CANON
  if (m_stopMotion->m_canon->getCameraCount() == 0) {
    m_shutterSpeedSlider->setDisabled(true);
    m_isoSlider->setDisabled(true);
    m_apertureSlider->setDisabled(true);
    m_exposureCombo->setDisabled(true);
    m_whiteBalanceCombo->setDisabled(true);
    m_kelvinSlider->setDisabled(true);
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
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshMode() {
#ifdef WITH_CANON
  if (m_stopMotion->m_canon->getCameraCount() == 0) {
    m_cameraModeLabel->setText("");
    m_cameraStatusLabel->hide();
    return;
  }
  QString mode    = m_stopMotion->m_canon->getMode();
  QString battery = m_stopMotion->m_canon->getCurrentBatteryLevel();
  m_cameraModeLabel->setText(tr("Mode: ") + mode);
  m_cameraStatusLabel->setText(tr("Mode: ") + mode + tr(" - Battery: ") +
                               battery);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshApertureList() {
#ifdef WITH_CANON
  m_apertureSlider->blockSignals(true);
  int count = 0;
  m_stopMotion->m_canon->getAvailableApertures();
  count = m_stopMotion->m_canon->getApertureOptions().size();

  if (count == 0) {
    m_apertureLabel->setText(tr("Aperture: Auto"));
    m_apertureSlider->setDisabled(true);
    m_apertureSlider->setRange(0, 0);
  } else {
    m_apertureSlider->setEnabled(true);
    m_apertureLabel->setText(tr("Aperture: ") +
                             m_stopMotion->m_canon->getCurrentAperture());
    m_apertureSlider->setRange(0, count - 1);
    m_apertureSlider->setValue(
        m_stopMotion->m_canon->getApertureOptions().lastIndexOf(
            m_stopMotion->m_canon->getCurrentAperture()));
  }
  m_apertureSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshShutterSpeedList() {
#ifdef WITH_CANON
  m_shutterSpeedSlider->blockSignals(true);
  int count = 0;
  m_stopMotion->m_canon->getAvailableShutterSpeeds();
  count = m_stopMotion->m_canon->getShutterSpeedOptions().size();

  if (count == 0) {
    m_shutterSpeedLabel->setText(tr("Shutter Speed: Auto"));
    m_shutterSpeedSlider->setDisabled(true);
    m_shutterSpeedSlider->setRange(0, 0);
  } else {
    m_shutterSpeedSlider->setEnabled(true);
    m_shutterSpeedLabel->setText(
        tr("Shutter Speed: ") +
        m_stopMotion->m_canon->getCurrentShutterSpeed());
    m_shutterSpeedSlider->setRange(0, count - 1);
    m_shutterSpeedSlider->setValue(
        m_stopMotion->m_canon->getShutterSpeedOptions().lastIndexOf(
            m_stopMotion->m_canon->getCurrentShutterSpeed()));
  }
  m_shutterSpeedSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshIsoList() {
#ifdef WITH_CANON
  m_isoSlider->blockSignals(true);
  int count = 0;
  m_stopMotion->m_canon->getAvailableIso();
  count = m_stopMotion->m_canon->getIsoOptions().size();

  if (count == 0) {
    m_isoLabel->setText(tr("Iso: ") + tr("Auto"));
    m_isoSlider->setDisabled(true);
    m_isoSlider->setRange(0, 0);
  } else {
    m_isoSlider->setEnabled(true);
    m_isoLabel->setText(tr("Iso: ") + m_stopMotion->m_canon->getCurrentIso());
    m_isoSlider->setRange(0, count - 1);
    m_isoSlider->setValue(m_stopMotion->m_canon->getIsoOptions().lastIndexOf(
        m_stopMotion->m_canon->getCurrentIso()));
  }
  m_isoSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshExposureList() {
#ifdef WITH_CANON
  m_exposureCombo->blockSignals(true);
  m_exposureCombo->clear();
  m_stopMotion->m_canon->getAvailableExposureCompensations();
  QStringList options = m_stopMotion->m_canon->getExposureOptions();
  m_exposureCombo->addItems(options);
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_exposureCombo->count() == 0) {
    m_exposureCombo->addItem(tr("Disabled"));
    m_exposureCombo->setDisabled(true);
    m_exposureCombo->setMaximumWidth(fontMetrics().width(tr("Disabled")) + 25);
  } else {
    m_exposureCombo->setEnabled(true);
    m_exposureCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentExposureCompensation());
    m_exposureCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_exposureCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshWhiteBalanceList() {
#ifdef WITH_CANON
  m_whiteBalanceCombo->blockSignals(true);
  m_whiteBalanceCombo->clear();
  m_stopMotion->m_canon->getAvailableWhiteBalances();
  QStringList options = m_stopMotion->m_canon->getWhiteBalanceOptions();
  m_whiteBalanceCombo->addItems(options);
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_whiteBalanceCombo->count() == 0) {
    m_whiteBalanceCombo->addItem(tr("Disabled"));
    m_whiteBalanceCombo->setDisabled(true);
    m_whiteBalanceCombo->setMaximumWidth(fontMetrics().width(tr("Disabled")) +
                                         25);
  } else {
    m_whiteBalanceCombo->setEnabled(true);
    m_whiteBalanceCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentWhiteBalance());
    m_whiteBalanceCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_whiteBalanceCombo->blockSignals(false);
  refreshColorTemperatureList();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshColorTemperatureList() {
#ifdef WITH_CANON
  m_kelvinSlider->blockSignals(true);
  int count = 0;
  count     = m_stopMotion->m_canon->getColorTemperatureOptions().size();

  if (count == 0 ||
      m_stopMotion->m_canon->getCurrentWhiteBalance() != "Color Temperature") {
    // m_kelvinCombo->addItem(tr("Disabled"));
    m_kelvinSlider->setDisabled(true);
    m_kelvinSlider->hide();
    m_kelvinValueLabel->hide();
  } else {
    m_kelvinSlider->show();
    m_kelvinValueLabel->show();
    m_kelvinSlider->setEnabled(true);
    m_kelvinSlider->setRange(0, count - 1);
    m_kelvinValueLabel->setText(
        tr("Temperature: ") +
        m_stopMotion->m_canon->getCurrentColorTemperature());
  }
  m_kelvinSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshImageQualityList() {
#ifdef WITH_CANON
  m_imageQualityCombo->blockSignals(true);
  m_imageQualityCombo->clear();
  m_stopMotion->m_canon->getAvailableImageQualities();
  QStringList options = m_stopMotion->m_canon->getImageQualityOptions();
  m_imageQualityCombo->addItems(options);
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_imageQualityCombo->count() == 0) {
    m_imageQualityCombo->addItem(tr("Disabled"));
    m_imageQualityCombo->setDisabled(true);
    m_imageQualityCombo->setMaximumWidth(fontMetrics().width(tr("Disabled")) +
                                         25);
  } else {
    m_imageQualityCombo->setEnabled(true);
    m_imageQualityCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentImageQuality());
    m_imageQualityCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_imageQualityCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshPictureStyleList() {
#ifdef WITH_CANON
  m_pictureStyleCombo->blockSignals(true);
  m_pictureStyleCombo->clear();
  m_stopMotion->m_canon->getAvailablePictureStyles();
  QStringList options = m_stopMotion->m_canon->getPictureStyleOptions();
  m_pictureStyleCombo->addItems(
      m_stopMotion->m_canon->getPictureStyleOptions());
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_pictureStyleCombo->count() == 0) {
    m_pictureStyleCombo->addItem(tr("Disabled"));
    m_pictureStyleCombo->setDisabled(true);
    m_pictureStyleCombo->setMaximumWidth(fontMetrics().width(tr("Disabled")) +
                                         25);
  } else {
    m_pictureStyleCombo->setEnabled(true);
    m_pictureStyleCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentPictureStyle());
    m_pictureStyleCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_pictureStyleCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onCameraListComboActivated(int comboIndex) {
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  int cameraCount            = cameras.size();
#ifdef WITH_CANON
  cameraCount += m_stopMotion->m_canon->getCameraCount();
#endif
  if (cameraCount != m_cameraListCombo->count() - 1) return;

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
    m_cameraStatusLabel->hide();
    m_pickZoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
    m_zoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
    m_pickZoomButton->setChecked(false);
    m_zoomButton->setChecked(false);
    m_dslrFrame->hide();
    m_webcamFrame->hide();
    m_noCameraFrame->show();
    m_alwaysUseLiveViewImagesButton->hide();
    // if (m_tabBar->tabText(1) == tr("Settings")) {
    //    m_tabBar->removeTab(1);
    //}
  } else if (useWebcam) {
    m_resolutionCombo->show();
    m_resolutionCombo->setEnabled(true);
    m_resolutionLabel->show();
    if (m_captureFilterSettingsBtn) m_captureFilterSettingsBtn->show();
    m_cameraStatusLabel->hide();
    m_webcamFrame->show();
    m_dslrFrame->hide();
    m_noCameraFrame->hide();
    m_alwaysUseLiveViewImagesButton->hide();
    getWebcamStatus();
    // if (m_tabBar->tabText(1) == tr("Options")) {
    //    m_tabBar->insertTab(1, tr("Settings"));
    //}
    m_webcamLabel->setText(m_cameraListCombo->currentText());
  } else {
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
    if (m_captureFilterSettingsBtn) m_captureFilterSettingsBtn->hide();
    m_cameraStatusLabel->show();
    m_dslrFrame->show();
    m_webcamFrame->hide();
    m_noCameraFrame->hide();
    m_alwaysUseLiveViewImagesButton->show();
    // if (m_tabBar->tabText(1) == tr("Options")) {
    //  m_tabBar->insertTab(1, tr("Settings"));
    //}
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamResolutionsChanged() {
  m_resolutionCombo->clear();
  QList<QSize> resolutions = m_stopMotion->m_webcam->getWebcamResolutions();
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

////-----------------------------------------------------------------------------
//
// void StopMotionController::onWebcamFocusFldEdited() {
//    int value = m_webcamFocusSlider->getValue();
//    value = value + abs((value % 5) - 5);
//    m_stopMotion->m_webcam->setWebcamFocusValue(value);
//}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamAutofocusToggled(bool on) {
  m_stopMotion->m_webcam->setWebcamAutofocusStatus(!on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamFocusSliderChanged(int value) {
  // int value = m_webcamFocusSlider->getValue();
  value = value + abs((value % 5) - 5);
  m_stopMotion->m_webcam->setWebcamFocusValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamExposureSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamExposureValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamBrightnessSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamBrightnessValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamContrastSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamContrastValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamGainSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamGainValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamSaturationSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamSaturationValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::getWebcamStatus() {
  m_webcamAutoFocusGB->setChecked(
      !m_stopMotion->m_webcam->getWebcamAutofocusStatus());
  m_webcamFocusSlider->setValue(m_stopMotion->m_webcam->getWebcamFocusValue());
  m_webcamExposureSlider->setValue(
      m_stopMotion->m_webcam->getWebcamExposureValue());
  m_webcamBrightnessSlider->setValue(
      m_stopMotion->m_webcam->getWebcamBrightnessValue());
  m_webcamContrastSlider->setValue(
      m_stopMotion->m_webcam->getWebcamContrastValue());
  m_webcamGainSlider->setValue(m_stopMotion->m_webcam->getWebcamGainValue());
  m_webcamSaturationSlider->setValue(
      m_stopMotion->m_webcam->getWebcamSaturationValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureFilterSettingsBtnPressed() {
  if (!m_stopMotion->m_webcam->getWebcam() ||
      m_stopMotion->m_webcam->getWebcamDeviceName().isNull())
    return;

  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  for (int c = 0; c < cameras.size(); c++) {
    if (cameras.at(c).deviceName() ==
        m_stopMotion->m_webcam->getWebcamDeviceName()) {
#ifdef _WIN32
      m_stopMotion->m_webcam->openSettingsWindow();
      // openCaptureFilterSettings(this, cameras.at(c).description());

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
#ifdef WITH_CANON
  m_apertureSlider->blockSignals(true);
  QStringList apertureOptions = m_stopMotion->m_canon->getApertureOptions();
  m_stopMotion->m_canon->setAperture(
      apertureOptions.at(m_apertureSlider->value()));
  m_apertureSlider->setRange(0, apertureOptions.size() - 1);
  m_apertureSlider->setValue(
      apertureOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentAperture()));
  m_apertureLabel->setText(tr("Aperture: ") +
                           m_stopMotion->m_canon->getCurrentAperture());
  m_apertureSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onApertureChangedSignal(QString text) {
#ifdef WITH_CANON
  m_apertureSlider->blockSignals(true);
  QStringList apertureOptions = m_stopMotion->m_canon->getApertureOptions();
  m_apertureLabel->setText(tr("Aperture: ") +
                           m_stopMotion->m_canon->getCurrentAperture());
  m_apertureSlider->setRange(0, apertureOptions.size() - 1);
  m_apertureSlider->setValue(
      apertureOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentAperture()));
  m_apertureSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onShutterSpeedChanged(int index) {
#ifdef WITH_CANON
  m_shutterSpeedSlider->blockSignals(true);
  QStringList shutterSpeedOptions =
      m_stopMotion->m_canon->getShutterSpeedOptions();
  m_stopMotion->m_canon->setShutterSpeed(
      shutterSpeedOptions.at(m_shutterSpeedSlider->value()));
  m_shutterSpeedSlider->setRange(0, shutterSpeedOptions.size() - 1);
  m_shutterSpeedSlider->setValue(shutterSpeedOptions.lastIndexOf(
      m_stopMotion->m_canon->getCurrentShutterSpeed()));
  m_shutterSpeedLabel->setText(tr("Shutter Speed: ") +
                               m_stopMotion->m_canon->getCurrentShutterSpeed());
  m_shutterSpeedSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onShutterSpeedChangedSignal(QString text) {
#ifdef WITH_CANON
  m_shutterSpeedSlider->blockSignals(true);
  QStringList shutterSpeedOptions =
      m_stopMotion->m_canon->getShutterSpeedOptions();
  m_shutterSpeedLabel->setText(tr("Shutter Speed: ") +
                               m_stopMotion->m_canon->getCurrentShutterSpeed());
  m_shutterSpeedSlider->setRange(0, shutterSpeedOptions.size() - 1);
  m_shutterSpeedSlider->setValue(shutterSpeedOptions.lastIndexOf(
      m_stopMotion->m_canon->getCurrentShutterSpeed()));
  m_shutterSpeedSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onIsoChanged(int index) {
#ifdef WITH_CANON
  m_isoSlider->blockSignals(true);
  QStringList isoOptions = m_stopMotion->m_canon->getIsoOptions();
  m_stopMotion->m_canon->setIso(isoOptions.at(m_isoSlider->value()));
  m_isoSlider->setRange(0, isoOptions.size() - 1);
  m_isoSlider->setValue(
      isoOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentIso()));
  m_isoLabel->setText(tr("Iso: ") + m_stopMotion->m_canon->getCurrentIso());
  m_isoSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onIsoChangedSignal(QString text) {
#ifdef WITH_CANON
  m_isoSlider->blockSignals(true);
  QStringList isoOptions = m_stopMotion->m_canon->getIsoOptions();
  m_isoSlider->setRange(0, isoOptions.size() - 1);
  m_isoSlider->setValue(
      isoOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentIso()));
  m_isoLabel->setText(tr("Iso: ") + m_stopMotion->m_canon->getCurrentIso());
  m_isoSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onExposureChanged(int index) {
#ifdef WITH_CANON
  m_exposureCombo->blockSignals(true);
  m_stopMotion->m_canon->setExposureCompensation(
      m_exposureCombo->currentText());
  m_exposureCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onExposureChangedSignal(QString text) {
#ifdef WITH_CANON
  m_exposureCombo->blockSignals(true);
  m_exposureCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentExposureCompensation());
  m_exposureCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onWhiteBalanceChanged(int index) {
#ifdef WITH_CANON
  m_whiteBalanceCombo->blockSignals(true);
  m_stopMotion->m_canon->setWhiteBalance(m_whiteBalanceCombo->currentText());
  m_whiteBalanceCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onWhiteBalanceChangedSignal(QString text) {
#ifdef WITH_CANON
  m_whiteBalanceCombo->blockSignals(true);
  m_whiteBalanceCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentWhiteBalance());
  refreshColorTemperatureList();
  m_whiteBalanceCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTemperatureChanged(int index) {
#ifdef WITH_CANON
  m_kelvinSlider->blockSignals(true);
  QStringList kelvinOptions =
      m_stopMotion->m_canon->getColorTemperatureOptions();
  m_stopMotion->m_canon->setColorTemperature(
      kelvinOptions.at(m_kelvinSlider->value()));
  m_kelvinSlider->setRange(0, kelvinOptions.size() - 1);
  m_kelvinSlider->setValue(kelvinOptions.lastIndexOf(
      m_stopMotion->m_canon->getCurrentColorTemperature()));
  m_kelvinValueLabel->setText(
      tr("Temperature: ") +
      m_stopMotion->m_canon->getCurrentColorTemperature());
  m_kelvinSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTemperatureChangedSignal(QString text) {
#ifdef WITH_CANON
  m_kelvinSlider->blockSignals(true);
  QStringList kelvinOptions =
      m_stopMotion->m_canon->getColorTemperatureOptions();
  m_kelvinSlider->setRange(0, kelvinOptions.size() - 1);
  m_kelvinSlider->setValue(kelvinOptions.lastIndexOf(
      m_stopMotion->m_canon->getCurrentColorTemperature()));
  m_kelvinValueLabel->setText(
      tr("Temperature: ") +
      m_stopMotion->m_canon->getCurrentColorTemperature());
  m_kelvinSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onImageQualityChanged(int index) {
#ifdef WITH_CANON
  m_imageQualityCombo->blockSignals(true);
  m_stopMotion->m_canon->setImageQuality(m_imageQualityCombo->currentText());
  m_imageQualityCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onImageQualityChangedSignal(QString text) {
#ifdef WITH_CANON
  m_imageQualityCombo->blockSignals(true);
  m_imageQualityCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentImageQuality());
  m_imageQualityCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPictureStyleChanged(int index) {
#ifdef WITH_CANON
  m_pictureStyleCombo->blockSignals(true);
  m_stopMotion->m_canon->setPictureStyle(m_pictureStyleCombo->currentText());
  m_pictureStyleCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPictureStyleChangedSignal(QString text) {
#ifdef WITH_CANON
  m_pictureStyleCombo->blockSignals(true);
  m_pictureStyleCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentPictureStyle());
  m_pictureStyleCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusCheckToggled(bool on) {
#ifdef WITH_CANON
  if (on) {
    m_zoomButton->setStyleSheet("border:1px solid rgba(0, 255, 0, 255);");
  } else {
    m_zoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
  }
  m_zoomButton->blockSignals(true);
  m_zoomButton->setChecked(on);
  m_zoomButton->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPickFocusCheckToggled(bool on) {
#ifdef WITH_CANON
  if (on) {
    m_pickZoomButton->setStyleSheet("border:1px solid rgba(0, 255, 0, 255);");

  } else {
    m_pickZoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
  }
  m_pickZoomButton->blockSignals(true);
  m_pickZoomButton->setChecked(on);
  m_pickZoomButton->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onZoomPressed() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->zoomLiveView();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPickZoomPressed() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->toggleZoomPicking();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusNear();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusFar();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear2() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusNear2();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar2() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusFar2();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear3() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusNear3();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar3() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusFar3();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::showEvent(QShowEvent *event) {
  bool hasCanon  = false;
  bool hasWebcam = false;
#ifdef WITH_CANON
  m_stopMotion->m_canon->initializeCanonSDK();
  if (!m_stopMotion->m_canon->m_sessionOpen) {
    m_dslrFrame->hide();
    m_alwaysUseLiveViewImagesButton->hide();
  } else {
    m_dslrFrame->show();
    m_alwaysUseLiveViewImagesButton->show();
    hasCanon = true;
  }
#else
  m_dslrFrame->hide();
  m_alwaysUseLiveViewImagesButton->hide();
#endif
  if (!m_stopMotion->m_usingWebcam) {
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
    m_webcamFrame->hide();
  } else {
    hasWebcam = true;
    m_webcamFrame->show();
  }

  if (!hasWebcam && !hasCanon) {
    m_noCameraFrame->show();
  } else {
    m_noCameraFrame->hide();
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::hideEvent(QHideEvent *event) {
  // stop interval timer if it is active
  if (m_timerCB->isChecked() && m_captureButton->isChecked()) {
    m_captureButton->setChecked(false);
    onCaptureButtonClicked(false);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::keyPressEvent(QKeyEvent *event) {
  int key          = event->key();
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  int origFrame    = fh->getFrame();
#ifdef WITH_CANON
  if ((m_stopMotion->m_canon->m_pickLiveViewZoom ||
       m_stopMotion->m_canon->m_zooming) &&
      (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up ||
       key == Qt::Key_Down || key == Qt::Key_2 || key == Qt::Key_4 ||
       key == Qt::Key_6 || key == Qt::Key_8)) {
    if (m_stopMotion->m_canon->m_liveViewZoomReadyToPick == true) {
      if (key == Qt::Key_Left || key == Qt::Key_4) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.x -= 10;
      }
      if (key == Qt::Key_Right || key == Qt::Key_6) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.x += 10;
      }
      if (key == Qt::Key_Up || key == Qt::Key_8) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.y += 10;
      }
      if (key == Qt::Key_Down || key == Qt::Key_2) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.y -= 10;
      }
      if (m_stopMotion->m_canon->m_zooming) {
        m_stopMotion->m_canon->setZoomPoint();
      }
    }
    m_stopMotion->m_canon->calculateZoomPoint();
    event->accept();
  } else if (m_stopMotion->m_canon->m_pickLiveViewZoom &&
             (key == Qt::Key_Escape || key == Qt::Key_Enter ||
              key == Qt::Key_Return)) {
    m_stopMotion->m_canon->toggleZoomPicking();
  } else
#endif
      if (key == Qt::Key_Up || key == Qt::Key_Left) {
    fh->prevFrame();
    event->accept();
  } else if (key == Qt::Key_Down || key == Qt::Key_Right) {
    fh->nextFrame();
    event->accept();
  } else if (key == Qt::Key_Home) {
    fh->firstFrame();
    event->accept();
  } else if (key == Qt::Key_End) {
    fh->lastFrame();
    event->accept();
  } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    m_captureButton->animateClick();
    event->accept();
  }

  else
    event->ignore();
}
//
////-----------------------------------------------------------------------------
//
// void StopMotionController::mousePressEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewToggleClicked() {
  m_stopMotion->toggleLiveView();
}

//-----------------------------------------------------------------------------

void StopMotionController::onAlwaysUseLiveViewImagesButtonClicked() {
  m_stopMotion->toggleAlwaysUseLiveViewImages();
}

//-----------------------------------------------------------------------------

void StopMotionController::onAlwaysUseLiveViewImagesToggled(bool on) {
  m_alwaysUseLiveViewImagesButton->blockSignals(true);
  m_alwaysUseLiveViewImagesButton->setChecked(on);
  m_alwaysUseLiveViewImagesButton->blockSignals(false);
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
  if (m_timerCB->isChecked()) {
    // start interval capturing
    if (on) {
      m_stopMotion->startInterval();
    }
    // stop interval capturing
    else {
      m_stopMotion->stopInterval();
    }
  }
  // capture immediately
  else {
    m_stopMotion->captureImage();
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalTimerCBToggled(bool on) {
  m_stopMotion->toggleInterval(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalSliderValueChanged(bool on) {
  m_stopMotion->setIntervalDSec(m_timerIntervalFld->getValue() * 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalCaptureTimerTimeout() {
  if (m_stopMotion->m_liveViewStatus < 1) {
    onCaptureButtonClicked(false);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalCountDownTimeout() {
  m_captureButton->setText(QString::number(
      m_stopMotion->m_intervalTimer->isActive()
          ? (m_stopMotion->m_intervalTimer->remainingTime() / 1000 + 1)
          : 0));
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalAmountChanged(int value) {
  m_timerIntervalFld->blockSignals(true);
  m_timerIntervalFld->setValue(value / 10.0);
  m_timerIntervalFld->blockSignals(false);
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalToggled(bool on) {
  m_timerCB->blockSignals(true);
  m_captureButton->setCheckable(on);
  if (on)
    m_captureButton->setText(tr("Start Capturing"));
  else
    m_captureButton->setText(tr("Capture"));
  m_timerCB->blockSignals(false);
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalStarted() {
  m_captureButton->setText(tr("Stop Capturing"));
  m_timerCB->setDisabled(true);
  m_timerIntervalFld->setDisabled(true);
  m_captureButton->blockSignals(true);
  m_captureButton->setChecked(true);
  m_captureButton->blockSignals(false);
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalStopped() {
  m_captureButton->setText(tr("Start Capturing"));
  m_timerCB->setDisabled(false);
  m_timerIntervalFld->setDisabled(false);
  m_captureButton->blockSignals(true);
  m_captureButton->setChecked(false);
  m_captureButton->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlaySoundToggled(bool on) {
  m_stopMotion->setPlayCaptureSound(on);
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
  m_stopMotion->setFrameNumber(m_frameNumberEdit->getValue().getNumber());
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

void StopMotionController::serialPortChanged(int index) {
  m_stopMotion->m_serial->setSerialPort(m_controlDeviceCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::updateStopMotion() {}
