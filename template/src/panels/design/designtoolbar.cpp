#include "designtoolbar.h"
#include <QPainter>
#include <QApplication>

static QIcon makeDesignIcon(const QColor& color, char shape) {
    QPixmap pm(22, 22); pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5));
    p.setBrush(Qt::NoBrush);
    switch (shape) {
    case 'm': p.drawLine(4,18,18,18); { // Move cursor
        QPointF arrow[] = {{4,18},{10,4},{18,18}}; p.drawPolyline(arrow, 3); } break;
    case 'f': p.drawRect(4,4,14,10); p.drawLine(14,14,18,18); break;     // Frame
    case 'c': p.drawEllipse(4,4,14,14); break;                            // Circle shape
    case 'p': p.drawLine(4,18,18,4); { QPointF pts[]={{6,10},{10,4},{14,8}}; p.drawPolyline(pts,3); } break; // Pen
    case 't': { QFont f = p.font(); f.setPixelSize(18); p.setFont(f); p.drawText(3,17,"T"); } break;     // Text
    case 'h': p.drawLine(6,18,10,6); p.drawLine(10,6,16,10); p.drawLine(16,10,18,8); break;  // Hand
    default: break;
    }
    p.end();
    return QIcon(pm);
}

DesignToolbar::DesignToolbar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(2);

    auto btnStyle = QStringLiteral(
        "QToolButton { background: transparent; border: 1px solid transparent; "
        "border-radius: 4px; } "
        "QToolButton:hover { background: #3a3a3a; } "
        "QToolButton:checked { background: #007acc; }");

    struct { const char* tip; char shape; QColor color; } tools[] = {
        {"Move Tool (V)", 'm', QColor(180,180,180)},
        {"Frame (F)",     'f', QColor(220,180,100)},
        {"Shape (R/O)",   'c', QColor(100,180,220)},
        {"Pen (P)",       'p', QColor(140,200,100)},
        {"Text (T)",      't', QColor(180,160,220)},
        {"Hand (H)",      'h', QColor(200,140,220)},
    };

    for (auto& t : tools) {
        auto* btn = new QToolButton(this);
        btn->setCheckable(true);
        btn->setIcon(makeDesignIcon(t.color, t.shape));
        btn->setToolTip(tr(t.tip));
        btn->setFixedSize(36, 32);
        btn->setIconSize(QSize(24, 24));
        btn->setAutoRaise(true);
        btn->setStyleSheet(btnStyle);
        connect(btn, &QToolButton::toggled, this, [this]() {
            auto* s = qobject_cast<QToolButton*>(sender());
            if (!s || !s->isChecked()) return;
            for (auto* b : findChildren<QToolButton*>())
                if (b != s) b->setChecked(false);
        });
        layout->addWidget(btn);
    }

    // Select the first one
    if (auto* first = findChild<QToolButton*>())
        first->setChecked(true);

    layout->addStretch();
}
