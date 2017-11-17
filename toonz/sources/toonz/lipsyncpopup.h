#pragma once

#ifndef LIPSYNCPOPUP_H
#define LIPSYNCPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"
#include "toonz/txshlevel.h"
#include "toonzqt/intfield.h"

#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

// forward declaration
class QLabel;
class TXshSimpleLevel;
class TXshChildLevel;
class TFrameId;

//=============================================================================
// LipSyncPopup
//-----------------------------------------------------------------------------

class LipSyncPopup final : public DVGui::Dialog {
  Q_OBJECT

  QLabel *m_aiLabel;
  QLabel *m_oLabel;
  QLabel *m_eLabel;
  QLabel *m_uLabel;
  QLabel *m_lLabel;
  QLabel *m_wqLabel;
  QLabel *m_mbpLabel;
  QLabel *m_fvLabel;
  QLabel *m_restLabel;
  QLabel *m_otherLabel;

  QLabel *m_imageLabels[10];
  QLabel *m_textLabels[10];
  QPushButton *m_navButtons[20];
  QPixmap m_pixmaps[10];
  QPushButton *m_applyButton;
  std::vector<TFrameId> m_levelFrameIds;
  std::vector<TFrameId> m_activeFrameIds;
  DVGui::FileField *m_file;
  TXshSimpleLevel *m_sl;
  TXshChildLevel *m_cl;
  TXshLevelP m_childLevel;
  DVGui::IntLineEdit *m_startAt;
  int m_col;
  bool m_valid = false;
  bool m_isEditingLevel;
  QStringList m_textLines;
  QCheckBox *m_restToEnd;

public:
  LipSyncPopup();

protected:
  void showEvent(QShowEvent *) override;
  void paintEvent(QPaintEvent *) override;

public slots:
  void onApplyButton();
  void imageNavClicked(int id);
  void onPathChanged();
  void onStartValueChanged();
};

#endif  // LIPSYNCPOPUP_H
