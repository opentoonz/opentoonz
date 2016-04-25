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

class TAirbrushRasterStyle : public TColorStyle, public TRasterStyleFx
{
protected:
	TPixel32 m_color;
	double m_blur;

public:
	TAirbrushRasterStyle(const TPixel32 &color, double blur)
		: m_color(color), m_blur(blur) {}

	TColorStyle *clone() const;

public:
	// n.b. per un plain color: isRasterStyle() == true, ma getRasterStyleFx() = 0

	TStrokeProp *makeStrokeProp(const TStroke *stroke) { return 0; }
	TRegionProp *makeRegionProp(const TRegion *region) { return 0; }
	TRasterStyleFx *getRasterStyleFx() { return this; }

	bool isRegionStyle() const { return false; }
	bool isStrokeStyle() const { return false; }
	bool isRasterStyle() const { return true; }
	void getEnlargement(int &borderIn, int &borderOut) const
	{
		borderIn = tceil(2 * m_blur);
		borderOut = tceil(m_blur);
	}

	bool hasMainColor() const { return true; }
	TPixel32 getMainColor() const { return m_color; }
	void setMainColor(const TPixel32 &color) { m_color = color; }

	int getColorParamCount() const { return 1; }
	TPixel32 getColorParamValue(int index) const { return m_color; }
	void setColorParamValue(int index, const TPixel32 &color)
	{
		m_color = color;
	}

	virtual QString getDescription() const { return QCoreApplication::translate("TAirbrushRasterStyle", "Airbrush"); }

	int getParamCount() const { return 1; }
	TColorStyle::ParamType getParamType(int index) const
	{
		assert(index == 0);
		return TColorStyle::DOUBLE;
	}

	QString getParamNames(int index) const
	{
		assert(index == 0);
		return QCoreApplication::translate("TAirbrushRasterStyle", "Blur value");
	}
	void getParamRange(int index, double &min, double &max) const
	{
		assert(index == 0);
		min = 0;
		max = 30;
	}
	double getParamValue(TColorStyle::double_tag, int index) const
	{
		assert(index == 0);
		return m_blur;
	}
	void setParamValue(int index, double value)
	{
		assert(index == 0);
		m_blur = value;
	}

	void invalidateIcon();

	//const TRaster32P &getIcon(const TDimension &d) {assert(false);return (TRaster32P)0;}

	TPixel32 getAverageColor() const { return m_color; }

	int getTagId() const { return 1150; }

	bool isInkStyle() const { return true; }
	bool isPaintStyle() const { return false; }

	bool compute(const Params &params) const;

protected:
	virtual void makeIcon(const TDimension &d);

	void arrangeIcon(const TDimension &d, const TRasterP &normalIc);

	void loadData(TInputStreamInterface &);
	void saveData(TOutputStreamInterface &) const;

	// per la compatibilita' con il passato
	void loadData(int oldId, TInputStreamInterface &){};
};

//=============================================================================

class TBlendRasterStyle : public TAirbrushRasterStyle
{
public:
	TBlendRasterStyle(const TPixel32 &color, double blur)
		: TAirbrushRasterStyle(color, blur) {}
	TColorStyle *clone() const;

	int getTagId() const { return 1160; }

	virtual QString getDescription() const { return QCoreApplication::translate("TBlendRasterStyle", "Blend"); }

	void makeIcon(const TDimension &d);

	bool compute(const TRasterStyleFx::Params &params) const;

private:
	double computeFactor(const TRasterStyleFx::Params &params) const;
};

//=============================================================================

class TNoColorRasterStyle : public TColorStyle, TRasterStyleFx
{
public:
	TNoColorRasterStyle() {}
	TColorStyle *clone() const { return new TNoColorRasterStyle(*this); }

	// n.b. per un plain color: isRasterStyle() == true, ma getRasterStyleFx() = 0

	TStrokeProp *makeStrokeProp(const TStroke *stroke) { return 0; }
	TRegionProp *makeRegionProp(const TRegion *region) { return 0; }
	TRasterStyleFx *getRasterStyleFx() { return this; }

	bool isRegionStyle() const { return false; }
	bool isStrokeStyle() const { return false; }
	bool isRasterStyle() const { return true; }

	virtual QString getDescription() const { return QCoreApplication::translate("TNoColorRasterStyle", "Markup"); }

	bool hasMainColor() { return false; }
	//TPixel32 getMainColor() const {return m_color;}
	//void setMainColor(const TPixel32 &color) {m_color = color;}

	int getColorParamCount() const { return 0; }
	TPixel32 getColorParamValue(int index) const
	{
		assert(false);
		return TPixel32();
	}
	void setColorParamValue(int index, const TPixel32 &color) { assert(false); }

	int getTagId() const { return 1151; }

	bool isInkStyle() const { return true; }
	bool isPaintStyle() const { return true; }

	bool compute(const Params &params) const { return false; }

protected:
	void makeIcon(const TDimension &d);

	void loadData(TInputStreamInterface &){};
	void saveData(TOutputStreamInterface &) const {};

	// per la compatibilita' con il passato
	void loadData(int oldId, TInputStreamInterface &){};
};

#endif
