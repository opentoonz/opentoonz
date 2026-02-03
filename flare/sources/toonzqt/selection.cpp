

#include "toonzqt/selection.h"
#include "toonzqt/tselectionhandle.h"
#include "assert.h"

//#include "menubar.h"
#include <QMenu>
#include <QWidget>

//=============================================================================
// TSelection
//-----------------------------------------------------------------------------

TSelection::TSelection() : m_view(0) {}

//-----------------------------------------------------------------------------

TSelection::~TSelection() {}

//-----------------------------------------------------------------------------

void TSelection::makeCurrent() {
  TSelectionHandle::getCurrent()->setSelection(this);
}

//-----------------------------------------------------------------------------

void TSelection::makeNotCurrent() {
  TSelectionHandle *sh = TSelectionHandle::getCurrent();
  if (sh->getSelection() == this) sh->setSelection(0);
}

//-----------------------------------------------------------------------------

TSelection *TSelection::getCurrent() {
  return TSelectionHandle::getCurrent()->getSelection();
}

//-----------------------------------------------------------------------------

void TSelection::setCurrent(TSelection *selection) {
  // assert(0);
  TSelectionHandle::getCurrent()->setSelection(selection);
}

//-----------------------------------------------------------------------------

void TSelection::enableCommand(CommandId cmdId,
                               CommandHandlerInterface *handler) {
  TSelectionHandle::getCurrent()->enableCommand(cmdId, handler);
}

void TSelection::addMenuAction(QMenu *menu, CommandId cmdId) {
  menu->addAction(CommandManager::instance()->getAction(cmdId));
}

void TSelection::notifyView() {
  if (m_view) m_view->onSelectionChanged();
}
