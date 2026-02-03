#pragma once

#ifndef LOGGER_INCLUDED
#define LOGGER_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI Logger {  // singleton
public:
  class Listener {
  public:
    virtual void onAdd() = 0;
    virtual ~Listener() {}
  };

private:
  Logger();
  std::vector<Listener *> m_listeners;
  std::vector<std::wstring> m_rows;

public:
  static Logger *instance();

  void add(std::wstring s);
  void clear();

  int getRowCount() const;
  std::wstring getRow(int i) const;

  void addListener(Listener *listener);
  void removeListener(Listener *listener);
};

#endif
