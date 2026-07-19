#include "checkbox.h"
#include <QMouseEvent>

using namespace DVGui;

CheckBox::CheckBox(QWidget* parent) : QCheckBox(parent) {}
CheckBox::CheckBox(const QString& text, QWidget* parent) : QCheckBox(text, parent) {}

void CheckBox::mousePressEvent(QMouseEvent* e) { click(); e->accept(); }
void CheckBox::mouseReleaseEvent(QMouseEvent* e) { e->accept(); }
void CheckBox::mouseMoveEvent(QMouseEvent* e) { e->accept(); }
