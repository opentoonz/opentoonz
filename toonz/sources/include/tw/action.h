#pragma once

#ifndef TNZ_ACTION_INCLUDED
#define TNZ_ACTION_INCLUDED

#include "tw/tw.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TButton;

//-------------------------------------------------------------------

class DVAPI TGenericCommandAction {
public:
  TGenericCommandAction() {}
  virtual ~TGenericCommandAction() {}

  virtual void sendCommand()                   = 0;
  virtual TGenericCommandAction *clone() const = 0;
};

//-------------------------------------------------------------------

template <class T>
class TCommandAction : public TGenericCommandAction {
public:
  typedef void (T::*CommandMethod)();

  T *m_target;
  CommandMethod m_method;

  TCommandAction<T>(T *target, CommandMethod method)
      : m_target(target), m_method(method){};
  void sendCommand() { (m_target->*m_method)(); };

  TGenericCommandAction *clone() const {
    return new TCommandAction<T>(m_target, m_method);
  }
};

//-------------------------------------------------------------------

template <class T, class Arg>
class TCommandAction1 : public TGenericCommandAction {
public:
  typedef void (T::*CommandMethod)(Arg arg);

  T *m_target;
  CommandMethod m_method;
  Arg m_arg;

  TCommandAction1<T, Arg>(T *target, CommandMethod method, Arg arg)
      : m_target(target), m_method(method), m_arg(arg){};
  void sendCommand() { (m_target->*m_method)(m_arg); };

  TGenericCommandAction *clone() const {
    return new TCommandAction1<T, Arg>(m_target, m_method, m_arg);
  }
};

//-------------------------------------------------------------------

class DVAPI TCommandSource {
  vector<TGenericCommandAction *> *m_actions;

public:
  TCommandSource();
  virtual ~TCommandSource();

  void addAction(TGenericCommandAction *action);
  void sendCommand();

private:
  // not implemented
  TCommandSource(const TCommandSource &);
  TCommandSource &operator=(const TCommandSource &);
};

//-------------------------------------------------------------------

template <class T>
inline void tconnect(TCommandSource &src, T *target, void (T::*method)()) {
  src.addAction(new TCommandAction<T>(target, method));
}

//-------------------------------------------------------------------

template <class T, class Arg>
inline void tconnect(TCommandSource &src, T *target, void (T::*method)(Arg arg),
                     Arg arg) {
  src.addAction(new TCommandAction1<T, Arg>(target, method, arg));
}

//-------------------------------------------------------------------

class DVAPI TGuiCommand {
  class Imp;
  Imp *m_imp;

public:
  TGuiCommand(string cmdName = "none");
  virtual ~TGuiCommand();
  TGuiCommand(const TGuiCommand &);
  TGuiCommand &operator=(const TGuiCommand &);

  // void setHelp(string longHelp, string shortHelp);
  // void setHelp(string help) {setHelp(help, help);}

  // void setTitle(string title);
  // string getTitle();

  bool isToggle() const;
  void setIsToggle(bool on);
  void setStatus(bool on);
  bool getStatus() const;

  void enable();
  void disable();

  void add(TButton *button);
  void setAction(TGenericCommandAction *action);

  // debug!!
  void sendCommand();

  static void execute(string cmdName);
  static void getNames(std::vector<string> &cmdNames);

private:
};

//-------------------------------------------------------------------

class DVAPI TGuiCommandExecutor : public TGuiCommand {
public:
  TGuiCommandExecutor(string cmdName) : TGuiCommand(cmdName) {
    setAction(new TCommandAction<TGuiCommandExecutor>(
        this, &TGuiCommandExecutor::onCommand));
  }

  virtual ~TGuiCommandExecutor() {}

  virtual void onCommand() = 0;
};

//-------------------------------------------------------------------

class TGuiCommandGenericTarget {
protected:
  TGuiCommand m_command;

public:
  TGuiCommandGenericTarget(string cmdName) : m_command(cmdName) {}
  virtual ~TGuiCommandGenericTarget() {}
  virtual void activate() = 0;
  virtual void deactivate() { m_command.setAction(0); }
};

//-------------------------------------------------------------------

template <class T>
class TGuiCommandTarget : public TGuiCommandGenericTarget {
public:
  typedef void (T::*Method)();
  T *m_target;
  Method m_method;

public:
  TGuiCommandTarget(string cmdName, T *target, Method method)
      : TGuiCommandGenericTarget(cmdName), m_target(target), m_method(method) {}

  void activate() {
    m_command.setAction(new TCommandAction<T>(m_target, m_method));
  }
};

#endif
