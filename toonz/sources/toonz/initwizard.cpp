#include "initwizard.h"

// Tnz6 includes
#include "tapp.h"

// TnzBase includes
#include "tenv.h"
#include "texception.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"
#include "thirdparty.h"

// TnzCore includes
#include "tsystem.h"
#include "tconvert.h"
#include "timage_io.h"

#include <QApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QPixmap>
#include <QKeyEvent>

namespace OTZSetup {

//-----------------------------------------------------------------------------

bool requireInitWizard() {
  Preferences *pref = Preferences::instance();
  if (pref->getBoolValue(restartInitWizard)) return true;
  return pref->getIntValue(initWizardComplete) < INITWIZARD_REVISION;
}

//-----------------------------------------------------------------------------

void flagInitWizard(bool completed) {
  Preferences *pref = Preferences::instance();

  // Clear restart flag
  if (pref->getBoolValue(restartInitWizard)) {
    pref->setValue(restartInitWizard, false);
  }

  // Mark as complete
  pref->setValue(initWizardComplete, completed ? INITWIZARD_REVISION : 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

LanguagePage::LanguagePage(QWidget *parent)
    : QWizardPage(parent), m_translator(NULL) {
  m_headLabel = new QLabel();
  m_headLabel->setWordWrap(true);
  m_footLabel = new QLabel();
  m_footLabel->setWordWrap(true);

  m_languageCombo = new QComboBox();

  Preferences *pref    = Preferences::instance();
  QStringList langList = pref->getLanguageList();

  QString curLang = pref->getCurrentLanguage();
  int curLangId   = m_languageCombo->findText(curLang);
  if (curLangId == -1) curLangId = 0;
  m_languageCombo->addItems(langList);
  m_languageCombo->setCurrentIndex(curLangId);

  bool ret = connect(m_languageCombo, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(languageChanged(int)));
  if (!ret) throw TException();

  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(m_headLabel);
  layout->addWidget(m_languageCombo);
  layout->addStretch(1);
  layout->addWidget(m_footLabel);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

LanguagePage::~LanguagePage() {
  QApplication::removeTranslator(m_translator);
  delete m_translator;
}

//-----------------------------------------------------------------------------

void LanguagePage::initializePage() {
  setTitle(tr("Initialization Wizard"));
  setSubTitle(tr("Welcome to OpenToonz!"));

  QString languagePathString =
      QString::fromStdString(::to_string(TEnv::getConfigDir() + "loc"));
#ifndef WIN32
  // the merge of menu on osx can cause problems with different languages with
  // the Preferences menu
  // qt_mac_set_menubar_merge(false);
  languagePathString += "/" + m_languageCombo->currentText();
#else
  languagePathString += "\\" + m_languageCombo->currentText();
#endif

  if (m_translator) {
    QApplication::removeTranslator(m_translator);
    delete m_translator;
  }
  std::string debugz = languagePathString.toStdString();
  m_translator = new QTranslator();
  m_translator->load("toonz", languagePathString);
  QApplication::installTranslator(m_translator);

  QStringList lines;
  lines.append(
      tr("This wizard will guide you through the initial configuration process."));
  lines.append("");
  lines.append(tr("Select your preferred language:"));
  m_headLabel->setText(lines.join('\n'));

  m_footLabel->setText(
      "\n" +
      tr("Some translations may be missing in languages other than English."));
}

//-----------------------------------------------------------------------------

void LanguagePage::languageChanged(int index) {
  Preferences *pref = Preferences::instance();
  pref->setValue(CurrentLanguageName, m_languageCombo->currentText());
  initializePage();  // refresh all text
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

ThirdPartyPage::ThirdPartyPage(QWidget *parent) : QWizardPage(parent) {
  m_footLabel = new QLabel();
  m_footLabel->setWordWrap(true);

  m_ffmpegButton = new QPushButton();
  m_ffmpegStatus = new QLabel();
  m_ffmpegLabel  = new QLabel();

  m_rhubarbButton = new QPushButton();
  m_rhubarbStatus = new QLabel();
  m_rhubarbLabel  = new QLabel();

  m_nextPage = UI_PAGE;

  bool ret =
      connect(m_ffmpegButton, SIGNAL(clicked()), this, SLOT(ffmpegSetup()));
  ret = ret &&
        connect(m_rhubarbButton, SIGNAL(clicked()), this, SLOT(rhubarbSetup()));
  if (!ret) throw TException();

  QGridLayout *layout = new QGridLayout();
  layout->addWidget(m_ffmpegButton, 0, 0);
  layout->addWidget(m_ffmpegStatus, 0, 1);
  layout->addWidget(m_ffmpegLabel, 0, 2);
  layout->addWidget(m_rhubarbButton, 1, 0);
  layout->addWidget(m_rhubarbStatus, 1, 1);
  layout->addWidget(m_rhubarbLabel, 1, 2);
  layout->setColumnStretch(2, 1);
  layout->setRowStretch(layout->rowCount(), 1);
  layout->addWidget(m_footLabel, layout->rowCount(), 0, 1, 3);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void ThirdPartyPage::initializePage() {
  setTitle(tr("3rd Party Extensions"));
  setSubTitle(tr("Configure extra functionality."));

  QStringList lines;
  lines.append(tr("All extensions are optional."));
  lines.append(
      tr("Settings can be changed later in \"File > Preferences...\"."));
  m_footLabel->setText(lines.join('\n'));

  QPixmap activeIcon  = svgToPixmap(":icons/setup/thirdparty_active.svg");
  QPixmap missingIcon = svgToPixmap(":icons/setup/thirdparty_missing.svg");

  m_ffmpegButton->setText(tr("Setup"));
  m_ffmpegLabel->setText(tr("FFmpeg: Extra video and audio formats"));
  m_rhubarbButton->setText(tr("Setup"));
  m_rhubarbLabel->setText(tr("Rhubarb: Automatic lip-syncing"));

  m_ffmpegStatus->setPixmap(ThirdParty::checkFFmpeg() ? activeIcon
                                                      : missingIcon);
  m_rhubarbStatus->setPixmap(ThirdParty::checkRhubarb() ? activeIcon
                                                        : missingIcon);

  m_nextPage = UI_PAGE;
}

//-----------------------------------------------------------------------------

void ThirdPartyPage::ffmpegSetup() {
  m_nextPage = FFMPEG_PAGE;
  wizard()->next();
  m_nextPage = UI_PAGE;
}

//-----------------------------------------------------------------------------

void ThirdPartyPage::rhubarbSetup() {
  m_nextPage = RHUBARB_PAGE;
  wizard()->next();
  m_nextPage = UI_PAGE;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

FFMPEGPage::FFMPEGPage(QWidget *parent) : QWizardPage(parent) {
  m_richLabel = new QLabel();
  m_richLabel->setTextFormat(Qt::RichText);
  m_richLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_richLabel->setOpenExternalLinks(true);

  m_pathLabel  = new QLabel();
  m_pathEdit   = new QLineEdit();
  m_pathBrowse = new QPushButton();

  m_statusIcon  = new QLabel();
  m_statusLabel = new QLabel();

  bool ret =
      connect(m_pathBrowse, SIGNAL(clicked()), this, SLOT(browsePath()));
  ret = ret && connect(m_pathEdit, SIGNAL(textChanged(const QString &)), this,
                       SLOT(testPath(const QString &)));
  if (!ret) throw TException();

  QGridLayout *layout = new QGridLayout();
  layout->addWidget(m_richLabel, 0, 0, 1, 4);
  layout->addWidget(m_pathLabel, 1, 0, 1, 2);
  layout->addWidget(m_pathEdit, 1, 2);
  layout->addWidget(m_pathBrowse, 1, 3);
  layout->setRowStretch(2, 1);
  layout->addWidget(m_statusIcon, 3, 0);
  layout->addWidget(m_statusLabel, 3, 1, 1, 3);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void FFMPEGPage::initializePage() {
  setTitle(tr("FFmpeg Setup"));
  setSubTitle(tr("Extra video and audio formats."));

  QStringList videos, audios;
  ThirdParty::getFFmpegVideoSupported(videos);
  ThirdParty::getFFmpegAudioSupported(audios);

  QStringList lines;
  lines.append(tr("OpenToonz can use FFmpeg for additional media formats:"));
  lines.append(tr("- Video: <b>") + videos.join(", ") + "</b>");
  lines.append(tr("- Audio: <b>") + audios.join(", ") + "</b>");
  lines.append("");
  lines.append(tr("You can download FFmpeg at: "));
  lines.append("<a href=\"https://ffmpeg.org/\">https://ffmpeg.org/</a>");
  lines.append("");
  m_richLabel->setText(lines.join("<br>"));

  QString path = Preferences::instance()->getFfmpegPath();
  m_pathLabel->setText(tr("Path:"));
  m_pathEdit->setText(path);
  m_pathBrowse->setText(tr("Browse"));

  QPixmap activeIcon  = svgToPixmap(":icons/setup/thirdparty_active.svg");
  QPixmap missingIcon = svgToPixmap(":icons/setup/thirdparty_missing.svg");

  bool ffmpeg_found = ThirdParty::checkFFmpeg();
  m_statusIcon->setPixmap(ffmpeg_found ? activeIcon : missingIcon);
  m_statusLabel->setText(
      ffmpeg_found ? tr("FFmpeg found: ") + path
                   : tr("FFmpeg not found! ffmpeg and/or ffprobe missing."));
}

//-----------------------------------------------------------------------------

void FFMPEGPage::testPath(const QString &path) {
  if (ThirdParty::findFFmpeg(path)) {
    Preferences::instance()->setValue(ffmpegPath, path);
    initializePage();
  }
}

//-----------------------------------------------------------------------------

void FFMPEGPage::browsePath() {
#if defined(_WIN32)
  QString path = Preferences::instance()->getFfmpegPath() + "/ffmpeg.exe";
#else
  QString path = Preferences::instance()->getFfmpegPath() + "/ffmpeg";
#endif

  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Find FFmpeg executable"), path, tr("All files (*.*)"));
  if (fileName.isEmpty()) return;

  TFilePath fp = TFilePath(fileName);
  path         = fp.getParentDir().getQString();
  m_pathEdit->setText(path);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

RhubarbPage::RhubarbPage(QWidget *parent) : QWizardPage(parent) {
  m_richLabel = new QLabel();
  m_richLabel->setTextFormat(Qt::RichText);
  m_richLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  m_richLabel->setOpenExternalLinks(true);

  m_pathLabel  = new QLabel();
  m_pathEdit   = new QLineEdit();
  m_pathBrowse = new QPushButton();

  m_statusIcon  = new QLabel();
  m_statusLabel = new QLabel();

  bool ret =
      connect(m_pathBrowse, SIGNAL(clicked()), this, SLOT(browsePath()));
  ret = ret && connect(m_pathEdit, SIGNAL(textChanged(const QString &)), this,
                       SLOT(testPath(const QString &)));
  if (!ret) throw TException();

  QGridLayout *layout = new QGridLayout();
  layout->addWidget(m_richLabel, 0, 0, 1, 4);
  layout->addWidget(m_pathLabel, 1, 0, 1, 2);
  layout->addWidget(m_pathEdit, 1, 2);
  layout->addWidget(m_pathBrowse, 1, 3);
  layout->setRowStretch(2, 1);
  layout->addWidget(m_statusIcon, 3, 0);
  layout->addWidget(m_statusLabel, 3, 1, 1, 3);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void RhubarbPage::initializePage() {
  setTitle(tr("Rhubarb Setup"));
  setSubTitle(tr("Automatic lip-syncing."));

  QStringList lines;
  lines.append(tr("OpenToonz can use Rhubarb for auto lip-syncing."));
  lines.append("");
  lines.append(tr("You can download Rhubarb at:"));
  lines.append(
      "<a "
      "href=\"https://github.com/DanielSWolf/rhubarb-lip-sync\">https://"
      "github.com/DanielSWolf/rhubarb-lip-sync</a>");
  lines.append("");
  m_richLabel->setText(lines.join("<br>"));

  QString path = Preferences::instance()->getRhubarbPath();
  m_pathLabel->setText(tr("Path:"));
  m_pathEdit->setText(path);
  m_pathBrowse->setText(tr("Browse"));

  QPixmap activeIcon  = svgToPixmap(":icons/setup/thirdparty_active.svg");
  QPixmap missingIcon = svgToPixmap(":icons/setup/thirdparty_missing.svg");

  bool found = ThirdParty::checkRhubarb();
  m_statusIcon->setPixmap(found ? activeIcon : missingIcon);
  m_statusLabel->setText(found ? tr("Rhubarb found: ") + path
                               : tr("Rhubarb not found!"));
}

//-----------------------------------------------------------------------------

void RhubarbPage::testPath(const QString &path) {
  if (ThirdParty::findRhubarb(path)) {
    Preferences::instance()->setValue(rhubarbPath, path);
    initializePage();
  }
}

//-----------------------------------------------------------------------------

void RhubarbPage::browsePath() {
#if defined(_WIN32)
  QString path = Preferences::instance()->getRhubarbPath() + "/rhubarb.exe";
#else
  QString path = Preferences::instance()->getRhubarbPath() + "/rhubarb";
#endif

  QString fileName = QFileDialog::getOpenFileName(
      this, tr("Find Rhubarb executable"), path, tr("All files (*.*)"));
  if (fileName.isEmpty()) return;

  TFilePath fp = TFilePath(fileName);
  path         = fp.getParentDir().getQString();
  m_pathEdit->setText(path);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

UIPage::UIPage(QWidget *parent) : QWizardPage(parent) {
  m_roomsLabel = new QLabel();
  m_roomsCombo = new QComboBox();
  m_resetRoom  = new QPushButton();

  m_themeLabel = new QLabel();
  m_themeCombo = new QComboBox();
  m_dIconLabel = new QLabel();
  m_dIconCombo = new QComboBox();

  m_themeIcon  = new QLabel();

  Preferences *pref = Preferences::instance();

  // load rooms or layouts
  QMap<int, QString> rooms = pref->getRoomMap();
  for (auto const &room : rooms) {
    m_roomsCombo->addItem(room);
  }

  // load styles
  QStringList styles = pref->getStyleSheetList();
  for (auto const &style : styles) {
    m_themeCombo->addItem(style);
  }
  m_dIconCombo->addItem("1");
  m_dIconCombo->addItem("2");
  m_dIconCombo->addItem("3");

  // find style
  int index = m_themeCombo->findText(
      Preferences::instance()->getStringValue(CurrentStyleSheetName));
  if (index == -1) index = 0;
  m_themeCombo->setCurrentIndex(index);
  themeChanged(index);  // generate preview

  bool ret = connect(m_themeCombo, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(themeChanged(int)));
  ret = ret && connect(m_resetRoom, SIGNAL(clicked()), this, SLOT(resetRoom()));
  if (!ret) throw TException();

  QGridLayout *layout = new QGridLayout();
  layout->addWidget(m_roomsLabel, 0, 0);
  layout->addWidget(m_roomsCombo, 0, 1);
  layout->addWidget(m_themeLabel, 1, 0);
  layout->addWidget(m_themeCombo, 1, 1);
  layout->addWidget(m_dIconLabel, 1, 2, Qt::AlignRight);
  layout->addWidget(m_dIconCombo, 1, 3);
  layout->setColumnStretch(1, 1);
  layout->setRowStretch(2, 1);
  layout->addWidget(m_themeIcon, 3, 0, 1, 4, Qt::AlignCenter);
  layout->addWidget(m_resetRoom, 3, 2, 1, 2, Qt::AlignBottom);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void UIPage::initializePage() {
  setTitle(tr("User Interface"));
  setSubTitle("Set your personal preferences.");

  m_roomsLabel->setText(tr("Rooms:"));
  m_themeLabel->setText(tr("Theme:"));
  m_dIconLabel->setText(tr("Icons:"));
  m_resetRoom->setText(tr(" Reset All Rooms "));
  m_dIconCombo->setItemText(0, tr("Best"));
  m_dIconCombo->setItemText(1, tr("Light"));
  m_dIconCombo->setItemText(2, tr("Dark"));
}

//-----------------------------------------------------------------------------

bool UIPage::validatePage() {
  Preferences *pref = Preferences::instance();

  // Save rooms setting
  QString room = m_roomsCombo->currentText();
  if (room != pref->getStringValue(CurrentRoomChoice)) {
    pref->setValue(CurrentRoomChoice, room);
  }

  // Setup and save dark icons
  bool darkIcons = m_dIconCombo->currentIndex() == 2;
  if (m_dIconCombo->currentIndex() == 0) {
    // Is dark icons best for the theme?
    QString name     = m_themeCombo->currentText();
    std::string path = "qss/" + name.toStdString();
    TFilePath fpDIco(TEnv::getConfigDir() + path + "/darkicons.txt");
    darkIcons = TSystem::doesExistFileOrLevel(fpDIco);
  }
  if (darkIcons != pref->getBoolValue(iconTheme))
    pref->setValue(iconTheme, darkIcons);

  // Save theme setting
  QString theme = m_themeCombo->currentText();
  if (theme != pref->getStringValue(CurrentStyleSheetName))
    pref->setValue(CurrentStyleSheetName, theme);

  return true;
}

//-----------------------------------------------------------------------------

void UIPage::themeChanged(int index) {
  QString name     = m_themeCombo->currentText();
  std::string path = "qss/" + name.toStdString();
  TFilePath fpPrev(TEnv::getConfigDir() + path + "/preview.png");

  QPixmap preview = QPixmap(192, 108);
  if (TSystem::doesExistFileOrLevel(fpPrev))
    preview.load(fpPrev.getQString());
  else
    preview.fill(Qt::transparent);
  m_themeIcon->setPixmap(preview);
}

//-----------------------------------------------------------------------------

void UIPage::resetRoom() {
  QString message(tr("Reset rooms to their default?"));
  message += "\n" + tr("All user rooms will be lost!");

  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Reset Rooms"), message,
      QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

  if (ret != QMessageBox::Yes) return;

  // Reflect changes to mainwindow.cpp: void MainWindow::resetRoomsLayout()
  TFilePath layoutDir = ToonzFolder::getMyRoomsDir();
  if (layoutDir != TFilePath()) {
    TSystem::rmDirTree(layoutDir);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

ProjectsPage::ProjectsPage(QWidget *parent) : QWizardPage(parent) {
  m_stuffCB   = new QCheckBox();
  m_myDocsCB  = new QCheckBox();
  m_desktopCB = new QCheckBox();
  m_customCB  = new QCheckBox();

  m_customPaths = new QListWidget();
  m_customAdd   = new QPushButton();
  m_customDel   = new QPushButton();
  m_customRefsh = new QPushButton();

  loadPaths();

  Preferences *pref = Preferences::instance();
  int projectPaths  = pref->getIntValue(projectRoot);
  m_stuffCB->setChecked(projectPaths & 0x08);
  m_stuffCB->setDisabled(true);
  m_myDocsCB->setChecked(projectPaths & 0x04);
  m_desktopCB->setChecked(projectPaths & 0x02);
  m_customCB->setChecked(projectPaths & 0x01);
  m_customPaths->setEnabled(projectPaths & 0x01);
  m_customAdd->setEnabled(projectPaths & 0x01);
  m_customDel->setEnabled(projectPaths & 0x01);

  bool ret = connect(m_customAdd, SIGNAL(clicked()), this, SLOT(addPath()));
  ret = ret && connect(m_customDel, SIGNAL(clicked()), this, SLOT(delPath()));
  ret = ret && connect(m_customRefsh, SIGNAL(clicked()), this, SLOT(refreshPaths()));
  ret = ret && connect(m_customCB, SIGNAL(stateChanged(int)), this,
                       SLOT(customCBChanged(int)));
  if (!ret) throw TException();

  QHBoxLayout *layoutBtns = new QHBoxLayout();
  layoutBtns->addWidget(m_customAdd);
  layoutBtns->addWidget(m_customDel);
  layoutBtns->addWidget(m_customRefsh);

  QVBoxLayout *layout = new QVBoxLayout();
  layout->addWidget(m_stuffCB);
  layout->addWidget(m_myDocsCB);
  layout->addWidget(m_desktopCB);
  layout->addWidget(m_customCB);
  layout->addWidget(m_customPaths);
  layout->addLayout(layoutBtns);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void ProjectsPage::loadPaths() {
  m_customPaths->clear();
  QString customPR = Preferences::instance()->getStringValue(customProjectRoot);
  for (QString path : customPR.split("**")) {
    if (!path.isEmpty()) {
      QListWidgetItem *item = new QListWidgetItem(path);
      item->setFlags(item->flags() | Qt::ItemIsEditable);
      m_customPaths->addItem(item);
    }
  }
}

//-----------------------------------------------------------------------------

void ProjectsPage::initializePage() {
  setTitle(tr("Project Locations"));
  setSubTitle("Where you want to manage your projects?");

  m_stuffCB->setText(tr("OpenToonz stuff/projects"));
  m_myDocsCB->setText(tr("My Documents/OpenToonz"));
  m_desktopCB->setText(tr("Desktop/OpenToonz"));
  m_customCB->setText(tr("Custom:"));

  m_customAdd->setText(tr("Add Path..."));
  m_customDel->setText(tr("Delete Selected"));
  m_customRefsh->setText(tr("Refresh List"));
}

//-----------------------------------------------------------------------------

bool ProjectsPage::validatePage() {
  Preferences *pref = Preferences::instance();

  int projectPaths = 0;
  if (m_stuffCB->isChecked()) projectPaths |= 0x08;
  if (m_myDocsCB->isChecked()) projectPaths |= 0x04;
  if (m_desktopCB->isChecked()) projectPaths |= 0x02;
  if (m_customCB->isChecked()) projectPaths |= 0x01;
  pref->setValue(projectRoot, projectPaths);

  QStringList paths;
  for (int i = 0; i < m_customPaths->count(); ++i) {
    QListWidgetItem *item = m_customPaths->item(i);

    for (QString path : item->text().split("**")) {
      QString pathTrimmed = path.trimmed();
      if (!pathTrimmed.isEmpty()) paths.append(pathTrimmed);
    }
  }
  pref->setValue(customProjectRoot, paths.join("**"));

  return true;
}

//-----------------------------------------------------------------------------

void ProjectsPage::customCBChanged(int state) {
  bool enable = m_customCB->isChecked();
  m_customPaths->setEnabled(enable);
  m_customAdd->setEnabled(enable);
  m_customDel->setEnabled(enable);
  m_customRefsh->setEnabled(enable);
  if (!enable) m_customPaths->clearSelection();
}

//-----------------------------------------------------------------------------

void ProjectsPage::addPath() {
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Open Project Root Directory"), QDir::currentPath(),
      QFileDialog::ShowDirsOnly);
  if (dir.isEmpty()) return;

#if defined(_WIN32)
  dir = dir.replace('/', '\\');
#endif

  QListWidgetItem *item = new QListWidgetItem(dir.trimmed());
  item->setFlags(item->flags() | Qt::ItemIsEditable);
  m_customPaths->addItem(item);
}

//-----------------------------------------------------------------------------

void ProjectsPage::delPath() {
  if (m_customPaths->selectedItems().count() != 0) {
    QListWidgetItem *item = m_customPaths->currentItem();
    if (item) delete m_customPaths->takeItem(m_customPaths->currentRow());
  }
}

//-----------------------------------------------------------------------------

void ProjectsPage::refreshPaths() {
  validatePage();
  loadPaths();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

InitWizard::InitWizard(QWidget *parent) : QWizard(parent) {
  setPage(LANGUAGE_PAGE, new LanguagePage);
  setPage(THIRDPARTY_PAGE, new ThirdPartyPage);
  setPage(FFMPEG_PAGE, new FFMPEGPage);
  setPage(RHUBARB_PAGE, new RhubarbPage);
  setPage(UI_PAGE, new UIPage);
  setPage(PROJECTS_PAGE, new ProjectsPage);

  setWindowTitle(QString::fromStdString(TEnv::getApplicationFullName()));
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setWizardStyle(QWizard::ModernStyle);

  // Auto detect 3rd party programs
  QString ffmpeg = ThirdParty::autodetectFFmpeg();
  if (!ffmpeg.isEmpty()) {
    ThirdParty::setFFmpegDir(ffmpeg);
  }
  QString rhubarb = ThirdParty::autodetectRhubarb();
  if (!rhubarb.isEmpty()) {
    ThirdParty::setRhubarbDir(rhubarb);
  }

  // install custom handlers
  bool ret = disconnect(button(QWizard::NextButton), SIGNAL(clicked()), this,
                        SLOT(next()));
  ret = ret && connect(button(QWizard::NextButton), SIGNAL(clicked()), this,
                       SLOT(customNext()));
  ret = ret && disconnect(button(QWizard::BackButton), SIGNAL(clicked()), this,
                          SLOT(back()));
  ret = ret && connect(button(QWizard::BackButton), SIGNAL(clicked()), this,
                       SLOT(customBack()));
  ret = ret && disconnect(button(QWizard::CancelButton), SIGNAL(clicked()),
                          this, SLOT(reject()));
  ret = ret && connect(button(QWizard::CancelButton), SIGNAL(clicked()), this,
                       SLOT(customCancel()));
  if (!ret) throw TException();
}

//-----------------------------------------------------------------------------

void InitWizard::keyPressEvent(QKeyEvent *e) {
  if (e->key() != Qt::Key_Escape) QDialog::keyPressEvent(e);
}

//-----------------------------------------------------------------------------

void InitWizard::customBack() {
  switch (currentId()) {
  case FFMPEG_PAGE:
  case RHUBARB_PAGE:
    back();
    currentPage()->initializePage();
    return;
  default:
    back();
    return;
  }
}

//-----------------------------------------------------------------------------

void InitWizard::customNext() {
  switch (currentId()) {
  case FFMPEG_PAGE:
  case RHUBARB_PAGE:
    back();
    currentPage()->initializePage();
    return;
  default:
    next();
    return;
  }
}

//-----------------------------------------------------------------------------

void InitWizard::customCancel() {
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Initialization Wizard"), tr("Skip Wizard and Start OpenToonz?"),
                                QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    currentPage()->validatePage();
    QDialog::accept();
  }
}

//-----------------------------------------------------------------------------

}  // namespace OTZSetup
