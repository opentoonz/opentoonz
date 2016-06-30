#pragma once

#ifndef ADDFILMSTRIPFRAMESPOPUP_H
#define ADDFILMSTRIPFRAMESPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"

// forward declaration
class QPushButton;
class QLineEdit;

//=============================================================================
// AddFilmstripFramesPopup
//-----------------------------------------------------------------------------

class AddFilmstripFramesPopup final : public DVGui::Dialog {
  Q_OBJECT

  QPushButton *m_okBtn;
  QPushButton *m_cancelBtn;

  DVGui::IntLineEdit *m_startFld, *m_endFld, *m_stepFld;

public slots:
  void onOk();

public:
  AddFilmstripFramesPopup();

  // void configureNotify(const TDimension &size);

  void draw();
  void update();

  void getParameters(int &startFrame, int &endFrame, int &stepFrame) const;
};

#endif  // ADDFILMSTRIPFRAMESPOPUP_H
