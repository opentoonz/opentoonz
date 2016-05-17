#pragma once

#ifndef SPREADSHEETVIEWER_H
#define SPREADSHEETVIEWER_H

#include "tcommon.h"
#include <QFrame>
#include <QScrollArea>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TFrameHandle;
class SpreadsheetViewer;

//-------------------------------------------------------------------

namespace Spreadsheet
{

// forward declaration
class GenericPanel;

//-------------------------------------------------------------------

class DVAPI FrameScroller
{
	QList<FrameScroller *> m_connectedScrollers;

public:
	FrameScroller();
	virtual ~FrameScroller();
	void connectScroller(FrameScroller *scroller);
	void disconnectScroller(FrameScroller *scroller);
	bool isScrollerConnected(FrameScroller *scroller);

	virtual QScrollArea *getFrameScrollArea() const = 0;

	void registerFrameScroller();
	void unregisterFrameScroller();

	void prepareToScroll(int dy);
	virtual void onPrepareToScroll(int dy) {}
};

//-------------------------------------------------------------------

class DVAPI DragTool
{
public:
	DragTool() {}
	virtual ~DragTool() {}

	virtual void click(int row, int col, QMouseEvent *e) {}
	virtual void drag(int row, int col, QMouseEvent *e) {}
	virtual void release(int row, int col, QMouseEvent *e) {}
};

//-------------------------------------------------------------------

class DVAPI SetFrameDragTool : public DragTool
{
	TFrameHandle *m_frameHandle;

public:
	SetFrameDragTool(TFrameHandle *frameHandle) : m_frameHandle(frameHandle) {}

	void click(int row, int col, QMouseEvent *e);
	void drag(int row, int col, QMouseEvent *e);
	void release(int row, int col, QMouseEvent *e);
};

//-------------------------------------------------------------------

class DVAPI SelectionDragTool : public DragTool
{
	SpreadsheetViewer *m_viewer;
	int m_firstRow, m_firstCol;

public:
	SelectionDragTool(SpreadsheetViewer *viewer);

	void click(int row, int col, QMouseEvent *e);
	void drag(int row, int col, QMouseEvent *e);
	void release(int row, int col, QMouseEvent *e);
};

//-------------------------------------------------------------------

class DVAPI PanTool : public DragTool
{
	SpreadsheetViewer *m_viewer;
	GenericPanel *m_panel;
	QPoint m_lastPos;

public:
	PanTool(GenericPanel *panel);

	void click(int row, int col, QMouseEvent *e);
	void drag(int row, int col, QMouseEvent *e);
	void release(int row, int col, QMouseEvent *e);
};

//-------------------------------------------------------------------

class DVAPI ScrollArea : public QScrollArea
{
	Q_OBJECT

public:
#if QT_VERSION >= 0x050500
	ScrollArea(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
	ScrollArea(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
	virtual ~ScrollArea();

protected:
	// keyPressEvent and wheelEvent are ignored by the ScrollArea
	// and therefore they are handled by the parent (Viewer)
	void keyPressEvent(QKeyEvent *e);
	void wheelEvent(QWheelEvent *e);
};

//-------------------------------------------------------------------

class DVAPI GenericPanel : public QWidget
{
	Q_OBJECT
	SpreadsheetViewer *m_viewer;
	DragTool *m_dragTool;

public:
	GenericPanel(SpreadsheetViewer *viewer);
	virtual ~GenericPanel();

	SpreadsheetViewer *getViewer() const { return m_viewer; }

	virtual DragTool *createDragTool(QMouseEvent *) { return 0; };

protected:
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void mouseMoveEvent(QMouseEvent *);
};

//-------------------------------------------------------------------

class DVAPI RowPanel : public GenericPanel
{
	Q_OBJECT
	const int m_xa; // frame cells start at m_xa
public:
	RowPanel(SpreadsheetViewer *viewer);
	virtual ~RowPanel() {}

	DragTool *createDragTool(QMouseEvent *);

protected:
	void paintEvent(QPaintEvent *);
	void drawRows(QPainter &p, int r0, int r1);
	void drawCurrentRowGadget(QPainter &p, int r0, int r1);
};

//-------------------------------------------------------------------

class DVAPI ColumnPanel : public GenericPanel
{
	Q_OBJECT
public:
	ColumnPanel(SpreadsheetViewer *viewer);
	virtual ~ColumnPanel() {}
};

//-------------------------------------------------------------------

class DVAPI CellPanel : public GenericPanel
{
	Q_OBJECT

public:
	CellPanel(SpreadsheetViewer *viewer);
	virtual ~CellPanel() {}

	DragTool *createDragTool(QMouseEvent *);

protected:
	void paintEvent(QPaintEvent *);
	virtual void drawCells(QPainter &p, int r0, int c0, int r1, int c1) {}
};
}

//-------------------------------------------------------------------

class DVAPI SpreadsheetViewer : public QFrame, public Spreadsheet::FrameScroller
{
	Q_OBJECT

	QColor m_lightLightBgColor; // RowPanel background (124,124,124)
	QColor m_bgColor;			// RowPanel background in scene range(164,164,164)
	QColor m_lightLineColor;	// horizontal line (146,144,146)

	Q_PROPERTY(QColor LightLightBGColor READ getLightLightBGColor WRITE setLightLightBGColor)
	Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)
	Q_PROPERTY(QColor LightLineColor READ getLightLineColor WRITE setLightLineColor)

	QColor m_currentRowBgColor; //current frame, column
	QColor m_markerLineColor;   //marker interval (0, 255, 246)
	QColor m_textColor;			//text (black)
	QColor m_verticalLineColor; //vertical line (black)
	Q_PROPERTY(QColor CurrentRowBgColor READ getCurrentRowBgColor WRITE setCurrentRowBgColor)
	Q_PROPERTY(QColor MarkerLineColor READ getMarkerLineColor WRITE setMarkerLineColor)
	Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)
	Q_PROPERTY(QColor VerticalLineColor READ getVerticalLineColor WRITE setVerticalLineColor)

	//key frame
	QColor m_keyFrameColor;			// (219,139,54)
	QColor m_keyFrameBorderColor;   // (82,51,20)
	QColor m_selectedKeyFrameColor; // (237,197,155)
	//key frame inbetween
	QColor m_inBetweenColor;		 // (194,194,176)
	QColor m_inBetweenBorderColor;   // (72,72,65)
	QColor m_selectedInBetweenColor; // (225,225,216)
	//empty cell
	QColor m_selectedEmptyColor; // (190,190,190)
	//empty cell in the scene range
	QColor m_selectedSceneRangeEmptyColor; // (210,210,210)
	Q_PROPERTY(QColor KeyFrameColor READ getKeyFrameColor WRITE setKeyFrameColor)
	Q_PROPERTY(QColor KeyFrameBorderColor READ getKeyFrameBorderColor WRITE setKeyFrameBorderColor)
	Q_PROPERTY(QColor SelectedKeyFrameColor READ getSelectedKeyFrameColor WRITE setSelectedKeyFrameColor)
	Q_PROPERTY(QColor InBetweenColor READ getInBetweenColor WRITE setInBetweenColor)
	Q_PROPERTY(QColor InBetweenBorderColor READ getInBetweenBorderColor WRITE setInBetweenBorderColor)
	Q_PROPERTY(QColor SelectedInBetweenColor READ getSelectedInBetweenColor WRITE setSelectedInBetweenColor)
	Q_PROPERTY(QColor SelectedEmptyColor READ getSelectedEmptyColor WRITE setSelectedEmptyColor)
	Q_PROPERTY(QColor SelectedSceneRangeEmptyColor READ getSelectedSceneRangeEmptyColor WRITE setSelectedSceneRangeEmptyColor)

	QColor m_columnHeaderBorderColor; //column header border lines (46,47,46)
	QColor m_selectedColumnTextColor; //selected column text (red)
	Q_PROPERTY(QColor ColumnHeaderBorderColor READ getColumnHeaderBorderColor WRITE setColumnHeaderBorderColor)
	Q_PROPERTY(QColor SelectedColumnTextColor READ getSelectedColumnTextColor WRITE setSelectedColumnTextColor)

	Spreadsheet::ScrollArea *m_columnScrollArea;
	Spreadsheet::ScrollArea *m_rowScrollArea;
	Spreadsheet::ScrollArea *m_cellScrollArea;
	TFrameHandle *m_frameHandle;

	int m_columnWidth;
	int m_rowHeight;

	// QPoint m_delta;
	int m_timerId;
	QPoint m_autoPanSpeed;
	QPoint m_lastAutoPanPos;
	int m_rowCount, m_columnCount;
	int m_currentRow;
	int m_markRowDistance, m_markRowOffset;
	// QRect m_selectedCells; // x=col, y=row
	bool m_isComputingSize;

public:
	SpreadsheetViewer(QWidget *parent);
	virtual ~SpreadsheetViewer();

	int getColumnWidth() const { return m_columnWidth; }
	void setColumnWidth(int width) { m_columnWidth = width; }

	int getRowHeight() const { return m_rowHeight; }
	void setRowHeight(int height) { m_rowHeight = height; }

	void setRowsPanel(Spreadsheet::RowPanel *rows);
	void setColumnsPanel(Spreadsheet::ColumnPanel *columns);
	void setCellsPanel(Spreadsheet::CellPanel *cells);

	int getRowCount() const { return m_rowCount; }

	// provvisorio
	QScrollArea *getFrameScrollArea() const { return m_cellScrollArea; }

	//QProperty
	void setLightLightBGColor(const QColor &color) { m_lightLightBgColor = color; }
	QColor getLightLightBGColor() const { return m_lightLightBgColor; }
	void setBGColor(const QColor &color) { m_bgColor = color; }
	QColor getBGColor() const { return m_bgColor; }
	void setLightLineColor(const QColor &color) { m_lightLineColor = color; }
	QColor getLightLineColor() const { return m_lightLineColor; }
	void setCurrentRowBgColor(const QColor &color) { m_currentRowBgColor = color; }
	QColor getCurrentRowBgColor() const { return m_currentRowBgColor; }
	void setMarkerLineColor(const QColor &color) { m_markerLineColor = color; }
	QColor getMarkerLineColor() const { return m_markerLineColor; }
	void setTextColor(const QColor &color) { m_textColor = color; }
	QColor getTextColor() const { return m_textColor; }
	void setVerticalLineColor(const QColor &color) { m_verticalLineColor = color; }
	QColor getVerticalLineColor() const { return m_verticalLineColor; }
	void setKeyFrameColor(const QColor &color) { m_keyFrameColor = color; }
	QColor getKeyFrameColor() const { return m_keyFrameColor; }
	void setKeyFrameBorderColor(const QColor &color) { m_keyFrameBorderColor = color; }
	QColor getKeyFrameBorderColor() const { return m_keyFrameBorderColor; }
	void setSelectedKeyFrameColor(const QColor &color) { m_selectedKeyFrameColor = color; }
	QColor getSelectedKeyFrameColor() const { return m_selectedKeyFrameColor; }
	void setInBetweenColor(const QColor &color) { m_inBetweenColor = color; }
	QColor getInBetweenColor() const { return m_inBetweenColor; }
	void setInBetweenBorderColor(const QColor &color) { m_inBetweenBorderColor = color; }
	QColor getInBetweenBorderColor() const { return m_inBetweenBorderColor; }
	void setSelectedInBetweenColor(const QColor &color) { m_selectedInBetweenColor = color; }
	QColor getSelectedInBetweenColor() const { return m_selectedInBetweenColor; }
	void setSelectedEmptyColor(const QColor &color) { m_selectedEmptyColor = color; }
	QColor getSelectedEmptyColor() const { return m_selectedEmptyColor; }
	void setSelectedSceneRangeEmptyColor(const QColor &color) { m_selectedSceneRangeEmptyColor = color; }
	QColor getSelectedSceneRangeEmptyColor() const { return m_selectedSceneRangeEmptyColor; }
	void setColumnHeaderBorderColor(const QColor &color) { m_columnHeaderBorderColor = color; }
	QColor getColumnHeaderBorderColor() const { return m_columnHeaderBorderColor; }
	void setSelectedColumnTextColor(const QColor &color) { m_selectedColumnTextColor = color; }
	QColor getSelectedColumnTextColor() const { return m_selectedColumnTextColor; }

	void scroll(QPoint delta);
	void onPrepareToScroll(int dy) { refreshContentSize(0, dy); }

	void setAutoPanSpeed(const QPoint &speed);
	void setAutoPanSpeed(const QRect &widgetBounds, const QPoint &mousePos);
	void stopAutoPan() { setAutoPanSpeed(QPoint()); }
	bool isAutoPanning() const { return m_autoPanSpeed.x() != 0 || m_autoPanSpeed.y() != 0; }

	int xToColumn(int x) const;
	int yToRow(int y) const;
	int columnToX(int col) const;
	int rowToY(int row) const;

	bool refreshContentSize(int scrollDx, int scrollDy);

	int getCurrentRow() const { return m_currentRow; }
	void setCurrentRow(int row) { m_currentRow = row; }

	virtual QRect getSelectedCells() const = 0;
	virtual void selectCells(const QRect &selectedCells) = 0;

	bool isSelectedCell(int row, int col) const
	{
		return getSelectedCells().contains(QPoint(col, row));
	}
	void setMarkRow(int distance, int offset)
	{
		m_markRowDistance = distance > 0 ? distance : 6;
		m_markRowOffset = offset;
	}
	void getMarkRow(int &distance, int &offset) const
	{
		distance = m_markRowDistance;
		offset = m_markRowOffset;
	}
	int isMarkRow(int row) const { return (row - m_markRowOffset) % m_markRowDistance == 0; }

	void setFrameHandle(TFrameHandle *frameHandle);
	TFrameHandle *getFrameHandle() const { return m_frameHandle; }

	void ensureVisibleCol(int col);

protected:
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);
	void resizeEvent(QResizeEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void wheelEvent(QWheelEvent *event);
	void timerEvent(QTimerEvent *);

public slots:
	void setRowCount(int rowCount);
	void setColumnCount(int columnCount);

	void frameSwitched();

	void updateAreas();
	void onVSliderChanged(int);
	void onHSliderChanged(int);
	/*
  void updateAllAree();
  void updateCellColumnAree();
  void updateCellRowAree();
  */
};

#endif
