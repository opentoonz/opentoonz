

#include "fullcolorbrushtool.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "tools/tooloptions.h"

#include "bluredbrush.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/strokegenerator.h"
#include "toonz/tstageobject.h"
#include "toonz/palettecontroller.h"

// TnzCore includes
#include "tgl.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "tenv.h"
#include "tpalette.h"
#include "trop.h"
#include "tstream.h"
#include "tstroke.h"
#include "timagecache.h"

// Qt includes
#include <QCoreApplication> // Qt translation support

//----------------------------------------------------------------------------------

TEnv::IntVar FullcolorBrushMinSize("FullcolorBrushMinSize", 1);
TEnv::IntVar FullcolorBrushMaxSize("FullcolorBrushMaxSize", 5);
TEnv::IntVar FullcolorPressureSensibility("FullcolorPressureSensibility", 1);
TEnv::DoubleVar FullcolorBrushHardness("FullcolorBrushHardness", 100);
TEnv::DoubleVar FullcolorMinOpacity("FullcolorMinOpacity", 100);
TEnv::DoubleVar FullcolorMaxOpacity("FullcolorMaxOpacity", 100);

//----------------------------------------------------------------------------------

#define CUSTOM_WSTR L"<custom>"

//----------------------------------------------------------------------------------

namespace
{

int computeThickness(int pressure, const TIntPairProperty &property, bool isPath = false)
{
	if (isPath)
		return 0.0;
	double p = pressure / 255.0;
	double t = p * p * p;
	int thick0 = property.getValue().first;
	int thick1 = property.getValue().second;
	return tround(thick0 + (thick1 - thick0) * t);
}

//----------------------------------------------------------------------------------

double computeThickness(int pressure, const TDoublePairProperty &property, bool isPath = false)
{
	if (isPath)
		return 0.0;
	double p = pressure / 255.0;
	double t = p * p * p;
	double thick0 = property.getValue().first;
	double thick1 = property.getValue().second;
	if (thick1 < 0.0001)
		thick0 = thick1 = 0.0;
	return (thick0 + (thick1 - thick0) * t);
}

//----------------------------------------------------------------------------------

class FullColorBrushUndo : public ToolUtils::TFullColorRasterUndo
{
	TPoint m_offset;
	QString m_id;

public:
	FullColorBrushUndo(TTileSetFullColor *tileSet,
					   TXshSimpleLevel *level, const TFrameId &frameId, bool isFrameCreated,
					   const TRasterP &ras, const TPoint &offset)
		: ToolUtils::TFullColorRasterUndo(tileSet, level, frameId, isFrameCreated, false, 0), m_offset(offset)
	{
		static int counter = 0;

		m_id = QString("FullColorBrushUndo") + QString::number(counter++);
		TImageCache::instance()->add(m_id.toStdString(), TRasterImageP(ras));
	}

	~FullColorBrushUndo()
	{
		TImageCache::instance()->remove(m_id);
	}

	void redo() const
	{
		insertLevelAndFrameIfNeeded();

		TRasterImageP image = getImage();
		TRasterP ras = image->getRaster();

		TRasterImageP srcImg = TImageCache::instance()->get(m_id.toStdString(), false);
		ras->copy(srcImg->getRaster(), m_offset);

		TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
		notifyImageChanged();
	}

	int getSize() const
	{
		return sizeof(*this) + ToolUtils::TFullColorRasterUndo::getSize();
	}

	virtual QString getToolName()
	{
		return QString("Raster Brush Tool");
	}
	int getHistoryType()
	{
		return HistoryType::BrushTool;
	}
};

} // namespace

//************************************************************************
//    FullColor Brush Tool implementation
//************************************************************************

FullColorBrushTool::FullColorBrushTool(std::string name)
	: TTool(name), m_thickness("Thickness", 1, 100, 1, 5, false), m_pressure("Pressure Sensitivity", true), m_opacity("Opacity:", 0, 100, 100, 100, true), m_hardness("Hardness:", 0, 100, 100), m_preset("Preset:"), m_styleId(0), m_oldOpacity(1), m_brush(0), m_tileSet(0), m_tileSaver(0), m_notifier(0), m_presetsLoaded(false), m_firstTime(true)
{
	bind(TTool::RasterImage | TTool::EmptyTarget);

	m_prop.bind(m_thickness);
	m_prop.bind(m_hardness);
	m_prop.bind(m_opacity);
	m_prop.bind(m_pressure);
	m_prop.bind(m_preset);
	m_preset.setId("BrushPreset");
}

//---------------------------------------------------------------------------------------------------

ToolOptionsBox *FullColorBrushTool::createOptionsBox()
{
	TPaletteHandle *currPalette = TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
	ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
	return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onCanvasSizeChanged()
{
	onDeactivate();
	setWorkAndBackupImages();
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateTranslation()
{
	m_thickness.setQStringName(tr("Thickness"));
	m_pressure.setQStringName(tr("Pressure Sensitivity"));
	m_opacity.setQStringName(tr("Opacity:"));
	m_hardness.setQStringName(tr("Hardness:"));
	m_preset.setQStringName(tr("Preset:"));
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onActivate()
{
	if (!m_notifier)
		m_notifier = new FullColorBrushToolNotifier(this);

	TTool::Application *app = getApplication();

	if (app->getCurrentObject()->isSpline()) {
		m_currentColor = TPixel32::Red;
		return;
	}

	int styleIndex = app->getCurrentLevelStyleIndex();
	TPalette *plt = app->getCurrentPalette()->getPalette();
	if (plt) {
		int style = app->getCurrentLevelStyleIndex();
		TColorStyle *colorStyle = plt->getStyle(style);
		m_currentColor = colorStyle->getMainColor();
	}

	if (m_firstTime) {
		m_firstTime = false;
		m_thickness.setValue(TIntPairProperty::Value(FullcolorBrushMinSize, FullcolorBrushMaxSize));
		m_pressure.setValue(FullcolorPressureSensibility ? 1 : 0);
		m_opacity.setValue(TDoublePairProperty::Value(FullcolorMinOpacity, FullcolorMaxOpacity));
		m_hardness.setValue(FullcolorBrushHardness);
	}

	m_brushPad = ToolUtils::getBrushPad(m_thickness.getValue().second, m_hardness.getValue() * 0.01);
	setWorkAndBackupImages();
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::onDeactivate()
{
	m_workRaster = TRaster32P();
	m_backUpRas = TRasterP();
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateWorkAndBackupRasters(const TRect &rect)
{
	TRasterImageP ri = TImageP(getImage(false, 1));
	if (!ri)
		return;

	TRasterP ras = ri->getRaster();

	TRect _rect = rect * ras->getBounds();
	TRect _lastRect = m_lastRect * ras->getBounds();

	if (_rect.isEmpty())
		return;

	if (m_lastRect.isEmpty()) {
		m_workRaster->extract(_rect)->clear();
		m_backUpRas->extract(_rect)->copy(ras->extract(_rect));
		return;
	}

	QList<TRect> rects = ToolUtils::splitRect(_rect, _lastRect);
	for (int i = 0; i < rects.size(); i++) {
		m_workRaster->extract(rects[i])->clear();
		m_backUpRas->extract(rects[i])->copy(ras->extract(rects[i]));
	}
}

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::preLeftButtonDown()
{
	touchImage();

	if (m_isFrameCreated)
		setWorkAndBackupImages();

	return true;
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e)
{
	m_brushPos = m_mousePos = pos;

	Viewer *viewer = getViewer();
	if (!viewer)
		return;

	TRasterImageP ri = (TRasterImageP)getImage(true);
	if (!ri)
		ri = (TRasterImageP)touchImage();

	if (!ri)
		return;

	TRasterP ras = ri->getRaster();
	TDimension dim = ras->getSize();

	if (!(m_workRaster && m_backUpRas))
		setWorkAndBackupImages();

	m_workRaster->lock();

	double maxThick = m_thickness.getValue().second;
	double thickness = m_pressure.getValue() ? computeThickness(e.m_pressure, m_thickness) : maxThick;
	double opacity = (m_pressure.getValue() ? computeThickness(e.m_pressure, m_opacity) : m_opacity.getValue().second) * 0.01;
	TPointD rasCenter = TPointD(dim.lx * 0.5, dim.ly * 0.5);
	TThickPoint point(pos + rasCenter, thickness);
	TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
	TRectD invalidateRect(pos - halfThick, pos + halfThick);

	m_points.clear();
	m_points.push_back(point);

	m_tileSet = new TTileSetFullColor(ras->getSize());
	m_tileSaver = new TTileSaverFullColor(ras, m_tileSet);
	double hardness = m_hardness.getValue() * 0.01;

	m_brush = new BluredBrush(m_workRaster, maxThick, m_brushPad, hardness == 1.0);
	m_strokeRect = m_brush->getBoundFromPoints(m_points);
	updateWorkAndBackupRasters(m_strokeRect);
	m_tileSaver->save(m_strokeRect);
	m_brush->addPoint(point, opacity);
	m_brush->updateDrawing(ras, m_backUpRas, m_currentColor, m_strokeRect, m_opacity.getValue().second * 0.01);
	m_oldOpacity = opacity;
	m_lastRect = m_strokeRect;

	invalidate(invalidateRect.enlarge(2));
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e)
{
	m_brushPos = m_mousePos = pos;

	TRasterImageP ri = (TRasterImageP)getImage(true);
	if (!ri)
		return;

	double maxThickness = m_thickness.getValue().second;
	double thickness = m_pressure.getValue() ? computeThickness(e.m_pressure, m_thickness) : maxThickness;
	double opacity = (m_pressure.getValue() ? computeThickness(e.m_pressure, m_opacity) : m_opacity.getValue().second) * 0.01;
	TDimension size = m_workRaster->getSize();
	TPointD rasCenter = TPointD(size.lx * 0.5, size.ly * 0.5);
	TThickPoint point(pos + rasCenter, thickness);

	TThickPoint old = m_points.back();
	if (norm2(point - old) < 4)
		return;

	TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
	m_points.push_back(mid);
	m_points.push_back(point);

	TRect bbox;
	int m = m_points.size();
	TRectD invalidateRect;
	if (m == 3) {
		// ho appena cominciato. devo disegnare un segmento
		TThickPoint pa = m_points.front();
		std::vector<TThickPoint> points;
		points.push_back(pa);
		points.push_back(mid);
		invalidateRect = ToolUtils::getBounds(points, maxThickness);
		bbox = m_brush->getBoundFromPoints(points);
		updateWorkAndBackupRasters(bbox + m_lastRect);
		m_tileSaver->save(bbox);
		m_brush->addArc(pa, (pa + mid) * 0.5, mid, m_oldOpacity, opacity);
		m_lastRect += bbox;
	} else {
		// caso generale: disegno un arco
		std::vector<TThickPoint> points;
		points.push_back(m_points[m - 4]);
		points.push_back(old);
		points.push_back(mid);
		invalidateRect = ToolUtils::getBounds(points, maxThickness);
		bbox = m_brush->getBoundFromPoints(points);
		updateWorkAndBackupRasters(bbox + m_lastRect);
		m_tileSaver->save(bbox);
		m_brush->addArc(m_points[m - 4], old, mid, m_oldOpacity, opacity);
		m_lastRect += bbox;
	}
	m_oldOpacity = opacity;
	m_brush->updateDrawing(ri->getRaster(), m_backUpRas, m_currentColor, bbox, m_opacity.getValue().second * 0.01);
	invalidate(invalidateRect.enlarge(2) - rasCenter);
	m_strokeRect += bbox;
}

//---------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e)
{
	m_brushPos = m_mousePos = pos;

	TRasterImageP ri = (TRasterImageP)getImage(true);
	if (!ri)
		return;

	if (m_points.size() != 1) {
		double maxThickness = m_thickness.getValue().second;
		double thickness = m_pressure.getValue() ? computeThickness(e.m_pressure, m_thickness) : maxThickness;
		double opacity = (m_pressure.getValue() ? computeThickness(e.m_pressure, m_opacity) : m_opacity.getValue().second) * 0.01;
		TPointD rasCenter = ri->getRaster()->getCenterD();
		TThickPoint point(pos + rasCenter, thickness);
		m_points.push_back(point);
		int m = m_points.size();
		std::vector<TThickPoint> points;
		points.push_back(m_points[m - 3]);
		points.push_back(m_points[m - 2]);
		points.push_back(m_points[m - 1]);
		TRect bbox = m_brush->getBoundFromPoints(points);
		updateWorkAndBackupRasters(bbox);
		m_tileSaver->save(bbox);
		m_brush->addArc(points[0], points[1], points[2], m_oldOpacity, opacity);
		m_brush->updateDrawing(ri->getRaster(), m_backUpRas, m_currentColor, bbox, m_opacity.getValue().second * 0.01);
		TRectD invalidateRect = ToolUtils::getBounds(points, maxThickness);
		invalidate(invalidateRect.enlarge(2) - rasCenter);
		m_strokeRect += bbox;
		m_lastRect.empty();
	}

	if (m_brush) {
		delete m_brush;
		m_brush = 0;
	}

	m_workRaster->unlock();

	if (m_tileSet->getTileCount() > 0) {
		delete m_tileSaver;
		TTool::Application *app = TTool::getApplication();
		TXshLevel *level = app->getCurrentLevel()->getLevel();
		TXshSimpleLevelP simLevel = level->getSimpleLevel();
		TFrameId frameId = getCurrentFid();
		TRasterP ras = ri->getRaster()->extract(m_strokeRect)->clone();
		TUndoManager::manager()->add(new FullColorBrushUndo(m_tileSet, simLevel.getPointer(), frameId,
															m_isFrameCreated, ras, m_strokeRect.getP00()));
	}

	notifyImageChanged();
	m_strokeRect.empty();
}

//---------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e)
{
	struct Locals {
		FullColorBrushTool *m_this;

		void setValue(TIntPairProperty &prop, const TIntPairProperty::Value &value)
		{
			prop.setValue(value);

			m_this->onPropertyChanged(prop.getName());
			TTool::getApplication()->getCurrentTool()->notifyToolChanged();
		}

		void addMinMax(TIntPairProperty &prop, double add)
		{
			const TIntPairProperty::Range &range = prop.getRange();

			TIntPairProperty::Value value = prop.getValue();
			value.second = tcrop<double>(value.second + add, range.first, range.second);
			value.first = tcrop<double>(value.first + add, range.first, range.second);

			setValue(prop, value);
		}

	} locals = {this};

	switch (e.getModifiersMask()) {
	/*-- Altキー+マウス移動で、ブラシサイズ（Min/Maxとも）を変える（CtrlやShiftでは誤操作の恐れがある） --*/
	case TMouseEvent::ALT_KEY: {
		// User wants to alter the minimum brush size
		const TPointD &diff = pos - m_mousePos;
		double add = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

		locals.addMinMax(m_thickness, int(add));

		break;
	}

	default:
		m_brushPos = pos;
		break;
	}

	m_mousePos = pos;
	invalidate();
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::draw()
{
	if (TRasterImageP ri = TRasterImageP(getImage(false))) {
		TRasterP ras = ri->getRaster();

		glColor3d(1.0, 0.0, 0.0);

		tglDrawCircle(m_brushPos, (m_minThick + 1) * 0.5);
		tglDrawCircle(m_brushPos, (m_maxThick + 1) * 0.5);
	}
}

//--------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onEnter()
{
	TImageP img = getImage(false);
	TRasterImageP ri(img);
	if (ri) {
		m_minThick = m_thickness.getValue().first;
		m_maxThick = m_thickness.getValue().second;
	} else {
		m_minThick = 0;
		m_maxThick = 0;
	}
	Application *app = getApplication();
	if (app->getCurrentObject()->isSpline()) {
		m_currentColor = TPixel32::Red;
		return;
	}

	TPalette *plt = app->getCurrentPalette()->getPalette();
	if (!plt)
		return;

	int style = app->getCurrentLevelStyleIndex();
	TColorStyle *colorStyle = plt->getStyle(style);
	m_currentColor = colorStyle->getMainColor();
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onLeave()
{
	m_minThick = 0;
	m_maxThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *FullColorBrushTool::getProperties(int targetType)
{
	if (!m_presetsLoaded)
		initPresets();

	return &m_prop;
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onImageChanged()
{
	setWorkAndBackupImages();
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::setWorkAndBackupImages()
{
	TRasterImageP ri = (TRasterImageP)getImage(false, 1);
	if (!ri)
		return;

	TRasterP ras = ri->getRaster();
	TDimension dim = ras->getSize();

	if (!m_workRaster || m_workRaster->getLx() > dim.lx || m_workRaster->getLy() > dim.ly)
		m_workRaster = TRaster32P(dim);

	if (!m_backUpRas || m_backUpRas->getLx() > dim.lx || m_backUpRas->getLy() > dim.ly ||
		m_backUpRas->getPixelSize() != ras->getPixelSize())
		m_backUpRas = ras->create(dim.lx, dim.ly);

	m_strokeRect.empty();
	m_lastRect.empty();
}

//------------------------------------------------------------------

bool FullColorBrushTool::onPropertyChanged(std::string propertyName)
{
	m_minThick = m_thickness.getValue().first;
	m_maxThick = m_thickness.getValue().second;
	if (propertyName == "Hardness:" || propertyName == "Thickness") {
		m_brushPad = ToolUtils::getBrushPad(m_thickness.getValue().second, m_hardness.getValue() * 0.01);
		TRectD rect(m_brushPos - TPointD(m_maxThick + 2, m_maxThick + 2),
					m_brushPos + TPointD(m_maxThick + 2, m_maxThick + 2));
		invalidate(rect);
	}
	/*if(propertyName == "Hardness:" || propertyName == "Opacity:")
    setWorkAndBackupImages();*/
	FullcolorBrushMinSize = m_minThick;
	FullcolorBrushMaxSize = m_maxThick;
	FullcolorPressureSensibility = m_pressure.getValue();
	FullcolorBrushHardness = m_hardness.getValue();
	FullcolorMinOpacity = m_opacity.getValue().first;
	FullcolorMaxOpacity = m_opacity.getValue().second;

	if (propertyName == "Preset:") {
		loadPreset();
		getApplication()->getCurrentTool()->notifyToolChanged();
		return true;
	}

	if (m_preset.getValue() != CUSTOM_WSTR) {
		m_preset.setValue(CUSTOM_WSTR);
		getApplication()->getCurrentTool()->notifyToolChanged();
	}

	return true;
}

//------------------------------------------------------------------

void FullColorBrushTool::initPresets()
{
	if (!m_presetsLoaded) {
		//If necessary, load the presets from file
		m_presetsLoaded = true;
		m_presetsManager.load(TEnv::getConfigDir() + "brush_raster.txt");
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

void FullColorBrushTool::loadPreset()
{
	const std::set<BrushData> &presets = m_presetsManager.presets();
	std::set<BrushData>::const_iterator it;

	it = presets.find(BrushData(m_preset.getValue()));
	if (it == presets.end())
		return;

	const BrushData &preset = *it;

	try //Don't bother with RangeErrors
	{
		m_thickness.setValue(TIntPairProperty::Value(tmax((int)preset.m_min, 1), preset.m_max));
		m_brushPad = ToolUtils::getBrushPad(preset.m_max, preset.m_hardness * 0.01);
		m_hardness.setValue(preset.m_hardness, true);
		m_opacity.setValue(TDoublePairProperty::Value(preset.m_opacityMin, preset.m_opacityMax));
		m_pressure.setValue(preset.m_pressure);
	} catch (...) {
	}
}

//------------------------------------------------------------------

void FullColorBrushTool::addPreset(QString name)
{
	//Build the preset
	BrushData preset(name.toStdWString());

	preset.m_min = m_thickness.getValue().first;
	preset.m_max = m_thickness.getValue().second;
	preset.m_hardness = m_hardness.getValue();
	preset.m_opacityMin = m_opacity.getValue().first;
	preset.m_opacityMax = m_opacity.getValue().second;
	preset.m_pressure = m_pressure.getValue();

	//Pass the preset to the manager
	m_presetsManager.addPreset(preset);

	//Reinitialize the associated preset enum
	initPresets();

	//Set the value to the specified one
	m_preset.setValue(preset.m_name);
}

//------------------------------------------------------------------

void FullColorBrushTool::removePreset()
{
	std::wstring name(m_preset.getValue());
	if (name == CUSTOM_WSTR)
		return;

	m_presetsManager.removePreset(name);
	initPresets();

	//No parameter change, and set the preset value to custom
	m_preset.setValue(CUSTOM_WSTR);
}

//==========================================================================================================

FullColorBrushToolNotifier::FullColorBrushToolNotifier(FullColorBrushTool *tool)
	: m_tool(tool)
{
	TTool::Application *app = m_tool->getApplication();
	TXshLevelHandle *levelHandle;
	if (app)
		levelHandle = app->getCurrentLevel();
	bool ret = false;
	if (levelHandle) {
		bool ret = connect(levelHandle, SIGNAL(xshCanvasSizeChanged()), this, SLOT(onCanvasSizeChanged()));
		assert(ret);
	}
}

//==========================================================================================================

FullColorBrushTool fullColorPencil("T_Brush");
