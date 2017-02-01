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

private:
  void drawIcon(QPainter &p, PredefinedRect rect, optional<QColor> fill,
                const QPixmap &pixmap) const;
  void drawLines(QPainter &p, const QRect &numberRect,
                 const QRect &nameRect) const;
};

#endif
