

#include "toonzqt/gutil.h"
#include "toonzqt/treemodel.h"

#include <QStringList>
#include <QTreeView>
#include <QHeaderView>
#include <QMouseEvent>
#include <qvariant.h>
#include <qicon.h>
#include <qtextedit.h>

#include "tfx.h"
#include <assert.h>

//====================================================================================================
// Item
//----------------------------------------------------------------------------------------------------

TreeModel::Item::Item()
    : m_model(0), m_parent(0), m_depth(0), m_row(0), m_opened(false) {}

//------------------------------------------------------------------------------------------------------------------

TreeModel::Item::~Item() {
  qDeleteAll(m_childItems);
  m_childItems.clear();
  m_model  = 0;
  m_row    = 0;
  m_depth  = 0;
  m_parent = 0;
}

//------------------------------------------------------------------------------------------------------------------

void TreeModel::Item::updateChild(Item *child, int row) {
  assert(m_model);
  child->m_model  = m_model;
  child->m_depth  = m_depth + 1;
  child->m_parent = this;
  child->m_row    = row;
}

//------------------------------------------------------------------------------------------------------------------

void TreeModel::Item::updateChildren() {
  int i;
  for (i = 0; i < m_childItems.size(); i++) updateChild(m_childItems[i], i);
}

//------------------------------------------------------------------------------------------------------------------

TreeModel::Item *TreeModel::Item::appendChild(TreeModel::Item *child) {
  assert(child);
  assert(!m_childItems.contains(child));
  updateChild(child, m_childItems.size());
  m_childItems.append(child);
  return child;
}

//------------------------------------------------------------------------------------------------------------------
/*
void TreeModel::Item::deleteChild(Item *child)
{
  int index = m_childItems.indexOf(child);
  if(index != -1)
  {
    m_childItems.takeAt(index);
    assert(!m_childItems.contains(child));
    // m_childItems is not supposed to contain duplicated entries
    delete child;
    updateChildren();
  }
}
*/

//------------------------------------------------------------------------------------------------------------------
/*
Item* matchItem(Item*item, QList<Item*> &items)
{
  void *itemData = item->getInternalPointer();
  if(!itemData) return 0;
  int i;
  for(i=0;i<items.size();i++)
    if(items.at(i)->getInternalPointer()==itemData)
      return items.at(i);
  return 0;
}
*/
//------------------------------------------------------------------------------------------------------------------

void TreeModel::Item::setChildren(QList<Item *> &newChildren) {
  assert(m_model);
  QModelIndex itemIndex = createIndex();

  // save old children and clear 'm_childItems'
  QList<Item *> oldChildren(m_childItems);
  m_childItems.clear();
  int i;

  // for each child to add
  for (i = 0; i < newChildren.size(); i++) {
    Item *newChild           = newChildren.at(i);
    void *newInternalPointer = newChild->getInternalPointer();
    if (newInternalPointer != 0) {
      // search among old children
      int j;
      for (j = 0; j < oldChildren.size(); j++)
        if (oldChildren.at(j)->getInternalPointer() == newInternalPointer)
          break;
      if (j < oldChildren.size()) {
        Item *oldChild = oldChildren.takeAt(j);
        if (oldChild != newChild) {
          // Found! Delete newChild, remove it from 'newChildren' and
          // update consequently the index
          delete newChild;
          newChildren.takeAt(i);
          i--;
          // use the found child instead of the new one.
          newChild = oldChild;
          oldChild->refresh();
        } else {
          // should never happen; (if it happens this is not a big problem)
          assert(0);
        }
      }
    }
    // add the new child to 'm_childItems'
    m_childItems.push_back(newChild);
  }
  // update children model, row, parent, etc.
  updateChildren();

  // postpone item deletion to avoid the (possible) reuse of the
  // same pointer for the newly created items. (Pointers match is
  // used updating persistent indices. see: TreeModel::endRefresh())
  for (i = 0; i < oldChildren.size(); i++) {
    Item *itemToDelete = oldChildren[i];
    if (!m_model->m_itemsToDelete.contains(itemToDelete))
      m_model->m_itemsToDelete.push_back(itemToDelete);
  }
}

//------------------------------------------------------------------------------------------------------------------

QVariant TreeModel::Item::data(int role) const {
  if (role == Qt::DecorationRole)
    return createQIcon("folder", true);
  else
    return QVariant();
}

//------------------------------------------------------------------------------------------------------------------

QModelIndex TreeModel::Item::createIndex() {
  return m_parent ? m_model->createIndex(m_row, 0, this) : QModelIndex();
}

//====================================================================================================
// TreeModel
//----------------------------------------------------------------------------------------------------

TreeModel::TreeModel(TreeView *parent)
    : QAbstractItemModel(parent), m_rootItem(0), m_view(parent) {}

//------------------------------------------------------------------------------------------------------------------

TreeModel::~TreeModel() { delete m_rootItem; }

//------------------------------------------------------------------------------------------------------------------

void TreeModel::setExpandedItem(const QModelIndex &index, bool expanded) {
  if (m_view) m_view->setExpanded(index, expanded);
}

//------------------------------------------------------------------------------------------------------------------

void TreeModel::beginRefresh() { emit layoutAboutToBeChanged(); }

//------------------------------------------------------------------------------------------------------------------

void TreeModel::endRefresh() {
  QList<QModelIndex> oldIndices, newIndices;
  int i;
  QList<Item *>::iterator it;

  // comment out as no subclass of TreeModel reimplement removeRows() for now
  // and it causes assertion failure on calling beginRemoveRows() when deleting
  // the last column in the xsheet
  /*
  for (i = m_itemsToDelete.size() - 1; i >= 0; i--) {
    int row          = m_itemsToDelete[i]->getRow();
    Item *parentItem = m_itemsToDelete[i]->getParent();
    QModelIndex parentIndex =
        parentItem ? parentItem->createIndex() : QModelIndex();

    beginRemoveRows(parentIndex, row, row);
    removeRows(row, 1, parentIndex);  // NOTE: This is currently doing NOTHING?
  (see
                                  // Qt's manual)
    endRemoveRows();
  }*/

  qDeleteAll(m_itemsToDelete);
  m_itemsToDelete.clear();

  if (!persistentIndexList().empty()) {
    for (i = 0; i < persistentIndexList().size(); i++) {
      QModelIndex oldIndex = persistentIndexList()[i];
      Item *item           = static_cast<Item *>(oldIndex.internalPointer());
      if (item) {
        QModelIndex newIndex = item->createIndex();
        if (oldIndex != newIndex) {
          oldIndices.push_back(oldIndex);
          newIndices.push_back(newIndex);
        }
      }
    }
    changePersistentIndexList(oldIndices, newIndices);
  }

  emit layoutChanged();
}

//------------------------------------------------------------------------------------------------------------------

int TreeModel::columnCount(const QModelIndex &parent) const { return 1; }

//------------------------------------------------------------------------------------------------------------------

Qt::ItemFlags TreeModel::flags(const QModelIndex &index) const {
  if (!index.isValid()) return Qt::ItemFlags();
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//------------------------------------------------------------------------------------------------------------------

QModelIndex TreeModel::index(int row, int column,
                             const QModelIndex &parent) const {
  // column=!0 are not supported
  if (column != 0) return QModelIndex();

  Item *parentItem =
      parent.isValid() ? (Item *)(parent.internalPointer()) : m_rootItem;
  // if m_rootItem has not been defined yet. (It should not happen, but just in
  // case)
  if (!parentItem) return QModelIndex();

  int childCount = parentItem->getChildCount();
  if (row < 0 || row >= childCount) return QModelIndex();

  Item *item = parentItem->getChild(row);
  if (!item) return QModelIndex();  // it should never happen
  return item->createIndex();
}

//------------------------------------------------------------------------------------------------------------------

QModelIndex TreeModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) return QModelIndex();

  Item *item = (Item *)index.internalPointer();

  TreeModel::Item *parentItem = item->getParent();

  if (!parentItem || parentItem == m_rootItem) return QModelIndex();

  return parentItem->createIndex();
}

//------------------------------------------------------------------------------------------------------------------

int TreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 0) return 0;

  if (!parent.isValid())
    return m_rootItem ? m_rootItem->getChildCount() : 0;
  else
    return ((Item *)(parent.internalPointer()))->getChildCount();
}

//------------------------------------------------------------------------------------------------------------------

void TreeModel::onExpanded(const QModelIndex &index) {
  if (!index.isValid()) return;

  Item *item = (Item *)(index.internalPointer());
  item->setIsOpen(true);
}

//---------------------------------------------------------------------------------------------------------------

void TreeModel::onCollapsed(const QModelIndex &index) {
  if (!index.isValid()) return;

  Item *item = (Item *)(index.internalPointer());
  item->setIsOpen(false);
}

//---------------------------------------------------------------------------------------------------------------

QVariant TreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();
  Item *item = static_cast<Item *>(index.internalPointer());
  return item->data(role);
}

//---------------------------------------------------------------------------------------------------------------

void TreeModel::setRootItem(Item *rootItem) {
  if (rootItem == m_rootItem) return;
  delete m_rootItem;
  m_rootItem = rootItem;
  if (m_rootItem) m_rootItem->setModel(this);
}

// postpone freeing, so existing items can be referenced while refreshing.
void TreeModel::setRootItem_NoFree(Item *rootItem) {
  if (rootItem == m_rootItem) return;
  m_rootItem = rootItem;
  if (m_rootItem) m_rootItem->setModel(this);
}

//---------------------------------------------------------------------------------------------------------------

void TreeModel::setRowHidden(int row, const QModelIndex &parent, bool hide) {
  if (m_view) m_view->setRowHidden(row, parent, hide);
}

//====================================================================================================
// TreeView
//----------------------------------------------------------------------------------------------------

TreeView::TreeView(QWidget *parent) : QTreeView(parent), m_dragging(false) {
  header()->hide();
  setUniformRowHeights(true);
  setIconSize(QSize(32, 32));
}

//-----------------------------------------------------------------------------

//! Resizes viewport to contents
void TreeView::resizeToConts(void) { resizeColumnToContents(0); }

//-----------------------------------------------------------------------------

void TreeView::resizeEvent(QResizeEvent *event) {
  resizeColumnToContents(0);
  QTreeView::resizeEvent(event);
}

//----------------------------------------------------------------------------------------------------------------

void TreeView::setModel(TreeModel *model) {
  QTreeView::setModel(model);
  disconnect();

  connect(this, SIGNAL(expanded(const QModelIndex &)), model,
          SLOT(onExpanded(const QModelIndex &)));
  connect(this, SIGNAL(collapsed(const QModelIndex &)), model,
          SLOT(onCollapsed(const QModelIndex &)));
  // setItemDelegate(new Delegate(this));

  // Connect all possible changes that can alter the
  // bottom horizontal scrollbar to resize contents...
  connect(this, SIGNAL(expanded(const QModelIndex &)), this,
          SLOT(resizeToConts()));

  connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
          SLOT(resizeToConts()));

  connect(this->model(), SIGNAL(layoutChanged()), this, SLOT(resizeToConts()));
}

//----------------------------------------------------------------------------------------------------------------

void TreeView::mouseDoubleClickEvent(QMouseEvent *) {
  // ignore double click!
}

void TreeView::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::RightButton) QTreeView::mousePressEvent(e);
  QModelIndex index = indexAt(e->pos());
  if (index.isValid()) {
    TreeModel::Item *item =
        static_cast<TreeModel::Item *>(index.internalPointer());
    QRect itemRect = visualRect(index);
    QPoint itemPos = e->pos() - itemRect.topLeft();
    if (e->button() == Qt::RightButton) {
      if (selectionMode() != QAbstractItemView::ExtendedSelection)
        setCurrentIndex(item->createIndex());
      onClick(item, itemPos, e);
      openContextMenu(item, e->globalPos());
    } else if (e->button() == Qt::LeftButton) {
      m_dragging = true;
      setMouseTracking(true);
      onClick(item, itemPos, e);
    }
    // for drag & drop
    else if (e->button() == Qt::MiddleButton) {
      m_dragging = true;
      setMouseTracking(true);
      onMidClick(item, itemPos, e);
    }
  }
}

//----------------------------------------------------------------------------------------------------------------

void TreeView::mouseMoveEvent(QMouseEvent *e) {
  QTreeView::mouseMoveEvent(e);
  if (m_dragging) {
    QModelIndex index = indexAt(e->pos());
    if (index.isValid()) {
      TreeModel::Item *item =
          static_cast<TreeModel::Item *>(index.internalPointer());
      QRect itemRect = visualRect(index);
      QPoint itemPos = e->pos() - itemRect.topLeft();
      onDrag(item, itemPos, e);
    }
  }
}

//----------------------------------------------------------------------------------------------------------------

void TreeView::mouseReleaseEvent(QMouseEvent *e) {
  QTreeView::mouseReleaseEvent(e);
  if (m_dragging) {
    m_dragging = false;
    setMouseTracking(false);
    onRelease();
  }
}

//----------------------------------------------------------------------------------------------------------------

/*
bool TreeView::Delegate::editorEvent(QEvent *e, QAbstractItemModel
*abstractModel, const QStyleOptionViewItem &option, const QModelIndex &index)
{
  if(e->type() != QEvent::MouseButtonPress) return false;
  TreeModel *model = dynamic_cast<TreeModel *>(abstractModel);
  if(!model || !index.isValid()) return false;

  TreeModel::Item *item = static_cast<TreeModel::Item
*>(index.internalPointer());
  QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(e);
  QPoint pos = mouseEvent->pos();

  m_treeView->onClick(item, pos, option);
  return true;
}
*/
