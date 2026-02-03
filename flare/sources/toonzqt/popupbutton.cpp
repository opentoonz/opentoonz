

// Qt includes
#include <QMenu>

#include "toonzqt/popupbutton.h"

//********************************************************************************
//    PopupButton class implementation
//********************************************************************************

PopupButton::PopupButton(QWidget *parent) : QPushButton(parent), m_index(-1) {
  setFocusPolicy(Qt::NoFocus);

  QMenu *menu = new QMenu(this);
  setMenu(menu);
  menu->setToolTipsVisible(true);
  connect(this, SIGNAL(clicked(bool)), this, SLOT(showMenu()));
}

//--------------------------------------------------------------------------

QAction *PopupButton::addItem(const QString &text) {
  QAction *action = menu()->addAction(text);
  connect(action, SIGNAL(triggered(bool)), this, SLOT(onIndexChange()));
  m_actions.push_back(action);
  return action;
}

//--------------------------------------------------------------------------

QAction *PopupButton::addItem(const QIcon &icon, const QString &text) {
  QAction *action = menu()->addAction(icon, text);
  connect(action, SIGNAL(triggered(bool)), this, SLOT(onIndexChange()));
  m_actions.push_back(action);
  return action;
}

//--------------------------------------------------------------------------

QAction *PopupButton::addSeparator() { return menu()->addSeparator(); }

//--------------------------------------------------------------------------

int PopupButton::currentIndex() const { return m_index; }

//--------------------------------------------------------------------------

const QAction *PopupButton::currentItem() const {
  return (m_index >= 0) ? m_actions[m_index] : 0;
}

//--------------------------------------------------------------------------

void PopupButton::setCurrentIndex(int index) {
  if (index < 0 || index >= m_actions.size()) return;

  m_index               = index;
  const QAction *action = currentItem();
  setIcon(action->icon());
  setText(action->text());

  emit currentIndexChanged(index);
}

//--------------------------------------------------------------------------

int PopupButton::count() const { return m_actions.size(); }

//--------------------------------------------------------------------------

int PopupButton::findText(const QString &text, Qt::CaseSensitivity cs) const {
  int i, size = m_actions.size();
  for (i = 0; i < size; ++i)
    if (QString::compare(m_actions[i]->text(), text, cs) == 0) return i;

  return -1;
}

//--------------------------------------------------------------------------

void PopupButton::onIndexChange() {
  QAction *action = (QAction *)QObject::sender();
  int i, size = m_actions.size();
  for (i = 0; i < size; ++i)
    if (m_actions[i] == action) break;

  if (i < size) setCurrentIndex(i);

  emit activated(i);
}
