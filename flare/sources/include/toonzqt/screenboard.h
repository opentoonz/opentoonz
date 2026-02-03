#pragma once

#ifndef SCREENBOARD_H
#define SCREENBOARD_H

#include "tcommon.h"

#include <QWidget>
#include <QList>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace DVGui {

//****************************************************************************
//    ScreenBoard class
//****************************************************************************

//! The ScreenBoard is a singleton class that allows self-drawing objects to
//! be rendered on the whole desktop surface.

/*! The Qt Framework does not provide a standard way to draw directly on the
    desktop.
\n\n
    Typically, users have to allocate a QWidget to host any drawing command
    to be rendered - but the rendering surface only covers the entirety of the
    widget geometry, and nothing more.

    Plus, the maximal drawing geometry should be known in advance when using a
    host widget, or else heavy flickering will result from attempts to move
    \a and resize the widget to cover new portions of the desktop.
\n\n
    Another important use for the ScreenBoard is that of allowing dektop-wide
mouse
    grabbing through the grabMouse() and releaseMouse() functions.
\n\n
    The ScreenBoard stores a private collection of static, inert and transparent
    <I> screen widgets <\I>, each overlapping a desktop screen, to be used as
    drawable surfaces for self-drawing objects (ScreenBoard::Drawing instances)
    that are added to the board.
\n\n
    Drawings can be added to the board by direct manipulation of the drawings()
    container list.

    Use the update() method to refresh the screen after drawing insertions or
    removals.
\n\n
    Screen widgets redirect any event they receive to drawings that accept
redirection
    through the Drawings::accpetScreenEvents() method.

    The drawings() list sorting affects the order in which events from screen
widgets
    are delivered to drawings.

    In particular, drawings' <I> stacking order <\I> is \b inverse to the their
list
    ordering - so, paint events are received in \b reverse with respect to other
    events. Event acceptance is ignored to determine event delivery to drawings.
\n\n
    Observe that upon every update() invocation, the screen widgets pool will
    be refreshed to keep it to a minimum.
*/

class DVAPI ScreenBoard final : public QObject {
  Q_OBJECT

public:
  class Drawing;

private:
  class ScreenWidget;

  QVector<ScreenWidget *> m_screenWidgets;
  QList<Drawing *> m_drawings;

  QCursor m_cursor;

  bool m_grabbing;
  bool m_mouseOnAScreen;
  bool m_updated;

public:
  static ScreenBoard *instance();

  const QList<Drawing *> &drawings() const { return m_drawings; }
  QList<Drawing *> &drawings() { return m_drawings; }

  void grabMouse(
      const QCursor &cursor);  //!< Grabs mouse inputs across the whole desktop.
  void releaseMouse();         //!< Releases the mouse grab after grabMouse().

  bool grabbingMouse() const {
    return m_grabbing;
  }  //!< Whether mouse grabbing is on.

private:
  ScreenBoard();

  void reallocScreenWidgets();
  void ensureMouseOnAScreen();

public slots:

  void update();  //!< Refreshes the screen widgets pool and updates them.

private slots:

  friend class ScreenWidget;

  void trackCursor();
  void doUpdate();
};

//****************************************************************************
//    ScreenBoard::Drawing class
//****************************************************************************

//! ScreenDrawing is the base class for objects that can be painted directly
//! in screen coordinates through the ScreenBoard.
class ScreenBoard::Drawing {
public:
  //! Generic event handler for drawings.
  /*! Reimplement it to receive events from screen widgets other than paint
events.
Paint events are \b not received in this handler, since they must be delivered
in reverse order.
*/
  virtual void event(QWidget *widget, QEvent *e) {}

  //! Paints the drawing on the specified screen widget. Use the widget's
  //! mapFromGlobal() function to match desktop coordinates to screen
  //! coordinates.
  virtual void paintEvent(QWidget *widget, QPaintEvent *pe) = 0;

  //! Returns whether the drawing is interested in events from the passed screen
  //! geometry.
  //! Accepting a screen causes a screen widget to be allocated in order to
  //! receive events.
  virtual bool acceptScreenEvents(const QRect &screenRect) const = 0;
};

}  // namespace DVGui

#endif  // SCREENBOARD_H
