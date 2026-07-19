#include "canvaspanel.h"
#include <QPainter>

CanvasPanel::CanvasPanel(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 150);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rebuildChecker();
}

void CanvasPanel::rebuildChecker() {
    int s = 32;
    m_checker = QPixmap(s * 2, s * 2);
    QPainter p(&m_checker);
    p.fillRect(0, 0, s, s, QColor(60, 60, 60));
    p.fillRect(s, 0, s, s, QColor(80, 80, 80));
    p.fillRect(0, s, s, s, QColor(80, 80, 80));
    p.fillRect(s, s, s, s, QColor(60, 60, 60));
}

void CanvasPanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.drawTiledPixmap(rect(), m_checker);
    p.setPen(Qt::white);
    p.setFont(QFont("sans-serif", 14));
    p.drawText(rect(), Qt::AlignCenter, "Canvas Area\n(Drag panels to rearrange)");
}

void CanvasPanel::resizeEvent(QResizeEvent*) {}
