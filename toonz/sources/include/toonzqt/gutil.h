#pragma once

#ifndef GUTIL_H
#define GUTIL_H

#include "tcommon.h"
#include <QImage>
#include <QFrame>
#include <QIconEngine>
#include <QColor>
#include "traster.h"
#include "toonz/preferences.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TMouseEvent;
class QMouseEvent;
class QTabletEvent;
class QKeyEvent;
class QUrl;
class TFilePath;

//=============================================================================
// Constant definition
//-----------------------------------------------------------------------------

namespace {
const QColor grey120(120, 120, 120);
const QColor grey210(210, 210, 210);
const QColor grey225(225, 225, 225);
const QColor grey190(190, 190, 190);
const QColor grey150(150, 150, 150);

struct SvgRenderParams {
  QSize size;
  QRectF rect;
};

}  // namespace

class QPainter;
class QIcon;
class TFilePath;
class QPainterPath;
class TStroke;

//-----------------------------------------------------------------------------

QString DVAPI fileSizeString(qint64 size, int precision = 2);

//-----------------------------------------------------------------------------

QImage DVAPI rasterToQImage(const TRasterP &ras, bool premultiplied = true,
                            bool mirrored = true);

//-----------------------------------------------------------------------------

QPixmap DVAPI rasterToQPixmap(const TRaster32P &ras, bool premultiplied = true,
                              bool setDevPixRatio = false);

//-----------------------------------------------------------------------------

TRaster32P DVAPI rasterFromQImage(QImage image, bool premultiply = true,
                                  bool mirror = true);

//-----------------------------------------------------------------------------

TRaster32P DVAPI rasterFromQPixmap(QPixmap pixmap, bool premultiply = true,
                                   bool mirror = true);

//-----------------------------------------------------------------------------

void DVAPI drawPolygon(QPainter &p, const std::vector<QPointF> &points,
                       bool fill = false, const QColor colorFill = Qt::white,
                       const QColor colorLine = Qt::black);

//-----------------------------------------------------------------------------

void DVAPI drawArrow(QPainter &p, const QPointF a, const QPointF b,
                     const QPointF c, bool fill = false,
                     const QColor colorFill = Qt::white,
                     const QColor colorLine = Qt::black);

//-----------------------------------------------------------------------------

QPixmap DVAPI scalePixmapKeepingAspectRatio(QPixmap p, QSize size,
                                            QColor color = Qt::white);

//-----------------------------------------------------------------------------

SvgRenderParams calculateSvgRenderParams(const QSize &desiredSize,
                                         QSize &imageSize,
                                         Qt::AspectRatioMode aspectRatioMode);

//-----------------------------------------------------------------------------

QPixmap DVAPI
svgToPixmap(const QString &svgFilePath, QSize size = QSize(),
            Qt::AspectRatioMode aspectRatioMode = Qt::IgnoreAspectRatio,
            QColor bgColor                      = Qt::transparent);

//-----------------------------------------------------------------------------

QImage DVAPI
svgToImage(const QString &svgFilePath, QSize size = QSize(),
           Qt::AspectRatioMode aspectRatioMode = Qt::IgnoreAspectRatio,
           QColor bgColor                      = Qt::transparent);

//-----------------------------------------------------------------------------
// returns device-pixel ratio. It is 1 for normal monitors and 2 (or higher
// ratio) for high DPI monitors. Setting "Display > Set custom text size(DPI)"
// for Windows corresponds to this ratio.
int DVAPI getDevicePixelRatio(const QWidget *widget = nullptr);

//-----------------------------------------------------------------------------

QImage DVAPI colorizeBlackPixels(const QImage &input,
                                 const QColor color = QColor());

//-----------------------------------------------------------------------------

QPixmap DVAPI expandPixmap(const QPixmap &pixmap,
                           const QSize &expandSize = QSize(),
                           const QColor bgColor    = Qt::transparent);

//-----------------------------------------------------------------------------

QIcon DVAPI createQIcon(const QString &iconName, bool isForMenuItem = false,
                        qreal dpr = 0.0, QSize newSize = QSize());
QIcon DVAPI createQIconPNG(const char *iconPNGName);
QIcon DVAPI createQIconOnOffPNG(const char *iconPNGName, bool withOver = true);
QIcon DVAPI createTemporaryIconFromName(const char *commandName);

QImage DVAPI adjustImageOpacity(const QImage &input, qreal opacity = 1.0);

inline QSize dimension2QSize(const TDimension &sz) {
  return QSize(sz.lx, sz.ly);
}
inline TDimension qsize2Dimension(const QSize &sz) {
  return TDimension(sz.width(), sz.height());
}
QString DVAPI toQString(const TFilePath &path);
bool DVAPI isSpaceString(const QString &str);
bool DVAPI isValidFileName(const QString &fileName);
bool DVAPI isValidFileName_message(const QString &fileName);
bool DVAPI isReservedFileName(const QString &fileName);
bool DVAPI isReservedFileName_message(const QString &fileName);

QString DVAPI elideText(const QString &columnName, const QFont &font,
                        int width);
QString DVAPI elideText(const QString &columnName, const QFontMetrics &fm,
                        int width, const QString &elideSymbol);
QUrl DVAPI pathToUrl(const TFilePath &path);

bool DVAPI isResource(const QString &path);
bool DVAPI isResource(const QUrl &url);
bool DVAPI isResourceOrFolder(const QUrl &url);

bool DVAPI acceptResourceDrop(const QList<QUrl> &urls);
bool DVAPI acceptResourceOrFolderDrop(const QList<QUrl> &urls);

inline QPointF toQPointF(const TPointD &p) { return QPointF(p.x, p.y); }
inline QPointF toQPointF(const TPoint &p) { return QPointF(p.x, p.y); }
inline QPoint toQPoint(const TPoint &p) { return QPoint(p.x, p.y); }
inline TPointD toTPointD(const QPointF &p) { return TPointD(p.x(), p.y()); }
inline TPointD toTPointD(const QPoint &p) { return TPointD(p.x(), p.y()); }
inline TPoint toTPoint(const QPoint &p) { return TPoint(p.x(), p.y()); }

inline QRect toQRect(const TRect &r) {
  return QRect(r.x0, r.y0, r.getLx(), r.getLy());
}
inline QRectF toQRectF(const TRectD &r) {
  return QRectF(r.x0, r.y0, r.getLx(), r.getLy());
}
inline QRectF toQRectF(const TRect &r) {
  return QRectF(r.x0, r.y0, r.getLx(), r.getLy());
}
inline TRect toTRect(const QRect &r) {
  return TRect(r.left(), r.top(), r.right(), r.bottom());
}
inline TRectD toTRectD(const QRectF &r) {
  return TRectD(r.left(), r.top(), r.right(), r.bottom());
}
inline TRectD toTRectD(const QRect &r) {
  return TRectD(r.left(), r.top(), r.right() + 1, r.bottom() + 1);
}

QPainterPath DVAPI strokeToPainterPath(TStroke *stroke);

//-----------------------------------------------------------------------------
// This widget is only used to set the background color of the tabBar
// using the styleSheet.
// It is also used to take 6px on the left before the tabBar

class DVAPI TabBarContainter final : public QFrame {
  Q_OBJECT
public:
  TabBarContainter(QWidget *parent = 0);

protected:
  QColor m_bottomBelowLineColor;
  Q_PROPERTY(QColor BottomBelowLineColor READ getBottomBelowLineColor WRITE
                 setBottomBelowLineColor);
  void paintEvent(QPaintEvent *event) override;
  void setBottomBelowLineColor(const QColor &color) {
    m_bottomBelowLineColor = color;
  }
  QColor getBottomBelowLineColor() const { return m_bottomBelowLineColor; }
};

//-----------------------------------------------------------------------------
// This widget is only used to set the background color of the playToolBar
// using the styleSheet. And to put a line in the upper zone

class DVAPI ToolBarContainer final : public QFrame {
public:
  ToolBarContainer(QWidget *parent = 0);

protected:
  void paintEvent(QPaintEvent *event) override;
};

QString DVAPI operator+(const QString &a, const TFilePath &fp);

//-----------------------------------------------------------------------------
// QPixmapCache

void DVAPI addToPixmapCache(const QString &key, const QPixmap &pixmap);
QPixmap DVAPI getFromPixmapCache(const QString &key);
void DVAPI clearQPixmapCache();

//-----------------------------------------------------------------------------
// ThemeManager

class DVAPI ThemeManager {
public:
  // Singleton Instance
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
  void setIconDisabledOpacity(const double &val) {
    m_iconDisabledOpacity = val;
  }
  double getIconDisabledOpacity() const { return m_iconDisabledOpacity; }

  void setInterfaceColor(const QColor &color) { m_interfaceColor = color; }
  QColor getInterfaceColor() const { return m_interfaceColor; }

  void setIconBaseColor(const QColor &color) { m_iconBaseColor = color; }
  QColor getIconBaseColor() const { return m_iconBaseColor; }

  void setIconActiveColor(const QColor &color) { m_iconActiveColor = color; }
  QColor getIconActiveColor() const { return m_iconActiveColor; }

  void setIconOnColor(const QColor &color) { m_iconOnColor = color; }
  QColor getIconOnColor() const { return m_iconOnColor; }

  void setIconSelectedColor(const QColor &color) {
    m_iconSelectedColor = color;
  }
  QColor getIconSelectedColor() const { return m_iconSelectedColor; }

  void setIconCloseColor(const QColor &color) { m_iconCloseColor = color; }
  QColor getIconCloseColor() const { return m_iconCloseColor; }

  void setIconPreviewColor(const QColor &color) { m_iconPreviewColor = color; }
  QColor getIconPreviewColor() const { return m_iconPreviewColor; }

  void setIconVCheckColor(const QColor &color) { m_iconVCheckColor = color; }
  QColor getIconVCheckColor() const { return m_iconVCheckColor; }

  void setIconLockColor(const QColor &color) { m_iconLockColor = color; }
  QColor getIconLockColor() const { return m_iconLockColor; }

  void setIconKeyframeColor(const QColor &color) {
    m_iconKeyframeColor = color;
  }
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
  // Constructor & Restriction
  ThemeManager();
  Q_DISABLE_COPY_MOVE(ThemeManager)

  // Icon Metadata Parsing
  QSize parseIconSize(const QString &iconPath) const;
  bool parseIsColored(const QString &iconPath) const;

  // Icon Data
  QHash<QString, QString> m_iconPaths;
  QHash<QString, QSize> m_iconSizes;
  QHash<QString, bool> m_coloredIcons;
  QHash<QString, bool> m_menuIcons;
  QHash<QString, QString> m_customProperties;

  // Theme Colors
  double m_iconBaseOpacity;
  double m_iconDisabledOpacity;

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

QString DVAPI getIconPath(const QString &iconSVGName);

//-----------------------------------------------------------------------------
// SVGIconEngine

// Cache Key Generator
QString DVAPI generateCacheKey(const QString &keyName, const QSize &size,
                               QIcon::Mode mode, QIcon::State state);

class DVAPI SvgIconEngine : public QIconEngine {
public:
  // Constructors
  SvgIconEngine(const QString &iconName, bool isForMenuItem = false,
                qreal dpr = 0.0, QSize newSize = QSize());
  SvgIconEngine(const QString &commandName, const QImage image);

  // Cloning
  QIconEngine *clone() const override;

  // Core Functionality
  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
             QIcon::State state) override;
  QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                 QIcon::State state) override;
  QColor getThemeColor(const QString &iconName, QIcon::Mode mode,
                       QIcon::State state);

private:
  // Icon Properties
  QString m_iconName;  // Icon base name
  QSize m_iconSize;    // Requested icon size
  qreal m_dpr;         // Device pixel ratio

  // Icon Type Flags
  bool m_isForMenuItem;  // If intended for menus
  bool m_isMenuIcon;     // If sized for menus
  bool m_isColored;      // If full-color image

  // Icon Color & Image
  QColor m_color;  // Theme color of the icon
  QImage m_image;  // For storing temporary command icon image
  bool m_isTemporaryCommandIcon;

  // Helper Methods
  qreal getOpacityForModeState(QIcon::Mode mode, QIcon::State state);
  QColor getUniqueIconColor(const QString &iconName, QIcon::Mode mode,
                            QIcon::State state);
  QPixmap loadPixmap(const QString &iconName, QIcon::Mode mode = QIcon::Normal,
                     QIcon::State state = QIcon::Off,
                     QSize physicalSize = QSize(), QColor color = QColor());
  bool shouldHideIcon(bool isForMenuItem);
  QSize getBestToolbarSizeByDpr(const QSize &requestedSize);
};

//-----------------------------------------------------------------------------

#endif  // GUTIL_H
