#pragma once

#ifndef GLWIDGET_FOR_HIGHDPI_H
#define GLWIDGET_FOR_HIGHDPI_H

//#include <QGLWidget>
#include <QOpenGlWidget>
#include <QApplication>
#include <QDesktopWidget>

// use obsolete QGLWidget instead of QOpenGLWidget for now...
// TODO: replace with the "modern" OpenGL source and transfer to QOpenGLWidget
class GLWidgetForHighDpi : public QOpenGLWidget {
public:
  GLWidgetForHighDpi(QWidget *parent              = Q_NULLPTR,
                     const QOpenGLWidget *shareWidget = Q_NULLPTR,
                     Qt::WindowFlags f            = Qt::WindowFlags())
      : QOpenGLWidget(parent, f) {}

  // returns device-pixel ratio. It is 1 for normal monitors and 2 (or higher
  // ratio) for high DPI monitors. Setting "Display > Set custom text size(DPI)"
  // for Windows corresponds to this ratio.
  int getDevPixRatio() const {
    static int devPixRatio = QApplication::desktop()->devicePixelRatio();
    return devPixRatio;
  }
  //  modify sizes for high DPI monitors
  int width() const { return QOpenGLWidget::width() * getDevPixRatio(); }
  int height() const { return QOpenGLWidget::height() * getDevPixRatio(); }
  QRect rect() const { return QRect(0, 0, width(), height()); }
};

#endif