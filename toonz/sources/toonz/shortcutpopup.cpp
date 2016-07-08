

#include "shortcutpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"

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
    CommandManager::instance()->setShortcut(m_action, "");
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

void ShortcutViewer::leaveEvent(QEvent *event) {
  update();
}

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

  m_list = new ShortcutTree(this);
  m_list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_sViewer   = new ShortcutViewer(this);
  m_removeBtn = new QPushButton(tr("Remove"), this);
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
      searchLay->addWidget(new QLabel("Search:", this), 0);
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

OpenPopupCommandHandler<ShortcutPopup> openShortcutPopup(MI_ShortcutPopup);
