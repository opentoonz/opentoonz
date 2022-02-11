#pragma once

#ifndef PENCILTESTPOPUP_H
#define PENCILTESTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonz/namebuilder.h"
#include "tfilepath.h"
#include "toonz/tproject.h"

#include <QAbstractVideoSurface>
#include <QRunnable>

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
class IntLineEdit;
}  // namespace DVGui

class CameraCaptureLevelControl;

class ApplyLutTask : public QRunnable {
protected:
  int m_fromY, m_toY;
  QImage& m_img;
  std::vector<int>& m_lut;

public:
  ApplyLutTask(int from, int to, QImage& img, std::vector<int>& lut)
      : m_fromY(from), m_toY(to), m_img(img), m_lut(lut) {}

private:
  virtual void run() override;
};

class ApplyGrayLutTask : public ApplyLutTask {
public:
  ApplyGrayLutTask(int from, int to, QImage& img, std::vector<int>& lut)
      : ApplyLutTask(from, to, img, lut) {}

private:
  void run() override;
};

//=============================================================================
// MyVideoSurface
//-----------------------------------------------------------------------------

class QVideoSurfaceFormat;
class MyVideoSurface : public QAbstractVideoSurface {
  Q_OBJECT
public:
  MyVideoSurface(QWidget* widget, QObject* parent = 0);

  QList<QVideoFrame::PixelFormat> supportedPixelFormats(
      QAbstractVideoBuffer::HandleType handleType =
          QAbstractVideoBuffer::NoHandle) const;
  bool isFormatSupported(const QVideoSurfaceFormat& format,
                         QVideoSurfaceFormat* similar) const;

  bool start(const QVideoSurfaceFormat& format);
  void stop();

  bool present(const QVideoFrame& frame);

  QRect videoRect() const { return m_targetRect; }
  QRect sourceRect() const { return m_sourceRect; }
  void updateVideoRect();

  QTransform transform() { return m_S2V_Transform; }

private:
  QWidget* m_widget;
  QImage::Format m_imageFormat;
  QRect m_targetRect;
  QSize m_imageSize;
  QRect m_sourceRect;
  QVideoFrame m_currentFrame;

  QTransform m_S2V_Transform;  // surface to video transform

signals:
  void frameCaptured(QImage& image);
};

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
  ~MyVideoWidget();

  void setImage(const QImage& image) {
    m_image = image;
    update();
  }
  QAbstractVideoSurface* videoSurface() const { return m_surface; }

  QSize sizeHint() const;

  void setShowOnionSkin(bool on) { m_showOnionSkin = on; }
  void setOnionOpacity(int value) { m_onionOpacity = value; }
  void setPreviousImage(QImage prevImage) { m_previousImage = prevImage; }

  void showCountDownTime(int time) {
    m_countDownTime = time;
    repaint();
  }

  void setSubCameraSize(QSize size);
  QRect subCameraRect() { return m_subCameraRect; }

protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

private:
  MyVideoSurface* m_surface;

protected slots:
  void onUpsideDownChecked(bool on) { m_upsideDown = on; }

signals:
  void startCamera();
  void stopCamera();
  void subCameraResized(bool isDragging);
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
// PencilTestPopup
//-----------------------------------------------------------------------------

class PencilTestPopup : public DVGui::Dialog {
  Q_OBJECT

  QCamera* m_currentCamera;
  QString m_deviceName;
  MyVideoWidget* m_videoWidget;

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

  QPushButton* m_subcameraButton;
  DVGui::IntLineEdit *m_subWidthFld, *m_subHeightFld;
  QSize m_allowedCameraSize;

  bool m_captureWhiteBGCue;
  bool m_captureCue;
  bool m_alwaysOverwrite = false;

  void processImage(QImage& procImage);
  bool importImage(QImage image);

  void setToNextNewLevel();
  void updateLevelNameAndFrame(std::wstring levelName);

public:
  PencilTestPopup();
  ~PencilTestPopup();

protected:
  void showEvent(QShowEvent* event);
  void hideEvent(QHideEvent* event);
  void keyPressEvent(QKeyEvent* event);

  bool event(QEvent* e) override;

protected slots:
  void refreshCameraList();
  void onCameraListComboActivated(int index);
  void onResolutionComboActivated(const QString&);
  void onFileFormatOptionButtonPressed();
  void onLevelNameEdited();
  void onNextName();
  void onPreviousName();
  void onColorTypeComboChanged(int index);
  void onFrameCaptured(QImage& image);
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
  void onSubCameraResized(bool isDragging);
  void onSubCameraSizeEdited();

public slots:
  void openSaveInFolderPopup();
};

#endif