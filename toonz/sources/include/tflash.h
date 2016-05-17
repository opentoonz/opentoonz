#pragma once

#ifndef TFLASH_INCLUDED
#define TFLASH_INCLUDED

//#include "tpixel.h"
//#include "tgeometry.h"
#include "timage.h"
#include "tsound.h"
#include "traster.h"
#include "tproperty.h"

#undef DVAPI
#undef DVVAR
#ifdef TVRENDER_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=========================================================

class TQuadratic;
class TSegment;
class TVectorImage;
class TStroke;
class TRegion;

class TColorFunction;

//=========================================================

namespace Tiio
{

class SwfWriterProperties : public TPropertyGroup
{
public:
	TEnumProperty m_lineQuality;
	TBoolProperty m_isCompressed;
	TBoolProperty m_autoplay;
	TBoolProperty m_looping;
	TBoolProperty m_preloader;
	TIntProperty m_jpgQuality;
	TStringProperty m_url;

	SwfWriterProperties();
};
}


class DVAPI TFlash
{
public:
	static const std::wstring ConstantLines;
	static const std::wstring MixedLines;
	static const std::wstring VariableLines;
        
	TFlash(int lx, int ly, int frameCount, int frameRate, TPropertyGroup *properties, bool keepImages = true) {}
	~TFlash() {}
	void enableAlphaChannelForRaster(bool doSaveIt) {}
	void setSoundRate(int soundrate) {}
	void setBackgroundColor(const TPixel &bgColor) {}
	void setThickness(double thickness) {}
	void setFillColor(const TPixel32 &color) {}
	void setLineColor(const TPixel32 &color) {}
	void setTexture(const TRaster32P &texture) {}
	void setFillStyleMatrix(const TAffine &aff) {}
	void setGradientFill(bool isLinear, const TPixel &color1, const TPixel &color2, double smooth) {}
	void drawLine(const TPointD &a, const TPointD &b) {}
	void drawPolygon(std::vector<std::vector<TQuadratic *>> &quads, int clippedShapes = 0) {}
	int drawRaster(TRaster32P r) { return 0; }
	void drawRegion(const TRegion &r, int clippedShapes = 0) {}
	void drawCenterline(const TStroke *s, bool drawAll) {}
	bool drawOutline(TStroke *s) { return false; }
	void drawSegments(const std::vector<TSegment> segmentArray, bool isGradientColor);
	void drawquads(const std::vector<TQuadratic> quadsArray);
	USHORT buildImage(const TImageP img, bool isMask) { return 0; }
	void draw(const TImageP vi, const TColorFunction *cf) {}
	void beginFrame(int frameIndex) {}
	int endFrame(bool isLast, int frameCountLoader, bool lastScene) { return 0; }
	int drawRectangle(const TRectD &rect) { return 0; }
	int drawPolyline(std::vector<TPointD> &poly) { return 0; }
	int drawEllipse(const TPointD &center, double radiusX, double radiusY) { return 0; }
	void drawDot(const TPointD &center, double radius) {}
	void pushMatrix() {}
	void popMatrix() {}
	void multMatrix(const TAffine &aff) {}
	void putSound(TSoundTrackP st, int offset) {}
	void writeMovie(FILE *fp) {}
	void drawPolygon(const std::list<TQuadratic *> &poly, bool isOutline) {}
	std::wstring getLineQuality() { return nullptr; }
	void cleanCachedImages() {}
	void enableMask() {} 
	void disableMask() {}
	void beginMask() {}
	void endMask() {}
	void drawHangedObjects() {}
	void setGlobalScale(const TAffine &aff) {}
};

#endif
