#pragma once

#ifndef AUTOINPUTCELLNUMBERPOPUP_H
#define AUTOINPUTCELLNUMBERPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshlevel.h"

class TSelection;
class QPushButton;
class FrameNumberLineEdit;
namespace DVGui {
class IntLineEdit;
}
//=============================================================================
// AutoInputCellNumberPopup
//-----------------------------------------------------------------------------

class AutoInputCellNumberPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::IntLineEdit *m_increment, *m_interval, *m_step, *m_repeat;
  FrameNumberLineEdit *m_from, *m_to;
  QPushButton *m_overwriteBtn, *m_insertBtn;

  bool getTarget(std::vector<int> &columnIndices,
                 std::vector<TXshLevelP> &levels, int &r0, int &r1,
                 bool forCheck = false);
  void doExecute(bool overwrite);

public:
  AutoInputCellNumberPopup();

public slots:
  void onOverwritePressed();
  void onInsertPressed();
  void onSelectionChanged();

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
};

#endif  // AUTOINPUTCELLNUMBERPOPUP_H