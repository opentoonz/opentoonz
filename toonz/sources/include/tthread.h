#pragma once

#ifndef TTHREAD_H
#define TTHREAD_H

#include "tsmartpointer.h"

#include <QThread>

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

namespace TThread {

//------------------------------------------------------------------------------

//! Initializes all TThread namespace components.
void DVAPI init();

//! Closes all TThread components as soon as possible in a safe manner.
//! \sa Executor::shutdown() method
void DVAPI shutdown();

//------------------------------------------------------------------------------

// Forward declarations
class ExecutorId;  // Private
class Runnable;

}  // namespace TThread
#ifdef _WIN32
template class DVAPI TSmartPointerT<TThread::Runnable>;
#endif
namespace TThread {

typedef TSmartPointerT<Runnable> RunnableP;

//------------------------------------------------------------------------------

/*!
  Runnable class is the abstract model for user-defined tasks which can be
  scheduled for execution into separate application threads.

  A user must first implement the run() method to provide a task with the
  very execution code. Worker threads created internally by the task manager
will
  take ownership of the task and make it run() at the most appropriate time,
  depending on the task load, insertion time and its scheduling priority.
\n \n
  The scheduling priority of a task can be set by reimplementing the
  \b schedulingPriority() method. Tasks whose scheduling priority is higher are
  always started before compared to the others added by the same executor.
\n \n
  The hosting thread's running priority may also be set reimplementing the
  runningPriority() method; see QThread class documentation in Qt manual.
\n \n
  A task's load is an important property that should be reimplemented in all
  resource-consuming tasks. It enables the user to declare the approximate
  CPU load produced by a task, allowing the task manager to effectively
  calculate the most appropriate moment to run it.

  \warning \n All built-in signals about the Runnable class are emitted in a
  mutex-protected environment in order to make sure that signal emission is
consistent
  with the task status - in other words, so that \a queued controller-emitted
canceled()
  and terminated() signals are not delivered \a before started() or \a after
  finished() and exception() worker-emitted signals.

  \warning Thus, setting up blocking connections or blocking direct slots is \b
not \b
  supported and will typically result in deadlocks; furthermore, the same also
applies to
  slot calls to the Executor API, which would have the aforementioned mutex to
be
  locked again.

  \warning In case the above blocking strategies are mandatory, the user should
add
  custom signals to be emitted inside the mutex-free run() block, just before or
after
  the actual code to be executed, like this:

  \code
  void MyTask::run()
  {
    try
    {
      emit myStarted(this);
      theRunCode();
      emit myFinished(this);
    }
    catch(...)
    {
      emit myException(this);
      throw;
    }
  }
  \endcode
  \code
  ..
  MyTask* myTask = new MyTask;
  connect(myTask, SIGNAL(myStarted(TThread::RunnableP)), myTask,
SLOT(onStarted(TThread::RunnableP)),
    Qt::BlockingQueuedConnection)   //theRunCode() waits for onStarted() to
complete
  ..
  \endcode

  \sa Executor class.
*/
class DVAPI Runnable : public QObject, public TSmartObject {
  Q_OBJECT
  DECLARE_CLASS_CODE

  ExecutorId *m_id;

  int m_load;
  int m_schedulingPriority;

  friend class Executor;     // Needed to confront Executor's and Runnable's ids
  friend class ExecutorImp;  // The internal task manager needs full control
                             // over the task
  friend class Worker;       // Workers force tasks to emit state signals

public:
  Runnable();
  virtual ~Runnable();

  //! The central code of the task that is executed by a worker thread.
  virtual void run() = 0;

  virtual int taskLoad();
  virtual int schedulingPriority();
  virtual QThread::Priority runningPriority();

Q_SIGNALS:

  void started(TThread::RunnableP sender);
  void finished(TThread::RunnableP sender);
  void exception(TThread::RunnableP sender);
  void canceled(TThread::RunnableP sender);
  void terminated(TThread::RunnableP sender);

public Q_SLOTS:

  virtual void onStarted(TThread::RunnableP sender);
  virtual void onFinished(TThread::RunnableP sender);
  virtual void onException(TThread::RunnableP sender);
  virtual void onCanceled(TThread::RunnableP sender);
  virtual void onTerminated(TThread::RunnableP sender);

private:
  inline bool needsAccumulation();
  inline bool customConditions();
};

//------------------------------------------------------------------------------

/*!
  Executor class provides an effective way for planning the execution of
  user-defined tasks that require separate working threads.

  When an application needs to perform a resource-consuming task, it is often
  a good idea to dedicate a separate thread for it, especially in GUI
applications;
  however, doing so eventually raises the problem of managing such intensive
  tasks in a way that constantly ensures the correct use of the machine
resources -
  so that in any given time the CPU usage is maximal, but not overloaded.
  Additional requests by the user may arise, including preferenced ordering
  among tasks, the necessity of salvaging some CPU resources or threads for
  incoming tasks, and so on.
\n \n
  The TThread namespace API contains two main classes, \b Runnable and \b
Executor,
  which provide a way for implementing consistent threading strategies.
\n \n
  In order to use the Executor class it is first necessary to install the thread
manager
  into application code by calling the static method init() appropriately.
\n \n
  Executors are then used to submit - or eventually remove - tasks for execution
into a
  separate working thread, by means of the \b addTask(), \b removeTask() and \b
cancelAll() methods.
  Each Executor's visibility is always limited to the tasks that it submits -
  so calling removeTask() or cancelAll() only affects the tasks previously
  added by that same Executor - which easily reflects the idea of an Executor
representing
  a group of tasks.
\n \n
  Basic control over the execution strategy for the group of tasks submitted by
an Executor
  can be acquired using the setMaxActiveTasks() and setMaxActiveLoad() methods,
both granting
  the possibility to bound the execution of tasks to custom maximum conditions.
  For example, use setMaxActiveTasks(1) to force the execution of 1 task only at
a time,
  or setMaxActiveLoad(100) to set a single CPU core available for the group.

  \sa \b Runnable class documentation.
*/
class DVAPI Executor {
  ExecutorId *m_id;

  friend class ExecutorImp;

public:
  Executor();
  ~Executor();

  static void init();
  static void shutdown();

  void addTask(RunnableP task);
  void removeTask(RunnableP task);
  void cancelAll();

  void setMaxActiveTasks(int count);
  void setMaxActiveLoad(int load);

  int maxActiveTasks() const;
  int maxActiveLoad() const;

  void setDedicatedThreads(bool dedicated, bool persistent = true);

private:
  // not implemented
  Executor &operator=(const Executor &);
  Executor(const Executor &);
};

}  // namespace TThread

#endif  // TTHREAD_H
