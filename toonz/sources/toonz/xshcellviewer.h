

#ifndef XSHCELLVIEWER_H
#define XSHCELLVIEWER_H

#include <QWidget>
#include <QLineEdit>

// forward declaration
class XsheetViewer;
class QMenu;
class TXsheetHandle;

namespace XsheetGUI
{

class NoteWidget;
class DragTool;

class RenameCellField : public QLineEdit
{
	Q_OBJECT

	int m_row;
	int m_col;
	XsheetViewer *m_viewer;

public:
	RenameCellField(QWidget *parent, XsheetViewer *viewer);
	~RenameCellField() {}

	void showInRowCol(int row, int col);

protected:
	void focusOutEvent(QFocusEvent *);
	void keyPressEvent(QKeyEvent *event);

	void renameCell();

protected slots:
	void onReturnPressed();
};

//=============================================================================
// CellArea
//-----------------------------------------------------------------------------

//!La classe si occupa della visualizzazione delle celle nel viewer.
class CellArea : public QWidget
{
	Q_OBJECT

	XsheetViewer *m_viewer;
	//smart tab
	QRect m_levelExtenderRect;
	//upper-directional smart tab
	QRect m_upperLevelExtenderRect;
	QList<QRect> m_soundLevelModifyRects;

	bool m_isPanning;
	bool m_isMousePressed;

	QPoint m_pos;
	QString m_tooltip;

	RenameCellField *m_renameCell;
	
	void drawCells(QPainter &p, const QRect toBeUpdated);
	void drawLevelCell(QPainter &p, int row, int col, bool isReference = false);
	void drawSoundTextCell(QPainter &p, int row, int col);
	void drawSoundCell(QPainter &p, int row, int col);
	void drawPaletteCell(QPainter &p, int row, int col, bool isReference = false);
	void drawKeyframe(QPainter &p, const QRect toBeUpdated);
	void drawNotes(QPainter &p, const QRect toBeUpdated);

	//Restistusce true
	bool getEaseHandles(int r0, int r1, double e0, double e1, int &rh0, int &rh1);

	DragTool *getDragTool() const;
	void setDragTool(DragTool *dragTool);

public:
#if QT_VERSION >= 0x050500
	CellArea(XsheetViewer *parent, Qt::WindowFlags flags = 0);
#else
	CellArea(XsheetViewer *parent, Qt::WFlags flags = 0);
#endif
	~CellArea();

	void mouseMoveEvent(QMouseEvent *event);

	// display upper-directional smart tab only when pressing ctrl key
	void onControlPressed(bool pressed);

	//	void keyUpDownPressed(int newRow);

protected:
	void paintEvent(QPaintEvent *);

	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void contextMenuEvent(QContextMenuEvent *event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dragLeaveEvent(QDragLeaveEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);
	bool event(QEvent *event);

	/*!Crea il menu' del tasto destro che si visualizza quando si clicca sulla cella,
      distinguendo i due casi: cella piena, cella vuota.*/
	void createCellMenu(QMenu &menu, bool isCellSelected);
	//!Crea il menu' del tasto destro che si visualizza si clicca su un key frame.
	void createKeyMenu(QMenu &menu);
	//!Crea il menu' del tasto destro che si visualizza quando si clicca sulla linea tre due key frame.
	void createKeyLineMenu(QMenu &menu, int row, int col);
	//!Crea il menu' del tasto destro che si visualizza quando si sopra una nota.
	void createNoteMenu(QMenu &menu);

protected slots:
	void openNote();
	void deleteNote();
	void onStepChanged(QAction *);
	// replace level with another level in the cast
	void onReplaceByCastedLevel(QAction *action);

};

} // namespace XsheetGUI

#endif // XSHCELLVIEWER_H
