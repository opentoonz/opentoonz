

#include "taskstatuspage.h"
#include "tw/treeview.h"
#include "tw/colors.h"
#include "tfarmcontroller.h"
#include "application.h"
#include "tw/mainshell.h"
#include "tw/textfield.h"
#include "tw/label.h"
#include "tw/scrollbar.h"
#include "tw/event.h"
#include "textlist.h"

#include "tw/message.h"

#include "pixmaps.h"

#include <vector>
using namespace std;

using namespace TwConsts;

namespace {
class TaskTreeItemRoot : public TTreeViewItem {
public:
  TaskTreeItemRoot(TTreeViewItem *parent = 0) : TTreeViewItem(parent) {}

  wstring getName() const { return toWideString("tasks"); }

  TDimension getIconSize() const { return TDimension(32, 30); }

  void drawIcon(TTreeView *w, const TPoint &origin) {
    TRect rect(origin, getIconSize());
    rect = rect.enlarge(-3);
    w->rectwrite(tasks_icon, rect.getP00());
  }
};

class TaskTreeItem : public TTreeViewItem {
public:
  TaskTreeItem(TTreeViewItem *parent, string &id, string &name,
               TaskState status)
      : TTreeViewItem(parent), m_id(id) {
    /*
setIsLeaf(true);
if(task.m_parentId =="")
setIsLeaf(false);
*/
    m_name   = " <" + m_id + "> " + name;
    m_status = status;
  }

  wstring getName() const { return toWideString(m_name); }

  string getId() const { return m_id; }

  TDimension getIconSize() const { return TDimension(24, 22); }

  void drawIcon(TTreeView *w, const TPoint &origin) {
    TRect rect(origin, getIconSize());
    rect = rect.enlarge(-3);
    switch (m_status) {
    case Suspended:
      w->rectwrite(casm_suspended, rect.getP00());
      break;
    case Waiting:
      w->rectwrite(casm_waiting, rect.getP00());
      break;
    case Running:
      w->rectwrite(casm_computing, rect.getP00());
      break;
    case Completed:
      w->rectwrite(casm_done, rect.getP00());
      break;
    case Aborted:
      w->rectwrite(casm_done_with_errors, rect.getP00());
      break;
      // case TaskUnknown:
    }
  }

  string m_name;
  string m_id;
  TaskState m_status;
};

class SubtaskTreeItem : public TaskTreeItem {
public:
  SubtaskTreeItem(TaskTreeItem *parent, string &id, string &name,
                  TaskState status)
      : TaskTreeItem(parent, id, name, status) {
    setIsLeaf(true);
  }

  void drawIcon(TTreeView *w, const TPoint &origin) {
    TRect rect(origin, getIconSize());
    rect = rect.enlarge(-3);
    switch (m_status) {
    case Suspended:
      w->rectwrite(suspended, rect.getP00());
      break;
    case Waiting:
      w->rectwrite(waiting, rect.getP00());
      break;
    case Running:
      w->rectwrite(computing, rect.getP00());
      break;
    case Completed:
      w->rectwrite(done, rect.getP00());
      break;
    case Aborted:
      w->rectwrite(done_with_errors, rect.getP00());
      break;
      // case TaskUnknown:
    }
  }
};

class TaskTree : public TTreeView {
  TaskStatusPage *m_statusPage;
  TPopupMenu *m_popupMenu;
  TPopupMenu *m_globalPopupMenu;

public:
  TaskTree(TaskStatusPage *parent, string name = "taskTree")
      : TTreeView(parent, name), m_statusPage(parent) {
    m_popupMenu = new TPopupMenu(this);

    TPopupMenuItem *item = new TPopupMenuItem(m_popupMenu, "remove");
    TGuiCommand("remove").setAction(
        new TCommandAction<TaskTree>(this, onRemove));
    TGuiCommand("remove").add(item);

    item = new TPopupMenuItem(m_popupMenu, "restart");
    TGuiCommand("restart").setAction(
        new TCommandAction<TaskTree>(this, onRestart));
    TGuiCommand("restart").add(item);

    m_globalPopupMenu = new TPopupMenu(this);

    TPopupMenuItem *item1 =
        new TPopupMenuItem(m_globalPopupMenu, "restart all");
    TGuiCommand("restart all")
        .setAction(new TCommandAction<TaskTree>(this, onRestartAll));
    TGuiCommand("restart all").add(item1);
  }

  void rightButtonDown(const TMouseEvent &e) {
    leftButtonDown(e);
    TTreeViewItem *item    = getSelectedItem();
    TaskTreeItem *taskItem = dynamic_cast<TaskTreeItem *>(item);
    if (taskItem) {
      TPoint pos =
          getAbsolutePosition() + TPoint(e.m_pos.x, getSize().ly - e.m_pos.y);
      m_popupMenu->popup(pos);
    }
    TaskTreeItemRoot *taskItemRoot = dynamic_cast<TaskTreeItemRoot *>(item);
    if (taskItemRoot) {
      TPoint pos =
          getAbsolutePosition() + TPoint(e.m_pos.x, getSize().ly - e.m_pos.y);
      m_globalPopupMenu->popup(pos);
    }
  }

  void onSelect(TTreeViewItem *item) {
    TaskTreeItem *taskItem = dynamic_cast<TaskTreeItem *>(item);
    if (taskItem) m_statusPage->showTaskInfo(taskItem->getId());
  }

  void onExpand(TTreeViewItem *item) {
    string id       = "";
    TaskTreeItem *i = dynamic_cast<TaskTreeItem *>(item);
    if (i) id       = i->getId();

    item->clearItems();
    TFarmController *controller = Application::instance()->getController();
    vector<string> tasks;
    try {
      controller->getTasks(id, tasks);

      vector<string>::iterator it = tasks.begin();
      for (int j = 0; it != tasks.end(); ++it, ++j) {
        string taskId = *it;
        if (taskId != "") {
          TFarmTask task;

          string parentId, name;
          TaskState status;
          controller->queryTaskShortInfo(taskId, parentId, name, status);

          if (!item->getItem(name))
            if (parentId == "")
              item->addItem(new TaskTreeItem(0, taskId, name, status));
            else
              item->addItem(new SubtaskTreeItem(0, taskId, name, status));
        }
      }

      /*
int count = tasks.size();
for (int j = 0; j< count; ++j)
if(tasks[j] != "")
{
TFarmTask task;
controller->queryTaskInfo(tasks[j], task);
//TMainshell::errorMessage("dopo queryTaskInfo" + tasks[j]);

if(!item->getItem(task.m_name))
if(task.m_parentId == "")
item->addItem(new TaskTreeItem(0, task));
else
item->addItem(new SubtaskTreeItem(0, task));
}
*/
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }

  void onCollapse(TTreeViewItem *item) {
    /*
string id = "";
TaskTreeItem *taskItem = dynamic_cast<TaskTreeItem*> (item);
if(taskItem)
*/
    item->clearItems();
  }

  void onRemove() {
    string id = "";
    TaskTreeItem *taskTreeItem =
        dynamic_cast<TaskTreeItem *>(getSelectedItem());
    if (taskTreeItem) id = taskTreeItem->getId();

    TFarmController *controller = Application::instance()->getController();
    try {
      controller->removeTask(id);
      update();
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }

  void onRestart() {
    string id = "";
    TaskTreeItem *taskTreeItem =
        dynamic_cast<TaskTreeItem *>(getSelectedItem());
    if (taskTreeItem) id = taskTreeItem->getId();

    TFarmController *controller = Application::instance()->getController();
    try {
      controller->restartTask(id);
      update();
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }

  void onRestartAll() {
    TFarmController *controller = Application::instance()->getController();
    try {
      vector<string> tasks;
      controller->getTasks("", tasks);
      int i = 0;
      for (; i < (int)tasks.size(); i++) controller->restartTask(tasks[i]);
      update();
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
    }
  }

  void update(TTreeViewItem *item) {
    if (!item->isOpen()) return;

    TFarmController *controller = Application::instance()->getController();

    string taskId;
    TaskTreeItem *task = dynamic_cast<TaskTreeItem *>(item);
    if (task) taskId   = task->getId();

    vector<TaskShortInfo> subTasks;
    try {
      controller->getTasks(taskId, subTasks);
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
      return;
    }

    vector<TTreeViewItem *> toBeDeleted;
    int count = item->getItemCount();
    for (int i = 0; i < count; ++i) {
      TaskTreeItem *subTask = dynamic_cast<TaskTreeItem *>(item->getItem(i));
      if (subTask) {
        vector<TaskShortInfo>::iterator it = subTasks.begin();
        for (; it != subTasks.end(); ++it) {
          string id = it->m_id;
          if (id == subTask->getId()) break;
        }

        if (it != subTasks.end()) {
          subTask->m_status = it->m_status;
          update(subTask);
        } else
          toBeDeleted.push_back(subTask);
      }
    }

    vector<TTreeViewItem *>::iterator it = toBeDeleted.begin();
    while (it != toBeDeleted.end()) {
      if (getSelectedItem() == *it) {
        TTreeViewItem *task = dynamic_cast<TTreeViewItem *>((*it)->getParent());
        if (task) select(task);
      }
      item->removeItem(*it++);
    }

#ifdef PRIMA
    TFarmController *controller = Application::instance()->getController();
    vector<TTreeViewItem *> toBeDeleted;
    int count = item->getItemCount();
    for (int i = 0; i < count; ++i) {
      TaskTreeItem *task = dynamic_cast<TaskTreeItem *>(item->getItem(i));
      if (task) {
        TFarmTask taskInfo;
        try {
          string parentId, name;
          TaskState status;
          controller->queryTaskShortInfo(task->getId(), parentId, name, status);
          if (status == TaskUnknown)
            toBeDeleted.push_back(task);
          else {
            task->m_status = status;
            update(task);
          }
        } catch (TException &e) {
          TMainshell::errorMessage(e.getMessage());
          return;
        }
      }
    }

    vector<TTreeViewItem *>::iterator it = toBeDeleted.begin();
    while (it != toBeDeleted.end()) {
      if (getSelectedItem() == *it) {
        TTreeViewItem *task = dynamic_cast<TTreeViewItem *>((*it)->getParent());
        if (task) select(task);
      }
      item->removeItem(*it++);
    }
#endif
  }

  void update() {
    TFarmController *controller = Application::instance()->getController();

    TTreeViewItem *item = getItem(0);
    if (!item->isOpen()) return;

    if (item) update(item);

    // aggiungo i nuovi
    vector<string> tasks;
    try {
      controller->getTasks("", tasks);
    } catch (TException &e) {
      TMessage::error(toString(e.getMessage()));
      return;
    }

    vector<string>::iterator it = tasks.begin();
    for (; it != tasks.end(); it++) {
      bool old  = false;
      int count = item->getItemCount();
      for (int i = 0; i < count; ++i) {
        TaskTreeItem *task = dynamic_cast<TaskTreeItem *>(item->getItem(i));
        if (task) old      = (*it == task->getId());
        if (old) break;
      }

      if (!old) {
        if (*it != "") {
          TFarmTask task;
          try {
            string parentId, name;
            TaskState status;
            controller->queryTaskShortInfo(*it, parentId, name, status);
            item->addItem(new TaskTreeItem(0, *it, name, status));
          } catch (TException &e) {
            TMessage::error(toString(e.getMessage()));
            return;
          }
        }
      }
    }
    updateVisibleItems();
    invalidate();
  }
};
};  // namespace

//==============================================================================

class TaskStatusPage::Data {
public:
  Data(TaskStatusPage *parent);

  void showTaskInfo(const TFarmTask &task);
  void configureNotify(const TDimension &size);
  void clearFields();

  TFarmTask m_currTask;

  TaskTree *m_tree;
  TTextField *m_taskId, *m_cmdLine, *m_server, *m_byUser, *m_onMachine,
      *m_priority, *m_submissionDate, *m_startDate, *m_completionDate,
      *m_taskStatus, *m_failedSteps, *m_successfullSteps, *m_stepCount;

  TLabel *m_taskIdLbl, *m_cmdLineLbl, *m_serverLbl, *m_byUserLbl,
      *m_onMachineLbl, *m_priorityLbl, *m_submissionDateLbl, *m_startDateLbl,
      *m_completionDateLbl, *m_taskStatusLbl, *m_failedStepsLbl,
      *m_successfullStepsLbl, *m_stepCountLbl, *m_dependenciesLbl;

  TTextList *m_dependencies;

  TScrollbar *m_scrollVBar, *m_scrollHBar;
};

//------------------------------------------------------------------------------

TaskStatusPage::Data::Data(TaskStatusPage *parent) {
  m_tree                = new TaskTree(parent, "taskTree");
  m_taskId              = new TTextField(parent, "taskId");
  m_cmdLine             = new TTextField(parent, "cmdLine");
  m_server              = new TTextField(parent, "server");
  m_byUser              = new TTextField(parent, "byUser");
  m_onMachine           = new TTextField(parent, "onMachine");
  m_priority            = new TTextField(parent, "priority");
  m_submissionDate      = new TTextField(parent, "submissionDate");
  m_startDate           = new TTextField(parent, "startDate");
  m_completionDate      = new TTextField(parent, "completionDate");
  m_taskStatus          = new TTextField(parent, "taskStatus");
  m_stepCount           = new TTextField(parent, "stepCount");
  m_failedSteps         = new TTextField(parent, "failedSteps");
  m_successfullSteps    = new TTextField(parent, "successfullSteps");
  m_dependencies        = new TTextList(parent);
  m_taskIdLbl           = new TLabel(parent, "task id");
  m_cmdLineLbl          = new TLabel(parent, "command line");
  m_serverLbl           = new TLabel(parent, "server");
  m_byUserLbl           = new TLabel(parent, "submitted by");
  m_onMachineLbl        = new TLabel(parent, "submitted on");
  m_priorityLbl         = new TLabel(parent, "priority");
  m_submissionDateLbl   = new TLabel(parent, "submission date");
  m_startDateLbl        = new TLabel(parent, "start date");
  m_completionDateLbl   = new TLabel(parent, "completion date");
  m_stepCountLbl        = new TLabel(parent, "step count");
  m_failedStepsLbl      = new TLabel(parent, "failed steps");
  m_successfullStepsLbl = new TLabel(parent, "successfull steps");
  m_dependenciesLbl     = new TLabel(parent, "dependencies");
  m_taskStatusLbl       = new TLabel(parent, "status");
  m_scrollVBar          = new TScrollbar(parent, "vscroll");
  m_scrollHBar          = new TScrollbar(parent, "hscroll");
  m_tree->setScrollbars(m_scrollHBar, m_scrollVBar);
}

//------------------------------------------------------------------------------

void TaskStatusPage::Data::showTaskInfo(const TFarmTask &task) {
  switch (task.m_status) {
  case Waiting:
    m_taskStatus->setText("Waiting");
    break;
  case Running:
    m_taskStatus->setText("Running");
    break;
  case Completed:
    m_taskStatus->setText("Completed");
    break;
  case Aborted:
    m_taskStatus->setText("Failed");
    break;
  case TaskUnknown:
    clearFields();
    return;
  }
  m_taskId->setText(task.m_id);
  m_cmdLine->setText(task.m_cmdline);
  m_server->setText(task.m_server);
  m_byUser->setText(task.m_user);
  m_onMachine->setText(task.m_hostName);
  m_priority->setText(toString(task.m_priority));
  m_submissionDate->setText(task.m_submissionDate);
  m_startDate->setText(task.m_startDate);
  m_completionDate->setText(task.m_completionDate);
  m_stepCount->setText(toString(task.m_stepCount));
  m_failedSteps->setText(toString(task.m_failedSteps));
  m_successfullSteps->setText(toString(task.m_successfullSteps));
  m_dependencies->clearAll();
  if (task.m_dependencies) {
    int count = task.m_dependencies->getTaskCount(), i = 0;
    TFarmController *controller = Application::instance()->getController();
    for (; i < count; ++i) {
      string id, parentId, name;
      TaskState status;
      id = task.m_dependencies->getTaskId(i);
      try {
        controller->queryTaskShortInfo(id, parentId, name, status);
      } catch (TException &e) {
        TMessage::error(toString(e.getMessage()));
      }
      string label = "<" + id + "> " + name;
      m_dependencies->addItem(new TTextListItem(id, label));
    }
  }
  m_dependencies->invalidate();
}

//------------------------------------------------------------------------------

void TaskStatusPage::Data::configureNotify(const TDimension &size) {
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
  const int scbSize = 15;
  TRect rect        = TRect(0, 0, leftSize, size.ly);
  int x             = leftSize;
  m_tree->setGeometry(rect.x0, rect.y0 + scbSize, x - 1 - scbSize, rect.y1);
  m_scrollVBar->setGeometry(x - scbSize, rect.y0 + scbSize, x - 1, rect.y1);
  m_scrollHBar->setGeometry(rect.x0, rect.y0, x - 1 - scbSize,
                            rect.y0 + scbSize - 1);

  // ora la parte a dx
  x0 = left;
  m_taskIdLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_taskId->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_taskStatusLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_taskStatus->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_cmdLineLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_cmdLine->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_serverLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_server->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_byUserLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_byUser->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_onMachineLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_onMachine->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_priorityLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_priority->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_submissionDateLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_submissionDate->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_startDateLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_startDate->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_completionDateLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_completionDate->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_stepCountLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_stepCount->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_failedStepsLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_failedSteps->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_successfullStepsLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_successfullSteps->setGeometry(x0, y0, x0 + w, y0 + h);

  x0 = left;
  y0 -= h + dy;
  m_dependenciesLbl->setGeometry(x0, y0, x0 + lw, y0 + h);
  x0 += lw + dx;
  m_dependencies->setGeometry(x0, y0 - 3 * h, x0 + w, y0 + h);
}

//------------------------------------------------------------------------------

void TaskStatusPage::Data::clearFields() {
  m_taskStatus->setText("");
  m_taskId->setText("");
  m_cmdLine->setText("");
  m_server->setText("");
  m_byUser->setText("");
  m_onMachine->setText("");
  m_priority->setText("");
  m_submissionDate->setText("");
  m_startDate->setText("");
  m_completionDate->setText("");
  m_stepCount->setText("");
  m_failedSteps->setText("");
  m_successfullSteps->setText("");
  m_dependencies->clearAll();
}

//==============================================================================

TaskStatusPage::TaskStatusPage(TWidget *parent) : TabPage(parent, "Tasks") {
  m_data = new Data(this);
}

//------------------------------------------------------------------------------

TaskStatusPage::~TaskStatusPage() {}

//------------------------------------------------------------------------------

void TaskStatusPage::configureNotify(const TDimension &size) {
  m_data->configureNotify(size);
}

//------------------------------------------------------------------------------

void TaskStatusPage::rightButtonDown(const TMouseEvent &e) {}

//------------------------------------------------------------------------------

void TaskStatusPage::onActivate() {
  if (m_data->m_tree && m_data->m_tree->getItemCount() < 1) {
    TaskTreeItemRoot *root = new TaskTreeItemRoot();
    root->setIsLeaf(false);
    m_data->m_tree->clearItems();
    m_data->m_tree->addItem(root);
  }
  m_data->m_tree->update();
  invalidate();
}

//------------------------------------------------------------------------------

void TaskStatusPage::onDeactivate() {}

//------------------------------------------------------------------------------

void TaskStatusPage::showTaskInfo(const string &id) {
  TFarmController *controller = Application::instance()->getController();
  try {
    TFarmTask task;
    controller->queryTaskInfo(id, task);

    if (task != m_data->m_currTask) {
      m_data->m_currTask = task;
      m_data->showTaskInfo(task);
    }
  } catch (TException &e) {
    TMessage::error(toString(e.getMessage()));
  }
}

//------------------------------------------------------------------------------

void TaskStatusPage::update() {
  m_data->m_tree->update();

  string id = "";
  TaskTreeItem *item =
      dynamic_cast<TaskTreeItem *>(m_data->m_tree->getSelectedItem());
  if (item) id = item->getId();

  if (id != "")
    showTaskInfo(id);
  else
    m_data->clearFields();
  invalidate();
}

//------------------------------------------------------------------------------
