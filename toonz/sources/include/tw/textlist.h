#pragma once

#ifndef TEXTLIST_INCLUDED
#define TEXTLIST_INCLUDED

#include "tw/tw.h"
#include "tw/scrollview.h"

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

class DVAPI TGenericTextListAction {
public:
  virtual ~TGenericTextListAction() {}
  virtual void sendCommand(int itemIndex) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TTextListAction : public TGenericTextListAction {
public:
  typedef void (T::*Method)(int itemIndex);
  TTextListAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(int itemIndex) { (m_target->*m_method)(itemIndex); }

private:
  T *m_target;
  Method m_method;
};

//-------------------------------------------------------------------

class DVAPI TTextListItem {
public:
  TTextListItem(const std::string &id, const std::string &caption);
  virtual ~TTextListItem() {}

  std::string getId() { return m_id; }
  std::string getCaption() { return m_caption; }

private:
  std::string m_id;
  std::string m_caption;
};

//-------------------------------------------------------------------

class DVAPI TTextList : public TWidget {
public:
  TTextList(TWidget *parent, std::string name = "textlist");
  ~TTextList();

  void addItem(TTextListItem *item);
  void removeItem(const std::string &itemId);
  void clearAll();

  int getItemCount() const;
  TTextListItem *getItem(int i) const;

  // returns the index of item, -1 if not present
  int itemToIndex(const std::string &itemId);

  int getSelectedItemCount() const;
  TTextListItem *getSelectedItem(int i) const;
  std::string getSelectedItemId(
      int i) const;  // returns the id of the i-th item selected

  void select(int i, bool on);
  void select(const std::string &itemId, bool on);
  void unselectAll();

  bool isSelected(int i) const;
  bool isSelected(const std::string &itemId) const;

  void setSelAction(TGenericTextListAction *action);
  void setDblClickAction(TGenericTextListAction *action);

  void draw();
  void configureNotify(const TDimension &d);

  void leftButtonDown(const TMouseEvent &e);
  void leftButtonDoubleClick(const TMouseEvent &e);
  void keyDown(int key, unsigned long mod, const TPoint &);

  void scrollTo(int y);

private:
  class Data;
  Data *m_data;
};

#endif
