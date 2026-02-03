#pragma once

#ifndef SUBMITPAGE_H
#define SUBMITPAGE_H

#include "tabPage.h"
#include "tasks.h"

class TFilePath;

//------------------------------------------------------------------------------

class TaskConfigPanel : public TWidget {
public:
  TaskConfigPanel(TWidget *parent) : TWidget(parent) {}

  virtual void setTask(SubmitPageTask *task) = 0;
  virtual SubmitPageTask *getTask() const    = 0;
};

//------------------------------------------------------------------------------

class SubmitPage : public TabPage {
public:
  SubmitPage(TWidget *parent);
  ~SubmitPage();

  void configureNotify(const TDimension &size);

  void onActivate();
  void onDeactivate();

  SubmitPageTask *getTask() const;
  void setTask(SubmitPageTask *task);
  void onTextField(const string &name, bool isName);

private:
  class Data;
  Data *m_data;
};

#endif
