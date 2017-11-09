

#include "toonzqt/menubarcommand.h"
//#include "menubarcommandids.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonz/toonzfolders.h"
#include "tsystem.h"
#include <assert.h>
#include <QObject>
#include <QAction>
#include <QSettings>
#include <QKeySequence>
#include <QApplication>

#include <sys/types.h>

using namespace DVGui;

//---------------------------------------------------------

namespace {

void updateToolTip(QAction *action) {
  QString tooltip  = action->text();
  QString shortcut = action->shortcut().toString();
  if (shortcut != "") tooltip += " (" + shortcut + ")";
  action->setToolTip(tooltip);
}

}  // namespace

//=========================================================

AuxActionsCreator::AuxActionsCreator() {
  AuxActionsCreatorManager::instance()->addAuxActionsCreator(this);
}
//-----------------------------------------------------------------------------

AuxActionsCreatorManager::AuxActionsCreatorManager()
    : m_auxActionsCreated(false) {}

AuxActionsCreatorManager *AuxActionsCreatorManager::instance() {
  static AuxActionsCreatorManager _instance;
  return &_instance;
}

void AuxActionsCreatorManager::addAuxActionsCreator(
    AuxActionsCreator *auxActionsCreator) {
  m_auxActionsCreators.push_back(auxActionsCreator);
}

void AuxActionsCreatorManager::createAuxActions(QObject *parent) {
  if (m_auxActionsCreated) return;
  m_auxActionsCreated = true;
  for (int i = 0; i < (int)m_auxActionsCreators.size(); i++)
    m_auxActionsCreators[i]->createActions(parent);
}

//=========================================================

CommandManager::CommandManager() {}

//---------------------------------------------------------

CommandManager::~CommandManager() {
  std::map<std::string, Node *>::iterator it;
  for (it = m_idTable.begin(); it != m_idTable.end(); ++it) delete it->second;
}

//---------------------------------------------------------

CommandManager *CommandManager::instance() {
  static CommandManager _instance;
  return &_instance;
}

//---------------------------------------------------------

//
// command id => command
//
CommandManager::Node *CommandManager::getNode(CommandId id,
                                              bool createIfNeeded) {
  AuxActionsCreatorManager::instance()->createAuxActions(qApp);
  std::map<std::string, Node *>::iterator it = m_idTable.find(id);
  if (it != m_idTable.end()) return it->second;
  if (createIfNeeded) {
    Node *node    = new Node(id);
    m_idTable[id] = node;
    return node;
  } else
    return 0;
}

//---------------------------------------------------------

void CommandManager::setShortcut(CommandId id, QAction *action,
                                 std::string shortcutString) {
  if (shortcutString != "")
    action->setShortcut(QKeySequence(QString::fromStdString(shortcutString)));
  else
    action->setShortcut(QKeySequence());
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("shortcuts.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.beginGroup("shortcuts");
  settings.setValue(QString(id), QString::fromStdString(shortcutString));
  settings.endGroup();
}

//---------------------------------------------------------

void CommandManager::define(CommandId id, CommandType type,
                            std::string defaultShortcutString,
                            QAction *qaction) {
  assert(type != UndefinedCommandType);
  assert(qaction != 0);
  assert(m_qactionTable.count(qaction) == 0);

  Node *node = getNode(id);
  if (node->m_type != UndefinedCommandType) {
    assert(!"Duplicate command id");
  }
  node->m_type    = type;
  node->m_qaction = qaction;
  node->m_qaction->setEnabled(
      node->m_enabled &&
          (!!node->m_handler || node->m_qaction->actionGroup() != 0) ||
      node->m_type == MiscCommandType ||
      node->m_type == ToolModifierCommandType);

  m_qactionTable[qaction] = node;
  qaction->setShortcutContext(Qt::ApplicationShortcut);
  // user defined shortcuts will be loaded afterwards in loadShortcuts()
  QString defaultShortcutQString =
      QString::fromStdString(defaultShortcutString);
  if (!defaultShortcutQString.isEmpty()) {
    qaction->setShortcut(QKeySequence(defaultShortcutQString));
    m_shortcutTable[defaultShortcutString] = node;
  }

  if (type == ToolCommandType) updateToolTip(qaction);
}

//---------------------------------------------------------

//
// set handler (id, handler)
//   possibly changes enable/disable qaction state
//
void CommandManager::setHandler(CommandId id,
                                CommandHandlerInterface *handler) {
  Node *node = getNode(id);
  if (node->m_handler != handler) {
    delete node->m_handler;
    node->m_handler = handler;
  }
  if (node->m_qaction) {
    node->m_qaction->setEnabled(
        node->m_enabled &&
        (!!node->m_handler || node->m_qaction->actionGroup() != 0));
  }
}

//---------------------------------------------------------
//
// qaction -> command; execute
//

void CommandManager::execute(QAction *qaction) {
  assert(qaction);
  std::map<QAction *, Node *>::iterator it = m_qactionTable.find(qaction);
  assert(it != m_qactionTable.end());
  if (it != m_qactionTable.end() && it->second->m_handler) {
    it->second->m_handler->execute();
  }
}

//---------------------------------------------------------

void CommandManager::execute(QAction *action, QAction *menuAction) {
  std::map<QAction *, Node *>::iterator it = m_qactionTable.find(action);
  if (it != m_qactionTable.end())
    execute(action);
  else
    execute(menuAction);
}

//---------------------------------------------------------

void CommandManager::execute(CommandId id) {
  Node *node = getNode(id, false);
  if (node && node->m_handler) {
    QAction *action = node->m_qaction;
    if (action && action->isCheckable()) {
      // principalmente per i tool
      action->setChecked(true);
    }
    node->m_handler->execute();
  }
}

//---------------------------------------------------------

void CommandManager::getActions(CommandType type,
                                std::vector<QAction *> &actions) {
  AuxActionsCreatorManager::instance()->createAuxActions(qApp);
  std::map<QAction *, Node *>::iterator it;
  for (it = m_qactionTable.begin(); it != m_qactionTable.end(); ++it)
    if (it->second->m_type == type) actions.push_back(it->first);
}

//---------------------------------------------------------

QAction *CommandManager::getActionFromShortcut(std::string shortcutString) {
  std::map<std::string, Node *>::iterator it =
      m_shortcutTable.find(shortcutString);
  return it != m_shortcutTable.end() ? it->second->m_qaction : 0;
}

//---------------------------------------------------------

std::string CommandManager::getShortcutFromAction(QAction *action) {
  std::map<std::string, Node *>::iterator it = m_shortcutTable.begin();
  for (; it != m_shortcutTable.end(); ++it) {
    if (it->second->m_qaction == action) return it->first;
  }
  return "";
}

//---------------------------------------------------------

std::string CommandManager::getShortcutFromId(const char *id) {
  QAction *action = getAction(id);
  assert(action);
  if (!action) return "";
  return getShortcutFromAction(action);
}

//---------------------------------------------------------

int CommandManager::getKeyFromShortcut(const std::string &shortcut) {
  QString qShortcut = QString::fromStdString(shortcut);
  if (qShortcut == "") return 0;

  QKeySequence ks(qShortcut);
  assert(ks.count() == 1);
  return ks[0];
}

//---------------------------------------------------------

int CommandManager::getKeyFromId(const char *id) {
  return getKeyFromShortcut(getShortcutFromId(id));
}

//---------------------------------------------------------

void CommandManager::setShortcut(QAction *action, std::string shortcutString,
                                 bool keepDefault) {
  QString shortcut = QString::fromStdString(shortcutString);

  std::string oldShortcutString = action->shortcut().toString().toStdString();
  if (oldShortcutString == shortcutString) return;

  // Cerco il nodo corrispondente ad action. Deve esistere
  std::map<QAction *, Node *>::iterator it = m_qactionTable.find(action);
  Node *node = it != m_qactionTable.end() ? it->second : 0;
  assert(node);
  assert(node->m_qaction == action);

  QKeySequence ks(shortcut);
  assert(ks.count() == 1 || ks.count() == 0 && shortcut == "");

  if (node->m_type == ZoomCommandType && ks.count() > 1) {
    DVGui::warning(
        QObject::tr("It is not possible to assign a shortcut with modifiers to "
                    "the visualization commands."));
    return;
  }
  // lo shortcut e' gia' assegnato?
  QString oldActionId;
  std::map<std::string, Node *>::iterator sit =
      m_shortcutTable.find(shortcutString);
  if (sit != m_shortcutTable.end()) {
    // la vecchia azione non ha piu' shortcut
    oldActionId = QString::fromStdString(sit->second->m_id);
    sit->second->m_qaction->setShortcut(QKeySequence());
    if (sit->second->m_type == ToolCommandType)
      updateToolTip(sit->second->m_qaction);
  }
  // assegno lo shortcut all'azione
  action->setShortcut(
      QKeySequence::fromString(QString::fromStdString(shortcutString)));
  if (node->m_type == ToolCommandType) updateToolTip(action);

  // Aggiorno la tabella shortcut -> azioni
  // Cancello il riferimento all'eventuale vecchio shortcut di action
  if (oldShortcutString != "") m_shortcutTable.erase(oldShortcutString);
  // e aggiungo il nuovo legame
  m_shortcutTable[shortcutString] = node;

  // registro il tutto
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("shortcuts.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.beginGroup("shortcuts");
  settings.setValue(QString::fromStdString(node->m_id),
                    QString::fromStdString(shortcutString));
  if (keepDefault) {
    if (oldActionId != "") settings.remove(oldActionId);
  }
  settings.endGroup();
}

//---------------------------------------------------------

void CommandManager::enable(CommandId id, bool enabled) {
  Node *node = getNode(id, false);
  if (!node) return;
  if (node->m_enabled == enabled) return;
  node->m_enabled = enabled;
  if (node->m_qaction)
    node->m_qaction->setEnabled(
        node->m_enabled &&
        (!!node->m_handler || node->m_qaction->actionGroup() != 0));
}

//---------------------------------------------------------

void CommandManager::setChecked(CommandId id, bool checked) {
  Node *node = getNode(id, false);
  if (!node) return;
  if (node->m_qaction) {
    node->m_qaction->setChecked(checked);
    if (node->m_handler) node->m_handler->execute();
  }
}

//---------------------------------------------------------

QAction *CommandManager::getAction(CommandId id, bool createIfNeeded) {
  Node *node = getNode(id, createIfNeeded);
  if (node) {
    return node->m_qaction;
  } else {
    return 0;
  }
}

//---------------------------------------------------------

QAction *CommandManager::createAction(CommandId id, QObject *parent,
                                      bool state) {
  Node *node = getNode(id, false);
  if (!node) return 0;
  QAction *refAction = node->m_qaction;
  if (!refAction) return 0;
  QString text = refAction->text();
  if (node->m_onText != "" && node->m_offText != "")
    text          = state ? node->m_onText : node->m_offText;
  QAction *action = new QAction(text, parent);
  action->setShortcut(refAction->shortcut());
  return action;
}

//---------------------------------------------------------

void CommandManager::setToggleTexts(CommandId id, const QString &onText,
                                    const QString &offText) {
  Node *node = getNode(id, false);
  if (node) {
    node->m_onText  = onText;
    node->m_offText = offText;
  }
}

//---------------------------------------------------------

std::string CommandManager::getIdFromAction(QAction *action) {
  std::map<QAction *, Node *>::iterator it = m_qactionTable.find(action);
  if (it != m_qactionTable.end())
    return it->second->m_id;
  else
    return "";
}

//---------------------------------------------------------
// load user defined shortcuts

void CommandManager::loadShortcuts() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("shortcuts.ini");
  if (!TFileStatus(fp).doesExist()) {
    // if user shortcut file does not exist, then try to load from template
    TFilePath tmplFp =
        ToonzFolder::getTemplateModuleDir() + TFilePath("shortcuts.ini");
    if (TFileStatus(tmplFp).doesExist()) TSystem::copyFile(fp, tmplFp);
    // if neither settings exist, do nothing and return
    else
      return;
  }

  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.beginGroup("shortcuts");
  QStringList ids = settings.allKeys();
  for (int i = 0; i < ids.size(); i++) {
    std::string id   = ids.at(i).toStdString();
    QString shortcut = settings.value(ids.at(i), "").toString();
    QAction *action  = getAction(&id[0], false);
    if (action) {
      QString oldShortcut = action->shortcut().toString();
      if (oldShortcut == shortcut) continue;
      if (!oldShortcut.isEmpty())
        m_shortcutTable.erase(oldShortcut.toStdString());
      if (!shortcut.isEmpty()) {
        QAction *other = getActionFromShortcut(shortcut.toStdString());
        if (other) other->setShortcut(QKeySequence());
        m_shortcutTable[shortcut.toStdString()] = getNode(&id[0]);
      }
      action->setShortcut(QKeySequence(shortcut));
    }
  }
  settings.endGroup();
}

/*
//---------------------------------------------------------

QString CommandManager::getFullText(CommandId id, bool state)
{
  std::map<std::string, Node*>::iterator it;
  it = m_idTable.find(id);
  if(it == m_idTable.end()) return "";
  Node *node = it->second;
  QAction *action = it->second->m_qaction;
  QString text = action->text();
  if(node->m_onText != "" && node->m_offText != "")
    text = state ? node->m_onText : node->m_offText;

  QString shortcut = QString::fromStdString(getShortcutFromAction(action));
  if(shortcut != "") text += " " + shortcut;
  return text;
}
*/

//---------------------------------------------------------

MenuItemHandler::MenuItemHandler(const char *cmdId) {
  CommandManager::instance()->setHandler(
      cmdId, new CommandHandlerHelper<MenuItemHandler>(
                 this, &MenuItemHandler::execute));
}

//=============================================================================
// DVAction
//-----------------------------------------------------------------------------

DVAction::DVAction(const QString &text, QObject *parent)
    : QAction(text, parent) {
  connect(this, SIGNAL(triggered()), this, SLOT(onTriggered()));
}

//-----------------------------------------------------------------------------

DVAction::DVAction(const QIcon &icon, const QString &text, QObject *parent)
    : QAction(icon, text, parent) {
  connect(this, SIGNAL(triggered()), this, SLOT(onTriggered()));
}

//-----------------------------------------------------------------------------

void DVAction::onTriggered() { CommandManager::instance()->execute(this); }

//=============================================================================
// DVMenuAction
//-----------------------------------------------------------------------------
/*! It is a menu' with action defined in \b actions.
                Actions can contain CommandId or simple action name.
                In first case action triggered is connected with action command
   execute directly.
                In second case action triggered is connected with menu action
   command execute;
                is helpful to use \b m_triggeredActionIndex to distingue action.
*/
DVMenuAction::DVMenuAction(const QString &text, QWidget *parent,
                           QList<QString> actions)
    : QMenu(text, parent), m_triggeredActionIndex(-1) {
  int i;
  for (i = 0; i < actions.size(); i++) addAction(actions.at(i));
  connect(this, SIGNAL(triggered(QAction *)), this,
          SLOT(onTriggered(QAction *)));
}

//-----------------------------------------------------------------------------
/*! Set a new list of action to menu'.
                NB. If action list is composed by action menaged and action to
   create pay
                attention to inserted order.*/
void DVMenuAction::setActions(QList<QString> actions) {
  if (m_triggeredActionIndex != -1)  // Sta facendo l'onTriggered
    return;
  clear();
  int i;
  for (i = 0; i < actions.size(); i++) {
    QString actionId = actions.at(i);
    // Se l'azione e' definita da un CommandId la aggiungo.
    QAction *action =
        CommandManager::instance()->getAction(actionId.toStdString().c_str());
    if (action) {
      addAction(action);
      continue;
    }
    // Altrimenti creo una nuova azione e la aggiungo.
    action = addAction(actionId);
    action->setData(QVariant(i));
  }
}

//-----------------------------------------------------------------------------

namespace {
QString changeStringNumber(QString str, int index) {
  QString newStr     = str;
  int n              = 3;
  if (index >= 10) n = 4;
  QString number;
  newStr.replace(0, n, number.number(index + 1) + QString(". "));
  return newStr;
}
}

//-----------------------------------------------------------------------------

void DVMenuAction::onTriggered(QAction *action) {
  QVariant data                              = action->data();
  if (data.isValid()) m_triggeredActionIndex = data.toInt();
  CommandManager::instance()->execute(action, menuAction());
  int oldIndex = m_triggeredActionIndex;
  if (m_triggeredActionIndex != -1) m_triggeredActionIndex = -1;
  QString str                                              = data.toString();
  QAction *tableAction =
      CommandManager::instance()->getAction(str.toStdString().c_str());
  if (tableAction || oldIndex == 0) return;
  QList<QAction *> acts = actions();
  removeAction(action);
  insertAction(acts[0], action);

  acts = actions();
  int i;
  for (i = 0; i <= oldIndex; i++) {
    QAction *a     = acts.at(i);
    QString newTxt = changeStringNumber(a->text(), i);
    a->setText(newTxt);
    a->setData(QVariant(i));
  }
  m_triggeredActionIndex = -1;
}

//-----------------------------------------------------------------------------
