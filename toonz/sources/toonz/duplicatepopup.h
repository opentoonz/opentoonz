#pragma once

#ifndef DUPLICATEPOPUP_H
#define DUPLICATEPOPUP_H

#include "tgeometry.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"

// forward declaration
class QPushButton;

//=============================================================================
// DuplicatePopup
//-----------------------------------------------------------------------------

class DuplicatePopup final : public QDialog {
  Q_OBJECT

  QPushButton *m_okBtn;
  QPushButton *m_cancelBtn;
  QPushButton *m_applyBtn;

  DVGui::IntLineEdit *m_countFld, *m_upToFld;

  int m_count, m_upTo;

public:
  DuplicatePopup();
  void getValues(int &start, int &step);

public slots:
  void updateValues();
  void onApplyPressed();
  void onOKPressed();
  void onSelectionChanged();

protected:
  void showEvent(QShowEvent *) override;
};

#endif  // DUPLICATEPOPUP_H
