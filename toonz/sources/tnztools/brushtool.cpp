

#include "brushtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tooloptions.h"
#include "bluredbrush.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/rasterstrokegenerator.h"
#include "toonz/ttileset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzimageutils.h"
#include "toonz/palettecontroller.h"
#include "toonz/stage2.h"

// TnzCore includes
#include "tstream.h"
#include "tcolorstyles.h"
#include "tvectorimage.h"
#include "tenv.h"
#include "tregion.h"
#include "tstroke.h"
#include "tgl.h"
#include "trop.h"

// Qt includes
#include <QPainter>

using namespace ToolUtils;

TEnv::DoubleVar VectorBrushMinSize("InknpaintVectorBrushMinSize", 1);
TEnv::DoubleVar VectorBrushMaxSize("InknpaintVectorBrushMaxSize", 5);
TEnv::IntVar VectorCapStyle("InknpaintVectorCapStyle", 0);
TEnv::IntVar VectorJoinStyle("InknpaintVectorJoinStyle", 0);
TEnv::IntVar VectorMiterValue("InknpaintVectorMiterValue", 4);
TEnv::DoubleVar RasterBrushMinSize("InknpaintRasterBrushMinSize", 1);
TEnv::DoubleVar RasterBrushMaxSize("InknpaintRasterBrushMaxSize", 5);
TEnv::DoubleVar BrushAccuracy("InknpaintBrushAccuracy", 20);
TEnv::IntVar BrushSelective("InknpaintBrushSelective", 0);
TEnv::IntVar BrushBreakSharpAngles("InknpaintBrushBreakSharpAngles", 0);
TEnv::IntVar RasterBrushPencilMode("InknpaintRasterBrushPencilMode", 0);
TEnv::IntVar BrushPressureSensibility("InknpaintBrushPressureSensibility", 1);
TEnv::DoubleVar RasterBrushHardness("RasterBrushHardness", 100);

//-------------------------------------------------------------------

#define ROUNDC_WSTR L"round_cap"
#define BUTT_WSTR L"butt_cap"
#define PROJECTING_WSTR L"projecting_cap"
#define ROUNDJ_WSTR L"round_join"
#define BEVEL_WSTR L"bevel_join"
#define MITER_WSTR L"miter_join"
#define CUSTOM_WSTR L"<custom>"

//-------------------------------------------------------------------
//
// (Da mettere in libreria) : funzioni che spezzano una stroke
// nei suoi punti angolosi. Lo facciamo specialmente per limitare
// i problemi di fill.
//
//-------------------------------------------------------------------

//
// Split a stroke in n+1 parts, according to n parameter values
// Input:
//      stroke            = stroke to split
//      parameterValues[] = vector of parameters where I want to split the stroke
//                          assert: 0<a[0]<a[1]<...<a[n-1]<1
// Output:
//      strokes[]         = the split strokes
//
// note: stroke is unchanged
//

void split(
	TStroke *stroke,
	const std::vector<double> &parameterValues,
	std::vector<TStroke *> &strokes)
{
	TThickPoint p2;
	std::vector<TThickPoint> points;
	TThickPoint lastPoint = stroke->getControlPoint(0);
	int n = parameterValues.size();
	int chunk;
	double t;
	int last_chunk = -1, startPoint = 0;
	double lastLocT = 0;

	for (int i = 0; i < n; i++) {
		points.push_back(lastPoint);	   // Add first point of the stroke
		double w = parameterValues[i];	 // Global parameter. along the stroke 0<=w<=1
		stroke->getChunkAndT(w, chunk, t); // t: local parameter in the chunk-th quadratic

		if (i == 0)
			startPoint = 1;
		else {
			int indexAfterLastT =
				stroke->getControlPointIndexAfterParameter(parameterValues[i - 1]);
			startPoint = indexAfterLastT;
			if ((indexAfterLastT & 1) && lastLocT != 1)
				startPoint++;
		}
		int endPoint = 2 * chunk + 1;
		if (lastLocT != 1 && i > 0) {
			if (last_chunk != chunk || t == 1)
				points.push_back(p2); // If the last local t is not an extreme
									  // add the point p2
		}

		for (int j = startPoint; j < endPoint; j++)
			points.push_back(stroke->getControlPoint(j));

		TThickPoint p, A, B, C;
		p = stroke->getPoint(w);
		C = stroke->getControlPoint(2 * chunk + 2);
		B = stroke->getControlPoint(2 * chunk + 1);
		A = stroke->getControlPoint(2 * chunk);
		p.thick = A.thick;

		if (last_chunk != chunk) {
			TThickPoint p1 = (1 - t) * A + t * B;
			points.push_back(p1);
			p.thick = p1.thick;
		} else {
			if (t != 1) {
				// If the i-th cut point belong to the same chunk of the (i-1)-th cut point.
				double tInters = lastLocT / t;
				TThickPoint p11 = (1 - t) * A + t * B;
				TThickPoint p1 = (1 - tInters) * p11 + tInters * p;
				points.push_back(p1);
				p.thick = p1.thick;
			}
		}

		points.push_back(p);

		if (t != 1)
			p2 = (1 - t) * B + t * C;

		assert(points.size() & 1);

		// Add new stroke
		TStroke *strokeAdd = new TStroke(points);
		strokeAdd->setStyle(stroke->getStyle());
		strokeAdd->outlineOptions() = stroke->outlineOptions();
		strokes.push_back(strokeAdd);

		lastPoint = p;
		last_chunk = chunk;
		lastLocT = t;
		points.clear();
	}
	// Add end stroke
	points.push_back(lastPoint);

	if (lastLocT != 1)
		points.push_back(p2);

	startPoint = stroke->getControlPointIndexAfterParameter(parameterValues[n - 1]);
	if ((stroke->getControlPointIndexAfterParameter(parameterValues[n - 1]) & 1) && lastLocT != 1)
		startPoint++;
	for (int j = startPoint; j < stroke->getControlPointCount(); j++)
		points.push_back(stroke->getControlPoint(j));

	assert(points.size() & 1);
	TStroke *strokeAdd = new TStroke(points);
	strokeAdd->setStyle(stroke->getStyle());
	strokeAdd->outlineOptions() = stroke->outlineOptions();
	strokes.push_back(strokeAdd);
	points.clear();
}

//Compute Parametric Curve Curvature
//By Formula:
// k(t)=(|p'(t) x p''(t)|)/Norm2(p')^3
// p(t) is parametric curve
// Input:
//      dp  = First Derivate.
//      ddp = Second Derivate
// Output:
//      return curvature value.
//      Note: if the curve is a single point (that's dp=0) or it is a straight line (that's ddp=0) return 0

double curvature(TPointD dp, TPointD ddp)
{
	if (dp == TPointD(0, 0))
		return 0;
	else
		return fabs(cross(dp, ddp) / pow(norm2(dp), 1.5));
}

// Find the max curvature points of a stroke.
// Input:
//      stroke.
//      angoloLim =  Value (radians) of the Corner between two tangent vector.
//                   Up this value the two corner can be considered angular.
//      curvMaxLim = Value of the max curvature.
//                   Up this value the point can be considered a max curvature point.
// Output:
//      parameterValues = vector of max curvature parameter points

void findMaxCurvPoints(
	TStroke *stroke,
	const float &angoloLim,
	const float &curvMaxLim,
	std::vector<double> &parameterValues)
{
	TPointD tg1, tg2; // Tangent vectors

	TPointD dp, ddp; // First and Second derivate.

	parameterValues.clear();
	int cpn = stroke ? stroke->getControlPointCount() : 0;
	for (int j = 2; j < cpn; j += 2) {
		TPointD p0 = stroke->getControlPoint(j - 2);
		TPointD p1 = stroke->getControlPoint(j - 1);
		TPointD p2 = stroke->getControlPoint(j);

		TPointD q = p1 - (p0 + p2) * 0.5;

		// Search corner point
		if (j > 2) {
			tg2 = -p0 + p2 + 2 * q;		  // Tangent vector to this chunk at t=0
			double prod_scal = tg2 * tg1; // Inner product between tangent vectors at t=0.
			assert(tg1 != TPointD(0, 0) || tg2 != TPointD(0, 0));
			// Compute corner between two tangent vectors
			double angolo = acos(prod_scal / (pow(norm2(tg2), 0.5) * pow(norm2(tg1), 0.5)));

			//Add corner point
			if (angolo > angoloLim) {
				double w = getWfromChunkAndT(stroke, (UINT)(0.5 * (j - 2)), 0); //  transform lacal t to global t
				parameterValues.push_back(w);
			}
		}
		tg1 = -p0 + p2 - 2 * q; // Tangent vector to this chunk at t=1

		// End search corner point

		// Search max curvature point
		// Value of t where the curvature function has got an extreme.
		// (Point where first derivate is null)
		double estremo_int = 0;
		double t = -1;
		if (q != TPointD(0, 0)) {
			t = 0.25 * (2 * q.x * q.x + 2 * q.y * q.y - q.x * p0.x + q.x * p2.x - q.y * p0.y + q.y * p2.y) / (q.x * q.x + q.y * q.y);

			dp = -p0 + p2 + 2 * q - 4 * t * q; // First derivate of the curve
			ddp = -4 * q;					   // Second derivate of the curve
			estremo_int = curvature(dp, ddp);

			double h = 0.01;
			dp = -p0 + p2 + 2 * q - 4 * (t + h) * q;
			double c_dx = curvature(dp, ddp);
			dp = -p0 + p2 + 2 * q - 4 * (t - h) * q;
			double c_sx = curvature(dp, ddp);
			// Check the point is a max and not a minimum
			if (estremo_int < c_dx && estremo_int < c_sx) {
				estremo_int = 0;
			}
		}
		double curv_max = estremo_int;

		//Compute curvature at the extreme of interval [0,1]
		//Compute curvature at t=0 (Left extreme)
		dp = -p0 + p2 + 2 * q;
		double estremo_sx = curvature(dp, ddp);

		//Compute curvature at t=1 (Right extreme)
		dp = -p0 + p2 - 2 * q;
		double estremo_dx = curvature(dp, ddp);

		// Compare curvature at the extreme of interval [0,1] with the internal value
		double t_ext;
		if (estremo_sx >= estremo_dx)
			t_ext = 0;
		else
			t_ext = 1;
		double maxEstremi = tmax(estremo_dx, estremo_sx);
		if (maxEstremi > estremo_int) {
			t = t_ext;
			curv_max = maxEstremi;
		}

		// Add max curvature point
		if (t >= 0 && t <= 1 && curv_max > curvMaxLim) {
			double w = getWfromChunkAndT(stroke, (UINT)(0.5 * (j - 2)), t); // transform local t to global t
			parameterValues.push_back(w);
		}
		// End search max curvature point
	}
	// Delete duplicate of parameterValues
	// Because some max cuvature point can coincide with the corner point
	if ((int)parameterValues.size() > 1) {
		std::sort(parameterValues.begin(), parameterValues.end());
		parameterValues.erase(std::unique(parameterValues.begin(), parameterValues.end()), parameterValues.end());
	}
}

void addStroke(TTool::Application *application, const TVectorImageP &vi, TStroke *stroke, bool breakAngles,
			   bool frameCreated, bool levelCreated)
{
	QMutexLocker lock(vi->getMutex());

	if (application->getCurrentObject()->isSpline()) {
		application->getCurrentXsheet()->notifyXsheetChanged();
		return;
	}

	std::vector<double> corners;
	std::vector<TStroke *> strokes;

	const float angoloLim = 1;	// Value (radians) of the Corner between two tangent vector.
								  // Up this value the two corner can be considered angular.
	const float curvMaxLim = 0.8; // Value of the max curvature.
								  // Up this value the point can be considered a max curvature point.

	findMaxCurvPoints(stroke, angoloLim, curvMaxLim, corners);
	TXshSimpleLevel *sl = application->getCurrentLevel()->getSimpleLevel();
	TFrameId id = application->getCurrentTool()->getTool()->getCurrentFid();

	if (!corners.empty()) {
		if (breakAngles)
			split(stroke, corners, strokes);
		else
			strokes.push_back(new TStroke(*stroke));

		int n = strokes.size();

		TUndoManager::manager()->beginBlock();
		for (int i = 0; i < n; i++) {
			std::vector<TFilledRegionInf> *fillInformation = new std::vector<TFilledRegionInf>;
			ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation, stroke->getBBox());
			TStroke *str = new TStroke(*strokes[i]);
			vi->addStroke(str);
			TUndoManager::manager()->add(new UndoPencil(str, fillInformation, sl, id, frameCreated, levelCreated));
		}
		TUndoManager::manager()->endBlock();
	} else {
		std::vector<TFilledRegionInf> *fillInformation = new std::vector<TFilledRegionInf>;
		ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation, stroke->getBBox());
		TStroke *str = new TStroke(*stroke);
		vi->addStroke(str);
		TUndoManager::manager()->add(new UndoPencil(str, fillInformation, sl, id, frameCreated, levelCreated));
	}

	for (int k = 0; k < (int)strokes.size(); k++)
		delete strokes[k];
	strokes.clear();

	application->getCurrentTool()->getTool()->notifyImageChanged();
}

//-------------------------------------------------------------------
//
// Gennaro: end
//
//-------------------------------------------------------------------

//===================================================================
//
// Helper functions and classes
//
//-------------------------------------------------------------------

namespace
{

//-------------------------------------------------------------------

void addStrokeToImage(TTool::Application *application, const TVectorImageP &vi, TStroke *stroke, bool breakAngles,
					  bool frameCreated, bool levelCreated)
{
	QMutexLocker lock(vi->getMutex());
	addStroke(application, vi.getPointer(), stroke, breakAngles, frameCreated, levelCreated);
	//la notifica viene gia fatta da addStroke!
	//getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
}

//=========================================================================================================

class RasterBrushUndo : public TRasterUndo
{
	std::vector<TThickPoint> m_points;
	int m_styleId;
	bool m_selective;
	bool m_isPencil;

public:
	RasterBrushUndo(TTileSetCM32 *tileSet,
					const std::vector<TThickPoint> &points,
					int styleId, bool selective,
					TXshSimpleLevel *level, const TFrameId &frameId, bool isPencil,
					bool isFrameCreated, bool isLevelCreated)
		: TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0), m_points(points), m_styleId(styleId), m_selective(selective), m_isPencil(isPencil)
	{
	}

	void redo() const
	{
		insertLevelAndFrameIfNeeded();
		TToonzImageP image = getImage();
		TRasterCM32P ras = image->getRaster();
		RasterStrokeGenerator m_rasterTrack(ras, BRUSH, NONE, m_styleId, m_points[0], m_selective, 0, !m_isPencil);
		m_rasterTrack.setPointsSequence(m_points);
		m_rasterTrack.generateStroke(m_isPencil);
		image->setSavebox(image->getSavebox() + m_rasterTrack.getBBox(m_rasterTrack.getPointsSequence()));
		ToolUtils::updateSaveBox();
		TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
		notifyImageChanged();
	}

	int getSize() const
	{
		return sizeof(*this) + TRasterUndo::getSize();
	}
	QString getToolName()
	{
		return QString("Brush Tool");
	}
	int getHistoryType()
	{
		return HistoryType::BrushTool;
	}
};

//=========================================================================================================

class RasterBluredBrushUndo : public TRasterUndo
{
	std::vector<TThickPoint> m_points;
	int m_styleId;
	bool m_selective;
	int m_maxThick;
	double m_hardness;

public:
	RasterBluredBrushUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
						  int styleId, bool selective, TXshSimpleLevel *level, const TFrameId &frameId,
						  int maxThick, double hardness, bool isFrameCreated, bool isLevelCreated)
		: TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0), m_points(points), m_styleId(styleId), m_selective(selective), m_maxThick(maxThick), m_hardness(hardness)
	{
	}

	void redo() const
	{
		if (m_points.size() == 0)
			return;
		insertLevelAndFrameIfNeeded();
		TToonzImageP image = getImage();
		TRasterCM32P ras = image->getRaster();
		TRasterCM32P backupRas = ras->clone();
		TRaster32P workRaster(ras->getSize());
		QRadialGradient brushPad = ToolUtils::getBrushPad(m_maxThick, m_hardness);
		workRaster->clear();
		BluredBrush brush(workRaster, m_maxThick, brushPad, false);

		std::vector<TThickPoint> points;
		points.push_back(m_points[0]);
		TRect bbox = brush.getBoundFromPoints(points);
		brush.addPoint(m_points[0], 1);
		brush.updateDrawing(ras, ras, bbox, m_styleId, m_selective);
		if (m_points.size() > 1) {
			points.clear();
			points.push_back(m_points[0]);
			points.push_back(m_points[1]);
			bbox = brush.getBoundFromPoints(points);
			brush.addArc(m_points[0], (m_points[1] + m_points[0]) * 0.5, m_points[1], 1, 1);
			brush.updateDrawing(ras, backupRas, bbox, m_styleId, m_selective);
			int i;
			for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
				points.clear();
				points.push_back(m_points[i]);
				points.push_back(m_points[i + 1]);
				points.push_back(m_points[i + 2]);
				bbox = brush.getBoundFromPoints(points);
				brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
				brush.updateDrawing(ras, backupRas, bbox, m_styleId, m_selective);
			}
		}
		ToolUtils::updateSaveBox();
		TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
		notifyImageChanged();
	}

	int getSize() const
	{
		return sizeof(*this) + TRasterUndo::getSize();
	}

	virtual QString getToolName()
	{
		return QString("Brush Tool");
	}
	int getHistoryType()
	{
		return HistoryType::BrushTool;
	}
};

//=========================================================================================================

double computeThickness(int pressure, const TDoublePairProperty &property, bool isPath)
{
	if (isPath)
		return 0.0;
	double p = pressure / 255.0;
	double t = p * p * p;
	double thick0 = property.getValue().first;
	double thick1 = property.getValue().second;
	if (thick1 < 0.0001)
		thick0 = thick1 = 0.0;
	return (thick0 + (thick1 - thick0) * t) * 0.5;
}

//---------------------------------------------------------------------------------------------------------

int computeThickness(int pressure, const TIntPairProperty &property, bool isPath)
{
	if (isPath)
		return 0.0;
	double p = pressure / 255.0;
	double t = p * p * p;
	int thick0 = property.getValue().first;
	int thick1 = property.getValue().second;
	return tround(thick0 + (thick1 - thick0) * t);
}

} // namespace

//===================================================================
//
// BrushTool
//
//-----------------------------------------------------------------------------

BrushTool::BrushTool(std::string name, int targetType)
	: TTool(name), m_thickness("Size", 0, 100, 0, 5), m_rasThickness("Size", 1, 100, 1, 5), m_accuracy("Accuracy:", 1, 100, 20), m_hardness("Hardness:", 0, 100, 100), m_preset("Preset:"), m_selective("Selective", false), m_breakAngles("Break", true), m_pencil("Pencil", false), m_pressure("Pressure", true), m_capStyle("Cap"), m_joinStyle("Join"), m_miterJoinLimit("Miter:", 0, 100, 4), m_rasterTrack(0), m_styleId(0), m_modifiedRegion(), m_bluredBrush(0), m_active(false), m_enabled(false), m_isPrompting(false), m_firstTime(true), m_presetsLoaded(false), m_workingFrameId(TFrameId())
{
	bind(targetType);

	if (targetType & TTool::Vectors) {
		m_prop[0].bind(m_thickness);
		m_prop[0].bind(m_accuracy);
		m_prop[0].bind(m_breakAngles);
		m_breakAngles.setId("BreakSharpAngles");
	}

	if (targetType & TTool::ToonzImage) {
		m_prop[0].bind(m_rasThickness);
		m_prop[0].bind(m_hardness);
		m_prop[0].bind(m_selective);
		m_prop[0].bind(m_pencil);
		m_pencil.setId("PencilMode");
		m_selective.setId("Selective");
	}

	m_prop[0].bind(m_pressure);
	m_prop[0].bind(m_preset);
	m_preset.setId("BrushPreset");
	m_preset.addValue(CUSTOM_WSTR);
	m_pressure.setId("PressureSensibility");

	m_capStyle.addValue(BUTT_WSTR);
	m_capStyle.addValue(ROUNDC_WSTR);
	m_capStyle.addValue(PROJECTING_WSTR);
	m_capStyle.setId("Cap");

	m_joinStyle.addValue(MITER_WSTR);
	m_joinStyle.addValue(ROUNDJ_WSTR);
	m_joinStyle.addValue(BEVEL_WSTR);
	m_joinStyle.setId("Join");

	m_miterJoinLimit.setId("Miter");

	if (targetType & TTool::Vectors) {
		m_prop[1].bind(m_capStyle);
		m_prop[1].bind(m_joinStyle);
		m_prop[1].bind(m_miterJoinLimit);
	}
}

//-------------------------------------------------------------------------------------------------------

ToolOptionsBox *BrushTool::createOptionsBox()
{
	TPaletteHandle *currPalette = TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
	ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
	return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//-------------------------------------------------------------------------------------------------------

void BrushTool::drawLine(const TPointD &point, const TPointD &centre, bool horizontal, bool isDecimal)
{
	if (!isDecimal) {
		if (horizontal) {
			tglDrawSegment(TPointD(point.x - 1.5, point.y + 0.5) + centre, TPointD(point.x - 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.y - 0.5, -point.x + 1.5) + centre, TPointD(point.y - 0.5, -point.x + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, -point.y + 0.5) + centre, TPointD(-point.x - 0.5, -point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre, TPointD(-point.y - 0.5, point.x + 0.5) + centre);

			tglDrawSegment(TPointD(point.y - 0.5, point.x + 0.5) + centre, TPointD(point.y - 0.5, point.x - 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 0.5, -point.y + 0.5) + centre, TPointD(point.x - 1.5, -point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre, TPointD(-point.y - 0.5, -point.x + 1.5) + centre);
			tglDrawSegment(TPointD(-point.x - 0.5, point.y + 0.5) + centre, TPointD(-point.x + 0.5, point.y + 0.5) + centre);
		} else {
			tglDrawSegment(TPointD(point.x - 1.5, point.y + 1.5) + centre, TPointD(point.x - 1.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 1.5, point.y + 0.5) + centre, TPointD(point.x - 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 0.5, -point.x + 1.5) + centre, TPointD(point.y - 0.5, -point.x + 1.5) + centre);
			tglDrawSegment(TPointD(point.y - 0.5, -point.x + 1.5) + centre, TPointD(point.y - 0.5, -point.x + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 0.5) + centre, TPointD(-point.x + 0.5, -point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, -point.y + 0.5) + centre, TPointD(-point.x - 0.5, -point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 1.5, point.x - 0.5) + centre, TPointD(-point.y - 0.5, point.x - 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre, TPointD(-point.y - 0.5, point.x + 0.5) + centre);

			tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre, TPointD(point.y - 0.5, point.x - 0.5) + centre);
			tglDrawSegment(TPointD(point.y - 0.5, point.x - 0.5) + centre, TPointD(point.y - 0.5, point.x + 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 1.5, -point.y - 0.5) + centre, TPointD(point.x - 1.5, -point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 1.5, -point.y + 0.5) + centre, TPointD(point.x - 0.5, -point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 1.5, -point.x + 1.5) + centre, TPointD(-point.y - 0.5, -point.x + 1.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 1.5) + centre, TPointD(-point.y - 0.5, -point.x + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, point.y + 1.5) + centre, TPointD(-point.x + 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre, TPointD(-point.x - 0.5, point.y + 0.5) + centre);
		}
	} else {
		if (horizontal) {
			tglDrawSegment(TPointD(point.x - 0.5, point.y + 0.5) + centre, TPointD(point.x + 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre, TPointD(point.y + 0.5, point.x + 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 0.5, -point.x + 0.5) + centre, TPointD(point.y + 0.5, -point.x - 0.5) + centre);
			tglDrawSegment(TPointD(point.x + 0.5, -point.y - 0.5) + centre, TPointD(point.x - 0.5, -point.y - 0.5) + centre);
			tglDrawSegment(TPointD(-point.x - 0.5, -point.y - 0.5) + centre, TPointD(-point.x + 0.5, -point.y - 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre, TPointD(-point.y - 0.5, -point.x - 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre, TPointD(-point.y - 0.5, point.x + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre, TPointD(-point.x - 0.5, point.y + 0.5) + centre);
		} else {
			tglDrawSegment(TPointD(point.x - 0.5, point.y + 1.5) + centre, TPointD(point.x - 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 0.5, point.y + 0.5) + centre, TPointD(point.x + 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 1.5, point.x - 0.5) + centre, TPointD(point.y + 0.5, point.x - 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre, TPointD(point.y + 0.5, point.x + 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 1.5, -point.x + 0.5) + centre, TPointD(point.y + 0.5, -point.x + 0.5) + centre);
			tglDrawSegment(TPointD(point.y + 0.5, -point.x + 0.5) + centre, TPointD(point.y + 0.5, -point.x - 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 0.5, -point.y - 1.5) + centre, TPointD(point.x - 0.5, -point.y - 0.5) + centre);
			tglDrawSegment(TPointD(point.x - 0.5, -point.y - 0.5) + centre, TPointD(point.x + 0.5, -point.y - 0.5) + centre);

			tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 1.5) + centre, TPointD(-point.x + 0.5, -point.y - 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 0.5) + centre, TPointD(-point.x - 0.5, -point.y - 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 1.5, -point.x + 0.5) + centre, TPointD(-point.y - 0.5, -point.x + 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre, TPointD(-point.y - 0.5, -point.x - 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 1.5, point.x - 0.5) + centre, TPointD(-point.y - 0.5, point.x - 0.5) + centre);
			tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre, TPointD(-point.y - 0.5, point.x + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, point.y + 1.5) + centre, TPointD(-point.x + 0.5, point.y + 0.5) + centre);
			tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre, TPointD(-point.x - 0.5, point.y + 0.5) + centre);
		}
	}
}

//-------------------------------------------------------------------------------------------------------

void BrushTool::drawEmptyCircle(TPointD pos, int thick, bool isLxEven, bool isLyEven, bool isPencil)
{
	if (isLxEven)
		pos.x += 0.5;
	if (isLyEven)
		pos.y += 0.5;

	if (!isPencil)
		tglDrawCircle(pos, (thick + 1) * 0.5);
	else {
		int x = 0, y = tround((thick * 0.5) - 0.5);
		int d = 3 - 2 * (int)(thick * 0.5);
		bool horizontal = true, isDecimal = thick % 2 != 0;
		drawLine(TPointD(x, y), pos, horizontal, isDecimal);
		while (y > x) {
			if (d < 0) {
				d = d + 4 * x + 6;
				horizontal = true;
			} else {
				d = d + 4 * (x - y) + 10;
				horizontal = false;
				y--;
			}
			x++;
			drawLine(TPointD(x, y), pos, horizontal, isDecimal);
		}
	}
}

//-------------------------------------------------------------------------------------------------------

void BrushTool::updateTranslation()
{
	m_thickness.setQStringName(tr("Size"));
	m_rasThickness.setQStringName(tr("Size"));
	m_hardness.setQStringName(tr("Hardness:"));
	m_accuracy.setQStringName(tr("Accuracy:"));
	m_selective.setQStringName(tr("Selective"));
	//m_filled.setQStringName(tr("Filled"));
	m_preset.setQStringName(tr("Preset:"));
	m_breakAngles.setQStringName(tr("Break"));
	m_pencil.setQStringName(tr("Pencil"));
	m_pressure.setQStringName(tr("Pressure"));
	m_capStyle.setQStringName(tr("Cap"));
	m_joinStyle.setQStringName(tr("Join"));
	m_miterJoinLimit.setQStringName(tr("Miter:"));
}

//---------------------------------------------------------------------------------------------------

void BrushTool::updateWorkAndBackupRasters(const TRect &rect)
{
	TToonzImageP ti = TImageP(getImage(false, 1));
	if (!ti)
		return;

	TRasterCM32P ras = ti->getRaster();

	TRect _rect = rect * ras->getBounds();
	TRect _lastRect = m_lastRect * ras->getBounds();

	if (_rect.isEmpty())
		return;

	if (m_lastRect.isEmpty()) {
		m_workRas->extract(_rect)->clear();
		m_backupRas->extract(_rect)->copy(ras->extract(_rect));
		return;
	}

	QList<TRect> rects = ToolUtils::splitRect(_rect, _lastRect);
	for (int i = 0; i < rects.size(); i++) {
		m_workRas->extract(rects[i])->clear();
		m_backupRas->extract(rects[i])->copy(ras->extract(rects[i]));
	}
}

//---------------------------------------------------------------------------------------------------

void BrushTool::onActivate()
{
	if (m_firstTime) {
		m_thickness.setValue(TDoublePairProperty::Value(VectorBrushMinSize, VectorBrushMaxSize));
		m_rasThickness.setValue(TDoublePairProperty::Value(RasterBrushMinSize, RasterBrushMaxSize));
		m_capStyle.setIndex(VectorCapStyle);
		m_joinStyle.setIndex(VectorJoinStyle);
		m_miterJoinLimit.setValue(VectorMiterValue);
		m_selective.setValue(BrushSelective ? 1 : 0);
		m_breakAngles.setValue(BrushBreakSharpAngles ? 1 : 0);
		m_pencil.setValue(RasterBrushPencilMode ? 1 : 0);
		m_pressure.setValue(BrushPressureSensibility ? 1 : 0);
		m_firstTime = false;
		m_accuracy.setValue(BrushAccuracy);
		m_hardness.setValue(RasterBrushHardness);
	}
	if (m_targetType & TTool::ToonzImage) {
		m_brushPad = ToolUtils::getBrushPad(m_rasThickness.getValue().second, m_hardness.getValue() * 0.01);
		setWorkAndBackupImages();
	}
	//TODO:app->editImageOrSpline();
}

//--------------------------------------------------------------------------------------------------

void BrushTool::onDeactivate()
{
	/*--- ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う ---*/
	if (m_tileSaver && !m_isPath) {
		bool isValid = m_enabled && m_active;
		m_enabled = false;
		if (isValid) {
			TImageP image = getImage(true);
			if (TToonzImageP ti = image)
				finishRasterBrush(m_mousePos, 1); /*-- 最後のストロークの筆圧は1とする --*/
		}
	}
	m_workRas = TRaster32P();
	m_backupRas = TRasterCM32P();
}

//--------------------------------------------------------------------------------------------------

bool BrushTool::preLeftButtonDown()
{
	touchImage();
	if (m_isFrameCreated)
		setWorkAndBackupImages();
	return true;
}

//--------------------------------------------------------------------------------------------------

void BrushTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e)
{
	TTool::Application *app = TTool::getApplication();
	if (!app)
		return;

	int col = app->getCurrentColumn()->getColumnIndex();
	m_isPath = app->getCurrentObject()->isSpline();
	m_enabled = col >= 0 || m_isPath;
	// todo: gestire autoenable
	if (!m_enabled)
		return;
	if (!m_isPath) {
		m_currentColor = TPixel32::Black;
		m_active = !!getImage(true);
		if (!m_active) {
			m_active = !!touchImage();
		}
		if (!m_active)
			return;

		if (m_active) {
			// nel caso che il colore corrente sia un cleanup/studiopalette color
			// oppure il colore di un colorfield
			m_styleId = app->getCurrentLevelStyleIndex();
			TColorStyle *cs = app->getCurrentLevelStyle();
			if (cs) {
				TRasterStyleFx *rfx = cs ? cs->getRasterStyleFx() : 0;
				m_active = cs != 0 && (cs->isStrokeStyle() || (rfx && rfx->isInkStyle()));
				m_currentColor = cs->getAverageColor();
				m_currentColor.m = 255;
			} else {
				m_styleId = 1;
				m_currentColor = TPixel32::Black;
			}
		}
	} else {
		m_currentColor = TPixel32::Red;
		m_active = true;
	}
	//assert(0<=m_styleId && m_styleId<2);
	TImageP img = getImage(true);
	TToonzImageP ri(img);
	if (ri) {
		TRasterCM32P ras = ri->getRaster();
		if (ras) {
			TPointD rasCenter = ras->getCenterD();
			m_tileSet = new TTileSetCM32(ras->getSize());
			m_tileSaver = new TTileSaverCM32(ras, m_tileSet);
			double maxThick = m_rasThickness.getValue().second;
			double thickness = (m_pressure.getValue() || m_isPath) ? computeThickness(e.m_pressure, m_rasThickness, m_isPath) * 2 : maxThick;

			/*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
			if (m_pressure.getValue() && e.m_pressure == 255)
				thickness = m_rasThickness.getValue().first;

			TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
			TRectD invalidateRect(pos - halfThick, pos + halfThick);
			if (m_hardness.getValue() == 100 || m_pencil.getValue()) {
				/*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる --*/
				if (!m_pencil.getValue())
					thickness -= 1.0;

				m_rasterTrack = new RasterStrokeGenerator(ras, BRUSH, NONE, m_styleId,
														  TThickPoint(pos + convert(ras->getCenter()), thickness),
														  m_selective.getValue(), 0, !m_pencil.getValue());
				m_tileSaver->save(m_rasterTrack->getLastRect());
				m_rasterTrack->generateLastPieceOfStroke(m_pencil.getValue());
			} else {
				m_points.clear();
				TThickPoint point(pos + rasCenter, thickness);
				m_points.push_back(point);
				m_bluredBrush = new BluredBrush(m_workRas, maxThick, m_brushPad, false);

				m_strokeRect = m_bluredBrush->getBoundFromPoints(m_points);
				updateWorkAndBackupRasters(m_strokeRect);
				m_tileSaver->save(m_strokeRect);
				m_bluredBrush->addPoint(point, 1);
				m_bluredBrush->updateDrawing(ri->getRaster(), m_backupRas, m_strokeRect, m_styleId, m_selective.getValue());
				m_lastRect = m_strokeRect;
			}
			/*-- 作業中のFidを登録 --*/
			m_workingFrameId = getFrameId();

			invalidate(invalidateRect);
		}
	} else {
		m_track.clear();
		double thickness = (m_pressure.getValue() || m_isPath) ? computeThickness(e.m_pressure, m_thickness, m_isPath) : m_thickness.getValue().second * 0.5;

		/*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
		if (m_pressure.getValue() && e.m_pressure == 255)
			thickness = m_rasThickness.getValue().first;

		m_track.add(TThickPoint(pos, thickness), getPixelSize() * getPixelSize());
	}
}

//-------------------------------------------------------------------------------------------------------------

void BrushTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e)
{
	m_brushPos = m_mousePos = pos;

	if (!m_enabled || !m_active)
		return;

	bool isAdded;

	if (TToonzImageP ti = TImageP(getImage(true))) {
		TPointD rasCenter = ti->getRaster()->getCenterD();
		int maxThickness = m_rasThickness.getValue().second;
		double thickness = (m_pressure.getValue() || m_isPath) ? computeThickness(e.m_pressure, m_rasThickness, m_isPath) * 2 : maxThickness;
		TRectD invalidateRect;
		if (m_rasterTrack && (m_hardness.getValue() == 100 || m_pencil.getValue())) {
			/*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる --*/
			if (!m_pencil.getValue())
				thickness -= 1.0;

			isAdded = m_rasterTrack->add(TThickPoint(pos + rasCenter, thickness));
			if (isAdded) {
				m_tileSaver->save(m_rasterTrack->getLastRect());
				m_rasterTrack->generateLastPieceOfStroke(m_pencil.getValue());
				std::vector<TThickPoint> brushPoints = m_rasterTrack->getPointsSequence();
				int m = (int)brushPoints.size();
				std::vector<TThickPoint> points;
				if (m == 3) {
					points.push_back(brushPoints[0]);
					points.push_back(brushPoints[1]);
				} else {
					points.push_back(brushPoints[m - 4]);
					points.push_back(brushPoints[m - 3]);
					points.push_back(brushPoints[m - 2]);
				}
				invalidateRect = ToolUtils::getBounds(points, maxThickness) - rasCenter;
			}
		} else {
			//antialiased brush
			assert(m_workRas.getPointer() && m_backupRas.getPointer());

			TThickPoint old = m_points.back();
			if (norm2(pos - old) < 4)
				return;

			TThickPoint point(pos + rasCenter, thickness);
			TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
			m_points.push_back(mid);
			m_points.push_back(point);

			TRect bbox;
			int m = (int)m_points.size();
			std::vector<TThickPoint> points;
			if (m == 3) {
				// ho appena cominciato. devo disegnare un segmento
				TThickPoint pa = m_points.front();
				points.push_back(pa);
				points.push_back(mid);
				bbox = m_bluredBrush->getBoundFromPoints(points);
				updateWorkAndBackupRasters(bbox + m_lastRect);
				m_tileSaver->save(bbox);
				m_bluredBrush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
				m_lastRect += bbox;
			} else {
				points.push_back(m_points[m - 4]);
				points.push_back(old);
				points.push_back(mid);
				bbox = m_bluredBrush->getBoundFromPoints(points);
				updateWorkAndBackupRasters(bbox + m_lastRect);
				m_tileSaver->save(bbox);
				m_bluredBrush->addArc(m_points[m - 4], old, mid, 1, 1);
				m_lastRect += bbox;
			}
			invalidateRect = ToolUtils::getBounds(points, maxThickness) - rasCenter;
			m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox, m_styleId, m_selective.getValue());
			m_strokeRect += bbox;
		}
		invalidate(invalidateRect.enlarge(2));
	} else {
		double thickness = (m_pressure.getValue() || m_isPath) ? computeThickness(e.m_pressure, m_thickness, m_isPath) : m_thickness.getValue().second * 0.5;
		m_track.add(TThickPoint(pos, thickness), getPixelSize() * getPixelSize());

		invalidate();
	}
}

//---------------------------------------------------------------------------------------------------------------

void BrushTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e)
{
	bool isValid = m_enabled && m_active;
	m_enabled = false;

	if (!isValid)
		return;

	if (m_isPath) {
		double error = 20.0 * getPixelSize();

		TStroke *stroke = m_track.makeStroke(error);
		int points = stroke->getControlPointCount();

		if (TVectorImageP vi = getImage(true)) {
			struct Cleanup {
				BrushTool *m_this;
				~Cleanup() { m_this->m_track.clear(), m_this->invalidate(); }
			} cleanup = {this};

			if (!isJustCreatedSpline(vi.getPointer())) {
				m_isPrompting = true;

				QString question("Are you sure you want to replace the motion path?");
				int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"), 0);

				m_isPrompting = false;

				if (ret == 2 || ret == 0)
					return;
			}

			QMutexLocker lock(vi->getMutex());

			TUndo *undo = new UndoPath(getXsheet()->getStageObject(getObjectId())->getSpline());

			while (vi->getStrokeCount() > 0)
				vi->deleteStroke(0);
			vi->addStroke(stroke, false);

			notifyImageChanged();
			TUndoManager::manager()->add(undo);
		}

		return;
	}

	TImageP image = getImage(true);
	if (TVectorImageP vi = image) {
		if (m_track.isEmpty()) {
			m_styleId = 0;
			m_track.clear();
			return;
		}
		m_track.filterPoints();
		double error = 30.0 / (1 + 0.5 * m_accuracy.getValue());
		error *= getPixelSize();

		TStroke *stroke = m_track.makeStroke(error);
		stroke->setStyle(m_styleId);
		{
			TStroke::OutlineOptions &options = stroke->outlineOptions();
			options.m_capStyle = m_capStyle.getIndex();
			options.m_joinStyle = m_joinStyle.getIndex();
			options.m_miterUpper = m_miterJoinLimit.getValue();
		}
		m_styleId = 0;

		QMutexLocker lock(vi->getMutex());
		if (stroke->getControlPointCount() == 3 && stroke->getControlPoint(0) != stroke->getControlPoint(2)) // gli stroke con solo 1 chunk vengono fatti dal tape tool...e devono venir riconosciuti come speciali di autoclose proprio dal fatto che hanno 1 solo chunk.
			stroke->insertControlPoints(0.5);

		addStrokeToImage(getApplication(), vi, stroke, m_breakAngles.getValue(), m_isFrameCreated, m_isLevelCreated);
		TRectD bbox = stroke->getBBox().enlarge(2) + m_track.getModifiedRegion();
		invalidate();
		assert(stroke);
		m_track.clear();
	} else if (TToonzImageP ti = image) {
		finishRasterBrush(pos, e.m_pressure);
	}
}

//---------------------------------------------------------------------------------------------------------------
/*! ドラッグ中にツールが切り替わった場合に備え、onDeactivate時とMouseRelease時にと同じ終了処理を行う
*/
void BrushTool::finishRasterBrush(const TPointD &pos, int pressureVal)
{
	TImageP image = getImage(true);
	TToonzImageP ti = image;
	if (!ti)
		return;

	TPointD rasCenter = ti->getRaster()->getCenterD();
	TTool::Application *app = TTool::getApplication();
	TXshLevel *level = app->getCurrentLevel()->getLevel();
	TXshSimpleLevelP simLevel = level->getSimpleLevel();

	/*-- 描画中にカレントフレームが変わっても、描画開始時のFidに対してUndoを記録する --*/
	TFrameId frameId = m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;

	if (m_rasterTrack && (m_hardness.getValue() == 100 || m_pencil.getValue())) {
		double thickness = m_pressure.getValue() ? computeThickness(pressureVal, m_rasThickness, m_isPath) : m_rasThickness.getValue().second;

		/*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
		if (m_pressure.getValue() && pressureVal == 255)
			thickness = m_rasThickness.getValue().first;

		/*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる --*/
		if (!m_pencil.getValue())
			thickness -= 1.0;

		bool isAdded = m_rasterTrack->add(TThickPoint(pos + convert(ti->getRaster()->getCenter()), thickness));
		if (isAdded) {
			m_tileSaver->save(m_rasterTrack->getLastRect());
			m_rasterTrack->generateLastPieceOfStroke(m_pencil.getValue(), true);

			std::vector<TThickPoint> brushPoints = m_rasterTrack->getPointsSequence();
			int m = (int)brushPoints.size();
			std::vector<TThickPoint> points;
			if (m == 3) {
				points.push_back(brushPoints[0]);
				points.push_back(brushPoints[1]);
			} else {
				points.push_back(brushPoints[m - 4]);
				points.push_back(brushPoints[m - 3]);
				points.push_back(brushPoints[m - 2]);
			}
			int maxThickness = m_rasThickness.getValue().second;
			invalidate(ToolUtils::getBounds(points, maxThickness).enlarge(2) - rasCenter);
		}

		if (m_tileSet->getTileCount() > 0) {
			TUndoManager::manager()->add(new RasterBrushUndo(m_tileSet,
															 m_rasterTrack->getPointsSequence(),
															 m_rasterTrack->getStyleId(),
															 m_rasterTrack->isSelective(),
															 simLevel.getPointer(),
															 frameId, m_pencil.getValue(),
															 m_isFrameCreated,
															 m_isLevelCreated));
		}
		delete m_rasterTrack;
		m_rasterTrack = 0;
	} else {
		if (m_points.size() != 1) {
			double maxThickness = m_rasThickness.getValue().second;
			double thickness = (m_pressure.getValue() || m_isPath) ? computeThickness(pressureVal, m_rasThickness, m_isPath)
																   : maxThickness;
			TPointD rasCenter = ti->getRaster()->getCenterD();
			TThickPoint point(pos + rasCenter, thickness);
			m_points.push_back(point);
			int m = m_points.size();
			std::vector<TThickPoint> points;
			points.push_back(m_points[m - 3]);
			points.push_back(m_points[m - 2]);
			points.push_back(m_points[m - 1]);
			TRect bbox = m_bluredBrush->getBoundFromPoints(points);
			updateWorkAndBackupRasters(bbox);
			m_tileSaver->save(bbox);
			m_bluredBrush->addArc(points[0], points[1], points[2], 1, 1);
			m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox, m_styleId, m_selective.getValue());
			TRectD invalidateRect = ToolUtils::getBounds(points, maxThickness);
			invalidate(invalidateRect.enlarge(2) - rasCenter);
			m_strokeRect += bbox;
			m_lastRect.empty();
		}
		delete m_bluredBrush;
		m_bluredBrush = 0;

		if (m_tileSet->getTileCount() > 0) {
			TUndoManager::manager()->add(new RasterBluredBrushUndo(m_tileSet,
																   m_points,
																   m_styleId,
																   m_selective.getValue(),
																   simLevel.getPointer(),
																   frameId,
																   m_rasThickness.getValue().second,
																   m_hardness.getValue() * 0.01,
																   m_isFrameCreated,
																   m_isLevelCreated));
		}
	}
	delete m_tileSaver;

	m_tileSaver = 0;

	/*-- FIdを指定して、描画中にフレームが動いても、
	　　描画開始時のFidのサムネイルが更新されるようにする。--*/
	notifyImageChanged(frameId);

	m_strokeRect.empty();

	ToolUtils::updateSaveBox();

	/*-- 作業中のフレームをリセット --*/
	m_workingFrameId = TFrameId();
}
//---------------------------------------------------------------------------------------------------------------

void BrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e)
{
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

	struct Locals {
		BrushTool *m_this;

		void setValue(TDoublePairProperty &prop, const TDoublePairProperty::Value &value)
		{
			prop.setValue(value);

			m_this->onPropertyChanged(prop.getName());
			TTool::getApplication()->getCurrentTool()->notifyToolChanged();
		}

		void addMinMax(TDoublePairProperty &prop, double add)
		{
			if (add == 0.0)
				return;
			const TDoublePairProperty::Range &range = prop.getRange();

			TDoublePairProperty::Value value = prop.getValue();
			value.first = tcrop(value.first + add, range.first, range.second);
			value.second = tcrop(value.second + add, range.first, range.second);

			setValue(prop, value);
		}

	} locals = {this};

	switch (e.getModifiersMask()) {
	/*-- Altキー+マウス移動で、ブラシサイズ（Min/Maxとも）を変える（CtrlやShiftでは誤操作の恐れがある） --*/
	case TMouseEvent::ALT_KEY: {
		// User wants to alter the minimum brush size
		const TPointD &diff = pos - m_mousePos;
		double add = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

		locals.addMinMax(TToonzImageP(getImage(false, 1)) ? m_rasThickness : m_thickness, add);

		break;
	}

	default:
		m_brushPos = pos;
		break;
	}

	m_mousePos = pos;
	invalidate();

	if (m_minThick == 0 && m_maxThick == 0) {
		if (m_targetType & TTool::ToonzImage) {
			m_minThick = m_rasThickness.getValue().first;
			m_maxThick = m_rasThickness.getValue().second;
		} else {
			m_minThick = m_thickness.getValue().first;
			m_maxThick = m_thickness.getValue().second;
		}
	}
}

//-------------------------------------------------------------------------------------------------------------

void BrushTool::draw()
{
	/*--ショートカットでのツール切り替え時に赤点が描かれるのを防止する--*/
	if (m_minThick == 0 && m_maxThick == 0)
		return;

	TImageP img = getImage(false, 1);

	// Draw track
	tglColor(m_isPrompting ? TPixel32::Green : m_currentColor);
	m_track.drawAllFragments();

	if (getApplication()->getCurrentObject()->isSpline())
		return;

	// Draw the brush outline - change color when the Ink / Paint check is activated
	if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) || (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) || (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
		glColor3d(0.5, 0.8, 0.8);
	//normally draw in red
	else
		glColor3d(1.0, 0.0, 0.0);

	if (TToonzImageP ti = img) {
		TRasterP ras = ti->getRaster();
		int lx = ras->getLx();
		int ly = ras->getLy();

		drawEmptyCircle(m_brushPos, tround(m_minThick), lx % 2 == 0, ly % 2 == 0, m_pencil.getValue());
		drawEmptyCircle(m_brushPos, tround(m_maxThick), lx % 2 == 0, ly % 2 == 0, m_pencil.getValue());
	} else {
		tglDrawCircle(m_brushPos, 0.5 * m_minThick);
		tglDrawCircle(m_brushPos, 0.5 * m_maxThick);
	}
}

//--------------------------------------------------------------------------------------------------------------

void BrushTool::onEnter()
{
	TImageP img = getImage(false);

	if (TToonzImageP(img)) {
		m_minThick = m_rasThickness.getValue().first;
		m_maxThick = m_rasThickness.getValue().second;
	} else {
		m_minThick = m_thickness.getValue().first;
		m_maxThick = m_thickness.getValue().second;
	}

	Application *app = getApplication();

	m_styleId = app->getCurrentLevelStyleIndex();
	TColorStyle *cs = app->getCurrentLevelStyle();
	if (cs) {
		TRasterStyleFx *rfx = cs->getRasterStyleFx();
		m_active = cs->isStrokeStyle() || (rfx && rfx->isInkStyle());
		m_currentColor = cs->getAverageColor();
		m_currentColor.m = 255;
	} else {
		m_currentColor = TPixel32::Black;
	}
	m_active = img;
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::onLeave()
{
	m_minThick = 0;
	m_maxThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *BrushTool::getProperties(int idx)
{
	if (!m_presetsLoaded)
		initPresets();

	return &m_prop[idx];
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::onImageChanged()
{
	TToonzImageP ti = (TToonzImageP)getImage(false, 1);
	if (!ti || !isEnabled())
		return;

	setWorkAndBackupImages();
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::setWorkAndBackupImages()
{
	TToonzImageP ti = (TToonzImageP)getImage(false, 1);
	if (!ti)
		return;

	TRasterP ras = ti->getRaster();
	TDimension dim = ras->getSize();

	double hardness = m_hardness.getValue() * 0.01;
	if (hardness == 1.0 && ras->getPixelSize() == 4) {
		m_workRas = TRaster32P();
		m_backupRas = TRasterCM32P();
	} else {
		if (!m_workRas || m_workRas->getLx() > dim.lx || m_workRas->getLy() > dim.ly)
			m_workRas = TRaster32P(dim);
		if (!m_backupRas || m_backupRas->getLx() > dim.lx || m_backupRas->getLy() > dim.ly)
			m_backupRas = TRasterCM32P(dim);

		m_strokeRect.empty();
		m_lastRect.empty();
	}
}

//------------------------------------------------------------------

bool BrushTool::onPropertyChanged(std::string propertyName)
{
	//Set the following to true whenever a different piece of interface must
	//be refreshed - done once at the end.
	bool notifyTool = false;

	/*--- 変更されたPropertyに合わせて処理を分ける ---*/

	/*--- m_thicknessとm_rasThicknessは同じName(="Size:")なので、
		扱っている画像がラスタかどうかで区別する---*/
	if (propertyName == m_thickness.getName()) {
		TImageP img = getImage(false);
		if (TToonzImageP(img)) //raster
		{
			RasterBrushMinSize = m_rasThickness.getValue().first;
			RasterBrushMaxSize = m_rasThickness.getValue().second;

			m_minThick = m_rasThickness.getValue().first;
			m_maxThick = m_rasThickness.getValue().second;
		} else //vector
		{
			VectorBrushMinSize = m_thickness.getValue().first;
			VectorBrushMaxSize = m_thickness.getValue().second;

			m_minThick = m_thickness.getValue().first;
			m_maxThick = m_thickness.getValue().second;
		}		
	} else if (propertyName == m_accuracy.getName()) {
		BrushAccuracy = m_accuracy.getValue();
	} else if (propertyName == m_preset.getName()) {
		loadPreset();
		notifyTool = true;
	} else if (propertyName == m_selective.getName()) {
		BrushSelective = m_selective.getValue();
	} else if (propertyName == m_breakAngles.getName()) {
		BrushBreakSharpAngles = m_breakAngles.getValue();
	} else if (propertyName == m_pencil.getName()) {
		RasterBrushPencilMode = m_pencil.getValue();
	} else if (propertyName == m_pressure.getName()) {
		BrushPressureSensibility = m_pressure.getValue();
	} else if (propertyName == m_capStyle.getName()) {
		VectorCapStyle = m_capStyle.getIndex();
	} else if (propertyName == m_joinStyle.getName()) {
		VectorJoinStyle = m_joinStyle.getIndex();
	} else if (propertyName == m_miterJoinLimit.getName()) {
		VectorMiterValue = m_miterJoinLimit.getValue();
	}

	if (m_targetType & TTool::Vectors) {
		if (propertyName == m_joinStyle.getName())
			notifyTool = true;
	}
	if (m_targetType & TTool::ToonzImage) {
		if (propertyName == m_hardness.getName())
			setWorkAndBackupImages();
		if (propertyName == m_hardness.getName() || propertyName == m_thickness.getName()) {
			m_brushPad = getBrushPad(m_rasThickness.getValue().second, m_hardness.getValue() * 0.01);
			TRectD rect(m_mousePos - TPointD(m_maxThick + 2, m_maxThick + 2),
						m_mousePos + TPointD(m_maxThick + 2, m_maxThick + 2));
			invalidate(rect);
		}
	}

	if (propertyName != m_preset.getName() && m_preset.getValue() != CUSTOM_WSTR) {
		m_preset.setValue(CUSTOM_WSTR);
		notifyTool = true;
	}

	if (notifyTool)
		getApplication()->getCurrentTool()->notifyToolChanged();

	return true;
}

//------------------------------------------------------------------

void BrushTool::initPresets()
{
	if (!m_presetsLoaded) {
		//If necessary, load the presets from file
		m_presetsLoaded = true;
		if (getTargetType() & TTool::Vectors)
			m_presetsManager.load(TEnv::getConfigDir() + "brush_vector.txt");
		else
			m_presetsManager.load(TEnv::getConfigDir() + "brush_toonzraster.txt");
	}

	//Rebuild the presets property entries
	const std::set<BrushData> &presets = m_presetsManager.presets();

	m_preset.deleteAllValues();
	m_preset.addValue(CUSTOM_WSTR);

	std::set<BrushData>::const_iterator it, end = presets.end();
	for (it = presets.begin(); it != end; ++it)
		m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------------------

void BrushTool::loadPreset()
{
	const std::set<BrushData> &presets = m_presetsManager.presets();
	std::set<BrushData>::const_iterator it;

	it = presets.find(BrushData(m_preset.getValue()));
	if (it == presets.end())
		return;

	const BrushData &preset = *it;

	try //Don't bother with RangeErrors
	{
		if (getTargetType() & TTool::Vectors) {
			m_thickness.setValue(TDoublePairProperty::Value(preset.m_min, preset.m_max));
			m_accuracy.setValue(preset.m_acc, true);
			m_breakAngles.setValue(preset.m_breakAngles);
			m_pressure.setValue(preset.m_pressure);
			m_capStyle.setIndex(preset.m_cap);
			m_joinStyle.setIndex(preset.m_join);
			m_miterJoinLimit.setValue(preset.m_miter);
		} else {
			m_rasThickness.setValue(TDoublePairProperty::Value(tmax(preset.m_min, 1.0), preset.m_max));
			m_brushPad = ToolUtils::getBrushPad(preset.m_max, preset.m_hardness * 0.01);
			m_hardness.setValue(preset.m_hardness, true);
			m_selective.setValue(preset.m_selective);
			m_pencil.setValue(preset.m_pencil);
			m_pressure.setValue(preset.m_pressure);
		}
	} catch (...) {
	}
}

//------------------------------------------------------------------

void BrushTool::addPreset(QString name)
{
	//Build the preset
	BrushData preset(name.toStdWString());

	if (getTargetType() & TTool::Vectors) {
		preset.m_min = m_thickness.getValue().first;
		preset.m_max = m_thickness.getValue().second;
	} else {
		preset.m_min = m_rasThickness.getValue().first;
		preset.m_max = m_rasThickness.getValue().second;
	}

	preset.m_acc = m_accuracy.getValue();
	preset.m_hardness = m_hardness.getValue();
	preset.m_selective = m_selective.getValue();
	preset.m_pencil = m_pencil.getValue();
	preset.m_breakAngles = m_breakAngles.getValue();
	preset.m_pressure = m_pressure.getValue();
	preset.m_cap = m_capStyle.getIndex();
	preset.m_join = m_joinStyle.getIndex();
	preset.m_miter = m_miterJoinLimit.getValue();

	//Pass the preset to the manager
	m_presetsManager.addPreset(preset);

	//Reinitialize the associated preset enum
	initPresets();

	//Set the value to the specified one
	m_preset.setValue(preset.m_name);
}

//------------------------------------------------------------------

void BrushTool::removePreset()
{
	std::wstring name(m_preset.getValue());
	if (name == CUSTOM_WSTR)
		return;

	m_presetsManager.removePreset(name);
	initPresets();

	//No parameter change, and set the preset value to custom
	m_preset.setValue(CUSTOM_WSTR);
}

//------------------------------------------------------------------
/*!	Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す
*/
bool BrushTool::isPencilModeActive()
{
	return getTargetType() == TTool::ToonzImage && m_pencil.getValue();
}

//==========================================================================================================

// Tools instantiation

BrushTool vectorPencil("T_Brush", TTool::Vectors | TTool::EmptyTarget);
BrushTool toonzPencil("T_Brush", TTool::ToonzImage | TTool::EmptyTarget);

//*******************************************************************************
//    Brush Data implementation
//*******************************************************************************

BrushData::BrushData()
	: m_name(), m_min(0.0), m_max(0.0), m_acc(0.0), m_hardness(0.0), m_opacityMin(0.0), m_opacityMax(0.0), m_selective(false), m_pencil(false), m_breakAngles(false), m_pressure(false), m_cap(0), m_join(0), m_miter(0)
{
}

//----------------------------------------------------------------------------------------------------------

BrushData::BrushData(const std::wstring &name)
	: m_name(name), m_min(0.0), m_max(0.0), m_acc(0.0), m_hardness(0.0), m_opacityMin(0.0), m_opacityMax(0.0), m_selective(false), m_pencil(false), m_breakAngles(false), m_pressure(false), m_cap(0), m_join(0), m_miter(0)
{
}

//----------------------------------------------------------------------------------------------------------

void BrushData::saveData(TOStream &os)
{
	os.openChild("Name");
	os << m_name;
	os.closeChild();
	os.openChild("Thickness");
	os << m_min << m_max;
	os.closeChild();
	os.openChild("Accuracy");
	os << m_acc;
	os.closeChild();
	os.openChild("Hardness");
	os << m_hardness;
	os.closeChild();
	os.openChild("Opacity");
	os << m_opacityMin << m_opacityMax;
	os.closeChild();
	os.openChild("Selective");
	os << (int)m_selective;
	os.closeChild();
	os.openChild("Pencil");
	os << (int)m_pencil;
	os.closeChild();
	os.openChild("Break_Sharp_Angles");
	os << (int)m_breakAngles;
	os.closeChild();
	os.openChild("Pressure_Sensitivity");
	os << (int)m_pressure;
	os.closeChild();
	os.openChild("Cap");
	os << m_cap;
	os.closeChild();
	os.openChild("Join");
	os << m_join;
	os.closeChild();
	os.openChild("Miter");
	os << m_miter;
	os.closeChild();
}

//----------------------------------------------------------------------------------------------------------

void BrushData::loadData(TIStream &is)
{
	std::string tagName;
	int val;

	while (is.matchTag(tagName)) {
		if (tagName == "Name")
			is >> m_name, is.matchEndTag();
		else if (tagName == "Thickness")
			is >> m_min >> m_max, is.matchEndTag();
		else if (tagName == "Accuracy")
			is >> m_acc, is.matchEndTag();
		else if (tagName == "Hardness")
			is >> m_hardness, is.matchEndTag();
		else if (tagName == "Opacity")
			is >> m_opacityMin >> m_opacityMax, is.matchEndTag();
		else if (tagName == "Selective")
			is >> val, m_selective = val, is.matchEndTag();
		else if (tagName == "Pencil")
			is >> val, m_pencil = val, is.matchEndTag();
		else if (tagName == "Break_Sharp_Angles")
			is >> val, m_breakAngles = val, is.matchEndTag();
		else if (tagName == "Pressure_Sensitivity")
			is >> val, m_pressure = val, is.matchEndTag();
		else if (tagName == "Cap")
			is >> m_cap, is.matchEndTag();
		else if (tagName == "Join")
			is >> m_join, is.matchEndTag();
		else if (tagName == "Miter")
			is >> m_miter, is.matchEndTag();
		else
			is.skipCurrentTag();
	}
}

//----------------------------------------------------------------------------------------------------------

PERSIST_IDENTIFIER(BrushData, "BrushData");

//*******************************************************************************
//    Brush Preset Manager implementation
//*******************************************************************************

void BrushPresetManager::load(const TFilePath &fp)
{
	m_fp = fp;

	std::string tagName;
	BrushData data;

	TIStream is(m_fp);
	try {
		while (is.matchTag(tagName)) {
			if (tagName == "version") {
				VersionNumber version;
				is >> version.first >> version.second;

				is.setVersion(version);
				is.matchEndTag();
			} else if (tagName == "brushes") {
				while (is.matchTag(tagName)) {
					if (tagName == "brush") {
						is >> data, m_presets.insert(data);
						is.matchEndTag();
					} else
						is.skipCurrentTag();
				}

				is.matchEndTag();
			} else
				is.skipCurrentTag();
		}
	} catch (...) {
	}
}

//------------------------------------------------------------------

void BrushPresetManager::save()
{
	TOStream os(m_fp);

	os.openChild("version");
	os << 1 << 19;
	os.closeChild();

	os.openChild("brushes");

	std::set<BrushData>::iterator it, end = m_presets.end();
	for (it = m_presets.begin(); it != end; ++it) {
		os.openChild("brush");
		os << (TPersist &)*it;
		os.closeChild();
	}

	os.closeChild();
}

//------------------------------------------------------------------

void BrushPresetManager::addPreset(const BrushData &data)
{
	m_presets.erase(data); //Overwriting insertion
	m_presets.insert(data);
	save();
}

//------------------------------------------------------------------

void BrushPresetManager::removePreset(const std::wstring &name)
{
	m_presets.erase(BrushData(name));
	save();
}
