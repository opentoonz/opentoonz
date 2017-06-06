

#include "xshtoolbar.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"

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
      QAction *reframeOnes =
          CommandManager::instance()->getAction("MI_Reframe1");
      m_toolbar->addAction(reframeOnes);
      QAction *reframeTwos =
          CommandManager::instance()->getAction("MI_Reframe2");
      m_toolbar->addAction(reframeTwos);
      QAction *reframeThrees =
          CommandManager::instance()->getAction("MI_Reframe3");
      m_toolbar->addAction(reframeThrees);

      m_toolbar->addSeparator();

      QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
      m_toolbar->addAction(repeat);

      m_toolbar->addSeparator();

      QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
      m_toolbar->addAction(collapse);
      QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
      m_toolbar->addAction(open);
      QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
      m_toolbar->addAction(leave);

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
  assert(ret);

  // m_leaveSubButton->hide();
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

void Toolbar::showToolbar(bool show) {
  show ? m_toolbar->show() : m_toolbar->hide();
}

//-----------------------------------------------------------------------------

void Toolbar::toggleXSheetToolbar() {
  bool toolbarEnabled = Preferences::instance()->isShowXSheetToolbarEnabled();
  Preferences::instance()->enableShowXSheetToolbar(!toolbarEnabled);
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("XSheetToolbar");
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
