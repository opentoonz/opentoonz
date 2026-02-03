#pragma once

#ifndef TSTRINGID_INCLUDED
#define TSTRINGID_INCLUDED

#include <tcommon.h>

#include <string>
#include <vector>
#include <map>

#undef DVAPI
#undef DVVAR
#ifdef TSTRINGID_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class DVAPI TStringId {
public:
  typedef std::map<std::string, int> Map;
  typedef Map::iterator Iterator;

  struct StaticData;
  static const Iterator& none();
  static Iterator genIter(const std::string &str);
  static Iterator findIter(int id);
  static Iterator findIter(const std::string &str);

private:
  Iterator iter;

  inline explicit TStringId(const Iterator &iter):
    iter(iter) { }

public:
  inline TStringId():
    iter(none()) { }
  inline explicit TStringId(const std::string &str):
    iter(genIter(str)) { }
  inline void reset()
    { iter = none(); }
  inline void set(const std::string &str)
    { if (iter->first != str) iter = genIter(str); }

  inline int id() const
    { return iter->second; }
  inline const std::string& str() const
    { return iter->first; }

  inline operator bool () const
    { return id() != 0; }
  inline bool operator== (const TStringId &other) const
    { return id() == other.id(); }
  inline bool operator!= (const TStringId &other) const
    { return id() != other.id(); }
  inline bool operator< (const TStringId &other) const
    { return id() < other.id(); }

  inline static TStringId find(int id)
    { return TStringId(findIter(id)); }
  inline static TStringId find(const std::string &str)
    { return TStringId(findIter(str)); }
};

#endif
