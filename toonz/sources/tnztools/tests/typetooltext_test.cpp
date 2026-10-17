#include "typetooltext.h"

#include <QGuiApplication>
#include <QFile>
#include <QTemporaryDir>

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

  std::cout << "Type Tool formatted text tests passed" << std::endl;
  return 0;
}
