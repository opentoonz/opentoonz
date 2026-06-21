#include "ruler.h"
#include <QPainter>
#include <QFontMetrics>

Ruler::Ruler(QWidget* parent, bool vertical)
    : QWidget(parent)
    , m_vertical(vertical)
    , m_parentBgColor(45, 45, 48)
    , m_scaleColor(160, 160, 160) {
    if (vertical)
        setFixedWidth(20);
    else
        setFixedHeight(20);
}

void Ruler::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), m_parentBgColor);
    p.setPen(m_scaleColor);
    if (m_vertical) drawVertical(p);
    else drawHorizontal(p);
    // Border
    p.setPen(QColor(80, 80, 80));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void Ruler::drawVertical(QPainter& p) {
    int h = height();
    double tickUnit = m_unit * m_zoomScale;
    if (tickUnit < 4) tickUnit *= 5;
    double start = fmod(m_origin - m_pan, tickUnit);

    for (double pos = start; pos < h; pos += tickUnit) {
        int y = h - 1 - static_cast<int>(pos);
        if (y < 0 || y >= h) continue;
        bool major = (static_cast<int>(pos / tickUnit) % 5 == 0);
        int tickLen = major ? 8 : 4;
        p.drawLine(width() - 1 - tickLen, y, width() - 1, y);
    }
}

void Ruler::drawHorizontal(QPainter& p) {
    int w = width();
    double tickUnit = m_unit * m_zoomScale;
    if (tickUnit < 4) tickUnit *= 5;
    double start = fmod(m_origin - m_pan, tickUnit);

    for (double pos = start; pos < w; pos += tickUnit) {
        int x = static_cast<int>(pos);
        if (x < 0 || x >= w) continue;
        bool major = (static_cast<int>(pos / tickUnit) % 5 == 0);
        int tickLen = major ? 8 : 4;
        p.drawLine(x, height() - 1 - tickLen, x, height() - 1);
    }
}
