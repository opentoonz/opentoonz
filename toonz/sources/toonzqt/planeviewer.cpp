

#include "toonzqt/planeviewer.h"

// TnzQt includes
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/stage.h"

// TnzCore includes
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tvectorimage.h"
#include "trop.h"
#include "tvectorrenderdata.h"
#include "tgl.h"
#include "tvectorgl.h"
#include "tpalette.h"

// Qt includes
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QHideEvent>

//#define PRINT_AFF

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

struct PlaneViewerZoomer final : public ImageUtils::ShortcutZoomer {
  PlaneViewerZoomer(PlaneViewer *planeViewer) : ShortcutZoomer(planeViewer) {}

private:
  bool zoom(bool zoomin, bool resetZoom) override;
};

//========================================================================

bool PlaneViewerZoomer::zoom(bool zoomin, bool resetZoom) {
  PlaneViewer &planeViewer = static_cast<PlaneViewer &>(*getWidget());

  resetZoom ? planeViewer.resetView()
            : zoomin ? planeViewer.zoomIn() : planeViewer.zoomOut();

  return true;
}

}  // namespace

//=========================================================================================

PlaneViewer::PlaneViewer(QWidget *parent)
    : GLWidgetForHighDpi(parent)
    , m_firstResize(true)
    , m_xpos(0)
    , m_ypos(0)
    , m_aff()  // initialized at the first resize
    , m_chessSize(40.0) {
  m_zoomRange[0] = 1e-3, m_zoomRange[1] = 1024.0;
  setBgColor(TPixel32(235, 235, 235), TPixel32(235, 235, 235));
}

//=========================================================================================

void PlaneViewer::setZoomRange(double zoomMin, double zoomMax) {
  m_zoomRange[0] = zoomMin, m_zoomRange[1] = zoomMax;
}

//------------------------------------------------------------------------

void PlaneViewer::setBgColor(const TPixel32 &color1, const TPixel32 &color2) {
  m_bgColorF[0] = color1.r / 255.0, m_bgColorF[1] = color1.g / 255.0,
  m_bgColorF[2] = color1.b / 255.0;
  m_bgColorF[3] = color2.r / 255.0, m_bgColorF[4] = color2.g / 255.0,
  m_bgColorF[5] = color2.b / 255.0;
}

//------------------------------------------------------------------------

void PlaneViewer::getBgColor(TPixel32 &color1, TPixel32 &color2) const {
  color1.r = m_bgColorF[0] * 255.0, color1.g = m_bgColorF[1] * 255.0,
  color1.b = m_bgColorF[2] * 255.0;
  color2.r = m_bgColorF[3] * 255.0, color2.g = m_bgColorF[4] * 255.0,
  color2.b = m_bgColorF[5] * 255.0;
}

//------------------------------------------------------------------------

void PlaneViewer::drawBackground() {
  glClearColor(m_bgColorF[0], m_bgColorF[1], m_bgColorF[2], 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (m_bgColorF[0] != m_bgColorF[3] || m_bgColorF[1] != m_bgColorF[4] ||
      m_bgColorF[2] != m_bgColorF[5]) {
    // Cast the widget rect to world rect
    TRectD rect(winToWorld(0, 0), winToWorld(width(), height()));

    // Deduce chess geometry
    TRect chessRect(tfloor(rect.x0 / m_chessSize),
                    tfloor(rect.y0 / m_chessSize), tceil(rect.x1 / m_chessSize),
                    tceil(rect.y1 / m_chessSize));

    // Draw chess squares
    glColor3f(m_bgColorF[3], m_bgColorF[4], m_bgColorF[5]);
    glBegin(GL_QUADS);

    int x, y;
    TPointD pos;
    double chessSize2 = 2.0 * m_chessSize;

    for (y = chessRect.y0; y < chessRect.y1; ++y) {
      pos.y = y * m_chessSize;
      for (x = chessRect.x0 + ((chessRect.x0 + y) % 2), pos.x = x * m_chessSize;
           x < chessRect.x1; x += 2, pos.x += chessSize2) {
        glVertex2d(pos.x, pos.y);
        glVertex2d(pos.x + m_chessSize, pos.y);
        glVertex2d(pos.x + m_chessSize, pos.y + m_chessSize);
        glVertex2d(pos.x, pos.y + m_chessSize);
      }
    }

    glEnd();
  }
}

//=========================================================================================

void PlaneViewer::initializeGL() { initializeOpenGLFunctions(); }

void PlaneViewer::resizeGL(int width, int height) {
  width *= getDevPixRatio();
  height *= getDevPixRatio();
  glViewport(0, 0, width, height);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, width, 0, height);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  if (m_firstResize) {
    m_firstResize = false;
    m_aff         = TTranslation(0.5 * width, 0.5 * height);
    m_width       = width;
    m_height      = height;
  } else {
    TPointD oldCenter(m_width * 0.5, m_height * 0.5);
    TPointD newCenter(width * 0.5, height * 0.5);

    m_aff    = m_aff.place(m_aff.inv() * oldCenter, newCenter);
    m_width  = width;
    m_height = height;
  }
}

//=========================================================================================

void PlaneViewer::mouseMoveEvent(QMouseEvent *event) {
  QPoint curPos = event->pos() * getDevPixRatio();
  if (event->buttons() & Qt::MidButton)
    moveView(curPos.x() - m_xpos, height() - curPos.y() - m_ypos);

  m_xpos = curPos.x(), m_ypos = height() - curPos.y();
}

//------------------------------------------------------

void PlaneViewer::mousePressEvent(QMouseEvent *event) {
  m_xpos = event->x() * getDevPixRatio();
  m_ypos = height() - event->y() * getDevPixRatio();
}

//------------------------------------------------------

void PlaneViewer::wheelEvent(QWheelEvent *event) {
  TPointD pos(event->x() * getDevPixRatio(),
              height() - event->y() * getDevPixRatio());
  double zoom_par = 1 + event->delta() * 0.001;

  zoomView(pos.x, pos.y, zoom_par);
}

//------------------------------------------------------

void PlaneViewer::keyPressEvent(QKeyEvent *event) {
  if (PlaneViewerZoomer(this).exec(event)) return;

  QOpenGLWidget::keyPressEvent(event);
}

//------------------------------------------------------

//! Disposes of the auxiliary internal rasterBuffer().
void PlaneViewer::hideEvent(QHideEvent *event) {
  m_rasterBuffer = TRaster32P();
}

//=========================================================================================

void PlaneViewer::resetView() {
  m_aff = TTranslation(0.5 * width(), 0.5 * height());
  update();
}

//------------------------------------------------------

void PlaneViewer::setViewPos(double x, double y) {
  m_aff.a13 = x, m_aff.a23 = y;
  update();

#ifdef PRINT_AFF
  qDebug("zoom = %.4f;  pos = %.4f, %.4f", m_aff.a11, m_aff.a13, m_aff.a23);
#endif
}

//------------------------------------------------------

void PlaneViewer::setViewZoom(double x, double y, double zoom) {
  zoom         = tcrop(zoom, m_zoomRange[0], m_zoomRange[1]);
  double delta = zoom / m_aff.a11;

  m_aff.a13 = x + delta * (m_aff.a13 - x);
  m_aff.a23 = y + delta * (m_aff.a23 - y);
  m_aff.a11 = m_aff.a22 = zoom;

  update();

#ifdef PRINT_AFF
  qDebug("zoom = %.4f;  pos = %.4f, %.4f", m_aff.a11, m_aff.a13, m_aff.a23);
#endif
}

//------------------------------------------------------

void PlaneViewer::zoomIn() {
  setViewZoom(ImageUtils::getQuantizedZoomFactor(m_aff.a11, true));
}

//------------------------------------------------------

void PlaneViewer::zoomOut() {
  setViewZoom(ImageUtils::getQuantizedZoomFactor(m_aff.a11, false));
}

//=========================================================================================

void PlaneViewer::pushGLWorldCoordinates() {
  m_matrix[0]  = m_aff.a11;
  m_matrix[4]  = m_aff.a12;
  m_matrix[12] = m_aff.a13;
  m_matrix[1]  = m_aff.a21;
  m_matrix[5]  = m_aff.a22;
  m_matrix[13] = m_aff.a23;

  m_matrix[2] = m_matrix[3] = m_matrix[6] = m_matrix[7] = m_matrix[8] =
      m_matrix[9] = m_matrix[10] = m_matrix[11] = m_matrix[14] = 0;

  m_matrix[15] = 1.0;

  glPushMatrix();
  glLoadMatrixd(m_matrix);  //(GLdouble*) &m_matrix
}

//------------------------------------------------------

void PlaneViewer::pushGLWinCoordinates() {
  glPushMatrix();
  glLoadIdentity();
}

//------------------------------------------------------

void PlaneViewer::popGLCoordinates() { glPopMatrix(); }

//=========================================================================================

TRaster32P PlaneViewer::rasterBuffer() {
  if (!m_rasterBuffer || m_rasterBuffer->getLx() != width() ||
      m_rasterBuffer->getLy() != height())
    m_rasterBuffer = TRaster32P(width(), height());

  return m_rasterBuffer;
}

//------------------------------------------------------

void PlaneViewer::flushRasterBuffer() {
  assert(m_rasterBuffer);

  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glRasterPos2d(0, 0);
  glDrawPixels(width(), height(), TGL_FMT, TGL_TYPE,
               m_rasterBuffer->getRawData());
}

//=========================================================================================

void PlaneViewer::draw(TRasterP ras, double dpiX, double dpiY, TPalette *pal) {
  TPointD rasCenter(ras->getCenterD());

  TRaster32P aux(rasterBuffer());

  aux->lock();
  ras->lock();

  glGetDoublev(GL_MODELVIEW_MATRIX, m_matrix);
  TAffine viewAff(m_matrix[0], m_matrix[4], m_matrix[12], m_matrix[1],
                  m_matrix[5], m_matrix[13]);
  viewAff = viewAff * TScale(Stage::inch / dpiX, Stage::inch / dpiY) *
            TTranslation(-rasCenter);

  pushGLWinCoordinates();

  aux->clear();
  if (pal)
    TRop::quickPut(aux, (TRasterCM32P)ras, pal, viewAff);
  else
    TRop::quickPut(aux, ras, viewAff);

  flushRasterBuffer();
  popGLCoordinates();
}

/*NOTE:
    glRasterPos2d could be used, along glBitmap and glPixelZoom...
    however, i've never been able to use them effectively...
*/

//------------------------------------------------------

void PlaneViewer::draw(TRasterImageP ri) {
  double dpiX, dpiY;
  ri->getDpi(dpiX, dpiY);

  if (dpiX == 0.0 || dpiY == 0.0) dpiX = dpiY = Stage::inch;

  draw(ri->getRaster(), dpiX, dpiY);
}

//------------------------------------------------------

void PlaneViewer::draw(TToonzImageP ti) {
  double dpiX, dpiY;
  ti->getDpi(dpiX, dpiY);

  if (dpiX == 0.0 || dpiY == 0.0) dpiX = dpiY = Stage::inch;

  draw(ti->getRaster(), dpiX, dpiY, ti->getPalette());
}

//------------------------------------------------------

void PlaneViewer::draw(TVectorImageP vi) {
  TRectD bbox(vi->getBBox());
  TRect bboxI(tfloor(bbox.x0), tfloor(bbox.y0), tceil(bbox.x1) - 1,
              tceil(bbox.y1) - 1);

  TVectorRenderData rd(TAffine(), bboxI, vi->getPalette(), 0, true, true);
  tglDraw(rd, vi.getPointer());
}

//------------------------------------------------------

void PlaneViewer::draw(TImageP img) {
  {
    TRasterImageP ri(img);
    if (ri) {
      draw(ri);
      return;
    }
  }
  {
    TToonzImageP ti(img);
    if (ti) {
      draw(ti);
      return;
    }
  }
  {
    TVectorImageP vi(img);
    if (vi) {
      draw(vi);
      return;
    }
  }
}
