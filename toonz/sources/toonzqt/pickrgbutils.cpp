

#include "tgl.h"

// Qt includes
#include <QPixmap>
#include <QImage>
#include <QOpenGLWidget>
#include <QDesktopWidget>
#include <QApplication>

#include "toonzqt/pickrgbutils.h"

//*************************************************************************************
//    Local namespace
//*************************************************************************************

namespace {

QRgb meanColor(const QImage &img, const QRect &rect) {
  if (rect.width() == 1 && rect.height() == 1)
    return img.pixel(rect.left(), rect.top());

  int r, g, b, m;
  int x, y, x1 = rect.right(), y1 = rect.bottom(),
            count = rect.width() * rect.height();

  r = g = b = m = 0;
  for (y = rect.top(); y <= y1; ++y)
    for (x = rect.left(); x <= x1; ++x) {
      QRgb color = img.pixel(x, y);

      r += (color & 0xff);
      g += ((color >> 8) & 0xff);
      b += ((color >> 16) & 0xff);
      m += ((color >> 24) & 0xff);
    }

  r = r / count;
  g = g / count;
  b = b / count;
  m = m / count;

  return r | (g << 8) | (b << 16) | (m << 24);
}
}

//==============================================================================

QRgb pickRGB(QWidget *widget, const QRect &rect) {
  QImage img(QPixmap::grabWidget(widget, rect.x(), rect.y(), rect.width(),
                                 rect.height())
                 .toImage());
  return meanColor(img, img.rect());
}

//------------------------------------------------------------------------------

QRgb pickScreenRGB(const QRect &rect) {
  QWidget *widget = QApplication::desktop();

#ifdef MACOSX

  //   #Bugzilla 6514, possibly related to #QTBUG 23516

  // Screen grabbing on a secondary screen seems to be broken on MAC
  // (see what happens using PixelTool), when no piece of the *primary*
  // screen is involved in the grab. As it seems to be very Qt-based,
  // the workaround is to trivially grab the smallest rect including the
  // requested one and a part of the primary screen.

  const QRect &screen0Geom = QApplication::desktop()->screenGeometry(0);

  int left   = std::min(rect.left(), screen0Geom.right());
  int top    = std::min(rect.top(), screen0Geom.bottom());
  int right  = std::max(rect.right(), screen0Geom.left());
  int bottom = std::max(rect.bottom(), screen0Geom.right());

  QRect theRect(QPoint(left, top), QPoint(right, bottom));

#else

  const QRect &theRect = rect;

#endif

  QImage img(QPixmap::grabWindow(widget->winId(), theRect.x(), theRect.y(),
                                 theRect.width(), theRect.height())
                 .toImage());
  return meanColor(
      img, QRect(rect.left() - theRect.left(), rect.top() - theRect.top(),
                 rect.width(), rect.height()));
}

//------------------------------------------------------------------------------

QRgb pickRGB(QOpenGLWidget *widget, const QRect &rect) {
  widget->makeCurrent();

  glReadBuffer(GL_FRONT);

  QImage img(rect.size(), QImage::Format_ARGB32);
  glReadPixels(rect.x(), widget->height() - rect.y(), rect.width(),
               rect.height(), GL_BGRA_EXT, GL_UNSIGNED_BYTE, img.bits());

  widget->doneCurrent();

  return meanColor(img, img.rect());
}
