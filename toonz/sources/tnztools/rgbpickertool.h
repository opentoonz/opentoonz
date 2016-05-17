#pragma once

#ifndef RGBPICKERTOOL_H
#define RGBPICKERTOOL_H

#include "tools/tool.h"
#include "tproperty.h"
#include "toonz/strokegenerator.h"
#include "tools/tooloptions.h"

#include <QCoreApplication>

class RGBPickerTool : public TTool
{
	Q_DECLARE_TR_FUNCTIONS(RGBPickerTool)

	bool m_firstTime;
	int m_currentStyleId;
	TPixel32 m_oldValue, m_currentValue;

	TRectD m_selectingRect;
	TRectD m_drawingRect;
	TPropertyGroup m_prop;
	TEnumProperty m_pickType;

	TBoolProperty m_passivePick;
	std::vector<RGBPickerToolOptionsBox *> m_toolOptionsBox;

	//Aggiunte per disegnare il lazzo a la polyline
	StrokeGenerator m_drawingTrack;
	StrokeGenerator m_workingTrack;
	TPointD m_firstDrawingPos, m_firstWorkingPos;
	TPointD m_mousePosition;
	double m_thick;
	TStroke *m_stroke;
	TStroke *m_firstStroke;
	std::vector<TPointD> m_drawingPolyline;
	std::vector<TPointD> m_workingPolyline;
	bool m_makePick;

public:
	RGBPickerTool();

	/*-- ToolOptionBox上にPassiveに拾った色を表示するため --*/
	void setToolOptionsBox(RGBPickerToolOptionsBox *toolOptionsBox);

	ToolType getToolType() const { return TTool::LevelReadTool; }

	void updateTranslation();

	// Used to notify and set the currentColor outside the draw() methods:
	// using special style there was a conflict between the draw() methods of the tool
	// and the genaration of the icon inside the style editor (makeIcon()) which use
	// another glContext

	void onImageChanged();

	void draw();

	void leftButtonDown(const TPointD &pos, const TMouseEvent &e);

	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);

	void leftButtonUp(const TPointD &pos, const TMouseEvent &);

	void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e);

	void mouseMove(const TPointD &pos, const TMouseEvent &e);

	void pick(TPoint pos);

	void pickRect();

	void pickStroke();

	bool onPropertyChanged(std::string propertyName);

	void onActivate();

	TPropertyGroup *getProperties(int targetType);

	int getCursorId() const;

	void doPolylinePick();

	//!Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del lazzo. Viene inizializzato \b m_firstPos
	void startFreehand(const TPointD &drawingPos, const TPointD &workingPos);

	//!Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del lazzo.
	void freehandDrag(const TPointD &drawingPos, const TPointD &workingPos);

	//!Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene creato lo stroke rappresentante il lazzo.
	void closeFreehand();

	//!Viene aggiunto un punto al vettore m_polyline.
	void addPointPolyline(const TPointD &drawingPos, const TPointD &workingPos);

	//!Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b m_polyline.front() alla fine del vettore)
	void closePolyline(const TPointD &drawingPos, const TPointD &workingPos);

	/*--- RGBPickerToolをFlipbookで有効にする ---*/
	void showFlipPickedColor(const TPixel32 &pix);
};

#endif