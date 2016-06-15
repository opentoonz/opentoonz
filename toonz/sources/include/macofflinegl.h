#pragma once

#ifndef MACOFFLINEGL_H
#define MACOFFLINEGL_H

#include "tofflinegl.h"
#include <AGL/agl.h>
#include <AGL/aglRenderers.h>
#include <OpenGL/gl.h>

class MacOfflineGL : public TOfflineGL::Imp {
public:
  AGLContext m_context;
  AGLContext m_oldContext;

  MacOfflineGL(TDimension rasterSize, const TOfflineGL::Imp *shared = 0);
  ~MacOfflineGL();

  void createContext(TDimension rasterSize, const TOfflineGL::Imp *shared);
  void makeCurrent();
  void doneCurrent();

  void getRaster(TRaster32P);

  void saveCurrentContext();
  void restoreCurrentContext();
};

#endif  // MACOFFLINEGL_H
