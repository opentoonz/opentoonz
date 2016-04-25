#pragma once

#ifndef SWATCHVIEWER_H
#define SWATCHVIEWER_H

#ifdef _WIN32
#pragma warning(disable : 4251)
#endif

#include "tcommon.h"
#include <QWidget>
#include "tfx.h"
#include "tparamset.h"
#include "trenderer.h"
#include "tthreadmessage.h"
#include "tthread.h"
#include "trop.h"

using namespace TThread;

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

class DVAPI BgPainter
{
	std::string m_name;

public:
	BgPainter(std::string name) : m_name(name) {}
	virtual ~BgPainter() {}
	std::string getName() const { return m_name; }
	virtual void paint(const TRaster32P &ras) = 0;
};

//=============================================================================

class DVAPI SolidColorBgPainter : public BgPainter
{
	TPixel32 m_color;

public:
	SolidColorBgPainter(std::string name, TPixel32 color)
		: BgPainter(name), m_color(color) {}

	void paint(const TRaster32P &ras)
	{
		ras->fill(m_color);
	}
};

//=============================================================================

class DVAPI CheckboardBgPainter : public BgPainter
{
	TPixel32 m_c0, m_c1;

public:
	CheckboardBgPainter(std::string name, TPixel32 c0, TPixel32 c1)
		: BgPainter(name), m_c0(c0), m_c1(c1) {}

	void paint(const TRaster32P &ras)
	{
		int n = 4, min = 4;
		TDimensionD d(tmax(min, ras->getLx() / n), tmax(min, ras->getLy() / n));
		d.lx = d.ly = tmax(d.lx, d.ly);
		TRop::checkBoard(ras, m_c0, m_c1, d, TPointD());
	}
};

//=============================================================================
/*! \brief SwatchViewer.

		Inherits \b QWidget.
*/
class DVAPI SwatchViewer : public QWidget
{
	Q_OBJECT

	//!La classe \b Point gestisce un punto che e' collegato a parametri \b TPointParam.
	/*!Questo tipo di punti consentono di modificare alcuni parametri dell'effetto corrente e
	sono editabili dall'utente direttamente nello SwatchViewer*/
	class Point
	{
	public:
		int m_index;
		TPointParamP m_param;
		bool m_pairFlag;
		Point(int index, const TPointParamP &param)
			: m_index(index), m_param(param), m_pairFlag(false) {}
	};

	TFxP m_fx;
	TFxP m_actualFxClone;

	TRaster32P m_raster;
	TRaster32P m_content;
	TAffine m_aff;
	TAffine m_fxAff;
	TAffine m_contentAff;

	bool m_cameraMode;
	TRect m_cameraRect;

	Qt::MouseButton m_mouseButton;

	std::vector<Point> m_points;
	std::vector<std::pair<int, int>> m_pointPairs;

	int m_selectedPoint;
	TPointD m_pointPosDelta;

	bool m_enabled;
	int m_frame;
	TThread::Executor m_executor;
	TThread::Mutex m_mutex;

	TRenderer m_renderer;

	BgPainter *m_bgPainter;

	TRaster32P m_crossIcon;

	TPoint m_pos;
	TPoint m_firstPos;
	TRaster32P m_oldContent, m_curContent;

	bool m_computing;

	friend class ContentRender;

public:
	class ContentRender : public TThread::Runnable
	{
	public:
		TRasterFxP m_fx;
		TRasterP m_raster;
		int m_frame;
		TDimension m_size;
		TAffine m_aff;
		SwatchViewer *m_viewer;
		bool m_started;

		ContentRender(TRasterFx *fx, int frame, const TDimension &size, SwatchViewer *viewer);
		~ContentRender();

		void run();
		int taskLoad();

		void onStarted(TThread::RunnableP task);
		void onFinished(TThread::RunnableP task);
		void onCanceled(TThread::RunnableP task);
	};

#if QT_VERSION >= 0x050500
	SwatchViewer(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
	SwatchViewer(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
	~SwatchViewer();

	static void suspendRendering(bool suspend, bool blocking = true);

	void setCameraMode(bool enabled) { m_cameraMode = enabled; }
	bool getCameraMode() const { return m_cameraMode; }

	void setCameraSize(const TDimension &cameraSize);
	TDimension getCameraSize() const { return m_cameraRect.getSize(); }

	void setFx(const TFxP &fx, const TFxP &actualFx, int frame);

	void updateFrame(int frame);

	bool isEnabled() { return m_enabled; }

public slots:
	void setEnable(bool enabled);
	void updateSize(const QSize &size);
	void setBgPainter(TPixel32 color1, TPixel32 color2 = TPixel32());

protected:
	void computeContent();
	TPoint world2win(const TPointD &p) const;
	TPointD win2world(const TPoint &p) const;
	void zoom(const TPoint &pos, double factor);
	void zoom(bool forward, bool reset);

	void updateRaster();

	const TRaster32P &getContent() const { return m_content; }
	void setContent(const TRaster32P &content, const TAffine &contentAff);

	void setAff(const TAffine &aff);

	void paintEvent(QPaintEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *);
	void keyPressEvent(QKeyEvent *event);
	void resizeEvent(QResizeEvent *event);
	void hideEvent(QHideEvent *event);

signals:
	void pointPositionChanged(int index, const TPointD &p);
};

#endif // SWATCHVIEWER_H
