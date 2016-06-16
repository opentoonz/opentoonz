#pragma once

#ifndef TNZ_MENUBAR_INCLUDED
#define TNZ_MENUBAR_INCLUDED

#include "tw/tw.h"
#include "tw/popupmenu.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//--------------------------------------------------------

class DVAPI TMenubar : public TWidget {
public:
  TMenubar(TWidget *parent, string name = "menubar");
  void configureNotify(const TDimension &);
  void draw();

  int getMinWidth();
};

//--------------------------------------------------------

class DVAPI TMenubarItem : public TWidget, public TPopupMenuListener {
  bool m_highlighted, m_pressed;
  TPopupMenu *m_popupMenu;
  wstring m_title;

protected:
  /*virtual*/ void create();

public:
  TMenubarItem(TMenubar *menubar, string name);

  void draw();
  void enter(const TPoint &p);
  void leave(const TPoint &p);
  void leftButtonDown(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  TPopupMenu *getMenu() const { return m_popupMenu; }
  TPopupMenuItem *addItem(string cmdname);
  // TPopupMenuItem *addItem(string cmdname, string title, string help);
  void addSeparator();

  void onMenuSelect(TPopupMenuItem *);

  TDimension getPreferredSize() const;
};

#endif
