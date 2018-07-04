

#include <QtGlobal>

#include "dvdirtreeview.h"

#include "filebrowsermodel.h"
#include "menubarcommandids.h"
#include "filebrowser.h"
#include "tconvert.h"
#include "tsystem.h"
#include "toonz/toonzscene.h"
#include "toonz/namebuilder.h"
#include "toonz/tproject.h"
#include "toonz/preferences.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"

#include <QPainter>
#include <QPixmap>
#include <QMouseEvent>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QUrl>
#include <QDir>
#include <QMimeData>
#include <QFileSystemWatcher>

using namespace DVGui;

namespace {
//---------------------------------------------------------------------------

QStringList getLevelFileNames(TFilePath path) {
  TFilePath dir = path.getParentDir();
  QDir qDir(QString::fromStdWString(dir.getWideString()));
  QString levelName =
      QRegExp::escape(QString::fromStdWString(path.getWideName()));
  QString levelType = QString::fromStdString(path.getType());
  QString exp(levelName + ".[0-9]{1,4}." + levelType);
  QRegExp regExp(exp);
  QStringList list = qDir.entryList(QDir::Files);
  return list.filter(regExp);
}
}  // namespace

//=============================================================================
// MyFileSystemWatcher
//-----------------------------------------------------------------------------

MyFileSystemWatcher::MyFileSystemWatcher() {
  m_watcher = new QFileSystemWatcher(this);

  bool ret = connect(m_watcher, SIGNAL(directoryChanged(const QString &)), this,
                     SIGNAL(directoryChanged(const QString &)));
  assert(ret);
}

void MyFileSystemWatcher::addPaths(const QStringList &paths, bool onlyNewPath) {
  if (paths.isEmpty()) return;
  for (int p = 0; p < paths.size(); p++) {
    QString path = paths.at(p);
    // if the path is not watched yet, try to start watching it
    if (!m_watchedPath.contains(path)) {
      // symlink path will not be watched
      if (m_watcher->addPath(path)) m_watchedPath.append(path);
    }
    // or just add path to the list
    else if (!onlyNewPath) {
      m_watchedPath.append(path);
    }
  }
}

void MyFileSystemWatcher::removePaths(const QStringList &paths) {
  if (m_watchedPath.isEmpty() || paths.isEmpty()) return;
  for (int p = 0; p < paths.size(); p++) {
    QString path = paths.at(p);
    // removeOne will return false for symlink paths
    bool ret = m_watchedPath.removeOne(path);
    if (ret && !m_watchedPath.contains(path)) m_watcher->removePath(path);
  }
}

void MyFileSystemWatcher::removeAllPaths() {
  if (m_watchedPath.isEmpty()) return;
  m_watcher->removePaths(m_watcher->directories());
  m_watchedPath.clear();
}

//=============================================================================
// DvDirTreeViewDelegate
//-----------------------------------------------------------------------------

DvDirTreeViewDelegate::DvDirTreeViewDelegate(DvDirTreeView *parent)
    : QItemDelegate(parent)  // QAbstractItemDelegate(parent)
    , m_treeView(parent) {}

//-----------------------------------------------------------------------------

DvDirTreeViewDelegate::~DvDirTreeViewDelegate() {}

//-----------------------------------------------------------------------------

QWidget *DvDirTreeViewDelegate::createEditor(QWidget *parent,
                                             const QStyleOptionViewItem &option,
                                             const QModelIndex &index) const {
  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  if (!node) return 0;
  DvDirModelFileFolderNode *fnode =
      dynamic_cast<DvDirModelFileFolderNode *>(node);
  if (!fnode || fnode->isProjectFolder()) return 0;
  QPixmap px = node->getPixmap(m_treeView->isExpanded(index));
  QRect rect = option.rect;
#if QT_VERSION >= 0x050000
  if (index.data().canConvert(QMetaType::QString)) {
#else
  if (qVariantCanConvert<QString>(index.data())) {
#endif
    NodeEditor *editor = new NodeEditor(parent, rect, px.width());
    editor->setText(index.data().toString());
    connect(editor, SIGNAL(editingFinished()), this,
            SLOT(commitAndCloseEditor()));
    return editor;
  } else {
    return QAbstractItemDelegate::createEditor(parent, option, index);
  }
}

//-----------------------------------------------------------------------------

bool DvDirTreeViewDelegate::editorEvent(QEvent *ev, QAbstractItemModel *model,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) {
  if (ev->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mev             = static_cast<QMouseEvent *>(ev);
    QRect bounds                 = option.rect;
    int x                        = mev->pos().x() - bounds.x();
    DvDirModelNode *node         = DvDirModel::instance()->getNode(index);
    DvDirModelProjectNode *pnode = dynamic_cast<DvDirModelProjectNode *>(node);
    DvDirVersionControlProjectNode *vcpNode =
        dynamic_cast<DvDirVersionControlProjectNode *>(node);

    // shrink / expand the tree by clicking the item
    if (node) {
      if (m_treeView->isExpanded(index))
        m_treeView->collapse(index);
      else
        m_treeView->expand(index);
    }

    if ((pnode && pnode->isCurrent() == false && 14 < x && x < 26) ||
        (vcpNode && vcpNode->isCurrent() == false && 14 < x && x < 26)) {
      if (pnode)
        pnode->makeCurrent();
      else if (vcpNode)
        vcpNode->makeCurrent();
      m_treeView->update();
      return true;
    } else {
      m_treeView->update();
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

void DvDirTreeViewDelegate::paint(QPainter *painter,
                                  const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const {
  QRect rect           = option.rect;
  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  if (!node) return;

  // current node and drag'n drop
  bool isCurrent = (m_treeView->getCurrentNode() == node);
  if (isCurrent) {
    painter->fillRect(rect.adjusted(-2, 0, 0, 0),
                      QBrush(m_treeView->getSelectedItemBackground()));

    DvDirModelNode *currentDropNode = m_treeView->getCurrentDropNode();
    if (currentDropNode) {
      QRect dropRect = m_treeView->visualRect(
          DvDirModel::instance()->getIndexByNode(currentDropNode));
      int dropDelta       = (dropRect.height() - 16) * 0.5;
      QRect selectionRect = dropRect.adjusted(-1, dropDelta, 1, -dropDelta);
      painter->setPen(QColor(50, 105, 200));
      painter->drawRect(selectionRect);
    }
  }

  // icon
  QPixmap px = node->getPixmap(m_treeView->isExpanded(index));
  if (!px.isNull()) {
    int x = rect.left();
    int y =
        rect.top() + (rect.height() - px.height() / px.devicePixelRatio()) / 2;
    painter->drawPixmap(QPoint(x, y), px);
  }

  DvDirModelProjectNode *pnode = dynamic_cast<DvDirModelProjectNode *>(node);
  DvDirVersionControlProjectNode *vcpNode =
      dynamic_cast<DvDirVersionControlProjectNode *>(node);
  DvDirModelFileFolderNode *fnode =
      dynamic_cast<DvDirModelFileFolderNode *>(node);
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(node);

  rect.adjust((pnode || vcpNode) ? 31 : 22, 0, 0, 0);

  // draw text
  QVariant d   = index.data();
  QString name = d.toString();

  // text color

  if (fnode && fnode->isProjectFolder()) {
    painter->setPen((isCurrent) ? m_treeView->getSelectedFolderTextColor()
                                : m_treeView->getFolderTextColor());
  } else {
    painter->setPen((isCurrent) ? m_treeView->getSelectedTextColor()
                                : m_treeView->getTextColor());
  }

  painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, name);

  // project folder node, version control node
  if (pnode || vcpNode) {
    painter->setPen(m_treeView->getTextColor());
    if ((pnode && pnode->isCurrent()) || (vcpNode && vcpNode->isCurrent()))
      painter->setBrush(Qt::red);
    else
      painter->setBrush(Qt::NoBrush);
    int d = 8;
    int y = (rect.height() - d) / 2;
    painter->drawEllipse(rect.x() - d - 4, rect.y() + y, d, d);
  }
  if (vcNode && vcNode->isUnderVersionControl() &&
      TFileStatus(vcNode->getPath()).doesExist() && !vcNode->isUnversioned()) {
    if (vcNode->isSynched()) {
      painter->setPen(Qt::darkGreen);
      painter->setBrush(Qt::green);
    } else {
      painter->setPen(Qt::darkYellow);
      painter->setBrush(Qt::yellow);
    }
    int d = 8;
    painter->drawRect(rect.x() - d, rect.y(), d, d);
  } else if (vcNode && vcNode->isUnversioned()) {
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::blue);
    int d = 4;
    painter->drawRect(rect.x() - d, rect.y(), d, d * 3);
    painter->drawRect(rect.x() - d * 2, rect.y() + d, d * 3, d);
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeViewDelegate::setEditorData(QWidget *editor,
                                          const QModelIndex &index) const {
#if QT_VERSION >= 0x050000
  if (index.data().canConvert(QMetaType::QString))
#else
  if (qVariantCanConvert<QString>(index.data()))
#endif
    NodeEditor *nodeEditor = qobject_cast<NodeEditor *>(editor);
  else
    QAbstractItemDelegate::setEditorData(editor, index);
}

//-----------------------------------------------------------------------------

void DvDirTreeViewDelegate::setModelData(QWidget *editor,
                                         QAbstractItemModel *model,
                                         const QModelIndex &index) const {
#if QT_VERSION >= 0x050000
  if (index.data().canConvert(QMetaType::QString))
#else
  if (qVariantCanConvert<QString>(index.data()))
#endif
  {
    NodeEditor *nodeEditor = qobject_cast<NodeEditor *>(editor);
    model->setData(index, qVariantFromValue(
                              nodeEditor->getText()));  // starEditor->text()));
  } else
    QAbstractItemDelegate::setModelData(editor, model, index);
}

//----------------------------------------------------------------------------

void DvDirTreeViewDelegate::commitAndCloseEditor() {
  NodeEditor *editor = qobject_cast<NodeEditor *>(sender());
  emit commitData(editor);
  emit closeEditor(editor);
  FileBrowser *fileBrowser =
      dynamic_cast<FileBrowser *>(m_treeView->parentWidget());
  if (fileBrowser) fileBrowser->onTreeFolderChanged();
}
//-----------------------------------------------------------------------------

QSize DvDirTreeViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const {
  return QSize(QItemDelegate::sizeHint(option, index).width() + 10, 22);
}

//-----------------------------------------------------------------------------

void DvDirTreeViewDelegate::updateEditorGeometry(
    QWidget *editor, const QStyleOptionViewItem &option,
    const QModelIndex &index) const {}

//=============================================================================
//
// DvDirTreeView
//
//-----------------------------------------------------------------------------

DvDirTreeView::DvDirTreeView(QWidget *parent)
    : QTreeView(parent)
    , m_globalSelectionEnabled(true)
    , m_currentDropItem(0)
    , m_refreshVersionControlEnabled(false)
    , m_currentRefreshedNode(0) {
  setModel(DvDirModel::instance());
  header()->close();
  setItemDelegate(new DvDirTreeViewDelegate(this));
  setSelectionMode(QAbstractItemView::SingleSelection);

  setIndentation(14);
  setAlternatingRowColors(true);

  // Connect all possible changes that can alter the
  // bottom horizontal scrollbar to resize contents...
  bool ret = true;
  ret      = ret && connect(this, SIGNAL(expanded(const QModelIndex &)), this,
                       SLOT(resizeToConts()));

  ret = ret && connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
                       SLOT(resizeToConts()));

  ret = ret && connect(this->model(), SIGNAL(layoutChanged()), this,
                       SLOT(resizeToConts()));

  if (Preferences::instance()->isWatchFileSystemEnabled()) {
    ret = ret && connect(this, SIGNAL(expanded(const QModelIndex &)), this,
                         SLOT(onExpanded(const QModelIndex &)));

    ret = ret && connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
                         SLOT(onCollapsed(const QModelIndex &)));
    addPathsToWatcher();
  }
  ret = ret && connect(MyFileSystemWatcher::instance(),
                       SIGNAL(directoryChanged(const QString &)), this,
                       SLOT(onMonitoredDirectoryChanged(const QString &)));

  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(preferenceChanged(const QString &)), this,
                       SLOT(onPreferenceChanged(const QString &)));

  assert(ret);

  setAcceptDrops(true);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::resizeToConts(void) { resizeColumnToContents(0); }

//-----------------------------------------------------------------------------

void DvDirTreeView::resizeEvent(QResizeEvent *event) {
  resizeColumnToContents(0);
  QTreeView::resizeEvent(event);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::dragEnterEvent(QDragEnterEvent *e) {
  const QMimeData *mimeData = e->mimeData();
  if (!acceptResourceDrop(mimeData->urls())) return;
  e->accept();
}

//-----------------------------------------------------------------------------

void DvDirTreeView::dragLeaveEvent(QDragLeaveEvent *e) {
  m_currentDropItem = 0;
  update();
}

//-----------------------------------------------------------------------------

void DvDirTreeView::dragMoveEvent(QDragMoveEvent *e) {
  const QMimeData *mimeData = e->mimeData();
  if (!acceptResourceDrop(mimeData->urls())) return;
  QModelIndex index = indexAt(e->pos());
  DvDirModelFileFolderNode *folderNode =
      dynamic_cast<DvDirModelFileFolderNode *>(
          DvDirModel::instance()->getNode(index));
  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  if (!node->isFolder()) return;
  m_currentDropItem = folderNode;
  update();
  e->accept();
}

//-----------------------------------------------------------------------------

void DvDirTreeView::dropEvent(QDropEvent *e) {
  const QMimeData *mimeData = e->mimeData();
  m_currentDropItem         = 0;
  update();
  QModelIndex index = indexAt(e->pos());
  DvDirModelFileFolderNode *folderNode =
      dynamic_cast<DvDirModelFileFolderNode *>(
          DvDirModel::instance()->getNode(index));
  if (!folderNode || !folderNode->isFolder()) return;
  if (!mimeData->hasUrls()) return;
  int count = 0;
  foreach (QUrl url, mimeData->urls()) {
    TFilePath srcFp(url.toLocalFile().toStdWString());
    TFilePath dstFp = folderNode->getPath();

    TFilePath path = dstFp + TFilePath(srcFp.getLevelNameW());
    NameBuilder *nameBuilder =
        NameBuilder::getBuilder(::to_wstring(path.getName()));
    std::wstring levelNameOut;
    do
      levelNameOut = nameBuilder->getNext();
    while (TSystem::doesExistFileOrLevel(path.withName(levelNameOut)));
    dstFp = path.withName(levelNameOut);

    if (dstFp != srcFp) {
      if (TSystem::copyFileOrLevel(dstFp, srcFp)) {
        TSystem::removeFileOrLevel(srcFp);
        FileBrowser::refreshFolder(srcFp.getParentDir());
      } else
        DVGui::error(tr("There was an error copying %1 to %2")
                         .arg(toQString(srcFp))
                         .arg(toQString(dstFp)));
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeView::contextMenuEvent(QContextMenuEvent *e) {
  QModelIndex index = indexAt(e->pos());
  if (!index.isValid()) return;

  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  if (!node) return;

  QMenu menu(this);

  if (!Preferences::instance()->isWatchFileSystemEnabled()) {
    QAction *refresh = CommandManager::instance()->getAction("MI_RefreshTree");
    menu.addAction(refresh);
  }

  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(node);
  if (vcNode) {
    TFilePath path       = vcNode->getPath();
    bool fileExists      = TFileStatus(path).doesExist();
    std::string pathType = path.getType();
    DvDirVersionControlProjectNode *vcProjectNode =
        dynamic_cast<DvDirVersionControlProjectNode *>(node);
    QAction *action;
    if (vcNode->isUnderVersionControl()) {
      if (vcProjectNode || (fileExists && pathType == "tnz")) {
        DvItemListModel::Status status = DvItemListModel::VC_None;
        // Check Version Control Status
        if (pathType == "tnz") {
          DvDirVersionControlNode *parent =
              dynamic_cast<DvDirVersionControlNode *>(vcNode->getParent());
          status = getItemVersionControlStatus(parent, path);
        } else {
          TFilePath fp =
              TProjectManager::instance()->projectFolderToProjectPath(path);
          status = getItemVersionControlStatus(vcNode, fp);
        }

        if (status == DvItemListModel::VC_ReadOnly) {
          action = menu.addAction(tr("Edit"));
          connect(action, SIGNAL(triggered()), this,
                  SLOT(editCurrentVersionControlNode()));
        } else if (status == DvItemListModel::VC_Edited) {
          action = menu.addAction("Unlock");
          connect(action, SIGNAL(triggered()), this,
                  SLOT(unlockCurrentVersionControlNode()));
        } else if (status == DvItemListModel::VC_Modified) {
          action = menu.addAction("Revert");
          connect(action, SIGNAL(triggered()), this,
                  SLOT(revertCurrentVersionControlNode()));
        }
      }

      action = menu.addAction(tr("Get"));
      connect(action, SIGNAL(triggered()), this,
              SLOT(updateCurrentVersionControlNode()));
    }
    if (fileExists) {
      action = menu.addAction(tr("Put..."));
      connect(action, SIGNAL(triggered()), this,
              SLOT(putCurrentVersionControlNode()));
    }
    if (vcNode->isUnderVersionControl() && fileExists) {
      DvDirVersionControlRootNode *rootNode =
          dynamic_cast<DvDirVersionControlRootNode *>(vcNode);
      if (!rootNode) {
        action = menu.addAction(tr("Delete"));
        connect(action, SIGNAL(triggered()), this,
                SLOT(deleteCurrentVersionControlNode()));
      }
    }
    if (pathType != "tnz" && vcNode->isUnderVersionControl()) {
      menu.addSeparator();

      action = menu.addAction(tr("Refresh"));
      connect(action, SIGNAL(triggered()), this,
              SLOT(refreshCurrentVersionControlNode()));

      if (fileExists) {
        menu.addSeparator();

        action = menu.addAction(tr("Cleanup"));
        connect(action, SIGNAL(triggered()), this,
                SLOT(cleanupCurrentVersionControlNode()));

        action = menu.addAction(tr("Purge"));
        connect(action, SIGNAL(triggered()), this,
                SLOT(purgeCurrentVersionControlNode()));
      }
    }
  }

  if (!menu.isEmpty()) menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void DvDirTreeView::createMenuAction(QMenu &menu, QString name,
                                     const char *slot, bool enable) {
  QAction *act = menu.addAction(name);
  act->setEnabled(enable);
  std::string slotName(slot);
  slotName = std::string("1") + slotName;
  connect(act, SIGNAL(triggered()), slotName.c_str());
}

//-----------------------------------------------------------------------------

QSize DvDirTreeView::sizeHint() const { return QSize(100, 100); }

//-----------------------------------------------------------------------------

TFilePath DvDirTreeView::getCurrentPath() const {
  DvDirModelFileFolderNode *node =
      dynamic_cast<DvDirModelFileFolderNode *>(getCurrentNode());

  return node ? node->getPath() : TFilePath();
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirTreeView::getCurrentNode() const {
  QModelIndex index    = currentIndex();
  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  return node;
}

//-----------------------------------------------------------------------------

void DvDirTreeView::enableCommands() {
  enableCommand(this, MI_Clear, &DvDirTreeView::deleteFolder);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::currentChanged(const QModelIndex &current,
                                   const QModelIndex &previous) {
  if (m_globalSelectionEnabled) {
    // rende la selezione corrente; serve per intercettare il comando MI_Clear
    makeCurrent();
  }

  // Automatic refresh of version control node

  if (refreshVersionControlEnabled() && isVisible() &&
      Preferences::instance()->isAutomaticSVNFolderRefreshEnabled()) {
    DvDirVersionControlNode *vcNode = dynamic_cast<DvDirVersionControlNode *>(
        DvDirModel::instance()->getNode(current));
    if (vcNode) refreshVersionControl(vcNode);
  }

  emit currentNodeChanged();
}

//-----------------------------------------------------------------------------

bool DvDirTreeView::edit(const QModelIndex &index, EditTrigger trigger,
                         QEvent *event) {
  return QAbstractItemView::edit(index, trigger, event);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::deleteFolder() {
  DvDirModel *model = DvDirModel::instance();
  QModelIndex index = currentIndex();
  if (!index.isValid()) return;
  QModelIndex parentIndex = index.parent();
  if (!parentIndex.isValid()) return;

  DvDirModelFileFolderNode *node =
      dynamic_cast<DvDirModelFileFolderNode *>(model->getNode(index));
  if (!node) return;
  if (!node->isRenameEnabled()) return;
  TFilePath fp = node->getPath();
  int ret = DVGui::MsgBox(tr("Delete folder ") + toQString(fp) + "?", tr("Yes"),
                          tr("No"), 1);
  if (ret == 2 || ret == 0) return;

  try {
    TSystem::rmDir(fp);
    IconGenerator::instance()->remove(fp);
  } catch (...) {
    DVGui::error(tr("It is not possible to delete the folder.") +
                 toQString(fp));
    return;
  }

  model->removeRow(index.row(), parentIndex);
  // m_model->refresh(parentIndex);
  setCurrentIndex(parentIndex);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::setCurrentNode(DvDirModelNode *node) {
  if (getCurrentNode() == node) return;
  QModelIndex index = DvDirModel::instance()->getIndexByNode(node);
  setCurrentIndex(index);
  scrollTo(index);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::setCurrentNode(const TFilePath &fp, bool expandNode) {
  DvDirModelFileFolderNode *node =
      dynamic_cast<DvDirModelFileFolderNode *>(getCurrentNode());
  if (node && node->getPath() == fp) return;
  QModelIndex index = DvDirModel::instance()->getIndexByPath(fp);
  setCurrentIndex(index);
  if (expandNode) expand(index);
  scrollTo(index /*, QAbstractItemView::PositionAtCenter*/);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::updateVersionControl(DvDirVersionControlNode *node) {
  DvDirVersionControlRootNode *rootNode =
      dynamic_cast<DvDirVersionControlRootNode *>(node);
  if (rootNode) {
    QString localPath = QString::fromStdWString(rootNode->getLocalPath());
    if (!QFile::exists(localPath)) {
      DVGui::warning(tr("The local path does not exist:") + " " + localPath);
      return;
    }

    VersionControl *vc = VersionControl::instance();
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));

    // Root node empty: perform an empty checkout on the top folder
    if (rootNode->getChildCount() == 0) {
      m_currentRefreshedNode = node;
      setRefreshVersionControlEnabled(false);

      if (isVisible())
        m_currentRefreshedNode->setTemporaryName(L" Checkout...");

      QStringList args;
      args << "checkout"
           << QString::fromStdWString(rootNode->getRepositoryPath()) << "."
           << "--depth=empty";
      connect(&m_thread, SIGNAL(error(const QString &)), this,
              SLOT(onCheckOutError(const QString &)));
      connect(&m_thread, SIGNAL(done(const QString &)), this,
              SLOT(onCheckOutDone(const QString &)));
      m_thread.executeCommand(localPath, "svn", args, true);
    }
    // Full checkout on the root node
    else
      vc->update(this, localPath + "/", QStringList(), 0);
  }
  // Perform a normal update (on an arbitrary node)
  else if (node) {
    VersionControl *vc = VersionControl::instance();

    // Get the root node to retrieve username and password
    DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
    if (rootNode) {
      vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
      vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
    }

    // Check if the path exist, otherwise, it is a missing folder / file that
    // has to be get.
    TFilePath path   = node->getPath();
    bool isSceneFile = path.getType() == "tnz";
    if (TFileStatus(path).doesExist() && !isSceneFile)
      vc->update(this, toQString(node->getPath()), QStringList("."), 0);
    else {
      // Find the workingDir (the first existing path)
      // and in the meantime, store the missing folders on files
      QStringList files;
      while (!TFileStatus(path).doesExist()) {
        files.prepend(toQString(path));
        path = path.getParentDir();
      }

      // Make the files relative to the workingDir and adjust the slash
      QString workingDir = toQString(path.getParentDir());
      QDir dir(workingDir);
      QStringList relativeFiles;
      for (int i = 0; i < files.count(); i++) {
#ifdef MACOSX
        relativeFiles << dir.relativeFilePath(files.at(i));
#else
        relativeFiles << dir.relativeFilePath(files.at(i)).replace("/", "\\");
#endif
      }

      relativeFiles.append(QString::fromStdWString(node->getName()));

      if (relativeFiles.count() == 1)
        vc->update(this, workingDir, relativeFiles, 0);
      else
        // Update the missing folders with non-recursive option ON
        vc->update(this, workingDir, relativeFiles, 0, true, false, true);
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeView::putVersionControl(DvDirVersionControlNode *node) {
  if (!node) return;
  VersionControl *vc = VersionControl::instance();

  // Get the root node to retrieve username and password
  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
  if (rootNode) {
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }

  TFilePath path   = node->getPath();
  bool isSceneFile = path.getType() == "tnz";

  if (node->isUnderVersionControl() && !isSceneFile) {
    QStringList files;
    files.append(".");

    if (dynamic_cast<DvDirVersionControlProjectNode *>(node)) {
      TFilePath fp =
          TProjectManager::instance()->projectFolderToProjectPath(path);
      files.append(toQString(fp.withoutParentDir()));
    }

    vc->commit(this, toQString(node->getPath()), files, true);
  } else
    vc->commit(this, toQString(path.getParentDir()),
               QStringList(QString::fromStdWString(node->getName())),
               !isSceneFile);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::cleanupVersionControl(DvDirVersionControlNode *node) {
  if (!node) return;

  TFilePath path   = node->getPath();
  bool isSceneFile = path.getType() == "tnz";
  if (isSceneFile) return;

  VersionControl *vc = VersionControl::instance();

  // Get the root node to retrieve username and password
  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
  if (rootNode) {
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }
  vc->cleanupFolder(this, toQString(path));
}

//-----------------------------------------------------------------------------

void DvDirTreeView::purgeVersionControl(DvDirVersionControlNode *node) {
  if (!node) return;

  TFilePath path   = node->getPath();
  bool isSceneFile = path.getType() == "tnz";
  if (isSceneFile) return;

  VersionControl *vc = VersionControl::instance();

  // Get the root node to retrieve username and password
  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
  if (rootNode) {
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }

  vc->purgeFolder(this, toQString(path));
}

//-----------------------------------------------------------------------------

void DvDirTreeView::deleteVersionControl(DvDirVersionControlNode *node) {
  if (!node) return;

  DvDirVersionControlNode *parentNode =
      dynamic_cast<DvDirVersionControlNode *>(node->getParent());
  if (!parentNode) return;

  VersionControl *vc = VersionControl::instance();

  // Get the root node to retrieve username and password
  DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
  if (rootNode) {
    vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
    vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
  }

  TFilePath path   = node->getPath();
  bool isSceneFile = path.getType() == "tnz";
  if (path.getType() == "tnz")
    vc->deleteFiles(this, toQString(parentNode->getPath()),
                    QStringList(QString::fromStdWString(node->getName())));
  else if (path.getType() == "")
    vc->deleteFolder(this, toQString(parentNode->getPath()),
                     QString::fromStdWString(node->getName()));
}

//-----------------------------------------------------------------------------

void DvDirTreeView::listVersionControl(
    DvDirVersionControlNode *lastExistingNode, const QString &relativePath) {
  if (!lastExistingNode) return;

  m_getSVNListRelativePath = relativePath;

  // Get existing node info (to retrieve its repository URL)
  QStringList args;
  args << "info";
  args << "--xml";

  m_thread.disconnect(SIGNAL(done(const QString &)));
  m_thread.disconnect(SIGNAL(error(const QString &)));
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onRefreshStatusError(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onInfoDone(const QString &)));
  m_thread.executeCommand(toQString(lastExistingNode->getPath()), "svn", args);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::refreshVersionControl(DvDirVersionControlNode *node,
                                          const QStringList &files) {
  if (!refreshVersionControlEnabled()) return;

  if (m_currentRefreshedNode) m_currentRefreshedNode->restoreName();

  m_currentRefreshedNode = node;
  QString tempName       = tr("Refreshing...");
  DvDirVersionControlRootNode *rootNode =
      dynamic_cast<DvDirVersionControlRootNode *>(node);
  if (rootNode) {
    QString path = QString::fromStdWString(rootNode->getLocalPath()) + "/";

    VersionControl *vc = VersionControl::instance();

    // Check if the localPath is a working copy (has a .svn subfolder)
    if (vc->isFolderUnderVersionControl(path)) {
      if (isVisible()) rootNode->setTemporaryName(tempName.toStdWString());

      if (rootNode) {
        vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
        vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
      }
      m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));
      m_thread.disconnect(SIGNAL(error(const QString &)));
      connect(&m_thread, SIGNAL(error(const QString &)), this,
              SLOT(onRefreshStatusError(const QString &)));
      connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
              SLOT(onRefreshStatusDone(const QString &)));
      setRefreshVersionControlEnabled(false);
      m_thread.getSVNStatus(path, true, true);
    }
  } else if (node) {
    TFilePath path   = node->getPath();
    bool isSceneFile = path.getType() == "tnz";

    QString nodePath = toQString(node->getPath());
    if (!isSceneFile) {
      // Check if there is a .svn folder inside... and set automatically the
      // version control
      nodePath.append("/");
      QDir dir(nodePath);
      dir.setFilter(QDir::Dirs | QDir::Hidden);
      node->setIsUnderVersionControl(
          !dir.exists() ||
          VersionControl::instance()->isFolderUnderVersionControl(nodePath));
    }

    if ((!isSceneFile && QDir(nodePath).exists()) ||
        (isSceneFile && QFile(nodePath).exists())) {
      VersionControl *vc                    = VersionControl::instance();
      DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
      if (rootNode) {
        vc->setUserName(QString::fromStdWString(rootNode->getUserName()));
        vc->setPassword(QString::fromStdWString(rootNode->getPassword()));
      }
      m_thread.disconnect(SIGNAL(statusRetrieved(const QString &)));
      m_thread.disconnect(SIGNAL(error(const QString &)));
      connect(&m_thread, SIGNAL(error(const QString &)), this,
              SLOT(onRefreshStatusError(const QString &)));
      connect(&m_thread, SIGNAL(statusRetrieved(const QString &)), this,
              SLOT(onRefreshStatusDone(const QString &)));
      if (files.isEmpty()) {
        if (isVisible() && node->isUnderVersionControl())
          node->setTemporaryName(tempName.toStdWString());

        if (isSceneFile) {
          if (node->isUnderVersionControl()) {
            node->setIsUnversioned(false);
            setRefreshVersionControlEnabled(false);
            m_thread.getSVNStatus(
                toQString(node->getPath().getParentDir()),
                QStringList(toQString(node->getPath().withoutParentDir())),
                false, true);
          }
        } else {
          if (node->isUnderVersionControl()) {
            node->setIsUnversioned(false);
            setRefreshVersionControlEnabled(false);
            m_thread.getSVNStatus(nodePath, true, true);
          }
        }
      } else {
        if (isVisible()) node->setTemporaryName(tempName.toStdWString());
        setRefreshVersionControlEnabled(false);
        m_thread.getSVNStatus(nodePath, files, true, true);
      }
    }
    // Missing node: call svn list to retrieve the list of files...
    // Missing node that isSceneFile doesn't needs any update
    else if (!isSceneFile) {
      DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
      if (rootNode) {
        VersionControl::instance()->setUserName(
            QString::fromStdWString(rootNode->getUserName()));
        VersionControl::instance()->setPassword(
            QString::fromStdWString(rootNode->getPassword()));

        QString nodePath = toQString(node->getPath());

        DvDirModelNode *n    = node->getParent();
        QString relativePath = QString::fromStdWString(node->getName());

        // get the last existing parent node, and store in the meantime, its
        // relative path
        while (!QDir(nodePath).exists() && n != rootNode) {
          if (!n) break;
          relativePath.prepend(QString::fromStdWString(n->getName()) + "/");
          n = n->getParent();
        }

        if (isVisible()) node->setTemporaryName(tempName.toStdWString());

        setRefreshVersionControlEnabled(false);
        listVersionControl(dynamic_cast<DvDirVersionControlNode *>(n),
                           relativePath);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeView::editCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;

  TFilePath path = vcNode->getPath();
  if (path.getType() == "tnz") {
    QStringList files;
    files.append(QString::fromStdWString(vcNode->getName()));

    int sceneIconsCount = 0;

    TFilePath iconPath = ToonzScene::getIconPath(path);
    if (TFileStatus(iconPath).doesExist()) {
      QDir dir(toQString(path.getParentDir()));

#ifdef MACOSX
      files.append(dir.relativeFilePath(toQString(iconPath)));
#else
      files.append(
          dir.relativeFilePath(toQString(iconPath)).replace("/", "\\"));
#endif

      sceneIconsCount++;
    }
    VersionControl::instance()->lock(this, toQString(path.getParentDir()),
                                     files, sceneIconsCount);
  } else {
    TFilePath fp =
        TProjectManager::instance()->projectFolderToProjectPath(path);

    TProject *currentProject =
        TProjectManager::instance()->getCurrentProject().getPointer();
    if (!currentProject) return;
    TFilePath sceneFolder =
        currentProject->decode(currentProject->getFolder(TProject::Scenes));
    TFilePath scenesDescPath = sceneFolder + "scenes.xml";
    QDir dir(toQString(fp.getParentDir()));

    QStringList files;
    files.append(toQString(fp.withoutParentDir()));

#ifdef MACOSX
    files.append(dir.relativeFilePath(toQString(scenesDescPath)));
#else
    files.append(
        dir.relativeFilePath(toQString(scenesDescPath)).replace("/", "\\"));
#endif

    VersionControl::instance()->lock(this, toQString(path), files, 0);
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeView::unlockCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;

  TFilePath path = vcNode->getPath();
  if (path.getType() == "tnz") {
    QStringList files;
    files.append(QString::fromStdWString(vcNode->getName()));

    int sceneIconsCount = 0;

    TFilePath iconPath = ToonzScene::getIconPath(path);
    if (TFileStatus(iconPath).doesExist()) {
      QDir dir(toQString(path.getParentDir()));

#ifdef MACOSX
      files.append(dir.relativeFilePath(toQString(iconPath)));
#else
      files.append(
          dir.relativeFilePath(toQString(iconPath)).replace("/", "\\"));
#endif

      sceneIconsCount++;
    }
    VersionControl::instance()->unlock(this, toQString(path.getParentDir()),
                                       files, sceneIconsCount);
  } else {
    TFilePath fp =
        TProjectManager::instance()->projectFolderToProjectPath(path);

    TProject *currentProject =
        TProjectManager::instance()->getCurrentProject().getPointer();
    if (!currentProject) return;
    TFilePath sceneFolder =
        currentProject->decode(currentProject->getFolder(TProject::Scenes));
    TFilePath scenesDescPath = sceneFolder + "scenes.xml";
    QDir dir(toQString(path));

    QStringList files;
    files.append(toQString(fp.withoutParentDir()));

#ifdef MACOSX
    files.append(dir.relativeFilePath(toQString(scenesDescPath)));
#else
    files.append(
        dir.relativeFilePath(toQString(scenesDescPath)).replace("/", "\\"));
#endif
    VersionControl::instance()->unlock(this, toQString(path), files, 0);
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeView::revertCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;

  TFilePath path = vcNode->getPath();
  if (path.getType() == "tnz") {
    QStringList files;
    files.append(QString::fromStdWString(vcNode->getName()));

    int sceneIconsCount = 0;

    TFilePath iconPath = ToonzScene::getIconPath(path);
    if (TFileStatus(iconPath).doesExist()) {
      QDir dir(toQString(path.getParentDir()));

#ifdef MACOSX
      files.append(dir.relativeFilePath(toQString(iconPath)));
#else
      files.append(
          dir.relativeFilePath(toQString(iconPath)).replace("/", "\\"));
#endif

      sceneIconsCount++;
    }
    VersionControl::instance()->revert(this, toQString(path.getParentDir()),
                                       files, false, sceneIconsCount);
  } else {
    TFilePath fp =
        TProjectManager::instance()->projectFolderToProjectPath(path);
    TProject *currentProject =
        TProjectManager::instance()->getCurrentProject().getPointer();
    if (!currentProject) return;
    TFilePath sceneFolder =
        currentProject->decode(currentProject->getFolder(TProject::Scenes));
    TFilePath scenesDescPath = sceneFolder + "scenes.xml";
    QDir dir(toQString(path));

    QStringList files;
    files.append(toQString(fp.withoutParentDir()));

#ifdef MACOSX
    files.append(dir.relativeFilePath(toQString(scenesDescPath)));
#else
    files.append(
        dir.relativeFilePath(toQString(scenesDescPath)).replace("/", "\\"));
#endif

    VersionControl::instance()->revert(this, toQString(path), files, false, 0);
  }
}

//-----------------------------------------------------------------------------

void DvDirTreeView::updateCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;
  updateVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::putCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;
  putVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::deleteCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;
  deleteVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::refreshCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;
  setRefreshVersionControlEnabled(true);
  refreshVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::cleanupCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;
  cleanupVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::purgeCurrentVersionControlNode() {
  DvDirVersionControlNode *vcNode =
      dynamic_cast<DvDirVersionControlNode *>(getCurrentNode());
  if (!vcNode) return;
  purgeVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onCheckOutError(const QString &text) {
  disconnect(&m_thread, SIGNAL(error(const QString &)), this,
             SLOT(onCheckOutError(const QString &)));
  disconnect(&m_thread, SIGNAL(done(const QString &)), this,
             SLOT(onCheckOutDone(const QString &)));

  if (isVisible()) {
    if (m_currentRefreshedNode) m_currentRefreshedNode->restoreName();
  }

  setRefreshVersionControlEnabled(true);

  DVGui::error(tr("Refresh operation failed:\n") + text);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onCheckOutDone(const QString &text) {
  disconnect(&m_thread, SIGNAL(error(const QString &)), this,
             SLOT(onCheckOutError(const QString &)));
  disconnect(&m_thread, SIGNAL(done(const QString &)), this,
             SLOT(onCheckOutDone(const QString &)));

  if (isVisible()) {
    if (!m_currentRefreshedNode) return;

    m_currentRefreshedNode->restoreName();
    QModelIndex index =
        DvDirModel::instance()->getIndexByNode(m_currentRefreshedNode);
    DvDirModel::instance()->refresh(index);
  }
  setRefreshVersionControlEnabled(true);
  // Refresh the node
  refreshCurrentVersionControlNode();
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onInfoDone(const QString &xmlResponse) {
  // Retrieve the node Repository URL
  SVNInfoReader ir(xmlResponse);
  QString repositoryURL = ir.getURL() + "/" + m_getSVNListRelativePath;

  QStringList args;
  args << "list";
  args << repositoryURL;
  args << "--xml";

  disconnect(&m_thread, SIGNAL(done(const QString &)), this,
             SLOT(onInfoDone(const QString &)));
  if (!m_currentRefreshedNode) return;

  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onListDone(const QString &)));
  m_thread.executeCommand(toQString(m_currentRefreshedNode->getPath()), "svn",
                          args);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onListDone(const QString &xmlResponse) {
  if (!m_currentRefreshedNode) return;

  m_currentRefreshedNode->restoreName();

  SVNListReader lr(xmlResponse);
  QStringList dirs = lr.getDirs();

  for (int i = 0; i < dirs.size(); i++) {
    SVNStatus s;
    s.m_item = "missing";
    s.m_path = dirs.at(i);
    m_currentRefreshedNode->insertVersionControlStatus(s.m_path, s);
  }

  QModelIndex index =
      DvDirModel::instance()->getIndexByNode(m_currentRefreshedNode);
  DvDirModel::instance()->refresh(index);

  setRefreshVersionControlEnabled(true);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onRefreshStatusDone(const QString &text) {
  if (!isVisible()) return;
  if (!m_currentRefreshedNode) return;

  m_currentRefreshedNode->restoreName();

  TFilePath nodePath = m_currentRefreshedNode->getPath();
  if (nodePath.isEmpty()) return;

  m_currentRefreshedNode->setExists(TFileStatus(nodePath).doesExist());

  bool nodeChanged = false;

  // If needed, transform the DvDirVersionControlNode "vcNode" to a
  // DvDirVersionControlProjectNode
  // And put it on the tree (removing the previous entry)
  if (m_currentRefreshedNode->getNodeType() != "Project" &&
      TProjectManager::instance()->isProject(nodePath)) {
    DvDirModelNode *parentNode = m_currentRefreshedNode->getParent();
    if (parentNode) {
      std::wstring name = m_currentRefreshedNode->getName();

      // Remove the old node (which is not a project node)
      DvDirModel::instance()->removeRows(
          m_currentRefreshedNode->getRow(), 1,
          DvDirModel::instance()->getIndexByNode(parentNode));
      // parentNode->removeChildren(vcNode->getRow(),1);

      // Add a new project node instead of the old one
      DvDirVersionControlProjectNode *newNode =
          new DvDirVersionControlProjectNode(parentNode, name, nodePath);
      parentNode->addChild(newNode);
      nodeChanged            = true;
      m_currentRefreshedNode = newNode;
    }
  }

  SVNStatusReader sr(text);
  QStringList checkPartialLockList =
      m_currentRefreshedNode->refreshVersionControl(sr.getStatus());

  if (!checkPartialLockList.isEmpty())
    checkPartialLock(toQString(nodePath), checkPartialLockList);
  else {
    // Refresh also the right side (thumbnails)
    if (nodeChanged && m_currentRefreshedNode)
      setCurrentNode(m_currentRefreshedNode);
    emit currentNodeChanged();

    QModelIndex index =
        DvDirModel::instance()->getIndexByNode(m_currentRefreshedNode);
    DvDirModel::instance()->refresh(index);
  }

  setRefreshVersionControlEnabled(true);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onRefreshStatusError(const QString &text) {
  if (!isVisible()) return;
  m_currentRefreshedNode->restoreName();
  setRefreshVersionControlEnabled(true);
  DVGui::error(tr("Refresh operation failed:\n") + text);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::checkPartialLock(const QString &workingDir,
                                     const QStringList &files) {
  QStringList args;
  args << "propget";
  args << "partial-lock";
  int filesCount = files.count();
  for (int i = 0; i < filesCount; i++) args << files.at(i);
  args << "--xml";

  m_thread.disconnect(SIGNAL(done(const QString &)));
  m_thread.disconnect(SIGNAL(error(const QString &)));
  connect(&m_thread, SIGNAL(error(const QString &)), this,
          SLOT(onCheckPartialLockError(const QString &)));
  connect(&m_thread, SIGNAL(done(const QString &)), this,
          SLOT(onCheckPartialLockDone(const QString &)));

  m_thread.executeCommand(workingDir, "svn", args, true);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onCheckPartialLockError(const QString &text) {
  m_thread.disconnect(SIGNAL(done(const QString &)));
  m_thread.disconnect(SIGNAL(error(const QString &)));
  setRefreshVersionControlEnabled(true);
  DVGui::error(tr("Refresh operation failed:\n") + text);
}

//-----------------------------------------------------------------------------

void DvDirTreeView::onCheckPartialLockDone(const QString &xmlResults) {
  if (!m_currentRefreshedNode) return;

  m_thread.disconnect(SIGNAL(done(const QString &)));
  m_thread.disconnect(SIGNAL(error(const QString &)));

  SVNPartialLockReader reader(xmlResults);
  QList<SVNPartialLock> list = reader.getPartialLock();

  QString userName = VersionControl::instance()->getUserName();
  QString hostName = QHostInfo::localHostName();

  int count = list.size();
  for (int i = 0; i < count; i++) {
    SVNPartialLock lock = list.at(i);

    // Check if the pair SVN username / Hostname is inside the lock list...
    bool havePartialLock           = false;
    unsigned int from              = 0;
    unsigned int to                = 0;
    QList<SVNPartialLockInfo> list = lock.m_partialLockList;
    for (int i = 0; i < list.size(); i++) {
      SVNPartialLockInfo info = list.at(i);
      if (info.m_userName == userName && info.m_hostName == hostName) {
        havePartialLock = true;
        from            = info.m_from;
        to              = info.m_to;
        break;
      }
    }

    // Modify the SVNStatus
    SVNStatus s =
        m_currentRefreshedNode->getVersionControlStatus(lock.m_fileName);

    if (havePartialLock) {
      s.m_isPartialEdited = true;
      s.m_isPartialLocked = false;
      s.m_editFrom        = from;
      s.m_editTo          = to;
    } else {
      s.m_isPartialEdited = false;
      s.m_isPartialLocked = true;
    }
    m_currentRefreshedNode->insertVersionControlStatus(lock.m_fileName, s);
  }
  m_currentRefreshedNode->restoreName();
  QModelIndex index =
      DvDirModel::instance()->getIndexByNode(m_currentRefreshedNode);
  DvDirModel::instance()->refresh(index);
  emit currentNodeChanged();
  setRefreshVersionControlEnabled(true);
}

//-----------------------------------------------------------------------------

DvItemListModel::Status DvDirTreeView::getItemVersionControlStatus(
    DvDirVersionControlNode *node, const TFilePath &fp) {
  if (!node) return DvItemListModel::VC_None;

  QString name = toQString(fp.withoutParentDir());

  QDir dir(toQString(node->getPath()));
  TFileStatus fs(node->getPath() + name.toStdWString());

  SVNStatus s = node->getVersionControlStatus(name);
  if (s.m_path == name) {
    if (s.m_item == "missing" ||
        s.m_item == "none" && s.m_repoStatus == "added")
      return DvItemListModel::VC_Missing;
    if (s.m_isLocked) {
      DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
      if (rootNode) {
        if (QString::fromStdWString(rootNode->getUserName()) != s.m_lockOwner ||
            TSystem::getHostName() != s.m_lockHostName)
          return DvItemListModel::VC_Locked;
        else if (s.m_item == "normal" && s.m_repoStatus == "none")
          return DvItemListModel::VC_Edited;
      }
    }

    if (s.m_item == "unversioned") return DvItemListModel::VC_Unversioned;
    if (s.m_isPartialEdited) {
      QString from                          = QString::number(s.m_editFrom);
      QString to                            = QString::number(s.m_editTo);
      DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();
      QString userName = QString::fromStdWString(rootNode->getUserName());
      QString hostName = TSystem::getHostName();

      TFilePath fp(s.m_path.toStdWString());
      QString tempFileName = QString::fromStdWString(fp.getWideName()) + "_" +
                             userName + "_" + hostName + "_" + from + "-" + to +
                             "." + QString::fromStdString(fp.getType());

      if (dir.exists(tempFileName))
        return DvItemListModel::VC_PartialModified;
      else
        return DvItemListModel::VC_PartialEdited;
    }
    if (s.m_isPartialLocked) return DvItemListModel::VC_PartialLocked;
    // Pay attention: "ToUpdate" is more important than "ReadOnly"
    if (s.m_item == "normal" && s.m_repoStatus == "modified")
      return DvItemListModel::VC_ToUpdate;
    if (!fs.isWritable() || s.m_item == "normal")
      return DvItemListModel::VC_ReadOnly;
    // If, for some errors, there is some item added locally but not committed
    // yet, use the modified status
    if (s.m_item == "modified" || s.m_item == "added")
      return DvItemListModel::VC_Modified;
  } else if (fp.getDots() == "..") {
    // Get the files list to control its status...
    QStringList levelNames = getLevelFileNames(fp);

    if (levelNames.isEmpty()) return DvItemListModel::VC_Missing;

    DvDirVersionControlRootNode *rootNode = node->getVersionControlRootNode();

    // Browse the files and count the single status
    unsigned int missingCount = 0, normalCount = 0, lockedCount = 0,
                 unversionedCount = 0, readOnlyCount = 0, modifiedCount = 0;
    int levelCount = levelNames.size();

    // In the meantime I will set the from and to range index
    int from = 0, to = 0;
    for (int i = 0; i < levelCount; i++) {
      SVNStatus s = node->getVersionControlStatus(levelNames.at(i));
      TFileStatus fs(node->getPath() + levelNames.at(i).toStdWString());

      if (s.m_item == "missing" ||
          s.m_item == "none" && s.m_repoStatus == "added")
        missingCount++;
      else if (s.m_item == "modified")
        modifiedCount++;
      else if (s.m_isLocked) {
        if (QString::fromStdWString(rootNode->getUserName()) != s.m_lockOwner)
          lockedCount++;
        else {
          if (from == 0) {
            from = i + 1;
            to   = i + 1;
          } else
            to = i + 1;
          normalCount++;
        }
      } else if (s.m_item == "unversioned")
        unversionedCount++;
      else if (s.m_item == "normal" && s.m_repoStatus == "modified")
        return DvItemListModel::VC_ToUpdate;
      else if (!fs.isWritable() || s.m_item == "normal")
        readOnlyCount++;
    }

    if (missingCount != 0) {
      if (missingCount == levelCount)
        return DvItemListModel::VC_Missing;
      else
        return DvItemListModel::VC_ToUpdate;
    } else if (unversionedCount != 0)
      return DvItemListModel::VC_Unversioned;
    else if (normalCount != 0) {
      if (normalCount == levelCount)
        return DvItemListModel::VC_Edited;
      else if (modifiedCount == 0) {
        // Check if exists the temporary hook file
        if (from != -1 && to != -1) {
          DvDirVersionControlRootNode *rootNode =
              node->getVersionControlRootNode();
          QString userName = QString::fromStdWString(rootNode->getUserName());
          QString hostName = TSystem::getHostName();

          QString tempFileName =
              QString::fromStdWString(fp.getWideName()) + "_" + userName + "_" +
              hostName + "_" + QString::number(from) + "-" +
              QString::number(to) + "." + QString::fromStdString(fp.getType());

          TFilePath hookFile = TXshSimpleLevel::getHookPath(
              fp.getParentDir() + tempFileName.toStdWString());
          if (TFileStatus(hookFile).doesExist())
            return DvItemListModel::VC_PartialModified;
        }
        return DvItemListModel::VC_PartialEdited;
      } else if (modifiedCount != 0)
        return DvItemListModel::VC_PartialModified;
    } else if (modifiedCount != 0) {
      if (modifiedCount == levelCount)
        return DvItemListModel::VC_Modified;
      else
        return DvItemListModel::VC_PartialModified;
    } else if (lockedCount != 0) {
      if (lockedCount == levelCount)
        return DvItemListModel::VC_Locked;
      else
        return DvItemListModel::VC_PartialLocked;
    } else if (readOnlyCount == levelCount)
      return DvItemListModel::VC_ReadOnly;
    return DvItemListModel::VC_None;
  }
  return DvItemListModel::VC_None;
}

/*- Refresh monitoring paths according to expand/shrink state of the folder tree
 * -*/
void DvDirTreeView::addPathsToWatcher() {
  QStringList paths;
  getExpandedPathsRecursive(rootIndex(), paths);
  if (!paths.isEmpty()) MyFileSystemWatcher::instance()->addPaths(paths);
}

void DvDirTreeView::getExpandedPathsRecursive(const QModelIndex &index,
                                              QStringList &paths) {
  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  DvDirModelFileFolderNode *fileFolderNode =
      dynamic_cast<DvDirModelFileFolderNode *>(node);
  if (fileFolderNode) {
    QString path = toQString(fileFolderNode->getPath());
    if (!paths.contains(path)) {
      paths.append(path);
    }
  }
  /*- serch child nodes if this node is expanded -*/
  if (index != rootIndex() && !isExpanded(index)) return;

  int count = DvDirModel::instance()->rowCount(index);
  for (int r = 0; r < count; r++) {
    QModelIndex child = DvDirModel::instance()->index(r, 0, index);
    getExpandedPathsRecursive(child, paths);
  }
}

void DvDirTreeView::onExpanded(const QModelIndex &index) {
  QStringList paths;
  int count = DvDirModel::instance()->rowCount(index);
  for (int r = 0; r < count; r++) {
    QModelIndex child = DvDirModel::instance()->index(r, 0, index);
    getExpandedPathsRecursive(child, paths);
  }
  MyFileSystemWatcher::instance()->addPaths(paths);
}

void DvDirTreeView::onCollapsed(const QModelIndex &index) {
  QStringList paths;
  int count = DvDirModel::instance()->rowCount(index);
  for (int r = 0; r < count; r++) {
    QModelIndex child = DvDirModel::instance()->index(r, 0, index);
    getExpandedPathsRecursive(child, paths);
  }
  MyFileSystemWatcher::instance()->removePaths(paths);
}

void DvDirTreeView::onMonitoredDirectoryChanged(const QString &dirPath) {
  DvDirModel::instance()->refreshFolder(TFilePath(dirPath));

  /*- the change may be adding of a new folder, which is needed to be added to
   * the monitored paths -*/
  DvDirModelNode *node = DvDirModel::instance()
                             ->getNode(rootIndex())
                             ->getNodeByPath(TFilePath(dirPath));
  if (!node) return;
  QStringList paths;
  for (int c = 0; c < node->getChildCount(); c++) {
    DvDirModelFileFolderNode *childNode =
        dynamic_cast<DvDirModelFileFolderNode *>(node->getChild(c));
    if (childNode) paths.append(toQString(childNode->getPath()));
  }
  MyFileSystemWatcher::instance()->addPaths(paths, true);
}

// on preference changed, update file browser's behavior
void DvDirTreeView::onPreferenceChanged(const QString &prefName) {
  // react only when the related preference is changed
  if (prefName != "WatchFileSystem") return;
  bool ret = true;
  if (Preferences::instance()->isWatchFileSystemEnabled()) {
    ret = ret && connect(this, SIGNAL(expanded(const QModelIndex &)), this,
                         SLOT(onExpanded(const QModelIndex &)));

    ret = ret && connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
                         SLOT(onCollapsed(const QModelIndex &)));
    addPathsToWatcher();
  } else {
    ret = ret && disconnect(this, SIGNAL(expanded(const QModelIndex &)), this,
                            SLOT(onExpanded(const QModelIndex &)));
    ret = ret && disconnect(this, SIGNAL(collapsed(const QModelIndex &)), this,
                            SLOT(onCollapsed(const QModelIndex &)));

    MyFileSystemWatcher::instance()->removeAllPaths();
  }
  assert(ret);
}

//=============================================================================
// NodeEditor
//-----------------------------------------------------------------------------

NodeEditor::NodeEditor(QWidget *parent, QRect rect, int leftMargin)
    : QWidget(parent) {
  setGeometry(rect);
  m_lineEdit          = new LineEdit();
  QHBoxLayout *layout = new QHBoxLayout();
  layout->setMargin(0);
  layout->addSpacing(leftMargin);
  layout->addWidget(m_lineEdit);
  setLayout(layout);
  connect(m_lineEdit, SIGNAL(editingFinished()), this, SLOT(emitFinished()));
}

//-----------------------------------------------------------------------------

void NodeEditor::focusInEvent(QFocusEvent *) {
  m_lineEdit->setFocus();
  m_lineEdit->selectAll();
}

//-----------------------------------------------------------------------------

void NodeEditor::emitFinished() { emit editingFinished(); }
