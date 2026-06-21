#pragma once

#include <QWidget>
#include <QColor>

// Simplified ruler widget — scale/graduation drawing only (no scene guides).
// Matches OpenToonz's Ruler visual appearance.
class Ruler : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor ParentBGColor READ parentBgColor WRITE setParentBgColor)
    Q_PROPERTY(QColor ScaleColor READ scaleColor WRITE setScaleColor)

public:
    Ruler(QWidget* parent, bool vertical);
    ~Ruler() {}

    void setZoomScale(double scale) { m_zoomScale = scale; }
    void setPan(double pan) { m_pan = pan; }
    void setOrigin(double origin) { m_origin = origin; }
    void setUnit(double unit) { m_unit = unit; }

    QColor parentBgColor() const { return m_parentBgColor; }
    void setParentBgColor(const QColor& c) { m_parentBgColor = c; }
    QColor scaleColor() const { return m_scaleColor; }
    void setScaleColor(const QColor& c) { m_scaleColor = c; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    bool m_vertical;
    QColor m_parentBgColor;
    QColor m_scaleColor;
    double m_zoomScale = 1.0;
    double m_pan = 0.0;
    double m_origin = 0.0;
    double m_unit = 1.0;

    void drawVertical(QPainter& p);
    void drawHorizontal(QPainter& p);
};
