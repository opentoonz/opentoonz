

// TnzCore includes
#include "tundo.h"

// TnzExt includes
#include "ext/plasticdeformerstorage.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txsheethandle.h"
#include "toonz/stage.h"

// tcg includes
#include "tcg/tcg_point_ops.h"
#include "tcg/tcg_algorithm.h"
#include "tcg/tcg_function_types.h"
#include "tcg/tcg_iterator_ops.h"

#include "plastictool.h"

using namespace PlasticToolLocals;

//****************************************************************************************
//    Local namespace  stuff
//****************************************************************************************

namespace {

TPointD closestMeshVertexPos(const TPointD &pos, double *distance = 0) {
  const TXshCell &imageCell = TTool::getImageCell();

  TXshSimpleLevel *sl = imageCell.getSimpleLevel();
  assert(sl);

  TMeshImageP mi(TTool::getImage(false));
  assert(mi);

  // Retrieve *level* dpi
  // NOTE: This is different than current IMAGE's dpi. An image is actually
  // displayed with
  //       its level owner's dpi, RATHER than its own.

  const TPointD &dpi = sl->getDpi(imageCell.getFrameId());
  assert(dpi.x > 0.0 && dpi.y > 0.0);

  // Cast pos to image coordinates
  const TPointD pos_mesh(pos.x * (dpi.x / Stage::inch),
                         pos.y * (dpi.y / Stage::inch));

  // Retrieve the closest vertex to pos_mesh
  const std::pair<double, PlasticTool::MeshIndex> &closest =
      PlasticToolLocals::closestVertex(*mi, pos_mesh);

  const TPointD &vxPos_mesh =
      mi->meshes()[closest.second.m_meshIdx]->vertex(closest.second.m_idx).P();

  if (distance)
    *distance =
        std::min(Stage::inch / dpi.x, Stage::inch / dpi.y) * closest.first;

  // Cast it back to world coordinates
  return TPointD(vxPos_mesh.x * (Stage::inch / dpi.x),
                 vxPos_mesh.y * (Stage::inch / dpi.y));
}

//------------------------------------------------------------------------

TPointD closestSkeletonVertexPos(const TPointD &pos) {
  struct locals {
    static inline double dist2(const TPointD &pos,
                               const PlasticSkeletonVertex &vx) {
      return tcg::point_ops::dist2(pos, vx.P());
    }
  };

  const PlasticSkeletonP &skeleton = l_plasticTool.skeleton();
  if (!skeleton || skeleton->empty()) return TConsts::napd;

  const PlasticSkeleton::vertices_container &vertices = skeleton->vertices();

  return tcg::min_transform(vertices.begin(), vertices.end(),
                            tcg::bind1st(&locals::dist2, pos))
      ->P();
}

}  // namespace

//****************************************************************************************
//    Undo  definitions
//****************************************************************************************

namespace {

class VertexUndo : public TUndo {
protected:
  int m_row, m_col;  //!< Xsheet coordinates

  int m_v, m_vParent;          //!< Indices of the added vertex and its parent
  PlasticSkeletonVertex m_vx;  //!< Added vertex

  std::vector<int> m_children;  //!< Children of the vertex to insert

public:
  VertexUndo() : m_row(::row()), m_col(::column()), m_v(-1), m_vParent(-1) {}

  int getSize() const override {
    return sizeof(*this);
  }  // sizeof this is roughly ok

  void storeChildren(const PlasticSkeleton &skeleton,
                     const PlasticSkeletonVertex &vx) {
    m_children.clear();

    // Traverse vx's edges and build its children table
    PlasticSkeletonVertex::edges_const_iterator et, eEnd(vx.edgesEnd());
    for (et = vx.edgesBegin(); et != eEnd; ++et) {
      int vChild = skeleton.edge(*et).vertex(1);
      if (vChild == vx.getIndex()) continue;

      m_children.push_back(vChild);
    }
  }

  void addVertex() {
    assert(m_vx.edges().empty());

    // Load the right skeleton by moving to the stored xsheet pos
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const PlasticSkeletonP &skeleton = l_plasticTool.skeleton();
    TCG_ASSERT(skeleton || m_vParent < 0, return );

    // Perform addition
    l_plasticTool.setSkeletonSelection(m_vParent);
    l_plasticTool.addVertex(m_vx);

    // Store data to invert the operation
    assert(l_plasticTool.skeletonVertexSelection().hasSingleObject());
    m_v = l_plasticTool.skeletonVertexSelection();
  }

  void insertVertex() {
    if (m_children.empty()) {
      addVertex();
      return;
    }

    assert(m_vx.edges().empty());

    TCG_ASSERT(m_vParent >= 0, return );

    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const PlasticSkeletonP &skeleton = l_plasticTool.skeleton();
    TCG_ASSERT(skeleton, return );

    // Perform insertion
    l_plasticTool.insertVertex(m_vx, m_vParent, m_children);

    // Store data to invert the operation
    assert(l_plasticTool.skeletonVertexSelection().hasSingleObject());
    m_v = l_plasticTool.skeletonVertexSelection();
  }

  void removeVertex() {
    TCG_ASSERT(m_v >= 0, return );

    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const PlasticSkeletonP &skeleton = l_plasticTool.skeleton();
    TCG_ASSERT(skeleton, return );

    // Store data to invert the operation
    {
      const PlasticSkeletonVertex &vx = skeleton->vertex(m_v);

      m_vParent = vx.parent();
      m_vx      = PlasticSkeletonVertex(vx.P());

      storeChildren(*skeleton, vx);
    }

    // Perform removal
    if (m_v > 0) {
      l_plasticTool.setSkeletonSelection(m_v);
      l_plasticTool.removeVertex();
    } else
      l_plasticTool.removeSkeleton(::skeletonId());
  }
};

//------------------------------------------------------------------------

class AddVertexUndo final : public VertexUndo {
public:
  AddVertexUndo(int vParent, const PlasticSkeletonVertex &vx) {
    m_vParent = vParent, m_vx = vx;
    assert(m_vx.edges().empty());
  }

  void redo() const override {
    const_cast<AddVertexUndo &>(*this).VertexUndo::addVertex();
  }
  void undo() const override {
    const_cast<AddVertexUndo &>(*this).VertexUndo::removeVertex();
  }
};

//------------------------------------------------------------------------

class RemoveVertexUndo final : public VertexUndo {
public:
  RemoveVertexUndo(int v) {
    assert(v >= 0);
    m_v = v;
  }

  void redo() const override {
    const_cast<RemoveVertexUndo &>(*this).VertexUndo::removeVertex();
  }
  void undo() const override {
    const_cast<RemoveVertexUndo &>(*this).VertexUndo::insertVertex();
  }
};

//========================================================================

class InsertVertexUndo final : public VertexUndo {
public:
  InsertVertexUndo(int e, const PlasticSkeletonVertex &vx) {
    const PlasticSkeleton &skeleton      = *l_plasticTool.skeleton();
    const PlasticSkeleton::edge_type &ed = skeleton.edge(e);

    m_vParent = ed.vertex(0), m_vx = vx;
    std::vector<int>(1, ed.vertex(1)).swap(m_children);
  }

  void redo() const override {
    const_cast<InsertVertexUndo &>(*this).VertexUndo::insertVertex();
  }

  void undo() const override {
    TCG_ASSERT(!m_children.empty(), return );

    const_cast<InsertVertexUndo &>(*this).VertexUndo::removeVertex();
  }
};

//========================================================================

class AddSkeletonUndo : public TUndo {
protected:
  int m_row, m_col;  //!< Xsheet coordinates

  int m_skelId;                 //!< The added skeleton's id
  PlasticSkeletonP m_skeleton;  //!< A COPY of the added skeleton

public:
  AddSkeletonUndo(int skelId, const PlasticSkeletonP &skeletonCopy)
      : m_row(::row())
      , m_col(::column())
      , m_skelId(skelId)
      , m_skeleton(skeletonCopy) {}

  // This is not correct... it's tough to build the right one! We're like
  // storing the whole
  // cleared deformation! So, I guess 1 MB (100 of these in the standard undos
  // pool)
  // is a reasonable estimate...
  int getSize() const override { return 1 << 20; }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    l_plasticTool.addSkeleton(m_skelId, new PlasticSkeleton(*m_skeleton));
    ::invalidateXsheet();
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    l_plasticTool.removeSkeleton(m_skelId);
  }
};

//========================================================================

class RemoveSkeletonUndo : public AddSkeletonUndo {
public:
  RemoveSkeletonUndo(int skelId)
      : AddSkeletonUndo(skelId, l_plasticTool.skeleton()) {}

  void redo() const override { AddSkeletonUndo::undo(); }
  void undo() const override { AddSkeletonUndo::redo(); }
};

//------------------------------------------------------------------------

class RemoveSkeletonUndo_WithKeyframes final : public RemoveSkeletonUndo {
  mutable std::vector<TDoubleKeyframe>
      m_skelIdsKeyframes;  //!< Skeleton Ids param curve keyframes
                           //!< for m_skelId
public:
  RemoveSkeletonUndo_WithKeyframes(int skelId) : RemoveSkeletonUndo(skelId) {}

  void redo() const override {
    // Erase all keyframes corresponding to m_skelId from sd's skeleton ids
    // curve
    const SkDP &sd = l_plasticTool.deformation();
    TCG_ASSERT(sd, return );

    const TDoubleParamP &skelIdsParam = sd->skeletonIdsParam();
    if (skelIdsParam->getKeyframeCount() > 0) {
      double frame;

      for (int k = 0; k >= 0; k = skelIdsParam->getNextKeyframe(frame)) {
        const TDoubleKeyframe &kf = skelIdsParam->getKeyframe(k);
        frame                     = kf.m_frame;

        if (m_skelId == (int)kf.m_value) {
          m_skelIdsKeyframes.push_back(kf);
          skelIdsParam->deleteKeyframe(frame);
        }
      }
    }

    RemoveSkeletonUndo::redo();  // Invalidates the xsheet
  }

  void undo() const override {
    l_plasticTool
        .touchDeformation();  // Skeleton removal could have destroyed the sd

    // Restore saved keyframes to sd's skelIdsParam curve
    const SkDP &sd = l_plasticTool.deformation();
    assert(sd);

    const TDoubleParamP &skelIdsParam = sd->skeletonIdsParam();

    std::vector<TDoubleKeyframe>::iterator kt, kEnd(m_skelIdsKeyframes.end());
    for (kt = m_skelIdsKeyframes.begin(); kt != kEnd; ++kt)
      skelIdsParam->setKeyframe(*kt);

    m_skelIdsKeyframes.clear();

    RemoveSkeletonUndo::undo();  // Invalidates the xsheet
  }
};

//========================================================================

class SetSkeletonIdUndo final : public TUndo {
  int m_row, m_col;  //!< Xsheet coordinates

  int m_skelId;  //!< The new skeleton id value
  mutable TDoubleKeyframe
      m_oldKf;  //!< Old keyframe values for skelIds parameter
  mutable bool m_added1stKeyframe;  //!< Whether the redo() added the first
                                    //! skelIds keyframe

public:
  SetSkeletonIdUndo(int skelId)
      : m_row(::row()), m_col(::column()), m_skelId(skelId) {}

  int getSize() const override { return sizeof(*this); }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const SkDP &sd = l_plasticTool.deformation();
    TCG_ASSERT(sd, return );

    const TDoubleParamP &skelIdsParam = sd->skeletonIdsParam();
    double frame                      = ::frame();

    m_oldKf            = skelIdsParam->getKeyframeAt(frame);
    m_added1stKeyframe = false;

    if (frame > 0.0 && (skelIdsParam->getKeyframeCount() == 0 ||
                        skelIdsParam->getKeyframe(0).m_frame >= frame)) {
      // Put a keyframe at the previous cell to preserve values before current
      // frame
      TDoubleKeyframe kf(frame - 1.0, skelIdsParam->getDefaultValue());
      kf.m_type = TDoubleKeyframe::Constant;

      skelIdsParam->setKeyframe(kf);
      m_added1stKeyframe = true;
    }

    TDoubleKeyframe kf(frame, m_skelId);
    kf.m_type = TDoubleKeyframe::Constant;

    skelIdsParam->setKeyframe(kf);

    // No need to invoke PlasticTool::storeSkeletonId() - automatic through
    // onChange()
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    const SkDP &sd = l_plasticTool.deformation();
    TCG_ASSERT(sd, return );

    const TDoubleParamP &skelIdsParam = sd->skeletonIdsParam();

    if (m_oldKf.m_isKeyframe)
      skelIdsParam->setKeyframe(m_oldKf);
    else
      skelIdsParam->deleteKeyframe(m_oldKf.m_frame);

    if (m_added1stKeyframe) {
      const TDoubleKeyframe &kf = skelIdsParam->getKeyframe(0);

      assert(kf.m_value == skelIdsParam->getDefaultValue());
      if (kf.m_value == skelIdsParam->getDefaultValue())
        skelIdsParam->deleteKeyframe(kf.m_frame);
    }
  }
};

//========================================================================

class MoveVertexUndo_Build final : public TUndo {
  int m_row, m_col;  //!< Xsheet coordinates

  std::vector<int> m_vIdxs;           //!< Moved vertices
  std::vector<TPointD> m_origVxsPos;  //!< Original vertex positions
  TPointD m_posShift;                 //!< Vertex positions shift

public:
  MoveVertexUndo_Build(const std::vector<int> &vIdxs,
                       const std::vector<TPointD> &origVxsPos,
                       const TPointD &posShift)
      : m_row(::row())
      , m_col(::column())
      , m_vIdxs(vIdxs)
      , m_origVxsPos(origVxsPos)
      , m_posShift(posShift) {
    assert(m_vIdxs.size() == m_origVxsPos.size());
  }

  int getSize() const override {
    return int(sizeof(*this) +
               m_vIdxs.size() * (sizeof(int) + 2 * sizeof(TPointD)));
  }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    l_plasticTool.setSkeletonSelection(m_vIdxs);
    l_plasticTool.moveVertex_build(m_origVxsPos, m_posShift);

    ::stageObject()
        ->invalidate();  // Should be a TStageObject's implementation detail ...
    l_plasticTool.invalidate();
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    l_plasticTool.setSkeletonSelection(m_vIdxs);
    l_plasticTool.moveVertex_build(m_origVxsPos, TPointD());

    ::stageObject()
        ->invalidate();  // Should be a TStageObject's implementation detail ...
    l_plasticTool.invalidate();
  }
};

}  // namespace

//****************************************************************************************
//    PlasticTool  functions
//****************************************************************************************

void PlasticTool::mouseMove_build(const TPointD &pos, const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;  // Needs to be done now - ensures m_pos is valid

  m_svHigh = m_seHigh = -1;  // Reset highlighted primitives

  double d, highlightRadius = getPixelSize() * HIGHLIGHT_DISTANCE;

  const PlasticSkeletonP &skeleton = this->skeleton();
  if (skeleton) {
    // Search nearest skeleton entities

    // Look for nearest vertex
    int v = skeleton->closestVertex(pos, &d);
    if (v >= 0 && d < highlightRadius)
      m_svHigh = v;
    else {
      // Look for nearest edge
      int e = skeleton->closestEdge(pos, &d);
      if (e >= 0 && d < highlightRadius) m_seHigh = e;
    }
  }

  if (m_svHigh < 0 && m_seHigh < 0) {
    // No highlighted skeleton primitive. Mouse position may be
    // a candidate for vertex addition - snap to mesh if required
    if (m_snapToMesh.getValue()) {
      const TPointD &mvPos = ::closestMeshVertexPos(
          pos, &d);  // No need to check against closest skeleton vertex,
                     // since vertex highlighting kicks in at the same time
      if (d < highlightRadius)  // the snapping would take place.
        m_pos = mvPos;
    }
  }

  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown_build(const TPointD &pos,
                                       const TMouseEvent &me) {
  // Track mouse position
  m_pressedPos = m_pos = pos;

  // Update selections
  {
    const PlasticSkeletonP &skel = skeleton();

    if (m_svHigh >= 0) {
      PlasticVertexSelection vSel(
          me.isShiftPressed()
              ? PlasticVertexSelection(branchSelection(m_svHigh))
              : PlasticVertexSelection(m_svHigh));

      if (me.isCtrlPressed())
        toggleSkeletonSelection(vSel);
      else if (!m_svSel.contains(vSel))
        setSkeletonSelection(vSel);
    } else if (m_seHigh >= 0) {
      // Insert a vertex in the edge
      TUndo *op = new InsertVertexUndo(
          m_seHigh, PlasticSkeletonVertex(projection(*skel, m_seHigh, m_pos)));
      TUndoManager::manager()->add(op);

      op->redo();
    } else if (!skel || skel->empty() || m_svSel.hasSingleObject()) {
      // Snap to mesh if required
      if (m_snapToMesh.getValue()) {
        double d, highlightRadius = getPixelSize() * HIGHLIGHT_DISTANCE;

        const TPointD &mvPos = ::closestMeshVertexPos(
            pos, &d);  // Again, no need to check against closest
                       // skeleton vertex.
        if (d < highlightRadius) m_pos = mvPos;
      }

      // Add a new vertex
      TUndo *op = new AddVertexUndo(m_svSel, PlasticSkeletonVertex(m_pos));
      TUndoManager::manager()->add(op);

      op->redo();

      assert(skeleton());
    } else
      setSkeletonSelection(-1);
  }

  // Start move vertex operation
  if (!m_svSel.isEmpty()) {
    struct locals {
      static TPointD vertexPos(const PlasticSkeleton &skel, int v) {
        return skel.vertex(v).P();
      }
    };

    const PlasticSkeletonP &skel = skeleton();
    assert(skel);

    // Adjust mouse press to the selected skeleton vertex, if necessary
    if (m_svSel.hasSingleObject()) m_pressedPos = skel->vertex(m_svSel).P();

    // Store original vertex positions
    m_pressedVxsPos = std::vector<TPointD>(
        tcg::make_cast_it(m_svSel.objects().begin(),
                          tcg::bind1st(&locals::vertexPos, *skel)),
        tcg::make_cast_it(m_svSel.objects().end(),
                          tcg::bind1st(&locals::vertexPos, *skel)));
  }

  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDrag_build(const TPointD &pos,
                                       const TMouseEvent &me) {
  // Track mouse position
  if (m_snapToMesh.getValue()) {
    const TPointD &mvPos = ::closestMeshVertexPos(pos),
                  &svPos = ::closestSkeletonVertexPos(mvPos);

    if (tcg::point_ops::dist(mvPos, svPos) >
        getPixelSize())  // 1) If said distance is sub-pixel, the user
      m_pos = mvPos;  //    just cannot see the assignment - so it's not safe.
  }                   // 2) The moveVertex_build() below manipulates m_pos
  else                //    (not how m_pressedPos is subtracted), so !=
    m_pos = pos;      //    is not a choice.

  moveVertex_build(m_pressedVxsPos, m_pos - m_pressedPos);
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonUp_build(const TPointD &pos,
                                     const TMouseEvent &me) {
  // Track mouse position
  if (m_snapToMesh.getValue()) {
    const TPointD &mvPos = ::closestMeshVertexPos(pos),
                  &svPos = ::closestSkeletonVertexPos(mvPos);

    if (tcg::point_ops::dist(mvPos, svPos) > getPixelSize())  // Same as above
      m_pos = mvPos;
  } else
    m_pos = pos;

  if (!m_svSel.isEmpty() && m_dragged) {
    TUndoManager::manager()->add(new MoveVertexUndo_Build(
        m_svSel.objects(), m_pressedVxsPos, m_pos - m_pressedPos));

    ::stageObject()
        ->invalidate();  // Should be a TStageObject's implementation detail ...
    invalidate();        // .. it's that it caches placement data and we must
  }                      // invalidate it. Gross. Can't we do anything about it?
}

//------------------------------------------------------------------------

void PlasticTool::addContextMenuActions_build(QMenu *menu) {
  bool ret = true;

  if (!m_svSel.isEmpty()) {
    QAction *deleteVertex = menu->addAction(tr("Delete Vertex"));
    ret = ret && connect(deleteVertex, SIGNAL(triggered()), &l_plasticTool,
                         SLOT(deleteSelectedVertex_undo()));

    menu->addSeparator();
  }

  assert(ret);
}

//------------------------------------------------------------------------

void PlasticTool::moveVertex_build(const std::vector<TPointD> &origVxsPos,
                                   const TPointD &posShift) {
  if (!m_svSel.isEmpty()) {
    const PlasticSkeletonP &skeleton = this->skeleton();

    // Move selected vertices
    int v, vCount = int(m_svSel.objects().size());
    for (v = 0; v != vCount; ++v)
      skeleton->moveVertex(m_svSel.objects()[v], origVxsPos[v] + posShift);

    // Deformation must be recompiled
    PlasticDeformerStorage::instance()->invalidateSkeleton(
        m_sd.getPointer(), ::skeletonId(), PlasticDeformerStorage::ALL);

    if (m_mode.getIndex() == ANIMATE_IDX)
      storeDeformation();  // Rebuild deformed skeleton - default values have
                           // changed
  }
}

//------------------------------------------------------------------------

void PlasticTool::addVertex(const PlasticSkeletonVertex &vx) {
  touchSkeleton();
  const PlasticSkeletonP &skeleton = this->skeleton();

  l_suspendParamsObservation =
      true;  // Some vertex parameters change during insert

  assert(
      m_svSel.isEmpty() ||
      m_svSel
          .hasSingleObject());  // Could there be no parent (inserting the root)
  setSkeletonSelection(skeleton->addVertex(vx, m_svSel));

  l_suspendParamsObservation = false;
  onChange();  // Update once after add

  // NOTE: Root addition does NOT currently add channels, so the above
  // onChange() is
  // quite necessary to cover that case too!

  // Xsheet change notification is necessary to inform the Function Editor that
  // new
  // channels (the vertex deformation ones) have been introduced
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  PlasticDeformerStorage::instance()->invalidateSkeleton(
      m_sd.getPointer(), ::skeletonId(), PlasticDeformerStorage::ALL);
}

//------------------------------------------------------------------------

void PlasticTool::insertVertex(const PlasticSkeletonVertex &vx, int parent,
                               const std::vector<int> &children) {
  const PlasticSkeletonP &skeleton = this->skeleton();
  assert(skeleton);

  l_suspendParamsObservation =
      true;  // Some vertex parameters change during insert.

  setSkeletonSelection(skeleton->insertVertex(vx, parent, children));

  l_suspendParamsObservation = false;
  onChange();  // Update once after insertion

  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  PlasticDeformerStorage::instance()->invalidateSkeleton(
      m_sd.getPointer(), ::skeletonId(), PlasticDeformerStorage::ALL);
}

//------------------------------------------------------------------------

void PlasticTool::insertVertex(const PlasticSkeletonVertex &vx, int e) {
  const PlasticSkeletonP &skeleton = this->skeleton();
  assert(skeleton);

  const PlasticSkeleton::edge_type &ed = skeleton->edge(e);
  insertVertex(vx, ed.vertex(0),
               std::vector<int>(1, skeleton->edge(e).vertex(1)));
}

//------------------------------------------------------------------------

void PlasticTool::removeVertex() {
  const PlasticSkeletonP &skeleton = this->skeleton();
  assert(skeleton && m_svSel.hasSingleObject() && m_svSel > 0);

  l_suspendParamsObservation =
      true;  // Some vertex parameters change during removal.
             // We need to avoid updating WHILE removing.
  skeleton->removeVertex(m_svSel);
  PlasticDeformerStorage::instance()->invalidateSkeleton(
      m_sd.getPointer(), ::skeletonId(), PlasticDeformerStorage::ALL);

  l_suspendParamsObservation = false;
  onChange();

  clearSkeletonSelections();  // Remove mesh references - could be
                              // invalidated...

  // Xsheet change notification is necessary to inform the Function Editor that
  // some
  // channels (the vertex deformation ones) have been removed
  TTool::getApplication()
      ->getCurrentXsheet()
      ->notifyXsheetChanged();  // NOTE: This COULD invoke invalidate()...

  // Rebuild the stage object's keyframes table
  stageObject()->updateKeyframes();
}

//------------------------------------------------------------------------

int PlasticTool::addSkeleton(const PlasticSkeletonP &skeleton) {
  assert(TTool::isEnabled());

  touchDeformation();

  int skelId;
  {
    if (m_sd->empty())
      skelId = 1;
    else {
      // Get the first unused skeleton id in m_sd
      SkD::skelId_iterator st, sEnd;
      m_sd->skeletonIds(st, sEnd);

      for (skelId = 1; st != sEnd && skelId == *st; ++skelId, ++st)
        ;
    }
  }

  addSkeleton(skelId, skeleton);
  return skelId;
}

//------------------------------------------------------------------------

void PlasticTool::addSkeleton(int skelId, const PlasticSkeletonP &skeleton) {
  assert(TTool::isEnabled());

  touchDeformation();
  m_sd->attach(skelId, skeleton.getPointer());

  emit skelIdsListChanged();
}

//------------------------------------------------------------------------

void PlasticTool::removeSkeleton(int skelId) {
  // Remove the entire deformation
  clearSkeletonSelections();

  if (m_sd) {
    // in order to solve the crash issue #1967, try releasing deformer data here
    PlasticDeformerStorage::instance()->releaseSkeletonData(
        stageObject()->getPlasticSkeletonDeformation().getPointer(), skelId);
    m_sd->detach(skelId);
    if (m_sd->empty())
      stageObject()->setPlasticSkeletonDeformation(
          PlasticSkeletonDeformationP());

    ::invalidateXsheet();  // Updates m_sd
    emit skelIdsListChanged();
  }
}

//------------------------------------------------------------------------

void PlasticTool::deleteSelectedVertex_undo() {
  if (m_svSel.isEmpty()) return;

  TUndoManager *manager = TUndoManager::manager();

  if (m_svSel.contains(0)) {
    TUndo *op = new RemoveSkeletonUndo(::skeletonId());
    manager->add(op);

    op->redo();
  } else {
    typedef PlasticVertexSelection::objects_container objects_container;

    objects_container vertexIdxs =
        m_svSel.objects();  // Each undo will reset the vertex selection,
                            // so we need a copy
    manager->beginBlock();
    {
      objects_container::const_iterator vit, viEnd = vertexIdxs.end();
      for (vit = vertexIdxs.begin(); vit != viEnd; ++vit) {
        TUndo *op = new RemoveVertexUndo(*vit);
        manager->add(op);

        op->redo();
      }
    }
    manager->endBlock();
  }
}

//------------------------------------------------------------------------

int PlasticTool::addSkeleton_undo(const PlasticSkeletonP &skeleton) {
  int skelId;

  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();
  {
    skelId = l_plasticTool.addSkeleton(skeleton);
    assert(l_plasticTool.deformation());

    TUndo *addUndo =
        new AddSkeletonUndo(skelId, new PlasticSkeleton(*skeleton));
    manager->add(addUndo);

    TUndo *setIdUndo = new SetSkeletonIdUndo(skelId);
    manager->add(setIdUndo);

    setIdUndo->redo();
  }
  manager->endBlock();

  ::invalidateXsheet();
  return skelId;
}

//------------------------------------------------------------------------

void PlasticTool::addSkeleton_undo(int skelId,
                                   const PlasticSkeletonP &skeleton) {
  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();
  {
    l_plasticTool.addSkeleton(skelId, skeleton);
    assert(l_plasticTool.deformation());

    TUndo *addUndo =
        new AddSkeletonUndo(skelId, new PlasticSkeleton(*skeleton));
    manager->add(addUndo);

    TUndo *setIdUndo = new SetSkeletonIdUndo(skelId);
    manager->add(setIdUndo);

    setIdUndo->redo();
  }
  manager->endBlock();

  ::invalidateXsheet();
}

//------------------------------------------------------------------------

void PlasticTool::removeSkeleton_undo(int skelId) {
  TUndo *op = new RemoveSkeletonUndo(skelId);
  TUndoManager::manager()->add(op);

  op->redo();
}

//------------------------------------------------------------------------

void PlasticTool::removeSkeleton_withKeyframes_undo(int skelId) {
  TUndo *op = new RemoveSkeletonUndo_WithKeyframes(skelId);
  TUndoManager::manager()->add(op);

  op->redo();
}

//------------------------------------------------------------------------

void PlasticTool::editSkelId_undo(int skelId) {
  TUndo *op = new SetSkeletonIdUndo(skelId);
  TUndoManager::manager()->add(op);

  op->redo();
}

//------------------------------------------------------------------------

void PlasticTool::draw_build() {
  double pixelSize = getPixelSize();

  // Draw original skeleton
  const PlasticSkeletonP &skeleton = this->skeleton();
  if (skeleton) {
    drawOnionSkinSkeletons_build(pixelSize);
    drawSkeleton(*skeleton, pixelSize);
    drawSelections(m_sd, *skeleton, pixelSize);
  }

  drawHighlights(m_sd, skeleton.getPointer(), pixelSize);

  if (!skeleton || skeleton->vertices().empty() ||
      (m_svSel.hasSingleObject() && m_svHigh < 0 && m_seHigh < 0)) {
    // Draw a handle at current mouse pos (will add a vertex)
    drawSquare(m_pos, HANDLE_SIZE * pixelSize);
  }
}
