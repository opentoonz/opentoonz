#pragma once

#ifndef STUDIOPALETTEVIEWER_H
#define STUDIOPALETTEVIEWER_H

#include "toonz/studiopalette.h"
#include "toonz/tproject.h"

#include <QTreeWidget>
#include <QSplitter>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QLabel;
class TPaletteHandle;
class TFrameHandle;
class PalettesScanPopup;
class TXsheetHandle;
class TXshLevelHandle;
class PaletteViewer;
//=============================================================================
//!	The StudioPaletteTreeViewer class provides an object to view and manage
//! palettes files.
/*!	Inherits \b QTreeWidget, \b StudioPalette::Listener and \b
   ProjectManager::Listener.

                This object provides interface for class \b StudioPalette.
                StudioPaletteTreeViewer is a \b QTreeWidget with three root item
   related to level palette
                folder and current project palette folder, the three root folder
   of \b StudioPalette.
*/
class DVAPI StudioPaletteTreeViewer final : public QTreeWidget,
                                            public StudioPalette::Listener,
                                            public TProjectManager::Listener {
  Q_OBJECT

  TPaletteP m_currentPalette;

  PalettesScanPopup *m_palettesScanPopup;
  QTreeWidgetItem *m_dropItem;
  TPaletteHandle *m_levelPaletteHandle;
  TPaletteHandle *m_studioPaletteHandle;
  TXsheetHandle *m_xsheetHandle;
  TXshLevelHandle *m_currentLevelHandle;

  QIcon m_folderIcon;
  QIcon m_levelPaletteIcon;
  QIcon m_studioPaletteIcon;
  // keep the checked item list in order to avoid multiple check
  QSet<QTreeWidgetItem *> m_openedItems;

public:
  StudioPaletteTreeViewer(QWidget *parent, TPaletteHandle *studioPaletteHandle,
                          TPaletteHandle *levelPaletteHandle,
                          TXsheetHandle *xsheetHandle,
                          TXshLevelHandle *currentLevelHandle);
  ~StudioPaletteTreeViewer();

  void setLevelPaletteHandle(TPaletteHandle *paletteHandle);
  TPaletteHandle *getLevelPaletteHandle() const { return m_levelPaletteHandle; }

  void setCurrentLevelHandle(TXshLevelHandle *currentLevelHandle);
  TXshLevelHandle *getCurrentLevelHandle() const {
    return m_currentLevelHandle;
  }

  void setStdPaletteHandle(TPaletteHandle *stdPaletteHandle);
  TPaletteHandle *getStdPaletteHandle() const { return m_studioPaletteHandle; }

  /*!	Overriden from StudioPalette::Listener. */
  void onStudioPaletteTreeChange() override { refresh(); }
  /*!	Overriden from StudioPalette::Listener. */
  void onStudioPaletteMove(const TFilePath &dstPath,
                           const TFilePath &srcPath) override {
    refresh();
  }
  /*!	Overriden from StudioPalette::Listener. */
  void onStudioPaletteChange(const TFilePath &palette) override { refresh(); }

  /*!	Overriden from TProjectManager::Listener. */
  void onProjectSwitched() override { resetProjectPaletteFolder(); }
  /*!	Overriden from TProjectManager::Listener. */
  void onProjectChanged() override { resetProjectPaletteFolder(); }

  TFilePath getCurrentItemPath() { return getItemPath(currentItem()); }

protected slots:
  /*! Refresh all item of three root item in tree and preserve current item. */
  void refresh();
  /*! Refresh item \b item and its children; take path concerning \b item and
                  compare \b StudioPalette folder in path with folder in item.
                  If are not equal add or remove child to current \b item.
     Recall itself
                  for each item child. */
  void refreshItem(QTreeWidgetItem *);

  /*! Delete old project palette item and create the new one.*/
  void resetProjectPaletteFolder();

  void onItemClicked(QTreeWidgetItem *item, int column);
  /*! If item \b item name change update path name in \b StudioPalette. */
  void onItemChanged(QTreeWidgetItem *item, int column);

  void onCurrentItemChanged(QTreeWidgetItem *current,
                            QTreeWidgetItem *previous);
  /*! When expand a tree, prepare the child items of it */
  void onTreeItemExpanded(QTreeWidgetItem *);
  /*! Refresh tree only when this widget has focus*/
  void onRefreshTreeShortcutTriggered();

public slots:
  /*! Create a new \b StudioPalette palette in current item path. */
  void addNewPalette();
  /*! Create a new \b StudioPalette folder in current item path. */
  void addNewFolder();
  /*! Delete all item selected recalling \b deleteItem(). */
  void deleteItems();
  /*! Open a \b PalettesScanPopup. */
  void searchForPalette();
  /*! Recall \b StudioPaletteCmd::loadIntoCurrentPalette. */
  void loadInCurrentPalette();
  void loadInCurrentPaletteAndAdaptLevel();
  /*! Recall \b StudioPaletteCmd::replaceWithCurrentPalette. */
  void replaceCurrentPalette();
  /*! Recall \b StudioPaletteCmd::mergeIntoCurrentPalette. */
  void mergeToCurrentPalette();
  /*! Convert level palette to studio palette. */
  void convertToStudioPalette();

protected:
  /*! Delete \b item path from \b StudioPalette. If item is a not empty
                  folder send a question to know if must delete item or not. */
  void deleteItem(QTreeWidgetItem *item);

  /*! Create and refresh root item: Studio Palette and Cleanup Palette. */
  QTreeWidgetItem *createRootItem(const TFilePath path);
  /*! Return true if \b item match with Studio Palette or Cleanup Palette
   * folder. */
  bool isRootItem(QTreeWidgetItem *item);

  /*! Create a new item related to path \b path. */
  QTreeWidgetItem *createItem(const TFilePath path);
  /*! Return path related to item \b item if \b item exist, otherwise return an
   * empty path \b TFilePath. */
  TFilePath getItemPath(QTreeWidgetItem *);

  /*! Return current item path. */
  TFilePath getCurrentFolderPath();
  /*! Return item identified by \b path; if it doesn't exist return 0. */
  QTreeWidgetItem *getItem(const TFilePath path);
  /*! Return item child of \b parent identified by \b path; if it doesn't exist
   * return 0. */
  QTreeWidgetItem *getFolderItem(QTreeWidgetItem *parent, const TFilePath path);

  void resetDropItem();

  void paintEvent(QPaintEvent *event) override;
  /*! Open a context menu considering current item data role \b Qt::UserRole. */
  void contextMenuEvent(QContextMenuEvent *event) override;
  /*! Add an action to menu \b menu; the action has text \b name and its
                  \b triggered() signal is connetted with \b slot. */
  void createMenuAction(QMenu &menu, const char *id, QString name,
                        const char *slot);
  /*! If button left is pressed start drag and drop. */
  void mouseMoveEvent(QMouseEvent *event) override;
  /*! If path related to current item exist and is a palette execute drag. */
  void startDragDrop();
  /*! Verify drag enter data, if it has an url and it's path is a palette or
     data
                  is a PaletteData accept drag event. */
  void dragEnterEvent(QDragEnterEvent *event) override;
  /*! Find item folder nearest to current position. */
  void dragMoveEvent(QDragMoveEvent *event) override;
  /*! Execute drop event. If dropped palette is in studio palette folder move
                  palette, otherwise copy palette in current folder. */
  void dropEvent(QDropEvent *event) override;
  /*! Set dropItem to 0 and update the tree. */
  void dragLeaveEvent(QDragLeaveEvent *event) override;
};

//=============================================================================
//!	The StudioPaletteViewer class provides an object to view and manage
//! studio palettes.
/*!	Inherits \b QFrame.
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
class DVAPI StudioPaletteViewer final : public QSplitter {
  Q_OBJECT

  StudioPaletteTreeViewer *m_studioPaletteTreeViewer;
  PaletteViewer *m_studioPaletteViewer;

public:
  StudioPaletteViewer(QWidget *parent, TPaletteHandle *studioPaletteHandle,
                      TPaletteHandle *levelPaletteHandle,
                      TFrameHandle *frameHandle, TXsheetHandle *xsheetHandle,
                      TXshLevelHandle *currentLevelHandle);
  ~StudioPaletteViewer();

  /*! In order to save current palette from the tool button in the PageViewer.*/
  TFilePath getCurrentItemPath();

  int getViewMode() const;
  void setViewMode(int mode);
};

#endif  // STUDIOPALETTEVIEWER_H
