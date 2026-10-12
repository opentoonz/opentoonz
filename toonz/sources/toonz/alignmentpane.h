#pragma once

#ifndef VECTORALIGNMENTPANE_H
#define VECTORALIGNMENTPANE_H

#include "toonzqt/dvdialog.h"

#include "toonz/txshlevel.h"
#include "toonzqt/tselectionhandle.h"

class QComboBox;
class QPushButton;
class QButtonGroup;

class AlignmentPane final : public QFrame {
  Q_OBJECT

  QComboBox *m_alignMethodCB;

  QPushButton *m_alignLeftBtn, *m_alignRightBtn, *m_alignTopBtn,
      *m_alignBottomBtn, *m_alignCenterHBtn, *m_alignCenterVBtn,
      *m_distributeHBtn, *m_distributeVBtn;

public:
  AlignmentPane(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~AlignmentPane(){};

private:
  void updateButtons();

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void onLevelSwitched(TXshLevel *);
  void onToolSwitched();
  void onSelectionChanged();
  void onAlignMethodChanged(int);
  void onSelectionSwitched(TSelection *, TSelection *);
};

#endif  // VECTORALIGNMENTPANE_H
