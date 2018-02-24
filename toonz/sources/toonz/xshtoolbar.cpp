#include "xshtoolbar.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "commandbarpopup.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/childstack.h"

// Qt includes
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
    : CommandBar(parent, flags, isCollapsible, true), m_viewer(parent) {
  setObjectName("cornerWidget");
  setFixedHeight(30);
  setObjectName("XSheetToolbar");
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

//-----------------------------------------------------------------------------

void XSheetToolbar::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = new QMenu(this);
  QAction *customizeCommandBar =
      menu->addAction(tr("Customize XSheet Toolbar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void XSheetToolbar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup(true);

  if (cbPopup->exec()) {
    fillToolbar(this, true);
  }
  delete cbPopup;
}

//============================================================

class ToggleXSheetToolbarCommand final : public MenuItemHandler {
public:
  ToggleXSheetToolbarCommand() : MenuItemHandler(MI_ToggleXSheetToolbar) {}
  void execute() override { XSheetToolbar::toggleXSheetToolbar(); }
} ToggleXSheetToolbarCommand;

//============================================================

}  // namespace XsheetGUI;
