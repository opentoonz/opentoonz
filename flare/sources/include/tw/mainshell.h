#pragma once

#ifndef MAINSHELL_INCLUDED
#define MAINSHELL_INCLUDED

//#include "tcommon.h"
#include "menubar.h"
//#include "toolbar.h"
//#include "label.h"
//#include "panel.h"
// #include "tcli.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TGenericCommandAction;

namespace TCli {
class Usage;
}

class DVAPI TMainshell : public TWidget {
protected:
  static TMainshell *theShell;
  virtual void create();

public:
#ifndef WIN32
  bool m_ending;
#endif

  static TMainshell *getMainshell() { return theShell; }
  static TMainshell *instance() { return theShell; }

  TMainshell(string name);

  virtual void init() {}
  virtual void atExit() {}

  virtual bool beforeShow() { return true; }
  virtual void close();
  virtual void closeWithoutQuestion();
  // void pushStatusMessage(string );
  // virtual void pushStatusMessage(std::wstring ) {}
  // virtual void popStatusMessage() {};

  // static void errorMessage(const string &str);
  // static bool questionMessage(const string &str);
  // static int multipleChoicesMessage(const string &str, vector<string>
  // &choices);

  // brutto (ma veloce da implementare :-)
  // restituisce 0=yes, 1=no, 2=cancel
  // static int yesNoCancelMessage(const string &str);

  virtual TDimension getPreferredSize() { return TDimension(800, 600); }

  virtual bool isResizable() const { return true; }

  virtual int getMainIconId() { return 0; }            // ha senso solo su NT
  virtual int getSplashScreenBitmapId() { return 0; }  // ha senso solo su NT

  // void setAccelerator(
  //    string acc,
  //    TGenericCommandAction *onKeyDown);

  void setAccelerator(string acc, string guiCommandName);
  string getAccelerator(string guiCommandName);

  virtual void forwardMessage(string msg) {}

  void contextHelp();
  virtual void onContextHelp(string reference) {}

  virtual string getAppId() const = 0;

  virtual void defineUsage(TCli::Usage &usage);
};

class DVAPI TKeyListener {
  string m_keyName;

public:
  TKeyListener(string keyName);
  virtual ~TKeyListener();

  virtual void onKeyDown()                      = 0;
  virtual void onKeyUp(bool mouseEventReceived) = 0;
  virtual bool autoreatIsEnabled() { return false; }

  string getKeyName() const { return m_keyName; }
};

//-------------------------------------------------------------------

template <class T>
class TCommandKey : public TKeyListener {
  typedef void (T::*Method)();
  T *m_target;
  Method m_method;

public:
  TCommandKey(string keyName, T *target, Method method)
      : TKeyListener(keyName), m_target(target), m_method(method) {}

  void onKeyDown() { (m_target->*m_method)(); }
  void onKeyUp(bool mouseEventReceived) {}
};

DVAPI void enableShortcuts(bool on);

DVAPI void enableShortcuts(bool on);

DVAPI void enableShortcuts(bool on);

#endif
