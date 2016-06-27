#pragma once

#ifndef XSHCOLUMNVIEWER_H
#define XSHCOLUMNVIEWER_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>

// forward declaration
class XsheetViewer;
class TObjectHandle;
class TXsheetHandle;
class TStageObjectId;
class TXshColumn;

//=============================================================================
namespace XsheetGUI {

class DragTool;

//=============================================================================
// MotionPathMenu
//-----------------------------------------------------------------------------

class MotionPathMenu : public QWidget {
  Q_OBJECT

  QRect m_mDeleteRect;
  QRect m_mNormalRect;
  QRect m_mRotateRect;
  QPoint m_pos;

public:
#if QT_VERSION >= 0x050500
  MotionPathMenu(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  MotionPathMenu(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif

  ~MotionPathMenu();

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
};

//=============================================================================
// ChangeParentObjectWidget
//-----------------------------------------------------------------------------

class ChangeObjectWidget : public QListWidget {
  Q_OBJECT

protected:
  TObjectHandle *m_objectHandle;
  TXsheetHandle *m_xsheetHandle;
  int m_width;

public:
  ChangeObjectWidget(QWidget *parent = 0);
  ~ChangeObjectWidget();

  void show(const QPoint &pos);
  void setObjectHandle(TObjectHandle *objectHandle);
  void setXsheetHandle(TXsheetHandle *xsheetHandle);

  virtual void refresh(){};

protected:
  void mouseMoveEvent(QMouseEvent *event) override;
  void focusOutEvent(QFocusEvent *e) override;
  void focusInEvent(QFocusEvent *e) override {}
  void selectCurrent(const QString &text);

protected slots:
  virtual void onTextChanged(const QString &) = 0;
};

//=============================================================================
// ChangeObjectParent
//-----------------------------------------------------------------------------

class ChangeObjectParent : public ChangeObjectWidget {
  Q_OBJECT

public:
  ChangeObjectParent(QWidget *parent = 0);
  ~ChangeObjectParent();

  void refresh() override;

protected slots:
  void onTextChanged(const QString &) override;
};

//=============================================================================
// ChangeObjectHandle
//-----------------------------------------------------------------------------

class ChangeObjectHandle : public ChangeObjectWidget {
  Q_OBJECT

public:
  ChangeObjectHandle(QWidget *parent = 0);
  ~ChangeObjectHandle();

  void refresh() override;

protected slots:
  void onTextChanged(const QString &) override;
};

//=============================================================================
// RenameColumnField
//-----------------------------------------------------------------------------

class RenameColumnField : public QLineEdit {
  Q_OBJECT

  int m_col;

  TXsheetHandle *m_xsheetHandle;

public:
  RenameColumnField(QWidget *parent, XsheetViewer *viewer);
  ~RenameColumnField() {}

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }

  void show(QPoint pos, int col);

protected:
  void focusOutEvent(QFocusEvent *) override;

protected slots:
  void renameColumn();
};

//=============================================================================
// ColumnArea
//-----------------------------------------------------------------------------

class ColumnTransparencyPopup : public QWidget {
  Q_OBJECT

  QSlider *m_slider;
  QLineEdit *m_value;
  TXshColumn *m_column;

public:
  ColumnTransparencyPopup(QWidget *parent);
  void setColumn(TXshColumn *column);

protected:
  // void mouseMoveEvent ( QMouseEvent * e );
  void mouseReleaseEvent(QMouseEvent *e) override;

protected slots:
  void onSliderReleased();
  void onSliderChange(int val);
  void onSliderValueChanged(int);
  void onValueChanged(const QString &);
};

//! La classe si occupa della visualizzazione dell'area che gestisce le colonne.
class ColumnArea : public QWidget {
  Q_OBJECT

  enum { ToggleTransparency = 1, TogglePreviewVisible, ToggleLock };

  ColumnTransparencyPopup *m_columnTransparencyPopup;
  QTimer *m_transparencyPopupTimer;
  int m_doOnRelease;
  XsheetViewer *m_viewer;
  int m_col;
  QRect m_indexBox;
  QRect m_tabBox;
  QRect m_nameBox;
  QRect m_linkBox;
  QRect m_prevViewBox;
  QRect m_tableViewBox;
  QRect m_lockBox;

  bool m_isPanning;

  QPoint m_pos;
  QString m_tooltip;

  RenameColumnField *m_renameColumnField;
#ifndef LINETEST
  ChangeObjectParent *m_changeObjectParent;
  ChangeObjectHandle *m_changeObjectHandle;
#else
  MotionPathMenu *m_motionPathMenu;
#endif

  QAction *m_subsampling1;
  QAction *m_subsampling2;
  QAction *m_subsampling3;
  QAction *m_subsampling4;

  DragTool *getDragTool() const;
  void setDragTool(DragTool *dragTool);
  void startTransparencyPopupTimer(QMouseEvent *e);

public:
#if QT_VERSION >= 0x050500
  ColumnArea(XsheetViewer *parent, Qt::WindowFlags flags = 0);
#else
  ColumnArea(XsheetViewer *parent, Qt::WFlags flags = 0);
#endif
  ~ColumnArea();

  void drawLevelColumnHead(QPainter &p, int col);
  void drawSoundColumnHead(QPainter &p, int col);
  void drawPaletteColumnHead(QPainter &p, int col);
  void drawSoundTextColumnHead(QPainter &p, int col);

  QPixmap getColumnIcon(int columnIndex);

protected:
  void select(int columnIndex, QMouseEvent *event);

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  bool event(QEvent *event) override;

protected slots:
  void onSubSampling(QAction *);
  void openTransparencyPopup();
};

//-----------------------------------------------------------------------------
}  // namespace XsheetGUI;
//-----------------------------------------------------------------------------

#endif  // XSHCOLUMNVIEWER_H
