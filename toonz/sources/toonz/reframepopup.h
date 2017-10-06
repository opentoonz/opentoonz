#pragma once

#ifndef REFRAMEPOPUP_H
#define REFRAMEPOPUP_H

#include "toonzqt/dvdialog.h"

#include <QList>

class QLabel;

namespace DVGui {
class IntLineEdit;
}

//=============================================================================
// ReframePopup
// - Enables reframe cells with specified blank frames.
// - The amount of blank frames is specified by the quotient, divided by the
//   reframe step, ( e.g. "3 steps - 2 blanks" will insert 6 blank cells.)
//   considering that it is common usage in Japanese animation industry.
// - Can be used both to the cell and the column selections.
//-----------------------------------------------------------------------------

class ReframePopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::IntLineEdit *m_step, *m_blank;
  QLabel* m_blankCellCountLbl;

public:
  ReframePopup();
  void getValues(int& step, int& blank);

public slots:
  void updateBlankCellCount();
};

#endif  // REFRAMEPOPUP_H