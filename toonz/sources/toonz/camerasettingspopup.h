#pragma once

#ifndef CAMERASETTINGSPOPUP_H
#define CAMERASETTINGSPOPUP_H

#include "tgeometry.h"
#include "toonz/tstageobject.h"
#include "toonzqt/dvdialog.h"

// forward declaration
class QLabel;
class QComboBox;
class QPushButton;
class QLineEdit;
class QRadioButton;
class TStageObject;
class TCamera;
class CameraSettingsWidget;
class TXshLevel;

namespace DVGui {
class LineEdit;
class DoubleLineEdit;
class CheckBox;
class ValueField;
class MeasuredDoubleLineEdit;
}

//=============================================================================
// CameraSettingsPopup
//-----------------------------------------------------------------------------

class CameraSettingsPopup final : public QDialog {
  Q_OBJECT
  static std::map<TStageObjectId, CameraSettingsPopup *> m_popups;

  DVGui::LineEdit *m_nameFld;
  CameraSettingsWidget *m_cameraSettingsWidget;
  TStageObjectId m_cameraId;  // if == NoneId then use the current camera

  TStageObject *getCameraObject();
  TCamera *getCamera();
  void updateWindowTitle();

public:
  CameraSettingsPopup();

  void attachToCamera(const TStageObjectId &id) { m_cameraId = id; }

  // create a popup attached to camera id (or return the already existent one)
  static CameraSettingsPopup *createPopup(const TStageObjectId &id);

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void moveEvent(QMoveEvent *e) override;

protected slots:

  void onChanged();
  void onNameChanged();
  void updateFields();
  void updateFields(bool) {
    updateFields();
  }  // to be connected to objectChanged(bool)
  void onLevelSwitched(TXshLevel *);

signals:
  void changed();
};

#endif  // CAMERASETTINGSPOPUP_H
