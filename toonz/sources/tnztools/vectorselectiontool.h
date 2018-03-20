#pragma once

#ifndef VECTORSELECTIONTOOL_H
#define VECTORSELECTIONTOOL_H

#include "selectiontool.h"

// TnzTools includes
#include "tools/strokeselection.h"
#include "tools/levelselection.h"

// TnzCore includes
#include "tstroke.h"
#include "tregion.h"

// Qt includes
#include <QSet>

// tcg includes
#include "tcg/tcg_unique_ptr.h"

//============================================================

//    Forward declarations

class VectorSelectionTool;

//============================================================

//=============================================================================
// Constants / Defines
//-----------------------------------------------------------------------------

enum SelectionTarget         //! Possible selection targets in a SelectionTool.
{ NORMAL_TYPE_IDX,           //!< Normal selection (strokes at current frame).
  SELECTED_FRAMES_TYPE_IDX,  //!< Selection of whole frames.
  ALL_LEVEL_TYPE_IDX,        //!< Selection of an entire level.
  SAME_STYLE_TYPE_IDX,       //!< Selected styles at current frame.
  STYLE_SELECTED_FRAMES_TYPE_IDX,  //!< Selected styles at selected frames.
  STYLE_LEVEL_TYPE_IDX,  //!< Selected styles at current frame on the whole
                         //! level.
  BOUNDARY_TYPE_IDX,     //!< Boundary strokes on current frame.
  BOUNDARY_SELECTED_FRAMES_TYPE_IDX,  //!< Boundary strokes on selected frames.
  BOUNDARY_LEVEL_TYPE_IDX,            //!< Boundary strokes on the whole level.
};

#define NORMAL_TYPE L"Standard"
#define SELECTED_FRAMES_TYPE L"Selected Frames"
#define ALL_LEVEL_TYPE L"Whole Level"
#define SAME_STYLE_TYPE L"Same Style"
#define STYLE_SELECTED_FRAMES_TYPE L"Same Style on Selected Frames"
#define STYLE_LEVEL_TYPE L"Same Style on Whole Level"
#define BOUNDARY_TYPE L"Boundary Strokes"
#define BOUNDARY_SELECTED_FRAMES_TYPE L"Boundaries on Selected Frames"
#define BOUNDARY_LEVEL_TYPE L"Boundaries on Whole Level"

#define ROUNDC_WSTR L"round_cap"
#define BUTT_WSTR L"butt_cap"
#define PROJECTING_WSTR L"projecting_cap"
#define ROUNDJ_WSTR L"round_join"
#define BEVEL_WSTR L"bevel_join"
#define MITER_WSTR L"miter_join"

//=============================================================================
// VectorFreeDeformer
//-----------------------------------------------------------------------------

class VectorFreeDeformer final : public FreeDeformer {
  TVectorImageP m_vi;
  std::set<int> m_strokeIndexes;
  std::vector<TStroke *> m_originalStrokes;

  bool m_preserveThickness, m_computeRegion, m_flip;

  TThickPoint deform(TThickPoint point);

public:
  VectorFreeDeformer(TVectorImageP vi, std::set<int> strokeIndexes);
  ~VectorFreeDeformer();

  void setPreserveThickness(bool preserveThickness);
  void setComputeRegion(bool computeRegion);
  void setFlip(bool flip);

  /*! Return \b index point, with index from 0 to 3. */
  TPointD getPoint(int index) const { return m_newPoints[index]; }
  /*! Set \b index point to \b p, with index from 0 to 3. */
  void setPoint(int index, const TPointD &p) override;
  /*! Helper function. */
  void setPoints(const TPointD &p0, const TPointD &p1, const TPointD &p2,
                 const TPointD &p3) override;
  /*! Helper function. */
  TVectorImage *getDeformedImage() const { return m_vi.getPointer(); }

  void deformRegions();
  void deformImage() override;
};

//=============================================================================
// DragSelectionTool
//-----------------------------------------------------------------------------

namespace DragSelectionTool {

//=============================================================================
// UndoChangeStrokes
//-----------------------------------------------------------------------------

class UndoChangeStrokes final : public ToolUtils::TToolUndo {
public:
  UndoChangeStrokes(TXshSimpleLevel *level, const TFrameId &frameId,
                    VectorSelectionTool *tool,
                    const StrokeSelection &selection);
  UndoChangeStrokes(TXshSimpleLevel *level, const TFrameId &frameId,
                    VectorSelectionTool *tool, const LevelSelection &selection);
  ~UndoChangeStrokes();

  void registerStrokes(bool beforeModify = false);
  void setFlip(bool value) { m_flip = value; }
  void transform(const std::vector<TStroke *> &strokes, FourPoints bbox,
                 TPointD center, DeformValues deformValue) const;
  void restoreRegions() const;
  void undo() const override;
  void redo() const override;
  int getSize() const override;

private:
  VectorSelectionTool *m_tool;

  std::vector<TStroke *> m_oldStrokes, m_newStrokes;

  std::vector<int> m_indexes;  //!< Selected indexes at current frame.
  std::vector<TFilledRegionInf> m_regionsData;

  int m_selectionCount;

  FourPoints m_oldBBox, m_newBBox;

  TPointD m_oldCenter, m_newCenter;

  DeformValues m_oldDeformValues, m_newDeformValues;

  bool m_flip;
};

//=============================================================================
// VectorDeformTool
//-----------------------------------------------------------------------------

class VectorDeformTool : public DeformTool {
public:
  VectorDeformTool(VectorSelectionTool *tool);
  ~VectorDeformTool();

  void applyTransform(FourPoints bbox) override;
  void addTransformUndo() override;

  /*! Trasform whole level and add undo. */
  void transformWholeLevel();
  bool isFlip();

protected:
  tcg::unique_ptr<UndoChangeStrokes> m_undo;

protected:
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {}

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;

  void draw() override {}

private:
  struct VFDScopedBlock;
  tcg::unique_ptr<VFDScopedBlock> m_vfdScopedBlock;
};

//=============================================================================
// VectorRotationTool
//-----------------------------------------------------------------------------

class VectorRotationTool final : public VectorDeformTool {
  tcg::unique_ptr<Rotation> m_rotation;

public:
  VectorRotationTool(VectorSelectionTool *tool);

  void transform(TAffine aff, double angle) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void draw() override;
};

//=============================================================================
// VectorFreeDeformTool
//-----------------------------------------------------------------------------

class VectorFreeDeformTool final : public VectorDeformTool {
  tcg::unique_ptr<FreeDeform> m_freeDeform;

public:
  VectorFreeDeformTool(VectorSelectionTool *tool);

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// VectorMoveSelectionTool
//-----------------------------------------------------------------------------

class VectorMoveSelectionTool final : public VectorDeformTool {
  tcg::unique_ptr<MoveSelection> m_moveSelection;

public:
  VectorMoveSelectionTool(VectorSelectionTool *tool);

  void transform(TAffine aff) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// VectorScaleTool
//-----------------------------------------------------------------------------

class VectorScaleTool final : public VectorDeformTool {
  tcg::unique_ptr<Scale> m_scale;

public:
  VectorScaleTool(VectorSelectionTool *tool, int type);

  TPointD transform(int index,
                    TPointD newPos) override;  //!< Returns scale value.

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// VectorChangeThicknessTool
//-----------------------------------------------------------------------------

class VectorChangeThicknessTool final : public DragTool {
  TPointD m_curPos, m_firstPos;
  std::map<int, std::vector<double>> m_strokesThickness;
  double m_thicknessChange;

  tcg::unique_ptr<UndoChangeStrokes> m_undo;

public:
  VectorChangeThicknessTool(VectorSelectionTool *tool);
  ~VectorChangeThicknessTool();

  void setStrokesThickness(TVectorImage &vi);
  void setThicknessChange(double value) { m_thicknessChange = value; }

  void changeImageThickness(TVectorImage &vi, double newThickness);
  void addUndo();

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void draw() override {}
};

}  // namespace DragSelectionTool

//=============================================================================
// VectorSelectionTool
//-----------------------------------------------------------------------------

/*!
  \brief    Selection tool for Toonz's vector images.
*/

class VectorSelectionTool final : public SelectionTool {
  Q_DECLARE_TR_FUNCTIONS(VectorSelectionTool)

public:
  VectorSelectionTool(int targetType);

  void setNewFreeDeformer() override;

  void setCanEnterGroup(bool value) { m_canEnterGroup = value; }

  bool isConstantThickness() const override {
    return m_constantThickness.getValue();
  }
  bool isLevelType() const override;
  bool isSelectedFramesType() const override;
  bool isSameStyleType() const override;
  bool isModifiableSelectionType() const override;

  const std::set<int> &selectedStyles() const {
    return m_levelSelection.styles();
  }
  std::set<int> &selectedStyles() { return m_levelSelection.styles(); }

  int getSelectionCount() const { return m_selectionCount; }

  void selectionOutlineStyle(int &capStyle, int &joinStyle);

  const StrokeSelection &strokeSelection() const { return m_strokeSelection; }

  const LevelSelection &levelSelection() const { return m_levelSelection; }

  TSelection *getSelection() override;
  bool isSelectionEmpty() override;

  void computeBBox() override;

  TPropertyGroup *getProperties(int targetType) override;

protected:
  void onActivate() override;
  void onDeactivate() override;

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override;
  void addContextMenuItems(QMenu *menu) override;

  void draw() override;

  void updateAction(TPointD pos, const TMouseEvent &e) override;
  void onSelectedFramesChanged() override;

  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;

private:
  class AttachedLevelSelection final : public LevelSelection {
    StrokeSelection &m_strokeSelection;  //!< Selection of strokes to be seen at
                                         //! current frame.

  public:
    AttachedLevelSelection(StrokeSelection &strokeSelection)
        : m_strokeSelection(strokeSelection) {}

    void selectNone() override {
      LevelSelection::selectNone(), m_strokeSelection.selectNone();
    }
    void enableCommands() override { m_strokeSelection.enableCommands(); }
  };

private:
  TEnumProperty m_selectionTarget;  //!< Selected target content (strokes, whole
                                    //! image, styles...).
  TBoolProperty m_constantThickness;

  StrokeSelection m_strokeSelection;        //!< Standard selection of a set of
                                            //! strokes in current image.
  AttachedLevelSelection m_levelSelection;  //!< Selection across multiple
                                            //! frames and specific styles.

  TEnumProperty m_capStyle;       //!< Style of stroke caps.
  TEnumProperty m_joinStyle;      //!< Style of stroke joins.
  TIntProperty m_miterJoinLimit;  //!< Stroke joins threshold value.

  TPropertyGroup m_outlineProps;

  int m_selectionCount;  //!< \deprecated  This is \a not related to stroke
                         //! selections;
  //!               but rather to bboxes count - and even that is deprecated.
  bool m_canEnterGroup;  //!< \deprecated  Used in Manga and Kids

private:
  void modifySelectionOnClick(TImageP image, const TPointD &pos,
                              const TMouseEvent &e) override;
  bool selectStroke(int index, bool toggle);

  void doOnActivate() override;
  void doOnDeactivate() override;

  void selectRegionVectorImage();

  void updateTranslation() override;

  /*! \details  The updateSelectionTarget() function reads the selection
          target (styles, frames, etc) selected by the user, and ensures
          that the associated internal selection is configured and
          \a current.

          The selected strokes set is \a may be cleared appropriately
          as a result of a change in target.                                  */

  void updateSelectionTarget();  //!< Reads widget fields and updates the
                                 //! selection target accordingly.
  void
  clearSelectedStrokes();    //!< Clears strokes visible in current selection.
  void finalizeSelection();  //!< Makes selection consistent, and invokes all
                             //! related cached data
  //!  updates needed for visualization.
  void drawInLevelType(const TVectorImage &vi);
  void drawSelectedStrokes(const TVectorImage &vi);
  void drawGroup(const TVectorImage &vi);
};

#endif  // VECTORSELECTIONTOOL_H
