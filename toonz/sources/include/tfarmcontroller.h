#pragma once

#ifndef TFARMCONTROLLER_H
#define TFARMCONTROLLER_H

#include <vector>

#include "tfarmtask.h"
//#include "texception.h"
#include "tconvert.h"
//#include "tfilepath.h"

class TFilePath;

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

enum ServerState { Ready, Busy, NotResponding, Down, Offline, ServerUnknown };

class ServerIdentity {
public:
  ServerIdentity(const QString &id, const QString &name)
      : m_id(id), m_name(name) {}

  QString m_id;
  QString m_name;
};

class ServerInfo {
public:
  QString m_name;
  QString m_ipAddress;
  QString m_portNumber;

  ServerState m_state;

  QString m_platform;

  int m_cpuCount;
  unsigned int m_totPhysMem;
  unsigned int m_totVirtMem;

  // the following fields can be used just if the server state is not Down
  unsigned int m_availPhysMem;
  unsigned int m_availVirtMem;
  QString m_currentTaskId;
};

//------------------------------------------------------------------------------

class TaskShortInfo {
public:
  TaskShortInfo(const QString &id, const QString &name, TaskState status)
      : m_id(id), m_name(name), m_status(status) {}

  QString m_id;
  QString m_name;
  TaskState m_status;
};

//------------------------------------------------------------------------------

class TFARMAPI TFarmController {
public:
  virtual ~TFarmController() {}

  virtual QString addTask(const TFarmTask &task, bool suspended) = 0;

  virtual void removeTask(const QString &id)   = 0;
  virtual void suspendTask(const QString &id)  = 0;
  virtual void activateTask(const QString &id) = 0;
  virtual void restartTask(const QString &id)  = 0;

  virtual void getTasks(std::vector<QString> &tasks)       = 0;
  virtual void getTasks(const QString &parentId,
                        std::vector<QString> &tasks)       = 0;
  virtual void getTasks(const QString &parentId,
                        std::vector<TaskShortInfo> &tasks) = 0;

  virtual void queryTaskInfo(const QString &id, TFarmTask &task) = 0;

  virtual void queryTaskShortInfo(const QString &id, QString &parentId,
                                  QString &name, TaskState &status) = 0;

  // used (by a server) to notify a server start
  virtual void attachServer(const QString &name, const QString &addr,
                            int port) = 0;

  // used (by a server) to notify a server stop
  virtual void detachServer(const QString &name, const QString &addr,
                            int port) = 0;

  // used by a server to notify a task submission error
  virtual void taskSubmissionError(const QString &taskId, int errCode) = 0;

  // used by a server to notify a task progress
  virtual void taskProgress(const QString &taskId, int step, int stepCount,
                            int frameNumber, FrameState state) = 0;

  // used by a server to notify a task completion
  virtual void taskCompleted(const QString &taskId, int exitCode) = 0;

  // fills the servers vector with the identities of the servers
  virtual void getServers(std::vector<ServerIdentity> &servers) = 0;

  // returns the state of the server whose id has been specified
  virtual ServerState queryServerState2(const QString &id) = 0;

  // fills info with the infoes about the server whose id is specified
  virtual void queryServerInfo(const QString &id, ServerInfo &info) = 0;

  // activates the server whose id has been specified
  virtual void activateServer(const QString &id) = 0;

  // deactivates the server whose id has been specified
  // once deactivated, a server is not available for task rendering
  virtual void deactivateServer(const QString &id,
                                bool completeRunningTasks = true) = 0;
};

//------------------------------------------------------------------------------

class TFARMAPI ControllerData {
public:
  ControllerData(const QString &hostName = "", const QString &ipAddr = "",
                 int port = 0)
      : m_hostName(hostName), m_ipAddress(ipAddr), m_port(port) {}

  bool operator==(const ControllerData &rhs) {
    return m_hostName == rhs.m_hostName && m_ipAddress == rhs.m_ipAddress &&
           m_port == rhs.m_port;
  }

  QString m_hostName;
  QString m_ipAddress;
  int m_port;
};

//------------------------------------------------------------------------------

class TFARMAPI TFarmControllerFactory {
public:
  TFarmControllerFactory();
  int create(const ControllerData &data, TFarmController **controller);
  int create(const QString &hostname, int port, TFarmController **controller);
};

//------------------------------------------------------------------------------

void TFARMAPI loadControllerData(const TFilePath &fp, ControllerData &data);

#endif
