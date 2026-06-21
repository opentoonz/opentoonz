#include "dvdialog.h"
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

using namespace DVGui;

Separator::Separator(QString name, QWidget* parent, bool isHorizontal)
    : QFrame(parent), m_name(name), m_isHorizontal(isHorizontal) {
    setFixedHeight(m_name.isEmpty() ? 2 : 20);
}

void Separator::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setPen(QColor(128, 128, 128));
    if (!m_name.isEmpty()) {
        p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, m_name);
        int textW = fontMetrics().horizontalAdvance(m_name) + 10;
        p.drawLine(textW, height() / 2, width() - 5, height() / 2);
    } else {
        p.drawLine(5, height() / 2, width() - 5, height() / 2);
    }
}

Dialog::Dialog(QWidget* parent, bool hasButton, bool hasFixedSize, const QString& name)
    : QDialog(parent) {
    setWindowTitle(name);
    if (hasFixedSize) setFixedSize(500, 400);

    m_topLayout = new QVBoxLayout(this);
    m_topLayout->setContentsMargins(0, 0, 0, 0);
    m_topLayout->setSpacing(0);

    m_mainFrame = new QFrame(this);
    auto* mainLayout = new QVBoxLayout(m_mainFrame);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    m_topLayout->addWidget(m_mainFrame);

    if (hasButton) {
        m_buttonFrame = new QFrame(this);
        m_buttonLayout = new QHBoxLayout(m_buttonFrame);
        m_buttonLayout->setContentsMargins(10, 5, 10, 5);
        m_topLayout->addWidget(m_buttonFrame);
    }
}

Dialog::~Dialog() {}

void Dialog::addWidget(QWidget* widget) {
    if (auto* l = m_mainFrame->layout())
        l->addWidget(widget);
}

void Dialog::addWidget(const QString& labelName, QWidget* widget) {
    auto* layout = qobject_cast<QHBoxLayout*>(m_mainFrame->layout());
    if (!layout) {
        auto* hl = new QHBoxLayout();
        hl->addWidget(new QLabel(labelName, m_mainFrame));
        hl->addWidget(widget);
        if (auto* ml = m_mainFrame->layout())
            ml->addItem(hl);
    }
}

void Dialog::addSeparator(const QString& name) {
    addWidget(new Separator(name, m_mainFrame));
}

void Dialog::setTopMargin(int margin) {
    m_topLayout->setContentsMargins(margin, margin, margin, margin);
}

void Dialog::setTopSpacing(int spacing) {
    m_topLayout->setSpacing(spacing);
}

int Dialog::getLabelWidth() const { return 100; }
void Dialog::setLabelWidth(int) {}
