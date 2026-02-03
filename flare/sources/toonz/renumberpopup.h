#pragma once

#ifndef RENUMBERPOPUP_H
#define RENUMBERPOPUP_H

#include "tgeometry.h"
#include "toonzqt/intfield.h"
#include "toonzqt/dvdialog.h"

// forward declaration
class QPushButton;

//=============================================================================
// RenumberPopup
//-----------------------------------------------------------------------------

class RenumberPopup final : public DVGui::Dialog {
  Q_OBJECT

  QPushButton *m_okBtn;
  QPushButton *m_cancelBtn;

  DVGui::IntLineEdit *m_startFld, *m_stepFld;

public:
  RenumberPopup();
  void setValues(int start, int step);
  void getValues(int &start, int &step);
};

#endif  // RENUMBERPOPUP_H
