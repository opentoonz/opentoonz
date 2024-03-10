

// TnzCore includes
#include "texception.h"
#include "tvectorgl.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tgl.h"
#include "tthreadmessage.h"
#include "tsystem.h"
#include "trop.h"

// Platform-specific includes
#if defined(LINUX) || defined(FREEBSD)

#include "qtofflinegl.h"
#include <X11/Xlib.h>
#include <GL/glx.h>

#include "tthread.h"

#elif defined(MACOSX) || defined(HAIKU)

#include "qtofflinegl.h"

#endif

#include "tofflinegl.h"

#ifndef checkErrorsByGL
#define checkErrorsByGL                                                        \
  {                                                                            \
    GLenum err = glGetError();                                                 \
    assert(err != GL_INVALID_ENUM);                                            \
    assert(err != GL_INVALID_VALUE);                                           \
    assert(err != GL_INVALID_OPERATION);                                       \
    assert(err != GL_STACK_OVERFLOW);                                          \
    assert(err != GL_STACK_UNDERFLOW);                                         \
    assert(err != GL_OUT_OF_MEMORY);                                           \
    assert(err == GL_NO_ERROR);                                                \
  }
#endif

#undef checkErrorsByGL
#define checkErrorsByGL /**/

using namespace std;

TGLContextManager *currentContextManager = 0;

void TOfflineGL::setContextManager(TGLContextManager *contextManager) {
  currentContextManager = contextManager;
  if (contextManager) contextManager->store();
}

//=============================================================================
// WIN32Implementation : implementazione offlineGL  WIN32
//-----------------------------------------------------------------------------

#ifdef _WIN32

namespace {
// We found out that our implementation of win32 opengl contexts can be someway
// not thread-safe. Deadlocks and errors could happen for wgl* and GDI functions
// on particular configurations (notably, Windows 7). So we mutex them as
// a temporary workaround.
static QMutex win32ImpMutex;
}  // namespace

//-------------------------------

class WIN32Implementation final : public TOfflineGL::Imp {
public:
  HDC m_offDC;
  HGDIOBJ m_oldobj;
  HGLRC m_hglRC;
  HBITMAP m_offDIB;
  void *m_offData;

  //-----------------------------------------------------------------------------

  WIN32Implementation(TDimension rasterSize,
                      std::shared_ptr<TOfflineGL::Imp> shared)
      : TOfflineGL::Imp(rasterSize.lx, rasterSize.ly) {
    m_offData = 0;
    createContext(
        rasterSize,
        std::move(shared));  // makeCurrent is called at the end of this

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    doneCurrent();  // doneCurrent must therefore be called here
  }

  //-----------------------------------------------------------------------------

  ~WIN32Implementation() {
    QMutexLocker locker(&win32ImpMutex);

    BOOL ret = wglMakeCurrent(m_offDC, NULL);
    assert(ret == TRUE);
    wglDeleteContext(m_hglRC);
    SelectObject(m_offDC, m_oldobj);
    DeleteObject(m_offDC);

    // si potrebbe passare un parametro che evita di distruggere la bitmap.
    // In tal caso il raster dovrebbe diventare owner del buffer, ma attualmente
    // questo non e' settabile in TRaster: quando gli si passa da fuori un
    // buffer,
    // automaticamente bufferOwner viene settato a false e non e' modificabile!
    DeleteObject(m_offDIB);
  }

  //-----------------------------------------------------------------------------

  void makeCurrent() override {
    QMutexLocker locker(&win32ImpMutex);

    int ret = wglMakeCurrent(m_offDC, m_hglRC);
    assert(ret == TRUE);
  }

  //-----------------------------------------------------------------------------

  void doneCurrent() override {
    QMutexLocker locker(&win32ImpMutex);

    glFlush();
    glFinish();
    assert(glGetError() == 0);
    wglMakeCurrent(NULL, NULL);
  }

  //-----------------------------------------------------------------------------

  void initBITMAPINFO(BITMAPINFO &info, const TDimension rasterSize) {
    memset(&info, 0, sizeof(BITMAPINFOHEADER));

    info.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth         = rasterSize.lx;
    info.bmiHeader.biHeight        = rasterSize.ly;
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

  void createContext(TDimension rasterSize,
                     std::shared_ptr<TOfflineGL::Imp> shared) override {
    QMutexLocker locker(&win32ImpMutex);

    BITMAPINFO info;

    initBITMAPINFO(info, rasterSize);

    // open an offscreen device
    m_offDC = CreateCompatibleDC(NULL);

    // and a bitmap image
    m_offDIB =
        CreateDIBSection(m_offDC, &info, DIB_RGB_COLORS, &m_offData, NULL, 0);

    assert(m_offDIB);
    assert(m_offData);

    if (!m_offDIB || !m_offData)
      throw TException("cannot create OpenGL context. Check system resources!");

    int dataSize =
        rasterSize.lx * rasterSize.ly * 4;  // number of byte of raster

    memset(m_offData, 0, dataSize);

    m_oldobj = SelectObject(m_offDC, m_offDIB);  // select BIB to write

    static PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),  // size of this pfd
        1,                              // version number
        0 |
            (false ? (PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER)
                   : (PFD_DRAW_TO_BITMAP | PFD_SUPPORT_GDI)) |
            PFD_SUPPORT_OPENGL,  // support OpenGL
        PFD_TYPE_RGBA,           // RGBA type
        32,                      // 32-bit color depth
        0,
        0,
        0,
        0,
        0,
        0,  // color bits ignored
        8,  // no alpha buffer /*===*/
        0,  // shift bit ignored
        0,  // no accumulation buffer
        0,
        0,
        0,
        0,               // accum bits ignored
        32,              // 32-bit z-buffer
        32,              // max stencil buffer
        0,               // no auxiliary buffer
        PFD_MAIN_PLANE,  // main layer
        0,               // reserved
        0,
        0,
        0  // layer masks ignored
    };

    // get the best available match of pixel format for the device context
    int iPixelFormat = ChoosePixelFormat(m_offDC, &pfd);
    assert(iPixelFormat != 0);

    // make that the pixel format of the device context
    int ret = SetPixelFormat(m_offDC, iPixelFormat, &pfd);
    assert(ret == TRUE);

    // make a valid context for OpenGL rendering

    m_hglRC = wglCreateContext(m_offDC);
    assert(m_hglRC);

    if (!m_hglRC)
      throw TException("cannot create OpenGL context. Check system resources!");

    if (shared) {
      // Share shared's display lists
      const WIN32Implementation *sharedImp =
          dynamic_cast<const WIN32Implementation *>(shared.get());
      assert(sharedImp);

      bool ok = wglShareLists(sharedImp->m_hglRC, m_hglRC);
      assert(ok);
    }

    ret = wglMakeCurrent(m_offDC, m_hglRC);
    assert(ret == TRUE);
  }

  //-----------------------------------------------------------------------------

  void swapRedBlueChannels(
      void *buffer,
      int bufferSize)  // Flips The Red And Blue Bytes (WidthxHeight)
  {
    void *b = buffer;  // Pointer To The Buffer

#if !defined(x64) && defined(_MSC_VER)
    __asm            // Assembler Code To Follow
    {
        mov ecx, bufferSize  // Counter Set To Dimensions Of Our Memory Block
        mov ebx, b  // Points ebx To Our Data (b)
        label:  // Label Used For Looping
          mov al,[ebx+0]  // Loads Value At ebx Into al
          mov ah,[ebx+2]  // Loads Value At ebx+2 Into ah
          mov [ebx+2],al  // Stores Value In al At ebx+2
          mov [ebx+0],ah  // Stores Value In ah At ebx

          add ebx,4  // Moves Through The Data By 4 Bytes
          dec ecx  // Decreases Our Loop Counter
          jnz label  // If Not Zero Jump Back To Label
    }
#else
    int size   = bufferSize;
    UCHAR *pix = (UCHAR *)b;
    while (size > 0) {
      UCHAR r    = *pix;
      UCHAR b    = *(pix + 2);
      *pix       = b;
      *(pix + 2) = r;
      pix += 4;
      size--;
    }
/*unsigned long ebx = (unsigned long)b;
          while(size>0)
          {
            unsigned char al =__readgsbyte(ebx);
                unsigned char ah =__readgsbyte(ebx+2);
                __writegsbyte(ebx+2,al);
                __writegsbyte(ebx,ah);
                ebx+=4;
            size--;
          }*/
#endif
  }

  //-----------------------------------------------------------------------------

  void getRaster(TRaster32P raster) override {
    makeCurrent();
    glFlush();

    int lx = raster->getLx();
    int ly = raster->getLy();

    raster->lock();
    glReadPixels(0, 0, lx, ly, GL_RGBA /*GL_BGRA_EXT*/, GL_UNSIGNED_BYTE,
                 raster->getRawData());

    swapRedBlueChannels(raster->getRawData(), lx * ly);
    raster->unlock();
  }
};

// default imp generator
static std::shared_ptr<TOfflineGL::Imp> defaultOfflineGLGenerator(
    const TDimension &dim, std::shared_ptr<TOfflineGL::Imp> shared) {
  return std::make_shared<WIN32Implementation>(dim, shared);
}

//=============================================================================
// XImplementation : implementazione offlineGL  Server X (MACOSX & LINUX & BSD)
//-----------------------------------------------------------------------------

#elif defined(LINUX) || defined(FREEBSD)
namespace {
// The XScopedLock stuff doesn't seem finished,
// why not just do the same as with win32 and use a Qt lock??
static QMutex linuxImpMutex;
}  // namespace

class XImplementation final : public TOfflineGL::Imp {
public:
  Display *m_dpy;
  GLXContext m_context;
  GLXPixmap m_pixmap;
  Pixmap m_xpixmap;
  TRaster32P m_raster;

  //-----------------------------------------------------------------------------

  XImplementation(TDimension rasterSize)
      : TOfflineGL::Imp(rasterSize.lx, rasterSize.ly) {
    createContext(rasterSize);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  //-----------------------------------------------------------------------------

  ~XImplementation() {
    glXDestroyContext(m_dpy, m_context);
    m_context = 0;
    safeGlXMakeCurrent(true);
    XCloseDisplay(m_dpy);
  }

  //-----------------------------------------------------------------------------

  bool safeGlXMakeCurrent(bool isDtor = false) {
    static std::map<pthread_t, GLXContext> m_glxContext;
    static TThread::Mutex mutex;

    QMutexLocker sl(&mutex);
    pthread_t self                               = pthread_self();
    std::map<pthread_t, GLXContext>::iterator it = m_glxContext.find(self);
    if (((it != m_glxContext.end()) && (it->second != m_context)) ||
        (it == m_glxContext.end())) {
      //	cout << "calling GLXMakeCurrent " << self << " " << m_context <<
      // endl;
      Bool ret;
      if (!isDtor) ret = glXMakeCurrent(m_dpy, m_pixmap, m_context);
      m_glxContext[self] = m_context;
      return ret;
    }
    // cout << "don't call GLXMakeCurrent " << self << " " << m_context << endl;
    return true;
  }

  //-----------------------------------------------------------------------------

  void makeCurrent() {
    QMutexLocker locker(&linuxImpMutex);

    // Bool ret = glXMakeCurrent(m_dpy,m_pixmap,m_context);

    Bool ret = safeGlXMakeCurrent();
    assert(ret == True);
  }

  //-----------------------------------------------------------------------------

  // DA IMPLEMENTARE !!!

  void doneCurrent() {}

  //-----------------------------------------------------------------------------

  void createContext(TDimension rasterSize) {
    m_dpy            = XOpenDisplay(NULL);
    Window win       = DefaultRootWindow(m_dpy);
    int attribList[] = {GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1,
                        GLX_BLUE_SIZE, 1,
                        //    			GLX_ALPHA_SIZE,  1,
                        GLX_STENCIL_SIZE, 8,

                        //			GLX_DEPTH_SIZE,  24,

                        None};

    int dbAttrib[] = {GLX_RGBA, GLX_RED_SIZE,     1,   GLX_GREEN_SIZE,
                      1,        GLX_BLUE_SIZE,    1,   GLX_STENCIL_SIZE,
                      8,        GLX_DOUBLEBUFFER, None};

    int w = rasterSize.lx;
    int h = rasterSize.ly;

    XVisualInfo *vis = glXChooseVisual(m_dpy, DefaultScreen(m_dpy), attribList);
    if (!vis) {
      std::cout << "unable to create sb visual" << std::endl;
      vis = glXChooseVisual(m_dpy, DefaultScreen(m_dpy), dbAttrib);
      assert(vis && "unable to create db visual");
    }

    m_context = glXCreateContext(m_dpy, vis, 0, False);
    //    std::cout << "Direct rendering: " << (glXIsDirect(m_dpy, m_context) ?
    //    "Yes" : "No" )<< std::endl;

    if (!m_context) assert("not m_context" && false);
    TRaster32P raster(w, h);

    m_xpixmap = XCreatePixmap(m_dpy, win, w, h, vis->depth);

    assert(m_xpixmap && "not m_xpixmap");

    m_pixmap = glXCreateGLXPixmap(m_dpy, vis, m_xpixmap);
    if (!m_pixmap) assert("not m_pixmap" && m_pixmap);
    /*
Bool ret = glXMakeCurrent(m_dpy,
                  m_pixmap,
                  m_context);
*/
    Bool ret = safeGlXMakeCurrent();
    assert(ret);

    m_raster = raster;
  }

  //-----------------------------------------------------------------------------

#if defined(MACOSX)
#if defined(powerpc)
  void rightRotateBits(UCHAR *buf, int bufferSize) {
    UINT *buffer = (UINT *)buf;
    UINT app;
    for (int i = 0; i < bufferSize; i++, buffer++) {
      app     = *buffer;
      *buffer = app >> 8 | app << 24;
    }
  }
#else
  void rightRotateBits(UCHAR *buf, int bufferSize) {
    UINT *buffer = (UINT *)buf;
    UINT app;
    for (int i = 0; i < bufferSize; i++, buffer++) {
      app     = *buffer;
      *buffer = (app >> 16 & 0x000000ff) | (app << 16 & 0x00ff0000) |
                (app & 0xff00ff00);
    }
  }
#endif
#endif
  //-----------------------------------------------------------------------------

  const TRaster32P &getRaster() {
    makeCurrent();
    glFlush();

    int lx = m_raster->getLx();
    int ly = m_raster->getLy();
    m_raster->lock();
    glReadPixels(0, 0, lx, ly, GL_RGBA /*GL_BGRA_EXT*/, GL_UNSIGNED_BYTE,
                 m_raster->getRawData());

#if defined(MACOSX)
    rightRotateBits(m_raster->getRawData(), lx * ly);
#warning "to do"
#endif
    m_raster->unlock();
    return m_raster;
  }

  //-----------------------------------------------------------------------------

  int getLx() const { return m_raster->getLx(); }

  //-----------------------------------------------------------------------------

  int getLy() const { return m_raster->getLy(); }
};

static std::shared_ptr<TOfflineGL::Imp> defaultOfflineGLGenerator(
    const TDimension &dim, std::shared_ptr<TOfflineGL::Imp> shared) {
  return std::make_shared<QtOfflineGL>(dim, shared);
}

#elif defined(MACOSX) || defined(HAIKU)

std::shared_ptr<TOfflineGL::Imp> defaultOfflineGLGenerator(
    const TDimension &dim, std::shared_ptr<TOfflineGL::Imp> shared) {
  return std::make_shared<QtOfflineGL>(dim, shared);
}

#endif

//=============================================================================

//-----------------------------------------------------------------------------

// current imp generator
namespace {
TOfflineGL::ImpGenerator *currentImpGenerator = defaultOfflineGLGenerator;
}  // namespace

//=============================================================================
// TOfflineGL
//-----------------------------------------------------------------------------

// namespace {

class MessageCreateContext final : public TThread::Message {
  friend class TOfflineGL;

  TOfflineGL *m_ogl;
  TDimension m_size;
  std::shared_ptr<TOfflineGL::Imp> m_shared;

public:
  MessageCreateContext(TOfflineGL *ogl, const TDimension &size,
                       TOfflineGL::Imp *shared)
      : m_ogl(ogl), m_size(size), m_shared(shared) {}

  void onDeliver() override {
    m_ogl->m_imp = currentImpGenerator(m_size, m_shared);
  }

  TThread::Message *clone() const override {
    return new MessageCreateContext(*this);
  }
};

//} // namespace

//--------------------------------------------------

TOfflineGL::TOfflineGL(TDimension dim, const TOfflineGL *shared) : m_imp(0) {
#if defined(LINUX) || defined(FREEBSD)
  QMutexLocker locker(&linuxImpMutex);
#endif

  /*
 The original code did some incomprehensible things like creating an offline
 renderer (to be called from another thread) and dispatching it to the main
 thread, but Q*GLContext can't go beyond the  thread context, so it is created
 directly and closed in this context. It does not dispatch to another thread.
*/
  m_imp = currentImpGenerator(dim, shared ? shared->m_imp : 0);

  initMatrix();
}

//-----------------------------------------------------------------------------

TOfflineGL::TOfflineGL(const TRaster32P &raster, const TOfflineGL *shared) {
#if defined(LINUX) || defined(FREEBSD)
  QMutexLocker locker(&linuxImpMutex);
#endif

  // m_imp = new Imp(raster->getSize());

  m_imp = currentImpGenerator(raster->getSize(), shared->m_imp);

  initMatrix();

  glRasterPos2d(0, 0);
  raster->lock();
  glDrawPixels(raster->getLx(), raster->getLy(), GL_BGRA_EXT, GL_UNSIGNED_BYTE,
               raster->getRawData());
  raster->unlock();
}

//-----------------------------------------------------------------------------

TOfflineGL::~TOfflineGL() {
  // delete m_imp;
}

//-----------------------------------------------------------------------------

TOfflineGL::ImpGenerator *TOfflineGL::defineImpGenerator(
    TOfflineGL::ImpGenerator *impGenerator) {
  TOfflineGL::ImpGenerator *ret = currentImpGenerator;
  currentImpGenerator           = impGenerator;
  return ret;
}

//-----------------------------------------------------------------------------

void TOfflineGL::makeCurrent() {
  if (currentContextManager) currentContextManager->store();
  // All the code was moved inside Imp
  m_imp->makeCurrent();
  assert(glGetError() == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------

void TOfflineGL::doneCurrent() {
  m_imp->doneCurrent();
  if (currentContextManager) {
    currentContextManager->restore();
  }
}

//-----------------------------------------------------------------------------

void TOfflineGL::initMatrix() {
  m_imp->makeCurrent();

  // cfr. help: OpenGL/Programming tip/OpenGL Correctness Tips
  glViewport(0, 0, m_imp->getLx(), m_imp->getLy());
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, m_imp->getLx(), 0, m_imp->getLy());
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  // glTranslatef(0.375, 0.375, 0.0);    //WRONG

  /*  (From Daniele)

Quoting from the aforementioned source:

"An optimum compromise that allows all primitives to be specified at integer
positions, while still ensuring predictable rasterization, is to translate x
and y by 0.375, as shown in the following code sample. Such a translation
keeps polygon and pixel image edges safely away from the centers of pixels,
while moving line vertices close enough to the pixel centers"

NOTE: This is not an acceptable excuse in our case - as we're NOT USING
    INTEGER COORDINATES ONLY. OpenGL has all the rights to render pixels
    at integer coordinates across the neighbouring 4 pixels - and their
    (0.5, 0.5) translations at the EXACT screen pixel.
*/
}

//-----------------------------------------------------------------------------

void TOfflineGL::clear(TPixel32 color) {
  const double maxValue = 255.0;
  makeCurrent();
  glClearColor((double)color.r / maxValue, (double)color.g / maxValue,
               (double)color.b / maxValue, (double)color.m / maxValue);
  glClear(GL_COLOR_BUFFER_BIT);
}

//-----------------------------------------------------------------------------

void TOfflineGL::draw(TVectorImageP image, const TVectorRenderData &rd,
                      bool doInitMatrix) {
  checkErrorsByGL;
  makeCurrent();
  checkErrorsByGL;

  if (doInitMatrix) {
    initMatrix();
    checkErrorsByGL;
  }

  if (image) {
    checkErrorsByGL;
    tglDraw(rd, image.getPointer());
    checkErrorsByGL;
  }

  checkErrorsByGL;
  glFlush();
  checkErrorsByGL;
}

//-----------------------------------------------------------------------------

void TOfflineGL::draw(TRasterImageP ri, const TAffine &aff, bool doInitMatrix) {
  makeCurrent();

  if (doInitMatrix) initMatrix();

  TRaster32P ras32 = ri->getRaster();
  if (!ras32) return;

  int lx = ras32->getLx();
  int ly = ras32->getLy();
  // lx e ly devono essere potenze di due
  assert((lx & (lx - 1)) == 0);
  assert((ly & (ly - 1)) == 0);

  glPushMatrix();
  tglMultMatrix(aff);

  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

  glEnable(GL_TEXTURE_2D);

  /*
TNZ_MACHINE_CHANNEL_ORDER_BGRM
TNZ_MACHINE_CHANNEL_ORDER_MBGR
TNZ_MACHINE_CHANNEL_ORDER_RGBM
TNZ_MACHINE_CHANNEL_ORDER_MRGB
*/

  GLenum fmt = TGL_FMT;
  /*
#if defined(TNZ_MACHINE_CHANNEL_ORDER_BGRM)
GL_BGRA_EXT;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MBGR)
GL_ABGR_EXT;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_RGBM)
GL_RGBA;
#elif defined(TNZ_MACHINE_CHANNEL_ORDER_MRGB)
#warning "to do"
GL_ABGR_EXT;
#else
Error  PLATFORM NOT SUPPORTED
#endif
*/

  // Generate a texture id and bind it.
  GLuint texId;
  glGenTextures(1, &texId);

  glBindTexture(GL_TEXTURE_2D, texId);

  glPixelStorei(GL_UNPACK_ROW_LENGTH,
                ras32->getWrap() != ras32->getLx() ? ras32->getWrap() : 0);

  ras32->lock();

  glTexImage2D(GL_TEXTURE_2D,  // target (is a 2D texture)
               0,              // is one level only
               GL_RGB,         //  number of component of a pixel
               lx,             // size width
               ly,             //      height
               0,              // size of a border
               fmt,
               GL_UNSIGNED_BYTE,  //
               ras32->getRawData());

  ras32->unlock();

  double halfWidth  = 0.5 * lx;
  double halfHeight = 0.5 * ly;

  double dpix = 1, dpiy = 1;
  ri->getDpi(dpix, dpiy);

  if (dpix != 0 && dpiy != 0) {
    double unit = 100;
    halfWidth *= unit / dpix;
    halfHeight *= unit / dpiy;
  }

  glBegin(GL_QUAD_STRIP);
  glTexCoord2d(0, 0);
  glVertex2d(-halfWidth, -halfHeight);
  glTexCoord2d(1, 0);
  glVertex2d(halfWidth, -halfHeight);
  glTexCoord2d(0, 1);
  glVertex2d(-halfWidth, halfHeight);
  glTexCoord2d(1, 1);
  glVertex2d(halfWidth, halfHeight);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  glPopMatrix();
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  // Delete texture
  glDeleteTextures(1, &texId);

  glFlush();
}

//-----------------------------------------------------------------------------

void TOfflineGL::flush() {
  makeCurrent();
  glFlush();
}

//-----------------------------------------------------------------------------

void TOfflineGL::getRaster(TRaster32P raster) {
  assert(raster->getLx() <= getLx() && raster->getLy() <= getLy());
  if (raster->getWrap() == raster->getLx()) {
    m_imp->getRaster(raster);
  } else {
    // There are 2 possible solutions: use glReadPixels multiple times for each
    // row of input raster,
    // OR allocate a new contiguous buffer, use glReadPixels once, and then
    // memcpy each row.
    // It actually seems that the *latter* is actually the fastest solution,
    // although it requires
    // allocating a temporary buffer of the same size of the requested raster...
    // I also could not actually manage to make the former work - the code
    // seemed right but results were weird...
    TRaster32P ras32(raster->getSize());
    m_imp->getRaster(ras32);
    TRop::copy(raster, ras32);
  }
}

//-----------------------------------------------------------------------------

void TOfflineGL::getRaster(TRasterP raster) {
  assert(raster->getLx() <= getLx() && raster->getLy() <= getLy());
  TRaster32P ras32 = raster;
  if (ras32 && (raster->getWrap() == raster->getLx())) {
    m_imp->getRaster(ras32);
  } else {
    ras32 = TRaster32P(raster->getSize());
    m_imp->getRaster(ras32);
    TRop::copy(raster, ras32);
  }
}

//-----------------------------------------------------------------------------

TRaster32P TOfflineGL::getRaster() {
  TRaster32P raster(getLx(), getLy());
  m_imp->getRaster(raster);
  return raster;
}

//-----------------------------------------------------------------------------

int TOfflineGL::getLx() const { return m_imp->getLx(); }

//-----------------------------------------------------------------------------

int TOfflineGL::getLy() const { return m_imp->getLy(); }

//-----------------------------------------------------------------------------

//=============================================================================

namespace {

struct DimensionLess final
    : public std::binary_function<TDimension, TDimension, bool> {
  bool operator()(const TDimension &d1, const TDimension &d2) const {
    return d1.lx < d2.lx || (d1.lx == d2.lx && d1.ly < d2.ly);
  }
};

//-----------------------------------------------------------------------------

class OglStock {  // singleton

  typedef std::map<const TDimension, TOfflineGL *, DimensionLess> ContextMap;
  ContextMap m_table;

  OglStock() {}

public:
  ~OglStock() {
    /*  // PER ADESSO, LASCIAMO IL MEMORY LEAK DATO CHE ALTRIMENTI VA IN CRASH
ContextMap::iterator it = m_table.begin();
for(; it!=m_table.end(); ++it)
{
delete it->second;
}
*/
  }

  TOfflineGL *get(const TDimension &d) {
    ContextMap::iterator it = m_table.find(d);
    if (it == m_table.end()) {
      TOfflineGL *glContext;
      glContext = new TOfflineGL(d);
      pair<ContextMap::iterator, bool> result =
          m_table.insert(ContextMap::value_type(d, glContext));
      assert(result.second);
      assert(m_table.size() < 15);
      return glContext;
    }
    return it->second;
  }

  static OglStock *instance() {
    static OglStock singleton;
    return &singleton;
  }
};

}  // namespace

//-----------------------------------------------------------------------------

TOfflineGL *TOfflineGL::getStock(const TDimension dim) {
  return OglStock::instance()->get(dim);
}
