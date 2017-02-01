#include "layerheaderpanel.h"

#include <QPainter>

#include "xsheetviewer.h"
#include "xshcolumnviewer.h"

using XsheetGUI::ColumnArea;

#if QT_VERSION >= 0x050500
LayerHeaderPanel::LayerHeaderPanel(XsheetViewer *viewer, QWidget *parent,
                                   Qt::WindowFlags flags)
#else
LayerHeaderPanel::LayerHeaderPanel(XsheetViewer *viewer, QWidget *parent,
                                   Qt::WFlags flags)
#endif
    : QWidget(parent, flags), m_viewer(viewer) {
  const Orientation *o = Orientations::leftToRight();
  QRect rect           = o->rect(PredefinedRect::LAYER_HEADER_PANEL);

  setObjectName("layerHeaderPanel");

  setFixedSize(rect.size());
}

LayerHeaderPanel::~LayerHeaderPanel() {}

namespace {

QColor mix(const QColor &a, const QColor &b, double w) {
  return QColor(a.red() * w + b.red() * (1 - w),
                a.green() * w + b.green() * (1 - w),
                a.blue() * w + b.blue() * (1 - w));
}

QColor withAlpha(const QColor &color, double alpha) {
  QColor result(color);
  result.setAlpha(alpha * 255);
  return result;
}

QRect shorter(const QRect original) { return original.adjusted(0, 2, 0, -2); }

QLine leftSide(const QRect &r) { return QLine(r.topLeft(), r.bottomLeft()); }

QLine rightSide(const QRect &r) { return QLine(r.topRight(), r.bottomRight()); }
}

void LayerHeaderPanel::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  const Orientation *o = Orientations::leftToRight();

  QColor background      = m_viewer->getBGColor();
  QColor slightlyLighter = {mix(background, Qt::white, 0.95)};
  p.fillRect(QRect(QPoint(0, 0), size()), slightlyLighter);

  drawIcon(p, PredefinedRect::EYE, XsheetGUI::PreviewVisibleColor,
           ColumnArea::Pixmaps::eye());
  drawIcon(p, PredefinedRect::PREVIEW_LAYER, boost::none,
           ColumnArea::Pixmaps::cameraStand());
  drawIcon(p, PredefinedRect::LOCK, QColor(255, 255, 255, 128),
           ColumnArea::Pixmaps::lock());
  drawIcon(p, PredefinedRect::SOUND_ICON, boost::none,
           ColumnArea::Pixmaps::sound());

  QRect numberRect = o->rect(PredefinedRect::LAYER_NUMBER);
  p.drawText(numberRect, Qt::AlignCenter | Qt::TextSingleLine, "#");

  QRect nameRect = o->rect(PredefinedRect::LAYER_NAME).adjusted(2, 0, -2, 0);
  p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
             QObject::tr("Layer name"));

  drawLines(p, numberRect, nameRect);
}

void LayerHeaderPanel::drawIcon(QPainter &p, PredefinedRect rect,
                                optional<QColor> fill,
                                const QPixmap &pixmap) const {
  QRect iconRect = Orientations::leftToRight()->rect(rect);
  if (fill) p.fillRect(iconRect, *fill);
  p.drawPixmap(iconRect, pixmap);
}

void LayerHeaderPanel::drawLines(QPainter &p, const QRect &numberRect,
                                 const QRect &nameRect) const {
  p.setPen(withAlpha(m_viewer->getTextColor(), 0.5));

  QLine line = {leftSide(shorter(numberRect)).translated(2, 0)};
  p.drawLine(line);

  line = rightSide(shorter(numberRect)).translated(-2, 0);
  p.drawLine(line);

  line = rightSide(shorter(nameRect));
  p.drawLine(line);
}

void LayerHeaderPanel::showOrHide(const Orientation *o) {
  QRect rect = o->rect(PredefinedRect::LAYER_HEADER_PANEL);
  if (rect.isEmpty())
    hide();
  else
    show();
}
