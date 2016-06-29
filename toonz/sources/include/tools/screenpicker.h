#pragma once

#ifndef SCREEN_PICKER_H
#define SCREEN_PICKER_H

#include "toonzqt/screenboard.h"

#include <QWidget>

//----------------------------------------------------------------------

class ScreenPicker final : public QObject, public DVGui::ScreenBoard::Drawing {
  Q_OBJECT

  QPoint m_start;
  QRect m_geometry;

  bool m_mousePressed, m_mouseGrabbed;

public:
  ScreenPicker(QWidget *parent = 0);

protected:
  void event(QWidget *widget, QEvent *e) override;

  void mousePressEvent(QWidget *widget, QMouseEvent *me);
  void mouseMoveEvent(QWidget *widget, QMouseEvent *me);
  void mouseReleaseEvent(QWidget *widget, QMouseEvent *me);

  void paintEvent(QWidget *widget, QPaintEvent *pe) override;

  bool acceptScreenEvents(const QRect &screenRect) const override;

public slots:

  void startGrab();
  void pick();
};

#endif  // SCREEN_PICKER_H
