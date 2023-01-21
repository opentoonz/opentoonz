#include "toonzqt/combohistogram.h"
#include "tcolorstyles.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QStackedWidget>
#include <QPainter>
#include <QLabel>
#include <QString>

#include "toonz/preferences.h"
#include "toonzqt/lutcalibrator.h"
#include "toonzqt/gutil.h"
#include "timagecache.h"
#include "trasterimage.h"

// TnzBase includes
#include "tenv.h"

#include <QPushButton>
#include <QDialog>

TEnv::IntVar HistogramChannelDisplayMode(
    "HistogramChannelDisplayMode",
    (int)ComboHistoRGBLabel::Display_8bit);  // 8bit display by default

namespace {

inline int idx(int y, int x) { return y * COMBOHIST_RESOLUTION_W + x; }

}  // namespace

//=============================================================================
// ChannelHistoGraph
//-----------------------------------------------------------------------------

ChannelHistoGraph::ChannelHistoGraph(int index, QWidget *parent,
                                     bool *showComparePtr)
    : QWidget(parent)
    , m_showComparePtr(showComparePtr)
    , m_channelIndex(index)
    , m_pickedValue(-1) {
  // width 256+2 pixels, height 100+2 pixels (with margin)
  setFixedSize(COMBOHIST_RESOLUTION_W + 2, COMBOHIST_RESOLUTION_H + 2);
  m_values[0].reserve(COMBOHIST_RESOLUTION_W);
  m_values[1].reserve(COMBOHIST_RESOLUTION_W);
}

//-----------------------------------------------------------------------------

ChannelHistoGraph::~ChannelHistoGraph() {
  m_values[0].clear();
  m_values[1].clear();
}

//-----------------------------------------------------------------------------

void ChannelHistoGraph::setValues(int *buf, bool isComp) {
  int id = (isComp) ? 1 : 0;
  m_values[id].clear();
  m_values[id].resize(COMBOHIST_RESOLUTION_W);
  m_maxValue[id] = 1;
  int i;
  // normalize with the maximum value
  int maxValue = 1;
  for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
    m_values[id][i] = buf[i];
    if (m_maxValue[id] < buf[i]) m_maxValue[id] = buf[i];
  }

  // for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
  //  int v = buf[i];
  //  m_values[id][i] =
  //      tround((double)(v * COMBOHIST_RESOLUTION_H) / (double)maxValue);
  //}
}

//-----------------------------------------------------------------------------

void ChannelHistoGraph::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  p.setPen(QColor(144, 144, 144));
  p.setBrush(QColor(214, 214, 214));
  p.drawRect(rect().x(), rect().y(), rect().width() - 1, rect().height() - 1);
  p.setBrush(Qt::NoBrush);

  int i;

  // draw scale marks
  if (areAlmostEqual(m_range, 1.)) {
    p.setPen(QColor(144, 144, 144));
    for (i = 1; i < 10; i++) {
      int posx = rect().width() * i / 10;
      p.drawLine(posx, 1, posx, COMBOHIST_RESOLUTION_H);
    }
  } else {
    int range_i = (int)std::round(m_range);
    for (i = 1; i < range_i; i++) {
      int posx = rect().width() * i / range_i;
      p.drawLine(posx, 1, posx, COMBOHIST_RESOLUTION_H);
    }
  }

  QColor compColor = (m_channelIndex == 0)   ? Qt::red
                     : (m_channelIndex == 1) ? Qt::green
                     : (m_channelIndex == 2) ? Qt::blue
                                             : Qt::magenta;
  compColor.setAlpha(120);

  int maxValue = m_maxValue[0];
  if (!m_values[1].isEmpty() && m_showComparePtr && *m_showComparePtr &&
      m_maxValue[0] < m_maxValue[1])
    maxValue = m_maxValue[1];

  p.translate(0, COMBOHIST_RESOLUTION_H);
  p.scale(1.0, -(double)COMBOHIST_RESOLUTION_H / (double)maxValue);

  for (int id = 0; id < 2; id++) {
    if (m_values[id].isEmpty()) continue;
    if (id == 1 && (!m_showComparePtr || !(*m_showComparePtr))) continue;

    p.setPen((id == 0) ? Qt::black : compColor);

    // draw each histogram
    for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
      int v = m_values[id][i];
      if (v <= 0) continue;
      int x = 1 + i;
      p.drawLine(x, 0, x, v);
    }

    // draw picked color's channel value
    if (m_pickedValue > -1) {
      p.setPen(Qt::white);
      int x = 1 + m_pickedValue;
      p.drawLine(x, 1, x, maxValue);
    }
  }
}

//-----------------------------------------------------------------------------

void ChannelHistoGraph::showCurrentChannelValue(int val) {
  m_pickedValue = val;
  update();
}

//=============================================================================
// ChannelHistoGraph
//-----------------------------------------------------------------------------

RGBHistoGraph::RGBHistoGraph(int index, QWidget *parent)
    : ChannelHistoGraph(index, parent) {
  m_histoImg = QImage(COMBOHIST_RESOLUTION_W, COMBOHIST_RESOLUTION_H,
                      QImage::Format_ARGB32_Premultiplied);
}

//-----------------------------------------------------------------------------

RGBHistoGraph::~RGBHistoGraph() {
  for (int i = 0; i < 3; i++) m_rgbValues[i].clear();
}

//-----------------------------------------------------------------------------

void RGBHistoGraph::setValues(int *buf, bool) {
  for (int chan = 0; chan < 3; chan++) {
    m_rgbValues[chan].clear();
    m_rgbValues[chan].resize(COMBOHIST_RESOLUTION_W);

    int i;

    // normalize with the maximum value
    int maxValue = 1;
    for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
      int count = buf[COMBOHIST_RESOLUTION_W * chan + i];
      if (maxValue < count) maxValue = count;
    }

    for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
      int v = buf[COMBOHIST_RESOLUTION_W * chan + i];
      m_rgbValues[chan][i] =
          tround((double)(v * COMBOHIST_RESOLUTION_H) / (double)maxValue);
    }
  }

  QPainter imgPainter(&m_histoImg);

  imgPainter.fillRect(m_histoImg.rect(), Qt::black);

  if (m_rgbValues[0].size() == 0 || m_rgbValues[1].size() == 0 ||
      m_rgbValues[2].size() == 0) {
    imgPainter.end();
    return;
  }

  imgPainter.setCompositionMode(QPainter::CompositionMode_Plus);

  for (int chan = 0; chan < 3; chan++) {
    imgPainter.setPen((chan == 0)   ? Qt::red
                      : (chan == 1) ? Qt::green
                                    : Qt::blue);

    for (int i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
      int v = m_rgbValues[chan][i];
      if (v <= 0) continue;
      int x = 1 + i;
      imgPainter.drawLine(x, COMBOHIST_RESOLUTION_H + 1 - v, x,
                          COMBOHIST_RESOLUTION_H);
    }
  }
  imgPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  imgPainter.end();
}

//-----------------------------------------------------------------------------

void RGBHistoGraph::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setPen(QColor(144, 144, 144));
  p.setBrush(QColor(64, 64, 64));
  p.drawRect(rect().x(), rect().y(), rect().width() - 1, rect().height() - 1);
  p.setBrush(Qt::NoBrush);

  p.drawImage(1, 1, m_histoImg);
}

//=============================================================================
// ChannelColorBar
//-----------------------------------------------------------------------------
ChannelColorBar::ChannelColorBar(QWidget *parent, QColor color)
    : QWidget(parent), m_color(color) {
  // 2 pixels margin for width
  setFixedSize(COMBOHIST_RESOLUTION_W + 2, 6);
}

//-----------------------------------------------------------------------------

void ChannelColorBar::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  QLinearGradient linearGrad(QPoint(1, 0), QPoint(COMBOHIST_RESOLUTION_W, 0));
  if (m_color == QColor(0, 0, 0, 0)) {
    linearGrad.setColorAt(0, m_color);
    linearGrad.setColorAt(1, Qt::white);
  } else {
    linearGrad.setColorAt(0, Qt::black);
    if (!areAlmostEqual(m_range, 1.))
      linearGrad.setColorAt(1. / m_range, m_color);
    linearGrad.setColorAt(1, m_color);
  }

  p.setBrush(QBrush(linearGrad));
  p.setPen(Qt::NoPen);
  p.drawRect(rect());

  if (!areAlmostEqual(m_range, 1.)) {
    static const QPointF points[3] = {QPointF(0.0, 0.0), QPointF(3.0, 6.0),
                                      QPointF(-3.0, 6.0)};

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.translate(COMBOHIST_RESOLUTION_W / m_range + 1, 0.);
    p.drawPolygon(points, 3);
  }
}

//=============================================================================
// ChannelHisto
//-----------------------------------------------------------------------------
ChannelHisto::ChannelHisto(int channelIndex, bool *showComparePtr,
                           QWidget *parent) {
  QString label;
  QColor color;
  switch (channelIndex) {
  case 0:
    label = tr("Red");
    color = Qt::red;
    break;
  case 1:
    label = tr("Green");
    color = Qt::green;
    break;
  case 2:
    label = tr("Blue");
    color = Qt::blue;
    break;
  case 3:
    label = tr("Alpha");
    color = QColor(0, 0, 0, 0);
    break;
  case 4:
    label = tr("RGB");
    color = Qt::white;
    break;
  }

  if (channelIndex != 4)
    m_histogramGraph =
        new ChannelHistoGraph(channelIndex, this, showComparePtr);
  else
    m_histogramGraph = new RGBHistoGraph(channelIndex, this);

  m_colorBar = new ChannelColorBar(this, color);

  // show/hide the alpha channel
  QPushButton *showAlphaChannelButton = 0;
  if (channelIndex == 3) {
    showAlphaChannelButton = new QPushButton("", this);
    showAlphaChannelButton->setObjectName("menuToggleButton");
    showAlphaChannelButton->setFixedSize(15, 15);
    showAlphaChannelButton->setIcon(createQIcon("menu_toggle"));
    showAlphaChannelButton->setCheckable(true);
    showAlphaChannelButton->setChecked(false);
    showAlphaChannelButton->setFocusPolicy(Qt::NoFocus);
  }

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(2);
  {
    QHBoxLayout *titleLay = new QHBoxLayout();
    titleLay->setMargin(0);
    titleLay->setSpacing(2);
    {
      titleLay->addWidget(new QLabel(label, this), 0);
      if (channelIndex == 3) titleLay->addWidget(showAlphaChannelButton, 0);
      titleLay->addStretch(1);
    }
    mainLayout->addLayout(titleLay);
    mainLayout->addSpacing(3);
    mainLayout->addWidget(m_histogramGraph);
    mainLayout->addWidget(m_colorBar);
  }
  setLayout(mainLayout);

  if (channelIndex == 3) {
    connect(showAlphaChannelButton, SIGNAL(toggled(bool)), this,
            SLOT(onShowAlphaButtonToggled(bool)));
    onShowAlphaButtonToggled(false);
  }
}

//-----------------------------------------------------------------------------
/*! update the picked color's channel value
 */
void ChannelHisto::showCurrentChannelValue(int val) {
  m_histogramGraph->showCurrentChannelValue(val);
}

//-----------------------------------------------------------------------------

void ChannelHisto::onShowAlphaButtonToggled(bool visible) {
  m_histogramGraph->setVisible(visible);
  m_colorBar->setVisible(visible);
  emit showButtonToggled(visible);
}

//=============================================================================
// ComboHistoRGBLabel
//-----------------------------------------------------------------------------
ComboHistoRGBLabel::ComboHistoRGBLabel(QColor color, QWidget *parent)
    : QWidget(parent)
    , m_color(color)
    , m_mode(DisplayMode::Display_8bit)
    , m_alphaVisible(false) {
  setFixedSize(COMBOHIST_RESOLUTION_W, 35);
}

void ComboHistoRGBLabel::setColorAndUpdate(QColor color) {
  if (m_color == color) return;
  m_color = color;
  update();
}

void ComboHistoRGBLabel::paintEvent(QPaintEvent *pe) {
  QPainter p(this);
  p.setPen(Qt::black);
  QRect bgRect = QRect(rect().left(), rect().top(), rect().width() - 1,
                       rect().height() - 1);

  if (m_color.alpha() == 0) {
    p.setBrush(Qt::gray);
    p.drawRect(bgRect);
    return;
  }
  if (LutManager::instance()->isValid()) {
    QColor convertedColor(m_color);
    LutManager::instance()->convert(convertedColor);
    p.setBrush(convertedColor);
  } else
    p.setBrush(m_color);

  p.drawRect(bgRect);

  // white text for dark color, black text for light color
  int val = m_color.red() * 30 + m_color.green() * 59 + m_color.blue() * 11;
  if (val < 12800)
    p.setPen(Qt::white);
  else
    p.setPen(Qt::black);
  p.setBrush(Qt::NoBrush);

  QFont font              = p.font();
  const int pixelSizes[3] = {18, 14, 14};
  font.setPixelSize(pixelSizes[(int)m_mode]);
  p.setFont(font);
  QString colorStr;
  QString colorStr2 = QString();
  switch (m_mode) {
  case Display_8bit: {
    colorStr = tr("R:%1 G:%2 B:%3")
                   .arg(m_color.red())
                   .arg(m_color.green())
                   .arg(m_color.blue());
    if (m_alphaVisible) colorStr += tr(" A:%1").arg(m_color.alpha());
    break;
  }
  case Display_16bit: {
    QRgba64 rgba64 = m_color.rgba64();
    colorStr       = tr("R:%1 G:%2 B:%3")
                   .arg(rgba64.red())
                   .arg(rgba64.green())
                   .arg(rgba64.blue());
    if (m_alphaVisible) colorStr += tr(" A:%1").arg(rgba64.alpha());
    break;
  }
  case Display_0_1: {
    if (!m_alphaVisible) {
      colorStr = tr("R:%1 G:%2 B:%3")
                     .arg(m_color.redF())
                     .arg(m_color.greenF())
                     .arg(m_color.blueF());
    } else {
      // display in 2 lines
      colorStr = tr("R:%1 G:%2 B:%3")
                     .arg(m_color.redF())
                     .arg(m_color.greenF())
                     .arg(m_color.blueF());
      colorStr2 = tr("A:%1").arg(m_color.alphaF());
    }
    break;
  }
  }
  if (colorStr2.isEmpty())
    p.drawText(rect(), Qt::AlignCenter, colorStr);
  else {
    QRect upRect = rect().adjusted(0, 0, 0, -rect().height() / 2);
    p.drawText(upRect, Qt::AlignCenter, colorStr);
    QRect dnRect = upRect.translated(0, rect().height() / 2);
    p.drawText(dnRect, Qt::AlignCenter, colorStr2);
  }
}

//=============================================================================
// ComboHistogram
//-----------------------------------------------------------------------------

ComboHistogram::ComboHistogram(QWidget *parent)
    : QWidget(parent)
    , m_raster(0)
    , m_palette(0)
    , m_showCompare(false)
    , m_compHistoIsValid(true)
    , m_rangeStep(0) {
  for (int chan = 0; chan < 4; chan++)
    m_histograms[chan] = new ChannelHisto(chan, &m_showCompare, this);
  m_histograms[4] = new ChannelHisto(4, &m_showCompare, this);

  // RGB label
  m_rgbLabel = new ComboHistoRGBLabel(QColor(128, 128, 128), this);
  // m_rgbLabel->setStyleSheet("font-size: 18px;");

  m_rectAverageRgbLabel = new ComboHistoRGBLabel(QColor(128, 128, 128), this);
  // m_rectAverageRgbLabel->setStyleSheet("font-size: 18px;");

  m_xPosLabel = new QLabel("", this);
  m_yPosLabel = new QLabel("", this);

  m_displayModeCombo = new QComboBox(this);

  m_rangeControlContainer = new QWidget(this);
  m_rangeUpBtn            = new QPushButton("", this);
  m_rangeDwnBtn           = new QPushButton("", this);
  m_rangeLabel            = new QLabel("1.0", this);

  //-----

  m_displayModeCombo->addItem(
      tr("8bit (0-255)"), (int)ComboHistoRGBLabel::DisplayMode::Display_8bit);
  m_displayModeCombo->addItem(
      tr("16bit (0-65535)"),
      (int)ComboHistoRGBLabel::DisplayMode::Display_16bit);
  m_displayModeCombo->addItem(
      tr("0.0-1.0"), (int)ComboHistoRGBLabel::DisplayMode::Display_0_1);

  m_rangeUpBtn->setIcon(createQIcon("prevkey"));
  m_rangeDwnBtn->setIcon(createQIcon("nextkey"));
  m_rangeUpBtn->setFixedWidth(17);
  m_rangeDwnBtn->setFixedWidth(17);
  m_rangeDwnBtn->setEnabled(false);
  m_rangeLabel->setFixedWidth(30);
  m_rangeLabel->setAlignment(Qt::AlignCenter);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);
  {
    mainLayout->addWidget(m_histograms[4]);  // RGB

    QHBoxLayout *labelLay = new QHBoxLayout();
    labelLay->setMargin(0);
    labelLay->setSpacing(0);
    {
      labelLay->addWidget(new QLabel(tr("Picked Color"), this), 0);
      labelLay->addStretch(1);
      labelLay->addWidget(m_displayModeCombo, 0);
    }
    mainLayout->addLayout(labelLay, 0);

    mainLayout->addWidget(m_rgbLabel, 0, Qt::AlignCenter);

    mainLayout->addWidget(new QLabel(tr("Average Color (Ctrl + Drag)"), this),
                          0, Qt::AlignLeft | Qt::AlignVCenter);
    mainLayout->addWidget(m_rectAverageRgbLabel, 0, Qt::AlignCenter);

    QHBoxLayout *infoParamLay = new QHBoxLayout();
    infoParamLay->setMargin(5);
    infoParamLay->setSpacing(3);
    {
      infoParamLay->addWidget(new QLabel(tr("X:"), this), 1,
                              Qt::AlignRight | Qt::AlignVCenter);
      infoParamLay->addWidget(m_xPosLabel, 1, Qt::AlignLeft | Qt::AlignVCenter);
      infoParamLay->addWidget(new QLabel(tr("Y:"), this), 1,
                              Qt::AlignRight | Qt::AlignVCenter);
      infoParamLay->addWidget(m_yPosLabel, 2, Qt::AlignLeft | Qt::AlignVCenter);

      // range control
      QHBoxLayout *rangeLay = new QHBoxLayout();
      rangeLay->setMargin(0);
      rangeLay->setSpacing(0);
      {
        rangeLay->addWidget(m_rangeUpBtn, 0);
        rangeLay->addWidget(m_rangeLabel, 0);
        rangeLay->addWidget(m_rangeDwnBtn, 0);
      }
      m_rangeControlContainer->setLayout(rangeLay);
      infoParamLay->addWidget(m_rangeControlContainer, 0);
    }
    mainLayout->addLayout(infoParamLay, 0);

    mainLayout->addWidget(m_histograms[0]);  // R
    mainLayout->addWidget(m_histograms[1]);  // G
    mainLayout->addWidget(m_histograms[2]);  // B
    mainLayout->addWidget(m_histograms[3]);  // A
    mainLayout->addStretch(1);
  }
  setLayout(mainLayout);

  m_rectAverageRgbLabel->setColorAndUpdate(Qt::transparent);

  connect(m_displayModeCombo, SIGNAL(activated(int)), this,
          SLOT(onDisplayModeChanged()));
  connect(m_histograms[3], SIGNAL(showButtonToggled(bool)), this,
          SLOT(onShowAlphaButtonToggled(bool)));
  connect(m_rangeUpBtn, SIGNAL(clicked()), this, SLOT(onRangeUp()));
  connect(m_rangeDwnBtn, SIGNAL(clicked()), this, SLOT(onRangeDown()));
}
//-----------------------------------------------------------------------------

ComboHistogram::~ComboHistogram() {
  memset(m_channelValue, 0, sizeof m_channelValue);
  memset(m_channelValueComp, 0, sizeof m_channelValueComp);
}

//-----------------------------------------------------------------------------

void ComboHistogram::setRaster(const TRasterP &raster,
                               const TPaletteP &palette) {
  if (palette.getPointer()) m_palette = palette;
  m_raster = raster;

  refreshHistogram();

  m_rangeControlContainer->setVisible(!!((TRasterFP)raster));
  update();
}

//-----------------------------------------------------------------------------

void ComboHistogram::refreshHistogram() {
  computeChannelsValue(&m_channelValue[0][0], sizeof(m_channelValue), m_raster);

  float range = 1.f;
  if (!!((TRasterFP)m_raster)) {
    range = std::pow(2.f, (float)m_rangeStep);
  }

  for (int chan = 0; chan < 4; chan++) {
    m_histograms[chan]->refleshValue(&m_channelValue[chan][0]);
    if (chan != 3)  // rgb channels
      m_histograms[chan]->setRange(range);
  }
  m_histograms[4]->refleshValue(&m_channelValue[0][0]);
  m_histograms[4]->setRange(range);
}

//-----------------------------------------------------------------------------

void ComboHistogram::computeChannelsValue(int *buf, size_t size, TRasterP ras,
                                          TPalette *extPlt) {
  memset(buf, 0, size);
  if (!ras.getPointer()) return;
  TRasterCM32P cmRaster = ras;
  bool isCmRaster       = !!cmRaster;

  TRaster64P raster64 = ras;
  bool is64bit        = !!raster64;

  TRasterFP rasF = ras;

  float factor = 1.f / std::pow(2.f, (float)m_rangeStep);

  int lx = ras->getLx();
  int ly = ras->getLy();
  if (lx > 1 && ly > 1) {
    int i, j;
    if (is64bit) {
      for (j = 0; j < ly; j++) {
        TPixel64 *pix_64 = raster64->pixels(j);
        for (i = 0; i < lx; i++, pix_64++) {
          int mValue = (int)byteFromUshort(pix_64->m);
          if (mValue != 0) {
            ++buf[idx(0, (int)byteFromUshort(pix_64->r))];
            ++buf[idx(1, (int)byteFromUshort(pix_64->g))];
            ++buf[idx(2, (int)byteFromUshort(pix_64->b))];
          }
          ++buf[idx(3, mValue)];
        }
      }
    } else if (isCmRaster) {
      TPalette *plt = (extPlt) ? extPlt : m_palette.getPointer();
      assert(plt);
      for (j = 0; j < ly; j++) {
        TPixelCM32 *pix_cm = cmRaster->pixels(j);
        for (i = 0; i < lx; i++, pix_cm++) {
          int styleId =
              pix_cm->getTone() < 127 ? pix_cm->getInk() : pix_cm->getPaint();
          TColorStyle *colorStyle = plt->getStyle(styleId);
          if (!colorStyle) continue;
          TPixel color = colorStyle->getAverageColor();
          int mValue   = color.m;
          if (mValue != 0) {
            ++buf[idx(0, color.r)];
            ++buf[idx(1, color.g)];
            ++buf[idx(2, color.b)];
          }
          ++buf[idx(3, color.m)];
        }
      }
    } else if (rasF) {
      for (j = 0; j < ly; j++) {
        TPixelF *pixF = rasF->pixels(j);
        for (i = 0; i < lx; i++, pixF++) {
          int mValue = (int)byteFromFloat(pixF->m);
          if (mValue != 0) {
            ++buf[idx(0, (int)byteFromFloat(pixF->r * factor))];
            ++buf[idx(1, (int)byteFromFloat(pixF->g * factor))];
            ++buf[idx(2, (int)byteFromFloat(pixF->b * factor))];
          }
          ++buf[idx(3, mValue)];
        }
      }
    } else  // 8bpc raster
    {
      for (j = 0; j < ly; j++) {
        TPixel *pix = (TPixel *)ras->getRawData(0, j);
        for (i = 0; i < lx; i++, pix++) {
          int mValue = pix->m;
          if (mValue != 0) {
            ++buf[idx(0, pix->r)];
            ++buf[idx(1, pix->g)];
            ++buf[idx(2, pix->b)];
          }
          ++buf[idx(3, pix->m)];
        }
      }
    }
  }
}
//-----------------------------------------------------------------------------

void ComboHistogram::updateInfo(const TPixel32 &pix, const TPointD &imagePos) {
  if (pix == TPixel32::Transparent) {
    m_histograms[0]->showCurrentChannelValue(-1);
    m_histograms[1]->showCurrentChannelValue(-1);
    m_histograms[2]->showCurrentChannelValue(-1);
    m_histograms[3]->showCurrentChannelValue(-1);
    m_rgbLabel->setColorAndUpdate(Qt::transparent);
    m_xPosLabel->setText("");
    m_yPosLabel->setText("");
  } else {
    // show picked color's channel values
    m_histograms[0]->showCurrentChannelValue((int)pix.r);
    m_histograms[1]->showCurrentChannelValue((int)pix.g);
    m_histograms[2]->showCurrentChannelValue((int)pix.b);
    m_histograms[3]->showCurrentChannelValue((int)pix.m);
    m_rgbLabel->setColorAndUpdate(
        QColor((int)pix.r, (int)pix.g, (int)pix.b, (int)pix.m));
    m_xPosLabel->setText(QString::number(tround(imagePos.x)));
    m_yPosLabel->setText(QString::number(tround(imagePos.y)));
  }
}

//-----------------------------------------------------------------------------

void ComboHistogram::updateInfo(const TPixel64 &pix, const TPointD &imagePos) {
  if (pix == TPixel64::Transparent) {
    m_histograms[0]->showCurrentChannelValue(-1);
    m_histograms[1]->showCurrentChannelValue(-1);
    m_histograms[2]->showCurrentChannelValue(-1);
    m_histograms[3]->showCurrentChannelValue(-1);
    m_rgbLabel->setColorAndUpdate(Qt::transparent);
    m_xPosLabel->setText("");
    m_yPosLabel->setText("");
  } else {
    TPixel32 pix32 = toPixel32(pix);
    // show picked color's channel values
    m_histograms[0]->showCurrentChannelValue((int)pix32.r);
    m_histograms[1]->showCurrentChannelValue((int)pix32.g);
    m_histograms[2]->showCurrentChannelValue((int)pix32.b);
    m_histograms[3]->showCurrentChannelValue((int)pix32.m);
    m_rgbLabel->setColorAndUpdate(QColor::fromRgba64(
        (ushort)pix.r, (ushort)pix.g, (ushort)pix.b, (ushort)pix.m));
    m_xPosLabel->setText(QString::number(tround(imagePos.x)));
    m_yPosLabel->setText(QString::number(tround(imagePos.y)));
  }
}

//-----------------------------------------------------------------------------

void ComboHistogram::updateInfo(const TPixelF &pix, const TPointD &imagePos) {
  if (pix == TPixelF::Transparent) {
    m_histograms[0]->showCurrentChannelValue(-1);
    m_histograms[1]->showCurrentChannelValue(-1);
    m_histograms[2]->showCurrentChannelValue(-1);
    m_histograms[3]->showCurrentChannelValue(-1);
    m_rgbLabel->setColorAndUpdate(Qt::transparent);
    m_xPosLabel->setText("");
    m_yPosLabel->setText("");
  } else {
    float factor   = std::pow(2.f, -(float)m_rangeStep);
    TPixel32 pix32 = toPixel32(
        TPixelF(pix.r * factor, pix.g * factor, pix.b * factor, pix.m));
    // show picked color's channel values
    m_histograms[0]->showCurrentChannelValue((int)pix32.r);
    m_histograms[1]->showCurrentChannelValue((int)pix32.g);
    m_histograms[2]->showCurrentChannelValue((int)pix32.b);
    m_histograms[3]->showCurrentChannelValue((int)pix32.m);
    m_rgbLabel->setColorAndUpdate(QColor::fromRgbF(pix.r, pix.g, pix.b, pix.m));
    m_xPosLabel->setText(QString::number(tround(imagePos.x)));
    m_yPosLabel->setText(QString::number(tround(imagePos.y)));
  }
}
//-----------------------------------------------------------------------------

void ComboHistogram::updateAverageColor(const TPixel32 &pix) {
  if (pix == TPixel32::Transparent) {
    m_rectAverageRgbLabel->setColorAndUpdate(Qt::transparent);
  } else {
    m_rectAverageRgbLabel->setColorAndUpdate(
        QColor((int)pix.r, (int)pix.g, (int)pix.b, (int)pix.m));
  }
}

//-----------------------------------------------------------------------------

void ComboHistogram::updateAverageColor(const TPixel64 &pix) {
  if (pix == TPixel64::Transparent) {
    m_rectAverageRgbLabel->setColorAndUpdate(Qt::transparent);
  } else {
    m_rectAverageRgbLabel->setColorAndUpdate(QColor::fromRgba64(
        (ushort)pix.r, (ushort)pix.g, (ushort)pix.b, (ushort)pix.m));
  }
}

//-----------------------------------------------------------------------------

void ComboHistogram::updateAverageColor(const TPixelF &pix) {
  if (pix == TPixelF::Transparent) {
    m_rectAverageRgbLabel->setColorAndUpdate(Qt::transparent);
  } else {
    m_rectAverageRgbLabel->setColorAndUpdate(
        QColor::fromRgbF(pix.r, pix.g, pix.b, pix.m));
  }
}

//-----------------------------------------------------------------------------

void ComboHistogram::onDisplayModeChanged() {
  ComboHistoRGBLabel::DisplayMode mode =
      static_cast<ComboHistoRGBLabel::DisplayMode>(
          m_displayModeCombo->currentData().toInt());
  m_rgbLabel->setDisplayMode(mode);
  m_rectAverageRgbLabel->setDisplayMode(mode);
  HistogramChannelDisplayMode = (int)mode;
  update();
}

//-----------------------------------------------------------------------------

void ComboHistogram::updateCompHistogram() {
  assert(!m_compHistoIsValid);
  m_compHistoIsValid = true;

  TImageP refimg =
      TImageCache::instance()->get(QString("TnzCompareImg"), false);

  if (!(TToonzImageP)refimg && !(TRasterImageP)refimg) return;

  computeChannelsValue(&m_channelValueComp[0][0], sizeof m_channelValueComp,
                       refimg->raster(), refimg->getPalette());

  for (int chan = 0; chan < 4; chan++)
    m_histograms[chan]->refleshValue(&m_channelValueComp[chan][0], true);
}

//-----------------------------------------------------------------------------

void ComboHistogram::showEvent(QShowEvent *) {
  if (!m_compHistoIsValid && m_showCompare) updateCompHistogram();

  int envMode = HistogramChannelDisplayMode;
  m_displayModeCombo->setCurrentIndex(m_displayModeCombo->findData(envMode));
  ComboHistoRGBLabel::DisplayMode mode =
      static_cast<ComboHistoRGBLabel::DisplayMode>(envMode);
  m_rgbLabel->setDisplayMode(mode);
  m_rectAverageRgbLabel->setDisplayMode(mode);
}

//-----------------------------------------------------------------------------

void ComboHistogram::onShowAlphaButtonToggled(bool alphaVisible) {
  m_rgbLabel->setAlphaVisible(alphaVisible);
  m_rectAverageRgbLabel->setAlphaVisible(alphaVisible);
  m_rgbLabel->update();
  m_rectAverageRgbLabel->update();
}

//-----------------------------------------------------------------------------

void ComboHistogram::onRangeUp() {
  m_rangeStep++;
  m_rangeDwnBtn->setEnabled(true);
  if (m_rangeStep == 3) m_rangeUpBtn->setDisabled(true);

  m_rangeLabel->setText(QString("%1.0").arg(std::pow(2, m_rangeStep)));

  refreshHistogram();
  m_compHistoIsValid = false;
  if (m_showCompare) updateCompHistogram();

  update();
}

//-----------------------------------------------------------------------------

void ComboHistogram::onRangeDown() {
  m_rangeStep--;
  m_rangeUpBtn->setEnabled(true);
  if (m_rangeStep == 0) m_rangeDwnBtn->setDisabled(true);

  m_rangeLabel->setText(QString("%1.0").arg(std::pow(2, m_rangeStep)));

  refreshHistogram();
  m_compHistoIsValid = false;
  if (m_showCompare) updateCompHistogram();

  update();
}