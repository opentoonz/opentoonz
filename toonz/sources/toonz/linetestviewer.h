#pragma once

#ifndef LINETESTVIEWER_H
#define LINETESTVIEWER_H

#ifdef LINETEST

//#define USE_QPAINTER

#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"

#include "toonzqt/flipconsole.h"

#ifdef USE_QPAINTER
#include <QWidget>
#else
#include <QOpenGLWidget>
#endif

//-----------------------------------------------------------------------------
//	LineTestViewer
// Andrebbe messo a fattor comune del codice con SceneViewer

#ifdef USE_QPAINTER
class LineTestViewer : public QWidget
#else
class LineTestViewer : public QOpenGLWidget
#endif
{
  Q_OBJECT

  QPoint m_pos;
  Qt::MouseButton m_mouseButton;

#ifndef USE_QPAINTER
  // bool m_pboSupported;
  GLuint m_textureId;
  GLuint m_pboId;
#endif

  //! Used to zoom and pan
  TAffine m_viewAffine;

public:
  LineTestViewer(QWidget *parent = 0);
  ~LineTestViewer();

  TPointD LineTestViewer::winToWorld(const QPoint &pos) const;
  // note: winPos in pixel, top-left origin;
  // when no camera movements have been defined then worldPos = 0 at camera
  // center
  TPointD winToWorld(const TPoint &winPos) const;

  // overriden from TTool::Viewer
  void pan(const TPoint &delta) { panQt(QPoint(delta.x, delta.y)); }
  // overriden from TTool::Viewer
  void zoom(const TPointD &center, double factor) {
    zoomQt(QPoint(center.x, height() - center.y), factor);
  }

  void resetInputMethod();

  void setFocus() { QWidget::setFocus(); };

  TAffine getViewMatrix() const;

  TAffine getNormalZoomScale();

  void zoomQt(bool forward, bool reset);

protected:
#ifdef USE_QPAINTER
  void paintEvent(QPaintEvent *);
#else
  void paintGL();
  void initializeGL();
  void resizeGL(int width, int height);
  // void checkPBOSupport();
  void paintRaster(TRasterP ras);
#endif

  void showEvent(QShowEvent *);
  void hideEvent(QHideEvent *);

  void leaveEvent(QEvent *);
  void enterEvent(QEvent *);
  void mouseMoveEvent(QMouseEvent *event);

  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void wheelEvent(QWheelEvent *e);
  void contextMenuEvent(QContextMenuEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void panQt(const QPoint &delta);
  void zoomQt(const QPoint &center, double factor);

public slots:
  void onButtonPressed(FlipConsole::EGadget button);
  void resetView();

protected slots:
  void onFrameSwitched();
  void onXsheetChanged();
  void onSceneSwitched();
signals:
  void onZoomChanged();
};

#endif  // LINETEST

#endif  // LINETESTVIEWER_H
