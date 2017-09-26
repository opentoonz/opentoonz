#include "layerheaderpanel.h"

#include <QPainter>
#include <QToolTip>

#include "xsheetviewer.h"
#include "xshcolumnviewer.h"

#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"

#include "toonz/preferences.h"

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

  setMouseTracking(true);
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
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const Orientation *o = Orientations::leftToRight();

  QColor background      = m_viewer->getBGColor();
  QColor slightlyLighter = {mix(background, Qt::white, 0.95)};
  QRect rect             = QRect(QPoint(0, 0), size());
  p.fillRect(rect.adjusted(0, 0, -3, 0), slightlyLighter);

  drawIcon(p, PredefinedRect::EYE, boost::none,
           m_viewer->getLayerHeaderPreviewImage());
  drawIcon(p, PredefinedRect::PREVIEW_LAYER, boost::none,
           m_viewer->getLayerHeaderCamstandImage());
  drawIcon(p, PredefinedRect::LOCK, boost::none,
           m_viewer->getLayerHeaderLockImage());

  QRect numberRect = o->rect(PredefinedRect::LAYER_NUMBER);

  int leftadj = 2;
  if (Preferences::instance()->isShowColumnNumbersEnabled()) {
    p.drawText(numberRect, Qt::AlignCenter | Qt::TextSingleLine, "#");

    leftadj += 20;
  }

  QRect nameRect =
      o->rect(PredefinedRect::LAYER_NAME).adjusted(leftadj, 0, -1, 0);
  p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
             QObject::tr("Layer name"));

  drawLines(p, numberRect, nameRect);
}

void LayerHeaderPanel::drawIcon(QPainter &p, PredefinedRect rect,
                                optional<QColor> fill,
                                const QImage &image) const {
  QRect iconRect =
      Orientations::leftToRight()->rect(rect).adjusted(-2, 0, -2, 0);

  if (fill) p.fillRect(iconRect, *fill);
  p.drawImage(iconRect, image);
}

void LayerHeaderPanel::drawLines(QPainter &p, const QRect &numberRect,
                                 const QRect &nameRect) const {
  p.setPen(withAlpha(m_viewer->getTextColor(), 0.5));

  QLine line = {leftSide(shorter(numberRect)).translated(-2, 0)};
  p.drawLine(line);

  if (Preferences::instance()->isShowColumnNumbersEnabled()) {
    line = rightSide(shorter(numberRect)).translated(-2, 0);
    p.drawLine(line);
  }

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

//-----------------------------------------------------------------------------

void LayerHeaderPanel::mousePressEvent(QMouseEvent *event) {
  const Orientation *o = Orientations::leftToRight();

  m_doOnRelease = 0;

  if (event->button() == Qt::LeftButton) {
    // get mouse position
    QPoint pos = event->pos();

    // preview button
    if (o->rect(PredefinedRect::EYE_AREA).contains(pos)) {
      m_doOnRelease = ToggleAllPreviewVisible;
    }
    // camstand button
    else if (o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(pos)) {
      m_doOnRelease = ToggleAllTransparency;
    }
    // lock button
    else if (o->rect(PredefinedRect::LOCK_AREA).contains(pos)) {
      m_doOnRelease = ToggleAllLock;
    }
  }

  update();
}

void LayerHeaderPanel::mouseMoveEvent(QMouseEvent *event) {
  const Orientation *o = Orientations::leftToRight();

  QPoint pos = event->pos();

  // preview button
  if (o->rect(PredefinedRect::EYE_AREA).contains(pos)) {
    m_tooltip = tr("Preview Visbility Toggle All");
  }
  // camstand button
  else if (o->rect(PredefinedRect::PREVIEW_LAYER_AREA).contains(pos)) {
    m_tooltip = tr("Camera Stand Visibility Toggle All");
  }
  // lock button
  else if (o->rect(PredefinedRect::LOCK).contains(pos)) {
    m_tooltip = tr("Lock Toggle All");
  } else {
    m_tooltip = tr("");
  }

  m_pos = pos;

  update();
}

//-----------------------------------------------------------------------------

bool LayerHeaderPanel::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    if (!m_tooltip.isEmpty())
      QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
    else
      QToolTip::hideText();
  }
  return QWidget::event(event);
}

//-----------------------------------------------------------------------------

void LayerHeaderPanel::mouseReleaseEvent(QMouseEvent *event) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = m_viewer->getXsheet();
  int col, totcols = xsh->getColumnCount();
  bool sound_changed = false;

  if (m_doOnRelease != 0 && totcols > 0) {
    for (col = 0; col < totcols; col++) {
      if (!xsh->isColumnEmpty(col)) {
        TXshColumn *column = xsh->getColumn(col);

        if (m_doOnRelease == ToggleAllPreviewVisible) {
          column->setPreviewVisible(!column->isPreviewVisible());
        } else if (m_doOnRelease == ToggleAllTransparency) {
          column->setCamstandVisible(!column->isCamstandVisible());
          if (column->getSoundColumn()) sound_changed = true;
        } else if (m_doOnRelease == ToggleAllLock) {
          column->lock(!column->isLocked());
        }
      }
    }

    if (sound_changed) {
      app->getCurrentXsheet()->notifyXsheetSoundChanged();
    }

    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
  m_viewer->updateColumnArea();
  update();
  m_doOnRelease = 0;
}
