#pragma once

#ifndef PLTGIZMOPOPUP_H
#define PLTGIZMOPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tgeometry.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"

//=============================================================================
// ValueAdjuster
//-----------------------------------------------------------------------------

class ValueAdjuster final : public QWidget {
  Q_OBJECT

  DVGui::DoubleLineEdit *m_valueLineEdit;

public:
#if QT_VERSION >= 0x050500
  ValueAdjuster(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ValueAdjuster(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ValueAdjuster();

protected slots:
  void onClickedPlus();
  void onClickedMinus();

signals:
  void adjust(double factor);
};

//=============================================================================
// ValueShifter
//-----------------------------------------------------------------------------

class ValueShifter final : public QWidget {
  Q_OBJECT

  DVGui::DoubleLineEdit *m_valueLineEdit;

public:
#if QT_VERSION >= 0x050500
  ValueShifter(bool isHue, QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ValueShifter(bool isHue, QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ValueShifter();

protected slots:
  void onClickedPlus();
  void onClickedMinus();

signals:
  void adjust(double factor);
};

//=============================================================================
// ColorFader
//-----------------------------------------------------------------------------

// TODO: spostare il colorfield qui dentro

class ColorFader final : public QWidget {
  Q_OBJECT

  DVGui::DoubleLineEdit *m_valueLineEdit;

public:
#if QT_VERSION >= 0x050500
  ColorFader(QString name = "", QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ColorFader(QString name = "", QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ColorFader();

protected slots:
  void onClicked();

signals:
  void valueChanged(double factor);
};

//=============================================================================
// PltGizmoPopup
//-----------------------------------------------------------------------------

class PltGizmoPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::ColorField *m_colorFld;

public:
  PltGizmoPopup();

  ~PltGizmoPopup();

public slots:

  void adjustV(double p);
  void adjustS(double p);
  void adjustH(double p);
  void adjustT(double p);

  void shiftV(double p);
  void shiftS(double p);
  void shiftH(double p);
  void shiftT(double p);

  void zeroMatte();
  void fullMatte();

  void onBlend();
  void onFade(double p);
};

#endif  // PLTGIZMOPOPUP_H
