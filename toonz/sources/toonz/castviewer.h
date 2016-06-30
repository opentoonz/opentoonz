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

class CastTreeViewer final : public QTreeWidget, public TSelection {
  Q_OBJECT
  QTreeWidgetItem *m_dropTargetItem;
  TFilePath m_dropFilePath;

  void populateFolder(QTreeWidgetItem *folder);

public:
  CastTreeViewer(QWidget *parent = 0);
  QSize sizeHint() const override;

  TFilePath getCurrentFolder() const;
  static TLevelSet *getLevelSet();

  // da TSelection
  bool isEmpty() const override { return false; }
  void selectNone() override {}
  void enableCommands() override;

protected:
  void paintEvent(QPaintEvent *) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void resizeEvent(QResizeEvent *) override;

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
class CastBrowser final : public QSplitter, public DvItemListModel {
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

  void sortByDataModel(DataType dataType, bool isDiscendent) override;

  int getItemCount() const override;
  void refreshData() override;
  QVariant getItemData(int index, DataType dataType,
                       bool isSelected = false) override;
  QMenu *getContextMenu(QWidget *parent, int index) override;
  void startDragDrop() override;
  bool acceptDrop(const QMimeData *data) const override;
  bool drop(const QMimeData *data) override;

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
