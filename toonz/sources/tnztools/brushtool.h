#pragma once

#ifndef BRUSHTOOL_H
#define BRUSHTOOL_H

#include "tgeometry.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "ttoonzimage.h"

#include "toonz/strokegenerator.h"

#include "tools/tool.h"
#include "tools/cursors.h"

#include <QCoreApplication>
#include <QRadialGradient>

//--------------------------------------------------------------

//  Forward declarations

class TTileSetCM32;
class TTileSaverCM32;
class RasterStrokeGenerator;
class BluredBrush;

//--------------------------------------------------------------

//************************************************************************
//    Brush Data declaration
//************************************************************************

struct BrushData : public TPersist {
	PERSIST_DECLARATION(BrushData)

	std::wstring m_name;
	double m_min, m_max, m_acc, m_hardness, m_opacityMin, m_opacityMax;
	bool m_selective, m_pencil, m_breakAngles, m_pressure;
	int m_cap, m_join, m_miter;

	BrushData();
	BrushData(const std::wstring &name);

	bool operator<(const BrushData &other) const { return m_name < other.m_name; }

	void saveData(TOStream &os);
	void loadData(TIStream &is);
};

//************************************************************************
//    Brush Preset Manager declaration
//************************************************************************

class BrushPresetManager
{
	TFilePath m_fp;				   //!< Presets file path
	std::set<BrushData> m_presets; //!< Current presets container

public:
	BrushPresetManager() {}

	void load(const TFilePath &fp);
	void save();

	const TFilePath &path() { return m_fp; };
	const std::set<BrushData> &presets() const { return m_presets; }

	void addPreset(const BrushData &data);
	void removePreset(const std::wstring &name);
};

//************************************************************************
//    Brush Tool declaration
//************************************************************************

class BrushTool : public TTool
{
	Q_DECLARE_TR_FUNCTIONS(BrushTool)

public:
	BrushTool(std::string name, int targetType);

	ToolType getToolType() const { return TTool::LevelWriteTool; }

	ToolOptionsBox *createOptionsBox();

	void updateTranslation();

	void onActivate();
	void onDeactivate();

	bool preLeftButtonDown();
	void leftButtonDown(const TPointD &pos, const TMouseEvent &e);
	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
	void leftButtonUp(const TPointD &pos, const TMouseEvent &e);
	void mouseMove(const TPointD &pos, const TMouseEvent &e);

	void draw();

	void onEnter();
	void onLeave();

	int getCursorId() const { return ToolCursor::PenCursor; }

	TPropertyGroup *getProperties(int targetType);
	bool onPropertyChanged(std::string propertyName);

	void onImageChanged();
	void setWorkAndBackupImages();
	void updateWorkAndBackupRasters(const TRect &rect);

	void initPresets();
	void loadPreset();
	void addPreset(QString name);
	void removePreset();

	void finishRasterBrush(const TPointD &pos, int pressureVal);
	//return true if the pencil mode is active in the Brush / PaintBrush / Eraser Tools.
	bool isPencilModeActive();

protected:
	TPropertyGroup m_prop[2];

	TDoublePairProperty m_thickness;
	TDoublePairProperty m_rasThickness;
	TDoubleProperty m_accuracy;
	TDoubleProperty m_hardness;
	TEnumProperty m_preset;
	TBoolProperty m_selective;
	TBoolProperty m_breakAngles;
	TBoolProperty m_pencil;
	TBoolProperty m_pressure;
	TEnumProperty m_capStyle;
	TEnumProperty m_joinStyle;
	TIntProperty m_miterJoinLimit;

	StrokeGenerator m_track;
	RasterStrokeGenerator *m_rasterTrack;

	TTileSetCM32 *m_tileSet;
	TTileSaverCM32 *m_tileSaver;

	TPixel32 m_currentColor;
	int m_styleId;
	double m_minThick, m_maxThick;

	TRectD m_modifiedRegion;
	TPointD m_dpiScale,
		m_mousePos, //!< Current mouse position, in world coordinates.
		m_brushPos; //!< World position the brush will be painted at.

	BluredBrush *m_bluredBrush;
	QRadialGradient m_brushPad;

	TRasterCM32P m_backupRas;
	TRaster32P m_workRas;

	std::vector<TThickPoint> m_points;
	TRect m_strokeRect,
		m_lastRect;

	BrushPresetManager m_presetsManager; //!< Manager for presets of this tool instance

	bool m_active,
		m_enabled,
		m_isPrompting, //!< Whether the tool is prompting for spline substitution.
		m_firstTime,
		m_isPath,
		m_presetsLoaded;

	/*--- 作業中のFrameIdをクリック時に保存し、マウスリリース時（Undoの登録時）に別のフレームに
  移動していたときの不具合を修正する。---*/
	TFrameId m_workingFrameId;

protected:
	static void drawLine(const TPointD &point, const TPointD &centre, bool horizontal, bool isDecimal);
	static void drawEmptyCircle(TPointD point, int thick, bool isLxEven, bool isLyEven, bool isPencil);
};

#endif //BRUSHTOOL_H
