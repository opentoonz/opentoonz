#pragma once

#ifndef CAST_VIEWER_INCLUDED
#define CAST_VIEWER_INCLUDED

#include <memory>

#include <QSplitter>
#include <QTreeWidget>

#include "dvitemview.h"
#include "tfilepath.h"

class QLabel;
class TLevelSet;
class CastItems;

//-----------------------------------------------------------------------------

class CastTreeViewer : public QTreeWidget, public TSelection {
  Q_OBJECT
  QTreeWidgetItem *m_dropTargetItem;
  TFilePath m_dropFilePath;

  void populateFolder(QTreeWidgetItem *folder);

public:
  CastTreeViewer(QWidget *parent = 0);
  QSize sizeHint() const;

  TFilePath getCurrentFolder() const;
  static TLevelSet *getLevelSet();

  // da TSelection
  bool isEmpty() const { return false; }
  void selectNone() {}
  void enableCommands();

protected:
  void paintEvent(QPaintEvent *);
  void dragEnterEvent(QDragEnterEvent *event);
  void dragMoveEvent(QDragMoveEvent *event);
  void dropEvent(QDropEvent *event);
  void dragLeaveEvent(QDragLeaveEvent *event);
  void resizeEvent(QResizeEvent *);

public slots:
  void onItemChanged(QTreeWidgetItem *item, int column);
  void onFolderChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void onSceneNameChanged();
  void onCastFolderAdded(const TFilePath &path);

  void rebuildCastTree();
  void folderUp();
  void newFolder();
  void deleteFolder();

  void resizeToConts(void);

signals:
  void itemMovedToFolder();
};

//-----------------------------------------------------------------------------

//! La classe si occupa della visualizzazione dello scene cast.
/*!E' figlia di \b QSplitter e contiene un albero che visualizza i folder \b
   m_treeView
   e un widget che consente di visualizzare i file \b m_sceneCastView.
   I suoi widget sono settati tramite un modello del tipo \b SceneCastModel.*/
class CastBrowser : public QSplitter, public DvItemListModel {
  Q_OBJECT

  CastTreeViewer *m_treeViewer;
  QLabel *m_folderName;
  DvItemViewer *m_itemViewer;

  std::unique_ptr<CastItems> m_castItems;

public:
#if QT_VERSION >= 0x050500
  CastBrowser(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  CastBrowser(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~CastBrowser();

  CastItems const &getCastItems() const { return *m_castItems; }

  void sortByDataModel(DataType dataType, bool isDiscendent);

  int getItemCount() const;
  void refreshData();
  QVariant getItemData(int index, DataType dataType, bool isSelected = false);
  QMenu *getContextMenu(QWidget *parent, int index);
  void startDragDrop();
  bool acceptDrop(const QMimeData *data) const;
  bool drop(const QMimeData *data);

  void expose();
  void edit();
  void showFolderContents();
  void viewFile();
  void viewFileInfo();

protected:
  bool dropMimeData(QTreeWidgetItem *parent, int index, const QMimeData *data,
                    Qt::DropAction action);
  Qt::DropActions supportedDropActions() const;

  // void showEvent(QShowEvent*);
  // void hideEvent(QHideEvent*);

protected slots:
  void folderChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void refresh();
};

//-----------------------------------------------------------------------------

#endif
