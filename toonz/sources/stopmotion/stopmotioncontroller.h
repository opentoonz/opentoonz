#pragma once

#ifndef STOPMOTIONCONTROLLER_H
#define STOPMOTIONCONTROLLER_H

// TnzCore includes
#include "stopmotion.h"
#include "penciltestpopup.h"

// TnzQt includes
#include "toonzqt/tabbar.h"
#include "toonzqt/gutil.h"

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
  QStackedWidget *m_stackedChooser;
  TabBarContainter *m_tabBarContainer;  //!< Tabs container for style types.
  QPushButton *m_toggleLiveViewButton, *m_setToCurrentXSheetFrameButton;
  QPushButton *m_fileFormatOptionButton, *m_captureButton, *m_zoomButton,
      *m_pickZoomButton, *m_focusNearButton, *m_focusFarButton,
      *m_focusNear2Button, *m_focusNear3Button, *m_focusFar2Button,
      *m_focusFar3Button, *m_captureFilterSettingsBtn;
  QHBoxLayout *m_focusAndZoomLayout;
  QLabel *m_frameInfoLabel, *m_cameraSettingsLabel, *m_cameraModeLabel,
      *m_kelvinLabel, *m_resolutionLabel, *m_directShowLabel;
  QToolButton *m_previousLevelButton, *m_previousFrameButton,
      *m_previousXSheetFrameButton;

  QComboBox *m_cameraListCombo, *m_fileTypeCombo, *m_isoCombo,
      *m_shutterSpeedCombo, *m_exposureCombo, *m_apertureCombo,
      *m_whiteBalanceCombo, *m_kelvinCombo, *m_resolutionCombo,
      *m_imageQualityCombo, *m_pictureStyleCombo;
  LevelNameLineEdit *m_levelNameEdit;
  QCheckBox *m_blackScreenForCapture, *m_useScaledFullSizeImages,
      *m_placeOnXSheetCB, *m_directShowCB, *m_liveViewOnAllFramesCB,
      *m_useMjpgCB, *m_useNumpadCB;
  DVGui::FileField *m_saveInFileFld;
  DVGui::IntLineEdit *m_xSheetFrameNumberEdit;
  FrameNumberLineEdit *m_frameNumberEdit;
  DVGui::IntField *m_onionOpacityFld, *m_postCaptureReviewFld,
      *m_subsamplingFld;
  PencilTestSaveInFolderPopup *m_saveInFolderPopup;

public:
  StopMotionController(QWidget *parent = 0);
  ~StopMotionController();

protected:
  void updateStopMotion();
  void showEvent(QShowEvent *event);
  void hideEvent(QHideEvent *event);
  // void mousePressEvent(QMouseEvent *event) override;
  // void keyPressEvent(QKeyEvent *event);

protected slots:
  void refreshCameraList();
  void refreshCameraListCalled();
  void refreshOptionsLists();
  void refreshApertureList();
  void refreshShutterSpeedList();
  void refreshIsoList();
  void refreshExposureList();
  void refreshWhiteBalanceList();
  void refreshColorTemperatureList();
  void refreshImageQualityList();
  void refreshPictureStyleList();
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
  void updateDimensions();
  void onSaveInPathEdited();
  void onSceneSwitched();
  void onPreviousXSheetFrame();
  void onNextXSheetFrame();
  void setToCurrentXSheetFrame();
  void onApertureChanged(int index);
  void onShutterSpeedChanged(int index);
  void onIsoChanged(int index);
  void onExposureChanged(int index);
  void onWhiteBalanceChanged(int index);
  void onColorTemperatureChanged(int index);
  void onImageQualityChanged(int index);
  void onPictureStyleChanged(int index);
  void refreshMode();
  void onZoomPressed();
  void onPickZoomPressed();
  void onFocusNear();
  void onFocusFar();
  void onFocusNear2();
  void onFocusFar2();
  void onFocusNear3();
  void onFocusFar3();
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
  void onUseNumpadSignal(bool);
  void onLiveViewChanged(bool);
  void onNewCameraSelected(int, bool);
  void onWebcamResolutionsChanged();
  void onNewWebcamResolutionSelected(int);
  void onApertureChangedSignal(QString);
  void onIsoChangedSignal(QString);
  void onShutterSpeedChangedSignal(QString);
  void onExposureChangedSignal(QString);
  void onWhiteBalanceChangedSignal(QString);
  void onColorTemperatureChangedSignal(QString);
  void onImageQualityChangedSignal(QString);
  void onPictureStyleChangedSignal(QString);

public slots:
  void openSaveInFolderPopup();
};

#endif  // STOPMOTIONCONTROLLER_H
