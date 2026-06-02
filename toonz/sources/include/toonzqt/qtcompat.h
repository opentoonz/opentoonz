#pragma once

#ifndef TOONZQT_QTCOMPAT_H
#define TOONZQT_QTCOMPAT_H

#include <QtGlobal>
#include <QCheckBox>
#include <QDropEvent>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QObject>

#include <utility>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
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
using TouchDeviceType = QInputDevice::DeviceType;

static constexpr TouchDeviceType TouchScreen =
    QInputDevice::DeviceType::TouchScreen;
static constexpr TouchDeviceType TouchPad = QInputDevice::DeviceType::TouchPad;
#else
using EnterEvent      = QEvent;
using TouchDeviceType = QTouchDevice::DeviceType;

static constexpr TouchDeviceType TouchScreen = QTouchDevice::TouchScreen;
static constexpr TouchDeviceType TouchPad    = QTouchDevice::TouchPad;
#endif

inline bool isStylusPointer(const QTabletEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->pointerType() != QPointingDevice::PointerType::Unknown;
#else
  return event->pointerType() != QTabletEvent::UnknownPointer;
#endif
}

inline bool isEraserPointer(const QTabletEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->pointerType() == QPointingDevice::PointerType::Eraser;
#else
  return event->pointerType() == QTabletEvent::Eraser;
#endif
}

inline QKeySequence keySequence(const QKeyEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QKeySequence(event->keyCombination());
#else
  return QKeySequence(event->key() + event->modifiers());
#endif
}

inline QImage convertToGLFormat(const QImage &image) {
  QImage converted = image.convertToFormat(QImage::Format_RGBA8888);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return converted.flipped(Qt::Vertical);
#else
  return converted.mirrored(false, true);
#endif
}

inline QPoint mouseEventPosition(const QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position().toPoint();
#else
  return event->pos();
#endif
}

inline QPoint mouseEventGlobalPosition(const QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->globalPosition().toPoint();
#else
  return event->globalPos();
#endif
}

inline QPoint dropEventPosition(const QDropEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return event->position().toPoint();
#else
  return event->pos();
#endif
}

template <typename Receiver, typename Func>
inline QMetaObject::Connection connectCheckStateChanged(
    QCheckBox *checkBox, Receiver *receiver, Func &&slot) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  return QObject::connect(checkBox, &QCheckBox::checkStateChanged, receiver,
                          std::forward<Func>(slot));
#else
  return QObject::connect(
      checkBox, &QCheckBox::stateChanged, receiver,
      [slot = std::forward<Func>(slot)](int state) mutable {
        slot(static_cast<Qt::CheckState>(state));
      });
#endif
}

}  // namespace QtCompat

#endif
