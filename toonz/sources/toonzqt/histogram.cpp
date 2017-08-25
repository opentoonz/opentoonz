

#include "tcolorstyles.h"

#include <QVBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QIcon>
#include <QStackedWidget>
#include <QPainter>

#include "toonzqt/histogram.h"
#include "toonzqt/gutil.h"

const int HistogramGraph::drawMargin = 10;

//*****************************************************************************
//    Local namespace
//*****************************************************************************

namespace {

void computeGreyValues(const TRasterGR8P &ras, int *greyValues) {
  int y, lx = ras->getLx(), ly = ras->getLy();
  TPixelGR8 *pix, *endPix;

  for (y = 0; y < ly; ++y)
    for (pix = ras->pixels(y), endPix = pix + lx; pix < endPix; ++pix)
      ++greyValues[pix->value];
}

//-----------------------------------------------------------------------------

void computeGreyValues(const TRasterGR16P &ras, int *greyValues) {
  int y, lx = ras->getLx(), ly = ras->getLy();
  TPixelGR16 *pix, *endPix;

  for (y = 0; y < ly; ++y)
    for (pix = ras->pixels(y), endPix = pix + lx; pix < endPix; ++pix)
      ++greyValues[pix->value >> 8];
}

//-----------------------------------------------------------------------------

void computeRGBMValues32(const TRaster32P &ras, int (*rgbmValues)[256]) {
  int y, lx = ras->getLx(), ly = ras->getLy();
  TPixel32 *pix, *endPix;

  for (y = 0; y < ly; ++y)
    for (pix = ras->pixels(y), endPix = pix + lx; pix < endPix; ++pix) {
      // NOTE: The insertion order is related to the way channels are
      // displayed inside the histogram's QStackedWidget instance
      if (pix->m) {
        ++rgbmValues[0][pix->r];
        ++rgbmValues[1][pix->g];
        ++rgbmValues[2][pix->b];
      }
      ++rgbmValues[3][pix->m];
    }
}

//-----------------------------------------------------------------------------

void computeRGBMValues64(const TRaster64P &ras, int (*rgbmValues)[256]) {
  int y, lx = ras->getLx(), ly = ras->getLy();
  TPixel64 *pix, *endPix;

  for (y = 0; y < ly; ++y)
    for (pix = ras->pixels(y), endPix = pix + lx; pix < endPix; ++pix) {
      // NOTE: The insertion order is related to the way channels are
      // displayed inside the histogram's QStackedWidget instance
      if (pix->m) {
        ++rgbmValues[0][pix->r >> 8];
        ++rgbmValues[1][pix->g >> 8];
        ++rgbmValues[2][pix->b >> 8];
      }
      ++rgbmValues[3][pix->m >> 8];
    }
}

//-----------------------------------------------------------------------------

void computeRGBMValues(const TRasterCM32P &ras, TPaletteP pal,
                       int (*rgbmValues)[256]) {
  assert(pal);

  int y, lx = ras->getLx(), ly = ras->getLy();
  TPixelCM32 *pix, *endPix;

  int styleId;
  TColorStyle *colorStyle;
  TPixel32 color;

  for (y = 0; y < ly; ++y)
    for (pix = ras->pixels(y), endPix = pix + lx; pix < endPix; ++pix) {
      styleId    = (pix->getTone() < 127) ? pix->getInk() : pix->getPaint();
      colorStyle = pal->getStyle(styleId);
      if (!colorStyle) continue;

      color = colorStyle->getAverageColor();
      if (color.m) {
        ++rgbmValues[0][color.r];
        ++rgbmValues[1][color.g];
        ++rgbmValues[2][color.b];
      }
      ++rgbmValues[3][color.m];
    }
}

//-----------------------------------------------------------------------------

void computeRGB(int (*channelValues)[256], int *rgbValues) {
  int i;
  for (i = 0; i < 256; ++i)
    rgbValues[i] =
        channelValues[0][i] + channelValues[1][i] + channelValues[2][i];
}

//-----------------------------------------------------------------------------

void computeRGBM(int (*channelValues)[256], int *rgbmValues) {
  int i;
  for (i          = 0; i < 256; ++i)
    rgbmValues[i] = channelValues[0][i] + channelValues[1][i] +
                    channelValues[2][i] + channelValues[3][i];
}

}  // namespace

//=============================================================================
// HistogramGraph
//-----------------------------------------------------------------------------

HistogramGraph::HistogramGraph(QWidget *parent, QColor color)
    : QWidget(parent)
    , m_color(color)
    , m_height(120)
    , m_values(0)
    , m_logScale(false) {
  if (m_color.alpha() == 0) m_color = Qt::black;

  setMinimumWidth(256 + drawMargin * 2 + 2);
  setMinimumHeight(m_height + drawMargin);
}

//-----------------------------------------------------------------------------

HistogramGraph::~HistogramGraph() { m_values.clear(); }

//-----------------------------------------------------------------------------

void HistogramGraph::setAlphaMask(int value) { m_color.setAlpha(value); }

//-----------------------------------------------------------------------------

void HistogramGraph::setValues(const int values[]) {
  m_values.clear();
  m_values.resize(256);

  int i;

  double maxValue = 0;
  for (i = 0; i < 256; i++) {
    int count = m_values[i]        = values[i];
    if (maxValue < count) maxValue = count;
  }

  m_viewValues.clear();
  m_logViewValues.clear();
  m_viewValues.resize(256);
  m_logViewValues.resize(256);

  double logMaxValue = log(1.0 + maxValue);

  for (i = 0; i < 256; i++) {
    m_viewValues[i] = double(values[i]) * double(m_height) / maxValue;
    m_logViewValues[i] =
        double(log(1.0 + values[i])) * double(m_height) / logMaxValue;
  }
}

//-----------------------------------------------------------------------------

void HistogramGraph::draw(QPainter *p, QPoint translation) {
  int x0 = translation.x() + drawMargin;
  int y0 = translation.y() + drawMargin - 2;
  int w  = width() - 2 * drawMargin;
  int h  = m_height + 1;

  p->setPen(Qt::NoPen);
  p->setBrush(Qt::white);
  p->drawRect(x0, y0, w - 1, h);
  p->setBrush(Qt::NoBrush);

  p->setPen(Qt::gray);
  int step   = w * 0.25;
  int deltaX = x0 + 1 + step;
  int y1     = y0 + h;
  p->drawLine(deltaX, y0 + 1, deltaX, y1);
  deltaX += step;
  p->drawLine(deltaX, y0 + 1, deltaX, y1);
  deltaX += step;
  p->drawLine(deltaX, y0 + 1, deltaX, y1);

  p->drawRect(x0, y0, w - 1, h);

  if (m_values.size() == 0) return;

  const QVector<int> &viewValues = m_logScale ? m_logViewValues : m_viewValues;

  p->setPen(m_color);
  w  = w - 2;
  x0 = x0 + 1;
  y1 = y1 - 1;
  int i;
  double gap = double(viewValues.size()) /
               double(w); /*-- 1チャンネル値あたりのグラフの横幅 --*/
  for (i = 0; i < w; i++) {
    int j = i * gap;
    assert(j >= 0 && j < viewValues.size());
    int v = viewValues[j];
    if (v <= 0) continue;
    int x = x0 + i;
    p->drawLine(x, y1 - v + 1, x, y1);
  }
}

//-----------------------------------------------------------------------------

void HistogramGraph::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  draw(&p);
}

//=============================================================================
// ChannelBar
//-----------------------------------------------------------------------------

ChannelBar::ChannelBar(QWidget *parent, QColor color, bool isHorizontal)
    : QWidget(parent)
    , m_color(color)
    , m_isHorizontal(isHorizontal)
    , m_colorBarLength(13)
    , m_drawNumbers(true) {
  int d = 256 + HistogramGraph::drawMargin * 2 + 2;

  if (m_isHorizontal)
    setMinimumWidth(d);
  else
    setFixedHeight(d);  // La barra verticale ha la size fissa

  setDrawNumbers(m_drawNumbers);

  if (color == Qt::black) m_color = Qt::white;
}

//-----------------------------------------------------------------------------

ChannelBar::~ChannelBar() {}

//-----------------------------------------------------------------------------

void ChannelBar::setDrawNumbers(bool onOff) {
  if (m_isHorizontal)
    setFixedHeight(onOff ? m_colorBarLength * 2 + 2 : m_colorBarLength + 2);
  else
    setFixedWidth(onOff ? m_colorBarLength + 20 + 2 : m_colorBarLength + 2);
}

//-----------------------------------------------------------------------------

void ChannelBar::draw(QPainter *p, QPoint translation) {
  // Calcolo i parametri necessari al draw a seconda del caso in cui mi trovo.
  int space = HistogramGraph::drawMargin;
  int w, h, x0, y0;
  QRect rect;
  QPoint initialPoint, finalPoint, delta;
  QColor initialColor, finalColor;
  if (m_isHorizontal) {
    w            = width() - 2 * HistogramGraph::drawMargin;
    h            = m_colorBarLength;
    x0           = translation.x() + space;
    y0           = translation.y();
    initialPoint = QPoint(x0, 0);
    finalPoint   = QPoint(w + x0, 0);
    initialColor = Qt::black;
    finalColor   = m_color;
    rect         = QRect(x0 - space, y0 + h, 20, 20);
    delta        = QPoint(w / 4, 0);
  } else {
    w            = m_colorBarLength;
    h            = height() - 2 * HistogramGraph::drawMargin;
    x0           = translation.x() + width() - w;
    y0           = translation.y() + space;
    initialPoint = QPoint(0, y0);
    finalPoint   = QPoint(0, h + y0);
    initialColor = m_color;
    finalColor   = Qt::black;
    rect         = QRect(0, h + y0 - space, 20, 20);
    delta        = QPoint(0, -h / 4);
  }

  if (m_color == QColor(0, 0, 0, 0)) {
    static QPixmap checkboard(":Resources/backg.png");
    p->drawTiledPixmap(x0, y0, w, h, checkboard);
    QColor color = initialColor;
    initialColor = finalColor;
    finalColor   = color;
  }

  QLinearGradient linearGrad(initialPoint, finalPoint);
  linearGrad.setColorAt(0, initialColor);
  linearGrad.setColorAt(1, finalColor);

  p->setBrush(QBrush(linearGrad));

  p->setPen(Qt::black);
  p->drawRect(x0, y0, w - 1, h - 1);

  if (m_drawNumbers) {
    p->drawText(rect, Qt::AlignCenter, QString::number(0));
    rect.translate(delta);
    p->drawText(rect, Qt::AlignCenter, QString::number(64));
    rect.translate(delta);
    p->drawText(rect, Qt::AlignCenter, QString::number(128));
    rect.translate(delta);
    p->drawText(rect, Qt::AlignCenter, QString::number(192));
    rect.translate(delta);
    p->drawText(rect, Qt::AlignCenter, QString::number(255));
  }
}

//-----------------------------------------------------------------------------

void ChannelBar::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  draw(&p);
}

//=============================================================================
// HistogramView
//-----------------------------------------------------------------------------

HistogramView::HistogramView(QWidget *parent, QColor color)
    : QWidget(parent), m_drawnWidget(parent) {
  setMinimumWidth(256 + 10 * 2 + 2);   // 10 == margin of internal widget
  setMinimumHeight(120 + 10 * 2 + 2);  // 10 == margin of internal widget

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(7);

  m_histogramGraph = new HistogramGraph(this, color);
  m_colorBar       = new ChannelBar(this, color);

  mainLayout->addWidget(m_histogramGraph);
  mainLayout->addWidget(m_colorBar);

  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

void HistogramView::setDrawnWidget(QWidget *widget) { m_drawnWidget = widget; }

//-----------------------------------------------------------------------------

void HistogramView::setValues(const int values[]) {
  m_histogramGraph->setValues(values);
  m_drawnWidget->update();
}

//-----------------------------------------------------------------------------

void HistogramView::draw(QPainter *painter, QPoint translation) {
  m_histogramGraph->draw(painter, translation);
  m_colorBar->draw(
      painter, QPoint(translation.x(),
                      translation.y() + m_histogramGraph->getHeight() + 17));
}

//-----------------------------------------------------------------------------

HistogramView::~HistogramView() {}

//=============================================================================
// Histograms
//-----------------------------------------------------------------------------

Histograms::Histograms(QWidget *parent, bool rgba)
    : QStackedWidget(parent)
    , m_raster(0)
    , m_palette(0)
    , m_computeAlsoRGBA(rgba)
    , m_channelsCount(rgba ? 6 : 5) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  HistogramView *histogramViews[6];

  int h                                      = 0;
  if (m_computeAlsoRGBA) histogramViews[h++] = new HistogramView(this);

  histogramViews[h++] = new HistogramView(this);
  histogramViews[h++] = new HistogramView(this, Qt::red);
  histogramViews[h++] = new HistogramView(this, Qt::green);
  histogramViews[h++] = new HistogramView(this, Qt::blue);
  histogramViews[h++] = new HistogramView(this, QColor(0, 0, 0, 0));

  int i;
  for (i = 0; i < m_channelsCount; ++i) addWidget(histogramViews[i]);
}

//-----------------------------------------------------------------------------

Histograms::~Histograms() { memset(m_channelValue, 0, sizeof m_channelValue); }

//-----------------------------------------------------------------------------

void Histograms::setRaster(const TRasterP &raster, const TPaletteP &palette) {
  if (palette.getPointer()) m_palette = palette;
  m_raster                            = raster;
  computeChannelsValue();
  int i;
  for (i = 0; i < count(); i++)
    getHistogramView(i)->setValues(m_channelValue[i]);
}

//-----------------------------------------------------------------------------

void Histograms::computeChannelsValue() {
  m_channelsCount = m_computeAlsoRGBA ? 6 : 5;

  memset(m_channelValue, 0, sizeof m_channelValue);
  if (!m_raster.getPointer()) return;

  int(*rgbChannelValues)[256] = m_channelValue + (m_computeAlsoRGBA ? 1 : 0);
  int(*channelValues)[256]    = rgbChannelValues + 1;

  TRasterCM32P cmRaster = m_raster;
  bool isCmRaster       = !!cmRaster;

  {
    TRaster32P ras32(m_raster);
    if (ras32) {
      ::computeRGBMValues32(ras32, channelValues);
      goto computeChannelsSums;
    }
  }

  {
    TRaster64P ras64(m_raster);
    if (ras64) {
      ::computeRGBMValues64(ras64, channelValues);
      goto computeChannelsSums;
    }
  }

  {
    TRasterGR8P rasGR8(m_raster);
    if (rasGR8) {
      m_channelsCount = 1;
      ::computeGreyValues(rasGR8, m_channelValue[0]);
      return;
    }
  }

  {
    TRasterGR16P rasGR16(m_raster);
    if (rasGR16) {
      m_channelsCount = 1;
      ::computeGreyValues(rasGR16, m_channelValue[0]);
      return;
    }
  }

  return;

computeChannelsSums:

  if (m_computeAlsoRGBA) ::computeRGBM(channelValues, m_channelValue[0]);
  ::computeRGB(channelValues, *rgbChannelValues);
}

//-----------------------------------------------------------------------------

HistogramView *Histograms::getHistogramView(int indexType) const {
  HistogramView *ch = dynamic_cast<HistogramView *>(widget(indexType));
  assert(ch);
  return ch;
}

//=============================================================================
// Histogram
//-----------------------------------------------------------------------------

Histogram::Histogram(QWidget *parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  setLayout(mainLayout);

  QHBoxLayout *upperLayout = new QHBoxLayout;
  mainLayout->addLayout(upperLayout);

  m_channelsListBox = new QComboBox(this);
  m_channelsListBox->setFixedSize(100, 20);

  upperLayout->addSpacing(HistogramGraph::drawMargin);
  upperLayout->addWidget(m_channelsListBox);
  upperLayout->addStretch(1);

  QIcon icon = createQIcon("histograms");

  QPushButton *logScaleButton = new QPushButton(icon, "", this);
  logScaleButton->setToolTip(tr("Logarithmic Scale"));
  logScaleButton->setFixedSize(20, 20);
  logScaleButton->setCheckable(true);

  upperLayout->addWidget(logScaleButton);
  upperLayout->addSpacing(HistogramGraph::drawMargin);

  m_histograms = new Histograms(this);
  m_histograms->setCurrentIndex(0);
  mainLayout->addWidget(m_histograms);

  connect(m_channelsListBox, SIGNAL(currentIndexChanged(int)), m_histograms,
          SLOT(setCurrentIndex(int)));
  connect(logScaleButton, SIGNAL(toggled(bool)), this, SLOT(setLogScale(bool)));

  updateChannelsList();
}

//-----------------------------------------------------------------------------

void Histogram::updateChannelsList() {
  if (m_histograms->channelsCount() != m_channelsListBox->count()) {
    QStringList channels;
    m_channelsListBox->clear();

    if (m_histograms->channelsCount() == 1)
      channels << "Value";
    else
      channels << "RGB"
               << "Red"
               << "Green"
               << "Blue"
               << "Alpha";

    m_channelsListBox->addItems(channels);
  }
}

//-----------------------------------------------------------------------------

void Histogram::setRaster(const TRasterP &raster, const TPaletteP &palette) {
  m_histograms->setRaster(raster, palette);
  updateChannelsList();
}

//-----------------------------------------------------------------------------

void Histogram::setLogScale(bool onOff) {
  int i, count = m_histograms->channelsCount();
  for (i = 0; i < count; ++i) {
    HistogramGraph *graph = m_histograms->getHistogramView(i)->histogramGraph();
    graph->setLogScale(onOff);
  }
  update();
}
