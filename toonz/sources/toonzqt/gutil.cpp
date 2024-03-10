

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
#include <QDirIterator>
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
#include <QScreen>
#include <QWindow>
#include <QDebug>

using namespace DVGui;

namespace {
inline bool hasScreensWithDifferentDevPixRatio() {
  static bool ret     = false;
  static bool checked = false;
  if (!checked) {  // check once
    int dpr = QApplication::desktop()->devicePixelRatio();
    for (auto screen : QGuiApplication::screens()) {
      if ((int)screen->devicePixelRatio() != dpr) {
        ret = true;
        break;
      }
    }
    checked = true;
  }
  return ret;
}

int getHighestDevicePixelRatio() {
  static int highestDevPixRatio = 0;
  if (highestDevPixRatio == 0) {
    for (auto screen : QGuiApplication::screens())
      highestDevPixRatio =
          std::max(highestDevPixRatio, (int)screen->devicePixelRatio());
  }
  return highestDevPixRatio;
}
}  // namespace
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
    pixmap.setDevicePixelRatio(getDevicePixelRatio());
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

int getDevicePixelRatio(const QWidget *widget) {
  if (hasScreensWithDifferentDevPixRatio() && widget) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    return widget->screen()->devicePixelRatio();
#else
    if (!widget->windowHandle()) widget->winId();
    if (widget->windowHandle())
      return widget->windowHandle()->devicePixelRatio();
#endif
  }
  static int devPixRatio = QApplication::desktop()->devicePixelRatio();
  return devPixRatio;
}

//-----------------------------------------------------------------------------

// Calculate render params for use by svgToImage() and svgToPixmap()
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
    } else {  // Qt::IgnoreAspectRatio:
      params.rect = QRectF(QPointF(), QSizeF(params.size));
    }
  }
  return params;
}

//-----------------------------------------------------------------------------

QPixmap svgToPixmap(const QString &svgFilePath, QSize size,
                    Qt::AspectRatioMode aspectRatioMode, QColor bgColor) {
  if (svgFilePath.isEmpty()) return QPixmap();

  QSvgRenderer svgRenderer(svgFilePath);

  // Check if SVG file was loaded correctly
  if (!svgRenderer.isValid()) {
    qDebug() << "Invalid SVG file:" << svgFilePath;
    return QPixmap();
  }

  static int devPixRatio = getHighestDevicePixelRatio();
  if (!size.isEmpty()) size *= devPixRatio;

  QSize imageSize = svgRenderer.defaultSize() * devPixRatio;
  SvgRenderParams params =
      calculateSvgRenderParams(size, imageSize, aspectRatioMode);
  QPixmap pixmap(params.size);
  QPainter painter;
  pixmap.fill(bgColor);

  if (!painter.begin(&pixmap)) {
    qDebug() << "Failed to begin QPainter on pixmap";
    return QPixmap();
  }

  svgRenderer.render(&painter, params.rect);
  painter.end();
  pixmap.setDevicePixelRatio(devPixRatio);
  return pixmap;
}

//-----------------------------------------------------------------------------

QImage svgToImage(const QString &svgFilePath, QSize size,
                  Qt::AspectRatioMode aspectRatioMode, QColor bgColor) {
  if (svgFilePath.isEmpty()) return QImage();

  QSvgRenderer svgRenderer(svgFilePath);

  // Check if SVG file was loaded correctly
  if (!svgRenderer.isValid()) {
    qDebug() << "Invalid SVG file:" << svgFilePath;
    return QImage();
  }

  static int devPixRatio = getHighestDevicePixelRatio();

  QSize imageSize = svgRenderer.defaultSize() * devPixRatio;
  SvgRenderParams params =
      calculateSvgRenderParams(size, imageSize, aspectRatioMode);
  QImage image(params.size, QImage::Format_ARGB32_Premultiplied);
  QPainter painter;
  image.fill(bgColor);

  if (!painter.begin(&image)) {
    qDebug() << "Failed to begin QPainter on image";
    return QImage();
  }

  svgRenderer.render(&painter, params.rect);
  painter.end();
  return image;
}

//-----------------------------------------------------------------------------

// Change the opacity of a QImage (0.0 - 1.0)
QImage adjustImageOpacity(const QImage &input, qreal opacity) {
  if (input.isNull()) return QImage();

  QImage result(input.size(), QImage::Format_ARGB32_Premultiplied);

  QPainter painter(&result);
  if (!painter.isActive()) return QImage();

  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(result.rect(), Qt::transparent);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.drawImage(0, 0, input);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.fillRect(
      result.rect(),
      QColor(0, 0, 0, qBound(0, static_cast<int>(opacity * 255), 255)));
  painter.end();

  return result;
}

//-----------------------------------------------------------------------------

// Resizes a QImage, if scaleInput is true then input image will scale according
// to newSize otherwise the input image will be centered and not scaled
QImage compositeImage(const QImage &input, QSize newSize, bool scaleInput,
                      QColor bgColor) {
  if (input.isNull()) return QImage();

  int devPixRatio = getHighestDevicePixelRatio();

  int w, h, x = 0, y = 0;
  if (newSize.isEmpty()) {
    w = input.width();
    h = input.height();
  } else {
    w = newSize.width() * devPixRatio;
    h = newSize.height() * devPixRatio;
    if (!scaleInput) {
      x = (w - input.width()) / 2;
      y = (h - input.height()) / 2;
    }
  }

  QImage newImage(w, h, QImage::Format_ARGB32_Premultiplied);
  newImage.fill(bgColor);

  if (scaleInput) {
    return input.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  } else {
    QPainter painter(&newImage);
    if (!painter.isActive()) return QImage();
    painter.drawImage(QPoint(x, y), input);
    painter.end();

    return newImage;
  }
}

//-----------------------------------------------------------------------------

// Convert QImage to QPixmap and set device pixel ratio
QPixmap convertImageToPixmap(const QImage &image) {
  if (image.isNull()) return QPixmap();

  QPixmap pixmap(QPixmap::fromImage(image));
  int devPixRatio = getHighestDevicePixelRatio();
  pixmap.setDevicePixelRatio(devPixRatio);

  return pixmap;
}

//-----------------------------------------------------------------------------

// Load, theme colorize and change opacity of an icon image
QImage generateIconImage(const QString &iconSVGName, qreal opacity,
                         QSize newSize, Qt::AspectRatioMode aspectRatioMode) {
  static ThemeManager &themeManager = ThemeManager::getInstance();

  if (iconSVGName.isEmpty() || !themeManager.hasIcon(iconSVGName)) {
    return QImage();
  }

  int devPixRatio = getHighestDevicePixelRatio();
  newSize *= devPixRatio;

  // Path to icon image
  const QString imgPath = themeManager.getIconPath(iconSVGName);

  // Convert SVG to QImage
  QImage image(svgToImage(imgPath, newSize, aspectRatioMode));

  // Colorize QImage
  image = themeManager.recolorBlackPixels(image);

  // Change opacity if required
  if (opacity != qreal(1.0)) image = adjustImageOpacity(image, opacity);

  return image;
}

//-----------------------------------------------------------------------------

// Load, theme colorize and change opacity of an icon image file
QPixmap generateIconPixmap(const QString &iconSVGName, qreal opacity,
                           QSize newSize, Qt::AspectRatioMode aspectRatioMode) {
  QImage image =
      generateIconImage(iconSVGName, opacity, newSize, aspectRatioMode);
  return convertImageToPixmap(image);
}

//-----------------------------------------------------------------------------

// Process and populate all modes and states of a QIcon
void addImagesToIcon(QIcon &icon, const QImage &baseImg, const QImage &overImg,
                     const QImage &onImg, bool useFullOpacity) {
  if (baseImg.isNull()) return;

  ThemeManager &themeManager = ThemeManager::getInstance();
  const qreal offOpacity = useFullOpacity ? 1.0 : themeManager.getOffOpacity();
  const qreal onOpacity  = themeManager.getOnOpacity();
  const qreal disabledOpacity = themeManager.getDisabledOpacity();

  // Generate more images using input images for other modes and states
  QImage offImg      = adjustImageOpacity(baseImg, offOpacity);
  QImage disabledImg = adjustImageOpacity(baseImg, disabledOpacity);
  QImage onDisabledImg =
      !onImg.isNull() ? adjustImageOpacity(onImg, disabledOpacity) : QImage();

  // Convert images to pixmaps and set device pixel ratio
  QPixmap basePm(convertImageToPixmap(baseImg));
  QPixmap offPm(convertImageToPixmap(offImg));
  QPixmap disabledPm(convertImageToPixmap(disabledImg));
  QPixmap overPm(convertImageToPixmap(overImg));
  QPixmap onPm(convertImageToPixmap(onImg));
  QPixmap onDisabledPm(convertImageToPixmap(onDisabledImg));

  // Add pixmaps to icon and fallback to basePm if 'over' and 'on' are null
  icon.addPixmap(offPm, QIcon::Normal, QIcon::Off);
  icon.addPixmap(disabledPm, QIcon::Disabled, QIcon::Off);
  icon.addPixmap(!overPm.isNull() ? overPm : basePm, QIcon::Active);
  icon.addPixmap(!onPm.isNull() ? onPm : basePm, QIcon::Normal, QIcon::On);
  icon.addPixmap(!onPm.isNull() ? onDisabledPm : disabledPm, QIcon::Disabled,
                 QIcon::On);
}

//-----------------------------------------------------------------------------

void addSpecifiedSizedImageToIcon(QIcon &icon, const char *iconSVGName,
                                  QSize newSize) {
  static int devPixRatio = getHighestDevicePixelRatio();
  newSize *= devPixRatio;

  // Construct icon filenames
  QString iconName     = QString::fromUtf8(iconSVGName);
  QString overIconName = iconName + "_over";
  QString onIconName   = iconName + "_on";

  // Generate icon images
  QImage baseImg = generateIconImage(iconName, 1.0, newSize);
  QImage overImg = generateIconImage(overIconName, 1.0, newSize);
  QImage onImg   = generateIconImage(onIconName, 1.0, newSize);

  // Add newly sized images to the icon
  addImagesToIcon(icon, baseImg, overImg, onImg);
}

//-----------------------------------------------------------------------------

// Add the same pixmap to all modes and states of a QIcon
void addPixmapToAllModesAndStates(QIcon &icon, const QPixmap &pixmap) {
  QIcon::Mode modes[]   = {QIcon::Normal, QIcon::Disabled, QIcon::Selected};
  QIcon::State states[] = {QIcon::On, QIcon::Off};

  for (const auto &mode : modes) {
    for (const auto &state : states) {
      icon.addPixmap(pixmap, mode, state);
    }
  }
  icon.addPixmap(pixmap, QIcon::Active, QIcon::Off);
}

//-----------------------------------------------------------------------------

/// @brief Return a themed icon
/// @param iconSVGName Name of the icon (SVG file base name)
/// @param useFullOpacity If true the icon will be max brightness
/// @param isForMenuItem For special handling of menu icons
/// @param newSize Render the SVG images of the icon at the specified size
/// @return QIcon
QIcon createQIcon(const QString &iconSVGName, bool useFullOpacity,
                  bool isForMenuItem, QSize newSize) {
  static ThemeManager &themeManager = ThemeManager::getInstance();
  if (iconSVGName.isEmpty() || !themeManager.hasIcon(iconSVGName)) {
    // Use debug to check if something calls for an icon that doesn't exist
    //qDebug () << "File not found:" << iconSVGName;
    return QIcon();
  }

  static int devPixRatio = getHighestDevicePixelRatio();

  QImage baseImg(generateIconImage(iconSVGName, qreal(1.0), newSize));
  QImage overImg(generateIconImage(iconSVGName + "_over", qreal(1.0), newSize));
  QImage onImg(generateIconImage(iconSVGName + "_on", qreal(1.0), newSize));

  QIcon icon;
  
  // START_BUG_WORKAROUND: #20230627
  // Set an empty pixmap for menu icons when hiding icons from menus is true,
  // search bug ID for more info.
#ifdef _WIN32
  bool showIconInMenu = Preferences::instance()->getBoolValue(showIconsInMenu);
  if (isForMenuItem && baseImg.width() == (16 * devPixRatio) &&
      baseImg.height() == (16 * devPixRatio) && !showIconInMenu) {
    static QPixmap emptyPm(16 * devPixRatio, 16 * devPixRatio);
    emptyPm.fill(Qt::transparent);
    addPixmapToAllModesAndStates(icon, emptyPm);
  } else
#endif  // END_BUG_WORKAROUND
  {
    addImagesToIcon(icon, baseImg, overImg, onImg, useFullOpacity);
  }

  // For tool bars we draw menu sized icons onto tool bar sized images otherwise
  // there can be scaling artifacts with high dpi and load these in addition
  if (baseImg.width() == (16 * devPixRatio) &&
      baseImg.height() == (16 * devPixRatio)) {
    for (auto screen : QApplication::screens()) {
      QSize expandSize(20, 20);
      int otherDevPixRatio = screen->devicePixelRatio();
      if (otherDevPixRatio != devPixRatio) {
        expandSize.setWidth(16 * otherDevPixRatio);
        expandSize.setHeight(16 * otherDevPixRatio);
      }
      QImage toolBaseImg(compositeImage(baseImg, expandSize));
      QImage toolOverImg(compositeImage(overImg, expandSize));
      QImage toolOnImg(compositeImage(onImg, expandSize));
      addImagesToIcon(icon, toolBaseImg, toolOverImg, toolOnImg,
                      useFullOpacity);
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
  const int visibleIconSize = 20;
  const int menubarIconSize = 16;
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
  // Prepare for both normal and high dpi
  for (int devPixelRatio = 1; devPixelRatio <= 2; devPixelRatio++) {
    QImage transparentImg(menubarIconSize * devPixelRatio,
                          menubarIconSize * devPixelRatio,
                          QImage::Format_ARGB32);
    transparentImg.fill(Qt::transparent);

    int pxSize = visibleIconSize * devPixelRatio;

    QImage charImg(pxSize, pxSize, QImage::Format_ARGB32_Premultiplied);
    QPainter painter;
    charImg.fill(Qt::transparent);
    painter.begin(&charImg);

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

    // For menu only
    addPixmapToAllModesAndStates(icon, QPixmap::fromImage(transparentImg));

    // For toolbars
    addImagesToIcon(icon, charImg);
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
  int srcWidth = metrix.horizontalAdvance(srcText);
  if (srcWidth < width) return srcText;
  int tilde = metrix.horizontalAdvance("~");
  int block = (width - tilde) / 2;
  QString text("");
  int i;
  for (i = 0; i < srcText.size(); i++) {
    text += srcText.at(i);
    if (metrix.horizontalAdvance(text) > block) break;
  }
  text[i] = '~';
  QString endText("");
  for (i = srcText.size() - 1; i >= 0; i--) {
    endText.push_front(srcText.at(i));
    if (metrix.horizontalAdvance(endText) > block) break;
  }
  endText.remove(0, 1);
  text += endText;
  return text;
}

//-----------------------------------------------------------------------------

QString elideText(const QString &srcText, const QFontMetrics &fm, int width,
                  const QString &elideSymbol) {
  QString text(srcText);

  for (int i = text.size(); i > 1 && fm.horizontalAdvance(text) > width;)
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

//=============================================================================
// Theme Manager
//-----------------------------------------------------------------------------

class ThemeManager::ThemeManagerImpl {
public:
  QMap<QString, QString> m_iconPaths;
  qreal m_onOpacity       = 1.0;
  qreal m_offOpacity      = 0.8;
  qreal m_disabledOpacity = 0.3;
};

//-----------------------------------------------------------------------------

ThemeManager::ThemeManager() : impl(new ThemeManagerImpl) {}

//-----------------------------------------------------------------------------

ThemeManager::~ThemeManager() {}

//-----------------------------------------------------------------------------

ThemeManager &ThemeManager::getInstance() {
  static ThemeManager instance;
  return instance;
}

//-----------------------------------------------------------------------------

// Populate a QMap with icon filepaths and assign their basename as the key
void ThemeManager::buildIconPathsMap(const QString &path) {
  QDir dir(path);
  if (!dir.exists(path)) {
    qDebug() << "Resource path does not exist:" << path;
    return;
  }

  QDirIterator it(path,
                  QStringList() << "*.svg"
                                << "*.png",
                  QDir::Files, QDirIterator::Subdirectories);

  while (it.hasNext()) {
    it.next();

    const QString iconPath = it.fileInfo().filePath();
    const QString iconName = it.fileInfo().baseName();

    if (!impl->m_iconPaths.contains(iconName)) {
      impl->m_iconPaths.insert(iconName, iconPath);
    } else {
      qDebug() << "Icon with file name already exists in iconPaths, ensure "
                  "icons have unique file names:"
               << "\nCurrently added:" << getIconPath(iconName)
               << "\nTried to add:" << iconPath;
    }
  }
}

//-----------------------------------------------------------------------------

// Get the full filepath of an icon image by basename
QString ThemeManager::getIconPath(const QString &iconName) const {
  return impl->m_iconPaths.value(iconName);
}

//-----------------------------------------------------------------------------

// Boolean to check if iconName is contained within the iconPaths QMap
bool ThemeManager::hasIcon(const QString &iconName) const {
  return impl->m_iconPaths.contains(iconName);
}

//-----------------------------------------------------------------------------

qreal ThemeManager::getOnOpacity() const { return impl->m_onOpacity; }

//-----------------------------------------------------------------------------

qreal ThemeManager::getOffOpacity() const { return impl->m_offOpacity; }

//-----------------------------------------------------------------------------

qreal ThemeManager::getDisabledOpacity() const {
  return impl->m_disabledOpacity;
}

//-----------------------------------------------------------------------------

// Colorize black pixels in a QImage while retaining other colors, however be
// mindful that if black pixels overlap color pixels it can cause artifacting
QImage ThemeManager::recolorBlackPixels(const QImage &input, QColor color) {
  if (input.isNull() || color == Qt::black) return QImage();

  // Default is icon theme color
  if (!color.isValid())
    color = Preferences::instance()->getIconTheme() ? Qt::black : Qt::white;

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

//-----------------------------------------------------------------------------

// Colorize black pixels in a QPixmap while retaining other colors, however be
// mindful that if black pixels overlap color pixels if can cause artifacting
QPixmap ThemeManager::recolorBlackPixels(const QPixmap &input, QColor color) {
  if (input.isNull() || color == Qt::black) return QPixmap();

  QImage image          = input.toImage();
  QImage recoloredImage = recolorBlackPixels(image, color);
  QPixmap pixmap        = convertImageToPixmap(recoloredImage);

  return pixmap;
}

//-----------------------------------------------------------------------------

// For debugging contents
void ThemeManager::printiconPathsMap() {
  const QMap<QString, QString> map = impl->m_iconPaths;
  qDebug() << "Contents of QMap:";
  for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
    qDebug() << it.key() << ":" << it.value();
  }
}

//-----------------------------------------------------------------------------

// Public version of ThemeManager::getIconPath()
QString getIconPath(const QString &path) {
  return ThemeManager::getInstance().getIconPath(path);
}
