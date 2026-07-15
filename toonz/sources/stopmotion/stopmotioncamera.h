#pragma once

#ifndef STOPMOTIONCAMERA_H
#define STOPMOTIONCAMERA_H

#include <QList>
#include <QSize>
#include <QString>
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QCameraDevice>
#include <QCameraFormat>
#include <QMediaDevices>
#else
#include <QCameraInfo>
#endif

namespace StopMotionCamera {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using Info = QCameraDevice;
#else
using Info = QCameraInfo;
#endif

inline QList<Info> availableCameras() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QMediaDevices::videoInputs();
#else
  return QCameraInfo::availableCameras();
#endif
}

inline QString description(const Info& camera) { return camera.description(); }

inline QString deviceName(const Info& camera) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return QString::fromUtf8(camera.id());
#else
  return camera.deviceName();
#endif
}

inline QList<QSize> resolutions(const Info& camera) {
  QList<QSize> sizes;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  for (const QCameraFormat& format : camera.videoFormats()) {
    const QSize size = format.resolution();
    if (!sizes.contains(size)) sizes.append(size);
  }
#else
  Q_UNUSED(camera)
#endif
  return sizes;
}

}  // namespace StopMotionCamera

#endif  // STOPMOTIONCAMERA_H
