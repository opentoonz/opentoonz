#pragma once

#include "traster.h"
#include <QWidget>

// class QPushButton;
class QAction;
class CleanupSwatch final : public QWidget {
  Q_OBJECT

  class CleanupSwatchArea final : public QWidget {
    QPoint m_pos;
    bool m_isLeft, m_panning;
    CleanupSwatch *m_sw;
    TRaster32P m_r;
    TAffine getFinalAff();

  public:
    CleanupSwatchArea(CleanupSwatch *parent, bool isLeft);
    void updateRaster(bool dragging = false);
    void updateCleanupped(bool dragging = false);

  protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
  };

  int m_lx, m_ly;
  bool m_enabled;
  CleanupSwatchArea *m_leftSwatch;
  CleanupSwatchArea *m_rightSwatch;

  TAffine m_viewAff, m_resampleAff;
  TRasterP m_resampledRaster;
  TRasterP m_origRaster;
  // TRasterP m_lastRasCleanupped;
  // TPointD  m_lastCleanuppedPos;
public:
  CleanupSwatch(QWidget *parent, int lx, int ly);

  void setRaster(TRasterP rasLeft, const TAffine &aff, TRasterP ras);
  void updateCleanupped();
  bool isEnabled();
  void enable(bool state);
  // void enableRightSwatch(bool state);
protected:
  // void hideEvent(QHideEvent* e);
  void resizeEvent(QResizeEvent *event) override;

signals:
  void enabled();
};
