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

#define COMBOHIST_RESOLUTION_W 256
#define COMBOHIST_RESOLUTION_H 100

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// 120405
//-----------------------------------------------------------------------------

class DVAPI ComboHistoRGBLabel final : public QWidget {
  Q_OBJECT
  QColor m_color;

public:
  ComboHistoRGBLabel(QColor color, QWidget *parent);

  ~ComboHistoRGBLabel() {}

  void setColorAndUpdate(QColor color);

protected:
  void paintEvent(QPaintEvent *pe) override;
};

//-----------------------------------------------------------------------------

class DVAPI ChannelHistoGraph : public QWidget {
  Q_OBJECT

  QVector<int> m_values;

  int m_pickedValue;

public:
  int *m_channelValuePtr;

  ChannelHistoGraph(QWidget *parent = 0, int *channelValue = 0);
  ~ChannelHistoGraph();

  virtual void setValues();

  void showCurrentChannelValue(int val);

protected:
  void paintEvent(QPaintEvent *event) override;
};

//-----------------------------------------------------------------------------

class DVAPI RGBHistoGraph final : public ChannelHistoGraph {
  Q_OBJECT

  QVector<int> m_rgbValues[3];

  QImage m_histoImg;

public:
  RGBHistoGraph(QWidget *parent = 0, int *channelValue = 0);
  ~RGBHistoGraph();

  void setValues() override;

protected:
  void paintEvent(QPaintEvent *event) override;
};
//-----------------------------------------------------------------------------

class DVAPI ChannelColorBar final : public QWidget {
  Q_OBJECT
  QColor m_color;

public:
  ChannelColorBar(QWidget *parent = 0, QColor m_color = QColor());
  ~ChannelColorBar() {}

protected:
  void paintEvent(QPaintEvent *event) override;
};

//-----------------------------------------------------------------------------

class DVAPI ChannelHisto final : public QWidget {
  Q_OBJECT

  ChannelHistoGraph *m_histogramGraph;
  ChannelColorBar *m_colorBar;

public:
  ChannelHisto(int channelIndex, int *channelValue, QWidget *parent = 0);
  ~ChannelHisto() {}

  void refleshValue() { m_histogramGraph->setValues(); }

  void showCurrentChannelValue(int val);

protected slots:
  void onShowAlphaButtonToggled(bool visible);
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

public:
  ComboHistogram(QWidget *parent = 0);
  ~ComboHistogram();

  TRasterP getRaster() const { return m_raster; }
  void setRaster(const TRasterP &raster, const TPaletteP &palette = 0);
  void updateInfo(const TPixel32 &pix, const TPointD &imagePos);
  void updateAverageColor(const TPixel32 &pix);

protected:
  void computeChannelsValue();
};

#endif
