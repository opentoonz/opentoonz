#pragma once

#ifndef QTOFFLINEGL_H
#define QTOFFLINEGL_H

#include <memory>

#include <QImage>
#include <QtGlobal>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QSurfaceFormat>
#include <QThread>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGLFormat>
#include <QGLPixelBuffer>
#endif

#include "tofflinegl.h"

class QtOfflineGL final : public TOfflineGL::Imp {
public:
  std::shared_ptr<QOpenGLContext> m_context;
  std::shared_ptr<QOpenGLContext> m_oldContext;
  std::shared_ptr<QOffscreenSurface> m_surface;
  std::shared_ptr<QOpenGLFramebufferObject> m_fbo;

  QtOfflineGL(TDimension rasterSize, std::shared_ptr<TOfflineGL::Imp> shared);
  ~QtOfflineGL();

  void createContext(TDimension rasterSize,
                     std::shared_ptr<TOfflineGL::Imp> shared) override;
  void makeCurrent() override;
  void doneCurrent() override;

  void saveCurrentContext();
  void restoreCurrentContext();

  void getRaster(TRaster32P raster) override;
};

//-----------------------------------------------------------------------------

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

class QtOfflineGLPBuffer final : public TOfflineGL::Imp {
public:
  std::shared_ptr<QGLPixelBuffer> m_context;

  QtOfflineGLPBuffer(TDimension rasterSize);
  ~QtOfflineGLPBuffer();

  void createContext(TDimension rasterSize);
  void makeCurrent() override;
  void doneCurrent() override;

  void getRaster(TRaster32P raster) override;
};

#endif

#endif
