

#include "toonzqt/intpairfield.h"

#include "toonzqt/dvdialog.h"

#include "tcommon.h"

#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QSlider>
#include <QHBoxLayout>

using namespace DVGui;

#ifdef MACOSX
#define MARGIN_OFFSET 7
#else
#define MARGIN_OFFSET 2
#endif

//=============================================================================
// IntPairField
//-----------------------------------------------------------------------------

IntPairField::IntPairField(QWidget *parent, bool isMaxRangeLimited)
    : QWidget(parent)
    , m_values(0, 0)
    , m_minValue(0)
    , m_maxValue(100)
    , m_grabOffset(0)
    , m_grabIndex(-1)
    , m_leftMargin(52)
    , m_rightMargin(52)
    , m_isMaxRangeLimited(isMaxRangeLimited)
    , m_isLinear(true) {
  setObjectName("IntPairField");
  setFixedHeight(WidgetHeight);

  m_leftLabel    = new QLabel("", this);
  m_leftLineEdit = new DVGui::IntLineEdit(this);

  m_rightLabel    = new QLabel("", this);
  m_rightLineEdit = new DVGui::IntLineEdit(this);

  m_leftLineEdit->setFixedWidth(40);
  m_rightLineEdit->setFixedWidth(40);

  //----layout
  QHBoxLayout *m_mainLayout = new QHBoxLayout;
  m_mainLayout->setMargin(0);
  m_mainLayout->setSpacing(5);
  {
    m_mainLayout->addWidget(m_leftLabel, 1);
    m_mainLayout->addWidget(m_leftLineEdit, 1, Qt::AlignLeft);
    m_mainLayout->addSpacing(35);
    m_mainLayout->addStretch(100);
    m_mainLayout->addWidget(m_rightLabel, 1);
    m_mainLayout->addWidget(m_rightLineEdit, 1, Qt::AlignRight);
  }
  setLayout(m_mainLayout);

  //---signal-slot connections
  bool ret = connect(m_leftLineEdit, SIGNAL(editingFinished()),
                     SLOT(onLeftEditingFinished()));
  ret      = ret && connect(m_rightLineEdit, SIGNAL(editingFinished()),
                       SLOT(onRightEditingFinished()));

  assert(ret);
}

//-----------------------------------------------------------------------------

int IntPairField::pos2value(int x) const {
  int xMin = m_leftMargin, xMax = width() - m_rightMargin - 1;
  if (m_isLinear)
    return m_minValue + (m_maxValue - m_minValue) * (x - xMin) / (xMax - xMin);

  // nonlinear slider case
  double posRatio = (double)(x - xMin) / (double)(xMax - xMin);
  double t;
  if (posRatio <= 0.5)
    t = 0.04 * posRatio;
  else if (posRatio <= 0.75)
    t = -0.02 + 0.08 * posRatio;
  else if (posRatio <= 0.9)
    t = -0.26 + 0.4 * posRatio;
  else
    t = -8.0 + 9.0 * posRatio;
  return m_minValue + (int)((double)(m_maxValue - m_minValue) * t);
}

//-----------------------------------------------------------------------------

int IntPairField::value2pos(int v) const {
  int xMin = m_leftMargin, xMax = width() - m_rightMargin - 1;
  if (m_isLinear)
    return xMin +
           ((xMax - xMin) * (v - m_minValue)) / (m_maxValue - m_minValue);

  // nonlinear slider case
  double valueRatio =
      (double)(v - m_minValue) / (double)(m_maxValue - m_minValue);
  double t;
  if (valueRatio <= 0.02)
    t = valueRatio / 0.04;
  else if (valueRatio <= 0.04)
    t = (valueRatio + 0.02) / 0.08;
  else if (valueRatio <= 0.1)
    t = (valueRatio + 0.26) / 0.4;
  else
    t = (valueRatio + 8.0) / 9.0;
  return xMin + (int)(t * (double)(xMax - xMin));
}

//-----------------------------------------------------------------------------

void IntPairField::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setBrush(Qt::NoBrush);

  int x0 = value2pos(m_minValue);
  int x1 = value2pos(m_maxValue);
  int y  = height() / 2;

  p.setPen(QPen(getDarkLineColor(), 4));
  p.drawLine(x0 - 1, y, x1, y);

  p.setPen(Qt::black);

  int y1 = height() - 1;
  int x;
  x                = value2pos(m_values.first);
  QRect sliderRect = QRect(x0, -5, x1 - x0 + 1, 10);

  if (sliderRect.contains(QPoint(x, 0)))
    p.drawPixmap(x - m_handleLeftPixmap.width() + 1, 2, m_handleLeftPixmap);
  else
    p.drawPixmap(sliderRect.right() - m_handleLeftPixmap.width() + 1, 2,
                 m_handleLeftGrayPixmap);

  x = value2pos(m_values.second);

  if (sliderRect.contains(QPoint(x, 0)))
    p.drawPixmap(x, 2, m_handleRightPixmap);
  else
    p.drawPixmap(sliderRect.right(), 2, m_handleRightGrayPixmap);
}

//-----------------------------------------------------------------------------

void IntPairField::setLeftText(const QString &text) {
  QPoint pos       = m_leftLabel->pos();
  QString oldText  = m_leftLabel->text();
  int oldLabelSize = fontMetrics().width(oldText);
  int newLabelSize = fontMetrics().width(text);
  int labelSize    = newLabelSize - oldLabelSize;
  m_leftMargin += labelSize + MARGIN_OFFSET;
  m_leftLabel->setText(text);
  update();
}

//-----------------------------------------------------------------------------

void IntPairField::setRightText(const QString &text) {
  QString oldText  = m_rightLabel->text();
  int oldLabelSize = fontMetrics().width(oldText);
  int newLabelSize = fontMetrics().width(text);
  int labelSize    = newLabelSize - oldLabelSize;
  m_rightMargin += labelSize + MARGIN_OFFSET;
  m_rightLabel->setText(text);
  update();
}

//-----------------------------------------------------------------------------

void IntPairField::setLabelsEnabled(bool enable) {
  if (!enable) {
    m_rightLabel->hide();
    m_leftLabel->hide();
  } else {
    m_rightLabel->show();
    m_leftLabel->show();
  }
}

//-----------------------------------------------------------------------------

void IntPairField::setValue(int value) {
  value = tcrop(value, m_minValue, m_maxValue);
  value = tround(value * 100) * 0.01;
  if (m_grabIndex == 0)  // Left grab
  {
    m_values.first = value;
    m_leftLineEdit->setValue(m_values.first);

    if (value > m_values.second) {
      m_values.second = value;
      m_rightLineEdit->setValue(m_values.second);
    }
  } else  // Right grab
  {
    m_values.second = value;
    m_rightLineEdit->setValue(m_values.second);

    if (value < m_values.first) {
      m_values.first = value;
      m_leftLineEdit->setValue(m_values.first);
    }
  }
}

//-----------------------------------------------------------------------------

void IntPairField::setValues(const std::pair<int, int> &values) {
  assert(values.first <= values.second);
  m_values.first = tcrop(values.first, m_minValue, m_maxValue);
  m_leftLineEdit->setValue(m_values.first);

  m_values.second = values.second;
  if (m_isMaxRangeLimited)
    m_values.second = tcrop(values.second, m_values.first, m_maxValue);
  m_rightLineEdit->setValue(m_values.second);

  update();
}

//-----------------------------------------------------------------------------

void IntPairField::setRange(int minValue, int maxValue) {
  m_minValue = minValue;
  m_maxValue = maxValue;

  if (!m_isMaxRangeLimited) maxValue = (std::numeric_limits<int>::max)();
  m_leftLineEdit->setRange(minValue, maxValue);
  m_rightLineEdit->setRange(minValue, maxValue);

  update();
}

//-----------------------------------------------------------------------------

void IntPairField::getRange(int &minValue, int &maxValue) {
  minValue = m_minValue;
  maxValue = m_maxValue;
}

//-----------------------------------------------------------------------------

void IntPairField::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int x = event->pos().x();
    int cur0, cur1;
    if (m_values.first > m_maxValue)
      cur0 = value2pos(m_maxValue) - 5;
    else
      cur0 = value2pos(m_values.first);
    if (m_values.second > m_maxValue)
      cur1 = value2pos(m_maxValue);
    else
      cur1 = value2pos(m_values.second);
    int d0 = abs(cur0 - x);
    int d1 = abs(cur1 - x);
    int d, cur;
    if (d0 < d1 || (d0 == d1 && x < cur0)) {
      d           = d0;
      cur         = cur0;
      m_grabIndex = 0;
    } else {
      d           = d1;
      cur         = cur1;
      m_grabIndex = 1;
    }
    if (d < 6)
      m_grabOffset = cur - x;
    else {
      m_grabOffset = 0;
      setValue(pos2value(x));
      emit valuesChanged(true);
      update();
    }
  }
}

//-----------------------------------------------------------------------------

void IntPairField::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons()) {
    std::pair<int, int> oldValues = m_values;
    int x                         = event->pos().x() + m_grabOffset;
    setValue(pos2value(x));
    if (oldValues == m_values) return;

    emit valuesChanged(true);
    update();
  }
}

//-----------------------------------------------------------------------------

void IntPairField::mouseReleaseEvent(QMouseEvent *event) {
  m_grabOffset = 0;
  m_grabIndex  = 0;
  emit valuesChanged(false);
}

//-----------------------------------------------------------------------------

void IntPairField::onLeftEditingFinished() {
  int value = m_leftLineEdit->getValue();
  if (value == m_values.first) return;
  if (!m_isMaxRangeLimited && value < m_minValue)
    value = m_minValue;
  else if (m_isMaxRangeLimited)
    value = tcrop(value, m_minValue, m_maxValue);
  m_values.first = value;
  if (m_values.first > m_values.second) {
    m_values.second = m_values.first;
    m_rightLineEdit->setValue(m_values.second);
  }
  emit valuesChanged(false);
  update();
}

//-----------------------------------------------------------------------------

void IntPairField::onRightEditingFinished() {
  int value = m_rightLineEdit->getValue();
  if (value == m_values.second) return;
  if (m_isMaxRangeLimited) value = tcrop(value, m_minValue, m_maxValue);
  m_values.second = value;
  if (m_values.second < m_values.first) {
    m_values.first = m_values.second;
    m_leftLineEdit->setValue(m_values.first);
  }
  emit valuesChanged(false);
  update();
}
