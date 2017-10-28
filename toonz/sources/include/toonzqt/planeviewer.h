#pragma once

#ifndef PLANE_VIEWER_H
#define PLANE_VIEWER_H

// TnzCore includes
#include "tcommon.h"
#include "traster.h"
#include "timage.h"

// TnzQt includes
#include "toonzqt/glwidget_for_highdpi.h"

// Qt includes
#include <QOpenGLWidget>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//----------------------------------------------------------------------------

//  Forward declarations
class TRasterImageP;
class TToonzImageP;
class TVectorImageP;

//----------------------------------------------------------------------------

/*!
  \brief    The PlaneViewer class implements a basic OpenGL widget showing a
  plane in a
            standard top-down view, and supports image drawing from the Toonz
  images API.

  \details  A PlaneViewer instance is designed to view bidimensional objects
  layed-out
            on a plane, providing standard mouse interaction functions and some
  built-in
            keyboard shortcuts to provide view navigation.

            The class implements the necessary methods to draw objects on the
  plane -
            prominently, functions to push both world and widget references for
  standard
            OpenGL drawing, conversions between world and widget coordinates,
  and
            efficient image-drawing functions for all Toonz image types.
*/

/*
CAUTION : Changing PlaneViewer to inherit QOpenGLWidget causes crash bug with
shader fx for some unknown reasons. So I will reluctantly keep using the
obsolete class until the shader fx being overhauled. 2016/6/22 Shun
*/

class DVAPI PlaneViewer : public GLWidgetForHighDpi {
public:
  PlaneViewer(QWidget *parent);

  // Background functions
  void setBgColor(const TPixel32 &color1, const TPixel32 &color2);
  void getBgColor(TPixel32 &color1, TPixel32 &color2) const;
  void setChessSize(double size);
  void drawBackground();

  // Image drawing functions
  void draw(TRasterP ras, double dpiX, double dpiY, TPalette *palette = 0);
  void draw(TRasterImageP ri);
  void draw(TToonzImageP ti);
  void draw(TVectorImageP vi);
  void draw(TImageP img);

  // World-Widget conversion functions
  TPointD winToWorld(int x, int y) { return m_aff.inv() * TPointD(x, y); }
  TPointD worldToWin(double x, double y) { return m_aff * TPointD(x, y); }
  TPoint qtWinToWin(int x, int y) { return TPoint(x, height() - y); }
  TPointD qtWinToWorld(int x, int y) {
    return m_aff.inv() * TPointD(x, height() - y);
  }
  TPoint winToQtWin(int x, int y) { return qtWinToWin(x, y); }
  TPointD worldToQtWin(double x, double y) {
    TPointD res(worldToWin(x, y));
    return TPointD(res.x, height() - res.y);
  }

  // World-Widget OpenGL references
  void pushGLWorldCoordinates();
  void pushGLWinCoordinates();
  void popGLCoordinates();

  // View functions
  TAffine &viewAff() { return m_aff; }
  const TAffine &viewAff() const { return m_aff; }

  void resetView();

  void zoomIn();
  void zoomOut();

  void setViewPos(double x, double y);
  void setViewZoom(double x, double y, double zoom);
  void setViewZoom(double zoom) {
    setViewZoom(0.5 * width(), 0.5 * height(), zoom);
  }

  void moveView(double dx, double dy) {
    setViewPos(m_aff.a13 + dx, m_aff.a23 + dy);
  }
  void zoomView(double x, double y, double delta) {
    setViewZoom(x, y, m_aff.a11 * delta);
  }

  void setZoomRange(double zoomMin, double zoomMax);

  // Auxiliary functions
  TRaster32P rasterBuffer();
  void flushRasterBuffer();

protected:
  int m_xpos, m_ypos;  //!< Mouse position on mouse operations.
  TAffine m_aff;       //!< Affine transform from world to widget coords.

  float m_bgColorF[6];  //!< Widget bg color cast in the [0, 1] interval.
  double m_chessSize;   //!< Size of the chessboard squares (default is 40).

  TRaster32P m_rasterBuffer;  //!< Auxiliary buffer used to draw on the widget
                              //! directly.

  double m_zoomRange[2];  //!< Viewport zoom range (default: [-1024, 1024]).

protected:
  virtual void mouseMoveEvent(QMouseEvent *event) override;
  virtual void mousePressEvent(QMouseEvent *event) override;
  virtual void wheelEvent(QWheelEvent *event) override;
  virtual void keyPressEvent(QKeyEvent *event) override;
  virtual void hideEvent(QHideEvent *event) override;

  void initializeGL() override final;
  void resizeGL(int width, int height) override final;

private:
  GLdouble m_matrix[16];
  bool m_firstResize;
  int m_width;
  int m_height;
};

#endif  // PLANE_VIEWER_H
