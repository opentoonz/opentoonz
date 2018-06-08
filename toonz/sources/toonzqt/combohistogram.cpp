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
#include <QPushButton>
#include <QDialog>

//=============================================================================
// ChannelHistoGraph
//-----------------------------------------------------------------------------

ChannelHistoGraph::ChannelHistoGraph(QWidget *parent, int *channelValue)
    : QWidget(parent), m_channelValuePtr(channelValue), m_pickedValue(-1) {
  // width 256+2 pixels, height 100+2 pixels (with margin)
  setFixedSize(COMBOHIST_RESOLUTION_W + 2, COMBOHIST_RESOLUTION_H + 2);
  m_values.reserve(COMBOHIST_RESOLUTION_W);
}

//-----------------------------------------------------------------------------

ChannelHistoGraph::~ChannelHistoGraph() { m_values.clear(); }

//-----------------------------------------------------------------------------

void ChannelHistoGraph::setValues() {
  m_values.clear();
  m_values.resize(COMBOHIST_RESOLUTION_W);

  int i;

  // normalize with the maximum value
  int maxValue = 1;
  for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
    int count                      = m_channelValuePtr[i];
    if (maxValue < count) maxValue = count;
  }

  for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
    int v = m_channelValuePtr[i];
    m_values[i] =
        tround((double)(v * COMBOHIST_RESOLUTION_H) / (double)maxValue);
  }
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
  p.setPen(QColor(144, 144, 144));
  for (i = 1; i < 10; i++) {
    int posx = rect().width() * i / 10;
    p.drawLine(posx, 1, posx, COMBOHIST_RESOLUTION_H);
  }

  if (m_values.size() == 0) return;

  p.setPen(Qt::black);

  // draw each histogram
  for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
    int v = m_values[i];
    if (v <= 0) continue;
    int x = 1 + i;
    p.drawLine(x, COMBOHIST_RESOLUTION_H + 1 - v, x, COMBOHIST_RESOLUTION_H);
  }

  // draw picked color's channel value
  if (m_pickedValue > -1) {
    p.setPen(Qt::white);
    int x = 1 + m_pickedValue;
    p.drawLine(x, 1, x, COMBOHIST_RESOLUTION_H);
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

RGBHistoGraph::RGBHistoGraph(QWidget *parent, int *channelValue)
    : ChannelHistoGraph(parent, channelValue) {
  m_histoImg = QImage(COMBOHIST_RESOLUTION_W, COMBOHIST_RESOLUTION_H,
                      QImage::Format_ARGB32_Premultiplied);
}

//-----------------------------------------------------------------------------

RGBHistoGraph::~RGBHistoGraph() {
  for (int i = 0; i < 3; i++) m_rgbValues[i].clear();
}

//-----------------------------------------------------------------------------

void RGBHistoGraph::setValues() {
  for (int chan = 0; chan < 3; chan++) {
    m_rgbValues[chan].clear();
    m_rgbValues[chan].resize(COMBOHIST_RESOLUTION_W);

    int i;

    // normalize with the maximum value
    int maxValue = 1;
    for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
      int count = m_channelValuePtr[COMBOHIST_RESOLUTION_W * chan + i];
      if (maxValue < count) maxValue = count;
    }

    for (i = 0; i < COMBOHIST_RESOLUTION_W; i++) {
      int v = m_channelValuePtr[COMBOHIST_RESOLUTION_W * chan + i];
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
    imgPainter.setPen((chan == 0) ? Qt::red
                                  : (chan == 1) ? Qt::green : Qt::blue);

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
    linearGrad.setColorAt(1, m_color);
  }

  p.setBrush(QBrush(linearGrad));
  p.setPen(Qt::NoPen);
  p.drawRect(rect());
}

//=============================================================================
// ChannelHisto
//-----------------------------------------------------------------------------
ChannelHisto::ChannelHisto(int channelIndex, int *channelValue,
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
    label = tr("RGBA");
    color = Qt::white;
    break;
  }

  if (channelIndex != 4)
    m_histogramGraph = new ChannelHistoGraph(this, channelValue);
  else
    m_histogramGraph = new RGBHistoGraph(this, channelValue);

  m_colorBar = new ChannelColorBar(this, color);

  // show/hide the alpha channel
  QPushButton *showAlphaChannelButton = 0;
  if (channelIndex == 3) {
    showAlphaChannelButton = new QPushButton("", this);
    showAlphaChannelButton->setObjectName("FxSettingsPreviewShowButton");
    showAlphaChannelButton->setFixedSize(15, 15);
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
}

//=============================================================================
// ComboHistoRGBLabel
//-----------------------------------------------------------------------------
ComboHistoRGBLabel::ComboHistoRGBLabel(QColor color, QWidget *parent)
    : QWidget(parent), m_color(color) {
  setFixedSize(COMBOHIST_RESOLUTION_W, 30);
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

  p.drawText(rect(), Qt::AlignCenter, tr("R:%1 G:%2 B:%3")
                                          .arg(m_color.red())
                                          .arg(m_color.green())
                                          .arg(m_color.blue()));
}

//=============================================================================
// ComboHistogram
//-----------------------------------------------------------------------------

ComboHistogram::ComboHistogram(QWidget *parent)
    : QWidget(parent), m_raster(0), m_palette(0) {
  for (int chan        = 0; chan < 4; chan++)
    m_histograms[chan] = new ChannelHisto(chan, &m_channelValue[chan][0], this);
  m_histograms[4]      = new ChannelHisto(4, &m_channelValue[0][0], this);

  // RGB label
  m_rgbLabel = new ComboHistoRGBLabel(QColor(128, 128, 128), this);
  m_rgbLabel->setStyleSheet("font-size: 18px;");

  m_rectAverageRgbLabel = new ComboHistoRGBLabel(QColor(128, 128, 128), this);
  m_rectAverageRgbLabel->setStyleSheet("font-size: 18px;");

  m_xPosLabel = new QLabel("", this);
  m_yPosLabel = new QLabel("", this);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);
  {
    mainLayout->addWidget(m_histograms[4]);  // RGB

    mainLayout->addWidget(new QLabel(tr("Picked Color"), this), 0,
                          Qt::AlignLeft | Qt::AlignVCenter);
    mainLayout->addWidget(m_rgbLabel, 0, Qt::AlignCenter);

    mainLayout->addWidget(new QLabel(tr("Average Color (Ctrl + Drag)"), this),
                          0, Qt::AlignLeft | Qt::AlignVCenter);
    mainLayout->addWidget(m_rectAverageRgbLabel, 0, Qt::AlignCenter);

    QGridLayout *infoParamLay = new QGridLayout();
    infoParamLay->setHorizontalSpacing(3);
    infoParamLay->setVerticalSpacing(5);
    {
      infoParamLay->addWidget(new QLabel(tr("X:"), this), 0, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
      infoParamLay->addWidget(m_xPosLabel, 0, 1,
                              Qt::AlignLeft | Qt::AlignVCenter);
      infoParamLay->addWidget(new QLabel(tr("Y:"), this), 1, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
      infoParamLay->addWidget(m_yPosLabel, 1, 1,
                              Qt::AlignLeft | Qt::AlignVCenter);
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
}
//-----------------------------------------------------------------------------

ComboHistogram::~ComboHistogram() {
  memset(m_channelValue, 0, sizeof m_channelValue);
}

//-----------------------------------------------------------------------------

void ComboHistogram::setRaster(const TRasterP &raster,
                               const TPaletteP &palette) {
  if (palette.getPointer()) m_palette = palette;
  m_raster                            = raster;
  computeChannelsValue();

  for (int i = 0; i < 5; i++) m_histograms[i]->refleshValue();

  update();
}

//-----------------------------------------------------------------------------

void ComboHistogram::computeChannelsValue() {
  memset(m_channelValue, 0, sizeof m_channelValue);
  if (!m_raster.getPointer()) return;
  TRasterCM32P cmRaster = m_raster;
  bool isCmRaster       = !!cmRaster;

  TRaster64P raster64 = m_raster;
  bool is64bit        = !!raster64;

  int lx = m_raster->getLx();
  int ly = m_raster->getLy();
  if (lx > 1 && ly > 1) {
    int i, j;
    if (is64bit) {
      for (j = 0; j < ly; j++) {
        TPixel64 *pix_64 = raster64->pixels(j);
        for (i = 0; i < lx; i++, pix_64++) {
          int mValue = (int)byteFromUshort(pix_64->m);
          if (mValue != 0) {
            ++m_channelValue[0][(int)byteFromUshort(pix_64->r)];
            ++m_channelValue[1][(int)byteFromUshort(pix_64->g)];
            ++m_channelValue[2][(int)byteFromUshort(pix_64->b)];
          }
          ++m_channelValue[3][mValue];
        }
      }
    } else if (isCmRaster) {
      assert(m_palette);
      for (j = 0; j < ly; j++) {
        TPixelCM32 *pix_cm = cmRaster->pixels(j);
        for (i = 0; i < lx; i++, pix_cm++) {
          int styleId =
              pix_cm->getTone() < 127 ? pix_cm->getInk() : pix_cm->getPaint();
          TColorStyle *colorStyle = m_palette->getStyle(styleId);
          if (!colorStyle) continue;
          TPixel color = colorStyle->getAverageColor();
          int mValue   = color.m;
          if (mValue != 0) {
            ++m_channelValue[0][color.r];
            ++m_channelValue[1][color.g];
            ++m_channelValue[2][color.b];
          }
          ++m_channelValue[3][color.m];
        }
      }
    } else  // 8bpc raster
    {
      for (j = 0; j < ly; j++) {
        TPixel *pix = (TPixel *)m_raster->getRawData(0, j);
        for (i = 0; i < lx; i++, pix++) {
          int mValue = pix->m;
          if (mValue != 0) {
            ++m_channelValue[0][pix->r];
            ++m_channelValue[1][pix->g];
            ++m_channelValue[2][pix->b];
          }
          ++m_channelValue[3][pix->m];
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
    m_rgbLabel->setColorAndUpdate(Qt::transparent);
    m_xPosLabel->setText("");
    m_yPosLabel->setText("");
  } else {
    // show picked color's channel values
    m_histograms[0]->showCurrentChannelValue((int)pix.r);
    m_histograms[1]->showCurrentChannelValue((int)pix.g);
    m_histograms[2]->showCurrentChannelValue((int)pix.b);
    m_rgbLabel->setColorAndUpdate(QColor((int)pix.r, (int)pix.g, (int)pix.b));
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
        QColor((int)pix.r, (int)pix.g, (int)pix.b));
  }
}

//-----------------------------------------------------------------------------
