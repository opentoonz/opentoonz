

#include "tpanels.h"

// Tnz6 includes
#include "pane.h"
#include "viewerpane.h"
#include "exportpanel.h"
#include "scriptconsolepanel.h"

#include "floatingpanelcommand.h"
#include "subscenecommand.h"

#include "xsheetviewer.h"
#include "sceneviewer.h"
#include "toolbar.h"
#include "commandbar.h"
#include "flipbook.h"
#include "castviewer.h"
#include "filebrowser.h"
#include "filmstrip.h"
#include "previewfxmanager.h"
#include "comboviewerpane.h"
#include "historypane.h"
#include "cleanupsettingspane.h"

#include "vectorguideddrawingpane.h"

#include "expressionreferencemanager.h"

#if defined(x64)
#include "stopmotioncontroller.h"
#endif

#include "tasksviewer.h"
#include "batchserversviewer.h"
#include "colormodelviewer.h"

#include "curveio.h"
#include "menubarcommandids.h"
#include "tapp.h"
#include "mainwindow.h"
#include "columncommand.h"
#include "levelcommand.h"

// TnzTools includes
#include "tools/tooloptions.h"
#include "tools/strokeselection.h"
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/schematicviewer.h"
#include "toonzqt/paletteviewer.h"
#include "toonzqt/styleeditor.h"
#include "toonzqt/studiopaletteviewer.h"
#include "toonzqt/functionviewer.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/tmessageviewer.h"
#include "toonzqt/scriptconsole.h"
#include "toonzqt/fxsettings.h"
#include "toonzqt/fxselection.h"
#include "stageobjectselection.h"

// TnzLib includes
#include "toonz/palettecontroller.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcamera.h"
#include "toonz/scenefx.h"
#include "toonz/tproject.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshcell.h"
#include "toonz/cleanupcolorstyles.h"
#include "toonz/palettecmd.h"
#include "toonz/preferences.h"
#include "tw/stringtable.h"
#include "toonz/toonzfolders.h"
#include "toonz/fxcommand.h"
#include "toonz/tstageobjectcmd.h"

// TnzBase includes
#include "trasterfx.h"
#include "toutputproperties.h"

// TnzCore includes
#include "tcolorstyles.h"
#include "trasterimage.h"
#include "timagecache.h"
#include "tstream.h"
#include "tsystem.h"

// Qt includes
#include <QAction>
#include <QScreen>

//=============================================================================
// XsheetViewer
//-----------------------------------------------------------------------------

class XsheetViewerFactory final : public TPanelFactory {
public:
  XsheetViewerFactory() : TPanelFactory("Xsheet") {}
  void initialize(TPanel *panel) override {
    panel->setWidget(new XsheetViewer(panel));
    panel->resize(500, 300);
  }
} xsheetViewerFactory;

//=============================================================================
// XsheetViewer - Timeline mode
//-----------------------------------------------------------------------------

class TimelineViewerFactory final : public TPanelFactory {
public:
  TimelineViewerFactory() : TPanelFactory("Timeline") {}
  void initialize(TPanel *panel) override {
    panel->setWidget(new XsheetViewer(panel));
    XsheetViewer *xsh = (XsheetViewer *)panel->widget();
    xsh->flipOrientation();
    panel->resize(500, 300);
  }
} timelineViewerFactory;

//=============================================================================
// SchematicSceneViewer
//-----------------------------------------------------------------------------

//=========================================================
// SchematicSceneViewerFactory
//---------------------------------------------------------

class SchematicSceneViewerFactory final : public QObject, public TPanelFactory {
public:
  SchematicSceneViewerFactory() : TPanelFactory("Schematic") {}

  TPanel *createPanel(QWidget *parent) override {
    SchematicScenePanel *panel = new SchematicScenePanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Schematic"));
    panel->setMinimumSize(230, 200);
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }

} schematicSceneViewerFactory;

//=============================================================================
// SchematicSceneViewer
//-----------------------------------------------------------------------------

SchematicScenePanel::SchematicScenePanel(QWidget *parent) : TPanel(parent) {
  TApp *app         = TApp::instance();
  m_schematicViewer = new SchematicViewer(this);
  setViewType(1);
  m_schematicViewer->setApplication(app);
  m_schematicViewer->updateSchematic();
  setFocusProxy(m_schematicViewer);
  setWidget(m_schematicViewer);

  resize(560, 460);
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onShowPreview(TFxP fx) {
  PreviewFxManager::instance()->showNewPreview(fx);
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onCollapse(const QList<TFxP> &fxs) {
  SubsceneCmd::collapse(fxs);
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onCollapse(QList<TStageObjectId> objects) {
  SubsceneCmd::collapse(objects);
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onExplodeChild(const QList<TFxP> &fxs) {
  int i;
  for (i = 0; i < fxs.size(); i++) {
    TLevelColumnFx *lcFx = dynamic_cast<TLevelColumnFx *>(fxs[i].getPointer());
    if (lcFx) SubsceneCmd::explode(lcFx->getColumnIndex());
  }
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onExplodeChild(QList<TStageObjectId> ids) {
  int i;
  for (i = 0; i < ids.size(); i++) {
    TStageObjectId id = ids[i];
    if (id.isColumn()) SubsceneCmd::explode(id.getIndex());
  }
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onEditObject() {
  TApp *app = TApp::instance();
  app->getCurrentTool()->setTool(T_Edit);
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::setViewType(int viewType) {
  m_schematicViewer->setStageSchematicViewed(viewType != 0);
}

//-----------------------------------------------------------------------------

int SchematicScenePanel::getViewType() {
  return m_schematicViewer->isStageSchematicViewed();
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onDeleteFxs(const FxSelection *selection) {
  if (selection->isEmpty()) return;
  std::set<int> colIndices;
  std::set<TFx *> fxs;
  for (auto index : selection->getColumnIndexes()) colIndices.insert(index);
  for (auto fx : selection->getFxs()) fxs.insert(fx.getPointer());

  if (!ColumnCmd::checkExpressionReferences(colIndices, fxs)) return;

  TApp *app = TApp::instance();
  TFxCommand::deleteSelection(selection->getFxs().toStdList(),
                              selection->getLinks().toStdList(),
                              selection->getColumnIndexes().toStdList(),
                              app->getCurrentXsheet(), app->getCurrentFx());
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onDeleteStageObjects(
    const StageObjectSelection *selection) {
  if (!ExpressionReferenceManager::instance()->checkReferenceDeletion(
          selection->getObjects()))
    return;

  TApp *app = TApp::instance();
  TStageObjectCmd::deleteSelection(
      selection->getObjects().toVector().toStdVector(),
      selection->getLinks().toStdList(), selection->getSplines().toStdList(),
      app->getCurrentXsheet(), app->getCurrentObject(), app->getCurrentFx());
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::onColumnPaste(const QList<TXshColumnP> &columns) {
  LevelCmd::addMissingLevelsToCast(columns);
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::showEvent(QShowEvent *e) {
  if (m_schematicViewer->isStageSchematicViewed())
    setWindowTitle(QObject::tr("Stage Schematic"));
  else
    setWindowTitle(QObject::tr("Fx Schematic"));

  TApp *app = TApp::instance();
  connect(m_schematicViewer, SIGNAL(showPreview(TFxP)), this,
          SLOT(onShowPreview(TFxP)));
  connect(m_schematicViewer, SIGNAL(doCollapse(const QList<TFxP> &)), this,
          SLOT(onCollapse(const QList<TFxP> &)));
  connect(m_schematicViewer, SIGNAL(doCollapse(QList<TStageObjectId>)), this,
          SLOT(onCollapse(QList<TStageObjectId>)));
  connect(m_schematicViewer, SIGNAL(doExplodeChild(const QList<TFxP> &)), this,
          SLOT(onExplodeChild(const QList<TFxP> &)));
  connect(m_schematicViewer, SIGNAL(doDeleteFxs(const FxSelection *)), this,
          SLOT(onDeleteFxs(const FxSelection *)));
  connect(m_schematicViewer, SIGNAL(doExplodeChild(QList<TStageObjectId>)),
          this, SLOT(onExplodeChild(QList<TStageObjectId>)));
  connect(m_schematicViewer,
          SIGNAL(doDeleteStageObjects(const StageObjectSelection *)), this,
          SLOT(onDeleteStageObjects(const StageObjectSelection *)));
  connect(m_schematicViewer, SIGNAL(editObject()), this, SLOT(onEditObject()));
  connect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()), m_schematicViewer,
          SLOT(updateScenes()));
  connect(app->getCurrentObject(), SIGNAL(objectSwitched()), m_schematicViewer,
          SLOT(updateScenes()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()), m_schematicViewer,
          SLOT(updateSchematic()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()), m_schematicViewer,
          SLOT(updateSchematic()));
  connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), m_schematicViewer,
          SLOT(onSceneSwitched()));
  connect(m_schematicViewer, SIGNAL(columnPasted(const QList<TXshColumnP> &)),
          this, SLOT(onColumnPaste(const QList<TXshColumnP> &)));
  m_schematicViewer->updateSchematic();
}

//-----------------------------------------------------------------------------

void SchematicScenePanel::hideEvent(QHideEvent *e) {
  TApp *app = TApp::instance();
  disconnect(m_schematicViewer, SIGNAL(showPreview(TFxP)), this,
             SLOT(onShowPreview(TFxP)));
  disconnect(m_schematicViewer, SIGNAL(doCollapse(const QList<TFxP> &)), this,
             SLOT(onCollapse(const QList<TFxP> &)));
  disconnect(m_schematicViewer, SIGNAL(doCollapse(QList<TStageObjectId>)), this,
             SLOT(onCollapse(QList<TStageObjectId>)));
  disconnect(m_schematicViewer, SIGNAL(doExplodeChild(const QList<TFxP> &)),
             this, SLOT(onExplodeChild(const QList<TFxP> &)));
  disconnect(m_schematicViewer, SIGNAL(doExplodeChild(QList<TStageObjectId>)),
             this, SLOT(onExplodeChild(QList<TStageObjectId>)));
  disconnect(m_schematicViewer, SIGNAL(editObject()), this,
             SLOT(onEditObject()));
  disconnect(app->getCurrentLevel(), SIGNAL(xshLevelChanged()),
             m_schematicViewer, SLOT(updateScenes()));
  disconnect(app->getCurrentObject(), SIGNAL(objectSwitched()),
             m_schematicViewer, SLOT(updateScenes()));
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()),
             m_schematicViewer, SLOT(updateSchematic()));
  disconnect(app->getCurrentXsheet(), SIGNAL(xsheetChanged()),
             m_schematicViewer, SLOT(updateSchematic()));
  disconnect(app->getCurrentScene(), SIGNAL(sceneSwitched()), m_schematicViewer,
             SLOT(onSceneSwitched()));
  disconnect(m_schematicViewer,
             SIGNAL(columnPasted(const QList<TXshColumnP> &)), this,
             SLOT(onColumnPaste(const QList<TXshColumnP> &)));
}

//=============================================================================
OpenFloatingPanel openSchematicSceneViewerCommand(MI_OpenSchematic, "Schematic",
                                                  QObject::tr("Schematic"));
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*-- ショートカットキーでPreviewFxを実行するためのコマンド --*/
class FxPreviewCommand {
public:
  FxPreviewCommand() {
    setCommandHandler("MI_PreviewFx", this, &FxPreviewCommand::onPreviewFx);
  }

  void onPreviewFx() {
    TFx *currentFx = TApp::instance()->getCurrentFx()->getFx();
    if (!currentFx) {
      DVGui::warning("Preview Fx : No Current Fx !");
      return;
    }
    /*--
     TLevelColumnFx,TZeraryColumnFx,TXsheetFx,通常のFxで使用可能
     TPaletteColumnFx, TOutputFxではPreviewは使用不可
     --*/
    TPaletteColumnFx *pfx = dynamic_cast<TPaletteColumnFx *>(currentFx);
    TOutputFx *ofx        = dynamic_cast<TOutputFx *>(currentFx);
    if (pfx || ofx) {
      DVGui::warning(
          "Preview Fx command is not available on Palette or Output node !");
      return;
    }

    PreviewFxManager::instance()->showNewPreview(currentFx);
  }
};

FxPreviewCommand fxPreviewCommand;
//-----------------------------------------------------------------------------

//=============================================================================
// FunctionViewer
//-----------------------------------------------------------------------------

FunctionViewerPanel::FunctionViewerPanel(QWidget *parent)
    : TPanel(parent), m_functionViewer(new FunctionViewer(this)) {
  setWidget(m_functionViewer);

  attachHandles();

  bool ret =
      connect(m_functionViewer,
              SIGNAL(curveIo(int, TDoubleParam *, const std::string &)), this,
              SLOT(onIoCurve(int, TDoubleParam *, const std::string &)));
  ret &&connect(m_functionViewer, SIGNAL(editObject()), this,
                SLOT(onEditObject()));

  assert(ret);
}

void FunctionViewerPanel::attachHandles() {
  TApp *app = TApp::instance();

  m_functionViewer->setXsheetHandle(app->getCurrentXsheet());
  m_functionViewer->setFrameHandle(app->getCurrentFrame());
  m_functionViewer->setObjectHandle(app->getCurrentObject());
  m_functionViewer->setFxHandle(app->getCurrentFx());
  m_functionViewer->setColumnHandle(app->getCurrentColumn());
  m_functionViewer->setSceneHandle(app->getCurrentScene());
}

void FunctionViewerPanel::detachHandles() {
  m_functionViewer->setXsheetHandle(0);
  m_functionViewer->setFrameHandle(0);
  m_functionViewer->setObjectHandle(0);
  m_functionViewer->setFxHandle(0);
  m_functionViewer->setColumnHandle(0);
  m_functionViewer->setSceneHandle(0);
}

void FunctionViewerPanel::reset() {
  attachHandles();
  m_functionViewer->rebuildModel();
}

void FunctionViewerPanel::onEditObject() {
  TApp *app = TApp::instance();
  app->getCurrentTool()->setTool(T_Edit);
}

void FunctionViewerPanel::widgetFocusOnEnter() {
  if (!m_functionViewer->isExpressionPageActive())
    m_functionViewer->setFocusColumnsOrGraph();
}
void FunctionViewerPanel::widgetClearFocusOnLeave() {
  m_functionViewer->clearFocusColumnsAndGraph();
}
bool FunctionViewerPanel::widgetInThisPanelIsFocused() {
  return m_functionViewer->columnsOrGraphHasFocus();
}

//=============================================================================

//------------------------------------------

void FunctionViewerPanel::onIoCurve(int type, TDoubleParam *curve,
                                    const std::string &name) {
  switch ((FunctionViewer::IoType)type) {
  case FunctionViewer::eSaveCurve:
    saveCurve(curve);
    break;
  case FunctionViewer::eLoadCurve:
    loadCurve(curve);
    break;
  case FunctionViewer::eExportCurve:
    exportCurve(curve, name);
    break;
  default:
    assert(false);
    break;
  }
}

//=============================================================================
// CurrentStyleChangeCommand
//-----------------------------------------------------------------------------

class CurrentStyleChangeCommand final : public ChangeStyleCommand {
public:
  CurrentStyleChangeCommand() {}
  ~CurrentStyleChangeCommand() {}

  bool onStyleChanged() override {
    TApp *app = TApp::instance();
    int styleIndex =
        app->getPaletteController()->getCurrentLevelPalette()->getStyleIndex();
    TSelection *currentSelection = app->getCurrentSelection()->getSelection();
    StrokeSelection *ss = dynamic_cast<StrokeSelection *>(currentSelection);
    if (!ss || ss->isEmpty()) return false;
    ss->changeColorStyle(styleIndex);
    return true;
  }
};

//=============================================================================
// PaletteViewer
//-----------------------------------------------------------------------------

PaletteViewerPanel::PaletteViewerPanel(QWidget *parent)
    : StyleShortcutSwitchablePanel(parent), m_isFrozen(false) {
  m_paletteHandle = new TPaletteHandle();
  connect(m_paletteHandle, SIGNAL(colorStyleSwitched()),
          SLOT(onColorStyleSwitched()));
  connect(m_paletteHandle, SIGNAL(paletteSwitched()),
          SLOT(onPaletteSwitched()));

  TApp *app       = TApp::instance();
  m_paletteViewer = new PaletteViewer(this, PaletteViewerGUI::LEVEL_PALETTE);
  m_paletteViewer->setPaletteHandle(
      app->getPaletteController()->getCurrentLevelPalette());
  m_paletteViewer->setFrameHandle(app->getCurrentFrame());
  m_paletteViewer->setXsheetHandle(app->getCurrentXsheet());
  // for clearing cache when paste style command called from the StyleSelection
  m_paletteViewer->setLevelHandle(app->getCurrentLevel());

  TSceneHandle *sceneHandle = app->getCurrentScene();
  connect(sceneHandle, SIGNAL(sceneSwitched()), SLOT(onSceneSwitched()));

  CurrentStyleChangeCommand *currentStyleChangeCommand =
      new CurrentStyleChangeCommand();
  m_paletteViewer->setChangeStyleCommand(currentStyleChangeCommand);

  setWidget(m_paletteViewer);
  initializeTitleBar();
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::setViewType(int viewType) {
  m_paletteViewer->setViewMode(viewType);
}

//-----------------------------------------------------------------------------

int PaletteViewerPanel::getViewType() { return m_paletteViewer->getViewMode(); }

//-----------------------------------------------------------------------------

void PaletteViewerPanel::reset() {
  m_paletteViewer->setPaletteHandle(
      TApp::instance()->getPaletteController()->getCurrentLevelPalette());
  m_freezeButton->setPressed(false);
  setFrozen(false);
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::initializeTitleBar() {
  m_freezeButton = new TPanelTitleBarButton(
      getTitleBar(), getIconThemePath("actions/20/pane_freeze.svg"));
  m_freezeButton->setToolTip(tr("Freeze"));
  getTitleBar()->add(QPoint(-54, 0), m_freezeButton);
  m_freezeButton->setPressed(m_isFrozen);
  connect(m_freezeButton, SIGNAL(toggled(bool)),
          SLOT(onFreezeButtonToggled(bool)));
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::onColorStyleSwitched() {
  TApp::instance()->getPaletteController()->setCurrentPalette(m_paletteHandle);
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::onPaletteSwitched() {
  TApp::instance()->getPaletteController()->setCurrentPalette(m_paletteHandle);
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::onFreezeButtonToggled(bool frozen) {
  if (isFrozen() == frozen) return;

  TApp *app          = TApp::instance();
  TPaletteHandle *ph = app->getPaletteController()->getCurrentLevelPalette();
  // Se sono sulla palette del livello corrente e le palette e' vuota non
  // consento di bloccare il pannello.
  if (!isFrozen() && !ph->getPalette()) {
    m_freezeButton->setPressed(false);
    return;
  }

  setFrozen(frozen);
  m_paletteViewer->enableSaveAction(!frozen);

  // Cambio il livello corrente
  if (!frozen) {
    m_frozenPalette = nullptr;
    std::set<TXshSimpleLevel *> levels;
    TXsheet *xsheet = app->getCurrentXsheet()->getXsheet();
    int row, column;
    findPaletteLevels(levels, row, column, m_paletteHandle->getPalette(),
                      xsheet);
    // Se non trovo livelli riferiti alla palette setto il palette viewer alla
    // palette del livello corrente.
    if (levels.empty()) {
      m_paletteViewer->setPaletteHandle(ph);
      update();
      return;
    }
    TXshSimpleLevel *level = *levels.begin();
    // Se sto editando l'xsheet devo settare come corrente anche la colonna e il
    // frame.
    if (app->getCurrentFrame()->isEditingScene()) {
      int currentColumn = app->getCurrentColumn()->getColumnIndex();
      if (currentColumn != column)
        app->getCurrentColumn()->setColumnIndex(column);
      int currentRow = app->getCurrentFrame()->getFrameIndex();
      TXshCell cell  = xsheet->getCell(currentRow, column);
      if (cell.isEmpty() || cell.getSimpleLevel() != level)
        app->getCurrentFrame()->setFrameIndex(row);

      TCellSelection *selection = dynamic_cast<TCellSelection *>(
          app->getCurrentSelection()->getSelection());
      if (selection) selection->selectNone();
    }
    app->getCurrentLevel()->setLevel(level);
    m_paletteViewer->setPaletteHandle(ph);
  } else {
    m_frozenPalette = ph->getPalette();
    m_paletteHandle->setPalette(ph->getPalette());
    m_paletteViewer->setPaletteHandle(m_paletteHandle);
  }
  update();
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::onSceneSwitched() {
  // Se e' il paletteHandle del livello corrente l'aggiornamento viene fatto
  // grazie all'aggiornamento del livello.
  if (!isFrozen()) return;

  // Setto a zero la palette del "paletteHandle bloccato".
  m_paletteHandle->setPalette(0);
  // Sblocco il viewer nel caso in cui il e' bloccato.
  if (isFrozen()) {
    setFrozen(false);
    m_freezeButton->setPressed(false);
    m_paletteViewer->setPaletteHandle(
        TApp::instance()->getPaletteController()->getCurrentLevelPalette());
  }
  m_paletteViewer->updateView();
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::showEvent(QShowEvent *) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  bool ret = connect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
                     this, SLOT(onPreferenceChanged(const QString &)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::hideEvent(QHideEvent *) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  if (sceneHandle) sceneHandle->disconnect(this);
}

//-----------------------------------------------------------------------------

void PaletteViewerPanel::onPreferenceChanged(const QString &prefName) {
  if (prefName == "ColorCalibration") update();
}

//=============================================================================

class PaletteViewerFactory final : public TPanelFactory {
public:
  PaletteViewerFactory() : TPanelFactory("LevelPalette") {}

  TPanel *createPanel(QWidget *parent) override {
    PaletteViewerPanel *panel = new PaletteViewerPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr(("Level Palette")));

    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }

} paletteViewerFactory;

//=============================================================================
OpenFloatingPanel openPaletteCommand(MI_OpenPalette, "LevelPalette",
                                     QObject::tr("Palette"));
//-----------------------------------------------------------------------------

//=============================================================================
// StudioPaletteViewer
//-----------------------------------------------------------------------------

StudioPaletteViewerPanel::StudioPaletteViewerPanel(QWidget *parent)
    : TPanel(parent) {
  m_studioPaletteHandle = new TPaletteHandle();
  connect(m_studioPaletteHandle, SIGNAL(colorStyleSwitched()),
          SLOT(onColorStyleSwitched()));
  connect(m_studioPaletteHandle, SIGNAL(paletteSwitched()),
          SLOT(onPaletteSwitched()));

  connect(m_studioPaletteHandle, SIGNAL(paletteLockChanged()),
          SLOT(onPaletteSwitched()));

  TApp *app             = TApp::instance();
  m_studioPaletteViewer = new StudioPaletteViewer(
      this, m_studioPaletteHandle,
      app->getPaletteController()->getCurrentLevelPalette(),
      app->getCurrentFrame(), app->getCurrentXsheet(), app->getCurrentLevel());
  setWidget(m_studioPaletteViewer);
}

//-----------------------------------------------------------------------------

void StudioPaletteViewerPanel::onColorStyleSwitched() {
  TApp::instance()->getPaletteController()->setCurrentPalette(
      m_studioPaletteHandle);
}

//-----------------------------------------------------------------------------

void StudioPaletteViewerPanel::onPaletteSwitched() {
  TApp::instance()->getPaletteController()->setCurrentPalette(
      m_studioPaletteHandle);
}

//-----------------------------------------------------------------------------

void StudioPaletteViewerPanel::setViewType(int viewType) {
  m_studioPaletteViewer->setViewMode(viewType);
}
//-----------------------------------------------------------------------------

int StudioPaletteViewerPanel::getViewType() {
  return m_studioPaletteViewer->getViewMode();
}

//=============================================================================
// StudioPaletteViewerFactory
//-----------------------------------------------------------------------------

class StudioPaletteViewerFactory final : public TPanelFactory {
public:
  StudioPaletteViewerFactory() : TPanelFactory("StudioPalette") {}

  TPanel *createPanel(QWidget *parent) override {
    StudioPaletteViewerPanel *panel = new StudioPaletteViewerPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Studio Palette"));

    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }

} studioPaletteViewerFactory;

//=============================================================================
OpenFloatingPanel openStudioPaletteCommand("MI_OpenStudioPalette",
                                           "StudioPalette",
                                           QObject::tr("Studio Palette"));
//-----------------------------------------------------------------------------

//=============================================================================
// ColorFieldEditorController
//-----------------------------------------------------------------------------

ColorFieldEditorController::ColorFieldEditorController() {
  m_currentColorField = 0;
  m_colorFieldHandle  = new TPaletteHandle();
  m_palette           = new TPalette();
  TColorStyle *style  = m_palette->getStyle(1);
  std::wstring name   = L"EmptyColorFieldPalette";
  m_palette->setPaletteName(name);
  m_colorFieldHandle->setPalette(m_palette.getPointer(), 1);
  DVGui::ColorField::setEditorController(this);
}

//-----------------------------------------------------------------------------

void ColorFieldEditorController::edit(DVGui::ColorField *colorField) {
  if (m_currentColorField) {
    if (m_currentColorField->isEditing())
      m_currentColorField->setIsEditing(false);
    disconnect(m_currentColorField,
               SIGNAL(colorChanged(const TPixel32 &, bool)), this,
               SLOT(onColorChanged(const TPixel32 &, bool)));
  }

  m_currentColorField = colorField;
  m_currentColorField->setIsEditing(true);

  // Setto come stile corrente quello del colorField
  TColorStyle *style = m_palette->getStyle(1);
  style->setMainColor(m_currentColorField->getColor());

  // Setto m_colorFieldHandle come paletteHandle corrente.
  TApp::instance()->getPaletteController()->setCurrentPalette(
      m_colorFieldHandle);

  connect(m_currentColorField, SIGNAL(colorChanged(const TPixel32 &, bool)),
          SLOT(onColorChanged(const TPixel32 &, bool)));
  connect(m_colorFieldHandle, SIGNAL(colorStyleChanged(bool)),
          SLOT(onColorStyleChanged(bool)));
}

//-----------------------------------------------------------------------------

void ColorFieldEditorController::hide() {
  disconnect(m_colorFieldHandle, SIGNAL(colorStyleChanged(bool)), this,
             SLOT(onColorStyleChanged(bool)));
}

//-----------------------------------------------------------------------------

void ColorFieldEditorController::onColorStyleChanged(bool isDragging) {
  if (!m_currentColorField) return;
  assert(!!m_palette);
  TPixel32 color = m_palette->getStyle(1)->getMainColor();
  if (m_currentColorField->getColor() == color && isDragging) return;
  m_currentColorField->setColor(color);
  m_currentColorField->notifyColorChanged(color, isDragging);
}

//-----------------------------------------------------------------------------

void ColorFieldEditorController::onColorChanged(const TPixel32 &color,
                                                bool isDragging) {
  if (!m_currentColorField) return;
  TColorStyle *style = m_palette->getStyle(1);
  if (style->getMainColor() == color) return;
  style->setMainColor(color);
  TApp::instance()
      ->getPaletteController()
      ->getCurrentPalette()
      ->notifyColorStyleChanged(isDragging);
}

//=============================================================================
ColorFieldEditorController colorFieldEditorController;
//-----------------------------------------------------------------------------

CleanupColorFieldEditorController::CleanupColorFieldEditorController()
    : m_currentColorField(0)
    , m_colorFieldHandle(new TPaletteHandle)
    , m_palette(new TPalette) {
  std::wstring name = L"EmptyColorFieldPalette";
  m_palette->setPaletteName(name);
  m_colorFieldHandle->setPalette(m_palette.getPointer(), 1);
  DVGui::CleanupColorField::setEditorController(this);
}

//-----------------------------------------------------------------------------

void CleanupColorFieldEditorController::edit(
    DVGui::CleanupColorField *colorField) {
  if (m_currentColorField && m_currentColorField->isEditing())
    m_currentColorField->setIsEditing(false);

  m_currentColorField = colorField;
  if (!colorField) return;
  m_currentColorField->setIsEditing(true);

  // Setto come stile corrente quello del colorField
  TColorStyle *fieldStyle = colorField->getStyle();

  m_blackColor = dynamic_cast<TBlackCleanupStyle *>(fieldStyle);
  if (m_blackColor) {
    m_palette->setStyle(1, new TSolidColorStyle);
    m_palette->getStyle(1)->setMainColor(fieldStyle->getColorParamValue(1));
  } else {
    m_palette->setStyle(1, new TColorCleanupStyle);

    TColorStyle *style = m_palette->getStyle(1);
    style->setMainColor(fieldStyle->getMainColor());
    style->setColorParamValue(1, fieldStyle->getColorParamValue(1));
  }

  // Setto m_colorFieldHandle come paletteHandle corrente.
  TApp::instance()->getPaletteController()->setCurrentPalette(
      m_colorFieldHandle);

  connect(m_colorFieldHandle, SIGNAL(colorStyleChanged(bool)),
          SLOT(onColorStyleChanged()));
}

//-----------------------------------------------------------------------------

void CleanupColorFieldEditorController::hide() {
  disconnect(m_colorFieldHandle, SIGNAL(colorStyleChanged(bool)), this,
             SLOT(onColorStyleChanged()));
}

//-----------------------------------------------------------------------------

void CleanupColorFieldEditorController::onColorStyleChanged() {
  if (!m_currentColorField) return;

  assert(!!m_palette);

  TColorStyle *style = m_palette->getStyle(1);
  if (m_blackColor) {
    if (m_currentColorField->getOutputColor() == style->getMainColor()) return;

    m_currentColorField->setOutputColor(style->getMainColor());
  } else {
    if (m_currentColorField->getColor() == style->getMainColor() &&
        m_currentColorField->getOutputColor() == style->getColorParamValue(1))
      return;

    m_currentColorField->setStyle(style);
  }
}

//=============================================================================
CleanupColorFieldEditorController cleanupColorFieldEditorController;
//-----------------------------------------------------------------------------

//=============================================================================
// style editor
//-----------------------------------------------------------------------------

StyleEditorPanel::StyleEditorPanel(QWidget *parent) : TPanel(parent) {
  m_styleEditor =
      new StyleEditor(TApp::instance()->getPaletteController(), this);
  setWidget(m_styleEditor);

  m_styleEditor->setLevelHandle(TApp::instance()->getCurrentLevel());
  setMinimumWidth(200);
  resize(340, 630);
}

//-----------------------------------------------------------------------------
void StyleEditorPanel::showEvent(QShowEvent *) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  bool ret = connect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
                     this, SLOT(onPreferenceChanged(const QString &)));
  onPreferenceChanged("ColorCalibration");
  assert(ret);
}
//-----------------------------------------------------------------------------
void StyleEditorPanel::hideEvent(QHideEvent *) {
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  if (sceneHandle) sceneHandle->disconnect(this);
}
//-----------------------------------------------------------------------------
void StyleEditorPanel::onPreferenceChanged(const QString &prefName) {
  if (prefName == "ColorCalibration") m_styleEditor->updateColorCalibration();
}
//-----------------------------------------------------------------------------

class StyleEditorFactory final : public TPanelFactory {
public:
  StyleEditorFactory() : TPanelFactory("StyleEditor") {}

  TPanel *createPanel(QWidget *parent) override {
    StyleEditorPanel *panel = new StyleEditorPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Style Editor"));
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }

} styleEditorFactory;

//=============================================================================
OpenFloatingPanel openStyleEditorCommand(MI_OpenStyleControl, "StyleEditor",
                                         QObject::tr("Style Editor"));
//-----------------------------------------------------------------------------

class ToolbarFactory final : public TPanelFactory {
public:
  ToolbarFactory() : TPanelFactory("ToolBar") {}
  void initialize(TPanel *panel) override {
    Toolbar *toolbar = new Toolbar(panel);
    panel->setWidget(toolbar);
    panel->setIsMaximizable(false);
    // panel->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
    panel->setFixedWidth(44);  // 35
    toolbar->setFixedWidth(34);
    panel->setWindowTitle(QString(""));
  }
} toolbarFactory;

//=========================================================
// Command Bar Panel
//---------------------------------------------------------

//-----------------------------------------------------------------------------
CommandBarPanel::CommandBarPanel(QWidget *parent)
    : TPanel(parent, 0, TDockWidget::horizontal) {
  CommandBar *xsheetToolbar = new CommandBar();
  setWidget(xsheetToolbar);
  setIsMaximizable(false);
  setFixedHeight(36);
}

class CommandBarFactory final : public TPanelFactory {
public:
  CommandBarFactory() : TPanelFactory("CommandBar") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new CommandBarPanel(parent);
    panel->setObjectName(getPanelType());
    return panel;
  }
  void initialize(TPanel *panel) override {}
} commandBarFactory;

//=============================================================================
OpenFloatingPanel openCommandBarCommand(MI_OpenCommandToolbar, "CommandBar",
                                        QObject::tr("Command Bar"));
//-----------------------------------------------------------------------------

//=========================================================
// ToolOptionPanel
//---------------------------------------------------------

ToolOptionPanel::ToolOptionPanel(QWidget *parent)
    : TPanel(parent, 0, TDockWidget::horizontal) {
  TApp *app    = TApp::instance();
  m_toolOption = new ToolOptions;

  setWidget(m_toolOption);
  setIsMaximizable(false);
  setFixedHeight(36);
}

//=============================================================================
// ToolOptionsFactory
//-----------------------------------------------------------------------------

class ToolOptionsFactory final : public TPanelFactory {
  TPanel *m_panel;

public:
  ToolOptionsFactory() : TPanelFactory("ToolOptions") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ToolOptionPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(getPanelType());
    panel->resize(600, panel->height());
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} toolOptionsFactory;

//=============================================================================
OpenFloatingPanel openToolOptionsCommand(MI_OpenToolOptionBar, "ToolOptions",
                                         QObject::tr("Tool Options"));
//-----------------------------------------------------------------------------

//=============================================================================
// FlipbookFactory
//-----------------------------------------------------------------------------

void FlipbookBasePanel::zoomContentsAndFitGeometry(bool forward) {
  if (!isFloating()) return;
  if (!m_flipbook->getImageViewer()->getImage()) {
    TPanel::zoomContentsAndFitGeometry(forward);
    return;
  }
  // resize the window leaving the top-left corner position unchanged
  // in order to gain consistency with Photoshop
  auto getScreen = [&]() {
    QScreen *ret = nullptr;
    ret          = QGuiApplication::screenAt(geometry().topLeft());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().topRight());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().center());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().bottomLeft());
    if (ret) return ret;
    ret = QGuiApplication::screenAt(geometry().bottomRight());
    return ret;
  };
  // Get screen geometry
  QScreen *screen = getScreen();
  if (!screen) return;
  QRect screenGeom  = screen->availableGeometry();
  QPoint oldTopLeft = geometry().topLeft();

  m_flipbook->zoomAndAdaptGeometry(forward);

  QRect newGeom(geometry());
  newGeom.moveTopLeft(oldTopLeft);
  if (newGeom.right() > screenGeom.right())
    newGeom.moveRight(screenGeom.right());
  else if (newGeom.left() < screenGeom.left())
    newGeom.moveLeft(screenGeom.left());
  if (newGeom.bottom() > screenGeom.bottom())
    newGeom.moveBottom(screenGeom.bottom());
  else if (newGeom.top() < screenGeom.top())
    newGeom.moveTop(screenGeom.top());
  setGeometry(newGeom);
}

//-----------------------------------------------------------------------------

FlipbookPanel::FlipbookPanel(QWidget *parent) : FlipbookBasePanel(parent) {
  m_flipbook = new FlipBook(this);
  setWidget(m_flipbook);
  // minimize button and safearea toggle
  initializeTitleBar(getTitleBar());

  MainWindow *mw =
      qobject_cast<MainWindow *>(TApp::instance()->getMainWindow());
  if (mw && mw->getLayoutName() == QString("flip"))
    connect(m_flipbook, SIGNAL(imageLoaded(QString &)), mw,
            SLOT(changeWindowTitle(QString &)));
}

//-----------------------------------------------------------------------------

void FlipbookPanel::reset() { m_flipbook->reset(); }

//-----------------------------------------------------------------------------

void FlipbookPanel::initializeTitleBar(TPanelTitleBar *titleBar) {
  bool ret      = true;
  int x         = -91;
  int iconWidth = 20;
  // safe area button
  TPanelTitleBarButtonForSafeArea *safeAreaButton =
      new TPanelTitleBarButtonForSafeArea(
          titleBar, getIconThemePath("actions/20/pane_safe.svg"));
  safeAreaButton->setToolTip(tr("Safe Area (Right Click to Select)"));
  titleBar->add(QPoint(x, 0), safeAreaButton);
  ret = ret && connect(safeAreaButton, SIGNAL(toggled(bool)),
                       CommandManager::instance()->getAction(MI_SafeArea),
                       SLOT(trigger()));
  ret = ret && connect(CommandManager::instance()->getAction(MI_SafeArea),
                       SIGNAL(triggered(bool)), safeAreaButton,
                       SLOT(setPressed(bool)));
  // sync the initial state
  safeAreaButton->setPressed(
      CommandManager::instance()->getAction(MI_SafeArea)->isChecked());

  x += 28 + iconWidth;
  // minimize button
  m_button = new TPanelTitleBarButton(
      titleBar, getIconThemePath("actions/20/pane_minimize.svg"));
  m_button->setToolTip(tr("Minimize"));
  m_button->setPressed(false);

  titleBar->add(QPoint(x, 0), m_button);
  ret = ret && connect(m_button, SIGNAL(toggled(bool)), this,
                       SLOT(onMinimizeButtonToggled(bool)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void FlipbookPanel::onMinimizeButtonToggled(bool doMinimize) {
  if (!isFloating()) return;

  m_flipbook->minimize(doMinimize);

  if (doMinimize) {
    m_floatingSize = window()->size();
    resize(240, 20);
  } else {
    resize(m_floatingSize);
  }
}

//-----------------------------------------------------------------------------

void FlipbookPanel::onDock(bool docked) { m_button->setVisible(!docked); }

//-----------------------------------------------------------------------------

class FlipbookFactory final : public TPanelFactory {
public:
  FlipbookFactory() : TPanelFactory("FlipBook") {}

  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new FlipbookPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("FlipBook"));
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }
} flipbookFactory;

//=============================================================================
// TasksViewerFactory
//-----------------------------------------------------------------------------

class TasksViewerFactory final : public TPanelFactory {
public:
  TasksViewerFactory() : TPanelFactory("Tasks") {}
  void initialize(TPanel *panel) override {
    panel->setWindowTitle(QObject::tr("Tasks"));
    panel->setWidget(new TasksViewer(panel));
  }
} tasksViewerFactory;

class BatchServersViewerFactory final : public TPanelFactory {
public:
  BatchServersViewerFactory() : TPanelFactory("BatchServers") {}
  void initialize(TPanel *panel) override {
    panel->setWindowTitle(QObject::tr("Batch Servers"));
    panel->setWidget(new BatchServersViewer(panel));
  }
} batchServersViewerFactory;

class BrowserFactory final : public TPanelFactory {
public:
  BrowserFactory() : TPanelFactory("Browser") {}
  void initialize(TPanel *panel) override {
    FileBrowser *browser = new FileBrowser(panel, 0, false, true);
    panel->setWidget(browser);
    panel->setWindowTitle(QObject::tr("File Browser"));
    TFilePath currentProjectFolder =
        TProjectManager::instance()->getCurrentProjectPath().getParentDir();
    browser->setFolder(currentProjectFolder, true);
    browser->enableDoubleClickToOpenScenes();
  }
} browserFactory;

//=============================================================================
// CastViewerFactory
//-----------------------------------------------------------------------------

class CastViewerFactory final : public TPanelFactory {
public:
  CastViewerFactory() : TPanelFactory("SceneCast") {}
  void initialize(TPanel *panel) override {
    panel->setWidget(new CastBrowser(panel));
    panel->setWindowTitle(QObject::tr("Scene Cast"));
  }
} castViewerFactory;

//=============================================================================
// FilmStripFactory
//-----------------------------------------------------------------------------

class FilmStripFactory final : public TPanelFactory {
public:
  FilmStripFactory() : TPanelFactory("FilmStrip") {}
  void initialize(TPanel *panel) override {
    Filmstrip *filmstrip = new Filmstrip(panel);
    panel->setWidget(filmstrip);
    panel->setIsMaximizable(false);
  }
} filmStripFactory;
//=============================================================================
// ExportFactory
//-----------------------------------------------------------------------------

// class ExportFactory final : public TPanelFactory {
// public:
//  ExportFactory() : TPanelFactory("Export") {}
//
//  TPanel *createPanel(QWidget *parent) {
//    ExportPanel *panel = new ExportPanel(parent);
//    panel->setObjectName(getPanelType());
//    panel->setWindowTitle(QObject::tr("Export"));
//    return panel;
//  }
//
//  void initialize(TPanel *panel) { assert(0); }
//} exportFactory;
//
// OpenFloatingPanel openExportPanelCommand(MI_OpenExport, "Export",
//                                         QObject::tr("Export"));

//=============================================================================
// ColorModelViewerFactory
//-----------------------------------------------------------------------------

class ColorModelViewerFactory final : public TPanelFactory {
public:
  ColorModelViewerFactory() : TPanelFactory("ColorModel") {}

  TPanel *createPanel(QWidget *parent) override {
    FlipbookBasePanel *panel     = new FlipbookBasePanel(parent);
    ColorModelViewer *colorModel = new ColorModelViewer(panel);
    panel->setWidget(colorModel);
    panel->setFlipbook(colorModel);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(getPanelType());
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }
} colorModelViewerFactory;

//=============================================================================
// FunctionViewerFactory
//-----------------------------------------------------------------------------

class FunctionViewerFactory final : public TPanelFactory {
public:
  FunctionViewerFactory() : TPanelFactory("FunctionEditor") {}

  TPanel *createPanel(QWidget *parent) override {
    FunctionViewerPanel *panel = new FunctionViewerPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Function Editor"));
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }

} functionViewerFactory;

OpenFloatingPanel openFunctionViewerCommand(MI_OpenFunctionEditor,
                                            "FunctionEditor",
                                            QObject::tr("Function Editor"));

//=============================================================================
// TMessageViewerFactory
//-----------------------------------------------------------------------------

class TMessageViewerFactory final : public TPanelFactory {
public:
  TMessageViewerFactory() : TPanelFactory("TMessage") {}
  void initialize(TPanel *panel) override {
    panel->setWindowTitle(QObject::tr("Message Center"));
    panel->setWidget(new TMessageViewer(panel));
    panel->setMinimumHeight(80);
  }
} TMessageViewerFactory;

OpenFloatingPanel openTMessageCommand(MI_OpenTMessage, "TMessage",
                                      QObject::tr("Message Center"));

//=============================================================================
// ScriptConsolePanelFactory
//-----------------------------------------------------------------------------

class ScriptConsolePanelFactory final : public TPanelFactory {
public:
  ScriptConsolePanelFactory() : TPanelFactory("ScriptConsole") {}

  TPanel *createPanel(QWidget *parent) override {
    ScriptConsolePanel *panel = new ScriptConsolePanel(parent);
    panel->setObjectName(getPanelType());

    // panel->setWindowTitle(QObject::tr("Function Editor"));
    // panel->setMinimumSize(220, 200);
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }
} scriptConsolePanelFactory;

OpenFloatingPanel openTScriptConsoleCommand("MI_OpenScriptConsole",
                                            "ScriptConsole",
                                            QObject::tr("Script Console"));

//=============================================================================
// ComboViewer : Viewer + Toolbar + Tool Options
//-----------------------------------------------------------------------------

ComboViewerPanelContainer::ComboViewerPanelContainer(QWidget *parent)
    : StyleShortcutSwitchablePanel(parent) {
  m_comboViewer = new ComboViewerPanel(parent);
  setFocusProxy(m_comboViewer);
  setWidget(m_comboViewer);

  m_comboViewer->initializeTitleBar(getTitleBar());
  bool ret = connect(m_comboViewer->getToolOptions(), SIGNAL(newPanelCreated()),
                     this, SLOT(updateTabFocus()));
  assert(ret);
}
// reimplementation of TPanel::widgetInThisPanelIsFocused
bool ComboViewerPanelContainer::widgetInThisPanelIsFocused() {
  return m_comboViewer->hasFocus();
}
// reimplementation of TPanel::widgetFocusOnEnter
void ComboViewerPanelContainer::widgetFocusOnEnter() {
  m_comboViewer->onEnterPanel();
}
void ComboViewerPanelContainer::widgetClearFocusOnLeave() {
  m_comboViewer->onLeavePanel();
}

//-----------------------------------------------------------------------------

class ComboViewerFactory final : public TPanelFactory {
public:
  ComboViewerFactory() : TPanelFactory("ComboViewer") {}
  TPanel *createPanel(QWidget *parent) override {
    ComboViewerPanelContainer *panel = new ComboViewerPanelContainer(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Combo Viewer"));
    panel->resize(700, 600);
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} comboViewerFactory;

//=============================================================================
OpenFloatingPanel openComboViewerCommand(MI_OpenComboViewer, "ComboViewer",
                                         QObject::tr("Combo Viewer"));
//-----------------------------------------------------------------------------

//=============================================================================
// SceneViewer
//-----------------------------------------------------------------------------

SceneViewerPanelContainer::SceneViewerPanelContainer(QWidget *parent)
    : StyleShortcutSwitchablePanel(parent) {
  m_sceneViewer = new SceneViewerPanel(parent);
  setFocusProxy(m_sceneViewer);
  setWidget(m_sceneViewer);

  m_sceneViewer->initializeTitleBar(getTitleBar());
}
// reimplementation of TPanel::widgetInThisPanelIsFocused
bool SceneViewerPanelContainer::widgetInThisPanelIsFocused() {
  return m_sceneViewer->hasFocus();
}
// reimplementation of TPanel::widgetFocusOnEnter
void SceneViewerPanelContainer::widgetFocusOnEnter() {
  m_sceneViewer->onEnterPanel();
}
void SceneViewerPanelContainer::widgetClearFocusOnLeave() {
  m_sceneViewer->onLeavePanel();
}

//-----------------------------------------------------------------------------

class SceneViewerFactory final : public TPanelFactory {
public:
  SceneViewerFactory() : TPanelFactory("SceneViewer") {}

  TPanel *createPanel(QWidget *parent) override {
    SceneViewerPanelContainer *panel = new SceneViewerPanelContainer(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Viewer"));
    panel->setMinimumSize(220, 280);
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} sceneViewerFactory;

//=============================================================================
OpenFloatingPanel openSceneViewerCommand(MI_OpenLevelView, "SceneViewer",
                                         QObject::tr("Viewer"));
//-----------------------------------------------------------------------------

//=============================================================================
// CleanupSettings DockWindow
//-----------------------------------------------------------------------------

class CleanupSettingsFactory final : public TPanelFactory {
public:
  CleanupSettingsFactory() : TPanelFactory("CleanupSettings") {}

  void initialize(TPanel *panel) override {
    panel->setWidget(new CleanupSettingsPane(panel));
    panel->setIsMaximizable(false);
  }

} cleanupSettingsFactory;

//=============================================================================
OpenFloatingPanel openCleanupSettingsDockCommand(
    MI_OpenCleanupSettings, "CleanupSettings", QObject::tr("Cleanup Settings"));
//-----------------------------------------------------------------------------

//=============================================================================
// History
//=============================================================================

//-----------------------------------------------------------------------------

class HistoryPanelFactory final : public TPanelFactory {
public:
  HistoryPanelFactory() : TPanelFactory("HistoryPanel") {}
  void initialize(TPanel *panel) override {
    HistoryPane *historyPane = new HistoryPane(panel);
    panel->setWidget(historyPane);
    panel->setWindowTitle(QObject::tr("History"));
    panel->setIsMaximizable(false);
  }
} historyPanelFactory;

OpenFloatingPanel openHistoryPanelCommand(MI_OpenHistoryPanel, "HistoryPanel",
                                          QObject::tr("History"));
//=============================================================================

#if defined(x64)
//=============================================================================
// StopMotion Controller
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

class StopMotionPanelFactory final : public TPanelFactory {
public:
  StopMotionPanelFactory() : TPanelFactory("StopMotionController") {}

  void initialize(TPanel *panel) override {
    StopMotionController *stopMotionController =
        new StopMotionController(panel);
    panel->setWidget(stopMotionController);
    panel->setWindowTitle(QObject::tr("Stop Motion Controller"));
    panel->setIsMaximizable(false);
  }
} stopMotionPanelFactory;

//=============================================================================
OpenFloatingPanel openStopMotionPanelCommand(
    MI_OpenStopMotionPanel, "StopMotionController",
    QObject::tr("Stop Motion Controller"));
//-----------------------------------------------------------------------------

#endif  // x64

//=============================================================================
// FxSettings
//-----------------------------------------------------------------------------

FxSettingsPanel::FxSettingsPanel(QWidget *parent) : TPanel(parent) {
  TApp *app            = TApp::instance();
  TSceneHandle *hScene = app->getCurrentScene();
  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);

  m_fxSettings = new FxSettings(this, col1, col2);
  m_fxSettings->setSceneHandle(hScene);
  m_fxSettings->setFxHandle(app->getCurrentFx());
  m_fxSettings->setFrameHandle(app->getCurrentFrame());
  m_fxSettings->setXsheetHandle(app->getCurrentXsheet());
  m_fxSettings->setLevelHandle(app->getCurrentLevel());
  m_fxSettings->setObjectHandle(app->getCurrentObject());

  m_fxSettings->setCurrentFx();

  setWidget(m_fxSettings);
}

// FxSettings will adjust its size according to the current fx
// so we only restore position of the panel.
void FxSettingsPanel::restoreFloatingPanelState() {
  TFilePath savePath = ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
  QSettings settings(QString::fromStdWString(savePath.getWideString()),
                     QSettings::IniFormat);
  settings.beginGroup("Panels");

  if (!settings.childGroups().contains("FxSettings")) return;

  settings.beginGroup("FxSettings");

  QRect geom = settings.value("geometry", saveGeometry()).toRect();
  // check if it can be visible in the current screen
  if (!(geom & QApplication::desktop()->availableGeometry(this)).isEmpty())
    move(geom.topLeft());

  // FxSettings has no optional settings (SaveLoadQSettings) to load
}

//=============================================================================
// FxSettingsFactory
//-----------------------------------------------------------------------------

class FxSettingsFactory final : public TPanelFactory {
public:
  FxSettingsFactory() : TPanelFactory("FxSettings") {}

  TPanel *createPanel(QWidget *parent) override {
    FxSettingsPanel *panel = new FxSettingsPanel(parent);
    panel->move(qApp->desktop()->screenGeometry(panel).center());
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Fx Settings"));
    panel->setMinimumSize(390, 85);
    panel->allowMultipleInstances(false);
    return panel;
  }

  void initialize(TPanel *panel) override { assert(0); }

} FxSettingsFactory;

//=============================================================================
OpenFloatingPanel openFxSettingsCommand(MI_FxParamEditor, "FxSettings",
                                        QObject::tr("Fx Settings"));

//=========================================================
// VectorGuidedDrawingPanel
//---------------------------------------------------------

//-----------------------------------------------------------------------------
VectorGuidedDrawingPanel::VectorGuidedDrawingPanel(QWidget *parent)
    : TPanel(parent) {
  VectorGuidedDrawingPane *pane = new VectorGuidedDrawingPane(this);
  setWidget(pane);
  setIsMaximizable(false);
}

//=============================================================================
// VectorGuidedDrawingFactory
//-----------------------------------------------------------------------------

class VectorGuidedDrawingFactory final : public TPanelFactory {
public:
  VectorGuidedDrawingFactory() : TPanelFactory("VectorGuidedDrawingPanel") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new VectorGuidedDrawingPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Vector Guided Drawing Controls"));
    panel->setMinimumSize(387, 265);

    return panel;
  }
  void initialize(TPanel *panel) override {}
} VectorGuidedDrawingFactory;

//=============================================================================
OpenFloatingPanel openVectorGuidedDrawingPanelCommand(
    MI_OpenGuidedDrawingControls, "VectorGuidedDrawingPanel",
    QObject::tr("Vector Guided Drawing"));

//-----------------------------------------------------------------------------

namespace {

void zoomAndFitPanel(bool forward) {
  TPanel *panel = dynamic_cast<TPanel *>(qApp->activeWindow());
  if (panel) panel->zoomContentsAndFitGeometry(forward);
}

}  // namespace

class ZoomInAndFitPanel final : public MenuItemHandler {
public:
  ZoomInAndFitPanel() : MenuItemHandler("MI_ZoomInAndFitPanel") {}
  void execute() override { zoomAndFitPanel(true); }
} zoomInAndFitPanel;

class ZoomOutAndFitPanel final : public MenuItemHandler {
public:
  ZoomOutAndFitPanel() : MenuItemHandler("MI_ZoomOutAndFitPanel") {}
  void execute() override { zoomAndFitPanel(false); }
} zoomOutAndFitPanel;