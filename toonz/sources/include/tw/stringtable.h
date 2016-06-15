#pragma once

#ifndef STRINGTABLE_INCLUDED
#define STRINGTABLE_INCLUDED

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TNZBASE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class TFilePath;

class DVAPI TStringTable {
public:
  static const TStringTable *instance();
  static std::wstring translate(std::string);

  class Item {
  public:
    std::wstring m_name, m_help, m_tip;
    Item() : m_name(), m_help(), m_tip(){};
  };

  virtual const Item *getItem(std::string name) const = 0;

  virtual std::pair<std::string, int> getDefaultFontNameAndSize() const = 0;

  virtual std::string getDefaultMacFontName() const = 0;

protected:
  TStringTable();
  virtual ~TStringTable();

private:
  // not implemented
  TStringTable(const TStringTable &);
  TStringTable &operator=(const TStringTable &);
};

#endif
