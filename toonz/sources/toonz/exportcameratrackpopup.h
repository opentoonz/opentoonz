#pragma once

#ifndef EXPORTCAMERATRACKPOPUP_H
#define EXPORTCAMERATRACKPOPUP_H

// Tnz6 includes
#include "toonzqt/dvdialog.h"

// Qt includes
#include <QScrollArea>
#include <QWidget>
#include <QSet>
#include <QColor>
#include <QFont>
#include <QPixmap>

namespace DVGui {
class FileField;
class ColorField;
class IntLineEdit;
class DoubleField;
}  // namespace DVGui

class QComboBox;
class QCheckBox;
class QFontComboBox;
class QLineEdit;

//*********************************************************************************
//    Export Camera Track Popup  declaration
//*********************************************************************************

struct ExportCameraTrackInfo {
  // Target column
  int columnId = -1;

  // Appearance settings
  double bgOpacity = 0.5;
  QColor lineColor = Qt::red;

  // Camera rectangle settings
  bool cameraRectOnKeys = true;
  bool cameraRectOnTags = false;
  QSet<int> cameraRectFrames;

  // Track line settings
  bool lineTL            = false;
  bool lineTR            = false;
  bool lineCenter        = true;
  bool lineBL            = false;
  bool lineBR            = false;
  int graduationInterval = 1;

  // Frame number settings
  Qt::Corner numberAt = Qt::TopLeftCorner;
  bool numbersOnLine  = true;
  QFont font;
};

//-----------------------------------------------------------------------------

class CameraTrackPreviewPane final : public QWidget {
  Q_OBJECT

  QPixmap m_pixmap;
  double m_scaleFactor = 1.0;

public:
  explicit CameraTrackPreviewPane(QWidget* parent = nullptr);

  void setPixmap(const QPixmap& pm);
  void doZoom(double d_scale);
  void fitScaleTo(QSize size);

protected:
  void paintEvent(QPaintEvent* event) override;
};

//-----------------------------------------------------------------------------

class CameraTrackPreviewArea final : public QScrollArea {
  Q_OBJECT

  QPoint m_mousePos;

public:
  explicit CameraTrackPreviewArea(QWidget* parent = nullptr)
      : QScrollArea(parent) {}

protected:
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private slots:
  void fitToWindow();
};

//-----------------------------------------------------------------------------

class ExportCameraTrackPopup final : public DVGui::Dialog {
  Q_OBJECT

  // UI Components
  CameraTrackPreviewPane* m_previewPane = nullptr;
  CameraTrackPreviewArea* m_previewArea = nullptr;

  QComboBox* m_targetColumnCombo       = nullptr;
  DVGui::DoubleField* m_bgOpacityField = nullptr;
  DVGui::ColorField* m_lineColorFld    = nullptr;

  QCheckBox* m_cameraRectOnKeysCB   = nullptr;
  QCheckBox* m_cameraRectOnTagsCB   = nullptr;
  QLineEdit* m_cameraRectFramesEdit = nullptr;

  QCheckBox* m_lineTL_CB               = nullptr;
  QCheckBox* m_lineTR_CB               = nullptr;
  QCheckBox* m_lineCenter_CB           = nullptr;
  QCheckBox* m_lineBL_CB               = nullptr;
  QCheckBox* m_lineBR_CB               = nullptr;
  QComboBox* m_graduationIntervalCombo = nullptr;

  QComboBox* m_numberAtCombo         = nullptr;
  QCheckBox* m_numbersOnLineCB       = nullptr;
  QFontComboBox* m_fontCombo         = nullptr;
  DVGui::IntLineEdit* m_fontSizeEdit = nullptr;

public:
  explicit ExportCameraTrackPopup();

protected:
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

private:
  void initialize();
  void saveSettings();
  void loadSettings();
  void updateTargetColumnComboItems();

  QImage generateCameraTrackImg(const ExportCameraTrackInfo& info,
                                bool isPreview);
  void getInfoFromUI(ExportCameraTrackInfo& info);

private slots:
  void updatePreview();
  void onExport();
};

#endif  // EXPORTCAMERATRACKPOPUP_H
