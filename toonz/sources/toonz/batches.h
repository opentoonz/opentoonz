#pragma once

#ifndef BATCHES_H
#define BATCHES_H

#include <QObject>
#include <map>
#include <set>
#include <vector>
#include <QString>
#include "tfarmtask.h"
#include "tfilepath.h"
#include "tthread.h"
#include "filebrowserpopup.h"
using std::map;
using std::set;
using std::vector;

class TFarmController;

//------------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4786)
#endif

//------------------------------------------------------------------------------
class TaskTreeModel;

class BatchesController final : public QObject {  // singleton
  Q_OBJECT
public:
  static BatchesController *instance();

  void setDirtyFlag(bool state);
  QString getListName() const;
  void addComposerTask(const TFilePath &taskFilePath);
  void addCleanupTask(const TFilePath &taskFilePath);

  void addTask(const QString &id, TFarmTask *task, bool doUpdate = true);
  void removeTask(const QString &id);
  void removeAllTasks();

  int getTaskCount() const;
  QString getTaskId(int index) const;
  TFarmTask *getTask(int index) const;
  TFarmTask *getTask(const QString &id) const;
  bool getTaskInfo(const QString &id, QString &parent, QString &name,
                   TaskState &status);
  TaskState getTaskStatus(const QString &id) const;
  void loadTask(bool isRenderTask);

  void getTasks(const QString &parentId, std::vector<QString> &tasks) const;
  void setTasksTree(TaskTreeModel *tree);

  TaskTreeModel *getTasksTree() { return m_tasksTree; }

  void startAll();
  void start(const QString &taskId);
  void stopAll();
  void stop(const QString &taskId);

  void load();
  void doLoad(const TFilePath &fp);
  void save();
  void saveas();
  void doSave(const TFilePath &fp = TFilePath());

  class Observer {
  public:
    virtual ~Observer();
    virtual void update() = 0;
  };

  void attach(Observer *obs);
  void detach(Observer *obs);
  void notify();

  void setController(TFarmController *controller);
  TFarmController *getController() const;
  void update();  // aggiorna lo stato del Batch interrogando il FarmController

public slots:
  void onExit(bool &);

protected:
  BatchesController();
  ~BatchesController();

  // non implementati
  BatchesController(const BatchesController &);
  void operator=(const BatchesController &);

private:
  bool m_dirtyFlag;
  TFilePath m_filepath;
  std::map<QString, TFarmTask *> m_tasks;
  set<Observer *> m_observers;
  int m_localControllerPortNumber;
  TFarmController *m_controller;
  std::map<QString, QString> m_farmIdsTable;
  TaskTreeModel *m_tasksTree;
  TThread::Executor m_localExecutor;

  static BatchesController *m_instance;

private:
  static inline QString taskBusyStr();
};
//-----------------------------------------------------------------------------
class LoadTaskListPopup final : public FileBrowserPopup {
  Q_OBJECT

public:
  LoadTaskListPopup();

  bool execute() override;
};

//-----------------------------------------------------------------------------

class LoadTaskPopup final : public FileBrowserPopup {
  Q_OBJECT

  bool m_isRenderTask;

public:
  LoadTaskPopup();

  bool execute() override;
  void open(bool isRenderTask);
};

//-----------------------------------------------------------------------------

class SaveTaskListPopup final : public FileBrowserPopup {
  Q_OBJECT

public:
  SaveTaskListPopup();

  bool execute() override;
};

//------------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
