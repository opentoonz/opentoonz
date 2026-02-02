#pragma once

#ifndef VERSION_CONTROL_H
#define VERSION_CONTROL_H

// Qt includes
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QProcess>
#include <QStringList>

// Version control includes
#include "versioncontrolxmlreader.h"

// Forward Declarations
class TXshSimpleLevel;
class ToonzScene;
class TLevelSet;
class QWidget;

//-----------------------------------------------------------------------------
/**
 * @brief Thread for executing version control commands asynchronously.
 *
 * This class provides a worker thread for executing Subversion (SVN) commands
 * in the background, allowing the UI to remain responsive during long-running
 * operations.
 */
class VersionControlThread final : public QThread {
  Q_OBJECT

public:
  /**
   * @brief Constructs a version control thread.
   *
   * @param parent Parent QObject.
   */
  explicit VersionControlThread(QObject* parent = nullptr);

  /**
   * @brief Destructor that ensures thread termination.
   */
  ~VersionControlThread() override;

  /**
   * @brief Executes a version control command.
   *
   * @param workingDir Working directory for the command.
   * @param binary Command binary to execute.
   * @param args Command arguments.
   * @param readOutputOnDone Whether to read output after command completion.
   */
  void executeCommand(const QString& workingDir, const QString& binary,
                      const QStringList& args, bool readOutputOnDone = true);

  /**
   * @brief Gets SVN status for a path.
   *
   * @param path Path to check status for.
   * @param showUpdates Whether to show updates.
   * @param nonRecursive Whether to perform non-recursive operation.
   * @param depthInfinity Whether to use infinite depth.
   */
  void getSVNStatus(const QString& path, bool showUpdates = false,
                    bool nonRecursive = false, bool depthInfinity = false);

  /**
   * @brief Gets SVN status for specific files.
   *
   * @param path Working directory.
   * @param files List of files to check.
   * @param showUpdates Whether to show updates.
   * @param nonRecursive Whether to perform non-recursive operation.
   * @param depthInfinity Whether to use infinite depth.
   */
  void getSVNStatus(const QString& path, const QStringList& files,
                    bool showUpdates = false, bool nonRecursive = false,
                    bool depthInfinity = false);

  /**
   * @brief Requests thread termination.
   */
  void requestAbort();

private:
  /**
   * @brief Main thread execution loop.
   */
  void run() override;

  /**
   * @brief Cleans up the QProcess instance.
   */
  void cleanupProcess();

private slots:
  /**
   * @brief Handles standard output availability.
   */
  void onStandardOutputReady();

signals:
  /**
   * @brief Emitted when an error occurs.
   *
   * @param errorString Error description.
   */
  void error(const QString& errorString);

  /**
   * @brief Emitted when command execution completes.
   *
   * @param response Command response.
   */
  void done(const QString& response);

  /**
   * @brief Emitted when output is retrieved during execution.
   *
   * @param text Output text.
   */
  void outputRetrieved(const QString& text);

  /**
   * @brief Emitted when status retrieval completes.
   *
   * @param xmlResponse XML status response.
   */
  void statusRetrieved(const QString& xmlResponse);

private:
  // Thread control
  bool m_abort            = false;
  bool m_restart          = false;
  bool m_getStatus        = false;
  bool m_readOutputOnDone = true;

  // Command parameters
  QString m_workingDir;
  QString m_binary;
  QStringList m_args;

  // Synchronization
  QMutex m_mutex;
  QWaitCondition m_condition;

  // Process
  QProcess* m_process = nullptr;
};

//-----------------------------------------------------------------------------
/**
 * @brief Manager for version control frame range operations.
 *
 * Handles setting and managing frame ranges for levels under version control,
 * including lock/unlock operations for specific frame ranges.
 */
class VersionControlManager final : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Gets the singleton instance.
   *
   * @return VersionControlManager instance.
   */
  static VersionControlManager* instance();

  /**
   * @brief Sets frame range for levels in a level set.
   *
   * @param levelSet Level set to process.
   * @param deleteLater Whether to delete level set after processing.
   */
  void setFrameRange(TLevelSet* levelSet, bool deleteLater = false);

private:
  /**
   * @brief Private constructor for singleton pattern.
   */
  VersionControlManager();

  /**
   * @brief Cleans up resources.
   */
  void cleanupResources();

private slots:
  /**
   * @brief Handles completion of frame range operation.
   *
   * @param text Response text.
   */
  void onFrameRangeDone(const QString& text);

  /**
   * @brief Handles version control errors.
   *
   * @param text Error text.
   */
  void onError(const QString& text);

private:
  // Thread for async operations
  VersionControlThread m_thread;

  // Scene and level set being processed
  ToonzScene* m_scene   = nullptr;
  TLevelSet* m_levelSet = nullptr;

  // State
  bool m_isRunning   = false;
  bool m_deleteLater = false;
};

//-----------------------------------------------------------------------------
/**
 * @brief Main version control interface.
 *
 * Provides access to version control operations and manages repositories
 * and user credentials.
 */
class VersionControl final : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Gets the singleton instance.
   *
   * @return VersionControl instance.
   */
  static VersionControl* instance();

  /**
   * @brief Initializes version control from configuration file.
   */
  void init();

  /**
   * @brief Tests version control setup.
   *
   * @return true if setup is valid, false otherwise.
   */
  bool testSetup();

  /**
   * @brief Checks if a folder is under version control.
   *
   * @param folderPath Path to check.
   * @return true if folder is under version control.
   */
  bool isFolderUnderVersionControl(const QString& folderPath);

  // User credential management
  void setUserName(const QString& userName) { m_userName = userName; }
  QString getUserName() const { return m_userName; }

  void setPassword(const QString& password) { m_password = password; }
  QString getPassword() const { return m_password; }

  // Repository management
  QList<SVNRepository> getRepositories() const { return m_repositories; }
  QString getExecutablePath() const { return m_executablePath; }

  // File operations
  void commit(QWidget* parent, const QString& workingDir,
              const QStringList& filesToCommit, bool folderOnly,
              int sceneIconAdded = 0);

  void update(QWidget* parent, const QString& workingDir,
              const QStringList& filesToUpdate, int sceneIconsCounts,
              bool folderOnly = true, bool updateToRevision = false,
              bool nonRecursive = false);

  void updateAndLock(QWidget* parent, const QString& workingDir,
                     const QStringList& files, int workingRevision,
                     int sceneIconAdded);

  void revert(QWidget* parent, const QString& workingDir,
              const QStringList& filesToRevert, bool folderOnly,
              int sceneIconAdded = 0);

  // Lock operations
  void lock(QWidget* parent, const QString& workingDir,
            const QStringList& filesToLock, int sceneIconAdded);

  void unlock(QWidget* parent, const QString& workingDir,
              const QStringList& filesToUnlock, int sceneIconAdded);

  // Frame range operations
  void lockFrameRange(QWidget* parent, const QString& workingDir,
                      const QString& file, int frameCount);

  void lockFrameRange(QWidget* parent, const QString& workingDir,
                      const QStringList& files);

  void unlockFrameRange(QWidget* parent, const QString& workingDir,
                        const QString& file);

  void unlockFrameRange(QWidget* parent, const QString& workingDir,
                        const QStringList& files);

  void showFrameRangeLockInfo(QWidget* parent, const QString& workingDir,
                              const QString& file);

  void showFrameRangeLockInfo(QWidget* parent, const QString& workingDir,
                              const QStringList& files);

  void commitFrameRange(QWidget* parent, const QString& workingDir,
                        const QString& file);

  void revertFrameRange(QWidget* parent, const QString& workingDir,
                        const QString& file, const QString& tempFileName);

  // Deletion operations
  void deleteFiles(QWidget* parent, const QString& workingDir,
                   const QStringList& filesToDelete, int sceneIconAdded = 0);

  void deleteFolder(QWidget* parent, const QString& workingDir,
                    const QString& folderName);

  // Maintenance operations
  void cleanupFolder(QWidget* parent, const QString& workingDir);
  void purgeFolder(QWidget* parent, const QString& workingDir);

  // Utility methods
  QStringList getSceneContents(const QString& workingDir,
                               const QString& sceneFileName);

  QStringList getCurrentSceneContents() const;

signals:
  /**
   * @brief Emitted when a version control command completes.
   *
   * @param files List of files affected by the command.
   */
  void commandDone(const QStringList& files);

private:
  /**
   * @brief Private constructor for singleton pattern.
   */
  VersionControl();

  // User credentials
  QString m_userName;
  QString m_password;

  // Repository configuration
  QList<SVNRepository> m_repositories;
  QString m_executablePath;
};

#endif  // VERSION_CONTROL_H
