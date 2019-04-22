#pragma once

#ifndef CANVASSIZEPOPUP_H
#define CANVASSIZEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include <QPixmap>

// forward declaration
class QButtonGroup;
class QComboBox;
class TMeasure;

namespace DVGui {
class DoubleLineEdit;
class CheckBox;
}

//=============================================================================

enum PeggingPositions { e00, e01, e02, e10, e11, e12, e20, e21, e22 };

//=============================================================================

class PeggingWidget final : public QWidget {
  Q_OBJECT

  QButtonGroup *m_buttonGroup;
  QPushButton *m_00, *m_01, *m_02;
  QPushButton *m_10, *m_11, *m_12;
  QPushButton *m_20, *m_21, *m_22;

  QPixmap m_topPix, m_topRightPix;

  PeggingPositions m_pegging;
  bool m_cutLx, m_cutLy;

public:
  PeggingWidget(QWidget *parent = 0);
  PeggingPositions getPeggingPosition() const { return m_pegging; }
  void resetWidget();
  void cutLx(bool value) { m_cutLx = value; }
  void cutLy(bool value) { m_cutLy = value; }
  void updateAnchor();

private:
  void createButton(QPushButton **button, PeggingPositions position);

protected:
  void paintEvent(QPaintEvent *) override;

public slots:
  void on00();
  void on01();
  void on02();
  void on10();
  void on11();
  void on12();
  void on20();
  void on21();
  void on22();
};

//=============================================================================
// CanvasSizePopup
//-----------------------------------------------------------------------------

class CanvasSizePopup final : public DVGui::Dialog {
  Q_OBJECT

  TXshSimpleLevelP m_sl;

  QLabel *m_currentXSize;
  QLabel *m_currentYSize;
  QComboBox *m_unit;
  DVGui::DoubleLineEdit *m_xSizeFld;
  DVGui::DoubleLineEdit *m_ySizeFld;
  DVGui::CheckBox *m_relative;
  PeggingWidget *m_pegging;

  TMeasure *m_xMeasure, *m_yMeasure;

public:
  CanvasSizePopup();

protected:
  void showEvent(QShowEvent *e) override;

public slots:
  void onOkBtn();
  void onSizeChanged();
  void onRelative(bool);
  void onUnitChanged(int);
};

#endif  // CANVASSIZEPOPUP_H
