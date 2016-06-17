#pragma once

#ifndef TASKSTATUSPAGE_H
#define TASKSTATUSPAGE_H

#include "tabPage.h"

class TaskStatusPage : public TabPage {
  class Data;
  Data *m_data;

public:
  TaskStatusPage(TWidget *parent);
  ~TaskStatusPage();

  void configureNotify(const TDimension &size);
  void rightButtonDown(const TMouseEvent &e);

  void onActivate();
  void onDeactivate();
  void update();

  void showTaskInfo(const string &id);
};

#endif
