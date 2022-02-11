

#include "studiopaletteviewer.h"
#include "palettesscanpopup.h"
#include "toonz/studiopalettecmd.h"
#include "toonzqt/menubarcommand.h"
#include "floatingpanelcommand.h"
#include "toonzqt/gutil.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonzqt/paletteviewer.h"
#include "toonzutil.h"
#include "tconvert.h"

#include "toonz/txshsimplelevel.h"

#include <QHeaderView>
#include <QContextMenuEvent>
#include <QMenu>
#include <QUrl>
#include <QPainter>
#include <QVBoxLayout>
#include <QToolBar>
#include <QSplitter>

#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"

using namespace std;
using namespace PaletteViewerGUI;

//=============================================================================
namespace {
//-----------------------------------------------------------------------------
/*! Return true if path is in folder \b rootPath of \b StudioPalette.
*/
bool isInStudioPaletteFolder(TFilePath path, TFilePath rootPath) {
  if (path.getType() != "tpl") return false;
  StudioPalette *studioPlt = StudioPalette::instance();
  std::vector<TFilePath> childrenPath;
  studioPlt->getChildren(childrenPath, rootPath);
  int i;
  for (i = 0; i < (int)childrenPath.size(); i++) {
    if (path == childrenPath[i])
      return true;
    else if (isInStudioPaletteFolder(path, childrenPath[i]))
      return true;
  }
  return false;
}

//-----------------------------------------------------------------------------
/*! Return true if path is in a \b StudioPalette folder.
*/
bool isInStudioPalette(TFilePath path) {
  if (path.getType() != "tpl") return false;
  StudioPalette *studioPlt = StudioPalette::instance();
  if (isInStudioPaletteFolder(path, studioPlt->getLevelPalettesRoot()))
    return true;
  if (isInStudioPaletteFolder(path, studioPlt->getCleanupPalettesRoot()))
    return true;
  if (isInStudioPaletteFolder(
          path,
          TFilePath("C:\\Toonz 6.0 stuff\\projects\\Project "
                    "Palettes")))  // DAFARE
                                   // studioPlt->getProjectPalettesRoot();
                                   // Per ora lo fisso))
    return true;
  return false;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
/*! \class StudioPaletteTreeViewer
                \brief The StudioPaletteTreeViewer class provides an object to
   view and manage
                                         palettes files.

                Inherits \b QTreeWidget and \b StudioPalette::Listener.

                This object provides interface for class \b StudioPalette.
                StudioPaletteTreeViewer is a \b QTreeWidget with three root item
   related to
                level palette folder, cleanup palette folder and current project
   palette folder,
                the three root folder of \b StudioPalette.
*/
/*!	\fn void StudioPaletteTreeViewer::onStudioPaletteTreeChange()
                Overridden from StudioPalette::Listener.
                \fn void StudioPaletteTreeViewer::onStudioPaletteMove(const
   TFilePath &dstPath, const TFilePath &srcPath)
                Overridden from StudioPalette::Listener.
                \fn void StudioPaletteTreeViewer::onStudioPaletteChange(const
   TFilePath &palette)
                Overridden from StudioPalette::Listener.
*/
StudioPaletteTreeViewer::StudioPaletteTreeViewer(
    QWidget *parent, TPaletteHandle *studioPaletteHandle,
    TPaletteHandle *levelPaletteHandle)
    : QTreeWidget(parent)
    , m_dropItem(0)
    , m_stdPltHandle(studioPaletteHandle)
    , m_levelPltHandle(levelPaletteHandle)
    , m_sceneHandle(0)
    , m_levelHandle(0) {
  header()->close();
  setIconSize(QSize(20, 20));

  // Da sistemare le icone dei tre folder principali
  static QPixmap PaletteIconPxmp(":Resources/icon.png");

  QList<QTreeWidgetItem *> paletteItems;

  StudioPalette *studioPlt = StudioPalette::instance();

  // static QPixmap PaletteLevelIconPxmp(":Resources/studio_plt_toonz.png");
  TFilePath levelPltPath = studioPlt->getLevelPalettesRoot();
  paletteItems.append(createRootItem(levelPltPath, PaletteIconPxmp));

  // static QPixmap PaletteCleanupIconPxmp(":Resources/studio_plt_cleanup.png");
  TFilePath cleanupPltPath = studioPlt->getCleanupPalettesRoot();
  paletteItems.append(createRootItem(cleanupPltPath, PaletteIconPxmp));

  // DAFARE
  // Oss.: se il folder non c'e' non fa nulla, non si aprono neanche i menu' da
  // tasto destro!
  // static QPixmap PaletteProjectIconPxmp(":Resources/studio_plt_project.png");
  TFilePath projectPltPath = TFilePath(
      "C:\\Toonz 6.0 stuff\\projects\\Project Palettes");  // studioPlt->getProjectPalettesRoot();
                                                           // Per ora lo fisso
  paletteItems.append(createRootItem(projectPltPath, PaletteIconPxmp));

  insertTopLevelItems(0, paletteItems);

  connect(this, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this,
          SLOT(onItemChanged(QTreeWidgetItem *, int)));

  connect(this, SIGNAL(itemExpanded(QTreeWidgetItem *)), this,
          SLOT(onItemExpanded(QTreeWidgetItem *)));
  connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem *)), this,
          SLOT(onItemCollapsed(QTreeWidgetItem *)));

  connect(this, SIGNAL(itemSelectionChanged()), this,
          SLOT(onItemSelectionChanged()));

  m_palettesScanPopup = new PalettesScanPopup();
  setAcceptDrops(true);
}

//-----------------------------------------------------------------------------

StudioPaletteTreeViewer::~StudioPaletteTreeViewer() {}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::setLevelPaletteHandle(
    TPaletteHandle *paletteHandle) {
  m_levelPltHandle = paletteHandle;
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::setStdPaletteHandle(
    TPaletteHandle *stdPltHandle) {
  m_stdPltHandle = stdPltHandle;
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::setSceneHandle(TSceneHandle *sceneHandle) {
  m_sceneHandle = sceneHandle;
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::setLevelHandle(TXshLevelHandle *levelHandle) {
  m_levelHandle = levelHandle;
}

//-----------------------------------------------------------------------------
/*! Create root item related to path \b path.
*/
QTreeWidgetItem *StudioPaletteTreeViewer::createRootItem(TFilePath path,
                                                         const QPixmap pix) {
  QString rootName = QString(path.getName().c_str());
  QTreeWidgetItem *rootItem =
      new QTreeWidgetItem((QTreeWidget *)0, QStringList(rootName));
  rootItem->setIcon(0, pix);
  rootItem->setData(1, Qt::UserRole, toQString(path));

  refreshItem(rootItem);

  return rootItem;
}

//-----------------------------------------------------------------------------
/*! Return true if \b item is a root item; false otherwise.
*/
bool StudioPaletteTreeViewer::isRootItem(QTreeWidgetItem *item) {
  assert(item);
  TFilePath path = getFolderPath(item);

  // DAFARE
  // Oss.: se il folder non c'e' non fa nulla, non si aprono neanche i menu' da
  // tasto destro!
  // static QPixmap PaletteProjectIconPxmp(":Resources/studio_plt_project.png");
  TFilePath projectPltPath = TFilePath(
      "C:\\Toonz 6.0 stuff\\projects\\Project Palettes");  // studioPlt->getProjectPalettesRoot();
                                                           // Per ora lo fisso

  StudioPalette *stdPalette = StudioPalette::instance();
  if (path == stdPalette->getCleanupPalettesRoot() ||
      path == stdPalette->getLevelPalettesRoot() || path == projectPltPath)
    return true;

  return false;
}

//-----------------------------------------------------------------------------
/*! Create a new item related to path \b path.
*/
QTreeWidgetItem *StudioPaletteTreeViewer::createItem(const TFilePath path) {
  static QPixmap PaletteIconPxmp(":Resources/icon.png");
  static QPixmap FolderIconPxmp(":Resources/newfolder_over.png");

  StudioPalette *studioPlt = StudioPalette::instance();

  QString itemName = QString(path.getName().c_str());
  QTreeWidgetItem *item =
      new QTreeWidgetItem((QTreeWidget *)0, QStringList(itemName));
  if (studioPlt->isPalette(path))
    item->setIcon(0, PaletteIconPxmp);
  else if (studioPlt->isFolder(path))
    item->setIcon(0, FolderIconPxmp);
  item->setData(1, Qt::UserRole, toQString(path));
  item->setFlags(item->flags() | Qt::ItemIsEditable);

  return item;
}

//-----------------------------------------------------------------------------
/*! Return path related to item \b item if \b item exist, otherwise return an
                empty path \b TFilePath.
*/
TFilePath StudioPaletteTreeViewer::getFolderPath(QTreeWidgetItem *item) {
  TFilePath path =
      (item) ? TFilePath(item->data(1, Qt::UserRole).toString().toStdWString())
             : TFilePath();
  return path;
}

//-----------------------------------------------------------------------------
/*! Return current item path.
*/
TFilePath StudioPaletteTreeViewer::getCurrentFolderPath() {
  return getFolderPath(currentItem());
}

//-----------------------------------------------------------------------------
/*! Return item identified by \b path; if it doesn't exist return 0.
*/
QTreeWidgetItem *StudioPaletteTreeViewer::getItem(const TFilePath path) {
  QList<QTreeWidgetItem *> oldItems =
      findItems(QString(""), Qt::MatchContains, 0);
  if (oldItems.isEmpty()) return 0;
  int i;
  for (i = 0; i < (int)oldItems.size(); i++) {
    TFilePath oldItemPath(
        oldItems[i]->data(1, Qt::UserRole).toString().toStdWString());
    if (oldItemPath == path)
      return oldItems[i];
    else {
      QTreeWidgetItem *item = getFolderItem(oldItems[i], path);
      if (item) return item;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------
/*! Return item child of \b parent identified by \b path; if it doesn't exist
 * return 0.
*/
QTreeWidgetItem *StudioPaletteTreeViewer::getFolderItem(QTreeWidgetItem *parent,
                                                        const TFilePath path) {
  int childrenCount = parent->childCount();
  int i;
  for (i = 0; i < childrenCount; i++) {
    QTreeWidgetItem *item = parent->child(i);
    if (getFolderPath(item) == path)
      return item;
    else {
      item = getFolderItem(item, path);
      if (item) return item;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------
/*! Refresh all item of three root item in tree and preserve current item.
*/
void StudioPaletteTreeViewer::refresh() {
  StudioPalette *studioPlt = StudioPalette::instance();

  TFilePath levelPltPath = studioPlt->getLevelPalettesRoot();
  refreshItem(getItem(levelPltPath));

  TFilePath cleanupPltPath = studioPlt->getCleanupPalettesRoot();
  refreshItem(getItem(cleanupPltPath));

  // DAFARE
  TFilePath projectPltPath = TFilePath(
      "C:\\Toonz 6.0 stuff\\projects\\Project Palettes");  // studioPlt->getProjectPalettesRoot();
                                                           // Per ora lo fisso
  refreshItem(getItem(projectPltPath));
}

//-----------------------------------------------------------------------------
/*! Refresh item \b item and its children; take path concerning \b item and
                compare \b StudioPalette folder in path with folder in item.
                If are not equal add or remove child to current \b item. Recall
   itself
                for each item child.
*/
void StudioPaletteTreeViewer::refreshItem(QTreeWidgetItem *item) {
  TFilePath folderPath = getFolderPath(item);
  assert(folderPath != TFilePath());
  std::vector<TFilePath> childrenPath;
  StudioPalette::instance()->getChildren(childrenPath, folderPath);
  int currentChildCount = item->childCount();
  std::vector<QTreeWidgetItem *> currentChildren;
  int i;
  for (i = 0; i < currentChildCount; i++)
    currentChildren.push_back(item->child(i));

  int childrenPathCount = childrenPath.size();
  int itemIndex         = 0;
  int pathIndex         = 0;
  while (itemIndex < currentChildCount || pathIndex < childrenPathCount) {
    TFilePath path =
        (pathIndex < childrenPathCount) ? childrenPath[pathIndex] : TFilePath();
    QTreeWidgetItem *currentItem =
        (itemIndex < currentChildCount) ? currentChildren[itemIndex] : 0;
    TFilePath currentItemPath = getFolderPath(currentItem);

    if (path == currentItemPath) {
      itemIndex++;
      pathIndex++;
      refreshItem(currentItem);
    } else if ((!path.isEmpty() && path < currentItemPath) ||
               currentItemPath.isEmpty()) {
      currentItem = createItem(path);
      item->insertChild(pathIndex, currentItem);
      refreshItem(currentItem);
      pathIndex++;
    } else {
      assert(currentItemPath < path || path.isEmpty());
      assert(currentItem);
      item->removeChild(currentItem);
      itemIndex++;
    }
  }
}

//-----------------------------------------------------------------------------
/*! If item \b item name changed update name item related path name in \b
 * StudioPalette.
*/
void StudioPaletteTreeViewer::onItemChanged(QTreeWidgetItem *item, int column) {
  if (item != currentItem()) return;
  string name       = item->text(column).toStdString();
  TFilePath oldPath = getCurrentFolderPath();
  if (oldPath.isEmpty() || name.empty() || oldPath.getName() == name) return;
  TFilePath newPath(oldPath.getParentDir() +
                    TFilePath(name + oldPath.getDottedType()));
  try {
    StudioPaletteCmd::movePalette(newPath, oldPath);
  } catch (TException &e) {
    error("Can't rename palette: " + QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't rename palette");
  }
  setCurrentItem(getItem(newPath));
}

//-----------------------------------------------------------------------------
/*! If item \b item is expanded change item icon.
*/
void StudioPaletteTreeViewer::onItemExpanded(QTreeWidgetItem *item) {
  if (!item || isRootItem(item)) return;
  static QPixmap FolderIconExpandedPxmp(":Resources/folder_open.png");
  item->setIcon(0, FolderIconExpandedPxmp);
}

//-----------------------------------------------------------------------------
/*! If item \b item is collapsed change item icon.
*/
void StudioPaletteTreeViewer::onItemCollapsed(QTreeWidgetItem *item) {
  if (!item || isRootItem(item)) return;
  static QPixmap FolderIconPxmp(":Resources/newfolder_over.png");
  item->setIcon(0, FolderIconPxmp);
}

//-----------------------------------------------------------------------------
/*! If current item is a palette set current studioPalette to it.
*/
void StudioPaletteTreeViewer::onItemSelectionChanged() {
  TFilePath path = getCurrentFolderPath();
  if (!m_stdPltHandle || path.getType() != "tpl") return;

  if (m_stdPltHandle->getPalette() &&
      m_stdPltHandle->getPalette()->getDirtyFlag()) {
    QString question;
    question =
        "Current Studio Palette has been modified.\n"
        "Do you want to save your changes?";
    int ret = MsgBox(0, question, QObject::tr("Save"),
                     QObject::tr("Don't Save"), QObject::tr("Cancel"), 0);

    TPaletteP oldPalette = m_stdPltHandle->getPalette();
    TFilePath oldPath =
        StudioPalette::instance()->getPalettePath(oldPalette->getGlobalName());

    if (ret == 3) {
      setCurrentItem(getItem(oldPath));
      return;
    }
    if (ret == 1)
      StudioPalette::instance()->save(oldPath, oldPalette.getPointer());
    oldPalette->setDirtyFlag(false);
  }
  m_stdPltHandle->setPalette(StudioPalette::instance()->getPalette(path, true));

  m_stdPltHandle->notifyPaletteSwitched();
}

//-----------------------------------------------------------------------------
/*! Create a new \b StudioPalette palette in current item path.
*/
void StudioPaletteTreeViewer::addNewPlt() {
  if (!currentItem()) {
    error("Error: No folder selected.");
    return;
  }
  TFilePath newPath;
  try {
    newPath = StudioPaletteCmd::createPalette(getCurrentFolderPath(), "", 0);
  } catch (TException &e) {
    error("Can't create palette: " + QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't create palette");
  }
  setCurrentItem(getItem(newPath));
}

//-----------------------------------------------------------------------------
/*! Create a new \b StudioPalette folder in current item path.
*/
void StudioPaletteTreeViewer::addNewFolder() {
  if (!currentItem()) {
    error("Error: No folder selected.");
    return;
  }
  TFilePath newPath;
  try {
    newPath = StudioPaletteCmd::addFolder(getCurrentFolderPath());
  } catch (TException &e) {
    error("Can't create palette folder: " +
          QString(toString(e.getMessage()).c_str()));
  } catch (...) {
    error("Can't create palette folder");
  }

  setCurrentItem(getItem(newPath));
}

//-----------------------------------------------------------------------------
/*! Delete current item path from \b StudioPalette. If item is a not empty
                folder send a question to know if must delete item or not.
*/
void StudioPaletteTreeViewer::deleteItem() {
  if (!currentItem()) {
    error("Nothing to delete");
    return;
  }
  QTreeWidgetItem *parent = currentItem()->parent();
  if (!parent) return;

  if (currentItem()->childCount() > 0) {
    QString question;
    question = tr("This folder is not empty. Delete anyway?");
    int ret  = MsgBox(0, question, QObject::tr("Yes"), QObject::tr("No"));
    if (ret == 0 || ret == 2) return;
  }

  TFilePath path           = getCurrentFolderPath();
  StudioPalette *studioPlt = StudioPalette::instance();
  if (studioPlt->isFolder(path)) {
    try {
      StudioPaletteCmd::deleteFolder(path);
    } catch (TException &e) {
      error("Can't delete palette: " +
            QString(toString(e.getMessage()).c_str()));
    } catch (...) {
      error("Can't delete palette");
    }
  } else {
    assert(studioPlt->isPalette(path));
    try {
      StudioPaletteCmd::deletePalette(path);
    } catch (TException &e) {
      error("Can't delete palette: " +
            QString(toString(e.getMessage()).c_str()));
    } catch (...) {
      error("Can't delete palette");
    }
  }
}

//-----------------------------------------------------------------------------
/*! Open a \b PalettesScanPopup.
*/
void StudioPaletteTreeViewer::searchForPlt() {
  m_palettesScanPopup->setCurrentFolder(getCurrentFolderPath());
  int ret = m_palettesScanPopup->exec();
  if (ret == QDialog::Accepted) refresh();
}

//-----------------------------------------------------------------------------
/*! Recall \b StudioPaletteCmd::loadIntoCleanupPalette.
*/
void StudioPaletteTreeViewer::loadInCurrentCleanupPlt() {
  StudioPaletteCmd::loadIntoCleanupPalette(m_levelPltHandle, m_sceneHandle,
                                           getCurrentFolderPath());
}

//-----------------------------------------------------------------------------
/*! Recall \b StudioPaletteCmd::replaceWithCleanupPalette.
*/
void StudioPaletteTreeViewer::replaceCurrentCleanupPlt() {
  StudioPaletteCmd::replaceWithCleanupPalette(
      m_levelPltHandle, m_stdPltHandle, m_sceneHandle, getCurrentFolderPath());
}

//-----------------------------------------------------------------------------
/*! Recall \b StudioPaletteCmd::loadIntoCurrentPalette.
*/
void StudioPaletteTreeViewer::loadInCurrentPlt() {
  StudioPaletteCmd::loadIntoCurrentPalette(m_levelPltHandle, m_sceneHandle,
                                           getCurrentFolderPath());
}

//-----------------------------------------------------------------------------
/*! Recall \b StudioPaletteCmd::replaceWithCurrentPalette.
*/
void StudioPaletteTreeViewer::replaceCurrentPlt() {
  StudioPaletteCmd::replaceWithCurrentPalette(
      m_levelPltHandle, m_stdPltHandle, m_sceneHandle, getCurrentFolderPath());
}

//-----------------------------------------------------------------------------
/*! Recall \b StudioPaletteCmd::mergeIntoCurrentPalette.
*/
void StudioPaletteTreeViewer::mergeToCurrentPlt() {
  StudioPaletteCmd::mergeIntoCurrentPalette(m_levelPltHandle, m_sceneHandle,
                                            getCurrentFolderPath());
}

//-----------------------------------------------------------------------------

void StudioPaletteTreeViewer::paintEvent(QPaintEvent *event) {
  QTreeWidget::paintEvent(event);
  QPainter p(viewport());
  if (m_dropItem) {
    QRect rect = visualItemRect(m_dropItem).adjusted(0, 0, -5, 0);
    p.setPen(Qt::red);
    p.drawRect(rect);
  }
}

//-----------------------------------------------------------------------------
/*! Open a context menu considering current item data role \b Qt::UserRole.
*/
void StudioPaletteTreeViewer::contextMenuEvent(QContextMenuEvent *event) {
  TFilePath path = getCurrentFolderPath();

  StudioPalette *studioPlt = StudioPalette::instance();

  // Verify if click position is in a row containing an item.
  QRect rect = visualItemRect(currentItem());
  if (!QRect(0, rect.y(), width(), rect.height()).contains(event->pos()))
    return;

  bool isLevelFolder =
      (studioPlt->isFolder(path) && !studioPlt->isCleanupFolder(path));

  QMenu menu(this);
  if (isLevelFolder) {
    createMenuAction(menu, tr("New Palette"), "addNewPlt()");
    createMenuAction(menu, tr("New Folder"), "addNewFolder()");
  } else if (studioPlt->isCleanupFolder(path)) {
    createMenuAction(menu, tr("New Cleanup Palette"), "addNewPlt()");
    createMenuAction(menu, tr("New Folder"), "addNewFolder()");
  }

  if (studioPlt->isFolder(path) && studioPlt->getLevelPalettesRoot() != path &&
      studioPlt->getCleanupPalettesRoot() != path &&
      TFilePath("C:\\Toonz 6.0 stuff\\projects\\Project Palettes") !=
          path)  // DAFARE studioPlt->getProjectPalettesRoot(); Per ora lo
                 // fisso)
  {
    menu.addSeparator();
    createMenuAction(menu, tr("Delete Folder"), "deleteItem()");
  } else if (studioPlt->isPalette(path)) {
    if (studioPlt->isCleanupPalette(path)) {
      createMenuAction(menu, tr("Load into Current Cleaunp Palette"),
                       "loadInCurrentCleanupPlt()");
      createMenuAction(menu, tr("Replace with Current Cleaunp Palette"),
                       "replaceCurrentCleanupPlt()");
      menu.addSeparator();
    } else if (m_stdPltHandle->getPalette()) {
      createMenuAction(menu, tr("Load into Current Palette"),
                       "loadInCurrentPlt()");
      createMenuAction(menu, tr("Merge to Current Palette"),
                       "mergeToCurrentPlt()");
      createMenuAction(menu, tr("Replace with Current Palette"),
                       "replaceCurrentPlt()");
      menu.addSeparator();
    }
    createMenuAction(menu, tr("Delete Palette"), "deleteItem()");
    menu.addSeparator();
    menu.addAction(
        CommandManager::instance()->getAction("MI_LoadColorModelInStdPlt"));
  }
  if (isLevelFolder) {
    menu.addSeparator();
    createMenuAction(menu, tr("Search for Palettes"), "searchForPlt()");
  }
  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------
/*! Add an action to menu \b menu; the action has text \b name and its
                \b triggered() signal is connetted with \b slot.
*/
void StudioPaletteTreeViewer::createMenuAction(QMenu &menu, QString name,
                                               const char *slot) {
  QAction *act = menu.addAction(name);
  string slotName(slot);
  slotName = string("1") + slotName;
  connect(act, SIGNAL(triggered()), slotName.c_str());
}

//-----------------------------------------------------------------------------
/*! If button left is pressed start drag and drop.
*/
void StudioPaletteTreeViewer::mouseMoveEvent(QMouseEvent *event) {
  // If left button is not pressed return; is not drag event.
  if (!(event->buttons() & Qt::LeftButton)) return;
  startDragDrop();
}

//-----------------------------------------------------------------------------
/*! If path related to current item exist and is a palette execute drag.
*/
void StudioPaletteTreeViewer::startDragDrop() {
  TFilePath path = getCurrentFolderPath();
  if (!path.isEmpty() && (path.getType() == "tpl" || path.getType() == "pli" ||
                          path.getType() == "tlv" || path.getType() == "tnz")) {
    QDrag *drag         = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QList<QUrl> urls;
    urls.append(pathToUrl(path));
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);

    Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
  }
  viewport()->update();
}

//-----------------------------------------------------------------------------
/*! Verify drag enter data, if it has an url and it's path is a palette or data
                is a PaletteData accept drag event.
*/
void StudioPaletteTreeViewer::dragEnterEvent(QDragEnterEvent *event) {
  const QMimeData *mimeData      = event->mimeData();
  const PaletteData *paletteData = dynamic_cast<const PaletteData *>(mimeData);

  if (mimeData->hasUrls()) {
    if (mimeData->urls().size() != 1) return;
    QUrl url = mimeData->urls()[0];
    TFilePath path(url.toLocalFile().toStdWString());
    if (path.isEmpty() || (path.getType() != "tpl" && path.getType() != "pli" &&
                           path.getType() != "tlv" && path.getType() != "tnz"))
      return;
    event->acceptProposedAction();
  } else if (paletteData && !paletteData->hasStyleIndeces())
    event->acceptProposedAction();
  viewport()->update();
}

//-----------------------------------------------------------------------------
/*! Find item folder nearest to current position.
*/
void StudioPaletteTreeViewer::dragMoveEvent(QDragMoveEvent *event) {
  QTreeWidgetItem *item = itemAt(event->pos());
  TFilePath newPath     = getFolderPath(item);

  if (newPath.getType() == "tpl") {
    m_dropItem = 0;
    event->ignore();
  } else {
    m_dropItem = item;
    event->acceptProposedAction();
  }
  viewport()->update();
}

//-----------------------------------------------------------------------------
/*! Execute drop event. If dropped palette is in studio palette folder move
                palette, otherwise copy palette in current folder.
*/
void StudioPaletteTreeViewer::dropEvent(QDropEvent *event) {
  TFilePath path;
  const QMimeData *mimeData = event->mimeData();
  if (!mimeData->hasUrls()) return;

  if (mimeData->urls().size() != 1) return;

  QUrl url = mimeData->urls()[0];
  path     = TFilePath(url.toLocalFile().toStdWString());

  event->setDropAction(Qt::CopyAction);
  event->accept();

  TFilePath newPath = getFolderPath(m_dropItem);

  StudioPalette *studioPlt = StudioPalette::instance();
  if (path == newPath || path.getParentDir() == newPath) return;

  if (isInStudioPalette(path)) {
    newPath += TFilePath(path.getName() + path.getDottedType());
    try {
      StudioPaletteCmd::movePalette(newPath, path);
    } catch (TException &e) {
      error("Can't rename palette: " +
            QString(toString(e.getMessage()).c_str()));
    } catch (...) {
      error("Can't rename palette");
    }
  } else {
    TPalette *palette = studioPlt->getPalette(path);
    // Se non trova palette sto importando: - la palette del livello corrente o
    // - la cleanupPalette!
    if (!palette) {
      if (path.getType() == "pli" || path.getType() == "tlv") {
        TXshLevel *level = m_levelHandle->getLevel();
        assert(level && level->getSimpleLevel());
        palette = level->getSimpleLevel()->getPalette();
        path    = level->getPath();
      } else if (path.getType() == "tnz") {
        palette =
            m_sceneHandle->getScene()->getProperties()->getCleanupPalette();
        path = TFilePath();
      }
    }
    try {
      StudioPaletteCmd::createPalette(newPath, path.getName(), palette);
    } catch (TException &e) {
      error("Can't create palette: " +
            QString(toString(e.getMessage()).c_str()));
    } catch (...) {
      error("Can't create palette");
    }
  }
  m_dropItem = 0;
}

//-----------------------------------------------------------------------------
/*! Set dropItem to 0 and update the tree.
*/
void StudioPaletteTreeViewer::dragLeaveEvent(QDragLeaveEvent *event) {
  m_dropItem = 0;
  update();
}

//-----------------------------------------------------------------------------
/*! Receive widget hide events. Remove this listener from \b StudioPalette.
*/
void StudioPaletteTreeViewer::hideEvent(QHideEvent *event) {
  StudioPalette::instance()->removeListener(this);
}

//-----------------------------------------------------------------------------
/*! Receive widget show events. Add this listener to \b StudioPalette and
                refresh tree.
*/
void StudioPaletteTreeViewer::showEvent(QShowEvent *event) {
  StudioPalette::instance()->addListener(this);
  refresh();
}

//=============================================================================
/*! \class StudioPaletteViewer
                \brief The StudioPaletteViewer class provides an object to view
   and manage
                                         studio palettes.

                Inherits \b QFrame.
                This object is composed of a splitter \b QSplitter that contain
   a vertical
                layout and a \b PaletteViewer. Vertical layout contain a \b
   StudioPaletteTreeViewer
                and a toolbar, this object allows to manage the palettes in
   studio palette folders.
                \b PaletteViewer is set to fixed view type: \b
   PaletteViewerGUI::STUDIO_PALETTE
                allows to show and modify current studio palette selected in
   tree.
*/
StudioPaletteViewer::StudioPaletteViewer(QWidget *parent,
                                         TPaletteHandle *studioPaletteHandle,
                                         TPaletteHandle *levelPaletteHandle,
                                         TSceneHandle *sceneHandle,
                                         TXshLevelHandle *levelHandle,
                                         TFrameHandle *frameHandle)
    : QFrame(parent) {
  setObjectName("studiopalette");
  setFrameStyle(QFrame::StyledPanel);
  setAcceptDrops(true);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setMargin(0);
  QSplitter *splitter = new QSplitter(this);
  splitter->setOrientation(Qt::Vertical);

  // First Splitter Widget
  QWidget *treeWidget      = new QWidget(this);
  QVBoxLayout *treeVLayout = new QVBoxLayout(treeWidget);
  treeVLayout->setMargin(0);
  treeVLayout->setSpacing(0);

  StudioPaletteTreeViewer *studioPltTreeViewer = new StudioPaletteTreeViewer(
      treeWidget, studioPaletteHandle, levelPaletteHandle);
  studioPltTreeViewer->setSceneHandle(sceneHandle);
  studioPltTreeViewer->setLevelHandle(levelHandle);
  treeVLayout->addWidget(studioPltTreeViewer);

  // Create toolbar. It is an horizontal layout with three internal toolbar.
  QWidget *treeToolbarWidget = new QWidget(this);
  treeToolbarWidget->setFixedHeight(22);
  QHBoxLayout *treeToolbarLayout = new QHBoxLayout(treeToolbarWidget);
  treeToolbarLayout->setMargin(0);
  treeToolbarLayout->setSpacing(0);

  QToolBar *newToolbar = new QToolBar(treeToolbarWidget);
  // New folder action
  QIcon newFolderIcon = createQIconPNG("folder_new");
  QAction *addFolder =
      new QAction(newFolderIcon, tr("&New Folder"), newToolbar);
  connect(addFolder, SIGNAL(triggered()), studioPltTreeViewer,
          SLOT(addNewFolder()));
  newToolbar->addAction(addFolder);
  // New palette action
  QIcon newPaletteIcon = createQIconPNG("newpalette");
  QAction *addPalette =
      new QAction(newPaletteIcon, tr("&New Palette"), newToolbar);
  connect(addPalette, SIGNAL(triggered()), studioPltTreeViewer,
          SLOT(addNewPlt()));
  newToolbar->addAction(addPalette);
  newToolbar->addSeparator();

  QToolBar *spacingToolBar = new QToolBar(treeToolbarWidget);
  spacingToolBar->setMinimumHeight(22);

  QToolBar *deleteToolbar = new QToolBar(treeToolbarWidget);
  // Delete folder and palette action
  QIcon deleteFolderIcon = createQIconPNG("delete");
  QAction *deleteFolder =
      new QAction(deleteFolderIcon, tr("&Delete"), deleteToolbar);
  connect(deleteFolder, SIGNAL(triggered()), studioPltTreeViewer,
          SLOT(deleteItem()));
  deleteToolbar->addSeparator();
  deleteToolbar->addAction(deleteFolder);

  treeToolbarLayout->addWidget(newToolbar, 0, Qt::AlignLeft);
  treeToolbarLayout->addWidget(spacingToolBar, 1);
  treeToolbarLayout->addWidget(deleteToolbar, 0, Qt::AlignRight);
  treeToolbarWidget->setLayout(treeToolbarLayout);

  treeVLayout->addWidget(treeToolbarWidget);
  treeWidget->setLayout(treeVLayout);

  // Second Splitter Widget
  PaletteViewer *studioPltViewer =
      new PaletteViewer(this, PaletteViewerGUI::STUDIO_PALETTE);
  studioPltViewer->setPaletteHandle(studioPaletteHandle);
  studioPltViewer->setFrameHandle(frameHandle);
  studioPltViewer->setLevelHandle(levelHandle);
  studioPltViewer->setSceneHandle(sceneHandle);

  splitter->addWidget(treeWidget);
  splitter->addWidget(studioPltViewer);
  layout->addWidget(splitter);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

StudioPaletteViewer::~StudioPaletteViewer() {}

//=============================================================================

OpenFloatingPanel openStudioPaletteCommand("MI_OpenStudioPalette",
                                           "StudioPalette", "Studio Palette");
