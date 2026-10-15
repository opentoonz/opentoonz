#include "toonzqt/fontstyletranslation.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QHash>
#include <QSignalBlocker>

namespace {

const char *const fontStyles[] = {
    QT_TRANSLATE_NOOP("FontStyleNames", "Regular"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Normal"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Book"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Roman"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Thin"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Extra Light"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Light"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Medium"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Semibold"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Demi Bold"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Bold"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Extra Bold"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Black"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Heavy"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Oblique"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Bold Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Bold Oblique"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Light Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Medium Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Semibold Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Extra Bold Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Black Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Condensed"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Condensed Bold"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Condensed Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Condensed Bold Italic"),
    QT_TRANSLATE_NOOP("FontStyleNames", "Expanded")};

QString normalizedStyle(const QString &style) {
  return style.simplified().toCaseFolded();
}

const QHash<QString, const char *> &fontStyleLookup() {
  static const QHash<QString, const char *> lookup = [] {
    QHash<QString, const char *> result;
    for (const char *style : fontStyles)
      result.insert(normalizedStyle(QString::fromLatin1(style)), style);
    result.insert(QStringLiteral("semi bold"), "Semibold");
    result.insert(QStringLiteral("demibold"), "Demi Bold");
    result.insert(QStringLiteral("extralight"), "Extra Light");
    result.insert(QStringLiteral("ultra light"), "Extra Light");
    result.insert(QStringLiteral("extrabold"), "Extra Bold");
    result.insert(QStringLiteral("ultra bold"), "Extra Bold");
    result.insert(QStringLiteral("semi bold italic"), "Semibold Italic");
    return result;
  }();
  return lookup;
}

}  // namespace

QString DVGui::fontStyleDisplayName(const QString &style) {
  const char *source = fontStyleLookup().value(normalizedStyle(style), nullptr);
  if (!source) return style;
  QString label = QCoreApplication::translate("FontStyleNames", source);
  // Without a translation, keep even an alias exactly as the font supplied it.
  return label == QString::fromLatin1(source) ? style : label;
}

QStringList DVGui::fontStyleDisplayNames(const QStringList &styles) {
  QStringList labels;
  QHash<QString, int> counts;
  for (const QString &style : styles) {
    QString label = fontStyleDisplayName(style);
    labels.append(label);
    ++counts[label];
  }
  for (int i = 0; i < labels.size(); ++i) {
    if (counts.value(labels[i]) > 1)
      labels[i] = QStringLiteral("%1 (%2)").arg(labels[i], styles[i]);
  }
  return labels;
}

void DVGui::populateFontStyleCombo(QComboBox *combo, const QStringList &styles,
                                   const QString &currentStyle) {
  QSignalBlocker blocker(combo);
  const QStringList labels = fontStyleDisplayNames(styles);
  combo->clear();
  for (int i = 0; i < styles.size(); ++i) {
    combo->addItem(labels[i], styles[i]);
    combo->setItemData(i, styles[i], Qt::ToolTipRole);
  }
  int index = combo->findData(currentStyle);
  combo->setCurrentIndex(index >= 0 ? index : (styles.isEmpty() ? -1 : 0));
}
