#pragma once

#include "lineedit.h"
#include <QIntValidator>
#include <limits>

namespace DVGui {

class IntLineEdit : public LineEdit {
    Q_OBJECT
public:
    IntLineEdit(QWidget* parent = nullptr, int value = 0,
                int minValue = -(std::numeric_limits<int>::max)(),
                int maxValue = (std::numeric_limits<int>::max)());
    ~IntLineEdit() {}

    void setValue(int value);
    int getValue() const;
    void setRange(int minValue, int maxValue);
    void getRange(int& minValue, int& maxValue) const;

signals:
    void valueEditedByHand();

protected:
    void focusOutEvent(QFocusEvent* e) override;

private:
    QIntValidator* m_validator;
};

} // namespace DVGui
