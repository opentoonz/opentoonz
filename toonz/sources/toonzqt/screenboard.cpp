

#include <QPaintEvent>
#include <QDesktopWidget>
#include <QApplication>
#include <QMetaObject>
#include <QCursor>
#include <QPainter>

#include "toonzqt/screenboard.h"

using namespace DVGui;

#include <QPalette>

//***********************************************************************************
//    Local namespace
//***********************************************************************************

namespace {

class MouseTrackerDrawing final : public ScreenBoard::Drawing {
public:
  bool acceptScreenEvents(const QRect &rect) const override {
    return rect.contains(QCursor::pos());
  }
  void paintEvent(QWidget *widget, QPaintEvent *pe) override {
// Seems that mouse masking is on by default on the drawn regions, when using
// WA_TranslucentBackground (which is weird). I think it's the Qt 2 autoMask
// feature
// that disappeared from Qt 3 on - now it must be managed internally.

// So, we have to fill the entire screen region with the most invisible color
// possible...

#ifdef MACOSX
#define MIN_ALPHA                                                              \
  13  // Again, very weird. 13 is the minimal alpha that gets converted to
      // 'mask-opaque' on MAC...
#else
#define MIN_ALPHA 1  // ... whereas 1 is sufficient on Windows. Argh!
#endif

    QPainter painter(widget);
    painter.fillRect(0, 0, widget->width(), widget->height(),
                     QColor(0, 0, 0, MIN_ALPHA));
  }

} tracker;

}  // namespace

//***********************************************************************************
//    ScreenWidget implementation
//***********************************************************************************

class ScreenBoard::ScreenWidget final : public QWidget {
  QList<ScreenBoard::Drawing *>
      m_drawings;        //!< Drawings intersecting the screen
  bool m_mouseOnScreen;  //!< Whether the mouse is inside this screen

public:
  ScreenWidget(QWidget *parent = 0, bool grabbing = false)
      : QWidget(
            parent,
            Qt::Tool |  // Tool does not force focus changes upon shows
                Qt::FramelessWindowHint |  // No decorations
                Qt::WindowStaysOnTopHint)  // Must be above all other windows
  {
    setAttribute(Qt::WA_TransparentForMouseEvents,
                 !grabbing);                     // Receives mouse events?
    setAttribute(Qt::WA_TranslucentBackground);  // Transparent widget
    setFocusPolicy(Qt::NoFocus);

    setMouseTracking(true);  // When receiving mouse events
  }

  const QList<Drawing *> &drawings() const { return m_drawings; }
  QList<Drawing *> &drawings() { return m_drawings; }

  bool mouseOnScreen() const { return m_mouseOnScreen; }

protected:
  bool event(QEvent *e) override {
    int i, size = m_drawings.size();
    if (e->type() == QEvent::Paint) {
      // Invoke paint events in reversed sorting order
      for (i = size - 1; i >= 0; --i)
        m_drawings[i]->paintEvent(this, static_cast<QPaintEvent *>(e));
    }

    // Invoke other events in straight sorting order
    for (i = 0; i < size; ++i) m_drawings[i]->event(this, e);

    return QWidget::event(e);
  }

  void leaveEvent(QEvent *e) override {
    m_mouseOnScreen = false;

    ScreenBoard *screenBoard = ScreenBoard::instance();
    if (screenBoard->m_grabbing) screenBoard->ensureMouseOnAScreen();
  }

  void enterEvent(QEvent *e) override {
    m_mouseOnScreen                           = true;
    ScreenBoard::instance()->m_mouseOnAScreen = true;
  }
};

//***********************************************************************************
//    ScreenBoard implementation
//***********************************************************************************

ScreenBoard::ScreenBoard() : m_grabbing(false) {}

//------------------------------------------------------------------------------

ScreenBoard *ScreenBoard::instance() {
  static ScreenBoard theInstance;
  return &theInstance;
}

//------------------------------------------------------------------------------

/*!
  Makes the ScreenBoard catch all mouse events (effectively preventing other
  windows or applications
  to get them), and delivers them to drawings. An appropriate cursor should be
  specified to inform the
  user that tout-court mouse grabbing takes place.
*/
void ScreenBoard::grabMouse(const QCursor &cursor) {
  m_grabbing = true;
  m_cursor   = cursor;

  // Place a mouse-tracking dummy drawing among drawings
  m_drawings.push_back(&::tracker);

  // Make all screen widgets react to mouse events, and show them
  int i, size = m_screenWidgets.size();
  for (i = 0; i < size; ++i) {
    QWidget *screenWidget = m_screenWidgets[i];
    if (screenWidget) {
      screenWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
      screenWidget->setCursor(m_cursor);
    }
  }
}

//------------------------------------------------------------------------------

/*!
  Restores the ScreenBoard to ignore mouse events (the default behaviour) after
  a
  call to grabMouse().
*/
void ScreenBoard::releaseMouse() {
  // Restore screen widgets to ignore mouse events
  int i, size = m_screenWidgets.size();
  for (i = 0; i < size; ++i) {
    QWidget *screenWidget = m_screenWidgets[i];
    if (screenWidget) {
      screenWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
      screenWidget->unsetCursor();
    }
  }

  // Remove the mouse-tracking drawing
  m_drawings.removeAt(m_drawings.indexOf(&::tracker));

  m_cursor   = QCursor();
  m_grabbing = false;
}

//------------------------------------------------------------------------------

// Refresh the screen widgets pool, depending on stored drawings
void ScreenBoard::reallocScreenWidgets() {
  QDesktopWidget *desktop = QApplication::desktop();

  int i, screensCount = desktop->numScreens();

  // Delete exceeding screens and resize to screensCount
  for (i = screensCount; i < m_screenWidgets.size(); ++i) {
    m_screenWidgets[i]->hide();
    m_screenWidgets[i]->deleteLater();  // Ensures no event about it is pending.
    // Note that updates may be invoked in event handlers.
  }

  m_screenWidgets.resize(desktop->numScreens());

  // Re-initialize the screen widgets list
  for (i = 0; i < screensCount; ++i) {
    ScreenWidget *screenWidget = m_screenWidgets[i];
    if (screenWidget) screenWidget->drawings().clear();
  }

  // Turn on a ScreenWidget for each screen crossed by any drawing
  int j, drawingsCount = m_drawings.size();
  for (i = 0; i < screensCount; ++i) {
    ScreenWidget *screenWidget = m_screenWidgets[i];
    const QRect &screenGeom    = desktop->screenGeometry(i);

    for (j = 0; j < drawingsCount; ++j) {
      Drawing *drawing = m_drawings[j];
      if (drawing->acceptScreenEvents(screenGeom)) {
        // Allocate the associated screen widget if necessary
        if (!screenWidget) {
          m_screenWidgets[i] = screenWidget = new ScreenWidget(0, m_grabbing);
          if (m_grabbing) screenWidget->setCursor(m_cursor);

          screenWidget->setGeometry(screenGeom);
          screenWidget->show();
        }

        // Add the drawing to the widget
        screenWidget->drawings().push_back(drawing);
      }
    }
  }

  // Remove screens without drawings
  for (i = 0; i < screensCount; ++i) {
    ScreenWidget *screenWidget = m_screenWidgets[i];
    if (screenWidget && screenWidget->drawings().empty()) {
      screenWidget->hide();
      screenWidget->deleteLater();
      m_screenWidgets[i] = 0;
    }
  }
}

//------------------------------------------------------------------------------

/*!
  This function must be called whenever a drawing needs to be refreshed.
  It has 2 consequences:

    \li The pool of invisible screen widgets that are used to catch Qt events
        is refreshed to satisfy the drawings requirements. Screen widgets whose
        events are not needed are released from the pool.

    \li All screen widgets are updated, propagating paintEvents to their
  drawings.

  Observe that this function awaits return to the main event loop before being
  processed.
  This ensures that update calls are processed outside event handlers, and
  multiple
  calls from different drawings gets coalesced into one.
*/
void ScreenBoard::update() {
  m_updated = false;
  QMetaObject::invokeMethod(this, "doUpdate", Qt::QueuedConnection);
}

//------------------------------------------------------------------------------

void ScreenBoard::doUpdate() {
  if (m_updated)  // Weak updates coalescence. It helps.
    return;

  m_updated = true;

  reallocScreenWidgets();

  // Update all screenWidgets
  int i, size = m_screenWidgets.size();
  for (i = 0; i < size; ++i)
    if (m_screenWidgets[i]) m_screenWidgets[i]->update();
}

//------------------------------------------------------------------------------

void ScreenBoard::ensureMouseOnAScreen() {
  // Find out if the mouse is on a screen
  m_mouseOnAScreen = false;

  int i, size = m_screenWidgets.size();
  for (i = 0; i < size; ++i) {
    ScreenWidget *screenWidget = m_screenWidgets[i];
    if (screenWidget) m_mouseOnAScreen |= screenWidget->mouseOnScreen();
  }

  if (!m_mouseOnAScreen)
    // Ensure that there is a screen under the mouse cursor.
    // We need a slot invocation, since this method could be called in an event
    // handler.
    QMetaObject::invokeMethod(this, "trackCursor", Qt::QueuedConnection);
}

//------------------------------------------------------------------------------

void ScreenBoard::trackCursor() {
  while (!m_mouseOnAScreen) {
    update();  // Refresh screens pool
    QCoreApplication::processEvents(
        QEventLoop::WaitForMoreEvents);  // Process events (-> enterEv.)
  }
}
