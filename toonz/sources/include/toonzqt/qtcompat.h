#pragma once

#ifndef TOONZQT_QTCOMPAT_H
#define TOONZQT_QTCOMPAT_H

#include <QtGlobal>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QObject>
#include <QStringList>
#include <QTouchEvent>
#include <QWheelEvent>

#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#include <QEventPoint>
#include <QInputDevice>
#include <QPointingDevice>
#include <QTabletEvent>
#else
#include <QEvent>
#include <QTabletEvent>
#include <QTouchDevice>
#endif

namespace QtCompat {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using EnterEvent      = QEnterEvent;
using TouchPoint      = QEventPoint;
using TouchDeviceType = QInputDevice::DeviceType;

static constexpr TouchDeviceType TouchScreen =
    QInputDevice::DeviceType::TouchScreen;
static constexpr TouchDeviceType TouchPad = QInputDevice::DeviceType::TouchPad;
#else
using EnterEvent      = QEvent;
using TouchPoint      = QTouchEvent::TouchPoint;
using TouchDeviceType = QTouchDevice::DeviceType;

static constexpr TouchDeviceType TouchScreen = QTouchDevice::TouchScreen;
static constexpr TouchDeviceType TouchPad    = QTouchDevice::TouchPad;
#endif

inline const QList<TouchPoint>& touchPoints(const QTouchEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->points();
#else
  return event->touchPoints();
#endif
}

inline QPointF touchPointPosition(const TouchPoint& point) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return point.position();
#else
  return point.pos();
#endif
}

inline QPointF touchPointLastPosition(const TouchPoint& point) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return point.lastPosition();
#else
  return point.lastPos();
#endif
}

inline bool isStylusPointer(const QTabletEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->pointerType() != QPointingDevice::PointerType::Unknown;
#else
  return event->pointerType() != QTabletEvent::UnknownPointer;
#endif
}

inline bool isEraserPointer(const QTabletEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->pointerType() == QPointingDevice::PointerType::Eraser;
#else
  return event->pointerType() == QTabletEvent::Eraser;
#endif
}

inline QKeySequence keySequence(const QKeyEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QKeySequence(event->keyCombination());
#else
  return QKeySequence(event->key() + event->modifiers());
#endif
}

inline QImage convertToGLFormat(const QImage& image) {
  QImage converted = image.convertToFormat(QImage::Format_RGBA8888);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return converted.flipped(Qt::Vertical);
#else
  return converted.mirrored(false, true);
#endif
}

inline QStringList fontDatabaseStyles(const QString& family) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QFontDatabase::styles(family);
#else
  QFontDatabase fontDatabase;
  return fontDatabase.styles(family);
#endif
}

inline QFont fontDatabaseFont(const QString& family, const QString& style,
                              int pointSize) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QFontDatabase::font(family, style, pointSize);
#else
  QFontDatabase fontDatabase;
  return fontDatabase.font(family, style, pointSize);
#endif
}

inline int fontMetricsHorizontalAdvance(const QFontMetrics& metrics,
                                        const QString& text) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
  return metrics.horizontalAdvance(text);
#else
  return metrics.width(text);
#endif
}

inline QPoint mouseEventPosition(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position().toPoint();
#else
  return event->pos();
#endif
}

inline QPointF mouseEventPositionF(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position();
#else
  return QPointF(event->pos());
#endif
}

inline QPointF mouseEventWindowPositionF(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->scenePosition();
#else
  return event->windowPos();
#endif
}

inline QPoint mouseEventGlobalPosition(const QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint();
#else
  return event->globalPos();
#endif
}

inline QPoint contextMenuEventPosition(const QContextMenuEvent* event) {
  return event->pos();
}

inline QPoint contextMenuEventGlobalPosition(const QContextMenuEvent* event) {
  return event->globalPos();
}

inline QPoint helpEventPosition(const QHelpEvent* event) {
  return event->pos();
}

inline QPoint helpEventGlobalPosition(const QHelpEvent* event) {
  return event->globalPos();
}

inline QPoint wheelEventPosition(const QWheelEvent* event) {
  return event->position().toPoint();
}

inline QPointF wheelEventPositionF(const QWheelEvent* event) {
  return event->position();
}

inline QPoint wheelEventAngleDelta(const QWheelEvent* event) {
  return event->angleDelta();
}

inline int wheelEventAngleDeltaY(const QWheelEvent* event) {
  return event->angleDelta().y();
}

inline QPoint wheelEventGlobalPosition(const QWheelEvent* event) {
  return event->globalPosition().toPoint();
}

inline QPoint tabletEventPosition(const QTabletEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position().toPoint();
#else
  return event->pos();
#endif
}

inline QPointF tabletEventPositionF(const QTabletEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position();
#else
  return event->posF();
#endif
}

inline QPoint tabletEventGlobalPosition(const QTabletEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint();
#else
  return event->globalPos();
#endif
}

inline QPoint hoverEventPosition(const QHoverEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position().toPoint();
#else
  return event->pos();
#endif
}

inline QPointF graphicsSceneMouseEventPositionF(
    const QGraphicsSceneMouseEvent* event) {
  return event->pos();
}

inline QPointF graphicsSceneContextMenuEventPositionF(
    const QGraphicsSceneContextMenuEvent* event) {
  return event->pos();
}

inline QPoint graphicsSceneContextMenuEventGlobalPosition(
    const QGraphicsSceneContextMenuEvent* event) {
  return event->screenPos();
}

inline QMouseEvent makeMouseEvent(QEvent::Type type, const QPointF& localPos,
                                  const QPointF& globalPos,
                                  Qt::MouseButton button,
                                  Qt::MouseButtons buttons,
                                  Qt::KeyboardModifiers modifiers) {
  return QMouseEvent(type, localPos, globalPos, button, buttons, modifiers);
}

inline QPoint dropEventPosition(const QDropEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position().toPoint();
#else
  return event->pos();
#endif
}

inline Qt::KeyboardModifiers dropEventModifiers(const QDropEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->modifiers();
#else
  return event->keyboardModifiers();
#endif
}

template <typename Receiver, typename Func>
inline QMetaObject::Connection connectCheckStateChanged(QCheckBox* checkBox,
                                                        Receiver* receiver,
                                                        Func&& slot) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  return QObject::connect(checkBox, &QCheckBox::checkStateChanged, receiver,
                          std::forward<Func>(slot));
#else
  return QObject::connect(checkBox, &QCheckBox::stateChanged, receiver,
                          [slot = std::forward<Func>(slot)](int state) mutable {
                            slot(static_cast<Qt::CheckState>(state));
                          });
#endif
}

template <typename Receiver, typename Func>
inline QMetaObject::Connection connectComboBoxActivatedIndex(
    QComboBox* comboBox, Receiver* receiver, Func&& slot) {
  return QObject::connect(
      comboBox, &QComboBox::textActivated, receiver,
      [comboBox, slot = std::forward<Func>(slot)](const QString&) mutable {
        slot(comboBox->currentIndex());
      });
}

}  // namespace QtCompat

#endif
