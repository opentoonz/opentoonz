#pragma once

#ifndef T_EXCEPTION_INCLUDED
#define T_EXCEPTION_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI TException {
  TString m_msg;

public:
  explicit TException(const std::string &msg = "Toonz Exception");
  explicit TException(const std::wstring &msg) : m_msg(msg) {}
  virtual ~TException() {}
  virtual TString getMessage() const { return m_msg; }
};

// DVAPI ostream& operator<<(ostream &out, const TException &e);

#endif  // T_EXCEPTION_INCLUDED
