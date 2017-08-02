

#include "tfarmcontroller.h"
#include "tfarmexecutor.h"
#include "tfarmserver.h"
#include "tsystem.h"
#include "tconvert.h"
#include "tfilepath_io.h"
#include "service.h"
#include "tcli.h"
#include "tversion.h"
using namespace TVER;

#include "tthreadmessage.h"
#include "tthread.h"
#include "tstream.h"
#include "tlog.h"

#include <QObject>
#include <QCoreApplication>
#include <QEventLoop>

#include "tthread.h"

#include <sstream>
#include <string>
using namespace std;

#ifndef _WIN32
#include <sys/param.h>
#include <unistd.h>
#include <sys/timeb.h>
#endif

int inline STRICMP(const QString &a, const QString &b) {
  return a.compare(b, Qt::CaseSensitive);
}
int inline STRICMP(const char *a, const char *b) {
  QString str(a);
  return str.compare(QString(b), Qt::CaseSensitive);
}
/*
#ifdef _WIN32
#define STRICMP stricmp
#else
#define STRICMP strcasecmp
#endif
*/

#ifndef _WIN32
#define NO_ERROR 0
#endif

//==============================================================================

namespace {

TFilePath getGlobalRoot() {
  TVER::ToonzVersion tver;
  TFilePath rootDir;

#ifdef _WIN32
  std::string regpath = "SOFTWARE\\" + tver.getAppName() + "\\" +
                        tver.getAppName() + "\\" + tver.getAppVersionString() +
                        "\\FARMROOT";
  TFilePath name(regpath);
  rootDir = TFilePath(TSystem::getSystemValue(name).toStdString());
#else

// Leggo la globalRoot da File txt
#ifdef MACOSX
  // If MACOSX, change to MACOSX path
  std::string unixpath = "./" + tver.getAppName() + "_" +
                         tver.getAppVersionString() +
                         ".app/Contents/Resources/configfarmroot.txt";
#else
  // set path to something suitable for most linux (Unix?) systems
  std::string unixpath = "/etc/" + tver.getAppName() + "/opentoonz.conf";
#endif
  TFilePath name(unixpath);
  Tifstream is(name);
  if (is) {
    char line[1024];
    is.getline(line, 80);

    char *s = line;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\"') s++;
    if (*s != '\0') {
      char *t = s;
      while (*t) t++;

      string pathName(s, t - 1);

      rootDir = TFilePath(pathName);
    }
  }

#endif
  return rootDir;
}

//--------------------------------------------------------------------

TFilePath getLocalRoot() {
  TVER::ToonzVersion tver;
  TFilePath lroot;

#ifdef _WIN32
std:
  string regpath = "SOFTWARE\\" + tver.getAppName() + "\\" + tver.getAppName() +
                   "\\" + tver.getAppVersionString() + "\\FARMROOT";
  TFilePath name(regpath);
  lroot = TFilePath(TSystem::getSystemValue(name).toStdString()) +
          TFilePath("toonzfarm");
#else
// Leggo la localRoot da File txt
#ifdef MACOSX
  // If MACOSX, change to MACOSX path
  std::string unixpath = "./" + tver.getAppName() + "_" +
                         tver.getAppVersionString() +
                         ".app/Contents/Resources/configfarmroot.txt";
#else
  // set path to something suitable for most linux (Unix?) systems
  std::string unixpath = "/etc/" + tver.getAppName() + "/opentoonz.conf";
#endif
  TFilePath name(unixpath);
  Tifstream is(name);
  if (is) {
    char line[1024];
    is.getline(line, 80);

    char *s = line;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\"') s++;
    if (*s != '\0') {
      char *t = s;
      while (*t) t++;

      string pathName(s, t - 1);

      lroot = TFilePath(pathName);
    }
  }
#endif
  return lroot;
}

//--------------------------------------------------------------------

bool myDoesExists(const TFilePath &fp) {
  bool exists = false;
#ifdef _WIN32
  TFileStatus fs(fp);
  exists = fs.doesExist();
#else
  int acc = access(::to_string(fp).c_str(), 00);  // 00 == solo esistenza
  exists  = acc != -1;
#endif
  return exists;
}

//--------------------------------------------------------------------

bool dirExists(const TFilePath &dirFp) {
  bool exists = false;
#ifdef _WIN32
  TFileStatus fs(dirFp);
  exists = fs.isDirectory();
#else
  int acc = access(::to_string(dirFp).c_str(), 00);  // 00 == solo esistenza
  exists  = acc != -1;
#endif
  return exists;
}

//-------------------------------------------------------------------

bool loadControllerData(QString &hostName, QString &addr, int &port) {
  TFilePath groot = getGlobalRoot();
  TFilePath fp    = groot + "config" + "controller.txt";

  ControllerData controllerData;
  ::loadControllerData(fp, controllerData);

  hostName = controllerData.m_hostName;
  addr     = controllerData.m_ipAddress;
  port     = controllerData.m_port;
  return true;
}

//-------------------------------------------------------------------

bool isAScript(TFarmTask *task) {
  return false;  // todo per gli script
}

}  // anonymous namespace

//==============================================================================

class CtrlFarmTask final : public TFarmTask {
public:
  CtrlFarmTask() : m_toBeDeleted(false), m_failureCount(0) {}

  CtrlFarmTask(const QString &id, const QString &name, const QString &cmdline,
               const QString &user, const QString &host, int stepCount,
               int priority)
      : TFarmTask(id, name, cmdline, user, host, stepCount, priority)
      , m_toBeDeleted(false)
      , m_failureCount(0) {
    m_id     = id;
    m_status = Waiting;
  }

  CtrlFarmTask(const CtrlFarmTask &rhs) : TFarmTask(rhs) {
    m_serverId    = rhs.m_serverId;
    m_subTasks    = rhs.m_subTasks;
    m_toBeDeleted = rhs.m_toBeDeleted;
  }

  // TPersist implementation
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  const TPersistDeclaration *getDeclaration() const override;

  QString m_serverId;
  vector<QString> m_subTasks;

  bool m_toBeDeleted;
  int m_failureCount;

  vector<QString> m_failedOnServers;
};

namespace {

class TFarmTaskDeclaration final : public TPersistDeclaration {
public:
  TFarmTaskDeclaration(const std::string &id) : TPersistDeclaration(id) {}

  TPersist *create() const override { return new CtrlFarmTask; }
} Declaration("tfarmtask");
}  // namespace

//------------------------------------------------------------------------------

void CtrlFarmTask::loadData(TIStream &is) {
  is >> m_name;

  QString cmdline;
  is >> cmdline;
  parseCommandLine(cmdline);

  is >> m_priority;

  is >> m_user;
  is >> m_hostName;

  is >> m_id;
  is >> m_parentId;

  int status;
  is >> status;
  m_status = (TaskState)status;
  is >> m_server;
  QString dateStr;
  is >> dateStr;
  m_submissionDate = QDateTime::fromString(dateStr);
  is >> dateStr;
  m_startDate = QDateTime::fromString(dateStr);
  is >> dateStr;
  m_completionDate = QDateTime::fromString(dateStr);

  is >> m_successfullSteps;
  is >> m_failedSteps;
  is >> m_stepCount;

  is >> m_serverId;

  string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "platform") {
      int plat;
      is >> plat;
      switch (plat) {
      case NoPlatform:
        m_platform = NoPlatform;
        break;
      case Windows:
        m_platform = Windows;
        break;
      case Irix:
        m_platform = Irix;
        break;
      case Linux:
        m_platform = Linux;
        break;
      }
    } else if (tagName == "dependencies") {
      int depCount = 0;
      is >> depCount;
      if (depCount > 0) m_dependencies = new Dependencies();
      while (depCount > 0) {
        TFarmTask::Id id;
        is >> id;
        m_dependencies->add(id);
        depCount--;
      }
    }

    is.closeChild();
  }
}

//------------------------------------------------------------------------------

void CtrlFarmTask::saveData(TOStream &os) {
  os << m_name;

  os << getCommandLine();

  os << m_priority;

  os << m_user;
  os << m_hostName;

  os << m_id;
  os << m_parentId;

  os << (int)m_status;
  os << m_server;
  os << m_submissionDate.toString();
  os << m_startDate.toString();
  os << m_completionDate.toString();

  os << m_successfullSteps;
  os << m_failedSteps;
  os << m_stepCount;

  os << m_serverId;

  os.openChild("platform");
  os << m_platform;
  os.closeChild();

  os.openChild("dependencies");
  if (m_dependencies) {
    int depCount = m_dependencies->getTaskCount();
    os << depCount;
    int i = 0;
    while (i < depCount) {
      TFarmTask::Id id = m_dependencies->getTaskId(i++);
      os << id;
    }
  } else
    os << 0;
  os.closeChild();
}

//------------------------------------------------------------------------------

const TPersistDeclaration *CtrlFarmTask::getDeclaration() const {
  return &Declaration;
}

//==============================================================================

class FarmServerProxy {
public:
  FarmServerProxy(const QString &hostName, const QString &addr, int port,
                  int maxTaskCount = 1)
      : m_hostName(hostName)
      , m_addr(addr)
      , m_port(port)
      , m_offline(false)
      , m_attached(false)
      , m_maxTaskCount(maxTaskCount)
      , m_platform(NoPlatform) {
    TFarmServerFactory serverFactory;
    serverFactory.create(m_hostName, m_addr, m_port, &m_server);
  }

  ~FarmServerProxy() {}

  QString getId() const { return getIpAddress(); }

  QString getHostName() const { return m_hostName; }

  QString getIpAddress() const { return m_addr; }

  int getPort() const { return m_port; }

  const vector<QString> &getTasks() const { return m_tasks; }

  int addTask(const CtrlFarmTask *task);
  void terminateTask(const QString &taskId);

  // e' possibile rimuovere un task solo se non running
  void removeTask(const QString &taskId);

  bool testConnection(int timeout);

  void queryHwInfo(TFarmServer::HwInfo &hwInfo) {
    m_server->queryHwInfo(hwInfo);
  }

  void attachController(const QString &name, const QString &addr, int port) {
    m_server->attachController(name, addr, port);
  }

  void detachController(const QString &name, const QString &addr, int port) {
    m_server->detachController(name, addr, port);
  }

  QString m_hostName;
  QString m_addr;
  int m_port;
  bool m_offline;
  bool m_attached;

  int m_maxTaskCount;
  TFarmPlatform m_platform;

  // vettore dei taskId assegnato al server
  vector<QString> m_tasks;

  TFarmServer *m_server;
};

//------------------------------------------------------------------------------

int FarmServerProxy::addTask(const CtrlFarmTask *task) {
  int rc = m_server->addTask(task->m_id, task->getCommandLine());
  if (rc == 0) m_tasks.push_back(task->m_id);
  return rc;
}

//------------------------------------------------------------------------------

void FarmServerProxy::terminateTask(const QString &taskId) {
  m_server->terminateTask(taskId);
}

//------------------------------------------------------------------------------

void FarmServerProxy::removeTask(const QString &taskId) {
  vector<QString>::iterator it = find(m_tasks.begin(), m_tasks.end(), taskId);
  if (it != m_tasks.end()) {
    m_tasks.erase(it);
  }
}

//------------------------------------------------------------------------------

static bool doTestConnection(const QString &hostName, const QString &addr,
                             int port) {
  TTcpIpClient client;

  int sock;
  int ret = client.connect(hostName, addr, port, sock);
  if (ret == OK) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
#ifdef _WIN32
class ConnectionTest final : public TThread::Runnable {
public:
  ConnectionTest(const FarmServerProxy *server, HANDLE hEvent)
      : m_server(server), m_hEvent(hEvent) {}

  void run() override;

  const FarmServerProxy *m_server;
  HANDLE m_hEvent;
};

void ConnectionTest::run() {
  bool res = doTestConnection(m_server->m_hostName, m_server->m_addr,
                              m_server->m_port);

  SetEvent(m_hEvent);
}

#endif

//------------------------------------------------------------------------------

bool FarmServerProxy::testConnection(int timeout) {
#ifdef _WIN32

  HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!hEvent) {
    // se fallisce la creazione dell'evento ci provo comunque
    // senza timeout
    return doTestConnection(m_hostName, m_addr, m_port);
  }

  TThread::Executor executor;
  executor.addTask(new ConnectionTest(this, hEvent));

  DWORD rr = WaitForSingleObject(hEvent, timeout);
  CloseHandle(hEvent);

  if (rr == WAIT_TIMEOUT)
    return false;
  else
    return true;

#else
  return doTestConnection(m_hostName, m_addr, m_port);
#endif

  TTcpIpClient client;

  int sock;
  int ret = client.connect(m_hostName, m_addr, m_port, sock);
  if (ret == OK) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    return true;
  }
  return false;
}

//==============================================================================

class TaskId {
  int m_id;
  int m_subId;

public:
  TaskId(int id, int subId = -1) : m_id(id), m_subId(subId){};
  TaskId(const QString &id) {
    int pos = id.indexOf(".");
    if (pos != -1) {
      m_id    = id.left(pos).toInt();
      m_subId = id.mid(pos + 1, id.length() - pos).toInt();
    } else {
      m_id    = id.toInt();
      m_subId = -1;
    }
  }

  inline bool operator==(const TaskId &f) const {
    return f.m_id == m_id && f.m_subId == m_subId;
  };
  inline bool operator!=(const TaskId &f) const {
    return (m_id != f.m_id || m_subId != f.m_subId);
  };
  inline bool operator<(const TaskId &f) const {
    return (m_id < f.m_id || (m_id == f.m_id && m_subId < f.m_subId));
  };
  inline bool operator>(const TaskId &f) const { return f < *this; }
  inline bool operator>=(const TaskId &f) const { return !operator<(f); }
  inline bool operator<=(const TaskId &f) const { return !operator>(f); }

  TaskId &operator=(const TaskId &f) {
    m_id    = f.m_id;
    m_subId = f.m_subId;
    return *this;
  }

  // operator string() const;
  QString toString() const {
    QString id(QString::number(m_id));
    if (m_subId >= 0) id += "." + ::QString::number(m_subId);
    return id;
  }
};

//==============================================================================

class FarmController final : public TFarmExecutor, public TFarmController {
public:
  FarmController(const QString &hostName, const QString &addr, int port,
                 TUserLog *log);

  void loadServersData(const TFilePath &globalRoot);

  // TFarmExecutor interface implementation

  QString execute(const vector<QString> &argv) override;

  // TFarmController interface methods implementation

  QString addTask(const QString &name, const QString &cmdline,
                  const QString &user, const QString &host, bool suspended,
                  int priority, TFarmPlatform platform);

  QString addTask(const TFarmTask &task, bool suspended) override;

  void removeTask(const QString &id) override;
  void suspendTask(const QString &id) override;
  void activateTask(const QString &id) override;
  void restartTask(const QString &id) override;

  void getTasks(vector<QString> &tasks) override;
  void getTasks(const QString &parentId, vector<QString> &tasks) override;
  void getTasks(const QString &parentId, vector<TaskShortInfo> &tasks) override;

  void queryTaskInfo(const QString &id, TFarmTask &task) override;

  void queryTaskShortInfo(const QString &id, QString &parentId, QString &name,
                          TaskState &status) override;

  // used (by a server) to notify a server start
  void attachServer(const QString &name, const QString &addr,
                    int port) override;

  // used (by a server) to notify a server stop
  void detachServer(const QString &name, const QString &addr,
                    int port) override;

  // used (by a server) to notify a task submission error
  void taskSubmissionError(const QString &taskId, int errCode) override;

  // used by a server to notify a task progress
  void taskProgress(const QString &taskId, int step, int stepCount,
                    int frameNumber, FrameState state) override;

  // used (by a server) to notify a task completion
  void taskCompleted(const QString &taskId, int exitCode) override;

  // fills the servers vector with the names of the servers
  void getServers(vector<ServerIdentity> &servers) override;

  // returns the state of the server whose id has been specified
  ServerState queryServerState2(const QString &id) override;

  // fills info with the infoes about the server whose id is specified
  void queryServerInfo(const QString &id, ServerInfo &info) override;

  // activates the server whose id has been specified
  void activateServer(const QString &id) override;

  // deactivates the server whose id has been specified
  // once deactivated, a server is not available for task rendering
  void deactivateServer(const QString &id, bool completeRunningTasks) override;

  // FarmController specific methods
  CtrlFarmTask *doAddTask(const QString &id, const QString &parentId,
                          const QString &name, const QString &cmdline,
                          const QString &user, const QString &host,
                          bool suspended, int stepCount, int priority,
                          TFarmPlatform platform);

  void startTask(CtrlFarmTask *task, FarmServerProxy *server);

  CtrlFarmTask *getTaskToStart(FarmServerProxy *server = 0);
  CtrlFarmTask *getNextTaskToStart(CtrlFarmTask *task, FarmServerProxy *server);

  // looks for a ready server to which to assign the task
  // returns true iff the task has been started
  bool tryToStartTask(CtrlFarmTask *task);

  ServerState getServerState(FarmServerProxy *server, QString &taskId);

  void initServer(FarmServerProxy *server);

  void load(const TFilePath &fp);
  void save(const TFilePath &fp) const;

  void doRestartTask(const QString &id, bool fromClient,
                     FarmServerProxy *server);

  void activateReadyServers();

  // controller name, address and port
  QString m_hostName;
  QString m_addr;
  int m_port;
  TUserLog *m_userLog;

  map<TaskId, CtrlFarmTask *> m_tasks;
  map<QString, FarmServerProxy *> m_servers;

  TThread::Mutex m_mutex;

  static int NextTaskId;
};

int FarmController::NextTaskId = 0;

//------------------------------------------------------------------------------

FarmController::FarmController(const QString &hostName, const QString &addr,
                               int port, TUserLog *log)
    : TFarmExecutor(port)
    , m_hostName(hostName)
    , m_addr(addr)
    , m_port(port)
    , m_userLog(log) {
  TFilePath rootDir            = getGlobalRoot();
  TFilePath lastUsedIdFilePath = rootDir + "config" + "id.txt";
  Tifstream is(lastUsedIdFilePath);
  if (is.good()) is >> NextTaskId;
}

//------------------------------------------------------------------------------

void FarmController::loadServersData(const TFilePath &globalRoot) {
  TFilePath fp = globalRoot + "config" + "servers.txt";

  Tifstream is(fp);
  while (!is.eof()) {
    char line[80];
    is.getline(line, 80);

    if (line[0] != '#' && QString(line) != "") {
      stringstream iss(line);

      char hostName[512];
      char ipAddr[80];
      int port;

      iss >> hostName >> ipAddr >> port;

      FarmServerProxy *server = new FarmServerProxy(hostName, ipAddr, port);
      m_servers.insert(make_pair(QString(ipAddr), server));

      if (server->testConnection(500)) {
        initServer(server);
        try {
          server->attachController(m_hostName, m_addr, m_port);
        } catch (TException &) {
        }
      }
    }
  }
}

//------------------------------------------------------------------------------

namespace {

inline QString toString(const TFarmTask &task, int ver) {
  QString ss = task.m_name + ",";

  ss += task.getCommandLine() + ",";
  ss += QString::number((int)(task.m_priority)) + ",";
  ss += task.m_user + ",";
  ss += task.m_hostName + ",";
  ss += task.m_id + ",";
  ss += task.m_parentId + ",";
  ss += QString::number((int)(task.m_status)) + ",";
  ss += task.m_server + ",";
  ss += task.m_submissionDate.toString() + ",";
  ss += task.m_startDate.toString() + ",";
  ss += task.m_completionDate.toString() + ",";
  ss += QString::number(task.m_successfullSteps) + ",";
  ss += QString::number(task.m_failedSteps) + ",";
  ss += QString::number(task.m_stepCount);

  if (ver == 2) {
    ss += ",";
    ss += QString::number(task.m_platform) + ",";

    int depCount = 0;
    if (task.m_dependencies) depCount= task.m_dependencies->getTaskCount();

    ss += QString::number(depCount);

    for (int i = 0; i < depCount; ++i) {
      TFarmTask::Id id = task.m_dependencies->getTaskId(i);
      ss += "," + id;
    }
  }

  ss += '\0';

  return ss;
}

inline QString toString(const ServerInfo &info) {
  QString ss = info.m_name + ",";

  ss += info.m_ipAddress + ",";
  ss += info.m_portNumber + ",";
  ss += QString::number((int)(info.m_state)) + ",";
  ss += info.m_platform + ",";

  ss += QString::number((int)info.m_cpuCount) + ",";
  ss += QString::number((int)info.m_totPhysMem) + ",";
  ss += QString::number((int)info.m_totVirtMem) + ",";
  ss += QString::number((int)info.m_availPhysMem) + ",";
  ss += QString::number((int)info.m_availVirtMem) + ",";
  ss += info.m_currentTaskId + '\0';

  return ss;
}

}  // anonymous namespace

//------------------------------------------------------------------------------

QString FarmController::execute(const vector<QString> &argv) {
  QMutexLocker sl(&m_mutex);

  if (argv.size() > 0) {
#ifdef TRACE
    for (int i = 0; i < argv.size(); i++) {
      m_userLog->info(argv[i]);
    }
    m_userLog->info('\n');
#endif

    if (argv[0] == "addTask@string@string" && argv.size() > 5) {
      // ordine degli argomenti:
      // name, cmdline, user, host, suspended, priority, platform

      bool suspended;
      fromStr((int &)suspended, argv[5]);

      int priority;
      fromStr(priority, argv[6]);

      TFarmPlatform platform;
      fromStr((int &)platform, argv[7]);
      return addTask(argv[1], argv[2], argv[3], argv[4], suspended, priority,
                     platform);

    } else if (argv[0] == "addTask@TFarmTask" && argv.size() > 5) {
      // ordine degli argomenti:
      // name, cmdline, user, host, suspended, priority,

      bool suspended;

      fromStr((int &)suspended, argv[5]);

      int priority;
      fromStr(priority, argv[6]);

      TFarmTaskGroup task("", argv[1], argv[2], argv[3], argv[4], priority, 50);

      for (int i = 7; i < (int)argv.size(); i += 5) {
        // ordine degli argomenti:
        // name, cmdline, user, host, priority,

        int subTaskPriority;

        fromStr(subTaskPriority, argv[i + 4]);

        TFarmTask *subTask =
            new TFarmTask("", argv[i], argv[i + 1], argv[i + 2], argv[i + 3],
                          subTaskPriority, 50);

        task.addTask(subTask);
      }

      return addTask(task, suspended);
    } else if (argv[0] == "addTask@TFarmTask_2" && argv.size() > 7) {
      // ordine degli argomenti:
      // name, cmdline, user, host, suspended, stepCount, priority, platform

      int suspended;
      fromStr(suspended, argv[5]);

      int stepCount;
      fromStr(stepCount, argv[6]);

      int priority;
      fromStr(priority, argv[7]);

      TFarmPlatform platform;
      fromStr((int &)platform, argv[8]);

      TFarmTaskGroup task("", argv[1], argv[2], argv[3], argv[4], stepCount,
                          priority);
      task.m_platform = platform;

      int depCount;
      fromStr(depCount, argv[9]);

      int i = 0;
      for (; i < depCount; ++i) {
        QString depTaskId = argv[10 + i];
        task.m_dependencies->add(depTaskId);
      }

      for (i = 10 + depCount; i < (int)argv.size(); i += 6) {
        // ordine degli argomenti:
        // name, cmdline, user, host, stepCount, priority,

        int subTaskStepCount;
        fromStr(subTaskStepCount, argv[i + 4]);

        int subTaskPriority;
        fromStr(subTaskPriority, argv[i + 5]);

        TFarmTask *subTask =
            new TFarmTask("", argv[i], argv[i + 1], argv[i + 2], argv[i + 3],
                          subTaskStepCount, subTaskPriority);

        subTask->m_dependencies =
            new TFarmTask::Dependencies(*task.m_dependencies);
        subTask->m_platform = platform;

        task.addTask(subTask);
      }

      return addTask(task, suspended != 0);
    } else if (argv[0] == "removeTask" && argv.size() > 0) {
      removeTask(argv[1]);
    } else if (argv[0] == "suspendTask" && argv.size() > 0) {
      suspendTask(argv[1]);
    } else if (argv[0] == "activateTask" && argv.size() > 0) {
      activateTask(argv[1]);
    } else if (argv[0] == "restartTask" && argv.size() > 0) {
      restartTask(argv[1]);
    } else if (argv[0] == "getTasks@vector") {
      vector<QString> tasks;
      getTasks(tasks);

      QString reply;
      std::vector<QString>::iterator it = tasks.begin();
      for (; it != tasks.end(); ++it) {
        reply += *it;
        reply += ",";
      }

      return reply;
    } else if (argv[0] == "getTasks@string@vector") {
      QString parentId;
      if (argv.size() > 1) parentId = argv[1];

      vector<QString> tasks;
      getTasks(parentId, tasks);

      QString reply;
      std::vector<QString>::iterator it = tasks.begin();
      for (; it != tasks.end(); ++it) {
        reply += *it;
        reply += ",";
      }

      if (reply.length() > 0) reply = reply.left(reply.length() - 1);
      return reply;
    } else if (argv[0] == "getTasks@string@vector$TaskShortInfo" &&
               argv.size() > 0) {
      vector<TaskShortInfo> tasks;
      getTasks(argv[1], tasks);

      QString reply;
      std::vector<TaskShortInfo>::iterator it = tasks.begin();
      for (; it != tasks.end(); ++it) {
        reply += (*it).m_id;
        reply += ",";
        reply += (*it).m_name;
        reply += ",";
        reply += QString::number((*it).m_status);
        reply += ",";
      }
      if (reply.length() > 0) reply = reply.left(reply.size() - 1);

      return reply;
    } else if (argv[0] == "queryTaskInfo" && argv.size() > 1) {
      TFarmTask task;
      queryTaskInfo(argv[1], task);
      return toString(task, 1);
    } else if (argv[0] == "queryTaskInfo_2" && argv.size() > 1) {
      TFarmTask task;
      queryTaskInfo(argv[1], task);
      return toString(task, 2);
    } else if (argv[0] == "queryTaskShortInfo" && argv.size() > 1) {
      QString parentId, name;
      TaskState status;
      queryTaskShortInfo(argv[1], parentId, name, status);

      QString reply;
      reply += parentId;
      reply += ",";
      reply += name;
      reply += ",";
      reply += QString::number((int)(status));
      return reply;
    } else if (argv[0] == "taskSubmissionError" && argv.size() > 2) {
      QString taskId = argv[1];
      int errCode;
      fromStr(errCode, argv[2]);
      taskSubmissionError(taskId, errCode);
      return "";
    } else if (argv[0] == "taskProgress" && argv.size() > 5) {
      int step, stepCount, frameNumber;
      step        = argv[2].toInt();
      stepCount   = argv[3].toInt();
      frameNumber = argv[4].toInt();

      FrameState state;
      state = (FrameState)argv[5].toInt();
      taskProgress(argv[1], step, stepCount, frameNumber, state);
      return "";
    } else if (argv[0] == "taskCompleted" && argv.size() > 2) {
      QString taskId = argv[1];
      int exitCode;
      exitCode = argv[2].toInt();

      taskCompleted(taskId, exitCode);
      return "";
    } else if (argv[0] == "getServers") {
      vector<ServerIdentity> servers;
      getServers(servers);

      QString reply;
      std::vector<ServerIdentity>::iterator it = servers.begin();
      for (; it != servers.end(); ++it) {
        reply += (*it).m_id;
        reply += ",";
        reply += (*it).m_name;
        reply += ",";
      }

      if (reply.length() > 0) reply = reply.left(reply.size() - 1);

      return reply;
    } else if (argv[0] == "queryServerState2" && argv.size() > 0) {
      ServerState state = queryServerState2(argv[1]);
      return QString::number(state);
    } else if (argv[0] == "queryServerInfo" && argv.size() > 0) {
      ServerInfo info;
      queryServerInfo(argv[1], info);
      return toString(info);
    } else if (argv[0] == "activateServer" && argv.size() > 0) {
      activateServer(argv[1]);
      return "";
    } else if (argv[0] == "deactivateServer" && argv.size() > 1) {
      int completeRunningTask = true;
      fromStr(completeRunningTask, argv[2]);
      deactivateServer(argv[1], !!completeRunningTask);
      return "";
    } else if (argv[0] == "attachServer" && argv.size() > 3) {
      int port;
      fromStr(port, argv[3]);
      attachServer(argv[1], argv[2], port);
      return "";
    } else if (argv[0] == "detachServer" && argv.size() > 3) {
      int port;
      fromStr(port, argv[3]);
      detachServer(argv[1], argv[2], port);
      return "";
    }
  }
#ifdef TRACE
  else
    m_userLog->info("empty command\n");
#endif
  return "";
}

//------------------------------------------------------------------------------

CtrlFarmTask *FarmController::doAddTask(
    const QString &id, const QString &parentId, const QString &name,
    const QString &cmdline, const QString &user, const QString &host,
    bool suspended, int stepCount, int priority, TFarmPlatform platform) {
  CtrlFarmTask *task =
      new CtrlFarmTask(id, name, cmdline, user, host, stepCount, priority);
  task->m_submissionDate = QDateTime::currentDateTime();
  task->m_parentId       = parentId;
  task->m_platform       = platform;

  m_tasks.insert(std::make_pair(TaskId(id), task));

  m_userLog->info("Task " + task->m_id + " received at " +
                  task->m_submissionDate.toString() + "\n");
  m_userLog->info("\"" + task->getCommandLine() + "\"\n");

  if (suspended) task->m_status = Suspended;

  return task;
}

//------------------------------------------------------------------------------

void FarmController::startTask(CtrlFarmTask *task, FarmServerProxy *server) {
  QMutexLocker sl(&m_mutex);

  CtrlFarmTask *taskToBeSubmittedParent = 0;
  CtrlFarmTask *taskToBeSubmitted       = 0;

  if (task->m_subTasks.empty()) {
    taskToBeSubmitted = task;
    if (task->m_parentId != "") {
      map<TaskId, CtrlFarmTask *>::iterator itTaskParent =
          m_tasks.find(TaskId(task->m_parentId));
      if (itTaskParent != m_tasks.end()) {
        taskToBeSubmittedParent = itTaskParent->second;
      }
    }
  } else {
    taskToBeSubmittedParent = task;

    // cerca il primo subtask WAITING
    std::vector<QString>::iterator itSubTaskId = task->m_subTasks.begin();
    for (; itSubTaskId != task->m_subTasks.end(); ++itSubTaskId) {
      QString subTaskId = *itSubTaskId;

      map<TaskId, CtrlFarmTask *>::iterator itSubTask =
          m_tasks.find(TaskId(subTaskId));
      if (itSubTask != m_tasks.end()) {
        CtrlFarmTask *subTask = itSubTask->second;
        if (subTask->m_status == Waiting) {
          taskToBeSubmitted = subTask;
          break;
        }
      }
    }
  }

  int rc = 0;
  try {
    server->addTask(taskToBeSubmitted);
  } catch (TException &e) {
    throw e;
  }

  if (rc == 0) {
    QDateTime startDate = QDateTime::currentDateTime();

    if (taskToBeSubmittedParent &&
        taskToBeSubmittedParent->m_status != Running) {
      taskToBeSubmittedParent->m_status    = Running;
      taskToBeSubmittedParent->m_startDate = startDate;
    }

    taskToBeSubmitted->m_status    = Running;
    taskToBeSubmitted->m_startDate = startDate;

    taskToBeSubmitted->m_serverId = server->getId();

    QString msg = "Task " + taskToBeSubmitted->m_id + " assigned to ";
    msg += server->getHostName();
    msg += "\n\n";
    m_userLog->info(msg);
  }
}

//------------------------------------------------------------------------------

CtrlFarmTask *FarmController::getTaskToStart(FarmServerProxy *server) {
  QMutexLocker sl(&m_mutex);

  int maxPriority         = 0;
  CtrlFarmTask *candidate = 0;

  map<TaskId, CtrlFarmTask *>::iterator itTask = m_tasks.begin();
  for (; itTask != m_tasks.end(); ++itTask) {
    CtrlFarmTask *task = itTask->second;
    if ((!server || (task->m_platform == NoPlatform ||
                     task->m_platform == server->m_platform)) &&
        ((task->m_status == Waiting && task->m_priority > maxPriority) ||
         (task->m_status == Aborted && task->m_failureCount < 3) &&
             task->m_parentId != "")) {
      bool dependenciesCompleted = true;

      if (task->m_dependencies) {
        int count = task->m_dependencies->getTaskCount();
        for (int i = 0; i < count; ++i) {
          TFarmTask::Id id = task->m_dependencies->getTaskId(i);
          map<TaskId, CtrlFarmTask *>::iterator itDepTask =
              m_tasks.find(TaskId(id));
          if (itDepTask != m_tasks.end()) {
            CtrlFarmTask *depTask = itDepTask->second;
            if (depTask->m_status != Completed) {
              dependenciesCompleted = false;
              break;
            }
          }
        }
      }

      if (dependenciesCompleted) {
        maxPriority = task->m_priority;
        candidate   = task;
      }
    }
  }

  return candidate;
}

//------------------------------------------------------------------------------

// determina il prossimo task da avviare tra quelli Waiting, escludendo except
// dalla ricerca

CtrlFarmTask *FarmController::getNextTaskToStart(CtrlFarmTask *except,
                                                 FarmServerProxy *server) {
  QMutexLocker sl(&m_mutex);

  int maxPriority         = 0;
  CtrlFarmTask *candidate = 0;

  map<TaskId, CtrlFarmTask *>::iterator itTask = m_tasks.begin();
  for (; itTask != m_tasks.end(); ++itTask) {
    CtrlFarmTask *task = itTask->second;
    if (except == task) continue;
    if ((task->m_platform == NoPlatform ||
         task->m_platform == server->m_platform) &&
        task->m_status == Waiting && task->m_priority > maxPriority) {
      bool dependenciesCompleted = true;

      if (task->m_dependencies) {
        int count = task->m_dependencies->getTaskCount();
        for (int i = 0; i < count; ++i) {
          TFarmTask::Id id = task->m_dependencies->getTaskId(i);
          map<TaskId, CtrlFarmTask *>::iterator itDepTask =
              m_tasks.find(TaskId(id));
          if (itDepTask != m_tasks.end()) {
            CtrlFarmTask *depTask = itDepTask->second;
            if (depTask->m_status != Completed) {
              dependenciesCompleted = false;
              break;
            }
          }
        }
      }

      if (dependenciesCompleted) {
        maxPriority = task->m_priority;
        candidate   = task;
      }
    }
  }

  return candidate;
}

//------------------------------------------------------------------------------

bool FarmController::tryToStartTask(CtrlFarmTask *task) {
  QMutexLocker sl(&m_mutex);

  bool dependenciesCompleted = true;

  if (task->m_dependencies) {
    int count = task->m_dependencies->getTaskCount();
    for (int i = 0; i < count; ++i) {
      TFarmTask::Id id = task->m_dependencies->getTaskId(i);
      map<TaskId, CtrlFarmTask *>::iterator itDepTask =
          m_tasks.find(TaskId(id));
      if (itDepTask != m_tasks.end()) {
        CtrlFarmTask *depTask = itDepTask->second;
        if (depTask->m_status != Completed) {
          dependenciesCompleted = false;
          break;
        }
      }
    }
  }

  if (!dependenciesCompleted) return false;

  if (task->m_subTasks.empty()) {
    vector<FarmServerProxy *> m_partiallyBusyServers;

    map<QString, FarmServerProxy *>::iterator it = m_servers.begin();
    for (; it != m_servers.end(); ++it) {
      FarmServerProxy *server = it->second;
      if (server->m_attached && !server->m_offline &&
          (int)server->getTasks().size() < server->m_maxTaskCount) {
        if (!(task->m_platform == NoPlatform ||
              task->m_platform == server->m_platform))
          continue;

        vector<QString>::iterator its =
            find(task->m_failedOnServers.begin(), task->m_failedOnServers.end(),
                 server->getId());

        if (its != task->m_failedOnServers.end()) continue;

        if (server->testConnection(500)) {
          if (server->getTasks().size() == 0) {
            try {
              startTask(task, server);
            } catch (TException & /*e*/) {
              continue;
            }

            return true;
          } else
            m_partiallyBusyServers.push_back(server);
        }
      }
    }

    vector<FarmServerProxy *>::iterator it2 = m_partiallyBusyServers.begin();
    for (; it2 != m_partiallyBusyServers.end(); ++it2) {
      FarmServerProxy *server = *it2;
      if (server->testConnection(500)) {
        try {
          startTask(task, server);
        } catch (TException & /*e*/) {
          continue;
        }
      }

      return true;
    }

    return false;
  } else {
    // un task composto e' considerato started sse e' started almeno uno
    // dei task che lo compongono

    bool started                          = false;
    vector<QString>::iterator itSubTaskId = task->m_subTasks.begin();
    for (; itSubTaskId != task->m_subTasks.end(); ++itSubTaskId) {
      map<TaskId, CtrlFarmTask *>::iterator itSubTask =
          m_tasks.find(TaskId(*itSubTaskId));
      if (itSubTask != m_tasks.end()) {
        CtrlFarmTask *subTask = itSubTask->second;
        if (tryToStartTask(subTask)) started= true;
      }
    }

    return started;
  }
}

//------------------------------------------------------------------------------

ServerState FarmController::getServerState(FarmServerProxy *server,
                                           QString &taskId) {
  ServerState state;

  bool connected = server->testConnection(500);
  if (!connected) {
    taskId = "";
    state  = Down;
  } else {
    if (server->m_offline)
      return Offline;
    else if (server->getTasks().size() > 0) {
      taskId = server->getTasks()[0];
      state  = Busy;
    } else {
      taskId = "";
      state  = Ready;
    }
  }
  return state;
}

//------------------------------------------------------------------------------

class ServerInitializer final : public TThread::Runnable {
public:
  ServerInitializer(FarmServerProxy *server) : m_server(server) {}

  void run() override {
    TFarmServer::HwInfo hwInfo;
    try {
      m_server->queryHwInfo(hwInfo);
    } catch (TException & /*e*/) {
      return;
    }

    m_server->m_attached = true;
    // m_server->m_maxTaskCount = hwInfo.m_cpuCount;
    m_server->m_maxTaskCount = 1;
  }

  FarmServerProxy *m_server;
};

//------------------------------------------------------------------------------

void FarmController::initServer(FarmServerProxy *server) {
  TFarmServer::HwInfo hwInfo;
  try {
    server->queryHwInfo(hwInfo);
  } catch (TException & /*e*/) {
    TThread::Executor exec;
    exec.addTask(new ServerInitializer(server));
    return;
  }

  server->m_attached = true;
  // server->m_maxTaskCount = hwInfo.m_cpuCount;
  server->m_maxTaskCount = 1;

  server->m_platform = hwInfo.m_type;
}

//------------------------------------------------------------------------------

QString FarmController::addTask(const QString &name, const QString &cmdline,
                                const QString &user, const QString &host,
                                bool suspended, int priority,
                                TFarmPlatform platform) {
  QString parentId = "";
  CtrlFarmTask *task =
      doAddTask(QString::number(NextTaskId++), parentId, name, cmdline, user,
                host, suspended, 1, priority, platform);

  return task->m_id;
}

//------------------------------------------------------------------------------

class TaskStarter final : public TThread::Runnable {
public:
  TaskStarter(FarmController *controller, CtrlFarmTask *task,
              FarmServerProxy *server = 0)
      : m_controller(controller), m_task(task), m_server(server) {}

  void run() override;

  FarmController *m_controller;
  CtrlFarmTask *m_task;
  FarmServerProxy *m_server;
};

void TaskStarter::run() {
  if (m_task->m_status != Suspended) {
    if (m_server)
      m_controller->startTask(m_task, m_server);
    else {
      m_controller->tryToStartTask(m_task);
    }
  }
}

//------------------------------------------------------------------------------

QString FarmController::addTask(const TFarmTask &task, bool suspended) {
  QString id = QString::number(NextTaskId++);

  CtrlFarmTask *myTask = 0;

  int count = task.getTaskCount();
  if (count == 1) {
    QString parentId = "";
    myTask = doAddTask(id, parentId, task.m_name, task.getCommandLine(),
                       task.m_user, task.m_hostName, suspended,
                       task.m_stepCount, task.m_priority, task.m_platform);
  } else {
    myTask =
        new CtrlFarmTask(id, task.m_name, task.getCommandLine(), task.m_user,
                         task.m_hostName, task.m_stepCount, task.m_priority);

    myTask->m_submissionDate = QDateTime::currentDateTime();
    myTask->m_parentId       = "";
    myTask->m_dependencies = new TFarmTask::Dependencies(*task.m_dependencies);

    if (suspended) myTask->m_status = Suspended;

    m_tasks.insert(std::make_pair(TaskId(id), myTask));

    for (int i = 0; i < count; ++i) {
      QString subTaskId  = id + "." + QString::number(i);
      TFarmTask &tt      = const_cast<TFarmTask &>(task);
      TFarmTask *subtask = tt.getTask(i);

      CtrlFarmTask *mySubTask = doAddTask(
          subTaskId, myTask->m_id, subtask->m_name, subtask->getCommandLine(),
          subtask->m_user, subtask->m_hostName, suspended, subtask->m_stepCount,
          subtask->m_priority, task.m_platform);

      mySubTask->m_dependencies =
          new TFarmTask::Dependencies(*task.m_dependencies);

      myTask->m_subTasks.push_back(subTaskId);
    }
  }

  TThread::Executor executor;
  executor.addTask(new TaskStarter(this, myTask));

  return id;
}

//------------------------------------------------------------------------------

void FarmController::removeTask(const QString &id) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.find(TaskId(id));
  if (it != m_tasks.end()) {
    CtrlFarmTask *task = it->second;

    bool aSubtaskIsRunning = false;

    vector<QString>::iterator it2 = task->m_subTasks.begin();
    for (; it2 != task->m_subTasks.end();) {
      QString subTaskId = *it2;
      map<TaskId, CtrlFarmTask *>::iterator it3 =
          m_tasks.find(TaskId(subTaskId));
      if (it3 != m_tasks.end()) {
        CtrlFarmTask *subTask = it3->second;
        if (subTask->m_status != Running) {
          it2 = task->m_subTasks.erase(it2);
          m_tasks.erase(it3);
          delete it3->second;
        } else {
          it2 = task->m_subTasks.erase(it2);

          map<QString, FarmServerProxy *>::iterator itServer =
              m_servers.find(subTask->m_serverId);

          if (itServer != m_servers.end()) {
            FarmServerProxy *server = itServer->second;
            if (server) {
              vector<QString>::const_iterator it3 =
                  find(server->getTasks().begin(), server->getTasks().end(),
                       subTask->m_id);

              if (it3 != server->getTasks().end()) {
                aSubtaskIsRunning = true;
                server->terminateTask(subTask->m_id);
              }
            }
          }

          subTask->m_toBeDeleted = true;
        }
      } else
        ++it2;
    }

    if (task->m_status != Running || !aSubtaskIsRunning) {
      m_tasks.erase(it);
      delete it->second;
    } else
      task->m_toBeDeleted = true;
  }
}

//------------------------------------------------------------------------------

void FarmController::suspendTask(const QString &id) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.find(TaskId(id));
  if (it != m_tasks.end()) {
    CtrlFarmTask *task = it->second;

    vector<QString>::iterator it2 = task->m_subTasks.begin();
    for (; it2 != task->m_subTasks.end(); ++it2) {
      QString subTaskId = *it2;
      map<TaskId, CtrlFarmTask *>::iterator it3 =
          m_tasks.find(TaskId(subTaskId));
      if (it3 != m_tasks.end()) {
        CtrlFarmTask *subTask = it3->second;
        if (subTask->m_status == Running) {
          map<QString, FarmServerProxy *>::iterator itServer =
              m_servers.find(subTask->m_serverId);

          if (itServer != m_servers.end()) {
            FarmServerProxy *server = itServer->second;
            if (server) {
              vector<QString>::const_iterator it3 =
                  find(server->getTasks().begin(), server->getTasks().end(),
                       subTask->m_id);

              if (it3 != server->getTasks().end())
                server->terminateTask(subTask->m_id);
            }
          }
        }
        subTask->m_status = Suspended;
      }
    }
    task->m_status = Suspended;
  }
}

//------------------------------------------------------------------------------

void FarmController::activateTask(const QString &id) {}

//------------------------------------------------------------------------------

void FarmController::restartTask(const QString &id) {
  // la scelta del server e' lasciata al controller
  FarmServerProxy *server = 0;
  doRestartTask(id, true, server);
}

//------------------------------------------------------------------------------

void FarmController::doRestartTask(const QString &id, bool fromClient,
                                   FarmServerProxy *server) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.find(TaskId(id));
  if (it != m_tasks.end()) {
    CtrlFarmTask *task = it->second;
    if (task->m_status != Running) {
      if (fromClient) {
        task->m_failedOnServers.clear();
        task->m_status = Waiting;
      }

      task->m_completionDate = QDateTime();
      task->m_server         = "";
      task->m_serverId       = "";
      task->m_failedSteps = task->m_successfullSteps = 0;
      task->m_failureCount                           = 0;

      if (!task->m_subTasks.empty()) {
        vector<QString>::iterator itSubTaskId = task->m_subTasks.begin();
        for (; itSubTaskId != task->m_subTasks.end(); ++itSubTaskId) {
          map<TaskId, CtrlFarmTask *>::iterator itSubTask =
              m_tasks.find(TaskId(*itSubTaskId));
          if (itSubTask != m_tasks.end()) {
            CtrlFarmTask *subtask = itSubTask->second;
            if (fromClient) {
              subtask->m_failedOnServers.clear();
              subtask->m_status = Waiting;
            }

            subtask->m_completionDate = QDateTime();
            subtask->m_server         = "";
            subtask->m_serverId       = "";
            subtask->m_failedSteps = subtask->m_successfullSteps = 0;
            subtask->m_failureCount                              = 0;
          }
        }
      }

      TThread::Executor executor;
      executor.addTask(new TaskStarter(this, task, server));
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::getTasks(vector<QString> &tasks) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    CtrlFarmTask *task = it->second;
    tasks.push_back(task->m_id);
  }
}

//------------------------------------------------------------------------------

void FarmController::getTasks(const QString &parentId, vector<QString> &tasks) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    CtrlFarmTask *task = it->second;
    if (task->m_parentId == parentId) tasks.push_back(task->m_id);
  }
}

//------------------------------------------------------------------------------

void FarmController::getTasks(const QString &parentId,
                              vector<TaskShortInfo> &tasks) {
  tasks.clear();

  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    CtrlFarmTask *task = it->second;
    if (task->m_parentId == parentId)
      tasks.push_back(TaskShortInfo(task->m_id, task->m_name, task->m_status));
  }
}

//------------------------------------------------------------------------------

void FarmController::queryTaskInfo(const QString &id, TFarmTask &task) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.find(TaskId(id));
  if (it != m_tasks.end()) {
    CtrlFarmTask *tt = it->second;
    task             = *tt;

    map<QString, FarmServerProxy *>::iterator it2 =
        m_servers.find(it->second->m_serverId);
    if (it2 != m_servers.end()) {
      FarmServerProxy *server = it2->second;
      task.m_server           = server->getHostName();
    } else
      task.m_server = "";
  } else
    task.m_status = TaskUnknown;
}

//------------------------------------------------------------------------------

void FarmController::queryTaskShortInfo(const QString &id, QString &parentId,
                                        QString &name, TaskState &status) {
  map<TaskId, CtrlFarmTask *>::iterator it = m_tasks.find(TaskId(id));
  if (it != m_tasks.end()) {
    CtrlFarmTask *task = it->second;
    parentId           = task->m_parentId;
    name               = task->m_name;
    status             = task->m_status;
  } else
    status = TaskUnknown;
}

//------------------------------------------------------------------------------

void FarmController::attachServer(const QString &name, const QString &addr,
                                  int port) {
  FarmServerProxy *server                      = 0;
  map<QString, FarmServerProxy *>::iterator it = m_servers.begin();
  for (; it != m_servers.end(); ++it) {
    FarmServerProxy *s = it->second;

    if (STRICMP(s->getHostName(), name) == 0 ||
        STRICMP(s->getIpAddress(), addr) == 0) {
      server = s;
      break;
    }
  }

  if (!server) {
    server = new FarmServerProxy(name, addr, port);
    m_servers.insert(make_pair(QString(addr), server));
  }

  initServer(server);
}

//------------------------------------------------------------------------------

void FarmController::detachServer(const QString &name, const QString &addr,
                                  int port) {
  map<QString, FarmServerProxy *>::iterator it = m_servers.begin();
  for (; it != m_servers.end(); ++it) {
    FarmServerProxy *s = it->second;
    if (STRICMP(s->getHostName(), name) == 0 ||
        STRICMP(s->getIpAddress(), addr) == 0) {
      s->m_attached = false;
      break;
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::taskSubmissionError(const QString &taskId, int errCode) {
  FarmServerProxy *server = 0;

  map<TaskId, CtrlFarmTask *>::iterator itTask = m_tasks.find(TaskId(taskId));
  if (itTask != m_tasks.end()) {
    CtrlFarmTask *task = itTask->second;
    task->m_status     = Aborted;

    task->m_completionDate = QDateTime::currentDateTime();

    if (task->m_toBeDeleted) m_tasks.erase(itTask);

    CtrlFarmTask *parentTask = 0;

    if (task->m_parentId != "") {
      map<TaskId, CtrlFarmTask *>::iterator itParent =
          m_tasks.find(TaskId(task->m_parentId));
      if (itParent != m_tasks.end()) {
        parentTask = itParent->second;

        TaskState parentTaskState = Aborted;
        std::vector<QString>::iterator itSubTaskId =
            parentTask->m_subTasks.begin();
        for (; itSubTaskId != parentTask->m_subTasks.end(); ++itSubTaskId) {
          QString subTaskId = *itSubTaskId;
          map<TaskId, CtrlFarmTask *>::iterator itSubTask =
              m_tasks.find(TaskId(subTaskId));
          if (itSubTask != m_tasks.end()) {
            CtrlFarmTask *subTask = itSubTask->second;
            if (subTask->m_status == Running || subTask->m_status == Waiting) {
              parentTaskState = Running;
              break;
            }
          }
        }

        parentTask->m_status = parentTaskState;
        if (parentTask->m_status == Aborted ||
            parentTask->m_status == Aborted) {
          parentTask->m_completionDate = task->m_completionDate;
          if (parentTask->m_toBeDeleted) m_tasks.erase(itParent);
        }
      }
    }

    map<QString, FarmServerProxy *>::iterator itServer =
        m_servers.find(task->m_serverId);
    if (itServer != m_servers.end()) server = itServer->second;

    if (server) {
      server->removeTask(taskId);
    }

    if (task->m_toBeDeleted) delete task;
    if (parentTask && parentTask->m_toBeDeleted) delete parentTask;
  }

  if (server && !server->m_offline) {
    // cerca un task da sottomettere al server

    itTask = m_tasks.begin();
    for (; itTask != m_tasks.end(); ++itTask) {
      CtrlFarmTask *task = itTask->second;
      if (task->m_status == Waiting) {
        try {
          startTask(task, server);
        } catch (TException & /*e*/) {
          continue;
        }

        break;
      }
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::taskProgress(const QString &taskId, int step,
                                  int stepCount, int frameNumber,
                                  FrameState state) {
  map<TaskId, CtrlFarmTask *>::iterator itTask = m_tasks.find(TaskId(taskId));
  if (itTask != m_tasks.end()) {
    CtrlFarmTask *task = itTask->second;
    if (state == FrameDone)
      ++task->m_successfullSteps;
    else
      ++task->m_failedSteps;

    if (task->m_parentId != "") {
      map<TaskId, CtrlFarmTask *>::iterator itParentTask =
          m_tasks.find(TaskId(task->m_parentId));
      CtrlFarmTask *parentTask = itParentTask->second;
      if (state == FrameDone)
        ++parentTask->m_successfullSteps;
      else
        ++parentTask->m_failedSteps;
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::taskCompleted(const QString &taskId, int exitCode) {
#ifdef TRACE
  m_userLog->info("completed chiamata\n\n");
#endif

  FarmServerProxy *server = 0;

  map<TaskId, CtrlFarmTask *>::iterator itTask = m_tasks.find(TaskId(taskId));
  if (itTask != m_tasks.end()) {
    CtrlFarmTask *task = itTask->second;

    if (task->getCommandLine().contains("runcasm")) {
      if (task->m_failedSteps == 0 && task->m_successfullSteps > 0)
        task->m_status = Completed;
      else
        task->m_status = Aborted;
    } else {
      switch (exitCode) {
      case 0:
        task->m_status = Completed;
        if (isAScript(task)) task->m_successfullSteps= task->m_stepCount;
        break;
      case RENDER_LICENSE_NOT_FOUND:
        task->m_status = Waiting;
        break;
      default:
        if (task->m_status != Suspended) task->m_status = Aborted;
        break;
      }
    }

    task->m_completionDate = QDateTime::currentDateTime();

    if (task->m_status == Aborted) {
      task->m_failedOnServers.push_back(task->m_serverId);
      ++task->m_failureCount;
    }

    if (task->m_toBeDeleted) m_tasks.erase(itTask);

    CtrlFarmTask *parentTask = 0;

    if (task->m_parentId != "") {
      map<TaskId, CtrlFarmTask *>::iterator itParent =
          m_tasks.find(TaskId(task->m_parentId));
      if (itParent != m_tasks.end()) {
        parentTask = itParent->second;

        TaskState parentTaskState = Completed;
        bool aSubTaskFailed       = false;
        bool noSubtaskRunning     = true;

        if (parentTask->m_status != Suspended &&
            parentTask->m_status != Aborted) {
          std::vector<QString>::iterator itSubTaskId =
              parentTask->m_subTasks.begin();
          for (; itSubTaskId != parentTask->m_subTasks.end(); ++itSubTaskId) {
            QString subTaskId = *itSubTaskId;
            map<TaskId, CtrlFarmTask *>::iterator itSubTask =
                m_tasks.find(TaskId(subTaskId));
            if (itSubTask != m_tasks.end()) {
              CtrlFarmTask *subTask = itSubTask->second;

              if (subTask->m_status == Running ||
                  subTask->m_status == Waiting) {
                parentTaskState  = Running;
                noSubtaskRunning = false;
                break;
              } else if (subTask->m_status == Aborted)
                aSubTaskFailed = true;
            }
          }
        } else
          aSubTaskFailed = true;
        ;  // si arriva se e solo se il task padre e' stato terminato

        if (aSubTaskFailed && noSubtaskRunning)
          parentTask->m_status = Aborted;
        else
          parentTask->m_status = parentTaskState;

        if (parentTask->m_status == Completed ||
            parentTask->m_status == Aborted) {
          parentTask->m_completionDate = task->m_completionDate;
          if (parentTask->m_toBeDeleted) m_tasks.erase(itParent);
        }
      }
    }

    map<QString, FarmServerProxy *>::iterator itServer =
        m_servers.find(task->m_serverId);
    if (itServer != m_servers.end()) server = itServer->second;

    if (server) {
      if (task->m_status == Completed) {
        QString msg = "Task " + taskId + " completed on ";
        msg += server->getHostName();
        msg += "\n\n";
        m_userLog->info(msg);
      } else {
        QString msg = "Task " + taskId + " failed on ";
        msg += server->getHostName();
        msg += " with exit code " + QString::number(exitCode);
        msg += "\n\n";
        m_userLog->info(msg);
      }

      server->removeTask(taskId);
    }

    bool allComplete = false;
    if (parentTask && parentTask->m_status == Completed) {
      m_userLog->info("Task " + parentTask->m_id + " completed\n\n");
      allComplete = true;
    }

    if (task->m_toBeDeleted) delete task;
    if (parentTask && parentTask->m_toBeDeleted) delete parentTask;
    //*
    if (allComplete) {
      activateReadyServers();
      return;
    }
    //*/
  }

  if (server && !server->m_offline && exitCode != RENDER_LICENSE_NOT_FOUND) {
    // cerca un task da sottomettere al server
    CtrlFarmTask *task = getTaskToStart(server);
    if (task) {
      try {
        if (task->m_status == Aborted) {
          vector<QString>::iterator it =
              find(task->m_failedOnServers.begin(),
                   task->m_failedOnServers.end(), server->getId());

          if (it == task->m_failedOnServers.end())
            doRestartTask(task->m_id, false, server);
          else {
            doRestartTask(task->m_id, false, 0);
            CtrlFarmTask *nextTask = getNextTaskToStart(task, server);
            if (nextTask) startTask(nextTask, server);
          }
        } else
          startTask(task, server);
      } catch (TException & /*e*/) {
      }
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::getServers(vector<ServerIdentity> &servers) {
  map<QString, FarmServerProxy *>::iterator it = m_servers.begin();
  for (; it != m_servers.end(); ++it) {
    FarmServerProxy *server = it->second;
    servers.push_back(ServerIdentity(server->m_addr, server->m_hostName));
  }
}

//------------------------------------------------------------------------------

ServerState FarmController::queryServerState2(const QString &id) {
  ServerState state = ServerUnknown;

  map<QString, FarmServerProxy *>::iterator it = m_servers.find(id);
  if (it != m_servers.end()) {
    FarmServerProxy *server = it->second;

    QString taskId;
    state = getServerState(server, taskId);
  }

  return state;
}

//------------------------------------------------------------------------------

void FarmController::queryServerInfo(const QString &id, ServerInfo &info) {
  map<QString, FarmServerProxy *>::iterator it = m_servers.find(id);
  if (it != m_servers.end()) {
    FarmServerProxy *server = it->second;

    info.m_name       = server->getHostName();
    info.m_ipAddress  = server->getIpAddress();
    info.m_portNumber = QString::number(server->getPort());

    info.m_state = getServerState(server, info.m_currentTaskId);
    if (info.m_state != Down && info.m_state != ServerUnknown) {
      TFarmServer::HwInfo hwInfo;
      server->queryHwInfo(hwInfo);

      info.m_cpuCount     = hwInfo.m_cpuCount;
      info.m_totPhysMem   = hwInfo.m_totPhysMem;
      info.m_totVirtMem   = hwInfo.m_totVirtMem;
      info.m_availPhysMem = hwInfo.m_availPhysMem;
      info.m_availVirtMem = hwInfo.m_availVirtMem;
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::activateServer(const QString &id) {
  map<QString, FarmServerProxy *>::iterator it = m_servers.find(id);
  if (it != m_servers.end()) {
    FarmServerProxy *server = it->second;
    server->m_offline       = false;

    for (int i = 0; i < server->m_maxTaskCount; ++i) {
      // cerca un task da sottomettere al server
      CtrlFarmTask *task = getTaskToStart(server);
      if (task) {
        try {
          if (task->m_status == Aborted) {
            vector<QString>::iterator it =
                find(task->m_failedOnServers.begin(),
                     task->m_failedOnServers.end(), server->getId());

            if (it == task->m_failedOnServers.end())
              doRestartTask(task->m_id, false, server);
            else {
              doRestartTask(task->m_id, false, 0);
              CtrlFarmTask *nextTask = getNextTaskToStart(task, server);
              if (nextTask) startTask(nextTask, server);
            }
          } else
            startTask(task, server);
        } catch (TException & /*e*/) {
        }
      }
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::deactivateServer(const QString &id,
                                      bool completeRunningTasks) {
  map<QString, FarmServerProxy *>::iterator it = m_servers.find(id);
  if (it != m_servers.end()) {
    FarmServerProxy *server = it->second;

    QString taskId;
    ServerState state = getServerState(server, taskId);
    if (state == Busy) {
      const vector<QString> &tasks       = server->getTasks();
      vector<QString>::const_iterator it = tasks.begin();
      for (; it != tasks.end(); ++it) {
        server->terminateTask(*it);
      }

      server->m_offline = true;
    } else
      server->m_offline = true;
  }
}

//------------------------------------------------------------------------------

void FarmController::load(const TFilePath &fp) {
  TIStream is(fp);

  m_tasks.clear();

  string tagName;
  is.openChild(tagName);

  if (tagName == "tfarmdata") {
    is.openChild(tagName);
    if (tagName == "tfarmtasks") {
      while (!is.eos()) {
        TPersist *p;
        is >> p;

        CtrlFarmTask *task = dynamic_cast<CtrlFarmTask *>(p);
        if (task) m_tasks.insert(make_pair(TaskId(task->m_id), task));
      }

      is.closeChild();
    }
  }

  is.closeChild();

  map<TaskId, CtrlFarmTask *>::const_iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    CtrlFarmTask *task = it->second;
    if (task->m_parentId != "") {
      map<TaskId, CtrlFarmTask *>::const_iterator it2 =
          m_tasks.find(TaskId(task->m_parentId));

      if (it2 != m_tasks.end()) {
        CtrlFarmTask *parent = it2->second;
        parent->m_subTasks.push_back(task->m_id);
      }
    }
  }
}

//------------------------------------------------------------------------------

void FarmController::save(const TFilePath &fp) const {
  TOStream os(fp);

  map<std::string, string> attributes;
  attributes.insert(make_pair("ver", "1.0"));
  os.openChild("tfarmdata", attributes);
  os.openChild("tfarmtasks");

  map<TaskId, CtrlFarmTask *>::const_iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    CtrlFarmTask *task = it->second;
    os << task;
  }

  os.closeChild();
  os.closeChild();
}

//------------------------------------------------------------------------------

void FarmController::activateReadyServers() {
  QMutexLocker sl(&m_mutex);

  map<QString, FarmServerProxy *>::iterator it = m_servers.begin();
  for (; it != m_servers.end(); ++it) {
    FarmServerProxy *server = it->second;

    ServerState state = queryServerState2(server->getId());
    int tasksCount    = server->m_tasks.size();
    if (state == Ready ||
        state == Busy && tasksCount < server->m_maxTaskCount) {
      for (int i = 0; i < (server->m_maxTaskCount - tasksCount); ++i) {
        // cerca un task da sottomettere al server
        CtrlFarmTask *task = getTaskToStart(server);
        if (task) {
          try {
            if (task->m_status == Aborted) {
              vector<QString>::iterator it =
                  find(task->m_failedOnServers.begin(),
                       task->m_failedOnServers.end(), server->getId());

              if (it == task->m_failedOnServers.end())
                doRestartTask(task->m_id, false, server);
              else {
                doRestartTask(task->m_id, false, 0);
                CtrlFarmTask *nextTask = getNextTaskToStart(task, server);
                if (nextTask) startTask(nextTask, server);
              }
            } else
              startTask(task, server);
          } catch (TException & /*e*/) {
          }
        }
      }
    }
  }
}

//==============================================================================

class ControllerService final : public TService {
public:
  ControllerService()
      : TService("ToonzFarmController", "ToonzFarm Controller")
      , m_controller(0) {}

  ~ControllerService() { delete m_controller; }

  void onStart(int argc, char *argv[]) override;
  void onStop() override;

  static TFilePath getTasksDataFile() {
    TFilePath fp = getGlobalRoot() + "data" + "tasks.txt";
    return fp;
  }

  FarmController *m_controller;
  TUserLog *m_userLog;
};

//------------------------------------------------------------------------------

void ControllerService::onStart(int argc, char *argv[]) {
  // Initialize thread components
  TThread::init();
  TVER::ToonzVersion tver;

  if (isRunningAsConsoleApp()) {
    // i messaggi verranno ridiretti sullo standard output
    m_userLog = new TUserLog();
  } else {
    TFilePath lRootDir  = getLocalRoot();
    bool lRootDirExists = dirExists(lRootDir);
    if (!lRootDirExists) {
      QString errMsg("Unable to start the Controller");
      errMsg += "\n";
      errMsg += "The directory " + lRootDir.getQString() +
                " specified as Local Root does not exist";
      errMsg += "\n";

      addToMessageLog(errMsg);

      // exit the program
      setStatus(TService::Stopped, NO_ERROR, 0);
    }

    TFilePath logFilePath = lRootDir + "controller.log";
    m_userLog             = new TUserLog(logFilePath);
  }
std:
  string appverinfo = tver.getAppVersionInfo("Farm Controller") + "\n\n";
  m_userLog->info(appverinfo.c_str());

  TFilePath globalRoot = getGlobalRoot();
  if (globalRoot.isEmpty()) {
    QString errMsg("Unable to get FARMROOT environment variable (" +
                   globalRoot.getQString() + ")\n");
    addToMessageLog(errMsg);

    // exit the program
    setStatus(TService::Stopped, NO_ERROR, 0);
  }

  bool globalRootExists = true;

  TFileStatus fs(globalRoot);
  if (!fs.isDirectory()) globalRootExists = false;

  if (!globalRootExists) {
    QString errMsg("The directory " + globalRoot.getQString() +
                   " specified as TFARMGLOBALROOT does not exist\n");
    addToMessageLog(errMsg);

    // exit the program
    setStatus(TService::Stopped, NO_ERROR, 0);
  }

  int port = 8000;
  QString hostName, addr;
  bool ret = loadControllerData(hostName, addr, port);
  if (!ret) {
    QString msg("Unable to get the port number of ");
    msg += TSystem::getHostName();
    msg += " from the Controller config file";
    msg += "\n";
    msg += "Using the default port number ";
    msg += QString::number(port);
    msg += "\n";
    m_userLog->info(msg);
  }

#ifdef __sgi
  {
    std::ofstream os("/tmp/.tfarmcontroller.dat");
    os << port;
  }
#endif

  m_controller = new FarmController(hostName, addr, port, m_userLog);

  // configurazione e inizializzazione dei server lato client
  // (il controller e' un client dei ToonzFarm server)
  m_controller->loadServersData(globalRoot);

  TFilePath fp = getTasksDataFile();
  if (myDoesExists(fp)) m_controller->load(fp);

  // si avvia uno dei task caricati da disco
  CtrlFarmTask *task = m_controller->getTaskToStart();
  if (task) {
    TThread::Executor executor;
    executor.addTask(new TaskStarter(m_controller, task));
  }

  QString msg("Starting Controller on port ");
  msg += QString::number(m_controller->m_port);
  msg += "\n\n";
  m_userLog->info(msg);

  // std::cout << msg;

  QEventLoop eventLoop;

  // Connect the server's listening finished signal to main loop quit.
  QObject::connect(m_controller, SIGNAL(finished()), &eventLoop, SLOT(quit()));

  // Start the TcpIp server's listening thread
  m_controller->start();

  // Enter main event loop
  eventLoop.exec();

  //----------------------Farm controller loops here------------------------

  msg = "Controller exited with exit code ";
  msg += QString::number(m_controller->getExitCode());
  msg += "\n";
  m_userLog->info(msg);

// std::cout << msg;

#ifdef __sgi
  { remove("/tmp/.tfarmcontroller.dat"); }
#endif
}

//------------------------------------------------------------------------------

void ControllerService::onStop() {
  // TFilePath fp = getTasksDataFile();
  // m_controller->save(fp);

  TFilePath rootDir            = getGlobalRoot();
  TFilePath lastUsedIdFilePath = rootDir + "config" + "id.txt";
  Tofstream os(lastUsedIdFilePath);
  if (os.good()) os << FarmController::NextTaskId;

  TTcpIpClient client;

  int socketId;
  int ret = client.connect(TSystem::getHostName(), "", m_controller->getPort(),
                           socketId);
  if (ret == OK) {
    client.send(socketId, "shutdown");
  }
}

//==============================================================================
//==============================================================================
//==============================================================================

int main(int argc, char **argv) {
  QCoreApplication a(argc, argv);

  // LO METTO A TRUE COSI' VEDO QUELLO CHE SCRIVE !!
  // RIMETTERLO A FALSE ALTRIMENTI NON PARTE COME SERVIZIO SU WIN
  bool console = false;

  if (argc > 1) {
    string serviceName(
        "ToonzFarmController");  // Must be the same of the installer's
    string serviceDisplayName = serviceName;

    TCli::SimpleQualifier consoleQualifier("-console", "Run as console app");
    TCli::StringQualifier installQualifier("-install name",
                                           "Install service as 'name'");
    TCli::SimpleQualifier removeQualifier("-remove", "Remove service");

    TCli::Usage usage(argv[0]);
    usage.add(consoleQualifier + installQualifier + removeQualifier);
    if (!usage.parse(argc, argv)) exit(1);

#ifdef _WIN32
    if (installQualifier.isSelected()) {
      char szPath[512];

      if (installQualifier.getValue() != "")
        serviceDisplayName = installQualifier.getValue();

      if (GetModuleFileName(NULL, szPath, 512) == 0) {
        std::cout << "Unable to install";
        std::cout << serviceName << " - ";
        std::cout << getLastErrorText().c_str() << std::endl << std::endl;

        return 0;
      }

      TService::install(serviceName, serviceDisplayName, TFilePath(szPath));

      return 0;
    }

    if (removeQualifier.isSelected()) {
      TService::remove(serviceName);
      return 0;
    }
#endif

    if (consoleQualifier.isSelected()) console = true;
  }

  TSystem::hasMainLoop(false);
  TService::instance()->run(argc, argv, console);

  return 0;
}

ControllerService Service;
