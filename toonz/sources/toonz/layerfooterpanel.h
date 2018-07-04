#pragma once

#ifndef LAYER_FOOTER_PANEL_INCLUDED
#define LAYER_FOOTER_PANEL_INCLUDED

#include <QWidget>
#include <QSlider>
#include <QKeyEvent>
#include <boost/optional.hpp>

#include "orientation.h"

using boost::optional;

class XsheetViewer;

// Panel showing column footers for layers in timeline mode
class LayerFooterPanel final : public QWidget {
	Q_OBJECT

	QString m_tooltip;
	QPoint m_pos;

	QSlider *m_frameZoomSlider;

	bool isCtrlPressed = false;
	bool m_zoomInHighlighted = false;
	bool m_zoomOutHighlighted = false;

private:
	XsheetViewer *m_viewer;

public:
#if QT_VERSION >= 0x050500
	LayerFooterPanel(XsheetViewer *viewer, QWidget *parent = 0,
		Qt::WindowFlags flags = 0);
#else
	LayerFooterPanel(XsheetViewer *viewer, QWidget *parent = 0,
		Qt::WFlags flags = 0);
#endif
	~LayerFooterPanel();

	void showOrHide(const Orientation *o);

	void setZoomSliderValue(int val);

	void onControlPressed(bool pressed);
	const bool isControlPressed();

protected:
	void paintEvent(QPaintEvent *event) override;

	void enterEvent(QEvent *) override;
	void leaveEvent(QEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	bool event(QEvent *event) override;

	void keyPressEvent(QKeyEvent *event) override { event->ignore(); }
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }

public slots:
   void onFrameZoomSliderValueChanged(int val);
};
#endif
#pragma once
