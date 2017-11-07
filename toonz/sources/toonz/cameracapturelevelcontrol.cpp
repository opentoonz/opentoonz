#include "cameracapturelevelcontrol.h"

#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>
#include <iostream>

using namespace DVGui;

namespace {
const int HISTOGRAM_HEIGHT    = 50;
const int SIDE_MARGIN         = 7;
static bool histogramObtained = false;

// returns the horizontal position of gamma slider (0-255)
int gammaToHPos(float gamma, int black, int white) {
  float ratio = std::pow(0.5f, gamma);
  return black + (int)std::round((float)(white - black) * ratio);
}

// returns the gamma value from the slider position
float hPosToGamma(int hPos, int black, int white) {
  if (hPos <= black + 1)
    return 9.99f;
  else if (hPos >= white - 1)
    return 0.01f;

  float ratio = (float)(hPos - black) / (float)(white - black);

  float gamma  = std::log(ratio) / std::log(0.5);
  int decimals = 2;
  gamma =
      std::round(gamma * std::pow(10.0, decimals)) / std::pow(10.0, decimals);
  return gamma;
}

void drawSliderHandle(QPoint pos, QPainter& p, QColor color, bool selected) {
  p.setPen((selected) ? Qt::yellow : Qt::black);
  p.setBrush(color);

  p.translate(pos);

  static const QPoint points[5] = {QPoint(0, 0), QPoint(-6, 8), QPoint(-3, 12),
                                   QPoint(3, 12), QPoint(6, 8)};
  p.drawConvexPolygon(points, 5);
  p.resetTransform();
}
};
//-----------------------------------------------------------------------------

CameraCaptureLevelHistogram::CameraCaptureLevelHistogram(QWidget* parent)
    : QFrame(parent)
    , m_histogramCue(false)
    , m_currentItem(None)
    , m_black(0)
    , m_white(255)
    , m_gamma(1.0)
    , m_threshold(128)
    , m_offset(0)
    , m_mode(Color_GrayScale)
    , m_histogramData(256) {
  setFixedSize(256 + SIDE_MARGIN * 2, HISTOGRAM_HEIGHT + 15);
  setMouseTracking(true);

  m_histogramData.fill(0);
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelHistogram::updateHistogram(QImage& image) {
  // obtain histogram only when clicked
  if (!m_histogramCue) return;

  QVector<int> tmpHisto(256);
  tmpHisto.fill(0);
  for (int y = 0; y < image.height(); y++) {
    QRgb* rgb_p = (QRgb*)(image.scanLine(y));
    for (int x = 0; x < image.width(); x++, rgb_p++) {
      tmpHisto[qGray(*rgb_p)]++;
    }
  }
  // obtain max value and normalize
  int max = 0;
  for (int c = 0; c < 256; c++) {
    if (tmpHisto.at(c) > max) max = tmpHisto.at(c);
  }
  for (int c = 0; c < 256; c++) {
    m_histogramData[c] = tmpHisto.at(c) * HISTOGRAM_HEIGHT / max;
  }
  histogramObtained = true;
  update();
  m_histogramCue = false;
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelHistogram::paintEvent(QPaintEvent* event) {
  QPainter p(this);

  QRect histoRect(SIDE_MARGIN, 1, 256, HISTOGRAM_HEIGHT);

  // draw histogram
  p.setPen(Qt::black);
  p.setBrush((m_currentItem == Histogram) ? Qt::darkGray : QColor(32, 32, 32));
  p.drawRect(histoRect.adjusted(-1, -1, 0, 0));

  if (histogramObtained) {
    p.setPen(Qt::white);
    for (int c = 0; c < 256; c++) {
      if (m_histogramData.at(c) == 0) continue;
      p.drawLine(histoRect.bottomLeft() + QPoint(c, 0),
                 histoRect.bottomLeft() + QPoint(c, -m_histogramData.at(c)));
    }
  }
  if (!histogramObtained || m_currentItem == Histogram) {
    p.setPen(Qt::white);
    p.drawText(histoRect, Qt::AlignCenter, tr("Click to Update Histogram"));
  }

  p.setRenderHint(QPainter::Antialiasing);
  // draw slider handles
  QPoint sliderBasePos(SIDE_MARGIN, HISTOGRAM_HEIGHT + 2);
  if (m_mode == Color_GrayScale) {
    // black
    drawSliderHandle(sliderBasePos + QPoint(m_black, 0), p, QColor(32, 32, 32),
                     m_currentItem == BlackSlider);
    // gamma
    drawSliderHandle(
        sliderBasePos + QPoint(gammaToHPos(m_gamma, m_black, m_white), 0), p,
        Qt::gray, m_currentItem == GammaSlider);
    // white
    drawSliderHandle(sliderBasePos + QPoint(m_white, 0), p,
                     QColor(220, 220, 220), m_currentItem == WhiteSlider);
  } else if (m_mode == BlackAndWhite)
    // threshold
    drawSliderHandle(sliderBasePos + QPoint(m_threshold, 0), p, Qt::gray,
                     m_currentItem == ThresholdSlider);
  p.setRenderHint(QPainter::Antialiasing, false);
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelHistogram::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  if (m_currentItem == Histogram) {
    m_histogramCue = true;
    return;
  }
  if (m_currentItem == None) return;
  QPoint pos = event->pos();
  if (m_currentItem == BlackSlider)
    m_offset = pos.x() - SIDE_MARGIN - m_black;
  else if (m_currentItem == GammaSlider)
    m_offset = pos.x() - SIDE_MARGIN - gammaToHPos(m_gamma, m_black, m_white);
  else if (m_currentItem == BlackSlider)
    m_offset = pos.x() - SIDE_MARGIN - m_white;
  else if (m_currentItem == ThresholdSlider)
    m_offset = pos.x() - SIDE_MARGIN - m_threshold;
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelHistogram::mouseMoveEvent(QMouseEvent* event) {
  QPoint pos = event->pos();
  if (event->buttons() & Qt::LeftButton) {
    if (m_currentItem == None || m_currentItem == Histogram) return;

    int hPos     = pos.x() - SIDE_MARGIN - m_offset;
    bool changed = false;
    if (m_currentItem == BlackSlider) {
      if (hPos < 0)
        hPos = 0;
      else if (hPos > m_white - 2)
        hPos = m_white - 2;
      if (hPos != m_black) {
        m_black = hPos;
        changed = true;
      }
    } else if (m_currentItem == GammaSlider) {
      if (hPos < m_black + 1)
        hPos = m_black + 1;
      else if (hPos > m_white - 1)
        hPos      = m_white - 1;
      float gamma = hPosToGamma(hPos, m_black, m_white);
      if (gamma != m_gamma) {
        m_gamma = gamma;
        changed = true;
      }
    } else if (m_currentItem == WhiteSlider) {
      if (hPos < m_black + 2)
        hPos = m_black + 2;
      else if (hPos > 255)
        hPos = 255;
      if (hPos != m_white) {
        m_white = hPos;
        changed = true;
      }
    } else if (m_currentItem == ThresholdSlider) {
      if (hPos < 0)
        hPos = 0;
      else if (hPos > 255)
        hPos = 255;
      if (hPos != m_threshold) {
        m_threshold = hPos;
        changed     = true;
      }
    }
    if (changed) {
      update();
      emit valueChange(m_currentItem);
    }
    return;
  }

  // on hover
  LevelControlItem oldItem = m_currentItem;
  m_currentItem            = None;
  setToolTip("");
  QRect histoRect(5, 1, 256, HISTOGRAM_HEIGHT);
  if (histoRect.contains(pos)) {
    setToolTip(tr("Click to Update Histogram"));
    m_currentItem = Histogram;
  } else {
    // slider handles
    QPoint sliderBasePos(SIDE_MARGIN, HISTOGRAM_HEIGHT + 2);
    QRect sliderRect(-6, 0, 12, 12);
    if (m_mode == Color_GrayScale) {
      // white
      if (sliderRect.translated(sliderBasePos + QPoint(m_white, 0))
              .contains(pos)) {
        m_currentItem = WhiteSlider;
        setToolTip(tr("Drag to Move White Point"));
      }
      // gamma
      else if (sliderRect
                   .translated(
                       sliderBasePos +
                       QPoint(gammaToHPos(m_gamma, m_black, m_white), 0))
                   .contains(pos)) {
        m_currentItem = GammaSlider;
        setToolTip(tr("Drag to Move Gamma"));
      }
      // black
      else if (sliderRect.translated(sliderBasePos + QPoint(m_black, 0))
                   .contains(pos)) {
        m_currentItem = BlackSlider;
        setToolTip(tr("Drag to Move Black Point"));
      }
    } else if (m_mode == BlackAndWhite) {
      // threshold
      if (sliderRect.translated(sliderBasePos + QPoint(m_threshold, 0))
              .contains(pos)) {
        m_currentItem = ThresholdSlider;
        setToolTip(tr("Drag to Move Threshold Point"));
      }
    }
  }
  if (oldItem != m_currentItem) update();
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelHistogram::mouseReleaseEvent(QMouseEvent* event) {
  m_offset = 0;
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelHistogram::leaveEvent(QEvent* event) {
  m_currentItem = None;
  m_offset      = 0;
  update();
}

//=============================================================================

CameraCaptureLevelControl::CameraCaptureLevelControl(QWidget* parent)
    : QFrame(parent) {
  m_histogram    = new CameraCaptureLevelHistogram(this);
  m_blackFld     = new IntLineEdit(this, m_histogram->black(), 0, 253);
  m_whiteFld     = new IntLineEdit(this, m_histogram->white(), 2, 255);
  m_thresholdFld = new IntLineEdit(this, m_histogram->threshold(), 0, 255);
  m_gammaFld     = new DoubleLineEdit(this, m_histogram->gamma());

  m_blackFld->setToolTip(tr("Black Point Value"));
  m_whiteFld->setToolTip(tr("White Point Value"));
  m_thresholdFld->setToolTip(tr("Threshold Value"));
  m_thresholdFld->hide();
  m_gammaFld->setRange(0.01, 9.99);
  m_gammaFld->setDecimals(2);
  m_gammaFld->setFixedWidth(54);
  m_gammaFld->setToolTip(tr("Gamma Value"));

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(4);
  {
    mainLay->addWidget(m_histogram, 0, Qt::AlignHCenter);
    QHBoxLayout* fieldsLay = new QHBoxLayout();
    fieldsLay->setMargin(1);
    fieldsLay->setSpacing(0);
    {
      fieldsLay->addWidget(m_blackFld, 0);
      fieldsLay->addStretch(1);
      fieldsLay->addWidget(m_gammaFld, 0);
      fieldsLay->addWidget(m_thresholdFld, 0);
      fieldsLay->addStretch(1);
      fieldsLay->addWidget(m_whiteFld, 0);
    }
    mainLay->addLayout(fieldsLay, 0);
  }
  setLayout(mainLay);

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  connect(m_histogram, SIGNAL(valueChange(int)), this,
          SLOT(onHistogramValueChanged(int)));
  connect(m_blackFld, SIGNAL(editingFinished()), this, SLOT(onFieldChanged()));
  connect(m_whiteFld, SIGNAL(editingFinished()), this, SLOT(onFieldChanged()));
  connect(m_gammaFld, SIGNAL(editingFinished()), this, SLOT(onFieldChanged()));
  connect(m_thresholdFld, SIGNAL(editingFinished()), this,
          SLOT(onFieldChanged()));
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelControl::onHistogramValueChanged(int itemId) {
  if (itemId == CameraCaptureLevelHistogram::BlackSlider) {
    m_blackFld->setValue(m_histogram->black());
    m_whiteFld->setRange(m_histogram->black() + 2, 255);
  } else if (itemId == CameraCaptureLevelHistogram::WhiteSlider) {
    m_whiteFld->setValue(m_histogram->white());
    m_blackFld->setRange(0, m_histogram->white() - 2);
  } else if (itemId == CameraCaptureLevelHistogram::GammaSlider) {
    m_gammaFld->setValue(m_histogram->gamma());
  } else if (itemId == CameraCaptureLevelHistogram::ThresholdSlider) {
    m_thresholdFld->setValue(m_histogram->threshold());
  }
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelControl::onFieldChanged() {
  if (m_histogram->mode() == CameraCaptureLevelHistogram::Color_GrayScale)
    m_histogram->setValues(m_blackFld->getValue(), m_whiteFld->getValue(),
                           m_gammaFld->getValue());
  else if (m_histogram->mode() == CameraCaptureLevelHistogram::BlackAndWhite)
    m_histogram->setThreshold(m_thresholdFld->getValue());

  m_histogram->update();
}

//-----------------------------------------------------------------------------

void CameraCaptureLevelControl::setMode(bool color_grayscale) {
  m_histogram->setMode((color_grayscale)
                           ? CameraCaptureLevelHistogram::Color_GrayScale
                           : CameraCaptureLevelHistogram::BlackAndWhite);
  m_blackFld->setVisible(color_grayscale);
  m_whiteFld->setVisible(color_grayscale);
  m_gammaFld->setVisible(color_grayscale);
  m_thresholdFld->setVisible(!color_grayscale);
  update();
}
