#pragma once

#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QString>

namespace TypeToolText {

constexpr int cTextHistoryLimit = 20;

enum class TypeToolTextFormat { PlainText, Markdown };

struct TypeToolFontSettings final {
  QString family;
  QString style;
  QString size;
};

struct TypeToolTextEntry final {
  QString source;
  TypeToolTextFormat format     = TypeToolTextFormat::PlainText;
  bool preserveSourceLineBreaks = true;
  TypeToolFontSettings font;
};

QString textFormatName(TypeToolTextFormat format);
TypeToolTextFormat textFormatFromName(const QString &name);
QString normalizedText(QString text);
bool sameTextEntry(const TypeToolTextEntry &first,
                   const TypeToolTextEntry &second);

// Baseline-relative outlines in Qt coordinates (positive Y points down).
// The preview and both drawing targets consume these same paths.
struct OutlineLayout final {
  QList<QPainterPath> paths;
  QRectF bounds;
  QString plainText;
  bool supported = true;
};

OutlineLayout buildOutlineLayout(const TypeToolTextEntry &entry,
                                 bool vertical = false);
QImage rasterizeOutlineLayout(const OutlineLayout &layout, QRect &bounds);
bool saveTextSource(const QString &fileName, const QString &source,
                    QString &error);
QByteArray serializeHistory(const QList<TypeToolTextEntry> &entries);
bool deserializeHistory(const QByteArray &bytes,
                        QList<TypeToolTextEntry> &entries, QString &error);
QList<TypeToolTextEntry> mergeHistory(const QList<TypeToolTextEntry> &loaded,
                                      const QList<TypeToolTextEntry> &current);

}  // namespace TypeToolText
