#pragma once

#ifndef COMBOHISTOGRAM_H
#define COMBOHISTOGRAM_H

#include "tcommon.h"
#include "traster.h"
#include "ttoonzimage.h"
#include "tpalette.h"

#include <QWidget>
#include <QStackedWidget>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4251)
#endif

class QStackedWidget;
class QString;
class QComboBox;
class QColor;

class RGBLabel;
class QLabel;
class QPushButton;

#define COMBOHIST_RESOLUTION_W 256
#define COMBOHIST_RESOLUTION_H 100

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// 120405
//-----------------------------------------------------------------------------

class DVAPI ComboHistoRGBLabel final : public QWidget {
  Q_OBJECT
public:
  enum DisplayMode { Display_8bit = 0, Display_16bit, Display_0_1 };

private:
  QColor m_color;

  DisplayMode m_mode;
  bool m_alphaVisible;

public:
  ComboHistoRGBLabel(QColor color, QWidget *parent);

  ~ComboHistoRGBLabel() {}

  void setColorAndUpdate(QColor color);
  void setDisplayMode(DisplayMode mode) { m_mode = mode; }
  void setAlphaVisible(bool visible) { m_alphaVisible = visible; }

protected:
  void paintEvent(QPaintEvent *pe) override;
};

//-----------------------------------------------------------------------------

class DVAPI ChannelHistoGraph : public QWidget {
  Q_OBJECT

  QVector<int> m_values[2];  // 0: current raster 1: snapshot
  int m_maxValue[2];

  int m_pickedValue;
  int m_channelIndex;
  float m_range;

public:
  bool *m_showComparePtr;

  ChannelHistoGraph(int index, QWidget *parent = nullptr,
                    bool *showComparePtr = nullptr);
  ~ChannelHistoGraph();

  virtual void setValues(int *buf, bool isComp);

  void showCurrentChannelValue(int val);
  void setRange(float range) { m_range = range; }

protected:
  void paintEvent(QPaintEvent *event) override;
};

//-----------------------------------------------------------------------------

class DVAPI RGBHistoGraph final : public ChannelHistoGraph {
  Q_OBJECT

  QVector<int> m_rgbValues[3];

  QImage m_histoImg;

public:
  RGBHistoGraph(int index, QWidget *parent = 0);
  ~RGBHistoGraph();

  void setValues(int *buf, bool isComp) override;

protected:
  void paintEvent(QPaintEvent *event) override;
};
//-----------------------------------------------------------------------------

class DVAPI ChannelColorBar final : public QWidget {
  Q_OBJECT
  QColor m_color;
  float m_range;

public:
  ChannelColorBar(QWidget *parent = 0, QColor m_color = QColor());
  ~ChannelColorBar() {}
  void setRange(float range) { m_range = range; }

protected:
  void paintEvent(QPaintEvent *event) override;
};

//-----------------------------------------------------------------------------

class DVAPI ChannelHisto final : public QWidget {
  Q_OBJECT

  ChannelHistoGraph *m_histogramGraph;
  ChannelColorBar *m_colorBar;

public:
  ChannelHisto(int channelIndex, bool *showComparePtr, QWidget *parent = 0);
  ~ChannelHisto() {}

  void refleshValue(int *buf, bool isComp = false) {
    m_histogramGraph->setValues(buf, isComp);
  }

  void showCurrentChannelValue(int val);

  void setRange(float range) {
    m_histogramGraph->setRange(range);
    m_colorBar->setRange(range);
  }

protected slots:
  void onShowAlphaButtonToggled(bool visible);

signals:
  void showButtonToggled(bool);
};

//-----------------------------------------------------------------------------

class DVAPI ComboHistogram final : public QWidget {
  Q_OBJECT

  TRasterP m_raster;
  TPaletteP m_palette;

  // rgba channels
  int m_channelValue[4][COMBOHIST_RESOLUTION_W];

  // rgba channels + composited
  ChannelHisto *m_histograms[5];

  ComboHistoRGBLabel *m_rgbLabel;

  ComboHistoRGBLabel *m_rectAverageRgbLabel;

  QLabel *m_xPosLabel;
  QLabel *m_yPosLabel;

  // graph range control (available only with TRasterF)
  QWidget *m_rangeControlContainer;
  QPushButton *m_rangeUpBtn, *m_rangeDwnBtn;
  QLabel *m_rangeLabel;
  int m_rangeStep;  // 0 = 1.0, 1 = 2.0, 2 = 4.0, 3 = 8.0...

  QComboBox *m_displayModeCombo;

  bool m_showCompare;
  bool m_compHistoIsValid;
  int m_channelValueComp[4][COMBOHIST_RESOLUTION_W];

public:
  ComboHistogram(QWidget *parent = 0);
  ~ComboHistogram();

  TRasterP getRaster() const { return m_raster; }
  void setRaster(const TRasterP &raster, const TPaletteP &palette = 0);
  void updateInfo(const TPixel32 &pix, const TPointD &imagePos);
  void updateInfo(const TPixel64 &pix, const TPointD &imagePos);
  void updateInfo(const TPixelF &pix, const TPointD &imagePos);
  void updateAverageColor(const TPixel32 &pix);
  void updateAverageColor(const TPixel64 &pix);
  void updateAverageColor(const TPixelF &pix);
  void updateCompHistogram();

  void setShowCompare(bool on) {
    m_showCompare = on;
    if (isVisible() && !m_compHistoIsValid) updateCompHistogram();
  }
  void invalidateCompHisto() {
    m_compHistoIsValid = false;
    if (isVisible() && m_showCompare) updateCompHistogram();
  }

  void refreshHistogram();

protected:
  void computeChannelsValue(int *buf, size_t size, TRasterP ras,
                            TPalette *extPlt = nullptr);
  void showEvent(QShowEvent *) override;

protected slots:
  void onDisplayModeChanged();
  void onShowAlphaButtonToggled(bool);
  void onRangeUp();
  void onRangeDown();
};

#endif
