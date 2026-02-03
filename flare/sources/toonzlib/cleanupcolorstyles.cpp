

#include "tpixelutils.h"
#include "tvectorimage.h"
#include "trop.h"

#include <QApplication>

#include "toonz/cleanupcolorstyles.h"

//-------------------------------------------------------------------

TCleanupStyle::TCleanupStyle(const TPixel32 &color)
    : TSolidColorStyle(color)
    , m_outColor(color)
    , m_brightness(0)
    , m_contrast(50)
    , m_canUpdate(true) {}

//-------------------------------------------------------------------

TCleanupStyle::TCleanupStyle(const TCleanupStyle &src)
    : TSolidColorStyle(src)
    , m_outColor(src.m_outColor)
    , m_brightness(src.m_brightness)
    , m_contrast(src.m_contrast)
    , m_contrastEnable(true)
    , m_canUpdate(src.m_canUpdate) {
  setName(src.getName());
}

//-------------------------------------------------------------------

TCleanupStyle::~TCleanupStyle() {}

//-------------------------------------------------------------------

void TCleanupStyle::setCanUpdate(bool state) { m_canUpdate = state; }

//--------------------------------------------

void TCleanupStyle::loadData(TInputStreamInterface &is) {
  TSolidColorStyle::loadData(is);

  VersionNumber version(is.versionNumber());
  if (version.first == 1 && version.second == 18) {
    // Toonz 6.3 ML had removed the output color option
    TPixel32 dummy;
    is >> dummy >> m_brightness >> m_contrast;
    m_outColor = getMainColor();
  } else
    is >> m_outColor >> m_brightness >> m_contrast;
}

//-------------------------------------------------------------------

void TCleanupStyle::saveData(TOutputStreamInterface &os) const {
  TSolidColorStyle::saveData(os);
  os << m_outColor << m_brightness << m_contrast;
}

//-------------------------------------------------------------------

void TCleanupStyle::setMainColor(const TPixel32 &color) {
  if (getMainColor() == m_outColor) m_outColor = color;
  TSolidColorStyle::setMainColor(color);
}

//-------------------------------------------------------------------

int TCleanupStyle::getColorParamCount() const { return 2; }

//-------------------------------------------------------------------

TPixel32 TCleanupStyle::getColorParamValue(int index) const {
  return (index == 0) ? getMainColor() : m_outColor;
}

//-------------------------------------------------------------------

void TCleanupStyle::setColorParamValue(int index, const TPixel32 &color) {
  if (index == 0)
    setMainColor(color);
  else
    m_outColor = color;
}

//-------------------------------------------------------------------

QString TCleanupStyle::getParamNames(int index) const {
  switch (index) {
  case 0:
    return QObject::tr("Brightness");
  case 1:
    return QObject::tr("Contrast");
  default:
    return QString("");
  }
}

//-------------------------------------------------------------------

void TCleanupStyle::getParamRange(int index, double &min, double &max) const {
  if (index == 0) {
    min = -100;
    max = 100;
  } else {
    min = 0;
    max = 100;
  }
}

//-------------------------------------------------------------------

double TCleanupStyle::getParamValue(TColorStyle::double_tag, int index) const {
  switch (index) {
  case 0:
    return getBrightness();
  case 1:
    return getContrast();
  default:
    return 0.0;
  }
}

//-------------------------------------------------------------------

void TCleanupStyle::setParamValue(int index, double value) {
  switch (index) {
  case 0:
    setBrightness(value);
    break;
  case 1:
    setContrast(value);
    break;
  }
}

//-------------------------------------------------------------------

void TCleanupStyle::setBrightness(double b) { m_brightness = b; }

//-------------------------------------------------------------------

void TCleanupStyle::setContrast(double c) { m_contrast = c; }

//-------------------------------------------------------------------

void TCleanupStyle::enableContrast(bool value) { m_contrastEnable = value; }

//-------------------------------------------------------------------

bool TCleanupStyle::isContrastEnabled() const { return m_contrastEnable; }

//-------------------------------------------------------------------

void TCleanupStyle::makeIcon(const TDimension &size) {
  // Build an icon with mainColor on the top half, and
  // getColorParamValue(1) on the bottom.
  if (!m_icon || m_icon->getSize() != size) {
    TRaster32P ras(size);
    m_icon = ras;
  }

  TPixel32 topCol(getMainColor()), botCol(getColorParamValue(1));

  int ly_2 = size.ly / 2;
  TRaster32P botRas(m_icon->extract(0, 0, size.lx, ly_2));
  TRaster32P topRas(m_icon->extract(0, ly_2 + 1, size.lx, size.ly));

  makeIcon(botRas, botCol);
  makeIcon(topRas, topCol);
}

//-------------------------------------------------------------------

void TCleanupStyle::makeIcon(TRaster32P &ras, const TPixel32 &col) {
  if (col.m == 255)
    ras->fill(col);
  else {
    TRaster32P fg(ras->getSize());
    fg->fill(premultiply(col));
    TRop::checkBoard(ras, TPixel32::Black, TPixel32::White, TDimensionD(6, 6),
                     TPointD());
    TRop::over(ras, fg);
  }
}

//===================================================================

TColorCleanupStyle::TColorCleanupStyle(const TPixel32 &color)
    : TCleanupStyle(color), m_hRange(60.0), m_lineWidth(90.0) {}

//-------------------------------------------------------------------

void TColorCleanupStyle::loadData(TInputStreamInterface &is) {
  TCleanupStyle::loadData(is);
  is >> m_hRange;

  if (is.versionNumber() >= VersionNumber(1, 18)) is >> m_lineWidth;
}

//-------------------------------------------------------------------

void TColorCleanupStyle::saveData(TOutputStreamInterface &os) const {
  TCleanupStyle::saveData(os);
  os << m_hRange;
  os << m_lineWidth;
}

//-------------------------------------------------------------------

TColorStyle *TColorCleanupStyle::clone() const {
  return new TColorCleanupStyle(*this);
}

//-------------------------------------------------------------------

QString TColorCleanupStyle::getDescription() const { return "CleanupColor"; }

//-------------------------------------------------------------------

int TColorCleanupStyle::getTagId() const { return 2001; }

//-------------------------------------------------------------------

QString TColorCleanupStyle::getParamNames(int index) const {
  if (index == 2) QObject::tr("HRange");
  if (index == 3) QObject::tr("Line Width");
  return TCleanupStyle::getParamNames(index);
}

//-------------------------------------------------------------------

void TColorCleanupStyle::getParamRange(int index, double &min,
                                       double &max) const {
  if (index >= 2) {
    min = 0;
    max = 100;
  } else
    TCleanupStyle::getParamRange(index, min, max);
}

//-------------------------------------------------------------------

double TColorCleanupStyle::getParamValue(TColorStyle::double_tag tag,
                                         int index) const {
  if (index == 2) return getHRange();
  if (index == 3) return getLineWidth();
  return TCleanupStyle::getParamValue(tag, index);
}

//-------------------------------------------------------------------

void TColorCleanupStyle::setParamValue(int index, double value) {
  if (index == 2)
    setHRange(value);
  else if (index == 3)
    setLineWidth(value);
  else
    TCleanupStyle::setParamValue(index, value);
}

//-------------------------------------------------------------------

void TColorCleanupStyle::setHRange(double hRange) { m_hRange = hRange; }

//-------------------------------------------------------------------

void TColorCleanupStyle::setLineWidth(double lineWidth) {
  m_lineWidth = lineWidth;
}

//===================================================================

TBlackCleanupStyle::TBlackCleanupStyle(const TPixel32 &color)
    : TCleanupStyle(color), m_colorThreshold(70), m_whiteThreshold(10) {}

//-------------------------------------------------------------------

TColorStyle *TBlackCleanupStyle::clone() const {
  return new TBlackCleanupStyle(*this);
}

//-------------------------------------------------------------------

void TBlackCleanupStyle::loadData(TInputStreamInterface &is) {
  TCleanupStyle::loadData(is);
  is >> m_colorThreshold >> m_whiteThreshold;
}

//-------------------------------------------------------------------

void TBlackCleanupStyle::saveData(TOutputStreamInterface &os) const {
  TCleanupStyle::saveData(os);
  os << m_colorThreshold << m_whiteThreshold;
}

//-------------------------------------------------------------------

QString TBlackCleanupStyle::getDescription() const { return "CleanupBlack"; }

//-------------------------------------------------------------------

int TBlackCleanupStyle::getTagId() const { return 2002; }

//-------------------------------------------------------------------

QString TBlackCleanupStyle::getParamNames(int index) const {
  switch (index) {
  case 2:
    return QObject::tr("ColorThres");
  case 3:
    return QObject::tr("WhiteThres");
  default:
    return TCleanupStyle::getParamNames(index);
  }
}

//-------------------------------------------------------------------

void TBlackCleanupStyle::getParamRange(int index, double &min,
                                       double &max) const {
  switch (index) {
  case 2:
  case 3:
    min = 0;
    max = 100;
    break;
  default:
    TCleanupStyle::getParamRange(index, min, max);
  }
}

//-------------------------------------------------------------------

double TBlackCleanupStyle::getParamValue(TColorStyle::double_tag tag,
                                         int index) const {
  switch (index) {
  case 2:
    return getColorThreshold();
  case 3:
    return getWhiteThreshold();
  default:
    return TCleanupStyle::getParamValue(tag, index);
  }
}

//-------------------------------------------------------------------

void TBlackCleanupStyle::setParamValue(int index, double value) {
  switch (index) {
  case 2:
    setColorThreshold(value);
    break;
  case 3:
    setWhiteThreshold(value);
    break;
  default:
    TCleanupStyle::setParamValue(index, value);
  }
}

//-------------------------------------------------------------------

void TBlackCleanupStyle::setColorThreshold(double threshold) {
  m_colorThreshold = threshold;
}

//-------------------------------------------------------------------

void TBlackCleanupStyle::setWhiteThreshold(double threshold) {
  m_whiteThreshold = threshold;
}

namespace {
TColorStyle::Declaration s0(new TBlackCleanupStyle());
TColorStyle::Declaration s1(new TColorCleanupStyle());
}
