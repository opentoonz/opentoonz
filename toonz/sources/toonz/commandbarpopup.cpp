#include "commandbarpopup.h"

// Tnz includes
#include "tapp.h"
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
// CommandBarCommandItem
//-----------------------------------------------------------------------------

class CommandBarCommandItem final : public QTreeWidgetItem {
  QAction* m_action;

public:
  CommandBarCommandItem(QTreeWidgetItem* parent, QAction* action)
      : QTreeWidgetItem(parent, UserType), m_action(action) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled |
             Qt::ItemNeverHasChildren);
    setText(0, m_action->text().remove("&"));
    setToolTip(0, QObject::tr("[Drag] to move position"));
  }
  QAction* getAction() const { return m_action; }
};

//=============================================================================
// CommandBarSeparatorItem
//-----------------------------------------------------------------------------

class CommandBarSeparatorItem final : public QTreeWidgetItem {
public:
  CommandBarSeparatorItem(QTreeWidgetItem* parent)
      : QTreeWidgetItem(parent, UserType) {
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled |
             Qt::ItemNeverHasChildren);
    setText(0, QObject::tr("----Separator----"));
    setToolTip(0, QObject::tr("[Drag] to move position"));
  }
};

//=============================================================================
// CommandBarTree
//-----------------------------------------------------------------------------

CommandBarTree::CommandBarTree(TFilePath& path, QWidget* parent)
    : QTreeWidget(parent) {
  setObjectName("SolidLineFrame");
  setAlternatingRowColors(true);
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);
  setDragDropMode(QAbstractItemView::DragDrop);
  setIconSize(QSize(21, 17));

  setColumnCount(1);
  header()->close();

  /*- Load path if it does exist. If not, then load from the template. -*/
  TFilePath fp;
  if (TFileStatus(path).isWritable())
    fp = path;
  else {
    if (path.getName() == "xsheettoolbar") {
      fp = ToonzFolder::getTemplateModuleDir() + TFilePath("xsheettoolbar.xml");
    } else {
      fp = ToonzFolder::getTemplateModuleDir() + TFilePath("commandbar.xml");
    }
  }

  loadMenuTree(fp);
}

//-----------------------------------------------------------------------------

void CommandBarTree::loadMenuTree(const TFilePath& fp) {
  QFile file(toQString(fp));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "commandbar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "command") {
          QString cmdName = reader.readElementText();

          QAction* action = CommandManager::instance()->getAction(
              cmdName.toStdString().c_str());
          if (action) {
            CommandBarCommandItem* item = new CommandBarCommandItem(0, action);
            addTopLevelItem(item);
          }
        } else if (reader.name() == "separator") {
          CommandBarSeparatorItem* sep = new CommandBarSeparatorItem(0);
          addTopLevelItem(sep);
          reader.skipCurrentElement();
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

void CommandBarTree::loadMenuRecursive(QXmlStreamReader& reader,
                                       QTreeWidgetItem* parentItem) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "command") {
      QString cmdName = reader.readElementText();
      QAction* action =
          CommandManager::instance()->getAction(cmdName.toStdString().c_str());
      if (action)
        CommandBarCommandItem* item =
            new CommandBarCommandItem(parentItem, action);
    } else if (reader.name() == "command_debug") {
#ifndef NDEBUG
      QString cmdName = reader.readElementText();
      QAction* action =
          CommandManager::instance()->getAction(cmdName.toStdString().c_str());
      if (action)
        CommandBarCommandItem* item =
            new CommandBarCommandItem(parentItem, action);
#else
      reader.skipCurrentElement();
#endif
    } else if (reader.name() == "separator") {
      CommandBarSeparatorItem* sep = new CommandBarSeparatorItem(parentItem);
      reader.skipCurrentElement();
    } else
      reader.skipCurrentElement();
  }
}

//-----------------------------------------------------------------------------

void CommandBarTree::saveMenuTree(TFilePath& path) {
  QFile file(toQString(path));
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return;
  }

  QXmlStreamWriter writer(&file);
  writer.setAutoFormatting(true);
  writer.writeStartDocument();

  writer.writeStartElement("commandbar");
  { saveMenuRecursive(writer, invisibleRootItem()); }
  writer.writeEndElement();

  writer.writeEndDocument();
}

//-----------------------------------------------------------------------------

void CommandBarTree::saveMenuRecursive(QXmlStreamWriter& writer,
                                       QTreeWidgetItem* parentItem) {
  for (int c = 0; c < parentItem->childCount(); c++) {
    CommandBarCommandItem* command =
        dynamic_cast<CommandBarCommandItem*>(parentItem->child(c));
    CommandBarSeparatorItem* sep =
        dynamic_cast<CommandBarSeparatorItem*>(parentItem->child(c));

    if (command)
      writer.writeTextElement(
          "command",
          QString::fromStdString(CommandManager::instance()->getIdFromAction(
              command->getAction())));
    else if (sep)
      writer.writeEmptyElement("separator");
  }
}

//-----------------------------------------------------------------------------

bool CommandBarTree::dropMimeData(QTreeWidgetItem* parent, int index,
                                  const QMimeData* data,
                                  Qt::DropAction action) {
  if (data->hasText()) {
    QString txt = data->text();
    QTreeWidgetItem* item;
    if (txt == "separator")
      item = new CommandBarSeparatorItem(0);
    else {
      QAction* act =
          CommandManager::instance()->getAction(txt.toStdString().c_str());
      if (!act) return false;
      item = new CommandBarCommandItem(0, act);
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

QStringList CommandBarTree::mimeTypes() const {
  QStringList qstrList;
  qstrList.append("text/plain");
  return qstrList;
}

//-----------------------------------------------------------------------------

void CommandBarTree::contextMenuEvent(QContextMenuEvent* event) {
  QTreeWidgetItem* item = itemAt(event->pos());
  if (item != currentItem()) setCurrentItem(item);
  QMenu* menu = new QMenu(this);
  QAction* action;

  if (item) {
    action = menu->addAction(tr("Remove \"%1\"").arg(item->text(0)));
    connect(action, SIGNAL(triggered()), this, SLOT(removeItem()));
  }

  menu->exec(event->globalPos());
  delete menu;
}

//-----------------------------------------------------------------------------

void CommandBarTree::removeItem() {
  QTreeWidgetItem* item = currentItem();
  if (!item) return;

  if (indexOfTopLevelItem(item) >= 0)
    takeTopLevelItem(indexOfTopLevelItem(item));
  else
    item->parent()->removeChild(item);

  delete item;
}

//=============================================================================
// CommandListTree
//-----------------------------------------------------------------------------

CommandBarListTree::CommandBarListTree(QWidget* parent) : QTreeWidget(parent) {
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
  addFolder(ShortcutTree::tr("Fill"), FillCommandType);
  addFolder(ShortcutTree::tr("Right-click Menu Commands"),
            RightClickMenuCommandType);
  addFolder(ShortcutTree::tr("Tool Modifiers"), ToolModifierCommandType);
  addFolder(ShortcutTree::tr("Visualization"), ZoomCommandType);
  addFolder(ShortcutTree::tr("Misc"), MiscCommandType);
  addFolder(ShortcutTree::tr("RGBA Channels"), RGBACommandType);

  CommandBarSeparatorItem* sep = new CommandBarSeparatorItem(0);
  sep->setToolTip(0, QObject::tr("[Drag&Drop] to copy separator to menu bar"));
  addTopLevelItem(sep);
}

//-----------------------------------------------------------------------------

void CommandBarListTree::addFolder(const QString& title, int commandType,
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
    CommandBarCommandItem* item = new CommandBarCommandItem(folder, actions[i]);
    item->setToolTip(0, QObject::tr("[Drag&Drop] to copy command to menu bar"));
  }
}

//-----------------------------------------------------------------------------

void CommandBarListTree::mousePressEvent(QMouseEvent* event) {
  setCurrentItem(itemAt(event->pos()));
  CommandBarCommandItem* commandItem =
      dynamic_cast<CommandBarCommandItem*>(itemAt(event->pos()));
  CommandBarSeparatorItem* separatorItem =
      dynamic_cast<CommandBarSeparatorItem*>(itemAt(event->pos()));

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
// CommandBarPopup
//-----------------------------------------------------------------------------

CommandBarPopup::CommandBarPopup(bool isXsheetToolbar)
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "CustomizeCommandBar") {
  QLabel* commandBarLabel;
  if (isXsheetToolbar) {
    m_path = ToonzFolder::getMyModuleDir() + TFilePath("xsheettoolbar.xml");
    commandBarLabel = new QLabel(tr("XSheet Toolbar"));
    setWindowTitle(tr("Customize XSheet Toolbar"));
  } else {
    m_path = ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
    commandBarLabel = new QLabel(tr("Command Bar"));
    setWindowTitle(tr("Customize Command Bar"));
  }

  m_commandListTree = new CommandBarListTree(this);
  m_menuBarTree     = new CommandBarTree(m_path, this);

  QPushButton* okBtn     = new QPushButton(tr("OK"), this);
  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);

  okBtn->setFocusPolicy(Qt::NoFocus);
  cancelBtn->setFocusPolicy(Qt::NoFocus);

  QLabel* commandItemListLabel = new QLabel(tr("Toolbar Items"), this);

  QFont f("Arial", 15, QFont::Bold);
  commandBarLabel->setFont(f);
  commandItemListLabel->setFont(f);

  QLabel* noticeLabel =
      new QLabel(tr("Duplicated commands will be ignored. Only "
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
      mainUILay->addWidget(commandBarLabel, 0, 0);
      mainUILay->addWidget(commandItemListLabel, 0, 1);
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

void CommandBarPopup::onOkPressed() {
  m_menuBarTree->saveMenuTree(m_path);

  accept();
}