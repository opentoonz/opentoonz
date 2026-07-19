#pragma once

#include "lineedit.h"
#include <QDoubleValidator>

namespace DVGui {

class DoubleLineEdit : public LineEdit {
    Q_OBJECT
public:
    DoubleLineEdit(QWidget* parent = nullptr, double value = 0.0);
    ~DoubleLineEdit() {}

    void setValue(double value);
    double getValue() const;
    void setRange(double minValue, double maxValue);

signals:
    void valueEditedByHand();

protected:
    void focusOutEvent(QFocusEvent* e) override;

private:
    QDoubleValidator* m_validator;
};

class MeasuredDoubleLineEdit : public DoubleLineEdit {
    Q_OBJECT
public:
    MeasuredDoubleLineEdit(QWidget* parent = nullptr, double value = 0.0);
    void setValue(double value);
    double getValue() const;
};

} // namespace DVGui
