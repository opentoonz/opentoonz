#pragma once

#ifndef TFARMSERVER_H
#define TFARMSERVER_H

#include <memory>

#include "tcommon.h"

#include <QString>

#include "tfarmplatforms.h"

//------------------------------------------------------------------------------

#ifdef TFARMAPI
#undef TFARMAPI
#endif

#ifdef _WIN32
#ifdef TFARM_EXPORTS
#define TFARMAPI __declspec(dllexport)
#else
#define TFARMAPI __declspec(dllimport)
#endif
#else
#define TFARMAPI
#endif

//------------------------------------------------------------------------------

class TFARMAPI TFarmServer {
public:
  class HwInfo {
  public:
    HwInfo()
        : m_cpuCount(0)
        , m_totPhysMem(0)
        , m_availPhysMem(0)
        , m_totVirtMem(0)
        , m_availVirtMem(0)
        , m_type(NoPlatform) {}

    int m_cpuCount;
    unsigned int m_totPhysMem;
    unsigned int m_availPhysMem;
    unsigned int m_totVirtMem;
    unsigned int m_availVirtMem;
    TFarmPlatform m_type;
  };

  virtual ~TFarmServer() {}

  virtual int addTask(const QString &taskid, const QString &cmdline) = 0;
  virtual int terminateTask(const QString &taskid)  = 0;
  virtual int getTasks(std::vector<QString> &tasks) = 0;

  virtual void queryHwInfo(HwInfo &hwInfo) = 0;

  // used (by a controller) to notify a controller start
  virtual void attachController(const QString &name, const QString &addr,
                                int port) = 0;

  // used (by a controller) to notify a controller stop
  virtual void detachController(const QString &name, const QString &addr,
                                int port) = 0;
};

//------------------------------------------------------------------------------

class TFARMAPI TFarmServerFactory {
public:
  TFarmServerFactory();
  ~TFarmServerFactory();

  int create(const QString &hostName, const QString &addr, int port,
             TFarmServer **tfserver);
};

//------------------------------------------------------------------------------

class TFARMAPI TFarmServerStub {
public:
  TFarmServerStub(TFarmServer *farmServer, int port);
  ~TFarmServerStub();

  int run();
  int shutdown();

  int getPort() const;

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;
};

#endif
