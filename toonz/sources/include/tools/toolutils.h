

#ifndef TOOLSUTILS_H
#define TOOLSUTILS_H

// TnzCore includes
#include "tcommon.h"
#include "tundo.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tregion.h"
#include "tcurves.h"
#include "tpixel.h"
#include "tfilepath.h"
#include "tpalette.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"

// TnzTools includes
#include "tools/tool.h"

#include "historytypes.h"

// Qt includes
#include <QString>
#include <QMenu>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//===================================================================

//    Forward declarations

class TVectorImageP;
class TTileSetCM32;
class TTileSetFullColor;
class TStageObjectSpline;

//===================================================================

//*****************************************************************************
//    Mixed Tool utilities
//*****************************************************************************

namespace ToolUtils
{

typedef std::vector<TStroke *> ArrayOfStroke;
typedef ArrayOfStroke::const_iterator citAS;
typedef ArrayOfStroke::iterator itAS;
typedef std::vector<TThickQuadratic *> ArrayOfTQ;
typedef std::vector<double> ArrayOfDouble;
typedef ArrayOfTQ::iterator itATQ;
typedef ArrayOfTQ::const_iterator citATQ;

// inserito per rendere piu' piccolo il numero di punti delle stroke ricampionate
const double ReduceControlPointCorrection = 2.0;

// controlla la dimensione del raggio di pick sotto la quale click and up
//  risulta insesibile
const double PickRadius = 1.5;

//-----------------------------------------------------------------------------

void DVAPI drawRect(const TRectD &rect, const TPixel32 &color, unsigned short stipple, bool doContrast = false);

//-----------------------------------------------------------------------------

void DVAPI fillRect(const TRectD &rect, const TPixel32 &color);

//-----------------------------------------------------------------------------

void drawPoint(const TPointD &q, double pixelSize);

//-----------------------------------------------------------------------------

void drawCross(const TPointD &q, double pixelSize);

//-----------------------------------------------------------------------------

void drawArrow(const TSegment &s, double pixelSize);

//-----------------------------------------------------------------------------

void DVAPI drawSquare(const TPointD &pos, double r, const TPixel32 &color);

//-----------------------------------------------------------------------------

void drawRectWhitArrow(const TPointD &pos, double r);

//-----------------------------------------------------------------------------

QRadialGradient getBrushPad(int size, double hardness);

//-----------------------------------------------------------------------------

//! Divide il primo rettangolo in piu' sottorettsngoli, in base all'intersezione con il secondo
QList<TRect> splitRect(const TRect &first, const TRect &second);

//-----------------------------------------------------------------------------

//! Return a transparent TRaster32P conteaining the self looped stroke fileed with white!
//! If the stroke isn't self looped, the first point of the stroke is addee at the end.
//! The returned image has the size of the stroke bounding box intrsected with the \b imageBounds.
//! If this intersection is empty a TRaster32P() is returned.
TRaster32P convertStrokeToImage(TStroke *stroke, const TRect &imageBounds, TPoint &pos);

//-----------------------------------------------------------------------------

void drawBalloon(
	const TPointD &pos,	// position "pointed" by the balloon (world units)
	std::string text,	  // balloon text
	const TPixel32 &color, // ballon background color (text is black)
	TPoint delta,		   // text position (pixels; pos is the origin; y grows upward)
	bool isPicking = false,
	std::vector<TRectD> *otherBalloons = 0); // avoid other balloons positions; add the new ballons positions

//-----------------------------------------------------------------------------

enum HookType { NormalHook,
				PassHookA,
				PassHookB,
				OtherLevelHook };
void drawHook(
	const TPointD &p,
	HookType type,
	bool highlighted = false,
	bool onionSkin = false);

//-----------------------------------------------------------------------------

TStroke *merge(const ArrayOfStroke &a);

//================================================================================================

class DVAPI TToolUndo : public TUndo
{
protected:
	TXshSimpleLevelP m_level;
	TFrameId m_frameId;
	int m_row, m_col;
	bool m_isEditingLevel;
	bool m_createdFrame;
	bool m_createdLevel;
	bool m_animationSheetEnabled;
	std::vector<int> m_cellsData; // represent original frame range when m_animationSheetEnabled, m_createdFrame and !m_isEditingLevel; see tool.cpp
	TPaletteP m_oldPalette;
	std::string m_imageId;
	static int m_idCount;

	void insertLevelAndFrameIfNeeded() const;
	void removeLevelAndFrameIfNeeded() const;

	void notifyImageChanged() const;

public:
	TToolUndo(TXshSimpleLevel *level, const TFrameId &frameId,
			  bool createdFrame = false, bool createdLevel = false, const TPaletteP &oldPalette = 0);
	~TToolUndo();

	virtual QString getToolName()
	{
		return QString("Tool");
	}
	virtual QString getHistoryString()
	{
		return QObject::tr("%1   Level : %2  Frame : %3")
			.arg(getToolName())
			.arg(QString::fromStdWString(m_level->getName()))
			.arg(QString::number(m_frameId.getNumber()));
	}
};

//================================================================================================

class DVAPI TRasterUndo : public TToolUndo
{

protected:
	TTileSetCM32 *m_tiles;

	TToonzImageP getImage() const;

public:
	TRasterUndo(TTileSetCM32 *tiles, TXshSimpleLevel *level, const TFrameId &frameId,
				bool createdFrame, bool createdLevel, const TPaletteP &oldPalette); //get tiles ownership
	~TRasterUndo();

	int getSize() const;
	void undo() const;

	virtual QString getToolName()
	{
		return QString("Raster Tool");
	}
};

//================================================================================================

class DVAPI TFullColorRasterUndo : public TToolUndo
{

protected:
	TTileSetFullColor *m_tiles;

	TRasterImageP getImage() const;

public:
	TFullColorRasterUndo(TTileSetFullColor *tiles, TXshSimpleLevel *level, const TFrameId &frameId,
						 bool createdFrame, bool createdLevel, const TPaletteP &oldPalette); //get tiles ownership
	~TFullColorRasterUndo();

	int getSize() const;
	void undo() const;

private:
	std::vector<TRect> paste(const TRasterImageP &ti, const TTileSetFullColor *tileSet) const;
};

//-----------------------------------------------------------------------------

class UndoModifyStroke : public TToolUndo
{
	std::vector<TThickPoint> m_before, m_after;
	bool m_selfLoopBefore, m_selfLoopAfter;

	int m_row;
	int m_column;

public:
	int m_strokeIndex;

	UndoModifyStroke(TXshSimpleLevel *level, const TFrameId &frameId, int strokeIndex);

	~UndoModifyStroke();

	void onAdd();
	void undo() const;
	void redo() const;
	int getSize() const;
};

//-----------------------------------------------------------------------------

class UndoModifyStrokeAndPaint : public UndoModifyStroke
{
	std::vector<TFilledRegionInf> *m_fillInformation;
	TRectD m_oldBBox;

public:
	UndoModifyStrokeAndPaint(TXshSimpleLevel *level, const TFrameId &frameId, int strokeIndex);

	~UndoModifyStrokeAndPaint();

	void onAdd();
	void undo() const;
	int getSize() const;

	QString getToolName()
	{
		return QObject::tr("Modify Stroke Tool");
	}
};

//-----------------------------------------------------------------------------

class UndoModifyListStroke : public TToolUndo
{
	std::list<UndoModifyStroke *> m_strokeList;
	std::list<UndoModifyStroke *>::iterator m_beginIt, m_endIt;

	std::vector<TFilledRegionInf> *m_fillInformation;
	TRectD m_oldBBox;

public:
	UndoModifyListStroke(TXshSimpleLevel *level, const TFrameId &frameId, const std::vector<TStroke *> &strokeList);

	~UndoModifyListStroke();

	void onAdd();
	void undo() const;
	void redo() const;
	int getSize() const;

	QString getToolName()
	{
		return QObject::tr("Modify Stroke Tool");
	}
};

//-----------------------------------------------------------------------------

class UndoPencil : public TToolUndo
{
	int m_strokeId;
	TStroke *m_stroke;
	std::vector<TFilledRegionInf> *m_fillInformation;

	bool m_autogroup;
	bool m_autofill;

public:
	UndoPencil(TStroke *stroke, std::vector<TFilledRegionInf> *fillInformation, TXshSimpleLevel *level,
			   const TFrameId &frameId, bool m_createdFrame, bool m_createdLevel,
			   bool autogroup = false, bool autofill = false);
	~UndoPencil();
	void undo() const;
	void redo() const;
	int getSize() const;

	QString getToolName()
	{
		return QString("Vector Brush Tool");
	}
	int getHistoryType()
	{
		return HistoryType::BrushTool;
	}
};

//-----------------------------------------------------------------------------
/*--  Hardness=100 又は Pencilモード のときのGeometricToolのUndo --*/
class UndoRasterPencil : public TRasterUndo
{
protected:
	TStroke *m_stroke;
	bool m_selective, m_filled, m_doAntialias;

	/*-- HistoryにPrimitive名を表示するため --*/
	std::string m_primitiveName;

public:
	UndoRasterPencil(TXshSimpleLevel *level, const TFrameId &frameId, TStroke *stroke,
					 bool selective, bool filled, bool doAntialias, bool createdFrame, bool createdLevel,
					 std::string primitiveName);
	~UndoRasterPencil();
	virtual void redo() const;
	int getSize() const;

	QString getToolName()
	{
		return QString("Geometric Tool : %1").arg(QString::fromStdString(m_primitiveName));
	}
	int getHistoryType()
	{
		return HistoryType::GeometricTool;
	}
};

//-----------------------------------------------------------------------------

class UndoFullColorPencil : public TFullColorRasterUndo
{
protected:
	TStroke *m_stroke;
	double m_opacity;
	bool m_doAntialias;

public:
	UndoFullColorPencil(TXshSimpleLevel *level, const TFrameId &frameId, TStroke *stroke, double opacity,
						bool doAntialias, bool createdFrame, bool createdLevel);
	~UndoFullColorPencil();
	virtual void redo() const;
	int getSize() const;
	QString getToolName()
	{
		return QString("Geometric Tool");
	}
	int getHistoryType()
	{
		return HistoryType::GeometricTool;
	}
};

//-----------------------------------------------------------------------------
//
// undo class (path strokes). call it BEFORE and register it AFTER path change
//
class UndoPath : public TUndo
{

	TStageObjectSpline *m_spline;
	std::vector<TThickPoint> m_before, m_after;
	bool m_selfLoopBefore;
	//TStroke *m_before;
	//TStroke *m_after;

public:
	UndoPath(TStageObjectSpline *spline);
	~UndoPath();
	void onAdd();
	void undo() const;
	void redo() const;
	int getSize() const;
	QString getHistoryString()
	{
		return QObject::tr("Modify Spline");
	}
	int getHistoryType()
	{
		return HistoryType::ControlPointEditorTool;
	}
};

//-----------------------------------------------------------------------------
//
// UndoControlPointEditor
//
class UndoControlPointEditor : public TToolUndo
{
	std::pair<int, VIStroke *> m_oldStroke;
	std::pair<int, VIStroke *> m_newStroke;
	bool m_isStrokeDelete;

	int m_row;
	int m_column;

public:
	UndoControlPointEditor(TXshSimpleLevel *level, const TFrameId &frameId);
	~UndoControlPointEditor();

	void onAdd();
	void addOldStroke(int index, VIStroke *vs);
	void addNewStroke(int index, VIStroke *vs);
	void isStrokeDelete(bool isStrokeDelete) { m_isStrokeDelete = isStrokeDelete; }

	void undo() const;
	void redo() const;

	int getSize() const
	{
		return sizeof(*this) + /*(m_oldFillInformation.capacity()+m_newFillInformation.capacity())*sizeof(TFilledRegionInf) +*/ 500;
	}
	virtual QString getToolName()
	{
		return QString("Control Point Editor");
	}
	int getHistoryType()
	{
		return HistoryType::ControlPointEditorTool;
	}
};

//-----------------------------------------------------------------------------
//
// DragMenu
//

class DragMenu : public QMenu
{
public:
	DragMenu();
	QAction *exec(const QPoint &p, QAction *action = 0);

protected:
	void mouseReleaseEvent(QMouseEvent *e);
};

//-----------------------------------------------------------------------------
//
// ChooseColumnMenu
//

class ColumChooserMenu : public DragMenu
{
public:
	ColumChooserMenu(TXsheet *xsh, const std::vector<int> &columnIndexes);
	int execute();
};

//-----------------------------------------------------------------------------

class ConeSubVolume
{
	const static double m_values[21];

public:
	ConeSubVolume()
	{
	}

	//calcola il sottovolume di un cono di raggio e volume unitario in base
	static double compute(double cover);
};

//-----------------------------------------------------------------------------

//! Return the right value in both case: LevelFrame and SceneFrame.
TFrameId DVAPI getFrameId();

/*
    The following functions set the specified (or current) level frame as edited (ie to be saved),
    and, in the colormap case, update the associated savebox.

    The 'Minimize Savebox after Editing' preference is used to determine if the savebox has to be
    shrunk to the image's bounding box or include the previous savebox.
  */

void DVAPI updateSaveBox(const TXshSimpleLevelP &sl, const TFrameId &fid);
void DVAPI updateSaveBox(const TXshSimpleLevelP &sl, const TFrameId &fid, TImageP img);
void DVAPI updateSaveBox();

bool DVAPI isJustCreatedSpline(TImage *image);

TRectD DVAPI interpolateRect(const TRectD &rect1, const TRectD &rect2, double t);

//bool DVAPI isASubRegion(int reg, const vector<TRegion*> &regions);

//!Returns a TRectD that contains all points in \b points
//!If \b maxThickness is not zero, the TRectD is computed using this value.
TRectD DVAPI getBounds(const std::vector<TThickPoint> &points, double maxThickness = 0);

//!Ritorna un raster uguale a quello dato ma ruotato di 90 gradi
//!Il parametro toRight indica se si sta ruotando a destra o a sinistra!
template <typename PIXEL>
TRasterPT<PIXEL> rotate90(const TRasterPT<PIXEL> &ras, bool toRight)
{
	if (!ras)
		return TRasterPT<PIXEL>(0);
	int lx = ras->getLy();
	int ly = ras->getLx();
	TRasterPT<PIXEL> workRas(lx, ly);
	for (int i = 0; i < ras->getLx(); i++) {
		for (int j = 0; j < ras->getLy(); j++) {
			if (toRight)
				workRas->pixels(ly - 1 - i)[j] = ras->pixels(j)[i];
			else
				workRas->pixels(i)[lx - 1 - j] = ras->pixels(j)[i];
		}
	}
	return workRas;
}

} // namespace ToolUtils

#endif // TOOLSUTILS_H
