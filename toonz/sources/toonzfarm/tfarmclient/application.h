#pragma once

#ifndef APPLICATION_H
#define APPLICATION_H

// forward declarations
class TFarmController;

#include "tfilepath.h"

//------------------------------------------------------------------------------

class RenderFarmTasksObserver {
public:
  virtual void onChange() = 0;
};

//------------------------------------------------------------------------------

class Application {  // singleton
public:
  ~Application();
  static Application *instance();

  void init();

  TFarmController *getController();
  bool testControllerConnection() const;
  void getControllerData(string &hostName, string &ipAddr, int &port) const;

  void setCurrentFolder(const TFilePath &fp);
  TFilePath getCurrentFolder();

private:
  class Imp;
  Imp *m_imp;

  Application();

  // not implemented
  Application(const Application &);
  Application &operator=(const Application &);
};

#endif
