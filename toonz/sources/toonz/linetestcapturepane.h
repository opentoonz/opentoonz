#pragma once

#ifndef LINETESTCAPTUREPANE_H
#define LINETESTCAPTUREPANE_H
#ifdef LINETEST
#include "tnzcamera.h"
#include "trasterimage.h"
#include "pane.h"

#include "toonz/toonzscene.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/colorfield.h"

#include <QApplication>
#include <QMainWindow>
#include <QDialog>
#include <QPainter>
#include <QLineEdit>
#include <QMouseEvent>

//-----------------------------------------------------------------------------
// forward declaration
class QSlider;
class QLabel;
class QComboBox;
class CaptureParameters;

//=============================================================================
// LineTestImageViewer

class LineTestImageViewer : public QWidget, public CameraImageViewer
{
	Q_OBJECT

	QPoint m_pos;
	Qt::MouseButton m_mouseButton;
	//!Used to zoom and pan
	TAffine m_viewAffine;

	TRasterP m_raster;

	bool m_isPainting;

	bool m_isOnionSkinActive;
	bool m_isViewFrameActive;

public:
	LineTestImageViewer(QWidget *parent);

	void setImage(TRasterP ras);

	void setOnionSkinActive(bool value) { m_isOnionSkinActive = value; }
	void setViewFrameActive(bool value) { m_isViewFrameActive = value; }

	TAffine getNormalZoomScale();
	TAffine getViewMatrix() const { return m_viewAffine; }

	void zoomQt(bool forward, bool reset);

public slots:
	void resetView();

signals:
	void rasterChanged();
	void onZoomChanged();

protected:
	TRasterP getCurrentImage() const;
	TDimension getImageSize() const;

	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void enterEvent(QEvent *);
	void keyPressEvent(QKeyEvent *event);
	void contextMenuEvent(QContextMenuEvent *event);

	void wheelEvent(QWheelEvent *e);
	void panQt(const QPoint &delta);
	void zoomQt(const QPoint &center, double factor);

	void paintEvent(QPaintEvent *e);
};

//===================================================================
//ChooseCameraDialog

class ChooseCameraDialog : public DVGui::Dialog
{
	Q_OBJECT

	int m_cameraIndex;

public:
	ChooseCameraDialog(QList<QString> cameras);

	int getCurrentCameraIndex() const { return m_cameraIndex; }

protected slots:
	void onCurrentRowChanged(int currentRow);
	void onOkButtonPressed();
	void onCancelButtonPressed();
};

//===================================================================
// CaptureSettingsPopup

class CaptureSettingsPopup : public DVGui::Dialog
{
	Q_OBJECT

	QPushButton *m_defineDeviceButton;
	DVGui::CheckBox *m_useWhiteImage;
	QPushButton *m_keepWhiteImage;
	DVGui::IntLineEdit *m_imageWidthLineEdit;
	DVGui::IntLineEdit *m_imageHeightLineEdit;
	DVGui::IntField *m_brightnessField;
	DVGui::IntField *m_contrastField;
	DVGui::CheckBox *m_upsideDown;

public:
	CaptureSettingsPopup();

public slots:
	void updateWidgets();

protected:
	CaptureParameters *getCaptureParameters();

protected:
	void showEvent(QShowEvent *e);
	void hideEvent(QHideEvent *e);

protected slots:
	void defineDevice();
	void onUseWhiteImageStateChanged(int state);
	void onKeepWhiteImage();
	void onImageWidthEditingFinished();
	void onImageHeightEditingFinished();
	void onContrastChanged(bool);
	void onBrightnessChanged(bool);
	void onUpsideDownStateChanged(int);

signals:
	void newDeviceDefined();
};

//===================================================================
// FileSettingsPopup

class FileSettingsPopup : public DVGui::Dialog
{
	Q_OBJECT

	DVGui::FileField *m_pathField;
	QComboBox *m_fileFormat;

public:
	FileSettingsPopup();

public slots:
	void updateWidgets();

protected:
	CaptureParameters *getCaptureParameters();

protected slots:
	void onPathChanged();
	void onFormatChanged(const QString &);
	void openSettingsPopup();
};

//=============================================================================
// LineTestCapturePane

class LineTestCapturePane : public TPanel
{
	Q_OBJECT

	LineTestImageViewer *m_imageView;

	DVGui::LineEdit *m_nameField;
	DVGui::LineEdit *m_frameField;
	QComboBox *m_saveMode;
	DVGui::CheckBox *m_onionSkin;
	DVGui::CheckBox *m_viewFrame;
	DVGui::IntLineEdit *m_incrementField;
	DVGui::IntLineEdit *m_stepField;
	DVGui::CheckBox *m_connectionCheckBox;
	DVGui::ColorField *m_lineColorField;
	QPushButton *m_captureButton;

	CaptureSettingsPopup *m_captureSettingsPopup;
	FileSettingsPopup *m_fileSettingsPopup;

	int m_timerId;
	bool m_canCapture;

public:
	LineTestCapturePane(QWidget *parent = 0);
	~LineTestCapturePane()
	{
	}

protected:
	CaptureParameters *getCaptureParameters();

	void closeEvent(QCloseEvent *e);
	void showEvent(QShowEvent *e);
	void hideEvent(QHideEvent *e);
	void timerEvent(QTimerEvent *e);

	bool capture();
	void setFrameField(TFrameId fid);

	void initializeTitleBar(TPanelTitleBar *titleBar);

protected slots:
	void showCaptureSettings();
	void showFileSettings();
	void onSaveModeChanged(int);
	void onIncrementFieldEditFinished();
	void onStepFieldEditFinished();
	void onConnectCheckboxStateChanged(int state);
	void onOnionSkinStateChanged(int state);
	void onViewFrameStateChanged(int);
	void onLinesColorChanged(const TPixel32 &, bool);
	void captureButton();
	void updateFileField();
	void onCaptureFinished();
	void onSceneSwitched();
	void changeWindowTitle();
	void freeze(bool);
};
#endif

#endif // LINETESTCAPTUREPANE
