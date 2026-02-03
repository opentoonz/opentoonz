

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

//=========================================================
//!	This class is an interface to Flash File Format (SWF) SDK.
/*!	
		Macromedia Flash File Format (SWF) SDK is an interface to write SWF files.
		It includes a set of C++ classes that mirror the tag structure of SWF. 
		There is a C++ class for each tag that SWF defines.
		There are classes for creating movies, frames, circles, rectangles, text and bitmaps. 
	*/
class DVAPI TFlash
{
	class Imp;
	Imp *m_imp;

public:
	static const wstring ConstantLines;
	static const wstring MixedLines;
	static const wstring VariableLines;

	//enum LineQuality{_ConstantLines=0, _MixedLines, _VariableLines};

	/*
	struct PropertiesForTab
	  {
    LineQuality m_lineQuality;
		bool m_isCompressed;
		bool m_stopAtStart;
		bool m_looping;
		bool m_loader;
		int m_jpgQuality;
		std::wstring m_url;
		PropertiesForTab()
		: m_lineQuality(_ConstantLines), m_isCompressed(true), m_stopAtStart(false), m_looping(true)
		, m_loader(false), m_jpgQuality(90), m_url(std::wstring()) {}

    PropertiesForTab(LineQuality q, 
		           bool isCompr, 
							 bool stopAtStart, 
							 bool looping, 
							 bool loader, 
							 int m_jpgQ, 
							 std::wstring url)
		: m_lineQuality(q), m_isCompressed(isCompr), m_stopAtStart(stopAtStart), m_looping(looping)
		, m_loader(loader), m_jpgQuality(m_jpgQ), m_url(url) {if (m_jpgQ>99) m_jpgQ=99; else if (m_jpgQ<1) m_jpgQ=1;}
		};

	void setProperties(const TFlash::PropertiesForTab& prop);*/

	/*!
		This constructor initialize internal main environment as follows:
	 \li	frame index is set to -1,
	 \li line thickness is set to 0,
	 \li line color is set to black,
	 \li strokes, regions and polylines are set to 0,
	 \li sound environment is reset,
	 \li palette pointer is reset,
	 \li a new empty affine transformation is created.

	The object is associate with a drawing style wich have in general two colors, thickness of the stroke, 
	smoothness of the gradient if any and type of the style that can be a texture, a line ,a solid style,
	and radial or linear gradient. 



		\param lx x-measure of the scene where the measure units are twips (a twips is an absolute length measure 
		of about 1/1440 inch ). It is used passing it the camera width view.
		\param ly y-measure of the scene where the measure units are twips. 
			It is used passing the camera height view.
		\param frameCount number of frames in the scene.
		\param frameRate number of frame per second.
		\param properties vector of swf properties as line and jpeg quality; 
		       compression, looping, autoplay and preloading capabilities.   


	*/
	TFlash(int lx, int ly, int frameCount, int frameRate, TPropertyGroup *properties, bool keepImages = true);
	/*!
		Deletes \p this object.
	*/
	~TFlash();
	/*!
		Sets scene' background to \p bgColor. 
	*/

	//if set to false, it does not save the alpha channel; default is on true
	void enableAlphaChannelForRaster(bool doSaveIt);

	//default soundrate is at 5512
	void setSoundRate(int soundrate);

	// Nota: per ora va chiamata una sola volta e prima di ogni altra cosa
	void setBackgroundColor(const TPixel &bgColor);
	//void setCameraDpi(double dpix, double dpiy, double inchFactor);
	//void getCameraDpi(double &dpix, double &dpiy);
	/*!
		Sets the thickness pf the stroke for painting the object.
	*/
	void setThickness(double thickness);
	/*!
		Sets the fill color of the drawing object.
		Sets the type of style to Solid.
	*/
	void setFillColor(const TPixel32 &color);
	/*!
		Sets the stroke color used to paint the object. 
	*/
	void setLineColor(const TPixel32 &color);
	/*!
		Sets the style to Texture.
		Sets texture of the object.
	*/
	void setTexture(const TRaster32P &texture);
	/*!
		Sets the affine transformation of the filling style with texture. 
	*/
	void setFillStyleMatrix(const TAffine &aff);
	/*!
		Sets parameters for filling with a gradient style.
	*/
	void setGradientFill(bool isLinear, const TPixel &color1, const TPixel &color2, double smooth);
	//void setProperties(TPropertyGroup* properties);
	/*!
		Draws a segment line.
	*/
	void drawLine(const TPointD &a, const TPointD &b);
	/*!
		Draws a polygon given the vertices.
	*/
	void drawPolygon(vector<vector<TQuadratic *>> &quads, int clippedShapes = 0); //first polyline outside, other are holes
	/*!
		Draws a raster image.
	*/
	int drawRaster(TRaster32P r);
	/*!
		Draws a closed region. 
	*/
	void drawRegion(const TRegion &r, int clippedShapes = 0);
	/*!
		Draws a line given a stroke style.
	*/
	void drawCenterline(const TStroke *s, bool drawAll);
	/*!
		Draws the outline of the stroke \p s.
	*/
	bool drawOutline(TStroke *s);
	/*!
		Draws the vector of segment lines \p segmentArray.
	*/
	void drawSegments(const vector<TSegment> segmentArray, bool isGradientColor);
	/*!
		Draws an array of boxes.
	*/
	void drawquads(const vector<TQuadratic> quadsArray);
	/*!this function puts objects in an image in current sprite;
	useful for image patterns 
	*/
	USHORT buildImage(const TImageP img, bool isMask);
	/*!
		Adds the image \p vi to the current data frame.
	*/
	void draw(const TImageP vi, const TColorFunction *cf);
	/*!
		Initialize flash frame data and egins with frame \e frameIndex. 
	*/
	void beginFrame(int frameIndex);
	/*!
		Ends the flash frame data. 
	*/
	int endFrame(bool isLast, int frameCountLoader, bool lastScene);
	/*!
		Draws a rectangle.
	*/
	int drawRectangle(const TRectD &rect);
	/*!
		Draws a polyline.
	*/
	int drawPolyline(vector<TPointD> &poly);
	/*!
		Draws an ellipse.
	*/
	int drawEllipse(const TPointD &center, double radiusX, double radiusY);
	/*!
		Draws a point.
	*/
	void drawDot(const TPointD &center, double radius);
	/*!
		Puts the current affine matrix on the stack of the affine tansformations.
		This stacks contains the transfomation sequence.  
	*/
	void pushMatrix();
	/*!
		Gets the matrix transformation from the stack and puts it 
		in the current affine matrix. 
	*/
	void popMatrix();
	/*!
		Multiplies current affine matrix by \p aff.
	*/
	void multMatrix(const TAffine &aff);
	/*!
		Puts audio stream to the scene beginning from the frame \p offset.
	*/
	void putSound(TSoundTrackP st, int offset);
	/*!
		Write the flash movie to file.
	*/
	void writeMovie(FILE *fp);

	void drawPolygon(const list<TQuadratic *> &poly, bool isOutline); // tolgo????
																	  /*!
		Returns the quality of the line, i.e. 
		"Low: Constant Thickness", "Medium: Mixed Thickness", "High: Variable Thickness".
	*/
	wstring getLineQuality();
	//void addPauseAtStart();
	/*!
		Clears the tables of images used in the drawing.
	*/
	void cleanCachedImages();
	/*!
		Enables the mask layer.
	*/
	void enableMask();
	/*!
		Disables the mask layer.
	*/
	void disableMask();
	/*!
		Create a new vector image with new palette, used as a mask layer.
	*/
	void beginMask();
	/*!
		Puts current mask layer to the frame data.
	*/
	void endMask();
	/*!
		Draws appended polylines and clears the tables of temporary images. 
	*/
	void drawHangedObjects();
	/*!
		Sets a global scale factor.
	*/
	void setGlobalScale(const TAffine &aff);

private:
	// not implemented
	TFlash();
	TFlash(const TFlash &);
	TFlash &operator=(const TFlash &);
};

#endif
