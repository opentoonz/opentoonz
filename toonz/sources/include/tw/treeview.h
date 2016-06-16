#pragma once

#ifndef TNZ_TREEVIEW_INCLUDED
#define TNZ_TREEVIEW_INCLUDED

#include "tw/tw.h"
#include "tw/dragdrop.h"

#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TWIN_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef WIN32
#pragma warning(disable : 4251)
#endif

//-------------------------------------------------------------------

class TTreeView;
class TTreeViewItem;
class TTreeViewItemParent;
class TScrollbar;
class TTextField;
class TContextMenu;

//-------------------------------------------------------------------

class DVAPI TTreeViewItemUpdater {
public:
  virtual bool done() const                                            = 0;
  virtual void next()                                                  = 0;
  virtual bool matchItem(const TTreeViewItem *) const                  = 0;
  virtual TTreeViewItem *createItem(TTreeViewItemParent *parent) const = 0;
  virtual ~TTreeViewItemUpdater() {}
};

//-------------------------------------------------------------------

class DVAPI TTreeViewItemParent {
  TTreeView *m_treeView;

protected:
  bool m_valid;

  typedef vector<TTreeViewItem *> ItemContainer;
  ItemContainer m_children;

public:
  TTreeViewItemParent() : m_treeView(0), m_valid(true) {}
  virtual ~TTreeViewItemParent();

  void clearItems();
  int getItemCount() const;
  TTreeViewItem *getItem(int index) const;
  TTreeViewItem *getItem(wstring name) const;
  TTreeViewItem *getItem(string name) const;

  void addItem(TTreeViewItem *item);
  void removeItem(TTreeViewItem *item);

  void updateItems(TTreeViewItemUpdater &updater);

  bool hasChild(TTreeViewItem *item) const;

  virtual void onItemChange();

  void setTreeView(TTreeView *treeView) { m_treeView = treeView; }
  TTreeView *getTreeView() const { return m_treeView; }

private:
  // not implemented
  TTreeViewItemParent(const TTreeViewItemParent &);
  TTreeViewItemParent &operator=(const TTreeViewItemParent &);
};

//-------------------------------------------------------------------

class DVAPI TTreeViewItem : public TTreeViewItemParent {
  unsigned char m_status;
  TTreeViewItemParent *m_parent;

public:
  TTreeViewItem(TTreeViewItemParent *parent);
  virtual ~TTreeViewItem();

  TTreeViewItemParent *getParent() const { return m_parent; }
  void setParent(TTreeViewItemParent *parent);

  void clearParent() { m_parent = 0; }

  virtual void onOpen() {}
  virtual void onClose() {}
  virtual void onDoubleClick();

  virtual TTreeViewItem *getSon(string sonName) { return 0; }
  virtual TTreeViewItem *getSon(wstring sonName) { return 0; }

  bool isLeaf() const;
  bool isEmpty() const;
  bool isOpen() const;
  bool isSelected() const;

  void open();
  void close();
  virtual void refresh();

  void setIsLeaf(bool isLeaf);
  void select(bool on);

  bool unselectDescendents();  // ritorna true se ce n'era uno selezionato

  // "origin" sono le coordinate del punto in basso a sinistra dell'icona
  virtual void drawIcon(TTreeView *w, const TPoint &origin);
  virtual void drawName(TTreeView *w, const TPoint &origin);
  virtual void draw(TTreeView *w, const TPoint &origin);
  virtual int getWidth(TTreeView *w);

  virtual wstring getName() const { return L"noname"; }

  virtual TDimension getIconSize() const;
  virtual wstring getTooltipString() { return wstring(); }
  virtual string getContextHelpReference() { return string(); }

  virtual bool isRenameEnabled() const { return false; }
  virtual void rename(wstring name){};

  virtual TContextMenu *createContextMenu() { return 0; }

  virtual bool acceptDrop(const TData *data) const { return false; }
  virtual bool drop(const TData *data) { return false; }
  virtual bool click(const TPoint &p) { return false; }
  virtual bool drag(const TPoint &p) { return false; }
  virtual bool rightClick(const TPoint &p);

private:
  // not implemented
  TTreeViewItem(const TTreeViewItem &);
  TTreeViewItem &operator=(const TTreeViewItem &);
};

//-------------------------------------------------------------------

class DVAPI TTreeView : public TWidget,
                        public TTreeViewItemParent,
                        public TDragDropListener {
public:
  class Listener {
  public:
    virtual void onTreeViewSelect(TTreeView *treeView) = 0;
    virtual ~Listener() {}
  };

protected:
  struct VisibleItem {
    TPoint m_pos;
    // m_pos e' la coordinata del punto in basso a sinistra dell'icona
    // (l'origine e' il punto in alto a sinistra della prima icona)
    int m_height;
    // m_height == getIconSize().ly

    TTreeViewItem *m_item;

    enum { LastSiblingFlag = 0x1, RootFlag = 0x2, SonFlag = 0x4 };
    int m_status;
    // int m_siblingLinkLength;
    // int m_childLinkLength;

    VisibleItem();
  };

  int m_xMargin, m_yMargin,
      m_iconMargin;  // distanza orizzontale fra il bottone "+/-" e l'icona

  vector<VisibleItem> m_visibleItems;

#ifndef MACOSX
  // togliere l'ifdef con il gcc3.3.2
  bool m_valid;
#endif
  int m_height;
  int m_yoffset;
  int m_xoffset;
  // int m_selectedItemIndex;
  std::set<int> m_selectedItemIndices;

  TScrollbar *m_vScb, *m_hScb;
  TTextField *m_renameTextField;
  bool m_multiSelectionEnabled;
  int m_dropIndex;
  vector<Listener *> m_listeners;
  TPoint m_lastMousePos;

public:
  // costruttore/distruttore
  TTreeView(TWidget *parent, string name = "treeView");
  ~TTreeView();

  // listeners
  void addListener(Listener *listener);
  void removeListener(Listener *listener);
  void notifyListeners();

  // configure
  void setScrollbars(TScrollbar *h, TScrollbar *v);
  void enableMultiSelection(bool on);
  bool isMultiSelectionEnabled() const { return m_multiSelectionEnabled; }

  // resize
  void configureNotify(const TDimension &s);

  // draw
  void repaint();
  virtual void drawButton(const TPoint &p, bool open);

  // event handlers
  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDrag(const TMouseEvent &e);
  void leftButtonUp(const TMouseEvent &e);

  void rightButtonDown(const TMouseEvent &e);
  void rightButtonDrag(const TMouseEvent &e);
  void rightButtonUp(const TMouseEvent &e);

  void middleButtonDown(const TMouseEvent &e);
  void middleButtonDrag(const TMouseEvent &e);
  void middleButtonUp(const TMouseEvent &e);

  void mouseWheel(const TMouseEvent &, int wheel);

  void leftButtonDoubleClick(const TMouseEvent &);

  TDropSource::DropEffect onEnter(const TDragDropListener::Event &event);
  TDropSource::DropEffect onOver(const TDragDropListener::Event &event);
  TDropSource::DropEffect onDrop(const TDragDropListener::Event &event);
  void onLeave();

  // to override
  virtual void onSelect(TTreeViewItem *item){};
  virtual bool startDragDrop(TTreeViewItem *item) { return false; }
  virtual void onExpand(TTreeViewItem *item){};
  virtual void onCollapse(TTreeViewItem *item){};

  // queries
  int getSelectedItemCount() const;
  TTreeViewItem *getSelectedItem(int index = 0) const;

  // selection
  void selectNone();
  void select(TTreeViewItem *item);
  void selectParent();
  void selectSon(string sonName);
  void selectItem(int index);  // provvisorio

  // refresh
  void refreshCurrentItem();
  void refresh();

  // tooltips &helps
  wstring getTooltipString(const TPoint &);
  string getContextHelpReference(const TPoint &p);

#ifndef MACOSX
  // togliere l'ifdef con il gcc3.3.2
  void onItemChange();
#endif

  // misc
  void scrollToX(int x);
  void scrollToY(int y);
  void renameCurrentItem(wstring s);
  void updateVisibleItems();

protected:
  void getPlacement(int index, TRect &rect);
  int getIndexFromY(int y);
  void addItemAndSons(TTreeViewItem *item, int indentation);
  void updateScrollbars();
};

#endif
