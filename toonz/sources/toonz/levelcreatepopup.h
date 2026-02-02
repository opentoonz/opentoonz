#pragma once

#ifndef LEVELCREATEPOPUP_H
#define LEVELCREATEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"

// Qt forward declarations
#include <QObject>
#include <QString>

class QLabel;
class QComboBox;
class QPushButton;
class QShowEvent;

namespace DVGui {
class LineEdit;
class MeasuredDoubleLineEdit;
}  // namespace DVGui

//=============================================================================
// LevelCreatePopup
//-----------------------------------------------------------------------------

class LevelCreatePopup final : public DVGui::Dialog {
  Q_OBJECT

  // UI Widgets
  DVGui::LineEdit *m_nameFld;
  DVGui::IntLineEdit *m_fromFld;
  DVGui::IntLineEdit *m_toFld;
  DVGui::IntLineEdit *m_stepFld;
  DVGui::IntLineEdit *m_incFld;

  QComboBox *m_levelTypeOm;
  DVGui::FileField *m_pathFld;

  QLabel *m_widthLabel;
  QLabel *m_heightLabel;
  QLabel *m_dpiLabel;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  DVGui::DoubleLineEdit *m_dpiFld;

  QLabel *m_rasterFormatLabel;
  QComboBox *m_rasterFormatOm;
  QPushButton *m_frameFormatBtn;

public:
  explicit LevelCreatePopup();

  // Disable copying
  LevelCreatePopup(const LevelCreatePopup &)            = delete;
  LevelCreatePopup &operator=(const LevelCreatePopup &) = delete;

  // Widget state control
  void setSizeWidgetEnable(bool isEnable);
  void setRasterWidgetVisible(bool isVisible);

  // Getters
  int getLevelType() const;

  // Operations
  void update();
  bool apply();

protected:
  // Set m_pathFld to the default path
  void updatePath();
  void nextName();

  // Event handlers
  void showEvent(QShowEvent *event) override;

  // Validation
  bool levelExists(const std::wstring &levelName);

public Q_SLOTS:
  void onLevelTypeChanged(int index);
  void onOkBtn();
  void onApplyButton();
  void onFrameFormatButton();
};

#endif  // LEVELCREATEPOPUP_H
