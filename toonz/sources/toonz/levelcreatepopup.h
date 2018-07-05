#pragma once

#ifndef LEVELCREATEPOPUP_H
#define LEVELCREATEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"

// forward declaration
class QLabel;
class QComboBox;
// class DVGui::MeasuredDoubleLineEdit;

//=============================================================================
// LevelCreatePopup
//-----------------------------------------------------------------------------

class LevelCreatePopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_nameFld;
  DVGui::IntLineEdit *m_fromFld;
  DVGui::IntLineEdit *m_toFld;
  QComboBox *m_levelTypeOm;
  DVGui::IntLineEdit *m_stepFld;
  DVGui::IntLineEdit *m_incFld;
  DVGui::FileField *m_pathFld;
  QLabel *m_widthLabel;
  QLabel *m_heightLabel;
  QLabel *m_dpiLabel;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  DVGui::DoubleLineEdit *m_dpiFld;

public:
  LevelCreatePopup();

  void setSizeWidgetEnable(bool isEnable);
  int getLevelType() const;

  void update();
  bool apply();

protected:
  // set m_pathFld to the default path
  void updatePath();
  void nextName();
  void showEvent(QShowEvent *) override;
  bool levelExists(std::wstring levelName);

public slots:
  void onLevelTypeChanged(int index);
  void onOkBtn();

  void onApplyButton();
};

#endif  // LEVELCREATEPOPUP_H
