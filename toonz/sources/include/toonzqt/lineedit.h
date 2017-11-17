#pragma once

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include "tcommon.h"

#include <QLineEdit>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

namespace DVGui {

//=============================================================================
/*! \brief It is a \b QLineEdit which lost focus when enter is pressed and emit
                                         focusIn signal when line edit take
   focus.

                Inherits \b QLineEdit.
*/
class DVAPI LineEdit : public QLineEdit {
  Q_OBJECT

  bool m_isReturnPressed;
  bool m_forbiddenSpecialChars;
  bool m_mouseDragEditing = false;

public:
  LineEdit(QWidget *parent = 0, bool forbiddenSpecialChars = false);
  LineEdit(const QString &contents, QWidget *parent = 0,
           bool forbiddenSpecialChars = false);

  bool isReturnPressed() const { return m_isReturnPressed; }

  ~LineEdit() {}

  // In the function editor, ctrl + dragging on the lineedit
  // can adjust the value.
  bool getMouseDragEditing() { return m_mouseDragEditing; }
  void setMouseDragEditing(bool status) { m_mouseDragEditing = status; }

protected:
  void focusInEvent(QFocusEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mouseMoveEvent(QMouseEvent *) override;

signals:
  void focusIn();
  void returnPressedNow();
  // this signal is only used for mouse drag value editing in the function
  // panel.
  void mouseMoved(QMouseEvent *);
};

//-----------------------------------------------------------------------------
}  // namespace DVGui
//-----------------------------------------------------------------------------

#endif  // LINEEDIT_H
