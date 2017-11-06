#pragma once

#ifndef MENUBAR_COMMAND_H
#define MENUBAR_COMMAND_H

#include <vector>
#include <map>
#include <string>
#include "tcommon.h"

#include <QAction>
#include <QMenu>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//
// forward declaration
//
class QAction;

//
// base class
//
class DVAPI CommandHandlerInterface {
public:
  virtual ~CommandHandlerInterface() {}
  virtual void execute() = 0;
};

//-----------------------------------------------------------------------------

//
// command identifier (e.g. "copy"). They are NOT command names (e.g. "&Copy")
//
typedef const char *CommandId;

enum CommandType {
  UndefinedCommandType = 0,
  RightClickMenuCommandType,
  MenuFileCommandType,
  MenuEditCommandType,
  MenuScanCleanupCommandType,
  MenuLevelCommandType,
  MenuXsheetCommandType,
  MenuCellsCommandType,
  MenuViewCommandType,
  MenuWindowsCommandType,
  PlaybackCommandType,
  RGBACommandType,
  FillCommandType,
  ToolCommandType,
  ToolModifierCommandType,
  ZoomCommandType,
  MiscCommandType,
  MenuCommandType
};

//-----------------------------------------------------------------------------

class AuxActionsCreator {
public:
  AuxActionsCreator();
  virtual ~AuxActionsCreator(){};
  virtual void createActions(QObject *parent) = 0;
};

//-----------------------------------------------------------------------------

class AuxActionsCreatorManager {
  bool m_auxActionsCreated;
  std::vector<AuxActionsCreator *> m_auxActionsCreators;
  AuxActionsCreatorManager();

public:
  static AuxActionsCreatorManager *instance();
  void addAuxActionsCreator(AuxActionsCreator *auxActionsCreator);
  void createAuxActions(QObject *parent);
};

//-----------------------------------------------------------------------------

//
// command manager:
//    setExecutor(id, executor)
//    setAction(id, qaction)
//    execute(qaction)/
//
class DVAPI CommandManager {  // singleton

  class Node {
  public:
    std::string m_id;
    CommandType m_type;
    QAction *m_qaction;
    CommandHandlerInterface *m_handler;
    bool m_enabled;
    QString m_onText,
        m_offText;  // for toggle commands. e.g. show/hide something

    Node(CommandId id)
        : m_id(id)
        , m_type(UndefinedCommandType)
        , m_qaction(0)
        , m_handler(0)
        , m_enabled(true) {}

    ~Node() {
      if (m_handler) delete m_handler;
    }
  };

  std::map<std::string, Node *> m_idTable;
  std::map<QAction *, Node *> m_qactionTable;
  std::map<std::string, Node *> m_shortcutTable;

  CommandManager();

  Node *getNode(CommandId id, bool createIfNeeded = true);
  void setShortcut(CommandId id, QAction *action, std::string shortcutString);
  void createAuxActions();

public:
  static CommandManager *instance();
  ~CommandManager();

  void setHandler(CommandId id, CommandHandlerInterface *handler);

  void define(CommandId id, CommandType type, std::string defaultShortcutString,
              QAction *action);

  QAction *createAction(const char *id, const char *name,
                        const char *defaultShortcut);

  void getActions(CommandType type, std::vector<QAction *> &actions);
  QAction *getActionFromShortcut(std::string shortcutString);
  std::string getShortcutFromAction(QAction *action);
  std::string getShortcutFromId(CommandId id);
  int getKeyFromShortcut(const std::string &shortcut);
  int getKeyFromId(CommandId id);
  void setShortcut(QAction *action, std::string shortcutString,
                   bool keepDefault = true);

  QAction *getAction(CommandId id, bool createIfNeeded = false);

  // createAction creates a new indepenent QAction with text and shortcut
  // if the action is a toggle action (e.g. show/hide something) the text is
  // controlled by state
  // you can use createAction() for context menu
  QAction *createAction(CommandId id, QObject *parent = 0, bool state = true);

  void execute(QAction *action);
  /*! If action is defined in m_qactionTable recall \b execute(action),
   * otherwise recall execute(menuAction).*/
  void execute(QAction *action, QAction *menuAction);
  void execute(CommandId id);
  void enable(CommandId id, bool enabled);

  // if id is a toggle (e.g. a checkable menu item) then set its status;
  // note: this will trigger any associated handler
  void setChecked(CommandId id, bool checked);

  // use setToggleTexts for toggle commands that have two names according to the
  // current status. e.g. show/hide something
  void setToggleTexts(CommandId id, const QString &onText,
                      const QString &offText);

  std::string getIdFromAction(QAction *action);

  // load user defined shortcuts
  void loadShortcuts();
};

//-----------------------------------------------------------------------------

//
// CommandHandlerHelper = target + method
//

template <class T>
class CommandHandlerHelper final : public CommandHandlerInterface {
  T *m_target;
  void (T::*m_method)();

public:
  CommandHandlerHelper(T *target, void (T::*method)())
      : m_target(target), m_method(method) {}
  void execute() override { (m_target->*m_method)(); }
};

template <class T, typename R>
class CommandHandlerHelper2 final : public CommandHandlerInterface {
  T *m_target;
  void (T::*m_method)(R value);
  R m_value;

public:
  CommandHandlerHelper2(T *target, void (T::*method)(R), R value)
      : m_target(target), m_method(method), m_value(value) {}
  void execute() override { (m_target->*m_method)(m_value); }
};

//-----------------------------------------------------------------------------

template <class T>
inline void setCommandHandler(CommandId id, T *target, void (T::*method)()) {
  CommandManager::instance()->setHandler(
      id, new CommandHandlerHelper<T>(target, method));
}

//-----------------------------------------------------------------------------

class DVAPI MenuItemHandler : public QObject {
  Q_OBJECT
public:
  MenuItemHandler(CommandId cmdId);
  virtual ~MenuItemHandler(){};
  virtual void execute() = 0;
};

template <class T>
class OpenPopupCommandHandler final : public MenuItemHandler {
  T *m_popup;
  CommandId m_id;

public:
  OpenPopupCommandHandler(CommandId cmdId)
      : MenuItemHandler(cmdId), m_popup(0) {}

  void execute() override {
    if (!m_popup) m_popup = new T();
    m_popup->show();
    m_popup->raise();
    m_popup->activateWindow();
  }
};

//-----------------------------------------------------------------------------

class DVAPI DVAction final : public QAction {
  Q_OBJECT
public:
  DVAction(const QString &text, QObject *parent);
  DVAction(const QIcon &icon, const QString &text, QObject *parent);

public slots:
  void onTriggered();
};

//-----------------------------------------------------------------------------

class DVAPI DVMenuAction final : public QMenu {
  Q_OBJECT

  int m_triggeredActionIndex;

public:
  DVMenuAction(const QString &text, QWidget *parent, QList<QString> actions);
  void setActions(QList<QString> actions);

  int getTriggeredActionIndex() { return m_triggeredActionIndex; }

public slots:
  void onTriggered(QAction *action);
};

//-----------------------------------------------------------------------------

#endif  // MENUBAR_COMMAND_H
