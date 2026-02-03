#include "toonz/menubarcommandids.h"
#include "toonz/menubar.h"
#include "toonz/ocaio.h"
#include "toonz/projectmanager.h"
#include "toonz/preferences.h"
#include "toonz/tapp.h"
#include "toonz/toonzfolders.h"

#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/filebrowserpopup.h"
#include "toonzqt/gutil.h"

#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDateTime>

using namespace DVGui;

class ImportFlashVectorCommand final : public MenuItemHandler {
public:
  ImportFlashVectorCommand() : MenuItemHandler(MI_ImportFlashVector) {}
  void execute() override;
} ImportFlashVectorCommand;

void ImportFlashVectorCommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  static GenericLoadFilePopup *loadPopup = 0;
  if (!loadPopup) {
    loadPopup = new GenericLoadFilePopup(
        QObject::tr("Import Flash (Vector via External Decompiler)"));
    loadPopup->addFilterType("swf");
    loadPopup->addFilterType("fla");
  }
  if (!scene->isUntitled())
    loadPopup->setFolder(scene->getScenePath().getParentDir());
  else
    loadPopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  TFilePath fp = loadPopup->getPath();
  if (fp.isEmpty()) {
    DVGui::info(QObject::tr("Flash import cancelled : empty filepath."));
    return;
  }

  // Create temporary output directory
  QString tmpDirName = QString("flare_flash_import_%1")
                           .arg(QDateTime::currentMSecsSinceEpoch());
  TFilePath outDir = TSystem::getTempDir() + TFilePath(tmpDirName.toStdString());
  if (!outDir.mkpath(".")) {
    DVGui::error(QObject::tr("Unable to create temporary directory for import."));
    return;
  }

  // Locate helper script in common development locations
  QStringList candidates;
  // Relative to application dir
  candidates << QDir(QCoreApplication::applicationDirPath())
                    .absoluteFilePath("../../tools/flash/decompile_flash.py");
  candidates << QDir(QCoreApplication::applicationDirPath())
                    .absoluteFilePath("tools/flash/decompile_flash.py");
  // Relative to source module dir
  QString moduleDir = ToonzFolder::getMyModuleDir().getQString();
  candidates << QDir(moduleDir).absoluteFilePath("../../../../tools/flash/decompile_flash.py");

  QString scriptPath;
  for (const QString &c : candidates) {
    if (QFile::exists(c)) {
      scriptPath = c;
      break;
    }
  }

  if (scriptPath.isEmpty()) {
    QString msg = QObject::tr(
        "Flash vector import requires the helper script "
        "'decompile_flash.py' and a compatible Flash decompiler (e.g., "
        "JPEXS). Install the tools and place the script in the 'tools/flash' "
        "directory of the source tree, or see the documentation for more "
        "information.");
    DVGui::warning(msg);
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../..")));
    return;
  }

  // Run the helper script using the system python
  QString python = "python";
  QStringList args;
  args << scriptPath;
  args << "--input" << fp.getQString();
  args << "--output" << outDir.getQString();

  // If the user configured a specific decompiler path in Preferences, pass it on
  QString prefDecompilerPath = Preferences::instance()->getFlashDecompilerPath();
  if (!prefDecompilerPath.isEmpty()) args << "--decompiler" << prefDecompilerPath;

  QProcess proc;
  proc.setProgram(python);
  proc.setArguments(args);
  proc.start();
  bool started = proc.waitForStarted(10000);
  if (!started) {
    QString err = QObject::tr("Unable to start Python. Please ensure Python is installed and on your PATH.");
    DVGui::warning(err);
    return;
  }

  // Wait for completion and capture output
  proc.waitForFinished(-1);
  int exitCode = proc.exitCode();
  QString stdErr = proc.readAllStandardError();
  QString stdOut = proc.readAllStandardOutput();

  if (exitCode != 0) {
    QString err = QObject::tr("Flash decompilation failed: %1").arg(stdErr);
    DVGui::warning(err);
    return;
  }

  QString successMsg = QObject::tr("Flash content has been exported to:\n%1\n\nOpen containing folder to review and import files.")
                           .arg(outDir.getQString());
  std::vector<QString> buttons = {QObject::tr("Open containing folder"),
                                  QObject::tr("OK")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, successMsg, buttons);

  if (ret == 1) {
    if (TSystem::isUNC(outDir))
      QDesktopServices::openUrl(QUrl(outDir.getQString()));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(outDir.getQString()));
  }
}

class ImportFlashContainerCommand final : public MenuItemHandler {
public:
  ImportFlashContainerCommand() : MenuItemHandler(MI_ImportFlashContainer) {}
  void execute() override;
} ImportFlashContainerCommand;

void ImportFlashContainerCommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  static GenericLoadFilePopup *loadPopup = 0;
  if (!loadPopup) {
    loadPopup = new GenericLoadFilePopup(QObject::tr("Import Flash/XFL/SWC (Native)"));
    loadPopup->addFilterType("xfl");
    loadPopup->addFilterType("swc");
    loadPopup->addFilterType("fla");
    loadPopup->addFilterType("swf");
    loadPopup->addFilterType("as");
    loadPopup->addFilterType("xml");
  }
  if (!scene->isUntitled())
    loadPopup->setFolder(scene->getScenePath().getParentDir());
  else
    loadPopup->setFolder(TProjectManager::instance()->getCurrentProject()->getScenesPath());

  TFilePath fp = loadPopup->getPath();
  if (fp.isEmpty()) {
    DVGui::info(QObject::tr("Import cancelled : empty filepath."));
    return;
  }

  // Create temporary output directory
  QString tmpDirName = QString("flare_flash_import_%1").arg(QDateTime::currentMSecsSinceEpoch());
  TFilePath outDir = TSystem::getTempDir() + TFilePath(tmpDirName.toStdString());
  if (!outDir.mkpath(".")) {
    DVGui::error(QObject::tr("Unable to create temporary directory for import."));
    return;
  }

  // Locate helper script in common development locations
  QStringList candidates;
  candidates << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../tools/flash/import_container.py");
  candidates << QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("tools/flash/import_container.py");
  QString moduleDir = ToonzFolder::getMyModuleDir().getQString();
  candidates << QDir(moduleDir).absoluteFilePath("../../../../tools/flash/import_container.py");

  QString scriptPath;
  for (const QString &c : candidates) {
    if (QFile::exists(c)) {
      scriptPath = c;
      break;
    }
  }

  if (scriptPath.isEmpty()) {
    QString msg = QObject::tr(
        "Native Flash/XFL/SWC import requires the helper script 'import_container.py'. See the documentation for more information.");
    DVGui::warning(msg);
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../..")));
    return;
  }

  // Run the helper script using the system python
  QString python = "python";
  QStringList args;
  args << scriptPath;
  args << "--input" << fp.getQString();
  args << "--output" << outDir.getQString();

  // Pass configured decompiler path (if any) to assist with embedded SWFs/SWC
  QString prefDecompilerPath = Preferences::instance()->getFlashDecompilerPath();
  if (!prefDecompilerPath.isEmpty()) args << "--decompiler" << prefDecompilerPath;

  QProcess proc;
  proc.setProgram(python);
  proc.setArguments(args);
  proc.start();
  bool started = proc.waitForStarted(10000);
  if (!started) {
    QString err = QObject::tr("Unable to start Python. Please ensure Python is installed and on your PATH.");
    DVGui::warning(err);
    return;
  }

  // Wait for completion and capture output
  proc.waitForFinished(-1);
  int exitCode = proc.exitCode();
  QString stdErr = proc.readAllStandardError();
  QString stdOut = proc.readAllStandardOutput();

  if (exitCode != 0) {
    QString err = QObject::tr("Import failed: %1").arg(stdErr);
    DVGui::warning(err);
    return;
  }

  QString successMsg = QObject::tr("Container export complete. Files exported to:\n%1\n\nOpen containing folder to review and import files.").arg(outDir.getQString());
  std::vector<QString> buttons = {QObject::tr("Open containing folder"), QObject::tr("OK")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, successMsg, buttons);

  if (ret == 1) {
    if (TSystem::isUNC(outDir))
      QDesktopServices::openUrl(QUrl(outDir.getQString()));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(outDir.getQString()));
  }
}
