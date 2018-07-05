#include "iwa_timecodefx.h"

#include "tparamuiconcept.h"

#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
//------------------------------------------------------------------

Iwa_TimeCodeFx::Iwa_TimeCodeFx()
    : m_displayType(new TIntEnumParam(TYPE_HHMMSSFF, "HH;MM;SS;FF"))
    , m_frameRate(24)
    , m_startFrame(0)
    , m_position(TPointD(0.0, 0.0))
    , m_size(25)
    , m_textColor(TPixel32::White)
    , m_showBox(true)
    , m_boxColor(TPixel32::Black) {
  m_displayType->addItem(TYPE_HHMMSSFF2, "HH:MM:SS:FF");
  m_displayType->addItem(TYPE_FRAME, "Frame Number");
  m_position->getX()->setMeasureName("fxLength");
  m_position->getY()->setMeasureName("fxLength");
  m_size->setMeasureName("fxLength");
  m_size->setValueRange(0.1, (std::numeric_limits<double>::max)());

  bindParam(this, "displayType", m_displayType);
  bindParam(this, "frameRate", m_frameRate);
  bindParam(this, "startFrame", m_startFrame);
  bindParam(this, "position", m_position);
  bindParam(this, "size", m_size);
  bindParam(this, "textColor", m_textColor);
  bindParam(this, "showBox", m_showBox);
  bindParam(this, "boxColor", m_boxColor);
}

//------------------------------------------------------------------

bool Iwa_TimeCodeFx::doGetBBox(double frame, TRectD &bBox,
                               const TRenderSettings &ri) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------------------------

void Iwa_TimeCodeFx::doCompute(TTile &tile, double frame,
                               const TRenderSettings &ri) {
  double fac   = sqrt(fabs(ri.m_affine.det()));
  int size     = (int)(fac * fabs(m_size->getValue(frame)));
  TPoint point = convert(
      fac * m_position->getValue(frame) -
      (tile.m_pos + tile.getRaster()->getCenterD()) +
      TPointD(ri.m_cameraBox.getLx() / 2.0, ri.m_cameraBox.getLy() / 2.0));
#ifdef _WIN32
  QFont font("Arial", size);
#else
  QFont font("Helvetica", size);
#endif
  font.setWeight(QFont::Normal);
  QFontMetrics fm(font);
  QString timeCodeStr = getTimeCodeStr(frame, ri);
  int width           = fm.width(timeCodeStr);
  int height          = fm.height();

  QImage img(width, height, QImage::Format_ARGB32);

  if (m_showBox->getValue()) {
    TPixel32 boxColor = m_boxColor->getValue(frame);
    img.fill(QColor((int)boxColor.r, (int)boxColor.g, (int)boxColor.b,
                    (int)boxColor.m));
  } else
    img.fill(Qt::transparent);

  QPainter painter(&img);
  TPixel32 color = m_textColor->getValue(frame);
  painter.setPen(
      QColor((int)color.r, (int)color.g, (int)color.b, (int)color.m));
  painter.setFont(font);
  painter.drawText(QPoint(0, fm.ascent()), timeCodeStr);

  tile.getRaster()->clear();
  TRaster32P ras32 = (TRaster32P)tile.getRaster();
  TRaster64P ras64 = (TRaster64P)tile.getRaster();
  if (ras32)
    putTimeCodeImage<TRaster32P, TPixel32>(ras32, point, img);
  else if (ras64)
    putTimeCodeImage<TRaster64P, TPixel64>(ras64, point, img);
  else
    throw TException("Iwa_TimeCodeFx: unsupported Pixel Type");
}

//------------------------------------------------------------------

void Iwa_TimeCodeFx::getParamUIs(TParamUIConcept *&concepts, int &length) {
  concepts = new TParamUIConcept[length = 1];

  concepts[0].m_type  = TParamUIConcept::POINT;
  concepts[0].m_label = "Position";
  concepts[0].m_params.push_back(m_position);
}

//------------------------------------------------------------------

std::string Iwa_TimeCodeFx::getAlias(double frame,
                                     const TRenderSettings &info) const {
  std::string alias = getFxType();
  alias += "[";

  std::string paramalias("");
  for (int i = 0; i < getParams()->getParamCount(); ++i) {
    TParam *param = getParams()->getParam(i);
    paramalias += param->getName() + "=" + param->getValueAlias(frame, 3);
  }

  return alias + std::to_string(frame) + "," + std::to_string(getIdentifier()) +
         paramalias + "]";
}

//------------------------------------------------------------------

QString Iwa_TimeCodeFx::getTimeCodeStr(double frame,
                                       const TRenderSettings &ri) {
  int f = (int)frame + m_startFrame->getValue();

  if (m_displayType->getValue() != TYPE_FRAME) {
    QString separator =
        (m_displayType->getValue() == TYPE_HHMMSSFF ? ";" : ":");
    bool neg = (f < 0);
    f        = abs(f);
    int fps  = m_frameRate->getValue();
    int hh   = f / (fps * 60 * 60);
    f -= hh * fps * 60 * 60;
    int mm = f / (fps * 60);
    f -= mm * fps * 60;
    int ss = f / fps;
    int ff = f % fps;
    return QString((neg) ? "-" : "") +
           QString::number(hh).rightJustified(2, '0') + separator +
           QString::number(mm).rightJustified(2, '0') + separator +
           QString::number(ss).rightJustified(2, '0') + separator +
           QString::number(ff).rightJustified(2, '0');
  } else {   // TYPE_FRAMENUMBER
    f += 1;  // starting from "000001" with no frame offset.
    return QString((f < 0) ? "-" : "") +
           QString::number(abs(f)).rightJustified(6, '0');
  }
}

//------------------------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_TimeCodeFx::putTimeCodeImage(const RASTER srcRas, TPoint &pos,
                                      QImage &img) {
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

FX_PLUGIN_IDENTIFIER(Iwa_TimeCodeFx, "iwa_TimeCodeFx");
