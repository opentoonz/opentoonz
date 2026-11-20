#pragma once

#ifndef GLWIDGET_FOR_HIGHDPI_H
#define GLWIDGET_FOR_HIGHDPI_H

#include <QOpenGLWidget>
#include <QApplication>
#include <QDesktopWidget>
#include <QOpenGLFunctions>
#include "toonzqt/gutil.h"

// An OpenGL widget whose GL coordinates are in device pixels.
//
// width(), height() and rect() hide the QWidget versions and return the size
// of the GL framebuffer in device pixels. The device pixel ratio can be
// fractional (e.g. 1.25 or 1.5 at 125% / 150% display scale).
class GLWidgetForHighDpi : public QOpenGLWidget, protected QOpenGLFunctions {
public:
  GLWidgetForHighDpi(QWidget *parent   = nullptr,
                     Qt::WindowFlags f = Qt::WindowFlags())
      : QOpenGLWidget(parent, f) {}

  // Same rounding as Qt uses to size the QOpenGLWidget framebuffer
  // (QSize * qreal rounds each side), so this matches it exactly.
  QSize deviceSize() const {
    return QOpenGLWidget::size() * QOpenGLWidget::devicePixelRatioF();
  }

  //  modify sizes for high DPI monitors
  int width() const { return deviceSize().width(); }
  int height() const { return deviceSize().height(); }
  QRect rect() const { return QRect(0, 0, width(), height()); }
  qreal getDevPixRatio() const { return QOpenGLWidget::devicePixelRatioF(); }

protected:
  // To be called by resizeGL() overrides, after they have set up the
  // viewport and projection for deviceSize().
  void markDeviceSizeApplied() { m_appliedDeviceSize = deviceSize(); }

  // To be called at the start of paintGL() overrides that set up their
  // projection in resizeGL().
  // - Re-runs resizeGL() when the device size changed without a resize event,
  //   e.g. when the window moved to a screen with a different scale.
  // - Resets the viewport, which Qt sets before paintGL() with truncated
  //   (not rounded) values at fractional ratios.
  void applyDeviceViewport() {
    if (m_appliedDeviceSize != deviceSize())
      resizeGL(QOpenGLWidget::width(), QOpenGLWidget::height());
    glViewport(0, 0, width(), height());
  }

private:
  QSize m_appliedDeviceSize;
};

#endif
