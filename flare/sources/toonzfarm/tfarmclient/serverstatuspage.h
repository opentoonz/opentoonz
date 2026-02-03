#pragma once

#ifndef SERVERSTATUSPAGE_H
#define SERVERSTATUSPAGE_H

#include "tabPage.h"

class ServerStatusPage : public TabPage {
  class Data;
  Data *m_data;

public:
  ServerStatusPage(TWidget *parent);
  ~ServerStatusPage();

  void configureNotify(const TDimension &size);

  void onActivate();
  void onDeactivate();
  void update();

  // void showServerInfo(const string &name);
};

#endif
