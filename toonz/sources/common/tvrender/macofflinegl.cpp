

#include "macofflinegl.h"
#include <traster.h>
#include <tconvert.h>

#include "tthread.h"

namespace {

#if defined(powerpc)
void rightRotateBits(UCHAR *buf, int bufferSize) {
  UINT *buffer = (UINT *)buf;
  register UINT app;
  for (int i = 0; i < bufferSize; i++, buffer++) {
    app     = *buffer;
    *buffer = app >> 8 | app << 24;
  }
}
#else
void rightRotateBits(UCHAR *buf, int bufferSize) {
  UINT *buffer = (UINT *)buf;
  register UINT app;
  for (int i = 0; i < bufferSize; i++, buffer++) {
    app     = *buffer;
    *buffer = (app >> 16 & 0x000000ff) | (app << 16 & 0x00ff0000) |
              (app & 0xff00ff00);
  }
}
#endif

}  // namespace

GLubyte *memBuffer;

//=============================================================================
// MacOfflineGL : implem. offlineGL usando Pixel Buffer (tramite AGL)
//-----------------------------------------------------------------------------

MacOfflineGL::MacOfflineGL(TDimension rasterSize, const TOfflineGL::Imp *shared)
    : TOfflineGL::Imp(rasterSize.lx, rasterSize.ly)
    , m_context(0)
    , m_oldContext(0) {
  createContext(rasterSize, shared);
}

//-----------------------------------------------------------------------------

MacOfflineGL::~MacOfflineGL() { aglDestroyContext(m_context); }

//-----------------------------------------------------------------------------

void MacOfflineGL::createContext(TDimension rasterSize,
                                 const TOfflineGL::Imp *shared) {
  GLint attribs[20], cnt = 0;

  // NOTE: AGL_OFFSCREEN *must* be selected - or it seems that gl surfaces are
  // never destructed correctly!
  // This may lead to a kernel panic!

  attribs[cnt++] = AGL_RGBA;
  attribs[cnt++] = GL_TRUE;
  attribs[cnt++] = AGL_PIXEL_SIZE;
  attribs[cnt++] = 32;
  attribs[cnt++] = AGL_BUFFER_SIZE;
  attribs[cnt++] = 32;
  attribs[cnt++] = AGL_STENCIL_SIZE;
  attribs[cnt++] = 8;
  attribs[cnt++] = AGL_DEPTH_SIZE;
  attribs[cnt++] = 24;
  attribs[cnt++] = AGL_OFFSCREEN;
  attribs[cnt++] = AGL_ALPHA_SIZE;
  attribs[cnt++] = 8;
  attribs[cnt]   = AGL_NONE;

  AGLPixelFormat fmt = aglChoosePixelFormat(0, 0, attribs);

  if (fmt == NULL) {
    GLenum err = aglGetError();
    std::cout << "Unable to create a pixel format, AGLError =  " << err
              << std::endl;
  }

  m_context = aglCreateContext(fmt, NULL);
  if (!m_context) {
    GLenum err = aglGetError();
    /*
    AGL_NO_ERROR                 0
    AGL_BAD_ATTRIBUTE        10000
    AGL_BAD_PROPERTY         10001
    AGL_BAD_PIXELFMT         10002
    AGL_BAD_RENDINFO         10003
    AGL_BAD_CONTEXT          10004
    AGL_BAD_DRAWABLE         10005
    AGL_BAD_GDEV             10006
    AGL_BAD_STATE            10007
    AGL_BAD_VALUE            10008
    AGL_BAD_MATCH            10009
    AGL_BAD_ENUM             10010
    AGL_BAD_OFFSCREEN        10011
    AGL_BAD_FULLSCREEN       10012
    AGL_BAD_WINDOW           10013
    AGL_BAD_POINTER          10014
    AGL_BAD_MODULE           10015
    AGL_BAD_ALLOC            10016
    AGL_BAD_CONNECTION       10017
*/
    std::cout << "Unable to create an OpenGL Context, AGLError = " << err
              << std::endl;
  }

  makeCurrent();

  // Creo il pixel buffer

  GLboolean ret;

  AGLPbuffer pbuffer;

  ret = aglCreatePBuffer(rasterSize.lx, rasterSize.ly, GL_TEXTURE_RECTANGLE_EXT,
                         GL_RGBA, 0, &pbuffer);
  if (!ret) {
    GLenum err = aglGetError();
    std::cout << "Unable to create a PBuffer, AGLError = " << err << std::endl;
  }

  // memBuffer = new GLubyte[rasterSize.lx*rasterSize.ly*4];

  ret = aglSetOffScreen(m_context, rasterSize.lx, rasterSize.ly,
                        rasterSize.lx * 4, memBuffer);

  ret = aglSetPBuffer(m_context, pbuffer, 0, 0, 0);
  if (!ret) {
    GLenum err = aglGetError();
    std::cout << "Unable to set a PBuffer, AGLError = " << err << std::endl;
  }

  // Non serve piu'
  aglDestroyPixelFormat(fmt);
}

//-----------------------------------------------------------------------------

void MacOfflineGL::makeCurrent() {
  if (m_context) {
    bool ret = aglSetCurrentContext(m_context);
    if (ret == GL_FALSE) {
      GLenum err = aglGetError();
      std::cout << "Unable to set current OpenGL Context, AGLError = " << err
                << std::endl;
    }
  } else
    m_oldContext = 0;
}

//-----------------------------------------------------------------------------

void MacOfflineGL::doneCurrent() {
  if (aglGetCurrentContext() != m_context) return;
  aglSetCurrentContext(0);
}

//-----------------------------------------------------------------------------

void MacOfflineGL::saveCurrentContext() {
  m_oldContext = aglGetCurrentContext();
}

//-----------------------------------------------------------------------------

void MacOfflineGL::restoreCurrentContext() {
  if (m_oldContext) aglSetCurrentContext(m_oldContext);
  m_oldContext = 0;
}

//-----------------------------------------------------------------------------

void MacOfflineGL::getRaster(TRaster32P raster) {
  makeCurrent();
  glFinish();

  int lx = raster->getLx();
  int ly = raster->getLy();

  raster->lock();

  glReadPixels(0, 0, lx, ly, GL_RGBA, GL_UNSIGNED_BYTE, raster->getRawData());

  rightRotateBits(raster->getRawData(), lx * ly);

  raster->unlock();
}
