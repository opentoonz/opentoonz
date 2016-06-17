#pragma once

#ifndef APPMAINSHELL_H
#define APPMAINSHELL_H

#include "tw/mainshell.h"

//==============================================================================

class AppMainshell : public TMainshell {
public:
  AppMainshell();
  ~AppMainshell();

  static AppMainshell *instance();

  void init();
  bool beforeShow();
  void configureNotify(const TDimension &size);

  void openProgressBar(string name);
  void closeProgressBar();
  bool setProgressBarFraction(int num, int den);

  void repaint();

  void close();

  TDimension getPreferredSize();
  int getMainIconId();

  void onTimer(int);
  string getAppId() const { return "TFarm"; }

private:
  class Data;
  Data *m_data;
};

#endif
