

#include "tfarmstuff.h"
#include "tsystem.h"
#include "ttcpip.h"
#include "tfarmcontroller.h"
#include "tenv.h"

namespace {

//------------------------------------------------------------------------------

ControllerData CtrlData;
TFarmController *FarmController = 0;

}  // anonymous namespace

//------------------------------------------------------------------------------

TFarmController *TFarmStuff::getTFarmController() {
  if (!FarmController) {
    TFilePath groot = getGlobalRoot();
    TFilePath fp    = groot + "config" + "controller.txt";

    ::loadControllerData(fp, CtrlData);

    TFarmControllerFactory factory;
    // int ret =
    factory.create(CtrlData, &FarmController);
  }

  return FarmController;
}

//------------------------------------------------------------------------------

namespace {

inline bool doTestConnection(const QString &hostname, const QString &ipaddr,
                             int port) {
  int sock;
  TTcpIpClient client;

  int ret = client.connect(hostname, ipaddr, port, sock);
  if (ret == OK) {
    client.disconnect(sock);
    return true;
  }
  return false;
}
}

//------------------------------------------------------------------------------

bool TFarmStuff::testConnection(const QString &hostname, int port) {
  return doTestConnection(hostname, "", port);
}

//------------------------------------------------------------------------------

bool TFarmStuff::testConnectionToController() {
  QString hostName;
  QString ipAddr;
  int port;

  getControllerData(hostName, ipAddr, port);
  return doTestConnection(hostName, ipAddr, port);
}

//------------------------------------------------------------------------------

void TFarmStuff::getControllerData(QString &hostName, QString &ipAddr,
                                   int &port) {
  ControllerData data;

  TFilePath groot   = getGlobalRoot();
  QString grootpath = QString::fromStdWString(groot.getWideString());

  if (groot.isEmpty() || grootpath == " ")
    throw TFarmStuff::TMissingGRootEnvironmentVariable();

  bool fileExists = false;
  TFileStatus fs(groot);
  fileExists = fs.doesExist();
  if (!fileExists) throw TFarmStuff::TMissingGRootFolder();

  TFilePath fp = groot + "config" + "controller.txt";
  ::loadControllerData(fp, data);

  hostName = data.m_hostName;
  ipAddr   = data.m_ipAddress;
  port     = data.m_port;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

TEnv::StringVar GRootEnvVar("TFarmGlobalRoot", "");
// TEnv::FilePathVar GRootEnvVar("TFarmGlobalRoot" , TFilePath());

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

TFilePath TFarmStuff::getGlobalRoot() {
  TFilePath groot = TFilePath(GRootEnvVar);
  return groot;
}

//------------------------------------------------------------------------------

void TFarmStuff::setGlobalRoot(const TFilePath &fp) {
  GRootEnvVar = ::to_string(fp);
  // GRootEnvVar = fp;
}
