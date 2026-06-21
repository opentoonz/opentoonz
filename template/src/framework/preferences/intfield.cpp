#include "intfield.h"
#include <QFocusEvent>

using namespace DVGui;

IntLineEdit::IntLineEdit(QWidget* parent, int value, int minValue, int maxValue)
    : LineEdit(parent) {
    setValue(value);
    m_validator = new QIntValidator(minValue, maxValue, this);
    setValidator(m_validator);
}

void IntLineEdit::setValue(int value) {
    setText(QString::number(value));
}

int IntLineEdit::getValue() const {
    return text().toInt();
}

void IntLineEdit::setRange(int minValue, int maxValue) {
    m_validator->setRange(minValue, maxValue);
}

void IntLineEdit::getRange(int& minValue, int& maxValue) const {
    minValue = m_validator->bottom();
    maxValue = m_validator->top();
}

void IntLineEdit::focusOutEvent(QFocusEvent* e) {
    emit valueEditedByHand();
    QLineEdit::focusOutEvent(e);
}
