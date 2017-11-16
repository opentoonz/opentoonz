

// TnzCore
#include "timagecache.h"
#include "tcurveutil.h"

// ToonzLib
#include "toonz/stage2.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcleanupper.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/observer.h"
#include "toonz/imagemanager.h"

#include "toonz/tscenehandle.h"

// TnzTools includes
#include "tools/toolutils.h"
#include "tools/cursors.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "historytypes.h"

// Toonz includes
#include "tapp.h"
#include "cleanupsettingsmodel.h"

// Qt includes
#include <QTimer>

#include "cleanuppreview.h"

// TODO: Avoid rebuilding the preview as long as parameters are untouched. Use a
// flag associated to
// each frame so we know when frames need to be rebuilt

//**********************************************************************************
//    Local namespace stuff
//**********************************************************************************

namespace {

PreviewToggleCommand previewToggle;
CameraTestToggleCommand cameraTestToggle;

}  // namespace

//**********************************************************************************
//    PreviewToggleCommand implementation
//**********************************************************************************

PreviewToggleCommand::PreviewToggleCommand()
    : MenuItemHandler("MI_CleanupPreview") {
  // Setup the processing timer. The timer is needed to prevent (or rather,
  // cumulate)
  // short-lived parameter changes to trigger any preview processing.
  m_timer.setSingleShot(true);
  m_timer.setInterval(500);
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::execute() {
  CleanupPreviewCheck *pc = CleanupPreviewCheck::instance();
  if (pc->isEnabled())
    enable();
  else
    disable();
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::enable() {
  // Cleanup Preview and Camera Test are exclusive. In case, disable the latter.
  // NOTE: This is done *before* attaching, since attach may invoke a preview
  // rebuild.
  CameraTestCheck *tc = CameraTestCheck::instance();
  tc->setIsEnabled(false);

  // Attach to the model
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  model->attach(CleanupSettingsModel::LISTENER |
                CleanupSettingsModel::PREVIEWER);

  // Connect signals
  bool ret = true;
  ret      = ret && connect(model, SIGNAL(previewDataChanged()), this,
                       SLOT(onPreviewDataChanged()));
  ret = ret && connect(model, SIGNAL(modelChanged(bool)), this,
                       SLOT(onModelChanged(bool)));
  ret = ret && connect(&m_timer, SIGNAL(timeout()), this, SLOT(postProcess()));

  TPaletteHandle *ph =
      TApp::instance()->getPaletteController()->getCurrentCleanupPalette();
  ret = ret &&
        connect(ph, SIGNAL(colorStyleChanged(bool)), &m_timer, SLOT(start()));
  ret = ret && connect(ph, SIGNAL(paletteChanged()), &m_timer, SLOT(start()));
  assert(ret);

  onPreviewDataChanged();

  // in preview cleanup mode, tools are forbidden! Reverting to hand...
  TApp::instance()->getCurrentTool()->setTool(T_Hand);
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::disable() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  model->detach(CleanupSettingsModel::LISTENER |
                CleanupSettingsModel::PREVIEWER);

  bool ret = true;
  ret      = ret && disconnect(model, SIGNAL(previewDataChanged()), this,
                          SLOT(onPreviewDataChanged()));
  ret = ret && disconnect(model, SIGNAL(modelChanged(bool)), this,
                          SLOT(onModelChanged(bool)));
  ret =
      ret && disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(postProcess()));

  // Cleanup palette changes all falls under post-processing stuff. And do not
  // involve the model.
  TPaletteHandle *ph =
      TApp::instance()->getPaletteController()->getCurrentCleanupPalette();
  ret = ret && disconnect(ph, SIGNAL(colorStyleChanged(bool)), &m_timer,
                          SLOT(start()));
  ret =
      ret && disconnect(ph, SIGNAL(paletteChanged()), &m_timer, SLOT(start()));
  assert(ret);

  clean();

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::onPreviewDataChanged() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  // Retrieve level under cleanup
  TXshSimpleLevel *sl;
  TFrameId fid;
  model->getCleanupFrame(sl, fid);

  // In case the level changes, release all previously previewed images
  if (m_sl.getPointer() != sl) clean();

  m_sl = sl;
  if (sl) {
    if (!(sl->getFrameStatus(fid) & TXshSimpleLevel::CleanupPreview)) {
      // The frame was not yet cleanup-previewed. Update its status then.
      m_fids.push_back(fid);
      sl->setFrameStatus(
          fid, sl->getFrameStatus(fid) | TXshSimpleLevel::CleanupPreview);
    }

    postProcess();
  }
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::onModelChanged(bool needsPostProcess) {
  if (needsPostProcess) m_timer.start();
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::postProcess() {
  TApp *app                   = TApp::instance();
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  TXshSimpleLevel *sl;
  TFrameId fid;
  model->getCleanupFrame(sl, fid);

  assert(sl);
  if (!sl) return;

  // Retrieve transformed image
  TRasterImageP transformed;
  {
    TRasterImageP original;
    TAffine transform;

    model->getPreviewData(original, transformed, transform);
    if (!transformed) return;
  }

  // Perform post-processing
  TRasterImageP preview(
      TCleanupper::instance()->processColors(transformed->getRaster()));

  TPointD dpi;
  transformed->getDpi(dpi.x, dpi.y);
  preview->setDpi(dpi.x, dpi.y);

  transformed = TRasterImageP();

  // Substitute current frame image with previewed one
  sl->setFrame(fid, preview);

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//--------------------------------------------------------------------------

void PreviewToggleCommand::clean() {
  // Release all previewed images
  if (m_sl) {
    int i, fidsCount = m_fids.size();
    for (i = 0; i < fidsCount; ++i) {
      const TFrameId &fid = m_fids[i];

      int status = m_sl->getFrameStatus(fid);
      if (status & TXshSimpleLevel::CleanupPreview) {
        // Preview images are not just invalidated, but *unbound* from the IM.
        // This is currently done hard here - should be skipped to m_sl,
        // though...
        ImageManager *im = ImageManager::instance();
        im->unbind(m_sl->getImageId(fid, TXshSimpleLevel::CleanupPreview));

        IconGenerator::instance()->remove(m_sl.getPointer(), fid);

        m_sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      }
    }
  }

  m_sl = TXshSimpleLevelP();
  m_fids.clear();
}

//**********************************************************************************
//    CameraTestToggleCommand implementation
//**********************************************************************************

CameraTestToggleCommand::CameraTestToggleCommand()
    : MenuItemHandler("MI_CameraTest"), m_oldTool(0) {
  m_timer.setSingleShot(true);
  m_timer.setInterval(500);
}

//--------------------------------------------------------------------------

void CameraTestToggleCommand::execute() {
  CameraTestCheck *tc = CameraTestCheck::instance();
  if (tc->isEnabled())
    enable();
  else
    disable();
}

//--------------------------------------------------------------------------

void CameraTestToggleCommand::enable() {
  /*---既に現在のツールがCameraTestになっている場合はreturn---*/
  m_oldTool = TApp::instance()->getCurrentTool()->getTool();
  if (m_oldTool->getName().compare("T_CameraTest") == 0) {
    CameraTestCheck::instance()->setIsEnabled(true);
    disable();
    return;
  }

  // Cleanup Preview and Camera Test are exclusive. In case, disable the latter.
  // NOTE: This is done *before* attaching, since attach may invoke a preview
  // rebuild.
  CleanupPreviewCheck *pc = CleanupPreviewCheck::instance();
  pc->setIsEnabled(false);

  // Attach to the model
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  model->attach(
      CleanupSettingsModel::LISTENER | CleanupSettingsModel::CAMERATEST, false);

  // Connect signals
  bool ret = true;
  ret      = ret && connect(model, SIGNAL(previewDataChanged()), this,
                       SLOT(onPreviewDataChanged()));
  assert(ret);

  onPreviewDataChanged();

  TApp::instance()->getCurrentTool()->setTool("T_CameraTest");
}

//--------------------------------------------------------------------------

void CameraTestToggleCommand::disable() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();
  model->detach(CleanupSettingsModel::LISTENER |
                CleanupSettingsModel::CAMERATEST);

  bool ret = true;
  ret      = ret && disconnect(model, SIGNAL(previewDataChanged()), this,
                          SLOT(onPreviewDataChanged()));
  assert(ret);

  clean();

  TApp::instance()->getCurrentLevel()->notifyLevelChange();

  TApp::instance()->getCurrentTool()->setTool(
      QString::fromStdString(m_oldTool->getName()));
  m_oldTool = 0;
}

//--------------------------------------------------------------------------

void CameraTestToggleCommand::onPreviewDataChanged() {
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  // Retrieve level under cleanup
  TXshSimpleLevel *sl;
  TFrameId fid;
  model->getCleanupFrame(sl, fid);

  // In case the level changes, release all previously previewed images
  if (m_sl.getPointer() != sl) clean();

  m_sl = sl;
  if (sl) {
    if (!(sl->getFrameStatus(fid) & TXshSimpleLevel::CleanupPreview)) {
      m_fids.push_back(fid);
      sl->setFrameStatus(
          fid, sl->getFrameStatus(fid) | TXshSimpleLevel::CleanupPreview);
    }

    postProcess();
  }
}

//--------------------------------------------------------------------------

void CameraTestToggleCommand::postProcess() {
  TApp *app                   = TApp::instance();
  CleanupSettingsModel *model = CleanupSettingsModel::instance();

  TXshSimpleLevel *sl;
  TFrameId fid;
  model->getCleanupFrame(sl, fid);

  assert(sl);
  if (!sl) return;

  // Retrieve transformed image
  TRasterImageP transformed;
  {
    TRasterImageP original;
    model->getCameraTestData(original, transformed);
  }

  // Substitute current frame image with previewed one
  sl->setFrame(fid, transformed);

  TApp::instance()->getCurrentLevel()->notifyLevelChange();
}

//--------------------------------------------------------------------------

void CameraTestToggleCommand::clean() {
  // Release all previewed images
  if (m_sl) {
    int i, fidsCount = m_fids.size();
    for (i = 0; i < fidsCount; ++i) {
      const TFrameId &fid = m_fids[i];

      int status = m_sl->getFrameStatus(fid);
      if (status & TXshSimpleLevel::CleanupPreview) {
        ImageManager *im = ImageManager::instance();
        im->unbind(m_sl->getImageId(fid, TXshSimpleLevel::CleanupPreview));

        IconGenerator::instance()->remove(m_sl.getPointer(), fid);

        m_sl->setFrameStatus(fid, status & ~TXshSimpleLevel::CleanupPreview);
      }
    }
  }

  m_sl = TXshSimpleLevelP();
  m_fids.clear();
}

//=============================================================================
/*! CameraTestのドラッグ移動のUndo
*/
class UndoCameraTestMove final : public TUndo {
  TPointD m_before, m_after;
  CleanupParameters *m_cp;

public:
  UndoCameraTestMove(const TPointD &before, const TPointD &after,
                     CleanupParameters *cp)
      : m_before(before), m_after(after), m_cp(cp) {}

  void onChange() const {
    CleanupSettingsModel::instance()->commitChanges();
    TApp::instance()->getCurrentTool()->getTool()->invalidate();
  }

  void undo() const override {
    /*--- 既にCleanupSettingsを移動していたらreturn ---*/
    CleanupParameters *cp =
        CleanupSettingsModel::instance()->getCurrentParameters();
    if (cp != m_cp) return;
    m_cp->m_offx = m_before.x;
    m_cp->m_offy = m_before.y;

    onChange();
  }
  void redo() const override {
    /*--- 既にCleanupSettingsを移動していたらreturn ---*/
    CleanupParameters *cp =
        CleanupSettingsModel::instance()->getCurrentParameters();
    if (cp != m_cp) return;
    m_cp->m_offx = m_after.x;
    m_cp->m_offy = m_after.y;

    onChange();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Move Cleanup Camera");
  }
  int getHistoryType() override { return HistoryType::EditTool_Move; }
};

//=============================================================================
/*! CameraTestのサイズ変更のUndo
*/
class UndoCameraTestScale final : public TUndo {
  TDimension m_resBefore, m_resAfter;
  TDimensionD m_sizeBefore, m_sizeAfter;
  CleanupParameters *m_cp;

public:
  UndoCameraTestScale(const TDimension &resBefore,
                      const TDimensionD &sizeBefore, const TDimension &resAfter,
                      const TDimensionD &sizeAfter, CleanupParameters *cp)
      : m_resBefore(resBefore)
      , m_sizeBefore(sizeBefore)
      , m_resAfter(resAfter)
      , m_sizeAfter(sizeAfter)
      , m_cp(cp) {}

  void onChange() const {
    CleanupSettingsModel::instance()->commitChanges();
    TApp::instance()->getCurrentTool()->getTool()->invalidate();
  }

  void undo() const override {
    /*--- 既にCleanupSettingsを移動していたらreturn ---*/
    CleanupParameters *cp =
        CleanupSettingsModel::instance()->getCurrentParameters();
    if (cp != m_cp) return;

    m_cp->m_camera.setSize(m_sizeBefore, false, false);
    m_cp->m_camera.setRes(m_resBefore);

    onChange();
  }

  void redo() const override {
    /*--- 既にCleanupSettingsを移動していたらreturn ---*/
    CleanupParameters *cp =
        CleanupSettingsModel::instance()->getCurrentParameters();
    if (cp != m_cp) return;

    m_cp->m_camera.setSize(m_sizeAfter, false, false);
    m_cp->m_camera.setRes(m_resAfter);

    onChange();
  }
  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Scale Cleanup Camera");
  }
  int getHistoryType() override { return HistoryType::EditTool_Move; }
};

//**********************************************************************************
//    CameraTestTool definition
//**********************************************************************************

class CameraTestTool final : public TTool {
  TPointD m_lastPos;
  bool m_dragged;
  int m_scaling;

  enum { eNoScale, e00, e01, e10, e11, eM0, e1M, eM1, e0M };

  /*--- +ShiftドラッグでX、Y軸平行移動機能のため ---*/
  TPointD m_firstPos;
  TPointD m_firstCameraOffset;
  // for scaling undo
  TDimension m_firstRes;
  TDimensionD m_firstSize;

public:
  CameraTestTool();

  void draw() override;

  void mouseMove(const TPointD &p, const TMouseEvent &e) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;

  ToolType getToolType() const override { return TTool::GenericTool; }

  int getCursorId() const override;

private:
  void drawCleanupCamera(double pixelSize);
  void drawClosestFieldCamera(double pixelSize);

} cameraTestTool;

//==========================================================================

CameraTestTool::CameraTestTool()
    : TTool("T_CameraTest")
    , m_lastPos(-1, -1)
    , m_dragged(false)
    , m_scaling(eNoScale)
    , m_firstRes(0, 0)
    , m_firstSize(0, 0) {
  bind(TTool::AllTargets);  // Deals with tool deactivation internally
}

//--------------------------------------------------------------------------

void CameraTestTool::drawCleanupCamera(double pixelSize) {
  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();

  TRectD rect = cp->m_camera.getStageRect();

  glColor3d(1.0, 0.0, 0.0);
  glLineStipple(1, 0xFFFF);
  glEnable(GL_LINE_STIPPLE);

  // box
  glBegin(GL_LINE_STRIP);
  glVertex2d(rect.x0, rect.y0);
  glVertex2d(rect.x0, rect.y1 - pixelSize);
  glVertex2d(rect.x1 - pixelSize, rect.y1 - pixelSize);
  glVertex2d(rect.x1 - pixelSize, rect.y0);
  glVertex2d(rect.x0, rect.y0);
  glEnd();

  // central cross
  double dx = 0.05 * rect.getP00().x;
  double dy = 0.05 * rect.getP00().y;
  tglDrawSegment(TPointD(-dx, -dy), TPointD(dx, dy));
  tglDrawSegment(TPointD(-dx, dy), TPointD(dx, -dy));

  glDisable(GL_LINE_STIPPLE);

  // camera name
  TPointD pos = rect.getP01() + TPointD(0, 4);
  glPushMatrix();
  glTranslated(pos.x, pos.y, 0);
  glScaled(2, 2, 2);
  tglDrawText(TPointD(), "Cleanup Camera");
  glPopMatrix();
}

//--------------------------------------------------------------------------

void CameraTestTool::drawClosestFieldCamera(double pixelSize) {
  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();

  double zoom = cp->m_closestField / cp->m_camera.getSize().lx;
  if (areAlmostEqual(zoom, 1.0, 1e-2)) return;

  TRectD rect(cp->m_camera.getStageRect());
  rect = rect.enlarge((zoom - 1) * (rect.x1 - rect.x0 + 1) / 2.0,
                      (zoom - 1) * (rect.y1 - rect.y0 + 1) / 2.0);

  glColor3d(0.0, 0.0, 1.0);
  glLineStipple(1, 0xFFFF);
  glEnable(GL_LINE_STIPPLE);

  // box
  glBegin(GL_LINE_STRIP);
  glVertex2d(rect.x0, rect.y0);
  glVertex2d(rect.x0, rect.y1 - pixelSize);
  glVertex2d(rect.x1 - pixelSize, rect.y1 - pixelSize);
  glVertex2d(rect.x1 - pixelSize, rect.y0);
  glVertex2d(rect.x0, rect.y0);
  glEnd();

  glDisable(GL_LINE_STIPPLE);

  // camera name
  TPointD pos = rect.getP01() + TPointD(0, 4);
  glPushMatrix();
  glTranslated(pos.x, pos.y, 0);
  glScaled(2, 2, 2);
  tglDrawText(TPointD(), "Closest Field");
  glPopMatrix();
}

//--------------------------------------------------------------------------

void CameraTestTool::draw() {
  double pixelSize = getPixelSize();
  glPushMatrix();

  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();

  glTranslated(-0.5 * cp->m_offx * Stage::inch, -0.5 * cp->m_offy * Stage::inch,
               0);
  drawCleanupCamera(pixelSize);
  // drawClosestFieldCamera(pixelSize);

  TRectD r(cp->m_camera.getStageRect());
  TPointD size(10, 10);

  double pixelSize4 = 4.0 * pixelSize;

  tglColor(TPixel::Red);
  ToolUtils::drawSquare(r.getP00(), pixelSize4, TPixel::Red);
  ToolUtils::drawSquare(r.getP01(), pixelSize4, TPixel::Red);
  ToolUtils::drawSquare(r.getP10(), pixelSize4, TPixel::Red);
  ToolUtils::drawSquare(r.getP11(), pixelSize4, TPixel::Red);

  TPointD center(0.5 * (r.getP00() + r.getP11()));

  ToolUtils::drawSquare(TPointD(center.x, r.y0), pixelSize4,
                        TPixel::Red);  // draw M0 handle
  ToolUtils::drawSquare(TPointD(r.x1, center.y), pixelSize4,
                        TPixel::Red);  // draw 1M handle
  ToolUtils::drawSquare(TPointD(center.x, r.y1), pixelSize4,
                        TPixel::Red);  // draw M1 handle
  ToolUtils::drawSquare(TPointD(r.x0, center.y), pixelSize4,
                        TPixel::Red);  // draw 0M handle

  glPopMatrix();
}

void CameraTestTool::mouseMove(const TPointD &p, const TMouseEvent &e) {
  if (m_lastPos.x != -1)  // left mouse button is clicked
  {
    m_scaling = eNoScale;
    return;
  }

  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();
  double pixelSize = getPixelSize();

  TPointD size(10 * pixelSize, 10 * pixelSize);
  TRectD r(cp->m_camera.getStageRect());
  TPointD aux(Stage::inch * TPointD(0.5 * cp->m_offx, 0.5 * cp->m_offy));
  r -= aux;
  double maxDist = 5 * pixelSize;

  if (TRectD(r.getP00() - size, r.getP00() + size).contains(p))
    m_scaling = e00;
  else if (TRectD(r.getP01() - size, r.getP01() + size).contains(p))
    m_scaling = e01;
  else if (TRectD(r.getP11() - size, r.getP11() + size).contains(p))
    m_scaling = e11;
  else if (TRectD(r.getP10() - size, r.getP10() + size).contains(p))
    m_scaling = e10;
  else if (isCloseToSegment(p, TSegment(r.getP00(), r.getP10()), maxDist))
    m_scaling = eM0;
  else if (isCloseToSegment(p, TSegment(r.getP10(), r.getP11()), maxDist))
    m_scaling = e1M;
  else if (isCloseToSegment(p, TSegment(r.getP11(), r.getP01()), maxDist))
    m_scaling = eM1;
  else if (isCloseToSegment(p, TSegment(r.getP01(), r.getP00()), maxDist))
    m_scaling = e0M;
  else
    m_scaling = eNoScale;
}

//--------------------------------------------------------------------------

void CameraTestTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_firstPos = m_lastPos = pos;
  /*-- カメラオフセットの初期値の取得 --*/
  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();

  if (m_scaling == eNoScale)
    m_firstCameraOffset = TPointD(cp->m_offx, cp->m_offy);
  /*-- サイズ変更のUndoのために値を格納 --*/
  else {
    m_firstRes  = cp->m_camera.getRes();
    m_firstSize = cp->m_camera.getSize();
  }

  // Limit commits to the sole interface updates. This is necessary since drags
  // would otherwise trigger full preview re-processings.
  CleanupSettingsModel::instance()->setCommitMask(
      CleanupSettingsModel::INTERFACE);
}

//--------------------------------------------------------------------------

void CameraTestTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  m_dragged = true;
  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();

  TPointD dp(pos - m_lastPos);
  if (m_scaling != eNoScale) {
    TDimensionD dim(cp->m_camera.getSize());
    TPointD delta;

    // Mid-edge cases
    if (m_scaling == e1M)
      delta = TPointD(dp.x / Stage::inch, 0);
    else if (m_scaling == e0M)
      delta = TPointD(-dp.x / Stage::inch, 0);
    else if (m_scaling == eM1)
      delta = TPointD(0, dp.y / Stage::inch);
    else if (m_scaling == eM0)
      delta = TPointD(0, -dp.y / Stage::inch);
    else {
      // Corner cases
      if (e.isShiftPressed()) {
        // Free adjust
        if (m_scaling == e11)
          delta = TPointD(dp.x / Stage::inch, dp.y / Stage::inch);
        else if (m_scaling == e00)
          delta = TPointD(-dp.x / Stage::inch, -dp.y / Stage::inch);
        else if (m_scaling == e10)
          delta = TPointD(dp.x / Stage::inch, -dp.y / Stage::inch);
        else if (m_scaling == e01)
          delta = TPointD(-dp.x / Stage::inch, dp.y / Stage::inch);
      } else {
        // A/R conservative
        bool xMaximalDp = (fabs(dp.x) > fabs(dp.y));

        if (m_scaling == e11)
          delta.x = (xMaximalDp ? dp.x : dp.y) / Stage::inch;
        else if (m_scaling == e00)
          delta.x = (xMaximalDp ? -dp.x : -dp.y) / Stage::inch;
        else if (m_scaling == e10)
          delta.x = (xMaximalDp ? dp.x : -dp.y) / Stage::inch;
        else if (m_scaling == e01)
          delta.x = (xMaximalDp ? -dp.x : dp.y) / Stage::inch;

        // Keep A/R
        delta.y = delta.x * dim.ly / dim.lx;
      }
    }

    TDimensionD newDim(dim.lx + 2.0 * delta.x, dim.ly + 2.0 * delta.y);
    if (newDim.lx < 2.0 || newDim.ly < 2.0) return;

    cp->m_camera.setSize(newDim,
                         true,    // Preserve DPI
                         false);  // A/R imposed above in corner cases
  } else {
    if (e.isShiftPressed()) {
      TPointD delta = pos - m_firstPos;
      cp->m_offx    = m_firstCameraOffset.x;
      cp->m_offy    = m_firstCameraOffset.y;
      if (fabs(delta.x) > fabs(delta.y)) {
        if (!cp->m_offx_lock) cp->m_offx += -delta.x * (2.0 / Stage::inch);
      } else {
        if (!cp->m_offy_lock) cp->m_offy += -delta.y * (2.0 / Stage::inch);
      }
    } else {
      if (!cp->m_offx_lock) cp->m_offx += -dp.x * (2.0 / Stage::inch);
      if (!cp->m_offy_lock) cp->m_offy += -dp.y * (2.0 / Stage::inch);
    }
  }

  m_lastPos = pos;

  CleanupSettingsModel::instance()->commitChanges();
  invalidate();
}

//--------------------------------------------------------------------------

void CameraTestTool::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  // Reset full commit status - invokes preview rebuild on its own.
  CleanupSettingsModel::instance()->setCommitMask(
      CleanupSettingsModel::FULLPROCESS);

  CleanupParameters *cp =
      CleanupSettingsModel::instance()->getCurrentParameters();
  /*-- カメラ位置移動のUndo --*/
  if (m_scaling == eNoScale) {
    /*-- 値が変わったらUndoを登録 --*/
    if (m_firstCameraOffset.x != cp->m_offx ||
        m_firstCameraOffset.y != cp->m_offy) {
      UndoCameraTestMove *undo = new UndoCameraTestMove(
          m_firstCameraOffset, TPointD(cp->m_offx, cp->m_offy), cp);
      TUndoManager::manager()->add(undo);
    }
  }
  /*-- サイズ変更のUndo --*/
  else {
    if (m_firstSize.lx != cp->m_camera.getSize().lx ||
        m_firstSize.ly != cp->m_camera.getSize().ly) {
      UndoCameraTestScale *undo = new UndoCameraTestScale(
          m_firstRes, m_firstSize, cp->m_camera.getRes(),
          cp->m_camera.getSize(), cp);
      TUndoManager::manager()->add(undo);
    }
  }

  m_firstPos          = TPointD(-1, -1);
  m_firstCameraOffset = TPointD(0, 0);
  m_firstRes          = TDimension(0, 0);
  m_firstSize         = TDimensionD(0, 0);

  m_lastPos = TPointD(-1, -1);
  m_dragged = false;
}

//--------------------------------------------------------------------------

int CameraTestTool::getCursorId() const {
  switch (m_scaling) {
  case eNoScale: {
    CleanupParameters *cp =
        CleanupSettingsModel::instance()->getCurrentParameters();
    if (cp->m_offx_lock && cp->m_offy_lock)
      return ToolCursor::DisableCursor;
    else if (cp->m_offx_lock)
      return ToolCursor::MoveNSCursor;
    else if (cp->m_offy_lock)
      return ToolCursor::MoveEWCursor;
    else
      return ToolCursor::MoveCursor;
  }
  case e11:
  case e00:
    return ToolCursor::ScaleCursor;
  case e10:
  case e01:
    return ToolCursor::ScaleInvCursor;
  case e1M:
  case e0M:
    return ToolCursor::ScaleHCursor;
  case eM1:
  case eM0:
    return ToolCursor::ScaleVCursor;
  default:
    assert(false);
    return 0;
  }
}

//==============================================================================
//
// OpacityCheckToggleCommand
//
//==============================================================================

class OpacityCheckToggleCommand final : public MenuItemHandler {
public:
  OpacityCheckToggleCommand() : MenuItemHandler("MI_OpacityCheck") {}
  void execute() override {
    CleanupSettingsModel *model = CleanupSettingsModel::instance();
    CleanupParameters *params   = model->getCurrentParameters();

    params->m_transparencyCheckEnabled = !params->m_transparencyCheckEnabled;

    /*-- OpacityCheckのON/OFFでは、Dirtyフラグは変化させない --*/
    bool dirty = params->getDirtyFlag();
    model->commitChanges();
    params->setDirtyFlag(dirty);
  }
} OpacityCheckToggle;
