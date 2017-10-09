#pragma once

#ifndef PENCILTESTPOPUP_H
#define PENCILTESTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonz/namebuilder.h"

#include <QFrame>

// forward decl.
class QCamera;
class QCameraImageCapture;

class QComboBox;
class QLineEdit;
class QSlider;
class QCheckBox;
class QPushButton;
class QVideoFrame;
class QTimer;
class QIntValidator;
class QRegExpValidator;
class QPushButton;
#ifdef MACOSX
class QCameraViewfinder;
#endif

namespace DVGui {
class FileField;
class IntField;
}

class CameraCaptureLevelControl;

//=============================================================================
// MyViewFinder
//-----------------------------------------------------------------------------

class MyViewFinder : public QFrame {
  Q_OBJECT

  QImage m_image;
  QImage m_previousImage;
  QCamera* m_camera;
  QRect m_imageRect;

  int m_countDownTime;

  bool m_showOnionSkin;
  int m_onionOpacity;
  bool m_upsideDown;

public:
  MyViewFinder(QWidget* parent = 0);
  void setImage(const QImage& image) {
    m_image = image;
    update();
  }
  void setCamera(QCamera* camera) { m_camera = camera; }
  void setShowOnionSkin(bool on) { m_showOnionSkin = on; }
  void setOnionOpacity(int value) { m_onionOpacity = value; }
  void setPreviousImage(QImage& prevImage) { m_previousImage = prevImage; }

  void showCountDownTime(int time) {
    m_countDownTime = time;
    repaint();
  }

  void updateSize();

protected:
  void paintEvent(QPaintEvent* event);
  void resizeEvent(QResizeEvent* event);
protected slots:
  void onUpsideDownChecked(bool on) { m_upsideDown = on; }
};

//=============================================================================
// FrameNumberLineEdit
// a special Line Edit which accepts imputting alphabets if the preference
// option
// "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
//-----------------------------------------------------------------------------

class FrameNumberLineEdit : public DVGui::LineEdit {
  Q_OBJECT
  /* having two validators and switch them according to the preferences*/
  QIntValidator* m_intValidator;
  QRegExpValidator* m_regexpValidator;

  void updateValidator();

public:
  FrameNumberLineEdit(QWidget* parent = 0, int value = 1);
  ~FrameNumberLineEdit() {}

  /*! Set text in field to \b value. */
  void setValue(int value);
  /*! Return an integer with text field value. */
  int getValue();

protected:
  /*! If focus is lost and current text value is out of range emit signal
  \b editingFinished.*/
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
// PencilTestPopup
//-----------------------------------------------------------------------------

class PencilTestPopup : public DVGui::Dialog {
  Q_OBJECT

  QCamera* m_currentCamera;
  QString m_deviceName;
  MyViewFinder* m_cameraViewfinder;
  QCameraImageCapture* m_cameraImageCapture;

  QComboBox *m_cameraListCombo, *m_resolutionCombo, *m_fileTypeCombo,
      *m_colorTypeCombo;
  LevelNameLineEdit* m_levelNameEdit;
  QCheckBox *m_upsideDownCB, *m_onionSkinCB, *m_saveOnCaptureCB, *m_timerCB;
  QPushButton *m_fileFormatOptionButton, *m_captureWhiteBGButton,
      *m_captureButton, *m_loadImageButton;
  DVGui::FileField* m_saveInFileFld;
  FrameNumberLineEdit* m_frameNumberEdit;
  DVGui::IntField *m_bgReductionFld, *m_onionOpacityFld, *m_timerIntervalFld;

  QTimer *m_captureTimer, *m_countdownTimer;

  QImage m_whiteBGImg;

  // used only for Windows
  QPushButton* m_captureFilterSettingsBtn;

  PencilTestSaveInFolderPopup* m_saveInFolderPopup;

  CameraCaptureLevelControl* m_camCapLevelControl;

  QLabel* m_frameInfoLabel;

  QToolButton* m_previousLevelButton;

#ifdef MACOSX
  QCameraViewfinder* m_dummyViewFinder;
#endif

  int m_timerId;
  QString m_cacheImagePath;
  bool m_captureWhiteBGCue;
  bool m_captureCue;

  void processImage(QImage& procImage);
  bool importImage(QImage& image);

  void setToNextNewLevel();
  void updateLevelNameAndFrame(std::wstring levelName);

public:
  PencilTestPopup();
  ~PencilTestPopup();

protected:
  void timerEvent(QTimerEvent* event);
  void showEvent(QShowEvent* event);
  void hideEvent(QHideEvent* event);

  void keyPressEvent(QKeyEvent* event);

protected slots:
  void refreshCameraList();
  void onCameraListComboActivated(int index);
  void onResolutionComboActivated(const QString&);
  void onFileFormatOptionButtonPressed();
  void onLevelNameEdited();
  void onNextName();
  void onPreviousName();
  void onColorTypeComboChanged(int index);
  void onImageCaptured(int, const QImage&);
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

public slots:
  void openSaveInFolderPopup();
};

#endif