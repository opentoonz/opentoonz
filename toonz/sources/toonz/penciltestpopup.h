#pragma once

#ifndef PENCILTESTPOPUP_H
#define PENCILTESTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonz/namebuilder.h"
#include "opencv2/opencv.hpp"
#include "tfilepath.h"
#include "toonz/tproject.h"
#include "filebrowserpopup.h"

#include <QAbstractVideoSurface>
#include <QRunnable>
#include <QLineEdit>
#include <QPushButton>

// forward decl.
class QCamera;
class QCameraImageCapture;

class QComboBox;
class QSlider;
class QCheckBox;
class QVideoFrame;
class QTimer;
class QIntValidator;
class QRegExpValidator;
class QLabel;
class QGroupBox;
class QRadioButton;
#ifdef MACOSX
class QCameraViewfinder;
#endif

namespace DVGui {
class FileField;
class IntField;
class IntLineEdit;
}  // namespace DVGui

class CameraCaptureLevelControl;

//=============================================================================
// MyVideoWidget
//-----------------------------------------------------------------------------

class MyVideoWidget : public QWidget {
  Q_OBJECT

  QImage m_image;
  QImage m_previousImage;
  int m_countDownTime;
  bool m_showOnionSkin;
  int m_onionOpacity;
  bool m_upsideDown;

  QRect m_subCameraRect;
  QRect m_preSubCameraRect;
  QPoint m_dragStartPos;

  QRect m_targetRect;
  QTransform m_S2V_Transform;  // surface to video transform
  enum SUBHANDLE {
    HandleNone,
    HandleFrame,
    HandleTopLeft,
    HandleTopRight,
    HandleBottomLeft,
    HandleBottomRight,
    HandleLeft,
    HandleTop,
    HandleRight,
    HandleBottom
  } m_activeSubHandle = HandleNone;
  void drawSubCamera(QPainter&);

public:
  MyVideoWidget(QWidget* parent = 0);

  void setImage(const QImage& image) {
    m_image = image;
    update();
  }

  void setShowOnionSkin(bool on) { m_showOnionSkin = on; }
  void setOnionOpacity(int value) { m_onionOpacity = value; }
  void setPreviousImage(QImage prevImage) { m_previousImage = prevImage; }

  void showCountDownTime(int time) {
    m_countDownTime = time;
    repaint();
  }

  void setSubCameraRect(QRect rect);
  QRect subCameraRect() { return m_subCameraRect; }

  void computeTransform(QSize imgSize);

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

protected slots:
  void onUpsideDownChecked(bool on) { m_upsideDown = on; }

signals:
  void startCamera();
  void stopCamera();
  void subCameraChanged(bool isDragging);
};

//=============================================================================
// FrameNumberLineEdit
// a special Line Edit which accepts imputting alphabets if the preference
// option
// "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
//-----------------------------------------------------------------------------

class FrameNumberLineEdit : public DVGui::LineEdit,
                            public TProjectManager::Listener {
  Q_OBJECT
  /* having two validators and switch them according to the preferences*/
  QRegExpValidator *m_regexpValidator, *m_regexpValidator_alt;

  void updateValidator();
  void updateSize();
  QString m_textOnFocusIn;

public:
  FrameNumberLineEdit(QWidget* parent = 0, TFrameId fId = TFrameId(1),
                      bool acceptLetter = true);
  ~FrameNumberLineEdit() {}

  /*! Set text in field to \b value. */
  void setValue(TFrameId fId);
  /*! Return an integer with text field value. */
  TFrameId getValue();

  // TProjectManager::Listener
  void onProjectSwitched() override;
  void onProjectChanged() override;

protected:
  /*! If focus is lost and current text value is out of range emit signal
  \b editingFinished.*/
  void focusInEvent(QFocusEvent*) override;
  void focusOutEvent(QFocusEvent*) override;
  void showEvent(QShowEvent* event) override { updateValidator(); }
};

//=============================================================================

class LevelNameLineEdit : public QLineEdit {
  Q_OBJECT
  QString m_textOnFocusIn;

public:
  LevelNameLineEdit(QWidget* parent = 0);

protected:
  void focusInEvent(QFocusEvent* e);

protected slots:
  void onEditingFinished();

signals:
  void levelNameEdited();
};

//=============================================================================
// FlexibleNameCreator
// Inherits NameCreator, added function for obtaining the previous name and
// setting the current name.

class FlexibleNameCreator final : public NameCreator {
public:
  FlexibleNameCreator() {}
  std::wstring getPrevious();
  bool setCurrent(std::wstring name);
};

//=============================================================================
// PencilTestSaveInFolderPopup
//-----------------------------------------------------------------------------

class PencilTestSaveInFolderPopup : public DVGui::Dialog {
  Q_OBJECT

  DVGui::FileField* m_parentFolderField;
  QLineEdit *m_projectField, *m_episodeField, *m_sequenceField, *m_sceneField,
      *m_subFolderNameField;

  QCheckBox *m_subFolderCB, *m_autoSubNameCB, *m_createSceneInFolderCB;
  QComboBox* m_subNameFormatCombo;

  void createSceneInFolder();

public:
  PencilTestSaveInFolderPopup(QWidget* parent = 0);
  QString getPath();
  QString getParentPath();
  void updateParentFolder();

protected:
  void showEvent(QShowEvent* event);

protected slots:
  void updateSubFolderName();
  void onAutoSubNameCBClicked(bool);
  void onShowPopupOnLaunchCBClicked(bool);
  void onCreateSceneInFolderCBClicked(bool);
  void onSetAsDefaultBtnPressed();
  void onOkPressed();
};

//=============================================================================
// SubCameraButton
// button with context menu to open preset
//-----------------------------------------------------------------------------

class SubCameraButton : public QPushButton {
  Q_OBJECT
  std::unique_ptr<QSettings> m_settings;

  QSize m_curResolution;
  QRect m_curSubCamera;
  double m_currentDpi;

public:
  SubCameraButton(const QString& text, QWidget* parent = 0);
  void setCurResolution(const QSize& size) { m_curResolution = size; }
  void setCurSubCamera(const QRect& rect) { m_curSubCamera = rect; }
  void setCurDpi(const double& dpi) { m_currentDpi = dpi; }

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
protected slots:
  void onSubCameraAct();
  void onSaveSubCamera();
  void onDeletePreset();
signals:
  void subCameraPresetSelected(const QRect&, const double dpi);
};

//=============================================================================
// PencilTestPopup
//-----------------------------------------------------------------------------

class PencilTestPopup : public DVGui::Dialog {
  Q_OBJECT

  QTimer* m_timer;
  cv::VideoCapture m_cvWebcam;
  QSize m_resolution;

  //--------

  QCamera* m_currentCamera;
  QString m_deviceName;
  MyVideoWidget* m_videoWidget;

  QComboBox *m_cameraListCombo, *m_resolutionCombo, *m_fileTypeCombo,
      *m_colorTypeCombo;
  LevelNameLineEdit* m_levelNameEdit;
  QCheckBox *m_upsideDownCB, *m_saveOnCaptureCB;
  QGroupBox *m_onionSkinGBox, *m_timerGBox;
  QPushButton *m_fileFormatOptionButton, *m_captureWhiteBGButton,
      *m_captureButton, *m_loadImageButton;
  DVGui::FileField* m_saveInFileFld;
  FrameNumberLineEdit* m_frameNumberEdit;
  DVGui::IntField *m_bgReductionFld, *m_onionOpacityFld, *m_timerIntervalFld;

  QTimer *m_captureTimer, *m_countdownTimer;

  cv::Mat m_whiteBGImg;

  // used only for Windows
  QPushButton* m_captureFilterSettingsBtn;

  PencilTestSaveInFolderPopup* m_saveInFolderPopup;

  CameraCaptureLevelControl* m_camCapLevelControl;

  QLabel* m_frameInfoLabel;

  QToolButton* m_previousLevelButton;

  SubCameraButton* m_subcameraButton;
  DVGui::IntLineEdit *m_subWidthFld, *m_subHeightFld;
  DVGui::IntLineEdit *m_subXPosFld, *m_subYPosFld;
  QSize m_allowedCameraSize;

  bool m_captureWhiteBGCue;
  bool m_captureCue;
  bool m_alwaysOverwrite = false;
  bool m_useMjpg;
#ifdef _WIN32
  bool m_useDirectShow;
#endif

  // calibration feature
  struct Calibration {
    // Parameters
    bool captureCue    = false;
    cv::Size boardSize = {10, 7};
    int refCaptured    = 0;
    std::vector<std::vector<cv::Point3f>> obj_points;
    std::vector<std::vector<cv::Point2f>> image_points;
    cv::Mat mapX, mapY;
    bool isValid = false;
    // UIs
    QPushButton *capBtn, *newBtn, *loadBtn, *cancelBtn, *exportBtn;
    QLabel* label;
    QGroupBox* groupBox;
  } m_calibration;

  // dpi settings
  bool m_doAutoDpi;
  double m_customDpi;
  QWidget* m_dpiMenuWidget;
  QPushButton* m_dpiBtn;
  QRadioButton *m_autoDpiRadioBtn, *m_customDpiRadioBtn;
  QLineEdit* m_customDpiField;

  void captureCalibrationRefImage(cv::Mat& procImage);

  void processImage(cv::Mat& procImage);
  bool importImage(QImage image);

  void setToNextNewLevel();
  void updateLevelNameAndFrame(std::wstring levelName);

  void getWebcamImage();

  QMenu* createOptionsMenu();

  int translateIndex(int camIndex);

  QString getCurrentCalibFilePath();

  QWidget* createDpiMenuWidget();

public:
  PencilTestPopup();
  ~PencilTestPopup();

protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

  bool event(QEvent* e) override;

protected slots:
  void refreshCameraList();
  void onCameraListComboActivated(int index);
  void onResolutionComboActivated();
  void onFileFormatOptionButtonPressed();
  void onLevelNameEdited();
  void onNextName();
  void onPreviousName();
  void onColorTypeComboChanged(int index);
  void onFrameCaptured(cv::Mat& image);
  void onCaptureWhiteBGButtonPressed();
  void onOnionCBToggled(bool);
  void onLoadImageButtonPressed();
  void onOnionOpacityFldEdited();
  void onTimerCBToggled(bool);
  void onCaptureTimerTimeout();
  void onCountDown();

  void onCaptureButtonClicked(bool);
  void onCaptureFilterSettingsBtnPressed();

  void refreshFrameInfo();

  void onSaveInPathEdited();
  void onSceneSwitched();

  void onSubCameraToggled(bool);
  void onSubCameraChanged(bool isDragging);
  void onSubCameraRectEdited();
  void onSubCameraPresetSelected(const QRect&, const double);

  void onTimeout();

  void onCalibCapBtnClicked();
  void onCalibNewBtnClicked();
  void resetCalibSettingsFromFile();
  void onCalibLoadBtnClicked();
  void onCalibExportBtnClicked();
  void onCalibReadme();
  void onPreferenceChanged(const QString&);

public slots:
  void openSaveInFolderPopup();
};

//=============================================================================

class ExportCalibrationFilePopup final : public GenericSaveFilePopup {
  Q_OBJECT
public:
  ExportCalibrationFilePopup(QWidget* parent);

protected:
  void showEvent(QShowEvent*) override;
};

//=============================================================================

class LoadCalibrationFilePopup final : public GenericLoadFilePopup {
  Q_OBJECT
public:
  LoadCalibrationFilePopup(QWidget* parent);

protected:
  void showEvent(QShowEvent*) override;
};

#endif
