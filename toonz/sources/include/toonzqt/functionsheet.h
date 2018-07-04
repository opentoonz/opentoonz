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

class FunctionSheetRowViewer final : public Spreadsheet::RowPanel {
  FunctionSheet *m_sheet;

public:
  FunctionSheetRowViewer(FunctionSheet *parent);

protected:
  void paintEvent(QPaintEvent *) override;

  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;
};

class FunctionSheetColumnHeadViewer final : public Spreadsheet::ColumnPanel {
  FunctionSheet *m_sheet;
  // enable drag and drop the expression arguments
  QPoint m_dragStartPosition;
  FunctionTreeModel::Channel *m_draggingChannel;

public:
  FunctionSheetColumnHeadViewer(FunctionSheet *parent);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  // update the tooltip
  void mouseMoveEvent(QMouseEvent *) override;

  void contextMenuEvent(QContextMenuEvent *) override;
};

class FunctionSheetCellViewer final : public Spreadsheet::CellPanel {
  Q_OBJECT
  FunctionSheet *m_sheet;
  DVGui::LineEdit *m_lineEdit;
  int m_editRow, m_editCol;

  // for mouse dragging to adjust the value
  double m_currentValue = 0.0;
  double m_updatedValue = 0.0;
  int m_mouseXPosition;

public:
  FunctionSheetCellViewer(FunctionSheet *parent);

  Spreadsheet::DragTool *createDragTool(QMouseEvent *) override;

protected:
  void drawCells(QPainter &p, int r0, int c0, int r1, int c1) override;

  void mouseDoubleClickEvent(QMouseEvent *) override;

  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void openContextMenu(QMouseEvent *);

private slots:
  void onCellEditorEditingFinished();

  // double clicking opens the line edit where mouse dragging
  // can change the value.  It sends a signal to this slot.
  void onMouseMovedInLineEdit(QMouseEvent *event);
};

class FunctionSheet final : public SpreadsheetViewer {
  Q_OBJECT

public:
  FunctionSheet(QWidget *parent = 0, bool isFloating = false);
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

  QRect getSelectedCells() const override;
  void selectCells(const QRect &selectedCells) override;

  FunctionSelection *getSelection() const { return m_selection; }
  void setSelection(FunctionSelection *selection);  // does NOT get ownership

  QString getSelectedParamName();
  int getColumnIndexByCurve(TDoubleParam *param) const;
  bool anyWidgetHasFocus();

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

private:
  FunctionSheetRowViewer *m_rowViewer;
  FunctionSheetColumnHeadViewer *m_columnHeadViewer;
  FunctionSheetCellViewer *m_cellViewer;
  FunctionSelection *m_selection;
  FunctionTreeModel *m_functionTreeModel;
  FunctionViewer *m_functionViewer;

  QRect m_selectedCells;
  bool m_isFloating;

public slots:

  void updateAll();
  void onFrameSwitched();
  /*---
   * カレントChannelが切り替わったら、NumericalColumnsがそのChannelを表示できるようにスクロールする。---*/
  void onCurrentChannelChanged(FunctionTreeModel::Channel *);
};

#endif
