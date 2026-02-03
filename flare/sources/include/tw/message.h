#pragma once

#ifndef TW_MESSAGE_INCLUDED
#define TW_MESSAGE_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFilePath;

namespace TMessage {

class DVAPI Arg {
  TString m_arg;

public:
  /*
#if defined(MACOSX)
Arg(const string &arg) {}
Arg(const wstring &arg) {}
Arg(const TFilePath &arg){}
#else
*/
  Arg(const string &arg);
  Arg(const wstring &arg);
  Arg(const TFilePath &arg);
  /*#endif*/

  TString getString() const { return m_arg; }
};

#ifdef WIN32
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

class DVAPI ArgList {
  vector<Arg> m_args;

public:
  ArgList() {}
  ArgList(const Arg &arg) { m_args.push_back(arg); }
  ArgList(const Arg &arg0, const Arg &arg1) {
    m_args.push_back(arg0);
    m_args.push_back(arg1);
  }
  ArgList(const Arg &arg0, const Arg &arg1, const Arg &arg2) {
    m_args.push_back(arg0);
    m_args.push_back(arg1);
    m_args.push_back(arg2);
  }
  int getCount() const { return m_args.size(); }
  TString getString(int index) const {
    assert(0 <= index && index < getCount());
    return m_args[index].getString();
  }
};

#ifdef WIN32
#pragma warning(pop)
#endif

enum Answer { NO = 0, YES = 1, CANCEL = -1 };

/*
#if defined(MACOSX)
DVAPI void error(const string &str, const ArgList &lst){}
DVAPI Answer question(const string &str, const ArgList &lst);
DVAPI Answer yesNoCancel(const string &str, const ArgList &lst);
#else
*/
DVAPI void error(const string &str, const ArgList &lst);

DVAPI void info(const string &str, const ArgList &lst);

DVAPI Answer question(const string &str, const ArgList &lst);
DVAPI Answer yesNoCancel(const string &str, const ArgList &lst);
//#endif

inline void info(const string &str) { info(str, ArgList()); }

inline void error(const string &str) { error(str, ArgList()); }
inline void error(const string &str, const Arg &arg0) {
  error(str, ArgList(arg0));
}
inline void error(const string &str, const Arg &arg0, const Arg &arg1) {
  error(str, ArgList(arg0, arg1));
}
inline void error(const string &str, const Arg &arg0, const Arg &arg1,
                  const Arg &arg2) {
  error(str, ArgList(arg0, arg1, arg2));
}

inline Answer question(const string &str) { return question(str, ArgList()); }
inline Answer question(const string &str, const Arg &arg0) {
  return question(str, ArgList(arg0));
}
inline Answer question(const string &str, const Arg &arg0, const Arg &arg1) {
  return question(str, ArgList(arg0, arg1));
}
inline Answer question(const string &str, const Arg &arg0, const Arg &arg1,
                       const Arg &arg2) {
  return question(str, ArgList(arg0, arg1, arg2));
}

inline Answer yesNoCancel(const string &str) {
  return yesNoCancel(str, ArgList());
}
inline Answer yesNoCancel(const string &str, const Arg &arg0) {
  return yesNoCancel(str, ArgList(arg0));
}
inline Answer yesNoCancel(const string &str, const Arg &arg0, const Arg &arg1) {
  return yesNoCancel(str, ArgList(arg0, arg1));
}
inline Answer yesNoCancel(const string &str, const Arg &arg0, const Arg &arg1,
                          const Arg &arg2) {
  return yesNoCancel(str, ArgList(arg0, arg1, arg2));
}

DVAPI int multipleChoicesQuestion(const string &question, const ArgList &argLst,
                                  std::vector<string> &buttons);

}  // namespace

#endif
