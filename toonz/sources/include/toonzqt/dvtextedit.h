#pragma once

#ifndef DVTEXTEDIT_H
#define DVTEXTEDIT_H

#ifdef _WIN32
#pragma warning(disable : 4251)
#endif

#include "tcommon.h"
#include "tpixel.h"

#include <QWidget>
#include <QString>
#include <QTextEdit>

class QLabel;
class QLineEdit;
class QToolBar;
class QAction;
class QActionGroup;
class QFontComboBox;
class QComboBox;
class QImage;

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

class ColorField;
//-----------------------------------------------------------------------------

class DvMiniToolBar : public QFrame
{
	QPoint m_dragPos;

public:
	DvMiniToolBar(QWidget *parent = 0);
	~DvMiniToolBar();

protected:
	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
};

//-----------------------------------------------------------------------------

class DvTextEditButton : public QWidget
{
	Q_OBJECT

public:
	DvTextEditButton(QWidget *parent = 0);
	~DvTextEditButton();

protected:
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *);

signals:
	void clicked();
};

//-----------------------------------------------------------------------------

class DVAPI DvTextEdit : public QTextEdit
{
	Q_OBJECT

	bool m_paintMode;

	bool m_miniToolBarEnabled;

	QPoint m_mousePos;

	QComboBox *m_sizeComboBox;
	QFontComboBox *m_fontComboBox;

	QActionGroup *m_alignActionGroup;

	QAction *m_boldAction,
		*m_italicAction,
		*m_underlineAction,
		*m_alignLeftAction,
		*m_alignRightAction,
		*m_alignCenterAction;

	ColorField *m_colorField;

	DvMiniToolBar *m_miniToolBar;
	DvTextEditButton *m_button;

public:
	DvTextEdit(QWidget *parent = 0);
	~DvTextEdit();

	void setMiniToolBarEnabled(bool value) { m_miniToolBarEnabled = value; }
	void changeFont(const QFont &f);

protected:
	void createActions();
	void createMiniToolBar();

	void showMiniToolBar(const QPoint &pos);
	void hideMiniToolBar();

	void mousePressEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void wheelEvent(QWheelEvent *);

	void focusInEvent(QFocusEvent *);
	void focusOutEvent(QFocusEvent *);

	void dragMoveEvent(QDragMoveEvent *);

private:
	void fontChanged(const QFont &f);
	void colorChanged(const QColor &c);
	void alignmentChanged(Qt::Alignment a);

	void mergeFormatOnWordOrSelection(const QTextCharFormat &format);

protected slots:

	void onCurrentCharFormatChanged(const QTextCharFormat &format);
	void onCursorPositionChanged();
	void onSelectionChanged();

	void onShowMiniToolBarClicked();

	void setTextFamily(const QString &);
	void setTextColor(const TPixel32 &color, bool isDragging);
	void setTextBold();
	void setTextItalic();
	void setTextUnderline();
	void setTextSize(const QString &p);
	void setTextAlign(QAction *);

signals:

	void focusIn();
};

//-----------------------------------------------------------------------------
} //namespace DVGui
//-----------------------------------------------------------------------------

#endif //DVTEXTEDIT_H
