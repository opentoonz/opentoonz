#pragma once

#ifndef XSHROWVIEWER_H
#define XSHROWVIEWER_H

#include <QWidget>

// forward declaration
class XsheetViewer;
class QMenu;

namespace XsheetGUI {

class DragTool;

//=============================================================================
// RowArea
//-----------------------------------------------------------------------------

//! La classe si occupa della visualizzazione dell'area che gestisce le righe.
class RowArea final : public QWidget {
  Q_OBJECT
  XsheetViewer *m_viewer;
  int m_row;

  enum ShowOnionToSetFlag {
    None = 0,
    Fos,
    Mos
  } m_showOnionToSet;  // TODO:明日はこれをFos,Mosどちらをハイライトしているのか判定させる！！！！

  enum Direction { up = 0, down };

  // Play ranges
  int m_r0;
  int m_r1;

  QPoint m_pos;
  bool m_playRangeActiveInMousePress;
  int m_mousePressRow;
  QString m_tooltip;

  // panning by middle-drag
  bool m_isPanning;

  void drawRows(QPainter &p, int r0, int r1);
  void drawPlayRange(QPainter &p, int r0, int r1);
  void drawCurrentRowGadget(QPainter &p, int r0, int r1);
  void drawOnionSkinSelection(QPainter &p);
  void drawPinnedCenterKeys(QPainter &p, int r0, int r1);

  DragTool *getDragTool() const;
  void setDragTool(DragTool *dragTool);

  // Return when the item-menu setAutoMarkers can be enabled.
  bool canSetAutoMarkers();
  // Return the number of the last non-empty cell finded. You can set the
  // direction of the search.
  int getNonEmptyCell(int row, int column, Direction);

public:
#if QT_VERSION >= 0x050500
  RowArea(XsheetViewer *parent, Qt::WindowFlags flags = 0);
#else
  RowArea(XsheetViewer *parent, Qt::WFlags flags = 0);
#endif
  ~RowArea();

protected:
  void paintEvent(QPaintEvent *) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  bool event(QEvent *event) override;

  void setMarker(int index);

protected slots:

  void onSetStartMarker();
  void onSetStopMarker();
  void onRemoveMarkers();

  // Set start and end marker automatically respect the current row and column.
  void onSetAutoMarkers();

  // set both the from and to markers at the specified row
  void onPreviewThis();
};

}  // namespace XsheetGUI;

#endif  // XSHROWVIEWER_H
