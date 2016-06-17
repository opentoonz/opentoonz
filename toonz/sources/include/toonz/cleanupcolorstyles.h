#pragma once

#ifndef CLEANUPCOLORSTYLES_INCLUDED
#define CLEANUPCOLORSTYLES_INCLUDED

#include "tsimplecolorstyles.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QString>

//-------------------------------------------------------------------

class DVAPI TCleanupStyle : public TSolidColorStyle {
  TPixel32 m_outColor;
  double m_brightness, m_contrast;
  bool m_contrastEnable;
  bool m_canUpdate;

public:
  TCleanupStyle(const TPixel32 &color = TPixel32::Black);
  TCleanupStyle(const TCleanupStyle &);
  ~TCleanupStyle();

  void setMainColor(const TPixel32 &color);

  int getColorParamCount() const;
  TPixel32 getColorParamValue(int index) const;
  void setColorParamValue(int index, const TPixel32 &color);

  int getParamCount() const { return 2; }
  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  double getBrightness() const { return m_brightness; }
  double getContrast() const { return m_contrastEnable ? m_contrast : 100; }
  void setBrightness(double b);
  void setContrast(double c);
  void enableContrast(bool value);
  bool isContrastEnabled() const;

  bool canUpdate() const { return m_canUpdate; }
  void setCanUpdate(bool state);

  void makeIcon(const TDimension &size);

protected:
  void loadData(TInputStreamInterface &);
  void saveData(TOutputStreamInterface &) const;

private:
  // not implemented
  TCleanupStyle &operator=(const TCleanupStyle &);

  void makeIcon(TRaster32P &ras, const TPixel32 &col);
};

//-------------------------------------------------------------------

class DVAPI TColorCleanupStyle : public TCleanupStyle {
  double m_hRange;
  double m_lineWidth;

public:
  TColorCleanupStyle(const TPixel32 &color = TPixel32::Black);

  TColorStyle *clone() const;
  QString getDescription() const;
  int getTagId() const;

  int getParamCount() const { return 4; }
  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  double getHRange() const { return m_hRange; }
  void setHRange(double hRange);

  double getLineWidth() const { return m_lineWidth; }
  void setLineWidth(double lineWidth);

protected:
  void loadData(TInputStreamInterface &);
  void saveData(TOutputStreamInterface &) const;

private:
  // not implemented
  TColorCleanupStyle &operator=(const TColorCleanupStyle &);
};

//-------------------------------------------------------------------

class DVAPI TBlackCleanupStyle : public TCleanupStyle {
  double m_colorThreshold, m_whiteThreshold;

public:
  TBlackCleanupStyle(const TPixel32 &color = TPixel32::Black);

  TColorStyle *clone() const;
  QString getDescription() const;
  int getTagId() const;

  int getParamCount() const { return 4; }
  QString getParamNames(int index) const;
  void getParamRange(int index, double &min, double &max) const;
  double getParamValue(TColorStyle::double_tag, int index) const;
  void setParamValue(int index, double value);

  double getColorThreshold() const { return m_colorThreshold; }
  void setColorThreshold(double t);
  double getWhiteThreshold() const { return m_whiteThreshold; }
  void setWhiteThreshold(double t);

protected:
  void loadData(TInputStreamInterface &);
  void saveData(TOutputStreamInterface &) const;

private:
  // not implemented
  TBlackCleanupStyle &operator=(const TBlackCleanupStyle &);
};

//-------------------------------------------------------------------

#endif
