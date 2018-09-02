

#include "filebrowser.h"

// Tnz6 includes
#include "dvdirtreeview.h"
#include "filebrowsermodel.h"
#include "fileselection.h"
#include "filmstripselection.h"
#include "castselection.h"
#include "menubarcommandids.h"
#include "floatingpanelcommand.h"
#include "iocommand.h"
#include "history.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/trepetitionguard.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/screensavermaker.h"
#include "toonz/tproject.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/namebuilder.h"
#include "toonz/toonzimageutils.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "tconvert.h"
#include "tfiletype.h"
#include "tlevel_io.h"

// Qt includes
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QBoxLayout>
#include <QLabel>
#include <QByteArray>
#include <QMenu>
#include <QDateTime>
#include <QInputDialog>
#include <QDesktopServices>
#include <QDirModel>
#include <QDir>
#include <QPixmap>
#include <QUrl>
#include <QRegExp>
#include <QScrollBar>
#include <QMap>
#include <QPushButton>
#include <QPalette>
#include <QCheckBox>
#include <QMutex>
#include <QMutexLocker>
#include <QMessageBox>
#include <QApplication>
#include <QFormLayout>
#include <QMainWindow>
#include <QLineEdit>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QFileSystemWatcher>

// tcg includes
#include "tcg/boost/range_utility.h"
#include "tcg/boost/permuted_range.h"

// boost includes
#include <boost/bind.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>

namespace ba = boost::adaptors;

using namespace DVGui;

//=============================================================================
//      Local declarations
//=============================================================================

//============================
//    FrameCountTask class
//----------------------------

class FrameCountTask final : public TThread::Runnable {
  bool m_started;

  TFilePath m_path;
  QDateTime m_modifiedDate;

public:
  FrameCountTask(const TFilePath &path, const QDateTime &modifiedDate);

  ~FrameCountTask();

  void run() override;

  QThread::Priority runningPriority() override;

public slots:

  void onStarted(TThread::RunnableP thisTask) override;
  void onCanceled(TThread::RunnableP thisTask) override;
};

//============================
//    FCData struct
//----------------------------

struct FCData {
  QDateTime m_date;
  int m_frameCount;
  bool m_underProgress;
  int m_retryCount;

  FCData() {}
  FCData(const QDateTime &date);
};

//=============================================================================
//      Local namespace
//=============================================================================

namespace {
std::set<FileBrowser *> activeBrowsers;
std::map<TFilePath, FCData> frameCountMap;
QMutex frameCountMapMutex;
QMutex levelFileMutex;

}  // namespace

inline bool isMultipleFrameType(std::string type) {
  return (type == "tlv" || type == "tzl" || type == "pli" || type == "mov" ||
          type == "avi" || type == "3gp" || type == "gif" || type == "mp4" ||
          type == "webm");
}

//=============================================================================
// FileBrowser
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
FileBrowser::FileBrowser(QWidget *parent, Qt::WindowFlags flags,
                         bool noContextMenu, bool multiSelectionEnabled)
#else
FileBrowser::FileBrowser(QWidget *parent, Qt::WFlags flags, bool noContextMenu,
                         bool multiSelectionEnabled)
#endif
    : QFrame(parent), m_folderName(0), m_itemViewer(0) {
  // style sheet
  setObjectName("FileBrowser");
  setFrameStyle(QFrame::StyledPanel);

  m_mainSplitter      = new QSplitter(this);
  m_folderTreeView    = new DvDirTreeView(this);
  QFrame *box         = new QFrame(this);
  QLabel *folderLabel = new QLabel(tr("Folder: "), this);
  m_folderName        = new QLineEdit();
  m_itemViewer = new DvItemViewer(box, noContextMenu, multiSelectionEnabled,
                                  DvItemViewer::Browser);
  DvItemViewerTitleBar *titleBar = new DvItemViewerTitleBar(m_itemViewer, box);
  DvItemViewerButtonBar *buttonBar =
      new DvItemViewerButtonBar(m_itemViewer, box);
  DvItemViewerPanel *viewerPanel = m_itemViewer->getPanel();

  viewerPanel->addColumn(DvItemListModel::FileType, 50);
  viewerPanel->addColumn(DvItemListModel::FrameCount, 50);
  viewerPanel->addColumn(DvItemListModel::FileSize, 50);
  viewerPanel->addColumn(DvItemListModel::CreationDate, 130);
  viewerPanel->addColumn(DvItemListModel::ModifiedDate, 130);
  if (Preferences::instance()->isSVNEnabled())
    viewerPanel->addColumn(DvItemListModel::VersionControlStatus, 120);

  viewerPanel->setSelection(new FileSelection());
  DVItemViewPlayDelegate *itemViewPlayDelegate =
      new DVItemViewPlayDelegate(viewerPanel);
  viewerPanel->setItemViewPlayDelegate(itemViewPlayDelegate);

  m_mainSplitter->setObjectName("FileBrowserSplitter");
  m_folderTreeView->setObjectName("DirTreeView");
  box->setObjectName("castFrame");
  box->setFrameStyle(QFrame::StyledPanel);

  m_itemViewer->setModel(this);

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(3);
  mainLayout->setSpacing(2);
  {
    mainLayout->addWidget(buttonBar);

    QHBoxLayout *folderLay = new QHBoxLayout();
    folderLay->setMargin(0);
    folderLay->setSpacing(0);
    {
      folderLay->addWidget(folderLabel, 0);
      folderLay->addWidget(m_folderName, 1);
    }
    mainLayout->addLayout(folderLay, 0);

    m_mainSplitter->addWidget(m_folderTreeView);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setMargin(0);
    boxLayout->setSpacing(0);
    {
      boxLayout->addWidget(titleBar, 0);
      boxLayout->addWidget(m_itemViewer, 1);
    }
    m_mainSplitter->addWidget(box);
    mainLayout->addWidget(m_mainSplitter, 1);
  }
  setLayout(mainLayout);

  m_mainSplitter->setSizes(QList<int>() << 270 << 500);

  // signal-slot connections
  bool ret = connect(m_folderTreeView, SIGNAL(currentNodeChanged()),
                     itemViewPlayDelegate, SLOT(resetPlayWidget()));
  // if the current forder is changed in the folder tree, then update in the
  // item view
  ret = ret && connect(m_folderTreeView, SIGNAL(currentNodeChanged()), this,
                       SLOT(onTreeFolderChanged()));

  ret = ret && connect(m_itemViewer, SIGNAL(clickedItem(int)), this,
                       SLOT(onClickedItem(int)));
  ret =
      ret && connect(m_itemViewer, SIGNAL(selectedItems(const std::set<int> &)),
                     this, SLOT(onSelectedItems(const std::set<int> &)));
  ret = ret && connect(buttonBar, SIGNAL(folderUp()), this, SLOT(folderUp()));
  ret = ret && connect(buttonBar, SIGNAL(newFolder()), this, SLOT(newFolder()));

  ret = ret && connect(&m_frameCountReader, SIGNAL(calculatedFrameCount()),
                       m_itemViewer->getPanel(), SLOT(update()));

  QAction *refresh = CommandManager::instance()->getAction(MI_RefreshTree);
  ret = ret && connect(refresh, SIGNAL(triggered()), this, SLOT(refresh()));
  addAction(refresh);

  // Version Control instance connection
  if (Preferences::instance()->isSVNEnabled())
    ret =
        ret && connect(VersionControl::instance(),
                       SIGNAL(commandDone(const QStringList &)), this,
                       SLOT(onVersionControlCommandDone(const QStringList &)));

  // if the folderName is edited, move the current folder accordingly
  ret = ret && connect(m_folderName, SIGNAL(editingFinished()), this,
                       SLOT(onFolderEdited()));

  // folder history
  ret = ret && connect(m_folderTreeView, SIGNAL(currentNodeChanged()), this,
                       SLOT(storeFolderHistory()));
  ret = ret && connect(buttonBar, SIGNAL(folderBack()), this,
                       SLOT(onBackButtonPushed()));
  ret = ret && connect(buttonBar, SIGNAL(folderFwd()), this,
                       SLOT(onFwdButtonPushed()));
  // when the history changes, enable/disable the history buttons accordingly
  ret = ret && connect(this, SIGNAL(historyChanged(bool, bool)), buttonBar,
                       SLOT(onHistoryChanged(bool, bool)));

  // check out the update of the current folder.
  // Use MyFileSystemWatcher which is shared by all browsers.
  // Adding and removing paths to the watcher is done in DvDirTreeView.
  ret = ret && connect(MyFileSystemWatcher::instance(),
                       SIGNAL(directoryChanged(const QString &)), this,
                       SLOT(onFileSystemChanged(const QString &)));

  // store the first item("Root") in the history
  m_indexHistoryList.append(m_folderTreeView->currentIndex());
  m_currentPosition = 0;

  refreshHistoryButtons();

  assert(ret);
}

//-----------------------------------------------------------------------------

FileBrowser::~FileBrowser() {}

//-----------------------------------------------------------------------------
/*! when the m_folderName is edited, move the current folder accordingly
*/
void FileBrowser::onFolderEdited() {
  TFilePath inputPath(m_folderName->text().toStdWString());
  QModelIndex index = DvDirModel::instance()->getIndexByPath(inputPath);

  // If there is no node matched
  if (!index.isValid()) {
    QMessageBox::warning(this, tr("Open folder failed"),
                         tr("The input folder path was invalid."));
    return;
  }
  m_folderTreeView->collapseAll();

  m_folderTreeView->setCurrentIndex(index);

  // expand the folder tree
  QModelIndex tmpIndex = index;
  while (tmpIndex.isValid()) {
    m_folderTreeView->expand(tmpIndex);
    tmpIndex = tmpIndex.parent();
  }

  m_folderTreeView->scrollTo(index);
  m_folderTreeView->update();
}

//-----------------------------------------------------------------------------

void FileBrowser::storeFolderHistory() {
  QModelIndex currentModelIndex = m_folderTreeView->currentIndex();

  if (!currentModelIndex.isValid()) return;

  if (m_indexHistoryList[m_currentPosition] == currentModelIndex) return;

  // If there is no next history item, then create it
  if (m_currentPosition == m_indexHistoryList.size() - 1) {
    m_indexHistoryList << currentModelIndex;
    m_currentPosition++;
  }
  // If the next hitory item is the same as the current one, just move to it
  else if (m_indexHistoryList[m_currentPosition + 1] == currentModelIndex) {
    m_currentPosition++;
  }
  // If the next history item is different from the current one, then replace
  // with the new one
  else {
    int size = m_indexHistoryList.size();
    // remove the old history items
    for (int i = m_currentPosition + 1; i < size; i++)
      m_indexHistoryList.removeLast();
    m_indexHistoryList << currentModelIndex;
    m_currentPosition++;
  }
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshHistoryButtons() {
  emit historyChanged((m_currentPosition != 0),
                      (m_currentPosition != m_indexHistoryList.size() - 1));
}

//-----------------------------------------------------------------------------

void FileBrowser::onBackButtonPushed() {
  if (m_currentPosition == 0) return;
  m_currentPosition--;
  QModelIndex currentIndex = m_indexHistoryList[m_currentPosition];
  m_folderTreeView->setCurrentIndex(currentIndex);
  m_folderTreeView->collapseAll();
  while (currentIndex.isValid()) {
    currentIndex = currentIndex.parent();
    m_folderTreeView->expand(currentIndex);
  }
  m_folderTreeView->update();

  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------

void FileBrowser::onFwdButtonPushed() {
  if (m_currentPosition >= m_indexHistoryList.size() - 1) return;
  m_currentPosition++;
  QModelIndex currentIndex = m_indexHistoryList[m_currentPosition];
  m_folderTreeView->setCurrentIndex(currentIndex);
  m_folderTreeView->collapseAll();
  while (currentIndex.isValid()) {
    currentIndex = currentIndex.parent();
    m_folderTreeView->expand(currentIndex);
  }
  m_folderTreeView->update();

  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------
/*! clear the history when the tree date is replaced
*/
void FileBrowser::clearHistory() {
  int size = m_indexHistoryList.size();
  // leave the last item
  for (int i        = 1; i < size; i++) m_indexHistoryList.removeLast();
  m_currentPosition = 0;
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------
/*! update the current folder when changes detected from QFileSystemWatcher
*/
void FileBrowser::onFileSystemChanged(const QString &folderPath) {
  if (folderPath != m_folder.getQString()) return;
  // changes may create/delete of folder, so update the DvDirModel
  QModelIndex parentFolderIndex = m_folderTreeView->currentIndex();
  DvDirModel::instance()->refresh(parentFolderIndex);

  refreshCurrentFolderItems();
}

//-----------------------------------------------------------------------------

void FileBrowser::sortByDataModel(DataType dataType, bool isDiscendent) {
  struct locals {
    static inline bool itemLess(int aIdx, int bIdx, FileBrowser &fb,
                                DataType dataType) {
      return (fb.compareData(dataType, aIdx, bIdx) > 0);
    }

    static inline bool indexLess(int aIdx, int bIdx,
                                 const std::vector<int> &vec) {
      return (vec[aIdx] < vec[bIdx]);
    }

    static inline int complement(int val, int max) {
      return (assert(0 <= val && val <= max), max - val);
    }
  };  // locals

  if (dataType != getCurrentOrderType()) {
    // Build the permutation table
    std::vector<int> new2OldIdx(
        boost::make_counting_iterator(0),
        boost::make_counting_iterator(int(m_items.size())));

    std::stable_sort(
        new2OldIdx.begin(), new2OldIdx.end(),
        boost::bind(locals::itemLess, _1, _2, boost::ref(*this), dataType));

    // Use the renumbering table to permutate elements
    std::vector<Item>(
        boost::make_permutation_iterator(m_items.begin(), new2OldIdx.begin()),
        boost::make_permutation_iterator(m_items.begin(), new2OldIdx.end()))
        .swap(m_items);

    // Use the permutation table to update current file selection, if any
    FileSelection *fs =
        static_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());

    if (!fs->isEmpty()) {
      std::vector<int> old2NewIdx(
          boost::make_counting_iterator(0),
          boost::make_counting_iterator(int(m_items.size())));

      std::sort(old2NewIdx.begin(), old2NewIdx.end(),
                boost::bind(locals::indexLess, _1, _2, boost::ref(new2OldIdx)));

      std::vector<int> newSelectedIndices;
      tcg::substitute(
          newSelectedIndices,
          tcg::permuted_range(old2NewIdx, fs->getSelectedIndices() |
                                              ba::filtered(boost::bind(
                                                  std::less<int>(), _1,
                                                  int(old2NewIdx.size())))));

      fs->select(!newSelectedIndices.empty() ? &newSelectedIndices.front() : 0,
                 int(newSelectedIndices.size()));
    }

    setIsDiscendentOrder(true);
    setOrderType(dataType);
  }

  // Reverse lists if necessary
  if (isDiscendentOrder() != isDiscendent) {
    std::reverse(m_items.begin(), m_items.end());

    // Reverse file selection, if any
    FileSelection *fs =
        dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());

    if (!fs->isEmpty()) {
      int iCount = int(m_items.size()), lastIdx = iCount - 1;

      std::vector<int> newSelectedIndices;
      tcg::substitute(
          newSelectedIndices,
          fs->getSelectedIndices() |
              ba::filtered(boost::bind(std::less<int>(), _1, iCount)) |
              ba::transformed(boost::bind(locals::complement, _1, lastIdx)));

      fs->select(!newSelectedIndices.empty() ? &newSelectedIndices.front() : 0,
                 int(newSelectedIndices.size()));
    }

    setIsDiscendentOrder(isDiscendent);
  }

  m_itemViewer->getPanel()->update();
}

//-----------------------------------------------------------------------------

void FileBrowser::setFilterTypes(const QStringList &types) { m_filter = types; }

//-----------------------------------------------------------------------------

void FileBrowser::addFilterType(const QString &type) {
  if (!m_filter.contains(type)) m_filter.push_back(type);
}

//-----------------------------------------------------------------------------

void FileBrowser::removeFilterType(const QString &type) {
  m_filter.removeAll(type);
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshCurrentFolderItems() {
  m_items.clear();

  // put the parent directory item
  TFilePath parentFp = m_folder.getParentDir();
  if (parentFp != TFilePath("") && parentFp != m_folder)
    m_items.push_back(Item(parentFp, true, false));

  // register the all folder items by using the folde tree model
  DvDirModel *model        = DvDirModel::instance();
  QModelIndex currentIndex = model->getIndexByPath(m_folder);
  if (currentIndex.isValid()) {
    for (int i = 0; i < model->rowCount(currentIndex); i++) {
      QModelIndex tmpIndex = model->index(i, 0, currentIndex);
      if (tmpIndex.isValid()) {
        DvDirModelFileFolderNode *node =
            dynamic_cast<DvDirModelFileFolderNode *>(model->getNode(tmpIndex));
        if (node) {
          TFilePath childFolderPath = node->getPath();
          if (TFileStatus(childFolderPath).isLink())
            m_items.push_back(Item(childFolderPath, true, true,
                                   QString::fromStdWString(node->getName())));
          else
            m_items.push_back(Item(childFolderPath, true, false,
                                   QString::fromStdWString(node->getName())));
        }
      }
    }
  } else
    setUnregisteredFolder(m_folder);

  // register the file items
  if (m_folder != TFilePath()) {
    TFilePathSet files;
    TFilePathSet all_files;  // for updating m_multiFileItemMap

    TFileStatus fpStatus(m_folder);
    // if the item is link, then set the link target of it
    if (fpStatus.isLink()) {
      QFileInfo info(toQString(m_folder));
      setFolder(TFilePath(info.symLinkTarget().toStdWString()));
      return;
    }
    if (fpStatus.doesExist() && fpStatus.isDirectory() &&
        fpStatus.isReadable()) {
      try {
        TSystem::readDirectory(files, all_files, m_folder);
      } catch (...) {
      }
    }
    TFilePathSet::iterator it;
    for (it = files.begin(); it != files.end(); ++it) {
      // skip the plt file (Palette file for TOONZ 4.6 and earlier)
      if (it->getType() == "plt") continue;

      // filter the file
      else if (m_filter.isEmpty()) {
        if (it->getType() != "tnz" && it->getType() != "scr" &&
            it->getType() != "tnzbat" && it->getType() != "mpath" &&
            it->getType() != "curve" && it->getType() != "tpl" &&
            TFileType::getInfo(*it) == TFileType::UNKNOW_FILE)
          continue;
      } else if (!m_filter.contains(QString::fromStdString(it->getType())))
        continue;
      // store the filtered file paths
      m_items.push_back(Item(*it));
    }

    // update the m_multiFileItemMap
    m_multiFileItemMap.clear();

    for (it = all_files.begin(); it != all_files.end(); it++) {
      TFrameId tFrameId;
      try {
        tFrameId = it->getFrame();
      } catch (TMalformedFrameException tmfe) {
        // Incorrect frame name sequence. Warning to the user in the message
        // center.
        DVGui::warning(QString::fromStdWString(
            tmfe.getMessage() + L": " +
            QObject::tr("Skipping frame.").toStdWString()));
        continue;
      }

      TFilePath levelName(it->getLevelName());

      if (levelName.isLevelName()) {
        Item &levelItem = m_multiFileItemMap[levelName];

        // TODO:
        // とりあえず残すが、FileInfoの取得に時間がかかるようならオプション化も検討
        // 2015/12/28 shun_iwasawa
        QFileInfo fileInfo(QString::fromStdWString(it->getWideString()));
        // Update level infos
        if (levelItem.m_creationDate.isNull() ||
            (fileInfo.created() < levelItem.m_creationDate))
          levelItem.m_creationDate = fileInfo.created();
        if (levelItem.m_modifiedDate.isNull() ||
            (fileInfo.lastModified() > levelItem.m_modifiedDate))
          levelItem.m_modifiedDate = fileInfo.lastModified();
        levelItem.m_fileSize += fileInfo.size();

        // store frameId
        levelItem.m_frameIds.push_back(tFrameId);

        levelItem.m_frameCount++;
      }
    }
  }

  // Set Missing Version Control Items
  int missingItemCount          = 0;
  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (node) {
    QList<TFilePath> list = node->getMissingFiles();
    missingItemCount      = list.size();
    for (int i = 0; i < missingItemCount; i++)
      m_items.push_back(Item(list.at(i)));
  }

  // Refresh Data (fill Item field)
  refreshData();

  // If I added some missing items I need to sort items.
  if (missingItemCount > 0) {
    DataType currentDataType = getCurrentOrderType();
    int i;
    for (i = 1; i < m_items.size(); i++) {
      int index = i;
      while (index > 0 && compareData(currentDataType, index - 1, index) > 0) {
        std::swap(m_items[index - 1], m_items[index]);
        index = index - 1;
      }
    }
  }
  // update the ordering rules
  bool discendentOrder     = isDiscendentOrder();
  DataType currentDataType = getCurrentOrderType();
  setOrderType(Name);
  setIsDiscendentOrder(true);
  sortByDataModel(currentDataType, discendentOrder);
}

//-----------------------------------------------------------------------------

void FileBrowser::setFolder(const TFilePath &fp, bool expandNode,
                            bool forceUpdate) {
  if (fp == m_folder && !forceUpdate) return;

  // set the current folder path
  m_folder        = fp;
  m_dayDateString = "";
  // set the folder name
  if (fp == TFilePath())
    m_folderName->setText("");
  else
    m_folderName->setText(toQString(fp));

  refreshCurrentFolderItems();

  if (!TFileStatus(fp).isLink())
    m_folderTreeView->setCurrentNode(fp, expandNode);
}

//-----------------------------------------------------------------------------
/*! process when inputting the folder which is not regitered in the folder tree
   (e.g. UNC path in Windows)
 */
void FileBrowser::setUnregisteredFolder(const TFilePath &fp) {
  if (fp != TFilePath()) {
    TFileStatus fpStatus(fp);
    // if the item is link, then set the link target of it
    if (fpStatus.isLink()) {
      QFileInfo info(toQString(fp));
      setFolder(TFilePath(info.symLinkTarget().toStdWString()));
      return;
    }

    // get both the folder & file list by readDirectory and
    // readDirectory_Dir_ReadExe
    TFilePathSet folders;
    TFilePathSet files;
    // for updating m_multiFileItemMap
    TFilePathSet all_files;

    if (fpStatus.doesExist() && fpStatus.isDirectory() &&
        fpStatus.isReadable()) {
      try {
        TSystem::readDirectory(files, all_files, fp);
        TSystem::readDirectory_Dir_ReadExe(folders, fp);
      } catch (...) {
      }
    }

    TFilePathSet::iterator it;

    // register all folder items
    for (it = folders.begin(); it != folders.end(); ++it) {
      if (TFileStatus(*it).isLink())
        m_items.push_back(Item(*it, true, true));
      else
        m_items.push_back(Item(*it, true, false));
    }

    for (it = files.begin(); it != files.end(); ++it) {
      // skip the plt file (Palette file for TOONZ 4.6 and earlier)
      if (it->getType() == "plt") continue;

      // filtering
      else if (m_filter.isEmpty()) {
        if (it->getType() != "tnz" && it->getType() != "scr" &&
            it->getType() != "tnzbat" && it->getType() != "mpath" &&
            it->getType() != "curve" && it->getType() != "tpl" &&
            TFileType::getInfo(*it) == TFileType::UNKNOW_FILE)
          continue;
      } else if (!m_filter.contains(QString::fromStdString(it->getType())))
        continue;

      m_items.push_back(Item(*it));
    }

    // update the m_multiFileItemMap
    m_multiFileItemMap.clear();
    for (it = all_files.begin(); it != all_files.end(); it++) {
      TFilePath levelName(it->getLevelName());
      if (levelName.isLevelName()) {
        Item &levelItem = m_multiFileItemMap[levelName];
        levelItem.m_frameIds.push_back(it->getFrame());
        levelItem.m_frameCount++;
      }
    }
  }
  // for all items in the folder, retrieve the file names(m_name) from the
  // paths(m_path)
  refreshData();

  // update the ordering rules
  bool discendentOrder     = isDiscendentOrder();
  DataType currentDataType = getCurrentOrderType();
  setOrderType(Name);
  setIsDiscendentOrder(true);
  sortByDataModel(currentDataType, discendentOrder);

  m_itemViewer->repaint();
}

//-----------------------------------------------------------------------------

void FileBrowser::setHistoryDay(std::string dayDateString) {
  m_folder                = TFilePath();
  m_dayDateString         = dayDateString;
  const History::Day *day = History::instance()->getDay(dayDateString);
  m_items.clear();
  if (day == 0) {
    m_folderName->setText("");
  } else {
    m_folderName->setText(QString::fromStdString(dayDateString));
    std::vector<TFilePath> files;
    day->getFiles(files);
    std::vector<TFilePath>::iterator it;
    for (it = files.begin(); it != files.end(); ++it)
      m_items.push_back(Item(*it));
  }
  refreshData();
}

//-----------------------------------------------------------------------------
/*! for all items in the folder, retrieve the file names(m_name) from the
 * paths(m_path)
*/
void FileBrowser::refreshData() {
  std::vector<Item>::iterator it;
  for (it = m_items.begin(); it != m_items.end(); ++it) {
    if (it->m_name == QString(""))
      it->m_name = toQString(it->m_path.withoutParentDir());
  }
}

//-----------------------------------------------------------------------------

int FileBrowser::getItemCount() const { return m_items.size(); }

//-----------------------------------------------------------------------------

void FileBrowser::readInfo(Item &item) {
  TFilePath fp = item.m_path;
  QFileInfo info(toQString(fp));
  if (info.exists()) {
    item.m_creationDate = info.created();
    item.m_modifiedDate = info.lastModified();
    item.m_fileType     = info.suffix();
    item.m_fileSize     = info.size();
    if (fp.getType() == "tnz") {
      ToonzScene scene;
      try {
        item.m_frameCount = scene.loadFrameCount(fp);
      } catch (...) {
      }
    } else
      readFrameCount(item);

    item.m_validInfo = true;
  } else if (fp.isLevelName())  // for levels johndoe..tif etc.
  {
    try {
      // Find this level's item
      std::map<TFilePath, Item>::iterator it =
          m_multiFileItemMap.find(TFilePath(item.m_path.getLevelName()));
      if (it == m_multiFileItemMap.end()) throw "";

      item.m_creationDate = it->second.m_creationDate;
      item.m_modifiedDate = it->second.m_modifiedDate;
      item.m_fileType     = it->second.m_fileType;
      item.m_fileSize     = it->second.m_fileSize;
      item.m_frameCount   = it->second.m_frameCount;
      item.m_validInfo    = true;

      // keep the list of frameIds at the first time and try to reuse it.
      item.m_frameIds = it->second.m_frameIds;

      // The old way
      /*TLevelReaderP lr(fp);
item.m_frameCount = lr->loadInfo()->getFrameCount();
item.m_creationDate = QDateTime();
item.m_modifiedDate = QDateTime();
item.m_fileSize = -1;
item.m_validInfo = true;*/
    } catch (...) {
      item.m_frameCount = 0;
    }
  }

  item.m_validInfo = true;
}

//-----------------------------------------------------------------------------

//! Frame count needs a special access function for viewable types - for they
//! are
//! calculated by using a dedicated thread and therefore cannot be simply
//! classified as *valid* or *invalid* infos...
void FileBrowser::readFrameCount(Item &item) {
  if (TFileType::isViewable(TFileType::getInfo(item.m_path))) {
    if (isMultipleFrameType(item.m_path.getType()))
      item.m_frameCount = m_frameCountReader.getFrameCount(item.m_path);
    else
      item.m_frameCount = 1;
  } else
    item.m_frameCount = 0;
}

//-----------------------------------------------------------------------------

QVariant FileBrowser::getItemData(int index, DataType dataType,
                                  bool isSelected) {
  if (index < 0 || index >= (int)m_items.size()) return QVariant();
  Item &item = m_items[index];
  if (dataType == Name) {
    // show two dots( ".." ) for the paret directory item
    if (item.m_path == m_folder.getParentDir())
      return QString("..");
    else
      return item.m_name;
  } else if (dataType == Thumbnail) {
    QSize iconSize = m_itemViewer->getPanel()->getIconSize();
    // parent folder icons
    if (item.m_path == m_folder.getParentDir()) {
      static QPixmap folderUpPixmap(svgToPixmap(":Resources/folderup_icon.svg",
                                                iconSize, Qt::KeepAspectRatio));
      return folderUpPixmap;
    }
    // folder icons
    else if (item.m_isFolder) {
      if (item.m_isLink) {
        static QPixmap linkIcon(svgToPixmap(":Resources/link_icon.svg",
                                            iconSize, Qt::KeepAspectRatio));
        return linkIcon;
      } else {
        static QPixmap folderIcon(svgToPixmap(":Resources/folder_icon.svg",
                                              iconSize, Qt::KeepAspectRatio));
        return folderIcon;
      }
    }

    QPixmap pixmap = IconGenerator::instance()->getIcon(item.m_path);
    if (pixmap.isNull()) {
      pixmap = QPixmap(iconSize);
      pixmap.fill(Qt::white);
    }
    return scalePixmapKeepingAspectRatio(pixmap, iconSize, Qt::transparent);
  } else if (dataType == Icon)
    return QVariant();
  else if (dataType == ToolTip || dataType == FullPath)
    return QString::fromStdWString(item.m_path.getWideString());

  else if (dataType == IsFolder) {
    return item.m_isFolder;
  }

  if (!item.m_validInfo) {
    readInfo(item);
    if (!item.m_validInfo) return QVariant();
  }

  if (dataType == CreationDate) return item.m_creationDate;
  if (dataType == ModifiedDate) return item.m_modifiedDate;
  if (dataType == FileType) {
    if (item.m_isLink)
      return QString("<LINK>");
    else if (item.m_isFolder)
      return QString("<DIR>");
    else
      return QString::fromStdString(item.m_path.getType()).toUpper();
  } else if (dataType == FileSize)
    return (item.m_fileSize == -1) ? QVariant() : item.m_fileSize;
  else if (dataType == FrameCount) {
    if (item.m_frameCount == -1) readFrameCount(item);
    return item.m_frameCount;
  } else if (dataType == PlayAvailable) {
    std::string type = item.m_path.getType();
    if (item.m_frameCount > 1 && type != "tzp" && type != "tzu") return true;
    return false;
  } else if (dataType == VersionControlStatus) {
    return getItemVersionControlStatus(item);
  } else
    return QVariant();
}

//-----------------------------------------------------------------------------

bool FileBrowser::isSceneItem(int index) const {
  return 0 <= index && index < (int)m_items.size() &&
         m_items[index].m_path.getType() == "tnz";
}

//-----------------------------------------------------------------------------

bool FileBrowser::canRenameItem(int index) const {
  // se sto guardando la history non posso rinominare nulla
  if (getFolder() == TFilePath()) return false;
  if (index < 0 || index >= (int)m_items.size()) return false;
  // for now, disable rename for folders
  if (m_items[index].m_isFolder) return false;
  TFilePath fp = m_items[index].m_path;
  return TFileStatus(fp).doesExist();
}

//-----------------------------------------------------------------------------

int FileBrowser::findIndexWithPath(TFilePath path) {
  int i;
  for (i = 0; i < m_items.size(); i++)
    if (m_items[i].m_path == path) return i;
  return -1;
}

//-----------------------------------------------------------------------------

void FileBrowser::renameItem(int index, const QString &newName) {
  if (getFolder() == TFilePath()) return;
  if (index < 0 || index >= (int)m_items.size()) return;

  TFilePath fp    = m_items[index].m_path;
  TFilePath newFp = fp;
  if (renameFile(newFp, newName)) {
    m_items[index].m_name = QString::fromStdWString(newFp.getLevelNameW());
    m_items[index].m_path = newFp;

    // ho rinominato anche la palette devo aggiornarla.
    if (newFp.getType() == "tlv" || newFp.getType() == "tzp" ||
        newFp.getType() == "tzu") {
      const char *type = (newFp.getType() == "tlv") ? "tpl" : "plt";
      int paletteIndex = findIndexWithPath(fp.withNoFrame().withType(type));
      if (paletteIndex >= 0) {
        TFilePath palettePath = newFp.withNoFrame().withType(type);
        m_items[paletteIndex].m_name =
            QString::fromStdWString(palettePath.getLevelNameW());
        m_items[paletteIndex].m_path = palettePath;
      }
    }
    m_itemViewer->update();

    if (fp.getType() == "tnz") {
      // ho cambiato il folder _files. Devo aggiornare il folder che lo contiene
      // nel tree view
      QModelIndex index = m_folderTreeView->currentIndex();
      if (index.isValid()) {
        DvDirModel::instance()->refresh(index);
        // m_folderTreeView->getDvDirModel()->refresh(index);
        m_folderTreeView->update();
      }
    }
  }
}

//-----------------------------------------------------------------------------

bool FileBrowser::renameFile(TFilePath &fp, QString newName) {
  if (isSpaceString(newName)) return true;

  TFilePath newFp(newName.toStdWString());
  if (newFp.getType() != "" && newFp.getType() != fp.getType()) {
    DVGui::error(tr("Can't change file extension"));
    return false;
  }
  if (newFp.getType() == "") newFp = newFp.withType(fp.getType());
  if (newFp.getFrame() != TFrameId::EMPTY_FRAME &&
      newFp.getFrame() != TFrameId::NO_FRAME) {
    DVGui::error(tr("Can't set a drawing number"));
    return false;
  }
  if (newFp.getDots() != fp.getDots()) {
    if (fp.getDots() == ".")
      newFp = newFp.withNoFrame();
    else if (fp.getDots() == "..")
      newFp = newFp.withFrame(TFrameId::EMPTY_FRAME);
  }
  newFp = newFp.withParentDir(fp.getParentDir());

  // se sono uguali non devo rinominare nulla
  if (newFp == fp) return false;

  if (TSystem::doesExistFileOrLevel(newFp)) {
    DVGui::error(tr("Can't rename. File already exists: ") + toQString(newFp));
    return false;
  }

  try {
    TSystem::renameFileOrLevel_throw(newFp, fp, true);
    IconGenerator::instance()->remove(fp);
    if (fp.getType() == "tnz") {
      /* TFilePath folder = fp.getParentDir() + (fp.getName() + "_files");
TFilePath newFolder = newFp.getParentDir() + (newFp.getName() + "_files");
TSystem::renameFile(newFolder, folder);
*/
      TFilePath sceneIconFp    = ToonzScene::getIconPath(fp);
      TFilePath sceneIconNewFp = ToonzScene::getIconPath(newFp);
      if (TFileStatus(sceneIconFp).doesExist()) {
        if (TFileStatus(sceneIconNewFp).doesExist())
          TSystem::deleteFile(sceneIconNewFp);
        TSystem::renameFile(sceneIconNewFp, sceneIconFp);
      }
    }

  } catch (...) {
    DVGui::error(tr("Couldn't rename ") + toQString(fp) + " to " +
                 toQString(newFp));
    return false;
  }

  fp = newFp;
  return true;
}

//-----------------------------------------------------------------------------

QMenu *FileBrowser::getContextMenu(QWidget *parent, int index) {
  auto isOldLevelType = [](TFilePath &path) -> bool {
    return path.getType() == "tzp" || path.getType() == "tzu";
  };

  bool ret = true;

  // TODO: spostare in questa classe anche la definizione delle azioni?
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return 0;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);

  QMenu *menu        = new QMenu(parent);
  CommandManager *cm = CommandManager::instance();

  if (files.empty()) {
    menu->addAction(cm->getAction(MI_ShowFolderContents));
    menu->addAction(cm->getAction(MI_SelectAll));
    if (!Preferences::instance()->isWatchFileSystemEnabled()) {
      menu->addAction(cm->getAction(MI_RefreshTree));
    }
    return menu;
  }

  if (files.size() == 1 && files[0].getType() == "tnz") {
    menu->addAction(cm->getAction(MI_LoadScene));
  }
#ifdef _WIN32
  else if (files.size() == 1 && files[0].getType() == "scr") {
    QAction *action;
    action = new QAction(tr("Preview Screensaver"), menu);
    ret    = ret && connect(action, SIGNAL(triggered()), this,
                         SLOT(previewScreenSaver()));
    menu->addAction(action);
    action = new QAction(tr("Install Screensaver"), menu);
    ret    = ret && connect(action, SIGNAL(triggered()), this,
                         SLOT(installScreenSaver()));
    menu->addAction(action);
  }
#endif

  bool areResources = true;
  bool areScenes    = false;
  int i, j;
  for (i = 0; i < (int)files.size(); i++) {
    TFileType::Type type = TFileType::getInfo(files[i]);
    if (areResources && !TFileType::isResource(type)) areResources = false;
    if (!areScenes && TFileType::isScene(type)) areScenes          = true;
  }

  bool areFullcolor = true;
  for (i = 0; i < (int)files.size(); i++) {
    TFileType::Type type = TFileType::getInfo(files[i]);
    if (!TFileType::isFullColor(type)) {
      areFullcolor = false;
      break;
    }
  }

  TFilePath clickedFile;
  if (0 <= index && index < (int)m_items.size())
    clickedFile = m_items[index].m_path;

  if (areResources) {
    QString title;
    if (clickedFile != TFilePath() && clickedFile.getType() == "tnz")
      title = tr("Load As Sub-xsheet");
    else
      title         = tr("Load");
    QAction *action = new QAction(title, menu);
    ret             = ret &&
          connect(action, SIGNAL(triggered()), this, SLOT(loadResources()));
    menu->addAction(action);
    menu->addSeparator();
  }

  menu->addAction(cm->getAction(MI_DuplicateFile));
  if (!areScenes) {
    menu->addAction(cm->getAction(MI_Copy));
    menu->addAction(cm->getAction(MI_Paste));
  }
  menu->addAction(cm->getAction(MI_Clear));
  menu->addAction(cm->getAction(MI_ShowFolderContents));
  menu->addAction(cm->getAction(MI_SelectAll));
  menu->addAction(cm->getAction(MI_FileInfo));
  if (!clickedFile.isEmpty() &&
      (clickedFile.getType() == "tnz" || clickedFile.getType() == "tab")) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_AddToBatchRenderList));
    menu->addAction(cm->getAction(MI_AddToBatchCleanupList));
  }

  for (i = 0; i < files.size(); i++)
    if (!TFileType::isViewable(TFileType::getInfo(files[i]))) break;
  if (i == files.size()) {
    std::string type = files[0].getType();
    for (j = 0; j < files.size(); j++)
      if (isOldLevelType(files[j])) break;
    if (j == files.size()) menu->addAction(cm->getAction(MI_ViewFile));

    for (j = 0; j < files.size(); j++) {
      if ((files[0].getType() == "pli" && files[j].getType() != "pli") ||
          (files[0].getType() != "pli" && files[j].getType() == "pli"))
        break;
      else if ((isOldLevelType(files[0]) && !isOldLevelType(files[j])) ||
               (!isOldLevelType(files[0]) && isOldLevelType(files[j])))
        break;
    }
    if (j == files.size()) {
      menu->addAction(cm->getAction(MI_ConvertFiles));
      // iwsw commented out temporarily
      // menu->addAction(cm->getAction(MI_ToonShadedImageToTLV));
    }
    if (!areFullcolor) menu->addSeparator();
  }
  if (files.size() == 1 && files[0].getType() != "tnz") {
    QAction *action = new QAction(tr("Rename"), menu);
    ret             = ret && connect(action, SIGNAL(triggered()), this,
                         SLOT(renameAsToonzLevel()));
    menu->addAction(action);
  }
#ifdef LEVO

  if (files.size() == 2 &&
      (files[0].getType() == "tif" || files[0].getType() == "tiff" ||
       files[0].getType() == "png" || files[0].getType() == "TIF" ||
       files[0].getType() == "TIFF" || files[0].getType() == "PNG") &&
      (files[1].getType() == "tif" || files[1].getType() == "tiff" ||
       files[1].getType() == "png" || files[1].getType() == "TIF" ||
       files[1].getType() == "TIFF" || files[1].getType() == "PNG")) {
    QAction *action = new QAction(tr("Convert to Painted TLV"), menu);
    ret             = ret && connect(action, SIGNAL(triggered()), this,
                         SLOT(convertToPaintedTlv()));
    menu->addAction(action);
  }
  if (areFullcolor) {
    QAction *action = new QAction(tr("Convert to Unpainted TLV"), menu);
    ret             = ret && connect(action, SIGNAL(triggered()), this,
                         SLOT(convertToUnpaintedTlv()));
    menu->addAction(action);
    menu->addSeparator();
  }
#endif

  if (!clickedFile.isEmpty() && (clickedFile.getType() == "tnz")) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_CollectAssets));
    menu->addAction(cm->getAction(MI_ImportScenes));
    menu->addAction(cm->getAction(MI_ExportScenes));
  }

  DvDirVersionControlNode *node = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (node) {
    // Check Version Control Status
    DvItemListModel::Status status =
        (DvItemListModel::Status)m_itemViewer->getModel()
            ->getItemData(index, DvItemListModel::VersionControlStatus)
            .toInt();

    // Remove the added actions
    if (status == DvItemListModel::VC_Missing) menu->clear();

    QMenu *vcMenu = new QMenu(tr("Version Control"), parent);
    QAction *action;

    if (status == DvItemListModel::VC_ReadOnly ||
        (status == DvItemListModel::VC_ToUpdate && files.size() == 1)) {
      if (status == DvItemListModel::VC_ReadOnly) {
        action = vcMenu->addAction(tr("Edit"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(editVersionControl()));

        TFilePath path       = files.at(0);
        std::string fileType = path.getType();
        if (fileType == "tlv" || fileType == "pli" || path.getDots() == "..") {
          action = vcMenu->addAction(tr("Edit Frame Range..."));
          ret    = ret && connect(action, SIGNAL(triggered()), this,
                               SLOT(editFrameRangeVersionControl()));
        }
      } else {
        action = vcMenu->addAction(tr("Edit"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(updateAndEditVersionControl()));
      }
    }

    if (status == DvItemListModel::VC_Modified) {
      action = vcMenu->addAction(tr("Put..."));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(putVersionControl()));

      action = vcMenu->addAction(tr("Revert"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(revertVersionControl()));
    }

    if (status == DvItemListModel::VC_ReadOnly ||
        status == DvItemListModel::VC_ToUpdate) {
      action = vcMenu->addAction(tr("Get"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(getVersionControl()));

      if (status == DvItemListModel::VC_ReadOnly) {
        action = vcMenu->addAction(tr("Delete"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(deleteVersionControl()));
      }

      vcMenu->addSeparator();

      if (files.size() == 1) {
        action         = vcMenu->addAction(tr("Get Revision..."));
        TFilePath path = files.at(0);
        if (path.getDots() == "..")
          ret = ret && connect(action, SIGNAL(triggered()), this,
                               SLOT(getRevisionVersionControl()));
        else
          ret = ret && connect(action, SIGNAL(triggered()), this,
                               SLOT(getRevisionHistory()));
      } else if (files.size() > 1) {
        action = vcMenu->addAction("Get Revision...");
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(getRevisionVersionControl()));
      }
    }

    if (status == DvItemListModel::VC_Edited) {
      action = vcMenu->addAction(tr("Unlock"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(unlockVersionControl()));
    }

    if (status == DvItemListModel::VC_Unversioned) {
      action = vcMenu->addAction(tr("Put..."));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(putVersionControl()));
    }

    if (status == DvItemListModel::VC_Locked && files.size() == 1) {
      action = vcMenu->addAction(tr("Edit Info"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(showLockInformation()));
    }

    if (status == DvItemListModel::VC_Missing) {
      action = vcMenu->addAction(tr("Get"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(getVersionControl()));

      if (files.size() == 1) {
        vcMenu->addSeparator();
        action         = vcMenu->addAction(tr("Revision History..."));
        TFilePath path = files.at(0);
        if (path.getDots() == "..")
          ret = ret && connect(action, SIGNAL(triggered()), this,
                               SLOT(getRevisionVersionControl()));
        else
          ret = ret && connect(action, SIGNAL(triggered()), this,
                               SLOT(getRevisionHistory()));
      }
    }

    if (status == DvItemListModel::VC_PartialLocked) {
      action = vcMenu->addAction(tr("Get"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(getVersionControl()));
      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Edit Frame Range..."));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(editFrameRangeVersionControl()));

        action = vcMenu->addAction(tr("Edit Info"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(showFrameRangeLockInfo()));
      }

    } else if (status == DvItemListModel::VC_PartialEdited) {
      action = vcMenu->addAction(tr("Get"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(getVersionControl()));

      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Unlock Frame Range"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(unlockFrameRangeVersionControl()));

        action = vcMenu->addAction(tr("Edit Info"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(showFrameRangeLockInfo()));
      }
    } else if (status == DvItemListModel::VC_PartialModified) {
      action = vcMenu->addAction(tr("Get"));
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                           SLOT(getVersionControl()));

      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Put..."));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(putFrameRangeVersionControl()));

        action = vcMenu->addAction(tr("Revert"));
        ret    = ret && connect(action, SIGNAL(triggered()), this,
                             SLOT(revertFrameRangeVersionControl()));
      }
    }

    if (!vcMenu->isEmpty()) {
      menu->addSeparator();
      menu->addMenu(vcMenu);
    }
  }

  if (!Preferences::instance()->isWatchFileSystemEnabled()) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_RefreshTree));
  }

  assert(ret);

  return menu;
}

//-----------------------------------------------------------------------------

void FileBrowser::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);
  if (files.empty()) return;

  QList<QUrl> urls;
  for (int i = 0; i < (int)files.size(); i++) {
    if (TSystem::doesExistFileOrLevel(files[i]))
      urls.append(QUrl::fromLocalFile(
          QString::fromStdWString(files[i].getWideString())));
  }
  if (urls.isEmpty()) return;

  QMimeData *mimeData = new QMimeData;
  mimeData->setUrls(urls);
  QDrag *drag    = new QDrag(this);
  QSize iconSize = m_itemViewer->getPanel()->getIconSize();
  QPixmap icon   = IconGenerator::instance()->getIcon(files[0]);
  QPixmap dropThumbnail =
      scalePixmapKeepingAspectRatio(icon, iconSize, Qt::transparent);
  if (!dropThumbnail.isNull()) drag->setPixmap(dropThumbnail);
  drag->setMimeData(mimeData);
  Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
}

//-----------------------------------------------------------------------------

bool FileBrowser::dropMimeData(QTreeWidgetItem *parent, int index,
                               const QMimeData *data, Qt::DropAction action) {
  return false;
}

//-----------------------------------------------------------------------------

void FileBrowser::onTreeFolderChanged() {
  DvDirModelNode *node = m_folderTreeView->getCurrentNode();
  if (node)
    node->visualizeContent(this);
  else
    setFolder(TFilePath());
  m_itemViewer->resetVerticalScrollBar();
  m_itemViewer->updateContentSize();
  m_itemViewer->getPanel()->update();
  m_frameCountReader.stopReading();
  IconGenerator::instance()->clearRequests();

  DvDirModelFileFolderNode *fileFolderNode =
      dynamic_cast<DvDirModelFileFolderNode *>(node);
  if (fileFolderNode) emit treeFolderChanged(fileFolderNode->getPath());
}

//-----------------------------------------------------------------------------

void FileBrowser::changeFolder(const QModelIndex &index) {}

//-----------------------------------------------------------------------------

void FileBrowser::onDataChanged(const QModelIndex &topLeft,
                                const QModelIndex &bottomRight) {
  onTreeFolderChanged();
}

//-----------------------------------------------------------------------------

bool FileBrowser::acceptDrop(const QMimeData *data) const {
  // se il browser non sta visualizzando un folder standard non posso accettare
  // nessun drop
  if (getFolder() == TFilePath()) return false;

  if (data->hasFormat("application/vnd.toonz.levels") ||
      data->hasFormat("application/vnd.toonz.currentscene") ||
      data->hasFormat("application/vnd.toonz.drawings") ||
      acceptResourceDrop(data->urls()))
    return true;

  return false;
}

//-----------------------------------------------------------------------------

bool FileBrowser::drop(const QMimeData *mimeData) {
  // se il browser non sta visualizzando un folder standard non posso accettare
  // nessun drop
  TFilePath folderPath = getFolder();
  if (folderPath == TFilePath()) return false;

  if (mimeData->hasFormat(CastItems::getMimeFormat())) {
    const CastItems *items = dynamic_cast<const CastItems *>(mimeData);
    if (!items) return false;

    int i;
    for (i = 0; i < items->getItemCount(); i++) {
      CastItem *item = items->getItem(i);
      if (TXshSimpleLevel *sl = item->getSimpleLevel()) {
        TFilePath levelPath = sl->getPath().withParentDir(getFolder());
        IoCmd::saveLevel(levelPath, sl, false);
      } else if (TXshSoundLevel *level = item->getSoundLevel()) {
        TFilePath soundPath = level->getPath().withParentDir(getFolder());
        IoCmd::saveSound(soundPath, level, false);
      }
    }
    refreshFolder(getFolder());
    return true;
  } else if (mimeData->hasFormat("application/vnd.toonz.currentscene")) {
    TFilePath scenePath;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene->isUntitled()) {
      bool ok;
      QString sceneName =
          QInputDialog::getText(this, tr("Save Scene"), tr("Scene name:"),
                                QLineEdit::Normal, QString(), &ok);
      if (!ok || sceneName == "") return false;
      scenePath = folderPath + sceneName.toStdWString();
    } else
      scenePath = folderPath + scene->getSceneName();
    return IoCmd::saveScene(scenePath, false);
  } else if (mimeData->hasFormat("application/vnd.toonz.drawings")) {
    TFilmstripSelection *s =
        dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
    if (!s) return false;
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl) return false;

    std::wstring levelName = sl->getName();
    folderPath +=
        TFilePath(levelName + ::to_wstring(sl->getPath().getDottedType()));
    if (TSystem::doesExistFileOrLevel(folderPath)) {
      QString question = "Level " + toQString(folderPath) +
                         " already exists\nDo you want to duplicate it?";
      int ret = DVGui::MsgBox(question, QObject::tr("Duplicate"),
                              QObject::tr("Don't Duplicate"), 0);
      if (ret == 2 || ret == 0) return false;
      TFilePath path = folderPath;
      NameBuilder *nameBuilder =
          NameBuilder::getBuilder(::to_wstring(path.getName()));
      do
        levelName = nameBuilder->getNext();
      while (TSystem::doesExistFileOrLevel(path.withName(levelName)));
      folderPath = path.withName(levelName);
    }
    assert(!TSystem::doesExistFileOrLevel(folderPath));

    TXshSimpleLevel *newSl = new TXshSimpleLevel();
    newSl->setType(sl->getType());
    newSl->clonePropertiesFrom(sl);
    newSl->setName(levelName);
    newSl->setPalette(sl->getPalette());
    newSl->setScene(sl->getScene());
    std::set<TFrameId> frames = s->getSelectedFids();
    for (auto const &fid : frames) {
      newSl->setFrame(fid, sl->getFrame(fid, false));
    }

    IoCmd::saveLevel(folderPath, newSl, false);
    refreshFolder(folderPath.getParentDir());
    return true;
  } else if (mimeData->hasUrls()) {
    int count = 0;
    foreach (QUrl url, mimeData->urls()) {
      TFilePath srcFp(url.toLocalFile().toStdWString());
      TFilePath dstFp = srcFp.withParentDir(folderPath);
      if (dstFp != srcFp) {
        if (!TSystem::copyFileOrLevel(dstFp, srcFp))
          DVGui::error(tr("There was an error copying %1 to %2")
                           .arg(toQString(srcFp))
                           .arg(toQString(dstFp)));
      }
    }
    refreshFolder(folderPath);
    return true;
  } else
    return false;
}

//-----------------------------------------------------------------------------

void FileBrowser::loadResources() {
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;

  std::vector<TFilePath> filePaths;
  fs->getSelectedFiles(filePaths);

  if (filePaths.empty()) return;

  IoCmd::LoadResourceArguments args;
  args.resourceDatas.assign(filePaths.begin(), filePaths.end());

  IoCmd::loadResources(args);
}

//-----------------------------------------------------------------------------

void RenameAsToonzPopup::onOk() {
  if (!isValidFileName(m_name->text())) {
    DVGui::error(
        tr("The file name cannot be empty or contain any of the following "
           "characters:(new line)  \\ / : * ? \"  |"));
    return;
  }
  accept();
}

RenameAsToonzPopup::RenameAsToonzPopup(const QString name, int frames)
    : Dialog(TApp::instance()->getMainWindow(), true, true, "RenameAsToonz") {
  setWindowTitle(QString(tr("Rename")));

  beginHLayout();

  QLabel *lbl;
  if (frames == -1)
    lbl = new QLabel(QString(tr("Renaming File ")) + name);
  else
    lbl = new QLabel(
        QString(tr("Creating an animation level of %1 frames").arg(frames)));
  lbl->setFixedHeight(20);
  lbl->setObjectName("TitleTxtLabel");

  m_name = new LineEdit(frames == -1 ? "" : name);
  m_name->setFixedHeight(20);
  // connect(m_name, SIGNAL(editingFinished()), SLOT(onNameChanged()));
  // addWidget(tr("Level Name:"),m_name);

  m_overwrite = new QCheckBox(tr("Delete Original Files"));
  m_overwrite->setFixedHeight(20);
  // addWidget(m_overwrite, false);

  QFormLayout *formLayout = new QFormLayout;

  QHBoxLayout *labelLayout = new QHBoxLayout;
  labelLayout->addStretch();
  labelLayout->addWidget(lbl);
  labelLayout->addStretch();

  formLayout->addRow(labelLayout);
  formLayout->addRow(tr("Level Name:"), m_name);
  formLayout->addRow(m_overwrite);

  addLayout(formLayout);

  endHLayout();

  m_okBtn = new QPushButton(tr("Rename"), this);
  m_okBtn->setDefault(true);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(m_okBtn, SIGNAL(clicked()), this, SLOT(onOk()));
  connect(m_cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  addButtonBarWidget(m_okBtn, m_cancelBtn);
}

namespace {

bool parsePathName(const QString &fullpath, QString &parentPath, QString &name,
                   QString &format) {
  int index              = fullpath.lastIndexOf('\\');
  if (index == -1) index = fullpath.lastIndexOf('/');

  QString filename;

  if (index != -1) {
    parentPath = fullpath.left(index + 1);
    filename   = fullpath.right(fullpath.size() - index - 1);
  } else {
    parentPath = "";
    filename   = fullpath;
  }

  index = filename.lastIndexOf('.');

  if (index <= 0) return false;

  format = filename.right(filename.size() - index - 1);
  if (format == "") return false;

  index--;
  if (!filename.at(index).isDigit()) return false;

  while (index >= 0 && filename.at(index).isDigit()) index--;

  if (index < 0) return false;

  name = filename.left(index + 1);

  return true;
}

//---------------------------------------------------------

void getLevelFiles(const QString &parentPath, const QString &name,
                   const QString &format, QStringList &pathIn) {
  QString dummy, dummy1, filter = "*." + format;
  QDir dir(parentPath, filter);
  QStringList list = dir.entryList();

  for (int i = 0; i < list.size(); i++) {
    QString item = list.at(i);
    QString itemName;
    if (!parsePathName(item, dummy, itemName, dummy1) || name != itemName)
      continue;

    pathIn.push_back(item);
  }
}

//---------------------------------------------------------

QString getFrame(const QString &filename) {
  int index = filename.lastIndexOf('.');

  if (index <= 0) return "";

  index--;
  if (!filename.at(index).isDigit()) return "";

  int to, from;
  to = from = index;
  while (from >= 0 && filename.at(from).isDigit()) from--;

  if (from < 0) return "";

  char padStr[5];
  padStr[4] = '\0';

  int i, frame = 0;

  QString number = filename.mid(from + 1, to - from);
  for (i = 0; i < 4 - number.size(); i++) padStr[i] = '0';
  for (i = 0; i < number.size(); i++)
#if QT_VERSION >= 0x050500
    padStr[4 - number.size() + i] = number.at(i).toLatin1();
#else
    padStr[4 - number.size() + i] = number.at(i).toAscii();
#endif
  return QString(padStr);
}

//------------------------------------------------------------------

//-----------------------------------------------------------

void renameSingleFileOrToonzLevel(const QString &fullpath) {
  TFilePath fpin(fullpath.toStdString());

  RenameAsToonzPopup popup(
      QString::fromStdWString(fpin.withoutParentDir().getWideString()));
  if (popup.exec() != QDialog::Accepted) return;

  std::string name = popup.getName().toStdString();

  if (name == fpin.getName()) {
    DVGui::error(QString(
        QObject::tr("The specified name is already assigned to the %1 file.")
            .arg(fullpath)));
    return;
  }

  if (popup.doOverwrite())
    TSystem::renameFileOrLevel(fpin.withName(name), fpin, true);
  else
    TSystem::copyFileOrLevel(fpin.withName(name), fpin);
}

//----------------------------------------------------------

void doRenameAsToonzLevel(const QString &fullpath) {
  QString parentPath, name, format;

  if (!parsePathName(fullpath, parentPath, name, format)) {
    renameSingleFileOrToonzLevel(fullpath);
    return;
  }

  QStringList pathIn;

  getLevelFiles(parentPath, name, format, pathIn);

  if (pathIn.empty()) return;

  while (name.endsWith('_') || name.endsWith('.') || name.endsWith(' '))
    name.chop(1);

  RenameAsToonzPopup popup(name, pathIn.size());
  if (popup.exec() != QDialog::Accepted) return;

  name = popup.getName();

  QString levelOutStr = parentPath + "/" + name + ".." + format;

  TFilePath levelOut(levelOutStr.toStdWString());
  if (TSystem::doesExistFileOrLevel(levelOut)) {
    QApplication::restoreOverrideCursor();
    int ret = DVGui::MsgBox(
        QObject::tr("Warning: level %1 already exists; overwrite?")
            .arg(toQString(levelOut)),
        QObject::tr("Yes"), QObject::tr("No"), 1);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (ret == 2 || ret == 0) return;
    TSystem::removeFileOrLevel(levelOut);
  }

  int i;
  for (i = 0; i < pathIn.size(); i++) {
    QString padStr = getFrame(pathIn[i]);
    if (padStr == "") continue;
    QString pathOut = parentPath + "/" + name + "." + padStr + "." + format;

    if (popup.doOverwrite()) {
      if (!QFile::rename(parentPath + "/" + pathIn[i], pathOut)) {
        QString tmp(parentPath + "/" + pathIn[i]);
        DVGui::error(QString(
            QObject::tr("It is not possible to rename the %1 file.").arg(tmp)));
        return;
      }
    } else if (!QFile::copy(parentPath + "/" + pathIn[i], pathOut)) {
      QString tmp(parentPath + "/" + pathIn[i]);
      DVGui::error(QString(
          QObject::tr("It is not possible to copy the %1 file.").arg(tmp)));

      return;
    }
  }
}

}  // namespace

//-------------------------------------------------------------------------------

void FileBrowser::renameAsToonzLevel() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.size() != 1) return;

  doRenameAsToonzLevel(QString::fromStdWString(filePaths[0].getWideString()));

  QApplication::restoreOverrideCursor();

  FileBrowser::refreshFolder(filePaths[0].getParentDir());
}

#ifdef LEVO

void FileBrowser::convertToUnpaintedTlv() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);

  QStringList sl;
  sl << "Apply Autoclose                        "
     << "Don't Apply Autoclose                          ";
  bool ok;
  QString autoclose = QInputDialog::getItem(
      this, tr("Convert To Unpainted Tlv"), "", sl, 0, false, &ok);
  if (!ok) return;

  QApplication::setOverrideCursor(Qt::WaitCursor);

  int i, totFrames = 0;
  std::vector<Convert2Tlv *> converters;
  for (i = 0; i < filePaths.size(); i++) {
    Convert2Tlv *converter =
        new Convert2Tlv(filePaths[i], TFilePath(), TFilePath(), -1, -1,
                        autoclose == sl.at(0), TFilePath(), 0, 0, 0);

    if (TSystem::doesExistFileOrLevel(converter->m_levelOut)) {
      QApplication::restoreOverrideCursor();
      int ret = DVGui::MsgBox(tr("Warning: level %1 already exists; overwrite?")
                                  .arg(toQString(converter->m_levelOut)),
                              tr("Yes"), tr("No"), 1);
      QApplication::setOverrideCursor(Qt::WaitCursor);
      if (ret == 2 || ret == 0) {
        delete converter;
        continue;
      }
      TSystem::removeFileOrLevel(converter->m_levelOut);
    }

    totFrames += converter->getFramesToConvertCount();
    converters.push_back(converter);
  }

  if (converters.empty()) {
    QApplication::restoreOverrideCursor();
    return;
  }

  ProgressDialog pb("", "Cancel", 0, totFrames);
  int j, l, k = 0;
  for (i = 0; i < converters.size(); i++) {
    std::string errorMessage;
    if (!converters[i]->init(errorMessage)) {
      converters[i]->abort();
      DVGui::error(QString::fromStdString(errorMessage));
      delete converters[i];
      converters[i] = 0;
      continue;
    }

    int count = converters[i]->getFramesToConvertCount();

    pb.setLabelText("Generating level " + toQString(converters[i]->m_levelOut));
    pb.show();

    for (j = 0; j < count; j++) {
      std::string errorMessage = "";
      if (!converters[i]->convertNext(errorMessage) || pb.wasCanceled()) {
        for (l = i; l < converters.size(); l++) {
          converters[l]->abort();
          delete converters[i];
          converters[i] = 0;
        }
        if (errorMessage != "")
          DVGui::error(QString::fromStdString(errorMessage));
        QApplication::restoreOverrideCursor();
        FileBrowser::refreshFolder(filePaths[0].getParentDir());
        return;
      }
      pb.setValue(++k);
    }
    TFilePath levelOut(converters[i]->m_levelOut);
    delete converters[i];
    IconGenerator::instance()->invalidate(levelOut);

    converters[i] = 0;
  }

  QApplication::restoreOverrideCursor();
  pb.hide();
  DVGui::info(tr("Done: All Levels  converted to TLV Format"));

  FileBrowser::refreshFolder(filePaths[0].getParentDir());
}

//-----------------------------------------------------------------------------

void FileBrowser::convertToPaintedTlv() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);

  if (filePaths.size() != 2) return;

  QStringList sl;
  sl << "Apply Autoclose                      "
     << "Don't Apply Autoclose                        ";
  bool ok;
  QString autoclose = QInputDialog::getItem(this, tr("Convert To Painted Tlv"),
                                            "", sl, 0, false, &ok);
  if (!ok) return;

  QApplication::setOverrideCursor(Qt::WaitCursor);

  Convert2Tlv *converter =
      new Convert2Tlv(filePaths[0], filePaths[1], TFilePath(), -1, -1,
                      autoclose == sl.at(0), TFilePath(), 0, 0, 0);

  if (TSystem::doesExistFileOrLevel(converter->m_levelOut)) {
    QApplication::restoreOverrideCursor();
    int ret = DVGui::MsgBox(tr("Warning: level %1 already exists; overwrite?")
                                .arg(toQString(converter->m_levelOut)),
                            tr("Yes"), tr("No"), 1);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (ret == 2 || ret == 0) {
      QApplication::restoreOverrideCursor();
      return;
    }
    TSystem::removeFileOrLevel(converter->m_levelOut);
  }

  std::string errorMessage;
  if (!converter->init(errorMessage)) {
    converter->abort();
    delete converter;
    DVGui::error(QString::fromStdString(errorMessage));
    QApplication::restoreOverrideCursor();
    return;
  }
  int count = converter->getFramesToConvertCount();

  ProgressDialog pb("Generating level " + toQString(converter->m_levelOut),
                    "Cancel", 0, count);
  pb.show();

  for (int i = 0; i < count; i++) {
    errorMessage = "";
    if (!converter->convertNext(errorMessage) || pb.wasCanceled()) {
      converter->abort();
      delete converter;
      if (errorMessage != "")
        DVGui::error(QString::fromStdString(errorMessage));
      QApplication::restoreOverrideCursor();
      FileBrowser::refreshFolder(filePaths[0].getParentDir());
      return;
    }

    pb.setValue(i + 1);
  }

  TFilePath levelOut(converter->m_levelOut);
  delete converter;
  IconGenerator::instance()->invalidate(levelOut);

  QApplication::restoreOverrideCursor();
  pb.hide();
  DVGui::info(tr("Done: 2 Levels  converted to TLV Format"));

  fs->selectNone();
  FileBrowser::refreshFolder(filePaths[0].getParentDir());
}
#endif

//-----------------------------------------------------------------------------

void FileBrowser::onSelectedItems(const std::set<int> &indexes) {
  if (indexes.empty()) return;

  std::set<TFilePath> filePaths;
  std::set<int>::const_iterator it;

  // pass the frameId list for reuse
  std::list<std::vector<TFrameId>> frameIDs;

  for (it = indexes.begin(); it != indexes.end(); ++it) {
    filePaths.insert(m_items[*it].m_path);
    frameIDs.insert(frameIDs.begin(), m_items[*it].m_frameIds);
  }

  // reuse the list of TFrameId in order to skip loadInfo() when loading the
  // level with sequencial frames.
  emit filePathsSelected(filePaths, frameIDs);
}

//-----------------------------------------------------------------------------

void FileBrowser::onClickedItem(int index) {
  if (0 <= index && index < (int)m_items.size()) {
    // if the folder is clicked, then move the current folder
    TFilePath fp = m_items[index].m_path;
    if (m_items[index].m_isFolder) {
      setFolder(fp, true);
      QModelIndex index = m_folderTreeView->currentIndex();
      if (index.isValid()) m_folderTreeView->scrollTo(index);
    } else
      emit filePathClicked(fp);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshFolder(const TFilePath &folderPath) {
  std::set<FileBrowser *>::iterator it;
  for (it = activeBrowsers.begin(); it != activeBrowsers.end(); ++it) {
    FileBrowser *browser = *it;
    DvDirModel::instance()->refreshFolder(folderPath);
    if (browser->getFolder() == folderPath) {
      browser->setFolder(folderPath, false, true);
    }
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::getExpandedFolders(DvDirModelNode *node,
                                     QList<DvDirModelNode *> &expandedNodes) {
  if (!node) return;
  QModelIndex newIndex = DvDirModel::instance()->getIndexByNode(node);
  if (!m_folderTreeView->isExpanded(newIndex)) return;
  expandedNodes.push_back(node);

  int i = 0;
  for (i = 0; i < node->getChildCount(); i++)
    getExpandedFolders(node->getChild(i), expandedNodes);
}

//-----------------------------------------------------------------------------

void FileBrowser::refresh() {
  TFilePath originalFolder(
      m_folder);  // setFolder is invoked by Qt throughout the following...

  int dx                   = m_folderTreeView->verticalScrollBar()->value();
  DvDirModelNode *rootNode = DvDirModel::instance()->getNode(QModelIndex());

  QModelIndex index = DvDirModel::instance()->getIndexByNode(rootNode);

  bool vcEnabled = m_folderTreeView->refreshVersionControlEnabled();

  m_folderTreeView->setRefreshVersionControlEnabled(false);
  DvDirModel::instance()->refreshFolderChild(index);
  m_folderTreeView->setRefreshVersionControlEnabled(vcEnabled);

  QList<DvDirModelNode *> expandedNodes;
  int i;
  for (i = 0; i < rootNode->getChildCount(); i++)
    getExpandedFolders(rootNode->getChild(i), expandedNodes);

  for (i = 0; i < expandedNodes.size(); i++) {
    DvDirModelNode *node = expandedNodes[i];
    if (!node || !node->hasChildren()) continue;
    QModelIndex ind = DvDirModel::instance()->getIndexByNode(node);
    if (!ind.isValid()) continue;
    m_folderTreeView->expand(ind);
  }
  m_folderTreeView->verticalScrollBar()->setValue(dx);

  setFolder(originalFolder, false, true);
}

//-----------------------------------------------------------------------------

void FileBrowser::folderUp() {
  QModelIndex index = m_folderTreeView->currentIndex();
  if (!index.isValid() || !index.parent().isValid()) return;
  m_folderTreeView->setCurrentIndex(index.parent());
  m_folderTreeView->scrollTo(index.parent());
}

//-----------------------------------------------------------------------------

void FileBrowser::newFolder() {
  TFilePath parentFolder = getFolder();
  if (parentFolder == TFilePath() || !TFileStatus(parentFolder).isDirectory())
    return;
  QString tempName(tr("New Folder"));
  std::wstring folderName = tempName.toStdWString();
  TFilePath folderPath    = parentFolder + folderName;
  int i                   = 1;
  while (TFileStatus(folderPath).doesExist())
    folderPath = parentFolder + (folderName + L" " + std::to_wstring(++i));

  try {
    TSystem::mkDir(folderPath);

  } catch (...) {
    DVGui::error(tr("It is not possible to create the %1 folder.")
                     .arg(toQString(folderPath)));
    return;
  }

  DvDirModel *model = DvDirModel::instance();

  QModelIndex parentFolderIndex = m_folderTreeView->currentIndex();
  model->refresh(parentFolderIndex);
  m_folderTreeView->expand(parentFolderIndex);

  std::wstring newFolderName = folderPath.getWideName();
  QModelIndex newFolderIndex =
      model->childByName(parentFolderIndex, newFolderName);
  if (newFolderIndex.isValid()) {
    m_folderTreeView->setCurrentIndex(newFolderIndex);
    m_folderTreeView->scrollTo(newFolderIndex);
    m_folderTreeView->QAbstractItemView::edit(newFolderIndex);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::previewScreenSaver() {
  std::vector<TFilePath> files;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(files);
  if (files.size() != 1 || files[0].getType() != "scr") return;

  QDesktopServices::openUrl(QUrl(toQString(files[0])));
}

//-----------------------------------------------------------------------------

void FileBrowser::installScreenSaver() {
  std::vector<TFilePath> files;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(files);
  if (files.size() != 1 || files[0].getType() != "scr") return;
  ::installScreenSaver(files[0]);
}

//-----------------------------------------------------------------------------

void FileBrowser::showEvent(QShowEvent *) {
  activeBrowsers.insert(this);
  // refresh
  if (getFolder() != TFilePath())
    setFolder(getFolder(), false, true);
  else if (getDayDateString() != "")
    setHistoryDay(getDayDateString());
  m_folderTreeView->scrollTo(m_folderTreeView->currentIndex());

  // Refresh SVN
  DvDirVersionControlNode *vcNode = dynamic_cast<DvDirVersionControlNode *>(
      m_folderTreeView->getCurrentNode());
  if (vcNode) m_folderTreeView->refreshVersionControl(vcNode);
}

//-----------------------------------------------------------------------------

void FileBrowser::hideEvent(QHideEvent *) {
  activeBrowsers.erase(this);
  m_itemViewer->getPanel()->getItemViewPlayDelegate()->resetPlayWidget();
}

//-----------------------------------------------------------------------------

void FileBrowser::makeCurrentProjectVisible() {}

//-----------------------------------------------------------------------------

void FileBrowser::enableGlobalSelection(bool enabled) {
  m_folderTreeView->enableGlobalSelection(enabled);
  m_itemViewer->enableGlobalSelection(enabled);
}

//-----------------------------------------------------------------------------

void FileBrowser::selectNone() { m_itemViewer->selectNone(); }

//=============================================================================
// FCData methods
//-----------------------------------------------------------------------------

FCData::FCData(const QDateTime &date)
    : m_date(date), m_frameCount(0), m_underProgress(true), m_retryCount(1) {}

//=============================================================================
// FrameCountReader methods
//-----------------------------------------------------------------------------

FrameCountReader::FrameCountReader() : m_executor() {
  m_executor.setMaxActiveTasks(2);
}

//-----------------------------------------------------------------------------

FrameCountReader::~FrameCountReader() {}

//-----------------------------------------------------------------------------

int FrameCountReader::getFrameCount(const TFilePath &fp) {
  QDateTime modifiedDate =
      QFileInfo(QString::fromStdWString(fp.getWideString())).lastModified();
  std::map<TFilePath, FCData>::iterator it;

  {
    // Access the static map to find an occurrence of the path.
    QMutexLocker locker(&frameCountMapMutex);
    it = frameCountMap.find(fp);

    if (it != frameCountMap.end()) {
      if (it->second.m_frameCount > 0 && it->second.m_date == modifiedDate) {
        // Found an unmodified occurrence with correctly calculated frame count
        return it->second.m_frameCount;
      }
    } else {
      // First time this frame count is calculated - initialize FC data
      frameCountMap[fp] = FCData(modifiedDate);
      goto calculateTask;
    }

    if ((modifiedDate == it->second.m_date) &&
        (it->second.m_underProgress || it->second.m_retryCount < 0))
      return -1;
  }

calculateTask:

  // Now, we have to calculate the frame count; first, create a frame count
  // calculation task and submit it.
  FrameCountTask *task = new FrameCountTask(fp, modifiedDate);
  connect(task, SIGNAL(finished(TThread::RunnableP)), this,
          SIGNAL(calculatedFrameCount()));
  connect(task, SIGNAL(exception(TThread::RunnableP)), this,
          SIGNAL(calculatedFrameCount()));

  m_executor.addTask(task);

  return -1;  // FrameCount has not yet been calculated
}

//-----------------------------------------------------------------------------

inline void FrameCountReader::stopReading() { m_executor.cancelAll(); }

//=============================================================================
// FrameCountTask methods
//-----------------------------------------------------------------------------

FrameCountTask::FrameCountTask(const TFilePath &path,
                               const QDateTime &modifiedDate)
    : m_path(path), m_modifiedDate(modifiedDate), m_started(false) {
  connect(this, SIGNAL(started(TThread::RunnableP)), this,
          SLOT(onStarted(TThread::RunnableP)));
  connect(this, SIGNAL(canceled(TThread::RunnableP)), this,
          SLOT(onCanceled(TThread::RunnableP)));
}

//-----------------------------------------------------------------------------

FrameCountTask::~FrameCountTask() {}

//-----------------------------------------------------------------------------

void FrameCountTask::run() {
  TLevelReaderP lr(m_path);
  int frameCount = lr->loadInfo()->getFrameCount();

  QMutexLocker fCMapMutex(&frameCountMapMutex);

  std::map<TFilePath, FCData>::iterator it = frameCountMap.find(m_path);

  if (it == frameCountMap.end()) return;

  // Memorize the found frameCount into the frameCountMap
  if (frameCount > 0) {
    it->second.m_frameCount = frameCount;
    it->second.m_date       = m_modifiedDate;
  } else {
    // Seems that tlv reads sometimes may fail, returning invalid frame counts
    // (typically 0).
    // However, if no exception was thrown, we try to recover it
    it->second.m_underProgress = false;
    it->second.m_retryCount--;
  }
}

//-----------------------------------------------------------------------------

QThread::Priority FrameCountTask::runningPriority() {
  return QThread::LowPriority;
}

//-----------------------------------------------------------------------------

// NOTE: onStarted and onCanceled are invoked on the same thread - so m_started
// operations are serialized, it can be non-thread-guarded.
void FrameCountTask::onStarted(TThread::RunnableP thisTask) {
  m_started = true;
}

//-----------------------------------------------------------------------------

void FrameCountTask::onCanceled(TThread::RunnableP thisTask) {
  if (!m_started) {
    QMutexLocker fCMapMutex(&frameCountMapMutex);

    frameCountMap.erase(m_path);
  }
}

//=============================================================================

OpenFloatingPanel openBrowserPane(MI_OpenFileBrowser, "Browser",
                                  QObject::tr("File Browser"));
