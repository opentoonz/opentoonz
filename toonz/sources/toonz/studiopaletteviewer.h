#pragma once

#ifndef STUDIOPALETTEVIEWER_H
#define STUDIOPALETTEVIEWER_H

#include "toonz/studiopalette.h"

#include <QTreeWidget>
#include <QFrame>

// forward declaration
class QLabel;
class PalettesScanPopup;
class TPaletteHandle;
class TSceneHandle;
class TXshLevelHandle;
class TFrameHandle;

//=============================================================================
// StudioPaletteTreeViewer
//-----------------------------------------------------------------------------

class StudioPaletteTreeViewer : public QTreeWidget,
                                public StudioPalette::Listener {
  Q_OBJECT

  PalettesScanPopup *m_palettesScanPopup;
  QTreeWidgetItem *m_dropItem;
  TPaletteHandle *m_levelPltHandle;
  TPaletteHandle *m_stdPltHandle;
  TSceneHandle *m_sceneHandle;
  TXshLevelHandle *m_levelHandle;

public:
  StudioPaletteTreeViewer(QWidget *parent                     = 0,
                          TPaletteHandle *studioPaletteHandle = 0,
                          TPaletteHandle *levelPaletteHandle  = 0);
  ~StudioPaletteTreeViewer();

  void setLevelPaletteHandle(TPaletteHandle *paletteHandle);
  TPaletteHandle *getLevelPaletteHandle() const { return m_levelPltHandle; }

  void setStdPaletteHandle(TPaletteHandle *stdPltHandle);
  TPaletteHandle *getStdPaletteHandle() const { return m_stdPltHandle; }

  void setSceneHandle(TSceneHandle *sceneHandle);
  TSceneHandle *getSceneHandle() const { return m_sceneHandle; }

  void setLevelHandle(TXshLevelHandle *levelHandle);
  TXshLevelHandle *getLevelHandle() const { return m_levelHandle; }

  // Overriden from StudioPalette::Listener
  void onStudioPaletteTreeChange() { refresh(); }
  void onStudioPaletteMove(const TFilePath &dstPath, const TFilePath &srcPath) {
    refresh();
  }
  void onStudioPaletteChange(const TFilePath &palette) { refresh(); }

protected slots:
  void refresh();
  void refreshItem(QTreeWidgetItem *);

  void onItemChanged(QTreeWidgetItem *item, int column);
  void onItemExpanded(QTreeWidgetItem *item);
  void onItemCollapsed(QTreeWidgetItem *item);
  void onItemSelectionChanged();

public slots:
  void addNewPlt();
  void addNewFolder();
  void deleteItem();
  void searchForPlt();
  void loadInCurrentCleanupPlt();
  void replaceCurrentCleanupPlt();
  void loadInCurrentPlt();
  void replaceCurrentPlt();
  void mergeToCurrentPlt();

protected:
  QTreeWidgetItem *createRootItem(const TFilePath path, const QPixmap pix);
  bool isRootItem(QTreeWidgetItem *item);

  QTreeWidgetItem *createItem(const TFilePath path);
  TFilePath getFolderPath(QTreeWidgetItem *);

  TFilePath getCurrentFolderPath();
  QTreeWidgetItem *getItem(const TFilePath path);
  QTreeWidgetItem *getFolderItem(QTreeWidgetItem *parent, const TFilePath path);

  void paintEvent(QPaintEvent *event);
  void contextMenuEvent(QContextMenuEvent *event);
  void createMenuAction(QMenu &menu, QString name, const char *slot);
  void mouseMoveEvent(QMouseEvent *event);
  void startDragDrop();
  void dragEnterEvent(QDragEnterEvent *event);
  void dragMoveEvent(QDragMoveEvent *event);
  void dropEvent(QDropEvent *event);
  void dragLeaveEvent(QDragLeaveEvent *event);

  void hideEvent(QHideEvent *event);
  void showEvent(QShowEvent *event);
};

//=============================================================================
// StudioPaletteViewer
//-----------------------------------------------------------------------------

class StudioPaletteViewer : public QFrame {
  Q_OBJECT

public:
  StudioPaletteViewer(QWidget *parent                     = 0,
                      TPaletteHandle *studioPaletteHandle = 0,
                      TPaletteHandle *levelPaletteHandle  = 0,
                      TSceneHandle *sceneHandle           = 0,
                      TXshLevelHandle *levelHandle        = 0,
                      TFrameHandle *frameHandle           = 0);
  ~StudioPaletteViewer();
};

#endif  // STUDIOPALETTEVIEWER_H
