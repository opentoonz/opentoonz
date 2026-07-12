

#include "versioncontrol.h"
#include "versioncontrolgui.h"

// ToonzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"

// Toonz includes
#include "toonz/sceneresources.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tproject.h"

// TnzCore includes
#include "tsystem.h"
#include "tenv.h"

// Tnz6 includes
#include "tapp.h"
#include "permissionsmanager.h"

// Qt includes
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QRegularExpression>

namespace {

/**
 * @brief Extracts frame range from a file path pattern.
 *
 * @param path The file path pattern (e.g., "image..png").
 * @param from Output parameter for start frame.
 * @param to Output parameter for end frame.
 * @return true if frame range was found, false otherwise.
 */
bool getFrameRange(const TFilePath& path, unsigned int& from,
                   unsigned int& to) {
  // Check for frame range pattern
  if (path.getDots() == "..") {
    TFilePath dir = path.getParentDir();
    QDir qDir(QString::fromStdWString(dir.getWideString()));

    // Escape level name for regex and create pattern
    QString levelName =
        QRegularExpression::escape(QString::fromStdWString(path.getWideName()));
    QString levelType = QString::fromStdString(path.getType());

    // Create regular expression for frame files
    QString pattern = QString("%1\\.[0-9]{1,4}\\.%2").arg(levelName, levelType);
    QRegularExpression regex(pattern);

    // Get files matching the pattern
    QStringList list        = qDir.entryList(QDir::Files);
    QStringList levelFrames = list.filter(regex);

    // Initialize frame range
    from = static_cast<unsigned int>(-1);
    to   = static_cast<unsigned int>(-1);

    // Determine writable frame range
    for (int i = 0; i < levelFrames.size(); ++i) {
      TFilePath frame = dir + TFilePath(levelFrames[i].toStdWString());

      if (frame.isEmpty() || !frame.isAbsolute()) {
        continue;
      }

      TFileStatus filestatus(frame);
      if (filestatus.isWritable()) {
        if (from == static_cast<unsigned int>(-1)) {
          from = static_cast<unsigned int>(i);
        } else {
          to = static_cast<unsigned int>(i);
        }
      } else if (from != static_cast<unsigned int>(-1) &&
                 to != static_cast<unsigned int>(-1)) {
        break;
      }
    }

    return (from != static_cast<unsigned int>(-1) &&
            to != static_cast<unsigned int>(-1));
  }

  return false;
}

}  // namespace

//=============================================================================
// VersionControlThread
//-----------------------------------------------------------------------------

VersionControlThread::VersionControlThread(QObject* parent)
    : QThread(parent)
    , m_abort(false)
    , m_restart(false)
    , m_getStatus(false)
    , m_readOutputOnDone(true)
    , m_process(nullptr) {}

//-----------------------------------------------------------------------------

VersionControlThread::~VersionControlThread() {
  // Signal thread to abort and wait for completion
  m_mutex.lock();
  m_abort = true;
  m_condition.wakeOne();
  m_mutex.unlock();

  wait();
}

//-----------------------------------------------------------------------------

void VersionControlThread::run() {
  forever {
    m_mutex.lock();
    QString workingDir = m_workingDir;
    QString binary     = m_binary;
    QStringList args   = m_args;

    // Add username and password if available
    VersionControl* vc = VersionControl::instance();
    QString userName   = vc->getUserName();
    QString password   = vc->getPassword();

    if (!userName.isEmpty() || !password.isEmpty()) {
      args << "--username" << userName << "--password" << password;
    }

    // Add global options
    args << "--non-interactive" << "--trust-server-cert";

    // Update binary path if executable path is specified
    QString executablePath = vc->getExecutablePath();
    if (!executablePath.isEmpty()) {
      binary = executablePath + "/" + m_binary;
    }

    m_mutex.unlock();

    // Check for abort
    if (m_abort) {
      cleanupProcess();
      return;
    }

    // Clean up previous process
    cleanupProcess();

    // Create new process
    m_process = new QProcess;

    // Set working directory
    if (!workingDir.isEmpty() && QFile::exists(workingDir)) {
      m_process->setWorkingDirectory(workingDir);
    }

    // Set environment to English locale
    QStringList env = QProcess::systemEnvironment();
    env << "LC_MESSAGES=en_EN";
    m_process->setEnvironment(env);

    // Connect output signal if not reading output at end
    if (!m_readOutputOnDone) {
      connect(m_process, &QProcess::readyReadStandardOutput, this,
              &VersionControlThread::onStandardOutputReady,
              Qt::DirectConnection);
    }

    // Start the process
    m_process->start(binary, args);
    if (!m_process->waitForStarted()) {
      QString err = QString("Unable to launch \"%1\": %2")
                        .arg(binary, m_process->errorString());
      emit error(err);
      return;
    }

    m_process->closeWriteChannel();

    // Wait for process completion with abort checking
    while (!m_process->waitForFinished(1000)) {
      if (m_abort) {
        m_process->kill();
        m_process->waitForFinished();
        return;
      }
    }

    // Check process exit status
    if (m_process->exitStatus() != QProcess::NormalExit) {
      QString err = QString("\"%1\" crashed.").arg(binary);
      emit error(err);
      return;
    }

    // Check exit code
    if (m_process->exitCode()) {
      emit error(QString::fromUtf8(m_process->readAllStandardError().data()));
      return;
    }

    // Emit appropriate signal based on operation type
    if (m_getStatus) {
      emit statusRetrieved(
          QString::fromUtf8(m_process->readAllStandardOutput().data()));
      m_getStatus = false;
    } else {
      emit done(QString::fromUtf8(m_process->readAllStandardOutput().data()));
    }

    // Wait for next command or exit
    m_mutex.lock();
    if (!m_restart) {
      m_condition.wait(&m_mutex);
    }
    m_restart = false;
    m_mutex.unlock();
  }
}

//-----------------------------------------------------------------------------

void VersionControlThread::cleanupProcess() {
  if (m_process) {
    m_process->close();
    disconnect(m_process, &QProcess::readyReadStandardOutput, this,
               &VersionControlThread::onStandardOutputReady);
    delete m_process;
    m_process = nullptr;
  }
}

//-----------------------------------------------------------------------------

void VersionControlThread::onStandardOutputReady() {
  QString str = QString::fromUtf8(m_process->readAllStandardOutput().data());
  if (str.isEmpty()) {
    return;
  }

  emit outputRetrieved(str);
}

//-----------------------------------------------------------------------------

void VersionControlThread::executeCommand(const QString& workingDir,
                                          const QString& binary,
                                          const QStringList& args,
                                          bool readOutputOnDone) {
  QMutexLocker locker(&m_mutex);

  m_readOutputOnDone = readOutputOnDone;
  m_workingDir       = workingDir;
  m_binary           = binary;
  m_args             = args;

  if (m_binary.isEmpty()) {
    return;
  }

  if (!isRunning()) {
    start(QThread::NormalPriority);
  } else {
    m_restart = true;
    m_condition.wakeOne();
  }
}

//-----------------------------------------------------------------------------

void VersionControlThread::getSVNStatus(const QString& path, bool showUpdates,
                                        bool nonRecursive, bool depthInfinity) {
  QMutexLocker locker(&m_mutex);

  m_workingDir     = path;
  m_binary         = "svn";
  QStringList args = {"status"};

  if (showUpdates) {
    args << "-vu";
  } else {
    args << "-v";
  }

  if (nonRecursive) {
    args << "--non-recursive";
  }

  if (depthInfinity) {
    args << "--depth" << "infinity";
  }

  args << "--xml";
  m_args = args;

  m_getStatus        = true;
  m_readOutputOnDone = true;

  if (!isRunning()) {
    start(QThread::NormalPriority);
  } else {
    m_restart = true;
    m_condition.wakeOne();
  }
}

//-----------------------------------------------------------------------------

void VersionControlThread::getSVNStatus(const QString& path,
                                        const QStringList& files,
                                        bool showUpdates, bool nonRecursive,
                                        bool depthInfinity) {
  QMutexLocker locker(&m_mutex);

  m_workingDir     = path;
  m_binary         = "svn";
  QStringList args = {"status"};

  if (showUpdates) {
    args << "-vu";
  } else {
    args << "-v";
  }

  // Add specific files to check
  for (const auto& file : files) {
    if (!file.isEmpty()) {
      args << file;
    }
  }

  if (nonRecursive) {
    args << "--non-recursive";
  }

  if (depthInfinity) {
    args << "--depth" << "infinity";
  }

  args << "--xml";
  m_args = args;

  m_getStatus        = true;
  m_readOutputOnDone = true;

  if (!isRunning()) {
    start(QThread::NormalPriority);
  } else {
    m_restart = true;
    m_condition.wakeOne();
  }
}

//=============================================================================
// VersionControlManager
//-----------------------------------------------------------------------------

VersionControlManager::VersionControlManager()
    : m_scene(nullptr)
    , m_levelSet(nullptr)
    , m_isRunning(false)
    , m_deleteLater(false) {
  // Modern signal/slot connection
  connect(&m_thread, &VersionControlThread::error, this,
          &VersionControlManager::onError);
}

//-----------------------------------------------------------------------------

VersionControlManager* VersionControlManager::instance() {
  static VersionControlManager instance;
  return &instance;
}

//-----------------------------------------------------------------------------

namespace {
/**
 * @brief Sets version control credentials based on repository path.
 *
 * @param currentPath The current file path.
 */
void setVersionControlCredentials(const QString& currentPath) {
  VersionControl* vc = VersionControl::instance();
  auto repositories  = vc->getRepositories();

  for (const auto& repo : repositories) {
    if (currentPath.startsWith(repo.m_localPath)) {
      vc->setUserName(repo.m_username);
      vc->setPassword(repo.m_password);
      return;
    }
  }
}
}  // namespace

//-----------------------------------------------------------------------------

void VersionControlManager::setFrameRange(TLevelSet* levelSet,
                                          bool deleteLater) {
  if (!levelSet || levelSet->getLevelCount() == 0) {
    return;
  }

  if (!m_isRunning) {
    // Reset state
    m_scene       = nullptr;
    m_levelSet    = levelSet;
    m_deleteLater = deleteLater;

    QStringList args         = {"proplist"};
    bool checkVersionControl = false;
    bool filesAddedToArgs    = false;

    // Process each level in the level set
    for (int i = 0; i < levelSet->getLevelCount(); ++i) {
      auto level       = levelSet->getLevel(i);
      auto simpleLevel = level->getSimpleLevel();

      if (simpleLevel && !checkVersionControl) {
        TFilePath parentDir = simpleLevel->getScene()->decodeFilePath(
            simpleLevel->getPath().getParentDir());

        if (VersionControl::instance()->isFolderUnderVersionControl(
                toQString(parentDir))) {
          checkVersionControl = true;
          setVersionControlCredentials(toQString(parentDir));
        }
      }

      if (simpleLevel && simpleLevel->isReadOnly()) {
        if (!m_scene) {
          m_scene = simpleLevel->getScene();
        }

        // Handle different level types
        if (simpleLevel->getType() == PLI_XSHLEVEL ||
            simpleLevel->getType() == TZP_XSHLEVEL) {
          filesAddedToArgs = true;
          args << toQString(m_scene->decodeFilePath(level->getPath()));
        } else if (simpleLevel->getType() == OVL_XSHLEVEL) {
          unsigned int from = 0, to = 0;
          bool ret = getFrameRange(m_scene->decodeFilePath(level->getPath()),
                                   from, to);

          if (ret) {
            simpleLevel->setEditableRange(
                from, to,
                VersionControl::instance()->getUserName().toStdWString());
          }
        }
      }
    }

    // Check if version control is needed
    if (!checkVersionControl || !filesAddedToArgs) {
      cleanupResources();
      return;
    }

    // Prepare SVN command
    args << "--xml" << "-v";
    TFilePath path     = m_scene->getScenePath();
    path               = m_scene->decodeFilePath(path);
    QString workingDir = toQString(path.getParentDir());

    // Connect and execute command
    disconnect(&m_thread, &VersionControlThread::done, this,
               &VersionControlManager::onFrameRangeDone);

    connect(&m_thread, &VersionControlThread::done, this,
            &VersionControlManager::onFrameRangeDone, Qt::QueuedConnection);

    m_thread.executeCommand(workingDir, "svn", args, true);
    m_isRunning = true;
  }
}

//-----------------------------------------------------------------------------

void VersionControlManager::onFrameRangeDone(const QString& text) {
  m_isRunning = false;

  SVNPartialLockReader lockReader(text);
  auto lockList = lockReader.getPartialLock();

  if (lockList.isEmpty()) {
    cleanupResources();
    return;
  }

  // Process lock information
  for (const auto& lock : lockList) {
    TFilePath currentPath(lock.m_fileName.toStdWString());
    setVersionControlCredentials(toQString(currentPath));

    // Find the corresponding level
    TXshSimpleLevel* level = nullptr;
    for (int i = 0; i < m_levelSet->getLevelCount(); ++i) {
      auto l              = m_levelSet->getLevel(i);
      TFilePath levelPath = m_scene->decodeFilePath(l->getPath());

      if (levelPath == currentPath) {
        level = l->getSimpleLevel();
        break;
      }
    }

    if (!level) {
      continue;
    }

    QString username = VersionControl::instance()->getUserName();
    QString hostName = TSystem::getHostName();

    // Find and apply lock information for current user/host
    for (const auto& lockInfo : lock.m_partialLockList) {
      if (lockInfo.m_userName == username && lockInfo.m_hostName == hostName) {
        level->setEditableRange(lockInfo.m_from - 1, lockInfo.m_to - 1,
                                lockInfo.m_userName.toStdWString());
        invalidateIcons(level, level->getEditableRange());

        // Update UI if this level is current
        auto app = TApp::instance();
        if (app->getCurrentLevel()->getLevel() == level) {
          app->getPaletteController()->getCurrentLevelPalette()->setPalette(
              level->getPalette());
          app->getCurrentLevel()->notifyLevelChange();
        }
        break;
      }
    }
  }

  cleanupResources();
}

//-----------------------------------------------------------------------------

void VersionControlManager::cleanupResources() {
  if (m_deleteLater && m_levelSet) {
    for (int i = m_levelSet->getLevelCount() - 1; i >= 0; --i) {
      auto level = m_levelSet->getLevel(i);
      if (auto simpleLevel = level->getSimpleLevel()) {
        simpleLevel->clearEditableRange();
      }
      m_levelSet->removeLevel(level);
    }

    delete m_levelSet;
    m_levelSet    = nullptr;
    m_deleteLater = false;
  }

  m_scene = nullptr;
}

//-----------------------------------------------------------------------------

void VersionControlManager::onError(const QString& text) {
  m_isRunning = false;
  cleanupResources();
  DVGui::warning(text);
}

//=============================================================================
// VersionControl
//-----------------------------------------------------------------------------

VersionControl::VersionControl()
    : m_userName(), m_password(), m_executablePath() {}

//-----------------------------------------------------------------------------

VersionControl* VersionControl::instance() {
  static VersionControl instance;
  return &instance;
}

//-----------------------------------------------------------------------------

void VersionControl::init() {
  QString configFileName = QString::fromStdWString(
      TFilePath(TEnv::getConfigDir() + "versioncontrol.xml").getWideString());

  if (QFile::exists(configFileName)) {
    QFile file(configFileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString content = QString(file.readAll());
      SVNConfigReader reader(content);
      m_repositories   = reader.getRepositories();
      m_executablePath = reader.getSVNPath();
    }
  }
}

//-----------------------------------------------------------------------------

bool VersionControl::testSetup() {
  QString configFileName = QString::fromStdWString(
      TFilePath(TEnv::getConfigDir() + "versioncontrol.xml").getWideString());

  int repositoriesCount = 0;
  QString path;

  // Read configuration file
  if (QFile::exists(configFileName)) {
    QFile file(configFileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString content = QString(file.readAll());
      SVNConfigReader reader(content);
      repositoriesCount = reader.getRepositories().size();
      path              = reader.getSVNPath();
    }
  }

  // Test configuration file
  if (repositoriesCount == 0) {
    DVGui::error(
        tr("The version control configuration file is empty or wrongly "
           "defined.\nPlease refer to the user guide for details."));
    return false;
  }

  // Test if SVN executable exists at specified path
#ifdef MACOSX
  if (!path.isEmpty() && !QFile::exists(path + "/svn"))
#else
  if (!path.isEmpty() && !QFile::exists(path + "/svn.exe"))
#endif
  {
    DVGui::error(tr(
        "The version control client application specified on the configuration "
        "file cannot be found.\nPlease refer to the user guide for details."));
    return false;
  }

  // Test SVN installation and version
  if (path.isEmpty()) {
    QProcess p;

    // Set environment to English locale
    QStringList env = QProcess::systemEnvironment();
    env << "LC_MESSAGES=en_EN";
    p.setEnvironment(env);

    p.start("svn", QStringList("--version"));

    if (!p.waitForStarted()) {
      DVGui::error(
          tr("The version control client application is not installed on your "
             "computer.\nSubversion 1.5 or later is required.\nPlease refer to "
             "the user guide for details."));
      return false;
    }

    if (!p.waitForFinished()) {
      DVGui::error(
          tr("The version control client application is not installed on your "
             "computer.\nSubversion 1.5 or later is required.\nPlease refer to "
             "the user guide for details."));
      return false;
    }

    QString output(p.readAllStandardOutput());
    QStringList list = output.split("\n");

    if (!list.isEmpty()) {
      QString firstLine = list.first();
      // Remove "svn, version 1." prefix to check decimal version
      firstLine = firstLine.remove("svn, version 1.");

      double version = firstLine.left(3).toDouble();
      if (version < 5) {
        DVGui::warning(
            tr("The version control client application installed on your "
               "computer needs to be updated, otherwise some features may not "
               "be available.\nSubversion 1.5 or later is required.\nPlease "
               "refer to the user guide for details."));
        return true;
      }
    }
  }

  return true;
}

//-----------------------------------------------------------------------------

bool VersionControl::isFolderUnderVersionControl(const QString& folderPath) {
  QDir dir(folderPath);

  // Check for .svn directory
  if (dir.entryList(QDir::AllDirs | QDir::Hidden).contains(".svn")) {
    return true;
  }

  // For SVN 1.7+, check parent directories
  while (dir.cdUp()) {
    if (dir.entryList(QDir::AllDirs | QDir::Hidden).contains(".svn")) {
      return true;
    }
  }

  return false;
}

//-----------------------------------------------------------------------------

// Dialog creation methods with modern signal connections

void VersionControl::commit(QWidget* parent, const QString& workingDir,
                            const QStringList& filesToCommit, bool folderOnly,
                            int sceneIconAdded) {
  auto dialog = new SVNCommitDialog(parent, workingDir, filesToCommit,
                                    folderOnly, sceneIconAdded);

  connect(dialog, &SVNCommitDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::revert(QWidget* parent, const QString& workingDir,
                            const QStringList& files, bool folderOnly,
                            int sceneIconAdded) {
  auto dialog = new SVNRevertDialog(parent, workingDir, files, folderOnly,
                                    sceneIconAdded);

  connect(dialog, &SVNRevertDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::update(QWidget* parent, const QString& workingDir,
                            const QStringList& filesToUpdate,
                            int sceneIconsCounts, bool folderOnly,
                            bool updateToRevision, bool nonRecursive) {
  auto dialog =
      new SVNUpdateDialog(parent, workingDir, filesToUpdate, sceneIconsCounts,
                          folderOnly, updateToRevision, nonRecursive);

  connect(dialog, &SVNUpdateDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::updateAndLock(QWidget* parent, const QString& workingDir,
                                   const QStringList& files,
                                   int workingRevision, int sceneIconAdded) {
  auto dialog = new SVNUpdateAndLockDialog(parent, workingDir, files,
                                           workingRevision, sceneIconAdded);

  connect(dialog, &SVNUpdateAndLockDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::lock(QWidget* parent, const QString& workingDir,
                          const QStringList& filesToLock, int sceneIconAdded) {
  auto dialog =
      new SVNLockDialog(parent, workingDir, filesToLock, true, sceneIconAdded);

  connect(dialog, &SVNLockDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::unlock(QWidget* parent, const QString& workingDir,
                            const QStringList& filesToUnlock,
                            int sceneIconAdded) {
  auto dialog = new SVNLockDialog(parent, workingDir, filesToUnlock, false,
                                  sceneIconAdded);

  connect(dialog, &SVNLockDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::lockFrameRange(QWidget* parent, const QString& workingDir,
                                    const QString& file, int frameCount) {
  auto dialog =
      new SVNLockFrameRangeDialog(parent, workingDir, file, frameCount);

  connect(dialog, &SVNLockFrameRangeDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::lockFrameRange(QWidget* parent, const QString& workingDir,
                                    const QStringList& files) {
  auto dialog = new SVNLockMultiFrameRangeDialog(parent, workingDir, files);

  connect(dialog, &SVNLockMultiFrameRangeDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::unlockFrameRange(QWidget* parent,
                                      const QString& workingDir,
                                      const QString& file) {
  auto dialog = new SVNUnlockFrameRangeDialog(parent, workingDir, file);

  connect(dialog, &SVNUnlockFrameRangeDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::unlockFrameRange(QWidget* parent,
                                      const QString& workingDir,
                                      const QStringList& files) {
  auto dialog = new SVNUnlockMultiFrameRangeDialog(parent, workingDir, files);

  connect(dialog, &SVNUnlockMultiFrameRangeDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::showFrameRangeLockInfo(QWidget* parent,
                                            const QString& workingDir,
                                            const QString& file) {
  auto dialog = new SVNFrameRangeLockInfoDialog(parent, workingDir, file);
  dialog->show();
  dialog->raise();
}

void VersionControl::showFrameRangeLockInfo(QWidget* parent,
                                            const QString& workingDir,
                                            const QStringList& files) {
  auto dialog = new SVNMultiFrameRangeLockInfoDialog(parent, workingDir, files);
  dialog->show();
  dialog->raise();
}

void VersionControl::commitFrameRange(QWidget* parent,
                                      const QString& workingDir,
                                      const QString& file) {
  auto dialog = new SVNCommitFrameRangeDialog(parent, workingDir, file);

  connect(dialog, &SVNCommitFrameRangeDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::revertFrameRange(QWidget* parent,
                                      const QString& workingDir,
                                      const QString& file,
                                      const QString& tempFileName) {
  auto dialog =
      new SVNRevertFrameRangeDialog(parent, workingDir, file, tempFileName);

  connect(dialog, &SVNRevertFrameRangeDialog::done, this,
          &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::deleteFiles(QWidget* parent, const QString& workingDir,
                                 const QStringList& filesToDelete,
                                 int sceneIconAdded) {
  auto dialog = new SVNDeleteDialog(parent, workingDir, filesToDelete, false,
                                    sceneIconAdded);

  connect(dialog, &SVNDeleteDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::deleteFolder(QWidget* parent, const QString& workingDir,
                                  const QString& folderName) {
  auto dialog =
      new SVNDeleteDialog(parent, workingDir, QStringList(folderName), true, 0);

  connect(dialog, &SVNDeleteDialog::done, this, &VersionControl::commandDone);
  dialog->show();
  dialog->raise();
}

void VersionControl::cleanupFolder(QWidget* parent, const QString& workingDir) {
  auto dialog = new SVNCleanupDialog(parent, workingDir);
  dialog->show();
  dialog->raise();
}

void VersionControl::purgeFolder(QWidget* parent, const QString& workingDir) {
  auto dialog = new SVNPurgeDialog(parent, workingDir);
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

QStringList VersionControl::getSceneContents(const QString& workingDir,
                                             const QString& sceneFileName) {
  QStringList sceneContents;

  TFilePath scenePath =
      TFilePath(workingDir.toStdWString()) + sceneFileName.toStdWString();

  if (!TFileStatus(scenePath).doesExist()) {
    return sceneContents;
  }

  ToonzScene scene;
  try {
    scene.load(scenePath);
  } catch (...) {
    return sceneContents;
  }

  // Get all levels in the scene
  std::vector<TXshLevel*> levels;
  scene.getLevelSet()->listLevels(levels);

  // Process each level
  for (auto level : levels) {
    TFilePath levelPath = scene.decodeFilePath(level->getPath());

    // Handle frame range files
    if (levelPath.getDots() == "..") {
      TFilePath dir = levelPath.getParentDir();
      QDir qDir(QString::fromStdWString(dir.getWideString()));

      // Create regex pattern for frame files
      QString levelName = QRegularExpression::escape(
          QString::fromStdWString(levelPath.getWideName()));
      QString levelType = QString::fromStdString(levelPath.getType());

      QString pattern =
          QString("%1\\.[0-9]{1,4}\\.%2").arg(levelName, levelType);
      QRegularExpression regex(pattern);

      // Find matching files
      QStringList list = qDir.entryList(QDir::Files);
      list             = list.filter(regex);

      // Add each frame file
      for (const auto& fileName : list) {
        sceneContents.append(toQString(dir + fileName.toStdWString()));
      }
    } else {
      sceneContents.append(toQString(levelPath));
    }

    // Get linked files for the level
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(levelPath, fpset);

    for (const auto& linkedFile : fpset) {
      sceneContents.append(toQString(scene.decodeFilePath(linkedFile)));
    }
  }

  return sceneContents;
}

//-----------------------------------------------------------------------------

QStringList VersionControl::getCurrentSceneContents() const {
  QStringList contents;

  auto scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) {
    return contents;
  }

  // Get all levels in current scene
  std::vector<TXshLevel*> levels;
  scene->getLevelSet()->listLevels(levels);

  // Process each level
  for (auto level : levels) {
    TFilePath levelPath = scene->decodeFilePath(level->getPath());
    contents.append(toQString(levelPath));

    // Get linked files for the level
    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(levelPath, fpset);

    for (const auto& linkedFile : fpset) {
      contents.append(toQString(scene->decodeFilePath(linkedFile)));
    }
  }

  return contents;
}
