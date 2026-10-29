#pragma once

#ifndef IWA_TEXTFXLAYOUT_H
#define IWA_TEXTFXLAYOUT_H

// Shared text layout for Text Iwa FX (Iwa_TextFx) when non-default
// kerning, spacing, indent, or shrink-to-fit options are selected.
// Header-only so both rendering modes use consistent layout geometry.

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTextLayout>
#include <QTextOption>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <vector>

namespace Iwa_TextFxLayout {

// Resolution of the column-transformed layout ("Transform with Column"), in
// layout pixels per fx length unit. The layout does not depend on the render
// resolution, so a text keeps its line breaks and proportions at every scale.
constexpr int transformedScale = 32;

// The column-transformed layout font from the font parameter: its size in
// layout pixels, with geometric outlines so glyphs scale and rotate with the
// column exactly.
inline QFont transformedFont(const QString &parameter) {
  QFont font;
  font.fromString(parameter);
  const int size = std::abs(font.pixelSize()) * transformedScale;
  font.setHintingPreference(QFont::PreferNoHinting);
  font.setStyleStrategy(
      QFont::StyleStrategy(QFont::ForceOutline | QFont::PreferQuality));
  font.setPixelSize(std::max(1, size));
  return font;
}

// Layout options; lengths in layout pixels.
struct Options {
  int hAlign = Qt::AlignLeft;  // AlignLeft, AlignRight, AlignHCenter or
                               // AlignJustify
  bool kerning         = true;
  double letterSpacing = 0;  // Added to the advance of every character
  double lineSpacing   = 0;  // Added to the font's line spacing
  double indent        = 0;  // Of the first line of every paragraph

  // The lengths for a font scaled by `factor`.
  Options scaled(double factor) const {
    Options result = *this;
    result.letterSpacing *= factor;
    result.lineSpacing *= factor;
    result.indent *= factor;
    return result;
  }
};

inline QFont spacedFont(QFont font, const Options &options) {
  font.setKerning(options.kerning);
  if (options.letterSpacing != 0)
    font.setLetterSpacing(QFont::AbsoluteSpacing, options.letterSpacing);
  return font;
}

// Text laid out in a box `width` pixels wide: y down, the box's left edge at
// x = 0 and the first line's top at y = 0. Paragraphs are separated by '\n'
// and wrap at word boundaries; a word wider than its line overflows the box.
// Lines are `lineSpacing` further apart than the font's line spacing (line
// height plus leading). Characters are placed with design metrics, so glyph
// positions scale exactly with the font size.
class Block {
  std::vector<std::unique_ptr<QTextLayout>> m_paragraphs;
  QRectF m_rect;

public:
  Block(const QString &text, const QFont &font, double width,
        const Options &options) {
    const QFont spaced   = spacedFont(font, options);
    const double leading = QFontMetricsF(spaced).leading();
    QTextOption option(Qt::Alignment(options.hAlign) &
                       Qt::AlignHorizontal_Mask);
    option.setWrapMode(QTextOption::WordWrap);
    option.setUseDesignMetrics(true);
    double top = 0, x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    bool empty = true;
    QString source(text);
    source.remove(QChar('\r'));
    for (const QString &paragraph : source.split(QChar('\n'))) {
      m_paragraphs.emplace_back(new QTextLayout(paragraph, spaced));
      QTextLayout &layout = *m_paragraphs.back();
      layout.setTextOption(option);
      layout.setCacheEnabled(true);
      layout.beginLayout();
      for (bool first = true;; first = false) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        const double indent = first ? options.indent : 0;
        line.setLineWidth(std::max(0.0, width - indent));
        line.setPosition(QPointF(indent, top));
        top += line.ascent() + line.descent() + leading + options.lineSpacing;
      }
      layout.endLayout();
      for (int index = 0; index < layout.lineCount(); ++index) {
        const QRectF rect = layout.lineAt(index).naturalTextRect();
        x0                = empty ? rect.left() : std::min(x0, rect.left());
        x1                = empty ? rect.right() : std::max(x1, rect.right());
        y0                = empty ? rect.top() : std::min(y0, rect.top());
        y1                = empty ? rect.bottom() : std::max(y1, rect.bottom());
        empty             = false;
      }
    }
    // Anchored like QPainter::drawText's bounding rectangle: at the box's
    // left edge, its right edge or around its middle.
    double left = std::min(0.0, x0), right = std::max(left, x1);
    if (options.hAlign & Qt::AlignRight) {
      left  = x0;
      right = std::max(width, x1);
    } else if (options.hAlign & Qt::AlignHCenter) {
      const double half = std::max({0.0, width / 2 - x0, x1 - width / 2});
      left              = width / 2 - half;
      right             = width / 2 + half;
    }
    m_rect = QRectF(left, y0, right - left, y1 - y0);
  }

  // The laid-out text, as the Text FX centers it on its center point.
  const QRectF &rect() const { return m_rect; }
  int paragraphCount() const { return int(m_paragraphs.size()); }
  const QTextLayout &paragraph(int index) const { return *m_paragraphs[index]; }
  // Draws with the painter's pen, the box origin at `offset`.
  void draw(QPainter &painter, const QPointF &offset) const {
    for (const auto &layout : m_paragraphs) layout->draw(&painter, offset);
  }
};

inline bool fits(const QRectF &rect, double width, double height) {
  return rect.width() <= width && rect.height() <= height;
}

// Lays `text` out in a box `width` x `height` pixels. With `shrink`, text
// that does not fit is laid out at the largest smaller font pixel size that
// fits, the length options scaled with it (nullptr when none does), as the
// Text FX does by default; `font` gets the size used.
inline std::unique_ptr<Block> layOut(const QString &text, QFont &font,
                                     double width, double height,
                                     const Options &options, bool shrink) {
  std::unique_ptr<Block> block(new Block(text, font, width, options));
  if (!shrink || fits(block->rect(), width, height)) return block;
  const int size = font.pixelSize();
  int low = 1, high = size - 1, best = 0;
  std::unique_ptr<Block> bestBlock;
  while (low <= high) {
    const int middle = (low + high) / 2;
    QFont smaller(font);
    smaller.setPixelSize(middle);
    std::unique_ptr<Block> candidate(
        new Block(text, smaller, width, options.scaled(double(middle) / size)));
    if (fits(candidate->rect(), width, height)) {
      best      = middle;
      bestBlock = std::move(candidate);
      low       = middle + 1;
    } else
      high = middle - 1;
  }
  if (!best) return nullptr;
  font.setPixelSize(best);
  return bestBlock;
}

}  // namespace Iwa_TextFxLayout

#endif
