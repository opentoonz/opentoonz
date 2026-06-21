#include "lineedit.h"

using namespace DVGui;

LineEdit::LineEdit(QWidget* parent) : QLineEdit(parent) { setObjectName("LineEdit"); }
LineEdit::LineEdit(const QString& text, QWidget* parent) : QLineEdit(text, parent) { setObjectName("LineEdit"); }

void LineEdit::setLineEditBackgroundColor(const QColor& color) {
    setStyleSheet(QString("background-color: %1").arg(color.name()));
}
