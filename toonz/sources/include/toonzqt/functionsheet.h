#pragma once

#ifndef FUNCTIONSHEET_H
#define FUNCTIONSHEET_H

#include "tcommon.h"
#include "functiontreeviewer.h"
#include "spreadsheetviewer.h"
#include "functionselection.h"
#include "toonzqt/lineedit.h"

#include <QWidget>
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class FunctionSheet;
class TDoubleParam;
class TFrameHandle;
class FunctionSelection;

class FunctionSheetRowViewer : public Spreadsheet::RowPanel {
  FunctionSheet *m_sheet;

public:
  FunctionSheetRowViewer(FunctionSheet *parent);

protected:
  void paintEvent(QPaintEvent *);

  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
  void contextMenuEvent(QContextMenuEvent *);
};

class FunctionSheetColumnHeadViewer : public Spreadsheet::ColumnPanel {
  FunctionSheet *m_sheet;
  // enable drag and drop the expression arguments
  QPoint m_dragStartPosition;
  FunctionTreeModel::Channel *m_draggingChannel;

public:
  FunctionSheetColumnHeadViewer(FunctionSheet *parent);

protected:
  void paintEvent(QPaintEvent *);
  void mousePressEvent(QMouseEvent *);
  // update the tooltip
  void mouseMoveEvent(QMouseEvent *);

  void contextMenuEvent(QContextMenuEvent *);
};

class FunctionSheetCellViewer : public Spreadsheet::CellPanel {
  Q_OBJECT
  FunctionSheet *m_sheet;
  DVGui::LineEdit *m_lineEdit;
  int m_editRow, m_editCol;

public:
  FunctionSheetCellViewer(FunctionSheet *parent);

  Spreadsheet::DragTool *createDragTool(QMouseEvent *);

protected:
  void drawCells(QPainter &p, int r0, int c0, int r1, int c1);

  void mouseDoubleClickEvent(QMouseEvent *);

  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
  void openContextMenu(QMouseEvent *);

private slots:
  void onCellEditorEditingFinished();
};

class FunctionSheet : public SpreadsheetViewer {
  Q_OBJECT

public:
  FunctionSheet(QWidget *parent = 0);
  ~FunctionSheet();

  void setModel(FunctionTreeModel *model);
  FunctionTreeModel *getModel() const { return m_functionTreeModel; }

  void setViewer(FunctionViewer *viewer);
  FunctionViewer *getViewer() const { return m_functionViewer; }

  void setCurrentFrame(int frame);
  int getCurrentFrame() const;

  int getChannelCount();
  TDoubleParam *getCurve(int column);
  FunctionTreeModel::Channel *getChannel(int column);

  QRect getSelectedCells() const;
  void selectCells(const QRect &selectedCells);

  FunctionSelection *getSelection() const { return m_selection; }
  void setSelection(FunctionSelection *selection);  // does NOT get ownership

  QString getSelectedParamName();
  int getColumnIndexByCurve(TDoubleParam *param) const;
  bool anyWidgetHasFocus();

protected:
  void showEvent(QShowEvent *e);
  void hideEvent(QHideEvent *e);

private:
  FunctionSheetRowViewer *m_rowViewer;
  FunctionSheetColumnHeadViewer *m_columnHeadViewer;
  FunctionSheetCellViewer *m_cellViewer;
  FunctionSelection *m_selection;
  FunctionTreeModel *m_functionTreeModel;
  FunctionViewer *m_functionViewer;

  QRect m_selectedCells;

public slots:

  void updateAll();
  void onFrameSwitched();
  /*---
   * カレントChannelが切り替わったら、NumericalColumnsがそのChannelを表示できるようにスクロールする。---*/
  void onCurrentChannelChanged(FunctionTreeModel::Channel *);
};

#endif
