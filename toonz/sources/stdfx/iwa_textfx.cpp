#include "iwa_textfx.h"

#include "tparamuiconcept.h"
#include <QFontMetrics>
#include <QImage>
#include <QPainter>

//------------------------------------------------------------------

Iwa_TextFx::Iwa_TextFx()
    : m_text(L"Lorem ipsum")
    , m_hAlign(new TIntEnumParam(Qt::AlignLeft, "Left"))
    , m_center(TPointD(0.0, 0.0))
    , m_width(200.0)
    , m_height(60.0)
    , m_font(new TFontParam())
    , m_textColor(TPixel32::Black)
    , m_boxColor(TPixel32::Transparent)
    , m_showBorder(false)
    , m_transformWithColumn(false)
    , m_kerning(true)
    , m_letterSpacing(0.0)
    , m_lineSpacing(0.0)
    , m_indent(0.0)
    , m_shrinkToFit(true) {
  m_targetType->setValue(INPUT_TEXT);

  m_hAlign->addItem(Qt::AlignRight, "Right");
  m_hAlign->addItem(Qt::AlignHCenter, "Center");
  m_hAlign->addItem(Qt::AlignJustify, "Justify");

  m_text->setMultiLineEnabled(true);

  m_center->getX()->setMeasureName("fxLength");
  m_center->getY()->setMeasureName("fxLength");
  m_width->setMeasureName("fxLength");
  m_height->setMeasureName("fxLength");
  m_letterSpacing->setMeasureName("fxLength");
  m_lineSpacing->setMeasureName("fxLength");
  m_indent->setMeasureName("fxLength");

  m_width->setValueRange(1.0, (std::numeric_limits<double>::max)());
  m_height->setValueRange(1.0, (std::numeric_limits<double>::max)());
  m_letterSpacing->setValueRange(-1000.0, 1000.0);
  m_lineSpacing->setValueRange(-1000.0, 1000.0);
  m_indent->setValueRange(-1000.0, 1000.0);

  // set the initial font size to 30
  QFont font;
  font.fromString(QString::fromStdWString(m_font->getValue()));
  font.setPixelSize(30);
  m_font->setValue(font.toString().toStdWString(), true);

  bindParam(this, "targetType", m_targetType);
  bindParam(this, "columnIndex", m_columnIndex);
  bindParam(this, "text", m_text);
  bindParam(this, "hAlign", m_hAlign);
  bindParam(this, "center", m_center);
  bindParam(this, "width", m_width);
  bindParam(this, "height", m_height);
  bindParam(this, "font", m_font);
  bindParam(this, "textColor", m_textColor);
  bindParam(this, "boxColor", m_boxColor);
  bindParam(this, "showBorder", m_showBorder);
  bindParam(this, "transformWithColumn", m_transformWithColumn);
  bindParam(this, "kerning", m_kerning);
  bindParam(this, "letterSpacing", m_letterSpacing);
  bindParam(this, "lineSpacing", m_lineSpacing);
  bindParam(this, "indent", m_indent);
  bindParam(this, "shrinkToFit", m_shrinkToFit);
}

//------------------------------------------------------------------

namespace {

// Resolution of the column-transformed layout in layout pixels per FX
// length unit, independent of the output render resolution.
constexpr int transformedLayoutScale = Iwa_TextFxLayout::transformedScale;

int textFlags(int hAlign) {
  // As in the camera mode, text is always wrapped at the box width.
  return hAlign | Qt::AlignVCenter | Qt::TextWordWrap;
}

// Layout pixels (y down, laid-out text centered at the origin) to fx length
// units in the column (y up) at `center`.
TAffine layoutToColumn(const TPointD &center) {
  return TTranslation(center) *
         TScale(1.0 / transformedLayoutScale, -1.0 / transformedLayoutScale);
}

}  // namespace

//------------------------------------------------------------------

QString Iwa_TextFx::sourceText() const {
  if (m_targetType->getValue() == INPUT_TEXT)
    return QString::fromStdWString(m_text->getValue());
  return m_noteLevelStr;
}

//------------------------------------------------------------------

bool Iwa_TextFx::usesLayoutOptions(double frame) const {
  return !m_kerning->getValue() || !m_shrinkToFit->getValue() ||
         m_letterSpacing->getValue(frame) != 0 ||
         m_lineSpacing->getValue(frame) != 0 || m_indent->getValue(frame) != 0;
}

//------------------------------------------------------------------

Iwa_TextFxLayout::Options Iwa_TextFx::layoutOptions(double frame,
                                                    double scale) const {
  Iwa_TextFxLayout::Options options;
  options.hAlign        = m_hAlign->getValue();
  options.kerning       = m_kerning->getValue();
  options.letterSpacing = m_letterSpacing->getValue(frame) * scale;
  options.lineSpacing   = m_lineSpacing->getValue(frame) * scale;
  options.indent        = m_indent->getValue(frame) * scale;
  return options;
}

//------------------------------------------------------------------

bool Iwa_TextFx::transformedLayout(
    double frame, const QString &text, QFont &font, QRectF &box, QRectF &block,
    std::unique_ptr<Iwa_TextFxLayout::Block> &layout) const {
  layout.reset();
  if (text.isEmpty()) return false;
  // Geometric outlines, so glyphs scale and rotate with the column exactly.
  font = Iwa_TextFxLayout::transformedFont(
      QString::fromStdWString(m_font->getValue()));
  box = QRectF(0, 0, m_width->getValue(frame) * transformedLayoutScale,
               m_height->getValue(frame) * transformedLayoutScale);
  if (usesLayoutOptions(frame)) {
    layout =
        Iwa_TextFxLayout::layOut(text, font, box.width(), box.height(),
                                 layoutOptions(frame, transformedLayoutScale),
                                 m_shrinkToFit->getValue());
    if (!layout) return false;
    block = layout->rect();
    return true;
  }
  const int flags = textFlags(m_hAlign->getValue());
  auto fits       = [&](const QRectF &rect) {
    return rect.width() <= box.width() && rect.height() <= box.height();
  };
  block = QFontMetricsF(font).boundingRect(box, flags, text);
  if (fits(block)) return true;
  // Like the camera mode, shrink the font until the text fits the box.
  int low = 1, high = font.pixelSize() - 1, best = 0;
  QRectF bestBlock;
  while (low <= high) {
    const int middle = (low + high) / 2;
    font.setPixelSize(middle);
    const QRectF rect = QFontMetricsF(font).boundingRect(box, flags, text);
    if (fits(rect)) {
      best      = middle;
      bestBlock = rect;
      low       = middle + 1;
    } else
      high = middle - 1;
  }
  if (!best) return false;
  font.setPixelSize(best);
  block = bestBlock;
  return true;
}

//------------------------------------------------------------------

bool Iwa_TextFx::doGetBBox(double frame, TRectD &bBox,
                           const TRenderSettings &ri) {
  if (!m_transformWithColumn->getValue()) {
    bBox = TConsts::infiniteRectD;
    return true;
  }
  QFont font;
  QRectF box, block;
  std::unique_ptr<Iwa_TextFxLayout::Block> layout;
  if (!transformedLayout(frame, sourceText(), font, box, block, layout)) {
    bBox = TRectD();
    return false;
  }
  // Glyphs may overhang their line box (italics, accents); the box color
  // margin is 0.2 of the font size. The box is in fx space: getBBox() maps
  // it through the render affine.
  const double margin = 0.5 * font.pixelSize();
  const QRectF local  = QRectF(-block.width() / 2, -block.height() / 2,
                               block.width(), block.height())
                           .adjusted(-margin, -margin, margin, margin);
  bBox = layoutToColumn(m_center->getValue(frame)) *
         TRectD(local.left(), local.top(), local.right(), local.bottom());
  return true;
}

//------------------------------------------------------------------

void Iwa_TextFx::computeTransformed(TTile &tile, double frame,
                                    const TRenderSettings &ri,
                                    const QString &text) {
  tile.getRaster()->clear();
  QFont font;
  QRectF box, block;
  std::unique_ptr<Iwa_TextFxLayout::Block> layout;
  if (!transformedLayout(frame, text, font, box, block, layout)) return;
  const TAffine toOutput =
      ri.m_affine * layoutToColumn(m_center->getValue(frame));
  if (std::abs(toOutput.det()) < 1e-12) return;
  const TDimension size = tile.getRaster()->getSize();
  // Output coordinates to image coordinates: the image is flipped vertically
  // when it is copied into the raster (see putTextImage).
  const TAffine toImage =
      TAffine(1, 0, -tile.m_pos.x, 0, -1, size.ly + tile.m_pos.y) * toOutput;
  QImage img(size.lx, size.ly, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter painter(&img);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
  painter.setTransform(QTransform(toImage.a11, toImage.a21, toImage.a12,
                                  toImage.a22, toImage.a13, toImage.a23));
  // The laid-out text is centered on the center point.
  const QPointF shift    = -block.center();
  const double lineWidth = 0.1 * font.pixelSize();
  const QRectF frameRect = block.translated(shift).adjusted(
      -2 * lineWidth, -lineWidth, 2 * lineWidth, lineWidth);
  const TPixel32 boxColor = m_boxColor->getValue(frame);
  const TPixel32 color    = m_textColor->getValue(frame);
  if (boxColor.m > 0)
    painter.fillRect(frameRect, QColor((int)boxColor.r, (int)boxColor.g,
                                       (int)boxColor.b, (int)boxColor.m));
  QPen pen(QColor((int)color.r, (int)color.g, (int)color.b, (int)color.m));
  painter.setPen(pen);
  painter.setFont(font);
  if (layout)
    layout->draw(painter, shift);
  else
    painter.drawText(box.translated(shift), textFlags(m_hAlign->getValue()),
                     text);
  if (m_showBorder->getValue()) {
    pen.setWidthF(lineWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(frameRect.adjusted(lineWidth / 2, lineWidth / 2,
                                        -lineWidth / 2, -lineWidth / 2));
  }
  painter.end();

  TPoint origin(0, 0);
  TRaster32P ras32 = (TRaster32P)tile.getRaster();
  TRaster64P ras64 = (TRaster64P)tile.getRaster();
  if (ras32)
    putTextImage<TRaster32P, TPixel32>(ras32, origin, img);
  else if (ras64)
    putTextImage<TRaster64P, TPixel64>(ras64, origin, img);
  else
    throw TException("Iwa_TextFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

void Iwa_TextFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  const QString text = sourceText();
  if (text.isEmpty()) return;
  if (m_transformWithColumn->getValue()) {
    computeTransformed(tile, frame, ri, text);
    return;
  }
  if (usesLayoutOptions(frame)) {
    computeWithLayoutOptions(tile, frame, ri, text);
    return;
  }

  QFont font;
  font.fromString(QString::fromStdWString(m_font->getValue()));

  double fac = sqrt(fabs(ri.m_affine.det()));
  int size   = (int)(fac * std::abs(font.pixelSize()));

  TPoint center = convert(
      fac * m_center->getValue(frame) -
      (tile.m_pos + tile.getRaster()->getCenterD()) +
      TPointD(ri.m_cameraBox.getLx() / 2.0, ri.m_cameraBox.getLy() / 2.0));

  QRect textBoxRect(0, 0, fac * m_width->getValue(frame),
                    fac * m_height->getValue(frame));
  textBoxRect.moveCenter(QPoint(center.x, center.y));

  Qt::AlignmentFlag hAlignFlag = (Qt::AlignmentFlag)m_hAlign->getValue();

  // For simplification, text will always be "wrapped".
  // If user would like to make the line with no break, they can just expand the
  // boundary or set the smaller font size.
  int flag = hAlignFlag | Qt::AlignVCenter | Qt::TextWordWrap;

  QFont tmpFont(font);
  tmpFont.setPixelSize(100);
  QFontMetricsF tmpFm(tmpFont);
  QRectF bbox = tmpFm.boundingRect(textBoxRect, flag, text);

  float ratio = std::min(textBoxRect.width() / bbox.width(),
                         textBoxRect.height() / bbox.height());

  // compute the font size which will just fit the item region
  int fontSize = (int)(100.0f * ratio);
  tmpFont.setPixelSize(fontSize);
  tmpFm = QFontMetricsF(tmpFont);
  bbox  = tmpFm.boundingRect(textBoxRect, flag, text);
  bool isInRect;
  if (textBoxRect.width() >= bbox.width() &&
      textBoxRect.height() >= bbox.height())
    isInRect = true;
  else
    isInRect = false;
  while (1) {
    fontSize += (isInRect) ? 1 : -1;
    if (fontSize <= 0)  // cannot draw
      return;
    if (isInRect && fontSize >= size) break;
    tmpFont.setPixelSize(fontSize);
    tmpFm = QFontMetricsF(tmpFont);
    bbox  = tmpFm.boundingRect(textBoxRect, flag, text);

    bool newIsInRect = (textBoxRect.width() >= bbox.width() &&
                        textBoxRect.height() >= bbox.height());
    if (isInRect != newIsInRect) {
      if (isInRect) fontSize--;
      break;
    }
  }

  if (size < fontSize) {
    fontSize = size;
  }

  font.setPixelSize(fontSize);
  tmpFm = QFontMetricsF(font);
  bbox  = tmpFm.boundingRect(textBoxRect, flag, text);

  double lineWidth = 0.1 * (double)fontSize;

  // Usually the text bounding box has less horizontal margin than vertical.
  // So here I added more margin to width.
  QImage img(bbox.width() + (int)(lineWidth * 4),
             bbox.height() + (int)(lineWidth * 2),
             QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);

  bbox.moveCenter(img.rect().center());

  QPainter painter(&img);
  TPixel32 boxColor = m_boxColor->getValue(frame);
  TPixel32 color    = m_textColor->getValue(frame);

  if (boxColor.m > 0)
    painter.fillRect(img.rect(), QColor((int)boxColor.r, (int)boxColor.g,
                                        (int)boxColor.b, (int)boxColor.m));

  QPen pen(QColor((int)color.r, (int)color.g, (int)color.b, (int)color.m));
  painter.setPen(pen);
  painter.setFont(font);
  painter.drawText(bbox, flag, text);

  if (m_showBorder->getValue()) {
    pen.setWidthF(lineWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.drawRect(img.rect().adjusted(lineWidth / 2, lineWidth / 2,
                                         -lineWidth / 2, -lineWidth / 2));
  }

  TPoint imgRootPos = center - TPoint(img.width() / 2, img.height() / 2);

  tile.getRaster()->clear();
  TRaster32P ras32 = (TRaster32P)tile.getRaster();
  TRaster64P ras64 = (TRaster64P)tile.getRaster();
  if (ras32)
    putTextImage<TRaster32P, TPixel32>(ras32, imgRootPos, img);
  else if (ras64)
    putTextImage<TRaster64P, TPixel64>(ras64, imgRootPos, img);
  else
    throw TException("Iwa_TextFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

void Iwa_TextFx::computeWithLayoutOptions(TTile &tile, double frame,
                                          const TRenderSettings &ri,
                                          const QString &text) {
  tile.getRaster()->clear();
  // As in the default camera mode: lengths in output pixels, scaled with the
  // column, and the center relative to the camera.
  QFont font;
  font.fromString(QString::fromStdWString(m_font->getValue()));
  const double fac = sqrt(fabs(ri.m_affine.det()));
  const int size   = (int)(fac * std::abs(font.pixelSize()));
  if (size <= 0) return;
  font.setPixelSize(size);
  const TPoint center = convert(
      fac * m_center->getValue(frame) -
      (tile.m_pos + tile.getRaster()->getCenterD()) +
      TPointD(ri.m_cameraBox.getLx() / 2.0, ri.m_cameraBox.getLy() / 2.0));
  const std::unique_ptr<Iwa_TextFxLayout::Block> layout =
      Iwa_TextFxLayout::layOut(text, font, fac * m_width->getValue(frame),
                               fac * m_height->getValue(frame),
                               layoutOptions(frame, fac),
                               m_shrinkToFit->getValue());
  if (!layout) return;
  const QRectF block     = layout->rect();
  const double lineWidth = 0.1 * font.pixelSize();
  QImage img((int)std::ceil(block.width()) + (int)(lineWidth * 4),
             (int)std::ceil(block.height()) + (int)(lineWidth * 2),
             QImage::Format_ARGB32_Premultiplied);
  if (img.isNull()) return;
  img.fill(Qt::transparent);

  QPainter painter(&img);
  const TPixel32 boxColor = m_boxColor->getValue(frame);
  const TPixel32 color    = m_textColor->getValue(frame);
  if (boxColor.m > 0)
    painter.fillRect(img.rect(), QColor((int)boxColor.r, (int)boxColor.g,
                                        (int)boxColor.b, (int)boxColor.m));
  QPen pen(QColor((int)color.r, (int)color.g, (int)color.b, (int)color.m));
  painter.setPen(pen);
  layout->draw(painter,
               QPointF(img.width() / 2.0, img.height() / 2.0) - block.center());
  if (m_showBorder->getValue()) {
    pen.setWidthF(lineWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.drawRect(img.rect().adjusted(lineWidth / 2, lineWidth / 2,
                                         -lineWidth / 2, -lineWidth / 2));
  }
  painter.end();

  TPoint imgRootPos = center - TPoint(img.width() / 2, img.height() / 2);
  TRaster32P ras32  = (TRaster32P)tile.getRaster();
  TRaster64P ras64  = (TRaster64P)tile.getRaster();
  if (ras32)
    putTextImage<TRaster32P, TPixel32>(ras32, imgRootPos, img);
  else if (ras64)
    putTextImage<TRaster64P, TPixel64>(ras64, imgRootPos, img);
  else
    throw TException("Iwa_TextFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

void Iwa_TextFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 2];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Center";
  concepts[0].m_params.push_back(m_center);

  concepts[1].m_type = TParamUIConcept::RECT;
  concepts[1].m_params.push_back(m_width);
  concepts[1].m_params.push_back(m_height);
  concepts[1].m_params.push_back(m_center);
}

//------------------------------------------------------------------

std::string Iwa_TextFx::getAlias(double frame,
                                 const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  std::string paramalias("");
  for (int i = 0; i < getParams()->getParamCount(); ++i) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }

  if (m_targetType->getValue() == INPUT_TEXT)
    return alias + "," + std::to_string(getIdentifier()) + paramalias + "]";
  else
    return alias + std::to_string(frame) + "," +
           std::to_string(getIdentifier()) + paramalias + "]";
}

//------------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_TextFx::putTextImage(const RASTER srcRas, TPoint &pos, QImage &img) {
  for (int j = 0; j < img.height(); j++) {
    int rasY = pos.y + j;
    if (rasY < 0) continue;
    if (srcRas->getLy() <= rasY) break;

    PIXEL *pix  = srcRas->pixels(rasY);
    QRgb *img_p = (QRgb *)img.scanLine(img.height() - j - 1);
    for (int i = 0; i < img.width(); i++, img_p++) {
      int rasX = pos.x + i;
      if (rasX < 0) continue;
      if (srcRas->getLx() <= rasX) break;

      pix[rasX].r = (typename PIXEL::Channel)(
          qRed(*img_p) * (int)PIXEL::maxChannelValue / (int)UCHAR_MAX);
      pix[rasX].g = (typename PIXEL::Channel)(
          qGreen(*img_p) * (int)PIXEL::maxChannelValue / (int)UCHAR_MAX);
      pix[rasX].b = (typename PIXEL::Channel)(
          qBlue(*img_p) * (int)PIXEL::maxChannelValue / (int)UCHAR_MAX);
      pix[rasX].m = (typename PIXEL::Channel)(
          qAlpha(*img_p) * (int)PIXEL::maxChannelValue / (int)UCHAR_MAX);
    }
  }
}

//==============================================================================

FX_PLUGIN_IDENTIFIER(Iwa_TextFx, "iwa_TextFx");