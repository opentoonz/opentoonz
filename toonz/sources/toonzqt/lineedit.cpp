

#include "toonzqt/lineedit.h"
#include "toonzqt/dvdialog.h"
#include <QKeyEvent>

using namespace DVGui;

//=============================================================================
// LineEdit
//-----------------------------------------------------------------------------

LineEdit::LineEdit(QWidget *parent, bool forbiddenSpecialChars)
    : QLineEdit(parent), m_forbiddenSpecialChars(forbiddenSpecialChars) {}

//-----------------------------------------------------------------------------

LineEdit::LineEdit(const QString &contents, QWidget *parent,
                   bool forbiddenSpecialChars)
    : QLineEdit(contents, parent)
    , m_isReturnPressed(false)
    , m_forbiddenSpecialChars(forbiddenSpecialChars) {}

//-----------------------------------------------------------------------------

void LineEdit::focusInEvent(QFocusEvent *event) {
  m_isReturnPressed = false;
  QLineEdit::focusInEvent(event);
  emit focusIn();
}

//-----------------------------------------------------------------------------

void LineEdit::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    m_isReturnPressed = true;
    emit returnPressedNow();
    clearFocus();
    return;
  } else {
    m_isReturnPressed = false;
    if (m_forbiddenSpecialChars) {
      switch (event->key()) {
      case Qt::Key_Backslash:
      case Qt::Key_Slash:
      case Qt::Key_Colon:
      case Qt::Key_Asterisk:
      case Qt::Key_Question:
      case Qt::Key_QuoteDbl:
      case Qt::Key_Greater:
      case Qt::Key_Less:
      case Qt::Key_Bar:
      case Qt::Key_Period:
        DVGui::info(
            tr("A file name cannot contains any of the following chracters: "
               "/\\:*?\"<>|."));
        return;
      default:
        QLineEdit::keyPressEvent(event);
        break;
      }
    } else
      QLineEdit::keyPressEvent(event);
  }
}

//-----------------------------------------------------------------------------

void LineEdit::mouseMoveEvent(QMouseEvent *event) {
  emit(mouseMoved(event));
  QLineEdit::mouseMoveEvent(event);
}
