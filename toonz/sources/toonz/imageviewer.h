#pragma once

#ifndef IMAGEVIEWER_INCLUDE
#define IMAGEVIEWER_INCLUDE

#include "toonz/imagepainter.h"
#include "toonzqt/glwidget_for_highdpi.h"

//-----------------------------------------------------------------------------

//  Forward declarations
class FlipBook;
class HistogramPopup;
class QOpenGLFramebufferObject;

//-----------------------------------------------------------------------------

//====================
//    ImageViewer
//--------------------

class ImageViewer final : public GLWidgetForHighDpi {
  Q_OBJECT
  enum DragType {
    eNone,
    eDrawRect  = 0x1,
    eMoveRect  = 0x2,
    eMoveLeft  = 0x4,
    eMoveRight = 0x8,
    eMoveUp    = 0x10,
    eMoveDown  = 0x20
  };
  int m_dragType;
  FlipBook *m_flipbook;
  TPoint m_pressedMousePos;

  Qt::MouseButton m_mouseButton;
  bool m_draggingZoomSelection;

  bool m_rectRGBPick;

  int m_FPS;

  ImagePainter::VisualSettings m_visualSettings;
  ImagePainter::CompareSettings m_compareSettings;

  TImageP m_image;
  TAffine m_viewAff;
  QPoint m_pos;
  bool m_isHistogramEnable;
  HistogramPopup *m_histogramPopup;

  bool m_isColorModel;
  // when fx parameter is modified with showing the fx preview,
  // a flipbook shows a red border line before the rendered result is shown.
  bool m_isRemakingPreviewFx;

  // used for color calibration with 3DLUT
  QOpenGLFramebufferObject *m_fbo = NULL;

  int getDragType(const TPoint &pos, const TRect &loadBox);
  void updateLoadbox(const TPoint &curPos);
  void updateCursor(const TPoint &curPos);

  // for RGB color picker
  void pickColor(QMouseEvent *event, bool putValueToStyleEditor = false);
  void rectPickColor(bool putValueToStyleEditor = false);
  void setPickedColorToStyleEditor(const TPixel32 &color);

public:
  ImageViewer(QWidget *parent, FlipBook *flipbook, bool showHistogram);
  ~ImageViewer();

  void setIsColorModel(bool isColorModel) { m_isColorModel = isColorModel; }
  bool isColorModel() { return m_isColorModel; }

  void setVisual(const ImagePainter::VisualSettings &settings);

  void setImage(TImageP image);
  TImageP getImage() { return m_image; }

  TAffine getViewAff() { return m_viewAff; }
  void setViewAff(TAffine viewAff);

  TAffine getImgToWidgetAffine(const TRectD &imgBounds) const;
  TAffine getImgToWidgetAffine() const;

  /*! If histogram popup exist and is open set its title to "Histogram: level
     name"
                  if level Name is not empty. */
  void setHistogramTitle(QString levelName);
  void setHistogramEnable(bool enable);
  /*! If histogram popup exist and is open, it is set to default and is hidden.
   */
  void hideHistogram();
  void zoomQt(bool forward, bool reset);

  void setIsRemakingPreviewFx(bool on) {
    m_isRemakingPreviewFx = on;
    update();
  }
  bool isRemakingPreviewFx() { return m_isRemakingPreviewFx; }

  void adaptView(const TRect &imgRect, const TRect &viewRect);
  void adaptView(const QRect &viewerRect);

  void doSwapBuffers();
  void changeSwapBehavior(bool enable);

protected:
  void contextMenuEvent(QContextMenuEvent *event) override;
  void initializeGL() override;
  void resizeGL(int width, int height) override;
  void paintGL() override;

  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *) override;

  void panQt(const QPoint &delta);
  void zoomQt(const QPoint &center, double factor);

  void dragCompare(const QPoint &dp);

public slots:

  void updateImageViewer();
  void resetView();
  void fitView();
  void showHistogram();
  void swapCompared();
};

#endif  // IMAGEVIEWER_INCLUDE
