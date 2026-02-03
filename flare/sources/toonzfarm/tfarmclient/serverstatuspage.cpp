

#include "serverstatuspage.h"
#include "tfarmcontroller.h"
#include "application.h"
#include "textlist.h"
#include "tw/mainshell.h"
#include "tw/textfield.h"
#include "tw/event.h"

#include "tw/message.h"

#include <vector>
#include <set>
using namespace std;

//==============================================================================

class ServerList : public TTextList {
public:
  ServerList(TWidget *parent);

  void rightButtonDown(const TMouseEvent &e);

  void onDeactivate();
  void onActivate();

  TPopupMenuItem *m_activationItem;
  TPopupMenu *m_popupMenu;
};

//------------------------------------------------------------------------------

ServerList::ServerList(TWidget *parent) : TTextList(parent, "servers") {
  m_popupMenu = new TPopupMenu(this);

  TPopupMenuItem *item = new TPopupMenuItem(m_popupMenu, "deactivate");
  TGuiCommand("deactivate")
      .setAction(new TCommandAction<ServerList>(this, onDeactivate));
  TGuiCommand("deactivate").add(item);

  item = new TPopupMenuItem(m_popupMenu, "activate");
  TGuiCommand("activate")
      .setAction(new TCommandAction<ServerList>(this, onActivate));
  TGuiCommand("activate").add(item);
}

//------------------------------------------------------------------------------

void ServerList::rightButtonDown(const TMouseEvent &e) {
  leftButtonDown(e);
  TTextListItem *item = getSelectedItem(0);
  if (item) {
    TFarmController *controller = Application::instance()->getController();
    ServerState state;
    try {
      state = controller->queryServerState2(item->getId());
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
      return;
    }

    if (state != Offline) {
      TGuiCommand("activate").disable();
      TGuiCommand("deactivate").enable();
    } else {
      TGuiCommand("activate").enable();
      TGuiCommand("deactivate").disable();
    }

    TPoint pos =
        getAbsolutePosition() + TPoint(e.m_pos.x, getSize().ly - e.m_pos.y);
    m_popupMenu->popup(pos);
  }
}

//------------------------------------------------------------------------------

void ServerList::onDeactivate() {
  TTextListItem *item = getSelectedItem(0);
  if (item) {
    TFarmController *controller = Application::instance()->getController();
    try {
      controller->deactivateServer(item->getId());
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }
}

//------------------------------------------------------------------------------

void ServerList::onActivate() {
  TTextListItem *item = getSelectedItem(0);
  if (item) {
    TFarmController *controller = Application::instance()->getController();
    try {
      controller->activateServer(item->getId());
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }
}

//==============================================================================

class ServerStatusPage::Data {
public:
  Data(ServerStatusPage *parent) {
    m_serverList = new ServerList(parent);
    m_serverList->setSelAction(
        new TTextListAction<Data>(this, &Data::onServerSelection));
    m_name       = new TTextField(parent, "name");
    m_ipAddress  = new TTextField(parent, "ipAddress");
    m_portNumber = new TTextField(parent, "portNumber");
    m_tasks      = new TTextField(parent, "tasks");
    m_state      = new TTextField(parent, "state");
    m_cpuCount   = new TTextField(parent, "cpuCount");
    m_totPhysMem = new TTextField(parent, "totalPhysMemory");

    m_nameLbl       = new TLabel(parent, "name");
    m_ipAddressLbl  = new TLabel(parent, "ipAddress");
    m_portNumberLbl = new TLabel(parent, "portNumber");
    m_tasksLbl      = new TLabel(parent, "tasks");
    m_stateLbl      = new TLabel(parent, "state");
    m_cpuCountLbl   = new TLabel(parent, "cpuCount");
    m_totPhysMemLbl = new TLabel(parent, "totalPhysMemory");
  }
  ~Data() {}

  void onServerSelection(int index) {
    TTextListItem *item = m_serverList->getSelectedItem(0);
    if (item) {
      showServerInfo(item->getId());
    }
  }

  void showServerInfo(const string &id) {
    TFarmController *controller = Application::instance()->getController();
    ServerInfo info;

    try {
      controller->queryServerInfo(id, info);
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }

    switch (info.m_state) {
    case Ready:
      m_state->setText("Ready");
      break;
    case Busy:
      m_state->setText("Busy");
      break;
    case NotResponding:
      m_state->setText("Not Responding");
      break;
    case Down:
      m_state->setText("Down");
      break;
    case Offline:
      m_state->setText("Offline");
      break;
    case ServerUnknown:
      m_state->setText("");
      m_name->setText("");
      m_ipAddress->setText("");
      m_portNumber->setText("");
      m_tasks->setText("");
      m_cpuCount->setText("");
      m_totPhysMem->setText("");
      return;
    }

    m_name->setText(info.m_name);
    m_ipAddress->setText(info.m_ipAddress);
    m_portNumber->setText(info.m_portNumber);
    if (info.m_currentTaskId == "")
      m_tasks->setText("");
    else {
      TFarmTask task;
      try {
        controller->queryTaskInfo(info.m_currentTaskId, task);
        m_tasks->setText("<" + task.m_id + "> " + task.m_name);
      } catch (TException &e) {
        m_tasks->setText("");
        TMessage::error(toString(e.getMessage()));
      }
    }

    if (info.m_state != Down) {
      m_cpuCount->setText(toString(info.m_cpuCount));
      m_totPhysMem->setText(toString((long)info.m_totPhysMem));
    } else {
      m_cpuCount->setText("");
      m_totPhysMem->setText("");
    }
  }

  void addServer(const ServerIdentity &sid) {
    m_serverList->addItem(new TTextListItem(sid.m_id, sid.m_name));
  }

  void configureNotify(const TDimension &size) {
    const int dx       = 10;
    const int dy       = 5;
    const int h        = 20;
    const int lw       = 120;
    const int leftSize = tmin(250, size.lx / 3);

    const int w = size.lx - leftSize - lw - dx * 3;
    int left    = leftSize + dx;
    int y0      = size.ly - 30;
    int x0;

    // prima la parte a sx
    m_serverList->setGeometry(0, 0, leftSize /*100*/, size.ly - 1);

    // ora la parte a dx
    x0 = left;
    m_nameLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_name->setGeometry(x0, y0, x0 + w, y0 + h);

    x0 = left;
    y0 -= h + dy;
    m_ipAddressLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_ipAddress->setGeometry(x0, y0, x0 + w, y0 + h);

    x0 = left;
    y0 -= h + dy;
    m_portNumberLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_portNumber->setGeometry(x0, y0, x0 + w, y0 + h);

    x0 = left;
    y0 -= h + dy;
    m_tasksLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_tasks->setGeometry(x0, y0, x0 + w, y0 + h);

    x0 = left;
    y0 -= h + dy;
    m_stateLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_state->setGeometry(x0, y0, x0 + w, y0 + h);

    x0 = left;
    y0 -= h + dy;
    m_cpuCountLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_cpuCount->setGeometry(x0, y0, x0 + w, y0 + h);

    x0 = left;
    y0 -= h + dy;
    m_totPhysMemLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
    x0 += lw + dx;
    m_totPhysMem->setGeometry(x0, y0, x0 + w, y0 + h);
  }

  TTextList *m_serverList;

  TTextField *m_name, *m_ipAddress, *m_portNumber, *m_tasks, *m_state,
      *m_cpuCount, *m_totPhysMem;

  TLabel *m_nameLbl, *m_ipAddressLbl, *m_portNumberLbl, *m_tasksLbl,
      *m_stateLbl, *m_cpuCountLbl, *m_totPhysMemLbl;
};

//------------------------------------------------------------------------------

void ServerStatusPage::onActivate() {
  TFarmController *controller = Application::instance()->getController();
  try {
    vector<ServerIdentity> servers;
    controller->getServers(servers);
    vector<ServerIdentity>::iterator it = servers.begin();
    for (; it != servers.end(); ++it) m_data->addServer(*it);
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
  }
}

//------------------------------------------------------------------------------

void ServerStatusPage::onDeactivate() {}

//------------------------------------------------------------------------------

ServerStatusPage::ServerStatusPage(TWidget *parent)
    : TabPage(parent, "Servers") {
  m_data = new Data(this);
}

//------------------------------------------------------------------------------

ServerStatusPage::~ServerStatusPage() {}

//------------------------------------------------------------------------------

void ServerStatusPage::configureNotify(const TDimension &size) {
  m_data->configureNotify(size);
}

//------------------------------------------------------------------------------

void ServerStatusPage::update() {
  TFarmController *controller = Application::instance()->getController();
  vector<ServerIdentity> servers;
  try {
    controller->getServers(servers);
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
    return;
  }

  vector<ServerIdentity>::iterator it = servers.begin();
  vector<ServerIdentity> newServers;
  std::set<int> oldServers;
  TTextList *sl = m_data->m_serverList;
  for (; it != servers.end(); ++it) {
    int index;
    if ((index = sl->itemToIndex(it->m_id)) == -1)
      newServers.push_back(*it);
    else
      oldServers.insert(index);
  }
  int i = 0, count = sl->getItemCount();
  for (; i < count; ++i)
    if (oldServers.find(i) == oldServers.end()) {
      string itemId = sl->getItem(i)->getId();
      /*
if(sl->isSelected(itemId))
sl->select((i+1)%sl->getItemCount(), true);
*/
      sl->removeItem(itemId);
    }
  it = newServers.begin();
  for (; it != newServers.end(); it++)
    sl->addItem(new TTextListItem(it->m_id, it->m_name));

  TTextListItem *item = m_data->m_serverList->getSelectedItem(0);
  if (item) m_data->showServerInfo(item->getId());
  invalidate();
}
