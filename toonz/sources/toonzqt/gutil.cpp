

#include "toonzqt/gutil.h"
#include "toonz/preferences.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzCore includes
#include "traster.h"
#include "tpixelutils.h"
#include "tfilepath.h"
#include "tfiletype.h"
#include "tstroke.h"
#include "tcurves.h"
#include "trop.h"
#include "tmsgcore.h"

// Qt includes
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QString>
#include <QApplication>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include <QUrl>
#include <QFileInfo>
#include <QDesktopWidget>
#include <QSvgRenderer>

using namespace DVGui;

//-----------------------------------------------------------------------------

QString fileSizeString(qint64 size, int precision) {
  if (size < 1024)
    return QString::number(size) + " Bytes";
  else if (size < 1024 * 1024)
    return QString::number(size / (1024.0), 'f', precision) + " KB";
  else if (size < 1024 * 1024 * 1024)
    return QString::number(size / (1024 * 1024.0), 'f', precision) + " MB";
  else
    return QString::number(size / (1024 * 1024 * 1024.0), 'f', precision) +
           " GB";
}

//----------------------------------------------------------------

QImage rasterToQImage(const TRasterP &ras, bool premultiplied, bool mirrored) {
  if (TRaster32P ras32 = ras) {
    QImage image(ras->getRawData(), ras->getLx(), ras->getLy(),
                 premultiplied ? QImage::Format_ARGB32_Premultiplied
                               : QImage::Format_ARGB32);
    if (mirrored) return image.mirrored();
    return image;
  } else if (TRasterGR8P ras8 = ras) {
    QImage image(ras->getRawData(), ras->getLx(), ras->getLy(), ras->getWrap(),
                 QImage::Format_Indexed8);
    static QVector<QRgb> colorTable;
    if (colorTable.size() == 0) {
      int i;
      for (i = 0; i < 256; i++) colorTable.append(QColor(i, i, i).rgb());
    }
    image.setColorTable(colorTable);
    if (mirrored) return image.mirrored();
    return image;
  }
  return QImage();
}

//-----------------------------------------------------------------------------

QPixmap rasterToQPixmap(const TRaster32P &ras, bool premultiplied,
                        bool setDevPixRatio) {
  QPixmap pixmap = QPixmap::fromImage(rasterToQImage(ras, premultiplied));
  if (setDevPixRatio) {
    pixmap.setDevicePixelRatio(getDevPixRatio());
  }
  return pixmap;
}

//-----------------------------------------------------------------------------

TRaster32P rasterFromQImage(
    QImage image, bool premultiply,
    bool mirror)  // no need of const& - Qt uses implicit sharing...
{
  QImage copyImage = mirror ? image.mirrored() : image;
  TRaster32P ras(image.width(), image.height(), image.width(),
                 (TPixelRGBM32 *)copyImage.bits(), false);
  if (premultiply) TRop::premultiply(ras);
  return ras->clone();
}

//-----------------------------------------------------------------------------

TRaster32P rasterFromQPixmap(
    QPixmap pixmap, bool premultiply,
    bool mirror)  // no need of const& - Qt uses implicit sharing...
{
  QImage image = pixmap.toImage();
  return rasterFromQImage(image, premultiply, mirror);
}

//-----------------------------------------------------------------------------

void drawPolygon(QPainter &p, const std::vector<QPointF> &points, bool fill,
                 const QColor colorFill, const QColor colorLine) {
  if (points.size() == 0) return;
  p.setPen(colorLine);
  QPolygonF E0Polygon;
  int i = 0;
  for (i = 0; i < (int)points.size(); i++) E0Polygon << QPointF(points[i]);
  E0Polygon << QPointF(points[0]);

  QPainterPath E0Path;
  E0Path.addPolygon(E0Polygon);
  if (fill) p.fillPath(E0Path, QBrush(colorFill));
  p.drawPath(E0Path);
}

//-----------------------------------------------------------------------------

void drawArrow(QPainter &p, const QPointF a, const QPointF b, const QPointF c,
               bool fill, const QColor colorFill, const QColor colorLine) {
  std::vector<QPointF> pts;
  pts.push_back(a);
  pts.push_back(b);
  pts.push_back(c);
  drawPolygon(p, pts, fill, colorFill, colorLine);
}

//-----------------------------------------------------------------------------

QPixmap scalePixmapKeepingAspectRatio(QPixmap pixmap, QSize size,
                                      QColor color) {
  if (pixmap.isNull()) return pixmap;
  if (pixmap.devicePixelRatio() > 1.0) size *= pixmap.devicePixelRatio();
  if (pixmap.size() == size) return pixmap;
  QPixmap scaledPixmap =
      pixmap.scaled(size.width(), size.height(), Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
  QPixmap newPixmap(size);
  newPixmap.fill(color);
  QPainter painter(&newPixmap);
  painter.drawPixmap(double(size.width() - scaledPixmap.width()) * 0.5,
                     double(size.height() - scaledPixmap.height()) * 0.5,
                     scaledPixmap);
  newPixmap.setDevicePixelRatio(pixmap.devicePixelRatio());
  return newPixmap;
}

//-----------------------------------------------------------------------------

QPixmap svgToPixmap(const QString &svgFilePath, const QSize &size,
                    Qt::AspectRatioMode aspectRatioMode, QColor bgColor) {
  static int devPixRatio = getDevPixRatio();
  QSvgRenderer svgRenderer(svgFilePath);
  QSize pixmapSize;
  QRectF renderRect;
  if (size.isEmpty()) {
    pixmapSize = svgRenderer.defaultSize() * devPixRatio;
    renderRect = QRectF(QPointF(), QSizeF(pixmapSize));
  } else {
    pixmapSize = size * devPixRatio;
    if (aspectRatioMode == Qt::KeepAspectRatio ||
        aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
      QSize imgSize = svgRenderer.defaultSize();
      QPointF scaleFactor((float)pixmapSize.width() / (float)imgSize.width(),
                          (float)pixmapSize.height() / (float)imgSize.height());
      float factor = (aspectRatioMode == Qt::KeepAspectRatio)
                         ? std::min(scaleFactor.x(), scaleFactor.y())
                         : std::max(scaleFactor.x(), scaleFactor.y());
      QSizeF renderSize(factor * (float)imgSize.width(),
                        factor * (float)imgSize.height());
      QPointF topLeft(
          ((float)pixmapSize.width() - renderSize.width()) * 0.5f,
          ((float)pixmapSize.height() - renderSize.height()) * 0.5f);
      renderRect = QRectF(topLeft, renderSize);
    } else {  // Qt::IgnoreAspectRatio:
      renderRect = QRectF(QPointF(), QSizeF(pixmapSize));
    }
  }
  QPixmap pixmap(pixmapSize);
  QPainter painter;
  pixmap.fill(bgColor);
  painter.begin(&pixmap);
  svgRenderer.render(&painter, renderRect);
  painter.end();
  pixmap.setDevicePixelRatio(devPixRatio);
  return pixmap;
}

//-----------------------------------------------------------------------------

int getDevPixRatio() {
  static int devPixRatio = QApplication::desktop()->devicePixelRatio();
  return devPixRatio;
}

//-----------------------------------------------------------------------------

QString getIconThemePath(const QString &fileSVGPath) {
  // Use as follows:
  // QPixmap pixmapIcon = getIconThemePath("path/to/file.svg");
  // Is equal to:            :icons/*theme*/path/to/file.svg

  // Set themeable directory
  static QString theme = Preferences::instance()->getIconTheme()
                             ? ":icons/dark/"
                             : ":icons/light/";

  // If no file in light icon theme directory, fallback to dark directory
  if (!QFile::exists(QString(theme + fileSVGPath))) theme = ":icons/dark/";

  return theme + fileSVGPath;
}

//-----------------------------------------------------------------------------

QPixmap setOpacity(QPixmap pixmap, const qreal &opacity) {
  static int devPixRatio = getDevPixRatio();
  const QSize pixmapSize(pixmap.width() * devPixRatio,
                         pixmap.height() * devPixRatio);

  QPixmap opacityPixmap(pixmapSize);
  opacityPixmap.setDevicePixelRatio(devPixRatio);
  opacityPixmap.fill(Qt::transparent);

  if (!pixmap.isNull()) {
    QPainter p(&opacityPixmap);
    QPixmap normalPixmap = pixmap.scaled(pixmapSize, Qt::KeepAspectRatio);
    normalPixmap.setDevicePixelRatio(devPixRatio);
    p.setBackgroundMode(Qt::TransparentMode);
    p.setBackground(QBrush(Qt::transparent));
    p.eraseRect(normalPixmap.rect());
    p.setOpacity(opacity);
    p.drawPixmap(0, 0, normalPixmap);
  }
  return opacityPixmap;
}

//-----------------------------------------------------------------------------

QPixmap recolorPixmap(QPixmap pixmap, QColor color) {
  // Change black pixels to any chosen color
  QImage img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < img.height(); y++) {
    QRgb *pixel = reinterpret_cast<QRgb *>(img.scanLine(y));
    QRgb *end   = pixel + img.width();
    for (; pixel != end; pixel++) {
      // Only recolor zero value (black) pixels
      if (QColor::fromRgba(*pixel).value() == 0)
        *pixel =
            QColor(color.red(), color.green(), color.blue(), qAlpha(*pixel))
                .rgba();
    }
  }
  pixmap = QPixmap::fromImage(img);
  return pixmap;
}

//-----------------------------------------------------------------------------

QPixmap compositePixmap(QPixmap pixmap, qreal opacity, int canvasWidth,
                        int canvasHeight, int iconWidth, int iconHeight,
                        int offset) {
  /* Creates a composite pixmap from two pixmaps. The canvas controls the final
   * size, whereas pixmap is the image to be composited ontop. You can control
   * the position of pixmap by setting an offset (top-left), default is 0. */

  static int devPixRatio = getDevPixRatio();

  QPixmap canvas(canvasWidth, canvasHeight);
  canvas.fill(Qt::transparent);  // set this to a color to debug
  QPixmap combined(canvasWidth, canvasHeight);
  combined.fill(Qt::transparent);
  if (!pixmap.isNull()) {
    QPainter painter;
    painter.begin(&combined);
    QRect canvasRect(0, 0, canvasWidth, canvasHeight);
    painter.drawPixmap(canvasRect, canvas);
    painter.setOpacity(opacity);
    QRect iconRect(offset, offset, iconWidth, iconHeight);
    painter.drawPixmap(iconRect, pixmap);
    painter.end();
  }

  return combined;
}

//-----------------------------------------------------------------------------

QIcon createQIcon(const char *iconSVGName, bool useFullOpacity) {
  static int devPixRatio = getDevPixRatio();

  // get icon size
  QIcon themeIcon = QIcon::fromTheme(iconSVGName);
  QSize iconSize(0, 0);
  for (QList<QSize> sizes = themeIcon.availableSizes(); !sizes.isEmpty();
       sizes.removeFirst())
    if (sizes.first().width() > iconSize.width()) iconSize = sizes.first();

  QString overStr         = QString(iconSVGName) + "_over";
  QString onStr           = QString(iconSVGName) + "_on";
  QPixmap themeIconPixmap = recolorPixmap(themeIcon.pixmap(iconSize));
  QPixmap overPixmap =
      recolorPixmap(QIcon::fromTheme(overStr).pixmap(iconSize));
  QPixmap onPixmap = recolorPixmap(QIcon::fromTheme(onStr).pixmap(iconSize));
  QIcon icon;

  // build icon
  for (int devPixRatio = 1; devPixRatio <= 2; devPixRatio++) {
    int iconW                   = themeIconPixmap.width();
    int iconH                   = themeIconPixmap.height();
    int canvasW                 = iconW;
    int canvasH                 = iconH;
    int offset                  = 0;
    const qreal normalOpacity   = useFullOpacity ? 1 : 0.8;
    const qreal disabledOpacity = 0.15;
    const qreal onOpacity       = 1;

    // off
    icon.addPixmap(compositePixmap(themeIconPixmap, normalOpacity, canvasW,
                                   canvasH, iconW, iconH),
                   QIcon::Normal, QIcon::Off);
    icon.addPixmap(compositePixmap(themeIconPixmap, disabledOpacity, canvasW,
                                   canvasH, iconW, iconH),
                   QIcon::Disabled);

    // over
    icon.addPixmap(
        compositePixmap(!overPixmap.isNull() ? overPixmap : themeIconPixmap,
                        onOpacity, canvasW, canvasH, iconW, iconH),
        QIcon::Active);

    // on
    if (!onPixmap.isNull()) {
      icon.addPixmap(
          compositePixmap(onPixmap, onOpacity, canvasW, canvasH, iconW, iconH),
          QIcon::Normal, QIcon::On);
      icon.addPixmap(compositePixmap(onPixmap, normalOpacity, canvasW, canvasH,
                                     iconW, iconH),
                     QIcon::Disabled, QIcon::On);
    } else {
      icon.addPixmap(compositePixmap(themeIconPixmap, onOpacity, canvasW,
                                     canvasH, iconW, iconH),
                     QIcon::Normal, QIcon::On);
      icon.addPixmap(compositePixmap(themeIconPixmap, disabledOpacity, canvasW,
                                     canvasH, iconW, iconH),
                     QIcon::Disabled, QIcon::On);
    }

    /* If size is 16x16 (suitable for menu) we composite it onto a separate
     * 20x20 pixmap so that it is compatible with toolbars, otherwise it will be
     * scaled up and blur. You need to add icons to all QIcon modes otherwise it
     * will use the original size, which is undesirable. This is equal to having
     * two sets loaded into the icon (16x16 and 20x20) and is dynamically used
     * depending on iconSize for toolbars.
     */
    if (iconSize == (QSize(16, 16))) {
      canvasW = 20 * devPixRatio;
      canvasH = 20 * devPixRatio;
      offset  = 2 * devPixRatio;

      // off
      icon.addPixmap(compositePixmap(themeIconPixmap, normalOpacity, canvasW,
                                     canvasH, iconW, iconH, offset),
                     QIcon::Normal, QIcon::Off);
      icon.addPixmap(compositePixmap(themeIconPixmap, disabledOpacity, canvasW,
                                     canvasH, iconW, iconH, offset),
                     QIcon::Disabled);
      // over
      icon.addPixmap(
          compositePixmap(!overPixmap.isNull() ? overPixmap : themeIconPixmap,
                          onOpacity, canvasW, canvasH, iconW, iconH, offset),
          QIcon::Active);

      // on
      if (!onPixmap.isNull()) {
        icon.addPixmap(compositePixmap(onPixmap, onOpacity, canvasW, canvasH,
                                       iconW, iconH, offset),
                       QIcon::Normal, QIcon::On);
        icon.addPixmap(compositePixmap(onPixmap, disabledOpacity, canvasW,
                                       canvasH, iconW, iconH, offset),
                       QIcon::Disabled, QIcon::On);
      } else {
        icon.addPixmap(compositePixmap(themeIconPixmap, onOpacity, canvasW,
                                       canvasH, iconW, iconH, offset),
                       QIcon::Normal, QIcon::On);
        icon.addPixmap(compositePixmap(themeIconPixmap, disabledOpacity,
                                       canvasW, canvasH, iconW, iconH, offset),
                       QIcon::Disabled, QIcon::On);
      }
    }
  }
  return icon;
}

//-----------------------------------------------------------------------------

QIcon createQIconPNG(const char *iconPNGName) {
  QString normal = QString(":Resources/") + iconPNGName + ".png";
  QString click  = QString(":Resources/") + iconPNGName + "_click.png";
  QString over   = QString(":Resources/") + iconPNGName + "_over.png";

  QIcon icon;
  icon.addFile(normal, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(click, QSize(), QIcon::Normal, QIcon::On);
  icon.addFile(over, QSize(), QIcon::Active);

  return icon;
}

//-----------------------------------------------------------------------------

QIcon createQIconOnOffPNG(const char *iconPNGName, bool withOver) {
  QString on   = QString(":Resources/") + iconPNGName + "_on.png";
  QString off  = QString(":Resources/") + iconPNGName + "_off.png";
  QString over = QString(":Resources/") + iconPNGName + "_over.png";

  QIcon icon;
  icon.addFile(off, QSize(), QIcon::Normal, QIcon::Off);
  icon.addFile(on, QSize(), QIcon::Normal, QIcon::On);
  if (withOver)
    icon.addFile(over, QSize(), QIcon::Active);
  else
    icon.addFile(on, QSize(), QIcon::Active);

  return icon;
}

//-----------------------------------------------------------------------------

QIcon createTemporaryIconFromName(const char *commandName) {
  const int visibleIconSize   = 20;
  const int menubarIconSize   = 16;
  const qreal normalOpacity   = 0.8;
  const qreal disabledOpacity = 0.15;
  const qreal onOpacity       = 1;
  QString name(commandName);
  QList<QChar> iconChar;

  for (int i = 0; i < name.length(); i++) {
    QChar c = name.at(i);
    if (c.isUpper() && iconChar.size() < 2)
      iconChar.append(c);
    else if (c.isDigit()) {
      if (iconChar.isEmpty())
        iconChar.append(c);
      else if (iconChar.size() <= 2) {
        if (iconChar.size() == 2) iconChar.removeLast();
        iconChar.append(c);
        break;
      }
    }
  }

  if (iconChar.isEmpty()) iconChar.append(name.at(0));

  QString iconStr;
  for (auto c : iconChar) iconStr.append(c);

  QIcon icon;
  // prepare for both normal and high dpi
  for (int devPixelRatio = 1; devPixelRatio <= 2; devPixelRatio++) {
    QPixmap transparentPm(menubarIconSize * devPixelRatio,
                          menubarIconSize * devPixelRatio);
    transparentPm.fill(Qt::transparent);

    int pxSize = visibleIconSize * devPixelRatio;

    QPixmap pixmap(pxSize, pxSize);
    QPainter painter;
    pixmap.fill(Qt::transparent);
    painter.begin(&pixmap);

    painter.setPen(Preferences::instance()->getIconTheme() ? Qt::black
                                                           : Qt::white);

    QRect rect(0, -2, pxSize, pxSize);
    if (iconStr.size() == 2) {
      painter.scale(0.6, 1.0);
      rect.setRight(pxSize / 0.6);
    }
    QFont font = painter.font();
    font.setPixelSize(pxSize);
    painter.setFont(font);

    painter.drawText(rect, Qt::AlignCenter, iconStr);

    painter.end();

    icon.addPixmap(transparentPm, QIcon::Normal, QIcon::Off);
    icon.addPixmap(transparentPm, QIcon::Active);
    icon.addPixmap(transparentPm, QIcon::Normal, QIcon::On);
    icon.addPixmap(transparentPm, QIcon::Disabled);
    icon.addPixmap(compositePixmap(pixmap, normalOpacity, pxSize, pxSize,
                                   pixmap.width(), pixmap.height()),
                   QIcon::Normal, QIcon::Off);
    icon.addPixmap(compositePixmap(pixmap, onOpacity, pxSize, pxSize,
                                   pixmap.width(), pixmap.height()),
                   QIcon::Normal, QIcon::On);
    icon.addPixmap(compositePixmap(pixmap, onOpacity, pxSize, pxSize,
                                   pixmap.width(), pixmap.height()),
                   QIcon::Active);
    icon.addPixmap(compositePixmap(pixmap, disabledOpacity, pxSize, pxSize,
                                   pixmap.width(), pixmap.height()),
                   QIcon::Disabled);
  }
  return icon;
}

//-----------------------------------------------------------------------------

QString toQString(const TFilePath &path) {
  return QString::fromStdWString(path.getWideString());
}

//-----------------------------------------------------------------------------

bool isSpaceString(const QString &str) {
  int i;
  QString space(" ");
  for (i = 0; i < str.size(); i++)
    if (str.at(i) != space.at(0)) return false;
  return true;
}

//-----------------------------------------------------------------------------

bool isValidFileName(const QString &fileName) {
  if (fileName.isEmpty() || fileName.contains(":") || fileName.contains("\\") ||
      fileName.contains("/") || fileName.contains(">") ||
      fileName.contains("<") || fileName.contains("*") ||
      fileName.contains("|") || fileName.contains("\"") ||
      fileName.contains("?") || fileName.trimmed().isEmpty())
    return false;
  return true;
}

//-----------------------------------------------------------------------------

bool isValidFileName_message(const QString &fileName) {
  return isValidFileName(fileName)
             ? true
             : (DVGui::error(
                    QObject::tr("The file name cannot be empty or contain any "
                                "of the following "
                                "characters: (new line) \\ / : * ? \" |")),
                false);
}

//-----------------------------------------------------------------------------

bool isReservedFileName(const QString &fileName) {
#ifdef _WIN32
  std::vector<QString> invalidNames{
      "AUX",  "CON",  "NUL",  "PRN",  "COM1", "COM2", "COM3", "COM4",
      "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
      "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

  if (std::find(invalidNames.begin(), invalidNames.end(), fileName) !=
      invalidNames.end())
    return true;
#endif

  return false;
}

//-----------------------------------------------------------------------------

bool isReservedFileName_message(const QString &fileName) {
  return isReservedFileName(fileName)
             ? (DVGui::error(QObject::tr(
                    "That is a reserved file name and cannot be used.")),
                true)
             : false;
}

//-----------------------------------------------------------------------------

QString elideText(const QString &srcText, const QFont &font, int width) {
  QFontMetrics metrix(font);
  int srcWidth = metrix.width(srcText);
  if (srcWidth < width) return srcText;
  int tilde = metrix.width("~");
  int block = (width - tilde) / 2;
  QString text("");
  int i;
  for (i = 0; i < srcText.size(); i++) {
    text += srcText.at(i);
    if (metrix.width(text) > block) break;
  }
  text[i] = '~';
  QString endText("");
  for (i = srcText.size() - 1; i >= 0; i--) {
    endText.push_front(srcText.at(i));
    if (metrix.width(endText) > block) break;
  }
  endText.remove(0, 1);
  text += endText;
  return text;
}

//-----------------------------------------------------------------------------

QString elideText(const QString &srcText, const QFontMetrics &fm, int width,
                  const QString &elideSymbol) {
  QString text(srcText);

  for (int i = text.size(); i > 1 && fm.width(text) > width;)
    text = srcText.left(--i).append(elideSymbol);

  return text;
}

//-----------------------------------------------------------------------------

QUrl pathToUrl(const TFilePath &path) {
  return QUrl::fromLocalFile(QString::fromStdWString(path.getWideString()));
}

//-----------------------------------------------------------------------------

bool isResource(const QString &path) {
  const TFilePath fp(path.toStdWString());
  TFileType::Type type = TFileType::getInfo(fp);

  return (TFileType::isViewable(type) || type & TFileType::MESH_IMAGE ||
          type == TFileType::AUDIO_LEVEL || type == TFileType::TABSCENE ||
          type == TFileType::TOONZSCENE || fp.getType() == "tpl");
}

//-----------------------------------------------------------------------------

bool isResource(const QUrl &url) { return isResource(url.toLocalFile()); }

//-----------------------------------------------------------------------------

bool isResourceOrFolder(const QUrl &url) {
  struct locals {
    static inline bool isDir(const QString &path) {
      return QFileInfo(path).isDir();
    }
  };  // locals

  const QString &path = url.toLocalFile();
  return (isResource(path) || locals::isDir(path));
}

//-----------------------------------------------------------------------------

bool acceptResourceDrop(const QList<QUrl> &urls) {
  int count = 0;
  for (const QUrl &url : urls) {
    if (isResource(url))
      ++count;
    else
      return false;
  }

  return (count > 0);
}

//-----------------------------------------------------------------------------

bool acceptResourceOrFolderDrop(const QList<QUrl> &urls) {
  int count = 0;
  for (const QUrl &url : urls) {
    if (isResourceOrFolder(url))
      ++count;
    else
      return false;
  }

  return (count > 0);
}

//-----------------------------------------------------------------------------

QPainterPath strokeToPainterPath(TStroke *stroke) {
  QPainterPath path;
  int i, chunkSize = stroke->getChunkCount();
  for (i = 0; i < chunkSize; i++) {
    const TThickQuadratic *q = stroke->getChunk(i);
    if (i == 0) path.moveTo(toQPointF(q->getThickP0()));
    path.quadTo(toQPointF(q->getThickP1()), toQPointF(q->getThickP2()));
  }
  return path;
}

//=============================================================================
// TabBarContainter
//-----------------------------------------------------------------------------

TabBarContainter::TabBarContainter(QWidget *parent) : QFrame(parent) {
  setObjectName("TabBarContainer");
  setFrameStyle(QFrame::StyledPanel);
}

//-----------------------------------------------------------------------------

void TabBarContainter::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setPen(getBottomAboveLineColor());
  p.drawLine(0, height() - 2, width(), height() - 2);
  p.setPen(getBottomBelowLineColor());
  p.drawLine(0, height() - 1, width(), height() - 1);
}

//=============================================================================
// ToolBarContainer
//-----------------------------------------------------------------------------

ToolBarContainer::ToolBarContainer(QWidget *parent) : QFrame(parent) {
  setObjectName("ToolBarContainer");
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
}

//-----------------------------------------------------------------------------

void ToolBarContainer::paintEvent(QPaintEvent *event) { QPainter p(this); }

//=============================================================================

QString operator+(const QString &a, const TFilePath &fp) {
  return a + QString::fromStdWString(fp.getWideString());
}
