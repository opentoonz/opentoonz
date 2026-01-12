

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
#include <QPixmapCache>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>
#include <QIconEngine>
#include <QString>
#include <QApplication>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QKeyEvent>
#include <QUrl>
#include <QFileInfo>
#include <QDesktopWidget>
#include <QSvgRenderer>
#include <QRegularExpression>
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
    return widget->screen()->devicePixelRatio();
  }
  static int devPixRatio = QApplication::desktop()->devicePixelRatio();
  return devPixRatio;
}

//-----------------------------------------------------------------------------

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

  QSize imageSize = !size.isEmpty() ? size : svgRenderer.defaultSize();
  SvgRenderParams params =
      calculateSvgRenderParams(size, imageSize, aspectRatioMode);
  QImage image(params.size, QImage::Format_ARGB32_Premultiplied);
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

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

// Expand area around pixmap and center it
QPixmap expandPixmap(const QPixmap &pixmap, const QSize &expandSize,
                     QColor bgColor) {
  if (pixmap.isNull()) return QPixmap();
  if (pixmap.size() == expandSize) return pixmap;

  // Get DPR from pixmap
  qreal dpr = pixmap.devicePixelRatioF();

  // Create a new pixmap with the specified size in pixels
  QPixmap expandedPixmap(expandSize);
  expandedPixmap.setDevicePixelRatio(dpr);

  // Fill with background color
  expandedPixmap.fill(bgColor);

  // Create a painter
  QPainter painter(&expandedPixmap);

  // Calculate the centered position in pixel coordinates
  int xPos = (expandSize.width() - pixmap.width()) / (2 * dpr);
  int yPos = (expandSize.height() - pixmap.height()) / (2 * dpr);

  // Draw the original pixmap
  painter.drawPixmap(xPos, yPos, pixmap);

  return expandedPixmap;
}

//-----------------------------------------------------------------------------

QIcon createQIcon(const QString &iconName, bool isForMenuItem, qreal dpr,
                  QSize newSize) {
  if (iconName.isEmpty() || !ThemeManager::getInstance().hasIcon(iconName)) {
    // qWarning() << "No icon with name " << iconName << " exists.";
    return QIcon();
  }

  return QIcon(new SvgIconEngine(iconName, isForMenuItem, dpr, newSize));
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
  QString name(commandName);
  QList<QChar> iconChar;

  // Extract up to 2 characters from the command name
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

  QColor textColor = Qt::black;

  // Create the image with the characters
  int devPixelRatio = 2;  // Using highest DPR for best quality
  int pxSize        = visibleIconSize * devPixelRatio;
  QImage charImg(pxSize, pxSize, QImage::Format_ARGB32_Premultiplied);
  charImg.fill(Qt::transparent);

  QPainter painter(&charImg);
  painter.setPen(textColor);

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

  return QIcon(new SvgIconEngine(iconStr, charImg));
}

//-----------------------------------------------------------------------------

QImage adjustImageOpacity(const QImage &input, qreal opacity) {
  if (input.isNull()) return QImage();
  if (opacity == 1.0) return input;

  QImage result(input.size(), QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&result);
  if (!painter.isActive()) return QImage();

  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.fillRect(result.rect(), Qt::transparent);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  painter.drawImage(0, 0, input);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.fillRect(result.rect(),
                   QColor(0, 0, 0, qBound(0, qRound(opacity * 255), 255)));

  return result;
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
// QPixmapCache

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

void addToPixmapCache(const QString &key, const QPixmap &pixmap) {
  if (!pixmap.isNull()) {
    QMutexLocker locker(&s_cacheMutex);
    QPixmapCache::insert(key, pixmap);
  } else {
    qWarning() << "Cannot add empty pixmap to QPixmapCache";
  }
}

//-----------------------------------------------------------------------------

QPixmap getFromPixmapCache(const QString &key) {
  QMutexLocker locker(&s_cacheMutex);
  QPixmap pm;
  if (QPixmapCache::find(key, &pm)) {
    return pm;
  } else {
    return QPixmap();
  }
}

//-----------------------------------------------------------------------------

void clearQPixmapCache() {
  QPixmapCache::clear();
  qDebug() << "Global QPixmapCache cleared";
}

//=============================================================================
// ThemeManager

QString getIconPath(const QString &iconSVGName) {
  ThemeManager &themeManager = ThemeManager::getInstance();
  return themeManager.getIconPath(iconSVGName);
}

//-----------------------------------------------------------------------------

ThemeManager::ThemeManager() {}

//-----------------------------------------------------------------------------

ThemeManager &ThemeManager::getInstance() {
  static ThemeManager instance;
  return instance;
}

//-----------------------------------------------------------------------------

void ThemeManager::initialize() { preloadIconMetadata(":/icons"); }

//-----------------------------------------------------------------------------

void ThemeManager::updateThemeProperties() {
  // Icon opacities
  setIconBaseOpacity(getCustomPropertyDouble("icon-base-opacity", 0.8));
  setIconDisabledOpacity(getCustomPropertyDouble("icon-disabled-opacity", 0.2));

  // Base icon colors
  setIconBaseColor(getCustomPropertyColor("icon-base-color"));
  setIconActiveColor(getCustomPropertyColor("icon-active-color"));
  setIconOnColor(getCustomPropertyColor("icon-on-color"));
  setIconSelectedColor(getCustomPropertyColor("icon-selected-color"));

  // Individual icon colors
  setIconCloseColor(getCustomPropertyColor("icon-close-color"));
  setIconPreviewColor(getCustomPropertyColor("icon-preview-color"));
  setIconLockColor(getCustomPropertyColor("icon-lock-color"));

  // Keyframe icon colors
  setIconKeyframeColor(getCustomPropertyColor("icon-keyframe-color"));
  setIconKeyframeModifiedColor(
      getCustomPropertyColor("icon-keyframe-modified-color"));

  // Viewer check icon colors
  setIconVCheckColor(getCustomPropertyColor("icon-vcheck-color"));

  // Invalidate cache to force re-rendering of icons
  clearQPixmapCache();
}

//-----------------------------------------------------------------------------

// Fetch icon SVG metadata
void ThemeManager::preloadIconMetadata(const QString &path) {
  qDebug() << "[ThemeManager:::preloadIconMetadata] STARTED with path:" << path;

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
    const QString fileName   = fileInfo.baseName();  // "name", "name_on", etc.

    QString iconName = fileName;
    QString state;

    // Check if the file matches a known state pattern
    QRegularExpressionMatch match = stateRegex.match(fileName);
    if (match.hasMatch()) {
      iconName = match.captured(1);  // Extract base icon name
      state    = match.captured(2);  // Extract state
    }

    // Store the icon path in m_iconPaths
    if (!m_iconPaths.contains(fileName)) {
      m_iconPaths.insert(fileName, iconPath);

      // Extract and store the icon size from dir path
      QSize iconSize = parseIconSize(iconPath);
      m_iconSizes.insert(iconName, iconSize);

      // Check if this is a menu icon (16x16)
      if (iconSize.width() == 16 && iconSize.height() == 16) {
        m_menuIcons.insert(iconName, true);
      }

      // Check if this is a colored icon
      if (parseIsColored(iconPath)) {
        m_coloredIcons.insert(iconName, true);
      }
    }
  }
}

//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------

QSize ThemeManager::parseIconSize(const QString &iconPath) const {
  // Match pattern like "16x16" in dir path
  static const QRegularExpression sizeRegex(R"(/(\d+)x(\d+)/)");

  QRegularExpressionMatch match = sizeRegex.match(iconPath);
  if (match.hasMatch()) {
    return QSize(match.captured(1).toInt(), match.captured(2).toInt());
  }

  qWarning() << "Could not parse size from path:" << iconPath;
  return QSize(16, 16);  // Fallback
}

//-----------------------------------------------------------------------------

bool ThemeManager::parseIsColored(const QString &iconPath) const {
  // Check if the path contains "colored"
  static const QRegularExpression coloredRegex(R"(/colored/)");

  return coloredRegex.match(iconPath).hasMatch();
}

//-----------------------------------------------------------------------------

bool ThemeManager::isColoredIcon(const QString &iconName) const {
  QString baseName = iconName;
  if (baseName.endsWith("_on") || baseName.endsWith("_over")) {
    baseName = baseName.left(baseName.lastIndexOf('_'));
  }
  return m_coloredIcons.contains(baseName);
}

//-----------------------------------------------------------------------------

// Parse custom properties from stylesheet to this format:
// :TOONZCOLORS { -custom-property: black; }
// Then get with: getCustomProperty(QColor("custom-property"));
void ThemeManager::parseCustomPropertiesFromStylesheet(
    const QString &styleSheet) {
  QRegularExpression rootRegex(R"(:TOONZCOLORS\s*\{([^}]*)\})");
  QRegularExpressionMatchIterator rootMatches =
      rootRegex.globalMatch(styleSheet);

  while (rootMatches.hasNext()) {
    QRegularExpressionMatch rootMatch = rootMatches.next();
    QString rootContent = rootMatch.captured(1);  // Extract block content

    QRegularExpression propertyRegex(R"(-([a-zA-Z0-9-]+):\s*([^;]+);)");
    QRegularExpressionMatchIterator propertyMatches =
        propertyRegex.globalMatch(rootContent);

    while (propertyMatches.hasNext()) {
      QRegularExpressionMatch propMatch = propertyMatches.next();
      QString propertyName              = propMatch.captured(1);
      QString propertyValue             = propMatch.captured(2).trimmed();

      // Set or override the property
      setCustomProperty(propertyName, propertyValue);
    }
  }

  updateThemeProperties();
}

//-----------------------------------------------------------------------------

void ThemeManager::setCustomProperty(const QString &name,
                                     const QString &value) {
  m_customProperties[name] = value;
}

//-----------------------------------------------------------------------------

QString ThemeManager::getCustomProperty(const QString &name,
                                        const QString &defaultValue) const {
  return m_customProperties.value(name, QString());
}

//-----------------------------------------------------------------------------

double ThemeManager::getCustomPropertyDouble(const QString &name,
                                             double defaultValue) const {
  QString value = getCustomProperty(name);
  bool ok       = false;
  double result = value.toDouble(&ok);
  return ok ? result : defaultValue;
}

//-----------------------------------------------------------------------------

QColor ThemeManager::getCustomPropertyColor(const QString &name,
                                            const QColor &defaultValue) const {
  QString value = getCustomProperty(name);
  if (value.isEmpty()) return defaultValue;

  // Try to parse the color from the string
  QColor color(value);
  return color.isValid() ? color : defaultValue;
}

//-----------------------------------------------------------------------------

QSize ThemeManager::getIconSize(const QString &iconName) const {
  QString baseName = iconName;
  if (baseName.endsWith("_on") || baseName.endsWith("_over")) {
    baseName = baseName.left(baseName.lastIndexOf('_'));
  }
  return m_iconSizes.value(baseName, QSize());
}

//-----------------------------------------------------------------------------

bool ThemeManager::hasIcon(const QString &iconName) const {
  return m_iconPaths.contains(iconName);
}

//-----------------------------------------------------------------------------

bool ThemeManager::isMenuIcon(const QString &iconName) const {
  QString baseName = iconName;
  if (baseName.endsWith("_on") || baseName.endsWith("_over")) {
    baseName = baseName.left(baseName.lastIndexOf('_'));
  }
  return m_menuIcons.contains(baseName);
}

//=============================================================================
// SvgIconEngine

//-----------------------------------------------------------------------------

// Regular icon
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

//-----------------------------------------------------------------------------

// For temporary icon with command name only
SvgIconEngine::SvgIconEngine(const QString &commandName, const QImage image)
    : SvgIconEngine("") {
  m_iconName               = commandName;
  m_isTemporaryCommandIcon = true;
  m_image                  = image;
}

//-----------------------------------------------------------------------------

QIconEngine *SvgIconEngine::clone() const {
  if (!m_iconName.isEmpty()) {  // Regular icon
    return new SvgIconEngine(m_iconName, m_isForMenuItem, m_dpr, m_iconSize);
  } else {  // For temporary command icon images
    return new SvgIconEngine(m_iconName, m_image);
  }
}

//-----------------------------------------------------------------------------

// Determine icon theme color (or individual color) for mode/state
QColor SvgIconEngine::getThemeColor(const QString &iconName, QIcon::Mode mode,
                                    QIcon::State state) {
  ThemeManager &tm = ThemeManager::getInstance();

  // Check for unique first
  QColor color = getUniqueIconColor(iconName, mode, state);
  if (color.isValid()) return color;

  // Regular
  if (state == QIcon::On) return tm.getIconOnColor();
  if (mode == QIcon::Active) return tm.getIconActiveColor();
  if (mode == QIcon::Selected) return tm.getIconSelectedColor();

  return tm.getIconBaseColor();
}

//-----------------------------------------------------------------------------

// Set colors to specific icons
QColor SvgIconEngine::getUniqueIconColor(const QString &iconName,
                                         QIcon::Mode mode, QIcon::State state) {
  ThemeManager &tm = ThemeManager::getInstance();

  // Rollover mode
  if (mode == QIcon::Active) {
    if (iconName.contains("closewindow")) return tm.getIconCloseColor();
  }

  // On state
  if (state == QIcon::On) {
    if (iconName == "preview") return tm.getIconPreviewColor();
    if (iconName == "subpreview") return tm.getIconPreviewColor();

    if (iconName.contains("freeze")) return tm.getIconLockColor();
    if (iconName.contains("lock")) return tm.getIconLockColor();

    if (iconName.contains("keyframe_partial")) return tm.getIconKeyframeColor();
    if (iconName.contains("keyframe_modified"))
      return tm.getIconKeyframeModifiedColor();
    if (iconName.contains("keyframe")) return tm.getIconKeyframeColor();

    if (iconName.contains("transparency_check")) return tm.getIconVCheckColor();
    if (iconName.contains("paint_check")) return tm.getIconVCheckColor();
    if (iconName.contains("opacity_check")) return tm.getIconVCheckColor();
    if (iconName.contains("ink_check")) return tm.getIconVCheckColor();
    if (iconName.contains("ink_no1_check")) return tm.getIconVCheckColor();
    if (iconName.contains("gap_check")) return tm.getIconVCheckColor();
    if (iconName.contains("fill_check")) return tm.getIconVCheckColor();
    if (iconName.contains("blackbg_check")) return tm.getIconVCheckColor();
    if (iconName.contains("inks_only")) return tm.getIconVCheckColor();
  }

  return QColor();
}

//-----------------------------------------------------------------------------

void SvgIconEngine::paint(QPainter *painter, const QRect &rect,
                          QIcon::Mode mode, QIcon::State state) {
  if (!painter || rect.isEmpty()) return;

  ThemeManager &tm = ThemeManager::getInstance();

  qreal dpr = painter->device()->devicePixelRatio();

  QPixmap pm = pixmap(rect.size() * dpr, mode, state);
  if (!pm.isNull()) {
    //! Don't set SmoothPixmapTransform otherwise at 1x pixmaps will look blurry

    pm.setDevicePixelRatio(dpr);

    // Calculate the target rect to maintain aspect ratio
    QSize pmSize     = pm.size() / dpr;  // Logical size of the pixmap
    QSize scaledSize = pmSize.scaled(rect.size(), Qt::KeepAspectRatio);

    // Center scaled pixmap within rect
    int x = rect.x() + (rect.width() - scaledSize.width()) / 2;
    int y = rect.y() + (rect.height() - scaledSize.height()) / 2;
    QRect targetRect(QPoint(x, y), scaledSize);

    painter->drawPixmap(targetRect, pm);
  }
}

//-----------------------------------------------------------------------------

qreal SvgIconEngine::getOpacityForModeState(QIcon::Mode mode,
                                            QIcon::State state) {
  auto &tm = ThemeManager::getInstance();

  const double baseOpacity     = tm.getIconBaseOpacity();
  const double disabledOpacity = tm.getIconDisabledOpacity();
  const double activeOpacity   = 1.0;

  qreal opacity;
  if (m_isColored) {
    opacity = (mode == QIcon::Disabled) ? disabledOpacity : activeOpacity;
  } else {
    opacity = (mode == QIcon::Disabled)                        ? disabledOpacity
              : (mode == QIcon::Normal && state == QIcon::Off) ? baseOpacity
                                                               : activeOpacity;
  }

  return opacity;
}

//-----------------------------------------------------------------------------

// Load SVG themed for mode/state
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

//-----------------------------------------------------------------------------

QPixmap SvgIconEngine::pixmap(const QSize &size, QIcon::Mode mode,
                              QIcon::State state) {
  qreal dpr         = m_dpr;
  QSize requestSize = !m_iconSize.isEmpty() ? (m_iconSize * m_dpr) : size;

  // Early out for hidden menu icons
  bool hideIcon = shouldHideIcon(m_isForMenuItem);
  if (hideIcon && size == QSize(16 * dpr, 16 * dpr)) {
    static QPixmap emptyPm(size);
    emptyPm.fill(Qt::transparent);
    return emptyPm;
  }

  // Generate cache key
  QString cacheKey = generateCacheKey(m_iconName, requestSize, mode, state);
  if (m_isTemporaryCommandIcon) cacheKey += "_cmdNameIcon";
  // Check cache for pixmap
  QPixmap cachedPm = getFromPixmapCache(cacheKey);
  if (!cachedPm.isNull()) return cachedPm;

  // --------------------------------------------------------------------------
  // NOT FOUND: RENDER NEW PIXMAPS
  // --------------------------------------------------------------------------

  ThemeManager &tm = ThemeManager::getInstance();

  // Determine icon theme color
  QColor color =
      m_isColored ? QColor() : getThemeColor(m_iconName, mode, state);

  // Get base icon size
  QSize baseIconLogicalSize  = tm.getIconSize(m_iconName);
  QSize baseIconPhysicalSize = baseIconLogicalSize * dpr;
  // Menu and Toolbar size
  QSize menuIconPhysicalSize(16 * dpr, 16 * dpr);
  QSize toolbarIconPhysicalSize = getBestToolbarSizeByDpr(size);

  QPixmap renderedPm;

  //---------- Temporary command icons by name abbreviation
  if (m_isTemporaryCommandIcon && size != menuIconPhysicalSize) {
    QImage adjusted = adjustImageOpacity(colorizeBlackPixels(m_image, color),
                                         getOpacityForModeState(mode, state));
    // Scale to physical size
    renderedPm = QPixmap::fromImage(adjusted).scaled(
        requestSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  //---------- Menu command icons (toolbar-sized request but is a menu icon)
  else if (requestSize == toolbarIconPhysicalSize && m_isMenuIcon) {
    QPixmap loadedPm =
        loadPixmap(m_iconName, mode, state, baseIconPhysicalSize, color)
            .scaled(toolbarIconPhysicalSize * 0.8, Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
    loadedPm.setDevicePixelRatio(dpr);
    renderedPm = expandPixmap(loadedPm, requestSize);
  }

  //---------- Regular icons
  else {
    // Maintain aspect ratio
    QSize adjustedSize =
        baseIconPhysicalSize.scaled(requestSize, Qt::KeepAspectRatio);

    QPixmap loadedPm = loadPixmap(m_iconName, mode, state, adjustedSize, color);
    loadedPm.setDevicePixelRatio(dpr);
    renderedPm = expandPixmap(loadedPm.scaled(requestSize, Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation),
                              requestSize);
  }

  renderedPm.setDevicePixelRatio(dpr);
  addToPixmapCache(cacheKey, renderedPm);

  return getFromPixmapCache(cacheKey);
}

//-----------------------------------------------------------------------------

// Determine if icon should be hidden in menus
bool SvgIconEngine::shouldHideIcon(bool isForMenuItem) {
  //! This is a workaround for a Qt bug, this can be removed and setVisible used
  //! instead when Qt is updated
#ifdef _WIN32
  if (isForMenuItem) {
    return !Preferences::instance()->getBoolValue(showIconsInMenu);
  }
  return false;
#else
  return false;
#endif
}

//-----------------------------------------------------------------------------

// Compares requestedSize to toolbar icon size checking for the best match
// scaling by each unique DPR on the system.
QSize SvgIconEngine::getBestToolbarSizeByDpr(const QSize &requestedSize) {
  //! This function should be removed when SvgIconEngine gets widget DPR

  const auto screens = QGuiApplication::screens();
  if (screens.isEmpty()) return QSize(20, 20);

  // Lambda to compute the difference between two sizes
  auto diff = [&requestedSize](const QSize &size) {
    return std::abs(size.width() - requestedSize.width()) +
           std::abs(size.height() - requestedSize.height());
  };

  // Initialize with the first screen's scaled size
  QSize bestSize =
      (QSizeF(20, 20) * screens.first()->devicePixelRatio()).toSize();
  int bestDiff = diff(bestSize);

  // Iterate through all screens to find the size with the smallest difference
  for (const QScreen *screen : screens) {
    QSize size      = (QSizeF(20, 20) * screen->devicePixelRatio()).toSize();
    int currentDiff = diff(size);
    if (currentDiff < bestDiff) {
      bestSize = size;
      bestDiff = currentDiff;
    }
  }

  return bestSize;
}
