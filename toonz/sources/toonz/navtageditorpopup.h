#pragma once

#ifndef TAGEDITORPOPUP_INCLUDED
#define TAGEDITORPOPUP_INCLUDED

#include "tcommon.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"

#include <QString>
#include <QColor>
#include <QComboBox>

class NavTagEditorPopup final : public DVGui::Dialog {
  Q_OBJECT

  DVGui::LineEdit *m_labelFld;
  QComboBox *m_colorCB;

  QString m_label;
  QColor m_color;

public:
  enum TagColors {
    Magenta = 0,
    Red,
    Green,
    Blue,
    Yellow,
    Cyan,
    White,
    DarkMagenta,
    DarkRed,
    DarkGreen,
    DarkBlue,
    DarkYellow,
    DarkCyan,
    DarkGray
  };

public:
  NavTagEditorPopup(int frame, QString label, QColor color);

  void accept() override;

  QString getLabel() { return m_label; }

  QColor getColor() { return m_color; }

protected slots:

  void onLabelChanged() {}
  void onColorChanged(int);
};

#endif
