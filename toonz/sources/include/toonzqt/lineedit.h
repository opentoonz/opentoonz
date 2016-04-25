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

namespace DVGui
{

//=============================================================================
/*! \brief It is a \b QLineEdit which lost focus when enter is pressed and emit
					 focusIn signal when line edit take focus.

		Inherits \b QLineEdit.
*/
class DVAPI LineEdit : public QLineEdit
{
	Q_OBJECT

	bool m_isReturnPressed;
	bool m_forbiddenSpecialChars;

public:
	LineEdit(QWidget *parent = 0, bool forbiddenSpecialChars = false);
	LineEdit(const QString &contents, QWidget *parent = 0, bool forbiddenSpecialChars = false);

	bool isReturnPressed() const { return m_isReturnPressed; }

	~LineEdit() {}

protected:
	void focusInEvent(QFocusEvent *event);
	void keyPressEvent(QKeyEvent *event);

signals:
	void focusIn();
};

//-----------------------------------------------------------------------------
} //namespace DVGui
//-----------------------------------------------------------------------------

#endif // LINEEDIT_H
