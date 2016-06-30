#pragma once

#ifndef ADJUST_LEVELS_POPUP_H
#define ADJUST_LEVELS_POPUP_H

// tnzcore includes
#include "traster.h"

// toonzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/planeviewer.h"
#include "toonzqt/marksbar.h"

class Histogram;
namespace DVGui {
class IntLineEdit;
}

class QShowEvent;
class QHideEvent;
class QPushButton;

//**************************************************************
//    EditableMarksBar  declaration
//**************************************************************

class EditableMarksBar final : public QFrame {
  Q_OBJECT

  MarksBar *m_marksBar;
  DVGui::IntLineEdit *m_fields[2];

public:
  EditableMarksBar(QWidget *parent = 0);
  ~EditableMarksBar();

public:
  const QVector<int> &marks() const { return m_marksBar->values(); }

  const MarksBar *marksBar() const { return m_marksBar; }
  MarksBar *marksBar() { return m_marksBar; }

  void getValues(int *values) const;

signals:

  void paramsChanged();

protected slots:

  void onFieldEdited();

public slots:

  void updateFields();
};

//**************************************************************
//    Adjust-Levels Popup  declaration
//**************************************************************

class AdjustLevelsPopup final : public DVGui::Dialog {
  Q_OBJECT

  Histogram *m_histogram;
  QPushButton *m_okBtn;

  EditableMarksBar *m_marksBar[10];

  TRasterP m_inputRas;

  double m_thresholdD;
  int m_threshold;

private:
  class Swatch;
  Swatch *m_viewer;

public:
  AdjustLevelsPopup();

protected:
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *se) override;

  void acquireRaster();
  void updateProcessedImage();
  void getParameters(int *in0, int *in1, int *out0, int *out1);

  void setThreshold(double t);

protected slots:

  void clampRange();
  void autoAdjust();
  void reset();

  void onSelectionChanged();
  void onParamsChanged();

  void apply();
};

#endif  // ADJUST_LEVELS_POPUP_H
