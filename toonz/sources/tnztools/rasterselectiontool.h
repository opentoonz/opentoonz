#pragma once

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

class RasterFreeDeformer final : public FreeDeformer {
  TRasterP m_ras;
  TRasterP m_newRas;
  bool m_noAntialiasing;

public:
  RasterFreeDeformer(TRasterP ras);
  ~RasterFreeDeformer();

  /*! Set \b index point to \b p, with index from 0 to 3. */
  void setPoint(int index, const TPointD &p) override;
  /*! Helper function. */
  void setPoints(const TPointD &p0, const TPointD &p1, const TPointD &p2,
                 const TPointD &p3) override;
  TRasterP getImage() const { return m_newRas; }
  void deformImage() override;
  void setNoAntialiasing(bool value) { m_noAntialiasing = value; }
};

//=============================================================================
// DragSelectionTool
//-----------------------------------------------------------------------------

namespace DragSelectionTool {

//=============================================================================
// UndoRasterDeform
//-----------------------------------------------------------------------------

class UndoRasterDeform final : public TUndo {
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

  void undo() const override;
  void redo() const override;

  int getSize() const override;

  QString getHistoryString() override { return QObject::tr("Deform Raster"); }
};

//=============================================================================
// UndoRasterTransform
//-----------------------------------------------------------------------------

class UndoRasterTransform final : public TUndo {
  RasterSelectionTool *m_tool;
  TAffine m_oldTransform, m_newTransform;
  TPointD m_oldCenter, m_newCenter;
  FourPoints m_oldBbox, m_newBbox;
  DragSelectionTool::DeformValues m_oldDeformValues, m_newDeformValues;

public:
  UndoRasterTransform(RasterSelectionTool *tool);
  void setChangedValues();
  void undo() const override;
  void redo() const override;

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Transform Raster");
  }
};

//=============================================================================
// RasterDeformTool
//-----------------------------------------------------------------------------

class RasterDeformTool : public DeformTool {
protected:
  TAffine m_transform;
  UndoRasterTransform *m_transformUndo;
  UndoRasterDeform *m_deformUndo;

  //! It's true when use RasterFreeDeformer
  bool m_isFreeDeformer;

  void applyTransform(FourPoints bbox, bool onFastDragging = false) override;
  void applyTransform(TAffine aff, bool modifyCenter);
  void addTransformUndo() override;

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override{};
  void draw() override{};

public:
  RasterDeformTool(RasterSelectionTool *tool, bool freeDeformer);
};

//=============================================================================
// RasterRotationTool
//-----------------------------------------------------------------------------

class RasterRotationTool final : public RasterDeformTool {
  Rotation *m_rotation;

public:
  RasterRotationTool(RasterSelectionTool *tool);
  void transform(TAffine aff, double angle) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void draw() override;
};

//=============================================================================
// RasterFreeDeformTool
//-----------------------------------------------------------------------------

class RasterFreeDeformTool final : public RasterDeformTool {
  FreeDeform *m_freeDeform;

public:
  RasterFreeDeformTool(RasterSelectionTool *tool);
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// RasterMoveSelectionTool
//-----------------------------------------------------------------------------

class RasterMoveSelectionTool final : public RasterDeformTool {
  MoveSelection *m_moveSelection;

public:
  RasterMoveSelectionTool(RasterSelectionTool *tool);
  void transform(TAffine aff) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// RasterScaleTool
//-----------------------------------------------------------------------------

class RasterScaleTool final : public RasterDeformTool {
  Scale *m_scale;

public:
  RasterScaleTool(RasterSelectionTool *tool, ScaleType type);
  /*! Return scale value. */
  TPointD transform(int index, TPointD newPos,
                    bool onFastDragging = false) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
};

}  // namespace DragSelectionTool

//=============================================================================
// RasterSelectionTool
//-----------------------------------------------------------------------------

class RasterSelectionTool final : public SelectionTool {
  Q_DECLARE_TR_FUNCTIONS(RasterSelectionTool)

  RasterSelection m_rasterSelection;
  int m_transformationCount;

  //! Used in ToonzRasterImage to switch from selection tool to modify savebox
  //! tool.
  TBoolProperty m_modifySavebox;
  SetSaveboxTool *m_setSaveboxTool;
  TBoolProperty m_noAntialiasing;

  //! Used to deform bbox strokes.
  VectorFreeDeformer *m_selectionFreeDeformer;

  void modifySelectionOnClick(TImageP image, const TPointD &pos,
                              const TMouseEvent &e) override;

  void drawFloatingSelection();

public:
  RasterSelectionTool(int targetType);

  void setBBox(const DragSelectionTool::FourPoints &points,
               int index = 0) override;

  void setNewFreeDeformer() override;
  VectorFreeDeformer *getSelectionFreeDeformer() const;

  bool isFloating() const override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override;

  void draw() override;

  TSelection *getSelection() override;
  bool isSelectionEmpty() override;

  void computeBBox() override;

  void doOnActivate() override;
  void doOnDeactivate() override;

  void onImageChanged() override;

  void transformFloatingSelection(const TAffine &affine, const TPointD &center,
                                  const DragSelectionTool::FourPoints &points);
  void increaseTransformationCount();
  void decreaseTransformationCount();

  void onActivate() override;
  TBoolProperty *getModifySaveboxProperty() {
    if (m_targetType & ToonzImage) return &m_modifySavebox;
    return 0;
  }
  bool onPropertyChanged(std::string propertyName) override;
  bool getNoAntialiasingValue() { return m_noAntialiasing.getValue(); }

  bool isSelectionEditable() override { return m_rasterSelection.isEditable(); }

protected:
  void updateTranslation() override;
};

#endif  // RASTERSELECTIONTOOL_INCLUDED
