#pragma once

#ifndef STOPMOTIONCONTROLLER_H
#define STOPMOTIONCONTROLLER_H

// TnzCore includes
#include "stopmotion.h"
#include "penciltestpopup.h"

// TnzQt includes
#include "toonzqt/tabbar.h"
#include "toonzqt/gutil.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/doublefield.h"

// Qt includes
#include <QWidget>
#include <QFrame>
#include <QTabBar>
#include <QSlider>
#include <QScrollArea>
#include <QPointF>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================

//    Forward declarations

class TColorStyle;
class TPalette;

class TXshLevelHandle;

class QGridLayout;
class QLabel;
class QStackedWidget;
class QSlider;
class QRadioButton;
class QButtonGroup;
class QPushButton;
class QTabWidget;
class QToolBar;
class QTimer;
class QGroupBox;

//=============================================================================
// StopMotionController
//-----------------------------------------------------------------------------

class StopMotionController final : public QWidget {
  Q_OBJECT
  StopMotion *m_stopMotion;

  QWidget *m_parent;
  TXshLevelHandle
      *m_levelHandle;  //!< for clearing the level cache when the color changed

  DVGui::TabBar *m_tabBar;
  QFrame *m_mainControlsPage;
  QFrame *m_cameraSettingsPage;
  QFrame *m_optionsPage;
  QFrame *m_motionPage;
  QFrame *m_lightPage;
  QFrame *m_dslrFrame;
  QFrame *m_webcamFrame;
  QFrame *m_noCameraFrame;
  QStackedWidget *m_stackedChooser;
  TabBarContainter *m_tabBarContainer;  //!< Tabs container for pages
  QPushButton *m_toggleLiveViewButton, *m_setToCurrentXSheetFrameButton,
      *m_alwaysUseLiveViewImagesButton;
  QPushButton *m_captureButton, *m_zoomButton, *m_fileFormatOptionButton,
      *m_pickZoomButton, *m_focusNearButton, *m_focusFarButton,
      *m_focusNear2Button, *m_focusNear3Button, *m_focusFar2Button,
      *m_focusFar3Button, *m_captureFilterSettingsBtn, *m_testLightsButton;
  QHBoxLayout *m_focusAndZoomLayout;
  QLabel *m_frameInfoLabel, *m_cameraSettingsLabel, *m_cameraModeLabel,
      *m_resolutionLabel, *m_cameraStatusLabel,
      *m_apertureLabel, *m_kelvinValueLabel, *m_isoLabel, *m_shutterSpeedLabel,
      *m_webcamLabel;
  QToolButton *m_previousLevelButton, *m_previousFrameButton,
      *m_previousXSheetFrameButton;
  QSlider *m_apertureSlider, *m_shutterSpeedSlider, *m_isoSlider,
      *m_kelvinSlider, *m_webcamFocusSlider, *m_webcamWhiteBalanceSlider,
      *m_webcamExposureSlider, *m_webcamBrightnessSlider,
      *m_webcamContrastSlider, *m_webcamGainSlider, *m_webcamSaturationSlider;
  QComboBox *m_cameraListCombo, *m_exposureCombo, *m_fileTypeCombo,
      *m_whiteBalanceCombo, *m_resolutionCombo, *m_imageQualityCombo,
      *m_pictureStyleCombo, *m_controlDeviceCombo;
  LevelNameLineEdit *m_levelNameEdit;
  QCheckBox *m_blackScreenForCapture, *m_useScaledFullSizeImages,
      *m_placeOnXSheetCB, *m_directShowCB, *m_liveViewOnAllFramesCB,
      *m_useMjpgCB, *m_useNumpadCB, *m_drawBeneathCB, *m_playSound;
  DVGui::FileField *m_saveInFileFld;
  DVGui::IntLineEdit *m_xSheetFrameNumberEdit;
  FrameNumberLineEdit *m_frameNumberEdit;
  DVGui::IntField *m_onionOpacityFld, *m_subsamplingFld;
  DVGui::DoubleField *m_postCaptureReviewFld;
  PencilTestSaveInFolderPopup *m_saveInFolderPopup;
  DVGui::DoubleField *m_timerIntervalFld;
  DVGui::ColorField *m_screen1ColorFld, *m_screen2ColorFld, *m_screen3ColorFld;
  QGroupBox *m_screen1Box;
  QGroupBox *m_screen2Box;
  QGroupBox *m_screen3Box;
  QGroupBox *m_webcamAutoFocusGB;
  QGroupBox *m_timerCB;
  QTimer *m_lightTestTimer;

public:
  StopMotionController(QWidget *parent = 0);
  ~StopMotionController();

protected:
  void updateStopMotion();
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  // void mousePressEvent(QMouseEvent *event) override;
  // void keyPressEvent(QKeyEvent *event);
  void keyPressEvent(QKeyEvent *event) override;

protected slots:
  void refreshCameraList(QString activeCamera = "");
  void refreshCameraListCalled();
  void refreshOptionsLists();
  void onCameraListComboActivated(int index);
  void onResolutionComboActivated(const QString &itemText);
  void onCaptureFilterSettingsBtnPressed();
  void onFileFormatOptionButtonPressed();
  void onLevelNameEdited();
  void onNextName();
  void onPreviousName();
  void onNextFrame();
  void onPreviousFrame();
  void onNextNewLevel();
  void onLastFrame();

  void onFileTypeActivated();
  void onFrameNumberChanged();
  void onXSheetFrameNumberChanged();
  void onFrameCaptured(QImage &image);
  void onOnionOpacityFldEdited();
  void onOnionOpacitySliderChanged(bool ignore);
  void onLiveViewToggleClicked();
  void onCaptureButtonClicked(bool);
  void setPage(int);
  void onScaleFullSizeImagesChanged(int checked);
  void onBlackScreenForCaptureChanged(int checked);
  void onPlaceOnXSheetChanged(int checked);
  void onUseMjpgChanged(int checked);
  void onUseDirectShowChanged(int checked);
  void onLiveViewOnAllFramesChanged(int checked);
  void onUseNumpadChanged(int checked);
  void onDrawBeneathChanged(int checked);
  void updateDimensions();
  void onSaveInPathEdited();
  void onSceneSwitched();
  void onPreviousXSheetFrame();
  void onNextXSheetFrame();
  void setToCurrentXSheetFrame();

  // motion control
  void serialPortChanged(int);

  // time lapse
  void onIntervalTimerCBToggled(bool);
  void onIntervalSliderValueChanged(bool);
  void onIntervalCaptureTimerTimeout();
  void onIntervalCountDownTimeout();
  void onIntervalAmountChanged(int);
  void onIntervalToggled(bool);
  void onIntervalStarted();
  void onIntervalStopped();

  // sound
  void onPlaySoundToggled(bool);

  // lights and screens
  void setScreen1Color(const TPixel32 &value, bool isDragging);
  void setScreen2Color(const TPixel32 &value, bool isDragging);
  void setScreen3Color(const TPixel32 &value, bool isDragging);
  void onScreen1OverlayToggled(bool);
  void onScreen2OverlayToggled(bool);
  void onScreen3OverlayToggled(bool);
  void onTestLightsPressed();
  void onTestLightsTimeout();
  void onScreen1ColorChanged(TPixel32);
  void onScreen2ColorChanged(TPixel32);
  void onScreen3ColorChanged(TPixel32);
  void onScreen1OverlayChanged(bool);
  void onScreen2OverlayChanged(bool);
  void onScreen3OverlayChanged(bool);

  // canon stuff
  void onApertureChanged(int index);
  void onShutterSpeedChanged(int index);
  void onIsoChanged(int index);
  void onExposureChanged(int index);
  void onWhiteBalanceChanged(int index);
  void onColorTemperatureChanged(int index);
  void onImageQualityChanged(int index);
  void onPictureStyleChanged(int index);
  void onZoomPressed();
  void onPickZoomPressed();
  void onFocusNear();
  void onFocusFar();
  void onFocusNear2();
  void onFocusFar2();
  void onFocusNear3();
  void onFocusFar3();
  void onApertureChangedSignal(QString);
  void onIsoChangedSignal(QString);
  void onShutterSpeedChangedSignal(QString);
  void onExposureChangedSignal(QString);
  void onWhiteBalanceChangedSignal(QString);
  void onColorTemperatureChangedSignal(QString);
  void onImageQualityChangedSignal(QString);
  void onPictureStyleChangedSignal(QString);
  void refreshApertureList();
  void refreshShutterSpeedList();
  void refreshIsoList();
  void refreshExposureList();
  void refreshWhiteBalanceList();
  void refreshColorTemperatureList();
  void refreshImageQualityList();
  void refreshPictureStyleList();
  void refreshMode();
  void onFocusCheckToggled(bool on);
  void onPickFocusCheckToggled(bool on);
  void onAlwaysUseLiveViewImagesButtonClicked();
  void onAlwaysUseLiveViewImagesToggled(bool);

  void onCaptureReviewFldEdited();
  void onCaptureReviewSliderChanged(bool ignore);
  void onSubsamplingFldEdited();
  void onSubsamplingSliderChanged(bool ignore);
  void onSubsamplingChanged(int);
  void onFilePathChanged(QString);
  void onLevelNameChanged(QString);
  void onFileTypeChanged(QString);
  void onXSheetFrameNumberChanged(int);
  void onFrameNumberChanged(int);
  void onFrameInfoTextChanged(QString);
  void onOpacityChanged(int opacity);
  void onScaleFullSizeImagesSignal(bool);
  void onBlackCaptureSignal(bool);
  void onLiveViewOnAllFramesSignal(bool);
  void onPlaceOnXSheetSignal(bool);
  void onUseMjpgSignal(bool);
  void onUseDirectShowSignal(bool);
  void onReviewTimeChangedSignal(int);
  void onPlayCaptureSignal(bool);
  void onUseNumpadSignal(bool);
  void onDrawBeneathSignal(bool);
  void onLiveViewChanged(bool);
  void onNewCameraSelected(int, bool);

  // webcam
  void onWebcamResolutionsChanged();
  void onNewWebcamResolutionSelected(int);
  void onWebcamAutofocusToggled(bool);
  void onWebcamFocusSliderChanged(int value);
  void onWebcamExposureSliderChanged(int value);
  void onWebcamBrightnessSliderChanged(int value);
  void onWebcamContrastSliderChanged(int value);
  void onWebcamGainSliderChanged(int value);
  void onWebcamSaturationSliderChanged(int value);
  void getWebcamStatus();

public slots:
  void openSaveInFolderPopup();
};

#endif  // STOPMOTIONCONTROLLER_H
