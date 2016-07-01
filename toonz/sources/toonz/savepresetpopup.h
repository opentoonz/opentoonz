#pragma once

#ifndef SAVEPRESETPOPUP_H
#define SAVEPRESETPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"

//=============================================================================
// SavePresetPopup
//-----------------------------------------------------------------------------

class SavePresetPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_nameFld;

public:
  SavePresetPopup();
  ~SavePresetPopup();
  bool apply();

protected slots:
  void onOkBtn();
};

#endif  // SAVEPRESETPOPUP_H
