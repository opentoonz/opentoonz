

#include "tfarmserver.h"
#include "tfarmexecutor.h"
#include "tsystem.h"

#include "tconvert.h"

#include <vector>
using namespace std;

//------------------------------------------------------------------------------

class TFarmServerStub::Imp final : public TFarmExecutor {
public:
  Imp(TFarmServer *server, int port) : TFarmExecutor(port), m_server(server) {
    assert(m_server);
  }

  // TFarmExecutor overrides
  QString execute(const std::vector<QString> &argv) override;

  TFarmServer *m_server;
};

//------------------------------------------------------------------------------

QString TFarmServerStub::Imp::execute(const std::vector<QString> &argv) {
#ifdef DEBUG
  std::cout << "executing " << argv[0].c_str() << endl;
#endif

  if (argv.size() > 0) {
#ifdef DEBUG
    for (int i = 0; i < (int)argv.size(); ++i) std::cout << argv[i] << " ";
#endif

    if (argv[0] == "addTask" && argv.size() == 3) {
      int ret = m_server->addTask(argv[1], argv[2]);
      return QString::number(ret);
    } else if (argv[0] == "terminateTask" && argv.size() > 1) {
      int ret = m_server->terminateTask(argv[1]);
      return QString::number(ret);
    } else if (argv[0] == "getTasks") {
      std::vector<QString> tasks;
      int ret = m_server->getTasks(tasks);

      QString reply(QString::number(ret));
      reply += ",";

      std::vector<QString>::iterator it = tasks.begin();
      for (; it != tasks.end(); ++it) {
        reply += *it;
        reply += ",";
      }

      if (!reply.isEmpty()) reply.remove(reply.length() - 1, 1);

      return reply;
    } else if (argv[0] == "queryHwInfo") {
      TFarmServer::HwInfo hwInfo;
      m_server->queryHwInfo(hwInfo);

      QString ret;
      ret += QString::number((int)hwInfo.m_cpuCount);
      ret += ",";
      ret += QString::number((int)(hwInfo.m_totPhysMem / 1024));
      ret += ",";
      ret += QString::number((int)(hwInfo.m_availPhysMem / 1024));
      ret += ",";
      ret += QString::number((int)(hwInfo.m_totVirtMem / 1024));
      ret += ",";
      ret += QString::number((int)(hwInfo.m_availVirtMem / 1024));
      ret += ",";
      ret += QString::number(hwInfo.m_type);
      return ret;
    } else if (argv[0] == "attachController" && argv.size() > 3) {
      int port = argv[3].toInt();
      m_server->attachController(argv[1], argv[2], port);
      return "";
    } else if (argv[0] == "detachController" && argv.size() > 3) {
      int port = argv[3].toInt();
      m_server->detachController(argv[1], argv[2], port);
      return "";
    }
  }

  return "";
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

TFarmServerStub::TFarmServerStub(TFarmServer *farmServer, int port)
    : m_imp(new Imp(farmServer, port)) {}

//------------------------------------------------------------------------------

TFarmServerStub::~TFarmServerStub() {}

//------------------------------------------------------------------------------

int TFarmServerStub::run() {
  m_imp->run();
  return m_imp->getExitCode();
}

//------------------------------------------------------------------------------

int TFarmServerStub::shutdown() {
  TTcpIpClient client;

  int socketId;
  int ret =
      client.connect(TSystem::getHostName(), "", m_imp->getPort(), socketId);
  if (ret == OK) {
    ret = client.send(socketId, "shutdown");
  }

  return ret;
}

//------------------------------------------------------------------------------

int TFarmServerStub::getPort() const { return m_imp->getPort(); }
