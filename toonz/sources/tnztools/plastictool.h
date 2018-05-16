#pragma once

#ifndef PLASTICTOOL_H
#define PLASTICTOOL_H

// TnzCore includes
#include "tproperty.h"
#include "tmeshimage.h"

// TnzBase includes
#include "tparamchange.h"
#include "tdoubleparamrelayproperty.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticskeletondeformation.h"
#include "ext/plasticvisualsettings.h"

// TnzQt includes
#include "toonzqt/plasticvertexselection.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/tooloptions.h"

// STD includes
#include <memory>

// tcg includes
#include "tcg/tcg_base.h"
#include "tcg/tcg_controlled_access.h"

// Qt includes
#include <QObject>

//****************************************************************************************
//    Metric defines
//****************************************************************************************

// Skeleton primitives

#define HIGHLIGHT_DISTANCE 8  // Pixels distance to highlight
#define HANDLE_SIZE 4         // Size of vertex handles
#define HIGHLIGHTED_HANDLE_SIZE HIGHLIGHT_DISTANCE  // Size of handle highlights
#define SELECTED_HANDLE_SIZE                                                   \
  HIGHLIGHTED_HANDLE_SIZE  // Size of handle selections

// Mesh primitives

#define MESH_HIGHLIGHT_DISTANCE 8
#define MESH_HIGHLIGHTED_HANDLE_SIZE 4
#define MESH_SELECTED_HANDLE_SIZE 2

//****************************************************************************************
//    PlasticTool  declaration
//****************************************************************************************

class PlasticTool final : public QObject,
                          public TTool,
                          public TParamObserver,
                          public TSelection::View {
  Q_OBJECT

  friend class PlasticToolOptionsBox;

public:
  class TemporaryActivation {
    bool m_activate;

  public:
    TemporaryActivation(int row, int col);
    ~TemporaryActivation();
  };

  struct MeshIndex final : public tcg::safe_bool<MeshIndex> {
    int m_meshIdx,  //!< Mesh index in a TMeshImage
        m_idx;      //!< Index in the referenced mesh

    explicit MeshIndex(int meshIdx = -1, int idx = -1)
        : m_meshIdx(meshIdx), m_idx(idx) {}

    bool operator_bool() const { return (m_meshIdx >= 0) && (m_idx >= 0); }

    bool operator<(const MeshIndex &other) const {
      return (m_meshIdx == other.m_meshIdx) ? (m_idx < other.m_idx)
                                            : (m_meshIdx < other.m_meshIdx);
    }
  };

  typedef MultipleSelection<MeshIndex> MeshSelection;

private:
  PlasticSkeletonDeformationP m_sd;  //!< Current column's skeleton deformation
  int m_skelId;                      //!< Current m_sd's skeleton id
  tcg::invalidable<PlasticSkeleton>
      m_deformedSkeleton;  //!< The interactively-deformed \a animation-mode
                           //! skeleton

  TMeshImageP m_mi;  //!< Current mesh image

  // Property-related vars  (ie, tool options)

  TPropertyGroup
      *m_propGroup;  //!< Tool properties groups (needed for toolbar-building)

  TEnumProperty m_mode;          //!< Editing mode (BUILD, ANIMATE, etc..)
  TStringProperty m_vertexName;  //!< Vertex name property

  TBoolProperty m_interpolate;  //!< Strict vertex interpolation property
  TBoolProperty m_snapToMesh;   //!< Snap to Mesh vertexes during skeleton build

  TDoubleProperty m_thickness;  //!< Brush radius, from 1 to 100
  TEnumProperty
      m_rigidValue;  //!< Rigidity drawing value (ie draw rigidity/flexibility)
                     //!< put a keyframe at current frame

  TBoolProperty
      m_globalKey;  //!< Whether animating a vertex will cause EVERY vertex to
  TBoolProperty
      m_keepDistance;  //!< Whether animation editing can alter vertex distances

  TStringProperty m_minAngle,
      m_maxAngle;  //!< Minimum and maximum angle values allowed

  TPropertyGroup m_relayGroup;  //!< Group for each vertex parameter relay

  TDoubleParamRelayProperty
      m_distanceRelay;  //!< Relay property for vertex distance
  TDoubleParamRelayProperty m_angleRelay;  //!< Relay property for vertex angle
  TDoubleParamRelayProperty m_soRelay;     //!< Relay property for vertex so

  TDoubleParamRelayProperty
      m_skelIdRelay;  //!< Relay property for m_sd's skeleton id

  // Mouse-related vars

  TPointD m_pos;         //!< Last known mouse position
  TPointD m_pressedPos;  //!< Last mouse press position
  bool m_dragged;        //!< Whether dragging occurred between a press/release

  std::vector<TPointD>
      m_pressedVxsPos;   //!< Position of selected vertices at mouse press
  SkDKey m_pressedSkDF;  //!< Skeleton deformation keyframes at mouse press

  // Selection/Highlighting-related vars

  int m_svHigh,                    //!< Highlighted skeleton vertexes
      m_seHigh;                    //!< Highlighted skeleton edges
  PlasticVertexSelection m_svSel;  //!< Selected skeleton vertexes

  MeshIndex m_mvHigh,     //!< Highlighted mesh vertexes
      m_meHigh;           //!< Highlighted mesh edges
  MeshSelection m_mvSel,  //!< Selected mesh vertexes
      m_meSel;            //!< Selected mesh edges

  // Drawing-related vars

  PlasticVisualSettings m_pvs;  //!< Visual options for plastic painting

  // Editing-related vars

  std::unique_ptr<tcg::polymorphic> m_rigidityPainter;  //!< Delegate class to
                                                        //! deal with (undoable)
  //! rigidity painting
  bool m_showSkeletonOS;  //!< Whether onion-skinned skeletons must be shown

  // Deformation-related vars

  bool m_recompileOnMouseRelease;  //!< Whether skeleton recompilation should
                                   //! happen on mouse release

public:
  enum Modes {
    MESH_IDX = 0,
    RIGIDITY_IDX,
    BUILD_IDX,
    ANIMATE_IDX,
    MODES_COUNT
  };

public:
  PlasticTool();
  ~PlasticTool();

  ToolType getToolType() const override;
  int getCursorId() const override { return ToolCursor::SplineEditorCursor; }

  ToolOptionsBox *createOptionsBox() override;

  TPropertyGroup *getProperties(int idx) override { return &m_propGroup[idx]; }

  void updateTranslation() override;

  void onSetViewer() override;

  void onActivate() override;
  void onDeactivate() override;

  void onEnter() override;
  void onLeave() override;

  void addContextMenuItems(QMenu *menu) override;

  void reset() override;

  bool onPropertyChanged(std::string propertyName) override;

public:
  // Methods reimplemented in each interaction mode
  void mouseMove(const TPointD &pos, const TMouseEvent &me) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &me) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &me) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &me) override;

  void draw() override;

public:
  // Skeleton methods

  void setSkeletonSelection(const PlasticVertexSelection &vSel);
  void toggleSkeletonSelection(const PlasticVertexSelection &vSel);
  void clearSkeletonSelections();

  const PlasticVertexSelection &skeletonVertexSelection() const {
    return m_svSel;
  }
  PlasticVertexSelection branchSelection(int vIdx) const;

  void moveVertex_build(const std::vector<TPointD> &originalVxsPos,
                        const TPointD &posShift);
  void addVertex(const PlasticSkeletonVertex &vx);
  void insertVertex(const PlasticSkeletonVertex &vx, int e);
  void insertVertex(const PlasticSkeletonVertex &vx, int parent,
                    const std::vector<int> &children);
  void removeVertex();
  void setVertexName(QString &name);

  int addSkeleton(const PlasticSkeletonP &skeleton);
  void addSkeleton(int skelId, const PlasticSkeletonP &skeleton);
  void removeSkeleton(int skelId);

  PlasticSkeletonP skeleton() const;
  void touchSkeleton();

  PlasticSkeletonDeformationP deformation() const { return m_sd; }
  void touchDeformation();

  void storeDeformation();  //!< Stores deformation of current column (copying
                            //! its reference)
  void storeSkeletonId();  //!< Stores current skeleton id associated to current
                           //! deformation

  void onChange();  //!< Updates the tool after a deformation parameter change.
                    //!< It can be used to refresh the tool in ANIMATION mode.
public:
  // Mesh methods

  const MeshSelection &meshVertexesSelection() const { return m_mvSel; }
  const MeshSelection &meshEdgesSelection() const { return m_meSel; }

  void setMeshVertexesSelection(const MeshSelection &vSel);
  void toggleMeshVertexesSelection(const MeshSelection &vSel);

  void setMeshEdgesSelection(const MeshSelection &eSel);
  void toggleMeshEdgesSelection(const MeshSelection &eSel);

  void clearMeshSelections();

  void storeMeshImage();

  void moveVertex_mesh(const std::vector<TPointD> &originalVxsPos,
                       const TPointD &posShift);

public:
  // Actions with associated undo
  int addSkeleton_undo(const PlasticSkeletonP &skeleton);
  void addSkeleton_undo(int skelId, const PlasticSkeletonP &skeleton);

  void removeSkeleton_undo(int skelId);
  void removeSkeleton_withKeyframes_undo(int skelId);

  void editSkelId_undo(int skelId);

public slots:

  void swapEdge_mesh_undo();
  void collapseEdge_mesh_undo();
  void splitEdge_mesh_undo();
  void cutEdges_mesh_undo();

  void deleteSelectedVertex_undo();

  void setKey_undo();
  void setGlobalKey_undo();
  void setRestKey_undo();
  void setGlobalRestKey_undo();

  void copySkeleton();
  void pasteSkeleton_undo();
  void copyDeformation();
  void pasteDeformation_undo();

signals:  // privates

  void skelIdsListChanged();
  void skelIdChanged();

protected:
  void mouseMove_mesh(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_mesh(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_mesh(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_mesh(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_mesh(QMenu *menu);

  void mouseMove_build(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_build(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_build(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_build(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_build(QMenu *menu);

  void mouseMove_rigidity(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_rigidity(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_rigidity(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_rigidity(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_rigidity(QMenu *menu);

  void mouseMove_animate(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_animate(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_animate(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_animate(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_animate(QMenu *menu);

  void draw_mesh();
  void draw_build();
  void draw_rigidity();
  void draw_animate();

private:
  // Skeleton methods

  PlasticSkeleton &deformedSkeleton();
  void updateDeformedSkeleton(PlasticSkeleton &deformedSkeleton);

  // Keyframe methods

  void keyFunc_undo(void (PlasticTool::*keyFunc)());

  void setKey();
  void setGlobalKey();
  void setRestKey();
  void setGlobalRestKey();

  // Rigidity methods
  static std::unique_ptr<tcg::polymorphic> createRigidityPainter();

  // Drawing methods

  void drawSkeleton(const PlasticSkeleton &skel, double pixelSize,
                    UCHAR alpha = 255);
  void drawOnionSkinSkeletons_build(double pixelSize);
  void drawOnionSkinSkeletons_animate(double pixelSize);

  void drawHighlights(const SkDP &sd, const PlasticSkeleton *skel,
                      double pixelSize);
  void drawSelections(const SkDP &sd, const PlasticSkeleton &skel,
                      double pixelSize);

  void drawAngleLimits(const SkDP &sd, int skeId, int v, double pixelSize);

  // Selection methods

  void setMeshSelection(MeshSelection &target, const MeshSelection &newSel);
  void toggleMeshSelection(MeshSelection &target,
                           const MeshSelection &addedSel);

  void onSelectionChanged() override;
  void enableCommands() override;

  // Parameter Observation methods

  void onChange(const TParamChange &) override;

private slots:

  void onFrameSwitched() override;
  void onColumnSwitched();
  void onXsheetChanged();

  void onShowMeshToggled(bool on);
  void onShowSOToggled(bool on);
  void onShowRigidityToggled(bool on);
  void onShowSkelOSToggled(bool on);
};

//****************************************************************************************
//    PlasticToolOptionsBox  declaration
//****************************************************************************************

class PlasticToolOptionsBox final : public GenericToolOptionsBox,
                                    public TProperty::Listener {
  Q_OBJECT

public:
  PlasticToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                        ToolHandle *toolHandle);

private:
  class SkelIdsComboBox;

private:
  TTool *m_tool;
  GenericToolOptionsBox **m_subToolbars;

  SkelIdsComboBox *m_skelIdComboBox;
  QPushButton *m_addSkelButton, *m_removeSkelButton;

private:
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *he) override;

  void onPropertyChanged() override;

private slots:

  void onSkelIdsListChanged();
  void onSkelIdChanged();
  void onSkelIdEdited();

  void onAddSkeleton();
  void onRemoveSkeleton();
};

//****************************************************************************************
//    PlasticTool  local functions
//****************************************************************************************

namespace PlasticToolLocals {

extern PlasticTool l_plasticTool;        //!< Tool instance.
extern bool l_suspendParamsObservation;  //!< Used to join multiple param change
                                         //! notifications.

//------------------------------------------------------------------------------

// Generic functions

TPointD projection(
    const PlasticSkeleton &skeleton, int e,
    const TPointD &pos);  //!< Projects specified position an a skeleton edge.

// Global getters

double frame();  //!< Returns current global xsheet frame.
int row();       //!< Returns current global xsheet row.

int column();                 //!< Returns current global xsheet column index.
TXshColumn *xshColumn();      //!< Returns current xsheet column object.
TStageObject *stageObject();  //!< Returns current stage object.

const TXshCell &xshCell();  //!< Returns current xsheet cell.
void setCell(
    int row,
    int col);  //!< Moves current xsheet cell to the specified position.

int skeletonId();  //!< Returns current skeleton id.
double sdFrame();  //!< Returns current stage object's <I>parameters time</I>
//!  (ie the frame value to be used with function editor curves,
//!  which takes cyclicity into consideration).

// Keyframe functions

void setKeyframe(
    TDoubleParamP &param,
    double frame);  //!< Sets a keyframe to the specified parameter curve.
void setKeyframe(
    SkVD *vd,
    double frame);  //!< Sets a keyframe to the specified vertex deformation.
void setKeyframe(
    const PlasticSkeletonDeformationP &sd,
    double frame);  //!< Sets a keyframe to an entire skeleton deformation.

void invalidateXsheet();  //!< Refreshes xsheet content.

// Draw functions

void drawSquare(const TPointD &pos,
                double radius);  //!< Draws the outline of a square
void drawFullSquare(const TPointD &pos,
                    double radius);  //!< Draws a filled square

// Mesh functions

std::pair<double, PlasticTool::MeshIndex> closestVertex(const TMeshImage &mi,
                                                        const TPointD &pos);
std::pair<double, PlasticTool::MeshIndex> closestEdge(const TMeshImage &mi,
                                                      const TPointD &pos);

}  // namespace PlasticToolLocals

#endif  // PLASTICTOOL_H
