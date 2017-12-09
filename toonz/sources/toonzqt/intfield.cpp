

#include "toonzqt/intfield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

#include <QIntValidator>
#include <QSlider>
#include <QHBoxLayout>
#include <QAction>
#include <QFocusEvent>
#include <QPainter>

using namespace DVGui;

//=============================================================================
// RollerField
//-----------------------------------------------------------------------------

RollerField::RollerField(QWidget *parent)
    : QWidget(parent)
    , m_value(0)
    , m_minValue(-100000.0)
    , m_maxValue(100000.0)
    , m_xPos(0)
    , m_step(1.0) {
  setMinimumSize(43, 7);
}

//-----------------------------------------------------------------------------

void RollerField::setValue(double value) {
  if (m_value == value) return;
  if (value < m_minValue) m_value = m_minValue;
  if (value > m_maxValue) m_value = m_maxValue;

  m_value = value;
}

//-----------------------------------------------------------------------------

double RollerField::getValue() const { return m_value; }

//-----------------------------------------------------------------------------

void RollerField::setRange(double minValue, double maxValue) {
  m_minValue = minValue;
  m_maxValue = maxValue;
}

//-----------------------------------------------------------------------------

void RollerField::getRange(double &minValue, double &maxValue) {
  minValue = m_minValue;
  maxValue = m_maxValue;
}

//-----------------------------------------------------------------------------

void RollerField::paintEvent(QPaintEvent *e) {
  QPainter p(this);

  int w = width();

  drawArrow(p, QPointF(3, 3), QPointF(5, 5), QPointF(5, 1), true, Qt::black,
            Qt::black);
  drawArrow(p, QPointF(w - 4, 3), QPointF(w - 6, 5), QPointF(w - 6, 1), true,
            Qt::black, Qt::black);

  p.drawLine(QPoint(3, 3), QPoint(w - 4, 3));
}

//-----------------------------------------------------------------------------

void RollerField::mousePressEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::LeftButton) {
    m_xPos = e->pos().x();
    e->accept();
  }
}

//-----------------------------------------------------------------------------

void RollerField::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::LeftButton) {
    if (m_xPos < e->pos().x())
      addValue(true);
    else if (m_xPos > e->pos().x())
      removeValue(true);
    m_xPos = e->pos().x();
    e->accept();
  }
}

//-----------------------------------------------------------------------------

void RollerField::mouseReleaseEvent(QMouseEvent *e) {
  e->accept();
  emit valueChanged(false);
}

//-----------------------------------------------------------------------------

void RollerField::addValue(bool isDragging) {
  double newValue = tcrop(m_value + m_step, m_minValue, m_maxValue);
  if (newValue == m_value) return;
  m_value = newValue;
  emit valueChanged(isDragging);
}

//-----------------------------------------------------------------------------

void RollerField::removeValue(bool isDragging) {
  double newValue = tcrop(m_value - m_step, m_minValue, m_maxValue);
  if (newValue == m_value) return;
  m_value = newValue;
  emit valueChanged(isDragging);
}

//=============================================================================
// IntLineEdit
//-----------------------------------------------------------------------------

IntLineEdit::IntLineEdit(QWidget *parent, int value, int minValue, int maxValue,
                         int showedDigits)
    : LineEdit(parent), m_showedDigits(showedDigits) {
  setFixedWidth(54);

  m_validator = new QIntValidator(this);
  setValue(value);
  setRange(minValue, maxValue);
  setValidator(m_validator);
}

//-----------------------------------------------------------------------------

void IntLineEdit::setValue(int value) {
  int minValue, maxValue;
  getRange(minValue, maxValue);
  if (value < minValue) value = minValue;
  if (value > maxValue) value = maxValue;
  QString str;
  str.setNum(value);
  if (m_showedDigits > 0) {
    while (str.length() < m_showedDigits) str.push_front("0");
    while (str.length() > m_showedDigits) str.remove(0, 1);
  }
  setText(str);

  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa da
  // visualizzare
  // e' piu' lunga del campo le cifre che vengono troncate sono le ultime e non
  // le prime.
  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

int IntLineEdit::getValue() { return text().toInt(); }

//-----------------------------------------------------------------------------

void IntLineEdit::setRange(int minValue, int maxValue) {
  m_validator->setRange(minValue, maxValue);
}

//-----------------------------------------------------------------------------

void IntLineEdit::getRange(int &minValue, int &maxValue) {
  minValue = m_validator->bottom();
  maxValue = m_validator->top();
}

//-----------------------------------------------------------------------------

void IntLineEdit::setBottomRange(int minValue) {
  m_validator->setBottom(minValue);
}

//-----------------------------------------------------------------------------

void IntLineEdit::setTopRange(int maxValue) { m_validator->setTop(maxValue); }

//-----------------------------------------------------------------------------

void IntLineEdit::focusOutEvent(QFocusEvent *e) {
  int value = getValue();
  int minValue, maxValue;
  getRange(minValue, maxValue);

  if (e->lostFocus()) setValue(value);

  QLineEdit::focusOutEvent(e);
  m_isTyping = false;
}

//-----------------------------------------------------------------------------

// for fps edit in flip console
void IntLineEdit::setLineEditBackgroundColor(QColor color) {
  QString sheet = QString("background-color: rgb(") +
                  QString::number(color.red()) + QString(",") +
                  QString::number(color.green()) + QString(",") +
                  QString::number(color.blue()) + QString(",") +
                  QString::number(color.alpha()) + QString(");");
  setStyleSheet(sheet);
}

//-----------------------------------------------------------------------------

void IntLineEdit::mousePressEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::MiddleButton) {
    m_xMouse           = e->x();
    m_mouseDragEditing = true;
  } else {
    QLineEdit::mousePressEvent(e);
    if (!m_isTyping) {  // only the first click will select all
      selectAll();
      m_isTyping = true;
    }
  }
}

//-----------------------------------------------------------------------------

void IntLineEdit::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::MiddleButton) {
    setValue(getValue() + ((e->x() - m_xMouse) / 2));
    m_xMouse = e->x();
  } else
    QLineEdit::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

void IntLineEdit::mouseReleaseEvent(QMouseEvent *e) {
  if ((e->buttons() == Qt::NoButton && m_mouseDragEditing)) {
    m_mouseDragEditing = false;
    clearFocus();
  } else
    QLineEdit::mouseReleaseEvent(e);
}

//=============================================================================
// IntField
//-----------------------------------------------------------------------------

IntField::IntField(QWidget *parent, bool isMaxRangeLimited, bool isRollerHide)
    : QWidget(parent)
    , m_lineEdit(0)
    , m_slider(0)
    , m_roller(0)
    , m_isMaxRangeLimited(isMaxRangeLimited) {
  setObjectName("IntField");
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(5);

  QWidget *field = new QWidget(this);
  field->setMaximumWidth(43);
  QVBoxLayout *vLayout = new QVBoxLayout(field);
  vLayout->setMargin(0);
  vLayout->setSpacing(0);

  m_lineEdit = new DVGui::IntLineEdit(field);
  bool ret   = connect(m_lineEdit, SIGNAL(editingFinished()), this,
                     SLOT(onEditingFinished()));
  vLayout->addWidget(m_lineEdit);

  m_roller = new RollerField(field);
  ret      = ret && connect(m_roller, SIGNAL(valueChanged(bool)), this,
                       SLOT(onRollerValueChanged(bool)));
  vLayout->addWidget(m_roller);

  if (isRollerHide) enableRoller(false);

  layout->addWidget(field);

  m_slider = new QSlider(Qt::Horizontal, this);
  ret      = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                       SLOT(onSliderChanged(int)));
  ret = ret && connect(m_slider, SIGNAL(sliderReleased()), this,
                       SLOT(onSliderReleased()));

  ret = ret && connect(m_lineEdit, SIGNAL(editingFinished()), this,
                       SIGNAL(valueEditedByHand()));
  ret = ret && connect(m_slider, SIGNAL(sliderReleased()), this,
                       SIGNAL(valueEditedByHand()));
  layout->addWidget(m_slider);

  setValues(0, 0, 100);

  setLayout(layout);
  assert(ret);
}

//-----------------------------------------------------------------------------

void IntField::getRange(int &minValue, int &maxValue) {
  double min, max;
  m_roller->getRange(min, max);
  minValue = tround(min);
  maxValue = tround(max);
}

//-----------------------------------------------------------------------------

void IntField::setRange(int minValue, int maxValue) {
  m_lineEdit->setRange(minValue, m_isMaxRangeLimited
                                     ? maxValue
                                     : (std::numeric_limits<int>::max)());
  m_slider->setRange(minValue, maxValue);
  m_roller->setRange(minValue, maxValue);
}

//-----------------------------------------------------------------------------

void IntField::setValue(int value) {
  if (m_lineEdit->getValue() == value) return;
  m_lineEdit->setValue(value);
  m_slider->setSliderPosition(value);
  m_roller->setValue((double)value);
}

//-----------------------------------------------------------------------------

int IntField::getValue() { return (m_lineEdit->getValue()); }

//-----------------------------------------------------------------------------

void IntField::setValues(int value, int minValue, int maxValue) {
  setRange(minValue, maxValue);
  setValue(value);
}

//-----------------------------------------------------------------------------

void IntField::enableSlider(bool enable) {
  m_slider->setEnabled(enable);
  if (enable)
    m_slider->show();
  else
    m_slider->hide();
}

//-----------------------------------------------------------------------------

bool IntField::sliderIsEnabled() { return m_slider->isEnabled(); }

//-----------------------------------------------------------------------------

void IntField::enableRoller(bool enable) {
  m_roller->setEnabled(enable);
  if (enable)
    m_roller->show();
  else
    m_roller->hide();
}

//-----------------------------------------------------------------------------

bool IntField::rollerIsEnabled() { return m_roller->isEnabled(); }

//-----------------------------------------------------------------------------

void IntField::setLineEditBackgroundColor(QColor color) {
  m_lineEdit->setLineEditBackgroundColor(color);
}

//-----------------------------------------------------------------------------

void IntField::onSliderChanged(int value) {
  // Controllo necessario per evitare che il segnale di cambiamento venga emesso
  // piu' volte.
  if (m_lineEdit->getValue() == value ||
      ((int)m_roller->getValue() == value && m_roller->isVisible()))
    return;
  m_lineEdit->setValue(value);
  m_roller->setValue((double)value);
  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa
  // da visualizzare e' piu' lunga del campo le cifre che vengono troncate sono
  // le ultime e non le prime (dovrebbero essere quelle dopo la virgola).
  m_lineEdit->setCursorPosition(0);
  emit valueChanged(true);
}

//-----------------------------------------------------------------------------

void IntField::onEditingFinished() {
  double value = m_lineEdit->getValue();
  // Controllo necessario per evitare che il segnale di cambiamento venga emesso
  // piu' volte.
  if ((m_slider->value() == value && m_slider->isVisible()) ||
      (int)m_roller->getValue() == value && m_roller->isVisible())
    return;
  m_slider->setValue(value);
  m_roller->setValue((double)value);
  emit valueChanged(false);
}

//-----------------------------------------------------------------------------

void IntField::onRollerValueChanged(bool isDragging) {
  int value = m_roller->getValue();
  if (value == m_lineEdit->getValue()) {
    assert(m_slider->value() == value || !m_slider->isVisible());
    // Se isDragging e' falso e' giusto che venga emessa la notifica di
    // cambiamento.
    if (!isDragging) emit valueChanged(isDragging);
    return;
  }
  m_slider->setValue(value);
  m_lineEdit->setValue(value);

  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa
  // da visualizzare e' piu' lunga del campo le cifre che vengono troncate sono
  // le ultime e non le prime (dovrebbero essere quelle dopo la virgola).
  m_lineEdit->setCursorPosition(0);

  emit valueChanged(isDragging);
}
