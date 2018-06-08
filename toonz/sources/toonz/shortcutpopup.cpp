

#include "shortcutpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "tenv.h"
#include "tsystem.h"

#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/preferences.h"

// Qt includes
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSizePolicy>
#include <QPushButton>
#include <QPainter>
#include <QAction>
#include <QKeyEvent>
#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QApplication>
#include <QTextStream>
#include <QGroupBox>

// STD includes
#include <vector>

//=============================================================================
// ShortcutItem
// ------------
// Lo ShortcutTree visualizza ShortcutItem (organizzati in folder)
// ogni ShortcutItem rappresenta una QAction (e relativo Shortcut)
//-----------------------------------------------------------------------------

class ShortcutItem final : public QTreeWidgetItem {
  QAction *m_action;

public:
  ShortcutItem(QTreeWidgetItem *parent, QAction *action)
      : QTreeWidgetItem(parent, UserType), m_action(action) {
    setFlags(parent->flags());
    updateText();
  }
  void updateText() {
    QString text = m_action->text();
    text.remove("&");
    setText(0, text);
    QString shortcut = m_action->shortcut().toString();
    setText(1, shortcut);
  }
  QAction *getAction() const { return m_action; }
};

//=============================================================================
// ShortcutViewer
//-----------------------------------------------------------------------------

ShortcutViewer::ShortcutViewer(QWidget *parent) : QWidget(parent), m_action(0) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

//-----------------------------------------------------------------------------

ShortcutViewer::~ShortcutViewer() {}

//-----------------------------------------------------------------------------

void ShortcutViewer::paintEvent(QPaintEvent *) {
  QPainter p(this);
  // sfondo azzurro se il widget ha il focus (e quindi si accorge dei tasti
  // premuti)
  p.fillRect(1, 1, width() - 1, height() - 1,
             QBrush(hasFocus() ? QColor(171, 206, 255) : Qt::white));
  // bordo
  p.setPen(QColor(184, 188, 127));
  p.drawRect(0, 0, width() - 1, height() - 1);
  if (m_action) {
    // lo shortcut corrente
    p.setPen(Qt::black);
    p.drawText(10, 13, m_action->shortcut().toString());
  }
}

//-----------------------------------------------------------------------------

void ShortcutViewer::setAction(QAction *action) {
  m_action = action;
  update();
  setFocus();
}

//-----------------------------------------------------------------------------

bool ShortcutViewer::event(QEvent *event) {
  // quando si vuole assegnare una combinazione che gia' assegnata bisogna
  // evitare che lo shortcut relativo agisca.
  if (event->type() == QEvent::ShortcutOverride) {
    event->accept();
    return true;
  } else
    return QWidget::event(event);
}

//-----------------------------------------------------------------------------

void ShortcutViewer::keyPressEvent(QKeyEvent *event) {
  int key = event->key();
  if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt) {
    event->ignore();
    return;
  }
  Qt::KeyboardModifiers modifiers = event->modifiers();

  // Tasti che non possono essere utilizzati come shortcut
  if ((modifiers | (Qt::CTRL | Qt::SHIFT | Qt::ALT)) !=
          (Qt::CTRL | Qt::SHIFT | Qt::ALT) ||
      key == Qt::Key_Home || key == Qt::Key_End || key == Qt::Key_PageDown ||
      key == Qt::Key_PageUp || key == Qt::Key_Escape || key == Qt::Key_Print ||
      key == Qt::Key_Pause || key == Qt::Key_ScrollLock) {
    if (key != Qt::Key_Plus && key != Qt::Key_Minus &&
        key != Qt::Key_Asterisk && key != Qt::Key_Slash) {
      event->ignore();
      return;
    } else
      modifiers = 0;
  }

  // If "Use Numpad and Tab keys for Switching Styles" option is activated,
  // then prevent to assign such keys
  if (Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
      modifiers == 0 && (key >= Qt::Key_0 && key <= Qt::Key_9)) {
    event->ignore();
    return;
  }

  if (m_action) {
    CommandManager *cm = CommandManager::instance();
    QKeySequence keySequence(key + modifiers);
    std::string shortcutString = keySequence.toString().toStdString();
    QAction *oldAction =
        cm->getActionFromShortcut(keySequence.toString().toStdString());
    if (oldAction == m_action) return;
    if (oldAction) {
      QString msg = tr("%1 is already assigned to '%2'\nAssign to '%3'?")
                        .arg(keySequence.toString())
                        .arg(oldAction->iconText())
                        .arg(m_action->iconText());
      int ret = DVGui::MsgBox(msg, tr("Yes"), tr("No"), 1);
      activateWindow();
      if (ret == 2 || ret == 0) return;
    }
    CommandManager::instance()->setShortcut(m_action, shortcutString);
    emit shortcutChanged();
  }
  event->accept();
  update();
}

//-----------------------------------------------------------------------------

void ShortcutViewer::removeShortcut() {
  if (m_action) {
    CommandManager::instance()->setShortcut(m_action, "", false);
    emit shortcutChanged();
    update();
  }
}

//-----------------------------------------------------------------------------

void ShortcutViewer::enterEvent(QEvent *event) {
  setFocus();
  update();
}

//-----------------------------------------------------------------------------

void ShortcutViewer::leaveEvent(QEvent *event) { update(); }

//=============================================================================
// ShortcutTree
//-----------------------------------------------------------------------------

ShortcutTree::ShortcutTree(QWidget *parent) : QTreeWidget(parent) {
  setObjectName("ShortcutTree");
  setIndentation(14);
  setAlternatingRowColors(true);

  setColumnCount(2);
  header()->close();
  // setStyleSheet("border-bottom:1px solid rgb(120,120,120); border-left:1px
  // solid rgb(120,120,120); border-top:1px solid rgb(120,120,120)");

  QTreeWidgetItem *menuCommandFolder = new QTreeWidgetItem(this);
  menuCommandFolder->setText(0, tr("Menu Commands"));
  m_folders.push_back(menuCommandFolder);

  addFolder(tr("Fill"), FillCommandType);
  addFolder(tr("File"), MenuFileCommandType, menuCommandFolder);
  addFolder(tr("Edit"), MenuEditCommandType, menuCommandFolder);
  addFolder(tr("Scan & Cleanup"), MenuScanCleanupCommandType,
            menuCommandFolder);
  addFolder(tr("Level"), MenuLevelCommandType, menuCommandFolder);
  addFolder(tr("Xsheet"), MenuXsheetCommandType, menuCommandFolder);
  addFolder(tr("Cells"), MenuCellsCommandType, menuCommandFolder);
  addFolder(tr("View"), MenuViewCommandType, menuCommandFolder);
  addFolder(tr("Windows"), MenuWindowsCommandType, menuCommandFolder);

  addFolder(tr("Right-click Menu Commands"), RightClickMenuCommandType);

  addFolder(tr("Tools"), ToolCommandType);
  addFolder(tr("Tool Modifiers"), ToolModifierCommandType);
  addFolder(tr("Visualization"), ZoomCommandType);
  addFolder(tr("Misc"), MiscCommandType);
  addFolder(tr("Playback Controls"), PlaybackCommandType);
  addFolder(tr("RGBA Channels"), RGBACommandType);

  sortItems(0, Qt::AscendingOrder);

  connect(
      this, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
      this, SLOT(onCurrentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
}

//-----------------------------------------------------------------------------

ShortcutTree::~ShortcutTree() {}

//-----------------------------------------------------------------------------

void ShortcutTree::addFolder(const QString &title, int commandType,
                             QTreeWidgetItem *parentFolder) {
  QTreeWidgetItem *folder;
  if (!parentFolder) {
    folder = new QTreeWidgetItem(this);
    m_folders.push_back(folder);
  } else {
    folder = new QTreeWidgetItem(parentFolder);
    m_subFolders.push_back(folder);
  }
  assert(folder);
  folder->setText(0, title);

  std::vector<QAction *> actions;
  CommandManager::instance()->getActions((CommandType)commandType, actions);
  for (int i = 0; i < (int)actions.size(); i++) {
    ShortcutItem *item = new ShortcutItem(folder, actions[i]);
    m_items.push_back(item);
  }
}

//-----------------------------------------------------------------------------

void ShortcutTree::searchItems(const QString &searchWord) {
  if (searchWord.isEmpty()) {
    for (int i = 0; i < (int)m_items.size(); i++) m_items[i]->setHidden(false);
    for (int f = 0; f < m_subFolders.size(); f++) {
      m_subFolders[f]->setHidden(false);
      m_subFolders[f]->setExpanded(false);
    }
    for (int f = 0; f < m_folders.size(); f++) {
      m_folders[f]->setHidden(false);
      m_folders[f]->setExpanded(f == 0);
    }
    show();
    emit searched(true);
    update();
    return;
  }

  QList<QTreeWidgetItem *> foundItems =
      findItems(searchWord, Qt::MatchContains | Qt::MatchRecursive, 0);
  if (foundItems.isEmpty()) {
    hide();
    emit searched(false);
    update();
    return;
  }

  // show all matched items, hide all unmatched items
  for (int i = 0; i < (int)m_items.size(); i++)
    m_items[i]->setHidden(!foundItems.contains(m_items[i]));

  // hide folders which does not contain matched items
  // show and expand folders containing matched items
  bool found;
  for (int f = 0; f < m_subFolders.size(); f++) {
    QTreeWidgetItem *sf = m_subFolders.at(f);
    found               = false;
    for (int i = 0; i < sf->childCount(); i++) {
      if (!sf->child(i)->isHidden()) {
        found = true;
        break;
      }
    }
    sf->setHidden(!found);
    sf->setExpanded(found);
  }
  for (int f = 0; f < m_folders.size(); f++) {
    QTreeWidgetItem *fol = m_folders.at(f);
    found                = false;
    for (int i = 0; i < fol->childCount(); i++) {
      if (!fol->child(i)->isHidden()) {
        found = true;
        break;
      }
    }
    fol->setHidden(!found);
    fol->setExpanded(found);
  }

  show();
  emit searched(true);
  update();
}

//-----------------------------------------------------------------------------

void ShortcutTree::resizeEvent(QResizeEvent *event) {
  header()->resizeSection(0, width() - 120);
  header()->resizeSection(1, 120);
  QTreeView::resizeEvent(event);
}

//-----------------------------------------------------------------------------

void ShortcutTree::onCurrentItemChanged(QTreeWidgetItem *current,
                                        QTreeWidgetItem *previous) {
  ShortcutItem *item = dynamic_cast<ShortcutItem *>(current);
  emit actionSelected(item ? item->getAction() : 0);
}

//-----------------------------------------------------------------------------

void ShortcutTree::onShortcutChanged() {
  int i;
  for (i = 0; i < (int)m_items.size(); i++) m_items[i]->updateText();
}

//=============================================================================
// ShortcutPopup
//-----------------------------------------------------------------------------

ShortcutPopup::ShortcutPopup()
    : Dialog(TApp::instance()->getMainWindow(), false, false, "Shortcut") {
  setWindowTitle(tr("Configure Shortcuts"));
  m_presetChoiceCB = new QComboBox(this);
  buildPresets();

  m_presetChoiceCB->setCurrentIndex(0);
  m_list = new ShortcutTree(this);
  m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_sViewer            = new ShortcutViewer(this);
  m_removeBtn          = new QPushButton(tr("Remove"), this);
  m_loadShortcutsPopup = NULL;
  m_saveShortcutsPopup = NULL;
  m_dialog             = NULL;
  m_exportButton       = new QPushButton(tr("Export Current Shortcuts"), this);
  m_exportButton->setToolTip(tr("Export Current Shortcuts"));
  m_deletePresetButton = new QPushButton(tr("Delete"), this);
  m_deletePresetButton->setToolTip(tr("Delete Current Preset"));
  m_deletePresetButton->setIcon(QIcon(":Resources/delete_on.svg"));
  m_savePresetButton = new QPushButton(tr("Save As"), this);
  m_savePresetButton->setToolTip(tr("Save Current Shortcuts as New Preset"));
  m_savePresetButton->setIcon(QIcon(":Resources/saveas_on.svg"));
  m_loadPresetButton = new QPushButton(tr("Load"));
  m_loadPresetButton->setToolTip(tr("Use selected preset as shortcuts"));
  m_loadPresetButton->setIcon(QIcon(":Resources/green.png"));
  QGroupBox *presetBox = new QGroupBox(tr("Shortcut Presets"), this);
  presetBox->setObjectName("SolidLineFrame");
  m_clearAllShortcutsButton = new QPushButton(tr("Clear All Shortcuts"));
  QLabel *noSearchResultLabel =
      new QLabel(tr("Couldn't find any matching command."), this);
  noSearchResultLabel->setHidden(true);

  QLineEdit *searchEdit = new QLineEdit(this);

  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(8);
  {
    QHBoxLayout *searchLay = new QHBoxLayout();
    searchLay->setMargin(0);
    searchLay->setSpacing(5);
    {
      searchLay->addWidget(new QLabel(tr("Search:"), this), 0);
      searchLay->addWidget(searchEdit);
    }
    m_topLayout->addLayout(searchLay, 0);

    QVBoxLayout *listLay = new QVBoxLayout();
    listLay->setMargin(0);
    listLay->setSpacing(0);
    {
      listLay->addWidget(noSearchResultLabel, 0,
                         Qt::AlignTop | Qt::AlignHCenter);
      listLay->addWidget(m_list, 1);
    }
    m_topLayout->addLayout(listLay, 1);

    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setMargin(0);
    bottomLayout->setSpacing(1);
    {
      bottomLayout->addWidget(m_sViewer, 1);
      bottomLayout->addWidget(m_removeBtn, 0);
    }
    m_topLayout->addLayout(bottomLayout, 0);
    m_topLayout->addSpacing(10);
    QHBoxLayout *presetLay = new QHBoxLayout();
    presetLay->setMargin(5);
    presetLay->setSpacing(5);
    {
      presetLay->addWidget(new QLabel(tr("Preset:"), this), 0);
      presetLay->addWidget(m_presetChoiceCB, 1);
      presetLay->addWidget(m_loadPresetButton, 0);
      presetLay->addWidget(m_savePresetButton, 0);
      presetLay->addWidget(m_deletePresetButton, 0);
    }
    presetBox->setLayout(presetLay);
    m_topLayout->addWidget(presetBox, 0, Qt::AlignCenter);
    m_topLayout->addSpacing(10);
    QHBoxLayout *exportLay = new QHBoxLayout();
    exportLay->setMargin(0);
    exportLay->setSpacing(5);
    {
      exportLay->addWidget(m_exportButton, 0);
      exportLay->addWidget(m_clearAllShortcutsButton, 0);
    }
    m_topLayout->addLayout(exportLay, 0);
    // m_topLayout->addWidget(m_exportButton, 0);
  }

  connect(m_list, SIGNAL(actionSelected(QAction *)), m_sViewer,
          SLOT(setAction(QAction *)));

  connect(m_removeBtn, SIGNAL(clicked()), m_sViewer, SLOT(removeShortcut()));

  connect(m_sViewer, SIGNAL(shortcutChanged()), m_list,
          SLOT(onShortcutChanged()));

  connect(m_list, SIGNAL(searched(bool)), noSearchResultLabel,
          SLOT(setHidden(bool)));
  connect(searchEdit, SIGNAL(textChanged(const QString &)), this,
          SLOT(onSearchTextChanged(const QString &)));
  connect(m_presetChoiceCB, SIGNAL(currentIndexChanged(int)),
          SLOT(onPresetChanged(int)));
  connect(m_exportButton, SIGNAL(clicked()), SLOT(onExportButton()));
  connect(m_deletePresetButton, SIGNAL(clicked()), SLOT(onDeletePreset()));
  connect(m_savePresetButton, SIGNAL(clicked()), SLOT(onSavePreset()));
  connect(m_loadPresetButton, SIGNAL(clicked()), SLOT(onLoadPreset()));
  connect(m_clearAllShortcutsButton, SIGNAL(clicked()),
          SLOT(clearAllShortcuts()));
}

//-----------------------------------------------------------------------------

ShortcutPopup::~ShortcutPopup() {}

//-----------------------------------------------------------------------------

void ShortcutPopup::onSearchTextChanged(const QString &text) {
  static bool busy = false;
  if (busy) return;
  busy = true;
  m_list->searchItems(text);
  busy = false;
}

//-----------------------------------------------------------------------------

void ShortcutPopup::onPresetChanged(int index) {
  if (m_presetChoiceCB->currentText() == "Load from file...") {
    importPreset();
  }
}

//-----------------------------------------------------------------------------

void ShortcutPopup::clearAllShortcuts(bool warning) {
  if (warning) {
    QString question(tr("This will erase ALL shortcuts. Continue?"));
    int ret =
        DVGui::MsgBox(question, QObject::tr("OK"), QObject::tr("Cancel"), 0);
    if (ret == 0 || ret == 2) {
      // cancel (or closed message box window)
      return;
    }
    showDialog("Clearing All Shortcuts");
  }
  for (int commandType = UndefinedCommandType; commandType <= MenuCommandType;
       commandType++) {
    std::vector<QAction *> actions;
    CommandManager::instance()->getActions((CommandType)commandType, actions);
    for (QAction *action : actions) {
      qApp->processEvents();
      m_sViewer->setAction(action);
      m_sViewer->removeShortcut();
    }
  }
  setCurrentPresetPref("DELETED");
  // if warning is true, this was called directly- need to hide the dialog after
  if (warning) m_dialog->hide();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::setPresetShortcuts(TFilePath fp) {
  QSettings preset(toQString(fp), QSettings::IniFormat);
  preset.beginGroup("shortcuts");
  QStringList allIds = preset.allKeys();
  QAction *action;
  for (QString id : allIds) {
    QByteArray ba      = id.toLatin1();
    const char *charId = ba.data();
    action = CommandManager::instance()->getAction((CommandId)charId);
    if (!action) continue;
    CommandManager::instance()->setShortcut(
        action, preset.value(id).toString().toStdString(), false);
  }
  preset.endGroup();
  emit m_sViewer->shortcutChanged();
  m_dialog->hide();
  buildPresets();
  setCurrentPresetPref(QString::fromStdString(fp.getName()));
}

//-----------------------------------------------------------------------------

bool ShortcutPopup::showConfirmDialog() {
  QString question(tr("This will overwrite all current shortcuts. Continue?"));
  int ret =
      DVGui::MsgBox(question, QObject::tr("OK"), QObject::tr("Cancel"), 0);
  if (ret == 0 || ret == 2) {
    // cancel (or closed message box window)
    return false;
  } else
    return true;
}

//-----------------------------------------------------------------------------

bool ShortcutPopup::showOverwriteDialog(QString name) {
  QString question(tr("A file named ") + name +
                   tr(" already exists.  Do you want to replace it?"));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"), 0);
  if (ret == 0 || ret == 2) {
    // cancel (or closed message box window)
    return false;
  } else
    return true;
}

//-----------------------------------------------------------------------------

void ShortcutPopup::showDialog(QString text) {
  if (m_dialog == NULL) {
    m_dialogLabel = new QLabel("", this);
    m_dialog      = new DVGui::Dialog(this, false, false);
    m_dialog->setWindowTitle(tr("OpenToonz - Setting Shortcuts"));
    m_dialog->setModal(false);

    m_dialog->setTopMargin(10);
    m_dialog->setTopSpacing(10);
    m_dialog->setLabelWidth(500);
    m_dialog->beginVLayout();
    m_dialog->addWidget(m_dialogLabel, false);
    m_dialog->endVLayout();
  }
  m_dialogLabel->setText(text);
  m_dialog->show();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::onExportButton(TFilePath fp) {
  if (fp == TFilePath()) {
    m_saveShortcutsPopup = new GenericSaveFilePopup("Save Current Shortcuts");
    m_saveShortcutsPopup->addFilterType("ini");
    fp = m_saveShortcutsPopup->getPath();
    if (fp == TFilePath()) return;
  }
  showDialog(tr("Saving Shortcuts"));
  QString shortcutString = "[shortcuts]\n";
  for (int commandType = UndefinedCommandType; commandType <= MenuCommandType;
       commandType++) {
    std::vector<QAction *> actions;
    CommandManager::instance()->getActions((CommandType)commandType, actions);
    for (QAction *action : actions) {
      qApp->processEvents();
      std::string id = CommandManager::instance()->getIdFromAction(action);
      std::string shortcut =
          CommandManager::instance()->getShortcutFromAction(action);
      if (shortcut != "") {
        shortcutString = shortcutString + QString::fromStdString(id) + "=" +
                         QString::fromStdString(shortcut) + "\n";
      }
    }
  }
  QFile file(fp.getQString());
  file.open(QIODevice::WriteOnly | QIODevice::Text);
  QTextStream out(&file);
  out << shortcutString;
  file.close();
  m_dialog->hide();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::onDeletePreset() {
  // change this to 4 once RETAS shortcuts are updated
  if (m_presetChoiceCB->currentIndex() <= 3) {
    DVGui::MsgBox(DVGui::CRITICAL, tr("Included presets cannot be deleted."));
    return;
  }

  QString question(tr("Are you sure you want to delete the preset: ") +
                   m_presetChoiceCB->currentText() + tr("?"));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"), 0);
  if (ret == 0 || ret == 2) {
    // cancel (or closed message box window)
    return;
  }
  TFilePath presetDir =
      ToonzFolder::getMyModuleDir() + TFilePath("shortcutpresets");
  QString presetName = m_presetChoiceCB->currentText();
  if (TSystem::doesExistFileOrLevel(presetDir +
                                    TFilePath(presetName + ".ini"))) {
    TSystem::deleteFile(presetDir + TFilePath(presetName + ".ini"));
    buildPresets();
    m_presetChoiceCB->setCurrentIndex(0);
  }
  if (Preferences::instance()->getShortcutPreset() == presetName)
    setCurrentPresetPref("DELETED");
  getCurrentPresetPref();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::importPreset() {
  m_loadShortcutsPopup = new GenericLoadFilePopup("Load Shortcuts File");
  m_loadShortcutsPopup->addFilterType("ini");
  TFilePath shortcutPath = m_loadShortcutsPopup->getPath();
  if (shortcutPath == TFilePath()) {
    m_presetChoiceCB->setCurrentIndex(0);
    return;
  }
  if (!showConfirmDialog()) return;

  TFilePath presetDir =
      ToonzFolder::getMyModuleDir() + TFilePath("shortcutpresets");
  if (!TSystem::doesExistFileOrLevel(presetDir)) {
    TSystem::mkDir(presetDir);
  }
  QString name        = shortcutPath.withoutParentDir().getQString();
  std::string strName = name.toStdString();
  if (TSystem::doesExistFileOrLevel(presetDir + TFilePath(name))) {
    if (!showOverwriteDialog(name)) return;
  }
  showDialog("Importing Shortcuts");
  TSystem::copyFile(presetDir + TFilePath(name), shortcutPath, true);
  clearAllShortcuts(false);
  setPresetShortcuts(presetDir + TFilePath(name));
  return;
}

//-----------------------------------------------------------------------------

void ShortcutPopup::onLoadPreset() {
  QString preset = m_presetChoiceCB->currentText();
  TFilePath presetDir =
      ToonzFolder::getMyModuleDir() + TFilePath("shortcutpresets");
  TFilePath defaultPresetDir =
      ToonzFolder::getProfileFolder() + TFilePath("layouts/shortcuts");
  if (preset == "") return;
  if (preset == "Load from file...") {
    importPreset();
    return;
  }

  if (!showConfirmDialog()) return;
  showDialog(tr("Setting Shortcuts"));
  if (preset == "OpenToonz") {
    clearAllShortcuts(false);
    TFilePath fp = defaultPresetDir + TFilePath("defopentoonz.ini");
    setPresetShortcuts(fp);
    return;
  } else if (preset == "Toon Boom Harmony") {
    clearAllShortcuts(false);
    TFilePath fp = defaultPresetDir + TFilePath("otharmony.ini");
    setPresetShortcuts(fp);
    return;
  } else if (preset == "Adobe Animate(Flash)") {
    clearAllShortcuts(false);
    TFilePath fp = defaultPresetDir + TFilePath("otadobe.ini");
    setPresetShortcuts(fp);
    return;
  } else if (preset == "RETAS PaintMan") {
    clearAllShortcuts(false);
    TFilePath fp = defaultPresetDir + TFilePath("otretas.ini");
    setPresetShortcuts(fp);
    return;
  } else if (TSystem::doesExistFileOrLevel(presetDir +
                                           TFilePath(preset + ".ini"))) {
    clearAllShortcuts(false);
    TFilePath fp = presetDir + TFilePath(preset + ".ini");
    setPresetShortcuts(fp);
    return;
  }
  m_dialog->hide();
}

//-----------------------------------------------------------------------------

QStringList ShortcutPopup::buildPresets() {
  QStringList presets;
  presets << ""
          << "OpenToonz"
          //<< "RETAS PaintMan"
          << "Toon Boom Harmony"
          << "Adobe Animate(Flash)";
  TFilePath presetDir =
      ToonzFolder::getMyModuleDir() + TFilePath("shortcutpresets");
  if (TSystem::doesExistFileOrLevel(presetDir)) {
    TFilePathSet fps = TSystem::readDirectory(presetDir, true, true, false);
    QStringList customPresets;
    for (TFilePath fp : fps) {
      if (fp.getType() == "ini") {
        customPresets << QString::fromStdString(fp.getName());
        std::string name = fp.getName();
      }
    }
    customPresets.sort();
    presets = presets + customPresets;
  }
  presets << tr("Load from file...");
  m_presetChoiceCB->clear();
  m_presetChoiceCB->addItems(presets);
  return presets;
}

//-----------------------------------------------------------------------------

void ShortcutPopup::onSavePreset() {
  QString presetName =
      DVGui::getText(tr("Enter Preset Name"), tr("Preset Name:"), "");
  if (presetName == "") return;
  TFilePath presetDir =
      ToonzFolder::getMyModuleDir() + TFilePath("shortcutpresets");
  if (!TSystem::doesExistFileOrLevel(presetDir)) {
    TSystem::mkDir(presetDir);
  }
  TFilePath fp;
  fp = presetDir + TFilePath(presetName + ".ini");
  if (TSystem::doesExistFileOrLevel(fp)) {
    if (!showOverwriteDialog(QString::fromStdString(fp.getName()))) return;
  }
  onExportButton(fp);

  buildPresets();
  setCurrentPresetPref(presetName);
}

//-----------------------------------------------------------------------------

void ShortcutPopup::showEvent(QShowEvent *se) { getCurrentPresetPref(); }

//-----------------------------------------------------------------------------

void ShortcutPopup::setCurrentPresetPref(QString name) {
  Preferences::instance()->setShortcutPreset(name.toStdString());
  getCurrentPresetPref();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::getCurrentPresetPref() {
  QString name = Preferences::instance()->getShortcutPreset();
  if (name == "DELETED")
    m_presetChoiceCB->setCurrentText("");
  else if (name == "defopentoonz")
    m_presetChoiceCB->setCurrentText("OpenToonz");
  else if (name == "otharmony")
    m_presetChoiceCB->setCurrentText("Toon Boom Harmony");
  else if (name == "otadobe")
    m_presetChoiceCB->setCurrentText("Adobe Animate(Flash)");
  else if (name == "otretas")
    m_presetChoiceCB->setCurrentText("RETAS PaintMan");

  else
    m_presetChoiceCB->setCurrentText(name);
}

OpenPopupCommandHandler<ShortcutPopup> openShortcutPopup(MI_ShortcutPopup);
