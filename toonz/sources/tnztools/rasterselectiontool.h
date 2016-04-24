

#ifndef RASTERSELECTIONTOOL_INCLUDED
#define RASTERSELECTIONTOOL_INCLUDED

#include "selectiontool.h"
#include "setsaveboxtool.h"
#include "tools/rasterselection.h"

// forward declaration
class RasterSelectionTool;
class VectorFreeDeformer;

//=============================================================================
// RasterFreeDeformer
//-----------------------------------------------------------------------------

class RasterFreeDeformer : public FreeDeformer
{
	TRasterP m_ras;
	TRasterP m_newRas;
	bool m_noAntialiasing;

public:
	RasterFreeDeformer(TRasterP ras);
	~RasterFreeDeformer();

	/*! Set \b index point to \b p, with index from 0 to 3. */
	void setPoint(int index, const TPointD &p);
	/*! Helper function. */
	void setPoints(const TPointD &p0, const TPointD &p1, const TPointD &p2, const TPointD &p3);
	TRasterP getImage() const { return m_newRas; }
	void deformImage();
	void setNoAntialiasing(bool value) { m_noAntialiasing = value; }
};

//=============================================================================
// DragSelectionTool
//-----------------------------------------------------------------------------

namespace DragSelectionTool
{

//=============================================================================
// UndoRasterDeform
//-----------------------------------------------------------------------------

class UndoRasterDeform : public TUndo
{
	static int m_id;
	RasterSelectionTool *m_tool;
	std::string m_oldFloatingImageId, m_newFloatingImageId;
	std::vector<TStroke> m_newStrokes;
	std::vector<TStroke> m_oldStrokes;
	DragSelectionTool::DeformValues m_oldDeformValues, m_newDeformValues;
	FourPoints m_oldBBox, m_newBBox;
	TPointD m_oldCenter, m_newCenter;

	TDimension m_dim;
	int m_pixelSize;

public:
	UndoRasterDeform(RasterSelectionTool *tool);
	~UndoRasterDeform();

	void registerRasterDeformation();

	void undo() const;
	void redo() const;

	int getSize() const;

	QString getHistoryString()
	{
		return QObject::tr("Deform Raster");
	}
};

//=============================================================================
// UndoRasterTransform
//-----------------------------------------------------------------------------

class UndoRasterTransform : public TUndo
{
	RasterSelectionTool *m_tool;
	TAffine m_oldTransform, m_newTransform;
	TPointD m_oldCenter, m_newCenter;
	FourPoints m_oldBbox, m_newBbox;
	DragSelectionTool::DeformValues m_oldDeformValues, m_newDeformValues;

public:
	UndoRasterTransform(RasterSelectionTool *tool);
	void setChangedValues();
	void undo() const;
	void redo() const;

	int getSize() const { return sizeof(*this); }

	QString getHistoryString()
	{
		return QObject::tr("Transform Raster");
	}
};

//=============================================================================
// RasterDeformTool
//-----------------------------------------------------------------------------

class RasterDeformTool : public DeformTool
{
protected:
	TAffine m_transform;
	UndoRasterTransform *m_transformUndo;
	UndoRasterDeform *m_deformUndo;

	//!It's true when use RasterFreeDeformer
	bool m_isFreeDeformer;

	void applyTransform(FourPoints bbox);
	void applyTransform(TAffine aff, bool modifyCenter);
	void addTransformUndo();

	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e){};
	void draw(){};

public:
	RasterDeformTool(RasterSelectionTool *tool, bool freeDeformer);
};

//=============================================================================
// RasterRotationTool
//-----------------------------------------------------------------------------

class RasterRotationTool : public RasterDeformTool
{
	Rotation *m_rotation;

public:
	RasterRotationTool(RasterSelectionTool *tool);
	void transform(TAffine aff, double angle);
	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
	void draw();
};

//=============================================================================
// RasterFreeDeformTool
//-----------------------------------------------------------------------------

class RasterFreeDeformTool : public RasterDeformTool
{
	FreeDeform *m_freeDeform;

public:
	RasterFreeDeformTool(RasterSelectionTool *tool);
	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
};

//=============================================================================
// RasterMoveSelectionTool
//-----------------------------------------------------------------------------

class RasterMoveSelectionTool : public RasterDeformTool
{
	MoveSelection *m_moveSelection;

public:
	RasterMoveSelectionTool(RasterSelectionTool *tool);
	void transform(TAffine aff);
	void leftButtonDown(const TPointD &pos, const TMouseEvent &e);
	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
};

//=============================================================================
// RasterScaleTool
//-----------------------------------------------------------------------------

class RasterScaleTool : public RasterDeformTool
{
	Scale *m_scale;

public:
	RasterScaleTool(RasterSelectionTool *tool, int type);
	/*! Return scale value. */
	TPointD transform(int index, TPointD newPos);
	void leftButtonDown(const TPointD &pos, const TMouseEvent &e);
	void leftButtonDrag(const TPointD &pos, const TMouseEvent &e);
};

} //namespace DragSelectionTool

//=============================================================================
// RasterSelectionTool
//-----------------------------------------------------------------------------

class RasterSelectionTool : public SelectionTool
{
	Q_DECLARE_TR_FUNCTIONS(RasterSelectionTool)

	RasterSelection m_rasterSelection;
	int m_transformationCount;

	//!Used in ToonzRasterImage to switch from selection tool to modify savebox tool.
	TBoolProperty m_modifySavebox;
	SetSaveboxTool *m_setSaveboxTool;
	TBoolProperty m_noAntialiasing;

	//!Used to deform bbox strokes.
	VectorFreeDeformer *m_selectionFreeDeformer;

	void modifySelectionOnClick(TImageP image, const TPointD &pos, const TMouseEvent &e);

	void drawFloatingSelection();

public:
	RasterSelectionTool(int targetType);

	void setBBox(const DragSelectionTool::FourPoints &points, int index = 0);

	void setNewFreeDeformer();
	VectorFreeDeformer *getSelectionFreeDeformer() const;

	bool isFloating() const;

	void leftButtonDown(const TPointD &pos, const TMouseEvent &);
	void mouseMove(const TPointD &pos, const TMouseEvent &e);
	void leftButtonDrag(const TPointD &pos, const TMouseEvent &);
	void leftButtonUp(const TPointD &pos, const TMouseEvent &);
	void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e);

	void draw();

	TSelection *getSelection();
	bool isSelectionEmpty();

	void computeBBox();

	void doOnActivate();
	void doOnDeactivate();

	void onImageChanged();

	void transformFloatingSelection(const TAffine &affine, const TPointD &center,
									const DragSelectionTool::FourPoints &points);
	void increaseTransformationCount();
	void decreaseTransformationCount();

	void onActivate();
	TBoolProperty *getModifySaveboxProperty()
	{
		if (m_targetType & ToonzImage)
			return &m_modifySavebox;
		return 0;
	}
	bool onPropertyChanged(std::string propertyName);
	bool getNoAntialiasingValue() { return m_noAntialiasing.getValue(); }

protected:
	void updateTranslation();
};

#endif //RASTERSELECTIONTOOL_INCLUDED
