#pragma once

#ifndef FLOWLINESTROKESTYLE_H
#define FLOWLINESTROKESTYLE_H

// TnzCore includes
#include "tsimplecolorstyles.h"
#include "tvectorimage.h"
#include "tstrokeprop.h"
#include "tgl.h"

#include "toonz/imagestyles.h"

#include <QImage>
class TVectorRendeData;
class TRandom;

#undef DVAPI
#undef DVVAR

#ifdef COLORFX_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class FlowLineStrokeStyle final : public TSimpleStrokeStyle {
  TPixel32 m_color;

  enum PARAMID {
    Density = 0,
    Extension,
    WidthScale,
    StraightenEnds,
    ParamCount
  };

  // thin line's density ×ü‚Ì–§“x
  double m_density;
  // extend the edges ’[•”‚ÌL‚Î‚·—Ê
  double m_extension;
  // extend the widths
  // •‚ÌŠg‘åBŒ³‚Ìü‚Ì•‚ð‘‚â‚·‚Æregion‚ÌŒvŽZ‚É–c‘å‚ÈŽžŠÔ‚ª‚©‚©‚é
  double m_widthScale;

  bool m_straightenEnds;

public:
  FlowLineStrokeStyle();

  TColorStyle *clone() const override;

  void invalidate() {}

  QString getDescription() const override {
    return QCoreApplication::translate("FlowLineStrokeStyle", "Flow Line");
  }
  std::string getBrushIdName() const override { return "FlowLineStrokeStyle"; }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getParamCount() const override;
  TColorStyle::ParamType getParamType(int index) const override;

  QString getParamNames(int index) const override;

  void getParamRange(int index, double &min, double &max) const override;
  double getParamValue(TColorStyle::double_tag, int index) const override;
  void setParamValue(int index, double value) override;

  bool getParamValue(TColorStyle::bool_tag, int index) const override;
  void setParamValue(int index, bool value) override;

  void drawStroke(const TColorFunction *cf,
                  const TStroke *stroke) const override;

  void loadData(TInputStreamInterface &is) override {
    int straightenEnds;
    is >> m_color >> m_density >> m_extension >> m_widthScale >> straightenEnds;
    m_straightenEnds = (straightenEnds == 0) ? false : true;
  }
  void saveData(TOutputStreamInterface &os) const override {
    int straightenEnds = m_straightenEnds ? 1 : 0;
    os << m_color << m_density << m_extension << m_widthScale
       << m_straightenEnds;
  }
  bool isSaveSupported() { return true; }

  int getTagId() const override { return 201; }

  TRectD getStrokeBBox(const TStroke *stroke) const override;
};

#endif  // FLOWLINESTROKESTYLE_H
