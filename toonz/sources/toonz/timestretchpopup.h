#pragma once

#ifndef TIMESTRETCHPOPUP_H
#define TIMESTRETCHPOPUP_H

#include "tgeometry.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/intfield.h"

// forward declaration
class QPushButton;
class QLabel;
class QComboBox;
class TSelection;

//=============================================================================
// TimeStretchPopup
//-----------------------------------------------------------------------------

class TimeStretchPopup final : public DVGui::Dialog {
  Q_OBJECT

  QPushButton *m_okBtn;
  QPushButton *m_cancelBtn;

  QComboBox *m_stretchType;
  DVGui::IntLineEdit *m_newRangeFld;
  QLabel *m_oldRange;

public:
  enum STRETCH_TYPE { eRegion = 0, eFrameRange = 1, eWholeXsheet = 2 };

private:
  STRETCH_TYPE m_currentStretchType;

public:
  TimeStretchPopup();

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void setCurrentStretchType(int index);
  void stretch();
  void updateValues();
  void updateValues(TSelection *selection);
  void updateValues(TSelection *selection, TSelection *newSelection) {
    updateValues(newSelection);
  }
};

#endif  // TIMESTRETCHPOPUP_H
