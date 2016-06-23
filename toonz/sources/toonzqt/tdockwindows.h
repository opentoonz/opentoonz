#pragma once

#ifndef TDOCKWINDOWS_H
#define TDOCKWINDOWS_H

#include "tcommon.h"

#include "docklayout.h"

#include <QFrame>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//========================================================================

// Forward declarations
class TDockWidget;

//========================================================================

//-----------------------
//    TMainWindow
//-----------------------

/*!
TMainWindow class is intended as a convenience QMainWindow-like interface
for DockLayout.

\sa TDockWidget class.
*/
class DVAPI TMainWindow : public QWidget {
  Q_OBJECT  // Just needed by qobject_cast, for now

      DockLayout *m_layout;
  QWidget *m_menu;

public:
  TMainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);
  virtual ~TMainWindow();

  void addDockWidget(TDockWidget *item);
  void removeDockWidget(TDockWidget *item);

  void setMenuWidget(QWidget *menubar);
  QWidget *menuWidget() const { return m_menu; }

  // Style options
  void setDecoAllocator(DockDecoAllocator *allocator);
  void setSeparatorsThickness(int thick);

  DockLayout *dockLayout() const { return m_layout; }

protected:
  void resizeEvent(QResizeEvent *event) override;
};

//========================================================================

//-------------------
//    TDockWidget
//-------------------

/*!
TDockWidget class is a convenience QDockWidget-like interface to DockWidget
class. It provides an internal base layout including a titlebar and a
content widget.

\sa TMainWindow class.
*/
class DVAPI TDockWidget : public DockWidget {
  Q_OBJECT  // Needed by qobject_cast

      QWidget *m_titlebar;
  QWidget *m_widget;

  int m_margin;

public:
  TDockWidget(const QString &title, QWidget *parent = 0,
              Qt::WindowFlags flags = 0);
  TDockWidget(QWidget *parent = 0, Qt::WindowFlags flags = 0);
  ~TDockWidget() {}

  void setTitleBarWidget(QWidget *titlebar);
  QWidget *titleBarWidget() const { return m_titlebar; }
  void setFloatingMargin(int margin) { m_margin = margin; }
  int getFloatingMargin() const { return m_margin; }

  void setWidget(QWidget *widget);
  QWidget *widget() const { return m_widget; }

  void setFloating(bool status = true);
  void setMaximized(bool status = true);

  enum Orientation { horizontal = 0, vertical = 1 };
  void setOrientation(bool direction = vertical);
  bool getOrientation() const;

private:
  QSize getDockedMinimumSize() override;
  QSize getDockedMaximumSize() override;
  void setFloatingAppearance() override;
  void setDockedAppearance() override;

  void selectDockPlaceholder(QMouseEvent *me) override;

  bool isDragGrip(QPoint p) override;
  int isResizeGrip(QPoint p) override;

  void windowTitleEvent(QEvent *e) override;
};

#endif  // TDOCKWINDOWS_H
