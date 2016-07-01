#pragma once

#ifndef TFARMTASK_H
#define TFARMTASK_H

#include <memory>

#include <QDateTime>
#include "tpersist.h"
#include "tfarmplatforms.h"
#include "tfilepath.h"
#ifdef TFARMAPI

#undef TFARMAPI
#endif

#ifdef _WIN32
#ifdef TFARM_EXPORTS
#define TFARMAPI __declspec(dllexport)
#else
#define TFARMAPI __declspec(dllimport)
#endif
#else
#define TFARMAPI
#endif

//------------------------------------------------------------------------------

const int cPortNumber = 51005;

enum TaskState { Suspended, Waiting, Running, Completed, Aborted, TaskUnknown };

enum FrameState { FrameDone, FrameFailed };

enum OverwriteBehavior { Overwrite_All = 0, Overwrite_NoPaint, Overwrite_Off };

#define RENDER_LICENSE_NOT_FOUND 888

//------------------------------------------------------------------------------

class TFARMAPI TFarmTask : public TPersist {
public:
  typedef QString Id;

  class TFARMAPI Dependencies {
  public:
    Dependencies();
    ~Dependencies();

    Dependencies(const Dependencies &);
    Dependencies &operator=(const Dependencies &);

    bool operator==(const Dependencies &);
    bool operator!=(const Dependencies &);

    void add(const QString &id);
    void remove(const QString &id);

    int getTaskCount() const;
    QString getTaskId(int i) const;

  private:
    class Data;
    Data *m_data;
  };

public:
  Id m_id;        //!< Internal task identifier
  Id m_parentId;  //!< Task id of the parent task (if any)

  bool m_isComposerTask;  //!< Whether this is a tcomposer task (opposed to
                          //! tcleanupper task)

  QString m_name;               //!< User-readable name
  TFilePath m_taskFilePath;     //!< Path of the input file affected by the task
  TFilePath m_outputPath;       //!< Path of the task's output file
  QString m_callerMachineName;  //!< Name of the... caller (submitter?) machine

  int m_priority;  //!< Priority value used for scheduling order

  QString m_user;      //!< User who submitted the task
  QString m_hostName;  //!< Machine on which the task was submitted

  TaskState
      m_status;      //!< The task's status (completed, work in progress... etc)
  QString m_server;  //!< Server node (name) the task was supplied to

  QDateTime m_submissionDate;
  QDateTime m_startDate;
  QDateTime m_completionDate;

  int m_successfullSteps;  //
  int m_failedSteps;       //
  int m_stepCount;  // Why 3 values ? One should be found with the other 2!

  int m_from, m_to, m_step, m_shrink;  //!< Range data
  int m_chunkSize;                     //!< Sub-tasks size

  int m_multimedia;
  int m_threadsIndex;
  int m_maxTileSizeIndex;

  OverwriteBehavior m_overwrite;
  bool m_onlyVisible;

  TFarmPlatform m_platform;

  Dependencies *m_dependencies;

public:
  TFarmTask(const QString &name = "");

  TFarmTask(const QString &id, const QString &name, const QString &cmdline,
            const QString &user, const QString &host, int stepCount,
            int priority);

  TFarmTask(const QString &id, const QString &name, bool composerTask,
            const QString &user, const QString &host, int stepCount,
            int priority, const TFilePath &taskFilePath,
            const TFilePath &outputPath, int from, int to, int step, int shrink,
            int multimedia, int chunksize, int threadsIndex,
            int maxTileSizeIndex, OverwriteBehavior overwrite,
            bool onlyvisible);

  virtual ~TFarmTask() { delete m_dependencies; }

  TFarmTask(const TFarmTask &);
  TFarmTask &operator=(const TFarmTask &);

  bool operator==(const TFarmTask &task);
  bool operator!=(const TFarmTask &task);

  virtual int getTaskCount() const { return 1; }
  virtual TFarmTask *getTask(int index) { return this; }

  QString getCommandLine(bool isFarmTask = false) const;
  void parseCommandLine(QString commandLine);

  // TPersist
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  const TPersistDeclaration *getDeclaration() const override;
};

//------------------------------------------------------------------------------

class TFARMAPI TFarmTaskGroup final : public TFarmTask {
public:
  TFarmTaskGroup();

  TFarmTaskGroup(const QString &id, const QString &name, const QString &user,
                 const QString &host, int stepCount, int priority,
                 const TFilePath &taskFilePath, const TFilePath &outputPath,
                 int from, int to, int step, int shrink, int multimedia,
                 int chunksize, int threadsIndex, int maxTileSizeIndex);

  TFarmTaskGroup(const QString &id, const QString &name, const QString &user,
                 const QString &host, int stepCount, int priority,
                 const TFilePath &taskFilePath, OverwriteBehavior overwrite,
                 bool onlyvisible);

  TFarmTaskGroup(const QString &id, const QString &name, const QString &cmdline,
                 const QString &user, const QString &host, int stepCount,
                 int priority);

  TFarmTaskGroup(const TFarmTaskGroup &src);

  ~TFarmTaskGroup();

  void addTask(TFarmTask *task);
  void removeTask(TFarmTask *task);

  int getTaskCount() const override;
  TFarmTask *getTask(int index) override;
  bool changeChunkSize(int chunksize);

  // TPersist
  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;
  const TPersistDeclaration *getDeclaration() const override;

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;
};

#endif
