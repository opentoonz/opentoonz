#include "styleshortcutswitchablepanel.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/styleselection.h"

// Tnz6 includes
#include "tapp.h"

#include <QKeyEvent>

//-----------------------------------------------------------------------------

void StyleShortcutSwitchablePanel::keyPressEvent(QKeyEvent *event) {
  if (!Preferences::instance()->isUseNumpadForSwitchingStylesEnabled()) return;
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool) return;
  if (tool->getName() == T_Type && tool->isActive()) return;
  if (event->modifiers() != Qt::NoModifier &&
      event->modifiers() != Qt::KeypadModifier)
    return;
  int key = event->key();
  if (Qt::Key_0 <= key && key <= Qt::Key_9) {
    TPaletteHandle *ph =
        TApp::instance()->getPaletteController()->getCurrentLevelPalette();

    TPalette *palette = ph->getPalette();
    if (palette) {
      int styleId = palette->getShortcutValue(key);
      if (styleId >= 0) {
        ph->setStyleIndex(styleId);
        TStyleSelection *selection = dynamic_cast<TStyleSelection *>(
            TApp::instance()->getCurrentSelection()->getSelection());
        if (selection) selection->selectNone();
      }
    }
    event->accept();
  } else if (key == Qt::Key_Tab || key == Qt::Key_Backtab) {
    TPaletteHandle *ph =
        TApp::instance()->getPaletteController()->getCurrentLevelPalette();

    TPalette *palette = ph->getPalette();
    if (palette) {
      palette->nextShortcutScope(key == Qt::Key_Backtab);
      ph->notifyPaletteChanged();
    }
    event->accept();
  }
}

//-----------------------------------------------------------------------------

void StyleShortcutSwitchablePanel::showEvent(QShowEvent *event) {
  TPanel::showEvent(event);
  bool ret = connect(TApp::instance()->getCurrentScene(),
                     SIGNAL(preferenceChanged(const QString &)), this,
                     SLOT(onPreferenceChanged(const QString &)));
  onPreferenceChanged("");
  assert(ret);
}

//-----------------------------------------------------------------------------

void StyleShortcutSwitchablePanel::hideEvent(QHideEvent *event) {
  TPanel::hideEvent(event);
  disconnect(TApp::instance()->getCurrentScene(),
             SIGNAL(preferenceChanged(const QString &)), this,
             SLOT(onPreferenceChanged(const QString &)));
}

//-----------------------------------------------------------------------------

void StyleShortcutSwitchablePanel::onPreferenceChanged(
    const QString &prefName) {
  if (prefName == "NumpadForSwitchingStyles" || prefName.isEmpty())
    updateTabFocus();
}

//-----------------------------------------------------------------------------

void StyleShortcutSwitchablePanel::updateTabFocus() {
  QList<QWidget *> widgets = findChildren<QWidget *>();
  if (Preferences::instance()->isUseNumpadForSwitchingStylesEnabled()) {
    // disable tab focus
    for (QWidget *widget : widgets) {
      Qt::FocusPolicy policy = widget->focusPolicy();
      if (policy == Qt::TabFocus || policy == Qt::StrongFocus ||
          policy == Qt::WheelFocus) {
        m_childrenFocusPolicies[widget] = policy;
        widget->setFocusPolicy((policy == Qt::TabFocus) ? Qt::NoFocus
                                                        : Qt::ClickFocus);
      }
    }
  } else {
    // revert tab focus
    QHashIterator<QWidget *, Qt::FocusPolicy> i(m_childrenFocusPolicies);
    while (i.hasNext()) {
      i.next();
      i.key()->setFocusPolicy(i.value());
    }
  }
}