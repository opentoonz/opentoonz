#include "thememanager.h"

#include <QDirIterator>
#include <QPixmap>
#include <QPixmapCache>
#include <QImage>
#include <QPainter>
#include <QIcon>
#include <QString>
#include <QApplication>
#include <QFileInfo>
#include <QSvgRenderer>
#include <QRegularExpression>
#include <QScreen>
#include <QWindow>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>

//=============================================================================
// Utility Functions
//=============================================================================

int getHighestDevicePixelRatio() {
    static int highestDevPixRatio = 0;
    if (highestDevPixRatio == 0) {
        for (auto screen : QGuiApplication::screens())
            highestDevPixRatio =
                std::max(highestDevPixRatio, (int)screen->devicePixelRatio());
    }
    return highestDevPixRatio;
}

SvgRenderParams calculateSvgRenderParams(const QSize &desiredSize,
                                         QSize &imageSize,
                                         Qt::AspectRatioMode aspectRatioMode) {
    SvgRenderParams params;
    if (desiredSize.isEmpty()) {
        params.size = imageSize;
        params.rect = QRectF(QPointF(), QSizeF(params.size));
    } else {
        params.size = desiredSize;
        if (aspectRatioMode == Qt::KeepAspectRatio ||
            aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
            QPointF scaleFactor(
                (float)params.size.width() / (float)imageSize.width(),
                (float)params.size.height() / (float)imageSize.height());
            float factor = (aspectRatioMode == Qt::KeepAspectRatio)
                               ? std::min(scaleFactor.x(), scaleFactor.y())
                               : std::max(scaleFactor.x(), scaleFactor.y());
            QSizeF renderSize(factor * (float)imageSize.width(),
                              factor * (float)imageSize.height());
            QPointF topLeft(
                ((float)params.size.width() - renderSize.width()) * 0.5f,
                ((float)params.size.height() - renderSize.height()) * 0.5f);
            params.rect = QRectF(topLeft, renderSize);
        } else {
            params.rect = QRectF(QPointF(), QSizeF(params.size));
        }
    }
    return params;
}

QImage svgToImage(const QString &svgFilePath, QSize size,
                  Qt::AspectRatioMode aspectRatioMode, QColor bgColor) {
    if (svgFilePath.isEmpty()) return QImage();

    QSvgRenderer svgRenderer(svgFilePath);
    if (!svgRenderer.isValid()) {
        qDebug() << "Invalid SVG file:" << svgFilePath;
        return QImage();
    }

    QSize imageSize = !size.isEmpty() ? size : svgRenderer.defaultSize();
    SvgRenderParams params =
        calculateSvgRenderParams(size, imageSize, aspectRatioMode);
    QImage image(params.size.toSize(), QImage::Format_ARGB32_Premultiplied);
    QPainter painter;
    image.fill(bgColor);

    if (!painter.begin(&image)) {
        qDebug() << "Failed to begin QPainter on image:" << svgFilePath;
        return QImage();
    }

    svgRenderer.render(&painter, params.rect);
    painter.end();
    return image;
}

QImage colorizeBlackPixels(const QImage &input, const QColor color) {
    if (input.isNull()) return QImage();
    if (!color.isValid()) return input;

    QImage image     = input.convertToFormat(QImage::Format_ARGB32);
    QRgb targetColor = color.rgb();
    int height       = image.height();
    int width        = image.width();
    for (int y = 0; y < height; ++y) {
        QRgb *pixel = reinterpret_cast<QRgb *>(image.scanLine(y));
        QRgb *end   = pixel + width;
        for (; pixel != end; ++pixel) {
            if (qGray(*pixel) == 0) {
                *pixel = (targetColor & 0x00FFFFFF) | (qAlpha(*pixel) << 24);
            }
        }
    }
    return image;
}

QImage adjustImageOpacity(const QImage &input, qreal opacity) {
    if (input.isNull()) return QImage();
    if (opacity >= 1.0) return input;

    QImage image = input.convertToFormat(QImage::Format_ARGB32);
    int height   = image.height();
    int width    = image.width();
    for (int y = 0; y < height; ++y) {
        QRgb *pixel = reinterpret_cast<QRgb *>(image.scanLine(y));
        QRgb *end   = pixel + width;
        for (; pixel != end; ++pixel)
            *pixel = (qRgba(qRed(*pixel), qGreen(*pixel), qBlue(*pixel),
                            (int)(qAlpha(*pixel) * opacity)));
    }
    return image;
}

//=============================================================================
// Pixmap Cache Helpers
//=============================================================================

static QMutex s_cacheMutex;

QString generateCacheKey(const QString &keyName, const QSize &size,
                         QIcon::Mode mode, QIcon::State state) {
    static QString modeNames[]  = {"Normal", "Disabled", "Active", "Selected"};
    static QString stateNames[] = {"Off", "On"};
    return QString("%1_%2x%3_%4_%5")
        .arg(keyName)
        .arg(size.width())
        .arg(size.height())
        .arg(modeNames[mode])
        .arg(stateNames[state]);
}

void addToPixmapCache(const QString &key, const QPixmap &pixmap) {
    if (!pixmap.isNull()) {
        QMutexLocker locker(&s_cacheMutex);
        QPixmapCache::insert(key, pixmap);
    }
}

QPixmap getFromPixmapCache(const QString &key) {
    QMutexLocker locker(&s_cacheMutex);
    QPixmap pm;
    QPixmapCache::find(key, &pm) ? void() : void();
    if (QPixmapCache::find(key, &pm)) return pm;
    return QPixmap();
}

void clearQPixmapCache() {
    QPixmapCache::clear();
}

//=============================================================================
// ThemeManager
//=============================================================================

QString getIconPath(const QString &iconSVGName) {
    ThemeManager &themeManager = ThemeManager::getInstance();
    return themeManager.getIconPath(iconSVGName);
}

ThemeManager::ThemeManager() {}

ThemeManager &ThemeManager::getInstance() {
    static ThemeManager instance;
    return instance;
}

void ThemeManager::initialize() {
    preloadIconMetadata(":/icons");
}

void ThemeManager::updateThemeProperties() {
    setIconBaseOpacity(getCustomPropertyDouble("icon-base-opacity", 0.8));
    setIconDisabledOpacity(getCustomPropertyDouble("icon-disabled-opacity", 0.2));

    setIconBaseColor(getCustomPropertyColor("icon-base-color"));
    setIconActiveColor(getCustomPropertyColor("icon-active-color"));
    setIconOnColor(getCustomPropertyColor("icon-on-color"));
    setIconSelectedColor(getCustomPropertyColor("icon-selected-color"));

    setIconCloseColor(getCustomPropertyColor("icon-close-color"));
    setIconPreviewColor(getCustomPropertyColor("icon-preview-color"));
    setIconLockColor(getCustomPropertyColor("icon-lock-color"));

    setIconKeyframeColor(getCustomPropertyColor("icon-keyframe-color"));
    setIconKeyframeModifiedColor(
        getCustomPropertyColor("icon-keyframe-modified-color"));

    setIconVCheckColor(getCustomPropertyColor("icon-vcheck-color"));

    clearQPixmapCache();
}

void ThemeManager::preloadIconMetadata(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        qWarning() << "[ThemeManager] Resource path does not exist:" << path;
        return;
    }

    QDirIterator it(path, {"*.svg", "*.png"}, QDir::Files,
                    QDirIterator::Subdirectories);

    static const QStringList states = {"on", "over"};
    static const QRegularExpression stateRegex(R"((.+)_(on|over)$)");

    while (it.hasNext()) {
        it.next();
        const QFileInfo fileInfo = it.fileInfo();
        const QString iconPath   = fileInfo.filePath();
        const QString fileName   = fileInfo.baseName();

        QString iconName = fileName;
        QString state;

        QRegularExpressionMatch match = stateRegex.match(fileName);
        if (match.hasMatch()) {
            iconName = match.captured(1);
            state    = match.captured(2);
        }

        if (!m_iconPaths.contains(fileName)) {
            m_iconPaths.insert(fileName, iconPath);
            QSize iconSize = parseIconSize(iconPath);
            m_iconSizes.insert(iconName, iconSize);

            if (iconSize.width() == 16 && iconSize.height() == 16)
                m_menuIcons.insert(iconName, true);

            if (parseIsColored(iconPath))
                m_coloredIcons.insert(iconName, true);
        }
    }
}

QString ThemeManager::getIconPath(const QString &iconBaseName, QIcon::Mode mode,
                                  QIcon::State state) const {
    if (!hasIcon(iconBaseName)) return QString();

    QString suffix;
    if (state == QIcon::On)
        suffix = "_on";
    else if (mode == QIcon::Active)
        suffix = "_over";

    QString fullName = iconBaseName + suffix;
    if (m_iconPaths.contains(fullName)) return m_iconPaths[fullName];
    if (m_iconPaths.contains(iconBaseName)) return m_iconPaths[iconBaseName];

    qWarning() << "Icon not found:" << iconBaseName;
    return QString();
}

QSize ThemeManager::parseIconSize(const QString &iconPath) const {
    static const QRegularExpression sizeRegex(R"(/(\d+)x(\d+)/)");
    QRegularExpressionMatch match = sizeRegex.match(iconPath);
    if (match.hasMatch())
        return QSize(match.captured(1).toInt(), match.captured(2).toInt());
    return QSize(16, 16);
}

bool ThemeManager::parseIsColored(const QString &iconPath) const {
    static const QRegularExpression coloredRegex(R"(/colored/)");
    return coloredRegex.match(iconPath).hasMatch();
}

bool ThemeManager::isColoredIcon(const QString &iconName) const {
    QString baseName = iconName;
    if (baseName.endsWith("_on") || baseName.endsWith("_over"))
        baseName = baseName.left(baseName.lastIndexOf('_'));
    return m_coloredIcons.contains(baseName);
}

void ThemeManager::parseCustomPropertiesFromStylesheet(
    const QString &styleSheet) {
    QRegularExpression rootRegex(R"(:TOONZCOLORS\s*\{([^}]*)\})");
    QRegularExpressionMatchIterator rootMatches =
        rootRegex.globalMatch(styleSheet);

    while (rootMatches.hasNext()) {
        QRegularExpressionMatch rootMatch = rootMatches.next();
        QString rootContent = rootMatch.captured(1);

        QRegularExpression propertyRegex(R"(-([a-zA-Z0-9-]+):\s*([^;]+);)");
        QRegularExpressionMatchIterator propertyMatches =
            propertyRegex.globalMatch(rootContent);

        while (propertyMatches.hasNext()) {
            QRegularExpressionMatch propMatch = propertyMatches.next();
            QString propertyName              = propMatch.captured(1);
            QString propertyValue             = propMatch.captured(2).trimmed();
            setCustomProperty(propertyName, propertyValue);
        }
    }

    updateThemeProperties();
}

void ThemeManager::setCustomProperty(const QString &name,
                                     const QString &value) {
    m_customProperties[name] = value;
}

QString ThemeManager::getCustomProperty(const QString &name,
                                        const QString &defaultValue) const {
    return m_customProperties.value(name, QString());
}

double ThemeManager::getCustomPropertyDouble(const QString &name,
                                             double defaultValue) const {
    QString value = getCustomProperty(name);
    bool ok       = false;
    double result = value.toDouble(&ok);
    return ok ? result : defaultValue;
}

QColor ThemeManager::getCustomPropertyColor(const QString &name,
                                            const QColor &defaultValue) const {
    QString value = getCustomProperty(name);
    if (value.isEmpty()) return defaultValue;
    QColor color(value);
    return color.isValid() ? color : defaultValue;
}

QSize ThemeManager::getIconSize(const QString &iconName) const {
    QString baseName = iconName;
    if (baseName.endsWith("_on") || baseName.endsWith("_over"))
        baseName = baseName.left(baseName.lastIndexOf('_'));
    return m_iconSizes.value(baseName, QSize());
}

bool ThemeManager::hasIcon(const QString &iconName) const {
    return m_iconPaths.contains(iconName);
}

bool ThemeManager::isMenuIcon(const QString &iconName) const {
    QString baseName = iconName;
    if (baseName.endsWith("_on") || baseName.endsWith("_over"))
        baseName = baseName.left(baseName.lastIndexOf('_'));
    return m_menuIcons.contains(baseName);
}

//=============================================================================
// createQIcon
//=============================================================================

QIcon createQIcon(const QString &iconName, bool isForMenuItem, qreal dpr,
                  QSize newSize) {
    if (iconName.isEmpty() || !ThemeManager::getInstance().hasIcon(iconName))
        return QIcon();
    return QIcon(new SvgIconEngine(iconName, isForMenuItem, dpr, newSize));
}

//=============================================================================
// SvgIconEngine
//=============================================================================

SvgIconEngine::SvgIconEngine(const QString &iconName, bool isForMenuItem,
                             qreal dpr, QSize newSize)
    : m_iconName(iconName)
    , m_isForMenuItem(isForMenuItem)
    , m_isTemporaryCommandIcon(false)
    , m_dpr(dpr > 0.0 ? dpr : getHighestDevicePixelRatio())
    , m_iconSize(newSize) {
    ThemeManager &tm = ThemeManager::getInstance();
    m_isMenuIcon = tm.isMenuIcon(iconName);
    m_isColored  = tm.isColoredIcon(iconName);
}

SvgIconEngine::SvgIconEngine(const QString &commandName, const QImage image)
    : SvgIconEngine("") {
    m_iconName               = commandName;
    m_isTemporaryCommandIcon = true;
    m_image                  = image;
}

QIconEngine *SvgIconEngine::clone() const {
    if (!m_iconName.isEmpty())
        return new SvgIconEngine(m_iconName, m_isForMenuItem, m_dpr, m_iconSize);
    else
        return new SvgIconEngine(m_iconName, m_image);
}

QColor SvgIconEngine::getThemeColor(const QString &iconName, QIcon::Mode mode,
                                    QIcon::State state) {
    ThemeManager &tm = ThemeManager::getInstance();
    QColor color = getUniqueIconColor(iconName, mode, state);
    if (color.isValid()) return color;

    if (state == QIcon::On) return tm.getIconOnColor();
    if (mode == QIcon::Active) return tm.getIconActiveColor();
    if (mode == QIcon::Selected) return tm.getIconSelectedColor();

    return tm.getIconBaseColor();
}

QColor SvgIconEngine::getUniqueIconColor(const QString &iconName,
                                         QIcon::Mode mode, QIcon::State state) {
    ThemeManager &tm = ThemeManager::getInstance();

    if (mode == QIcon::Active) {
        if (iconName.contains("closewindow")) return tm.getIconCloseColor();
    }

    if (state == QIcon::On) {
        if (iconName == "preview" || iconName == "subpreview")
            return tm.getIconPreviewColor();
        if (iconName.contains("freeze") || iconName.contains("lock"))
            return tm.getIconLockColor();
        if (iconName.contains("keyframe_partial"))
            return tm.getIconKeyframeColor();
        if (iconName.contains("keyframe_modified"))
            return tm.getIconKeyframeModifiedColor();
        if (iconName.contains("keyframe"))
            return tm.getIconKeyframeColor();
        if (iconName.contains("transparency_check") ||
            iconName.contains("paint_check") ||
            iconName.contains("opacity_check") ||
            iconName.contains("ink_check") ||
            iconName.contains("ink_no1_check") ||
            iconName.contains("gap_check") ||
            iconName.contains("fill_check") ||
            iconName.contains("blackbg_check") ||
            iconName.contains("inks_only"))
            return tm.getIconVCheckColor();
    }

    return QColor();
}

void SvgIconEngine::paint(QPainter *painter, const QRect &rect,
                          QIcon::Mode mode, QIcon::State state) {
    if (!painter || rect.isEmpty()) return;

    qreal dpr = painter->device()->devicePixelRatio();
    QPixmap pm = pixmap(rect.size() * dpr, mode, state);
    if (!pm.isNull()) {
        pm.setDevicePixelRatio(dpr);
        QSize pmSize     = pm.size() / dpr;
        QSize scaledSize = pmSize.scaled(rect.size(), Qt::KeepAspectRatio);
        int x = rect.x() + (rect.width() - scaledSize.width()) / 2;
        int y = rect.y() + (rect.height() - scaledSize.height()) / 2;
        QRect targetRect(QPoint(x, y), scaledSize);
        painter->drawPixmap(targetRect, pm);
    }
}

qreal SvgIconEngine::getOpacityForModeState(QIcon::Mode mode,
                                            QIcon::State state) {
    auto &tm = ThemeManager::getInstance();
    const double baseOpacity     = tm.getIconBaseOpacity();
    const double disabledOpacity = tm.getIconDisabledOpacity();
    const double activeOpacity   = 1.0;

    if (m_isColored)
        return (mode == QIcon::Disabled) ? disabledOpacity : activeOpacity;
    else
        return (mode == QIcon::Disabled)                        ? disabledOpacity
               : (mode == QIcon::Normal && state == QIcon::Off) ? baseOpacity
                                                                : activeOpacity;
}

QPixmap SvgIconEngine::loadPixmap(const QString &iconName, QIcon::Mode mode,
                                  QIcon::State state, QSize physicalSize,
                                  QColor color) {
    ThemeManager &tm = ThemeManager::getInstance();
    QString path     = tm.getIconPath(iconName, mode, state);

    if (physicalSize.isEmpty()) physicalSize = tm.getIconSize(iconName);
    qreal opacity = getOpacityForModeState(mode, state);

    QImage img(svgToImage(path, physicalSize));
    img = adjustImageOpacity(colorizeBlackPixels(img, color), opacity);
    return QPixmap::fromImage(img);
}

QPixmap SvgIconEngine::pixmap(const QSize &size, QIcon::Mode mode,
                              QIcon::State state) {
    if (m_isTemporaryCommandIcon) return QPixmap();

    ThemeManager &tm = ThemeManager::getInstance();

    if (!tm.hasIcon(m_iconName)) {
        // Fallback for built-in icons that might not be in our theme
        return QPixmap();
    }

    QColor color = getThemeColor(m_iconName, mode, state);
    if (!color.isValid()) color = tm.getIconBaseColor();

    QSize targetSize = m_iconSize.isEmpty() ? size : m_iconSize;
    if (shouldHideIcon(m_isForMenuItem)) return QPixmap();

    QString cacheKey =
        generateCacheKey(m_iconName, targetSize, mode, state);

    QPixmap cachedPm = getFromPixmapCache(cacheKey);
    if (!cachedPm.isNull()) return cachedPm;

    QPixmap pm = loadPixmap(m_iconName, mode, state, targetSize, color);
    if (!pm.isNull()) {
        pm.setDevicePixelRatio(m_dpr);
        addToPixmapCache(cacheKey, pm);
    }

    return pm;
}

bool SvgIconEngine::shouldHideIcon(bool isForMenuItem) {
    return isForMenuItem && !m_isMenuIcon;
}

QSize SvgIconEngine::getBestToolbarSizeByDpr(const QSize &requestedSize) {
    return requestedSize;
}
