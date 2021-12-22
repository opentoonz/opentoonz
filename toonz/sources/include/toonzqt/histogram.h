#pragma once

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

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

//=============================================================================
// HistogramGraph
//-----------------------------------------------------------------------------

class DVAPI HistogramGraph final : public QWidget {
  Q_OBJECT

  QColor m_color;
  /*! Height of graph, without margin.*/
  int m_height;

  QVector<int> m_values, m_viewValues, m_logViewValues;
  bool m_logScale;

public:
  static const int drawMargin;

public:
  HistogramGraph(QWidget *parent = 0, QColor m_color = QColor());
  ~HistogramGraph();

  void setAlphaMask(int value);

  void setHeight(int height) { m_height = height; }
  int getHeight() { return m_height; }

  void setValues(const int values[]);
  const QVector<int> &values() const { return m_values; }

  void setLogScale(bool onOff) { m_logScale = onOff; }
  bool logScale() const { return m_logScale; }

  void draw(QPainter *painter, QPoint translation = QPoint(0, 0),
            int size = -1);

protected:
  void paintEvent(QPaintEvent *pe) override;
};

//=============================================================================
// ChannelBar
//-----------------------------------------------------------------------------

class DVAPI ChannelBar final : public QWidget {
  Q_OBJECT

public:
  enum Range { Range_0_255, Range_0_1 };

private:
  QColor m_color;
  int m_colorBarLength;

  bool m_isHorizontal;
  bool m_drawNumbers;

  QColor m_textColor;
  Range m_range;

  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)

  int m_size;

public:
  ChannelBar(QWidget *parent = 0, QColor m_color = QColor(),
             bool isHorizontal = true);
  ~ChannelBar();

  QColor getColor() const { return m_color; }

  void setDrawNumbers(bool onOff);
  bool drawNumbers() const { return m_drawNumbers; }

  void draw(QPainter *painter, QPoint translation = QPoint(0, 0),
            int size = -1);

  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }

  void setLabelRange(Range range) { m_range = range; }

protected:
  void paintEvent(QPaintEvent *event) override;
};

//=============================================================================
// HistogramView
//-----------------------------------------------------------------------------

class DVAPI HistogramView final : public QWidget {
  Q_OBJECT

  HistogramGraph *m_histogramGraph;
  ChannelBar *m_colorBar;

  QWidget *m_drawnWidget;

public:
  HistogramView(QWidget *parent = 0, QColor color = Qt::black);
  ~HistogramView();

  const HistogramGraph *histogramGraph() const { return m_histogramGraph; }
  HistogramGraph *histogramGraph() { return m_histogramGraph; }

  const ChannelBar *channelBar() const { return m_colorBar; }
  ChannelBar *channelBar() { return m_colorBar; }

  QColor getChannelBarColor() const { return m_colorBar->getColor(); }

  // Deve essere fatto prima di chiamare setValues()
  void setGraphHeight(int height) { m_histogramGraph->setHeight(height); }
  void setGraphAlphaMask(int value) { m_histogramGraph->setAlphaMask(value); }
  void setDrawnWidget(QWidget *widget);

  void setValues(const int values[]);
  const QVector<int> &values() const { return m_histogramGraph->values(); }

  void draw(QPainter *painter, QPoint translation = QPoint(0, 0),
            int width = -1);
};

//=============================================================================
// Histograms
//-----------------------------------------------------------------------------

class DVAPI Histograms final : public QStackedWidget {
  Q_OBJECT

  TRasterP m_raster;
  TPaletteP m_palette;  // Necessario per le tlv
  int m_channelValue[6][256];
  int m_channelsCount;
  bool m_computeAlsoRGBA;

public:
  Histograms(QWidget *parent = 0, bool rgba = false);
  ~Histograms();

  TRasterP getRaster() const { return m_raster; }
  void setRaster(const TRasterP &raster, const TPaletteP &palette = 0);

  HistogramView *getHistogramView(int indexType) const;
  int channelsCount() const { return m_channelsCount; }

protected:
  void computeChannelsValue();
};

//=============================================================================
// Histogram
//-----------------------------------------------------------------------------

class DVAPI Histogram final : public QWidget {
  Q_OBJECT

  QComboBox *m_channelsListBox;
  Histograms *m_histograms;

public:
  Histogram(QWidget *parent = 0);
  ~Histogram() {}

  Histograms *getHistograms() const { return m_histograms; }

  TRasterP getRaster() const { return m_histograms->getRaster(); }
  void setRaster(const TRasterP &raster, const TPaletteP &palette = 0);

public slots:

  void setLogScale(bool onOff);

private:
  void updateChannelsList();
};

#endif  // HISTOGRAM_H
