

#ifdef USE_MESA
#include <GL/osmesa.h>
#endif

#include "tvectorrenderdata.h"
#include "tvectorimage.h"
#include "tstrokeutil.h"
#include "tmathutil.h"

#include "tgl.h"
#include "tcurves.h"

#ifndef __sgi
#include <cstdio>
#include <cstdlib>
#include <cstring>
#else
#include <math.h>
#include <string.h>
#include <stdlib.h>
#endif

#if defined(LINUX) || defined(FREEBSD) || defined(__sgi)
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifdef USE_MESA

extern "C" int GLAPIENTRY wglChoosePixelFormat(HDC,
                                               const PIXELFORMATDESCRIPTOR *);
extern "C" int GLAPIENTRY wglDescribePixelFormat(HDC, int, unsigned int,
                                                 LPPIXELFORMATDESCRIPTOR);
extern "C" int GLAPIENTRY wglSetPixelFormat(HDC, int,
                                            const PIXELFORMATDESCRIPTOR *);
extern "C" int GLAPIENTRY wglSwapBuffers(HDC);

#define ChoosePixelFormat wglChoosePixelFormat
#define SetPixelFormat wglSetPixelFormat
#define SwapBuffers wglSwapBuffers
#endif

//=============================================================================

#ifdef WIN32

// init a BITMAPINFO structure
void initBITMAPINFO(BITMAPINFO &info, const TRasterP img) {
  memset(&info, 0, sizeof(BITMAPINFOHEADER));

  info.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth         = img->getLx();
  info.bmiHeader.biHeight        = img->getLy();
  info.bmiHeader.biPlanes        = 1;
  info.bmiHeader.biBitCount      = 32;
  info.bmiHeader.biCompression   = BI_RGB;
  info.bmiHeader.biSizeImage     = 0;
  info.bmiHeader.biXPelsPerMeter = 1000;
  info.bmiHeader.biYPelsPerMeter = 1000;
  info.bmiHeader.biClrUsed       = 0;
  info.bmiHeader.biClrImportant  = 0;
}

//-----------------------------------------------------------------------------
#ifdef BUBU
void hardRenderVectorImage_MESA(const TVectorRenderData &rd, TRaster32P &ras,
                                const TVectorImageP &vimg) {
  int rasterWidth  = ras->getLx();
  int rasterHeight = ras->getLy();

//--- begin mesa stuff

/* Create an RGBA-mode context */
#if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
  /* specify Z, stencil, accum sizes */
  OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
#else
  OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
#endif
  if (!ctx) {
    throw TException("OSMesaCreateContext failed!\n");
  }
  ras->lock();
  /* Bind the buffer to the context and make it current */
  if (!OSMesaMakeCurrent(ctx, ras->getRawData(), GL_UNSIGNED_BYTE, ras->getLx(),
                         ras->getLy())) {
    {
      ras->unlock();
      throw TException("OSMesaMakeCurrent failed!\n");
    }
  }

  //---end mesa stuff
  // cfr. help: OpenGL/Programming tip/OpenGL Correctness Tips
  glViewport(0, 0, rasterWidth, rasterHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, rasterWidth, 0, rasterHeight);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.375, 0.375, 0.0);
  /*
glClearColor(0.0f,0.0f,0.0f,0.0f);
glClear(GL_COLOR_BUFFER_BIT);

// draw background
glRasterPos2d(0, 0);
glDrawPixels( ras->getLx(),ras->getLy(), GL_BGRA_EXT, GL_UNSIGNED_BYTE,
ras->getRawData());
*/
  // do OpenGL draw
  assert(vimg);
  tglDraw(rd, vimg.getPointer());

  // force to finish
  glFlush();
  glFinish();
  /*
// set info in out raster
TDimension size = ras->getSize();

if( ras->getWrap() == rasterWidth )
memcpy( ras->getRawData(), offData, ras->getPixelSize()*size.lx*size.ly );
else
{
TRaster32P  temp( ras->getLx(), ras->getLy());
memcpy( temp->getRawData(), offData, ras->getPixelSize()*size.lx*size.ly );
ras->copy(temp);
}
*/
  OSMesaDestroyContext(ctx);
  ras->unlock();
}
#endif  // BUBU
void hardRenderVectorImage(const TVectorRenderData &rd, TRaster32P &ras,
                           const TVectorImageP &vimg) {
  int rasterWidth  = ras->getLx();
  int rasterHeight = ras->getLy();
  ras->lock();

#ifdef USE_MESA
/* Create an RGBA-mode context */
#if OSMESA_MAJOR_VERSION * 100 + OSMESA_MINOR_VERSION >= 305
  /* specify Z, stencil, accum sizes */
  OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
#else
  OSMesaContext ctx = OSMesaCreateContext(OSMESA_RGBA, NULL);
#endif
  if (!ctx) {
    ras->unlock();
    throw TException("OSMesaCreateContext failed!\n");
  }

  /* Bind the buffer to the context and make it current */

  if (!OSMesaMakeCurrent(ctx, ras->getRawData(), GL_UNSIGNED_BYTE, ras->getLx(),
                         ras->getLy())) {
    {
      ras->unlock();
      throw TException("OSMesaMakeCurrent failed!\n");
    }
  }
#else
  BITMAPINFO info;

  initBITMAPINFO(info, ras);

  void *offData = 0;  // a pointer to buffer

  // open an offscreen device
  HDC offDC = CreateCompatibleDC(NULL);

  HBITMAP offDIB =  // and a bitmap image
      CreateDIBSection(offDC, &info, DIB_RGB_COLORS, &offData, NULL, 0);

  assert(offDIB);
  assert(offData);

  int dataSize =  // number of byte of raster
      rasterWidth * rasterHeight * 4;

  memset(offData, 0, dataSize);

  HGDIOBJ oldobj =  // select BIB to write
      SelectObject(offDC, offDIB);

  static PIXELFORMATDESCRIPTOR pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
      1,                              // version number
      0 | (false ? (PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER)
                 : (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI)) |
          PFD_SUPPORT_OPENGL,  // support OpenGL
      PFD_TYPE_RGBA,           // RGBA type
      32,                      // 32-bit color depth
      0,
      0, 0, 0, 0, 0,   // color bits ignored
      0,               // no alpha buffer
      0,               // shift bit ignored
      0,               // no accumulation buffer
      0, 0, 0, 0,      // accum bits ignored
      32,              // 32-bit z-buffer
      0,               // no stencil buffer
      0,               // no auxiliary buffer
      PFD_MAIN_PLANE,  // main layer
      0,               // reserved
      0, 0, 0          // layer masks ignored
  };

  // get the best available match of pixel format for the device context
  int iPixelFormat = ChoosePixelFormat(offDC, &pfd);
  assert(iPixelFormat != 0);

  // make that the pixel format of the device context
  int ret = SetPixelFormat(offDC, iPixelFormat, &pfd);
  assert(ret == TRUE);

  // make a valid context for OpenGL rendering
  HGLRC hglRC = wglCreateContext(offDC);
  assert(hglRC);
  ret = wglMakeCurrent(offDC, hglRC);
  assert(ret == TRUE);
#endif
  // cfr. help: OpenGL/Programming tip/OpenGL Correctness Tips
  glViewport(0, 0, rasterWidth, rasterHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, rasterWidth, 0, rasterHeight);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.375, 0.375, 0.0);
#ifndef USE_MESA
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // draw background
  glRasterPos2d(0, 0);
  glDrawPixels(ras->getLx(), ras->getLy(), GL_BGRA_EXT, GL_UNSIGNED_BYTE,
               ras->getRawData());
#endif
  // do OpenGL draw
  assert(vimg);

  tglDraw(rd, vimg.getPointer());

  // force to finish
  glFlush();

#ifdef USE_MESA
  OSMesaDestroyContext(ctx);
#else
  // set info in out raster
  TDimension size = ras->getSize();

  if (ras->getWrap() == rasterWidth)
    memcpy(ras->getRawData(), offData, ras->getPixelSize() * size.lx * size.ly);
  else {
    TRaster32P temp(ras->getLx(), ras->getLy());
    memcpy(temp->getRawData(), offData,
           ras->getPixelSize() * size.lx * size.ly);
    ras->copy(temp);
  }

  ret = wglMakeCurrent(offDC, NULL);
  assert(ret == TRUE);
  wglDeleteContext(hglRC);

  // release object
  SelectObject(offDC, oldobj);
  DeleteObject(offDIB);
  DeleteObject(offDC);
#endif
  ras->unlock();
}

// end of WIN32
#elif defined(__sgi) || defined(LINUX) || defined(FREEBSD)

//=============================================================================

namespace {

GLXContext ctx;
XVisualInfo *visinfo;
GC gc;
Window make_rgb_window(Display *dpy, unsigned int width, unsigned int height) {
  const int sbAttrib[] = {GLX_RGBA, GLX_RED_SIZE,  1, GLX_GREEN_SIZE,
                          1,        GLX_BLUE_SIZE, 1, None};
  const int dbAttrib[] = {GLX_RGBA, GLX_RED_SIZE,  1, GLX_GREEN_SIZE,
                          1,        GLX_BLUE_SIZE, 1, GLX_DOUBLEBUFFER,
                          None};
  int scrnum;
  XSetWindowAttributes attr;
  TUINT32 mask;
  Window root;
  Window win;

  scrnum = DefaultScreen(dpy);
  root   = RootWindow(dpy, scrnum);

  visinfo = glXChooseVisual(dpy, scrnum, (int *)sbAttrib);
  if (!visinfo) {
    visinfo = glXChooseVisual(dpy, scrnum, (int *)dbAttrib);
    if (!visinfo) {
      printf("Error: couldn't get an RGB visual\n");
      exit(1);
    }
  }

  /* window attributes */
  attr.background_pixel = 0;
  attr.border_pixel     = 0;
  /* TODO: share root colormap if possible */
  attr.colormap   = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
  attr.event_mask = StructureNotifyMask | ExposureMask;
  mask            = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  win = XCreateWindow(dpy, root, 0, 0, width, height, 0, visinfo->depth,
                      InputOutput, visinfo->visual, mask, &attr);

  /* make an X GC so we can do XCopyArea later */
  gc = XCreateGC(dpy, win, 0, NULL);

  /* need indirect context */
  ctx = glXCreateContext(dpy, visinfo, NULL, False);
  if (!ctx) {
    printf("Error: glXCreateContext failed\n");
    exit(-1);
  }

  printf("Direct rendering: %s\n", glXIsDirect(dpy, ctx) ? "Yes" : "No");

  return win;
}

GLXPixmap make_pixmap(Display *dpy, Window win, unsigned int width,
                      unsigned int height, Pixmap *pixmap) {
  Pixmap pm;
  GLXPixmap glxpm;
  XWindowAttributes attr;

  pm = XCreatePixmap(dpy, win, width, height, visinfo->depth);
  if (!pm) {
    printf("Error: XCreatePixmap failed\n");
    exit(-1);
  }

  XGetWindowAttributes(dpy, win, &attr);

/*
    * IMPORTANT:
    *   Use the glXCreateGLXPixmapMESA funtion when using Mesa because
    *   Mesa needs to know the colormap associated with a pixmap in order
    *   to render correctly.  This is because Mesa allows RGB rendering
    *   into any kind of visual, not just TrueColor or DirectColor.
    */
#ifdef PROBLEMI_CON_IL_DRIVER_NVIDIA  // GLX_MESA_pixmap_colormap //
  if (strstr(glXQueryExtensionsString(dpy, 0), "GLX_MESA_pixmap_colormap")) {
    /* stand-alone Mesa, specify the colormap */
    glxpm = glXCreateGLXPixmapMESA(dpy, visinfo, pm, attr.colormap);
  } else {
    glxpm = glXCreateGLXPixmap(dpy, visinfo, pm);
  }
#else
  /* This will work with Mesa too if the visual is TrueColor or DirectColor */
  glxpm = glXCreateGLXPixmap(dpy, visinfo, pm);
#endif

  if (!glxpm) {
    printf("Error: GLXCreateGLXPixmap failed\n");
    exit(-1);
  }

  *pixmap = pm;

  return glxpm;
}
}

// void  offscreenRender(TRaster32P& ras, const TVectorImageP& vimg, const
// TAffine& aff)
void hardRenderVectorImage(const TVectorRenderData &rd, TRaster32P &ras,
                           const TVectorImageP &vimg) {
  Display *dpy;
  Window win;
  Pixmap pm;
  GLXPixmap glxpm;

  ras->lock();

  dpy = XOpenDisplay(NULL);

  win   = make_rgb_window(dpy, ras->getLx(), ras->getLy());
  glxpm = make_pixmap(dpy, win, ras->getLx(), ras->getLy(), &pm);

  GLXContext oldctx  = glXGetCurrentContext();
  GLXDrawable olddrw = glXGetCurrentDrawable();

  glXMakeCurrent(dpy, glxpm, ctx);
  // printf("GL_RENDERER = %s\n", (char *) glGetString(GL_RENDERER));

  // cfr. help: OpenGL/Programming tip/OpenGL Correctness Tips
  glViewport(0, 0, ras->getLx(), ras->getLy());

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, ras->getLx(), 0, ras->getLy());
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.375, 0.375, 0.0);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // draw background
  glRasterPos2d(0, 0);
  glDrawPixels(ras->getLx(), ras->getLy(), GL_RGBA, GL_UNSIGNED_BYTE,
               ras->getRawData());

  // do OpenGL draw
  assert(vimg);

  tglDraw(rd, vimg.getPointer());

  glFlush();

#if defined(__sgi)

  glReadPixels(0, 0, ras->getLx(), ras->getLy(), GL_ABGR_EXT, GL_UNSIGNED_BYTE,
               ras->getRawData());

#elif defined(LINUX) || defined(FREEBSD)

  glReadPixels(0, 0, ras->getLx(), ras->getLy(), GL_RGBA, GL_UNSIGNED_BYTE,
               ras->getRawData());
#endif

  Bool ret = glXMakeCurrent(dpy, olddrw, oldctx);
#ifdef DEBUG
  if (!ret) {
    std::cerr << __FUNCTION__ << " error in glXMakeCurrent olddrw=" << olddrw
              << " oldctx=" << oldctx << std::endl;
  }
#endif
  ras->unlock();
}

#endif
