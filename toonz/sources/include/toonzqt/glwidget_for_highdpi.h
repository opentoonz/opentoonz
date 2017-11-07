#pragma once

#ifndef GLWIDGET_FOR_HIGHDPI_H
#define GLWIDGET_FOR_HIGHDPI_H

#include <QOpenGLWidget>
#include <QApplication>
#include <QDesktopWidget>
#include <QOpenGLFunctions>
#include "toonzqt/gutil.h"

class GLWidgetForHighDpi : public QOpenGLWidget, protected QOpenGLFunctions {
public:
  GLWidgetForHighDpi(QWidget *parent   = Q_NULLPTR,
                     Qt::WindowFlags f = Qt::WindowFlags())
      : QOpenGLWidget(parent, f) {}

  //  modify sizes for high DPI monitors
  int width() const { return QOpenGLWidget::width() * getDevPixRatio(); }
  int height() const { return QOpenGLWidget::height() * getDevPixRatio(); }
  QRect rect() const { return QRect(0, 0, width(), height()); }
};

#endif