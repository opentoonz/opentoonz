#include "doublefield.h"
#include <QFocusEvent>

using namespace DVGui;

DoubleLineEdit::DoubleLineEdit(QWidget* parent, double value)
    : LineEdit(parent) {
    setValue(value);
    m_validator = new QDoubleValidator(this);
    setValidator(m_validator);
}

void DoubleLineEdit::setValue(double value) { setText(QString::number(value, 'f', 4)); }
double DoubleLineEdit::getValue() const { return text().toDouble(); }
void DoubleLineEdit::setRange(double minValue, double maxValue) { m_validator->setRange(minValue, maxValue); }
void DoubleLineEdit::focusOutEvent(QFocusEvent* e) { emit valueEditedByHand(); QLineEdit::focusOutEvent(e); }

MeasuredDoubleLineEdit::MeasuredDoubleLineEdit(QWidget* parent, double value)
    : DoubleLineEdit(parent, value) {}
void MeasuredDoubleLineEdit::setValue(double value) { DoubleLineEdit::setValue(value); }
double MeasuredDoubleLineEdit::getValue() const { return DoubleLineEdit::getValue(); }
