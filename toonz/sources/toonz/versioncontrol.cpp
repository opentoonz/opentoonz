

#include "versioncontrol.h"
#include "versioncontrolgui.h"

#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"
#include "toonz/sceneresources.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tscenehandle.h"
#include "tsystem.h"
#include "tenv.h"
#include "tapp.h"

#include <QFile>
#include <QDir>
#include <QSettings>
#include <QRegExp>

namespace {

bool getFrameRange(const TFilePath &path, unsigned int &from,
                   unsigned int &to) {
  // Set frame range
  if (path.getDots() == "..") {
    TFilePath dir = path.getParentDir();
    QDir qDir(QString::fromStdWString(dir.getWideString()));
    QString levelName =
        QRegExp::escape(QString::fromStdWString(path.getWideName()));
    QString levelType = QString::fromStdString(path.getType());
    QString exp(levelName + ".[0-9]{1,4}." + levelType);
    QRegExp regExp(exp);
    QStringList list        = qDir.entryList(QDir::Files);
    QStringList levelFrames = list.filter(regExp);

    from = -1;
    to   = -1;
    for (int i = 0; i < levelFrames.size(); i++) {
      TFilePath frame = dir + TFilePath(levelFrames[i].toStdWString());
      if (frame.isEmpty() || !frame.isAbsolute()) continue;
      TFileStatus filestatus(frame);
      if (filestatus.isWritable()) {
        if (from == -1)
          from = i;
        else
          to = i;
      } else if (from != -1 && to != -1)
        break;
    }

    if (from != -1 && to != -1)
      return true;
    else
      return false;
  }
  return false;
}

}  // namespace

//=============================================================================
// VersionControlThread
//-----------------------------------------------------------------------------

VersionControlThread::VersionControlThread(QObject *parent)
    : QThread(parent)
    , m_abort(false)
    , m_restart(false)
    , m_getStatus(false)
    , m_readOutputOnDone(true)
    , m_process(0) {}

//-----------------------------------------------------------------------------

VersionControlThread::~VersionControlThread() {
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

    // Add Username and Password (if exists)

    VersionControl *vc = VersionControl::instance();

    QString userName = vc->getUserName();
    QString password = vc->getPassword();

    if (!userName.isEmpty() || !password.isEmpty()) {
      args << QString("--username") << userName << QString("--password")
           << password;
    }

    // Add "non-interactive" and "trust-server-cert" global options
    args << "--non-interactive";
    args << "--trust-server-cert";

    QString executablePath                = vc->getExecutablePath();
    if (!executablePath.isEmpty()) binary = executablePath + "/" + m_binary;

    m_mutex.unlock();

    if (m_abort) {
      if (m_process) {
        m_process->close();
        disconnect(m_process, SIGNAL(readyReadStandardOutput()), this,
                   SLOT(onStandardOutputReady()));
        delete m_process;
        m_process = 0;
      }
      return;
    }

    if (m_process) {
      m_process->close();
      disconnect(m_process, SIGNAL(readyReadStandardOutput()), this,
                 SLOT(onStandardOutputReady()));
      delete m_process;
      m_process = 0;
    }

    m_process = new QProcess;
    if (!workingDir.isEmpty() && QFile::exists(workingDir))
      m_process->setWorkingDirectory(workingDir);

    QStringList env = QProcess::systemEnvironment();
    env << "LC_MESSAGES=en_EN";  // Add this environment variables to fix the
                                 // language to English
    m_process->setEnvironment(env);

    if (!m_readOutputOnDone)
      connect(m_process, SIGNAL(readyReadStandardOutput()), this,
              SLOT(onStandardOutputReady()));

    m_process->start(binary, args);
    if (!m_process->waitForStarted()) {
      QString err = QString("Unable to launch \"%1\": %2")
                        .arg(binary, m_process->errorString());
      emit error(err);
      return;
    }

    m_process->closeWriteChannel();

    // Wait until users press cancel (every 1 sec I check for abort..
    while (!m_process->waitForFinished(1000)) {
      if (m_abort) {
        m_process->kill();
        m_process->waitForFinished();
        return;
      }
    }

    if (m_process->exitStatus() != QProcess::NormalExit) {
      QString err = QString("\"%1\" crashed.").arg(binary);
      emit error(err);
      return;
    }

    if (m_process->exitCode()) {
      emit error(QString::fromUtf8(m_process->readAllStandardError().data()));
      return;
    }

    if (m_getStatus) {
      emit statusRetrieved(
          QString::fromUtf8(m_process->readAllStandardOutput().data()));
      m_getStatus = false;
    } else
      emit done(QString::fromUtf8(m_process->readAllStandardOutput().data()));

    m_mutex.lock();
    if (!m_restart) m_condition.wait(&m_mutex);
    m_restart = false;
    m_mutex.unlock();
  }
}

//-----------------------------------------------------------------------------

void VersionControlThread::onStandardOutputReady() {
  QString str = QString::fromUtf8(m_process->readAllStandardOutput().data());
  if (str.isEmpty()) return;
  emit outputRetrieved(str);
}

//-----------------------------------------------------------------------------

void VersionControlThread::executeCommand(const QString &workingDir,
                                          const QString &binary,
                                          const QStringList &args,
                                          bool readOutputOnDone) {
  QMutexLocker locker(&m_mutex);

  m_readOutputOnDone = readOutputOnDone;
  m_workingDir       = workingDir;
  m_binary           = binary;
  m_args             = args;

  if (m_binary.isEmpty()) return;

  if (!isRunning()) {
    start(QThread::NormalPriority);
  } else {
    m_restart = true;
    m_condition.wakeOne();
  }
}

//-----------------------------------------------------------------------------

void VersionControlThread::getSVNStatus(const QString &path, bool showUpdates,
                                        bool nonRecursive, bool depthInfinity) {
  QMutexLocker locker(&m_mutex);

  m_workingDir = path;
  m_binary     = QString("svn");
  QStringList args;
  args << "status";
  if (showUpdates)
    args << "-vu";
  else
    args << "-v";
  if (nonRecursive) args << "--non-recursive";
  if (depthInfinity)
    args << "--depth"
         << "infinity";

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

void VersionControlThread::getSVNStatus(const QString &path,
                                        const QStringList &files,
                                        bool showUpdates, bool nonRecursive,
                                        bool depthInfinity) {
  QMutexLocker locker(&m_mutex);
  m_workingDir = path;
  m_binary     = QString("svn");
  QStringList args;
  args << "status";
  if (showUpdates)
    args << "-vu";
  else
    args << "-v";

  int filesCount = files.size();
  for (int i = 0; i < filesCount; i++) {
    QString f = files.at(i);
    if (!f.isEmpty()) args << f;
  }
  if (nonRecursive) args << "--non-recursive";
  if (depthInfinity)
    args << "--depth"
         << "infinity";

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
    : m_scene(0), m_levelSet(0), m_isRunning(false), m_deleteLater(false) {
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onError(const QString &)));
}

//-----------------------------------------------------------------------------

VersionControlManager *VersionControlManager::instance() {
  static VersionControlManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void VersionControlManager::setFrameRange(TLevelSet *levelSet,
                                          bool deleteLater) {
  if (levelSet->getLevelCount() == 0) return;

  if (!m_isRunning) {
    m_scene       = 0;  // Just to be sure
    m_levelSet    = levelSet;
    m_deleteLater = deleteLater;

    QStringList args;
    args << "propget";
    args << "partial-lock";

    bool checkVersionControl = false;
    bool filesAddedToArgs    = false;

    for (int i = 0; i < levelSet->getLevelCount(); i++) {
      TXshLevel *level    = levelSet->getLevel(i);
      TXshSimpleLevel *sl = level->getSimpleLevel();

      if (sl && !checkVersionControl) {
        TFilePath parentDir =
            sl->getScene()->decodeFilePath(sl->getPath().getParentDir());
        if (VersionControl::instance()->isFolderUnderVersionControl(
                toQString(parentDir)))
          checkVersionControl = true;
      }

      if (sl && sl->isReadOnly()) {
        if (!m_scene) m_scene = sl->getScene();

        if (sl->getType() == PLI_XSHLEVEL || sl->getType() == TZP_XSHLEVEL) {
          filesAddedToArgs = true;
          args << toQString(m_scene->decodeFilePath(level->getPath()));
        } else if (sl->getType() == OVL_XSHLEVEL) {
          unsigned int from;
          unsigned int to;
          bool ret = getFrameRange(m_scene->decodeFilePath(level->getPath()),
                                   from, to);
          if (ret)
            sl->setEditableRange(
                from, to,
                VersionControl::instance()->getUserName().toStdWString());
        }
      }
    }

    if (!checkVersionControl || !filesAddedToArgs) {
      if (m_deleteLater) {
        m_deleteLater = false;
        for (int i = m_levelSet->getLevelCount() - 1; i >= 0; i--) {
          TXshLevel *l = m_levelSet->getLevel(i);
          m_levelSet->removeLevel(l);
        }
        delete m_levelSet;
        m_levelSet = 0;
      }
      m_scene = 0;
      return;
    }

    args << "--xml";

    TFilePath path     = m_scene->getScenePath();
    path               = m_scene->decodeFilePath(path);
    QString workingDir = toQString(path.getParentDir());

    m_thread.disconnect(SIGNAL(done(const QString &)));
    connect(&m_thread, SIGNAL(done(const QString &)), this,
            SLOT(onFrameRangeDone(const QString)));
    m_thread.executeCommand(workingDir, "svn", args, true);
    m_isRunning = true;
  }
}

//-----------------------------------------------------------------------------

void VersionControlManager::onFrameRangeDone(const QString &text) {
  m_isRunning = false;

  SVNPartialLockReader lockReader(text);
  QList<SVNPartialLock> list = lockReader.getPartialLock();
  if (list.isEmpty()) {
    if (m_deleteLater) {
      m_deleteLater = false;

      for (int i = m_levelSet->getLevelCount() - 1; i >= 0; i--) {
        TXshLevel *l        = m_levelSet->getLevel(i);
        TXshSimpleLevel *sl = l->getSimpleLevel();
        if (sl) sl->clearEditableRange();
        m_levelSet->removeLevel(l);
      }

      delete m_levelSet;
      m_levelSet = 0;
    }
    m_scene = 0;
    return;
  }

  int listSize = list.count();
  for (int l = 0; l < listSize; l++) {
    SVNPartialLock pl = list.at(l);

    QList<SVNPartialLockInfo> lockInfos = pl.m_partialLockList;

    TFilePath currentPath = TFilePath(pl.m_fileName.toStdWString());

    TXshSimpleLevel *level;
    for (int i = 0; i < m_levelSet->getLevelCount(); i++) {
      TXshLevel *l        = m_levelSet->getLevel(i);
      TFilePath levelPath = m_scene->decodeFilePath(l->getPath());
      if (levelPath == currentPath) {
        level = l->getSimpleLevel();
        break;
      }
    }

    QString username = VersionControl::instance()->getUserName();
    QString hostName = TSystem::getHostName();

    int count = lockInfos.size();
    for (int i = 0; i < count; i++) {
      SVNPartialLockInfo info = lockInfos.at(i);
      if (info.m_userName == username && info.m_hostName == hostName) {
        level->setEditableRange(info.m_from - 1, info.m_to - 1,
                                info.m_userName.toStdWString());
        invalidateIcons(level, level->getEditableRange());
        TApp *app = TApp::instance();
        if (app->getCurrentLevel()->getLevel() == level) {
          app->getPaletteController()->getCurrentLevelPalette()->setPalette(
              level->getPalette());
          app->getCurrentLevel()->notifyLevelChange();
        }
        break;
      }
    }
  }

  if (m_deleteLater) {
    m_deleteLater = false;

    for (int i = m_levelSet->getLevelCount() - 1; i >= 0; i--) {
      TXshLevel *l = m_levelSet->getLevel(i);
      m_levelSet->removeLevel(l);
    }

    delete m_levelSet;
    m_levelSet = 0;
  }
  m_scene = 0;
}

//-----------------------------------------------------------------------------

void VersionControlManager::onError(const QString &text) {
  m_isRunning = false;
  if (m_deleteLater) {
    m_deleteLater = false;
    for (int i = m_levelSet->getLevelCount() - 1; i >= 0; i--) {
      TXshLevel *l = m_levelSet->getLevel(i);
      m_levelSet->removeLevel(l);
    }
    delete m_levelSet;
    m_levelSet = 0;
  }
  m_scene = 0;
  DVGui::warning(text);
}

//=============================================================================
// VersionControl
//-----------------------------------------------------------------------------

VersionControl::VersionControl()
    : m_userName(), m_password(), m_executablePath() {}

//-----------------------------------------------------------------------------

VersionControl *VersionControl::instance() {
  static VersionControl _instance;
  return &_instance;
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

// Test if svnPath is correct (adding another "/" before binary name doesn't
// create any issue both on Windows and on Mac
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

  // Try to run svn executable, if the path is not specified in the config file,
  // to test if exist
  // In the meantime check for svn version
  if (path.isEmpty()) {
    QProcess p;

    QStringList env = QProcess::systemEnvironment();
    env << "LC_MESSAGES=en_EN";  // Add this environment variables to fix the
                                 // language to English
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
      firstLine         = firstLine.remove("svn, version ");

      double version = firstLine.left(3).toDouble();
      if (version <= 1.5) {
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
bool VersionControl::isFolderUnderVersionControl(const QString &folderPath) {
  QDir dir(folderPath);
  return dir.entryList(QDir::AllDirs | QDir::Hidden).contains(".svn");
}

//-----------------------------------------------------------------------------

void VersionControl::commit(QWidget *parent, const QString &workingDir,
                            const QStringList &filesToCommit, bool folderOnly,
                            int sceneIconAdded) {
  SVNCommitDialog *dialog = new SVNCommitDialog(
      parent, workingDir, filesToCommit, folderOnly, sceneIconAdded);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::revert(QWidget *parent, const QString &workingDir,
                            const QStringList &files, bool folderOnly,
                            int sceneIconAdded) {
  SVNRevertDialog *dialog = new SVNRevertDialog(parent, workingDir, files,
                                                folderOnly, sceneIconAdded);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::update(QWidget *parent, const QString &workingDir,
                            const QStringList &filesToUpdate,
                            int sceneIconsCounts, bool folderOnly,
                            bool updateToRevision, bool nonRecursive) {
  SVNUpdateDialog *dialog =
      new SVNUpdateDialog(parent, workingDir, filesToUpdate, sceneIconsCounts,
                          folderOnly, updateToRevision, nonRecursive);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::updateAndLock(QWidget *parent, const QString &workingDir,
                                   const QStringList &files,
                                   int workingRevision, int sceneIconAdded) {
  SVNUpdateAndLockDialog *dialog = new SVNUpdateAndLockDialog(
      parent, workingDir, files, workingRevision, sceneIconAdded);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::lock(QWidget *parent, const QString &workingDir,
                          const QStringList &filesToLock, int sceneIconAdded) {
  SVNLockDialog *dialog =
      new SVNLockDialog(parent, workingDir, filesToLock, true, sceneIconAdded);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::unlock(QWidget *parent, const QString &workingDir,
                            const QStringList &filesToUnlock,
                            int sceneIconAdded) {
  SVNLockDialog *dialog = new SVNLockDialog(parent, workingDir, filesToUnlock,
                                            false, sceneIconAdded);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::lockFrameRange(QWidget *parent, const QString &workingDir,
                                    const QString &file, int frameCount) {
  SVNLockFrameRangeDialog *dialog =
      new SVNLockFrameRangeDialog(parent, workingDir, file, frameCount);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::lockFrameRange(QWidget *parent, const QString &workingDir,
                                    const QStringList &files) {
  SVNLockMultiFrameRangeDialog *dialog =
      new SVNLockMultiFrameRangeDialog(parent, workingDir, files);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::unlockFrameRange(QWidget *parent,
                                      const QString &workingDir,
                                      const QString &file) {
  SVNUnlockFrameRangeDialog *dialog =
      new SVNUnlockFrameRangeDialog(parent, workingDir, file);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::unlockFrameRange(QWidget *parent,
                                      const QString &workingDir,
                                      const QStringList &files) {
  SVNUnlockMultiFrameRangeDialog *dialog =
      new SVNUnlockMultiFrameRangeDialog(parent, workingDir, files);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::showFrameRangeLockInfo(QWidget *parent,
                                            const QString &workingDir,
                                            const QString &file) {
  SVNFrameRangeLockInfoDialog *dialog =
      new SVNFrameRangeLockInfoDialog(parent, workingDir, file);
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::showFrameRangeLockInfo(QWidget *parent,
                                            const QString &workingDir,
                                            const QStringList &files) {
  SVNMultiFrameRangeLockInfoDialog *dialog =
      new SVNMultiFrameRangeLockInfoDialog(parent, workingDir, files);
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::commitFrameRange(QWidget *parent,
                                      const QString &workingDir,
                                      const QString &file) {
  SVNCommitFrameRangeDialog *dialog =
      new SVNCommitFrameRangeDialog(parent, workingDir, file);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::revertFrameRange(QWidget *parent,
                                      const QString &workingDir,
                                      const QString &file,
                                      const QString &tempFileName) {
  SVNRevertFrameRangeDialog *dialog =
      new SVNRevertFrameRangeDialog(parent, workingDir, file, tempFileName);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::deleteFiles(QWidget *parent, const QString &workingDir,
                                 const QStringList &filesToDelete,
                                 int sceneIconAdded) {
  SVNDeleteDialog *dialog = new SVNDeleteDialog(
      parent, workingDir, filesToDelete, false, sceneIconAdded);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::deleteFolder(QWidget *parent, const QString &workingDir,
                                  const QString &folderName) {
  SVNDeleteDialog *dialog =
      new SVNDeleteDialog(parent, workingDir, QStringList(folderName), true, 0);
  connect(dialog, SIGNAL(done(const QStringList &)), this,
          SIGNAL(commandDone(const QStringList &)));
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::cleanupFolder(QWidget *parent, const QString &workingDir) {
  SVNCleanupDialog *dialog = new SVNCleanupDialog(parent, workingDir);
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

void VersionControl::purgeFolder(QWidget *parent, const QString &workingDir) {
  SVNPurgeDialog *dialog = new SVNPurgeDialog(parent, workingDir);
  dialog->show();
  dialog->raise();
}

//-----------------------------------------------------------------------------

QStringList VersionControl::getSceneContents(const QString &wokingDir,
                                             const QString &sceneFileName) {
  QStringList sceneContents;

  TFilePath scenePath =
      TFilePath(wokingDir.toStdWString()) + sceneFileName.toStdWString();
  if (!TFileStatus(scenePath).doesExist()) return sceneContents;

  ToonzScene scene;
  try {
    scene.load(scenePath);
  } catch (...) {
  }
  std::vector<TXshLevel *> levels;
  scene.getLevelSet()->listLevels(levels);
  std::vector<TXshLevel *>::iterator it;
  for (it = levels.begin(); it != levels.end(); ++it) {
    TFilePath levelPath = scene.decodeFilePath((*it)->getPath());

    if (levelPath.getDots() == "..") {
      TFilePath dir = levelPath.getParentDir();
      QDir qDir(QString::fromStdWString(dir.getWideString()));
      QString levelName =
          QRegExp::escape(QString::fromStdWString(levelPath.getWideName()));
      QString levelType = QString::fromStdString(levelPath.getType());
      QString exp(levelName + ".[0-9]{1,4}." + levelType);
      QRegExp regExp(exp);
      QStringList list = qDir.entryList(QDir::Files);
      list             = list.filter(regExp);
      for (int i = 0; i < list.size(); i++) {
        QString fileName = list.at(i);
        sceneContents.append(toQString(dir + fileName.toStdWString()));
      }
    } else
      sceneContents.append(toQString(levelPath));

    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(levelPath, fpset);

    TFilePathSet::iterator it;
    for (it = fpset.begin(); it != fpset.end(); ++it)
      sceneContents.append(toQString(scene.decodeFilePath((*it))));
  }

  return sceneContents;
}

//-----------------------------------------------------------------------------

QStringList VersionControl::getCurrentSceneContents() const {
  QStringList contents;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return QStringList();

  std::vector<TXshLevel *> levels;
  scene->getLevelSet()->listLevels(levels);
  std::vector<TXshLevel *>::iterator it;
  for (it = levels.begin(); it != levels.end(); ++it) {
    TFilePath levelPath = scene->decodeFilePath((*it)->getPath());
    contents.append(toQString(levelPath));

    TFilePathSet fpset;
    TXshSimpleLevel::getFiles(levelPath, fpset);

    TFilePathSet::iterator it;
    for (it = fpset.begin(); it != fpset.end(); ++it)
      contents.append(toQString(scene->decodeFilePath((*it))));
  }
  return contents;
}
