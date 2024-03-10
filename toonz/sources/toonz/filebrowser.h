#pragma once

#ifndef FILEBROWSER_INCLUDED
#define FILEBROWSER_INCLUDED

#include <QFrame>
#include <QTreeWidget>
#include <QDateTime>
#include <QItemDelegate>
#include <QCheckBox>
#include <QList>
#include <QModelIndex>
#include "dvitemview.h"
#include "tfilepath.h"
#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

#include "tthread.h"

class QLineEdit;
class QTreeWidgetItem;
class QSplitter;
class DvDirModelNode;
class DvDirTreeView;
class QFileSystemWatcher;

//-----------------------------------------------------------------------------

//! FrameCountReader is the class responsible for calculation of levels' frame
//! counts
//! in the file browser. Since on many file formats this requires to open the
//! level file
//! and scan each frame (MOV-like), and on some machine configurations such a
//! task
//! can be time consuming, we dedicate a separate thread for it - just like the
//! icon
//! generator does. Calculated frame counts are also stored for quick lookup
//! once they
//! have been calculated the first time.
class FrameCountReader final : public QObject {
  Q_OBJECT

  TThread::Executor m_executor;

public:
  FrameCountReader();
  ~FrameCountReader();

  int getFrameCount(const TFilePath &path);
  void stopReading();

signals:

  void calculatedFrameCount();
};

//-----------------------------------------------------------------------------

class FileBrowser final : public QFrame, public DvItemListModel {
  Q_OBJECT

public:
  FileBrowser(QWidget *parent, Qt::WindowFlags flags = Qt::WindowFlags(),
              bool noContextMenu = false, bool multiSelectionEnabled = false);
  ~FileBrowser();

  void sortByDataModel(DataType dataType, bool isDiscendent) override;
  void refreshData() override;

  int getItemCount() const override;
  QVariant getItemData(int index, DataType dataType,
                       bool isSelected = false) override;

  bool canRenameItem(int index) const override;
  void renameItem(int index, const QString &newName) override;

  bool isSceneItem(int index) const override;
  void startDragDrop() override;
  QMenu *getContextMenu(QWidget *parent, int index) override;

  /*!
This functions adds to the types to be filtered a new type;
if this function is never  called, the default filter is all image
files and scene files and palette files
*/
  void addFilterType(const QString &type);

  /*!
The setFilterTypes function directly specifies the list of file
types to be displayed in the file browser.
*/
  void setFilterTypes(const QStringList &types);
  const QStringList &getFilterTypes() const { return m_filter; }
  void removeFilterType(const QString &type);

  void setFolder(const TFilePath &fp, bool expandNode = false,
                 bool forceUpdate = false);
  // process when inputting the folder which is not registered in the folder
  // tree (e.g. UNC path in Windows)
  void setUnregisteredFolder(const TFilePath &fp);

  void setHistoryDay(std::string dayDateString);

  TFilePath getFolder() const { return m_folder; }
  std::string getDayDateString() const { return m_dayDateString; }

  static void refreshFolder(const TFilePath &folder);

  static void updateItemViewerPanel();

  // ritorna true se il file e' stato rinominato. dopo la chiamata fp contiene
  // il nuovo path
  static bool renameFile(TFilePath &fp, QString newName);

  void makeCurrentProjectVisible();

  void enableGlobalSelection(bool enabled);
  void selectNone();

  QSplitter *getMainSplitter() const { return m_mainSplitter; }

  // Enable double-click to open a scene.
  // This is not always desirable (e.g. if a user double-clicks on a file in
  // a "Save As" dialog, they expect the file will be saved to, not opened).
  // So it is disabled by default.
  void enableDoubleClickToOpenScenes();

protected:
  int findIndexWithPath(TFilePath path);
  void getExpandedFolders(DvDirModelNode *node,
                          QList<DvDirModelNode *> &expandedNodes);

  bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data,
                    Qt::DropAction action);

  bool acceptDrop(const QMimeData *data) const override;
  bool drop(const QMimeData *data) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  // Fill the QStringList with files selected in the browser, auxiliary files
  // (palette for tlv, hooks, sceneIcons)
  // retrieve also the path, and return also the sceneIconsCount
  void setupVersionControlCommand(QStringList &files, QString &path,
                                  int &sceneIconsCount);
  void setupVersionControlCommand(QString &file, QString &path);

  void refreshHistoryButtons();

public slots:

  void onTreeFolderChanged();

protected slots:

  void refresh();

  void changeFolder(const QModelIndex &index);
  void onDataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight);
  void loadResources();
  void onClickedItem(int index);
  void onDoubleClickedItem(int index);
  void onSelectedItems(const std::set<int> &indexes);
  void folderUp();
  void newFolder();

  void onBackButtonPushed();
  void onFwdButtonPushed();
  void onFolderEdited();
  void storeFolderHistory();
  void clearHistory();

  void renameAsToonzLevel();
  void updateAndEditVersionControl();
  void editVersionControl();
  void unlockVersionControl();

  void editFrameRangeVersionControl();
  void unlockFrameRangeVersionControl();

  void putFrameRangeVersionControl();
  void revertFrameRangeVersionControl();

  void showLockInformation();
  void showFrameRangeLockInfo();

  void putVersionControl();
  void revertVersionControl();
  void deleteVersionControl();
  void getVersionControl();
  void getRevisionVersionControl();
  void getRevisionHistory();

  void onVersionControlCommandDone(const QStringList &files);

  void onFileSystemChanged(const QString &folderPath);

  // If filePath is a valid scene file, open it. Otherwise do nothing.
  void tryToOpenScene(const TFilePath &filePath);

signals:

  void filePathClicked(const TFilePath &);
  void filePathDoubleClicked(const TFilePath &);
  // reuse the list of TFrameId in order to skip loadInfo() when loading the
  // level with sequential frames.
  void filePathsSelected(const std::set<TFilePath> &,
                         const std::list<std::vector<TFrameId>> &);
  void treeFolderChanged(const TFilePath &);

  // for activating/deactivating the folder history buttons( back button &
  // forward button )
  void historyChanged(bool, bool);

private:
  struct Item {
    QString m_name;
    qlonglong m_fileSize;
    QDateTime m_creationDate;
    QDateTime m_modifiedDate;
    int m_frameCount;
    QString m_fileType;
    TFilePath m_path;
    bool m_validInfo;

    bool m_isFolder;
    bool m_isLink;
    // calling loadInfo to the level with sequential frames is time consuming.
    // so keep the list of frameIds at the first time and try to reuse it.
    std::vector<TFrameId> m_frameIds;

    Item() : m_frameCount(0), m_validInfo(false), m_fileSize(0) {}
    Item(const TFilePath &path, bool folder = false, bool link = false,
         QString name = QString(""))
        : m_path(path)
        , m_frameCount(0)
        , m_validInfo(false)
        , m_fileSize(0)
        , m_isFolder(folder)
        , m_isLink(link)
        , m_name(name) {}
  };

private:
  DvDirTreeView *m_folderTreeView;
  QSplitter *m_mainSplitter;
  QLineEdit *m_folderName;
  DvItemViewer *m_itemViewer;
  FrameCountReader m_frameCountReader;

  // folder history
  QList<QModelIndex> m_indexHistoryList;
  int m_currentPosition;

  std::vector<Item> m_items;
  TFilePath m_folder;
  std::string m_dayDateString;
  QStringList m_filter;
  std::map<TFilePath, Item> m_multiFileItemMap;

private:
  void readFrameCount(Item &item);
  void readInfo(Item &item);

  void refreshCurrentFolderItems();

  DvItemListModel::Status getItemVersionControlStatus(
      const FileBrowser::Item &item);
};

//--------------------------------------------------------------------
class RenameAsToonzPopup final : public DVGui::Dialog {
  Q_OBJECT
  QPushButton *m_okBtn, *m_cancelBtn;
  DVGui::LineEdit *m_name;
  QCheckBox *m_overwrite;

public:
  RenameAsToonzPopup(const QString name = "", int frames = -1);

  bool doOverwrite() { return m_overwrite->isChecked(); }
  QString getName() { return m_name->text(); }

private:
  // TPropertyGroup* getFormatProperties(const std::string &ext);

public slots:
  //! Starts the conversion.
  // void onConvert();
  // void onOptionsClicked();
  void onOk();
};

//-----------------------------------------------------------

#endif
