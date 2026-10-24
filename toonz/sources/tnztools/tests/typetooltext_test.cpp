#include "typetooltext.h"

#include <QGuiApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace TypeToolText;

namespace {
void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

TypeToolTextEntry markdown(const QString &source) {
  TypeToolTextEntry entry;
  entry.source = source;
  entry.format = TypeToolTextFormat::Markdown;
  entry.font   = {QStringLiteral("DejaVu Sans"), QStringLiteral("Book"),
                  QStringLiteral("24")};
  return entry;
}
}  // namespace

int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  TypeToolTextEntry entry       = markdown(QStringLiteral(
            "# Title\n\nA **bold** and *italic* paragraph.\n\n- First\n- Second"));
  const OutlineLayout formatted = buildOutlineLayout(entry);
  require(formatted.supported && !formatted.paths.isEmpty(),
          "Markdown produced no outlines");
  require(formatted.plainText.contains(
              QStringLiteral("A bold and italic paragraph.")) &&
              !formatted.plainText.contains(QStringLiteral("#")) &&
              !formatted.plainText.contains(QStringLiteral("**")),
          "Markdown markup leaked into formatted text");
  require(formatted.plainText.contains(QStringLiteral("First")) &&
              formatted.plainText.contains(QStringLiteral("Second")),
          "List contents were lost");

  const OutlineLayout list =
      buildOutlineLayout(markdown(QStringLiteral("- First\n- Second")));
  require(list.paths.size() >= 4, "List markers were omitted from the artwork");

  entry                       = markdown(QStringLiteral("# Title"));
  const OutlineLayout heading = buildOutlineLayout(entry);
  entry.format                = TypeToolTextFormat::PlainText;
  entry.source                = QStringLiteral("Title");
  const OutlineLayout body    = buildOutlineLayout(entry);
  require(heading.bounds.height() > body.bounds.height() * 1.3,
          "Heading size was not applied");
  entry.font.size = QStringLiteral("48");
  require(buildOutlineLayout(entry).bounds.width() > body.bounds.width() * 1.7,
          "Changing the font size did not update the outlines");

  entry = markdown(QStringLiteral("first line\nsecond line"));
  const OutlineLayout hardBreak  = buildOutlineLayout(entry);
  entry.preserveSourceLineBreaks = false;
  const OutlineLayout softBreak  = buildOutlineLayout(entry);
  require(hardBreak.bounds.height() > softBreak.bounds.height() * 1.5,
          "The Markdown line-break option has no effect");

  entry = markdown(QStringLiteral("```\n**literal**\nnext\n```"));
  const QString preservedCode    = buildOutlineLayout(entry).plainText;
  entry.preserveSourceLineBreaks = false;
  require(preservedCode == buildOutlineLayout(entry).plainText &&
              preservedCode.contains(QStringLiteral("**literal**\nnext")),
          "Preserving line breaks changed fenced code contents");

  entry = markdown(QString::fromUtf8("**Caf\xc3\xa9** and \xce\xa9"));
  const OutlineLayout horizontal = buildOutlineLayout(entry);
  const OutlineLayout vertical   = buildOutlineLayout(entry, true);
  require(
      std::abs(horizontal.bounds.width() - vertical.bounds.height()) < 0.01 &&
          std::abs(horizontal.bounds.height() - vertical.bounds.width()) < 0.01,
      "Vertical preview and placement do not share the same rotation");

  QRect bounds;
  const QImage image = rasterizeOutlineLayout(formatted, bounds);
  require(!image.isNull() && image.size() == bounds.size(),
          "Formatted text could not be rasterized");
  int inkPixels = 0;
  for (int y = 0; y < image.height(); ++y)
    for (int x = 0; x < image.width(); ++x)
      if (image.constScanLine(y)[x] < 255) ++inkPixels;
  require(inkPixels > 0 && inkPixels < image.width() * image.height() / 2,
          "Rasterization lost the glyphs or filled the document background");

  OutlineLayout oversized;
  QPainterPath path;
  path.addRect(0, 0, 40000, 100);
  oversized.paths.append(path);
  oversized.bounds = path.boundingRect();
  require(rasterizeOutlineLayout(oversized, bounds).isNull(),
          "An oversized raster allocation was accepted");

  QTemporaryDir directory;
  require(directory.isValid(), "Could not create a temporary test directory");
  const QString source =
      QString::fromUtf8("# Caf\xc3\xa9\n**Bold** and \xce\xa9\n");
  const QString fileName = directory.filePath(QStringLiteral("source.md"));
  QString error;
  require(saveTextSource(fileName, source, error), "Text source export failed");
  QFile saved(fileName);
  require(saved.open(QIODevice::ReadOnly) && saved.readAll() == source.toUtf8(),
          "Text export changed Markdown, Unicode, or line breaks");
  require(
      !saveTextSource(directory.filePath(QStringLiteral("missing/source.md")),
                      source, error) &&
          !error.isEmpty(),
      "A failed text export was reported as successful");

  TypeToolTextEntry first                 = markdown(source);
  first.font.style                        = QStringLiteral("Bold Italic");
  first.preserveSourceLineBreaks          = false;
  TypeToolTextEntry second                = first;
  second.format                           = TypeToolTextFormat::PlainText;
  second.font.size                        = QStringLiteral("86");
  const QList<TypeToolTextEntry> original = {first, second};
  QList<TypeToolTextEntry> loaded;
  const QByteArray archive = serializeHistory(original);
  require(
      deserializeHistory(archive, loaded, error) && loaded.size() == 2 &&
          sameTextEntry(loaded[0], first) && sameTextEntry(loaded[1], second),
      "History round trip changed source, formatting, order, or font settings");

  const QList<TypeToolTextEntry> merged = mergeHistory({first}, original);
  require(merged.size() == 2 && sameTextEntry(merged[1], second),
          "History merge lost a different appearance or duplicated an entry");
  QList<TypeToolTextEntry> fullHistory;
  for (int i = 0; i < cTextHistoryLimit; ++i) {
    TypeToolTextEntry item = first;
    item.source            = QStringLiteral("Existing %1").arg(i);
    fullHistory.append(item);
  }
  const QList<TypeToolTextEntry> capped = mergeHistory(original, fullHistory);
  require(
      capped.size() == cTextHistoryLimit && sameTextEntry(capped[0], first) &&
          sameTextEntry(capped[1], second) &&
          sameTextEntry(capped.last(), fullHistory[cTextHistoryLimit - 3]),
      "History merge changed loaded order or exceeded the recent-entry limit");

  QJsonObject invalid = QJsonDocument::fromJson(archive).object();
  invalid.insert(QStringLiteral("version"), 2);
  loaded = original;
  require(!deserializeHistory(QJsonDocument(invalid).toJson(), loaded, error) &&
              !error.isEmpty() && sameTextEntry(loaded[0], first),
          "An unsupported history version modified existing entries");
  invalid = QJsonDocument::fromJson(archive).object();
  QJsonArray invalidEntries =
      invalid.value(QStringLiteral("entries")).toArray();
  invalidEntries.append(QJsonObject());
  invalid.insert(QStringLiteral("entries"), invalidEntries);
  require(
      !deserializeHistory(QJsonDocument(invalid).toJson(), loaded, error) &&
          loaded.size() == original.size() && sameTextEntry(loaded[1], second),
      "Partially valid history was applied before all entries were validated");
  QByteArray invalidUtf8 = archive;
  invalidUtf8.insert(invalidUtf8.indexOf("Caf") + 3, char(0xff));
  require(!deserializeHistory(invalidUtf8, loaded, error),
          "History accepted invalid UTF-8 data");
  fullHistory.append(first);
  require(!deserializeHistory(serializeHistory(fullHistory), loaded, error),
          "An oversized history was silently truncated");
  require(deserializeHistory(serializeHistory({}), loaded, error) &&
              loaded.isEmpty(),
          "An empty history could not be restored");

  std::cout << "Type Tool text, export, and history tests passed" << std::endl;
  return 0;
}
