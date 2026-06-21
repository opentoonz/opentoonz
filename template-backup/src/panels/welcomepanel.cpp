#include "welcomepanel.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QLabel>

WelcomePanel::WelcomePanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("YZ UI Template", this);
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #cccccc;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        "Custom Docking Framework\n\n"
        "Features:\n"
        "  - Drag-to-dock / float panels\n"
        "  - Multi-room workspace switching\n"
        "  - Layout persistence (QSettings)\n"
        "  - Maximizable panels\n"
        "  - Global action/command system\n"
        "  - Tab-based room navigation\n\n"
        "Use the tabs above to switch rooms.\n"
        "Drag panel title bars to rearrange.",
        this);
    subtitle->setStyleSheet("font-size: 13px; color: #888888;");
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(subtitle);
}

void WelcomePanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(45, 45, 48));
}
