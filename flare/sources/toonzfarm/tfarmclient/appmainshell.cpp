

#include "appmainshell.h"
#include "application.h"
#include "tfarmcontroller.h"
#include "tw/colors.h"
#include "tw/menubar.h"
#include "tw/event.h"
#include "tw/action.h"

#include "traster.h"
#include "tsystem.h"
#include "tenv.h"

#include "tgrid.h"
#include "tw/tabbedwindow.h"

#include "taskstatuspage.h"
#include "serverstatuspage.h"
#include "casmsubmitpage.h"
#include "submitpage.h"

#include "filebrowserpopup.h"

#include "tw/message.h"

using namespace TwConsts;

//========================================================================

class MyTabbedWindow : public TTabbedWindow {
public:
  MyTabbedWindow(TWidget *parent) : TTabbedWindow(parent) {}

  void leftButtonDown(const TMouseEvent &e) {
    TabPage *page1 = dynamic_cast<TabPage *>(getCurrentPanel());
    TTabbedWindow::leftButtonDown(e);
    TabPage *page2 = dynamic_cast<TabPage *>(getCurrentPanel());

    if (page1 != page2)
      if (page1) page1->onDeactivate();
    if (page2) page2->onActivate();
  }
};

//==============================================================================

class AppMainshell::Data {
public:
  Data()
      : m_tabbedWindow(0)
      , m_menubar(0)
      , m_updatePeriod(3000)
      , m_retryCount(0)
      , m_retryPeriod(15000) {}

  MyTabbedWindow *m_tabbedWindow;
  TMenubar *m_menubar;

  int m_updatePeriod;
  int m_retryCount;
  int m_retryPeriod;
};

//========================================================================

namespace {

//========================================================================

TEnv::RootSystemVar systemVar(
    TFilePath("SOFTWARE\\Digital Video\\ToonzFarm\\1.0\\LOCALROOT"));

inline void errorMessage(const string &msg) { TMessage::error(msg); }

const int menuBarLy = 21;

}  // anonymous namespace

//========================================================================

AppMainshell::AppMainshell() : TMainshell("mainshell") {
  m_name = "TFarmClient, v1.0 alfa - " + TSystem::getUserName() + "@" +
           TSystem::getHostName();

  m_data                 = new Data;
  m_data->m_tabbedWindow = new MyTabbedWindow(this);
  m_data->m_tabbedWindow->addPanel(new TaskStatusPage(m_data->m_tabbedWindow));
  m_data->m_tabbedWindow->addPanel(
      new ServerStatusPage(m_data->m_tabbedWindow));
  // m_data->m_tabbedWindow->addPanel(new
  // CasmSubmitPage(m_data->m_tabbedWindow));
  m_data->m_tabbedWindow->addPanel(new SubmitPage(m_data->m_tabbedWindow));

  m_data->m_menubar = new TMenubar(this);

  TMenubarItem *menu = new TMenubarItem(m_data->m_menubar, "File");
  menu->addItem("Quit");
  TGuiCommand("Quit").setAction(new TCommandAction<AppMainshell>(this, close));

  /*
menu = new TMenubarItem(m_data->m_menubar, "Tasks");

menu->addItem("Casm");
TGuiCommand("Casm").setAction(new TCommandAction<AppMainshell>(this,
onTaskCasm));

menu->addItem("Notepad");
TGuiCommand("Notepad").setAction(new TCommandAction<AppMainshell>(this,
onTaskNotepad));
*/
}

//-------------------------------------------------------------------

AppMainshell::~AppMainshell() {}

//-------------------------------------------------------------------

HANDLE HFile;

void AppMainshell::init() {
  TSystem::loadStandardPlugins();

  try {
    Application::instance()->init();
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
    return;
  }
}

//-------------------------------------------------------------------

bool AppMainshell::beforeShow() {
  if (!Application::instance()->testControllerConnection()) {
    string hostName, addr;
    int port;
    Application::instance()->getControllerData(hostName, addr, port);

    string msg("Unable to connect to the ToonzFarm Controller\n");
    msg += "The Controller should run on " + hostName + " at port ";
    msg += toString(port) + "\n";
    msg += "Please start the Controller before running this application";

    TMessage::error(msg);
    return false;
  }

  TWidget *w    = m_data->m_tabbedWindow->getCurrentPanel();
  TabPage *page = dynamic_cast<TabPage *>(w);
  if (page) page->onActivate();

  startTimer(m_data->m_updatePeriod);
  return true;
}

//-------------------------------------------------------------------

void AppMainshell::configureNotify(const TDimension &size) {
  TMainshell::configureNotify(size);

  m_data->m_tabbedWindow->setGeometry(0, 0, size.lx - 1,
                                      size.ly - menuBarLy - 1);

  m_data->m_menubar->setGeometry(0, size.ly - menuBarLy, size.lx - 1,
                                 size.ly - 1);
  m_data->m_menubar->invalidate();
}

//-------------------------------------------------------------------

void AppMainshell::close() {
  if (TMessage::question("Are you sure?") == TMessage::YES) {
    TMainshell::close();
  }
}

//-------------------------------------------------------------------

void AppMainshell::repaint() {
  TDimension size = getSize();

  int y = size.ly - 1;
  int x = 20;
  setColor(White);
  fillRect(x, 0, size.lx - 1, y);

  setColor(Gray60);
  fillRect(0, 0, x - 1, y);

  y -= 21;
  drawLine(x, y, x, y - 3);
  drawLine(x + 1, y, x + 1, y - 1);
  drawLine(x + 2, y, x + 3, y);
}

//-------------------------------------------------------------------

TDimension AppMainshell::getPreferredSize() { return TDimension(1200, 500); }

//-------------------------------------------------------------------

int AppMainshell::getMainIconId() { return 101 /*IDI_ZCOMP*/; }

//-------------------------------------------------------------------

void AppMainshell::onTimer(int) {
  if (false && !Application::instance()->testControllerConnection()) {
    if (m_data->m_retryCount == 0) {
      stopTimer();
      startTimer(m_data->m_retryPeriod);
    }

    ++m_data->m_retryCount;

    if (m_data->m_retryCount == 3) {
      stopTimer();
      startTimer(4 * m_data->m_retryPeriod);

      string hostName, addr;
      int port;
      Application::instance()->getControllerData(hostName, addr, port);

      string msg("The connection to the ToonzFarm Controller has been lost\n");
      msg += "The Controller should run on " + hostName + " at port ";
      msg += toString(port) + "\n";
      msg += "Please check the Controller state";

      TMessage::error(msg);
    }
  } else {
    if (m_data->m_retryCount > 0) {
      m_data->m_retryCount = 0;
      string msg("Reconnected to the ToonzFarm Controller\n");
      TMessage::error(msg);

      stopTimer();
      startTimer(m_data->m_updatePeriod);
    }

    TWidget *w    = m_data->m_tabbedWindow->getCurrentPanel();
    TabPage *page = dynamic_cast<TabPage *>(w);
    if (page) page->update();
  }
}

//-------------------------------------------------------------------
//-------------------------------------------------------------------

AppMainshell mainshell;
