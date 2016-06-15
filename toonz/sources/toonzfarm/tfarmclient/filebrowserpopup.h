#pragma once

#ifndef FILEBROWSERPOPUP_INCLUDED
#define FILEBROWSERPOPUP_INCLUDED

#include "tw/popup.h"
#include "tfilepath.h"

//-------------------------------------------------------------------

class TGenericFileBrowserPopupAction {
public:
  virtual ~TGenericFileBrowserPopupAction() {}
  virtual void sendCommand(const TFilePath &) = 0;
};

//-------------------------------------------------------------------

template <class T>
class TFileBrowserPopupAction : public TGenericFileBrowserPopupAction {
public:
  typedef void (T::*Method)(const TFilePath &);
  TFileBrowserPopupAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(const TFilePath &fp) { (m_target->*m_method)(fp); }

private:
  T *m_target;
  Method m_method;
};

//------------------------------------------------------------------------------

class FileBrowserPopup : public TPopup {
public:
  FileBrowserPopup(TWidget *parent);
  FileBrowserPopup(TWidget *parent, const vector<string> &fileTypes);

  ~FileBrowserPopup();

  void configureNotify(const TDimension &d);
  TDimension getPreferredSize() const;
  void draw();

  void setCurrentDir(const TFilePath &dirPath);
  void setFilter(const vector<string> &fileTypes);

  void setOkAction(TGenericFileBrowserPopupAction *action);

  void popup(const TPoint &p);

private:
  class Data;
  Data *m_data;
};

#endif
