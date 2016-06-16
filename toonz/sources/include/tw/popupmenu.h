#pragma once

#ifndef TNZ_POPUPMENU_INCLUDED
#define TNZ_POPUPMENU_INCLUDED

#include "tw/action.h"
#include "tw/popup.h"
#include "tw/button.h"

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------

class TPopupMenu;
class TPopupMenuItem;

//-------------------------------------------------------------

class DVAPI TPopupMenuListener {
public:
  TPopupMenuListener() {}
  virtual ~TPopupMenuListener() {}

  virtual void onMenuSelect(TPopupMenuItem *item) = 0;
};

//-------------------------------------------------------------

class DVAPI TPopupMenuItem : public TButton {
  wstring m_title;
  bool m_selected;

public:
  TPopupMenuItem(TPopupMenu *parent, string name);
  void repaint();

  void enter(const TPoint &);
  void leave(const TPoint &);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void setTitle(wstring title);

  TPopupMenu *getPopup();

  void select(bool on);
  bool isSelected() const { return m_selected; }

  virtual int getWidth();
};

//-------------------------------------------------------------

class DVAPI TPopupMenuSeparator : public TWidget {
public:
  TPopupMenuSeparator(TPopupMenu *parent);
  void repaint();
};

//-------------------------------------------------------------

//
// Note: to implement right-mouse-bottom menu use
// the class TContextMenu (see below)
//

class DVAPI TPopupMenu : public TPopup {
  TPopupMenuItem *m_currentItem;
  TPopupMenuListener *m_listener;
  int m_firstVisibleItem;
  int m_visibleItemCount;
  bool m_leftAlignment;

public:
  TPopupMenu(TWidget *parent, string name = "popupMenu");
  void draw();
  void configureNotify(const TDimension &);

  void leftButtonDrag(const TPoint &, UCHAR);
  void leftButtonUp(const TPoint &);

  void popup(const TPoint &pos, int width = 120);
  void close();

  TPopupMenuItem *getCurrentItem() { return m_currentItem; }
  void setListener(TPopupMenuListener *);

  static TPopupMenu *getCurrentMenu();

  void setLeftAlignment(bool a) { m_leftAlignment = a; }

private:
  void updateItemsVisibility();
  void updateSize();
  void computeSize(TPoint &pos, int maxY);

  friend class TPopupMenuArrow;
};

//-------------------------------------------------------------

//
// TContextMenu :
//  a class specifically dedicated to
//  right-mouse-button popup menu (aka Context Menu)
//
// Note: DON'T use the method popup() (inherited from TPopupMenu)
//
// Note: the context menu will be automatically destroyed
// DON'T keep a reference to the context menu
//
// see examples below
//

/*---- Example 1 (position indipendent) ----

   void Pluto::rightButtonUp(const TMouseEvent &e) {
     TContextMenu *menu = new TContextMenu(this);
     menu->addCommand("MI_Cut");
     menu->addCommand("MI_Copy");
     menu->addCommand("MI_Paste");
     menu->addCommand("MI_Clear");
     TContextMenu::open(menu, e.m_pos);
   }


  ---- Example 2 (position dependent) ----


   class MyContextMenu : public TContextMenu {
     int m_myValue;
   public:
     MyContextMenu(TWidget*parent, int myValue)
     : TContextMenu(parent), m_myValue(myValue), m_col(col) {
        tconnect(this, "foo",  this, &CellContextMenu::onFoo);
        tconnect(this, "bar",  this, &CellContextMenu::onBar);
     }
     void onFoo() { ... }
     void onBar() { ... }
   };

   void Pluto::rightButtonUp(const TMouseEvent &e) {
     int row = ....
     int col = ....
     TContextMenu::open(new CellContextMenu(this, row, col), e.m_pos);
   }

  -----------------------------------------*/

class DVAPI TContextMenu : public TPopupMenu {
public:
  class CommandFilter {
  public:
    virtual bool check(string cmdName) const = 0;
    virtual ~CommandFilter() {}
  };

  TContextMenu(TWidget *parent);

  void add(string name, TGenericCommandAction *action);
  void addCommand(string cmdName);
  void addCommand(string cmdName, TGenericCommandAction *action);

  void addSeparator() { new TPopupMenuSeparator(this); }

  /*
template <class T>
inline void addCommand(string cmdName, T*target, void (T::*method)()) {
//addCommand(cmdName, new TCommandAction<T>(target, method));
}
*/
  static void open(TContextMenu *menu, const TPoint &pos);

  static void setCommandFilter(CommandFilter *filter);

  void onTimer(int);
};

template <class T>
inline void tconnect(TContextMenu *menu, string cmdName, T *target,
                     void (T::*method)()) {
  menu->addCommand(cmdName, new TCommandAction<T>(target, method));
}

template <class T, class Arg>
inline void tconnect(TContextMenu *menu, string cmdName, T *target,
                     void (T::*method)(Arg), Arg arg) {
  menu->addCommand(cmdName, new TCommandAction1<T, Arg>(target, method, arg));
}

#endif
