#pragma once

#ifndef TSIMPLECOLORSTYLES_H
#define TSIMPLECOLORSTYLES_H

// TnzCore includes
#include "tcolorstyles.h"
#include "tlevel.h"
#include "traster.h"
#include "tstrokeoutline.h"

// Qt includes
#include <QCoreApplication>

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=================================================

//    Forward declarations

class TStrokeProp;
class TRegionProp;
class TRegionOutline;
class TTessellator;
class TColorFunction;
class TFlash;
class TVectorImage;

//=================================================

//**********************************************************************************
//    TSimpleStrokeStyle  declaration
//**********************************************************************************

/*!
  Base classs for stroke color styles.
*/

class DVAPI TSimpleStrokeStyle : public TColorStyle
{
public:
	bool isRegionStyle() const { return false; }
	bool isStrokeStyle() const { return true; }

	virtual TStrokeProp *makeStrokeProp(const TStroke *stroke);
	virtual TRegionProp *makeRegionProp(const TRegion *)
	{
		assert(false);
		return 0;
	}

	virtual void drawStroke(const TColorFunction *cf, const TStroke *stroke) const = 0;
};

//**********************************************************************************
//    TOutlineStyle  declaration
//**********************************************************************************

class DVAPI TOutlineStyle : public TColorStyle
{
public:
	class StrokeOutlineModifier
	{

	public:
		StrokeOutlineModifier() {}
		virtual ~StrokeOutlineModifier() {}
		virtual StrokeOutlineModifier *clone() const = 0;

		virtual void modify(TStrokeOutline &outline) const = 0;
	};

	class RegionOutlineModifier
	{

	public:
		RegionOutlineModifier() {}
		virtual ~RegionOutlineModifier() {}
		virtual RegionOutlineModifier *clone() const = 0;

		virtual void modify(TRegionOutline &outline) const = 0;
	};

protected:
	//  StrokeOutlineModifier *m_strokeOutlineModifier;
	RegionOutlineModifier *m_regionOutlineModifier;

public:
	TOutlineStyle();
	TOutlineStyle(const TOutlineStyle &);
	virtual ~TOutlineStyle();

	//StrokeOutlineModifier* getStrokeOutlineModifier() const { return m_strokeOutlineModifier; }
	//void setStrokeOutlineModifier(StrokeOutlineModifier *modifier);

	RegionOutlineModifier *getRegionOutlineModifier() const { return m_regionOutlineModifier; }
	void setRegionOutlineModifier(RegionOutlineModifier *modifier);

	bool isRegionStyle() const { return true; }
	bool isStrokeStyle() const { return true; }

	virtual void computeOutline(const TStroke *stroke,
								TStrokeOutline &outline,
								TOutlineUtil::OutlineParameter param) const;

	TStrokeProp *makeStrokeProp(const TStroke *stroke);
	TRegionProp *makeRegionProp(const TRegion *region);

	//virtual void drawRegion( const TVectorRenderData &rd, TRegionOutline &outline ) const =0 ;
	virtual void drawRegion(const TColorFunction *cf, const bool antiAliasing, TRegionOutline &outline) const = 0;

	virtual void drawRegion(TFlash &, const TRegion *) const {};
	virtual void drawStroke(const TColorFunction *cf, TStrokeOutline *outline, const TStroke *stroke) const = 0;

	virtual void drawStroke(TFlash &flash, const TStroke *stroke) const { TColorStyle::drawStroke(flash, stroke); }
	virtual void setFill(TFlash &) const {};

protected:
	// Not assignable
	TOutlineStyle &operator=(const TOutlineStyle &);
};

//-------------------------------------------------------------------

typedef TSmartPointerT<TOutlineStyle> TOutlineStyleP;

//**********************************************************************************
//    TSolidColorStyle  declaration
//**********************************************************************************

class DVAPI TSolidColorStyle : public TOutlineStyle
{
	TPixel32 m_color;
	TTessellator *m_tessellator;

protected:
	void makeIcon(const TDimension &d);

	virtual void loadData(TInputStreamInterface &);
	virtual void saveData(TOutputStreamInterface &) const;

public:
	TSolidColorStyle(const TPixel32 &color = TPixel32::Black);
	TSolidColorStyle(const TSolidColorStyle &);
	~TSolidColorStyle();

	TColorStyle *clone() const;

	QString getDescription() const;

	bool hasMainColor() const { return true; }
	TPixel32 getMainColor() const { return m_color; }
	void setMainColor(const TPixel32 &color) { m_color = color; }

	void drawRegion(const TColorFunction *cf, const bool antiAliasing, TRegionOutline &outline) const;
	virtual void drawRegion(TFlash &flash, const TRegion *r) const;

	void drawStroke(const TColorFunction *cf, TStrokeOutline *outline, const TStroke *s) const;

	void setFill(TFlash &flash) const;

	int getTagId() const;

private:
	// Not assignable
	TSolidColorStyle &operator=(const TSolidColorStyle &);
};

//**********************************************************************************
//    TCenterLineStrokeStyle  declaration
//**********************************************************************************

/*!
  Constant thickness stroke style.
*/

class DVAPI TCenterLineStrokeStyle : public TSimpleStrokeStyle
{
	TPixel32 m_color;
	USHORT m_stipple;
	double m_width;

public:
	TCenterLineStrokeStyle(const TPixel32 &color = TPixel32(0, 0, 0, 255),
						   USHORT stipple = 0x0,
						   double width = 1.0);

	TColorStyle *clone() const;

	QString getDescription() const;

	TPixel32 getColor() const { return m_color; }
	USHORT getStipple() const { return m_stipple; }

	void drawStroke(const TColorFunction *cf, const TStroke *stroke) const;
	void drawStroke(TFlash &flash, const TStroke *s) const;

	bool hasMainColor() const { return true; }
	TPixel32 getMainColor() const { return m_color; }
	void setMainColor(const TPixel32 &color) { m_color = color; }

	int getParamCount() const;

	TColorStyle::ParamType getParamType(int index) const;

	QString getParamNames(int index) const;
	void getParamRange(int index, double &min, double &max) const;
	double getParamValue(TColorStyle::double_tag, int index) const;
	void setParamValue(int index, double value);

	int getTagId() const;

protected:
	void loadData(TInputStreamInterface &);
	void saveData(TOutputStreamInterface &) const;

private:
	// Not assignable
	TCenterLineStrokeStyle &operator=(const TCenterLineStrokeStyle &);
};

//------------------------------------------------------------------------------

//**********************************************************************************
//    TRasterImagePatternStrokeStyle  declaration
//**********************************************************************************

class DVAPI TRasterImagePatternStrokeStyle : public TColorStyle
{
	static TFilePath m_rootDir;

protected:
	TLevelP m_level;
	std::string m_name;
	double m_space,
		m_rotation;

public:
	TRasterImagePatternStrokeStyle();
	TRasterImagePatternStrokeStyle(const std::string &patternName);

	bool isRegionStyle() const { return false; }
	bool isStrokeStyle() const { return true; }

	int getLevelFrameCount() { return m_level->getFrameCount(); }

	void computeTransformations(std::vector<TAffine> &positions, const TStroke *stroke) const;
	void drawStroke(const TVectorRenderData &rd, const std::vector<TAffine> &positions, const TStroke *stroke) const;
	void drawStroke(TFlash &flash, const TStroke *stroke) const;

	void invalidate(){};

	TColorStyle *clone() const;

	QString getDescription() const { return "TRasterImagePatternStrokeStyle"; }

	bool hasMainColor() const { return false; }
	TPixel32 getMainColor() const { return TPixel32::Black; }
	void setMainColor(const TPixel32 &) {}

	TStrokeProp *makeStrokeProp(const TStroke *stroke);
	TRegionProp *makeRegionProp(const TRegion *)
	{
		assert(false);
		return 0;
	};

	int getTagId() const { return 2000; };
	void getObsoleteTagIds(std::vector<int> &ids) const;

	void loadLevel(const std::string &patternName);
	static TFilePath getRootDir();
	static void setRootDir(const TFilePath &path) { m_rootDir = path + "custom styles"; }

	int getParamCount() const;
	TColorStyle::ParamType getParamType(int index) const;

	QString getParamNames(int index) const;
	void getParamRange(int index, double &min, double &max) const;
	double getParamValue(TColorStyle::double_tag, int index) const;
	void setParamValue(int index, double value);

protected:
	void makeIcon(const TDimension &d);

	void loadData(TInputStreamInterface &);
	void loadData(int oldId, TInputStreamInterface &);

	void saveData(TOutputStreamInterface &) const;

private:
	// Not assignable
	TRasterImagePatternStrokeStyle &operator=(const TRasterImagePatternStrokeStyle &);
};

//**********************************************************************************
//    TVectorImagePatternStrokeStyle  declaration
//**********************************************************************************

class DVAPI TVectorImagePatternStrokeStyle : public TColorStyle
{
	static TFilePath m_rootDir;

protected:
	TLevelP m_level;
	std::string m_name;
	double m_space, m_rotation;

public:
	TVectorImagePatternStrokeStyle();
	TVectorImagePatternStrokeStyle(const std::string &patternName);

	bool isRegionStyle() const { return false; }
	bool isStrokeStyle() const { return true; }

	int getLevelFrameCount() { return m_level->getFrameCount(); }

	void computeTransformations(std::vector<TAffine> &positions, const TStroke *stroke) const;
	void drawStroke(const TVectorRenderData &rd, const std::vector<TAffine> &positions, const TStroke *stroke) const;
	void drawStroke(TFlash &flash, const TStroke *stroke) const;

	void invalidate(){};

	TColorStyle *clone() const;

	QString getDescription() const { return "TVectorImagePatternStrokeStyle"; }

	bool hasMainColor() const { return false; }
	TPixel32 getMainColor() const { return TPixel32::Black; }
	void setMainColor(const TPixel32 &) {}

	TStrokeProp *makeStrokeProp(const TStroke *stroke);
	TRegionProp *makeRegionProp(const TRegion *)
	{
		assert(false);
		return 0;
	};

	int getTagId() const { return 2800; };
	void getObsoleteTagIds(std::vector<int> &ids) const;

	void loadLevel(const std::string &patternName);
	static TFilePath getRootDir();
	static void setRootDir(const TFilePath &path) { m_rootDir = path + "custom styles"; }

	int getParamCount() const;
	TColorStyle::ParamType getParamType(int index) const;

	QString getParamNames(int index) const;
	void getParamRange(int index, double &min, double &max) const;
	double getParamValue(TColorStyle::double_tag, int index) const;
	void setParamValue(int index, double value);

	static void clearGlDisplayLists();

protected:
	void makeIcon(const TDimension &d);

	void loadData(TInputStreamInterface &);
	void loadData(int oldId, TInputStreamInterface &);

	void saveData(TOutputStreamInterface &) const;

private:
	// Not assignable
	TVectorImagePatternStrokeStyle &operator=(const TVectorImagePatternStrokeStyle &);
};

#endif // TSIMPLECOLORSTYLES_H
