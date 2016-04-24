#ifndef QTOFFLINEGL_H
#define QTOFFLINEGL_H

#include <memory>

#include <QtOpenGL>
#include <QGLFormat>
#include <QGLContext>
#include <QGLPixelBuffer>

#include "tofflinegl.h"

class QtOfflineGL : public TOfflineGL::Imp
{
public:
	QOpenGLContext *m_context;
	QOpenGLContext *m_oldContext;
	QOffscreenSurface *m_surface;

	QtOfflineGL(TDimension rasterSize, std::shared_ptr<TOfflineGL::Imp> shared);
	~QtOfflineGL();

	void createContext(TDimension rasterSize, std::shared_ptr<TOfflineGL::Imp> shared);
	void makeCurrent();
	void doneCurrent();

	void saveCurrentContext();
	void restoreCurrentContext();

	void getRaster(TRaster32P raster);
};

//-----------------------------------------------------------------------------

class QtOfflineGLPBuffer : public TOfflineGL::Imp
{
public:
	QGLPixelBuffer *m_context;

	QtOfflineGLPBuffer(TDimension rasterSize);
	~QtOfflineGLPBuffer();

	void createContext(TDimension rasterSize);
	void makeCurrent();
	void doneCurrent();

	void getRaster(TRaster32P raster);
};

#endif
