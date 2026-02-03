#pragma once

#ifndef TABPAGE_H
#define TABPAGE_H

#include "tw/tw.h"

class TabPage : public TWidget {
public:
  TabPage(TWidget *parent, const string &name) : TWidget(parent, name) {}
  virtual ~TabPage() {}

  virtual void onActivate() {}
  virtual void onDeactivate() {}

  virtual void update() {}
};

#endif
