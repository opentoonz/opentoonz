

// TnzCore includes
#include "tundo.h"
#include "tgl.h"

// TnzExt includes
#include "ext/plasticdeformerstorage.h"

// TnzLib includes
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tframehandle.h"

// tcg includes
#include "tcg/tcg_point_ops.h"

// boost includes
#include <boost/noncopyable.hpp>

#include "plastictool.h"

using namespace PlasticToolLocals;

//****************************************************************************************
//    Local namespace  stuff
//****************************************************************************************

namespace {

enum { RIGID_IDX = 0, FLEX_IDX };

}  // namespace

//****************************************************************************************
//    Undo  definitions
//****************************************************************************************

namespace {

class PaintRigidityUndo final : public TUndo {
  TXshCell m_cell;  //!< Affected image (cell == level + frame)
  std::vector<std::map<int, double>> m_vertices;  //!< Affected vertices

  double m_paintValue;  //!< Rigidity value the vertices were
                        //!< painted with
public:
  PaintRigidityUndo(const TXshCell &cell,
                    const std::vector<std::map<int, double>> &vertices,
                    double paintValue)
      : m_cell(cell), m_vertices(vertices), m_paintValue(paintValue) {}

  int getSize() const override { return 1 << 20; }

  void redo() const override {
    TXshSimpleLevel *sl =
        static_cast<TXshSimpleLevel *>(m_cell.m_level.getPointer());
    sl->setDirtyFlag(true);

    TMeshImageP mi(sl->getFrame(m_cell.m_frameId, true));
    if (!mi || mi->meshes().size() != m_vertices.size()) return;

    int m, mCount = int(mi->meshes().size());
    for (m = 0; m != mCount; ++m) {
      TTextureMesh &mesh = *mi->meshes()[m];

      std::map<int, double>::const_iterator vt, vEnd(m_vertices[m].end());
      for (vt = m_vertices[m].begin(); vt != vEnd; ++vt)
        mesh.vertex(vt->first).P().rigidity = m_paintValue;
    }

    PlasticDeformerStorage::instance()->invalidateMeshImage(
        mi.getPointer(), PlasticDeformerStorage::MESH);
  }

  void undo() const override {
    TXshSimpleLevel *sl =
        static_cast<TXshSimpleLevel *>(m_cell.m_level.getPointer());
    sl->setDirtyFlag(true);

    TMeshImageP mi(sl->getFrame(m_cell.m_frameId, true));
    if (!mi || mi->meshes().size() != m_vertices.size()) return;

    int m, mCount = int(mi->meshes().size());
    for (m = 0; m != mCount; ++m) {
      TTextureMesh &mesh = *mi->meshes()[m];

      std::map<int, double>::const_iterator vt, vEnd(m_vertices[m].end());
      for (vt = m_vertices[m].begin(); vt != vEnd; ++vt)
        mesh.vertex(vt->first).P().rigidity = vt->second;
    }

    PlasticDeformerStorage::instance()->invalidateMeshImage(
        mi.getPointer(), PlasticDeformerStorage::MESH);
  }
};

}  // namespace

//****************************************************************************************
//    RigidityPainter  definition
//****************************************************************************************

namespace {

class RigidityPainter final : public tcg::polymorphic {
  std::vector<std::map<int, double>>
      m_oldRigidities;         //!< The original values of painted vertices
  double m_sqRadius, m_value;  //!< Drawing parameters

public:
  RigidityPainter() : m_sqRadius(), m_value() {}

  void startPainting(double radius, int rigidIdx);
  void paint(const TPointD &pos);
  void commit();

private:
  void reset() {
    m_sqRadius = 0.0, m_value = 0.0;
    std::vector<std::map<int, double>>().swap(m_oldRigidities);
  }
};

//------------------------------------------------------------------------

void RigidityPainter::startPainting(double radius, int rigidIdx) {
  m_sqRadius = sq(radius);
  m_value    = (rigidIdx == RIGID_IDX) ? 1e4 : 1.0;

  assert(m_oldRigidities.empty());
}

//------------------------------------------------------------------------

void RigidityPainter::paint(const TPointD &pos) {
  const TXshCell &cell = ::xshCell();

  TXshSimpleLevel *sl =
      dynamic_cast<TXshSimpleLevel *>(cell.m_level.getPointer());
  if (!sl) return;

  TMeshImageP meshImg = TTool::getImage(true);
  if (!meshImg) return;

  // Soil the level - schedules it for save
  sl->setDirtyFlag(true);

  // Paint all mesh vertices inside the circle with center pos and given radius
  const std::vector<TTextureMeshP> &meshes = meshImg->meshes();
  int m, mCount = int(meshImg->meshes().size());

  m_oldRigidities.resize(mCount);

  for (m = 0; m != mCount; ++m) {
    TTextureMesh &mesh = *meshes[m];

    int v, vCount = mesh.verticesCount();
    for (v = 0; v != vCount; ++v) {
      RigidPoint &vxPos = mesh.vertex(v).P();

      if (tcg::point_ops::dist2(pos, (const TPointD &)vxPos) < m_sqRadius) {
        if (!m_oldRigidities[m].count(v))
          m_oldRigidities[m][v] = vxPos.rigidity;

        vxPos.rigidity = m_value;
      }
    }
  }

  PlasticDeformerStorage::instance()->invalidateMeshImage(
      meshImg.getPointer(), PlasticDeformerStorage::MESH);
}

//------------------------------------------------------------------------

void RigidityPainter::commit() {
  TUndoManager::manager()->add(
      new PaintRigidityUndo(::xshCell(), m_oldRigidities, m_value));
  reset();
}

}  // namespace

//****************************************************************************************
//    PlasticTool  functions
//****************************************************************************************

std::unique_ptr<tcg::polymorphic> PlasticTool::createRigidityPainter() {
  return std::unique_ptr<tcg::polymorphic>(new RigidityPainter);
}

//------------------------------------------------------------------------

void PlasticTool::mouseMove_rigidity(const TPointD &pos, const TMouseEvent &e) {
  // Track mouse position
  m_pos = pos;  // Needs to be done now - ensures m_pos is valid

  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown_rigidity(const TPointD &pos,
                                          const TMouseEvent &) {
  // Track mouse position
  m_pressedPos = m_pos = pos;

  RigidityPainter *painter =
      static_cast<RigidityPainter *>(m_rigidityPainter.get());

  painter->startPainting(m_thickness.getValue(), m_rigidValue.getIndex());
  painter->paint(m_pos);

  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDrag_rigidity(const TPointD &pos,
                                          const TMouseEvent &) {
  // Track mouse position
  m_pos = pos;

  RigidityPainter *painter =
      static_cast<RigidityPainter *>(m_rigidityPainter.get());

  painter->paint(m_pos);
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonUp_rigidity(const TPointD &pos,
                                        const TMouseEvent &) {
  // Track mouse position
  m_pos = pos;

  RigidityPainter *painter =
      static_cast<RigidityPainter *>(m_rigidityPainter.get());

  painter->commit();
}

//------------------------------------------------------------------------

void PlasticTool::addContextMenuActions_rigidity(QMenu *menu) {}

//------------------------------------------------------------------------

void PlasticTool::draw_rigidity() {
  if (TTool::getApplication()->getCurrentFrame()->isEditingScene()) {
    // In the rigidity case, we're editing the mesh level - so the implicit
    // transformation affine loaded by OpenGL gets multiplied by the level's
    // dpi scale. We have to revert the scale before showing column-related
    // data.

    const TPointD &dpiScale = TTool::getViewer()->getDpiScale();
    glPushMatrix();
    {
      tglMultMatrix(TScale(1.0 / dpiScale.x, 1.0 / dpiScale.y));

      double pixelSize = sqrt(tglGetPixelSize2());

      // Draw original skeleton
      const PlasticSkeletonP &skeleton = this->skeleton();
      if (skeleton) {
        drawOnionSkinSkeletons_build(pixelSize);
        drawSkeleton(*skeleton, pixelSize);
        drawSelections(m_sd, *skeleton, pixelSize);
      }
    }
    glPopMatrix();
  }

  // Draw a circle centered at m_pos with m_thickness radius
  glColor3f(1.0f, 0.0f, 0.0f);  // Red
  tglDrawCircle(m_pos, m_thickness.getValue());
}
