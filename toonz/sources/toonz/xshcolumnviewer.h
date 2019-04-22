#pragma once

#ifndef XSHCOLUMNVIEWER_H
#define XSHCOLUMNVIEWER_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPoint>
#include <QColor>

// forward declaration
class XsheetViewer;
class TObjectHandle;
class TXsheetHandle;
class TStageObjectId;
class TXshColumn;
class QComboBox;
class Orientation;
class TApp;
class TXsheet;

//=============================================================================
namespace XsheetGUI {

class DragTool;

//=============================================================================
// MotionPathMenu
//-----------------------------------------------------------------------------

class MotionPathMenu final : public QWidget {
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

class ChangeObjectParent final : public ChangeObjectWidget {
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

class ChangeObjectHandle final : public ChangeObjectWidget {
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

class RenameColumnField final : public QLineEdit {
  Q_OBJECT

  int m_col;

  TXsheetHandle *m_xsheetHandle;

public:
  RenameColumnField(QWidget *parent, XsheetViewer *viewer);
  ~RenameColumnField() {}

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }

  void show(const QRect &rect, int col);

protected:
  void focusOutEvent(QFocusEvent *) override;

protected slots:
  void renameColumn();
};

//=============================================================================
// ColumnArea
//-----------------------------------------------------------------------------

class ColumnTransparencyPopup final : public QWidget {
  Q_OBJECT

  QSlider *m_slider;
  QLineEdit *m_value;
  TXshColumn *m_column;

  QComboBox *m_filterColorCombo;

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

  void onFilterColorChanged(int id);
};

class SoundColumnPopup final : public QWidget {
  Q_OBJECT

  QSlider *m_slider;
  QLineEdit *m_value;
  TXshColumn *m_column;

public:
  SoundColumnPopup(QWidget *parent);
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

//! The class in charge of the region showing layer headers
class ColumnArea final : public QWidget {
  Q_OBJECT

  enum {
    ToggleTransparency = 1,
    ToggleAllTransparency,
    TogglePreviewVisible,
    ToggleAllPreviewVisible,
    ToggleLock,
    ToggleAllLock,
    OpenSettings
  };

  ColumnTransparencyPopup *m_columnTransparencyPopup;
  SoundColumnPopup *m_soundColumnPopup;
  QTimer *m_transparencyPopupTimer;
  int m_doOnRelease;
  XsheetViewer *m_viewer;
  int m_col;
  QRect m_indexBox;
  QRect m_tabBox;
  QRect m_nameBox;
  QRect m_linkBox;

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

  // extracted all variables of drawSomething methods
  class DrawHeader {
    ColumnArea *area;
    QPainter &p;
    int col;
    XsheetViewer *m_viewer;
    const Orientation *o;
    TApp *app;
    TXsheet *xsh;
    bool isEmpty, isCurrent;
    TXshColumn *column;
    QPoint orig;

  public:
    DrawHeader(ColumnArea *area, QPainter &p, int col);

    void prepare() const;

    void levelColors(QColor &columnColor, QColor &dragColor) const;
    void soundColors(QColor &columnColor, QColor &dragColor) const;
    void paletteColors(QColor &columnColor, QColor &dragColor) const;

    void drawBaseFill(const QColor &columnColor, const QColor &dragColor) const;
    void drawEye() const;
    void drawPreviewToggle(int opacity) const;
    void drawLock() const;
    void drawConfig() const;
    void drawColumnNumber() const;
    void drawColumnName() const;
    void drawThumbnail(QPixmap &iconPixmap) const;
    void drawPegbarName() const;
    void drawParentHandleName() const;
    void drawFilterColor() const;

    void drawSoundIcon(bool isPlaying) const;
    void drawVolumeControl(double volume) const;
  };

public:
#if QT_VERSION >= 0x050500
  ColumnArea(XsheetViewer *parent, Qt::WindowFlags flags = 0);
#else
  ColumnArea(XsheetViewer *parent, Qt::WFlags flags = 0);
#endif
  ~ColumnArea();

  void onControlPressed(bool pressed);
  const bool isControlPressed();

  void drawFoldedColumnHead(QPainter &p, int col);
  void drawLevelColumnHead(QPainter &p, int col);
  void drawSoundColumnHead(QPainter &p, int col);
  void drawPaletteColumnHead(QPainter &p, int col);
  void drawSoundTextColumnHead(QPainter &p, int col);

  QPixmap getColumnIcon(int columnIndex);

  class Pixmaps {
  public:
    static const QPixmap &sound();
    static const QPixmap &soundPlaying();
  };

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
  void openSoundColumnPopup();
};

//-----------------------------------------------------------------------------
}  // namespace XsheetGUI;
//-----------------------------------------------------------------------------

#endif  // XSHCOLUMNVIEWER_H
