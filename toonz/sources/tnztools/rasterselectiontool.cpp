

#include "rasterselectiontool.h"
#include "vectorselectiontool.h"
#include "drawutil.h"
#include "tenv.h"
#include "tools/toolhandle.h"
#include "toonz/tdistort.h"
#include "toonz/glrasterpainter.h"
#include "toonz/toonzimageutils.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/imageutils.h"

using namespace ToolUtils;
using namespace DragSelectionTool;

//=============================================================================
// RasterFreeDeformer
//-----------------------------------------------------------------------------

RasterFreeDeformer::RasterFreeDeformer(TRasterP ras)
    : FreeDeformer(), m_ras(ras), m_newRas(), m_noAntialiasing(false) {
  TRect r       = ras->getBounds();
  m_originalP00 = convert(r.getP00());
  m_originalP11 = convert(r.getP11());
  m_newPoints.push_back(m_originalP00);
  m_newPoints.push_back(convert(r.getP10()));
  m_newPoints.push_back(m_originalP11);
  m_newPoints.push_back(convert(r.getP01()));
}

//-----------------------------------------------------------------------------

RasterFreeDeformer::~RasterFreeDeformer() {}

//-----------------------------------------------------------------------------

void RasterFreeDeformer::setPoint(int index, const TPointD &p) {
  m_newPoints[index] = p;
}

//-----------------------------------------------------------------------------

void RasterFreeDeformer::setPoints(const TPointD &p0, const TPointD &p1,
                                   const TPointD &p2, const TPointD &p3) {
  m_newPoints[0] = p0;
  m_newPoints[1] = p1;
  m_newPoints[2] = p2;
  m_newPoints[3] = p3;
}

//-----------------------------------------------------------------------------

void RasterFreeDeformer::deformImage() {
  TPointD p00 = TPointD();
  TPointD p10 = m_newPoints[1] - m_newPoints[0];
  TPointD p11 = m_newPoints[2] - m_newPoints[0];
  TPointD p01 = m_newPoints[3] - m_newPoints[0];

  double x0 = std::min({p00.x, p10.x, p11.x, p01.x});
  double y0 = std::min({p00.y, p10.y, p11.y, p01.y});
  double x1 = std::max({p00.x, p10.x, p11.x, p01.x});
  double y1 = std::max({p00.y, p10.y, p11.y, p01.y});

  TRectD sourceRect(TPointD(), TPointD(m_ras->getLx(), m_ras->getLy()));
  BilinearDistorterBase dist(sourceRect.getP00(), sourceRect.getP10(),
                             sourceRect.getP01(), sourceRect.getP11(), p00, p10,
                             p01, p11);

  TRect destRect(tfloor(x0), tfloor(y0), tceil(x1) - 1, tceil(y1) - 1);
  if (TRasterCM32P ras = (TRasterCM32P)m_ras)
    m_newRas = TRasterCM32P(destRect.getLx(), destRect.getLy());
  else if (TRaster32P ras = (TRaster32P)m_ras)
    m_newRas = TRaster32P(destRect.getLx(), destRect.getLy());
  TRasterP newRas(m_newRas);  // Someway, conversion from TRasterCM32P to
                              // TRasterP is not automatic
  distort(newRas, m_ras, dist, destRect.getP00(),
          m_noAntialiasing ? TRop::ClosestPixel : TRop::Bilinear);
}

//=============================================================================
// UndoRasterDeform
//-----------------------------------------------------------------------------

DragSelectionTool::UndoRasterDeform::UndoRasterDeform(RasterSelectionTool *tool)
    : TUndo()
    , m_tool(tool)
    , m_oldBBox(tool->getBBox())
    , m_newBBox()
    , m_oldCenter(tool->getCenter())
    , m_newCenter()
    , m_dim() {
  RasterSelection *selection = (RasterSelection *)tool->getSelection();
  m_oldStrokes               = selection->getStrokes();
  m_oldFloatingImageId =
      "UndoRasterDeform_old_floating_" + std::to_string(m_id++);
  TRasterP floatingRas = selection->getFloatingSelection();
  TImageP floatingImage;
  if (TRasterCM32P toonzRas = (TRasterCM32P)(floatingRas)) {
    floatingImage = TToonzImageP(toonzRas, toonzRas->getBounds());
    m_dim         = toonzRas->getSize();
    m_pixelSize   = toonzRas->getPixelSize();
  }
  if (TRaster32P fullColorRas = (TRaster32P)(floatingRas)) {
    floatingImage = TRasterImageP(fullColorRas);
    m_dim         = fullColorRas->getSize();
    m_pixelSize   = fullColorRas->getPixelSize();
  }
  if (TRasterGR8P grRas = (TRasterGR8P)(floatingRas)) {
    floatingImage = TRasterImageP(grRas);
    m_dim         = grRas->getSize();
    m_pixelSize   = grRas->getPixelSize();
  }
  TImageCache::instance()->add(m_oldFloatingImageId, floatingImage, false);
}

//-----------------------------------------------------------------------------

DragSelectionTool::UndoRasterDeform::~UndoRasterDeform() {
  if (TImageCache::instance()->isCached(m_oldFloatingImageId))
    TImageCache::instance()->remove(m_oldFloatingImageId);
  if (TImageCache::instance()->isCached(m_newFloatingImageId))
    TImageCache::instance()->remove(m_newFloatingImageId);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoRasterDeform::registerRasterDeformation() {
  RasterSelection *selection = (RasterSelection *)m_tool->getSelection();
  m_newStrokes               = selection->getStrokes();
  m_newFloatingImageId =
      "UndoRasterDeform_new_floating_" + std::to_string(m_id);
  TRasterP floatingRas = selection->getFloatingSelection();
  TImageP floatingImage;
  if (TRasterCM32P toonzRas = (TRasterCM32P)(floatingRas))
    floatingImage = TToonzImageP(toonzRas, toonzRas->getBounds());
  if (TRaster32P fullColorRas = (TRaster32P)(floatingRas))
    floatingImage = TRasterImageP(fullColorRas);
  if (TRasterGR8P grRas = (TRasterGR8P)(floatingRas))
    floatingImage = TRasterImageP(grRas);
  TImageCache::instance()->add(m_newFloatingImageId, floatingImage, false);
  m_newBBox   = m_tool->getBBox();
  m_newCenter = m_tool->getCenter();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoRasterDeform::undo() const {
  RasterSelection *selection = (RasterSelection *)m_tool->getSelection();
  if (!selection->isFloating()) return;
  TImageP img = TImageCache::instance()->get(m_oldFloatingImageId, false);
  TRasterP ras;
  if (TToonzImageP ti = (TToonzImageP)(img)) ras = ti->getRaster();
  if (TRasterImageP ri = (TRasterImageP)(img)) ras = ri->getRaster();
  selection->setFloatingSeletion(ras);
  selection->setStrokes(m_oldStrokes);
  m_tool->setBBox(m_oldBBox);
  m_tool->setCenter(m_oldCenter);
  m_tool->invalidate();
  m_tool->decreaseTransformationCount();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoRasterDeform::redo() const {
  RasterSelection *selection = (RasterSelection *)m_tool->getSelection();
  if (!selection->isFloating()) return;
  TToonzImageP img = TImageCache::instance()->get(m_newFloatingImageId, false);
  TRasterP ras;
  if (TToonzImageP ti = (TToonzImageP)(img)) ras = ti->getRaster();
  if (TRasterImageP ri = (TRasterImageP)(img)) ras = ri->getRaster();
  selection->setStrokes(m_newStrokes);
  m_tool->setBBox(m_newBBox);
  m_tool->setCenter(m_newCenter);
  m_tool->invalidate();
  m_tool->increaseTransformationCount();
}

//-----------------------------------------------------------------------------

int DragSelectionTool::UndoRasterDeform::getSize() const {
  return sizeof(*this) + (m_dim.lx * m_dim.ly * m_pixelSize);
}

//-----------------------------------------------------------------------------

int DragSelectionTool::UndoRasterDeform::m_id = 0;

//=============================================================================
// UndoRasterTransform
//-----------------------------------------------------------------------------

DragSelectionTool::UndoRasterTransform::UndoRasterTransform(
    RasterSelectionTool *tool)
    : m_tool(tool) {
  m_oldDeformValues = m_tool->m_deformValues;
  RasterSelection *selection =
      dynamic_cast<RasterSelection *>(tool->getSelection());
  m_oldTransform = selection->getTransformation();
  m_oldCenter    = tool->getCenter();
  m_oldBbox      = tool->getBBox();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoRasterTransform::setChangedValues() {
  m_newDeformValues = m_tool->m_deformValues;
  RasterSelection *selection =
      dynamic_cast<RasterSelection *>(m_tool->getSelection());
  m_newTransform = selection->getTransformation();
  m_newCenter    = m_tool->getCenter();
  m_newBbox      = m_tool->getBBox();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoRasterTransform::undo() const {
  m_tool->transformFloatingSelection(m_oldTransform, m_oldCenter, m_oldBbox);
  m_tool->m_deformValues = m_oldDeformValues;
  m_tool->decreaseTransformationCount();
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoRasterTransform::redo() const {
  m_tool->transformFloatingSelection(m_newTransform, m_newCenter, m_newBbox);
  m_tool->m_deformValues = m_newDeformValues;
  m_tool->increaseTransformationCount();
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//=============================================================================
// RasterDeformTool
//-----------------------------------------------------------------------------

DragSelectionTool::RasterDeformTool::RasterDeformTool(RasterSelectionTool *tool,
                                                      bool freeDeformer)
    : DeformTool(tool)
    , m_transformUndo(0)
    , m_deformUndo(0)
    , m_isFreeDeformer(freeDeformer) {
  if (!m_isFreeDeformer) m_transformUndo = new UndoRasterTransform(tool);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterDeformTool::applyTransform(FourPoints bbox) {
  RasterSelectionTool *tool = (RasterSelectionTool *)getTool();
  tool->setNewFreeDeformer();
  if (!m_deformUndo) m_deformUndo = new UndoRasterDeform(tool);
  RasterSelection *selection =
      dynamic_cast<RasterSelection *>(tool->getSelection());
  assert(selection);
  FourPoints realBbox = bbox * selection->getTransformation().inv();
  RasterFreeDeformer *freeDeformer =
      (RasterFreeDeformer *)tool->getFreeDeformer();
  if (!freeDeformer) return;
  freeDeformer->setNoAntialiasing(tool->getNoAntialiasingValue());
  freeDeformer->setPoints(realBbox.getP00(), realBbox.getP10(),
                          realBbox.getP11(), realBbox.getP01());
  freeDeformer->deformImage();
  selection->setFloatingSeletion(freeDeformer->getImage());
  VectorFreeDeformer *vectorFreeDeformer = tool->getSelectionFreeDeformer();
  if (vectorFreeDeformer) {
    vectorFreeDeformer->setPoints(realBbox.getP00(), realBbox.getP10(),
                                  realBbox.getP11(), realBbox.getP01());
    vectorFreeDeformer->deformImage();
    TVectorImage *vi = vectorFreeDeformer->getDeformedImage();
    std::vector<TStroke> newStrokes;
    int i;
    for (i = 0; i < (int)vi->getStrokeCount(); i++)
      newStrokes.push_back(*(vi->getStroke(i)));
    selection->setStrokes(newStrokes);
  }
  tool->m_deformValues.m_isSelectionModified = true;
  if (!m_isDragging) tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterDeformTool::applyTransform(TAffine aff,
                                                         bool modifyCenter) {
  m_transform *= aff;
  RasterSelectionTool *tool = dynamic_cast<RasterSelectionTool *>(getTool());
  RasterSelection *rasterSelection =
      dynamic_cast<RasterSelection *>(getTool()->getSelection());
  rasterSelection->transform(aff);
  tool->setBBox(tool->getBBox() * aff);
  if (modifyCenter) tool->setCenter(aff * tool->getCenter());
  if (!m_isDragging && !rasterSelection->isFloating())
    rasterSelection->makeFloating();
  else if (!m_isDragging)
    tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterDeformTool::addTransformUndo() {
  RasterSelection *rasterSelection =
      dynamic_cast<RasterSelection *>(getTool()->getSelection());
  if (!rasterSelection || !rasterSelection->isFloating()) return;
  RasterSelectionTool *tool = dynamic_cast<RasterSelectionTool *>(getTool());
  assert(tool);
  if (!m_isFreeDeformer) {
    if (!m_transformUndo) return;
    m_transformUndo->setChangedValues();
    m_transform = TAffine();
    TUndoManager::manager()->add(m_transformUndo);
  } else {
    if (!m_deformUndo) return;
    m_deformUndo->registerRasterDeformation();
    TUndoManager::manager()->add(m_deformUndo);
  }
  tool->increaseTransformationCount();
}

//=============================================================================
// RasterRotationTool
//-----------------------------------------------------------------------------

DragSelectionTool::RasterRotationTool::RasterRotationTool(
    RasterSelectionTool *tool)
    : RasterDeformTool(tool, false) {
  m_rotation = new Rotation(this);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterRotationTool::transform(TAffine aff,
                                                      double angle) {
  applyTransform(aff, false);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterRotationTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  m_rotation->leftButtonDrag(pos, e);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterRotationTool::draw() { m_rotation->draw(); }

//=============================================================================
// RasterFreeDeformTool
//-----------------------------------------------------------------------------

DragSelectionTool::RasterFreeDeformTool::RasterFreeDeformTool(
    RasterSelectionTool *tool)
    : RasterDeformTool(tool, true) {
  m_freeDeform = new FreeDeform(this);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterFreeDeformTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  m_freeDeform->leftButtonDrag(pos, e);
}

//=============================================================================
// RasterMoveSelectionTool
//-----------------------------------------------------------------------------

DragSelectionTool::RasterMoveSelectionTool::RasterMoveSelectionTool(
    RasterSelectionTool *tool)
    : RasterDeformTool(tool, false) {
  m_moveSelection = new MoveSelection(this);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterMoveSelectionTool::transform(TAffine aff) {
  applyTransform(aff, true);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterMoveSelectionTool::leftButtonDown(
    const TPointD &pos, const TMouseEvent &e) {
  m_moveSelection->leftButtonDown(pos, e);
  RasterDeformTool::leftButtonDown(pos, e);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterMoveSelectionTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  m_moveSelection->leftButtonDrag(pos, e);
}

//=============================================================================
// RasterScaleTool
//-----------------------------------------------------------------------------

DragSelectionTool::RasterScaleTool::RasterScaleTool(RasterSelectionTool *tool,
                                                    int type)
    : RasterDeformTool(tool, true) {
  m_scale = new Scale(this, type);
}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::RasterScaleTool::transform(int index,
                                                      TPointD newPos) {
  SelectionTool *tool = getTool();
  TPointD scaleValue  = tool->m_deformValues.m_scaleValue;

  std::vector<FourPoints> startBboxs = m_scale->getStartBboxs();
  FourPoints bbox =
      m_scale->bboxScaleInCenter(index, startBboxs[0], newPos, scaleValue,
                                 m_scale->getStartCenter(), true);
  if (bbox == startBboxs[0]) return scaleValue;

  // Se non ho scalato rispetto al centro calcolo la posizione del nuovo centro
  if (!m_scale->scaleInCenter())
    tool->setCenter(m_scale->getNewCenter(index, startBboxs[0], scaleValue));

  applyTransform(bbox);

  tool->setBBox(bbox);

  return scaleValue;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterScaleTool::leftButtonDown(const TPointD &pos,
                                                        const TMouseEvent &e) {
  m_scale->leftButtonDown(pos, e);
  RasterDeformTool::leftButtonDown(pos, e);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::RasterScaleTool::leftButtonDrag(const TPointD &pos,
                                                        const TMouseEvent &e) {
  m_scale->leftButtonDrag(pos, e);
}

TEnv::IntVar ModifySavebox("ModifySavebox", 0);
TEnv::IntVar NoAntialiasing("NoAntialiasing", 0);

//=============================================================================
// RasterSelectionTool
//-----------------------------------------------------------------------------

RasterSelectionTool::RasterSelectionTool(int targetType)
    : SelectionTool(targetType)
    , m_transformationCount(0)
    , m_selectionFreeDeformer(0)
    , m_noAntialiasing("No Antialiasing", false)
    , m_modifySavebox("Modify Savebox", false)
    , m_setSaveboxTool(0) {
  m_prop.bind(m_noAntialiasing);
  m_rasterSelection.setView(this);
  if (m_targetType & ToonzImage) {
    m_setSaveboxTool = new SetSaveboxTool(this);
    m_modifySavebox.setId("ModifySavebox");
  }
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::setBBox(const DragSelectionTool::FourPoints &points,
                                  int index) {
  if (m_bboxs.empty()) return;
  assert((int)m_bboxs.size() > index);
  m_bboxs[index]                  = points;
  TAffine aff                     = m_rasterSelection.getTransformation();
  DragSelectionTool::FourPoints p = points * aff.inv();
  m_rasterSelection.setSelectionBbox(p.getBox());
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::setNewFreeDeformer() {
  if (!m_freeDeformers.empty() || isSelectionEmpty()) return;

  TImageP image = (TImageP)getImage(true);

  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;

  if (!isFloating()) m_rasterSelection.makeFloating();
  m_freeDeformers.push_back(
      new RasterFreeDeformer(m_rasterSelection.getFloatingSelection()));
  std::vector<TStroke> strokes = m_rasterSelection.getOriginalStrokes();
  if (!strokes.empty()) {
    TVectorImage *vi = new TVectorImage();
    std::set<int> indices;
    // Devo deformare anche gli strokes della selezione!!!
    int i;
    for (i = 0; i < (int)strokes.size(); i++) {
      vi->addStroke(new TStroke(strokes[i]));
      indices.insert(i);
    }
    m_selectionFreeDeformer = new VectorFreeDeformer(vi, indices);
    m_selectionFreeDeformer->setPreserveThickness(true);
  }
}

//-----------------------------------------------------------------------------

VectorFreeDeformer *RasterSelectionTool::getSelectionFreeDeformer() const {
  return m_selectionFreeDeformer;
}

//-----------------------------------------------------------------------------

bool RasterSelectionTool::isFloating() const {
  return m_rasterSelection.isFloating();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::modifySelectionOnClick(TImageP image,
                                                 const TPointD &pos,
                                                 const TMouseEvent &e) {
  const TXshCell &imageCell = TTool::getImageCell();

  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;
  m_rasterSelection.makeCurrent();
  updateAction(pos, e);

  m_firstPos = m_curPos = pos;
  if (!m_rasterSelection.isEmpty() && !m_rasterSelection.isFloating() &&
      e.isShiftPressed() && !m_rasterSelection.isTransformed()) {
    m_selectingRect.empty();
    m_transformationCount = 0;
    m_selecting           = true;
  } else if (!m_rasterSelection.isEmpty()) {
    m_selectingRect.empty();
    m_selecting = false;
    if (m_what == Outside && m_rasterSelection.isFloating()) {
      m_rasterSelection.pasteFloatingSelection();
    } else if (m_what == Outside && !m_rasterSelection.isFloating()) {
      m_rasterSelection.setCurrentImage(image, imageCell);
      m_rasterSelection.selectNone();
      m_bboxs.clear();
      m_selectingRect.empty();
      m_selecting = true;
    } else {
      if (!m_rasterSelection.isFloating() &&
          (m_what == Inside || m_what == ROTATION || m_what == SCALE ||
           m_what == SCALE_X || m_what == SCALE_Y)) {
        m_rasterSelection.makeFloating();
        m_transformationCount = 0;
        m_rasterSelection.setTransformationCount(0);
      }
    }
  } else {
    if (m_what == Outside) {
      m_rasterSelection.setCurrentImage(image, imageCell);
      m_rasterSelection.selectNone();
      m_bboxs.clear();
      m_selectingRect.empty();
      m_selecting = true;
    }
  }
  invalidate();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::leftButtonDown(const TPointD &pos,
                                         const TMouseEvent &e) {
  if (m_setSaveboxTool && m_modifySavebox.getValue()) {
    m_setSaveboxTool->leftButtonDown(pos);
    return;
  }
  SelectionTool::leftButtonDown(pos, e);
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  if (m_setSaveboxTool && m_modifySavebox.getValue()) {
    if (!m_leftButtonMousePressed)
      m_cursorId = m_setSaveboxTool->getCursorId(pos);
    return;
  }
  SelectionTool::mouseMove(pos, e);
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::leftButtonDrag(const TPointD &pos,
                                         const TMouseEvent &e) {
  if (m_setSaveboxTool && m_modifySavebox.getValue()) {
    m_setSaveboxTool->leftButtonDrag(pos);
    invalidate();
    return;
  }
  if (m_dragTool) {
    m_dragTool->leftButtonDrag(pos, e);
    invalidate();
    return;
  }

  TImageP image    = getImage(true);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;

  if (m_selecting) {
    if (m_strokeSelectionType.getValue() == RECT_SELECTION) {
      TDimension imageSize;
      if (ti)
        imageSize = ti->getSize();
      else if (ri)
        imageSize = ri->getRaster()->getSize();
      TPointD p(imageSize.lx % 2 ? 0.5 : 0.0, imageSize.ly % 2 ? 0.5 : 0.0);
      TRectD rectD(tround(std::min(m_firstPos.x, pos.x) - p.x) + p.x,
                   tround(std::min(m_firstPos.y, pos.y) - p.y) + p.y,
                   tround(std::max(m_firstPos.x, pos.x) - p.x) + p.x,
                   tround(std::max(m_firstPos.y, pos.y) - p.y) + p.y);

      m_selectingRect = rectD;
      m_bboxs.clear();
      invalidate();
    } else if (m_strokeSelectionType.getValue() == FREEHAND_SELECTION) {
      freehandDrag(pos);
      invalidate();
    }
    return;
  }

  double pixelSize        = getPixelSize();
  TTool::Application *app = TTool::getApplication();
  if (!app || m_justSelected || !m_selecting ||
      tdistance2(pos, m_curPos) < 9.0 * pixelSize * pixelSize)
    return;

  m_curPos = pos;

  if (m_strokeSelectionType.getValue() == FREEHAND_SELECTION) {
    freehandDrag(pos);
    invalidate();
  } else if (m_strokeSelectionType.getValue() == RECT_SELECTION) {
    bool selectOverlappingStroke = (m_firstPos.x > pos.x);
    TRectD rect(m_firstPos, pos);
    m_selectingRect = rect;
    invalidate();
  }
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::leftButtonUp(const TPointD &pos,
                                       const TMouseEvent &e) {
  if (m_setSaveboxTool && m_modifySavebox.getValue()) {
    m_setSaveboxTool->leftButtonUp(pos);
    invalidate();
    return;
  }
  m_leftButtonMousePressed = false;
  m_shiftPressed           = false;

  if (m_dragTool) {
    m_dragTool->leftButtonUp(pos, e);
    delete m_dragTool;
    m_dragTool = 0;
    invalidate();
    notifyImageChanged();
    return;
  }

  if (!m_selecting) return;

  // Se stavo modificando la selezione la completo

  TImageP image    = getImage(true);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (ti || ri) {
    if (m_strokeSelectionType.getValue() == RECT_SELECTION) {
      m_bboxs.push_back(m_selectingRect);
      m_rasterSelection.select(
          TRectD(m_selectingRect.getP00(), m_selectingRect.getP11()));
      m_rasterSelection.setFrameId(getCurrentFid());
      m_selectingRect.empty();
    } else if (m_strokeSelectionType.getValue() == FREEHAND_SELECTION) {
      closeFreehand(pos);
      if (m_stroke->getControlPointCount() > 5) {
        m_rasterSelection.select(*m_stroke);
        m_rasterSelection.setFrameId(getCurrentFid());
        m_rasterSelection.makeCurrent();
      }
      m_track.clear();
    }
  }
  m_selecting    = false;
  m_justSelected = false;
  invalidate();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::leftButtonDoubleClick(const TPointD &pos,
                                                const TMouseEvent &e) {
  TImageP image    = getImage(true);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;
  if (m_strokeSelectionType.getValue() == POLYLINE_SELECTION &&
      !m_polyline.empty()) {
    closePolyline(pos);
    if (m_stroke) {
      m_rasterSelection.select(*m_stroke);
      m_rasterSelection.setFrameId(getCurrentFid());
      m_rasterSelection.makeCurrent();
    }
    m_selecting = false;
    return;
  }
}

//-----------------------------------------------------------------------------
/*-- Paste後のフローティング状態の画像の描画 --*/
void RasterSelectionTool::drawFloatingSelection() {
  double pixelSize =
      TTool::getApplication()->getCurrentTool()->getTool()->getPixelSize();

  TAffine aff = m_rasterSelection.getTransformation();
  glPushMatrix();
  tglMultMatrix(aff);

  // draw m_floatingSelection
  if (isFloating()) {
    TRasterP floatingSelection = m_rasterSelection.getFloatingSelection();
    TImageP app;
    if (TRasterCM32P toonzRas = (TRasterCM32P)(floatingSelection))
      app = TToonzImageP(toonzRas, toonzRas->getBounds());
    if (TRaster32P fullColorRas = (TRaster32P)(floatingSelection))
      app = TRasterImageP(fullColorRas);
    if (TRasterGR8P grRas = (TRasterGR8P)(floatingSelection))
      app = TRasterImageP(grRas);
    app->setPalette(m_rasterSelection.getCurrentImage()->getPalette());
    FourPoints points = getBBox() * aff.inv();
    TRectD bbox       = points.getBox();
    TPointD center((bbox.getP00() + bbox.getP11()) * 0.5);
    if (TToonzImageP ti = (TToonzImageP)app)
      GLRasterPainter::drawRaster(TTranslation(center), ti, false);
    if (TRasterImageP ri = (TRasterImageP)app)
      GLRasterPainter::drawRaster(TTranslation(center), ri, true);
  }

  std::vector<TStroke> strokes = m_rasterSelection.getStrokes();
  int i;
  for (i = 0; i < (int)strokes.size(); i++) {
    TStroke stroke = strokes[i];
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0xF0F0);
    tglColor(TPixel32::Black);
    drawStrokeCenterline(stroke, pixelSize);
    glDisable(GL_LINE_STIPPLE);
  }

  glPopMatrix();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::draw() {
  TImageP image    = getImage(false);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;

  if (m_setSaveboxTool && m_modifySavebox.getValue()) {
    m_setSaveboxTool->draw();
    return;
  }

  glPushMatrix();

  /*-- フローティング画像の描画 --*/
  drawFloatingSelection();

  if (m_strokeSelectionType.getValue() == POLYLINE_SELECTION &&
      !m_rasterSelection.isFloating())
    drawPolylineSelection();
  else if (m_strokeSelectionType.getValue() == FREEHAND_SELECTION &&
           !m_rasterSelection.isFloating())
    drawFreehandSelection();
  if (m_rasterSelection.isEmpty()) m_bboxs.clear();

  /*-- 選択範囲の変形ハンドルの描画 --*/
  if (getBBoxsCount() > 0) drawCommandHandle(image.getPointer());

  /*-- 選択範囲の四角形の描画 --*/
  if (m_selecting && !m_selectingRect.isEmpty())
    drawRectSelection(image.getPointer());

  /*-- バウンディングボックスの描画 --*/
  /*
if(ti)
{
   TRectD saveBox = ToonzImageUtils::convertRasterToWorld(ti->getSavebox(), ti);
drawRect(saveBox.enlarge(0.5)*ti->getSubsampling(), TPixel32::Black, 0x5555,
true);
}
*/

  glPopMatrix();
}

//-----------------------------------------------------------------------------

TSelection *RasterSelectionTool::getSelection() { return &m_rasterSelection; }

//-----------------------------------------------------------------------------

bool RasterSelectionTool::isSelectionEmpty() {
  TImageP image    = getImage(false);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;

  if (!ti && !ri) return true;

  return m_rasterSelection.isEmpty();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::computeBBox() {
  TImageP image    = getImage(false);
  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;

  m_deformValues.reset();

  m_bboxs.clear();
  m_centers.clear();

  {
    std::vector<TStroke> strokes = m_rasterSelection.getStrokes();
    TRectD strokesRect           = m_rasterSelection.getStrokesBound(strokes);
    DragSelectionTool::FourPoints p;
    p = strokesRect;
    p = p * m_rasterSelection.getTransformation();
    m_bboxs.push_back(p);
    m_centers.push_back((p.getP00() + p.getP11()) * 0.5);
    m_rasterSelection.setSelectionBbox(strokesRect);
  }

  if (!m_freeDeformers.empty()) clearPointerContainer(m_freeDeformers);

  if (m_selectionFreeDeformer) {
    delete m_selectionFreeDeformer;
    m_selectionFreeDeformer = 0;
  }

  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::doOnActivate() {
  const TXshCell &imageCell = TTool::getImageCell();

  TImageP image =
      imageCell.getImage(false, 1);  // => See the onImageChanged() warning !

  TToonzImageP ti  = (TToonzImageP)image;
  TRasterImageP ri = (TRasterImageP)image;
  if (!ti && !ri) return;

  m_rasterSelection.makeCurrent();
  m_rasterSelection.setCurrentImage(image, imageCell);
  m_rasterSelection.selectNone();
  m_noAntialiasing.setValue(NoAntialiasing);
  m_rasterSelection.setNoAntialiasing(m_noAntialiasing.getValue());
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::doOnDeactivate() {
  TTool::getApplication()->getCurrentSelection()->setSelection(0);
  m_rasterSelection.setCurrentImage(0, TXshCell());
  m_rasterSelection.selectNone();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::onImageChanged() {
  // ATTENTION: using getImage(false, 1) *works* here, but it's trickier than
  // you could
  // expect. It works because:

  // 1. Invoking getImage(true) after getImage(false, 1) will return the same
  // image
  // 2. The cached fullsampled image may be in the 'not modified' ImageManager
  // state.
  //    Users could then alter the required image level subsampling - but doing
  //    so will
  //    immediately redirect here through 'onXshLevelChanged()' (thus resetting
  //    the subs back to 1).

  TImageP image    = getImage(false, 1);
  TToonzImageP ti  = image;
  TRasterImageP ri = image;

  if ((!ti && !ri) || image != m_rasterSelection.getCurrentImage())
    m_rasterSelection.selectNone();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::transformFloatingSelection(
    const TAffine &affine, const TPointD &center,
    const DragSelectionTool::FourPoints &points) {
  m_rasterSelection.setTransformation(affine);
  if (isFloating()) {
    setBBox(points);
    setCenter(center);
  }
  invalidate();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::increaseTransformationCount() {
  if (m_rasterSelection.getTransformationCount() != m_transformationCount)
    m_transformationCount = 0;
  m_transformationCount++;
  m_rasterSelection.setTransformationCount(m_transformationCount);
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::decreaseTransformationCount() {
  m_transformationCount--;
  m_rasterSelection.setTransformationCount(m_transformationCount);
  if (m_rasterSelection.getTransformationCount() == 0)
    m_rasterSelection.pasteFloatingSelection();
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::onActivate() {
  if (m_firstTime) {
    if (m_targetType & ToonzImage)
      m_modifySavebox.setValue(ModifySavebox ? 1 : 0);
  }

  SelectionTool::onActivate();
}

//-----------------------------------------------------------------------------

bool RasterSelectionTool::onPropertyChanged(std::string propertyName) {
  if (!SelectionTool::onPropertyChanged(propertyName)) return false;
  if (m_targetType & ToonzImage) {
    ModifySavebox = (int)(m_modifySavebox.getValue());
    invalidate();
  }
  if (propertyName == m_noAntialiasing.getName()) {
    NoAntialiasing = m_noAntialiasing.getValue() ? 1 : 0;
    m_rasterSelection.setNoAntialiasing(m_noAntialiasing.getValue());
  }

  return true;
}

//-----------------------------------------------------------------------------

void RasterSelectionTool::updateTranslation() {
  if (m_targetType & ToonzImage)
    m_modifySavebox.setQStringName(tr("Modify Savebox"));

  m_noAntialiasing.setQStringName(tr("No Antialiasing"));
  SelectionTool::updateTranslation();
}

//=============================================================================

RasterSelectionTool toonzRasterSelectionTool(TTool::ToonzImage);
RasterSelectionTool fullColorRasterSelectionTool(TTool::RasterImage);
