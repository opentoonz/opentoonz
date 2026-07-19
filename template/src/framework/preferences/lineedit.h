#pragma once

#include <QLineEdit>
#include <QColor>

namespace DVGui {

class LineEdit : public QLineEdit {
    Q_OBJECT
public:
    LineEdit(QWidget* parent = nullptr);
    LineEdit(const QString& text, QWidget* parent = nullptr);
    ~LineEdit() {}
    void setLineEditBackgroundColor(const QColor& color);
};

} // namespace DVGui
