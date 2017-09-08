

#ifdef _WIN32
#define _WIN32_WINNT 0x0500  // per CreateJobObject e affini
#endif

#include "tsystem.h"
#include "mainwindow.h"
#include "batches.h"
#include "tthreadmessage.h"
#include "tfarmexecutor.h"
#include "tstream.h"
#include "tfarmstuff.h"
#include "tenv.h"
#include "toutputproperties.h"
#include "trasterfx.h"
#include "tasksviewer.h"
#include "tapp.h"
#include "filebrowserpopup.h"
#include "tmsgcore.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/preferences.h"

#include "toonzqt/gutil.h"

#include <QString>
#include <QProcess>
#include <QHostInfo>

namespace {

class NotifyMessage final : public TThread::Message {
public:
  NotifyMessage() {}

  void onDeliver() override { BatchesController::instance()->update(); }

  TThread::Message *clone() const override { return new NotifyMessage(*this); }
};

}  // namespace

//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------

LoadTaskListPopup::LoadTaskListPopup()
    : FileBrowserPopup(tr("Load Task List")) {
  addFilterType("tnzbat");
  setOkText(tr("Load"));
}

bool LoadTaskListPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(toQString(fp) + tr(" does not exist."));
    return false;
  } else if (fp.getType() != "tnzbat") {
    DVGui::error(toQString(fp) +
                 tr("It is possible to load only TNZBAT files."));
    return false;
  }

  BatchesController::instance()->doLoad(fp);
  return true;
}

//----------------------------------------------------------------------------------------------------------------

LoadTaskPopup::LoadTaskPopup() : FileBrowserPopup(""), m_isRenderTask(true) {
  addFilterType("tnz");
  setOkText(tr("Add"));
}

void LoadTaskPopup::open(bool isRenderTask) {
  m_isRenderTask = isRenderTask;
  setWindowTitle(isRenderTask ? tr("Add Render Task to Batch List")
                              : tr("Add Cleanup Task to Batch List"));

  if (isRenderTask)
    removeFilterType("cln");
  else
    addFilterType("cln");

  exec();
}

bool LoadTaskPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(toQString(fp) + tr(" does not exist."));
    return false;
  } else if (m_isRenderTask && fp.getType() != "tnz") {
    DVGui::error(toQString(fp) +
                 tr(" you can load only TNZ files for render task."));
    return false;
  } else if (!m_isRenderTask && fp.getType() != "tnz" &&
             fp.getType() != "cln") {
    DVGui::error(toQString(fp) +
                 tr(" you can load only TNZ or CLN files for cleanup task."));
    return false;
  }

  if (m_isRenderTask)
    BatchesController::instance()->addComposerTask(fp);
  else
    BatchesController::instance()->addCleanupTask(fp);

  return true;
}

//=============================================================================

SaveTaskListPopup::SaveTaskListPopup()
    : FileBrowserPopup(tr("Save Task List")) {
  setOkText(tr("Save"));
}

bool SaveTaskListPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  BatchesController::instance()->doSave(fp.withType("tnzbat"));
  return true;
}

//------------------------------------------------------------------------------
QMutex TasksMutex;
map<QString, QProcess *> RunningTasks;

class TaskRunner final : public TThread::Runnable {
public:
  TaskRunner(TFarmTask *task, int localControllerPort)
      : m_task(task), m_localControllerPort(localControllerPort) {}

  void run() override;

  int taskLoad() override;

  void doRun(TFarmTask *task);

  void runCleanup() { BatchesController::instance()->notify(); }

  TFarmTask *m_task;
  int m_localControllerPort;
};

void TaskRunner::run() { doRun(m_task); }

int TaskRunner::taskLoad() {
  // We assume that CPU control is under the user's responsibility about
  // tcomposer tasks - so they don't figure in CPU consumption calculations.
  return 0;
}

/*QString getFarmAddress()
{
if (BatchesController::instance()->getController())
  {
  QString dummystr, address;
  int dummyint;
  TFarmStuff::getControllerData(dummystr, address, dummyint);

  return address;
  }
else return  "127.0.0.1"; //localHost
}*/

void TaskRunner::doRun(TFarmTask *task) {
  // Suspended tasks are not executed. (Should only waiting ones be executed?)
  if (task->m_status == Suspended) return;

  if (task->m_dependencies && task->m_dependencies->getTaskCount() != 0) {
    task->m_status = Waiting;
    int depCount   = task->m_dependencies->getTaskCount();
    for (int i = 0; i < depCount; ++i) {
      QString depTaskId  = task->m_dependencies->getTaskId(i);
      TFarmTask *depTask = BatchesController::instance()->getTask(depTaskId);
      if (depTask) {
        doRun(depTask);
      }
    }
  }

  task->m_successfullSteps = task->m_failedSteps = 0;
  task->m_completionDate                         = QDateTime();

  task->m_status    = Running;
  task->m_server    = TSystem::getHostName();
  task->m_startDate = QDateTime::currentDateTime();

  NotifyMessage().send();

  int count = task->getTaskCount();
  if (count > 1) {
    // Launch each subtask
    for (int i = 0; i < task->getTaskCount(); i++) {
      if (task->m_status == Suspended) break;

      doRun(task->getTask(i));
    }

    // Check their status
    task->m_status           = Completed;
    task->m_successfullSteps = task->m_to - task->m_from + 1;
    task->m_completionDate   = QDateTime::currentDateTime();
    for (int i = 0; i < task->getTaskCount(); i++) {
      if (task->getTask(i)->m_status == Aborted) task->m_status = Aborted;
    }

    NotifyMessage().send();

    return;
  }

  // int exitCode;

  /*QString commandline = task->getCommandLine();

commandline += " -farm ";
commandline += QString::number(m_localControllerPort) + "@localhost";
commandline += " -id " + task->m_id;*/

  QProcess *process = new QProcess();
  std::map<QString, QProcess *>::iterator it;

  {
    QMutexLocker sl(&TasksMutex);
    if ((it = RunningTasks.find(task->m_id)) != RunningTasks.end()) {
      it->second->kill();
      RunningTasks.erase(task->m_id);
    }

    RunningTasks[task->m_id] = process;
  }

  process->start(task->getCommandLine());
  process->waitForFinished(-1);

  {
    QMutexLocker sl(&TasksMutex);
    RunningTasks.erase(task->m_id);
  }
  // exitCode = QProcess::execute(task->getCommandLine());

  int error     = process->error();
  bool statusOK = (error == QProcess::UnknownError) &&
                  (process->exitCode() == 0 ||
                   task->m_successfullSteps == task->m_stepCount);

  if (statusOK) {
    task->m_status           = Completed;
    task->m_successfullSteps = task->m_to - task->m_from + 1;
  } else {
    task->m_status      = Aborted;
    task->m_failedSteps = task->m_to - task->m_from + 1;
  }

  task->m_completionDate = QDateTime::currentDateTime();

  NotifyMessage().send();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BatchesController::BatchesController()
    : m_controller(0), m_tasksTree(0), m_filepath(), m_dirtyFlag(false) {}

//------------------------------------------------------------------------------

BatchesController::~BatchesController() {}

//------------------------------------------------------------------------------

void BatchesController::setTasksTree(TaskTreeModel *tree) {
  m_tasksTree = tree;
}

//------------------------------------------------------------------------------

inline bool isMovieType(string type) {
  return (type == "mov" || type == "avi" || type == "3gp");
}

//------------------------------------------------------------------------------

static int TaskCounter = 1;

//------------------------------------------------------------------------------

void BatchesController::addCleanupTask(const TFilePath &taskFilePath) {
  setDirtyFlag(true);
  QString id = QString::number(TaskCounter++);
  while (BatchesController::instance()->getTask(id))
    id = QString::number(TaskCounter++);

  TFarmTaskGroup *taskGroup = new TFarmTaskGroup(
      id,                                              // id
      QString::fromStdString(taskFilePath.getName()),  // name
      TSystem::getUserName(),                          // user
      TSystem::getHostName(),                          // host
      1,                                               // stepCount
      50,                                              // priority
      taskFilePath,                                    // taskFilePath
      Overwrite_Off,                                   // Overwrite
      false);                                          // onlyvisible

  try {
    BatchesController::instance()->addTask(id, taskGroup);
  } catch (TException &) {
  }
}

//------------------------------------------------------------------------------

void BatchesController::addComposerTask(const TFilePath &_taskFilePath) {
  setDirtyFlag(true);
  TFilePath taskFilePath;
  try {
    taskFilePath = TSystem::toUNC(_taskFilePath);
  } catch (TException &) {
  }

  ToonzScene scene;
  try {
    scene.loadNoResources(taskFilePath);
  } catch (...) {
    return;
  }

  TSceneProperties *sprop      = scene.getProperties();
  const TOutputProperties &out = *sprop->getOutputProperties();
  const TRenderSettings &rs    = out.getRenderSettings();

  QString name = QString::fromStdString(taskFilePath.getName());
  int r0, r1, step;
  out.getRange(r0, r1, step);

  int sceneFrameCount = scene.getFrameCount();
  if (r0 < 0) r0      = 0;
  if (r1 >= sceneFrameCount)
    r1 = sceneFrameCount - 1;
  else if (r1 < r0)
    r1 = sceneFrameCount - 1;

  r0++, r1++;
  int shrink = rs.m_shrinkX;

  int multimedia       = out.getMultimediaRendering();
  int threadsIndex     = out.getThreadIndex();
  int maxTileSizeIndex = out.getMaxTileSizeIndex();

  TFilePath outputPath =
      scene.decodeFilePath(sprop->getOutputProperties()->getPath());
  if (outputPath.getWideName() == L"")
    outputPath = outputPath.withName(scene.getScenePath().getName());
  int taskChunkSize;
  if (isMovieType(outputPath.getType()))
    taskChunkSize = sceneFrameCount;
  else {
    int size = Preferences::instance()->getDefaultTaskChunkSize();
    if (r1 - r0 + 1 < size)
      taskChunkSize = r1 - r0 + 1;
    else
      taskChunkSize = size;
  }

  QString id = QString::number(TaskCounter++);
  while (BatchesController::instance()->getTask(id))
    id = QString::number(TaskCounter++);

  TFarmTaskGroup *taskGroup = new TFarmTaskGroup(
      id, name, TSystem::getUserName(), TSystem::getHostName(), sceneFrameCount,
      50, taskFilePath, outputPath, r0, r1, step, shrink, multimedia,
      taskChunkSize, threadsIndex, maxTileSizeIndex);

  try {
    BatchesController::instance()->addTask(id, taskGroup);
  } catch (TException &) {
    // TMessage::error(toString(e.getMessage()));
  }
  // m_data->m_scene.setProject( mainprogramProj);
  // TModalPopup::closePopup();
}

//-------------------------------------------------------------------------------------------------------

/*--- id はタスクを追加するごとにインクリメントされる数字 ---*/
void BatchesController::addTask(const QString &id, TFarmTask *task,
                                bool doUpdate) {
#ifdef _DEBUG
  std::map<QString, TFarmTask *>::iterator it = m_tasks.find(id);
// assert(it == m_tasks.end());
#endif
  if (task->m_id.isEmpty()) task->m_id = id;

  task->m_status         = Suspended;
  task->m_submissionDate = QDateTime::currentDateTime();

  m_tasks.insert(std::make_pair(id, task));

  int count = task->getTaskCount();
  if (count == 1 && task->getTask(0) == task) return;

  for (int i = 0; i < count; ++i) {
    TFarmTask *subtask = task->getTask(i);
    assert(subtask);
    addTask(subtask->m_id, subtask);
  }
  if (doUpdate) update();
}

//------------------------------------------------------------------------------

QString BatchesController::taskBusyStr() {
  return tr(
      "The %1 task is currently active.\nStop it or wait for its completion "
      "before removing it.");
}

//------------------------------------------------------------------------------

void BatchesController::removeTask(const QString &id) {
  std::map<QString, TFarmTask *>::iterator it = m_tasks.find(id);
  if (it == m_tasks.end()) return;

  TFarmTask *task = it->second;
  if (task->m_status == Running || task->m_status == Waiting) {
    DVGui::warning(taskBusyStr().arg(task->m_name));
    return;
  }

  setDirtyFlag(true);

  if (!task->m_parentId.isEmpty()) {
    it                     = m_tasks.find(task->m_parentId);
    TFarmTaskGroup *parent = dynamic_cast<TFarmTaskGroup *>(it->second);
    assert(parent);
    parent->removeTask(task);
  }

  TFarmTaskGroup *taskGroup = dynamic_cast<TFarmTaskGroup *>(task);
  if (taskGroup) {
    if (m_controller) {
      // si sta usando la farm
      std::map<QString, QString>::iterator it = m_farmIdsTable.find(id);
      if (it != m_farmIdsTable.end()) {
        m_controller->suspendTask(it->second);
      }
    }

    // rimuove i subtask
    int taskCount = taskGroup->getTaskCount();
    for (int i = 0; i < taskCount; ++i) {
      TFarmTask *subTask = taskGroup->getTask(i);
      assert(subTask);
      m_tasks.erase(subTask->m_id);
    }
  }

  m_tasks.erase(id);
  delete task;
}

//------------------------------------------------------------------------------

namespace {
void DeleteTask(const std::pair<QString, TFarmTask *> &mapItem) {
  if (mapItem.second->m_parentId.isEmpty()) delete mapItem.second;
}
}

void BatchesController::removeAllTasks() {
  std::map<QString, TFarmTask *>::iterator tt, tEnd(m_tasks.end());
  for (tt = m_tasks.begin(); tt != tEnd; ++tt) {
    TFarmTask *task = tt->second;
    if (task->m_status == Running || task->m_status == Waiting) {
      DVGui::warning(taskBusyStr().arg(task->m_name));
      return;
    }
  }

  m_filepath = TFilePath();
  setDirtyFlag(false);

  m_tasks.clear();
  notify();
}

//------------------------------------------------------------------------------

int BatchesController::getTaskCount() const { return m_tasks.size(); }

//------------------------------------------------------------------------------

QString BatchesController::getTaskId(int index) const {
  assert(index >= 0 && index < (int)m_tasks.size());
  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.begin();
  std::advance(it, index);
  return it->first;
}

//------------------------------------------------------------------------------

TFarmTask *BatchesController::getTask(int index) const {
  assert(index >= 0 && index < (int)m_tasks.size());
  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.begin();
  std::advance(it, index);
  return it->second;
}

//------------------------------------------------------------------------------

TFarmTask *BatchesController::getTask(const QString &id) const {
  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.find(id);
  if (it != m_tasks.end()) return it->second;
  return 0;
}

//------------------------------------------------------------------------------

bool BatchesController::getTaskInfo(const QString &id, QString &parent,
                                    QString &name, TaskState &status) {
  TFarmTask *task = getTask(id);
  if (task) {
    parent = task->m_parentId;
    name   = task->m_name;
    status = task->m_status;
  }

  return task != 0;
}

//------------------------------------------------------------------------------

TaskState BatchesController::getTaskStatus(const QString &id) const {
  TFarmTask *task = getTask(id);
  if (task)
    return task->m_status;
  else
    return TaskUnknown;
}

void BatchesController::setDirtyFlag(bool state) {
  static bool FirstTime = true;

  if (FirstTime) {
    FirstTime = false;
    bool ret  = connect(TApp::instance()->getMainWindow(), SIGNAL(exit(bool &)),
                       SLOT(onExit(bool &)));
    assert(ret);
  }

  if (state == m_dirtyFlag) return;

  m_dirtyFlag = state;
  update();
}

//------------------------------------------------------------------------------

void BatchesController::getTasks(const QString &parentId,
                                 std::vector<QString> &tasks) const {
  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    TFarmTask *task = it->second;
    assert(task);
    if (task->m_parentId == parentId) tasks.push_back(task->m_id);
  }
}

//------------------------------------------------------------------------------

void BatchesController::startAll() {
  // viene usata la farm <==> m_controller != 0
  if (!m_controller) {
    // uso una multimap per ordinare rispetto alla priority
    std::multimap<int, TFarmTask *> tasksByPriority;

    std::map<QString, TFarmTask *>::reverse_iterator it = m_tasks.rbegin();
    // uso il reverse iterator anche qui altrimenti se ho task con priorita'
    // uguale li esegue dall'ultimo al primo
    for (; it != m_tasks.rend(); ++it) {
      TFarmTask *task = it->second;
      tasksByPriority.insert(std::make_pair(task->m_priority, task));
    }

    std::multimap<int, TFarmTask *>::reverse_iterator it2 =
        tasksByPriority.rbegin();
    for (; it2 != tasksByPriority.rend(); ++it2) {
      TFarmTask *task = it2->second;
      assert(task);
      task->m_status = Waiting;
      if (task->m_parentId.isEmpty())
        m_localExecutor.addTask(
            new TaskRunner(task, m_localControllerPortNumber));
    }
  } else {
    std::map<QString, TFarmTask *>::const_iterator it = m_tasks.begin();
    for (; it != m_tasks.end(); ++it) {
      TFarmTask *task = it->second;
      assert(task);
      if (task->m_parentId.isEmpty()) start(task->m_id);
    }
  }

  notify();
}

//------------------------------------------------------------------------------

void BatchesController::start(const QString &taskId) {
  QString computerName = QHostInfo().localHostName();

  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.find(taskId);
  assert(it != m_tasks.end());
  TFarmTask *task = it->second;
  assert(task);
  task->m_status      = Waiting;
  task->m_failedSteps = task->m_successfullSteps = 0;
  task->m_completionDate = task->m_startDate = QDateTime();
  task->m_callerMachineName                  = computerName;
  int count                                  = task->getTaskCount();
  if (count > 1)
    for (int i = 0; i < count; ++i) {
      TFarmTask *subtask = task->getTask(i);
      assert(subtask);
      subtask->m_status      = Waiting;
      subtask->m_failedSteps = subtask->m_successfullSteps = 0;
      subtask->m_completionDate = subtask->m_startDate = QDateTime();
      subtask->m_callerMachineName                     = computerName;
    }

  // viene usata la farm <==> m_controller != 0
  if (!m_controller) {
    m_localExecutor.addTask(new TaskRunner(task, m_localControllerPortNumber));
  } else {
    std::map<QString, QString>::iterator ktor = m_farmIdsTable.find(taskId);
    if (ktor != m_farmIdsTable.end()) {
      QString farmTaskId = ktor->second;
      m_controller->restartTask(farmTaskId);

      if (dynamic_cast<TFarmTaskGroup *>(task)) {
        std::vector<QString> subtasks;
        m_controller->getTasks(farmTaskId, subtasks);

        if (!subtasks.empty()) {
          assert((int)subtasks.size() == task->getTaskCount());

          std::vector<QString>::iterator jtor = subtasks.begin();
          for (int i = 0; i < task->getTaskCount(); ++i, ++jtor)
            m_farmIdsTable.insert(
                std::make_pair(task->getTask(i)->m_id, *jtor));
        }
      }
    } else {
      bool suspended = false;
      QString id     = m_controller->addTask(*task, suspended);
      m_farmIdsTable.insert(std::make_pair(task->m_id, id));

      if (dynamic_cast<TFarmTaskGroup *>(task)) {
        std::vector<QString> subtasks;
        m_controller->getTasks(id, subtasks);
        assert((int)subtasks.size() == task->getTaskCount());

        std::vector<QString>::iterator jtor = subtasks.begin();
        for (int i = 0; i < task->getTaskCount(); ++i, ++jtor)
          m_farmIdsTable.insert(std::make_pair(task->getTask(i)->m_id, *jtor));
      }
    }
  }
  notify();
}

//------------------------------------------------------------------------------

void BatchesController::stop(const QString &taskId) {
  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.find(taskId);
  if (it == m_tasks.end()) return;

  TFarmTask *task = it->second;
  assert(task);

  // Update the task status
  if (task->m_status == Waiting) task->m_status = Suspended;

  // Local farm <==> m_controller != 0
  if (!m_controller) {
    QMutexLocker sl(&TasksMutex);
    std::map<QString, QProcess *>::iterator it;

    // Kill the associated process if any
    if ((it = RunningTasks.find(task->m_id)) != RunningTasks.end()) {
      it->second->kill();
      RunningTasks.erase(task->m_id);
    }

    // Do the same for child tasks
    int count = task->getTaskCount();
    if (count > 1) {
      for (int i = 0; i < count; ++i) {
        TFarmTask *subtask                                  = task->getTask(i);
        if (subtask->m_status == Waiting) subtask->m_status = Suspended;
        if ((it = RunningTasks.find(subtask->m_id)) != RunningTasks.end()) {
          it->second->kill();
          RunningTasks.erase(subtask->m_id);
        }
      }
    }
  } else {
    std::map<QString, QString>::iterator it = m_farmIdsTable.find(task->m_id);
    if (it != m_farmIdsTable.end()) {
      QString farmId = it->second;
      m_controller->suspendTask(farmId);
    }
  }
  notify();
}

//------------------------------------------------------------------------------

void BatchesController::stopAll() {
  if (!m_controller) {
    // Stop all running QProcesses
    QMutexLocker sl(&TasksMutex);

    std::map<QString, QProcess *>::iterator it;
    for (it = RunningTasks.begin(); it != RunningTasks.end(); ++it)
      it->second->kill();

    RunningTasks.clear();
    m_localExecutor.cancelAll();
  } else {
    // Suspend all farmIds
    std::map<QString, QString>::iterator it;
    for (it = m_farmIdsTable.begin(); it != m_farmIdsTable.end(); ++it) {
      QString farmId = it->second;
      m_controller->suspendTask(farmId);
    }
  }

  // Update all waiting task status
  std::map<QString, TFarmTask *>::iterator jt;
  for (jt = m_tasks.begin(); jt != m_tasks.end(); ++jt) {
    if (jt->second->m_status == Waiting) jt->second->m_status = Suspended;
  }

  notify();
}

//------------------------------------------------------------------------------

void BatchesController::onExit(bool &ret) {
  int answer = 0;

  if (m_dirtyFlag)
    answer =
        DVGui::MsgBox(QString(tr("The current task list has been modified.\nDo "
                                 "you want to save your changes?")),
                      tr("Save"), tr("Discard"), tr("Cancel"), 1);

  ret = true;
  if (answer == 3)
    ret = false;
  else if (answer == 1)
    save();
}

//------------------------------------------------------------------------------

void BatchesController::load() {
  static LoadTaskListPopup *popup = 0;

  if (!popup) popup = new LoadTaskListPopup();
  popup->exec();
}

//------------------------------------------------------------------------------
void BatchesController::loadTask(bool isRenderTask) {
  static LoadTaskPopup *popup = new LoadTaskPopup();
  popup->open(isRenderTask);
}

//------------------------------------------------------------------------------

void BatchesController::doLoad(const TFilePath &fp) {
  if (m_dirtyFlag) {
    int ret =
        DVGui::MsgBox(QString(tr("The current task list has been modified.\nDo "
                                 "you want to save your changes?")),
                      tr("Save"), tr("Discard"), tr("Cancel"));
    if (ret == 1)
      save();
    else if (ret == 3 || ret == 0)
      return;
  }

  setDirtyFlag(false);

  m_tasks.clear();
  m_farmIdsTable.clear();

  try {
    TIStream is(fp);
    if (is) {
    } else
      throw TException(fp.getWideString() + L": Can't open file");

    m_filepath = fp;

    std::string tagName = "";
    if (!is.matchTag(tagName)) throw TException("Bad file format");

    if (tagName == "tnzbatches") {
      std::string rootTagName = tagName;
      std::string v           = is.getTagAttribute("version");
      while (is.matchTag(tagName)) {
        if (tagName == "generator") {
          std::string program = is.getString();
        } else if (tagName == "batch") {
          while (!is.eos()) {
            TPersist *p = 0;
            is >> p;
            TFarmTask *task = dynamic_cast<TFarmTask *>(p);
            if (task) addTask(task->m_id, task, false);
          }
        } else {
          throw TException(tagName + " : unexpected tag");
        }

        if (!is.matchEndTag()) throw TException(tagName + " : missing end tag");
      }
      if (!is.matchEndTag())
        throw TException(rootTagName + " : missing end tag");
    } else {
      throw TException("Bad file format");
    }
  } catch (TException &e) {
    throw e;
  } catch (...) {
    throw TException("Loading error.");
  }
  update();
  notify();
}

//------------------------------------------------------------------------------

QString BatchesController::getListName() const {
  QString str = (m_filepath == TFilePath()
                     ? tr("Tasks")
                     : QString::fromStdString(m_filepath.getName()));
  return str + (m_dirtyFlag ? "*" : "");
}

//------------------------------------------------------------------------------

void BatchesController::saveas() {
  if (getTaskCount() == 0) {
    DVGui::warning(tr("The Task List is empty!"));
    return;
  }

  static SaveTaskListPopup *popup = 0;
  if (!popup) popup               = new SaveTaskListPopup();

  popup->exec();
}

void BatchesController::save() {
  if (m_filepath.isEmpty())
    saveas();
  else
    doSave();
}

void BatchesController::doSave(const TFilePath &_fp) {
  setDirtyFlag(false);

  TFilePath fp;
  if (_fp == TFilePath())
    fp = m_filepath;
  else
    fp = m_filepath = _fp;

  TOStream os(fp);

  std::map<std::string, string> attr;
  attr["version"] = "1.0";

  os.openChild("tnzbatches", attr);
  os.child("generator") << TEnv::getApplicationFullName();

  os.openChild("batch");
  std::map<QString, TFarmTask *>::const_iterator it = m_tasks.begin();
  for (; it != m_tasks.end(); ++it) {
    TFarmTask *task = it->second;
    assert(task);
    os << task;
  }
  os.closeChild();  // "batch"
  os.closeChild();  // "tnzbatches"

  update();
}

//------------------------------------------------------------------------------

void BatchesController::attach(BatchesController::Observer *obs) {
  m_observers.insert(obs);
}

//------------------------------------------------------------------------------

void BatchesController::detach(BatchesController::Observer *obs) {
  m_observers.erase(obs);
}

//------------------------------------------------------------------------------

namespace {

void notifyObserver(BatchesController::Observer *obs) { obs->update(); }
}

void BatchesController::notify() {
  std::for_each(m_observers.begin(), m_observers.end(), notifyObserver);
}

//------------------------------------------------------------------------------

void BatchesController::setController(TFarmController *controller) {
  m_controller = controller;
}

//------------------------------------------------------------------------------

TFarmController *BatchesController::getController() const {
  return m_controller;
}

//------------------------------------------------------------------------------

namespace {

class ControllerFailureMsg final : public TThread::Message {
public:
  ControllerFailureMsg(const TException &e) : m_e(e) {}

  void onDeliver() override {
    // throw m_e;
  }

  TThread::Message *clone() const override {
    return new ControllerFailureMsg(m_e);
  }

  TException m_e;
};

}  // namespace

//------------------------------------------------------------------------------

void BatchesController::update() {
  if (m_controller) {
    try {
      std::map<QString, QString>::iterator it = m_farmIdsTable.begin();
      for (; it != m_farmIdsTable.end(); ++it) {
        QString farmTaskId    = it->second;
        QString batchesTaskId = it->first;

        TFarmTask *batchesTask = getTask(batchesTaskId);
        TFarmTask farmTask     = *batchesTask;

        if (batchesTask) {
          QString batchesTaskParentId = batchesTask->m_parentId;
          m_controller->queryTaskInfo(farmTaskId, farmTask);
          int chunkSize            = batchesTask->m_chunkSize;
          *batchesTask             = farmTask;
          batchesTask->m_chunkSize = chunkSize;
          batchesTask->m_id        = batchesTaskId;
          batchesTask->m_parentId  = batchesTaskParentId;
        }
      }
    } catch (TException &e) {
      ControllerFailureMsg(e).send();
    }
  }

  std::map<QString, TFarmTask *>::iterator jtor = m_tasks.begin();
  for (; jtor != m_tasks.end(); ++jtor) {
    TFarmTask *task = jtor->second;
    assert(task);
    if (task->m_status == Completed) m_farmIdsTable.erase(task->m_id);
  }

  assert(m_tasksTree);
  m_tasksTree->setupModelData();

  notify();
}

//------------------------------------------------------------------------------

BatchesController *BatchesController::m_instance;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BatchesController::Observer::~Observer() {}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

namespace {

class MyLocalController final : public TFarmController, public TFarmExecutor {
public:
  MyLocalController(int port) : TFarmExecutor(port) {}

  QString execute(const std::vector<QString> &argv) override;

  QString addTask(const TFarmTask &task, bool suspended) override;
  void removeTask(const QString &id) override;
  void suspendTask(const QString &id) override;
  void activateTask(const QString &id) override;
  void restartTask(const QString &id) override;

  void getTasks(vector<QString> &tasks) override;
  void getTasks(const QString &parentId, std::vector<QString> &tasks) override;
  void getTasks(const QString &parentId,
                std::vector<TaskShortInfo> &tasks) override;

  void queryTaskInfo(const QString &id, TFarmTask &task) override;

  void queryTaskShortInfo(const QString &id, QString &parentId, QString &name,
                          TaskState &status) override;

  void attachServer(const QString &name, const QString &addr,
                    int port) override {
    assert(false);
  }

  void detachServer(const QString &name, const QString &addr,
                    int port) override {
    assert(false);
  }

  void taskSubmissionError(const QString &taskId, int errCode) override {
    assert(false);
  }

  void taskProgress(const QString &taskId, int step, int stepCount,
                    int frameNumber, FrameState state) override;

  void taskCompleted(const QString &taskId, int exitCode) override;

  void getServers(vector<ServerIdentity> &servers) override { assert(false); }

  ServerState queryServerState2(const QString &id) override {
    assert(false);
    return ServerUnknown;
  }

  void queryServerInfo(const QString &id, ServerInfo &info) override {
    assert(false);
  }

  void activateServer(const QString &id) override { assert(false); }

  void deactivateServer(const QString &id, bool completeRunningTasks) override {
    assert(false);
  }

private:
  std::map<QString, TFarmTask> m_tasks;
};

QString MyLocalController::execute(const std::vector<QString> &argv) {
  if (argv.size() > 5 && argv[0] == "taskProgress") {
    int step, stepCount, frameNumber;

    fromStr(step, argv[2]);
    fromStr(stepCount, argv[3]);
    fromStr(frameNumber, argv[4]);

    FrameState state;
    fromStr((int &)state, argv[5]);
    taskProgress(argv[1], step, stepCount, frameNumber, state);
    return "";
  } else if (argv.size() > 2 && argv[0] == "taskCompleted") {
    QString taskId = argv[1];
    int exitCode;
    fromStr(exitCode, argv[2]);

    taskCompleted(taskId, exitCode);
    return "";
  }

  return "";
}

QString MyLocalController::addTask(const TFarmTask &task, bool suspended) {
  assert(false);
  return "";
}

void MyLocalController::removeTask(const QString &id) { assert(false); }

void MyLocalController::suspendTask(const QString &id) { assert(false); }

void MyLocalController::activateTask(const QString &id) { assert(false); }

void MyLocalController::restartTask(const QString &id) { assert(false); }

void MyLocalController::getTasks(vector<QString> &tasks) { assert(false); }

void MyLocalController::getTasks(const QString &parentId,
                                 std::vector<QString> &tasks) {
  assert(false);
}

void MyLocalController::getTasks(const QString &parentId,
                                 std::vector<TaskShortInfo> &tasks) {
  assert(false);
}

void MyLocalController::queryTaskInfo(const QString &id, TFarmTask &task) {
  assert(false);
}

void MyLocalController::queryTaskShortInfo(const QString &id, QString &parentId,
                                           QString &name, TaskState &status) {
  assert(false);
}

void MyLocalController::taskProgress(const QString &taskId, int step,
                                     int stepCount, int frameNumber,
                                     FrameState state) {
  TFarmTask *task = BatchesController::instance()->getTask(taskId);
  assert(task);

  if (state == FrameDone)
    ++task->m_successfullSteps;
  else
    ++task->m_failedSteps;

  if (task->m_parentId != "") {
    TFarmTask *taskParent =
        BatchesController::instance()->getTask(task->m_parentId);
    assert(taskParent);
    if (state == FrameDone)
      ++taskParent->m_successfullSteps;
    else
      ++taskParent->m_failedSteps;
  }

  NotifyMessage().send();
}

void MyLocalController::taskCompleted(const QString &taskId, int exitCode) {}

class MyLocalControllerController final : public TThread::Runnable {
  MyLocalController *m_controller;

public:
  MyLocalControllerController(int port)
      : m_controller(new MyLocalController(port)) {
    TThread::Executor executor;
    executor.addTask(this);
    connect(this, SIGNAL(finished(TThread::RunnableP)), this,
            SLOT(onFinished(TThread::RunnableP)));
    connect(this, SIGNAL(exception(TThread::RunnableP)), this,
            SLOT(onFinished(TThread::RunnableP)));
  }

  void run() override { m_controller->run(); }

public slots:

  void onFinished(TThread::RunnableP thisTask) override {
    BatchesController::instance()->notify();
  }
};

}  // anonymous namespace

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

BatchesController *BatchesController::instance() {
  if (!m_instance) {
    m_instance = new BatchesController;

    // scandisce un range di porte fino a troverne una libera da utilizzare come
    // porta del local controller

    const int portRangeSize = 100;

    int i          = 0;
    int portNumber = cPortNumber;
    for (; i < portRangeSize; ++i, ++portNumber) {
      bool portBusy = TFarmStuff::testConnection("localhost", portNumber);
      if (!portBusy) break;
    }

    if (i < portRangeSize) {
      m_instance->m_localControllerPortNumber = portNumber;

      //      static MyLocalControllerController *TheLocalController =
      new MyLocalControllerController(portNumber);
    }
  }
  return m_instance;
}
