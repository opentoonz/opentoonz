
#ifdef LINUX
#define GL_GLEXT_PROTOTYPES
#endif

// Toonz includes
#include "tapp.h"
#include "viewerpane.h"
#include "onionskinmaskgui.h"
#include "viewerdraw.h"
#include "menubarcommandids.h"
#include "ruler.h"
#include "locatorpopup.h"

// TnzTools includes
#include "tools/cursors.h"
#include "tools/cursormanager.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tools/toolutils.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tcamera.h"
#include "toonz/stage2.h"
#include "toonz/stage.h"
#include "toonz/stageplayer.h"
#include "toonz/stagevisitor.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/childstack.h"
#include "toonz/dpiscale.h"
#include "toonz/txshlevel.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/preferences.h"
#include "toonz/glrasterpainter.h"
#include "toonz/cleanupparameters.h"
#include "toonz/toonzimageutils.h"
#include "toonz/txshleveltypes.h"
#include "toonz/preferences.h"
#include "subcameramanager.h"

// TnzCore includes
#include "tpalette.h"
#include "tropcm.h"
#include "tgl.h"
#include "tofflinegl.h"
#include "tstopwatch.h"
#include "trop.h"
#include "timagecache.h"
#include "trasterimage.h"
#include "tstroke.h"
#include "tgldisplaylistsmanager.h"
#include "ttoonzimage.h"

// Qt includes
#include <QMenu>
#include <QApplication>
#include <QDesktopWidget>
#if QT_VERSION >= 0x050000
#include <QInputMethod>
#else
#include <QInputContext>
#endif
#include <QGLContext>

#include "sceneviewer.h"

void drawSpline(const TAffine &viewMatrix, const TRect &clipRect, bool camera3d,
                double pixelSize);

//-------------------------------------------------------------------------------
namespace {
//-------------------------------------------------------------------------------

int l_displayListsSpaceId =
    -1;  //!< Display lists space id associated with SceneViewers
QGLWidget *l_proxy = 0;  //!< Proxy associated with the above
std::set<TGlContext>
    l_contexts;  //!< Stores every SceneViewer context (see ~SceneViewer)

//-------------------------------------------------------------------------------

QGLWidget *touchProxy() {
  struct GLWidgetProxy final : public TGLDisplayListsProxy {
    ~GLWidgetProxy() {
      delete l_proxy;
      l_proxy = 0;
    }

    void makeCurrent() override { l_proxy->makeCurrent(); }
    void doneCurrent() override { l_proxy->doneCurrent(); }
  };

  // If it does not exist, create the viewer's display lists proxy
  if (!l_proxy) {
    l_proxy = new QGLWidget;
    l_displayListsSpaceId =
        TGLDisplayListsManager::instance()->storeProxy(new GLWidgetProxy);
  }

  return l_proxy;
}

//-------------------------------------------------------------------------------

double getActualFrameRate() {
  // compute frame per second
  static double fps = 0;
  static TStopWatch stopwatch;
  static int frame = 0;
  ++frame;

  stopwatch.start();

  unsigned long tt = stopwatch.getTotalTime();

  // wait a time greater than one second
  if (tt > 1000) {
    stopwatch.stop();
    fps = troundp(((1000 * frame) / (double)tt));
    stopwatch.start(true);
    frame = 0;
  }
  return fps;
}

//-----------------------------------------------------------------------------

void copyFrontBufferToBackBuffer() {
  static GLint viewport[4];
  static GLfloat raster_pos[4];

  glGetIntegerv(GL_VIEWPORT, viewport);

  /* set source buffer */
  glReadBuffer(GL_FRONT);

  /* set projection matrix */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, viewport[2], 0, viewport[3]);

  /* set modelview matrix */
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  /* save old raster position */
  glGetFloatv(GL_CURRENT_RASTER_POSITION, raster_pos);

  /* set raster position */
  glRasterPos4f(0.0, 0.0, 0.0, 1.0);

  /* copy buffer */
  glCopyPixels(0, 0, viewport[2], viewport[3], GL_COLOR);

  /* restore old raster position */
  glRasterPos4fv(raster_pos);

  /* restore old matrices */
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  /* restore source buffer */
  glReadBuffer(GL_BACK);
}
//-----------------------------------------------------------------------------
/*! Compute new 3Dposition and new 2D position. */
T3DPointD computeNew3DPosition(T3DPointD start3DPos, TPointD delta2D,
                               TPointD &new2dPos, GLdouble modelView3D[16],
                               GLdouble projection3D[16], GLint viewport3D[4],
                               int devPixRatio) {
  GLdouble pos2D_x, pos2D_y, pos2D_z;
  gluProject(-start3DPos.x, -start3DPos.y, start3DPos.z, modelView3D,
             projection3D, viewport3D, &pos2D_x, &pos2D_y, &pos2D_z);
  new2dPos = TPointD(pos2D_x + delta2D.x, pos2D_y + delta2D.y);
  GLdouble pos3D_x, pos3D_y, pos3D_z;
  gluUnProject(new2dPos.x, new2dPos.y, 1, modelView3D, projection3D, viewport3D,
               &pos3D_x, &pos3D_y, &pos3D_z);
  new2dPos.y = viewport3D[3] - new2dPos.y - 20 * devPixRatio;
  return T3DPointD(pos3D_x, pos3D_y, pos3D_z);
}

//-----------------------------------------------------------------------------
#ifdef DA_RIVEDERE

// Il metodo copia una porzione del BACK_BUFFER definita da rect nel
// FRONTE_BUFFER
// Attualmente si notano delle brutture intorno al rettangolo.
// Per il momento si lavora nel BACK_BUFFER e sicopia tutto il FRONT_BUFFER.
// Riattivando questo metodo bisogna ricordarsi di disattivare lo swapbuffer in
// GLInvalidateRect prima di
// fare updateGL() e riattivarlo immediatamernte dopo!
void copyBackBufferToFrontBuffer(const TRect &rect) {
  static GLint viewport[4];
  static GLfloat raster_pos[4];

  glGetIntegerv(GL_VIEWPORT, viewport);

  /* set source buffer */
  glReadBuffer(GL_BACK);
  glDrawBuffer(GL_FRONT);

  /* set projection matrix */
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, viewport[2], 0, viewport[3]);

  /* set modelview matrix */
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  /* save old raster position */
  glGetFloatv(GL_CURRENT_RASTER_POSITION, raster_pos);

  /* set raster position */
  glRasterPos4f(0.0, 0.0, 0.0, 1.0);

  /* copy buffer */
  glCopyPixels(0, 0, viewport[2], viewport[3], GL_COLOR);

  /* restore old raster position */
  glRasterPos4fv(raster_pos);

  /* restore old matrices */
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  /* restore source buffer */
  glDrawBuffer(GL_BACK);
}

#endif
//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// ToggleCommand
//-----------------------------------------------------------------------------

ToggleCommandHandler::ToggleCommandHandler(CommandId id, bool startStatus)
    : MenuItemHandler(id), m_status(startStatus) {}

void ToggleCommandHandler::execute() {
  m_status = !m_status;
  // emit sceneChanged WITHOUT dirty flag
  TApp::instance()->getCurrentScene()->notifySceneChanged(false);
}

//-----------------------------------------------------------------------------

ToggleCommandHandler viewTableToggle(MI_ViewTable, false);
ToggleCommandHandler fieldGuideToggle(MI_FieldGuide, false);
ToggleCommandHandler safeAreaToggle(MI_SafeArea, false);
ToggleCommandHandler rasterizePliToggle(MI_RasterizePli, false);

ToggleCommandHandler viewClcToggle("MI_ViewColorcard", false);
ToggleCommandHandler viewCameraToggle("MI_ViewCamera", false);
ToggleCommandHandler viewBBoxToggle("MI_ViewBBox", false);
ToggleCommandHandler viewGuideToggle("MI_ViewGuide", false);
ToggleCommandHandler viewRulerToggle("MI_ViewRuler", false);

//-----------------------------------------------------------------------------

void invalidateIcons() {
  ToonzCheck *tc = ToonzCheck::instance();
  int mask       = tc->getChecks();
  IconGenerator::Settings s;
  s.m_blackBgCheck      = mask & ToonzCheck::eBlackBg;
  s.m_transparencyCheck = mask & ToonzCheck::eTransparency;
  s.m_inksOnly          = mask & ToonzCheck::eInksOnly;
  // emphasize lines with style#1 regardless of the current style
  if (mask & ToonzCheck::eInk1) s.m_inkIndex = 1;
  // emphasize lines with the current style
  else if (mask & ToonzCheck::eInk)
    s.m_inkIndex = tc->getColorIndex();
  else
    s.m_inkIndex = -1;
  s.m_paintIndex = mask & ToonzCheck::ePaint ? tc->getColorIndex() : -1;
  IconGenerator::instance()->setSettings(s);

  // Do not remove icons here as they will be re-used for updating icons in the
  // level strip

  // emit sceneChanged WITHOUT dirty flag
  TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  TApp::instance()->getCurrentLevel()->notifyLevelViewChange();
}

//--------------------------------------------------------------

static void executeCheck(int checkType) {
  ToonzCheck::instance()->toggleCheck(checkType);
  invalidateIcons();
}

//-----------------------------------------------------------------------------

class TCheckToggleCommand final : public MenuItemHandler {
public:
  TCheckToggleCommand() : MenuItemHandler("MI_TCheck") {}
  void execute() override { executeCheck(ToonzCheck::eTransparency); }
} tcheckToggle;

//-----------------------------------------------------------------------------

class ICheckToggleCommand final : public MenuItemHandler {
public:
  ICheckToggleCommand() : MenuItemHandler("MI_ICheck") {}
  void execute() override { executeCheck(ToonzCheck::eInk); }
} icheckToggle;

//-----------------------------------------------------------------------------

class PCheckToggleCommand final : public MenuItemHandler {
public:
  PCheckToggleCommand() : MenuItemHandler("MI_PCheck") {}
  void execute() override { executeCheck(ToonzCheck::ePaint); }
} pcheckToggle;

//-----------------------------------------------------------------------------

class BCheckToggleCommand final : public MenuItemHandler {
public:
  BCheckToggleCommand() : MenuItemHandler("MI_BCheck") {}
  void execute() override { executeCheck(ToonzCheck::eBlackBg); }
} bcheckToggle;

//-----------------------------------------------------------------------------

class TAutocloseToggleCommand final : public MenuItemHandler {
public:
  TAutocloseToggleCommand() : MenuItemHandler("MI_ACheck") {}
  void execute() override { executeCheck(ToonzCheck::eAutoclose); }
} tautocloseToggle;

//-----------------------------------------------------------------------------

class TGapToggleCommand final : public MenuItemHandler {
public:
  TGapToggleCommand() : MenuItemHandler("MI_GCheck") {}
  void execute() override { executeCheck(ToonzCheck::eGap); }
} tgapToggle;

//-----------------------------------------------------------------------------

class TInksOnlyToggleCommand final : public MenuItemHandler {
public:
  TInksOnlyToggleCommand() : MenuItemHandler("MI_IOnly") {}
  void execute() override { executeCheck(ToonzCheck::eInksOnly); }
} tinksOnlyToggle;

//-----------------------------------------------------------------------------
/*! emphasize lines with style#1 regardless of the current style
*/
class Ink1CheckToggleCommand final : public MenuItemHandler {
public:
  Ink1CheckToggleCommand() : MenuItemHandler("MI_Ink1Check") {}
  void execute() override { executeCheck(ToonzCheck::eInk1); }
} ink1checkToggle;

//=============================================================================

class TShiftTraceToggleCommand final : public MenuItemHandler {
  CommandId m_cmdId;

public:
  TShiftTraceToggleCommand(CommandId cmdId)
      : MenuItemHandler(cmdId), m_cmdId(cmdId) {}
  void execute() override {
    CommandManager *cm = CommandManager::instance();
    QAction *action    = cm->getAction(m_cmdId);
    bool checked       = action->isChecked();
    if (std::string(m_cmdId) == MI_ShiftTrace) {
      cm->enable(MI_EditShift, checked);
      cm->enable(MI_NoShift, checked);
      if (!checked) {
        cm->setChecked(MI_EditShift, false);
      }
      //     cm->getAction(MI_NoShift)->setChecked(false);
    } else if (std::string(m_cmdId) == MI_EditShift) {
      if (checked) {
        QAction *noShiftAction =
            CommandManager::instance()->getAction(MI_NoShift);
        if (noShiftAction) noShiftAction->setChecked(false);
        TApp::instance()->getCurrentTool()->setPseudoTool("T_ShiftTrace");
      } else {
        TApp::instance()->getCurrentTool()->unsetPseudoTool();
      }
      CommandManager::instance()->enable(MI_NoShift, !checked);
    } else if (std::string(m_cmdId) == MI_NoShift) {
    }
    updateShiftTraceStatus();
  }

  bool isChecked(CommandId id) const {
    QAction *action = CommandManager::instance()->getAction(id);
    return action != 0 && action->isChecked();
  }
  void updateShiftTraceStatus() {
    OnionSkinMask::ShiftTraceStatus status = OnionSkinMask::DISABLED;
    if (isChecked(MI_ShiftTrace)) {
      if (isChecked(MI_EditShift))
        status = OnionSkinMask::EDITING_GHOST;
      else if (isChecked(MI_NoShift))
        status = OnionSkinMask::ENABLED_WITHOUT_GHOST_MOVEMENTS;
      else
        status = OnionSkinMask::ENABLED;
    }
    OnionSkinMask osm =
        TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
    osm.setShiftTraceStatus(status);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
  }
};

TShiftTraceToggleCommand shiftTraceToggleCommand(MI_ShiftTrace),
    editShiftToggleCommand(MI_EditShift), noShiftToggleCommand(MI_NoShift);

class TResetShiftTraceCommand final : public MenuItemHandler {
public:
  TResetShiftTraceCommand() : MenuItemHandler(MI_ResetShift) {}
  void execute() override {
    OnionSkinMask osm =
        TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
    osm.setShiftTraceGhostCenter(0, TPointD());
    osm.setShiftTraceGhostCenter(1, TPointD());
    osm.setShiftTraceGhostAff(0, TAffine());
    osm.setShiftTraceGhostAff(1, TAffine());
    TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TTool *tool = TApp::instance()->getCurrentTool()->getTool();
    if (tool) tool->reset();
  }
} resetShiftTraceCommand;

//=============================================================================
// SceneViewer
//-----------------------------------------------------------------------------

SceneViewer::SceneViewer(ImageUtils::FullScreenWidget *parent)
    : GLWidgetForHighDpi(parent, touchProxy())
    , m_pressure(0)
    , m_lastMousePos(0, 0)
    , m_mouseButton(Qt::NoButton)
    , m_foregroundDrawing(false)
    , m_tabletEvent(false)
    , m_tabletActive(false)
    , m_tabletMove(false)
    , m_tabletPressed(false)
    , m_tabletReleased(false)
    , m_buttonClicked(false)
    , m_referenceMode(NORMAL_REFERENCE)
    , m_previewMode(NO_PREVIEW)
    , m_isMouseEntered(false)
    , m_forceGlFlush(true)
    , m_freezedStatus(NO_FREEZED)
    , m_viewGrabImage(0)
    , m_FPS(0)
    , m_hRuler(0)
    , m_vRuler(0)
    , m_viewMode(SCENE_VIEWMODE)
    , m_pos(0, 0)
    , m_pan3D(TPointD(0, 0))
    , m_zoomScale3D(0.1)
    , m_theta3D(20)
    , m_phi3D(30)
    , m_dpiScale(TPointD(1, 1))
    , m_compareSettings()
    , m_minZ(0)
    , m_tableDLId(-1)
    , m_groupIndexToBeEntered(-1)
    , m_pixelSize(1)
    , m_eraserPointerOn(false)
    , m_backupTool("")
    , m_clipRect()
    , m_isPicking(false)
    , m_current3DDevice(NONE)
    , m_sideRasterPos()
    , m_topRasterPos()
    , m_toolDisableReason("")
    , m_editPreviewSubCamera(false)
    , m_locator(NULL)
    , m_isLocator(false)
    , m_isBusyOnTabletMove(false) {
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  // Enables multiple key input.
  setAttribute(Qt::WA_KeyCompression);
  // Enables input methods for Asian languages.
  setAttribute(Qt::WA_InputMethodEnabled);
  setFocusPolicy(Qt::StrongFocus);
  setAcceptDrops(true);
  this->setMouseTracking(true);
  // to be introduced from Qt 5.9
  // this->setTabletTracking(true);

  for (int i = 0; i < tArrayCount(m_viewAff); i++)
    setViewMatrix(getNormalZoomScale(), i);

  m_3DSideR = rasterFromQPixmap(svgToPixmap(":Resources/3Dside_r.svg"));
  m_3DSideL = rasterFromQPixmap(svgToPixmap(":Resources/3Dside_l.svg"));
  m_3DTop   = rasterFromQPixmap(svgToPixmap(":Resources/3Dtop.svg"));

  makeCurrent();
  TGlContext context(tglGetCurrentContext());
  doneCurrent();

  TGLDisplayListsManager::instance()->attachContext(l_displayListsSpaceId,
                                                    context);
  l_contexts.insert(context);

  // iwsw commented out temporarily
  // if (Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() &&
  // Ghibli3DLutUtil::m_isValid)
  //  m_ghibli3DLutUtil = new Ghibli3DLutUtil();
}

//-----------------------------------------------------------------------------

void SceneViewer::setVisual(const ImagePainter::VisualSettings &settings) {
  // m_visualSettings.m_blankColor = settings.m_blankColor;//for the blank
  // frames, I don't have to repaint the viewer are using updateGl!
  bool repaint     = m_visualSettings.needRepaint(settings);
  m_visualSettings = settings;
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();
  if (repaint) update();
}

//-----------------------------------------------------------------------------

SceneViewer::~SceneViewer() {
  // Due to a BUG on some old driver we've tested, it's necessary to remove the
  // proxy first,
  // before any QGLWidget which shares the context gets killed in turn. The bug
  // caused a crash
  // when the destruction order of the SceneViewer was inverted with respect to
  // the (shared) creation.
  if (l_proxy) {
    std::set<TGlContext>::iterator ct, cEnd(l_contexts.end());
    for (ct = l_contexts.begin(); ct != cEnd; ++ct)
      TGLDisplayListsManager::instance()->releaseContext(*ct);
    assert(!l_proxy);
  }

  makeCurrent();
  if (m_tableDLId != -1) glDeleteLists(m_tableDLId, 1);
  doneCurrent();

  // iwsw commented out temporarily
  /*
if (m_ghibli3DLutUtil)
{
    m_ghibli3DLutUtil->onEnd();
    delete m_ghibli3DLutUtil;
}
*/
}

//-------------------------------------------------------------------------------

// Builds the view area, in camera reference
TRectD SceneViewer::getPreviewRect() const {
  TApp *app               = TApp::instance();
  int row                 = app->getCurrentFrame()->getFrame();
  TXsheet *xsh            = app->getCurrentXsheet()->getXsheet();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  double cameraZ          = xsh->getZ(cameraId, row);
  TAffine cameraAff =
      xsh->getPlacement(cameraId, row) * TScale((1000 + cameraZ) / 1000);

  TDimensionD cameraSize =
      app->getCurrentScene()->getScene()->getCurrentCamera()->getSize();
  TDimension cameraRes =
      app->getCurrentScene()->getScene()->getCurrentCamera()->getRes();
  TScale cameraScale(Stage::inch * cameraSize.lx / cameraRes.lx,
                     Stage::inch * cameraSize.ly / cameraRes.ly);

  TRectD viewRect(-width() * 0.5, -height() * 0.5, width() * 0.5,
                  height() * 0.5);

  return (getViewMatrix() * cameraAff * cameraScale).inv() * viewRect;
}

//-------------------------------------------------------------------------------

void SceneViewer::onRenderStarted(int frame) { emit previewStatusChanged(); }

//-------------------------------------------------------------------------------

void SceneViewer::onRenderCompleted(int frame) {
  invalidateAll();
  emit previewStatusChanged();
}

//-------------------------------------------------------------------------------

void SceneViewer::onPreviewUpdate() {
  updateGL();
  emit previewStatusChanged();
}

//-----------------------------------------------------------------------------

void SceneViewer::startForegroundDrawing() {
  makeCurrent();
  setAutoBufferSwap(false);
  glPushMatrix();
  tglMultMatrix(getViewMatrix());

  if (is3DView()) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();
    mult3DMatrix();
  }

  glDrawBuffer(GL_FRONT);

  assert(glGetError() == GL_NO_ERROR);
  m_foregroundDrawing = true;
}

//-----------------------------------------------------------------------------

void SceneViewer::endForegroundDrawing() {
  makeCurrent();
  glFlush();
  glDrawBuffer(GL_BACK);
  glPopMatrix();

  if (is3DView()) {
    glDisable(GL_DEPTH_TEST);
    glPopMatrix();
    assert(glGetError() == GL_NO_ERROR);
  }

  setAutoBufferSwap(true);
  m_foregroundDrawing = false;
}

//-----------------------------------------------------------------------------

void SceneViewer::setReferenceMode(int referenceMode) {
  if (m_referenceMode == referenceMode) return;

  TApp *app = TApp::instance();
  if (app->getCurrentFrame()->isEditingLevel())
    app->getCurrentFrame()->setFrame(app->getCurrentFrame()->getFrame());
  if (m_freezedStatus != NO_FREEZED) {
    freeze(false);
    emit freezeStateChanged(false);
  }

  m_referenceMode = referenceMode;
  invalidateAll();
  emit onZoomChanged();
}

//-------------------------------------------------------------------------------

void SceneViewer::freeze(bool on) {
  if (!on) {
    m_viewGrabImage = TRaster32P();
    m_freezedStatus = NO_FREEZED;
  } else {
    setCursor(Qt::ForbiddenCursor);
    m_freezedStatus = UPDATE_FREEZED;
  }
  update();
}

//-------------------------------------------------------------------------------

void SceneViewer::enablePreview(int previewMode) {
  if (m_previewMode == previewMode) return;

  TApp *app = TApp::instance();
  if (app->getCurrentFrame()->isEditingLevel() && previewMode != NO_PREVIEW)
    app->getCurrentFrame()->setFrame(app->getCurrentFrame()->getFrame());

  if (m_freezedStatus != NO_FREEZED) {
    freeze(false);
    emit freezeStateChanged(false);
  }

  if (m_previewMode != NO_PREVIEW)
    Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW)
        ->removeListener(this);

  // Schedule as a listener to Previewer.
  if (previewMode != NO_PREVIEW) {
    Previewer *previewer =
        Previewer::instance(previewMode == SUBCAMERA_PREVIEW);
    previewer->addListener(this);
    previewer->update();
  }

  m_previewMode = previewMode;

  update();

  // for updating the title bar
  emit previewToggled();
}

//-----------------------------------------------------------------------------

TPointD SceneViewer::winToWorld(const QPoint &pos) const {
  // coordinate window (origine in alto a sinistra) -> coordinate colonna
  // (origine al centro dell'immagine)
  TPointD pp(pos.x() - width() / 2, -pos.y() + height() / 2);
  if (is3DView()) {
    TXsheet *xsh            = TApp::instance()->getCurrentXsheet()->getXsheet();
    TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
    double z                = xsh->getStageObject(cameraId)->getZ(
        TApp::instance()->getCurrentFrame()->getFrame());

    TPointD p(pp.x - m_pan3D.x, pp.y - m_pan3D.y);
    p               = p * (1 / m_zoomScale3D);
    double theta    = m_theta3D * M_PI_180;
    double phi      = m_phi3D * M_PI_180;
    double cs_phi   = cos(phi);
    double sn_phi   = sin(phi);
    double cs_theta = cos(theta);
    double sn_theta = sin(theta);
    TPointD a(cs_phi, sn_theta * sn_phi);   // proiezione di (1,0,0)
    TPointD b(0, cs_theta);                 // proiezione di (0,1,0)
    TPointD c(sn_phi, -sn_theta * cs_phi);  // proiezione di (0,0,1)
    TPointD aa = rotate90(a);
    TPointD bb = rotate90(b);

    double abb = a * bb;
    double baa = b * aa;
    if (fabs(abb) > 0.001 && fabs(baa) > 0.001) {
      p -= c * z;
      TPointD g((p * bb) / (a * bb), (p * aa) / (b * aa));
      return TAffine() * g;
    } else
      return TAffine() * TPointD(0, 0);
  }

  return getViewMatrix().inv() * pp;
}

//-----------------------------------------------------------------------------

TPointD SceneViewer::winToWorld(const TPoint &winPos) const {
  return winToWorld(QPoint(winPos.x, height() - winPos.y));
}

//-----------------------------------------------------------------------------

TPoint SceneViewer::worldToPos(const TPointD &worldPos) const {
  TPointD p = getViewMatrix() * worldPos;
  return TPoint(width() / 2 + p.x, height() / 2 + p.y);
}

//-----------------------------------------------------------------------------

void SceneViewer::showEvent(QShowEvent *) {
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();

  // Se il viewer e' show e il preview e' attivo aggiungo il listner al preview
  if (m_previewMode != NO_PREVIEW)
    Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW)->addListener(this);

  TApp *app = TApp::instance();

  TSceneHandle *sceneHandle = app->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneSwitched()), this, SLOT(resetSceneViewer()));
  connect(sceneHandle, SIGNAL(sceneChanged()), this, SLOT(onSceneChanged()));

  TFrameHandle *frameHandle = app->getCurrentFrame();
  connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(onFrameSwitched()));

  TPaletteHandle *paletteHandle =
      app->getPaletteController()->getCurrentLevelPalette();
  connect(paletteHandle, SIGNAL(colorStyleChanged()), this, SLOT(update()));

  connect(app->getCurrentObject(), SIGNAL(objectSwitched()), this,
          SLOT(onObjectSwitched()));
  connect(app->getCurrentObject(), SIGNAL(objectChanged(bool)), this,
          SLOT(update()));

  connect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
          SLOT(update()));

  connect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()), this,
          SLOT(update()));
  connect(app->getCurrentLevel(), SIGNAL(xshCanvasSizeChanged()), this,
          SLOT(update()));
  // when level is switched, update m_dpiScale in order to show white background
  // for Ink&Paint work properly
  connect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(onLevelSwitched()));

  connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), this,
          SLOT(onXsheetChanged()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()), this,
          SLOT(update()));

  // update tooltip when tool options are changed
  connect(app->getCurrentTool(), SIGNAL(toolChanged()), this,
          SLOT(onToolChanged()));
  connect(app->getCurrentTool(), SIGNAL(toolCursorTypeChanged()), this,
          SLOT(onToolChanged()));

  connect(app, SIGNAL(tabletLeft()), this, SLOT(resetTabletStatus()));

  if (m_hRuler && m_vRuler) {
    if (!viewRulerToggle.getStatus()) {
      m_hRuler->hide();
      m_vRuler->hide();
    } else {
      m_hRuler->show();
      m_vRuler->show();
    }
  }
  if (m_shownOnce == false) {
    fitToCamera();
    m_shownOnce = true;
  }
}

//-----------------------------------------------------------------------------

void SceneViewer::hideEvent(QHideEvent *) {
  // Se il viewer e' hide e il preview e' attivo rimuovo il listner dal preview
  if (m_previewMode != NO_PREVIEW)
    Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW)
        ->removeListener(this);

  TApp *app = TApp::instance();

  TSceneHandle *sceneHandle = app->getCurrentScene();
  if (sceneHandle) sceneHandle->disconnect(this);

  TFrameHandle *frameHandle = app->getCurrentFrame();
  if (frameHandle) frameHandle->disconnect(this);

  TPaletteHandle *paletteHandle =
      app->getPaletteController()->getCurrentLevelPalette();
  if (paletteHandle) paletteHandle->disconnect(this);

  TObjectHandle *objectHandle = app->getCurrentObject();
  if (objectHandle) objectHandle->disconnect(this);

  TOnionSkinMaskHandle *onionHandle = app->getCurrentOnionSkin();
  if (onionHandle) onionHandle->disconnect(this);

  TXshLevelHandle *levelHandle = app->getCurrentLevel();
  if (levelHandle) levelHandle->disconnect(this);

  TXsheetHandle *xsheetHandle = app->getCurrentXsheet();
  if (xsheetHandle) xsheetHandle->disconnect(this);

  ToolHandle *toolHandle = app->getCurrentTool();
  if (toolHandle) toolHandle->disconnect(this);

  disconnect(app, SIGNAL(tabletLeft()), this, SLOT(resetTabletStatus()));
  // hide locator
  if (m_locator && m_locator->isVisible()) m_locator->hide();
}

int SceneViewer::getVGuideCount() {
  if (viewGuideToggle.getStatus())
    return m_vRuler->getGuideCount();
  else
    return 0;
}
int SceneViewer::getHGuideCount() {
  if (viewGuideToggle.getStatus())
    return m_hRuler->getGuideCount();
  else
    return 0;
}

double SceneViewer::getVGuide(int index) { return m_vRuler->getGuide(index); }
double SceneViewer::getHGuide(int index) { return m_hRuler->getGuide(index); }

//-----------------------------------------------------------------------------

void SceneViewer::initializeGL() {
  // glClearColor(1.0,1.0,1.0,1);
  glClear(GL_COLOR_BUFFER_BIT);

  // iwsw commented out temporarily
  // if (Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() &&
  // m_ghibli3DLutUtil)
  //	  m_ghibli3DLutUtil->onInit();
}

//-----------------------------------------------------------------------------

void SceneViewer::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  glOrtho(0, w, 0, h, -4000, 4000);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0.375, 0.375, 0.0);
  // make the center of the viewer = origin
  glTranslated(w * 0.5, h * 0.5, 0);

  if (m_freezedStatus == NORMAL_FREEZED) m_freezedStatus = UPDATE_FREEZED;

  if (m_previewMode != NO_PREVIEW) requestTimedRefresh();

  // for updating the navigator in levelstrip
  emit refreshNavi();
  // iwsw commented out temporarily
  // if (Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() &&
  // m_ghibli3DLutUtil)
  //	m_ghibli3DLutUtil->onResize(w, h);
}

//-----------------------------------------------------------------------------

void SceneViewer::drawBuildVars() {
  TApp *app = TApp::instance();

  int frame    = app->getCurrentFrame()->getFrame();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();

  // Camera affine
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera    = xsh->getStageObject(cameraId);
  TAffine cameraPlacement = camera->getPlacement(frame);
  double cameraZ          = camera->getZ(frame);

  m_drawCameraAff =
      getViewMatrix() * cameraPlacement * TScale((1000 + cameraZ) / 1000);

  // Table affine
  TStageObject *table    = xsh->getStageObject(TStageObjectId::TableId);
  TAffine tablePlacement = table->getPlacement(frame);
  double tableZ          = table->getZ(frame);
  TAffine placement;

  m_drawIsTableVisible = TStageObject::perspective(
      placement, cameraPlacement, cameraZ, tablePlacement, tableZ, 0);
  m_drawTableAff = getViewMatrix() * tablePlacement;

  // Camera test check
  m_drawCameraTest = CameraTestCheck::instance()->isEnabled();

  if (m_previewMode == NO_PREVIEW) {
    m_drawEditingLevel = app->getCurrentFrame()->isEditingLevel();
    m_viewMode         = m_drawEditingLevel ? LEVEL_VIEWMODE : SCENE_VIEWMODE;
    m_draw3DMode       = is3DView() && (m_previewMode != SUBCAMERA_PREVIEW);
  } else {
    m_drawEditingLevel = false;
    m_viewMode         = app->getCurrentFrame()->isEditingLevel();
    m_draw3DMode       = false;
  }

  // Clip rect
  if (!m_clipRect.isEmpty() && !m_draw3DMode) {
    m_actualClipRect = getActualClipRect(getViewMatrix());
    m_actualClipRect += TPoint(width() * 0.5, height() * 0.5);
  }

  TTool *tool = app->getCurrentTool()->getTool();
  if (tool && !m_isLocator) tool->setViewer(this);
}

//-----------------------------------------------------------------------------

void SceneViewer::drawEnableScissor() {
  if (!m_clipRect.isEmpty() && !m_draw3DMode) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(m_actualClipRect.x0, m_actualClipRect.y0,
              m_actualClipRect.getLx(), m_actualClipRect.getLy());
  }
}

//-----------------------------------------------------------------------------

void SceneViewer::drawDisableScissor() {
  if (!m_clipRect.isEmpty() && !m_draw3DMode) glDisable(GL_SCISSOR_TEST);
}

//-----------------------------------------------------------------------------

void SceneViewer::drawBackground() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  if (m_visualSettings.m_colorMask == 0) {
    TPixel32 bgColor;

    if (isPreviewEnabled())
      bgColor = Preferences::instance()->getPreviewBgColor();
    else
      bgColor = Preferences::instance()->getViewerBgColor();
    glClearColor(bgColor.r / 255.0f, bgColor.g / 255.0f, bgColor.b / 255.0f,
                 1.0);
  } else
    glClearColor(0, 0, 0, 1.0);

  glClear(GL_COLOR_BUFFER_BIT);
  if (glGetError() == GL_INVALID_FRAMEBUFFER_OPERATION) {
    /* 起動時一回目になぜか GL_FRAMEBUFFER_COMPLETE なのに invalid operation
     * が出る  */
    GLenum status = 0;
#ifdef _WIN32
    PROC proc = wglGetProcAddress("glCheckFramebufferStatusEXT");
    if (proc != nullptr)
      status = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC>(proc)(
          GL_FRAMEBUFFER);
#else
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
#endif
    printf("GL_INVALID_FRAMEBUFFER_OPERATION: framebuffer:%d\n", status);
  }
}

static bool check_framebuffer_status() {
#ifdef _WIN32
  PROC proc = wglGetProcAddress("glCheckFramebufferStatusEXT");
  if (proc == nullptr) return true;
  GLenum s = reinterpret_cast<PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC>(proc)(
      GL_FRAMEBUFFER);
#else
  GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
#endif
  if (s == GL_FRAMEBUFFER_UNDEFINED)
    printf("Warning: FB undefined: %d\n", s);
  else if (s == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
    printf("Warning: FB incomplete(attachment): %d\n", s);
  else if (s == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
    printf("Warning: FB incomplete(missing attachment): %d\n", s);
  else if (s == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
    printf("Warning: FB incomplete(draw buffer): %d\n", s);
  else if (s == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
    printf("Warning: FB incomplete(read buffer): %d\n", s);
  else if (s == GL_FRAMEBUFFER_UNSUPPORTED)
    printf("Warning: FB unsupported: %d\n", s);
  else if (s == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
    printf("Warning: FB incomplete(multi-sample): %d\n", s);
  else if (s == GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
    printf("Warning: FB incomplete(multi-sample): %d\n", s);
  else if (s == GL_FRAMEBUFFER_COMPLETE)
    ;
  else
    printf("Warning: FB not complete(unknown cause): %d\n", s);
  return s == GL_FRAMEBUFFER_COMPLETE;
}

//-----------------------------------------------------------------------------

void SceneViewer::drawCameraStand() {
  GLint e = glGetError();
  check_framebuffer_status();
  assert(e == GL_NO_ERROR);

  TXshSimpleLevel::m_rasterizePli = rasterizePliToggle.getStatus();

  // clear
  if (m_draw3DMode) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glClear(GL_DEPTH_BUFFER_BIT);
    glPushMatrix();
    mult3DMatrix();
  } else
    glDisable(GL_DEPTH_TEST);

  assert(glGetError() == GL_NO_ERROR);

  // draw table
  if (!m_draw3DMode && viewTableToggle.getStatus() && m_drawIsTableVisible &&
      m_visualSettings.m_colorMask == 0 && m_drawEditingLevel == false &&
      !m_drawCameraTest) {
    glPushMatrix();
    tglMultMatrix(m_drawTableAff);
    ViewerDraw::drawDisk(m_tableDLId);
    glPopMatrix();
  }

  // draw colorcard (with camera BG color)
  // Hide camera BG when level editing mode.
  if (m_drawEditingLevel == false && viewClcToggle.getStatus() &&
      !m_drawCameraTest) {
    glPushMatrix();
    tglMultMatrix(m_drawCameraAff);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ViewerDraw::drawColorcard(m_visualSettings.m_colorMask);
    glDisable(GL_BLEND);
    glPopMatrix();
  }

  // Show white background when level editing mode.
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (m_drawEditingLevel && tool && tool->isEnabled()) {
    if (!m_isLocator) tool->setViewer(this);
    glPushMatrix();
    if (m_referenceMode == CAMERA3D_REFERENCE) {
      mult3DMatrix();
      tglMultMatrix(tool->getMatrix());
    } else {
      tglMultMatrix(getViewMatrix() * tool->getMatrix());
    }
    glScaled(m_dpiScale.x, m_dpiScale.y, 1);

    TImageP image = tool->getImage(false);

    TToonzImageP ti = image;
    if (ti) {
      TRect imgRect(0, 0, ti->getSize().lx - 1, ti->getSize().ly - 1);
      TRectD bbox = ToonzImageUtils::convertRasterToWorld(imgRect, ti);

      TPixel32 imgRectColor;
      // draw black rectangle instead, if the BlackBG check is ON
      if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
        imgRectColor = TPixel::Black;
      else
        imgRectColor = TPixel::White;
      ToolUtils::fillRect(bbox * ti->getSubsampling(), imgRectColor);
    }
    glPopMatrix();
  }

  // draw 3dframe
  if (m_draw3DMode) {
    glDepthFunc(GL_LEQUAL);
    ViewerDraw::draw3DFrame(m_minZ, m_phi3D);
    glDepthFunc(GL_ALWAYS);
  }

  // draw scene
  assert(glGetError() == GL_NO_ERROR);
  drawScene();
  assert((glGetError()) == GL_NO_ERROR);
}

//-----------------------------------------------------------------------------

void SceneViewer::drawPreview() {
  const double inch   = Stage::inch;
  TApp *app           = TApp::instance();
  int row             = app->getCurrentFrame()->getFrame();
  TCamera *currCamera = app->getCurrentScene()->getScene()->getCurrentCamera();
  TDimensionD cameraSize = currCamera->getSize();

  Previewer *previewer =
      Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW);

  TRasterP ras =
      previewer->getRaster(row, m_visualSettings.m_recomputeIfNeeded);
  if (ras) {
    TRectD previewStageRectD, cameraStageRectD = currCamera->getStageRect();

    TRect subCameraRect(currCamera->getInterestRect());
    if (m_previewMode == SUBCAMERA_PREVIEW && subCameraRect.getLx() > 0 &&
        subCameraRect.getLy() > 0)
      previewStageRectD = currCamera->getInterestStageRect();
    else
      previewStageRectD = cameraStageRectD;

    TAffine rasterToStageRef(
        previewStageRectD.getLx() / ras->getLx(), 0.0,
        previewStageRectD.x0 + 0.5 * previewStageRectD.getLx(), 0.0,
        previewStageRectD.getLy() / ras->getLy(),
        previewStageRectD.y0 + 0.5 * previewStageRectD.getLy());

    TDimension dim(width(), height());
    TAffine finalAff              = m_drawCameraAff * rasterToStageRef;
    m_visualSettings.m_useTexture = !Preferences::instance()->useDrawPixel();
    ImagePainter::paintImage(TRasterImageP(ras), ras->getSize(), dim, finalAff,
                             m_visualSettings, m_compareSettings, TRect());
  }

  glPushMatrix();
  tglMultMatrix(m_drawCameraAff);

  TRectD frameRect(cameraSize);
  frameRect.x1 *= inch;
  frameRect.y1 *= inch;
  frameRect -= 0.5 * (frameRect.getP00() + frameRect.getP11());

  if (m_visualSettings.m_blankColor != TPixel::Transparent) {
    tglColor(m_visualSettings.m_blankColor);
    tglFillRect(frameRect);
  }

  if (!previewer->isFrameReady(row) ||
      app->getCurrentFrame()->isPlaying() && previewer->isBusy()) {
    glColor3d(1, 0, 0);

    tglDrawRect(frameRect);
    tglDrawRect(frameRect.enlarge(5));
  }

  glPopMatrix();
}

//-----------------------------------------------------------------------------

void SceneViewer::drawOverlay() {
  TApp *app = TApp::instance();

  // draw camera mask
  if (m_referenceMode == CAMERA_REFERENCE && !m_drawCameraTest) {
    glPushMatrix();
    tglMultMatrix(m_drawCameraAff);
    ViewerDraw::drawCameraMask(this);
    glPopMatrix();
  }

  // draw FieldGuide
  if (fieldGuideToggle.getStatus()) {
    glPushMatrix();
    tglMultMatrix(m_drawTableAff);
    ViewerDraw::drawFieldGuide();
    glPopMatrix();
  }

  if (!m_drawCameraTest) {
    // draw grid & guides
    if (viewGuideToggle.getStatus() &&
        (m_vRuler && m_vRuler->getGuideCount() ||
         m_hRuler && m_hRuler->getGuideCount())) {
      glPushMatrix();
      tglMultMatrix(getViewMatrix());
      ViewerDraw::drawGridAndGuides(
          this, (m_draw3DMode) ? m_zoomScale3D : m_viewAff[m_viewMode].det(),
          m_vRuler, m_hRuler, false);
      glPopMatrix();
    }

    // draw camera
    if (viewCameraToggle.getStatus() && m_drawEditingLevel == false) {
      unsigned long f = 0;
      if (m_referenceMode == CAMERA_REFERENCE)
        f |= ViewerDraw::CAMERA_REFERENCE;
      if (m_draw3DMode) f |= ViewerDraw::CAMERA_3D;
      if (m_previewMode == SUBCAMERA_PREVIEW || m_editPreviewSubCamera)
        f |= ViewerDraw::SUBCAMERA;
      if (m_draw3DMode)
        ViewerDraw::draw3DCamera(f, m_minZ, m_phi3D);
      else {
        glPushMatrix();
        tglMultMatrix(m_drawCameraAff);
        m_pixelSize = sqrt(tglGetPixelSize2()) * getDevPixRatio();
        ViewerDraw::drawCamera(f, m_pixelSize);
        glPopMatrix();
      }
    }

    // safe area
    if (safeAreaToggle.getStatus() && m_drawEditingLevel == false &&
        !is3DView()) {
      glPushMatrix();
      tglMultMatrix(m_drawCameraAff);
      ViewerDraw::drawSafeArea();
      glPopMatrix();
    }

    // record fps (frame per second)
    if (app->getCurrentFrame()->isPlaying())
      m_FPS = getActualFrameRate();
    else
      m_FPS = 0;

    if (m_freezedStatus != NO_FREEZED) {
      tglColor(TPixel32::Red);
      tglDrawText(TPointD(0, 0), "FROZEN");
    }
    assert(glGetError() == GL_NO_ERROR);

  }  //! cameraTest

  // draw 3d Top/Side Buttons
  if (m_draw3DMode && !m_isPicking) {
    tglColor(TPixel32::Black);

    GLdouble modelView3D[16];
    GLdouble projection3D[16];
    GLint viewport3D[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, modelView3D);
    glGetDoublev(GL_PROJECTION_MATRIX, projection3D);
    glGetIntegerv(GL_VIEWPORT, viewport3D);

    if (m_phi3D > 0) {
      T3DPointD topRasterPos3D = computeNew3DPosition(
          T3DPointD(500, 500, 1000), TPointD(-10, -10), m_topRasterPos,
          modelView3D, projection3D, viewport3D, getDevPixRatio());
      glRasterPos3f(topRasterPos3D.x, topRasterPos3D.y, topRasterPos3D.z);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glDrawPixels(m_3DTop->getWrap(), m_3DTop->getLy(), TGL_FMT, TGL_TYPE,
                   m_3DTop->getRawData());

      T3DPointD sideRasterPos3D = computeNew3DPosition(
          T3DPointD(-500, -500, 1000), TPointD(-10, -10), m_sideRasterPos,
          modelView3D, projection3D, viewport3D, getDevPixRatio());
      glRasterPos3f(sideRasterPos3D.x, sideRasterPos3D.y, sideRasterPos3D.z);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glDrawPixels(m_3DSideR->getWrap(), m_3DSideR->getLy(), TGL_FMT, TGL_TYPE,
                   m_3DSideR->getRawData());
    } else {
      T3DPointD topRasterPos3D = computeNew3DPosition(
          T3DPointD(-500, 500, 1000), TPointD(-10, -10), m_topRasterPos,
          modelView3D, projection3D, viewport3D, getDevPixRatio());
      glRasterPos3f(topRasterPos3D.x, topRasterPos3D.y, topRasterPos3D.z);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glDrawPixels(m_3DTop->getWrap(), m_3DTop->getLy(), TGL_FMT, TGL_TYPE,
                   m_3DTop->getRawData());

      T3DPointD sideRasterPos3D = computeNew3DPosition(
          T3DPointD(500, -500, 1000), TPointD(-10, -10), m_sideRasterPos,
          modelView3D, projection3D, viewport3D, getDevPixRatio());
      glRasterPos3f(sideRasterPos3D.x, sideRasterPos3D.y, sideRasterPos3D.z);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
      glDrawPixels(m_3DSideL->getWrap(), m_3DSideL->getLy(), TGL_FMT, TGL_TYPE,
                   m_3DSideL->getRawData());
    }
  }

  if (m_draw3DMode) {
    glDisable(GL_DEPTH_TEST);
    glPopMatrix();
    assert(glGetError() == GL_NO_ERROR);
  }

  // draw tool gadgets
  TTool *tool         = app->getCurrentTool()->getTool();
  TXshSimpleLevel *sl = app->getCurrentLevel()->getSimpleLevel();
  // Call tool->draw() even if the level is read only (i.e. to show hooks)
  if (tool && (tool->isEnabled() || (sl && sl->isReadOnly()))) {
    // tool->setViewer(this);                            // Moved at
    // drawBuildVars(), before drawing anything
    glPushMatrix();
    if (m_draw3DMode) {
      mult3DMatrix();
      tglMultMatrix(tool->getMatrix());
    } else
      tglMultMatrix(getViewMatrix() * tool->getMatrix());
    if (tool->getToolType() & TTool::LevelTool &&
        !app->getCurrentObject()->isSpline())
      glScaled(m_dpiScale.x, m_dpiScale.y, 1);
    m_pixelSize = sqrt(tglGetPixelSize2()) * getDevPixRatio();
    tool->draw();
    glPopMatrix();
    // Used (only in the T_RGBPicker tool) to notify and set the currentColor
    // outside the draw() methods:
    // using special style there was a conflict between the draw() methods of
    // the tool
    // and the genaration of the icon inside the style editor (makeIcon()) which
    // use
    // another glContext
    if (tool->getName() == "T_RGBPicker") tool->onImageChanged();

    // draw cross at the center of the locator window
    if (m_isLocator) {
      glColor3d(1.0, 0.0, 0.0);
      tglDrawSegment(TPointD(-4, 0), TPointD(5, 0));
      tglDrawSegment(TPointD(0, -4), TPointD(0, 5));
    }
  }
}

//-----------------------------------------------------------------------------

static void drawFpsGraph(int t0, int t1) {
  glDisable(GL_BLEND);
  static std::deque<std::pair<int, int>> times;
  times.push_back(std::make_pair(t0, t1));
  while (times.size() > 200) times.pop_front();
  double x0 = 10, y0 = 10;
  double x1 = x0 + 200;
  double y1 = y0 + 150;
  glPushMatrix();
  glLoadIdentity();
  glColor3d(0, 0, 0);
  glRectd(x0, y0, x1, y1);
  glColor3d(0, 0.5, 1);
  glBegin(GL_LINE_STRIP);
  glVertex2d(x0, y0);
  glVertex2d(x1, y0);
  glVertex2d(x1, y1);
  glVertex2d(x0, y1);
  glVertex2d(x0, y0);
  glEnd();
  glColor3d(0.5, 0.5, 0.5);
  glBegin(GL_LINES);
  for (int y = y0 + 5; y < y1; y += 20) {
    glVertex2d(x0, y);
    glVertex2d(x1, y);
  }
  for (int i = 0; i < (int)times.size(); i++) {
    double x = x1 - i;
    glColor3d(1, 0, 0);
    glVertex2d(x, y0);
    glVertex2d(x, y0 + 5 + times[i].first / 5);
    glColor3d(0, 1, 0);
    glVertex2d(x, y0 + 5 + times[i].first / 5);
    glVertex2d(x, y0 + 5 + times[i].second / 5);
  }
  glEnd();
  glPopMatrix();
}

//-----------------------------------------------------------------------------

class Qt_GLContextManager final : public TGLContextManager {
  QGLContext *m_context;

public:
  Qt_GLContextManager() : m_context(0) {}
  void store() override {
    // m_context = const_cast<QGLContext *>(QGLContext::currentContext());
  }
  void store(QGLContext *context) { m_context = context; }
  void restore() override {
    if (m_context) m_context->makeCurrent();
  }
};

//-----------------------------------------------------------------------------

//#define FPS_HISTOGRAM

void SceneViewer::paintGL() {
  if (!check_framebuffer_status()) {
    /* QGLWidget の widget 生成/削除のタイミングで(platform によって?)
     * GL_FRAMEBUFFER_UNDEFINED の状態で paintGL() が呼ばれてしまうようだ */
    return;
  }
#ifdef WIN32
  // following line is necessary to solve a problem Windows 7
  SetWindowRgn((HWND)winId(), 0, FALSE);
#else
  // followin lines are necessary to solve a problem on iMac20
  // It seems that for some errors in the openGl implementation, buffers are not
  // set corretly.
  if (m_isMouseEntered && m_forceGlFlush) {
    m_isMouseEntered = false;
    m_forceGlFlush   = false;
    glDrawBuffer(GL_FRONT);
    glFlush();
    glDrawBuffer(GL_BACK);
  }
#endif

#ifdef FPS_HISTOGRAM
  QTime time;
  time.start();
#endif

  // iwsw commented out temporarily
  // if (!m_isPicking &&
  //	Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() &&
  // m_ghibli3DLutUtil)
  //	m_ghibli3DLutUtil->startDraw();

  if (m_hRuler && m_vRuler) {
    if (!viewRulerToggle.getStatus() &&
        (m_hRuler->isVisible() || m_vRuler->isVisible())) {
      m_hRuler->hide();
      m_vRuler->hide();
    } else if (viewRulerToggle.getStatus() &&
               (!m_hRuler->isVisible() || !m_vRuler->isVisible())) {
      m_hRuler->show();
      m_vRuler->show();
    }
  }

  // Il freezed e' attivo ed e' in stato "normale": mostro l'immagine grabbata.
  if (m_freezedStatus == NORMAL_FREEZED) {
    assert(!!m_viewGrabImage);
    m_viewGrabImage->lock();
    glPushMatrix();
    glLoadIdentity();

    glRasterPos2d(0, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    glDrawPixels(m_viewGrabImage->getLx(), m_viewGrabImage->getLy(), TGL_FMT,
                 TGL_TYPE, m_viewGrabImage->getRawData());

    glPopMatrix();
    m_viewGrabImage->unlock();

    // iwsw commented out temporarily
    // if (!m_isPicking &&
    //	Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() &&
    // m_ghibli3DLutUtil)
    //	m_ghibli3DLutUtil->endDraw();

    return;
  }

  drawBuildVars();

  check_framebuffer_status();
  // iwsw commented out temporarily
  // if (!m_isPicking &&
  //	  !Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() ||
  //! m_ghibli3DLutUtil)
  copyFrontBufferToBackBuffer();
  check_framebuffer_status();
  drawEnableScissor();
  check_framebuffer_status();
  drawBackground();
  check_framebuffer_status();

  if (m_previewMode != FULL_PREVIEW) {
    check_framebuffer_status();
    drawCameraStand();
    check_framebuffer_status();
  }

  if (isPreviewEnabled()) drawPreview();

  drawOverlay();

  drawDisableScissor();

  // Il freezed e' attivo ed e' in stato "update": faccio il grab del viewer.
  if (m_freezedStatus == UPDATE_FREEZED) {
    m_viewGrabImage = rasterFromQImage(grabFrameBuffer(false));
    m_freezedStatus = NORMAL_FREEZED;
  }

#ifdef FPS_HISTOGRAM
  int t0 = time.elapsed();
  glFlush();
  glFinish();
  int t1 = time.elapsed();
  drawFpsGraph(t0, t1);
#endif
  // TOfflineGL::setContextManager(0);

  // iwsw commented out temporarily
  // if (!m_isPicking &&
  //	Preferences::instance()->isDoColorCorrectionByUsing3DLutEnabled() &&
  // m_ghibli3DLutUtil)
  //	m_ghibli3DLutUtil->endDraw();
}

//-----------------------------------------------------------------------------

void SceneViewer::drawScene() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  int frame         = app->getCurrentFrame()->getFrame();
  TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
  TRect clipRect    = getActualClipRect(getViewMatrix());
  clipRect += TPoint(width() * 0.5, height() * 0.5);

  ChildStack *childStack = scene->getChildStack();
  bool editInPlace =
      childStack->getEditInPlace() && !app->getCurrentFrame()->isEditingLevel();

  bool fillFullColorRaster = TXshSimpleLevel::m_fillFullColorRaster;
  TXshSimpleLevel::m_fillFullColorRaster = false;

  m_minZ = 0;
  if (is3DView()) {
    Stage::OpenGlPainter painter(getViewMatrix(), clipRect, m_visualSettings,
                                 true, false);
    painter.enableCamera3D(true);
    painter.setPhi(m_phi3D);
    int xsheetLevel = 0;
    std::pair<TXsheet *, int> xr;
    if (editInPlace) {
      xr          = childStack->getAncestor(frame);
      xsheetLevel = childStack->getAncestorCount();
    } else
      xr = std::make_pair(xsh, frame);

    Stage::VisitArgs args;
    args.m_scene       = scene;
    args.m_xsh         = xr.first;
    args.m_row         = xr.second;
    args.m_col         = app->getCurrentColumn()->getColumnIndex();
    OnionSkinMask osm  = app->getCurrentOnionSkin()->getOnionSkinMask();
    args.m_osm         = &osm;
    args.m_camera3d    = true;
    args.m_xsheetLevel = xsheetLevel;
    Stage::visit(painter, args);

    m_minZ = painter.getMinZ();
  } else {
    // camera 2D (normale)
    TDimension viewerSize(width(), height());

    TAffine viewAff = getViewMatrix();

    if (editInPlace) {
      TAffine aff;
      if (scene->getChildStack()->getAncestorAffine(aff, frame))
        viewAff = viewAff * aff.inv();
    }

    m_visualSettings.m_showBBox = viewBBoxToggle.getStatus();

    Stage::RasterPainter painter(viewerSize, viewAff, clipRect,
                                 m_visualSettings, true);

    // darken blended view mode for viewing the non-cleanuped and stacked
    // drawings
    painter.setRasterDarkenBlendedView(
        Preferences::instance()
            ->isShowRasterImagesDarkenBlendedInViewerEnabled());

    TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
    if (app->getCurrentFrame()->isEditingLevel()) {
      Stage::visit(painter, app->getCurrentLevel()->getLevel(),
                   app->getCurrentFrame()->getFid(),
                   app->getCurrentOnionSkin()->getOnionSkinMask(),
                   frameHandle->isPlaying());
    } else {
      std::pair<TXsheet *, int> xr;
      int xsheetLevel = 0;
      if (editInPlace) {
        xr          = scene->getChildStack()->getAncestor(frame);
        xsheetLevel = scene->getChildStack()->getAncestorCount();
      } else
        xr = std::make_pair(xsh, frame);

      Stage::VisitArgs args;
      args.m_scene       = scene;
      args.m_xsh         = xr.first;
      args.m_row         = xr.second;
      args.m_col         = app->getCurrentColumn()->getColumnIndex();
      OnionSkinMask osm  = app->getCurrentOnionSkin()->getOnionSkinMask();
      args.m_osm         = &osm;
      args.m_xsheetLevel = xsheetLevel;
      args.m_isPlaying   = frameHandle->isPlaying();
      Stage::visit(painter, args);
    }

    assert(glGetError() == 0);
    painter.flushRasterImages();

    TXshSimpleLevel::m_fillFullColorRaster = fillFullColorRaster;

    assert(glGetError() == 0);
    if (m_viewMode != LEVEL_VIEWMODE)
      drawSpline(getViewMatrix(), clipRect,
                 m_referenceMode == CAMERA3D_REFERENCE, m_pixelSize);
    assert(glGetError() == 0);
  }
}

//------------------------------------------------------------------------------

void SceneViewer::mult3DMatrix() {
  glTranslated(m_pan3D.x, m_pan3D.y, 0);
  glScaled(m_zoomScale3D, m_zoomScale3D, 1);
  glRotated(m_theta3D, 1, 0, 0);
  glRotated(m_phi3D, 0, 1, 0);
}

//-----------------------------------------------------------------------------

double SceneViewer::projectToZ(const TPoint &delta) {
  glPushMatrix();
  mult3DMatrix();
  GLint viewport[4];
  double modelview[16], projection[16];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glGetDoublev(GL_PROJECTION_MATRIX, projection);
  glGetDoublev(GL_MODELVIEW_MATRIX, modelview);

  double ax, ay, az, bx, by, bz;
  gluProject(0, 0, 0, modelview, projection, viewport, &ax, &ay, &az);
  gluProject(0, 0, 1, modelview, projection, viewport, &bx, &by, &bz);

  glPopMatrix();
  TPointD zdir(bx - ax, by - ay);
  double zdirLength2 = norm2(zdir);
  if (zdirLength2 > 0.0) {
    double dz = (TPointD(delta.x, delta.y) * zdir) / zdirLength2;
    return dz;
  } else
    return 0.0;
}

//-----------------------------------------------------------------------------

TRect SceneViewer::getActualClipRect(const TAffine &aff) {
  TDimension viewerSize(width(), height());
  TRect clipRect(viewerSize);

  if (is3DView()) {
    TPointD p00 = winToWorld(clipRect.getP00());
    TPointD p01 = winToWorld(clipRect.getP01());
    TPointD p10 = winToWorld(clipRect.getP10());
    TPointD p11 = winToWorld(clipRect.getP11());
    clipRect    = TRect(TPoint(std::min(p00.x, p01.x), std::min(p00.y, p10.y)),
                     TPoint(std::max(p11.x, p10.x), std::max(p11.y, p01.y)));
  } else if (m_clipRect.isEmpty())
    clipRect -= TPoint(viewerSize.lx / 2, viewerSize.ly / 2);
  else {
    TRectD app = aff * (m_clipRect.enlarge(3));
    clipRect =
        TRect(tceil(app.x0), tceil(app.y0), tfloor(app.x1), tfloor(app.y1));
  }

  return clipRect;
}

//-----------------------------------------------------------------------------

TAffine SceneViewer::getViewMatrix() const {
  int viewMode = TApp::instance()->getCurrentFrame()->isEditingLevel()
                     ? LEVEL_VIEWMODE
                     : SCENE_VIEWMODE;
  if (is3DView()) return TAffine();
  if (m_referenceMode == CAMERA_REFERENCE) {
    int frame    = TApp::instance()->getCurrentFrame()->getFrame();
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    TAffine aff  = xsh->getCameraAff(frame);
    return m_viewAff[viewMode] * aff.inv();
  } else
    return m_viewAff[viewMode];
}

//-----------------------------------------------------------------------------

TAffine SceneViewer::getSceneMatrix() const {
  int viewMode = TApp::instance()->getCurrentFrame()->isEditingLevel()
                     ? LEVEL_VIEWMODE
                     : SCENE_VIEWMODE;
  if (is3DView()) return TAffine();
  return m_viewAff[viewMode];
}

//-----------------------------------------------------------------------------

void SceneViewer::setViewMatrix(const TAffine &aff, int viewMode) {
  m_viewAff[viewMode] = aff;

  // In case the previewer is on, request a delayed update
  if (m_previewMode != NO_PREVIEW) requestTimedRefresh();
}

//-----------------------------------------------------------------------------

bool SceneViewer::is3DView() const {
  bool isCameraTest = CameraTestCheck::instance()->isEnabled();
  return (m_referenceMode == CAMERA3D_REFERENCE && !isCameraTest);
}

//-----------------------------------------------------------------------------

void SceneViewer::invalidateAll() {
  m_clipRect.empty();
  update();
  if (m_vRuler) m_vRuler->update();
  if (m_hRuler) m_hRuler->update();
}

//-----------------------------------------------------------------------------
/*! Pan the viewer by using "navigator" (red rectangle) in level strip
*/
void SceneViewer::navigatorPan(const QPoint &delta) {
  panQt(delta);
  m_pos += delta;
}

//-----------------------------------------------------------------------------

void SceneViewer::GLInvalidateAll() {
  m_clipRect.empty();
  updateGL();
  if (m_vRuler) m_vRuler->update();
  if (m_hRuler) m_hRuler->update();
}

//-----------------------------------------------------------------------------

void SceneViewer::GLInvalidateRect(const TRectD &rect) {
  m_clipRect = rect;
  updateGL();
  m_clipRect.empty();
  if (m_vRuler) m_vRuler->update();
  if (m_hRuler) m_hRuler->update();
}

//-----------------------------------------------------------------------------

// delta.x: right panning, pixel; delta.y: down panning, pixel
void SceneViewer::panQt(const QPoint &delta) {
  if (delta == QPoint()) return;
  if (is3DView())
    m_pan3D += TPointD(delta.x(), -delta.y());
  else {
    // TAffine &viewAff = m_viewAff[m_viewMode];
    // viewAff = TTranslation(delta.x(), -delta.y()) * viewAff;
    setViewMatrix(TTranslation(delta.x(), -delta.y()) * m_viewAff[m_viewMode],
                  m_viewMode);
  }
  invalidateAll();
  emit refreshNavi();
}

//-----------------------------------------------------------------------------

void SceneViewer::zoomQt(bool forward, bool reset) {
  TPointD delta(m_lastMousePos.x() - width() / 2,
                -m_lastMousePos.y() + height() / 2);

  if (is3DView()) {
    if (reset || ((m_zoomScale3D < 500 || !forward) &&
                  (m_zoomScale3D > 0.01 || forward))) {
      double oldZoomScale = m_zoomScale3D;
      m_zoomScale3D       = reset ? 1 : ImageUtils::getQuantizedZoomFactor(
                                      m_zoomScale3D, forward);

      m_pan3D = -(m_zoomScale3D / oldZoomScale) * -m_pan3D;
    }
  } else {
    // a factor for getting pixel-based zoom ratio
    double dpiFactor = getDpiFactor();
    // when showing the viewer with full-screen mode,
    // add a zoom factor which can show image fitting with the screen size
    double zoomScaleFittingWithScreen = 0.0f;
    if (dpiFactor != 1.0) {
      // check if the viewer is in full screen mode
      ImageUtils::FullScreenWidget *fsWidget =
          dynamic_cast<ImageUtils::FullScreenWidget *>(parentWidget());
      if (fsWidget && (fsWidget->windowState() & Qt::WindowFullScreen) != 0)
        zoomScaleFittingWithScreen = getZoomScaleFittingWithScreen();
    }

    int i;

    for (i = 0; i < 2; i++) {
      TAffine &viewAff = m_viewAff[i];
      double scale2    = viewAff.det();
      if (reset || ((scale2 < 100000 || !forward) &&
                    (scale2 > 0.001 * 0.05 || forward))) {
        double oldZoomScale = sqrt(scale2) * dpiFactor;
        double zoomScale    = reset ? 1 : ImageUtils::getQuantizedZoomFactor(
                                           oldZoomScale, forward);

        // threshold value -0.001 is intended to absorb the error of calculation
        if ((oldZoomScale - zoomScaleFittingWithScreen) *
                (zoomScale - zoomScaleFittingWithScreen) <
            -0.001)
          zoomScale = zoomScaleFittingWithScreen;
        // zoom center = viewer center
        if (Preferences::instance()->getViewerZoomCenter())
          setViewMatrix(TScale(zoomScale / oldZoomScale) * viewAff, i);
        // zoom center = mouse pos
        else
          setViewMatrix(TTranslation(delta) * TScale(zoomScale / oldZoomScale) *
                            TTranslation(-delta) * viewAff,
                        i);
      }
    }
  }

  GLInvalidateAll();
  emit onZoomChanged();
}

//-----------------------------------------------------------------------------
/*! a factor for getting pixel-based zoom ratio
*/
double SceneViewer::getDpiFactor() {
  // When the current unit is "pixels", always use a standard dpi
  if (Preferences::instance()->getPixelsOnly()) {
    return Stage::inch / Stage::standardDpi;
  }
  // When preview mode, use a camera DPI
  else if (isPreviewEnabled()) {
    return Stage::inch /
           TApp::instance()
               ->getCurrentScene()
               ->getScene()
               ->getCurrentCamera()
               ->getDpi()
               .x;
  }
  // When level editing mode, use an image DPI
  else if (TApp::instance()->getCurrentFrame()->isEditingLevel()) {
    TXshSimpleLevel *sl;
    sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl) return 1.;
    if (sl->getType() == PLI_XSHLEVEL) return 1.;
    if (sl->getImageDpi() != TPointD())
      return Stage::inch / sl->getImageDpi().x;
    if (sl->getDpi() != TPointD()) return Stage::inch / sl->getDpi().x;
    return 1.;
  }
  // When the special case in the scene editing mode:
  // If the option "ActualPixelViewOnSceneEditingMode" is ON,
  // use  current level's DPI set in the level settings.
  else if (Preferences::instance()
               ->isActualPixelViewOnSceneEditingModeEnabled() &&
           !CleanupPreviewCheck::instance()->isEnabled() &&
           !CameraTestCheck::instance()->isEnabled()) {
    TXshSimpleLevel *sl;
    sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl) return 1.;
    if (sl->getType() == PLI_XSHLEVEL) return 1.;
    if (sl->getDpi() == TPointD()) return 1.;
    // use default value for the argument of getDpi() (=TFrameId::NO_FRAME）
    // so that the dpi of the first frame in the level will be returned.
    return Stage::inch / sl->getDpi().x;
  }
  // When the scene editing mode without any option, don't think about DPI
  else {
    return 1.;
  }
}

//-----------------------------------------------------------------------------
/*! when showing the viewer with full-screen mode,
        add a zoom factor which can show image fitting with the screen size
*/

double SceneViewer::getZoomScaleFittingWithScreen() {
  TDimension imgSize;

  // get the image size to be rendered

  if (isPreviewEnabled())
    imgSize = TApp::instance()
                  ->getCurrentScene()
                  ->getScene()
                  ->getCurrentCamera()
                  ->getRes();
  else if (TApp::instance()->getCurrentFrame()->isEditingLevel()) {
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL || sl->getImageDpi() == TPointD())
      return 0.0;

    imgSize = sl->getResolution();
  } else if (Preferences::instance()
                 ->isActualPixelViewOnSceneEditingModeEnabled() &&
             !CleanupPreviewCheck::instance()->isEnabled() &&
             !CameraTestCheck::instance()->isEnabled()) {
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl || sl->getType() == PLI_XSHLEVEL || sl->getDpi() == TPointD())
      return 0.0;
    imgSize = sl->getResolution();

  } else  // SCENE_VIEWMODE
    return 0.0;

  // add small margin on the edge of the image
  int margin = 20;
  // get the desktop resolution
  QRect rec = QApplication::desktop()->screenGeometry();

  // fit to either direction
  int moni_x = rec.width() - (margin * 2);
  int moni_y = rec.height() - (margin * 2);
  return std::min((double)moni_x / (double)imgSize.lx,
                  (double)moni_y / (double)imgSize.ly);
}

//-----------------------------------------------------------------------------

// center: window coordinate, pixels, topleft origin
void SceneViewer::zoomQt(const QPoint &center, double factor) {
  if (factor == 1.0) return;
  TPointD delta(center.x() - width() / 2, -center.y() + height() / 2);
  double oldZoomScale = m_zoomScale3D;

  if (is3DView()) {
    if ((m_zoomScale3D < 500 || factor < 1) &&
        (m_zoomScale3D > 0.01 || factor > 1)) {
      m_zoomScale3D *= factor;
      m_pan3D = -(m_zoomScale3D / oldZoomScale) * (delta - m_pan3D) + delta;
    }
  } else {
    int i;
    for (i = 0; i < 2; i++) {
      TAffine &viewAff = m_viewAff[i];
      double scale2    = fabs(viewAff.det());
      if ((scale2 < 100000 || factor < 1) &&
          (scale2 > 0.001 * 0.05 || factor > 1))
        if (i == m_viewMode)
          // viewAff = TTranslation(delta) * TScale(factor) *
          // TTranslation(-delta) * viewAff;
          setViewMatrix(TTranslation(delta) * TScale(factor) *
                            TTranslation(-delta) * viewAff,
                        i);
        else
          // viewAff = TScale(factor) * viewAff;
          setViewMatrix(TScale(factor) * viewAff, i);
    }
  }

  GLInvalidateAll();
  emit onZoomChanged();
}

void SceneViewer::zoom(const TPointD &center, double factor) {
  zoomQt(QPoint(center.x, height() - center.y), factor);
}

//-----------------------------------------------------------------------------

void SceneViewer::rotate(const TPointD &center, double angle) {
  if (angle == 0) return;
  TPointD realCenter = m_viewAff[m_viewMode] * center;
  setViewMatrix(TRotation(realCenter, angle) * m_viewAff[m_viewMode],
                m_viewMode);
  invalidateAll();
}

//-----------------------------------------------------------------------------

void SceneViewer::rotate3D(double dPhi, double dTheta) {
  if (dPhi == 0 && dTheta == 0) return;
  m_phi3D   = (float)tcrop(m_phi3D + dPhi, -90.0, 90.0);
  m_theta3D = (float)tcrop(m_theta3D + dTheta, 0.0, 90.0);
  invalidateAll();
}

//-----------------------------------------------------------------------------

void SceneViewer::regeneratePreview() {
  Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW)->clear();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::regeneratePreviewFrame() {
  Previewer::instance(m_previewMode == SUBCAMERA_PREVIEW)
      ->clear(TApp::instance()->getCurrentFrame()->getFrame());

  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::swapCompared() {
  m_compareSettings.m_swapCompared = !m_compareSettings.m_swapCompared;
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::fitToCamera() {
  // Reset the view scale to 1:1.
  resetSceneViewer();

  TXsheet *xsh            = TApp::instance()->getCurrentXsheet()->getXsheet();
  int frame               = TApp::instance()->getCurrentFrame()->getFrame();
  TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *camera    = xsh->getStageObject(cameraId);
  TAffine cameraPlacement = camera->getPlacement(frame);
  double cameraZ          = camera->getZ(frame);
  TAffine cameraAff =
      getViewMatrix() * cameraPlacement * TScale((1000 + cameraZ) / 1000);

  QRect viewRect    = rect();
  TRectD cameraRect = ViewerDraw::getCameraRect();
  TPointD P00       = cameraAff * cameraRect.getP00();
  TPointD P10       = cameraAff * cameraRect.getP10();
  TPointD P01       = cameraAff * cameraRect.getP01();
  TPointD P11       = cameraAff * cameraRect.getP11();
  TPointD p0        = TPointD(std::min({P00.x, P01.x, P10.x, P11.x}),
                       std::min({P00.y, P01.y, P10.y, P11.y}));
  TPointD p1 = TPointD(std::max({P00.x, P01.x, P10.x, P11.x}),
                       std::max({P00.y, P01.y, P10.y, P11.y}));
  cameraRect = TRectD(p0.x, p0.y, p1.x, p1.y);

  // Pan
  if (!is3DView()) {
    TPointD cameraCenter = (cameraRect.getP00() + cameraRect.getP11()) * 0.5;
    panQt(QPoint(-cameraCenter.x, cameraCenter.y));
  }

  double xratio = (double)viewRect.width() / cameraRect.getLx();
  double yratio = (double)viewRect.height() / cameraRect.getLy();
  double ratio  = std::min(xratio, yratio);

  // Scale and center on the center of \a rect.
  QPoint c = viewRect.center();
  zoom(TPointD(c.x(), c.y()), ratio);
}

//-----------------------------------------------------------------------------

void SceneViewer::resetSceneViewer() {
  m_visualSettings.m_sceneProperties =
      TApp::instance()->getCurrentScene()->getScene()->getProperties();

  for (int i = 0; i < tArrayCount(m_viewAff); i++)
    setViewMatrix(getNormalZoomScale(), i);

  m_pos         = QPoint(0, 0);
  m_pan3D       = TPointD(0, 0);
  m_zoomScale3D = 0.1;
  m_theta3D     = 20;
  m_phi3D       = 30;
  emit onZoomChanged();

  invalidateAll();
}

//-----------------------------------------------------------------------------

void SceneViewer::setActualPixelSize() {
  TApp *app           = TApp::instance();
  TXshLevel *l        = app->getCurrentLevel()->getLevel();
  TXshSimpleLevel *sl = l ? l->getSimpleLevel() : 0;
  if (!sl) return;

  TFrameId fid(app->getCurrentFrame()->getFid());

  TPointD dpi;
  if (CleanupPreviewCheck::instance()->isEnabled()) {
    // The previewed cleanup image has no image infos yet - retrieve the dpi
    // through
    // the cleanup camera data
    CleanupParameters *cleanupParams = app->getCurrentScene()
                                           ->getScene()
                                           ->getProperties()
                                           ->getCleanupParameters();

    TDimension dim(0, 0);
    cleanupParams->getOutputImageInfo(dim, dpi.x, dpi.y);
  } else
    dpi = sl->getDpi(fid);

  const double inch = Stage::inch;
  for (int i = 0; i < tArrayCount(m_viewAff); i++)
    setViewMatrix(dpi == TPointD(0, 0) ? getNormalZoomScale()
                                       : TScale(dpi.x / inch, dpi.y / inch),
                  i);

  m_pos         = QPoint(0, 0);
  m_pan3D       = TPointD(0, 0);
  m_zoomScale3D = 0.1;
  m_theta3D     = 20;
  m_phi3D       = 30;
  emit onZoomChanged();

  invalidateAll();
}

//-----------------------------------------------------------------------------

void SceneViewer::onLevelChanged() {
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool) {
    TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
    if (level && level->getSimpleLevel())
      m_dpiScale =
          getCurrentDpiScale(level->getSimpleLevel(), tool->getCurrentFid());
    else
      m_dpiScale = TPointD(1, 1);
  }
}
//-----------------------------------------------------------------------------
/*! when level is switched, update m_dpiScale in order to show white background
 * for Ink&Paint work properly
*/
void SceneViewer::onLevelSwitched() {
  TApp *app        = TApp::instance();
  TTool *tool      = app->getCurrentTool()->getTool();
  TXshLevel *level = app->getCurrentLevel()->getLevel();
  if (level && level->getSimpleLevel())
    m_dpiScale =
        getCurrentDpiScale(level->getSimpleLevel(), tool->getCurrentFid());
  else
    m_dpiScale = TPointD(1, 1);
}

//-----------------------------------------------------------------------------

void SceneViewer::onXsheetChanged() {
  m_forceGlFlush = true;
  TTool *tool    = TApp::instance()->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) tool->updateMatrix();
  onLevelChanged();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::onObjectSwitched() {
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) tool->updateMatrix();
  onLevelChanged();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::onSceneChanged() {
  onLevelChanged();
  update();
}

//-----------------------------------------------------------------------------

void SceneViewer::onFrameSwitched() {
  invalidateToolStatus();

  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool && tool->isEnabled()) {
    tool->setViewer(this);
    tool->updateMatrix();
    tool->onEnter();
  }

  update();
}

//-----------------------------------------------------------------------------
/*! when tool options are changed, update tooltip immediately
*/
void SceneViewer::onToolChanged() {
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool) setToolCursor(this, tool->getCursorId());
  update();
}

//-----------------------------------------------------------------------------

int SceneViewer::pick(const TPoint &point) {
  // pick is typically called in a mouse event handler.
  // QGLWidget::makeCurrent() is not automatically called in these events.
  // (to exploit the bug: open the FxEditor preview and then select the edit
  // tool)
  m_isPicking = true;
  makeCurrent();
  assert(glGetError() == GL_NO_ERROR);
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  GLuint selectBuffer[512];
  glSelectBuffer(tArrayCount(selectBuffer), selectBuffer);
  glRenderMode(GL_SELECT);

  // definisco la matrice di proiezione
  glMatrixMode(GL_PROJECTION);
  GLdouble mat[16];
  glGetDoublev(GL_PROJECTION_MATRIX, mat);
  glPushMatrix();
  glLoadIdentity();
  gluPickMatrix(point.x, point.y, 5, 5, viewport);
  glMultMatrixd(mat);
  assert(glGetError() == GL_NO_ERROR);

  // disegno la scena
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glInitNames();
  assert(glGetError() == GL_NO_ERROR);

  // WARNING: We have to draw the scene in CAMERASTAND mode. Observe that the
  // preview mode may
  // invoke event processing - therefore triggering other pick events while in
  // GL_SELECT
  // render mode...
  int previewMode = m_previewMode;
  m_previewMode   = NO_PREVIEW;

  //   OPTIMIZATION / QUICK FIX
  // A 1x1 clipping rect around the picked pos can very well be used instead of
  // redrawing
  // the *entire viewer*.
  // This is needed especially since some graphic cards (all NVidias we have
  // tested) are
  // very slow otherwise. This could be due to this particular rendering mode -
  // or because
  // we could be painting OUTSIDE a paintEvent()...

  TRectD oldClipRect(m_clipRect);
  m_clipRect = TRectD(point.x, point.y, point.x + 1, point.y + 1);

  paintGL();  // draw identifiable objects

  m_clipRect = oldClipRect;

  m_previewMode = previewMode;

  assert(glGetError() == GL_NO_ERROR);
  glPopMatrix();

  // rimetto a posto la matrice di proiezione
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);

  assert(glGetError() == GL_NO_ERROR);

  // conto gli hits
  int ret      = -1;
  int hitCount = glRenderMode(GL_RENDER);
  GLuint *p    = selectBuffer;
  for (int i = 0; i < hitCount; ++i) {
    GLuint nameCount = *p++;
    GLuint zmin      = *p++;
    GLuint zmax      = *p++;
    if (nameCount > 0) {
      GLuint name = *p;
      ret         = name;  // items.push_back(PickItem(name, zmin, zmax));
    }
    p += nameCount;
  }
  assert(glGetError() == GL_NO_ERROR);
  m_isPicking = false;
  return ret;
}

//-----------------------------------------------------------------------------

int SceneViewer::posToColumnIndex(const TPoint &p, double distance,
                                  bool includeInvisible) const {
  std::vector<int> ret;
  posToColumnIndexes(p, ret, distance, includeInvisible);
  return ret.empty() ? -1 : ret.back();
}

//-----------------------------------------------------------------------------

void SceneViewer::posToColumnIndexes(const TPoint &p, std::vector<int> &indexes,
                                     double distance,
                                     bool includeInvisible) const {
  int oldRasterizePli    = TXshSimpleLevel::m_rasterizePli;
  TApp *app              = TApp::instance();
  ToonzScene *scene      = app->getCurrentScene()->getScene();
  TXsheet *xsh           = app->getCurrentXsheet()->getXsheet();
  int frame              = app->getCurrentFrame()->getFrame();
  int currentColumnIndex = app->getCurrentColumn()->getColumnIndex();
  OnionSkinMask osm      = app->getCurrentOnionSkin()->getOnionSkinMask();

  TPointD pos = TPointD(p.x - width() / 2, p.y - height() / 2);
  Stage::Picker picker(getViewMatrix(), pos, m_visualSettings);
  picker.setDistance(distance);

  TXshSimpleLevel::m_rasterizePli = 0;

  Stage::VisitArgs args;
  args.m_scene       = scene;
  args.m_xsh         = xsh;
  args.m_row         = frame;
  args.m_col         = currentColumnIndex;
  args.m_osm         = &osm;
  args.m_onlyVisible = includeInvisible;

  Stage::visit(picker, args); /*
picker,
scene,
xsh,
frame,
currentColumnIndex,
osm,
false,
0,
false,
includeInvisible);
*/

  TXshSimpleLevel::m_rasterizePli = oldRasterizePli;
  picker.getColumnIndexes(indexes);
}

//-----------------------------------------------------------------------------

int SceneViewer::posToRow(const TPoint &p, double distance,
                          bool includeInvisible) const {
  int oldRasterizePli    = TXshSimpleLevel::m_rasterizePli;
  TApp *app              = TApp::instance();
  ToonzScene *scene      = app->getCurrentScene()->getScene();
  TXsheet *xsh           = app->getCurrentXsheet()->getXsheet();
  int frame              = app->getCurrentFrame()->getFrame();
  int currentColumnIndex = app->getCurrentColumn()->getColumnIndex();
  OnionSkinMask osm      = app->getCurrentOnionSkin()->getOnionSkinMask();

  TPointD pos = TPointD(p.x - width() / 2, p.y - height() / 2);
  Stage::Picker picker(getViewMatrix(), pos, m_visualSettings);
  picker.setDistance(distance);

  TXshSimpleLevel::m_rasterizePli = 0;

  Stage::VisitArgs args;
  args.m_scene       = scene;
  args.m_xsh         = xsh;
  args.m_row         = frame;
  args.m_col         = currentColumnIndex;
  args.m_osm         = &osm;
  args.m_onlyVisible = includeInvisible;

  Stage::visit(picker, args);

  TXshSimpleLevel::m_rasterizePli = oldRasterizePli;
  return picker.getRow();
}

//-----------------------------------------------------------------------------

void drawSpline(const TAffine &viewMatrix, const TRect &clipRect, bool camera3d,
                double pixelsize) {
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  TStageObjectId objId = TApp::instance()->getCurrentObject()->getObjectId();

  TStageObject *pegbar =
      objId != TStageObjectId::NoneId ? xsh->getStageObject(objId) : 0;
  const TStroke *stroke                     = 0;
  if (pegbar && pegbar->getSpline()) stroke = pegbar->getSpline()->getStroke();
  if (!stroke) return;

  int frame = TApp::instance()->getCurrentFrame()->getFrame();

  TAffine aff;
  double objZ = 0, objNoScaleZ = 0;
  if (objId != TStageObjectId::NoneId) {
    aff         = xsh->getParentPlacement(objId, frame);
    objZ        = xsh->getZ(objId, frame);
    objNoScaleZ = xsh->getStageObject(objId)->getGlobalNoScaleZ();
  }

  glPushMatrix();
  if (camera3d) {
    tglMultMatrix(aff);
    aff = TAffine();
    glTranslated(0, 0, objZ);
  } else {
    TStageObjectId cameraId = xsh->getStageObjectTree()->getCurrentCameraId();
    double camZ             = xsh->getZ(cameraId, frame);
    TAffine camAff          = xsh->getPlacement(cameraId, frame);
    TAffine tmp;
    TStageObject::perspective(tmp, camAff, camZ, aff, objZ, objNoScaleZ);
    aff = viewMatrix * tmp;
  }

  if (TApp::instance()->getCurrentObject()->isSpline()) {
    glColor3d(1.0, 0.5, 0);
    glLineStipple(1, 0x18FF);
  } else {
    glLineStipple(1, 0xCCCC);
    glColor3d(1, 0, 1);
  }

  glEnable(GL_LINE_STIPPLE);
  tglMultMatrix(aff);

  double pixelSize    = std::max(0.1, pixelsize);
  double strokeLength = stroke->getLength();
  int n               = (int)(5 + (strokeLength / pixelSize) * 0.1);

  glBegin(GL_LINE_STRIP);
  for (int i = 0; i < n; i++)
    tglVertex(stroke->getPoint((double)i / (double)(n - 1)));
  glEnd();
  glDisable(GL_LINE_STIPPLE);
  int cpCount = stroke->getControlPointCount();
  for (int i = 0; i * 4 < cpCount; i++) {
    double t    = stroke->getParameterAtControlPoint(i * 4);
    TPointD pos = stroke->getPoint(t);
    tglDrawText(pos, QString::number(i).toStdString().c_str());
  }

  if (pegbar) {
    TAffine parentAff = xsh->getParentPlacement(objId, frame);
    TAffine aff       = xsh->getPlacement(objId, frame);
    TPointD center    = Stage::inch * xsh->getCenter(objId, frame);
    glPushMatrix();
    tglMultMatrix(parentAff.inv() * TTranslation(aff * center));
    center = TPointD();

    // draw center
    // tglDrawDisk(center,pixelSize*5);
    tglDrawDisk(center, sqrt(tglGetPixelSize2()) * 5);

    glPopMatrix();
  }

  glPopMatrix();
}

//-----------------------------------------------------------------------------

void SceneViewer::resetInputMethod() {
#if QT_VERSION >= 0x050000
  QGuiApplication::inputMethod()->reset();
#else
  qApp->inputContext()->reset();
#endif
}

//-----------------------------------------------------------------------------

void SceneViewer::set3DLeftSideView() {
  m_phi3D   = -90;
  m_theta3D = 0;
  invalidateAll();
}

//-----------------------------------------------------------------------------

void SceneViewer::set3DRightSideView() {
  m_phi3D   = 90;
  m_theta3D = 0;
  invalidateAll();
}

//-----------------------------------------------------------------------------

void SceneViewer::set3DTopView() {
  m_phi3D   = 0;
  m_theta3D = 90;
  invalidateAll();
}

//-----------------------------------------------------------------------------

bool SceneViewer::canSwapCompared() const {
  return m_visualSettings.m_doCompare && m_previewMode != NO_PREVIEW;
}

//-----------------------------------------------------------------------------

TAffine SceneViewer::getNormalZoomScale() {
  return TScale(getDpiFactor()).inv();
}

//-----------------------------------------------------------------------------

void SceneViewer::invalidateToolStatus() {
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (tool) {
    m_toolDisableReason = tool->updateEnabled();
    if (tool->isEnabled()) {
      setToolCursor(this, tool->getCursorId());
      tool->setViewer(this);
      tool->updateMatrix();
    } else
      setCursor(Qt::ForbiddenCursor);
  } else
    setCursor(Qt::ForbiddenCursor);
}

//-----------------------------------------------------------------------------
/*! return the viewer geometry in order to avoid picking the style outside of
   the viewer
    when using the stylepicker and the finger tools
*/

TRectD SceneViewer::getGeometry() const {
  int devPixRatio = getDevPixRatio();
  TTool *tool     = TApp::instance()->getCurrentTool()->getTool();
  TPointD topLeft =
      tool->getMatrix().inv() * winToWorld(geometry().topLeft() * devPixRatio);
  TPointD bottomRight = tool->getMatrix().inv() *
                        winToWorld(geometry().bottomRight() * devPixRatio);

  TObjectHandle *objHandle = TApp::instance()->getCurrentObject();
  if (tool->getToolType() & TTool::LevelTool && !objHandle->isSpline()) {
    topLeft.x /= m_dpiScale.x;
    topLeft.y /= m_dpiScale.y;
    bottomRight.x /= m_dpiScale.x;
    bottomRight.y /= m_dpiScale.y;
  }

  return TRectD(topLeft, bottomRight);
}

//-----------------------------------------------------------------------------
/*! delete preview - subcamera executed from context menu
*/
void SceneViewer::doDeleteSubCamera() {
  PreviewSubCameraManager::instance()->deleteSubCamera(this);
}
