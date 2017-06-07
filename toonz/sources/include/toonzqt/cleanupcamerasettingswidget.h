#pragma once

#ifndef CLEANUPCAMERASETTINGSWIDGET_H
#define CLEANUPCAMERASETTINGSWIDGET_H

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

#include "tcommon.h"
#include "tgeometry.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QFrame>

class TCamera;
class QRadioButton;
class QComboBox;
class ResListManager;
class CleanupParameters;
class TFilePath;
class TXshLevel;
class QPushButton;
class CameraSettingsWidget;
class QLabel;
class QCheckBox;

namespace DVGui {
class LineEdit;
class DoubleLineEdit;
class IntLineEdit;
class MeasuredDoubleLineEdit;
class CheckBox;
}

class DVAPI CleanupCameraSettingsWidget : public QFrame {
  Q_OBJECT
  CameraSettingsWidget *m_cameraWidget;

public:
  DVGui::MeasuredDoubleLineEdit *m_offsX, *m_offsY;
  /*--- オフセットを軸毎にロックする ---*/
  QCheckBox *m_offsx_lock, *m_offsy_lock;

public:
  CleanupCameraSettingsWidget();
  ~CleanupCameraSettingsWidget();

  QSize sizeHint() const override { return minimumSize(); }

  void setCameraPresetListFile(const TFilePath &fp);

  // CleanupParameters => CleanupCameraSettingsWidget fields
  void setFields(CleanupParameters *cleanupParameters);

  // CleanupCameraSettingsWidget fields => CleanupParameters
  void getFields(CleanupParameters *cleanupParameters);

  double getClosestFieldValue() const;

  void setImageInfo(const TFilePath &imgPath);
  void setImageInfo(int w, int h, double dpix, double dpiy);

  // needed by the "use level settings" button
  void setCurrentLevel(TXshLevel *);

signals:
  void cleanupSettingsChanged();
};

#endif
