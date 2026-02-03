#pragma once

#ifndef SHORTCUTMANAGER_INCLUDED
#define SHORTCUTMANAGER_INCLUDED

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

// forward declarations
class TKeyListener;
class TFilePath;
//
// Shortcut
//
class DVAPI TShortcut {
  string m_id;
  wstring m_name;
  TKeyListener *m_listener;

public:
  TShortcut(string id);
  virtual ~TShortcut();

  string getId() const { return m_id; }

  void setName(wstring name) { m_name = name; }
  wstring getName() const { return m_name; }

  string getKeyName() const;
  void setKeyName(string keyName);

  virtual void onKeyDown() = 0;
  virtual void onKeyUp(bool mouseEventReceived) {}

  virtual string getType() const = 0;

private:
  // not implemented
  TShortcut(const TShortcut &);
  TShortcut &operator=(const TShortcut &);
};

//
// Shortcut controller
//
class DVAPI TShortcutManager {  // singleton

  class Imp;
  Imp *m_imp;

  TShortcutManager();

public:
  ~TShortcutManager();
  static TShortcutManager *instance();

  void setKeyName(string id, string keyName);
  string getKeyName(string id);

  TShortcut *getShortcutByKeyName(string keyName);
  TShortcut *getShortcutById(string keyName);

  void getShortcuts(std::vector<const TShortcut *> &shortcuts);

  // get ownership
  void add(TShortcut *shortcut);

  void setPath(const TFilePath &path);
  const TFilePath &getPath() const;

  void load();
  void save();
};

#endif
