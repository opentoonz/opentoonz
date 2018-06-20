#pragma once

#ifndef LAYER_HEADER_PANEL_INCLUDED
#define LAYER_HEADER_PANEL_INCLUDED

#include <QWidget>
#include <boost/optional.hpp>

#include "orientation.h"

using boost::optional;

class XsheetViewer;

// Panel showing column headers for layers in timeline mode
class LayerHeaderPanel final : public QWidget {
  Q_OBJECT

  enum { ToggleAllTransparency = 1, ToggleAllPreviewVisible, ToggleAllLock };

  enum { NoButton, PreviewButton, CamstandButton, LockButton };

  int m_doOnRelease;
  QString m_tooltip;
  QPoint m_pos;
  int m_buttonHighlighted;

private:
  XsheetViewer *m_viewer;

public:
#if QT_VERSION >= 0x050500
  LayerHeaderPanel(XsheetViewer *viewer, QWidget *parent = 0,
                   Qt::WindowFlags flags = 0);
#else
  LayerHeaderPanel(XsheetViewer *viewer, QWidget *parent = 0,
                   Qt::WFlags flags = 0);
#endif
  ~LayerHeaderPanel();

  void showOrHide(const Orientation *o);

protected:
  void paintEvent(QPaintEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void enterEvent(QEvent *);
  void leaveEvent(QEvent *);
  bool event(QEvent *event) override;

private:
  void drawIcon(QPainter &p, PredefinedRect rect, optional<QColor> fill,
                const QImage &image) const;
  void drawLines(QPainter &p, const QRect &numberRect,
                 const QRect &nameRect) const;
};

#endif
