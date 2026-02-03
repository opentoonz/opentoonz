

#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"

//=============================================================================
// TSelectionHandle
//-----------------------------------------------------------------------------

TSelectionHandle::TSelectionHandle() { m_selectionStack.push_back(0); }

//-----------------------------------------------------------------------------

TSelectionHandle::~TSelectionHandle() {}

//-----------------------------------------------------------------------------

TSelection *TSelectionHandle::getSelection() const {
  return m_selectionStack.back();
}

//-----------------------------------------------------------------------------

void TSelectionHandle::setSelection(TSelection *selection) {
  if (getSelection() == selection) return;
  TSelection *oldSelection       = getSelection();
  CommandManager *commandManager = CommandManager::instance();
  if (oldSelection) {
    oldSelection->selectNone();
    // disable selection related commands
    int i;
    for (i = 0; i < (int)m_enabledCommandIds.size(); i++)
      commandManager->setHandler(m_enabledCommandIds[i].c_str(), 0);
    m_enabledCommandIds.clear();

    // revert alternative command names to original ones kept in the icon texts
    for (auto commandId : oldSelection->alternativeCommandNames().keys()) {
      QAction *action = commandManager->getAction(commandId);
      action->setText(action->iconText());
    }
  }
  m_selectionStack.back() = selection;
  if (selection) {
    selection->enableCommands();

    // set alternative command names
    QMap<CommandId, QString>::const_iterator i =
        selection->alternativeCommandNames().constBegin();
    while (i != selection->alternativeCommandNames().constEnd()) {
      QAction *action = commandManager->getAction(i.key());
      action->setText(i.value());
      ++i;
    }
  }
  emit selectionSwitched(oldSelection, selection);
}

//-----------------------------------------------------------------------------

void TSelectionHandle::pushSelection() {
  // NOTE
  //  We push 0, and NOT a copy of the last item. I think this is done on
  //  purpose,
  //  as having it copied and then selecting a different one invokes
  //  selectNone()
  //  on the former.
  m_selectionStack.push_back(0);
}

//-----------------------------------------------------------------------------

void TSelectionHandle::popSelection() {
  if (m_selectionStack.size() > 1) m_selectionStack.pop_back();
  TSelection *selection = getSelection();
  if (selection) selection->enableCommands();
}

//-----------------------------------------------------------------------------

void TSelectionHandle::enableCommand(std::string cmdId,
                                     CommandHandlerInterface *handler) {
  CommandManager::instance()->setHandler(cmdId.c_str(), handler);
  m_enabledCommandIds.push_back(cmdId);
}

//-----------------------------------------------------------------------------

void TSelectionHandle::notifySelectionChanged() {
  emit selectionChanged(m_selectionStack.back());
}

//-----------------------------------------------------------------------------

TSelectionHandle *TSelectionHandle::getCurrent() {
  static TSelectionHandle _currentSelection;
  return &_currentSelection;
}
