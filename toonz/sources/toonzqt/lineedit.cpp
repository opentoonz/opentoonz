

#include "toonzqt/lineedit.h"
#include "toonzqt/dvdialog.h"
#include <QKeyEvent>

using namespace DVGui;

//=============================================================================
// LineEdit
//-----------------------------------------------------------------------------

LineEdit::LineEdit(QWidget *parent, bool forbiddenSpecialChars)
	: QLineEdit(parent), m_forbiddenSpecialChars(forbiddenSpecialChars)
{
}

//-----------------------------------------------------------------------------

LineEdit::LineEdit(const QString &contents, QWidget *parent, bool forbiddenSpecialChars)
	: QLineEdit(contents, parent), m_isReturnPressed(false), m_forbiddenSpecialChars(forbiddenSpecialChars)
{
}

//-----------------------------------------------------------------------------

void LineEdit::focusInEvent(QFocusEvent *event)
{
	m_isReturnPressed = false;
	QLineEdit::focusInEvent(event);
	emit focusIn();
}

//-----------------------------------------------------------------------------

void LineEdit::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		m_isReturnPressed = true;
		clearFocus();
		return;
	} else {
		m_isReturnPressed = false;
		if (m_forbiddenSpecialChars) {
			switch (event->key()) {
				CASE Qt::Key_Backslash : __OR Qt::Key_Slash : __OR Qt::Key_Colon : __OR Qt::Key_Asterisk : __OR Qt::Key_Question : __OR Qt::Key_QuoteDbl : __OR Qt::Key_Greater : __OR Qt::Key_Less : __OR Qt::Key_Bar : __OR Qt::Key_Period:
				{
					DVGui::info(tr("A file name cannot contains any of the following chracters: /\\:*?\"<>|."));
					return;
				}
			default:
				QLineEdit::keyPressEvent(event);
			}
		} else
			QLineEdit::keyPressEvent(event);
	}
}
