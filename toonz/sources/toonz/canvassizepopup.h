#pragma once

#ifndef CANVASSIZEPOPUP_H
#define CANVASSIZEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/txshsimplelevel.h"
#include "tgeometry.h"
#include <QPixmap>

class QButtonGroup;
class QComboBox;
class QHideEvent;
class QLabel;
class TMeasure;
class TXshLevel;

namespace DVGui {
class DoubleLineEdit;
class CheckBox;
}

void updateCanvasSizeCommandEnabled();

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
  void setPeggingPosition(PeggingPositions position);
  void resetWidget();
  void cutLx(bool value) { m_cutLx = value; }
  void cutLy(bool value) { m_cutLy = value; }
  void updateAnchor();

private:
  void createButton(QPushButton **button, PeggingPositions position);

protected:
  void paintEvent(QPaintEvent *) override;

signals:
  void peggingChanged();

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
  DVGui::CheckBox *m_updateCamera;
  PeggingWidget *m_pegging;

  TMeasure *m_xMeasure, *m_yMeasure;

  TRectD m_currentRect, m_proposedRect;
  TDimension m_currentDim;
  bool m_sessionActive;
  bool m_ignoreSync;
  bool m_fromTool;

public:
  CanvasSizePopup();

  static CanvasSizePopup *instance();

  bool isSessionActive() const { return m_sessionActive; }
  TRectD currentCanvasRect() const { return m_currentRect; }
  TRectD proposedCanvasRect() const { return m_proposedRect; }
  PeggingPositions peggingPosition() const;

  void openSession();
  void cancelFromOutside();
  void setProposedRectFromTool(const TRectD &rect, PeggingPositions peg);

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

  TDimension proposedPixelSize() const;
  void initFromLevel();
  void endSession();
  void updateProposedFromFields();
  void syncFieldsFromRect();
  void refreshOverlay(bool wholeViewer = false);

public slots:
  void onOkBtn();
  void onSizeChanged();
  void onRelative(bool);
  void onUnitChanged(int);
  void onPeggingChanged();
  void onLevelSwitched(TXshLevel *);
  void onReset();
};

#endif  // CANVASSIZEPOPUP_H
