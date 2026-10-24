#include "typetooltext.h"

#include <QAbstractTextDocumentLayout>
#include <QFontDatabase>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextCodec>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPathStroker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <cmath>

namespace TypeToolText {
namespace {

class OutlinePaintEngine final : public QPaintEngine {
public:
  QList<QPainterPath> paths;

  OutlinePaintEngine() : QPaintEngine(QPaintEngine::AllFeatures) {}
  bool begin(QPaintDevice *) override {
    setActive(true);
    return true;
  }
  bool end() override {
    setActive(false);
    return true;
  }
  Type type() const override { return User; }
  void updateState(const QPaintEngineState &) override {}

  void drawPath(const QPainterPath &path) override {
    const QBrush brush         = painter()->brush();
    const QPen pen             = painter()->pen();
    const QTransform transform = painter()->worldTransform();
    if (brush.style() != Qt::NoBrush && brush.color() == QColor(Qt::black))
      paths.append(transform.map(path));
    if (pen.style() != Qt::NoPen && pen.color() == QColor(Qt::black)) {
      QPainterPathStroker stroker;
      stroker.setWidth(pen.widthF() > 0 ? pen.widthF() : 1);
      stroker.setCapStyle(pen.capStyle());
      stroker.setJoinStyle(pen.joinStyle());
      if (pen.style() != Qt::SolidLine)
        stroker.setDashPattern(pen.dashPattern());
      paths.append(transform.map(stroker.createStroke(path)));
    }
  }

  void drawPolygon(const QPointF *points, int count,
                   PolygonDrawMode mode) override {
    if (count < 1) return;
    QPainterPath path(points[0]);
    for (int i = 1; i < count; ++i) path.lineTo(points[i]);
    if (mode != PolylineMode) path.closeSubpath();
    path.setFillRule(mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
    drawPath(path);
  }

  // QTextDocument's default text painting becomes glyph outlines through
  // QPaintEngine::drawTextItem, including shaping and per-run font styles.
  void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override {}
};

class OutlinePaintDevice final : public QPaintDevice {
  mutable OutlinePaintEngine m_engine;

public:
  QPaintEngine *paintEngine() const override { return &m_engine; }
  const QList<QPainterPath> &paths() const { return m_engine.paths; }

protected:
  int metric(PaintDeviceMetric metric) const override {
    switch (metric) {
    case PdmWidth:
    case PdmHeight:
      return 32768;
    case PdmWidthMM:
    case PdmHeightMM:
      return 8670;
    case PdmDpiX:
    case PdmDpiY:
    case PdmPhysicalDpiX:
    case PdmPhysicalDpiY:
      return 96;
    case PdmDepth:
      return 32;
    case PdmNumColors:
      return 0;
    case PdmDevicePixelRatio:
      return 1;
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    case PdmDevicePixelRatioScaled:
      return QPaintDevice::devicePixelRatioFScale();
#endif
    }
    return 0;
  }
};

class TextDocument final : public QTextDocument {
protected:
  // Text placement never embeds image resources from imported documents.
  QVariant loadResource(int, const QUrl &) override { return QVariant(); }
};

QString preserveMarkdownLineBreaks(const QString &source) {
  QStringList lines = source.split('\n');
  QChar fence;
  int fenceLength = 0;
  const QRegularExpression fencePattern(QStringLiteral("^(`{3,}|~{3,})"));
  for (int i = 0; i + 1 < lines.size(); ++i) {
    const QString trimmed               = lines[i].trimmed();
    const QRegularExpressionMatch match = fencePattern.match(trimmed);
    if (match.hasMatch()) {
      const QString marker = match.captured(1);
      if (fence.isNull()) {
        fence       = marker[0];
        fenceLength = marker.size();
      } else if (marker[0] == fence && marker.size() >= fenceLength &&
                 trimmed.mid(marker.size()).trimmed().isEmpty()) {
        fence = QChar();
      }
      continue;
    }
    if (!fence.isNull() || trimmed.isEmpty() ||
        lines[i].startsWith(QStringLiteral("    ")) ||
        lines[i].startsWith('\t') || lines[i].endsWith(QStringLiteral("  ")) ||
        trimmed.endsWith('\\') || lines[i + 1].trimmed().isEmpty())
      continue;
    lines[i] += QStringLiteral("  ");
  }
  return lines.join('\n');
}

}  // namespace

QString textFormatName(TypeToolTextFormat format) {
  return format == TypeToolTextFormat::Markdown ? QStringLiteral("Markdown")
                                                : QStringLiteral("PlainText");
}

TypeToolTextFormat textFormatFromName(const QString &name) {
  return name.compare(QStringLiteral("Markdown"), Qt::CaseInsensitive) == 0
             ? TypeToolTextFormat::Markdown
             : TypeToolTextFormat::PlainText;
}

QString normalizedText(QString text) {
  text.replace("\r\n", "\n");
  text.replace('\r', '\n');
  return text;
}

bool sameTextEntry(const TypeToolTextEntry &first,
                   const TypeToolTextEntry &second) {
  return first.source == second.source && first.format == second.format &&
         first.preserveSourceLineBreaks == second.preserveSourceLineBreaks &&
         first.font.family == second.font.family &&
         first.font.style == second.font.style &&
         first.font.size == second.font.size;
}

OutlineLayout buildOutlineLayout(const TypeToolTextEntry &entry,
                                 bool vertical) {
  OutlineLayout result;
  TextDocument document;
  bool validSize = false;
  int size       = entry.font.size.toInt(&validSize);
  if (!validSize || size < 1 || size > 1000) size = 70;
  const QFont font =
      QFontDatabase().font(entry.font.family, entry.font.style, size);
  document.setDefaultFont(font);
  document.setDocumentMargin(0);
  OutlinePaintDevice device;
  document.documentLayout()->setPaintDevice(&device);
  document.setUseDesignMetrics(true);
  if (entry.format == TypeToolTextFormat::Markdown) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QString source = entry.preserveSourceLineBreaks
                               ? preserveMarkdownLineBreaks(entry.source)
                               : entry.source;
    document.setMarkdown(source, QTextDocument::MarkdownFeatures(
                                     QTextDocument::MarkdownDialectGitHub) |
                                     QTextDocument::MarkdownNoHTML);
#else
    result.supported = false;
    return result;
#endif
  } else {
    document.setPlainText(entry.source);
  }

  // All text uses the selected palette style. Preserve font/paragraph
  // formatting, but omit document backgrounds and link-specific colors.
  QTextCursor cursor(&document);
  cursor.select(QTextCursor::Document);
  QTextCharFormat characterFormat;
  characterFormat.setForeground(Qt::black);
  characterFormat.setBackground(Qt::NoBrush);
  cursor.mergeCharFormat(characterFormat);
  QTextBlockFormat blockFormat;
  blockFormat.setBackground(Qt::NoBrush);
  cursor.mergeBlockFormat(blockFormat);
  document.setTextWidth(-1);
  document.size();  // Lay out before reading the first line's baseline.
  const QTextBlock firstBlock    = document.begin();
  const QTextLayout *firstLayout = firstBlock.layout();
  qreal baseline                 = 0;
  if (firstLayout && firstLayout->lineCount()) {
    const QTextLine line = firstLayout->lineAt(0);
    baseline = document.documentLayout()->blockBoundingRect(firstBlock).top() +
               line.y() + line.ascent();
  }

  QPainter painter(&device);
  painter.setRenderHint(QPainter::TextAntialiasing);
  document.drawContents(&painter);
  painter.end();
  QTransform transform;
  if (vertical) transform.rotate(90);
  transform.translate(0, -baseline);
  for (const QPainterPath &path : device.paths()) {
    if (path.isEmpty()) continue;
    const QPainterPath outline = transform.map(path);
    result.paths.append(outline);
    result.bounds = result.bounds.united(outline.boundingRect());
  }
  result.plainText = document.toPlainText();
  return result;
}

QImage rasterizeOutlineLayout(const OutlineLayout &layout, QRect &bounds) {
  bounds = QRect();
  if (layout.paths.isEmpty()) return QImage();
  const QRectF box = layout.bounds.adjusted(-1, -1, 1, 1);
  if (!std::isfinite(box.width()) || !std::isfinite(box.height()) ||
      box.width() > 32768 || box.height() > 32768 ||
      std::abs(box.left()) > 100000000 || std::abs(box.top()) > 100000000)
    return QImage();
  const QRect rasterBounds = box.toAlignedRect();
  if (qint64(rasterBounds.width()) * rasterBounds.height() > 16 * 1024 * 1024)
    return QImage();
  QImage image(rasterBounds.size(), QImage::Format_Grayscale8);
  if (image.isNull()) return image;
  image.fill(255);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  painter.translate(-rasterBounds.left(), -rasterBounds.top());
  for (const QPainterPath &path : layout.paths) painter.drawPath(path);
  painter.end();
  bounds = rasterBounds;
  return image;
}

bool saveTextSource(const QString &fileName, const QString &source,
                    QString &error) {
  QSaveFile file(fileName);
  const QByteArray bytes = source.toUtf8();
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit()) {
    error = file.errorString();
    return false;
  }
  error.clear();
  return true;
}

QByteArray serializeHistory(const QList<TypeToolTextEntry> &entries) {
  QJsonArray array;
  for (const TypeToolTextEntry &entry : entries) {
    QJsonObject font;
    font.insert(QStringLiteral("family"), entry.font.family);
    font.insert(QStringLiteral("style"), entry.font.style);
    font.insert(QStringLiteral("size"), entry.font.size);
    QJsonObject item;
    item.insert(QStringLiteral("source"), entry.source);
    item.insert(QStringLiteral("format"), textFormatName(entry.format));
    item.insert(QStringLiteral("preserveLineBreaks"),
                entry.preserveSourceLineBreaks);
    item.insert(QStringLiteral("font"), font);
    array.append(item);
  }
  QJsonObject object;
  object.insert(QStringLiteral("format"),
                QStringLiteral("OpenToonzTypeToolHistory"));
  object.insert(QStringLiteral("version"), 1);
  object.insert(QStringLiteral("entries"), array);
  return QJsonDocument(object).toJson(QJsonDocument::Indented);
}

bool deserializeHistory(const QByteArray &bytes,
                        QList<TypeToolTextEntry> &entries, QString &error) {
  auto invalid = [&error](const char *message) {
    error = QCoreApplication::translate("TypeToolTextHistoryPopup", message);
    return false;
  };
  QTextCodec::ConverterState state;
  QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(),
                                               &state);
  if (state.invalidChars || state.remainingChars)
    return invalid(
        QT_TRANSLATE_NOOP("TypeToolTextHistoryPopup",
                          "The history file contains invalid UTF-8 text."));
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject())
    return invalid(
        QT_TRANSLATE_NOOP("TypeToolTextHistoryPopup",
                          "The history file is not a valid JSON object."));
  const QJsonObject object = document.object();
  if (object.value(QStringLiteral("format")).toString() !=
          QStringLiteral("OpenToonzTypeToolHistory") ||
      !object.value(QStringLiteral("version")).isDouble() ||
      object.value(QStringLiteral("version")).toDouble() != 1 ||
      !object.value(QStringLiteral("entries")).isArray())
    return invalid(
        QT_TRANSLATE_NOOP("TypeToolTextHistoryPopup",
                          "The file is not a supported Type Tool history."));
  const QJsonArray array = object.value(QStringLiteral("entries")).toArray();
  if (array.size() > cTextHistoryLimit)
    return invalid(QT_TRANSLATE_NOOP(
        "TypeToolTextHistoryPopup",
        "The history file exceeds the Recent Text entry limit."));
  QList<TypeToolTextEntry> loaded;
  for (const QJsonValue &value : array) {
    if (!value.isObject())
      return invalid(
          QT_TRANSLATE_NOOP("TypeToolTextHistoryPopup",
                            "The history file contains an invalid entry."));
    const QJsonObject item = value.toObject();
    const QJsonObject font = item.value(QStringLiteral("font")).toObject();
    const QString format   = item.value(QStringLiteral("format")).toString();
    if (!item.value(QStringLiteral("source")).isString() ||
        item.value(QStringLiteral("source")).toString().isEmpty() ||
        (format != QStringLiteral("PlainText") &&
         format != QStringLiteral("Markdown")) ||
        !item.value(QStringLiteral("preserveLineBreaks")).isBool() ||
        !font.value(QStringLiteral("family")).isString() ||
        !font.value(QStringLiteral("style")).isString() ||
        !font.value(QStringLiteral("size")).isString())
      return invalid(
          QT_TRANSLATE_NOOP("TypeToolTextHistoryPopup",
                            "The history file contains an invalid entry."));
    TypeToolTextEntry entry;
    entry.source =
        normalizedText(item.value(QStringLiteral("source")).toString());
    entry.format = textFormatFromName(format);
    entry.preserveSourceLineBreaks =
        item.value(QStringLiteral("preserveLineBreaks")).toBool();
    entry.font.family = font.value(QStringLiteral("family")).toString();
    entry.font.style  = font.value(QStringLiteral("style")).toString();
    entry.font.size   = font.value(QStringLiteral("size")).toString();
    bool validSize    = false;
    const int size    = entry.font.size.toInt(&validSize);
    if (!entry.font.size.isEmpty() && (!validSize || size < 1 || size > 1000))
      return invalid(
          QT_TRANSLATE_NOOP("TypeToolTextHistoryPopup",
                            "The history file contains an invalid font size."));
    loaded.append(entry);
  }
  // Publish only after every entry has passed validation.
  entries = mergeHistory(loaded, QList<TypeToolTextEntry>());
  error.clear();
  return true;
}

QList<TypeToolTextEntry> mergeHistory(const QList<TypeToolTextEntry> &loaded,
                                      const QList<TypeToolTextEntry> &current) {
  QList<TypeToolTextEntry> result;
  const QList<TypeToolTextEntry> candidates = loaded + current;
  for (const TypeToolTextEntry &entry : candidates) {
    bool duplicate = false;
    for (const TypeToolTextEntry &saved : result) {
      if (sameTextEntry(entry, saved)) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) result.append(entry);
    if (result.size() == cTextHistoryLimit) break;
  }
  return result;
}

}  // namespace TypeToolText
