#include "layerheaderpanel.h"

#include <QPainter>

#include "xsheetviewer.h"

#if QT_VERSION >= 0x050500
LayerHeaderPanel::LayerHeaderPanel (XsheetViewer *viewer, QWidget *parent, Qt::WindowFlags flags)
#else
LayerHeaderPanel::LayerHeaderPanel (XsheetViewer *viewer, QWidget *parent, Qt::WFlags flags)
#endif
  : QWidget (parent, flags), m_viewer (viewer)
{
  const Orientation *o = orientations.leftToRight ();
  QRect rect = o->rect (PredefinedRect::LAYER_HEADER_PANEL);
  
  setObjectName ("layerHeaderPanel");

  setFixedSize (rect.size ());
}

LayerHeaderPanel::~LayerHeaderPanel () {
}

QColor mix (const QColor &a, const QColor &b, double w) {
  return QColor (
    a.red () * w + b.red () * (1 - w),
    a.green () * w + b.green () * (1 - w),
    a.blue () * w + b.blue () * (1 - w));
}

void LayerHeaderPanel::paintEvent (QPaintEvent *event) {
  QPainter p (this);

  const Orientation *o = orientations.leftToRight ();
  
  QColor background = m_viewer->getBGColor ();
  QColor slightlyLighter = { mix (background, Qt::white, 0.95) };
  p.fillRect (QRect (QPoint (0, 0), size ()), slightlyLighter);

  // code duplication with: xshcolumnviewer.cpp DrawHeader methods

  static QPixmap eyePixmap = QPixmap (":Resources/x_prev_eye.png");
  drawIcon (p, PredefinedRect::EYE, XsheetGUI::PreviewVisibleColor, eyePixmap);

  static QPixmap camstandPixmap = QPixmap (":Resources/x_table_view.png");
  drawIcon (p, PredefinedRect::PREVIEW_LAYER, boost::none, camstandPixmap);

  static QPixmap lockPixmap = QPixmap (":Resources/x_lock.png");
  drawIcon (p, PredefinedRect::LOCK, QColor (255, 255, 255, 128), lockPixmap);

  static QPixmap soundPixmap = QPixmap (":Resources/sound_header_off.png");
  drawIcon (p, PredefinedRect::SOUND_ICON, boost::none, soundPixmap);

  QRect numberRect = o->rect (PredefinedRect::LAYER_NUMBER);
  p.drawText (numberRect, Qt::AlignCenter | Qt::TextSingleLine, "#");

  QRect nameRect = o->rect (PredefinedRect::LAYER_NAME).adjusted (2, 0, -2, 0);
  p.drawText (nameRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
    QObject::tr ("Layer name"));
}

void LayerHeaderPanel::drawIcon (QPainter &p, PredefinedRect rect, optional<QColor> fill, const QPixmap &pixmap) const {
  QRect iconRect = orientations.leftToRight ()->rect (rect);
  if (fill)
    p.fillRect (iconRect, *fill);
  p.drawPixmap (iconRect, pixmap);
}

void LayerHeaderPanel::showOrHide (const Orientation *o) {
  QRect rect = o->rect (PredefinedRect::LAYER_HEADER_PANEL);
  if (rect.isEmpty ())
    hide ();
  else
    show ();
}
