#include "layerfooterpanel.h"

#include <QPainter>
#include <QToolTip>
#include <QScrollBar>

#include "xsheetviewer.h"
#include "xshcolumnviewer.h"

#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tobjecthandle.h"

#include "toonz/preferences.h"

#include "toonzqt/gutil.h"

using XsheetGUI::ColumnArea;

#if QT_VERSION >= 0x050500
LayerFooterPanel::LayerFooterPanel(XsheetViewer *viewer, QWidget *parent,
	Qt::WindowFlags flags)
#else
LayerFooterPanel::LayerFooterPanel(XsheetViewer *viewer, QWidget *parent,
	Qt::WFlags flags)
#endif
	: QWidget(parent, flags), m_viewer(viewer) {
	const Orientation *o = Orientations::leftToRight();
	QRect rect = o->rect(PredefinedRect::LAYER_FOOTER_PANEL);

	setObjectName("layerFooterPanel");

	setFixedSize(rect.size());
	
	setMouseTracking(true);

	m_frameZoomSlider = new QSlider(Qt::Horizontal, this);
	m_frameZoomSlider->setMinimum(20);
	m_frameZoomSlider->setMaximum(100);
	m_frameZoomSlider->setValue(m_viewer->getFrameZoomFactor());
	m_frameZoomSlider->setToolTip(tr("Zoom in/out of timeline"));

	connect(m_frameZoomSlider, SIGNAL(valueChanged(int)), this,
	SLOT(onFrameZoomSliderValueChanged(int)));
}

LayerFooterPanel::~LayerFooterPanel() {}

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

void LayerFooterPanel::paintEvent(QPaintEvent *event) {
	QPainter p(this);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);

	const Orientation *o = Orientations::leftToRight();

	QRect zoomSliderRect = o->rect(PredefinedRect::ZOOM_SLIDER_AREA);
	p.fillRect(zoomSliderRect, Qt::NoBrush);

	QRect sliderObjRect = o->rect(PredefinedRect::ZOOM_SLIDER);
	m_frameZoomSlider->setGeometry(sliderObjRect);

	static QPixmap zoomIn = svgToPixmap(":Resources/zoom_in.svg");
	static QPixmap zoomInRollover = svgToPixmap(":Resources/zoom_in_rollover.svg");
	const QRect zoomInImgRect = o->rect(PredefinedRect::ZOOM_IN);

	static QPixmap zoomOut = svgToPixmap(":Resources/zoom_out.svg");
	static QPixmap zoomOutRollover = svgToPixmap(":Resources/zoom_out_rollover.svg");
	const QRect zoomOutImgRect = o->rect(PredefinedRect::ZOOM_OUT);

	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	if (m_zoomInHighlighted)
		p.drawPixmap(zoomInImgRect, zoomInRollover);
	else
		p.drawPixmap(zoomInImgRect, zoomIn);

	if (m_zoomOutHighlighted)
		p.drawPixmap(zoomOutImgRect, zoomOutRollover);
	else
		p.drawPixmap(zoomOutImgRect, zoomOut);

	p.setPen(withAlpha(m_viewer->getTextColor(), 0.5));

	QLine line = { leftSide(shorter(zoomOutImgRect)).translated(-2, 0) };
	p.drawLine(line);
}

void LayerFooterPanel::showOrHide(const Orientation *o) {
	QRect rect = o->rect(PredefinedRect::LAYER_FOOTER_PANEL);
	if (rect.isEmpty())
		hide();
	else
		show();
}

//-----------------------------------------------------------------------------
void LayerFooterPanel::enterEvent(QEvent *) {
	m_zoomInHighlighted = false;
	m_zoomOutHighlighted = false;

	update();
}

void LayerFooterPanel::leaveEvent(QEvent *) {
	m_zoomInHighlighted = false;
	m_zoomOutHighlighted = false;

	update();
}

void LayerFooterPanel::mousePressEvent(QMouseEvent *event) {
	const Orientation *o = Orientations::leftToRight();

	if (event->button() == Qt::LeftButton) {
		// get mouse position
		QPoint pos = event->pos();
		if (o->rect(PredefinedRect::ZOOM_IN_AREA).contains(pos)) {
		int newFactor = isCtrlPressed ? m_frameZoomSlider->maximum()
		: m_frameZoomSlider->value() + 10;
		m_frameZoomSlider->setValue(newFactor);
		}
		else if (o->rect(PredefinedRect::ZOOM_OUT_AREA).contains(pos)) {
		int newFactor = isCtrlPressed ? m_frameZoomSlider->minimum()
		: m_frameZoomSlider->value() - 10;
		m_frameZoomSlider->setValue(newFactor);
		}
	}

	update();
}

void LayerFooterPanel::mouseMoveEvent(QMouseEvent *event) {
	const Orientation *o = Orientations::leftToRight();

	QPoint pos = event->pos();

	m_zoomInHighlighted = false;
	m_zoomOutHighlighted = false;
	if (o->rect(PredefinedRect::ZOOM_IN_AREA).contains(pos)) {
		m_zoomInHighlighted = true;
		m_tooltip = tr("Zoom in (Ctrl-click to zoom in all the way)");
	}
	else if (o->rect(PredefinedRect::ZOOM_OUT_AREA)
		.contains(pos)) {
		m_zoomOutHighlighted = true;
		m_tooltip = tr("Zoom out (Ctrl-click to zoom out all the way)");
	}
	else {
		m_tooltip = tr("");
	}

	m_pos = pos;

	update();
}

//-----------------------------------------------------------------------------

bool LayerFooterPanel::event(QEvent *event) {
	if (event->type() == QEvent::ToolTip) {
		if (!m_tooltip.isEmpty())
			QToolTip::showText(mapToGlobal(m_pos), m_tooltip);
		else
			QToolTip::hideText();
	}
	return QWidget::event(event);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::setZoomSliderValue(int val) {
	if (val > m_frameZoomSlider->maximum())
		val = m_frameZoomSlider->maximum();
	else if (val < m_frameZoomSlider->minimum())
		val = m_frameZoomSlider->minimum();

	m_frameZoomSlider->blockSignals(true);
	m_frameZoomSlider->setValue(val);
	m_frameZoomSlider->blockSignals(false);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onFrameZoomSliderValueChanged(int val) {
	m_viewer->zoomOnFrame(m_viewer->getCurrentRow(), val);
}

//-----------------------------------------------------------------------------

void LayerFooterPanel::onControlPressed(bool pressed) {
	isCtrlPressed = pressed;
	update();
}

//-----------------------------------------------------------------------------

const bool LayerFooterPanel::isControlPressed() { return isCtrlPressed; }
