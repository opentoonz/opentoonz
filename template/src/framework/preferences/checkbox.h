#pragma once

#include <QCheckBox>

namespace DVGui {

class CheckBox : public QCheckBox {
    Q_OBJECT
public:
    CheckBox(QWidget* parent = nullptr);
    CheckBox(const QString& text, QWidget* parent = nullptr);
    ~CheckBox() {}

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent*) override;
};

} // namespace DVGui
