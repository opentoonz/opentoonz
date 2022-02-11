

#include "shortcutpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "tenv.h"
#include "tsystem.h"

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
    // removing accelerator key indicator
    text = text.replace(QRegExp("&([^& ])"), "\\1");
    // removing doubled &s
    text = text.replace("&&", "&");
    setText(0, text);
    QString shortcut = m_action->shortcut().toString();
    setText(1, shortcut);
  }
  QAction *getAction() const { return m_action; }
};

//=============================================================================
// ShortcutViewer
//-----------------------------------------------------------------------------

ShortcutViewer::ShortcutViewer(QWidget *parent)
    : QKeySequenceEdit(parent), m_action(0) {
  setObjectName("ShortcutViewer");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  connect(this, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));
}

//-----------------------------------------------------------------------------

ShortcutViewer::~ShortcutViewer() {}

//-----------------------------------------------------------------------------

void ShortcutViewer::setAction(QAction *action) {
  m_action = action;
  if (m_action) {
    setEnabled(true);
    setKeySequence(m_action->shortcut());
    setFocus();
  } else {
    setEnabled(false);
    setKeySequence(QKeySequence());
  }
}

//-----------------------------------------------------------------------------

void ShortcutViewer::keyPressEvent(QKeyEvent *event) {
  int key                         = event->key();
  Qt::KeyboardModifiers modifiers = event->modifiers();
  if (key == Qt::Key_Home || key == Qt::Key_End || key == Qt::Key_PageDown ||
      key == Qt::Key_PageUp || key == Qt::Key_Escape || key == Qt::Key_Print ||
      key == Qt::Key_Pause || key == Qt::Key_ScrollLock) {
    event->ignore();
    return;
  }
  // If "Use Numpad and Tab keys for Switching Styles" option is activated,
  // then prevent to assign such keys
  if (Preferences::instance()->isUseNumpadForSwitchingStylesEnabled() &&
      modifiers == 0 && (key >= Qt::Key_0 && key <= Qt::Key_9)) {
    event->ignore();
    return;
  }
  QKeySequenceEdit::keyPressEvent(event);
}

//-----------------------------------------------------------------------------

void ShortcutViewer::onEditingFinished() {
  // limit to one shortcut key input
  QKeySequence keys = (keySequence().isEmpty())
                          ? QKeySequence()
                          : QKeySequence(keySequence()[0]);

  if (m_action) {
    CommandManager *cm         = CommandManager::instance();
    std::string shortcutString = keys.toString().toStdString();
    QAction *oldAction =
        cm->getActionFromShortcut(keys.toString().toStdString());
    if (oldAction == m_action) return;
    if (oldAction) {
      QString msg = tr("%1 is already assigned to '%2'\nAssign to '%3'?")
                        .arg(keys.toString())
                        .arg(oldAction->iconText())
                        .arg(m_action->iconText());
      int ret = DVGui::MsgBox(msg, tr("Yes"), tr("No"), 1);
      activateWindow();
      if (ret == 2 || ret == 0) {
        setKeySequence(m_action->shortcut());
        setFocus();
        return;
      }
    }
    CommandManager::instance()->setShortcut(m_action, shortcutString);
    emit shortcutChanged();
  }
  setKeySequence(keys);
}

//-----------------------------------------------------------------------------

void ShortcutViewer::removeShortcut() {
  if (m_action) {
    CommandManager::instance()->setShortcut(m_action, "", false);
    emit shortcutChanged();
    clear();
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
  addFolder(tr("Play"), MenuPlayCommandType, menuCommandFolder);
  addFolder(tr("Render"), MenuRenderCommandType, menuCommandFolder);
  addFolder(tr("View"), MenuViewCommandType, menuCommandFolder);
  addFolder(tr("Windows"), MenuWindowsCommandType, menuCommandFolder);
  addFolder(tr("Help"), MenuHelpCommandType, menuCommandFolder);

  addFolder(tr("Right-click Menu Commands"), RightClickMenuCommandType);
  QTreeWidgetItem *rcmSubFolder = m_folders.back();
  addFolder(tr("Cell Mark"), CellMarkCommandType, rcmSubFolder);

  addFolder(tr("Tools"), ToolCommandType);
  addFolder(tr("Tool Modifiers"), ToolModifierCommandType);
  addFolder(tr("Stop Motion"), StopMotionCommandType);
  addFolder(tr("Visualization"), ZoomCommandType);
  addFolder(tr("Misc"), MiscCommandType);
  addFolder(tr("RGBA Channels"), RGBACommandType);

  sortItems(0, Qt::AscendingOrder);

  connect(
      this, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
      this, SLOT(onCurrentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
  connect(this, SIGNAL(clicked(const QModelIndex &)), this,
          SLOT(onItemClicked(const QModelIndex &)));
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

//-----------------------------------------------------------------------------

void ShortcutTree::onItemClicked(const QModelIndex &index) {
  isExpanded(index) ? collapse(index) : expand(index);
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
  m_deletePresetButton->setIcon(createQIcon("delete"));
  m_savePresetButton = new QPushButton(tr("Save As"), this);
  m_savePresetButton->setToolTip(tr("Save Current Shortcuts as New Preset"));
  m_savePresetButton->setIcon(createQIcon("saveas"));
  m_loadPresetButton = new QPushButton(tr("Load"));
  m_loadPresetButton->setToolTip(tr("Use selected preset as shortcuts"));
  m_loadPresetButton->setIcon(createQIcon("load"));
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
          SLOT(onPresetChanged()));
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

void ShortcutPopup::onPresetChanged() {
  if (m_presetChoiceCB->currentData().toString() == QString("LoadFromFile")) {
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

  QSettings preset(toQString(fp), QSettings::IniFormat);
  preset.beginGroup("shortcuts");

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
        preset.setValue(QString::fromStdString(id),
                        QString::fromStdString(shortcut));
      }
    }
  }

  preset.endGroup();

  m_dialog->hide();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::onDeletePreset() {
  // change this to 5 once RETAS shortcuts are updated
  if (m_presetChoiceCB->currentIndex() <= 4) {
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
  QString presetName = m_presetChoiceCB->currentData().toString();
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
  QString preset = m_presetChoiceCB->currentData().toString();
  TFilePath presetDir;
  if (m_presetChoiceCB->currentIndex() <= 4)
    presetDir =
        ToonzFolder::getProfileFolder() + TFilePath("layouts/shortcuts");
  else
    presetDir = ToonzFolder::getMyModuleDir() + TFilePath("shortcutpresets");

  if (preset.isEmpty()) return;
  if (preset == QString("LoadFromFile")) {
    importPreset();
    return;
  }

  if (!showConfirmDialog()) return;
  showDialog(tr("Setting Shortcuts"));
  TFilePath presetFilePath(preset + ".ini");
  if (TSystem::doesExistFileOrLevel(presetDir + presetFilePath)) {
    clearAllShortcuts(false);
    TFilePath fp = presetDir + presetFilePath;
    setPresetShortcuts(fp);
    return;
  }
  m_dialog->hide();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::buildPresets() {
  m_presetChoiceCB->clear();

  m_presetChoiceCB->addItem("", QString(""));
  m_presetChoiceCB->addItem("OpenToonz", QString("defopentoonz"));
  // m_presetChoiceCB->addItem("RETAS PaintMan", QString("otretas"));
  m_presetChoiceCB->addItem("Toon Boom Harmony", QString("otharmony"));
  m_presetChoiceCB->addItem("Adobe Animate", QString("otanimate"));
  m_presetChoiceCB->addItem("Adobe Flash Pro", QString("otadobe"));

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
    for (auto customPreset : customPresets)
      m_presetChoiceCB->addItem(customPreset, customPreset);
  }
  m_presetChoiceCB->addItem(tr("Load from file..."), QString("LoadFromFile"));
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
  Preferences::instance()->setValue(shortcutPreset, name);
  getCurrentPresetPref();
}

//-----------------------------------------------------------------------------

void ShortcutPopup::getCurrentPresetPref() {
  QString name = Preferences::instance()->getShortcutPreset();
  if (name == "DELETED") name = "";

  m_presetChoiceCB->setCurrentIndex(m_presetChoiceCB->findData(name));
}

OpenPopupCommandHandler<ShortcutPopup> openShortcutPopup(MI_ShortcutPopup);
