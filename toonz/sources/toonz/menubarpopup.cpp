#include "menubarpopup.h"

// Tnz includes
#include "tapp.h"
#include "mainwindow.h"
#include "menubar.h"
#include "shortcutpopup.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QtDebug>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDataStream>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>
#include <QLabel>

//=============================================================================
// MenuBarCommandItem
//-----------------------------------------------------------------------------

class MenuBarCommandItem final : public QTreeWidgetItem {
  QAction* m_action;

public:
  MenuBarCommandItem(QTreeWidgetItem* parent, QAction* action)
      : QTreeWidgetItem(parent, UserType), m_action(action) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled |
             Qt::ItemNeverHasChildren);
    setText(0, m_action->text().remove("&"));
    setToolTip(0, QObject::tr("[Drag] to move position"));
  }
  QAction* getAction() const { return m_action; }
};

//=============================================================================
// MenuBarSeparatorItem
//-----------------------------------------------------------------------------

class MenuBarSeparatorItem final : public QTreeWidgetItem {
public:
  MenuBarSeparatorItem(QTreeWidgetItem* parent)
      : QTreeWidgetItem(parent, UserType) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled |
             Qt::ItemNeverHasChildren);
    setText(0, QObject::tr("----Separator----"));
    setToolTip(0, QObject::tr("[Drag] to move position"));
  }
};

//=============================================================================
// MenuBarSubmenuItem
//-----------------------------------------------------------------------------

class MenuBarSubmenuItem final : public QTreeWidgetItem {
  /*- title before translation -*/
  QString m_orgTitle;

public:
  MenuBarSubmenuItem(QTreeWidgetItem* parent, QString& title)
      : QTreeWidgetItem(parent, UserType), m_orgTitle(title) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled |
             Qt::ItemIsDropEnabled | Qt::ItemIsEnabled);
    /*- Menu title will be translated if the title is registered in translation
     * file -*/
    setText(0, StackedMenuBar::tr(title.toStdString().c_str()));
    QIcon subMenuIcon(":Resources/browser_folder_close.svg");
    subMenuIcon.addFile(":Resources/browser_folder_open.svg", QSize(),
                        QIcon::Normal, QIcon::On);
    setIcon(0, subMenuIcon);
    setToolTip(0, QObject::tr(
                      "[Drag] to move position, [Double Click] to edit title"));
  }

  QString getOrgTitle() const { return m_orgTitle; }
  void setOrgTitle(const QString title) { m_orgTitle = title; }
};

//=============================================================================
// MenuBarTree
//-----------------------------------------------------------------------------

MenuBarTree::MenuBarTree(TFilePath& path, QWidget* parent)
    : QTreeWidget(parent), m_path(path) {
  setObjectName("SolidLineFrame");
  setAlternatingRowColors(true);
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);
  setDragDropMode(QAbstractItemView::DragDrop);
  setIconSize(QSize(21, 17));

  setColumnCount(1);
  header()->close();

  /*- Load m_path if it does exist. If not, then load from the template. -*/
  TFilePath fp;
  if (TFileStatus(path).isWritable())
    fp = m_path;
  else {
    fp = m_path.withParentDir(ToonzFolder::getTemplateRoomsDir());
    if (!TFileStatus(path).isReadable())
      fp = ToonzFolder::getTemplateRoomsDir() + "menubar_template.xml";
  }

  loadMenuTree(fp);

  bool ret = connect(this, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this,
                     SLOT(onItemChanged(QTreeWidgetItem*, int)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void MenuBarTree::loadMenuTree(const TFilePath& fp) {
  QFile file(toQString(fp));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "menubar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "menu") {
          QString title = reader.attributes().value("title").toString();
          MenuBarSubmenuItem* menu = new MenuBarSubmenuItem(0, title);
          addTopLevelItem(menu);
          loadMenuRecursive(reader, menu);
        } else if (reader.name() == "command") {
          QString cmdName = reader.readElementText();

          QAction* action = CommandManager::instance()->getAction(
              cmdName.toStdString().c_str());
          if (action) {
            MenuBarCommandItem* item = new MenuBarCommandItem(0, action);
            addTopLevelItem(item);
          }
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  }

  if (reader.hasError()) {
    qDebug() << "Cannot read menubar xml";
  }
}

//-----------------------------------------------------------------------------

void MenuBarTree::loadMenuRecursive(QXmlStreamReader& reader,
                                    QTreeWidgetItem* parentItem) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "menu") {
      QString title = reader.attributes().value("title").toString();
      MenuBarSubmenuItem* subMenu = new MenuBarSubmenuItem(parentItem, title);
      loadMenuRecursive(reader, subMenu);
    } else if (reader.name() == "command") {
      QString cmdName = reader.readElementText();
      QAction* action =
          CommandManager::instance()->getAction(cmdName.toStdString().c_str());
      if (action)
        MenuBarCommandItem* item = new MenuBarCommandItem(parentItem, action);
    } else if (reader.name() == "command_debug") {
#ifndef NDEBUG
      QString cmdName = reader.readElementText();
      QAction* action =
          CommandManager::instance()->getAction(cmdName.toStdString().c_str());
      if (action)
        MenuBarCommandItem* item = new MenuBarCommandItem(parentItem, action);
#else
      reader.skipCurrentElement();
#endif
    } else if (reader.name() == "separator") {
      MenuBarSeparatorItem* sep = new MenuBarSeparatorItem(parentItem);
      reader.skipCurrentElement();
    } else
      reader.skipCurrentElement();
  }
}

//-----------------------------------------------------------------------------

void MenuBarTree::saveMenuTree() {
  QFile file(toQString(m_path));
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return;
  }

  QXmlStreamWriter writer(&file);
  writer.setAutoFormatting(true);
  writer.writeStartDocument();

  writer.writeStartElement("menubar");
  { saveMenuRecursive(writer, invisibleRootItem()); }
  writer.writeEndElement();  // menubar

  writer.writeEndDocument();
}

//-----------------------------------------------------------------------------

void MenuBarTree::saveMenuRecursive(QXmlStreamWriter& writer,
                                    QTreeWidgetItem* parentItem) {
  for (int c = 0; c < parentItem->childCount(); c++) {
    MenuBarCommandItem* command =
        dynamic_cast<MenuBarCommandItem*>(parentItem->child(c));
    MenuBarSeparatorItem* sep =
        dynamic_cast<MenuBarSeparatorItem*>(parentItem->child(c));
    MenuBarSubmenuItem* subMenu =
        dynamic_cast<MenuBarSubmenuItem*>(parentItem->child(c));
    if (command)
      writer.writeTextElement(
          "command",
          QString::fromStdString(CommandManager::instance()->getIdFromAction(
              command->getAction())));
    else if (sep)
      writer.writeEmptyElement("separator");
    else if (subMenu) {
      writer.writeStartElement("menu");
      // save original title instead of translated one
      writer.writeAttribute("title", subMenu->getOrgTitle());

      saveMenuRecursive(writer, subMenu);

      writer.writeEndElement();  // menu
    } else {
    }
  }
}

//-----------------------------------------------------------------------------

bool MenuBarTree::dropMimeData(QTreeWidgetItem* parent, int index,
                               const QMimeData* data, Qt::DropAction action) {
  if (data->hasText()) {
    QString txt = data->text();
    QTreeWidgetItem* item;
    if (txt == "separator")
      item = new MenuBarSeparatorItem(0);
    else {
      QAction* act =
          CommandManager::instance()->getAction(txt.toStdString().c_str());
      if (!act) return false;
      item = new MenuBarCommandItem(0, act);
    }

    if (parent)
      parent->insertChild(index, item);
    else
      insertTopLevelItem(index, item);

    return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

QStringList MenuBarTree::mimeTypes() const {
  QStringList qstrList;
  qstrList.append("text/plain");
  return qstrList;
}

//-----------------------------------------------------------------------------

void MenuBarTree::contextMenuEvent(QContextMenuEvent* event) {
  QTreeWidgetItem* item = itemAt(event->pos());
  if (item != currentItem()) setCurrentItem(item);
  QMenu* menu = new QMenu(this);
  QAction* action;
  if (!item || indexOfTopLevelItem(item) >= 0)
    action = menu->addAction(tr("Insert Menu"));
  else
    action = menu->addAction(tr("Insert Submenu"));

  connect(action, SIGNAL(triggered()), this, SLOT(insertMenu()));

  if (item) {
    action = menu->addAction(tr("Remove \"%1\"").arg(item->text(0)));
    connect(action, SIGNAL(triggered()), this, SLOT(removeItem()));
  }

  menu->exec(event->globalPos());
  delete menu;
}

//-----------------------------------------------------------------------------

void MenuBarTree::insertMenu() {
  QTreeWidgetItem* item       = currentItem();
  QString title               = tr("New Menu");
  MenuBarSubmenuItem* insItem = new MenuBarSubmenuItem(0, title);
  if (!item)
    addTopLevelItem(insItem);
  else if (indexOfTopLevelItem(item) >= 0)
    insertTopLevelItem(indexOfTopLevelItem(item), insItem);
  else
    item->parent()->insertChild(item->parent()->indexOfChild(item), insItem);
}

//-----------------------------------------------------------------------------

void MenuBarTree::removeItem() {
  QTreeWidgetItem* item = currentItem();
  if (!item) return;

  if (indexOfTopLevelItem(item) >= 0)
    takeTopLevelItem(indexOfTopLevelItem(item));
  else
    item->parent()->removeChild(item);

  delete item;
}

//-----------------------------------------------------------------------------

void MenuBarTree::onItemChanged(QTreeWidgetItem* item, int column) {
  MenuBarSubmenuItem* submenuItem = dynamic_cast<MenuBarSubmenuItem*>(item);
  if (!submenuItem) return;
  submenuItem->setOrgTitle(submenuItem->text(0));
}

//=============================================================================
// CommandListTree
//-----------------------------------------------------------------------------

CommandListTree::CommandListTree(QWidget* parent) : QTreeWidget(parent) {
  setObjectName("SolidLineFrame");
  setAlternatingRowColors(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setColumnCount(1);
  setIconSize(QSize(21, 17));
  header()->close();

  QIcon menuFolderIcon(":Resources/browser_project_close.svg");
  menuFolderIcon.addFile(":Resources/browser_project_open.svg", QSize(),
                         QIcon::Normal, QIcon::On);
  invisibleRootItem()->setIcon(0, menuFolderIcon);

  QTreeWidgetItem* menuCommandFolder = new QTreeWidgetItem(this);
  menuCommandFolder->setFlags(Qt::ItemIsEnabled);
  menuCommandFolder->setText(0, ShortcutTree::tr("Menu Commands"));
  menuCommandFolder->setExpanded(true);
  menuCommandFolder->setIcon(0, invisibleRootItem()->icon(0));

  addFolder(ShortcutTree::tr("File"), MenuFileCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Edit"), MenuEditCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Scan & Cleanup"), MenuScanCleanupCommandType,
            menuCommandFolder);
  addFolder(ShortcutTree::tr("Level"), MenuLevelCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Xsheet"), MenuXsheetCommandType,
            menuCommandFolder);
  addFolder(ShortcutTree::tr("Cells"), MenuCellsCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("View"), MenuViewCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Windows"), MenuWindowsCommandType,
            menuCommandFolder);

  addFolder(ShortcutTree::tr("Tools"), ToolCommandType);
  addFolder(ShortcutTree::tr("Playback"), PlaybackCommandType);

  MenuBarSeparatorItem* sep = new MenuBarSeparatorItem(0);
  sep->setToolTip(0, QObject::tr("[Drag&Drop] to copy separator to menu bar"));
  addTopLevelItem(sep);
}

//-----------------------------------------------------------------------------

void CommandListTree::addFolder(const QString& title, int commandType,
                                QTreeWidgetItem* parentFolder) {
  QTreeWidgetItem* folder;
  if (!parentFolder)
    folder = new QTreeWidgetItem(this);
  else
    folder = new QTreeWidgetItem(parentFolder);
  assert(folder);
  folder->setText(0, title);
  folder->setIcon(0, invisibleRootItem()->icon(0));

  std::vector<QAction*> actions;
  CommandManager::instance()->getActions((CommandType)commandType, actions);
  for (int i = 0; i < (int)actions.size(); i++) {
    MenuBarCommandItem* item = new MenuBarCommandItem(folder, actions[i]);
    item->setToolTip(0, QObject::tr("[Drag&Drop] to copy command to menu bar"));
  }
}

//-----------------------------------------------------------------------------

void CommandListTree::mousePressEvent(QMouseEvent* event) {
  setCurrentItem(itemAt(event->pos()));
  MenuBarCommandItem* commandItem =
      dynamic_cast<MenuBarCommandItem*>(itemAt(event->pos()));
  MenuBarSeparatorItem* separatorItem =
      dynamic_cast<MenuBarSeparatorItem*>(itemAt(event->pos()));

  if (commandItem || separatorItem) {
    std::string dragStr;
    QString dragPixmapTxt;
    if (commandItem) {
      dragStr =
          CommandManager::instance()->getIdFromAction(commandItem->getAction());
      dragPixmapTxt = commandItem->getAction()->text();
      dragPixmapTxt.remove("&");
    } else {
      dragStr       = "separator";
      dragPixmapTxt = tr("----Separator----");
    }

    QMimeData* mimeData = new QMimeData;
    mimeData->setText(QString::fromStdString(dragStr));

    QFontMetrics fm(QApplication::font());
    QPixmap pix(fm.boundingRect(dragPixmapTxt).adjusted(-2, -2, 2, 2).size());
    QPainter painter(&pix);
    painter.fillRect(pix.rect(), Qt::white);
    painter.setPen(Qt::black);
    painter.drawText(pix.rect(), Qt::AlignCenter, dragPixmapTxt);

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pix);

    drag->exec(Qt::CopyAction);
  }

  QTreeWidget::mousePressEvent(event);
}

//=============================================================================
// MenuBarPopup
//-----------------------------------------------------------------------------

MenuBarPopup::MenuBarPopup(Room* room)
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "CustomizeMenuBar") {
  setWindowTitle(tr("Customize Menu Bar of Room \"%1\"").arg(room->getName()));

  /*- get menubar setting file path -*/
  std::string mbFileName = room->getPath().getName() + "_menubar.xml";
  TFilePath mbPath       = ToonzFolder::getMyRoomsDir() + mbFileName;

  m_commandListTree = new CommandListTree(this);
  m_menuBarTree     = new MenuBarTree(mbPath, this);

  QPushButton* okBtn     = new QPushButton(tr("OK"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  okBtn->setFocusPolicy(Qt::NoFocus);
  cancelBtn->setFocusPolicy(Qt::NoFocus);

  QLabel* menuBarLabel =
      new QLabel(tr("%1 Menu Bar").arg(room->getName()), this);
  QLabel* menuItemListLabel = new QLabel(tr("Menu Items"), this);

  QFont f("Arial", 15, QFont::Bold);
  menuBarLabel->setFont(f);
  menuItemListLabel->setFont(f);

  QLabel* noticeLabel = new QLabel(
      tr("N.B. If you put unique title to submenu, it may not be translated to "
         "another language.\nN.B. Duplicated commands will be ignored. Only "
         "the last one will appear in the menu bar."),
      this);
  QFont nf("Arial", 9, QFont::Normal);
  nf.setItalic(true);
  noticeLabel->setFont(nf);

  //--- layout
  QVBoxLayout* mainLay = new QVBoxLayout();
  m_topLayout->setMargin(0);
  m_topLayout->setSpacing(0);
  {
    QGridLayout* mainUILay = new QGridLayout();
    mainUILay->setMargin(5);
    mainUILay->setHorizontalSpacing(8);
    mainUILay->setVerticalSpacing(5);
    {
      mainUILay->addWidget(menuBarLabel, 0, 0);
      mainUILay->addWidget(menuItemListLabel, 0, 1);
      mainUILay->addWidget(m_menuBarTree, 1, 0);
      mainUILay->addWidget(m_commandListTree, 1, 1);

      mainUILay->addWidget(noticeLabel, 2, 0, 1, 2);
    }
    mainUILay->setRowStretch(0, 0);
    mainUILay->setRowStretch(1, 1);
    mainUILay->setRowStretch(2, 0);
    mainUILay->setColumnStretch(0, 1);
    mainUILay->setColumnStretch(1, 1);

    m_topLayout->addLayout(mainUILay, 1);
  }

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(30);
  {
    m_buttonLayout->addStretch(1);
    m_buttonLayout->addWidget(okBtn, 0);
    m_buttonLayout->addWidget(cancelBtn, 0);
    m_buttonLayout->addStretch(1);
  }

  //--- signal/slot connections

  bool ret = connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkPressed()));
  ret      = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void MenuBarPopup::onOkPressed() {
  m_menuBarTree->saveMenuTree();

  accept();
}