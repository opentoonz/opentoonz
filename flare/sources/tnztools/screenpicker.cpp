

#include "tcommon.h"

#include "toonzqt/pickrgbutils.h"
#include "toonzqt/screenboard.h"
#include "toonzqt/menubarcommand.h"

#include "tools/RGBpicker.h"
#include "tools/cursors.h"
#include "tools/cursormanager.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QCoreApplication>
#include <QCursor>
#include <QTimer>

#include "tools/screenpicker.h"

//***********************************************************************************
//    Screen Picker implementation
//***********************************************************************************

ScreenPicker::ScreenPicker(QWidget *parent)
    : m_mousePressed(false), m_mouseGrabbed(false) {}

//------------------------------------------------------------------

bool ScreenPicker::acceptScreenEvents(const QRect &screenRect) const {
  return screenRect.contains(QCursor::pos()) ||  // Accept mouse events
         screenRect.intersects(m_geometry);      // Accept paint events
}

//------------------------------------------------------------------

void ScreenPicker::event(QWidget *widget, QEvent *e) {
  switch (e->type()) {
  case QEvent::MouseButtonPress:
    mousePressEvent(widget, static_cast<QMouseEvent *>(e));
    break;

  case QEvent::MouseMove:
    mouseMoveEvent(widget, static_cast<QMouseEvent *>(e));
    break;

  case QEvent::MouseButtonRelease:
    mouseReleaseEvent(widget, static_cast<QMouseEvent *>(e));

  default:
    break;
  }
}

//------------------------------------------------------------------

void ScreenPicker::mousePressEvent(QWidget *widget, QMouseEvent *me) {
  m_mousePressed = true;
  m_start        = widget->mapToGlobal(me->pos());

  m_geometry = QRect(m_start, QSize(1, 1));
  DVGui::ScreenBoard::instance()->update();
}

//------------------------------------------------------------------

void ScreenPicker::mouseMoveEvent(QWidget *widget, QMouseEvent *me) {
  // On fast movements, the mouse release can fire before the mouse movement
  // assert(m_mouseGrabbed); - can cause a crash
  if (!m_mousePressed || !m_mouseGrabbed) return;

  QPoint pos(widget->mapToGlobal(me->pos()));
  m_geometry = QRect(QRect(m_start, QSize(1, 1)) | QRect(pos, QSize(1, 1)));

  DVGui::ScreenBoard::instance()->update();
}

//------------------------------------------------------------------

void ScreenPicker::mouseReleaseEvent(QWidget *widget, QMouseEvent *me) {
  assert(m_mouseGrabbed);
  if (!m_mousePressed) return;

  m_mouseGrabbed = m_mousePressed = false;

  DVGui::ScreenBoard *screenBoard = DVGui::ScreenBoard::instance();
  QList<DVGui::ScreenBoard::Drawing *> &drawings = screenBoard->drawings();

  drawings.removeAt(drawings.indexOf(this));
  screenBoard->releaseMouse();
  screenBoard->update();

  QPoint pos(widget->mapToGlobal(me->pos()));
  m_geometry = QRect(QRect(m_start, QSize(1, 1)) | QRect(pos, QSize(1, 1)));

  // TimerEvents execution is delayed until all other events have been
  // processed.
  // In particular, we want to pick after the screen refreshes
  QTimer::singleShot(0, this, SLOT(pick()));
}

//------------------------------------------------------------------

void ScreenPicker::pick() {
  // Someway, on MACOSX 10.7 (Lion) there may be screen refresh events pending
  // at this point.
  // Process them before picking.
  QCoreApplication::processEvents();

  QColor color(pickScreenRGB(m_geometry));
  RGBPicker::setCurrentColorWithUndo(
      TPixel32(color.red(), color.green(), color.blue()));

  m_geometry = QRect();
}

//------------------------------------------------------------------

void ScreenPicker::paintEvent(QWidget *widget, QPaintEvent *pe) {
  if (!m_mousePressed) return;

  QPainter painter(widget);

  QRect relativeGeom(widget->mapFromGlobal(m_geometry.topLeft()),
                     widget->mapFromGlobal(m_geometry.bottomRight()));

  painter.setPen(QColor(0, 0, 255, 128));
  painter.setBrush(QColor(0, 0, 255, 64));
  painter.drawRect(relativeGeom);
}

//------------------------------------------------------------------

void ScreenPicker::startGrab() {
  if (!m_mouseGrabbed) {
    m_mouseGrabbed = true;

    DVGui::ScreenBoard *screenBoard = DVGui::ScreenBoard::instance();

    screenBoard->drawings().push_back(this);
    screenBoard->grabMouse(getToolCursor(ToolCursor::PickerCursor));
    screenBoard->update();
  }
}

//***********************************************************************************
//    Pick Screen Command instantiation
//***********************************************************************************

class PickScreenCommandHandler final : public MenuItemHandler {
public:
  PickScreenCommandHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}
  void execute() override {
    static ScreenPicker *picker = new ScreenPicker;
    picker->startGrab();
  }
} pickScreenCHInstance("A_ToolOption_PickScreen");
