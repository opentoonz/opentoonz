#pragma once

#ifndef GUTIL_H
#define GUTIL_H

#include "tcommon.h"
#include <QImage>
#include <QFrame>
#include <QColor>
#include "traster.h"

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

QPixmap DVAPI
svgToPixmap(const QString &svgFilePath, const QSize &size = QSize(),
            Qt::AspectRatioMode aspectRatioMode = Qt::IgnoreAspectRatio,
            QColor bgColor                      = Qt::transparent);

//-----------------------------------------------------------------------------
// returns device-pixel ratio. It is 1 for normal monitors and 2 (or higher
// ratio) for high DPI monitors. Setting "Display > Set custom text size(DPI)"
// for Windows corresponds to this ratio.
int DVAPI getDevPixRatio();

//-----------------------------------------------------------------------------
QIcon DVAPI createQIcon(const char *iconSVGName);
QIcon DVAPI createQIconPNG(const char *iconPNGName);
QIcon DVAPI createQIconOnOff(const char *iconSVGName, bool withOver = true);
QIcon DVAPI createQIconOnOffPNG(const char *iconPNGName, bool withOver = true);

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
  QColor m_bottomBelowLineColor, m_bottomAboveLineColor;
  Q_PROPERTY(QColor BottomBelowLineColor READ getBottomBelowLineColor WRITE
                 setBottomBelowLineColor);
  Q_PROPERTY(QColor BottomAboveLineColor READ getBottomAboveLineColor WRITE
                 setBottomAboveLineColor);
  void paintEvent(QPaintEvent *event) override;
  void setBottomBelowLineColor(const QColor &color) {
    m_bottomBelowLineColor = color;
  }
  QColor getBottomBelowLineColor() const { return m_bottomBelowLineColor; }
  void setBottomAboveLineColor(const QColor &color) {
    m_bottomAboveLineColor = color;
  }
  QColor getBottomAboveLineColor() const { return m_bottomAboveLineColor; }
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

#endif  // GUTIL_H
