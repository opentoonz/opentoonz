#pragma once

#ifndef VERSION_CONTROL_H
#define VERSION_CONTROL_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QProcess>

#include "versioncontrolxmlreader.h"

// Forward Declarations

class TXshSimpleLevel;
class ToonzScene;
class TLevelSet;

//-----------------------------------------------------------------------------

class VersionControlThread final : public QThread {
  Q_OBJECT

  bool m_abort;
  bool m_restart;
  bool m_getStatus;

  bool m_readOutputOnDone;

  QString m_workingDir;
  QString m_binary;
  QStringList m_args;

  QWaitCondition m_condition;

  QProcess *m_process;

public:
  QMutex m_mutex;

  VersionControlThread(QObject *parent = 0);
  ~VersionControlThread();

  void executeCommand(const QString &workingDir, const QString &binary,
                      const QStringList &args, bool readOutputOnDone = true);

  void getSVNStatus(const QString &path, bool showUpdates = false,
                    bool nonRecursive = false, bool depthInfinity = false);
  void getSVNStatus(const QString &path, const QStringList &files,
                    bool showUpdates = false, bool nonRecursive = false,
                    bool depthInfinity = false);

protected:
  void run() override;

protected slots:
  void onStandardOutputReady();

signals:
  void error(const QString &errorString);
  void done(const QString &response);
  void outputRetrieved(const QString &text);
  void statusRetrieved(const QString &xmlResponse);
};

//-----------------------------------------------------------------------------

class VersionControlManager final : public QObject {
  Q_OBJECT

  VersionControlThread m_thread;

  VersionControlManager();

  //! For set Frame Range
  ToonzScene *m_scene;
  TLevelSet *m_levelSet;

  bool m_isRunning;
  bool m_deleteLater;

public:
  static VersionControlManager *instance();
  void setFrameRange(TLevelSet *levelSet, bool deleteLater = false);

protected slots:
  void onFrameRangeDone(const QString &text);
  void onError(const QString &text);
};

//-----------------------------------------------------------------------------

class VersionControl final : public QObject {
  Q_OBJECT

  QString m_userName;
  QString m_password;

  VersionControl();

  QList<SVNRepository> m_repositories;

  QString m_executablePath;

public:
  static VersionControl *instance();

  // Read Version Control repositories from config files
  void init();

  // Check version control version and config file data, return false if there
  // is some setup issue
  bool testSetup();

  bool isFolderUnderVersionControl(const QString &folderPath);

  void setUserName(const QString &userName) { m_userName = userName; }
  QString getUserName() const { return m_userName; }

  void setPassword(const QString &password) { m_password = password; }
  QString getPassword() const { return m_password; }

  // filesToCommit must have relative path to the working dir
  // Convert QStringList to TFilePath
  void commit(QWidget *parent, const QString &workingDir,
              const QStringList &filesToCommit, bool folderOnly,
              int sceneIconAdded = 0);

  void update(QWidget *parent, const QString &workingDir,
              const QStringList &filesToUpdate, int sceneIconsCounts,
              bool folderOnly = true, bool updateToRevision = false,
              bool nonRecursive = false);
  void updateAndLock(QWidget *parent, const QString &workingDir,
                     const QStringList &files, int workingRevision,
                     int sceneIconAdded);

  void revert(QWidget *parent, const QString &workingDir,
              const QStringList &filesToRevert, bool folderOnly,
              int sceneIconAdded = 0);

  void lock(QWidget *parent, const QString &workingDir,
            const QStringList &filesToLock, int sceneIconAdded);
  void unlock(QWidget *parent, const QString &workingDir,
              const QStringList &filesToUnlock, int sceneIconAdded);

  void lockFrameRange(QWidget *parent, const QString &workingDir,
                      const QString &file, int frameCount);
  void lockFrameRange(QWidget *parent, const QString &workingDir,
                      const QStringList &files);

  void unlockFrameRange(QWidget *parent, const QString &workingDir,
                        const QString &file);
  void unlockFrameRange(QWidget *parent, const QString &workingDir,
                        const QStringList &files);

  void showFrameRangeLockInfo(QWidget *parent, const QString &workingDir,
                              const QString &file);
  void showFrameRangeLockInfo(QWidget *parent, const QString &workingDir,
                              const QStringList &files);

  void commitFrameRange(QWidget *parent, const QString &workingDir,
                        const QString &file);

  void revertFrameRange(QWidget *parent, const QString &workingDir,
                        const QString &file, const QString &tempFileName);

  void deleteFiles(QWidget *parent, const QString &workingDir,
                   const QStringList &filesToDelete, int sceneIconAdded = 0);

  void deleteFolder(QWidget *parent, const QString &workingDir,
                    const QString &folderName);

  void cleanupFolder(QWidget *parent, const QString &workingDir);

  void purgeFolder(QWidget *parent, const QString &workingDir);

  // Utility methods
  QStringList getSceneContents(const QString &wokingDir,
                               const QString &sceneFileName);

  QStringList getCurrentSceneContents() const;

  QList<SVNRepository> getRepositories() const { return m_repositories; }

  QString getExecutablePath() const { return m_executablePath; }

signals:
  void commandDone(const QStringList &files);
};

#endif  // VERSION_CONTROL_H
