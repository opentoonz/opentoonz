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
    , m_showBorder(false) {
  m_targetType->setValue(INPUT_TEXT);

  m_hAlign->addItem(Qt::AlignRight, "Right");
  m_hAlign->addItem(Qt::AlignHCenter, "Center");
  m_hAlign->addItem(Qt::AlignJustify, "Justify");

  m_text->setMultiLineEnabled(true);

  m_center->getX()->setMeasureName("fxLength");
  m_center->getY()->setMeasureName("fxLength");
  m_width->setMeasureName("fxLength");
  m_height->setMeasureName("fxLength");

  m_width->setValueRange(1.0, (std::numeric_limits<double>::max)());
  m_height->setValueRange(1.0, (std::numeric_limits<double>::max)());

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
}

//------------------------------------------------------------------

bool Iwa_TextFx::doGetBBox(double frame, TRectD &bBox,
                           const TRenderSettings &ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------------

void Iwa_TextFx::doCompute(TTile &tile, double frame,
                           const TRenderSettings &ri) {
  QString text;
  if (m_targetType->getValue() == INPUT_TEXT)
    text = QString::fromStdWString(m_text->getValue());
  else
    text = m_noteLevelStr;
  if (text.isEmpty()) return;

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