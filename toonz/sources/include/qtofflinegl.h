

#ifndef QTOFFLINEGL_H
#define QTOFFLINEGL_H

#include <QtOpenGL>
#include <QGLFormat>
#include <QGLContext>
#include <QGLPixelBuffer>
#include <QOpenGLFramebufferObject>

#include "tofflinegl.h"

class QtOfflineGL : public TOfflineGL::Imp
{
public:
	QOpenGLContext *m_context;
	QOpenGLContext *m_oldContext;
	QOffscreenSurface *m_surface;
        QOpenGLFramebufferObject *m_fbo;


	QtOfflineGL(TDimension rasterSize, const TOfflineGL::Imp *shared = 0);
	~QtOfflineGL();

	void createContext(TDimension rasterSize, const TOfflineGL::Imp *shared);
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
