#pragma once
#ifndef SEPARATECOLORSSWATCH_H
#define SEPARATECOLORSSWATCH_H

#include "traster.h"
#include <QWidget>

class QAction;
class QLabel;

enum SWATCH_TYPE { Original, Main, Sub1, Sub2, Sub3 };

class SeparateSwatch;

class SeparateSwatchArea : public QWidget {
  Q_OBJECT
  QPoint m_pos;
  SWATCH_TYPE m_type;
  bool m_panning;
  SeparateSwatch* m_sw;
  TRaster32P m_r;
  TAffine getFinalAff();

  TPixel32 m_transparentColor;

public:
  SeparateSwatchArea(SeparateSwatch* parent, SWATCH_TYPE type);
  void updateRaster(bool dragging = false);
  void updateSeparated(bool dragging = false);
  void setTranspColor(TPixel32& lineColor, bool showAlpha = false);

protected:
  void paintEvent(QPaintEvent* event);
  void mousePressEvent(QMouseEvent* event);
  void mouseMoveEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);
  void wheelEvent(QWheelEvent* event);
  void keyPressEvent(QKeyEvent* event);
  // focus on mouse enter
  void enterEvent(QEvent* event);
};

class SeparateSwatch : public QWidget {
  Q_OBJECT

public:
  int m_lx, m_ly;
  bool m_enabled;
  SeparateSwatchArea* m_orgSwatch;
  SeparateSwatchArea* m_mainSwatch;
  SeparateSwatchArea* m_sub1Swatch;
  SeparateSwatchArea* m_sub2Swatch;
  SeparateSwatchArea* m_sub3Swatch;

  QLabel* m_sub3Label;

  TAffine m_viewAff;
  TRasterP m_orgRaster;
  TRasterP m_mainRaster;
  TRasterP m_sub1Raster;
  TRasterP m_sub2Raster;
  TRasterP m_sub3Raster;

  SeparateSwatch(QWidget* parent, int lx, int ly);

  void setRaster(TRasterP orgRas, TRasterP mainRas, TRasterP sub1Ras,
                 TRasterP sub2Ras);
  void setRaster(TRasterP orgRas, TRasterP mainRas, TRasterP sub1Ras,
                 TRasterP sub2Ras, TRasterP sub3Ras);
  void updateSeparated();
  bool isEnabled();
  void enable(bool state);
  void setTranspColors(TPixel32& mainCol, TPixel32& sub1Col, TPixel32& sub2Col,
                       TPixel32& sub3Col, bool showAlpha = false);

protected:
  void resizeEvent(QResizeEvent* event);

protected slots:
  void showSub3Swatch(bool);

signals:
  void enabled();
};

#endif