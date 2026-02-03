

#include "application.h"
#include "tfarmcontroller.h"
#include "ttcpip.h"
#include "tfilepath.h"
#include "tsystem.h"
#include "tthread.h"
#include "tenv.h"
#include "tconvert.h"
#include "tfilepath_io.h"
#include <vector>
#include <set>
#include <assert.h>
#include <fstream>
using namespace std;

//===================================================================

namespace {

TFilePath getGlobalRoot() {
#ifdef WIN32
  TFilePath groot(TSystem::getSystemValue(
      TFilePath("SOFTWARE\\Digital Video\\ToonzFarm\\1.0\\GLOBALROOT")));

  return groot;
#else
  TFilePath name    = "TFARMGLOBALROOT";
  char *s           = getenv(name.getFullPath().c_str());
  TFilePath rootDir = string(s ? s : "");
  return rootDir;
#endif
}
};

class Application::Imp {
public:
  Imp() : m_farmController(0) {}
  ~Imp() {}

  void loadControllerData();

  TFarmController *m_farmController;
  TFilePath m_currentFolder;

  ControllerData m_controllerData;
};

//------------------------------------------------------------------------------

void Application::Imp::loadControllerData() {
  TFilePath groot = getGlobalRoot();
  TFilePath fp    = groot + "config" + "controller.txt";

  ::loadControllerData(fp, m_controllerData);
}

//===================================================================

namespace {

//===================================================================

Application *theApp;
bool programEnded = false;

class Cleanup {
public:
  ~Cleanup() {
    delete theApp;
    theApp       = 0;
    programEnded = true;
  }
} cleanup;

//===================================================================

}  // end of anonymous namespace

//===================================================================

Application::Application() : m_imp(new Imp) {}

//------------------------------------------------------------------------------

Application::~Application() { delete m_imp; }

//------------------------------------------------------------------------------

Application *Application::instance() {
  assert(!programEnded);
  if (!theApp) {
    static TThread::Mutex AppMutex;
    TThread::ScopedLock sl(AppMutex);
    if (programEnded) return 0;

    if (!theApp) theApp = new Application;
  }
  return theApp;
}

//------------------------------------------------------------------------------

TFarmController *Application::getController() {
  return m_imp->m_farmController;
}

//------------------------------------------------------------------------------

bool Application::testControllerConnection() const {
  TTcpIpClient client;

  int sock;
  int ret = client.connect(m_imp->m_controllerData.m_hostName,
                           m_imp->m_controllerData.m_ipAddress,
                           m_imp->m_controllerData.m_port, sock);
  if (ret == OK) {
    client.disconnect(sock);
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------

void Application::getControllerData(string &hostName, string &ipAddr,
                                    int &port) const {
  hostName = m_imp->m_controllerData.m_hostName;
  ipAddr   = m_imp->m_controllerData.m_ipAddress;
  port     = m_imp->m_controllerData.m_port;
}

//------------------------------------------------------------------------------

void Application::init() {
  m_imp->loadControllerData();

  TFarmControllerFactory factory;
  int ret = factory.create(m_imp->m_controllerData, &m_imp->m_farmController);
}

//------------------------------------------------------------------------------

void Application::setCurrentFolder(const TFilePath &fp) {
  m_imp->m_currentFolder = fp;
}

//------------------------------------------------------------------------------

TFilePath Application::getCurrentFolder() { return m_imp->m_currentFolder; }
