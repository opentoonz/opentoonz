

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
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QPushButton>
#include <QWidgetAction>

//=============================================================================

namespace XsheetGUI {

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
XSheetToolbar::XSheetToolbar(XsheetViewer *parent, Qt::WindowFlags flags,
                             bool isCollapsible)
#else
XSheetToolbar::XSheetToolbar(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QToolBar(parent), m_viewer(parent), m_isCollapsible(isCollapsible) {
  setObjectName("cornerWidget");
  setFixedHeight(30);
  setObjectName("XSheetToolbar");

  TApp *app        = TApp::instance();
  m_keyFrameButton = new ViewerKeyframeNavigator(this, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  QWidgetAction *keyFrameAction = new QWidgetAction(this);
  keyFrameAction->setDefaultWidget(m_keyFrameButton);

  {
    QAction *newVectorLevel =
        CommandManager::instance()->getAction("MI_NewVectorLevel");
    addAction(newVectorLevel);
    QAction *newToonzRasterLevel =
        CommandManager::instance()->getAction("MI_NewToonzRasterLevel");
    addAction(newToonzRasterLevel);
    QAction *newRasterLevel =
        CommandManager::instance()->getAction("MI_NewRasterLevel");
    addAction(newRasterLevel);
    addSeparator();
    QAction *reframeOnes = CommandManager::instance()->getAction("MI_Reframe1");
    addAction(reframeOnes);
    QAction *reframeTwos = CommandManager::instance()->getAction("MI_Reframe2");
    addAction(reframeTwos);
    QAction *reframeThrees =
        CommandManager::instance()->getAction("MI_Reframe3");
    addAction(reframeThrees);

    addSeparator();

    QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
    addAction(repeat);

    addSeparator();

    QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
    addAction(collapse);
    QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
    addAction(open);
    QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
    addAction(leave);

    addSeparator();
    addAction(keyFrameAction);

    if (!Preferences::instance()->isShowXSheetToolbarEnabled() &&
        m_isCollapsible) {
      hide();
    }
  }
}

//-----------------------------------------------------------------------------

void XSheetToolbar::showToolbar(bool show) {
  if (!m_isCollapsible) return;
  show ? this->show() : this->hide();
}

//-----------------------------------------------------------------------------

void XSheetToolbar::toggleXSheetToolbar() {
  bool toolbarEnabled = Preferences::instance()->isShowXSheetToolbarEnabled();
  Preferences::instance()->enableShowXSheetToolbar(!toolbarEnabled);
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("XSheetToolbar");
}

//-----------------------------------------------------------------------------

void XSheetToolbar::showEvent(QShowEvent *e) {
  if (Preferences::instance()->isShowXSheetToolbarEnabled() || !m_isCollapsible)
    show();
  else
    hide();
  emit updateVisibility();
}

//============================================================

class ToggleXSheetToolbarCommand final : public MenuItemHandler {
public:
  ToggleXSheetToolbarCommand() : MenuItemHandler(MI_ToggleXSheetToolbar) {}
  void execute() override { XSheetToolbar::toggleXSheetToolbar(); }
} ToggleXSheetToolbarCommand;

//============================================================

}  // namespace XsheetGUI;
