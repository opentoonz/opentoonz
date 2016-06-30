#pragma once

#ifndef _RASTERSTYLES_H_
#define _RASTERSTYLES_H_

#include "tcolorstyles.h"

#include "traster.h"

#include <QCoreApplication>

class TStroke;
class TRegion;
class TStrokeProp;
class TRegionProp;
class TInputStreamInterface;
class TOutputStreamInterface;

//=============================================================================

class TAirbrushRasterStyle : public TColorStyle, public TRasterStyleFx {
protected:
  TPixel32 m_color;
  double m_blur;

public:
  TAirbrushRasterStyle(const TPixel32 &color, double blur)
      : m_color(color), m_blur(blur) {}

  TColorStyle *clone() const override;

public:
  // n.b. per un plain color: isRasterStyle() == true, ma getRasterStyleFx() = 0

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override { return 0; }
  TRegionProp *makeRegionProp(const TRegion *region) override { return 0; }
  TRasterStyleFx *getRasterStyleFx() override { return this; }

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return false; }
  bool isRasterStyle() const override { return true; }
  void getEnlargement(int &borderIn, int &borderOut) const override {
    borderIn  = tceil(2 * m_blur);
    borderOut = tceil(m_blur);
  }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getColorParamCount() const override { return 1; }
  TPixel32 getColorParamValue(int index) const override { return m_color; }
  void setColorParamValue(int index, const TPixel32 &color) override {
    m_color = color;
  }

  QString getDescription() const override {
    return QCoreApplication::translate("TAirbrushRasterStyle", "Airbrush");
  }

  int getParamCount() const override { return 1; }
  TColorStyle::ParamType getParamType(int index) const override {
    assert(index == 0);
    return TColorStyle::DOUBLE;
  }

  QString getParamNames(int index) const override {
    assert(index == 0);
    return QCoreApplication::translate("TAirbrushRasterStyle", "Blur value");
  }
  void getParamRange(int index, double &min, double &max) const override {
    assert(index == 0);
    min = 0;
    max = 30;
  }
  double getParamValue(TColorStyle::double_tag, int index) const override {
    assert(index == 0);
    return m_blur;
  }
  void setParamValue(int index, double value) override {
    assert(index == 0);
    m_blur = value;
  }

  void invalidateIcon();

  // const TRaster32P &getIcon(const TDimension &d) {assert(false);return
  // (TRaster32P)0;}

  TPixel32 getAverageColor() const override { return m_color; }

  int getTagId() const override { return 1150; }

  bool isInkStyle() const override { return true; }
  bool isPaintStyle() const override { return false; }

  bool compute(const Params &params) const override;

protected:
  void makeIcon(const TDimension &d) override;

  void arrangeIcon(const TDimension &d, const TRasterP &normalIc);

  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;

  // per la compatibilita' con il passato
  void loadData(int oldId, TInputStreamInterface &) override{};
};

//=============================================================================

class TBlendRasterStyle final : public TAirbrushRasterStyle {
public:
  TBlendRasterStyle(const TPixel32 &color, double blur)
      : TAirbrushRasterStyle(color, blur) {}
  TColorStyle *clone() const override;

  int getTagId() const override { return 1160; }

  QString getDescription() const override {
    return QCoreApplication::translate("TBlendRasterStyle", "Blend");
  }

  void makeIcon(const TDimension &d) override;

  bool compute(const TRasterStyleFx::Params &params) const override;

private:
  double computeFactor(const TRasterStyleFx::Params &params) const;
};

//=============================================================================

class TNoColorRasterStyle final : public TColorStyle, TRasterStyleFx {
public:
  TNoColorRasterStyle() {}
  TColorStyle *clone() const override { return new TNoColorRasterStyle(*this); }

  // n.b. per un plain color: isRasterStyle() == true, ma getRasterStyleFx() = 0

  TStrokeProp *makeStrokeProp(const TStroke *stroke) override { return 0; }
  TRegionProp *makeRegionProp(const TRegion *region) override { return 0; }
  TRasterStyleFx *getRasterStyleFx() override { return this; }

  bool isRegionStyle() const override { return false; }
  bool isStrokeStyle() const override { return false; }
  bool isRasterStyle() const override { return true; }

  QString getDescription() const override {
    return QCoreApplication::translate("TNoColorRasterStyle", "Markup");
  }

  bool hasMainColor() { return false; }
  // TPixel32 getMainColor() const {return m_color;}
  // void setMainColor(const TPixel32 &color) {m_color = color;}

  int getColorParamCount() const override { return 0; }
  TPixel32 getColorParamValue(int index) const override {
    assert(false);
    return TPixel32();
  }
  void setColorParamValue(int index, const TPixel32 &color) override {
    assert(false);
  }

  int getTagId() const override { return 1151; }

  bool isInkStyle() const override { return true; }
  bool isPaintStyle() const override { return true; }

  bool compute(const Params &params) const override { return false; }

protected:
  void makeIcon(const TDimension &d) override;

  void loadData(TInputStreamInterface &) override{};
  void saveData(TOutputStreamInterface &) const override{};

  // per la compatibilita' con il passato
  void loadData(int oldId, TInputStreamInterface &) override{};
};

#endif
