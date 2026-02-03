#pragma once

#ifndef TEXTLIST_INCLUDED
#define TEXTLIST_INCLUDED

#include "tw/tw.h"
#include "tw/scrollview.h"

//-------------------------------------------------------------------

class TGenericTextListAction {
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

class TTextListItem {
public:
  TTextListItem(const string &id, const string &caption);
  virtual ~TTextListItem() {}

  string getId() { return m_id; }
  string getCaption() { return m_caption; }

private:
  string m_id;
  string m_caption;
};

//-------------------------------------------------------------------

class TTextList : public TWidget {
public:
  TTextList(TWidget *parent, string name = "textlist");
  ~TTextList();

  void addItem(TTextListItem *item);
  void removeItem(const string &itemId);
  void clearAll();

  int getItemCount() const;
  TTextListItem *getItem(int i) const;

  // returns the index of item, -1 if not present
  int itemToIndex(const string &itemId);

  int getSelectedItemCount() const;
  TTextListItem *getSelectedItem(int i) const;
  string getSelectedItemId(
      int i) const;  // returns the id of the i-th item selected

  void select(int i, bool on);
  void select(const string &itemId, bool on);
  void unselectAll();

  bool isSelected(int i) const;
  bool isSelected(const string &itemId) const;

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
