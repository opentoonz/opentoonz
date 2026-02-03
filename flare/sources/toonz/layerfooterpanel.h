#pragma once

#ifndef LAYER_FOOTER_PANEL_INCLUDED
#define LAYER_FOOTER_PANEL_INCLUDED

#include <QWidget>
#include <QSlider>
#include <QToolButton>
#include <QKeyEvent>
#include "orientation.h"

class XsheetViewer;

class LayerFooterPanel final : public QWidget {
  Q_OBJECT

private:
  XsheetViewer* m_viewer;
  QSlider* m_frameZoomSlider;
  QToolButton* m_zoomInButton;
  QToolButton* m_zoomOutButton;
  bool isCtrlPressed;

public:
  LayerFooterPanel(XsheetViewer* viewer, QWidget* parent = nullptr,
                   Qt::WindowFlags flags = Qt::WindowFlags());
  ~LayerFooterPanel();

  void showOrHide(const Orientation* o);
  void setZoomSliderValue(int val);
  void onControlPressed(bool pressed);
  bool isControlPressed() const;

protected:
  void paintEvent(QPaintEvent* event) override;
  void contextMenuEvent(QContextMenuEvent*) override;
  void keyPressEvent(QKeyEvent* event) override { event->ignore(); }
  void wheelEvent(QWheelEvent* event) override { event->ignore(); }

private slots:
  void onFrameZoomSliderValueChanged(int val);
  void onFramesPerPageSelected();
  void onZoomInClicked();
  void onZoomOutClicked();
};

#endif
