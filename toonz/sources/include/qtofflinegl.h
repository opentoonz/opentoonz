#pragma once

#ifndef QTOFFLINEGL_H
#define QTOFFLINEGL_H

#include <memory>

#include <QtOpenGL>
#include <QGLFormat>
#include <QGLContext>
#include <QGLPixelBuffer>
#include <QOpenGLFramebufferObject>

#include "tofflinegl.h"

class QtOfflineGL : public TOfflineGL::Imp
{
public:
        std::shared_ptr<QOpenGLContext> m_context;
        std::shared_ptr<QOpenGLContext> m_oldContext;
        std::shared_ptr<QOffscreenSurface> m_surface;
        std::shared_ptr<QOpenGLFramebufferObject> m_fbo;

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
        std::shared_ptr<QGLPixelBuffer> m_context;

	QtOfflineGLPBuffer(TDimension rasterSize);
	~QtOfflineGLPBuffer();

	void createContext(TDimension rasterSize);
	void makeCurrent();
	void doneCurrent();

	void getRaster(TRaster32P raster);
};

#endif
