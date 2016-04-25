#pragma once

#ifndef T_VECTOR_BRUSH_STYLE_H
#define T_VECTOR_BRUSH_STYLE_H

#include "tfilepath.h"
#include "tvectorimage.h"
#include "tcolorstyles.h"

#undef DVAPI
#undef DVVAR

#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************
//    Vector Brush Style  declaration
//**********************************************************************

class DVAPI TVectorBrushStyle : public TColorStyle
{
	static TFilePath m_rootDir;

	std::string m_brushName;
	TVectorImageP m_brush;
	int m_colorCount;

public:
	TVectorBrushStyle();
	TVectorBrushStyle(const std::string &brushName, TVectorImageP vi = TVectorImageP());
	~TVectorBrushStyle();

	TColorStyle *clone() const;
	QString getDescription() const;

	static TFilePath getRootDir() { return m_rootDir; }
	static void setRootDir(const TFilePath &path) { m_rootDir = path + "vector brushes"; }

	void loadBrush(const std::string &brushName);

	int getTagId() const { return 3000; }

	TVectorImageP getImage() const { return m_brush; }

	bool isRegionStyle() const { return false; }
	bool isStrokeStyle() const { return true; }

	TStrokeProp *makeStrokeProp(const TStroke *stroke);
	TRegionProp *makeRegionProp(const TRegion *region)
	{
		assert(false);
		return 0;
	}

	bool hasMainColor() const { return true; }
	TPixel32 getMainColor() const;
	void setMainColor(const TPixel32 &color);

	int getColorParamCount() const;
	TPixel32 getColorParamValue(int index) const;
	void setColorParamValue(int index, const TPixel32 &color);

protected:
	void loadData(TInputStreamInterface &);
	void saveData(TOutputStreamInterface &) const;

private:
	int getColorStyleId(int index) const;
};

#endif //T_VECTOR_BRUSH_STYLE_H
