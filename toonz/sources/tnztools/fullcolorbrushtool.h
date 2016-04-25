#pragma once

#ifndef FULLCOLORBRUSHTOOL_H
#define FULLCOLORBRUSHTOOL_H

#include "brushtool.h"

//==============================================================

//  Forward declarations

class TTileSetFullColor;
class TTileSaverFullColor;
class BluredBrush;
class FullColorBrushToolNotifier;

//==============================================================

//************************************************************************
//    FullColor Brush Tool declaration
//************************************************************************

class FullColorBrushTool : public TTool
{
	Q_DECLARE_TR_FUNCTIONS(FullColorBrushTool)

public:
	FullColorBrushTool(std::string name);

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

	void onCanvasSizeChanged();

protected:
	TPropertyGroup m_prop;

	TIntPairProperty m_thickness;
	TBoolProperty m_pressure;
	TDoublePairProperty m_opacity;
	TDoubleProperty m_hardness;
	TEnumProperty m_preset;

	TPixel32 m_currentColor;
	int m_styleId,
		m_minThick,
		m_maxThick;

	double m_oldOpacity;

	TPointD m_dpiScale,
		m_mousePos, //!< Current mouse position, in world coordinates.
		m_brushPos; //!< World position the brush will be painted at.

	TRasterP m_backUpRas;
	TRaster32P m_workRaster;

	TRect m_strokeRect,
		m_lastRect;

	QRadialGradient m_brushPad;

	std::vector<TThickPoint> m_points;
	BluredBrush *m_brush;

	TTileSetFullColor *m_tileSet;
	TTileSaverFullColor *m_tileSaver;

	BrushPresetManager m_presetsManager; //!< Manager for presets of this tool instance
	FullColorBrushToolNotifier *m_notifier;

	bool m_presetsLoaded;
	bool m_firstTime;
};

//------------------------------------------------------------

class FullColorBrushToolNotifier : public QObject
{
	Q_OBJECT

	FullColorBrushTool *m_tool;

public:
	FullColorBrushToolNotifier(FullColorBrushTool *tool);

protected slots:

	void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
};

#endif //FULLCOLORBRUSHTOOL_H
