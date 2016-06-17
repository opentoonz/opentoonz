#pragma once

#ifndef DEPENDEDLIST_H
#define DEPENDEDLIST_H

#include "tw/popup.h"
#include "tw/button.h"
#include "tw/mainshell.h"

#include "textlist.h"
#include <map>

class TaskShortInfo;

//-------------------------------------------------------------------

class TGenericDependedPopupAction {
public:
  virtual ~TGenericDependedPopupAction() {}
  virtual void sendCommand(const vector<string> &) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TDependedPopupAction : public TGenericDependedPopupAction {
public:
  typedef void (T::*Method)(const vector<string> &);
  TDependedPopupAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(const vector<string> &tasks) {
    (m_target->*m_method)(tasks);
  }

private:
  T *m_target;
  Method m_method;
};

//==============================================================================

class DependedList : public TWidget {
  TTextList *m_depList;
  TButton *m_add;
  TButton *m_remove;
  map<string, string> m_tasks;

public:
  DependedList(TWidget *parent);

  void configureNotify(const TDimension &size);
  void onAdd();
  void onRemove();
  void clearAll();
  void setList(const map<string, string> &tasks);
  void AddItems(const vector<string> &tasksId);
};

//==============================================================================

class DependedPopup : public TModalPopup {
public:
  DependedPopup(TWidget *parent);

  void configureNotify(const TDimension &d);

  TDimension getPreferredSize() const;

  void onOk();
  void setList(const vector<TaskShortInfo> &tasks);

  void setOkAction(TGenericDependedPopupAction *action);

private:
  TTextList *m_submitList;
  TButton *m_ok;
  TButton *m_cancel;
  TGenericDependedPopupAction *m_okAction;
};

#endif
