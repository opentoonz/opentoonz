

#include "linetestviewer.h"
#ifdef LINETEST
#include "tapp.h"
#include "viewerdraw.h"
#include "timagecache.h"
#include "tgl.h"
#include "trop.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage2.h"
#include "toonz/stagevisitor.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/imagemanager.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/childstack.h"
#include "toonz/dpiscale.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tcamera.h"
#include "tools/toolhandle.h"
#include "tools/cursormanager.h"
#include "tools/toolcommandids.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/imageutils.h"

#include <QPainter>
#include <QPaintEvent>
#include <QApplication>
#include <QInputContext>
#include <QMenu>

#ifndef USE_QPAINTER
GLshort vertices[] = {0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0};

GLshort coords[] = {0, 1, 0, 0, 1, 0, 1, 1};

// function pointers for PBO Extension
// Windows needs to get function pointers from ICD OpenGL drivers,
// because opengl32.dll does not support extensions higher than v1.1.
#ifdef _WIN32
PFNGLGENBUFFERSARBPROC pglGenBuffersARB = 0;  // VBO Name Generation Procedure
PFNGLBINDBUFFERARBPROC pglBindBufferARB = 0;  // VBO Bind Procedure
PFNGLBUFFERDATAARBPROC pglBufferDataARB = 0;  // VBO Data Loading Procedure
PFNGLBUFFERSUBDATAARBPROC pglBufferSubDataARB =
    0;  // VBO Sub Data Loading Procedure
PFNGLDELETEBUFFERSARBPROC pglDeleteBuffersARB = 0;  // VBO Deletion Procedure
PFNGLGETBUFFERPARAMETERIVARBPROC pglGetBufferParameterivARB =
    0;  // return various parameters of VBO
PFNGLMAPBUFFERARBPROC pglMapBufferARB     = 0;  // map VBO procedure
PFNGLUNMAPBUFFERARBPROC pglUnmapBufferARB = 0;  // unmap VBO procedure
#define glGenBuffersARB pglGenBuffersARB
#define glBindBufferARB pglBindBufferARB
#define glBufferDataARB pglBufferDataARB
#define glBufferSubDataARB pglBufferSubDataARB
#define glDeleteBuffersARB pglDeleteBuffersARB
#define glGetBufferParameterivARB pglGetBufferParameterivARB
#define glMapBufferARB pglMapBufferARB
#define glUnmapBufferARB pglUnmapBufferARB
#endif

#endif  // USE_QPAINTER

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

void initToonzEvent(TMouseEvent &toonzEvent, QMouseEvent *event,
                    int widgetHeight, double pressure, bool isTablet) {
  toonzEvent.m_pos =
      TPoint(event->pos().x(), widgetHeight - 1 - event->pos().y());
  toonzEvent.setModifiers(event->modifiers() & Qt::ShiftModifier,
                          event->modifiers() & Qt::AltModifier,
                          event->modifiers() & Qt::CtrlModifier);

  if (isTablet)
    toonzEvent.m_pressure = (int)(255 * pressure);
  else
    toonzEvent.m_pressure = 255;
}

//-----------------------------------------------------------------------------

class ViewerZoomer final : public ImageUtils::ShortcutZoomer {
public:
  ViewerZoomer(QWidget *parent) : ShortcutZoomer(parent) {}
  void zoom(bool zoomin, bool resetZoom) {
    if (resetZoom)
      ((LineTestViewer *)getWidget())->resetView();
    else
      ((LineTestViewer *)getWidget())->zoomQt(zoomin, resetZoom);
  }
  void fit() {
    //((LineTestViewer*)getWidget())->fitToCamera();
  }
  void setActualPixelSize() {
    //((LineTestViewer*)getWidget())->setActualPixelSize();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

// definito - per ora - in tapp.cpp
extern string updateToolEnableStatus(TTool *tool);

//=============================================================================
// LineTestViewer
//-----------------------------------------------------------------------------

LineTestViewer::LineTestViewer(QWidget *parent)
#ifdef USE_QPAINTER
    : QWidget(parent)
#else
    : QOpenGLWidget(parent)
//, m_pboSupported(false)
#endif  // USE_QPAINTER
    , m_pos()
    , m_mouseButton(Qt::NoButton)
    , m_viewAffine() {
  m_viewAffine = getNormalZoomScale();
  setMouseTracking(true);
}

//-----------------------------------------------------------------------------

LineTestViewer::~LineTestViewer() {
  //#ifndef USE_QPAINTER
  // clean up PBO
  // if(m_pboSupported)
  //	glDeleteBuffersARB(1, &m_pboId);
  //#endif //USE_QPAINTER
}

//-----------------------------------------------------------------------------

TPointD LineTestViewer::winToWorld(const QPoint &pos) const {
  // coordinate window (origine in alto a sinistra) -> coordinate colonna
  // (origine al centro dell'immagine)
  TPointD pp(pos.x() - width() * 0.5, -pos.y() + height() * 0.5);
  return getViewMatrix().inv() * pp;
}

//-----------------------------------------------------------------------------

TPointD LineTestViewer::winToWorld(const TPoint &winPos) const {
  return winToWorld(QPoint(winPos.x, height() - winPos.y));
}
//-----------------------------------------------------------------------------

void LineTestViewer::resetInputMethod() { qApp->inputContext()->reset(); }

//-----------------------------------------------------------------------------

TAffine LineTestViewer::getViewMatrix() const {
  int frame    = TApp::instance()->getCurrentFrame()->getFrame();
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  TAffine aff  = xsh->getCameraAff(frame);
  return m_viewAffine * aff.inv();
}

//-----------------------------------------------------------------------------

#ifdef USE_QPAINTER

void LineTestViewer::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.begin(this);
  p.resetMatrix();

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  p.fillRect(visibleRegion().boundingRect(), QBrush(QColor(Qt::white)));
  TPixel32 cameraBgColor = scene->getProperties()->getBgColor();
  TPixel32 viewerBgColor = scene->getProperties()->getViewerBgColor();
  p.fillRect(visibleRegion().boundingRect(),
             QBrush(QColor(viewerBgColor.r, viewerBgColor.g, viewerBgColor.b,
                           viewerBgColor.m)));

  int h = height();
  int w = width();
  TDimension viewerSize(w, h);
  TRect clipRect(viewerSize);
  clipRect -= TPoint(viewerSize.lx * 0.5, viewerSize.ly * 0.5);

  TAffine viewAff = getViewMatrix();

  Stage::RasterPainter stagePainter(viewerSize, viewAff, clipRect, 0, true);

  TFrameHandle *frameHandle = app->getCurrentFrame();
  if (app->getCurrentFrame()->isEditingLevel()) {
    Stage::visit(stagePainter, app->getCurrentLevel()->getLevel(),
                 app->getCurrentFrame()->getFid(), OnionSkinMask(),
                 frameHandle->isPlaying());
  } else {
    int xsheetLevel = 0;
    TXsheet *xsh    = app->getCurrentXsheet()->getXsheet();
    int frame       = app->getCurrentFrame()->getFrame();

    Stage::VisitArgs args;
    args.scene              = scene;
    args.xsh                = xsh;
    args.row                = frame;
    args.currentColumnIndex = app->getCurrentColumn()->getColumnIndex();
    OnionSkinMask osm       = app->getCurrentOnionSkin()->getOnionSkinMask();
    args.osm                = &osm;
    args.xsheetLevel        = xsheetLevel;
    args.isPlaying          = frameHandle->isPlaying();
    args.onlyVisible        = true;
    args.checkPreviewVisibility = true;
    Stage::visit(stagePainter, args);
  }

  // Draw camera rect and mask
  TRectD rect             = ViewerDraw::getCameraRect();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  int row                 = app->getCurrentFrame()->getFrameIndex();
  double cameraZ          = xsh->getZ(cameraId, row);
  TAffine aff =
      xsh->getPlacement(cameraId, row) * TScale((1000 + cameraZ) / 1000);
  aff = TAffine(1, 0, 0, 0, -1, viewerSize.ly) *
        TTranslation(viewerSize.lx * 0.5, viewerSize.ly * 0.5) * viewAff * aff;
  QMatrix matrix(aff.a11, aff.a21, aff.a12, aff.a22, aff.a13, aff.a23);
  QPolygon cameraPolygon(
      QRect(rect.getP00().x, rect.getP00().y, rect.getLx(), rect.getLy()));
  cameraPolygon = cameraPolygon * matrix;

  p.fillRect(cameraPolygon.boundingRect(),
             QBrush(QColor(cameraBgColor.r, cameraBgColor.g, cameraBgColor.b,
                           cameraBgColor.m)));
  stagePainter.drawRasterImages(p, cameraPolygon);

  p.end();
}

#else

//-----------------------------------------------------------------------------

void LineTestViewer::initializeGL() {
  glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  // checkPBOSupport();

  // Generater pbuffer
  // if(m_pboSupported)
  //  glGenBuffersARB(1, &m_pboId);
}

//-----------------------------------------------------------------------------

void LineTestViewer::resizeGL(int width, int height) {
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, 0, height, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  // To maintain pixel accuracy
  glTranslated(0.375, 0.375, 0.0);
}

//-----------------------------------------------------------------------------

//#include "tstopwatch.h"

void LineTestViewer::paintRaster(TRasterP ras) {
  // TStopWatch timer;
  // TStopWatch timer2;
  // timer.start();
  // timer2.start();

  int w = ras->getLx();
  int h = ras->getLy();

  vertices[1]  = h;
  vertices[6]  = w;
  vertices[9]  = w;
  vertices[10] = h;

  //int dataSize = w * h * 4;

  // timer.stop();
  // timer2.stop();
  // qDebug("time 1: %d",timer.getTotalTime());
  // timer.start(true);
  // timer2.start();

  /****************************************************************************************
ATTENZIONE!!! Qeesto codice potrebbe dare problemi!!!!!
quando si utilizzano le texture le loro dimensioni dovrebbero essere potenze di
dur...
le nuove schede grafiche sembrerebbero funzionare lo stesso ma le vecchie a
volte danno problemi!!!
Inoltre non viene fatto il controllo se le dimenzioni dei raster da disegnare
superano le dimenzioni
supportate dalla scheda grafica... se cio' accade bisognerebbe spezzare il
raster in due...
Questo codice sembra essere meolto veloce (cosa di cui abbiamo bisogno) ma puo'
dare seri problemi!!!!
*****************************************************************************************/

  ras->lock();
  int pixelSize = ras->getPixelSize();
  glGenTextures(1, &m_textureId);
  glBindTexture(GL_TEXTURE_2D, m_textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  if (pixelSize == 4)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 (GLvoid *)0);
  else if (pixelSize == 1) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, w, h, 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, (GLvoid *)0);
  } else
    return;

  // if(m_pboSupported)
  // {
  //   // bind PBO to update pixel values
  //   glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, m_pboId);

  //   // map the buffer object into client's memory
  //   // Note that glMapBufferARB() causes sync issue.
  //   // If GPU is working with this buffer, glMapBufferARB() will wait(stall)
  //   // for GPU to finish its job. To avoid waiting (stall), you can call
  //   // first glBufferDataARB() with NULL pointer before glMapBufferARB().
  //   // If you do that, the previous data in PBO will be discarded and
  //   // glMapBufferARB() returns a new allocated pointer immediately
  //   // even if GPU is still working with the previous data.

  //   glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, dataSize, 0,
  //   GL_STREAM_DRAW_ARB);
  //   GLubyte* ptr = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB,
  //   GL_WRITE_ONLY_ARB);
  //	if(ptr)
  //	{
  // 		ras->lock();
  //		memcpy( ptr, ras->getRawData(), dataSize);
  //		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB); // release pointer
  // to
  // mapping buffer
  //		ras->unlock();
  //   }

  //   // it is good idea to release PBOs with ID 0 after use.
  //   // Once bound with 0, all pixel operations behave normal ways.
  //   //glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
  //
  //   // bind the texture and PBO
  //   glBindTexture(GL_TEXTURE_2D, m_textureId);
  //   glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, m_pboId);
  //   // copy pixels from PBO to texture object
  //   // Use offset instead of ponter.
  //   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE,
  //   0);
  // }
  // else
  // {
  // start to copy pixels from system memory to textrure object
  glBindTexture(GL_TEXTURE_2D, m_textureId);
  if (pixelSize == 4)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE,
                    (GLvoid *)ras->getRawData());
  else if (pixelSize == 1)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, (GLvoid *)ras->getRawData());
  else
    return;
  ras->unlock();
  // }

  glEnable(GL_BLEND);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_SHORT, 0, vertices);
  glTexCoordPointer(2, GL_SHORT, 0, &coords);
  glDrawArrays(GL_QUADS, 0, 4);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  glDisable(GL_BLEND);

  // clean up texture
  glDeleteTextures(1, &m_textureId);

  // timer.stop();
  // timer2.stop();
  // qDebug("time 2: %d",timer.getTotalTime());
  // qDebug("total time: %d",timer2.getTotalTime());
}

//-----------------------------------------------------------------------------

void LineTestViewer::paintGL() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  TPixel32 cameraBgColor = scene->getProperties()->getBgColor();
  TPixel32 viewerBgColor = scene->getProperties()->getViewerBgColor();
  glClearColor(viewerBgColor.r / 255.0, viewerBgColor.g / 255.0,
               viewerBgColor.b / 255.0, viewerBgColor.m / 255.0);
  glClear(GL_COLOR_BUFFER_BIT);

  int h = height();
  int w = width();
  TDimension viewerSize(w, h);
  TRect clipRect(viewerSize);
  // clipRect -= TPoint(viewerSize.lx*0.5,viewerSize.ly*0.5);

  TAffine viewAff = getViewMatrix();

  Stage::RasterPainter stagePainter(viewerSize, viewAff, clipRect, 0, true);

  TFrameHandle *frameHandle = app->getCurrentFrame();
  int frame                 = frameHandle->getFrame();
  if (app->getCurrentFrame()->isEditingLevel()) {
    Stage::visit(stagePainter, app->getCurrentLevel()->getLevel(),
                 frameHandle->getFid(), OnionSkinMask(),
                 frameHandle->isPlaying());
  } else {
    int xsheetLevel = 0;
    TXsheet *xsh    = app->getCurrentXsheet()->getXsheet();

    Stage::VisitArgs args;
    args.scene              = scene;
    args.xsh                = xsh;
    args.row                = frame;
    args.currentColumnIndex = app->getCurrentColumn()->getColumnIndex();
    OnionSkinMask osm       = app->getCurrentOnionSkin()->getOnionSkinMask();
    args.osm                = &osm;
    args.xsheetLevel        = xsheetLevel;
    args.isPlaying          = frameHandle->isPlaying();
    args.onlyVisible        = true;
    args.checkPreviewVisibility = true;
    Stage::visit(stagePainter, args);
  }

  // Draw camera rect and mask
  TRectD cameraRect       = ViewerDraw::getCameraRect();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  int row                 = frameHandle->getFrameIndex();
  double cameraZ          = xsh->getZ(cameraId, row);
  TAffine aff =
      xsh->getPlacement(cameraId, row) * TScale((1000 + cameraZ) / 1000);
  aff = TTranslation(viewerSize.lx * 0.5, viewerSize.ly * 0.5) * viewAff * aff;
  QMatrix matrix(aff.a11, aff.a21, aff.a12, aff.a22, aff.a13, aff.a23);
  QPolygon cameraPolygon(QRect(cameraRect.getP00().x, cameraRect.getP00().y,
                               cameraRect.getLx(), cameraRect.getLy()));
  cameraPolygon = cameraPolygon * matrix;

  tglColor(cameraBgColor);

  glBegin(GL_POLYGON);
  glVertex2d(cameraPolygon.at(0).x(), cameraPolygon.at(0).y());
  glVertex2d(cameraPolygon.at(1).x(), cameraPolygon.at(1).y());
  glVertex2d(cameraPolygon.at(2).x(), cameraPolygon.at(2).y());
  glVertex2d(cameraPolygon.at(3).x(), cameraPolygon.at(3).y());
  glEnd();

  tglColor(TPixel::White);

  int nodesSize = stagePainter.getNodesCount();
  if (nodesSize == 0) return;

  int cameraWidth  = cameraPolygon.at(2).x() - cameraPolygon.at(0).x();
  int cameraHeight = cameraPolygon.at(2).y() - cameraPolygon.at(0).y();

  int i;
  GLfloat mat[4][4];
  QMatrix mtx;
  // GLubyte *pbo;
  // TRaster32P pboRaster;
  // if(m_pboSupported)
  //{
  //	glGenTextures(1, &m_textureId);
  //	glBindTexture(GL_TEXTURE_2D, m_textureId);
  //	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  //	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  //	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  //	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  //	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cameraWidth, cameraHeight, 0,
  // GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid*)0);

  //	glBindTexture(GL_TEXTURE_2D, m_textureId);
  //  glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, m_pboId);
  //   // copy pixels from PBO to texture object
  //   // Use offset instead of ponter.
  //   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, cameraWidth, cameraHeight,
  //   GL_BGRA, GL_UNSIGNED_BYTE, 0);

  //	// bind PBO to update pixel values
  //	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, m_pboId);

  //	// map the buffer object into client's memory
  //	// Note that glMapBufferARB() causes sync issue.
  //	// If GPU is working with this buffer, glMapBufferARB() will wait(stall)
  //	// for GPU to finish its job. To avoid waiting (stall), you can call
  //	// first glBufferDataARB() with NULL pointer before glMapBufferARB().
  //	// If you do that, the previous data in PBO will be discarded and
  //	// glMapBufferARB() returns a new allocated pointer immediately
  //	// even if GPU is still working with the previous data.
  //	glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, cameraWidth * cameraHeight *
  // 4, 0, GL_STREAM_DRAW_ARB);
  //	pbo = (GLubyte*)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB,
  // GL_WRITE_ONLY_ARB);

  //	pboRaster = TRaster32P(cameraWidth, cameraHeight,cameraWidth,
  //(TPixelRGBM32*) pbo, false);
  //	pboRaster->fill(TPixel32::Green);
  //}

  glScissor(cameraPolygon.at(0).x(), cameraPolygon.at(0).y(), cameraWidth,
            cameraHeight);
  glEnable(GL_SCISSOR_TEST);

  // if(!m_pboSupported)
  glPushMatrix();
  for (i = 0; i < nodesSize; i++) {
    TRasterP ras = stagePainter.getRaster(i, mtx);
    if (!ras.getPointer()) continue;

    // if(!m_pboSupported)
    //{
    // Matrix tranformation
    mat[0][0] = mtx.m11();
    mat[0][1] = mtx.m12();
    mat[0][2] = 0;
    mat[0][3] = 0;

    mat[1][0] = mtx.m21();
    mat[1][1] = mtx.m22();
    mat[1][2] = 0;
    mat[1][3] = 0;

    mat[2][0] = 0;
    mat[2][1] = 0;
    mat[2][2] = 1;
    mat[2][3] = 0;

    mat[3][0] = mtx.dx();
    mat[3][1] = mtx.dy();
    mat[3][2] = 0;
    mat[3][3] = 1;

    // Draw Raster
    // CASO NON PBO
    glLoadMatrixf(&mat[0][0]);
    paintRaster(ras);
    //}
    // else
    //{
    //	TAffine aff;//
    //(mtx.m11(),mtx.m12(),mtx.dx(),mtx.m21(),mtx.m22(),mtx.dy());
    //	//TRop::quickPut(pboRaster, ras, aff);
    //}
  }
  // if(!m_pboSupported)
  glPopMatrix();

  // Convert PBO to texture and draw it
  // if(m_pboSupported)
  //{
  //	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

  //	vertices[0] = cameraPolygon.at(0).x();
  //	vertices[1] = cameraPolygon.at(0).y() + cameraHeight;

  //	vertices[3] = cameraPolygon.at(0).x();
  //	vertices[4] = cameraPolygon.at(0).y();

  //	vertices[6] = cameraPolygon.at(0).x() + cameraWidth;
  //	vertices[7] = cameraPolygon.at(0).y();

  //	vertices[9] = cameraPolygon.at(0).x() + cameraWidth;
  //	vertices[10] = cameraPolygon.at(0).y() + cameraHeight;

  //	glBindTexture(GL_TEXTURE_2D, m_textureId);

  //   glEnable(GL_BLEND);
  //	glEnableClientState(GL_VERTEX_ARRAY);
  //	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  //	glVertexPointer(3, GL_SHORT, 0, vertices);
  //	glTexCoordPointer(2,GL_SHORT,0,&coords);
  //	glDrawArrays(GL_QUADS, 0, 4);
  //	glDisableClientState(GL_VERTEX_ARRAY);
  //	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  //	glDisable(GL_BLEND);

  //  // clean up texture
  //	glDeleteTextures(1, &m_textureId);
  //}

  glDisable(GL_SCISSOR_TEST);

  stagePainter.clearNodes();
}

#endif  // USE_QPAINTER

//-----------------------------------------------------------------------------

void LineTestViewer::showEvent(QShowEvent *) {
  TApp *app = TApp::instance();

  bool ret = true;

  ret = ret && connect(TApp::instance()->getCurrentObject(),
                       SIGNAL(objectSwitched()), this, SLOT(update()));
  ret = ret && connect(TApp::instance()->getCurrentObject(),
                       SIGNAL(objectChanged(bool)), this, SLOT(update()));

  ret = ret && connect(TApp::instance()->getCurrentOnionSkin(),
                       SIGNAL(onionSkinMaskChanged()), this, SLOT(update()));

  ret = ret && connect(TApp::instance()->getCurrentLevel(),
                       SIGNAL(xshLevelChanged()), this, SLOT(update()));

  TFrameHandle *frameHandle = app->getCurrentFrame();
  ret                       = ret &&
        connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(update()));

  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  ret                         = ret &&
        connect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(update()));
  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetSwitched()), this, SLOT(update()));

  TSceneHandle *sceneHandle = app->getCurrentScene();
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));
  ret =
      ret && connect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(update()));

  ret = ret && connect(TApp::instance()->getCurrentOnionSkin(),
                       SIGNAL(onionSkinMaskChanged()), this, SLOT(update()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void LineTestViewer::hideEvent(QHideEvent *e) {
  TApp *app = TApp::instance();

  disconnect(TApp::instance()->getCurrentObject(), SIGNAL(objectSwitched()),
             this, SLOT(update()));
  disconnect(TApp::instance()->getCurrentObject(), SIGNAL(objectChanged(bool)),
             this, SLOT(update()));

  disconnect(TApp::instance()->getCurrentOnionSkin(),
             SIGNAL(onionSkinMaskChanged()), this, SLOT(update()));

  disconnect(TApp::instance()->getCurrentLevel(), SIGNAL(xshLevelChanged()),
             this, SLOT(update()));

  TFrameHandle *frameHandle = app->getCurrentFrame();
  disconnect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(update()));

  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  disconnect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(update()));
  disconnect(xsheetHandle, SIGNAL(xsheetSwitched()), this, SLOT(update()));

  TSceneHandle *sceneHandle = app->getCurrentScene();
  disconnect(sceneHandle, SIGNAL(sceneSwitched()), this,
             SLOT(onSceneSwitched()));
  disconnect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(update()));

  disconnect(TApp::instance()->getCurrentOnionSkin(),
             SIGNAL(onionSkinMaskChanged()), this, SLOT(update()));

  QWidget::hideEvent(e);
}

//-----------------------------------------------------------------------------

void LineTestViewer::leaveEvent(QEvent *) {
  // update();
  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------

void LineTestViewer::enterEvent(QEvent *) {
  QApplication::setOverrideCursor(Qt::ForbiddenCursor);
  // update();
  setFocus();
}

//-----------------------------------------------------------------------------

void LineTestViewer::mouseMoveEvent(QMouseEvent *event) {
  if (m_mouseButton == Qt::MidButton) {
    // panning
    QPoint curPos = event->pos();
    panQt(curPos - m_pos);
    m_pos = curPos;
    return;
  }
}

//-----------------------------------------------------------------------------

void LineTestViewer::mousePressEvent(QMouseEvent *event) {
  m_pos         = event->pos();
  m_mouseButton = event->button();
}

//-----------------------------------------------------------------------------

void LineTestViewer::mouseReleaseEvent(QMouseEvent *event) {
  m_pos         = QPoint(0, 0);
  m_mouseButton = Qt::NoButton;
}

//-----------------------------------------------------------------------------

void LineTestViewer::wheelEvent(QWheelEvent *e) {
  if (e->orientation() == Qt::Horizontal) return;
  int delta = e->delta() > 0 ? 120 : -120;
  zoomQt(e->pos(), exp(0.001 * delta));
}

//-----------------------------------------------------------------------------

// delta.x: right panning, pixel; delta.y: down panning, pixel
void LineTestViewer::panQt(const QPoint &delta) {
  if (delta == QPoint()) return;
  m_viewAffine = TTranslation(delta.x(), -delta.y()) * m_viewAffine;
  update();
}

//-----------------------------------------------------------------------------

void LineTestViewer::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu  = new QMenu(this);
  QAction *act = menu->addAction("Reset View");
  act->setShortcut(
      QKeySequence(CommandManager::instance()->getKeyFromId(T_ZoomReset)));
  connect(act, SIGNAL(triggered()), this, SLOT(resetView()));
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void LineTestViewer::keyPressEvent(QKeyEvent *event) {
  ViewerZoomer(this).exec(event);
}

//-----------------------------------------------------------------------------

// center: window coordinate, pixels, topleft origin
void LineTestViewer::zoomQt(const QPoint &center, double factor) {
  if (factor == 1.0) return;
  TPointD delta(center.x() - width() * 0.5, -center.y() + height() * 0.5);
  double scale2 = fabs(m_viewAffine.det());
  if ((scale2 < 100000 || factor < 1) && (scale2 > 0.001 * 0.05 || factor > 1))
    m_viewAffine = TTranslation(delta) * TScale(factor) * TTranslation(-delta) *
                   m_viewAffine;
  update();
  emit onZoomChanged();
}

//-----------------------------------------------------------------------------

void LineTestViewer::zoomQt(bool forward, bool reset) {
  double normalZoom = sqrt(getNormalZoomScale().det());
  double scale2     = m_viewAffine.det();
  if (reset ||
      ((scale2 < 256 || !forward) && (scale2 > 0.001 * 0.05 || forward))) {
    double oldZoomScale = sqrt(scale2);
    double zoomScale    = reset ? 1 : ImageUtils::getQuantizedZoomFactor(
                                       oldZoomScale / normalZoom, forward);
    m_viewAffine = TScale(zoomScale * normalZoom / oldZoomScale) * m_viewAffine;
  }
  update();
  emit onZoomChanged();
}

//-----------------------------------------------------------------------------

void LineTestViewer::onFrameSwitched() { update(); }

//-----------------------------------------------------------------------------

void LineTestViewer::onXsheetChanged() { update(); }

//-----------------------------------------------------------------------------

void LineTestViewer::onSceneSwitched() { resetView(); }

//-----------------------------------------------------------------------------

void LineTestViewer::resetView() {
  m_viewAffine = getNormalZoomScale();
  update();
  emit onZoomChanged();
}

//-----------------------------------------------------------------------------

void LineTestViewer::onButtonPressed(FlipConsole::EGadget button) {
  switch (button) {
  case FlipConsole::eFilledRaster:
    TXshSimpleLevel::m_fillFullColorRaster =
        !TXshSimpleLevel::m_fillFullColorRaster;
    update();
    break;
  }
}

//-----------------------------------------------------------------------------

TAffine LineTestViewer::getNormalZoomScale() {
  TApp *app         = TApp::instance();
  const double inch = Stage::inch;
  TCamera *camera   = app->getCurrentScene()->getScene()->getCurrentCamera();
  double size;
  int res;
  if (camera->isXPrevalence()) {
    size = camera->getSize().lx;
    res  = camera->getRes().lx;
  } else {
    size = camera->getSize().ly;
    res  = camera->getRes().ly;
  }

  TScale cameraScale(inch * size / res);
  return cameraScale.inv();
}

#endif
