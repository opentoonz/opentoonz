#pragma once

#ifndef FILEBROWSER_INCLUDED
#define FILEBROWSER_INCLUDED

#include "tw/tw.h"
#include "tfilepath.h"

//-------------------------------------------------------------------

class GenericFileBrowserAction {
public:
  virtual ~GenericFileBrowserAction() {}
  virtual void sendCommand(const TFilePath &) = 0;
};

//-------------------------------------------------------------------

template <class T>
class FileBrowserAction : public GenericFileBrowserAction {
public:
  typedef void (T::*Method)(const TFilePath &);
  FileBrowserAction(T *target, Method method)
      : m_target(target), m_method(method) {}
  void sendCommand(const TFilePath &fp) { (m_target->*m_method)(fp); }

private:
  T *m_target;
  Method m_method;
};

//-------------------------------------------------------------------

class FileBrowser : public TWidget {
public:
  FileBrowser(TWidget *parent, string name, const vector<string> &fileTypes);
  ~FileBrowser();

  void setFilter(const vector<string> &fileTypes);

  void configureNotify(const TDimension &d);
  void setBackgroundColor(const TGuiColor &);
  void draw();

  void setFileSelChangeAction(GenericFileBrowserAction *action);
  void setFileDblClickAction(GenericFileBrowserAction *action);

  TFilePath getCurrentDir() const;
  void setCurrentDir(const TFilePath &dirPath);

  void selectParentDirectory();

private:
  class Data;
  Data *m_data;
};

#endif
