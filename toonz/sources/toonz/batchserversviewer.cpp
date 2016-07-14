

#include "batchserversviewer.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "tfarmstuff.h"
#include "batches.h"
#include "tsystem.h"

#include <QGridLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QMouseEvent>
#include <QMenu>

using namespace DVGui;
using namespace TFarmStuff;

//------------------------------------------------------------------------------

class MyListItem final : public QListWidgetItem {
public:
  QString m_id;

  MyListItem(const QString &id, const QString &name, QListWidget *w)
      : QListWidgetItem(name, w), m_id(id) {}
};

//-----------------------------------------------------------------------------

FarmServerListView::FarmServerListView(QWidget *parent) : QListWidget(parent) {
  setFrameStyle(QFrame::StyledPanel);
}

//-----------------------------------------------------------------------------

void FarmServerListView::update() {
  try {
    while (count()) delete takeItem(0);

    TFarmController *controller = getTFarmController();

    std::vector<ServerIdentity> servers;
    controller->getServers(servers);
    std::vector<ServerIdentity>::iterator it = servers.begin();
    for (; it != servers.end(); ++it) {
      ServerIdentity sid = *it;
      new MyListItem(sid.m_id, sid.m_name, this);
    }
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
  }
}

//-----------------------------------------------------------------------------

void FarmServerListView::activate() {
  MyListItem *item = (MyListItem *)currentItem();
  if (!item) return;

  TFarmController *controller = getTFarmController();
  try {
    controller->activateServer(item->m_id);
    BatchesController::instance()->update();
    static_cast<BatchServersViewer *>(parentWidget())->updateSelected();
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
  }
}

//-----------------------------------------------------------------------------

void FarmServerListView::deactivate() {
  MyListItem *item = (MyListItem *)currentItem();
  if (!item) return;

  TFarmController *controller = getTFarmController();
  try {
    controller->deactivateServer(item->m_id);
    BatchesController::instance()->update();
    static_cast<BatchServersViewer *>(parentWidget())->updateSelected();
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
  }
}
//-----------------------------------------------------------------------------

void FarmServerListView::openContextMenu(const QPoint &p) {
  MyListItem *item = (MyListItem *)currentItem();
  if (!item) return;

  m_menu.reset(new QMenu(this));

  TFarmController *controller = getTFarmController();
  ServerState state;
  try {
    state = controller->queryServerState2(item->m_id);
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
    return;
  }

  QAction *action;
  if (state == Offline) {
    action = new QAction(tr("Activate"), this);
    connect(action, SIGNAL(triggered(bool)), this, SLOT(activate()));
  } else {
    action = new QAction(tr("Deactivate"), this);
    connect(action, SIGNAL(triggered(bool)), this, SLOT(deactivate()));
  }

  m_menu->addAction(action);
  m_menu->exec(p);
}

//-----------------------------------------------------------------------------

void FarmServerListView::mousePressEvent(QMouseEvent *event) {
  QListWidget::mousePressEvent(event);

  if (event->button() == Qt::RightButton) openContextMenu(event->globalPos());
}

//-----------------------------------------------------------------------------

void BatchServersViewer::updateServerInfo(const QString &id) {
  if (id == "") {
    m_name->setText("");
    m_ip->setText("");
    m_port->setText("");
    m_state->setText("");
    m_tasks->setText("");
    m_cpu->setText("");
    m_mem->setText("");
    return;
  }

  if (id == "localhost") {
    m_name->setText(TSystem::getHostName());
    m_ip->setText("");
    m_port->setText("");
    m_state->setText("");
    m_tasks->setText("");
    m_cpu->setText(QString::number(TSystem::getProcessorCount()));

#ifdef _WIN32
    // Please observe that the commented value is NOT the same reported by
    // tfarmserver...
    MEMORYSTATUSEX buff;
    buff.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&buff);

    unsigned long mem = buff.ullTotalPhys >> 30;
    m_mem->setText(QString::number(mem, 'g', 2) + " GB");
#else
    double mem = (double)TSystem::getFreeMemorySize(false) / (1024 * 1024);
    m_mem->setText(QString::number(mem, 'g', 2) + " GB");
#endif

    return;
  }

  TFarmController *controller = getTFarmController();
  ServerInfo info;

  try {
    controller->queryServerInfo(id, info);
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
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
    m_ip->setText("");
    m_port->setText("");
    m_tasks->setText("");
    m_cpu->setText("");
    m_mem->setText("");
    return;
  }

  m_name->setText(info.m_name);
  m_ip->setText(info.m_ipAddress);
  m_port->setText(info.m_portNumber);
  if (info.m_currentTaskId == "")
    m_tasks->setText("");
  else {
    TFarmTask task;
    try {
      controller->queryTaskInfo(info.m_currentTaskId, task);
      m_tasks->setText("<" + task.m_id + "> " + task.m_name);
    } catch (TException &e) {
      m_tasks->setText("");
      DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
    }
  }

  if (info.m_state != Down) {
    m_cpu->setText(QString::number(info.m_cpuCount));
    double mem = (double)info.m_totPhysMem / (1024 * 1024);
    m_mem->setText(QString::number(mem, 'g', 2) + " GB");
  } else {
    m_cpu->setText("");
    m_mem->setText("");
  }
}

//-----------------------------------------------------------------------------

void BatchServersViewer::updateSelected() {
  MyListItem *item = (MyListItem *)m_serverList->currentItem();
  onCurrentItemChanged(item);
}

//===================================================================

void BatchServersViewer::onCurrentItemChanged(QListWidgetItem *_item) {
  MyListItem *item = (MyListItem *)_item;
  updateServerInfo(item ? item->m_id : QString());
}

//=============================================================================
/*! \class BatchServersViewer
                \brief The BatchServersViewer class is a ...

                Inherits \b QFrame.
*/

static LineEdit *create(QGridLayout *layout, const QString &name, int &row,
                        bool readOnly = true) {
  layout->addWidget(new QLabel(name), row, 0, Qt::AlignRight);
  LineEdit *ret = new LineEdit();
  ret->setReadOnly(readOnly);
  layout->addWidget(ret, row++, 1);
  return ret;
}

#if QT_VERSION >= 0x050500
BatchServersViewer::BatchServersViewer(QWidget *parent, Qt::WindowFlags flags)
#else
BatchServersViewer::BatchServersViewer(QWidget *parent, Qt::WFlags flags)
#endif
    : QFrame(parent) {
  int row = 0;

  setFrameStyle(QFrame::StyledPanel);
  setObjectName("OnePixelMarginFrame");

  QGridLayout *layout = new QGridLayout();
  layout->setMargin(15);
  layout->setSpacing(8);

  layout->addWidget(new QLabel(tr("Process with:")), row, 0, Qt::AlignRight);
  m_processWith = new QComboBox();
  QStringList type;
  type << tr("Local") << tr("Render Farm");
  m_processWith->addItems(type);
  layout->addWidget(m_processWith, row++, 1);
  connect(m_processWith, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onProcessWith(int)));

  m_farmRootField = ::create(layout, tr("Farm Global Root:"), row, false);

  QString path = QSettings().value("TFarmGlobalRoot").toString();
  if (path != "") {
    m_farmRootField->setText(path);
    TFarmStuff::setGlobalRoot(TFilePath(path.toStdWString()));
  }
  connect(m_farmRootField, SIGNAL(editingFinished()), this, SLOT(setGRoot()));

  m_serverList = new FarmServerListView(this);
  connect(m_serverList,
          SIGNAL(currentItemChanged(QListWidgetItem *, QListWidgetItem *)),
          this, SLOT(onCurrentItemChanged(QListWidgetItem *)));
  // connect(m_serverList, SIGNAL(itemPressed(QListWidgetItem *)), this,
  // SLOT(onCurrentItemChanged(QListWidgetItem *)));

  layout->addWidget(m_serverList, row, 0, 18, 2);
  row += 18 + 1;

  m_name  = ::create(layout, tr("Name:"), row);
  m_ip    = ::create(layout, tr("IP Address:"), row);
  m_port  = ::create(layout, tr("Port Number:"), row);
  m_tasks = ::create(layout, tr("Tasks:"), row);
  m_state = ::create(layout, tr("State:"), row);
  m_cpu   = ::create(layout, tr("Number of CPU:"), row);
  m_mem   = ::create(layout, tr("Physical Memory:"), row);

  setLayout(layout);
}

//-----------------------------------------------------------------------------

BatchServersViewer::~BatchServersViewer() {}

//-----------------------------------------------------------------------------

void BatchServersViewer::setGRoot() {
  TFarmStuff::setGlobalRoot(TFilePath(m_farmRootField->text().toStdWString()));
  QSettings().setValue("TFarmGlobalRoot", m_farmRootField->text());
}

//=============================================================================
// vinz: GRootEnvVarPopup togliere?

void BatchServersViewer::onProcessWith(int index) {
  if (index == 0) {
    BatchesController::instance()->setController(0);
    m_serverList->clear();
    m_serverId = "localhost";
    return;
  }

  bool connected = false;
  try {
    connected = TFarmStuff::testConnectionToController();
  } catch (TMissingGRootEnvironmentVariable &) {
    DVGui::warning(
        QString(tr("In order to use the render farm you have to define the "
                   "Farm Global Root first.")));
    m_processWith->setCurrentIndex(0);
    return;
  } catch (TMissingGRootFolder &) {
    DVGui::warning(
        tr("The Farm Global Root folder doesn't exist\nPlease create this "
           "folder before using the render farm."));
    m_processWith->setCurrentIndex(0);
    return;
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
    m_processWith->setCurrentIndex(0);
    return;
  }

  if (!TFarmStuff::testConnectionToController()) {
    QString hostName, addr;
    int port;
    TFarmStuff::getControllerData(hostName, addr, port);

    QString msg(tr("Unable to connect to the ToonzFarm Controller\n \
  The Controller should run on %1 at port %2\n \
  Please start the Controller before using the ToonzFarm")
                    .arg(hostName)
                    .arg(QString::number(port)));

    DVGui::warning(msg);
    m_processWith->setCurrentIndex(0);
    return;
  }

  BatchesController::instance()->setController(getTFarmController());

  m_serverList->update();

  // m_serversListView->update();
  // if (m_serversListView->getItemCount() > 0)
  //  m_serversListView->select(0, true);
}

//-------------------------------------------------------------------------------

OpenFloatingPanel openBatchServersCommand(MI_OpenBatchServers, "BatchServers",
                                          QObject::tr("Batch Servers"));
