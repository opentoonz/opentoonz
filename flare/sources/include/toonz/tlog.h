#pragma once

#ifndef TSYSLOG_H
#define TSYSLOG_H

#include <memory>

#include "tcommon.h"
class TFilePath;

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TSysLog {
void success(const std::string &msg);
void warning(const std::string &msg);
void error(const std::string &msg);
void info(const std::string &msg);
}

//------------------------------------------------------------------------------

class DVAPI TUserLogAppend {
public:
  TUserLogAppend();  // used to redirect log messages to the console
  TUserLogAppend(const TFilePath &fp);
  ~TUserLogAppend();

  void warning(const std::string &msg);
  void error(const std::string &msg);
  void info(const std::string &msg);

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;
};

#endif
