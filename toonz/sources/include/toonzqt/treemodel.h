#pragma once

#ifndef _FUNCTIONTREEMODEL_
#define _FUNCTIONTREEMODEL_

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QList>
#include <QVariant>
#include <QAbstractItemModel>
#include <QModelIndex>
#include <QTreeView>
#include <QItemDelegate>

//----------------------------------------------------------------------------------------------------
class TreeView;

//! generic tree model. It maintains internally a tree structure
//! of TreeModel::Item's. Each QModelIndex created by the TreeModel
//! points internally to the corrispondent TreeModel::Item
class DVAPI TreeModel : public QAbstractItemModel {
  Q_OBJECT

public:
  class DVAPI Item {
  public:
    Item();
    virtual ~Item();

    Item *getParent() const { return m_parent; }

    int getChildCount() const { return m_childItems.count(); }
    Item *getChild(int row) const { return m_childItems.value(row); }

    Item *appendChild(Item *child);  // gets ownership; returns child

    //! see setChildren() below
    virtual void *getInternalPointer() const { return 0; }

    // Replace old children with new ones.
    // If old and new child internalPointer are equal
    // and not zero then the old child is used and the new one
    // is discarded
    // This method is used to "refresh" the child list.
    // Take ownership of children
    // After the call 'children' contains the new children only
    // Unused old child are queued into m_itemsToDelete. They will eventually
    // be deleted by endRefresh()
    void setChildren(QList<Item *> &children);

    // called by setChildren on "used" old child
    virtual void refresh() {}

    TreeModel *getModel() const { return m_model; }
    void setModel(TreeModel *model) { m_model = model; }

    //! row is the Item index in the parent. (used by QModelIndex)
    int getRow() const { return m_row; }
    void setRow(int row) { m_row = row; }

    //! root_depth == 0; son_depth == parent_depth + 1
    int getDepth() const { return m_depth; }

    bool isOpen() const { return m_opened; }
    void setIsOpen(bool opened) { m_opened = opened; }

    //! used by TreeModel::data(); default implementation provides
    //! open/close folder icons
    virtual QVariant data(int role) const;

    QModelIndex createIndex();

  private:
    // update children data (e.g.: parent, model, depth and row)
    void updateChild(Item *child, int row);
    void updateChildren();  // Note: is not ricursive, i.e. doesn't call itself
                            // on children

    TreeModel *m_model;
    Item *m_parent;
    QList<Item *> m_childItems;
    int m_depth;
    int m_row;
    bool m_opened;

  public:
    // int index() const {return
    // (m_parent)?m_parent->m_childItems.indexOf(const_cast<Item*>(this)):0;}
  };  // class Item

  TreeModel(TreeView *parent = 0);
  virtual ~TreeModel();

  void setExpandedItem(const QModelIndex &index, bool expanded);

  TreeView *getView() { return m_view; }

public slots:
  virtual void onExpanded(const QModelIndex &index);
  virtual void onCollapsed(const QModelIndex &index);

  //! to update the model:
  //!   beginRefresh(),
  //!   Item::setChildren() and/or Item::appendChild() and/or setRootItem(),
  //!   endRefresh().
  void beginRefresh();
  void endRefresh();

  // const QList<Item*> &getItemsToDelete() const {return m_itemsToDelete;}

  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role) const override;

  void setRowHidden(int row, const QModelIndex &parent, bool hide);

protected:
  void setRootItem(Item *rootItem);
  void setRootItem_NoFree(Item *rootItem);
  Item *getRootItem() const { return m_rootItem; }

private:
  Item *m_rootItem;
  QList<Item *> m_itemsToDelete;

protected:
  TreeView *m_view;
};

//----------------------------------------------------------------------------------------------------

class DVAPI TreeView : public QTreeView {
  Q_OBJECT
  bool m_dragging;

public:
  TreeView(QWidget *parent);

protected:
  /*
class Delegate final : public QItemDelegate
{
public:
Delegate(TreeView *parent) : QItemDelegate(parent), m_treeView(parent) {}
bool editorEvent(QEvent *e, QAbstractItemModel *model, const
QStyleOptionViewItem &option, const QModelIndex &index);
private:
TreeView *m_treeView;

};
friend Delegate;
*/

  // virtual void onClick(TreeModel::Item *item, const QPoint &pos, const
  // QStyleOptionViewItem &option) {}
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void resizeEvent(QResizeEvent *) override;

  void setModel(TreeModel *model);

  virtual void onClick(TreeModel::Item *item, const QPoint &itemPos,
                       QMouseEvent *e) {}
  virtual void onDrag(TreeModel::Item *item, const QPoint &itemPos,
                      QMouseEvent *e) {}
  virtual void onRelease() {}
  virtual void onMidClick(TreeModel::Item *item, const QPoint &itemPos,
                          QMouseEvent *e) {}

  virtual void openContextMenu(TreeModel::Item *item, const QPoint &globalPos) {
  }

public slots:
  void resizeToConts(void);
};

#endif
