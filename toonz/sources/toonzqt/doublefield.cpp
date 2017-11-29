

#include <math.h>
#include "toonzqt/doublefield.h"
#include "toonzqt/dvdialog.h"
#include "tunit.h"

#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QFocusEvent>
#include <QSlider>

using namespace DVGui;

//=============================================================================
// DoubleValueLineEdit
//-----------------------------------------------------------------------------

void DoubleValueLineEdit::focusOutEvent(QFocusEvent *e) {
  double value = getValue();
  double minValue, maxValue;
  getRange(minValue, maxValue);

  bool isOutOfRange;
  /*---誤差を吸収する---*/
  MeasuredDoubleLineEdit *lineEdit =
      qobject_cast<MeasuredDoubleLineEdit *>(this);
  if (lineEdit) {
    int decimal  = lineEdit->getDecimals();
    isOutOfRange = (value < minValue - pow(0.1, decimal + 1) ||
                    value > maxValue + pow(0.1, decimal + 1));
  } else
    isOutOfRange = (value < minValue || value > maxValue);

  if (isOutOfRange) {
    setValue(tcrop(value, minValue, maxValue));
    emit editingFinished();
  }
  QLineEdit::focusOutEvent(e);
  m_isTyping = false;
}

//-----------------------------------------------------------------------------

void DoubleValueLineEdit::mousePressEvent(QMouseEvent *e) {
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

void DoubleValueLineEdit::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons() == Qt::MiddleButton) {
    setValue(getValue() + ((e->x() - m_xMouse) / 2));
    m_xMouse = e->x();
    emit valueChanged();
  } else
    QLineEdit::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

void DoubleValueLineEdit::mouseReleaseEvent(QMouseEvent *e) {
  if ((e->buttons() == Qt::NoButton && m_mouseDragEditing)) {
    m_mouseDragEditing = false;
    clearFocus();
  } else
    QLineEdit::mouseReleaseEvent(e);
}

//=============================================================================
// DoubleValueField
//-----------------------------------------------------------------------------

DoubleValueField::DoubleValueField(QWidget *parent,
                                   DoubleValueLineEdit *lineEdit)
    : QWidget(parent), m_lineEdit(lineEdit), m_slider(0), m_roller(0) {
  assert(m_lineEdit);

  QWidget *field = new QWidget(this);
  m_roller       = new RollerField(field);
  m_slider       = new QSlider(Qt::Horizontal, this);

  field->setMaximumWidth(100);

  //---layout
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);
  layout->setSpacing(5);
  {
    QVBoxLayout *vLayout = new QVBoxLayout(field);
    vLayout->setMargin(0);
    vLayout->setSpacing(0);
    {
      vLayout->addWidget(m_lineEdit);
      vLayout->addWidget(m_roller);
    }
    layout->addWidget(field);
    layout->addWidget(m_slider);
  }
  setLayout(layout);

  //----signal/slot connections
  bool ret = true;
  ret      = ret && connect(m_lineEdit, SIGNAL(valueChanged()),
                       SLOT(onLineEditValueChanged()));
  ret = ret && connect(m_roller, SIGNAL(valueChanged(bool)),
                       SLOT(onRollerValueChanged(bool)));
  ret = ret && connect(m_slider, SIGNAL(valueChanged(int)),
                       SLOT(onSliderChanged(int)));
  ret = ret &&
        connect(m_slider, SIGNAL(sliderReleased()), SLOT(onSliderReleased()));
  ret = ret && connect(m_lineEdit, SIGNAL(editingFinished()), this,
                       SIGNAL(valueEditedByHand()));
  ret = ret && connect(m_slider, SIGNAL(sliderReleased()), this,
                       SIGNAL(valueEditedByHand()));
  assert(ret);

  m_spaceWidget = new QWidget();
  m_spaceWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  layout->addWidget(m_spaceWidget, 1, Qt::AlignLeft);

  setRange(-100.0, 100.0);
}

//-----------------------------------------------------------------------------

void DoubleValueField::getRange(double &minValue, double &maxValue) {
  m_lineEdit->getRange(minValue, maxValue);
}

//-----------------------------------------------------------------------------

void DoubleValueField::setRange(double minValue, double maxValue) {
  m_lineEdit->setRange(minValue, maxValue);

  m_roller->setRange(minValue, maxValue);

  int dicimal   = m_lineEdit->getDecimals();
  int sliderMax = maxValue * pow(10., dicimal);
  int sliderMin = minValue * pow(10., dicimal);

  m_slider->setRange(sliderMin, sliderMax);

  enableSlider(maxValue - minValue < 10000);
}

//-----------------------------------------------------------------------------

void DoubleValueField::setValue(double value) {
  if (m_lineEdit->getValue() == value) return;
  m_lineEdit->setValue(value);
  m_roller->setValue(value);

  int dicimal     = m_lineEdit->getDecimals();
  int sliderValue = value * pow(10., dicimal);

  m_slider->setValue(sliderValue);
  // forzo il repaint... non sempre si aggiorna e l'update non sembra risolvere
  // il ptroblema!!!
  m_slider->repaint();
}

//-----------------------------------------------------------------------------

double DoubleValueField::getValue() { return (m_lineEdit->getValue()); }

//-----------------------------------------------------------------------------

void DoubleValueField::setValues(double value, double minValue,
                                 double maxValue) {
  setRange(minValue, maxValue);
  setValue(value);
}

//-----------------------------------------------------------------------------

void DoubleValueField::enableSlider(bool enable) {
  m_slider->setEnabled(enable);
  m_spaceWidget->setEnabled(!enable);
  if (enable) {
    m_slider->show();
    m_spaceWidget->hide();
  } else {
    m_slider->hide();
    m_spaceWidget->show();
  }
}

//-----------------------------------------------------------------------------

bool DoubleValueField::isSliderEnabled() { return m_slider->isEnabled(); }

//-----------------------------------------------------------------------------

void DoubleValueField::enableRoller(bool enable) {
  m_roller->setEnabled(enable);
  if (enable)
    m_roller->show();
  else
    m_roller->hide();
}

//-----------------------------------------------------------------------------

bool DoubleValueField::isRollerEnabled() { return m_roller->isEnabled(); }

//-----------------------------------------------------------------------------

void DoubleValueField::onSliderChanged(int value) {
  int dicimal = m_lineEdit->getDecimals();
  double val  = double(value) * pow(0.1, dicimal);

  // Controllo necessario per evitare che il segnale di cambiamento venga emesso
  // piu' volte.
  if (m_lineEdit->getValue() == val ||
      (m_roller->getValue() == val && m_roller->isVisible()))
    return;
  m_lineEdit->setValue(val);
  m_roller->setValue(val);
  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa
  // da visualizzare e' piu' lunga del campo le cifre che vengono troncate sono
  // le ultime e non le prime (dovrebbero essere quelle dopo la virgola).
  m_lineEdit->setCursorPosition(0);

  emit valueChanged(true);
}

//-----------------------------------------------------------------------------

void DoubleValueField::onLineEditValueChanged() {
  double value       = m_lineEdit->getValue();
  int dicimal        = m_lineEdit->getDecimals();
  double sliderValue = value * pow(10., dicimal);

  // Controllo necessario per evitare che il segnale di cambiamento venga emesso
  // piu' volte.
  if ((m_slider->value() == sliderValue && m_slider->isVisible()) ||
      (m_roller->getValue() == value && m_roller->isVisible()))
    return;
  m_slider->setValue(sliderValue);
  m_roller->setValue(value);
  emit valueChanged(false);
}

//-----------------------------------------------------------------------------

void DoubleValueField::onRollerValueChanged(bool isDragging) {
  double value = m_roller->getValue();

  int dicimal        = m_lineEdit->getDecimals();
  double sliderValue = value * pow(10., dicimal);

  if (sliderValue == m_lineEdit->getValue()) {
    assert(m_slider->value() == value || !m_slider->isVisible());
    // Se isDragging e' falso e' giusto che venga emessa la notifica di
    // cambiamento.
    if (!isDragging) emit valueChanged(isDragging);
    return;
  }
  m_slider->setValue(sliderValue);
  m_lineEdit->setValue(value);

  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa
  // da visualizzare e' piu' lunga del campo le cifre che vengono troncate sono
  // le ultime e non le prime (dovrebbero essere quelle dopo la virgola).
  m_lineEdit->setCursorPosition(0);

  emit valueChanged(isDragging);
}

//=============================================================================
// DoubleLineEdit
//-----------------------------------------------------------------------------

DoubleLineEdit::DoubleLineEdit(QWidget *parent, double value)
    : DoubleValueLineEdit(parent) {
  m_validator =
      new QDoubleValidator(-(std::numeric_limits<double>::max)(),
                           (std::numeric_limits<double>::max)(), 8, this);
  setValidator(m_validator);

  setValue(value);

  bool ret = connect(this, SIGNAL(editingFinished()), SIGNAL(valueChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void DoubleLineEdit::setValue(double value) {
  double minValue, maxValue;
  getRange(minValue, maxValue);
  if (value < minValue) value = minValue;
  if (value > maxValue) value = maxValue;
  QString str;
  str.setNum(value);
  setText(str);

  // Faccio in modo che il cursore sia sulla prima cifra, cosi' se la stringa
  // da visualizzare e' piu' lunga del campo le cifre che vengono troncate sono
  // le ultime e non le prime (dovrebbero essere quelle dopo la virgola).
  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

double DoubleLineEdit::getValue() { return text().toDouble(); }

//-----------------------------------------------------------------------------

void DoubleLineEdit::setRange(double minValue, double maxValue) {
  m_validator->setRange(minValue, maxValue, m_validator->decimals());
}

//-----------------------------------------------------------------------------

void DoubleLineEdit::getRange(double &minValue, double &maxValue) {
  minValue = m_validator->bottom();
  maxValue = m_validator->top();
}

//-----------------------------------------------------------------------------

void DoubleLineEdit::setDecimals(int decimals) {
  m_validator->setDecimals(decimals);
}

//-----------------------------------------------------------------------------

int DoubleLineEdit::getDecimals() { return m_validator->decimals(); }

//=============================================================================
// DoubleField
//-----------------------------------------------------------------------------

DoubleField::DoubleField(QWidget *parent, bool isRollerHide, int decimals)
    : DoubleValueField(parent, new DoubleLineEdit(0)) {
  if (isRollerHide) enableRoller(false);

  DoubleLineEdit *lineEdit = dynamic_cast<DoubleLineEdit *>(m_lineEdit);
  lineEdit->setDecimals(decimals);

  /*--rollerにもStepを設定--*/
  if (!isRollerHide) m_roller->setStep(pow(0.1, decimals));
}

//=============================================================================
// MeasuredDoubleLineEdit
//-----------------------------------------------------------------------------

MeasuredDoubleLineEdit::MeasuredDoubleLineEdit(QWidget *parent)
    : DoubleValueLineEdit(parent)
    , m_minValue(-(std::numeric_limits<double>::max)())
    , m_maxValue((std::numeric_limits<double>::max)())
    , m_modified(false)
    , m_errorHighlighting(0)
    , m_errorHighlightingTimerId(0)
    , m_decimals(7) {
  setObjectName("ValueLineEdit");
  m_value = new TMeasuredValue("length");
  valueToText();
  bool ret =
      connect(this, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));
  ret = ret && connect(this, SIGNAL(textChanged(const QString &)), this,
                       SLOT(onTextChanged(const QString &)));
  assert(ret);
}

//-----------------------------------------------------------------------------

MeasuredDoubleLineEdit::~MeasuredDoubleLineEdit() { delete m_value; }

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::valueToText() {
  bool oldModified = m_modified;
  setText(QString::fromStdWString(m_value->toWideString(m_decimals)));
  setCursorPosition(0);
  m_modified = oldModified;
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::setValue(double value) {
  m_value->setValue(TMeasuredValue::MainUnit, value);
  valueToText();
  m_modified = false;
}

//-----------------------------------------------------------------------------

double MeasuredDoubleLineEdit::getValue() {
  return m_value->getValue(TMeasuredValue::MainUnit);
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::setRange(double minValue, double maxValue) {
  m_minValue = minValue;
  m_maxValue = maxValue;
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::getRange(double &minValue, double &maxValue) {
  minValue = m_minValue;
  maxValue = m_maxValue;
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::setMeasure(std::string name) {
  delete m_value;
  m_value = new TMeasuredValue(name != "" ? name : "dummy");
  valueToText();
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::setDecimals(int decimals) {
  m_decimals = decimals;
  valueToText();
}

//-----------------------------------------------------------------------------

int MeasuredDoubleLineEdit::getDecimals() { return m_decimals; }

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::onTextChanged(const QString &) {
  m_modified = true;
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::onEditingFinished() {
  if (!m_modified) return;
  m_modified = false;

  double oldValue       = getValue();
  QString oldStyleSheet = styleSheet();

  int err = -10;
  m_value->setValue(text().toStdWString(), &err);

  bool outOfRange = false;
  if (!err) {
    double v   = getValue();
    outOfRange = m_minValue > v || m_maxValue < v;
  }

  if (err) {
    m_errorHighlighting = 1;
    if (m_errorHighlightingTimerId == 0)
      m_errorHighlightingTimerId = startTimer(40);
  } else {
    if (m_errorHighlightingTimerId != 0) killTimer(m_errorHighlightingTimerId);
    m_errorHighlightingTimerId = 0;
    m_errorHighlighting        = 0;
    setStyleSheet("");
  }

  double newValue = getValue();
  if (m_minValue > newValue || m_maxValue < newValue) {
    m_value->setValue(TMeasuredValue::MainUnit, oldValue);
    valueToText();
    emit valueChanged();
    return;
  }
  valueToText();
  emit valueChanged();
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::timerEvent(QTimerEvent *) {
  if (m_errorHighlighting < 0.01) {
    if (m_errorHighlightingTimerId != 0) killTimer(m_errorHighlightingTimerId);
    m_errorHighlightingTimerId = 0;
    m_errorHighlighting        = 0;
    setStyleSheet("");
  } else {
    int v               = 255 - (int)(m_errorHighlighting * 255);
    m_errorHighlighting = m_errorHighlighting * 0.8;
    int c               = 255 << 16 | v << 8 | v;
    setStyleSheet(QString("#ValueLineEdit {background-color:#%1}")
                      .arg(c, 6, 16, QLatin1Char('0')));
  }
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::mousePressEvent(QMouseEvent *e) {
  if ((e->buttons() == Qt::MiddleButton) || m_labelClicked) {
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

void MeasuredDoubleLineEdit::mouseMoveEvent(QMouseEvent *e) {
  if ((e->buttons() == Qt::MiddleButton) || m_labelClicked) {
    int precision = (m_maxValue > 100) ? 0 : ((m_maxValue > 10) ? 1 : 2);
    m_value->modifyValue((e->x() - m_xMouse) / 2, precision);
    m_xMouse = e->x();
    valueToText();
    m_modified = false;
  } else
    QLineEdit::mouseMoveEvent(e);
}

//-----------------------------------------------------------------------------

void MeasuredDoubleLineEdit::mouseReleaseEvent(QMouseEvent *e) {
  if ((e->buttons() == Qt::NoButton && m_mouseDragEditing) || m_labelClicked) {
    m_xMouse   = -1;
    m_modified = true;
    onEditingFinished();
    clearFocus();
    m_mouseDragEditing = false;
    m_labelClicked     = false;

  } else
    QLineEdit::mouseReleaseEvent(e);
}

void MeasuredDoubleLineEdit::receiveMousePress(QMouseEvent *e) {
  m_labelClicked = true;
  mousePressEvent(e);
}

void MeasuredDoubleLineEdit::receiveMouseMove(QMouseEvent *e) {
  mouseMoveEvent(e);
}

void MeasuredDoubleLineEdit::receiveMouseRelease(QMouseEvent *e) {
  mouseReleaseEvent(e);
  m_labelClicked = false;
}

//=============================================================================
// MeasuredDoubleField
//-----------------------------------------------------------------------------

MeasuredDoubleField::MeasuredDoubleField(QWidget *parent, bool isRollerHide)
    : DoubleValueField(parent, new MeasuredDoubleLineEdit(0)) {
  m_lineEdit->setMaximumWidth(100);
  if (isRollerHide) enableRoller(false);
}

///----------------------------------------------------------------------------

void MeasuredDoubleField::setMeasure(std::string measureName) {
  MeasuredDoubleLineEdit *lineEdit =
      dynamic_cast<MeasuredDoubleLineEdit *>(m_lineEdit);
  assert(lineEdit);
  lineEdit->setMeasure(measureName);
}

//----------------------------------------------------------------------------

void MeasuredDoubleField::setDecimals(int decimals) {
  MeasuredDoubleLineEdit *lineEdit =
      qobject_cast<MeasuredDoubleLineEdit *>(m_lineEdit);
  if (lineEdit) lineEdit->setDecimals(decimals);

  /*--- rollerにもStepを設定 ---*/
  if (isRollerEnabled()) m_roller->setStep(pow(0.1, std::max(decimals - 1, 1)));
}
