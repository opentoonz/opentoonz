#pragma once

#ifndef CAMERASETTINGSWIDGET_H
#define CAMERASETTINGSWIDGET_H

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
#include <QPushButton>
#include <QValidator>
#include <QLineEdit>

class TCamera;
class QRadioButton;
class QComboBox;
class QLabel;
class ResListManager;
class TFilePath;
class TXshSimpleLevel;
class TXshLevel;

namespace DVGui {
class LineEdit;
class DoubleLineEdit;
class IntLineEdit;
class MeasuredDoubleLineEdit;
class CheckBox;
}  // namespace DVGui

//---------------------------------------------------------------

class SimpleExpValidator final : public QValidator {
public:
  SimpleExpValidator(QObject *parent) : QValidator(parent){};
  State validate(QString &input, int &pos) const override;
};

//---------------------------------------------------------------
// for A/R input field
class SimpleExpField final : public QLineEdit {
  SimpleExpValidator *m_validator;
  QString m_previousValue;

public:
  SimpleExpField(QWidget *parent);

  void setValue(double);
  void setValue(double, int, int);
  double getValue();

protected:
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
};

//---------------------------------------------------------------

class DVAPI CameraSettingsWidget final : public QFrame {
  friend class StartupPopup;
  Q_OBJECT

  bool m_forCleanup;

  QRadioButton *m_xPrev, *m_yPrev, *m_arPrev;
  QRadioButton *m_inchPrev, *m_dotPrev;

  DVGui::MeasuredDoubleLineEdit *m_lxFld, *m_lyFld;
  SimpleExpField *m_arFld;
  double m_arValue;
  DVGui::IntLineEdit *m_xResFld, *m_yResFld;
  DVGui::DoubleLineEdit *m_xDpiFld, *m_yDpiFld;
  QLabel *m_dpiLabel;
  QLabel *m_resLabel;
  QLabel *m_xLabel;
  QLabel *m_unitLabel;

  QPushButton *m_fspChk;  // Force Squared Pixel => dpix == dpiy

  QPushButton *m_useLevelSettingsBtn;
  QComboBox *m_presetListOm;
  QPushButton *m_addPresetBtn, *m_removePresetBtn;

  DVGui::MeasuredDoubleLineEdit *m_offsX, *m_offsY;

  QString m_presetListFile;

  // needed by "use level settings"
  TXshSimpleLevel *m_currentLevel;

  void savePresetList();
  void loadPresetList();
  bool parsePresetString(const QString &str, QString &name, int &xres,
                         int &yres, QString &ar);

  bool parsePresetString(const QString &str, QString &name, int &xres,
                         int &yres, double &fx, double &fy, QString &xoffset,
                         QString &yoffset, double &ar, bool forCleanup = false);

public:
  CameraSettingsWidget(bool forCleanup = false);
  ~CameraSettingsWidget();

  void setPresetListFile(const TFilePath &fp);

  // Defines the level referred by the button "Use level settings".
  // Calling setCurrentLevel(0) disables the button
  void setCurrentLevel(TXshLevel *);

  // camera => widget fields (i.e. initialize widget)
  void setFields(const TCamera *camera);

  // widget fields => camera return true if the value is actually changed
  bool getFields(TCamera *camera);

  QSize sizeHint() const override { return minimumSize(); }

  // The aspect ratio can be expressed as a fraction (e.g. "4/3")
  // The following methods convert code/decode the value
  static double aspectRatioStringToValue(const QString &s);
  /*---
   * カメラの縦横ピクセル値を入力できるようにし、valueがX/Yの値に近かったら、"X/Y"と表示する
   * ---*/
  static QString aspectRatioValueToString(double ar, int width = 0,
                                          int height = 0);

  // the current camera dimension (in inches)
  TDimensionD getSize() const;

  // the current camera resolution (in pixels)
  TDimension getRes() const;

  /*--- cleanupCameraSettingsWidgetからポインタを受け取る ---*/
  void setOffsetWidgetPointers(DVGui::MeasuredDoubleLineEdit *offsX,
                               DVGui::MeasuredDoubleLineEdit *offsY) {
    m_offsX = offsX;
    m_offsY = offsY;
  }

protected:
  bool eventFilter(QObject *obj, QEvent *e) override;
  void showEvent(QShowEvent *e) override;

  void hComputeLx();
  void hComputeLy();
  void vComputeLx();
  void vComputeLy();
  void computeAr();
  void computeXRes();
  void computeYRes();
  void computeXDpi();
  void computeYDpi();
  void computeResOrDpi();

  void updatePresetListOm();

  void setArFld(double ar);

protected slots:
  void onLxChanged();
  void onLyChanged();
  void onArChanged();
  void onXResChanged();
  void onYResChanged();
  void onXDpiChanged();
  void onYDpiChanged();
  void onFspChanged(bool checked);
  void onPrevToggled(bool checked);
  void onPresetSelected(const QString &);
  void addPreset();
  void removePreset();
  void useLevelSettings();

signals:
  void changed();  // some value has been changed
  void
  levelSettingsUsed();  // the "Use level settings" button has been pressed.
  // Note: a changed() signal is always emitted after levelSettingsUsed()
};

#endif
