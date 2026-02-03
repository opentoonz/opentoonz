

#include "tflash.h"
//#include "tstroke.h"
#include "tcurves.h"
#include "tregion.h"
#include "tstrokeprop.h"
#include "tregionprop.h"

#include "tpalette.h"
#include "tvectorimage.h"
#include "tmachine.h"
#include "trasterimage.h"
#include "tsimplecolorstyles.h"
#include "tcolorfunctions.h"
#include "F3SDK.h"
#include "FFixed.h"
#include "tsop.h"
#include "tropcm.h"
#include "tsweepboundary.h"
#include "tiio_jpg_util.h"
#include "zlib.h"
//#include "trop.h"
#include "ttoonzimage.h"
#include "tconvert.h"
#include "timage_io.h"
#include "tsystem.h"
#include <stack>
#include <fstream>

#if !defined(TNZ_LITTLE_ENDIAN)
TNZ_LITTLE_ENDIAN undefined !!
#endif

	int Tw = 0;

bool areTwEqual(double x, double y)
{
	assert(Tw != 0);

	return (int)(Tw * x) == (int)(Tw * y);
}

bool areTwEqual(TPointD p0, TPointD p1)
{
	assert(Tw != 0);

	return areTwEqual(p0.x, p1.x) && areTwEqual(p0.y, p1.y);
}
//-------------------------------------------------------------------

const wstring TFlash::ConstantLines = L"Low: Constant Thickness";
const wstring TFlash::MixedLines = L"Medium: Mixed Thickness";
const wstring TFlash::VariableLines = L"High: Variable Thickness";

Tiio::SwfWriterProperties::SwfWriterProperties()
	: m_lineQuality("Curve Quality"), m_isCompressed("File Compression", true), m_autoplay("Autoplay", true), m_looping("Looping", true), m_jpgQuality("Jpg Quality", 0, 100, 90), m_url("URL", wstring()), m_preloader("Insert Preloader", false)
{
	m_lineQuality.addValue(TFlash::MixedLines);
	m_lineQuality.addValue(TFlash::ConstantLines);
	m_lineQuality.addValue(TFlash::VariableLines);

	bind(m_lineQuality);
	bind(m_isCompressed);
	bind(m_autoplay);
	bind(m_looping);
	bind(m_jpgQuality);
	bind(m_url);
	bind(m_preloader);

	TEnumProperty::Range range = m_lineQuality.getRange();
}

//-------------------------------------------------------------------

enum PolyType { None,
				Centerline,
				Solid,
				Texture,
				LinearGradient,
				RadialGradient };

class PolyStyle
{
public:
	PolyType m_type;
	TPixel32 m_color1;	//only if type!=Texture
	TPixel32 m_color2;	//only if type==LinearGradient || type==RadialGradient
	double m_smooth;	  //only if type==RadialGradient
	double m_thickness;   //only if type==Centerline
	TAffine m_matrix;	 //only if type==Texture
	TRaster32P m_texture; //only if type==Texture
	//bool m_isRegion;            //only if type!=Centerline
	//bool m_isHole;              //only if type!=Centerline && m_isRegion==true
	PolyStyle() : m_type(None), m_color1(), m_color2(), m_smooth(0), m_thickness(0), m_matrix(), m_texture() /*, m_isRegion(false), m_isHole(false)*/ {}
	bool operator==(const PolyStyle &p) const;
	bool operator<(const PolyStyle &p) const;
};

class FlashPolyline
{
public:
	UINT m_depth;
	bool m_skip;
	bool m_toBeDeleted;
	bool m_isPoint;
	vector<TQuadratic *> m_quads;
	PolyStyle m_fillStyle1;
	PolyStyle m_fillStyle2;
	PolyStyle m_lineStyle;
	//PolyStyle m_bgStyle;
	FlashPolyline() : m_depth(0), m_skip(false), m_toBeDeleted(false), m_isPoint(false), m_fillStyle1(), m_fillStyle2(), m_lineStyle() {}
	bool operator<(const FlashPolyline &p) const { return m_depth < p.m_depth; }
};

class biPoint
{
public:
	TPointD p0, p1;

	biPoint(TPointD _p0, TPointD _p1) : p0(_p0), p1(_p1) {}
	biPoint() {}
	bool operator<(const biPoint &b) const
	{
		biPoint aux;
		aux.p0.x = areTwEqual(p0.x, b.p0.x) ? p0.x : b.p0.x;
		aux.p0.y = areTwEqual(p0.y, b.p0.y) ? p0.y : b.p0.y;
		aux.p1.x = areTwEqual(p1.x, b.p1.x) ? p1.x : b.p1.x;
		aux.p1.y = areTwEqual(p1.y, b.p1.y) ? p1.y : b.p1.y;

		return (p0.x == aux.p0.x) ? ((p0.y == aux.p0.y) ? ((p1.x == aux.p1.x) ? (p1.y < aux.p1.y) : (p1.x < aux.p1.x)) : (p0.y < aux.p0.y)) : p0.x < aux.p0.x;
	}
	void revert() { tswap(p0, p1); }
};

class wChunk
{
public:
	double w0, w1;
	wChunk(double _w0, double _w1) : w0(_w0), w1(_w1) {}
	bool operator<(const wChunk &b) const { return (w1 < b.w0); }
};

//-------------------------------------------------------------------

const int c_soundRate = 5512; //  5512; //11025
const int c_soundBps = 16;
const bool c_soundIsSigned = false;
const int c_soundChannelNum = 1;
const int c_soundCompression = 3; //per compatibilita' con MAC!!!

//-------------------------------------------------------------------

class FlashImageData
{
public:
	FlashImageData(TAffine aff, TImageP img, const TColorFunction *cf, bool isMask, bool isMasked)
		: m_aff(aff), m_img(img), m_cf(cf), m_isMask(isMask), m_isMasked(isMasked)
	{
		assert(!isMask || !isMasked);
	}
	TAffine m_aff;
	const TColorFunction *m_cf;
	bool m_isMask, m_isMasked;
	TImageP m_img;
};

class FlashColorStyle
{
public:
	TPixel m_color;
	double m_thickness;
	U32 m_id;
	FlashColorStyle(TPixel color, double thickness, U32 id)
		: m_color(color), m_thickness(thickness), m_id(id) {}
};

class TFlash::Imp
{
public:
	//double m_totMem;
	bool m_supportAlpha;
	int m_tw;
	UCHAR m_version;
	SCOORD m_sCoord1;
	bool m_loaderAdded;
	TAffine m_globalScale;
	//typedef triple FlashImageData;
	typedef vector<FlashImageData> FrameData;
	FObjCollection m_tags;
	FDTSprite *m_currSprite;
	int m_currDepth;
	int m_frameRate;
	int m_currFrameIndex;
	int m_lx, m_ly;
	//ouble cameradpix, cameradpiy, inchFactor;
	const TPalette *m_currPalette;
	int m_soundRate;
	Tiio::SwfWriterProperties m_properties;

	bool m_maskEnabled;
	bool m_isMask;

	bool m_keepImages;

	std::list<FlashPolyline> m_polylinesArray;
	//std::set<Polyline> m_currentEdgeArray;

	TPixel32 m_lineColor;
	double m_thickness;

	PolyStyle m_polyData;
	//vector<PolyStyle> m_currentBgStyle;
	int m_regionDepth;
	int m_strokeCount;
	/*TPixel32 m_fillColor;
  TAffine m_fillMatrix;
  TRaster32P m_texture;
  GradientType m_gradientType;
  TPixel32 m_gradientColor1, m_gradientColor2;*/
	//std::ofstream m_of;

	TAffine m_affine;
	vector<TAffine> m_matrixStack;
	map<const TImage *, USHORT> m_imagesMap;
	map<const TImage *, double> m_imagesScaleMap;
	map<TEdge, FlashPolyline *> m_edgeMap;
	map<const void *, USHORT> m_texturesMap;
	map<biPoint, FlashPolyline *> m_autocloseMap;
	map<const TStroke *, std::set<wChunk>> m_strokeMap;

	vector<TStroke *> m_outlines;
	TPixel m_currStrokeColor;
	//std::set<TPixel> m_outlineColors;

	FrameData *m_frameData;
	FrameData *m_oldFrameData;
	//bool m_notClipped;
	vector<TSoundTrackP> m_sound;
	int m_soundSize;
	vector<UCHAR *> m_soundBuffer;
	int m_soundOffset;
	TVectorImageP m_currMask;

	vector<vector<UCHAR> *> m_toBeDeleted;
	vector<TQuadratic *> m_quadsToBeDeleted;
	vector<TStroke *> m_strokesToBeDeleted;
	void drawPolygon(const vector<TQuadratic *> &poly, bool isOutline);
	int setFill(FDTDefineShape3 *shape);
	inline FMatrix *affine2Matrix(const TAffine &aff);
	void drawHangedObjects();
	void setStyles(const list<FlashPolyline> &polylines,
				   vector<U32> &lineStyleID, vector<U32> &fillStyle1ID, vector<U32> &fillStyle2ID,
				   FDTDefineShape3 *polygon);

	U32 findStyle(const PolyStyle &p, std::map<PolyStyle, U32> &idMap, FDTDefineShape3 *polygon);
	void addEdge(const TEdge &e, TPointD &p0, TPointD &p1);
	void addNewEdge(const TEdge &e);
	//void closeRegion(int numEdges);

	void drawHangedOutlines();

	void addAutoclose(biPoint &bp, int edgeIndex);

	inline TPoint toTwips(const TPointD &p) { return TPoint((int)(m_tw * p.x), (int)(m_tw * (-p.y))); }

	~Imp()
	{
		clearPointerContainer(m_toBeDeleted);
		clearPointerContainer(m_quadsToBeDeleted);
		clearPointerContainer(m_strokesToBeDeleted);
		if (m_oldFrameData)
			delete m_oldFrameData;

		while (!m_soundBuffer.empty()) {
			delete[] * m_soundBuffer.rbegin();
			m_soundBuffer.pop_back();
		}
	}

	//===================================================================

	/*
  l'inizializzazione di m_currDepth e' 3 poiche' si riservanola depth 1
  per la clipcamera e la depth 2 per l'eventuale bottone (non visibile)
  del play non automatico
*/
	Imp(int lx, int ly, int frameCount, int frameRate, TPropertyGroup *properties, bool keepImages)
		: m_version(4), m_tags(), m_currSprite(0), m_currDepth(3), m_frameRate(frameRate), m_currFrameIndex(-1), m_lx(lx), m_ly(ly), m_currPalette(0), m_maskEnabled(false), m_isMask(false), m_polylinesArray(), m_lineColor(TPixel32::Black), m_thickness(0), m_polyData(), m_regionDepth(0), m_strokeCount(0), m_affine(), m_matrixStack(), m_imagesMap(), m_imagesScaleMap(), m_edgeMap(), m_texturesMap(), m_autocloseMap(), m_strokeMap(), m_outlines(), m_currStrokeColor(0, 0, 0, 0), m_frameData(0), m_oldFrameData(0)
		  //, m_notClipped(true)
		  ,
		  m_sound(), m_soundSize(0), m_soundBuffer(), m_currMask(), m_toBeDeleted(), m_quadsToBeDeleted(), m_strokesToBeDeleted(), m_soundOffset(0), m_loaderAdded(false), m_globalScale(), m_keepImages(keepImages), m_supportAlpha(true), m_soundRate(c_soundRate)
	//, m_totMem(0)

	//, m_of("c:\\temp\\boh.txt")

	{
		m_tags.AddFObj(new FCTFrameLabel(new FString("DigitalVideoRm")));

		if (properties)
			m_properties.setProperties(properties);
		//m_currentBgStyle.push_back(PolyStyle());

		m_tw = 16384 / tmax(m_lx, m_ly);
		if (m_tw > 20)
			m_tw = 20;
		Tw = m_tw;
		m_sCoord1 = m_tw;
		//addCameraClip();
		if (!m_properties.m_autoplay.getValue() && !m_properties.m_preloader.getValue())
			addPause();
	}

	void drawSubregions(TFlash *tf, const TRegion *r, const TPalette *palette);
	void doDrawPolygon(list<FlashPolyline> &polylines, int clippedShapes = 0);
	int drawSegments(const vector<TSegment> segmentArray, bool isGradientColor);
	int drawquads(const vector<TQuadratic> quadsArray);
	int drawRectangle(const TRectD &rect);
	int drawPolyline(vector<TPointD> &poly);
	int drawEllipse(const TPointD &center, double radiusX, double radiusY);
	void drawDot(const TPointD &center, double radius);

	void buildRegion(TFlash *tf, const TVectorImageP &vi, int regionIndex);
	void buildStroke(TFlash *tf, const TVectorImageP &vi, int strokeIndex);

	//void addCameraClip(int index);
	void writeFrame(TFlash *tf, bool isLast, int frameCountLoader, bool lastScene);
	U16 getTexture(const PolyStyle &p, int &lx, int &ly);

	void addSoundToFrame(bool isLast);

	void addActionStop();
	void addLoader();
	void addSkipLoader(int jumpToFrame);
	void addPause();
	void beginMask();
	void endMask();
	void addUrlLink(string url);
	USHORT buildImage(const TImageP vi, TFlash *tf, double &scaleFactor, bool isMask);
	USHORT buildVectorImage(const TVectorImageP &img, TFlash *tf, double &scaleFactor, bool isMask);
	USHORT buildRasterImage(const TImageP rimg, TFlash *tf);
	bool drawOutline(TStroke *s, bool separeDifferentColors = true);
	inline void addEdgeStraightToShape(FDTDefineShape3 *shape, int x, int y);
	inline void addEdgeStraightToShape(FDTDefineShape3 *shape, const TPoint &p);
};

//===================================================================

void TFlash::setSoundRate(int soundrate)
{
	m_imp->m_soundRate = soundrate;
}

//===================================================================

void TFlash::enableAlphaChannelForRaster(bool supportAlpha)
{
	m_imp->m_supportAlpha = supportAlpha;
}

//===================================================================
namespace
{
inline void addShape(FDTDefineShape3 *polygon, bool newStyle, bool lStyle,
					 bool fillStyle1, bool fillStyle0, bool move, int x, int y,
					 int style0, int style1, int lineStyle)
{
	polygon->AddShapeRec(new FShapeRecChange(newStyle, lStyle, fillStyle1, fillStyle0, move,
											 x, y, style0, style1, lineStyle, 0, 0));
}

inline void addShape(FDTDefineShape3 *polygon, bool newStyle, bool lStyle,
					 bool fillStyle1, bool fillStyle0, bool move, TPoint *p,
					 int style0, int style1, int lineStyle)
{
	polygon->AddShapeRec(new FShapeRecChange(newStyle, lStyle, fillStyle1, fillStyle0, move,
											 p ? p->x : 0, p ? p->y : 0, style0, style1, lineStyle, 0, 0));
}
}

//===================================================================

inline void TFlash::Imp::addEdgeStraightToShape(FDTDefineShape3 *shape, int x, int y)
{
	if (x == 0 && y == 0)
		return;

	//m_of<< "ADD STRAIGHT LINE: "<<x<<", "<<y<<std::endl;

	if (abs(x) > 65535 || abs(y) > 65535) //flash non sa scrivere segmenti piu' lunghi di cosi', spezzo
	{
		shape->AddShapeRec(new FShapeRecEdgeStraight((x + 1) / 2, (y + 1) / 2));
		shape->AddShapeRec(new FShapeRecEdgeStraight(x / 2, y / 2));
	} else
		shape->AddShapeRec(new FShapeRecEdgeStraight(x, y));
}

inline void TFlash::Imp::addEdgeStraightToShape(FDTDefineShape3 *shape, const TPoint &p)
{
	addEdgeStraightToShape(shape, p.x, p.y);
}

//-------------------------------------------------------------------

double computeAverageThickness(const TStroke *s)
{
	int count = s->getControlPointCount();
	double resThick = 0;
	int i;

	for (i = 0; i < s->getControlPointCount(); i++) {
		double thick = s->getControlPoint(i).thick;
		if (i >= 2 && i < s->getControlPointCount() - 2)
			resThick += thick;
	}
	if (count < 6)
		return s->getControlPoint(count / 2 + 1).thick;

	return resThick / (s->getControlPointCount() - 4);
}

//-------------------------------------------------------------------

inline FMatrix *TFlash::Imp::affine2Matrix(const TAffine &aff)
{
	if (aff != TAffine()) {
		bool hasA11OrA22, hasA12OrA21;

		hasA11OrA22 = hasA12OrA21 = (aff.a12 != 0 || aff.a21 != 0);
		if (!hasA12OrA21)
			hasA11OrA22 = !areAlmostEqual(aff.det(), 1.0, 1e-3);
		return new FMatrix(hasA11OrA22, FloatToFixed(aff.a11), FloatToFixed(aff.a22),
						   hasA12OrA21, FloatToFixed(-aff.a21), FloatToFixed(-aff.a12),
						   (TINT32)tround(aff.a13 * m_tw), -(TINT32)tround(aff.a23 * m_tw));
	} else
		return 0;
}

//-------------------------------------------------------------------

int TFlash::Imp::drawSegments(const vector<TSegment> segmentArray, bool isGradientColor)
{
	int i;
	assert(m_currSprite);

	if (segmentArray.empty())
		return 0;

	TPointD firstPoint = segmentArray[0].getP0();

	FDTDefineShape3 *polygon = new FDTDefineShape3();

	U32 lineStyleID1, lineStyleID2, lineStyleID4;
	lineStyleID1 = polygon->AddLineStyle((int)m_thickness * m_tw, new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, m_lineColor.m));
	if (isGradientColor) {
		lineStyleID2 = polygon->AddLineStyle((int)m_thickness * m_tw, new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, int(0.50 * (m_lineColor.m))));
		//U32 lineStyleID3  = polygon->AddLineStyle(m_tw, new FColor(m_color.r, m_color.g, m_color.b, int(0.25*(m_color.m))));
		lineStyleID4 = polygon->AddLineStyle((int)m_thickness * m_tw, new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, int(0.125 * (m_lineColor.m))));
	}
	polygon->FinishStyleArrays();

	TRectD box;
	box.x0 = firstPoint.x;
	box.x1 = firstPoint.x;
	box.y0 = firstPoint.y;
	box.y1 = firstPoint.y;

	for (i = 0; i < (int)segmentArray.size(); i++) {
		box += segmentArray[i].getBBox();
		TPoint p0 = convert(segmentArray[i].getP0());
		TPoint dp = convert(segmentArray[i].getP1()) - p0;
		TPoint p((int)(m_tw * p0.x), (int)(-m_tw * p0.y));
		addShape(polygon, false, isGradientColor || (i == 0), false, false, true, &p, 0,
				 0, (isGradientColor || (i == 0)) ? lineStyleID1 : 0);

		if (isGradientColor)
			addEdgeStraightToShape(polygon, 5 * dp.x, -5 * dp.y);
		else {
			addEdgeStraightToShape(polygon, (int)(m_tw * dp.x), (int)(m_tw * -dp.y));
			continue;
		}

		addShape(polygon, false, true, false, false, false, 0, 0, 0, lineStyleID2);

		addEdgeStraightToShape(polygon, 5 * dp.x, -5 * dp.y);

		addShape(polygon, false, true, false, false, false, 0, 0, 0, lineStyleID4);

		addEdgeStraightToShape(polygon, 5 * dp.x, -5 * dp.y);
	}

	polygon->AddShapeRec(new FShapeRecEnd());
	polygon->setBounds(new FRect((int)(m_tw * box.x0), -(int)(m_tw * box.y0), (int)(m_tw * box.x1), -(int)(m_tw * box.y1)));

	m_tags.AddFObj(polygon);

	FCTPlaceObject2 *placePolygon = new FCTPlaceObject2(false, // ~ _hasClipDepth
														false, true, false,
														m_currDepth++, polygon->ID(), affine2Matrix(m_affine), 0, 0, 0, 0 /**/);

	m_currSprite->AddFObj(placePolygon);
	return polygon->ID();
}

//-------------------------------------------------------------------

int TFlash::Imp::drawquads(const vector<TQuadratic> quadsArray)
{
	int i;
	assert(m_currSprite);
	if (quadsArray.empty())
		return 0;

	TPointD firstPoint = quadsArray[0].getP0();

	TRectD box;
	box.x0 = firstPoint.x;
	box.x1 = firstPoint.x;
	box.y0 = firstPoint.y;
	box.y1 = firstPoint.y;

	for (i = 0; i < (int)quadsArray.size(); i++)
		box += quadsArray[i].getBBox();

	FDTDefineShape3 *polygon = new FDTDefineShape3(new FRect((int)(m_tw * box.x0), -(int)(m_tw * box.y0), (int)(m_tw * box.x1), -(int)(m_tw * box.y1)));

	U32 lineStyleID;
	lineStyleID = polygon->AddLineStyle(m_tw, new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, m_lineColor.m));

	polygon->FinishStyleArrays();

	for (i = 0; i < (int)quadsArray.size(); i++) {
		TPoint p0 = toTwips(quadsArray[i].getP0());

		addShape(polygon, false, i == 0, false, false, true, &p0, 0, 0, (i == 0) ? lineStyleID : 0);

		TPoint dp1 = convert((quadsArray[i].getP1() - quadsArray[i].getP0()));
		TPoint dp2 = convert((quadsArray[i].getP2() - quadsArray[i].getP1()));

		if ((dp1 == TPoint())) {
			if (dp2 == TPoint())
				continue;
			addEdgeStraightToShape(polygon, (int)(m_tw * dp2.x), (int)(m_tw * -dp2.y));
		} else if ((dp2 == TPoint()))
			addEdgeStraightToShape(polygon, (int)(m_tw * dp1.x), (int)(m_tw * -dp1.y));
		else
			polygon->AddShapeRec(new FShapeRecEdgeCurved((int)(m_tw * dp1.x), (int)(m_tw * -dp1.y), (int)(m_tw * dp2.x), (int)(m_tw * -dp2.y)));
	}

	polygon->AddShapeRec(new FShapeRecEnd());

	m_tags.AddFObj(polygon);

	FCTPlaceObject2 *placePolygon = new FCTPlaceObject2(false, // ~ _hasClipDepth
														false, true, false,
														m_currDepth++, polygon->ID(), affine2Matrix(m_affine), 0, 0, 0, 0 /**/);

	m_currSprite->AddFObj(placePolygon);
	return polygon->ID();
}

//-------------------------------------------------------------------

void putquads(const TStroke *s, double w0, double w1, vector<TQuadratic *> &quads)
{
	int chunkIndex0, chunkIndex1, i;
	double dummy;
	bool ret;

	ret = s->getChunkAndT(w0, chunkIndex0, dummy);
	assert(!ret);
	ret = s->getChunkAndT(w1, chunkIndex1, dummy);
	assert(!ret);
	assert(chunkIndex0 <= chunkIndex1);

	for (i = chunkIndex0; i <= chunkIndex1; i++)
		quads.push_back((TQuadratic *)s->getChunk(i));
}

//-------------------------------------------------------------------

void computeOutlineBoundary(vector<TStroke *> &outlines, list<FlashPolyline> &polylinesArray, const TPixel &color)
{
	UINT size = polylinesArray.size();

	vector<vector<TQuadratic *>> quads;
	computeSweepBoundary(outlines, quads);

	outlines.clear();
	std::list<FlashPolyline>::iterator it = polylinesArray.begin();
	std::advance(it, size);
	for (int i = 0; i < (int)quads.size(); i++) {
		vector<TQuadratic *> &q = quads[i];

		polylinesArray.push_back(FlashPolyline());
		polylinesArray.back().m_quads = quads[i];
		polylinesArray.back().m_toBeDeleted = true;
		polylinesArray.back().m_fillStyle1.m_type = Solid;
		polylinesArray.back().m_fillStyle1.m_color1 = color;
	}
}

//-------------------------------------------------------------------

void TFlash::Imp::drawHangedObjects()
{
	//int i=0;

	if (!m_outlines.empty())
		computeOutlineBoundary(m_outlines, m_polylinesArray, m_currStrokeColor);

	m_currStrokeColor = TPixel::Transparent;
	//m_outlineColors.clear();

	if (!m_polylinesArray.empty()) {
		doDrawPolygon(m_polylinesArray, false);

		std::list<FlashPolyline>::iterator it;
		for (it = m_polylinesArray.begin(); it != m_polylinesArray.end(); ++it)
			if (it->m_toBeDeleted)
				clearPointerContainer(it->m_quads);
		m_polylinesArray.clear();
		m_edgeMap.clear();
		m_autocloseMap.clear();
		m_strokeMap.clear();
	}

	clearPointerContainer(m_strokesToBeDeleted);
}

//-------------------------------------------------------------------

int TFlash::Imp::drawPolyline(vector<TPointD> &poly)
{
	TRect box(1000, 1000, -1000, -1000);

	int i;

	FDTDefineShape3 *polyLine = new FDTDefineShape3();
	U16 id = polyLine->ID();

	U32 fillID = setFill(polyLine);

	U32 lineStyleID = 0;

	if (m_thickness > 0)
		lineStyleID = polyLine->AddLineStyle((int)(m_thickness * m_tw), new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, m_lineColor.m));

	polyLine->FinishStyleArrays();

	TPointD currP = TPointD(m_tw * poly[0].x, -m_tw * poly[0].y);
	TPoint oldIntCurrP, intCurrP = convert(currP);

	addShape(polyLine, false, lineStyleID != 0, false, fillID != 0, true,
			 &intCurrP, fillID, 0, lineStyleID);

	poly.push_back(poly.front()); //con le approssimazioni,le poly chiuse potrebbero non esserlo
	for (i = 0; i < (int)poly.size() - 1; i++) {
		currP += TPointD(m_tw * (+poly[i + 1].x - poly[i].x), m_tw * (-poly[i + 1].y + poly[i].y));
		oldIntCurrP = intCurrP;
		intCurrP = convert(currP);
		if (intCurrP != oldIntCurrP)
			addEdgeStraightToShape(polyLine, intCurrP.x - oldIntCurrP.x, intCurrP.y - oldIntCurrP.y);

		if (intCurrP.x > box.x1)
			box.x1 = intCurrP.x;
		if (intCurrP.x < box.x0)
			box.x0 = intCurrP.x;
		if (intCurrP.y > box.y1)
			box.y1 = intCurrP.y;
		if (intCurrP.y < box.y0)
			box.y0 = intCurrP.y;
	}
	poly.pop_back(); //ritolgo, per non alterare il vettore in ingresso alla funzione

	polyLine->AddShapeRec(new FShapeRecEnd());
	polyLine->setBounds(new FRect(box.x0, box.y0, box.x1, box.y1));
	m_tags.AddFObj(polyLine);

	FCTPlaceObject2 *placePoly = new FCTPlaceObject2(false, false, true, false,
													 m_currDepth++, id, affine2Matrix(m_affine), 0, 0, 0, 0);

	m_currSprite->AddFObj(placePoly);
	return id;
}

//-------------------------------------------------------------------

int TFlash::Imp::drawRectangle(const TRectD &rect)
{
	TRect box = convert(TRectD(rect.x0 * m_tw, rect.y0 * m_tw, rect.x1 * m_tw, rect.y1 * m_tw));

	FDTDefineShape3 *rectangle = new FDTDefineShape3(new FRect(box.x0, box.y0, box.x1, box.y1));
	U16 id = rectangle->ID();

	U32 fillID = setFill(rectangle);

	U32 lineStyleID = 0;

	if (m_thickness > 0)
		lineStyleID = rectangle->AddLineStyle((int)(m_thickness * m_tw), new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, m_lineColor.m));

	rectangle->FinishStyleArrays();

	addShape(rectangle, false, true, false, fillID != 0, true,
			 box.x0, -box.y0, fillID, 0, lineStyleID);

	addEdgeStraightToShape(rectangle, box.x1 - box.x0, 0);
	addEdgeStraightToShape(rectangle, 0, -box.y1 + box.y0);
	addEdgeStraightToShape(rectangle, -box.x1 + box.x0, 0);
	addEdgeStraightToShape(rectangle, 0, box.y1 - box.y0);
	rectangle->AddShapeRec(new FShapeRecEnd());

	m_tags.AddFObj(rectangle);

	FCTPlaceObject2 *placeRectangle = new FCTPlaceObject2(false, false, true, false,
														  m_currDepth++, id, affine2Matrix(m_affine), 0, 0, 0, 0);

	m_currSprite->AddFObj(placeRectangle);
	return id;
}

//-------------------------------------------------------------------

//-------------------------------------------------------------------

U16 TFlash::Imp::getTexture(const PolyStyle &p, int &lx, int &ly)
{
	assert(p.m_type == Texture);
	assert(p.m_texture->getPixelSize() == 4);
	lx = p.m_texture->getLx(), ly = p.m_texture->getLy();

	std::map<const void *, USHORT>::iterator it = m_texturesMap.find(p.m_texture.getPointer());
	if (it != m_texturesMap.end())
		return (*it).second;

	assert(p.m_texture->getWrap() == lx);

	std::vector<UCHAR> *buffer = new std::vector<UCHAR>();
	m_toBeDeleted.push_back(buffer);
	Tiio::createJpg(*buffer, p.m_texture, m_properties.m_jpgQuality.getValue());
	FDTDefineBitsJPEG2 *bitmap = new FDTDefineBitsJPEG2((UCHAR *)&(*buffer)[0], buffer->size());

	m_tags.AddFObj(bitmap);
	m_texturesMap[p.m_texture.getPointer()] = bitmap->ID();
	//delete bufout;
	return bitmap->ID();
}

//-------------------------------------------------------------------

void TFlash::Imp::drawDot(const TPointD &center, double radius)
{
	FlashPolyline quads;
	quads.m_lineStyle.m_type = Centerline;
	quads.m_lineStyle.m_thickness = radius * 1.5;
	quads.m_lineStyle.m_color1 = (m_polyData.m_color1 == TPixel::Transparent) ? m_lineColor : m_polyData.m_color1;

	//quads.m_lineStyle.m_isRegion = false;
	//quads.m_lineStyle.m_isHole = false;

	int x = (int)((m_tw * center.x) + 0.5);
	x += 3;
	TPointD aux = TPointD((double)x / m_tw, center.y);

	quads.m_quads.push_back((TQuadratic *)new TQuadratic(center, 0.5 * (center + aux), aux));
	m_polylinesArray.push_back(quads);
}

//-------------------------------------------------------------------

int TFlash::Imp::drawEllipse(const TPointD &center, double radiusX, double radiusY)
{
	int xmin = (int)(m_tw * (center.x - radiusX));  // x coordinate of the upper left corner of the bounding rectangle
	int ymin = (int)(m_tw * (-center.y - radiusY)); // y coordinate of the upper left corner of the bounding rectangle
	int xmax = (int)(m_tw * (center.x + radiusX));  // x coordinate of the bottom right corner of the bounding rectangle
	int ymax = (int)(m_tw * (-center.y + radiusY)); // y coordinate of the bottom right corner of the bounding rectangle
	int dx = xmax - xmin;							// dx is width diameter
	int dy = ymax - ymin;							// dy is height diameter

	// connect a serie of curves to draw the circle
	int c1dx = (int)(0.1465 * dx);
	int c1dy = (int)(0.1465 * dy);
	int c2dx = (int)(0.2070 * dx);
	int c2dy = (int)(0.2070 * dy);

	if (c1dx == 0 || c1dy == 0 || c2dx == 0 || c2dy == 0)
		return 0;

	FDTDefineShape3 *ellipse = new FDTDefineShape3(new FRect(xmin, ymin, xmax, ymax));

	U16 ellipseID = ellipse->ID();
	U32 fillID = setFill(ellipse);

	U32 lineStyleID = 0;
	if (m_thickness > 0)
		lineStyleID = ellipse->AddLineStyle((int)(2 * m_thickness * m_tw),
											new FColor(m_lineColor.r, m_lineColor.g, m_lineColor.b, m_lineColor.m));

	ellipse->FinishStyleArrays();

	addShape(ellipse, false, lineStyleID > 0, false, fillID != 0, true,
			 xmax, -(int)(m_tw * center.y), fillID, 0, lineStyleID);

	ellipse->AddShapeRec(new FShapeRecEdgeCurved(0, -c2dy, -c1dx, -c1dy));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(-c1dx, -c1dy, -c2dx, 0));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(-c2dx, 0, -c1dx, c1dy));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(-c1dx, c1dy, 0, c2dy));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(0, c2dy, c1dx, c1dy));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(c1dx, c1dy, c2dx, 0));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(c2dx, 0, c1dx, -c1dy));
	ellipse->AddShapeRec(new FShapeRecEdgeCurved(c1dx, -c1dy, 0, -c2dy));

	//TPoint dp1 = convert(m_tw*(outPolyline[0]->getP0()-outPolyline.back()->getP2()));

	ellipse->AddShapeRec(new FShapeRecEnd());

	m_tags.AddFObj(ellipse);
	FCTPlaceObject2 *placePolygon = new FCTPlaceObject2(false, // ~ _hasClipDepth
														false, true, false,
														m_currDepth++, ellipseID, affine2Matrix(m_affine), 0, 0, 0, 0 /**/);

	m_currSprite->AddFObj(placePolygon);
	return ellipseID;
}
//-------------------------------------------------------------------
int TFlash::Imp::setFill(FDTDefineShape3 *shape)
{
	if (m_polyData.m_type == Texture) {
		int lx, ly;
		U16 texId = getTexture(m_polyData, lx, ly);

		FMatrix *app = affine2Matrix(m_polyData.m_matrix * TScale(2048.0 / lx, 2048.0 / ly));
		return shape->AddFillStyle(new FFillStyleBitmap(true, texId, app));
	} else if (m_polyData.m_type == LinearGradient || m_polyData.m_type == RadialGradient) {
		FGradient *grad = new FGradient();
		FGradRecord *gradRec1 = new FGradRecord(0, new FColor(m_polyData.m_color1.r, m_polyData.m_color1.g, m_polyData.m_color1.b, m_polyData.m_color1.m));
		FGradRecord *gradRec2 = new FGradRecord(255, new FColor(m_polyData.m_color2.r, m_polyData.m_color2.g, m_polyData.m_color2.b, m_polyData.m_color2.m));
		grad->Add(gradRec1);
		grad->Add(gradRec2);
		return shape->AddFillStyle(new FFillStyleGradient(m_polyData.m_type == LinearGradient, affine2Matrix(m_polyData.m_matrix * TScale(10.0)), grad));
	} else if (m_polyData.m_type == Solid) {
		FColor *color1 = new FColor(m_polyData.m_color1.r, m_polyData.m_color1.g, m_polyData.m_color1.b, m_polyData.m_color1.m);
		return shape->AddSolidFillStyle(color1);
	}
	return 0;
}

//-------------------------------------------------------------------

bool PolyStyle::operator==(const PolyStyle &p) const
{
	if (m_type != p.m_type)
		return false;

	switch (m_type) {
	case Centerline:
		return m_thickness == p.m_thickness && m_color1 == p.m_color1;
		CASE Solid : return m_color1 == p.m_color1;
		CASE Texture : return m_matrix == p.m_matrix && m_texture.getPointer() == p.m_texture.getPointer();
		CASE LinearGradient : __OR RadialGradient : return m_color1 == p.m_color1 && m_color2 == p.m_color2 && m_matrix == p.m_matrix && m_smooth == p.m_smooth;
	DEFAULT:
		assert(false);
		return false;
	}
}

//-------------------------------------------------------------------

bool affineMinorThen(const TAffine &m0, const TAffine &m1)
{
	if (m0.a11 == m1.a11) {
		if (m0.a12 == m1.a12) {
			if (m0.a13 == m1.a13) {
				if (m0.a21 == m1.a21) {
					if (m0.a22 == m1.a22)
						return m0.a23 < m1.a23;
					else
						return m0.a22 < m1.a22;
				} else
					return m0.a21 < m1.a21;
			} else
				return m0.a13 < m1.a13;
		} else
			return m0.a12 < m1.a12;
	} else
		return m0.a11 < m1.a11;
}

//-------------------------------------------------------------------

bool PolyStyle::operator<(const PolyStyle &p) const
{
	if (m_type == p.m_type)
		switch (m_type) {
		case Centerline:
			return (m_thickness == p.m_thickness) ? m_color1 < p.m_color1 : m_thickness < p.m_thickness;
			CASE Solid : return m_color1 < p.m_color1;
			CASE Texture : return m_texture.getPointer() < p.m_texture.getPointer(); //ignoro la matrice!!!!
			CASE LinearGradient : __OR RadialGradient : return (m_smooth == p.m_smooth) ? ((m_color1 == p.m_color1) ? ((m_color2 == p.m_color2) ? affineMinorThen(m_matrix, p.m_matrix) : m_color2 < p.m_color2) : m_color1 < p.m_color1) : m_smooth < p.m_smooth;
		DEFAULT:
			assert(false);
			return false;
		}
	else
		return m_type < p.m_type;
}

//-------------------------------------------------------------------

U32 TFlash::Imp::findStyle(const PolyStyle &p, std::map<PolyStyle, U32> &idMap,
						   FDTDefineShape3 *polygon)
{
	U32 styleID = 0;
	std::map<PolyStyle, U32>::iterator it;
	it = idMap.find(p);

	if (it != idMap.end())
		return (*it).second;
	else {
		switch (p.m_type) {
		case Centerline: {
			FColor *color = new FColor(p.m_color1.r, p.m_color1.g,
									   p.m_color1.b, p.m_color1.m);
			int thickness = (int)(2 * p.m_thickness * m_tw);
			if (p.m_thickness > 0 && thickness == 0)
				thickness = 1;

			styleID = polygon->AddLineStyle(thickness, color);
		}
			CASE Solid:
			{
				if (p.m_color1.m == 0)
					styleID = 0;
				else {
					FColor *color = new FColor(p.m_color1.r, p.m_color1.g,
											   p.m_color1.b, p.m_color1.m);
					styleID = polygon->AddSolidFillStyle(color);
				}
			}
			CASE Texture:
			{
				try {
					int lx, ly;
					U16 texId = getTexture(p, lx, ly);
					FMatrix *app = affine2Matrix(p.m_matrix * TScale(2048.0 / lx, 2048.0 / ly));
					styleID = polygon->AddFillStyle(new FFillStyleBitmap(true, texId, app));
				} catch (TException &) {
					FColor *color = new FColor(0, 0, 0, 255);
					styleID = polygon->AddSolidFillStyle(color);
				}
			}
			CASE RadialGradient:
			{
				FGradient *grad = new FGradient();
				//FGradRecord *gradRec1 = new FGradRecord(0,   new FColor(p.m_color1.r, p.m_color1.g, p.m_color1.b, p.m_color1.m));
				//FGradRecord *gradRec2 = new FGradRecord(255, new FColor(p.m_color2.r, p.m_color2.g, p.m_color2.b, p.m_color2.m));
				int fac = (int)(127.0 - 0.56 * p.m_smooth);
				assert(fac >= 0 && fac < 128);
				FGradRecord *gradRec1 = new FGradRecord(fac, new FColor(p.m_color1.r, p.m_color1.g, p.m_color1.b, p.m_color1.m));
				FGradRecord *gradRec2 = new FGradRecord(255 - fac, new FColor(p.m_color2.r, p.m_color2.g, p.m_color2.b, p.m_color2.m));
				grad->Add(gradRec1);
				grad->Add(gradRec2);
				styleID = polygon->AddFillStyle(new FFillStyleGradient(false, affine2Matrix(p.m_matrix * TScale(15.0)), grad));
			}
			CASE LinearGradient:
			{
				FGradient *grad = new FGradient();
				FGradRecord *gradRec1 = new FGradRecord(0, new FColor(p.m_color1.r, p.m_color1.g, p.m_color1.b, p.m_color1.m));
				FGradRecord *gradRec2 = new FGradRecord(255, new FColor(p.m_color2.r, p.m_color2.g, p.m_color2.b, p.m_color2.m));
				grad->Add(gradRec1);
				grad->Add(gradRec2);
				styleID = polygon->AddFillStyle(new FFillStyleGradient(true, affine2Matrix(p.m_matrix * TScale(10.0)), grad));
			}
		DEFAULT:
			assert(false);
		}
		idMap[p] = styleID;
		return styleID;
	}
}

//-------------------------------------------------------------------

void TFlash::Imp::setStyles(const list<FlashPolyline> &polylines,
							vector<U32> &lineStyleID, vector<U32> &fillStyle1ID, vector<U32> &fillStyle2ID, FDTDefineShape3 *polygon)
{
	int i;
	std::list<FlashPolyline>::const_iterator it, itOld;

	std::map<PolyStyle, U32> idMap;

	for (i = 0, it = polylines.begin(); it != polylines.end(); ++i, itOld = it, ++it) {
		if (it->m_lineStyle.m_type == None)
			lineStyleID[i] = 0;
		else if (i > 0 && it->m_lineStyle == itOld->m_lineStyle)
			lineStyleID[i] = lineStyleID[i - 1];
		else
			lineStyleID[i] = findStyle(it->m_lineStyle, idMap, polygon);

		if (it->m_fillStyle1.m_type == None)
			fillStyle1ID[i] = 0;
		else if (i > 0 && it->m_fillStyle1 == itOld->m_fillStyle1)
			fillStyle1ID[i] = fillStyle1ID[i - 1];
		else
			fillStyle1ID[i] = findStyle(it->m_fillStyle1, idMap, polygon);

		if (it->m_fillStyle2.m_type == None)
			fillStyle2ID[i] = 0;
		else if (i > 0 && it->m_fillStyle2 == itOld->m_fillStyle2)
			fillStyle2ID[i] = fillStyle2ID[i - 1];
		else
			fillStyle2ID[i] = findStyle(it->m_fillStyle2, idMap, polygon);
	}
}

//-------------------------------------------------------------------

TPoint drawPoint(const TQuadratic *poly, FDTDefineShape3 *polygon, double radius, TRect &box)
{
	TPointD center = poly->getP0();
	int xmin = (int)(Tw * (center.x - radius));  // x coordinate of the upper left corner of the bounding rectangle
	int ymin = (int)(Tw * (-center.y - radius)); // y coordinate of the upper left corner of the bounding rectangle
	int xmax = (int)(Tw * (center.x + radius));  // x coordinate of the bottom right corner of the bounding rectangle
	int ymax = (int)(Tw * (-center.y + radius)); // y coordinate of the bottom right corner of the bounding rectangle
	int dx = xmax - xmin;						 // dx is width diameter
	int dy = ymax - ymin;						 // dy is height diameter

	// connect a serie of curves to draw the circle
	int c1dx = (int)(0.1465 * dx);
	int c1dy = (int)(0.1465 * dy);
	int c2dx = (int)(0.2070 * dx);
	int c2dy = (int)(0.2070 * dy);

	if (c1dx == 0 || c1dy == 0 || c2dx == 0 || c2dy == 0)
		return TPoint();

	if (xmax > box.x1)
		box.x1 = xmax;
	if (xmin < box.x0)
		box.x0 = xmin;
	if (ymax > box.y1)
		box.y1 = ymax;
	if (ymin < box.y0)
		box.y0 = ymin;

	//polygon->AddShapeRec(new FShapeRecEdgeCurved(dp1.x, -dp1.y, dp2.x, -dp2.y));
	//

	addShape(polygon, false, true, false, false, true,
			 xmax, (int)(Tw * -center.y), 0, 0, 0);

	polygon->AddShapeRec(new FShapeRecEdgeCurved(0, -c2dy, -c1dx, -c1dy));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(-c1dx, -c1dy, -c2dx, 0));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(-c2dx, 0, -c1dx, c1dy));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(-c1dx, c1dy, 0, c2dy));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(0, c2dy, c1dx, c1dy));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(c1dx, c1dy, c2dx, 0));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(c2dx, 0, c1dx, -c1dy));
	polygon->AddShapeRec(new FShapeRecEdgeCurved(c1dx, -c1dy, 0, -c2dy));

	return TPoint(xmax, (int)(Tw * -center.y));
}

//-------------------------------------------------------------------
inline void updateBBox(TRect &box, const TPoint &p)
{
	if (p.x > box.x1)
		box.x1 = p.x;
	if (p.x < box.x0)
		box.x0 = p.x;
	if (p.y > box.y1)
		box.y1 = p.y;
	if (p.y < box.y0)
		box.y0 = p.y;
}

void TFlash::Imp::doDrawPolygon(list<FlashPolyline> &polylines, int clippedShapes)
{
	assert(m_currSprite);
	assert(!polylines.empty());

	FDTDefineShape3 *polygon = new FDTDefineShape3();
	U16 polygonID = polygon->ID();

	//U32 fillID       = 0;

	/*
if (isOutline || isRegion)
	fillID = ((clippedShapes>0)?polygon->AddSolidFillStyle( new FColor(0, 0, 0)):setFill(polygon));
*/

	std::map<double, std::map<TPixel32, U32>> idMap;

	vector<U32> fillStyle1ID(polylines.size());
	vector<U32> fillStyle2ID(polylines.size());
	vector<U32> lineStyleID(polylines.size());

	int i, j;
	setStyles(polylines, lineStyleID, fillStyle1ID, fillStyle2ID, polygon);
	polygon->FinishStyleArrays();

	std::list<FlashPolyline>::iterator itOld, it = polylines.begin(), it_e = polylines.end();

	for (i = 0; it != it_e; ++i, ++it) //le maschere non vengono bene con regioni con fill2 opaco e fill1 trasparente. le giro
		if (fillStyle2ID[i] != 0 && fillStyle1ID[i] == 0) {
			fillStyle1ID[i] = fillStyle2ID[i];
			fillStyle2ID[i] = 0;
			vector<TQuadratic *> &v = (*it).m_quads;
			std::reverse(v.begin(), v.end());
			for (j = 0; j < (int)v.size(); j++) {
				v[j] = new TQuadratic(v[j]->getP2(), v[j]->getP1(), v[j]->getP0());
				m_quadsToBeDeleted.push_back(v[j]);
			}
		}

	TPoint lastPoint, firstPoint = toTwips(polylines.front().m_quads[0]->getP0());

	TPoint currP = TPoint(firstPoint.x, firstPoint.y);
	TRect box;
	box.x0 = firstPoint.x;
	box.x1 = firstPoint.x;
	box.y0 = firstPoint.y;
	box.y1 = firstPoint.y;

	//const PolyStyle& p = polylines.front().m_fillStyle1;

	//assert(firstPoint.x!=0 && firstPoint.y!=0);

	U32 currLineStyle = 0, currFillStyle1 = 0, currFillStyle2 = 0;

	//if (lineStyleID[0]!=0 && (isOutline||p.m_isRegion))
	currLineStyle = lineStyleID[0];

	//if (fillStyle1ID[0]!=0 && (isOutline||p.m_isRegion))
	currFillStyle1 = fillStyle1ID[0];
	//if (fillStyle2ID[0]!=0 && p.m_isRegion)
	currFillStyle2 = fillStyle2ID[0];

	//m_of << "DRAW  POLYGON da (" << firstPoint.x<< ", "<<firstPoint.y<<")"<<std::endl;

	addShape(polygon, false, lineStyleID[0] != 0, true, true, true,
			 &firstPoint, currFillStyle1, currFillStyle2, currLineStyle);

	//vengono aggiunti gli edges al polygon, compresi i buchi. Attenzione: i buchi sono aggiunti
	//nel verso opposto di percorrenza, se no non funzionerebbe quando il polygon funge da mask!

	for (it = polylines.begin(), j = 0; it != it_e; ++j, itOld = it, ++it) {

		if (it->m_skip) //m_of << "  SKIPPOOOOO! " <<std::endl;
			continue;

		const vector<TQuadratic *> &poly = it->m_quads;
		firstPoint = toTwips(poly[0]->getP0());
		lastPoint = toTwips(poly.back()->getP2());
		/* m_of << "  POLYLINE # da ( " 
    				      <<firstPoint.x
				      <<", "
				      <<firstPoint.y
				      <<") a ("
				      <<lastPoint.x
				      <<", "
				      <<lastPoint.y
				      <<")"
				      <<std::endl;*/

		if (j > 0) {
			//faccio una move se il salto e' sensato...evito di mettere inutili addShapeRec nell'swf per renderlo piu' piccolo...

			bool isMove = (currP != firstPoint);

			//bool isMove =  (currP.x-firstPoint.x)<-20 || (currP.x-firstPoint.x)>20||(currP.y-firstPoint.y)<-20||(currP.y-firstPoint.y)>20;
			currP = firstPoint;

			updateBBox(box, currP);

			bool isLineStyle = (lineStyleID[j] != currLineStyle);

			if (isLineStyle)
				currLineStyle = lineStyleID[j];

			bool isFillStyle1 = (fillStyle1ID[j] != currFillStyle1); //se sto passando da una regione a una linea, devo mettere a zero lo stile di fill!
			if (isFillStyle1)
				currFillStyle1 = fillStyle1ID[j];

			bool isFillStyle2 = (fillStyle2ID[j] != currFillStyle2); //se sto passando da una regione a una linea, devo mettere a zero lo stile di fill!
			if (isFillStyle2)
				currFillStyle2 = fillStyle2ID[j];
			assert(firstPoint.x != 0 || firstPoint.y != 0);

			if (isMove || isLineStyle || isFillStyle1 || isFillStyle2)
				addShape(polygon, false, isLineStyle, isFillStyle2, isFillStyle1, isMove, &firstPoint,
						 currFillStyle1, currFillStyle2, currLineStyle);
		}

		for (i = 0; i < (int)poly.size(); i++) {
			if (i > 0) {
				TPoint dp = toTwips(poly[i]->getP0()) - toTwips(poly[i - 1]->getP2());
				if (dp.x != 0 || dp.y != 0) {
					addEdgeStraightToShape(polygon, dp);
					currP.x += dp.x;
					currP.y += dp.y;
				}
			}

			if (!it->m_isPoint) {
				TPoint dp1 = toTwips(poly[i]->getP1()) - toTwips(poly[i]->getP0());
				TPoint dp2 = toTwips(poly[i]->getP2()) - toTwips(poly[i]->getP1());
				if (dp1 == dp2)
					addEdgeStraightToShape(polygon, dp1 + dp2);
				else if (dp1 == TPoint(0, 0))
					addEdgeStraightToShape(polygon, dp2);
				else if (dp2 == TPoint(0, 0))
					addEdgeStraightToShape(polygon, dp1);
				else
					polygon->AddShapeRec(new FShapeRecEdgeCurved(dp1.x, dp1.y, dp2.x, dp2.y));

				currP.x += dp1.x + dp2.x;
				currP.y += dp1.y + dp2.y;
				//m_of<<"CURRPOINT: ("<<currP.x<<", "<<currP.y<<")"<<std::endl;
			} else //e' un punto isolato! bisogna disegnarlo
			{
				currP = drawPoint(poly[0], polygon, it->m_lineStyle.m_thickness, box);
				currLineStyle = 0;
			}

			updateBBox(box, currP);
		}

		if (poly.size() != 1 && currP != lastPoint) {
			addEdgeStraightToShape(polygon, lastPoint - currP);
			currP.x += lastPoint.x - currP.x;
			currP.y += lastPoint.y - currP.y;
			//m_of<<"AGGIUNTO RACCORDO!CURRPOINT: ("<<currP.x<<", "<<currP.y<<")"<<std::endl;
		}
	}

	polygon->AddShapeRec(new FShapeRecEnd());

	box = box.enlarge((int)(m_thickness * m_tw) + 1000);

	polygon->setBounds(new FRect(box.x0, box.y0, box.x1, box.y1));
	m_tags.AddFObj(polygon);
	//m_of<< "aggiungo poly " <<polygon->ID()<< " a depth "<<m_currDepth<<endl;
	FCTPlaceObject2 *placePolygon = new FCTPlaceObject2(clippedShapes > 0, // ~ _hasClipDepth
														false, true, false,
														m_currDepth, polygonID,
														affine2Matrix(m_affine), 0, 0, 0,
														(clippedShapes > 0) ? m_currDepth + clippedShapes : 0 /**/);

	m_currDepth++;
	m_currSprite->AddFObj(placePolygon);
}

//-------------------------------------------------------------------
#ifdef LEVO
void TFlash::Imp::addCameraClip(int index)
{
	m_notClipped = false;
	FRect *clipRectBounds = new FRect(0, 0, m_lx * m_tw, m_ly * m_tw); //coordinate values are in TWIPS

	FDTDefineShape3 *clipRectangle = new FDTDefineShape3(clipRectBounds);

	FColor black = FColor(0, 0, 0);

	U32 blackfillID = clipRectangle->AddSolidFillStyle(new FColor(black));
	clipRectangle->FinishStyleArrays();
	addShape(clipRectangle, false, true, true, false, true, 0, 0, blackfillID, 0);

	addEdgeStraightToShape(clipRectangle, 0, m_ly * m_tw);
	addEdgeStraightToShape(clipRectangle, m_lx * m_tw, 0);
	addEdgeStraightToShape(clipRectangle, 0, -m_ly * m_tw);
	addEdgeStraightToShape(clipRectangle, -m_lx * m_tw, 0);
	clipRectangle->AddShapeRec(new FShapeRecEnd());

	// la depth e' 1 perche' riservata per la camera
	m_tags.InsertFObj(index, new FCTPlaceObject2(true, false, true, false, 1,
												 clipRectangle->ID(), 0, 0, 0, 0, (U16)60000 /**/));
	m_tags.InsertFObj(index, clipRectangle);
}
#endif
//-------------------------------------------------------------------

void computeQuadChain(const TEdge &e,
					  vector<TQuadratic *> &quadArray, vector<TQuadratic *> &toBeDeleted)
{
	int chunk_b, chunk_e, chunk = -1;
	double t_b, t_e, w0, w1;
	TThickQuadratic *q_b = 0, *q_e = 0;
	TThickQuadratic dummy;
	bool reversed = false;

	if (e.m_w0 > e.m_w1) {
		reversed = true;
		w0 = e.m_w1;
		w1 = e.m_w0;
	} else {
		w0 = e.m_w0;
		w1 = e.m_w1;
	}

	if (w0 == 0.0)
		chunk_b = 0;
	else {
		if (e.m_s->getChunkAndT(w0, chunk, t_b))
			assert(false);
		q_b = new TThickQuadratic();
		toBeDeleted.push_back(q_b);
		e.m_s->getChunk(chunk)->split(t_b, dummy, *q_b);
		chunk_b = chunk + 1;
	}

	if (w1 == 1.0)
		chunk_e = e.m_s->getChunkCount() - 1;
	else {
		if (e.m_s->getChunkAndT(w1, chunk_e, t_e))
			assert(false);
		q_e = new TThickQuadratic();
		toBeDeleted.push_back(q_e);
		if (chunk_e == chunk) {
			if (q_b) {
				t_e = q_b->getT(e.m_s->getChunk(chunk)->getPoint(t_e));
				q_b->split(t_e, *q_e, dummy);
			} else
				e.m_s->getChunk(0)->split(t_e, *q_e, dummy);

			if (!reversed)
				quadArray.push_back(q_e);
			else {
				quadArray.push_back(new TQuadratic(q_e->getP2(), q_e->getP1(), q_e->getP0()));
				toBeDeleted.push_back(quadArray.back());
			}
			return;
		}
		e.m_s->getChunk(chunk_e)->split(t_e, *q_e, dummy);
		chunk_e--;
	}

	int i;
	assert(chunk_e >= chunk_b - 1);

	if (reversed) {
		if (q_e) {
			q_e->reverse();
			quadArray.push_back(q_e);
		}
		for (i = chunk_e; i >= chunk_b; i--) {
			const TThickQuadratic *qAux = e.m_s->getChunk(i);
			quadArray.push_back(new TQuadratic(qAux->getP2(), qAux->getP1(), qAux->getP0()));
			toBeDeleted.push_back(quadArray.back());
		}

		if (q_b) {
			q_b->reverse();
			quadArray.push_back(q_b);
		}
	} else {
		if (q_b)
			quadArray.push_back(q_b);
		for (i = chunk_b; i <= chunk_e; i++)
			quadArray.push_back((TQuadratic *)e.m_s->getChunk(i));
		if (q_e)
			quadArray.push_back(q_e);
	}
}

//-------------------------------------------------------------------

namespace
{
inline bool isOuterEdge(const FlashPolyline &p)
{
	bool isTrasp1 = p.m_fillStyle1.m_type == None || (p.m_fillStyle1.m_type == Solid && p.m_fillStyle1.m_color1.m == 0);
	bool isTrasp2 = p.m_fillStyle2.m_type == None || (p.m_fillStyle2.m_type == Solid && p.m_fillStyle2.m_color1.m == 0);
	return (isTrasp1 ^ isTrasp2);
}
}

//-------------------------------------------------------------------

void TFlash::Imp::addAutoclose(biPoint &bp, int edgeIndex)
{
	std::map<biPoint, FlashPolyline *>::iterator it = m_autocloseMap.end();

	bp.revert();
	it = m_autocloseMap.find(bp);
	bp.revert();
	if (it != m_autocloseMap.end())
		(*it).second->m_fillStyle2 = m_polyData;
	else if ((it = m_autocloseMap.find(bp)) != m_autocloseMap.end())
		(*it).second->m_fillStyle1 = m_polyData;
	else {
		FlashPolyline quadArray1;
		quadArray1.m_depth = m_regionDepth * m_strokeCount + edgeIndex + 1;
		quadArray1.m_fillStyle1 = m_polyData;
		quadArray1.m_quads.push_back(new TQuadratic(bp.p0, .5 * (bp.p0 + bp.p1), bp.p1));
		m_quadsToBeDeleted.push_back(quadArray1.m_quads.back());

		m_polylinesArray.push_back(quadArray1);
		m_autocloseMap[bp] = &m_polylinesArray.back();
	}

	if (m_isMask && it != m_autocloseMap.end())
		(*it).second->m_skip = !isOuterEdge(*(*it).second);
}

//-------------------------------------------------------------------

void TFlash::Imp::addNewEdge(const TEdge &e)
{
	m_polylinesArray.push_back(FlashPolyline());
	FlashPolyline &quadArray = m_polylinesArray.back();
	m_edgeMap[e] = &m_polylinesArray.back();

	computeQuadChain(e, quadArray.m_quads, m_quadsToBeDeleted);

	quadArray.m_fillStyle1 = m_polyData;
	quadArray.m_depth = m_regionDepth * m_strokeCount + e.m_index + 1;

	if (e.m_s->getAverageThickness() > 0) {
		assert(m_currPalette);

		TStrokeProp *prop = e.m_s->getProp();
		/////questo codice stava dentro tstroke::getprop/////////
		TColorStyle *style = m_currPalette->getStyle(e.m_s->getStyle());
		if (!style->isStrokeStyle() || style->isEnabled() == false)
			prop = 0;
		else {
			if (!prop || style != prop->getColorStyle()) {
				e.m_s->setProp(style->makeStrokeProp(e.m_s));
				prop = e.m_s->getProp();
			}
		}

		///////////
		if (prop) {
			OutlineStrokeProp *aux = dynamic_cast<OutlineStrokeProp *>(prop);

			if (aux) {
				const TSolidColorStyle *st = dynamic_cast<const TSolidColorStyle *>(aux->getColorStyle());
				if (st) {
					quadArray.m_lineStyle.m_color1 = st->getMainColor();
					quadArray.m_lineStyle.m_type = Centerline;
					quadArray.m_lineStyle.m_thickness = e.m_s->getAverageThickness();
				}
			}
		}
		//quadArray.m_lineStyle.m_color1 = e.m_s->getStyle();
	}
	if (quadArray.m_fillStyle1.m_type == None) {
		//assert(clippedShapes>0);
		quadArray.m_fillStyle1.m_type = Solid;
		quadArray.m_fillStyle1.m_color1 = TPixel::Black;
	}
}

//-------------------------------------------------------------------

void TFlash::Imp::addEdge(const TEdge &e, TPointD &pBegin, TPointD &pEnd)
{
	TPointD auxP = pEnd;

	TEdge aux = e;
	tswap(aux.m_w0, aux.m_w1);
	std::map<TEdge, FlashPolyline *>::iterator it;
	//PolyStyle style = m_polyData;
	std::stack<PolyStyle> auxs;

	if ((it = m_edgeMap.find(aux)) != m_edgeMap.end()) {
		(*it).second->m_fillStyle2 = m_polyData;

		pBegin = (*it).second->m_quads.back()->getP2();
		pEnd = (*it).second->m_quads.front()->getP0();
	} else if ((it = m_edgeMap.find(e)) != m_edgeMap.end()) {
		(*it).second->m_fillStyle1 = m_polyData;

		pBegin = (*it).second->m_quads.front()->getP0();
		pEnd = (*it).second->m_quads.back()->getP2();
	} else {
		addNewEdge(e);
		pBegin = m_polylinesArray.back().m_quads[0]->getP0();
		pEnd = m_polylinesArray.back().m_quads.back()->getP2();
	}

	if (m_isMask && it != m_edgeMap.end()) //per fare le maschere, non metto gli edge che hanno entrambe le sponde non trasparenti
		(*it).second->m_skip = !isOuterEdge(*(*it).second);

	if (!areTwEqual(auxP, pBegin)) {
		biPoint tmp(auxP, pBegin);
		addAutoclose(tmp, e.m_index);
	}

	if (m_properties.m_lineQuality.getValue() == ConstantLines) {
		std::map<const TStroke *, std::set<wChunk>>::iterator it;

		it = m_strokeMap.find(e.m_s);
		if (it != m_strokeMap.end()) {
			std::set<wChunk>::iterator it1;
			wChunk wc(tmin(e.m_w0, e.m_w1), tmax(e.m_w0, e.m_w1));

			it1 = it->second.find(wc);
			if (it1 == it->second.end())
				it->second.insert(wc);
		} else {
			std::set<wChunk> chunkSet;
			chunkSet.insert(wChunk(tmin(e.m_w0, e.m_w1), tmax(e.m_w0, e.m_w1)));
			m_strokeMap[e.m_s] = chunkSet;
		}
	}
}

//-------------------------------------------------------------------

void TFlash::Imp::drawSubregions(TFlash *tf, const TRegion *r, const TPalette *palette)
{
	int i;

	//m_currentBgStyle.push_back(m_polyData);
	m_regionDepth++;

	for (i = 0; i < (int)r->getSubregionCount(); i++) {
		TRegion *region = r->getSubregion(i);
		TRegionProp *prop = region->getProp(/*palette*/);
		////prima questo codice stava dentro tregion::getprop////
		int styleId = region->getStyle();
		//if (styleId)
		{
			TColorStyle *style = palette->getStyle(styleId);
			if (!style->isRegionStyle() || style->isEnabled() == false)
				prop = 0;
			else if (!prop || style != prop->getColorStyle()) {
				region->setProp(style->makeRegionProp(region));
				prop = region->getProp();
			}

			////////
			if (prop)
				prop->draw(*tf);

			//m_currentBgStyle.push_back(m_polyData);
			drawSubregions(tf, region, palette);
			//m_currentBgStyle.pop_back();
		}
	}
	m_regionDepth--;

	//m_currentBgStyle.pop_back();
}
//-------------------------------------------------------------------

bool comparevector(const FlashPolyline &p1, const FlashPolyline &p2)
{
	return p2.m_depth > p1.m_depth;
}

//-------------------------------------------------------------------

void TFlash::Imp::buildRegion(TFlash *tf, const TVectorImageP &vi, int regionIndex)
{
	TRegion *region = vi->getRegion(regionIndex);
	TRectD rect = region->getBBox();
	double lx = rect.x1 - rect.x0;
	double ly = rect.y1 - rect.y0;
	if (lx < 0.5 || ly < 0.5)
		return;

	TRegionProp *prop = region->getProp();

	int styleId = region->getStyle();

	TColorStyle *style = vi->getPalette()->getStyle(styleId);

	if (!style->isRegionStyle() || style->isEnabled() == false)
		prop = 0;
	else if (!prop || style != prop->getColorStyle()) {
		region->setProp(style->makeRegionProp(region));
		prop = region->getProp();
	}

	tf->setThickness(0);
	if (prop)
		prop->draw(*tf);

	drawSubregions(tf, region, m_currPalette);
}

//-------------------------------------------------------------------

void TFlash::Imp::buildStroke(TFlash *tf, const TVectorImageP &vi, int strokeIndex)
{
	TStroke *stroke = vi->getStroke(strokeIndex);
	if (stroke->getBBox().isEmpty())
		return;

	TStrokeProp *prop = stroke->getProp();
	/////questo codice stava dentro tstroke::getprop/////////
	TColorStyle *style = vi->getPalette()->getStyle(stroke->getStyle() /*m_imp->m_styleId*/);
	if (!style->isStrokeStyle() || style->isEnabled() == false)
		prop = 0;
	else if (!prop || style != prop->getColorStyle()) {
		stroke->setProp(style->makeStrokeProp(stroke));
		prop = stroke->getProp();
	}

	if (prop)
		prop->draw(*tf);
}

//-------------------------------------------------------------------

USHORT TFlash::Imp::buildVectorImage(const TVectorImageP &_vi, TFlash *tf, double &scaleFactor, bool isMask)
{
	USHORT id;
	int i;
	const TPalette *oldPalette;

	assert(m_regionDepth == 0);

	m_strokeCount = _vi->getStrokeCount();

	std::vector<int> strokes;
	for (i = 0; i < m_strokeCount; i++)
		strokes.push_back(i);

	_vi->enableMinimizeEdges(false);
	_vi->notifyChangedStrokes(strokes, vector<TStroke *>());
	_vi->enableMinimizeEdges(true);

	TRectD box = _vi->getBBox();
	int dim = tmax(convert(box).getLx(), convert(box).getLy());

	TVectorImageP vi;
	if (dim * m_tw > 32767.0) {
		scaleFactor = dim * m_tw / 32767.0;
		vi = _vi->clone();
		vi->transform(TScale(1.0 / scaleFactor), true);
	} else
		vi = _vi;

	oldPalette = m_currPalette;
	m_currPalette = vi->getPalette();

	bool newSprite = false;

	if (m_currSprite == 0) //non e' un image pattern!!!
	{
		m_currSprite = new FDTSprite();
		m_currDepth = 1;
		newSprite = true;
	}
	/*assert(m_currSprite==0);
m_currSprite = new FDTSprite();
m_currDepth = 1;*/

	tf->setThickness(0);
	m_isMask = isMask;
	if (!isMask)
		for (i = 0; i < (int)vi->getStrokeCount(); i++)
			vi->getStroke(i)->setAverageThickness((m_properties.m_lineQuality.getValue() == ConstantLines) ? computeAverageThickness(vi->getStroke(i)) : 0);

	UINT strokeIndex = 0;
	int parentDepth = 0;
	while (strokeIndex < vi->getStrokeCount()) // ogni ciclo di while fa un gruppo
	{
		FDTSprite *parentSprite = 0;
		int currStrokeIndex = strokeIndex;
		if (vi->isStrokeGrouped(currStrokeIndex) != 0) {
			parentSprite = m_currSprite;
			parentDepth = m_currDepth;
			m_currSprite = new FDTSprite();
			m_currDepth = 0;
		}
		for (UINT regionIndex = 0; regionIndex < vi->getRegionCount(); regionIndex++)
			if (vi->sameGroupStrokeAndRegion(currStrokeIndex, regionIndex))
				buildRegion(tf, vi, regionIndex);

		if (m_properties.m_lineQuality.getValue() != ConstantLines)
			tf->drawHangedObjects();
		//else
		m_polylinesArray.sort(comparevector);

		while (strokeIndex < vi->getStrokeCount() && vi->sameGroup(strokeIndex, currStrokeIndex)) {
			if (!isMask)
				buildStroke(tf, vi, strokeIndex);
			strokeIndex++;
		}

		if (isMask && vi->getRegionCount() == 0) //pezza:se non ci sono regioni, la maschera
												 //deve mostrare il nulla; per fare cio', metto
												 //un poligono posticcio(non mascherante, puro stroke
												 //centerline) nella sprite, altrimenti
												 //invece di non vedersi nulla si vede l'
												 //immagine mascherata completa..
		{
			FlashPolyline quads;
			TQuadratic qaux(TPointD(0, 0), TPointD(10, 10), TPointD(20, 20));
			quads.m_quads.push_back(&qaux);
			m_polylinesArray.push_back(quads);
		}
		tf->drawHangedObjects();
		if (parentSprite) {
			m_tags.AddFObj(m_currSprite);
			parentSprite->AddFObj(new FCTPlaceObject2(false,					// hasClipDepth
													  false,					// hasRatio,
													  true,						// hasCharId,
													  false,					// hasMove,
													  parentDepth++,			//depth
													  m_currSprite->ID(),		//id
													  affine2Matrix(TAffine()), //matrix
													  0, 0, 0, 0));

			m_currSprite = parentSprite;
			m_currDepth = parentDepth;
			parentSprite = 0;
		}
	}

	id = m_currSprite->ID();

	if (newSprite) {
		m_currSprite->AddFObj(new FCTShowFrame());
		m_tags.AddFObj(m_currSprite);
		m_currSprite = 0;
	}
	m_currPalette = oldPalette;

	return id;
}

//-------------------------------------------------------------------

USHORT TFlash::Imp::buildRasterImage(const TImageP img, TFlash *tf)
{
	if (m_currSprite) //e' un custom style
	{
		std::map<const TImage *, USHORT>::iterator it;
		if (m_keepImages && (it = m_imagesMap.find(img.getPointer())) != m_imagesMap.end()) {
			m_currSprite->AddFObj(new FCTPlaceObject2(false, // hasClipDepth
													  false, // hasRatio,
													  true,  // hasCharId,
													  false, //i!=0,   // hasMove,
													  m_currDepth++, it->second, affine2Matrix(m_affine),
													  0, 0, 0, 0));

			return 0;
		}
	}

	TToonzImageP tim = (TToonzImageP)img;
	TRasterImageP rim = (TRasterImageP)img;
	TRasterP ri;

	if (tim) {
		TRaster32P raux(tim->getSize());
		TRop::convert(raux, tim->getRaster(), img->getPalette(), TRect(0, 0, tim->getSize().lx - 1, tim->getSize().ly - 1), false, true);
		ri = raux;
	} else if (!((TRaster32P)rim->getRaster())) {
		TRaster32P raux(rim->getRaster()->getSize());
		TRop::convert(raux, rim->getRaster());
		ri = raux;
	} else
		ri = rim->getRaster();

	//TImageWriter::save(TFilePath("C:\\temp\\flame.tif"), ri);

	int lx = ri->getLx(), ly = ri->getLy(), wrap = ri->getWrap();

	/*
double dpix=72.,dpiy=72.;

if (rim)
  rim->getDpi(dpix,dpiy);
else
  tim->getDpi(dpix,dpiy);

if (dpix==0 && dpiy==0)
{
dpix=cameradpix;
dpiy=cameradpiy;
}
*/

	//const double factor = Stage::inch;

	//double unit = 100;
	//int realLx = lx;//(int)(cameradpix * lx / dpix);
	//int realLy = ly;//(int)(cameradpiy * ly / dpiy);

	std::vector<UCHAR> *buffer = new std::vector<UCHAR>();

	//TRaster32P newRaster= TRop::copyAndSwapRBChannels(ri);

	//#ifdef TNZ_MACHINE_CHANNEL_ORDER_MRGB

	//TRaster32P newRaster= TRop::copyAndSwapRBChannels(ri);
	//Tiio::createJpg(*buffer, newRaster, m_properties.m_jpgQuality.getValue());
	//#else

	Tiio::createJpg(*buffer, ri, m_properties.m_jpgQuality.getValue());
	m_toBeDeleted.push_back(buffer);

	//#endif

	int bitmapId;
	if (m_supportAlpha) {
		std::vector<UCHAR> alphachannel(lx * ly);

		ri->lock();
		TPixel *auxin, *bufin = (TPixel *)ri->getRawData();

		int i, j, count = 0;
		for (i = 0; i < ly; i++) {
			auxin = bufin + (ly - i - 1) * wrap;
			for (j = 0; j < lx; j++, auxin++)
				alphachannel[count++] = auxin->m;
		}
		ri->unlock();

		U32 zippedAlphaSizeOut = (U32)(2 * ((alphachannel.size() * 1.1) + 12));
		std::vector<UCHAR> *zippedAlphaChannel = new std::vector<UCHAR>(zippedAlphaSizeOut);
		compress(&(*zippedAlphaChannel)[0], (uLongf *)&zippedAlphaSizeOut, (const UCHAR *)&alphachannel[0], alphachannel.size());
		zippedAlphaChannel->resize(zippedAlphaSizeOut);
		m_toBeDeleted.push_back(zippedAlphaChannel);
		FDTDefineBitsJPEG3 *bitmap = new FDTDefineBitsJPEG3((U8 *)&(*buffer)[0], buffer->size(), &(*zippedAlphaChannel)[0], zippedAlphaSizeOut);
		m_tags.AddFObj(bitmap);
		bitmapId = bitmap->ID();
	} else {
		FDTDefineBitsJPEG2 *bitmap = new FDTDefineBitsJPEG2((U8 *)&(*buffer)[0], buffer->size());
		m_tags.AddFObj(bitmap);
		bitmapId = bitmap->ID();
	}

	//
	//m_totMem += (buffer->size()/*+zippedAlphaChannel->size()*/)/1024.0;

	FRect *rectBounds = new FRect(-lx * m_sCoord1 / 2, -ly * m_sCoord1 / 2, lx * m_sCoord1 / 2, ly * m_sCoord1 / 2);
	FDTDefineShape3 *rectangle = new FDTDefineShape3(rectBounds);

	FMatrix *matrix1 = new FMatrix(true, m_tw * Fixed1, m_tw * Fixed1, false, 0, 0, -(lx / 2) * m_sCoord1, -(ly / 2) * m_sCoord1);
	FFillStyle *fill1 = new FFillStyleBitmap(false, bitmapId, matrix1);

	U32 fillStyle1_ID = rectangle->AddFillStyle(fill1);

	rectangle->FinishStyleArrays();
	addShape(rectangle, false, false, true, false, true, (lx / 2) * m_sCoord1, (ly / 2) * m_sCoord1, 0,
			 fillStyle1_ID, 0);

	addEdgeStraightToShape(rectangle, -lx * m_sCoord1, 0);
	addEdgeStraightToShape(rectangle, 0, -ly * m_sCoord1);
	addEdgeStraightToShape(rectangle, lx * m_sCoord1, 0);
	addEdgeStraightToShape(rectangle, 0, ly * m_sCoord1);
	rectangle->AddShapeRec(new FShapeRecEnd());

	m_tags.AddFObj(rectangle);

	if (m_currSprite) {
		//e' un custom style

		m_imagesMap[img.getPointer()] = rectangle->ID();

		m_currSprite->AddFObj(new FCTPlaceObject2(false, // hasClipDepth
												  false, // hasRatio,
												  true,  // hasCharId,
												  false, //i!=0,   // hasMove,
												  m_currDepth++, rectangle->ID(), affine2Matrix(m_affine),
												  0, 0, 0, 0));
	}

	return rectangle->ID();
	//delete bufout;
}

//-------------------------------------------------------------------

USHORT TFlash::Imp::buildImage(const TImageP img, TFlash *tf, double &scaleFactor, bool isMask)
{
	scaleFactor = 1.0;

	if (img->getType() == TImage::VECTOR)
		return buildVectorImage((TVectorImageP)img, tf, scaleFactor, isMask);
	else
		return buildRasterImage(img, tf);
}

//-------------------------------------------------------------------

void TFlash::Imp::addSoundToFrame(bool isLast)
{
	if (!m_sound.empty()) {
		TSoundTrackP sound = *m_sound.begin();
		if ((int)m_sound.size() == m_soundSize) {
			bool is16Bit = (sound->getBitPerSample() == 16);
			bool isStereo = (sound->getChannelCount() == 2);
			UCHAR rate;
			switch (sound->getSampleRate() / 5000) {
			case 1: // 5k
				rate = 0;
				break;
			case 2: // 11k
				rate = 1;
				break;
			case 4: // 22k
				rate = 2;
				break;
			case 8: // 44k
				rate = 3;
				break;
			default:
				rate = 0;
			}
			UCHAR format = 4 * (rate) + is16Bit * 2 + isStereo;
			FDTSoundStreamHead2 *head = new FDTSoundStreamHead2(
				format, c_soundCompression, rate, is16Bit, isStereo, (USHORT)sound->getSampleCount());
			m_tags.AddFObj(head);
		}
		int bytes = sound->getSampleCount() * sound->getSampleSize();
		U8 *aux = new U8[bytes];

#if TNZ_LITTLE_ENDIAN
		memcpy(aux, sound->getRawData(), bytes);
#else //su mac, gli short vanno girati!!
		int ii;
		assert(c_soundBps == 16);
		const U8 *buf = sound->getRawData();
		for (ii = 0; ii < (bytes & (~0x1)); ii += 2)
			aux[ii] = buf[ii + 1], aux[ii + 1] = buf[ii];
#endif
		m_tags.AddFObj(new FDTSoundStreamBlock(bytes, aux));

		m_soundBuffer.push_back(aux);
		m_sound.erase(m_sound.begin());
		if (isLast)
			m_sound.erase(m_sound.begin(), m_sound.end());
	}
}

//-------------------------------------------------------------------
void TFlash::setGlobalScale(const TAffine &aff)
{
	m_imp->m_globalScale = aff;
}

//-------------------------------------------------------------------

void TFlash::Imp::writeFrame(TFlash *tf, bool isLast, int frameCountLoader, bool lastScene)
{
	if (!m_frameData)
		return;
	int oldSize = m_oldFrameData ? (int)m_oldFrameData->size() : -1;
	int depth;
	//bool firstTime = true;

	static bool loaderAdded = false;
	bool insiderLoader = (frameCountLoader == 1);
	int currTagIndex = m_tags.GetFObjCount() - 1;
	//bool putCameraClip = false;
	//aggiungo l'istruzione per evitare preloader se e' gia
	//caricato
	if (m_currFrameIndex == 1) {
		if (m_properties.m_preloader.getValue() && frameCountLoader >= 1)
			addSkipLoader(frameCountLoader + 1);

		if (m_properties.m_url.getValue().length() > 0)
			addUrlLink(toString(m_properties.m_url.getValue()));
	}

	if (m_currFrameIndex > m_soundOffset)
		addSoundToFrame(isLast);

	double scaleFactor;

	std::map<const TImage *, USHORT>::iterator it;
	bool lastOneIsNotMasked = true;
	TImageP currMask = 0;
	int currMaskDepth;
	bool animatedPalette = false;

	for (depth = 0; depth < tmax((int)m_frameData->size(), oldSize); depth++) {

		FCXFormWAlpha *form = 0;

		if (depth < (int)m_frameData->size()) {
			TImageP img = (*m_frameData)[depth].m_img;

			if ((*m_frameData)[depth].m_isMask) {
				currMask = img;
				currMaskDepth = depth;
				lastOneIsNotMasked = true;
				continue;
			} else if ((*m_frameData)[depth].m_isMasked && lastOneIsNotMasked) {
				//assert(currMask);
				lastOneIsNotMasked = false;
				int numMasked = 1;
				while (depth + numMasked < (int)m_frameData->size() && (*m_frameData)[depth + numMasked].m_isMasked)
					numMasked++;
				if (m_oldFrameData && currMaskDepth < (int)m_oldFrameData->size())
					m_tags.AddFObj(new FCTRemoveObject2(currMaskDepth + 3));
				if (currMaskDepth < (int)m_frameData->size() && currMask) //&&
																		  //(currMask->getType()!=TImage::VECTOR || ((TVectorImage*)currMask)->getRegionCount()>0))
				{
					UINT id = buildImage(currMask, tf, scaleFactor, true);
					assert(scaleFactor >= 1.0);
					TAffine aff = TTranslation(0.5 * m_lx, -0.5 * m_ly) * m_globalScale * TScale(scaleFactor);
					m_tags.AddFObj(new FCTPlaceObject2(true,  // hasClipDepth
													   false, // hasRatio,
													   true,  // hasCharId,
													   false, //i!=0,   // hasMove,
													   currMaskDepth + 3, id, affine2Matrix(aff),
													   0, 0, 0, currMaskDepth + 3 + numMasked));
				}

			} else if (!(*m_frameData)[depth].m_isMasked)
				lastOneIsNotMasked = true;
			TVectorImageP vimg = (TVectorImageP)img;
			if (vimg) {
				assert(vimg->getPalette());
				animatedPalette = vimg->getPalette()->isAnimated();
			}
		}

		if (m_oldFrameData && depth < (int)m_oldFrameData->size() && depth < (int)m_frameData->size() &&
			(*m_frameData)[depth].m_img.getPointer() == (*m_oldFrameData)[depth].m_img.getPointer() && !animatedPalette) {
			TImageP img = (*m_frameData)[depth].m_img;
			const TColorFunction *ct = (*m_frameData)[depth].m_cf;
			TColorFunction::Parameters p;
			if (ct && ct->getParameters(p))
				form = new FCXFormWAlpha(1, 1, (TINT32)(p.m_mR * 256), (TINT32)(p.m_mG * 256), (TINT32)(p.m_mB * 256), (TINT32)(p.m_mM * 256),
										 (TINT32)(p.m_cR), (TINT32)(p.m_cG), (TINT32)(p.m_cB), (TINT32)(p.m_cM));

			if ((*m_frameData)[depth].m_aff != (*m_oldFrameData)[depth].m_aff) {
				std::map<const TImage *, double>::iterator it1;
				it1 = m_imagesScaleMap.find(img.getPointer());
				assert(it1 != m_imagesScaleMap.end());
				scaleFactor = (*it1).second;

				TAffine aff = TTranslation(0.5 * m_lx, -0.5 * m_ly) * m_globalScale * (*m_frameData)[depth].m_aff * TScale(scaleFactor);
				/*if (m_notClipped && !putCameraClip)
			  {
        TRectD box = aff*img->getBBox();
			  if (box.x0<0 || -box.y1<0 || box.x1>m_lx || -box.y0>m_ly)
			    putCameraClip = true;
        }*/
				m_tags.AddFObj(new FCTPlaceObject2(false, // hasClipDepth
												   false, // hasRatio,
												   false, // hasCharId,
												   true,  //i!=0,   // hasMove,
												   depth + 3, 0, affine2Matrix(aff),
												   form, 0, 0, 0));
			}
		} else {
			if (m_oldFrameData && depth < (int)m_oldFrameData->size())
				m_tags.AddFObj(new FCTRemoveObject2(depth + 3));
			if (depth < (int)m_frameData->size()) {
				TImageP img = (*m_frameData)[depth].m_img;
				const TColorFunction *cf = (*m_frameData)[depth].m_cf;
				TColorFunction::Parameters p;
				if (cf && cf->getParameters(p))
					form = new FCXFormWAlpha(1, 1, (TINT32)(p.m_mR * 256), (TINT32)(p.m_mG * 256), (TINT32)(p.m_mB * 256), (TINT32)(p.m_mM * 256),
											 (TINT32)(p.m_cR), (TINT32)(p.m_cG), (TINT32)(p.m_cB), (TINT32)(p.m_cM));
				it = m_keepImages ? m_imagesMap.find(img.getPointer()) : m_imagesMap.end();
				USHORT id;
				if (it == m_imagesMap.end()) {
					id = buildImage(img, tf, scaleFactor, false);
					if (!animatedPalette) {
						m_imagesMap[img.getPointer()] = id;
						m_imagesScaleMap[img.getPointer()] = scaleFactor;
					}
				} else {
					id = (*it).second;
					std::map<const TImage *, double>::iterator it1;
					it1 = m_imagesScaleMap.find(img.getPointer());
					assert(it1 != m_imagesScaleMap.end());
					scaleFactor = (*it1).second;
				}
				assert(scaleFactor >= 1.0);
				TAffine aff = TTranslation(0.5 * m_lx, -0.5 * m_ly) * m_globalScale * (*m_frameData)[depth].m_aff * TScale(scaleFactor);
				/*if (m_notClipped && !putCameraClip)
			  {
        TRectD box = img->getBBox();

			  box = aff*box;

			  if (box.x0<0 || -box.y1<0 || box.x1>m_lx || -box.y0>m_ly)
			    putCameraClip = true;
        }*/

				/*Commento al seguente if:
		questa cosa e' davvero FOLLE. C'era un baco per cui se facevi rewind 
		nel flash player mentre era in play restavano appese alcune immagini. 
		Dopo indagini e confronti con gli swf generati da flash mx, ho scoperto che , quando si fa place di una sprite 
		ad una certa depth, sostituendo un'altra sprite alla stessa depth, mx mette hasRatio=true con un valore 
		di ratio pari a 0x8!!!! Non capisco, ma lo faccio anch'io e funziona, don't ask me why.  Vincenzo. */

				int ratioVal = 0;
				if (m_oldFrameData && (int)m_oldFrameData->size() > depth && ((*m_oldFrameData)[depth].m_img))
					ratioVal = 0x8;

				m_tags.AddFObj(new FCTPlaceObject2(false,		 // hasClipDepth
												   ratioVal > 0, // hasRatio,
												   true,		 // hasCharId,
												   false,		 //i!=0,   // hasMove,
												   depth + 3, id, affine2Matrix(aff),
												   form, ratioVal, 0, 0));
			}
		}
	}

	//inserisce lo stop sull'ultimo frame
	if (isLast && lastScene && !m_properties.m_looping.getValue()) {
		addActionStop();
		//loaderAdded = false;
	}

	m_tags.AddFObj(new FCTShowFrame());

	//if (putCameraClip)
	//addCameraClip(currTagIndex);

	//ho una sola scena => loader interno I frame oppure
	//ci sono piu' scene => la I fa da loader
	if (m_properties.m_preloader.getValue() && !m_loaderAdded && (insiderLoader || (isLast && frameCountLoader > 1))) {
		for (depth = 0; depth < (int)m_frameData->size(); depth++)
			m_tags.AddFObj(new FCTRemoveObject2(depth + 3));
		if (m_oldFrameData)
			delete m_oldFrameData;
		m_oldFrameData = m_frameData;
		m_frameData = 0;
		addLoader();
		m_tags.AddFObj(new FCTShowFrame());
		m_currFrameIndex++;
		//inserisce lo stop al primo frame che segue il loader
		if (!m_properties.m_autoplay.getValue() && loaderAdded && m_currFrameIndex == frameCountLoader + 1)
			addPause();
		//m_properties.m_preloader = false;
		m_loaderAdded = true;
	}
	//else if (!m_properties.m_autoplay.getValue())
	// addPauseAtStart();

	if (m_frameData && isLast && m_currFrameIndex != 1)
		for (depth = 0; depth < (int)m_frameData->size(); depth++)
			m_tags.AddFObj(new FCTRemoveObject2(depth + 3));
}

//-------------------------------------------------------------------

void TFlash::Imp::addActionStop()
{
	//construct an empty CTDoAction tag object
	FCTDoAction *doAction = new FCTDoAction();

	//add the stop action to the CTDoAction tag object
	doAction->AddAction(new FActionStop());

	//add the CTDoAction object to allTags
	m_tags.AddFObj(doAction);
}

//-------------------------------------------------------------------
void TFlash::Imp::addLoader()
{
	string target = "";
	//construct an empty CTDoAction tag object
	FCTDoAction *doAction = new FCTDoAction();

	doAction->AddAction(new FActionGotoFrame((U16)0));
	doAction->AddAction(new FActionPlay());

	//add the CTDoAction object to allTags
	m_tags.AddFObj(doAction);
}

//-------------------------------------------------------------------

void TFlash::Imp::addSkipLoader(int jumpToFrame)
{
	string target = "";
	//construct an empty CTDoAction tag object
	FCTDoAction *doAction = new FCTDoAction();

	//controlla se il file e' stato tutto caricato
	doAction->AddAction(new FActionPush(new FString((U8 *)target.c_str())));
	doAction->AddAction(new FActionPush(7, FLOAT(12)));
	doAction->AddAction(new FActionGetProperty());

	doAction->AddAction(new FActionPush(new FString((U8 *)target.c_str())));
	doAction->AddAction(new FActionPush(7, FLOAT(5)));
	doAction->AddAction(new FActionGetProperty());

	doAction->AddAction(new FActionEquals2());
	doAction->AddAction(new FActionNot());
	doAction->AddAction(new FActionIf(5 + 1));
	doAction->AddAction(new FActionGotoFrame((U16)jumpToFrame));
	doAction->AddAction(new FActionPlay());

	//add the CTDoAction object to allTags
	m_tags.AddFObj(doAction);
}

//-------------------------------------------------------------------

void TFlash::Imp::addPause()
{
	addActionStop();

	FRect *clipRectBounds = new FRect(0, 0, m_lx * m_tw, m_ly * m_tw); //coordinate values are in TWIPS

	FDTDefineShape3 *clipRectangle = new FDTDefineShape3(clipRectBounds);

	FColor black = FColor(0, 0, 0);

	U32 blackfillID = clipRectangle->AddSolidFillStyle(new FColor(black));
	clipRectangle->FinishStyleArrays();
	addShape(clipRectangle, false, true, true, false, true, 0, 0, blackfillID, 0);

	addEdgeStraightToShape(clipRectangle, 0, m_ly * m_tw);
	addEdgeStraightToShape(clipRectangle, m_lx * m_tw, 0);
	addEdgeStraightToShape(clipRectangle, 0, -m_ly * m_tw);
	addEdgeStraightToShape(clipRectangle, -m_lx * m_tw, 0);
	clipRectangle->AddShapeRec(new FShapeRecEnd());

	m_tags.AddFObj(clipRectangle);

	FDTDefineButton2 *theButton = new FDTDefineButton2(0); //flag is 0 to indicate a push button
	FMatrix *mx = new FMatrix();
	FCXFormWAlpha *cxf = new FCXFormWAlpha(false, false, 0, 0, 0, 0, 0, 0, 0, 0);
	FButtonRecord2 *bRec = new FButtonRecord2(true, false, false, false, clipRectangle->ID(), 1, mx, cxf);
	theButton->AddButtonRecord(bRec);

	//the button action
	FActionCondition *ac = new FActionCondition();
	ac->AddCondition(OverDownToOverUp);
	ac->AddActionRecord(new FActionPlay());
	theButton->AddActionCondition(ac);

	m_tags.AddFObj(theButton);

	FMatrix *matrix = new FMatrix();
	// la depth e' 2 perche' riservata per il bottone
	FCTPlaceObject2 *placeButton = new FCTPlaceObject2(false, // ~ _hasClipDepth
													   true, true, false,
													   2, theButton->ID(), matrix, 0, 0, 0, 0);
	m_tags.AddFObj(placeButton);
}

//-------------------------------------------------------------------

void TFlash::Imp::addUrlLink(const string _url)
{
	//addActionStop();

	string url;
	if (url.find("http") == -1)
		url = "http://" + _url;
	else
		url = _url;

	FRect *clipRectBounds = new FRect(0, 0, m_lx * m_tw, m_ly * m_tw); //coordinate values are in TWIPS

	FDTDefineShape3 *clipRectangle = new FDTDefineShape3(clipRectBounds);

	FColor black = FColor(0, 0, 0, 0);

	U32 blackfillID = clipRectangle->AddSolidFillStyle(new FColor(black));
	clipRectangle->FinishStyleArrays();
	addShape(clipRectangle, false, true, true, false, true, 0, 0, blackfillID, 0);

	addEdgeStraightToShape(clipRectangle, 0, m_ly * m_tw);
	addEdgeStraightToShape(clipRectangle, m_lx * m_tw, 0);
	addEdgeStraightToShape(clipRectangle, 0, -m_ly * m_tw);
	addEdgeStraightToShape(clipRectangle, -m_lx * m_tw, 0);
	clipRectangle->AddShapeRec(new FShapeRecEnd());

	m_tags.AddFObj(clipRectangle);

	FDTDefineButton2 *theButton = new FDTDefineButton2(0); //flag is 0 to indicate a push button
	FMatrix *mx = new FMatrix();
	FCXFormWAlpha *cxf = new FCXFormWAlpha(false, false, 0, 0, 0, 0, 0, 0, 0, 0);
	FButtonRecord2 *bRec = new FButtonRecord2(true, false, false, false, clipRectangle->ID(), 1, mx, cxf);
	theButton->AddButtonRecord(bRec);

	//the button action
	FActionCondition *ac = new FActionCondition();
	ac->AddCondition(OverDownToOverUp);
	ac->AddActionRecord(new FActionGetURL(new FString(url.c_str()), new FString("")));
	theButton->AddActionCondition(ac);

	m_tags.AddFObj(theButton);

	FMatrix *matrix = new FMatrix();
	// la depth e' 2 perche' riservata per il bottone
	FCTPlaceObject2 *placeButton = new FCTPlaceObject2(false, // ~ _hasClipDepth
													   true, true, false,
													   2, theButton->ID(), matrix, 0, 0, 0, 0);
	m_tags.AddFObj(placeButton);
}

//-------------------------------------------------------------------

void TFlash::Imp::beginMask()
{
	//m_currMask = TVectorImageP();

	m_currMask = new TVectorImage();
	m_currMask->setPalette(new TPalette());
}

//-------------------------------------------------------------------

void TFlash::Imp::endMask()
{
	m_frameData->push_back(FlashImageData(TAffine(), m_currMask, 0, true, false));
	m_currMask = TVectorImageP();
}

//-------------------------------------------------------------------

bool TFlash::Imp::drawOutline(TStroke *s, bool separeDifferentColors)
{
	if (m_polyData.m_color1 != m_currStrokeColor) {
		//bool inserted = m_outlineColors.insert(m_polyData.m_color1).second;
		if (!m_outlines.empty()) {
			computeOutlineBoundary(m_outlines, m_polylinesArray, m_currStrokeColor);
		}
		if (!m_polylinesArray.empty() && (separeDifferentColors /* || !inserted*/)) //non posso accorpare strokes di colore diverso....flash macromedia non le importa bene! lo stronzo.
			drawHangedObjects();
		m_currStrokeColor = m_polyData.m_color1;
	}
	m_outlines.push_back(s);
	return true;
}

//===================================================================
TFlash::TFlash(int lx, int ly, int frameCount, int frameRate, TPropertyGroup *properties, bool keepImages)
	: m_imp(new Imp(lx, ly, frameCount, frameRate, properties, keepImages))
{
}

//-------------------------------------------------------------------

TFlash::~TFlash()
{
	delete m_imp;
}

//-------------------------------------------------------------------

void TFlash::setBackgroundColor(const TPixel32 &bgColor)
{
	FCTSetBackgroundColor *background =
		new FCTSetBackgroundColor(new FColor(bgColor.r, bgColor.g, bgColor.b));
	m_imp->m_tags.AddFObj(background);
}

//-------------------------------------------------------------------

void TFlash::setThickness(double thickness)
{
	m_imp->m_thickness = thickness;
}

//-------------------------------------------------------------------

void TFlash::setFillColor(const TPixel32 &color)
{
	m_imp->m_polyData.m_color1 = color;
	m_imp->m_polyData.m_type = Solid;
}

//-------------------------------------------------------------------

void TFlash::setLineColor(const TPixel32 &color)
{
	m_imp->m_lineColor = color;
}

//-------------------------------------------------------------------

void TFlash::setTexture(const TRaster32P &texture)
{
	m_imp->m_polyData.m_type = Texture;
	m_imp->m_polyData.m_texture = texture;
}

//-------------------------------------------------------------------

void TFlash::setGradientFill(bool isLinear, const TPixel &color1, const TPixel &color2, double smooth)
{
	m_imp->m_polyData.m_type = (isLinear) ? LinearGradient : RadialGradient;
	m_imp->m_polyData.m_color1 = color1;
	m_imp->m_polyData.m_color2 = color2;
	m_imp->m_polyData.m_smooth = smooth;
}

//-------------------------------------------------------------------

void TFlash::setFillStyleMatrix(const TAffine &aff)
{
	m_imp->m_polyData.m_matrix = aff;
}

//-------------------------------------------------------------------

void TFlash::drawSegments(const vector<TSegment> segmentArray, bool isGradientColor)
{
	m_imp->drawSegments(segmentArray, isGradientColor);
}

//-------------------------------------------------------------------

void TFlash::drawquads(const vector<TQuadratic> quadArray)
{
	m_imp->drawquads(quadArray);
}

//-------------------------------------------------------------------

void TFlash::cleanCachedImages()
{
	m_imp->m_imagesMap.clear();
	m_imp->m_imagesScaleMap.clear();
	m_imp->m_texturesMap.clear();
}

//===================================================================

void TFlash::drawCenterline(const TStroke *s, bool drawAll)
{
	//vector<TQuadratic*> quads;
	int i;
	double thickness;

	if (m_imp->m_thickness > 0)
		thickness = m_imp->m_thickness;
	else if (m_imp->m_properties.m_lineQuality.getValue() != ConstantLines)
		thickness = computeAverageThickness(s);
	else
		thickness = s->getAverageThickness();

	if (m_imp->m_lineColor.m == 0 || thickness == 0)
		return;

	if (m_imp->m_properties.m_lineQuality.getValue() != ConstantLines && m_imp->m_lineColor != m_imp->m_currStrokeColor) {
		drawHangedObjects();
		//m_imp->m_outlineColors.insert(m_imp->m_lineColor).second;
		m_imp->m_currStrokeColor = m_imp->m_lineColor;
	}

	FlashPolyline quads;
	if (s->getChunkCount() == 1 &&
		norm2(m_imp->toTwips(s->getChunk(0)->getP2()) - m_imp->toTwips(s->getChunk(0)->getP0())) <= 4 //metto lo stile di fill: i punti si disegnano come cerchi
		&& thickness > 0) {
		quads.m_isPoint = true;
		quads.m_fillStyle1.m_type = Solid;
		quads.m_fillStyle1.m_color1 = m_imp->m_lineColor;
	}

	quads.m_lineStyle.m_type = Centerline;
	quads.m_lineStyle.m_thickness = thickness;
	quads.m_lineStyle.m_color1 = m_imp->m_lineColor;

	//quads.m_lineStyle.m_isRegion = false;
	//quads.m_lineStyle.m_isHole = false;

	std::map<const TStroke *, std::set<wChunk>>::iterator it = m_imp->m_strokeMap.end();

	if (!drawAll)
		it = m_imp->m_strokeMap.find(s);

	if (it == m_imp->m_strokeMap.end()) {
		for (i = 0; i < s->getChunkCount(); i++)
			quads.m_quads.push_back((TQuadratic *)s->getChunk(i));
		m_imp->m_polylinesArray.push_back(quads);
	} else {
		std::set<wChunk>::iterator it1;
		double oldW1 = 0;
		for (it1 = it->second.begin(); it1 != it->second.end(); it1++) {
			if (it1->w0 > oldW1) {
				putquads(s, oldW1, it1->w0, quads.m_quads);
				m_imp->m_polylinesArray.push_back(quads);
				quads.m_quads.clear();
			}
			oldW1 = it1->w1;
		}
		if (oldW1 < 1.0) {
			putquads(s, oldW1, 1.0, quads.m_quads);
			m_imp->m_polylinesArray.push_back(quads);
			quads.m_quads.clear();
		}
	}
}
void TFlash::drawHangedObjects()
{
	m_imp->drawHangedObjects();
}
//-------------------------------------------------------------------

int TFlash::drawRectangle(const TRectD &rect)
{
	m_imp->drawHangedObjects();

	vector<TPointD> v;

	v.push_back(rect.getP00());
	v.push_back(rect.getP01());
	v.push_back(rect.getP11());
	v.push_back(rect.getP10());

	return m_imp->drawPolyline(v);
}

//-------------------------------------------------------------------

int TFlash::drawPolyline(vector<TPointD> &poly)
{
	m_imp->drawHangedObjects();

	return m_imp->drawPolyline(poly);
}

//-------------------------------------------------------------------

void TFlash::drawPolygon(vector<vector<TQuadratic *>> &quads, int clippedShapes)
{
	m_imp->drawHangedObjects();

	//std::list<Polyline>::iterator it = polylines.begin(), it_e = polylines.end();
	list<FlashPolyline> polylines;

	for (int i = 0; i < (int)quads.size(); i++) {
		polylines.push_back(FlashPolyline());
		polylines.back().m_quads = quads[i];
		polylines.back().m_fillStyle1 = m_imp->m_polyData;
	}

	m_imp->doDrawPolygon(polylines, clippedShapes);
}

//-------------------------------------------------------------------
void TFlash::drawDot(const TPointD &center, double radius)
{
	m_imp->drawDot(center, radius);
}

//-------------------------------------------------------------------

int TFlash::drawEllipse(const TPointD &center, double radiusX, double radiusY)
{
	m_imp->drawHangedObjects();

	return m_imp->drawEllipse(center, radiusX, radiusY);
}

//-------------------------------------------------------------------

void TFlash::pushMatrix()
{
	m_imp->m_matrixStack.push_back(m_imp->m_affine);
}

//-------------------------------------------------------------------

void TFlash::popMatrix()
{
	assert(!m_imp->m_matrixStack.empty());
	m_imp->m_affine = m_imp->m_matrixStack.back();
	m_imp->m_matrixStack.pop_back();
}

//-------------------------------------------------------------------

void TFlash::multMatrix(const TAffine &aff)
{
	m_imp->m_affine *= aff;
}

//-------------------------------------------------------------------

void TFlash::putSound(TSoundTrackP st, int offset)
{
	m_imp->m_soundSize = 0;
	m_imp->m_sound.clear();

	TSoundTrackP st1 = st;

	if (st1->getBitPerSample() != c_soundBps ||
		st1->isSampleSigned() != c_soundIsSigned ||
		st1->getChannelCount() != c_soundChannelNum)
		st1 = TSop::convert(st, TSoundTrackFormat(m_imp->m_soundRate, c_soundBps, c_soundChannelNum, c_soundIsSigned));
	else if (st1->getSampleRate() != (UINT)m_imp->m_soundRate)
		st1 = TSop::resample(st1, m_imp->m_soundRate);

	int frameCount = st1->getSampleCount() / (st1->getSampleRate() / m_imp->m_frameRate);
	if (!frameCount)
		return;
	int averagePerFrame = st1->getSampleCount() / frameCount;
	int sampleCount = st1->getSampleCount();
	TINT32 firstSample = 0;
	m_imp->m_soundOffset = offset;

	while (sampleCount > 0) {
		sampleCount -= averagePerFrame;
		if (sampleCount >= 0) {
			TSoundTrackP snd = st1->extract(firstSample, firstSample + averagePerFrame - 1);
			m_imp->m_sound.push_back(snd);
			firstSample += averagePerFrame;
		} else {
			//deversamente dalla documentazione anche x il raw data deve essere
			//sempre lo stesso il numero di campioni per ogni blocco aggiunto
			//allo streaming audio
			TSoundTrackP snd = st1->extract(firstSample, st1->getSampleCount() - 1);
			snd = TSop::insertBlank(snd, snd->getSampleCount(), abs(sampleCount));
			m_imp->m_sound.push_back(snd);
		}
	}
	m_imp->m_soundSize = m_imp->m_sound.size();
}

//-------------------------------------------------------------------

void TFlash::writeMovie(FILE *fp)
{
	//m_imp->writeFrame(this, true);

	//if (m_imp->m_putCameraClip)
	//  m_imp->addCameraClip();

	if (m_imp->m_properties.m_isCompressed.getValue())
		m_imp->m_tags.CreateCompressedMovie(fp,
											FRect(0, 0, m_imp->m_lx * m_imp->m_tw,
												  m_imp->m_ly * m_imp->m_tw),
											m_imp->m_frameRate);
	else
		m_imp->m_tags.CreateMovie(fp,
								  FRect(0, 0, m_imp->m_lx * m_imp->m_tw,
										m_imp->m_ly * m_imp->m_tw),
								  m_imp->m_frameRate,
								  m_imp->m_version);
}

//-------------------------------------------------------------------
void TFlash::drawRegion(const TRegion &r, int clippedShapes)
{
	int i;
	vector<FlashPolyline> polylines;
	TPointD firstPoint, lastPoint;

	if (clippedShapes > 0 || m_imp->m_isMask) {
		if (clippedShapes > 0)
			m_imp->drawHangedObjects();

		//setFillColor(TPixel::Black);
	}

	TPointD pBegin, pEnd, pBeginRegion;

	pEnd = r.getEdge(r.getEdgeCount() - 1)->m_s->getPoint(r.getEdge(r.getEdgeCount() - 1)->m_w1);
	for (i = 0; i < (int)r.getEdgeCount(); i++)
		m_imp->addEdge(*r.getEdge(i), pBegin, pEnd);

	//m_imp->closeRegion(r.getEdgeCount());

	m_imp->m_regionDepth++;

	for (i = 0; i < (int)r.getSubregionCount(); i++) {
		TRegion &rr = *r.getSubregion(i);
		pEnd = rr.getEdge(0)->m_s->getPoint(rr.getEdge(0)->m_w0);
		for (int j = rr.getEdgeCount() - 1; j >= 0; j--) {
			TEdge *e = rr.getEdge(j);
			tswap(e->m_w0, e->m_w1);
			m_imp->addEdge(*e, pBegin, pEnd);
			tswap(e->m_w0, e->m_w1);
		}
	}

	m_imp->m_regionDepth--;

	if (clippedShapes > 0) {
		m_imp->doDrawPolygon(m_imp->m_polylinesArray, clippedShapes);
		//clearPointerContainer(m_imp->m_polylinesArray);
		m_imp->m_polylinesArray.clear();
		m_imp->m_edgeMap.clear();
		m_imp->m_strokeMap.clear();
		m_imp->m_autocloseMap.clear();
	}
}

//-------------------------------------------------------------------

USHORT TFlash::buildImage(const TImageP img, bool isMask)
{
	double scalefactor;
	return m_imp->buildImage(img, this, scalefactor, isMask);
	assert(scalefactor == 1.0);
}

void TFlash::beginFrame(int frameIndex)
{
	m_imp->m_frameData = new TFlash::Imp::FrameData;
	m_imp->m_currFrameIndex = frameIndex;
	assert(m_imp->m_matrixStack.empty());
	m_imp->m_affine = TAffine();
}

//-------------------------------------------------------------------
int TFlash::endFrame(bool isLast, int frameCountLoader, bool lastScene)
{
	m_imp->writeFrame(this, isLast, frameCountLoader, lastScene);
	if (m_imp->m_oldFrameData)
		delete m_imp->m_oldFrameData;
	m_imp->m_oldFrameData = m_imp->m_frameData;
	m_imp->m_frameData = 0;

	//QString msg = "mem: " + QString::number(m_imp->m_totMem/1024.0)+"\n";
	//TSystem::outputDebug(msg.toStdString());
	return m_imp->m_currFrameIndex;
}

//-------------------------------------------------------------------

//void TFlash::addPauseAtStart()
//{
//  m_imp->addPauseAtStart();
//}

//-------------------------------------------------------------------

void TFlash::enableMask()
{
	m_imp->m_maskEnabled = true;
}

//-------------------------------------------------------------------

void TFlash::disableMask()
{
	m_imp->m_maskEnabled = false;
}

//-------------------------------------------------------------------

void TFlash::beginMask()
{
	m_imp->beginMask();
}

//-------------------------------------------------------------------

void TFlash::endMask()
{
	m_imp->endMask();
}

//-------------------------------------------------------------------
void TFlash::draw(const TImageP img, const TColorFunction *cf)
{
	//std::map<const TVectorImage *, USHORT>::iterator it;

	if (m_imp->m_currMask) {
		if (img->getType() == TImage::VECTOR)
			m_imp->m_currMask->mergeImage((TVectorImageP)img, m_imp->m_affine);
	} else if (img)
		m_imp->m_frameData->push_back(FlashImageData(m_imp->m_affine, img, cf ? cf->clone() : 0, false, m_imp->m_maskEnabled));
}

//-------------------------------------------------------------------

wstring TFlash::getLineQuality()
{
	return m_imp->m_properties.m_lineQuality.getValue();
}

//-------------------------------------------------------------------

bool TFlash::drawOutline(TStroke *s)
{
	assert(m_imp->m_polyData.m_type == Solid);

	m_imp->drawHangedObjects();

	TThickPoint p0 = s->getThickPoint(0.0), p1 = s->getThickPoint(1.0);

	if (tdistance(p0, p1) < (p0.thick + p1.thick)) //pezza infame!nelle curve che si autochiudono, lo sweepboundary fa casini.
												   //le splitto in due pezzi, e per poter metterli nello steso poligono
												   //senza vedere le sovrapposizioni cambio leggermente il colore al secondo....eh eh eh
	{
		TStroke *s0 = new TStroke(), *s1 = new TStroke();

		s->split(0.5, *s0, *s1);
		m_imp->drawOutline(s0, true);
		int aux = m_imp->m_polyData.m_color1.b;
		m_imp->m_polyData.m_color1.b = (m_imp->m_polyData.m_color1.b == 255) ? 254 : m_imp->m_polyData.m_color1.b + 1;

		//m_imp->drawHangedObjects();
		m_imp->drawOutline(s1, false);
		m_imp->m_polyData.m_color1.b = aux;
		//m_imp->drawHangedObjects();
		m_imp->m_strokesToBeDeleted.push_back(s0);
		m_imp->m_strokesToBeDeleted.push_back(s1);
		return true;
	}

	return m_imp->drawOutline(s, true);
}

//-------------------------------------------------------------
