// TnzQt includes
#include "toonzqt/imageutils.h"
// TnzTools includes
#include "tools/tooloptions.h"
// Tnz6 includes
#include "tapp.h"
#include "toolbar.h"
#include "ruler.h"
#include "menubarcommandids.h"
// Qt includes
#include <QVBoxLayout>
#include <QGridLayout>
#include <QAction>
#include <QMenu>
#include <QSettings>

#include "comboviewerpane.h"

// this enum is to keep comaptibility with older versions
enum CV_Parts {
  CVPARTS_None        = 0,
  CVPARTS_TOOLBAR     = 0x1,
  CVPARTS_TOOLOPTIONS = 0x2,
  CVPARTS_FLIPCONSOLE = 0x4,
  CVPARTS_End         = 0x8,
  CVPARTS_ALL = CVPARTS_TOOLBAR | CVPARTS_TOOLOPTIONS | CVPARTS_FLIPCONSOLE
};
//=============================================================================
//
// ComboViewerPanel
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

ComboViewerPanel::ComboViewerPanel(QWidget *parent, Qt::WindowFlags flags)
    : BaseViewerPanel(parent, flags) {
  setObjectName("ComboViewerPanel");

  TApp *app = TApp::instance();

  // ToolBar
  m_toolbar = new Toolbar(this, false);
  // Tool Options
  m_toolOptions = new ToolOptions();
  m_toolOptions->setObjectName("ComboViewerToolOptions");
  // Rulers
  m_vRuler = new Ruler(this, m_sceneViewer, true);
  m_hRuler = new Ruler(this, m_sceneViewer, false);
  m_sceneViewer->setRulers(m_vRuler, m_hRuler);

  /* --- layout --- */
  {
    m_mainLayout->insertWidget(0, m_toolbar, 0);
    m_mainLayout->insertWidget(1, m_toolOptions, 0);

    QGridLayout *viewerL = new QGridLayout();
    viewerL->setContentsMargins(0, 0, 0, 0);
    viewerL->setSpacing(0);
    {
      viewerL->addWidget(m_vRuler, 1, 0);
      viewerL->addWidget(m_hRuler, 0, 1);
      viewerL->addWidget(m_fsWidget, 1, 1);
    }
    viewerL->setRowStretch(1, 1);
    viewerL->setColumnStretch(1, 1);
    m_mainLayout->insertLayout(2, viewerL, 1);
  }
  setLayout(m_mainLayout);

  bool ret = true;

  ret = ret && connect(m_sceneViewer, SIGNAL(previewToggled()),
                       SLOT(changeWindowTitle()));
  assert(ret);

  // initial state of the parts
  m_visiblePartsFlag = VPPARTS_COMBO_ALL;
  updateShowHide();
}

//-----------------------------------------------------------------------------
/*! toggle show/hide of the widgets according to m_visibleFlag
 */

void ComboViewerPanel::updateShowHide() {
  // toolbar
  m_toolbar->setVisible(m_visiblePartsFlag & VPPARTS_TOOLBAR);
  // tool options bar
  m_toolOptions->setVisible(m_visiblePartsFlag & VPPARTS_TOOLOPTIONS);
  // flip console
  BaseViewerPanel::updateShowHide();
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::addShowHideContextMenu(QMenu *menu) {
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));
  // actions
  QAction *toolbarSHAct     = showHideMenu->addAction(tr("Toolbar"));
  QAction *toolOptionsSHAct = showHideMenu->addAction(tr("Tool Options Bar"));
  QAction *playbarSHAct     = showHideMenu->addAction(tr("Playback Toolbar"));
  QAction *frameSliderSHAct = showHideMenu->addAction(tr("Frame Slider"));

  toolbarSHAct->setCheckable(true);
  toolbarSHAct->setChecked(m_visiblePartsFlag & VPPARTS_TOOLBAR);
  toolbarSHAct->setData((UINT)VPPARTS_TOOLBAR);

  toolOptionsSHAct->setCheckable(true);
  toolOptionsSHAct->setChecked(m_visiblePartsFlag & VPPARTS_TOOLOPTIONS);
  toolOptionsSHAct->setData((UINT)VPPARTS_TOOLOPTIONS);

  playbarSHAct->setCheckable(true);
  playbarSHAct->setChecked(m_visiblePartsFlag & VPPARTS_PLAYBAR);
  playbarSHAct->setData((UINT)VPPARTS_PLAYBAR);

  frameSliderSHAct->setCheckable(true);
  frameSliderSHAct->setChecked(m_visiblePartsFlag & VPPARTS_FRAMESLIDER);
  frameSliderSHAct->setData((UINT)VPPARTS_FRAMESLIDER);

  QActionGroup *showHideActGroup = new QActionGroup(this);
  showHideActGroup->setExclusive(false);
  showHideActGroup->addAction(toolbarSHAct);
  showHideActGroup->addAction(toolOptionsSHAct);
  showHideActGroup->addAction(playbarSHAct);
  showHideActGroup->addAction(frameSliderSHAct);

  connect(showHideActGroup, SIGNAL(triggered(QAction *)), this,
          SLOT(onShowHideActionTriggered(QAction *)));

  showHideMenu->addSeparator();
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewCamera));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewTable));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_FieldGuide));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_LayoutGuide));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewBBox));
  showHideMenu->addAction(
      CommandManager::instance()->getAction(MI_ViewColorcard));
  showHideMenu->addAction(CommandManager::instance()->getAction(MI_ViewRuler));
}

//-----------------------------------------------------------------------------

void ComboViewerPanel::checkOldVersionVisblePartsFlags(QSettings &settings) {
  if (settings.contains("viewerVisibleParts") ||
      !settings.contains("visibleParts"))
    return;
  UINT oldVisiblePartsFlag =
      settings.value("visibleParts", CVPARTS_ALL).toUInt();
  m_visiblePartsFlag = VPPARTS_None;
  if (oldVisiblePartsFlag & CVPARTS_TOOLBAR)
    m_visiblePartsFlag |= VPPARTS_TOOLBAR;
  if (oldVisiblePartsFlag & CVPARTS_TOOLOPTIONS)
    m_visiblePartsFlag |= VPPARTS_TOOLOPTIONS;
  if (oldVisiblePartsFlag & CVPARTS_FLIPCONSOLE)
    m_visiblePartsFlag |= VPPARTS_PLAYBAR | VPPARTS_FRAMESLIDER;
}