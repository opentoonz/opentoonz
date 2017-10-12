#pragma once

#ifndef GLWIDGET_FOR_HIGHDPI_H
#define GLWIDGET_FOR_HIGHDPI_H

#include <QOpenGLWidget>
#include <QApplication>
#include <QDesktopWidget>
#include "toonzqt/gutil.h"

// use obsolete QGLWidget instead of QOpenGLWidget for now...
// TODO: replace with the "modern" OpenGL source and transfer to QOpenGLWidget
class GLWidgetForHighDpi : public QOpenGLWidget {
public:
  GLWidgetForHighDpi(QWidget *parent              = Q_NULLPTR,
                     const QOpenGLWidget *shareWidget = Q_NULLPTR,
                     Qt::WindowFlags f            = Qt::WindowFlags())
      : QOpenGLWidget(parent, f) {}

  //  modify sizes for high DPI monitors
  int width() const { return QOpenGLWidget::width() * getDevPixRatio(); }
  int height() const { return QOpenGLWidget::height() * getDevPixRatio(); }
  QRect rect() const { return QRect(0, 0, width(), height()); }
};

#endif