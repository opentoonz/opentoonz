

#include "tfarmserver.h"
//#include "ttcpip.h"
#include "tfarmproxy.h"

#include "tconvert.h"
namespace {

//------------------------------------------------------------------------------

class FarmServerProxy final : public TFarmServer, public TFarmProxy {
public:
  FarmServerProxy(const QString &hostName, const QString &addr, int port)
      : TFarmProxy(hostName, addr, port) {}

  // TFarmServer interface implementation
  int addTask(const QString &taskid, const QString &cmdline) override;
  int terminateTask(const QString &taskid) override;
  int getTasks(std::vector<QString> &tasks) override;

  void queryHwInfo(HwInfo &hwInfo) override;

  void attachController(const QString &name, const QString &addr,
                        int port) override;
  void detachController(const QString &name, const QString &addr,
                        int port) override;
};

//------------------------------------------------------------------------------

int FarmServerProxy::addTask(const QString &taskid, const QString &cmdline) {
  QString data("addTask");
  data += ",";
  data += taskid;
  data += ",";
  data += cmdline;

  QString reply = sendToStub(data);
  if (reply.isEmpty()) return -1;

  int rc = reply.toInt();
  return rc;
}

//------------------------------------------------------------------------------

int FarmServerProxy::terminateTask(const QString &taskid) {
  QString data("terminateTask");
  data += ",";
  data += taskid;
  QString reply = sendToStub(data);

  return 0;
}
//------------------------------------------------------------------------------

int FarmServerProxy::getTasks(std::vector<QString> &tasks) {
  QString data("getTasks");
  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  int count = extractArgs(reply, argv);

  assert(count > 0);
  int taskCount = argv[0].toInt();

  tasks.clear();
  std::vector<QString>::iterator it = argv.begin();
  std::advance(it, 1);
  for (; it != argv.end(); ++it) tasks.push_back(*it);

  return taskCount;
}

//------------------------------------------------------------------------------

void FarmServerProxy::queryHwInfo(HwInfo &hwInfo) {
  QString data("queryHwInfo");
  QString reply = sendToStub(data);

  // la stringa restituita contiene le informazioni desiderate separate da ","
  std::vector<QString> argv;
  extractArgs(reply, argv);

  assert(argv.size() > 4);

  int cpuCount, totPhysMem, totVirtMem, availPhysMem, availVirtMem;

  cpuCount     = argv[0].toInt();
  totPhysMem   = argv[1].toInt();
  availPhysMem = argv[2].toInt();
  totVirtMem   = argv[3].toInt();
  availVirtMem = argv[4].toInt();

  hwInfo.m_cpuCount     = cpuCount;
  hwInfo.m_totPhysMem   = totPhysMem;
  hwInfo.m_totVirtMem   = totVirtMem;
  hwInfo.m_availPhysMem = availPhysMem;
  hwInfo.m_availVirtMem = availVirtMem;

  if (argv.size() > 5) hwInfo.m_type = (TFarmPlatform)argv[5].toInt();
}

//------------------------------------------------------------------------------

void FarmServerProxy::attachController(const QString &name, const QString &addr,
                                       int port) {
  QString data("attachController");
  data += ",";
  data += name;
  data += ",";
  data += addr;
  data += ",";
  data += QString::number(port);

  sendToStub(data);
}

//------------------------------------------------------------------------------

void FarmServerProxy::detachController(const QString &name, const QString &addr,
                                       int port) {
  QString data("detachController");
  data += ",";
  data += name;
  data += ",";
  data += addr;
  data += ",";
  data += QString::number(port);

  QString reply = sendToStub(data);
}

}  // anonymous namespace

//==============================================================================

TFarmServerFactory::TFarmServerFactory() {}

//------------------------------------------------------------------------------

TFarmServerFactory::~TFarmServerFactory() {}

//------------------------------------------------------------------------------

int TFarmServerFactory::create(const QString &hostName, const QString &addr,
                               int port, TFarmServer **tfserver) {
  *tfserver = new FarmServerProxy(hostName, addr, port);
  return 0;
}
