#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QSize>
#include <QHash>
#include <QIcon>
#include <QIconEngine>

// SVG render parameters
struct SvgRenderParams {
    QSizeF size;
    QRectF rect;
};

SvgRenderParams calculateSvgRenderParams(const QSize &desiredSize,
                                         QSize &imageSize,
                                         Qt::AspectRatioMode aspectRatioMode);

// Utility functions
int getHighestDevicePixelRatio();
QImage svgToImage(const QString &svgFilePath, QSize size = QSize(),
                  Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio,
                  QColor bgColor = Qt::transparent);
QImage colorizeBlackPixels(const QImage &input, const QColor color);
QImage adjustImageOpacity(const QImage &input, qreal opacity);
QString generateCacheKey(const QString &keyName, const QSize &size,
                         QIcon::Mode mode, QIcon::State state);
void addToPixmapCache(const QString &key, const QPixmap &pixmap);
QPixmap getFromPixmapCache(const QString &key);
void clearQPixmapCache();
QIcon createQIcon(const QString &iconName, bool isForMenuItem = false,
                  qreal dpr = 0.0, QSize newSize = QSize());

//-------------------------------------------------------------------------
// ThemeManager
//-------------------------------------------------------------------------

class ThemeManager {
public:
    static ThemeManager &getInstance();
    void initialize();

    // Icon Management
    void preloadIconMetadata(const QString &path);
    void updateThemeProperties();
    QString getIconPath(const QString &iconName, QIcon::Mode mode = QIcon::Normal,
                        QIcon::State state = QIcon::Off) const;
    QSize getIconSize(const QString &iconName) const;
    bool hasIcon(const QString &iconName) const;
    bool isMenuIcon(const QString &iconName) const;
    bool isColoredIcon(const QString &iconName) const;

    // Icon Color Management
    void setIconBaseOpacity(const double &val) { m_iconBaseOpacity = val; }
    double getIconBaseOpacity() const { return m_iconBaseOpacity; }
    void setIconDisabledOpacity(const double &val) { m_iconDisabledOpacity = val; }
    double getIconDisabledOpacity() const { return m_iconDisabledOpacity; }

    void setInterfaceColor(const QColor &color) { m_interfaceColor = color; }
    QColor getInterfaceColor() const { return m_interfaceColor; }

    void setIconBaseColor(const QColor &color) { m_iconBaseColor = color; }
    QColor getIconBaseColor() const { return m_iconBaseColor; }

    void setIconActiveColor(const QColor &color) { m_iconActiveColor = color; }
    QColor getIconActiveColor() const { return m_iconActiveColor; }

    void setIconOnColor(const QColor &color) { m_iconOnColor = color; }
    QColor getIconOnColor() const { return m_iconOnColor; }

    void setIconSelectedColor(const QColor &color) { m_iconSelectedColor = color; }
    QColor getIconSelectedColor() const { return m_iconSelectedColor; }

    void setIconCloseColor(const QColor &color) { m_iconCloseColor = color; }
    QColor getIconCloseColor() const { return m_iconCloseColor; }

    void setIconPreviewColor(const QColor &color) { m_iconPreviewColor = color; }
    QColor getIconPreviewColor() const { return m_iconPreviewColor; }

    void setIconVCheckColor(const QColor &color) { m_iconVCheckColor = color; }
    QColor getIconVCheckColor() const { return m_iconVCheckColor; }

    void setIconLockColor(const QColor &color) { m_iconLockColor = color; }
    QColor getIconLockColor() const { return m_iconLockColor; }

    void setIconKeyframeColor(const QColor &color) { m_iconKeyframeColor = color; }
    QColor getIconKeyframeColor() const { return m_iconKeyframeColor; }

    void setIconKeyframeModifiedColor(const QColor &color) {
        m_iconKeyframeModifiedColor = color;
    }
    QColor getIconKeyframeModifiedColor() const {
        return m_iconKeyframeModifiedColor;
    }

    // Stylesheet Parsing
    void parseCustomPropertiesFromStylesheet(const QString &styleSheet);
    void setCustomProperty(const QString &name, const QString &value);
    QString getCustomProperty(const QString &name,
                              const QString &defaultValue = QString()) const;
    double getCustomPropertyDouble(const QString &name,
                                   double defaultValue = 1.0) const;
    QColor getCustomPropertyColor(const QString &name,
                                  const QColor &defaultValue = QColor()) const;

private:
    ThemeManager();
    Q_DISABLE_COPY_MOVE(ThemeManager)

    QSize parseIconSize(const QString &iconPath) const;
    bool parseIsColored(const QString &iconPath) const;

    QHash<QString, QString> m_iconPaths;
    QHash<QString, QSize> m_iconSizes;
    QHash<QString, bool> m_coloredIcons;
    QHash<QString, bool> m_menuIcons;
    QHash<QString, QString> m_customProperties;

    double m_iconBaseOpacity = 0.8;
    double m_iconDisabledOpacity = 0.2;

    QColor m_interfaceColor;
    QColor m_iconBaseColor;
    QColor m_iconActiveColor;
    QColor m_iconOnColor;
    QColor m_iconSelectedColor;
    QColor m_iconCloseColor;
    QColor m_iconPreviewColor;
    QColor m_iconVCheckColor;
    QColor m_iconLockColor;
    QColor m_iconKeyframeColor;
    QColor m_iconKeyframeModifiedColor;
};

QString getIconPath(const QString &iconSVGName);

//-------------------------------------------------------------------------
// SvgIconEngine
//-------------------------------------------------------------------------

class SvgIconEngine : public QIconEngine {
public:
    SvgIconEngine(const QString &iconName, bool isForMenuItem = false,
                  qreal dpr = 0.0, QSize newSize = QSize());
    SvgIconEngine(const QString &commandName, const QImage image);

    QIconEngine *clone() const override;
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
               QIcon::State state) override;
    QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                   QIcon::State state) override;
    QColor getThemeColor(const QString &iconName, QIcon::Mode mode,
                         QIcon::State state);

private:
    QString m_iconName;
    QSize m_iconSize;
    qreal m_dpr;
    bool m_isForMenuItem;
    bool m_isMenuIcon;
    bool m_isColored;
    QColor m_color;
    QImage m_image;
    bool m_isTemporaryCommandIcon = false;

    qreal getOpacityForModeState(QIcon::Mode mode, QIcon::State state);
    QColor getUniqueIconColor(const QString &iconName, QIcon::Mode mode,
                              QIcon::State state);
    QPixmap loadPixmap(const QString &iconName, QIcon::Mode mode = QIcon::Normal,
                       QIcon::State state = QIcon::Off,
                       QSize physicalSize = QSize(), QColor color = QColor());
    bool shouldHideIcon(bool isForMenuItem);
    QSize getBestToolbarSizeByDpr(const QSize &requestedSize);
};
