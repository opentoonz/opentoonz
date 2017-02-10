

#include "xshtoolbar.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"

// Qt includes
#include <QPushButton>

//=============================================================================

namespace XsheetGUI {

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
Toolbar::Toolbar(XsheetViewer *parent, Qt::WindowFlags flags)
#else
Toolbar::Toolbar(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QFrame(parent), m_viewer(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("cornerWidget");
  m_toolbar = new QToolBar();
  m_toolbar->setFixedHeight(30);
  m_toolbar->setObjectName("XSheetToolbar");

  m_newVectorLevelButton = new QPushButton(this);
  m_newVectorLevelButton->setIconSize(QSize(18, 18));
  QIcon newVectorIcon = createQIconPNG("new_vector_level");
  m_newVectorLevelButton->setIcon(newVectorIcon);
  m_newVectorLevelButton->setObjectName("XSheetToolbarLevelButton");
  m_newVectorLevelButton->setToolTip(tr("New Vector Level"));

  m_newToonzRasterLevelButton = new QPushButton(this);
  m_newToonzRasterLevelButton->setIconSize(QSize(18, 18));
  QIcon newToonzRasterIcon = createQIconPNG("new_toonz_raster_level");
  m_newToonzRasterLevelButton->setIcon(newToonzRasterIcon);
  m_newToonzRasterLevelButton->setObjectName("XSheetToolbarLevelButton");
  m_newToonzRasterLevelButton->setToolTip(tr("New Toonz Raster Level"));

  m_newRasterLevelButton = new QPushButton(this);
  m_newRasterLevelButton->setIconSize(QSize(18, 18));
  QIcon newRasterIcon = createQIconPNG("new_raster_level");
  m_newRasterLevelButton->setIcon(newRasterIcon);
  m_newRasterLevelButton->setObjectName("XSheetToolbarLevelButton");
  m_newRasterLevelButton->setToolTip(tr("New Raster Level"));

  m_reframe1sButton = new QPushButton(tr("1's"), this);
  m_reframe1sButton->setToolTip(tr("Reframe on 1's"));
  m_reframe1sButton->setFixedHeight(25);
  m_reframe1sButton->setObjectName("XSheetToolbarButton");

  m_reframe2sButton = new QPushButton(tr("2's"), this);
  m_reframe2sButton->setToolTip(tr("Reframe on 2's"));
  m_reframe2sButton->setFixedHeight(25);
  m_reframe2sButton->setObjectName("XSheetToolbarButton");

  m_reframe3sButton = new QPushButton(tr("3's"), this);
  m_reframe3sButton->setToolTip(tr("Reframe on 3's"));
  m_reframe3sButton->setFixedHeight(25);
  m_reframe3sButton->setObjectName("XSheetToolbarButton");

  m_repeatButton = new QPushButton(this);
  m_repeatButton->setMinimumWidth(26);
  m_repeatButton->setIconSize(QSize(18, 18));
  QIcon repeatIcon = createQIconPNG("repeat_icon");
  m_repeatButton->setIcon(repeatIcon);
  m_repeatButton->setObjectName("XSheetToolbarButton");
  m_repeatButton->setToolTip(tr("Repeat Selection"));

  m_collapseSubButton = new QPushButton(this);
  m_collapseSubButton->setIconSize(QSize(37, 19));
  QIcon collapseSubIcon = createQIconPNG("collapse");
  m_collapseSubButton->setIcon(collapseSubIcon);
  m_collapseSubButton->setObjectName("XSheetToolbarLevelButton");
  m_collapseSubButton->setToolTip(tr("Collapse into Sub-XSheet"));

  m_enterSubButton = new QPushButton(this);
  m_enterSubButton->setIconSize(QSize(19, 19));
  QIcon enterSubIcon = createQIconPNG("sub_enter");
  m_enterSubButton->setIcon(enterSubIcon);
  m_enterSubButton->setObjectName("XSheetToolbarLevelButton");
  m_enterSubButton->setToolTip(tr("Open Sub-XSheet"));

  m_leaveSubButton = new QPushButton(this);
  m_leaveSubButton->setIconSize(QSize(19, 19));
  QIcon leaveSubIcon = createQIconPNG("sub_leave");
  m_leaveSubButton->setIcon(leaveSubIcon);
  m_leaveSubButton->setObjectName("XSheetToolbarLevelButton");
  m_leaveSubButton->setToolTip(tr("Close Sub-XSheet"));

  TApp *app        = TApp::instance();
  m_keyFrameButton = new ViewerKeyframeNavigator(this, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  QVBoxLayout *mainLay = new QVBoxLayout();
  mainLay->setMargin(0);
  mainLay->setSpacing(5);
  {
    mainLay->addStretch(1);
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(2);
    toolbarLayout->setMargin(0);
    {
      m_toolbar->addWidget(m_newVectorLevelButton);
      m_toolbar->addWidget(m_newToonzRasterLevelButton);
      m_toolbar->addWidget(m_newRasterLevelButton);
      m_toolbar->addSeparator();
      m_toolbar->addWidget(m_reframe1sButton);
      m_toolbar->addWidget(m_reframe2sButton);
      m_toolbar->addWidget(m_reframe3sButton);
      m_toolbar->addSeparator();
      m_toolbar->addWidget(m_repeatButton);
      m_toolbar->addSeparator();
      m_toolbar->addWidget(m_collapseSubButton);
      m_toolbar->addWidget(m_enterSubButton);
      m_toolbar->addWidget(m_leaveSubButton);
      m_toolbar->addSeparator();
      m_toolbar->addWidget(m_keyFrameButton);
      toolbarLayout->addWidget(m_toolbar);
      toolbarLayout->addStretch(0);
    }
    mainLay->addLayout(toolbarLayout, 0);
    if (!Preferences::instance()->isShowXSheetToolbarEnabled()) {
      m_toolbar->hide();
    }

    mainLay->addStretch(1);
  }
  setLayout(mainLay);

  // signal-slot connections
  bool ret = true;
  ret      = ret && connect(m_newVectorLevelButton, SIGNAL(released()), this,
                       SLOT(onNewVectorLevelButtonPressed()));
  ret = ret && connect(m_newToonzRasterLevelButton, SIGNAL(released()), this,
                       SLOT(onNewToonzRasterLevelButtonPressed()));
  ret = ret && connect(m_newRasterLevelButton, SIGNAL(released()), this,
                       SLOT(onNewRasterLevelButtonPressed()));
  ret = ret && connect(m_reframe1sButton, SIGNAL(released()), this,
                       SLOT(onReframe1sButtonPressed()));
  ret = ret && connect(m_reframe2sButton, SIGNAL(released()), this,
                       SLOT(onReframe2sButtonPressed()));
  ret = ret && connect(m_reframe3sButton, SIGNAL(released()), this,
                       SLOT(onReframe3sButtonPressed()));
  ret = ret && connect(m_repeatButton, SIGNAL(released()), this,
                       SLOT(onRepeatButtonPressed()));
  ret = ret && connect(m_collapseSubButton, SIGNAL(released()), this,
                       SLOT(onCollapseSubButtonPressed()));
  ret = ret && connect(m_enterSubButton, SIGNAL(released()), this,
                       SLOT(onEnterSubButtonPressed()));
  ret = ret && connect(m_leaveSubButton, SIGNAL(released()), this,
                       SLOT(onLeaveSubButtonPressed()));

  assert(ret);

  m_leaveSubButton->hide();
}

//-----------------------------------------------------------------------------

void Toolbar::onNewVectorLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(PLI_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void Toolbar::onNewToonzRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  // Preferences::instance()->setOldDefLevelType(defaultLevelType);
  Preferences::instance()->setDefLevelType(TZP_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void Toolbar::onNewRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setDefLevelType(OVL_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setDefLevelType(defaultLevelType);
}

//-----------------------------------------------------------------------------

void Toolbar::onReframe1sButtonPressed() {
  CommandManager::instance()->execute("MI_Reframe1");
}

//-----------------------------------------------------------------------------

void Toolbar::onReframe2sButtonPressed() {
  CommandManager::instance()->execute("MI_Reframe2");
}

//-----------------------------------------------------------------------------

void Toolbar::onReframe3sButtonPressed() {
  CommandManager::instance()->execute("MI_Reframe3");
}

//-----------------------------------------------------------------------------

void Toolbar::onRepeatButtonPressed() {
  CommandManager::instance()->execute("MI_Dup");
}

//-----------------------------------------------------------------------------

void Toolbar::onCollapseSubButtonPressed() {
  CommandManager::instance()->execute("MI_Collapse");
}

//-----------------------------------------------------------------------------

void Toolbar::onEnterSubButtonPressed() {
  CommandManager::instance()->execute("MI_OpenChild");
}

//-----------------------------------------------------------------------------

void Toolbar::onLeaveSubButtonPressed() {
  CommandManager::instance()->execute("MI_CloseChild");
}

//-----------------------------------------------------------------------------

void Toolbar::showToolbar(bool show) {
  show ? m_toolbar->show() : m_toolbar->hide();
}

//-----------------------------------------------------------------------------

void Toolbar::toggleXSheetToolbar() {
  bool toolbarEnabled = Preferences::instance()->isShowXSheetToolbarEnabled();
  Preferences::instance()->enableShowXSheetToolbar(!toolbarEnabled);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

//============================================================

class ToggleXSheetToolbarCommand final : public MenuItemHandler {
public:
  ToggleXSheetToolbarCommand() : MenuItemHandler(MI_ToggleXSheetToolbar) {}
  void execute() override { Toolbar::toggleXSheetToolbar(); }
} ToggleXSheetToolbarCommand;

//============================================================

}  // namespace XsheetGUI;
