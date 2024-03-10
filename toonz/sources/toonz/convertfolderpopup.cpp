
#include "convertfolderpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "filebrowser.h"
#include "filebrowserpopup.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tproject.h"

// TnzCore includes
#include "tsystem.h"
#include "tfiletype.h"
#include "tlevel_io.h"
#include "tiio.h"
#include "tenv.h"

// Qt includes
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QApplication>
#include <QMainWindow>
#include <QCheckBox>
#include <QListWidget>
#include <QMessageBox>
#include <QString>
#include <QTextEdit>
#include <QTextStream>

class ConvertFolderPopup::Converter final : public QThread {
  int m_skippedCount;
  int m_doneCount;
  ConvertFolderPopup* m_parent;
  QString m_logTxt;

public:
  Converter(ConvertFolderPopup* parent, QString logTxt)
      : m_parent(parent), m_skippedCount(0), m_doneCount(0), m_logTxt(logTxt) {}
  void run() override;
  void convertLevel(const TFilePath& fp);
  int getSkippedCount() const { return m_skippedCount; }
  int getDoneCount() const { return m_doneCount; }
  QString logTxt() const { return m_logTxt; }
};

void ConvertFolderPopup::Converter::run() {
  ToonzScene* sc = TApp::instance()->getCurrentScene()->getScene();
  DVGui::ProgressDialog* progressDialog = m_parent->m_progressDialog;
  int levelCount                        = m_parent->m_srcFilePaths.size();

  QDir targetDir(m_parent->m_convertFolderFld->getPath());

  for (int i = 0; !m_parent->m_notifier->abortTask() && i < levelCount; i++) {
    TFilePath sourceLevelPath = sc->decodeFilePath(m_parent->m_srcFilePaths[i]);
    QString levelName =
        targetDir.relativeFilePath(sourceLevelPath.getQString());

    TFilePath dstFolder = sourceLevelPath.getParentDir();
    // check already existent levels
    TFilePath dstFilePath =
        m_parent->getDestinationFilePath(m_parent->m_srcFilePaths[i]);

    if (TSystem::doesExistFileOrLevel(dstFilePath)) {
      if (m_parent->m_skip->isChecked()) {
        m_logTxt.append(tr("Level %1 already exists; skipped.\n")
                            .arg(dstFilePath.getQString()));
        m_skippedCount++;
        continue;
      } else {
        bool ok = TSystem::removeFileOrLevel(dstFilePath);
        if (!ok) {
          m_logTxt.append(tr("Failed to remove existing level %1; skipped.\n")
                              .arg(dstFilePath.getQString()));
          m_skippedCount++;
          continue;
        }
      }
    }

    progressDialog->setLabelText(QString(tr("Converting level %1 of %2: %3")
                                             .arg(i + 1)
                                             .arg(levelCount)
                                             .arg(levelName)));

    convertLevel(sourceLevelPath);
  }
  if (m_parent->m_notifier->abortTask())
    m_logTxt.append(tr("Convert aborted.\n"));
}

void ConvertFolderPopup::Converter::convertLevel(
    const TFilePath& sourceFileFullPath) {
  ToonzScene* sc            = TApp::instance()->getCurrentScene()->getScene();
  ConvertFolderPopup* popup = m_parent;

  QString levelName = QString::fromStdString(sourceFileFullPath.getLevelName());
  TFilePath dstFileFullPath = popup->getDestinationFilePath(sourceFileFullPath);

  TFrameId from, to;

  if (TFileType::isLevelFilePath(sourceFileFullPath)) {
    popup->getFrameRange(sourceFileFullPath, from, to);

    if (from == TFrameId() || to == TFrameId()) {
      m_logTxt.append(tr("Level %1 has no frame; skipped.")
                          .arg(sourceFileFullPath.getQString()));
      popup->m_notifier->notifyError();
      return;
    }
  }
  // convert old levels (tzp/tzu) to tlv
  ImageUtils::convertOldLevel2Tlv(sourceFileFullPath, dstFileFullPath, from, to,
                                  m_parent->m_notifier);
  m_doneCount++;
  popup->m_notifier->notifyLevelCompleted(dstFileFullPath);
}

//==================================================================
//    SaveLogTxtPopup
//==================================================================

class SaveLogTxtPopup final : public FileBrowserPopup {
  QString m_logTxt;

public:
  SaveLogTxtPopup(QString txt)
      : FileBrowserPopup(QObject::tr("Save log text")), m_logTxt(txt) {
    setModal(true);
    addFilterType("txt");
    setOkText(QObject::tr("Save"));
  }

  bool execute() override {
    if (m_selectedPaths.empty()) return false;

    TFilePath savePath(*m_selectedPaths.begin());
    if (savePath.isEmpty()) return false;

    savePath = savePath.withNoFrame().withType("txt");  // Just to be sure
    if (TFileStatus(savePath).doesExist()) {
      int ret = DVGui::MsgBox(
          QObject::tr(
              "The log file already exists.\n Do you want to overwrite it?")
              .arg(toQString(savePath.withoutParentDir())),
          QObject::tr("Overwrite"), QObject::tr("Don't Overwrite"), 0);

      if (ret == 2) return false;
    }

    ToonzScene* sc = TApp::instance()->getCurrentScene()->getScene();
    savePath       = sc->decodeFilePath(savePath);

    QFile file(savePath.getQString());
    if (!file.open(QIODevice::WriteOnly)) {
      DVGui::error(tr("Failed to open the file %1").arg(savePath.getQString()));
      return false;
    }
    QTextStream out(&file);
    out << m_logTxt;
    file.close();

    return true;
  }
};
//=============================================================================

ConvertResultPopup::ConvertResultPopup(QString log, TFilePath path)
    : QDialog(), m_logTxt(log), m_targetPath(path) {
  setModal(true);

  QTextEdit* edit            = new QTextEdit(this);
  QPushButton* saveLogButton = new QPushButton(tr("Save log file.."), this);
  QPushButton* closeButton   = new QPushButton(tr("Close"), this);

  edit->setPlainText(m_logTxt);
  edit->setReadOnly(true);

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(5);
  mainLay->setSpacing(10);
  {
    mainLay->addWidget(edit, 1);
    mainLay->addWidget(new QLabel(tr("Do you want to save the log?")), 0);

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setMargin(0);
    buttonsLay->setSpacing(10);
    buttonsLay->setAlignment(Qt::AlignCenter);
    {
      buttonsLay->addWidget(saveLogButton, 0);
      buttonsLay->addWidget(closeButton, 0);
    }
    mainLay->addLayout(buttonsLay, 0);
  }
  setLayout(mainLay);

  connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
  connect(saveLogButton, SIGNAL(clicked()), this, SLOT(onSaveLog()));
}

void ConvertResultPopup::onSaveLog() {
  SaveLogTxtPopup popup(m_logTxt);

  popup.setFolder(m_targetPath);
  popup.setFilename(TFilePath("tzp_convert_log.txt"));
  if (popup.exec() == QDialog::Accepted) close();
}

//=============================================================================

ConvertFolderPopup::ConvertFolderPopup()
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "ConvertTZPInFolder")
    , m_converter(0)
    , m_isConverting(false) {
  setModal(false);
  setWindowTitle(tr("Convert TZP In Folder"));

  m_okBtn     = new QPushButton(tr("Convert"), this);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);

  m_notifier = new ImageUtils::FrameTaskNotifier();

  m_progressDialog = new DVGui::ProgressDialog("", tr("Cancel"), 0, 0);
  m_skip           = new DVGui::CheckBox(tr("Skip Existing Files"), this);
  m_subfolder      = new DVGui::CheckBox(tr("Apply to Subfolder"), this);

  m_convertFolderFld = new DVGui::FileField(0, QString(""), true);

  m_srcFileList = new QListWidget(this);

  m_convertFolderFld->setFileMode(
      QFileDialog::Directory);  // implies ShowDirsOnly
  //-----------------------

  m_progressDialog->setWindowTitle(tr("Convert TZP in Folder"));
  m_progressDialog->setWindowFlags(
      Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
  m_progressDialog->setWindowModality(Qt::WindowModal);

  //----layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(5);
  {
    QHBoxLayout* folderLay = new QHBoxLayout();
    folderLay->setMargin(0);
    folderLay->setSpacing(5);
    {
      folderLay->addWidget(new QLabel(tr("Folder to convert:"), this), 0);
      folderLay->addWidget(m_convertFolderFld, 1);
    }
    m_topLayout->addLayout(folderLay, 0);

    QHBoxLayout* mainLay = new QHBoxLayout();
    mainLay->setMargin(0);
    mainLay->setSpacing(5);
    {
      QVBoxLayout* leftLay = new QVBoxLayout();
      leftLay->setMargin(0);
      leftLay->setSpacing(5);
      {
        leftLay->addWidget(m_skip, 0);
        leftLay->addWidget(m_subfolder, 0);
        leftLay->addStretch(1);
      }
      mainLay->addLayout(leftLay, 0);

      mainLay->addWidget(m_srcFileList, 1);
    }
    m_topLayout->addLayout(mainLay, 1);
  }
  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(20);
  {
    m_buttonLayout->addWidget(m_okBtn);
    m_buttonLayout->addWidget(m_cancelBtn);
  }

  //--- signal-slot connections
  qRegisterMetaType<TFilePath>("TFilePath");

  bool ret = true;
  ret      = ret && connect(m_okBtn, SIGNAL(clicked()), this, SLOT(apply()));
  ret = ret && connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret && connect(m_notifier, SIGNAL(frameCompleted(int)),
                       m_progressDialog, SLOT(setValue(int)));
  ret = ret && connect(m_notifier, SIGNAL(levelCompleted(const TFilePath&)),
                       this, SLOT(onLevelConverted(const TFilePath&)));
  ret = ret && connect(m_progressDialog, SIGNAL(canceled()), m_notifier,
                       SLOT(onCancelTask()));

  ret = ret && connect(m_convertFolderFld, SIGNAL(pathChanged()), this,
                       SLOT(onFileInFolderChanged()));
  ret = ret && connect(m_skip, SIGNAL(clicked()), this, SLOT(onSkipChanged()));
  ret = ret && connect(m_subfolder, SIGNAL(clicked()), this,
                       SLOT(onSubfolderChanged()));

  assert(ret);
}

//------------------------------------------------------------------

ConvertFolderPopup::~ConvertFolderPopup() {
  delete m_notifier;
  delete m_progressDialog;
  delete m_converter;
}

//------------------------------------------------------------------

void ConvertFolderPopup::setFiles() {
  if (m_convertFolderFld->getPath().isEmpty()) {
    m_srcFilePaths.clear();
    m_srcFileList->clear();
    return;
  }
  ToonzScene* scene       = TApp::instance()->getCurrentScene()->getScene();
  TFilePath srcFolderPath = scene->decodeFilePath(
      TFilePath(m_convertFolderFld->getPath().toStdString()));

  bool skip      = m_skip->isChecked();
  bool subFolder = m_subfolder->isChecked();

  QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

  TFilePathSet fps;
  if (subFolder)
    TSystem::readDirectoryTree(fps, srcFolderPath, true, true);
  else
    TSystem::readDirectory(fps, srcFolderPath, true, true);

  m_srcFilePaths.clear();
  QStringList tgtItems;
  QStringList skipItems;
  for (const auto fp : fps) {
    // applies to only tzp
    if (fp.getType() != "tzp") continue;

    // check the destination file existence
    TFilePath dstFp   = getDestinationFilePath(fp);
    TFilePath relPath = fp - srcFolderPath;
    if (!TSystem::doesExistFileOrLevel(dstFp)) {
      m_srcFilePaths.push_back(fp);
      tgtItems.append(relPath.getQString());
    } else if (skip)
      skipItems.append(tr("[SKIP] ") + relPath.getQString());
    else {
      tgtItems.append(tr("[OVERWRITE] ") + relPath.getQString());
      m_srcFilePaths.push_back(fp);
    }
  }

  m_srcFileList->clear();
  m_srcFileList->addItems(tgtItems);
  m_srcFileList->addItems(skipItems);

  QGuiApplication::restoreOverrideCursor();
}

//------------------------------------------------------------------

void ConvertFolderPopup::apply() {
  if (m_convertFolderFld->getPath().isEmpty()) {
    DVGui::error(tr("Target folder is not specified."));
    return;
  }
  if (m_srcFilePaths.empty()) {
    DVGui::error(tr("No files will be converted."));
    return;
  }

  QMessageBox::StandardButton btn = QMessageBox::question(
      this, tr("Cofirmation"),
      tr("Converting %1 files. Are you sure?").arg(m_srcFilePaths.size()),
      QMessageBox::Yes | QMessageBox::No);
  if (btn != QMessageBox::Yes) return;

  // parameters are ok: close the dialog first
  close();

  m_isConverting = true;
  m_progressDialog->reset();
  m_progressDialog->setMinimum(0);
  m_progressDialog->setMaximum(100);
  m_progressDialog->show();
  m_notifier->reset();
  QApplication::setOverrideCursor(Qt::WaitCursor);

  QString logTxt =
      tr("Convert TZP in folder\n") +
      tr("Target Folder: %1\n").arg(m_convertFolderFld->getPath()) +
      tr("Skip Existing Files: %1\n")
          .arg((m_skip->isChecked()) ? "True" : "False") +
      tr("Apply to Subfolder: %1\n")
          .arg((m_subfolder->isChecked()) ? "True" : "False") +
      tr("Approx. levels to be converted: %1\n\n").arg(m_srcFilePaths.size()) +
      tr("Started: ") + QDateTime::currentDateTime().toString() + "\n";
  m_converter = new Converter(this, logTxt);
  bool ret =
      connect(m_converter, SIGNAL(finished()), this, SLOT(onConvertFinished()));
  Q_ASSERT(ret);

  // start converting. Conversion end is handled by onConvertFinished() slot
  m_converter->start();
}

//------------------------------------------------------------------

void ConvertFolderPopup::onConvertFinished() {
  m_isConverting = false;
  TFilePath dstFolderPath(m_convertFolderFld->getPath().toStdWString());
  FileBrowser::refreshFolder(dstFolderPath);

  m_progressDialog->close();
  QApplication::restoreOverrideCursor();

  QString logTxt = m_converter->logTxt();
  // opens result

  int errorCount   = m_notifier->getErrorCount();
  int skippedCount = m_converter->getSkippedCount();
  int doneCount    = m_converter->getDoneCount();

  if (m_notifier->abortTask())
    logTxt.append(tr("Convert aborted:"));
  else
    logTxt.append(tr("Convert completed:"));

  logTxt.append(
      tr("  %1 level(s) done, %2 level(s) skipped with %3 error(s).\n")
          .arg(doneCount)
          .arg(skippedCount)
          .arg(errorCount));
  logTxt.append(tr("Ended: ") + QDateTime::currentDateTime().toString());

  delete m_converter;
  m_converter = 0;

  ConvertResultPopup resultPopup(logTxt, dstFolderPath);
  resultPopup.exec();
}

//------------------------------------------------------------------

void ConvertFolderPopup::onLevelConverted(const TFilePath& fullPath) {
  IconGenerator::instance()->invalidate(fullPath);
}

//------------------------------------------------------------------

void ConvertFolderPopup::onFileInFolderChanged() { setFiles(); }

//------------------------------------------------------------------

void ConvertFolderPopup::onSkipChanged() { setFiles(); }

//------------------------------------------------------------------

void ConvertFolderPopup::onSubfolderChanged() { setFiles(); }

//------------------------------------------------------------------

TFilePath ConvertFolderPopup::getDestinationFilePath(
    const TFilePath& sourceFilePath) {
  TFilePath destFolder = sourceFilePath.getParentDir();
  ToonzScene* scene    = TApp::instance()->getCurrentScene()->getScene();

  // Build the output level name
  const std::string& ext   = "tlv";
  const std::wstring& name = sourceFilePath.getWideName();

  TFilePath destName = TFilePath(name).withType(ext);

  // Merge the two
  return destFolder + destName;
}

//------------------------------------------------------------------

void ConvertFolderPopup::getFrameRange(const TFilePath& sourceFilePath,
                                       TFrameId& from, TFrameId& to) {
  from = to = TFrameId();

  if (!TFileType::isLevelFilePath(sourceFilePath)) return;

  TLevelReaderP lr(sourceFilePath);
  if (!lr) return;

  TLevelP level = lr->loadInfo();
  if (level->begin() == level->end()) return;

  from = level->begin()->first;
  to   = (--level->end())->first;
}

//=============================================================================
// ConvertFolderPopup
//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ConvertFolderPopup> openConvertTZPInFolderPopup(
    MI_ConvertTZPInFolder);
