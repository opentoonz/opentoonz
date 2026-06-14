
#include "sceneviewercontextmenu.h"

#include "menubarcommandids.h"
#include "sceneviewer.h"

#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"

#include "toonzqt/menubarcommand.h"

#include "tapp.h"
#include "tvectorimage.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QObject>

namespace {

QString menuLabel(bool showOn) {
  return showOn ? QObject::tr("Hide Line: Show Invisible/Hidden Strokes")
                : QObject::tr("Hide Line: Show Invisible/Hidden Strokes");
}

bool menuContainsAction(QMenu *menu, QAction *action) {
  if (!menu || !action) return false;
  for (QAction *a : menu->actions()) {
    if (a == action) return true;
    if (QMenu *sub = a->menu()) {
      if (menuContainsAction(sub, action)) return true;
    }
  }
  return false;
}

QMenu *findMenuContainingCommand(QMenu *menu, const char *cmdId) {
  if (!menu) return nullptr;
  QAction *target = CommandManager::instance()->getAction(cmdId);
  if (!target) return nullptr;
  for (QAction *a : menu->actions()) {
    if (a == target) return menu;
    if (QMenu *sub = a->menu()) {
      if (QMenu *found = findMenuContainingCommand(sub, cmdId)) return found;
    }
  }
  return nullptr;
}

QMenu *findViewMenu(QMenuBar *menuBar) {
  if (!menuBar) return nullptr;
  for (QAction *top : menuBar->actions()) {
    if (QMenu *found =
            findMenuContainingCommand(top->menu(), MI_RasterizePli))
      return found;
    if (QMenu *found =
            findMenuContainingCommand(top->menu(), MI_VectorGuidedDrawing))
      return found;
  }
  return nullptr;
}

void refreshViewer(SceneViewer *viewer) {
  TApp *app = TApp::instance();
  if (!app) return;
  if (TSceneHandle *sh = app->getCurrentScene()) sh->notifySceneChanged(false);
  if (TXshLevelHandle *lh = app->getCurrentLevel())
    lh->notifyLevelViewChange();
  if (viewer)
    viewer->GLInvalidateAll();
  else if (SceneViewer *active = app->getActiveViewer())
    active->GLInvalidateAll();
}

}  // namespace

namespace HideLineStrokeGui {

void syncCommandActionLabel() {
  QAction *action =
      CommandManager::instance()->getAction(MI_ShowHideLineStrokes);
  if (!action) return;
  const bool on = Preferences::instance()->getShowHideLineStrokes();
  action->setCheckable(true);
  action->setText(menuLabel(on));
  if (action->isChecked() != on) {
    action->blockSignals(true);
    action->setChecked(on);
    action->blockSignals(false);
  }
}

void setShowEnabled(bool on, SceneViewer *viewer) {
  Preferences::instance()->setValue(showHideLineStrokes, on);
  syncCommandActionLabel();
  refreshViewer(viewer);
}

void ensureViewMenuEntry(QMenuBar *menuBar) {
  if (!menuBar) return;
  QAction *showAction =
      CommandManager::instance()->getAction(MI_ShowHideLineStrokes);
  if (!showAction) return;
  for (QAction *top : menuBar->actions()) {
    if (top->menu() && menuContainsAction(top->menu(), showAction)) {
      return;
    }
  }

  QMenu *viewMenu = findViewMenu(menuBar);
  if (!viewMenu) return;

  QAction *before = CommandManager::instance()->getAction(MI_RasterizePli);
  if (before && viewMenu->actions().contains(before))
    viewMenu->insertAction(before, showAction);
  else
    viewMenu->addAction(showAction);

  syncCommandActionLabel();
}

}  // namespace HideLineStrokeGui

class ShowHideLineStrokesToggle final : public MenuItemHandler {
public:
  ShowHideLineStrokesToggle() : MenuItemHandler(MI_ShowHideLineStrokes) {}
  void execute() override {
    QAction *action =
        CommandManager::instance()->getAction(MI_ShowHideLineStrokes);
    if (!action) return;
    HideLineStrokeGui::setShowEnabled(action->isChecked(), nullptr);
  }
} showHideLineStrokesToggle;
