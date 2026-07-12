

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
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>

// C++ includes
#include <memory>  // std::unique_ptr, std::make_unique

namespace ba = boost::adaptors;

using namespace DVGui;

//=============================================================================
//      Local declarations
//=============================================================================

//=============================================================================
//    FrameCountTask class
//-----------------------------------------------------------------------------

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

//=============================================================================
//    FCData struct
//-----------------------------------------------------------------------------

struct FCData {
  QDateTime m_date;
  int m_frameCount;
  bool m_underProgress;
  int m_retryCount;

  FCData() = default;
  explicit FCData(const QDateTime &date);
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

//=============================================================================
// FileBrowser
//-----------------------------------------------------------------------------

FileBrowser::FileBrowser(QWidget *parent, Qt::WindowFlags flags,
                         bool noContextMenu, bool multiSelectionEnabled)
    : QFrame(parent), m_folderName(nullptr), m_itemViewer(nullptr) {
  // style sheet
  setObjectName("FileBrowser");
  setFrameStyle(QFrame::StyledPanel);

  m_mainSplitter      = new QSplitter(this);
  m_folderTreeView    = new DvDirTreeView(this);
  QFrame *box         = new QFrame(this);
  QLabel *folderLabel = new QLabel(tr("Folder: "), this);
  m_folderName        = new QLineEdit(this);
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
  mainLayout->setContentsMargins(3, 3, 3, 3);
  mainLayout->setSpacing(2);
  {
    mainLayout->addWidget(buttonBar);

    QHBoxLayout *folderLay = new QHBoxLayout();
    folderLay->setContentsMargins(0, 0, 0, 0);
    folderLay->setSpacing(0);
    {
      folderLay->addWidget(folderLabel, 0);
      folderLay->addWidget(m_folderName, 1);
    }
    mainLayout->addLayout(folderLay, 0);

    m_mainSplitter->addWidget(m_folderTreeView);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(0, 0, 0, 0);
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

  connect(m_folderTreeView, &DvDirTreeView::currentNodeChanged,
          itemViewPlayDelegate, &DVItemViewPlayDelegate::resetPlayWidget);
  connect(m_folderTreeView, &DvDirTreeView::currentNodeChanged, this,
          &FileBrowser::onTreeFolderChanged);

  connect(m_itemViewer, &DvItemViewer::clickedItem, this,
          &FileBrowser::onClickedItem);
  connect(m_itemViewer, &DvItemViewer::doubleClickedItem, this,
          &FileBrowser::onDoubleClickedItem);
  connect(m_itemViewer, &DvItemViewer::selectedItems, this,
          &FileBrowser::onSelectedItems);
  connect(buttonBar, &DvItemViewerButtonBar::folderUp, this,
          &FileBrowser::folderUp);
  connect(buttonBar, &DvItemViewerButtonBar::newFolder, this,
          &FileBrowser::newFolder);

  connect(&m_frameCountReader, &FrameCountReader::calculatedFrameCount,
          m_itemViewer->getPanel(), qOverload<>(&DvItemViewerPanel::update));

  QAction *refresh = CommandManager::instance()->getAction(MI_RefreshTree);
  connect(refresh, &QAction::triggered, this, &FileBrowser::refresh);
  addAction(refresh);

  // Version Control instance connection
  if (Preferences::instance()->isSVNEnabled()) {
    connect(VersionControl::instance(), &VersionControl::commandDone, this,
            &FileBrowser::onVersionControlCommandDone);
  }

  // if the folderName is edited, move the current folder accordingly
  connect(m_folderName, &QLineEdit::editingFinished, this,
          &FileBrowser::onFolderEdited);

  // folder history
  connect(m_folderTreeView, &DvDirTreeView::currentNodeChanged, this,
          &FileBrowser::storeFolderHistory);
  connect(buttonBar, &DvItemViewerButtonBar::folderBack, this,
          &FileBrowser::onBackButtonPushed);
  connect(buttonBar, &DvItemViewerButtonBar::folderFwd, this,
          &FileBrowser::onFwdButtonPushed);
  // when the history changes, enable/disable the history buttons accordingly
  connect(this, &FileBrowser::historyChanged, buttonBar,
          &DvItemViewerButtonBar::onHistoryChanged);

  // check out the update of the current folder.
  // Use MyFileSystemWatcher which is shared by all browsers.
  // Adding and removing paths to the watcher is done in DvDirTreeView.
  connect(MyFileSystemWatcher::instance(),
          &MyFileSystemWatcher::directoryChanged, this,
          &FileBrowser::onFileSystemChanged);

  // store the first item("Root") in the history
  m_indexHistoryList.append(m_folderTreeView->currentIndex());
  m_currentPosition = 0;

  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------

FileBrowser::~FileBrowser() = default;  // all child widgets are auto-deleted

//-----------------------------------------------------------------------------

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
    ++m_currentPosition;
  }
  // If the next history item is the same as the current one, just move to it
  else if (m_indexHistoryList[m_currentPosition + 1] == currentModelIndex) {
    ++m_currentPosition;
  }
  // If the next history item is different from the current one, then replace
  // with the new one
  else {
    int size = m_indexHistoryList.size();
    // remove the old history items
    for (int i = m_currentPosition + 1; i < size; ++i)
      m_indexHistoryList.removeLast();
    m_indexHistoryList << currentModelIndex;
    ++m_currentPosition;
  }
  refreshHistoryButtons();
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshHistoryButtons() {
  emit historyChanged(m_currentPosition != 0,
                      m_currentPosition != m_indexHistoryList.size() - 1);
}

//-----------------------------------------------------------------------------

void FileBrowser::onBackButtonPushed() {
  if (m_currentPosition == 0) return;
  --m_currentPosition;
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
  ++m_currentPosition;
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
  for (int i = 1; i < size; ++i) m_indexHistoryList.removeLast();
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
      return fb.compareData(dataType, aIdx, bIdx) > 0;
    }

    static inline bool indexLess(int aIdx, int bIdx,
                                 const std::vector<int> &vec) {
      return vec[aIdx] < vec[bIdx];
    }

    static inline int complement(int val, int max) {
      return (assert(0 <= val && val <= max), max - val);
    }
  };

  if (dataType != getCurrentOrderType()) {
    // Build the permutation table
    std::vector<int> new2OldIdx(
        boost::make_counting_iterator(0),
        boost::make_counting_iterator(int(m_items.size())));

    std::stable_sort(new2OldIdx.begin(), new2OldIdx.end(),
                     [this, dataType](int x, int y) {
                       return locals::itemLess(x, y, *this, dataType);
                     });

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
                [&new2OldIdx](int aIdx, int bIdx) {
                  return locals::indexLess(aIdx, bIdx, new2OldIdx);
                });

      std::vector<int> newSelectedIndices;
      tcg::substitute(newSelectedIndices,
                      tcg::permuted_range(
                          old2NewIdx, fs->getSelectedIndices() |
                                          ba::filtered([&old2NewIdx](int x) {
                                            return x < old2NewIdx.size();
                                          })));

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

    if (!fs || fs->isEmpty()) {
      // nothing to do
    } else {
      int iCount = int(m_items.size()), lastIdx = iCount - 1;

      std::vector<int> newSelectedIndices;
      tcg::substitute(newSelectedIndices,
                      fs->getSelectedIndices() | ba::filtered([iCount](int x) {
                        return x < iCount;
                      }) | ba::transformed([lastIdx](int x) {
                        return locals::complement(x, lastIdx);
                      }));

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
    m_items.emplace_back(parentFp, true, false);

  // register all folder items by using the folder tree model
  DvDirModel *model        = DvDirModel::instance();
  QModelIndex currentIndex = model->getIndexByPath(m_folder);
  if (currentIndex.isValid()) {
    for (int i = 0; i < model->rowCount(currentIndex); ++i) {
      QModelIndex tmpIndex = model->index(i, 0, currentIndex);
      if (tmpIndex.isValid()) {
        DvDirModelFileFolderNode *node =
            dynamic_cast<DvDirModelFileFolderNode *>(model->getNode(tmpIndex));
        if (node) {
          TFilePath childFolderPath = node->getPath();
          if (TFileStatus(childFolderPath).isLink())
            m_items.emplace_back(childFolderPath, true, true,
                                 QString::fromStdWString(node->getName()));
          else
            m_items.emplace_back(childFolderPath, true, false,
                                 QString::fromStdWString(node->getName()));
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

    for (const TFilePath &it : files) {
#ifdef _WIN32
      // include folder shortcut items
      if (it.getType() == "lnk") {
        TFileStatus info(it);
        if (info.isLink() && info.isDirectory()) {
          m_items.emplace_back(it, true, true,
                               QString::fromStdString(it.getName()));
        }
        continue;
      }
#endif
      // skip the plt file (Palette file for TOONZ 4.6 and earlier)
      if (it.getType() == "plt") continue;

      // filter the file
      else if (m_filter.isEmpty()) {
        if (it.getType() != "tnz" && it.getType() != "scr" &&
            it.getType() != "tnzbat" && it.getType() != "mpath" &&
            it.getType() != "curve" && it.getType() != "tpl" &&
            TFileType::getInfo(it) == TFileType::UNKNOW_FILE)
          continue;
      } else if (!m_filter.contains(QString::fromStdString(it.getType())))
        continue;
      // store the filtered file paths
      m_items.emplace_back(it);
    }

    // update the m_multiFileItemMap
    m_multiFileItemMap.clear();

    for (const TFilePath &it : all_files) {
      TFrameId tFrameId = it.getFrame();
      TFilePath levelName(it.getLevelName());

      if (levelName.isLevelName()) {
        Item &levelItem = m_multiFileItemMap[levelName];

        // TODO:
        // For now, leave it as is, but if obtaining FileInfo takes too much
        // time, consider making it optional 2015/12/28 shun_iwasawa
        QFileInfo fileInfo(QString::fromStdWString(it.getWideString()));
        // Get creation time safely across platforms
        QDateTime creationTime = fileInfo.birthTime();
        if (!creationTime.isValid())
          creationTime = fileInfo.metadataChangeTime();

        // Update level infos
        if (levelItem.m_creationDate.isNull() ||
            creationTime < levelItem.m_creationDate)
          levelItem.m_creationDate = creationTime;

        if (levelItem.m_modifiedDate.isNull() ||
            fileInfo.lastModified() > levelItem.m_modifiedDate)
          levelItem.m_modifiedDate = fileInfo.lastModified();

        levelItem.m_fileSize += fileInfo.size();

        // Store frame ID
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
    for (int i = 0; i < missingItemCount; ++i) m_items.emplace_back(list.at(i));
  }

  // Refresh Data (fill Item field)
  refreshData();

  // If I added some missing items I need to sort items.
  if (missingItemCount > 0) {
    DataType currentDataType = getCurrentOrderType();
    for (int i = 1; i < (int)m_items.size(); ++i) {
      int index = i;
      while (index > 0 && compareData(currentDataType, index - 1, index) > 0) {
        std::swap(m_items[index - 1], m_items[index]);
        --index;
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
/*! process when inputting the folder which is not registered in the folder tree
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

    // register all folder items
    for (const TFilePath &it : folders) {
      if (TFileStatus(it).isLink())
        m_items.emplace_back(it, true, true);
      else
        m_items.emplace_back(it, true, false);
    }

    for (const TFilePath &it : files) {
#ifdef _WIN32
      // include folder shortcut items
      if (it.getType() == "lnk") {
        TFileStatus info(it);
        if (info.isLink() && info.isDirectory()) {
          m_items.emplace_back(it, true, true,
                               QString::fromStdString(it.getName()));
        }
        continue;
      }
#endif
      // skip the plt file (Palette file for TOONZ 4.6 and earlier)
      if (it.getType() == "plt") continue;

      // filtering
      else if (m_filter.isEmpty()) {
        if (it.getType() != "tnz" && it.getType() != "scr" &&
            it.getType() != "tnzbat" && it.getType() != "mpath" &&
            it.getType() != "curve" && it.getType() != "tpl" &&
            TFileType::getInfo(it) == TFileType::UNKNOW_FILE)
          continue;
      } else if (!m_filter.contains(QString::fromStdString(it.getType())))
        continue;

      m_items.emplace_back(it);
    }

    // update the m_multiFileItemMap
    m_multiFileItemMap.clear();
    for (const TFilePath &it : all_files) {
      TFilePath levelName(it.getLevelName());
      if (levelName.isLevelName()) {
        Item &levelItem = m_multiFileItemMap[levelName];
        levelItem.m_frameIds.push_back(it.getFrame());
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
  if (day == nullptr) {
    m_folderName->setText("");
  } else {
    m_folderName->setText(QString::fromStdString(dayDateString));
    std::vector<TFilePath> files;
    day->getFiles(files);
    for (const TFilePath &it : files) m_items.emplace_back(it);
  }
  refreshData();
}

//-----------------------------------------------------------------------------
/*! for all items in the folder, retrieve the file names(m_name) from the
 * paths(m_path)
 */
void FileBrowser::refreshData() {
  for (Item &it : m_items) {
    if (it.m_name.isEmpty())
      it.m_name = toQString(it.m_path.withoutParentDir());
  }
}

//-----------------------------------------------------------------------------

int FileBrowser::getItemCount() const { return int(m_items.size()); }

//-----------------------------------------------------------------------------

void FileBrowser::readInfo(Item &item) {
  TFilePath fp = item.m_path;
  QFileInfo info(toQString(fp));
  if (info.exists()) {
    // Use birthTime(), fall back to metadataChangeTime() if unavailable
    item.m_creationDate = info.birthTime();
    if (!item.m_creationDate.isValid())
      item.m_creationDate = info.metadataChangeTime();
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
  } else if (fp.isLevelName()) {
    try {
      // Find this level's item
      auto it = m_multiFileItemMap.find(TFilePath(item.m_path.getLevelName()));
      if (it == m_multiFileItemMap.end()) throw "";

      item.m_creationDate = it->second.m_creationDate;
      item.m_modifiedDate = it->second.m_modifiedDate;
      item.m_fileType     = it->second.m_fileType;
      item.m_fileSize     = it->second.m_fileSize;
      item.m_frameCount   = it->second.m_frameCount;
      item.m_validInfo    = true;

      // keep the list of frameIds at the first time and try to reuse it.
      item.m_frameIds = it->second.m_frameIds;
    } catch (...) {
      item.m_frameCount = 0;
    }
  }

  item.m_validInfo = true;
}

//-----------------------------------------------------------------------------

//! Frame count needs a special access function for viewable types - for they
//! are calculated by using a dedicated thread and therefore cannot be simply
//! classified as *valid* or *invalid* infos...
void FileBrowser::readFrameCount(Item &item) {
  if (!item.m_isFolder &&
      TFileType::isViewable(TFileType::getInfo(item.m_path))) {
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
  if (index < 0 || index >= getItemCount()) return QVariant();
  Item &item = m_items[index];
  if (dataType == Name) {
    // show two dots( ".." ) for the parent directory item
    if (item.m_path == m_folder.getParentDir())
      return QString("..");
    else
      return item.m_name;
  } else if (dataType == Thumbnail) {
    QSize iconSize = m_itemViewer->getPanel()->getIconSize();
    // parent folder icons
    if (item.m_path == m_folder.getParentDir()) {
      static QPixmap folderUpPixmap(getIconPath("folder_browser_up"));
      return folderUpPixmap;
    }
    // folder icons
    else if (item.m_isFolder) {
      if (item.m_isLink) {
        static QPixmap folderLinkPixmap(getIconPath("folder_browser_link"));
        return folderLinkPixmap;
      } else {
        static QPixmap folderPixmap(getIconPath("folder_browser"));
        return folderPixmap;
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

  else if (dataType == IsFolder)
    return item.m_isFolder;

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
  return 0 <= index && index < getItemCount() &&
         m_items[index].m_path.getType() == "tnz";
}

//-----------------------------------------------------------------------------

bool FileBrowser::canRenameItem(int index) const {
  // if viewing history, cannot rename anything
  if (getFolder() == TFilePath()) return false;
  if (index < 0 || index >= getItemCount()) return false;
  // for now, disable rename for folders
  if (m_items[index].m_isFolder) return false;
  TFilePath fp = m_items[index].m_path;
  return TFileStatus(fp).doesExist();
}

//-----------------------------------------------------------------------------

int FileBrowser::findIndexWithPath(TFilePath path) {
  for (int i = 0; i < (int)m_items.size(); ++i)
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

    // I have also renamed the palette, I must update it.
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
      // I changed the _files folder. I must update the folder that contains it
      // in the tree view
      QModelIndex index = m_folderTreeView->currentIndex();
      if (index.isValid()) {
        DvDirModel::instance()->refresh(index);
        m_folderTreeView->update();
      }
    }
  }
}

//-----------------------------------------------------------------------------

bool FileBrowser::renameFile(TFilePath &fp, QString newName) {
  if (isSpaceString(newName)) return true;

  TFilePath newFp(newName.toStdWString());
  if (!newFp.getType().empty() && newFp.getType() != fp.getType()) {
    DVGui::error(tr("Can't change file extension"));
    return false;
  }
  if (newFp.getType().empty()) newFp = newFp.withType(fp.getType());
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

  // if they are the same, I don't need to rename anything
  if (newFp == fp) return false;

  if (TSystem::doesExistFileOrLevel(newFp)) {
    DVGui::error(tr("Can't rename. File already exists: ") + toQString(newFp));
    return false;
  }

  try {
    TSystem::renameFileOrLevel_throw(newFp, fp, true);
    IconGenerator::instance()->remove(fp);
    if (fp.getType() == "tnz") {
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
  auto isOldLevelType = [](const TFilePath &path) -> bool {
    return path.getType() == "tzp" || path.getType() == "tzu";
  };

  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return nullptr;
  std::vector<TFilePath> files;
  fs->getSelectedFiles(files);

  // Use unique_ptr for automatic memory management
  auto menu          = std::make_unique<QMenu>(parent);
  CommandManager *cm = CommandManager::instance();

  // when folder item is right-clicked
  if (0 <= index && index < (int)m_items.size() && m_items[index].m_isFolder) {
    DvDirModel *model    = DvDirModel::instance();
    DvDirModelNode *node = model->getNode(model->getIndexByPath(files[0]));

    if (!node || !node->isRenameEnabled()) return nullptr;

    DvDirModelFileFolderNode *fnode =
        dynamic_cast<DvDirModelFileFolderNode *>(node);
    if (!fnode || fnode->isProjectFolder()) return nullptr;

    menu->addAction(cm->getAction(MI_Clear));
    QAction *action =
        menu->addAction(QIcon(createQIcon("rename")), tr("Rename"));
    connect(action, &QAction::triggered, this, &FileBrowser::renameFolder);
    return menu.release();
  }

  if (files.empty()) {
    menu->addAction(cm->getAction(MI_ShowFolderContents));
    menu->addAction(cm->getAction(MI_SelectAll));
    if (!Preferences::instance()->isWatchFileSystemEnabled()) {
      menu->addAction(cm->getAction(MI_RefreshTree));
    }
    return menu.release();
  }

  if (files.size() == 1 && files[0].getType() == "tnz") {
    menu->addAction(cm->getAction(MI_LoadScene));
  }

  bool areResources = true;
  bool areScenes    = false;
  for (const TFilePath &f : files) {
    TFileType::Type type = TFileType::getInfo(f);
    if (areResources && !TFileType::isResource(type)) areResources = false;
    if (!areScenes && TFileType::isScene(type)) areScenes = true;
  }

  bool areFullcolor = true;
  for (const TFilePath &f : files) {
    TFileType::Type type = TFileType::getInfo(f);
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
      title = tr("Load");
    QAction *action = menu->addAction(QIcon(createQIcon("import")), title);
    connect(action, &QAction::triggered, this, &FileBrowser::loadResources);
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

  int i;
  for (i = 0; i < (int)files.size(); ++i)
    if (!TFileType::isViewable(TFileType::getInfo(files[i])) &&
        files[i].getType() != "tpl")
      break;
  if (i == (int)files.size()) {
    std::string type = files[0].getType();
    int j;
    for (j = 0; j < (int)files.size(); ++j)
      if (isOldLevelType(files[j])) break;
    if (j == (int)files.size()) menu->addAction(cm->getAction(MI_ViewFile));

    for (j = 0; j < (int)files.size(); ++j) {
      if ((files[0].getType() == "pli" && files[j].getType() != "pli") ||
          (files[0].getType() != "pli" && files[j].getType() == "pli"))
        break;
      else if ((isOldLevelType(files[0]) && !isOldLevelType(files[j])) ||
               (!isOldLevelType(files[0]) && isOldLevelType(files[j])))
        break;
    }
    if (j == (int)files.size()) {
      menu->addAction(cm->getAction(MI_ConvertFiles));
      // iwsw commented out temporarily
      // menu->addAction(cm->getAction(MI_ToonShadedImageToTLV));
    }
    if (areFullcolor) menu->addAction(cm->getAction(MI_SeparateColors));

    if (!areFullcolor) menu->addSeparator();
  }
  if (files.size() == 1 && files[0].getType() != "tnz") {
    QAction *action =
        menu->addAction(QIcon(createQIcon("rename")), tr("Rename"));
    connect(action, &QAction::triggered, this,
            &FileBrowser::renameAsToonzLevel);
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
    connect(action, &QAction::triggered, this,
            &FileBrowser::convertToPaintedTlv);
    menu->addAction(action);
  }
  if (areFullcolor) {
    QAction *action = new QAction(tr("Convert to Unpainted TLV"), menu);
    connect(action, &QAction::triggered, this,
            &FileBrowser::convertToUnpaintedTlv);
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
    DvItemListModel::Status status = DvItemListModel::Status(
        m_itemViewer->getModel()
            ->getItemData(index, DvItemListModel::VersionControlStatus)
            .toInt());

    // Remove the added actions
    if (status == DvItemListModel::VC_Missing) menu->clear();

    auto vcMenu     = std::make_unique<QMenu>(tr("Version Control"), parent);
    QAction *action = nullptr;

    if (status == DvItemListModel::VC_ReadOnly ||
        (status == DvItemListModel::VC_ToUpdate && files.size() == 1)) {
      if (status == DvItemListModel::VC_ReadOnly) {
        action = vcMenu->addAction(tr("Edit"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::editVersionControl);

        TFilePath path       = files.at(0);
        std::string fileType = path.getType();
        if (fileType == "tlv" || fileType == "pli" || path.getDots() == "..") {
          action = vcMenu->addAction(tr("Edit Frame Range..."));
          connect(action, &QAction::triggered, this,
                  &FileBrowser::editFrameRangeVersionControl);
        }
      } else {
        action = vcMenu->addAction(tr("Edit"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::updateAndEditVersionControl);
      }
    }

    if (status == DvItemListModel::VC_Modified) {
      action = vcMenu->addAction(tr("Put..."));
      connect(action, &QAction::triggered, this,
              &FileBrowser::putVersionControl);

      action = vcMenu->addAction(tr("Revert"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::revertVersionControl);
    }

    if (status == DvItemListModel::VC_ReadOnly ||
        status == DvItemListModel::VC_ToUpdate) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (status == DvItemListModel::VC_ReadOnly) {
        action = vcMenu->addAction(tr("Delete"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::deleteVersionControl);
      }

      vcMenu->addSeparator();

      if (files.size() == 1) {
        action         = vcMenu->addAction(tr("Get Revision..."));
        TFilePath path = files.at(0);
        if (path.getDots() == "..")
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionVersionControl);
        else
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionHistory);
      } else if (files.size() > 1) {
        action = vcMenu->addAction("Get Revision...");
        connect(action, &QAction::triggered, this,
                &FileBrowser::getRevisionVersionControl);
      }
    }

    if (status == DvItemListModel::VC_Edited) {
      action = vcMenu->addAction(tr("Unlock"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::unlockVersionControl);
    }

    if (status == DvItemListModel::VC_Unversioned) {
      action = vcMenu->addAction(tr("Put..."));
      connect(action, &QAction::triggered, this,
              &FileBrowser::putVersionControl);
    }

    if (status == DvItemListModel::VC_Locked && files.size() == 1) {
      action = vcMenu->addAction(tr("Unlock"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::unlockVersionControl);

      action = vcMenu->addAction(tr("Edit Info"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::showLockInformation);
    }

    if (status == DvItemListModel::VC_Missing) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (files.size() == 1) {
        vcMenu->addSeparator();
        action         = vcMenu->addAction(tr("Revision History..."));
        TFilePath path = files.at(0);
        if (path.getDots() == "..")
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionVersionControl);
        else
          connect(action, &QAction::triggered, this,
                  &FileBrowser::getRevisionHistory);
      }
    }

    if (status == DvItemListModel::VC_PartialLocked) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);
      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Edit Frame Range..."));
        connect(action, &QAction::triggered, this,
                &FileBrowser::editFrameRangeVersionControl);

        action = vcMenu->addAction(tr("Edit Info"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::showFrameRangeLockInfo);
      }
    } else if (status == DvItemListModel::VC_PartialEdited) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Unlock Frame Range"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::unlockFrameRangeVersionControl);

        action = vcMenu->addAction(tr("Edit Info"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::showFrameRangeLockInfo);
      }
    } else if (status == DvItemListModel::VC_PartialModified) {
      action = vcMenu->addAction(tr("Get"));
      connect(action, &QAction::triggered, this,
              &FileBrowser::getVersionControl);

      if (files.size() == 1) {
        action = vcMenu->addAction(tr("Put..."));
        connect(action, &QAction::triggered, this,
                &FileBrowser::putFrameRangeVersionControl);

        action = vcMenu->addAction(tr("Revert"));
        connect(action, &QAction::triggered, this,
                &FileBrowser::revertFrameRangeVersionControl);
      }
    }

    if (!vcMenu->isEmpty()) {
      menu->addSeparator();
      menu->addMenu(vcMenu.release());  // transfer ownership to menu
    }
  }

  if (!Preferences::instance()->isWatchFileSystemEnabled()) {
    menu->addSeparator();
    menu->addAction(cm->getAction(MI_RefreshTree));
  }

  return menu.release();
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
  for (const TFilePath &f : files) {
    if (TSystem::doesExistFileOrLevel(f))
      urls.append(
          QUrl::fromLocalFile(QString::fromStdWString(f.getWideString())));
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

  drag->exec(Qt::CopyAction);
}

//-----------------------------------------------------------------------------

bool FileBrowser::dropMimeData(QTreeWidgetItem *, int, const QMimeData *,
                               Qt::DropAction) {
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

void FileBrowser::changeFolder(const QModelIndex &) {}

//-----------------------------------------------------------------------------

void FileBrowser::onDataChanged(const QModelIndex &, const QModelIndex &) {
  onTreeFolderChanged();
}

//-----------------------------------------------------------------------------

bool FileBrowser::acceptDrop(const QMimeData *data) const {
  // if the browser is not displaying a standard folder, cannot accept any drop
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
  // if the browser is not displaying a standard folder, cannot accept any drop
  TFilePath folderPath = getFolder();
  if (folderPath == TFilePath()) return false;

  if (mimeData->hasFormat(CastItems::getMimeFormat())) {
    const CastItems *items = dynamic_cast<const CastItems *>(mimeData);
    if (!items) return false;

    for (int i = 0; i < items->getItemCount(); ++i) {
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
      if (!ok || sceneName.isEmpty()) return false;
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
      QString question =
          tr("Level %1 already exists\nDo you want to duplicate it?")
              .arg(toQString(folderPath));
      int ret =
          DVGui::MsgBox(question, tr("Duplicate"), tr("Don't Duplicate"), 0);
      if (ret == 2 || ret == 0) return false;
      TFilePath path = folderPath;
      NameBuilder *nameBuilder =
          NameBuilder::getBuilder(::to_wstring(path.getName()));
      do levelName = nameBuilder->getNext();
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
    for (const TFrameId &fid : frames) {
      newSl->setFrame(fid, sl->getFrame(fid, false));
    }

    IoCmd::saveLevel(folderPath, newSl, false);
    refreshFolder(folderPath.getParentDir());
    return true;
  } else if (mimeData->hasUrls()) {
    for (const QUrl &url : mimeData->urls()) {
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

RenameAsToonzPopup::RenameAsToonzPopup(const QString &name, int frames,
                                       bool isFolder)
    : Dialog(TApp::instance()->getMainWindow(), true, true, "RenameAsToonz") {
  setWindowTitle(tr("Rename"));

  beginHLayout();

  QLabel *lbl;
  if (frames == -1) {
    if (isFolder)
      lbl = new QLabel(tr("Renaming Folder ") + name, this);
    else
      lbl = new QLabel(tr("Renaming File ") + name, this);
  } else
    lbl = new QLabel(tr("Creating an animation level of %1 frames").arg(frames),
                     this);
  lbl->setFixedHeight(20);
  lbl->setObjectName("TitleTxtLabel");

  m_name = new LineEdit(frames == -1 ? QString() : name, this);
  m_name->setFixedHeight(20);

  m_overwrite = new QCheckBox(tr("Delete Original Files"), this);
  m_overwrite->setFixedHeight(20);

  QFormLayout *formLayout  = new QFormLayout(this);
  QHBoxLayout *labelLayout = new QHBoxLayout;
  labelLayout->addStretch();
  labelLayout->addWidget(lbl);
  labelLayout->addStretch();

  formLayout->addRow(labelLayout);
  formLayout->addRow((isFolder) ? tr("Folder Name:") : tr("Level Name:"),
                     m_name);
  if (!isFolder) formLayout->addRow(m_overwrite);

  addLayout(formLayout);
  endHLayout();

  m_okBtn = new QPushButton(tr("Rename"), this);
  m_okBtn->setDefault(true);
  m_cancelBtn = new QPushButton(tr("Cancel"), this);

  connect(m_okBtn, &QPushButton::clicked, this, &RenameAsToonzPopup::onOk);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  addButtonBarWidget(m_okBtn, m_cancelBtn);
}

//-----------------------------------------------------------------------------

void RenameAsToonzPopup::onOk() {
  if (!isValidFileName(m_name->text())) {
    DVGui::error(
        tr("The file name cannot be empty or contain any of the following "
           "characters:(new line)  \\ / : * ? \"  |"));
    return;
  }
  if (isReservedFileName_message(m_name->text())) return;
  accept();
}

//-----------------------------------------------------------------------------

namespace {

bool parsePathName(const QString &fullpath, QString &parentPath, QString &name,
                   QString &format) {
  int index = fullpath.lastIndexOf('\\');
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
  if (format.isEmpty()) return false;

  --index;
  if (!filename.at(index).isDigit()) return false;

  while (index >= 0 && filename.at(index).isDigit()) --index;

  if (index < 0) return false;

  name = filename.left(index + 1);

  return true;
}

//---------------------------------------------------------

void getLevelFiles(const QString &parentPath, const QString &name,
                   const QString &format, QStringList &pathIn) {
  QString filter = "*." + format;
  QDir dir(parentPath, filter);
  QStringList list = dir.entryList();

  for (const QString &item : list) {
    QString dummy, dummy1, itemName;
    if (!parsePathName(item, dummy, itemName, dummy1) || name != itemName)
      continue;
    pathIn.push_back(item);
  }
}

//---------------------------------------------------------

QString getFrame(const QString &filename) {
  int index = filename.lastIndexOf('.');

  if (index <= 0) return QString();

  --index;
  if (!filename.at(index).isDigit()) return QString();

  int to = index, from = index;
  while (from >= 0 && filename.at(from).isDigit()) --from;

  if (from < 0) return QString();

  char padStr[5] = {0};
  QString number = filename.mid(from + 1, to - from);
  for (int i = 0; i < 4 - number.size(); ++i) padStr[i] = '0';
  for (int i = 0; i < number.size(); ++i)
    padStr[4 - number.size() + i] = number.at(i).toLatin1();
  return QString(padStr);
}

//-----------------------------------------------------------

void renameSingleFileOrToonzLevel(const QString &fullpath) {
  TFilePath fpin(fullpath.toStdString());

  RenameAsToonzPopup popup(
      QString::fromStdWString(fpin.withoutParentDir().getWideString()));
  if (popup.exec() != QDialog::Accepted) return;

  std::string name = popup.getName().toStdString();

  if (name == fpin.getName()) {
    DVGui::error(
        QObject::tr("The specified name is already assigned to the %1 file.")
            .arg(fullpath));
    return;
  }

  if (popup.doOverwrite())
    TSystem::renameFileOrLevel(fpin.withName(name), fpin, true);
  else {
    if (TSystem::doesExistFileOrLevel(fpin.withName(name))) {
      DVGui::error(
          QObject::tr("The specified name is already assigned to the %1 file.")
              .arg(fpin.withName(name).getQString()));
      return;
    } else
      TSystem::copyFileOrLevel(fpin.withName(name), fpin);
  }
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

  for (const QString &fname : pathIn) {
    QString padStr = getFrame(fname);
    if (padStr.isEmpty()) continue;
    QString pathOut = parentPath + "/" + name + "." + padStr + "." + format;

    if (popup.doOverwrite()) {
      if (!QFile::rename(parentPath + "/" + fname, pathOut)) {
        QString tmp(parentPath + "/" + fname);
        DVGui::error(
            QObject::tr("It is not possible to rename the %1 file.").arg(tmp));
        return;
      }
    } else if (!QFile::copy(parentPath + "/" + fname, pathOut)) {
      QString tmp(parentPath + "/" + fname);
      DVGui::error(
          QObject::tr("It is not possible to copy the %1 file.").arg(tmp));
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

//-------------------------------------------------------------------------------

void FileBrowser::renameFolder() {
  std::vector<TFilePath> filePaths;
  FileSelection *fs =
      dynamic_cast<FileSelection *>(m_itemViewer->getPanel()->getSelection());
  if (!fs) return;
  fs->getSelectedFiles(filePaths);
  if (filePaths.size() != 1) return;

  TFilePath srcPath = filePaths[0];

  RenameAsToonzPopup popup(srcPath.withoutParentDir().getQString(), -1, true);
  if (popup.exec() != QDialog::Accepted) return;
  std::string name = popup.getName().toStdString();
  if (name == srcPath.getName()) {
    DVGui::error(
        QObject::tr("The specified name is already assigned to the folder."));
    return;
  }

  TFilePath dstPath = srcPath.getParentDir() + TFilePath(popup.getName());

  try {
    TSystem::renameFile(dstPath, srcPath);
  } catch (...) {
    return;
  }

  QApplication::restoreOverrideCursor();
  refreshFolder(srcPath.getParentDir());
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
  std::set<TFilePath> filePaths;
  std::list<std::vector<TFrameId>> frameIDs;

  if (indexes.empty()) {
    emit filePathsSelected(filePaths, frameIDs);
    return;
  }

  size_t itemsSize = m_items.size();
  for (int idx : indexes) {
    if (idx < 0 || static_cast<size_t>(idx) >= itemsSize) continue;

    filePaths.insert(m_items[idx].m_path);
    frameIDs.push_back(m_items[idx].m_frameIds);
  }

  emit filePathsSelected(filePaths, frameIDs);
}

//-----------------------------------------------------------------------------

void FileBrowser::onClickedItem(int index) {
  if (0 <= index && index < (int)m_items.size()) {
    TFilePath fp = m_items[index].m_path;
    if (m_items[index].m_isFolder) {
      setFolder(fp, true);
      QModelIndex idx = m_folderTreeView->currentIndex();
      if (idx.isValid()) m_folderTreeView->scrollTo(idx);
    } else
      emit filePathClicked(fp);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::onDoubleClickedItem(int index) {
  // TODO: Avoid duplicate code with onClickedItem().
  if (0 <= index && index < (int)m_items.size()) {
    TFilePath fp = m_items[index].m_path;
    if (m_items[index].m_isFolder) {
      setFolder(fp, true);
      QModelIndex idx = m_folderTreeView->currentIndex();
      if (idx.isValid()) m_folderTreeView->scrollTo(idx);
    } else
      emit filePathDoubleClicked(fp);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::refreshFolder(const TFilePath &folderPath) {
  for (FileBrowser *browser : activeBrowsers) {
    DvDirModel::instance()->refreshFolder(folderPath);
    if (browser->getFolder() == folderPath) {
      browser->setFolder(folderPath, false, true);
    }
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::updateItemViewerPanel() {
  for (FileBrowser *browser : activeBrowsers) {
    browser->m_itemViewer->getPanel()->update();
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::getExpandedFolders(DvDirModelNode *node,
                                     QList<DvDirModelNode *> &expandedNodes) {
  if (!node) return;
  QModelIndex newIndex = DvDirModel::instance()->getIndexByNode(node);
  if (!m_folderTreeView->isExpanded(newIndex)) return;
  expandedNodes.push_back(node);

  for (int i = 0; i < node->getChildCount(); ++i)
    getExpandedFolders(node->getChild(i), expandedNodes);
}

//-----------------------------------------------------------------------------

void FileBrowser::refresh() {
  TFilePath originalFolder(m_folder);

  int dx                   = m_folderTreeView->verticalScrollBar()->value();
  DvDirModelNode *rootNode = DvDirModel::instance()->getNode(QModelIndex());

  QModelIndex index = DvDirModel::instance()->getIndexByNode(rootNode);

  bool vcEnabled = m_folderTreeView->refreshVersionControlEnabled();

  m_folderTreeView->setRefreshVersionControlEnabled(false);
  DvDirModel::instance()->refreshFolderChild(index);
  m_folderTreeView->setRefreshVersionControlEnabled(vcEnabled);

  QList<DvDirModelNode *> expandedNodes;
  for (int i = 0; i < rootNode->getChildCount(); ++i)
    getExpandedFolders(rootNode->getChild(i), expandedNodes);

  for (DvDirModelNode *node : expandedNodes) {
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
  if (!index.isValid() || !index.parent().isValid()) {
    // cannot go up tree view, so try going to parent directory
    TFilePath parentFp = m_folder.getParentDir();
    if (parentFp != TFilePath("") && parentFp != m_folder) {
      setFolder(parentFp, true);
    }
    return;
  }
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
    m_folderTreeView->QTreeView::edit(newFolderIndex);
  }
}

//-----------------------------------------------------------------------------

void FileBrowser::showEvent(QShowEvent *) {
  activeBrowsers.insert(this);
  // refresh
  if (getFolder() != TFilePath())
    setFolder(getFolder(), false, true);
  else if (!getDayDateString().empty())
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

//-----------------------------------------------------------------------------

void FileBrowser::enableDoubleClickToOpenScenes() {
  connect(this, &FileBrowser::filePathDoubleClicked, this,
          &FileBrowser::tryToOpenScene);
}

//-----------------------------------------------------------------------------

void FileBrowser::tryToOpenScene(const TFilePath &filePath) {
  if (filePath.getType() == "tnz") {
    IoCmd::loadScene(filePath);
  }
}

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

FrameCountReader::~FrameCountReader() = default;

//-----------------------------------------------------------------------------

int FrameCountReader::getFrameCount(const TFilePath &fp) {
  QDateTime modifiedDate =
      QFileInfo(QString::fromStdWString(fp.getWideString())).lastModified();

  {
    QMutexLocker locker(&frameCountMapMutex);
    auto it = frameCountMap.find(fp);

    if (it != frameCountMap.end()) {
      if (it->second.m_frameCount > 0 && it->second.m_date == modifiedDate) {
        return it->second.m_frameCount;
      }
      if ((modifiedDate == it->second.m_date) &&
          (it->second.m_underProgress || it->second.m_retryCount < 0)) {
        return -1;
      }
    } else {
      frameCountMap[fp] = FCData(modifiedDate);
    }
  }

  // We have to calculate the frame count; create a task and submit it.
  auto *task = new FrameCountTask(fp, modifiedDate);
  connect(task, &FrameCountTask::finished, this,
          &FrameCountReader::calculatedFrameCount);
  connect(task, &FrameCountTask::exception, this,
          &FrameCountReader::calculatedFrameCount);

  m_executor.addTask(task);
  return -1;
}

//-----------------------------------------------------------------------------

void FrameCountReader::stopReading() { m_executor.cancelAll(); }

//=============================================================================
// FrameCountTask methods
//-----------------------------------------------------------------------------

FrameCountTask::FrameCountTask(const TFilePath &path,
                               const QDateTime &modifiedDate)
    : m_path(path), m_modifiedDate(modifiedDate), m_started(false) {
  connect(this, &FrameCountTask::started, this, &FrameCountTask::onStarted);
  connect(this, &FrameCountTask::canceled, this, &FrameCountTask::onCanceled);
}

//-----------------------------------------------------------------------------

FrameCountTask::~FrameCountTask() = default;

//-----------------------------------------------------------------------------

void FrameCountTask::run() {
  TLevelReaderP lr(m_path);
  int frameCount = lr->loadInfo()->getFrameCount();

  QMutexLocker fCMapMutex(&frameCountMapMutex);

  auto it = frameCountMap.find(m_path);
  if (it == frameCountMap.end()) return;

  // Memorize the found frameCount into the frameCountMap
  if (frameCount > 0) {
    it->second.m_frameCount = frameCount;
    it->second.m_date       = m_modifiedDate;
  } else {
    // Seems that tlv reads sometimes may fail, returning invalid frame counts
    // (typically 0). However, if no exception was thrown, we try to recover it
    it->second.m_underProgress = false;
    it->second.m_retryCount--;
  }
}

//-----------------------------------------------------------------------------

QThread::Priority FrameCountTask::runningPriority() {
  return QThread::LowPriority;
}

//-----------------------------------------------------------------------------

void FrameCountTask::onStarted(TThread::RunnableP) { m_started = true; }

//-----------------------------------------------------------------------------

void FrameCountTask::onCanceled(TThread::RunnableP) {
  if (!m_started) {
    QMutexLocker fCMapMutex(&frameCountMapMutex);
    frameCountMap.erase(m_path);
  }
}

//=============================================================================

OpenFloatingPanel openBrowserPane(MI_OpenFileBrowser, "Browser",
                                  QObject::tr("File Browser"));
