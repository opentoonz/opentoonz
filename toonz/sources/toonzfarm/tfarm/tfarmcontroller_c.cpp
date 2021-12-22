

#include "tconvert.h"
#include "tfarmcontroller.h"
#include "ttcpip.h"

#include "texception.h"
#include "tfarmproxy.h"
#include "tfilepath_io.h"

#include <sstream>

namespace {

//------------------------------------------------------------------------------

class Controller final : public TFarmController, public TFarmProxy {
public:
  Controller(const QString &hostName, const QString &addr, int port)
      : TFarmProxy(hostName, addr, port) {}

  // TFarmController interface implementation

  QString addTask(const TFarmTask &task, bool suspended) override;

  void removeTask(const QString &id) override;
  void suspendTask(const QString &id) override;
  void activateTask(const QString &id) override;
  void restartTask(const QString &id) override;

  void getTasks(std::vector<QString> &tasks) override;
  void getTasks(const QString &parentId, std::vector<QString> &tasks) override;
  void getTasks(const QString &parentId,
                std::vector<TaskShortInfo> &tasks) override;

  void queryTaskInfo(const QString &id, TFarmTask &task) override;

  void queryTaskShortInfo(const QString &id, QString &parentId, QString &name,
                          TaskState &status) override;

  void attachServer(const QString &name, const QString &addr,
                    int port) override;

  void detachServer(const QString &name, const QString &addr,
                    int port) override;

  void taskSubmissionError(const QString &taskId, int errCode) override;

  void taskProgress(const QString &taskId, int step, int stepCount,
                    int frameNumber, FrameState state) override;

  void taskCompleted(const QString &taskId, int exitCode) override;

  // fills the servers vector with the names of the servers
  void getServers(std::vector<ServerIdentity> &servers) override;

  // returns the state of the server whose id has been specified
  ServerState queryServerState2(const QString &id) override;

  // fills info with the infoes about the server whose id has been specified
  void queryServerInfo(const QString &id, ServerInfo &info) override;

  // activates the server whose id has been specified
  void activateServer(const QString &id) override;

  // deactivates the server whose id has been specified
  // once deactivated, a server is not available for task rendering
  void deactivateServer(const QString &id,
                        bool completeRunningTasks = true) override;
};

//------------------------------------------------------------------------------

QString Controller::addTask(const TFarmTask &task, bool suspended) {
  int i, count = task.getTaskCount();

  QString data("addTask@TFarmTask_2");
  data += ",";
  data += task.m_name;
  data += ",";
  data += task.getCommandLine(true);
  data += ",";
  data += task.m_user;
  data += ",";
  data += task.m_hostName;
  data += ",";
  data += QString::number(suspended);
  data += ",";
  data += QString::number(task.m_stepCount);
  data += ",";
  data += QString::number(task.m_priority);
  data += ",";
  data += QString::number(task.m_platform);

  data += ",";
  int depCount = task.m_dependencies->getTaskCount();
  data += QString::number(depCount);

  i = 0;
  for (; i < depCount; ++i) {
    data += ",";
    data += task.m_dependencies->getTaskId(i);
  }

  for (i = 0; i < count; ++i) {
    TFarmTask &tt      = const_cast<TFarmTask &>(task);
    TFarmTask *subtask = tt.getTask(i);

    data += ",";
    data += subtask->m_name;
    data += ",";
    data += subtask->getCommandLine(true);
    data += ",";
    data += subtask->m_user;
    data += ",";
    data += subtask->m_hostName;
    data += ",";
    data += QString::number(subtask->m_stepCount);
    data += ",";
    data += QString::number(subtask->m_priority);
  }

  QString reply = sendToStub(data);
  // MessageBox(NULL, data.c_str(), "ciao", 0);
  return reply;
}

//------------------------------------------------------------------------------

void Controller::removeTask(const QString &id) {
  QString data("removeTask");
  data += ",";
  data += id;

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::suspendTask(const QString &id) {
  QString data("suspendTask");
  data += ",";
  data += id;

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::activateTask(const QString &id) {
  QString data("activateTask");
  data += ",";
  data += id;

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::restartTask(const QString &id) {
  QString data("restartTask");
  data += ",";
  data += id;

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::getTasks(std::vector<QString> &tasks) {
  QString data("getTasks@vector");
  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  extractArgs(reply, argv);

  tasks.clear();
  std::vector<QString>::iterator it = argv.begin();
  for (; it != argv.end(); ++it) tasks.push_back(*it);
}

//------------------------------------------------------------------------------

void Controller::getTasks(const QString &parentId,
                          std::vector<QString> &tasks) {
  QString data("getTasks@string@vector");
  data += ",";
  data += parentId;

  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni deiderate separate da ","
  std::vector<QString> argv;
  extractArgs(reply, argv);

  tasks.clear();
  std::vector<QString>::iterator it = argv.begin();
  for (; it != argv.end(); ++it) tasks.push_back(*it);
}

//------------------------------------------------------------------------------

void Controller::getTasks(const QString &parentId,
                          std::vector<TaskShortInfo> &tasks) {
  QString data("getTasks@string@vector$TaskShortInfo");
  data += ",";
  data += parentId;

  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  extractArgs(reply, argv);

  tasks.clear();
  std::vector<QString>::iterator it = argv.begin();
  for (; it != argv.end(); ++it) {
    QString id   = *it++;
    QString name = *it++;
    TaskState status;
    status = (TaskState)(*it).toInt();

    tasks.push_back(TaskShortInfo(id, name, status));
  }
}

//------------------------------------------------------------------------------

void Controller::queryTaskInfo(const QString &id, TFarmTask &task) {
  QString data("queryTaskInfo_2");
  data += ",";
  data += id;

  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  int count = extractArgs(reply, argv);

  if (reply == "") return;

  assert(argv.size() > 15);
  int incr    = 0;
  task.m_name = argv[incr++];
  task.parseCommandLine(argv[incr++]);
  task.m_priority = argv[incr++].toInt();

  task.m_user     = argv[incr++];
  task.m_hostName = argv[incr++];

  task.m_id       = argv[incr++];
  task.m_parentId = argv[incr++];

  task.m_status = (TaskState)argv[incr++].toInt();

  task.m_server         = argv[incr++];
  task.m_submissionDate = QDateTime::fromString(argv[incr++]);
  task.m_startDate      = QDateTime::fromString(argv[incr++]);
  task.m_completionDate = QDateTime::fromString(argv[incr++]);

  task.m_successfullSteps = argv[incr++].toInt();
  task.m_failedSteps      = argv[incr++].toInt();
  task.m_stepCount        = argv[incr++].toInt();

  if (incr < count) {
    task.m_platform = (TFarmPlatform)argv[incr++].toInt();
    int depCount    = 0;
    depCount        = argv[incr++].toInt();
    if (depCount > 0) {
      task.m_dependencies = new TFarmTask::Dependencies;
      for (int i = 0; i < depCount; ++i) task.m_dependencies->add(argv[incr++]);
    }
  }
}

//------------------------------------------------------------------------------

void Controller::queryTaskShortInfo(const QString &id, QString &parentId,
                                    QString &name, TaskState &status) {
  QString data("queryTaskShortInfo");
  data += ",";
  data += id;

  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  extractArgs(reply, argv);

  assert(argv.size() == 3);

  parentId = argv[0];
  name     = argv[1];
  status   = (TaskState)argv[2].toInt();
}

//------------------------------------------------------------------------------

void Controller::attachServer(const QString &name, const QString &addr,
                              int port) {
  QString data("attachServer");
  data += ",";
  data += name;
  data += ",";
  data += addr;
  data += ",";
  data += QString::number(port);

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::detachServer(const QString &name, const QString &addr,
                              int port) {
  QString data("detachServer");
  data += ",";
  data += name;
  data += ",";
  data += addr;
  data += ",";
  data += QString::number(port);

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::taskSubmissionError(const QString &taskId, int errCode) {
  QString data("taskSubmissionError");
  data += ",";
  data += taskId;
  data += ",";
  data += QString::number(errCode);

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::taskProgress(const QString &taskId, int step, int stepCount,
                              int frameNumber, FrameState state) {
  QString data("taskProgress");
  data += ",";
  data += taskId;
  data += ",";
  data += QString::number(step);
  data += ",";
  data += QString::number(stepCount);
  data += ",";
  data += QString::number(frameNumber);
  data += ",";
  data += QString::number(state);

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::taskCompleted(const QString &taskId, int exitCode) {
  QString data("taskCompleted");
  data += ",";
  data += taskId;
  data += ",";
  data += QString::number(exitCode);

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::getServers(std::vector<ServerIdentity> &servers) {
  QString data("getServers");
  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  extractArgs(reply, argv);

  servers.clear();
  std::vector<QString>::iterator it = argv.begin();
  for (; it != argv.end(); it += 2)
    servers.push_back(ServerIdentity(*it, *(it + 1)));
}

//------------------------------------------------------------------------------

ServerState Controller::queryServerState2(const QString &name) {
  QString data("queryServerState2");
  data += ",";
  data += name;

  QString reply = sendToStub(data);

  ServerState serverState = (ServerState)reply.toInt();
  return serverState;
}

//------------------------------------------------------------------------------

void Controller::queryServerInfo(const QString &id, ServerInfo &info) {
  QString data("queryServerInfo");
  data += ",";
  data += id;

  QString reply = sendToStub(data);
  if (reply != "") {
    // la stringa restituita contiene le informazioni desiderate separate da ","
    std::vector<QString> argv;
    extractArgs(reply, argv);

    info.m_name       = argv[0];
    info.m_ipAddress  = argv[1];
    info.m_portNumber = argv[2];
    info.m_state      = (ServerState)argv[3].toInt();
    info.m_platform   = argv[4];

    int cpuCount, totPhysMem, totVirtMem, availPhysMem, availVirtMem;

    cpuCount             = argv[5].toInt();
    totPhysMem           = argv[6].toInt();
    totVirtMem           = argv[7].toInt();
    availPhysMem         = argv[8].toInt();
    availVirtMem         = argv[9].toInt();
    info.m_currentTaskId = argv[10];

    info.m_cpuCount     = cpuCount;
    info.m_totPhysMem   = totPhysMem;
    info.m_totVirtMem   = totVirtMem;
    info.m_availPhysMem = availPhysMem;
    info.m_availVirtMem = availVirtMem;
  }
}

//------------------------------------------------------------------------------

void Controller::activateServer(const QString &id) {
  QString data("activateServer");
  data += ",";
  data += id;

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

void Controller::deactivateServer(const QString &id,
                                  bool completeRunningTasks) {
  QString data("deactivateServer");
  data += ",";
  data += id;
  data += ",";
  data += QString::number(completeRunningTasks);

  QString reply = sendToStub(data);
}

//------------------------------------------------------------------------------

}  // anonymous namespace

//==============================================================================

TFarmControllerFactory::TFarmControllerFactory() {}

//------------------------------------------------------------------------------

int TFarmControllerFactory::create(const ControllerData &data,
                                   TFarmController **controller)

{
  *controller = new Controller(data.m_hostName, data.m_ipAddress, data.m_port);
  return 0;
}

//------------------------------------------------------------------------------

int TFarmControllerFactory::create(const QString &hostname, int port,
                                   TFarmController **controller) {
  *controller = new Controller(hostname, "", port);
  return 0;
}

//------------------------------------------------------------------------------

void loadControllerData(const TFilePath &fp, ControllerData &data) {
  Tifstream is(fp);
  if (!is || !is.good()) {
    throw TException(L"Unable to get Farm Controller Data (looking for '" +
                     fp.getWideString() + L"')");
  }

  while (!is.eof()) {
    char line[1024];
    is.getline(line, 1024);

    if (line[0] != '#' && QString(line) != "") {
      std::stringstream iss(line);

      char hostName[512];
      char ipAddr[80];
      int port;

      iss >> hostName >> ipAddr >> port;

      data.m_hostName  = hostName;
      data.m_ipAddress = ipAddr;
      data.m_port      = port;
      break;
    }
  }
}
