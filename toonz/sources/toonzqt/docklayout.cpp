

#include "docklayout.h"
#include "docktabstrip.h"
#include "tdockwindows.h"
#include "toonzqt/gutil.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QVariant>

#include <assert.h>
#include <math.h>
#include <algorithm>

#include <QTextStream>
#include <QApplication>
#include <QDesktopWidget>

//========================================================
// TO DO:
//  * Use the macro QWIDGETSIZE_MAX for the maximum settable size for a widget
//      => After that, it would be enough to truncate all sums that exceed that
//      value... Anyway...
//  * Recalculating extremal sizes is useless in resize events... Try to cut
//  such things - optimize!
//      => However, it generally makes sense for redistribute to recalculate
//      them - maybe we could make a redistribute with a bool input.
//      If the containing operation cannot change the
//      extremal sizes, then set false..
//  * Implement stretch factors
//  * Hiding docked windows...? Very tricky...
//  * A dock widget might not only have docked and floating states... What
//  happens if it is a subwindow not docked in a dockLayout??
//  * Split tdockwindows.h into tmainwindow.h and tdockwidget.h, as in Qt?
//  * Moving or performing operations on a dockWidget in its current state is
//  not safe if it is not assigned to a DockLayout!! However, it can be
//  assumed that the parent of a DockWidget is not even a QWidget
//  that implements a DockLayout. This assumption may break in a specific
//  implementation like TDockWidget?
//      Example: see calculateDockPlaceholders on drag, it gets called anyway...
//  * Does it make sense to put DockLayout and DockWidget in DVAPI? Perhaps if I
//  defined appropriate inlines in TDockWidget...
//  * What is contained in DockSeparator::mousePress and mouseMove should be
//  made public. Also the geometry of the regions could be
//  user-editable... but first stretch factors should be done!!
//      > I.e., if you explicitly want to set the geometry of a region, you
//      could do that by setting
//        that region's stretch factor to infinity (or almost), and the others
//        to 1...
//  * There should be a way to specify the position of separators from code?
//    Tricky. However, now it's enough to specify the geometry of widgets before
//    docking; redistribute tries to adapt.
//  * It often happens to consider the hypothetical new root of the structure.
//  Why not put it directly??
//  X It is not possible to cover all docking possibilities with the current
//  system, even if it is more extensive than Qt's.
//      Example:           |
//                    -----|
//                      |  |        => !!
//                      |-----
//                      |
//========================================================

//------------------------
//    Geometry inlines
//------------------------

// QRectF::toRect seems to work in the commented way inside the following
// function. Of course that way the rect borders are
// *not* approximated to the nearest integer coordinates:
//  e.g.:   topLeft= (1/3, 1/3); width= 4/3, height= 4/3 => left= top= right=
//  bottom= 0.
inline QRect toRect(const QRectF &rect) {
  // return QRect(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()),
  // qRound(rect.height()));
  return QRect(rect.topLeft().toPoint(),
               rect.bottomRight().toPoint() -= QPoint(1, 1));
}

//-----------------
//    Dock Layout
//-----------------

DockLayout::DockLayout()
    : m_maximizedDock(0)
    , m_decoAllocator(new DockDecoAllocator())
    , m_tabMergePreview(0)
    , m_tabMergeTargetRegion(0) {}

//-------------------------------------

DockLayout::~DockLayout() {
  hideTabMergePreview();
  delete m_tabMergePreview;
  m_tabMergePreview = 0;

  // Dock widgets may outlive this layout during application shutdown. Clear
  // their back-pointer so ~DockWidget does not call into a destroyed layout.
  for (unsigned int i = 0; i < m_items.size(); ++i) {
    if (DockWidget *dw = static_cast<DockWidget *>(m_items[i]->widget()))
      dw->m_parentLayout = 0;
  }

  // Deleting Regions (separators are Widgets with parent, so they are
  // recursively deleted)
  unsigned int i;
  for (i = 0; i < m_regions.size(); ++i) delete m_regions[i];

  // Deleting dockWidgets
  for (i = 0; i < m_items.size(); ++i) {
    // delete m_items[i]->widget();
    delete m_items[i];
  }

  // Delete deco allocator
  delete m_decoAllocator;
}

//-------------------------------------

int DockLayout::count() const { return m_items.size(); }

//-------------------------------------

void DockLayout::addItem(QLayoutItem *item) {
  DockWidget *addedItem = dynamic_cast<DockWidget *>(item->widget());

  // Ensure that added item is effectively a DockWidget type;
  assert(addedItem);

  // Check if item is already under layout's control. If so, quit.
  if (find(addedItem)) return;

  // Force reparenting to unify the coordinate system for all items.
  // the same geometry() reference. Also store parentLayout for convenience.
  addedItem->m_parentLayout = this;
  addedItem->setParent(parentWidget());

  // Remember that reparenting a widget produces a window flags reset if the new
  // parent is not the current one (see Qt's manual). So, first reassign
  // standard floating flags, then call for
  // custom appearance (which may eventually reassign the flags).
  addedItem->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  addedItem->setFloatingAppearance();

  m_items.push_back(item);
}

//-------------------------------------

QLayoutItem *DockLayout::takeAt(int idx) {
  if (idx < 0 || idx >= (int)m_items.size()) return 0;

  QLayoutItem *item = m_items[idx];
  DockWidget *dw    = static_cast<DockWidget *>(item->widget());

  // If docked, undock item
  if (!dw->isFloating()) undockItem(dw);

  // Reset item's parentLayout
  dw->m_parentLayout = 0;

  m_items.erase(m_items.begin() + idx);

  return item;
}

//-------------------------------------

QLayoutItem *DockLayout::itemAt(int idx) const {
  if (idx >= (int)m_items.size()) return 0;
  return m_items[idx];
}

//-------------------------------------

QWidget *DockLayout::widgetAt(int idx) const { return itemAt(idx)->widget(); }

//-------------------------------------

QSize DockLayout::minimumSize() const {
  if (!m_regions.empty()) {
    Region *r = m_regions.front();
    r->calculateExtremalSizes();
    return QSize(r->m_minimumSize[0], r->m_minimumSize[1]);
  }

  return QSize(0, 0);
}

//-------------------------------------

QSize DockLayout::maximumSize() const {
  if (!m_regions.empty()) {
    Region *r = m_regions.front();
    r->calculateExtremalSizes();
    return QSize(r->m_maximumSize[0], r->m_maximumSize[1]);
  }

  return QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

//-------------------------------------

QSize DockLayout::sizeHint() const {
  QSize s(0, 0);
  int n = m_items.size();
  if (n > 0) s = QSize(100, 70);  // start with a nice default size
  int i = 0;
  while (i < n) {
    QLayoutItem *o = m_items[i];
    s              = s.expandedTo(o->sizeHint());
    ++i;
  }
  return s + n * QSize(spacing(), spacing());

  // return QSize(0,0);
}

//----------------------
//    Custom methods
//----------------------

QWidget *DockLayout::containerOf(QPoint point) const {
  // Search among regions, from leaf regions to root.
  int i;
  unsigned int j;
  for (i = m_regions.size() - 1; i >= 0; --i) {
    Region *currRegion = m_regions[i];

    if (currRegion->hasTabGroup() && currRegion->tabStripContainer() &&
        currRegion->tabStripContainer()->geometry().contains(point))
      return currRegion->tabStripContainer();

    DockWidget *item = currRegion->getItem();
    if (!item && currRegion->hasTabGroup()) item = currRegion->activeTab();

    // First check if item contains it
    if (item && item->geometry().contains(point)) return item;

    // Then, search among separators
    for (j = 0; j < currRegion->separators().size(); ++j)
      if (currRegion->separator(j)->geometry().contains(point))
        return currRegion->separator(j);
  }

  return 0;
}

//-------------------------------------

void DockLayout::setMaximized(DockWidget *item, bool state) {
  if (item && state != item->m_maximized) {
    if (state) {
      // Maximize
      if (m_maximizedDock) {
        // If maximized already exists, normalize it
        Region *r = find(m_maximizedDock);
        m_maximizedDock->setGeometry(toRect(r->getGeometry()));
        m_maximizedDock->m_maximized = false;
      }

      // Now, attempt requested item maximization
      QSize minimumSize = item->minimumSize();
      QSize maximumSize = item->maximumSize();

      if (contentsRect().width() > minimumSize.width() &&
          contentsRect().height() > minimumSize.height() &&
          contentsRect().width() < maximumSize.width() &&
          contentsRect().height() < maximumSize.height()) {
        // Maximization succeeds
        item->setGeometry(contentsRect());
        item->raise();
        item->m_maximized = true;
        m_maximizedDock   = item;

        // A maximized panel covers the tab strip of the group it belongs to,
        // so it needs its own title bar back to stay double-clickable.
        restorePanelTitleBar(item);

        // Hide all the other docked widgets (no need to update them. Moreover,
        // doing so
        // could eventually result in painting over the newly maximized widget)
        DockWidget *currWidget;
        for (int i = 0; i < count(); ++i) {
          currWidget = (DockWidget *)itemAt(i)->widget();
          if (currWidget != item && !currWidget->isFloating())
            currWidget->hide();
        }
      }
    } else {
      // Normalize
      Region *r = find(m_maximizedDock);
      if (r) m_maximizedDock->setGeometry(toRect(r->getGeometry()));

      m_maximizedDock->m_maximized = false;
      m_maximizedDock              = 0;

      restoreDockedWidgetVisibility(item);

      // The title bar shown while maximized goes back to being hidden when
      // the panel returns into a tab group.
      if (r && r->hasTabGroup()) updateTabVisibility(r);
    }
  }
}

//======================================
//      Layout Geometry Handler
//======================================

//! NOTE: This method is currently unused by DockLayout implementation...
void DockLayout::setGeometry(const QRect &rect) {
  // Just pass the info to the widget (it's somehow necessary...)
  QLayout::setGeometry(rect);
}

//--------------------------------------------------------------------------

//! Defines cursors for separators of the layout: if it is not possible to
//! move a separator, its cursor must be an arrow.
void DockLayout::updateSeparatorCursors() {
  Region *r, *child;

  unsigned int i, j;
  int jInt, k;
  for (i = 0; i < m_regions.size(); ++i) {
    r = m_regions[i];
    if (r->hasTabGroup()) continue;

    const int childCount     = static_cast<int>(r->getChildList().size());
    const int separatorCount = static_cast<int>(r->separators().size());
    if (childCount == 0 || separatorCount == 0) continue;

    bool orientation = r->getOrientation();

    // If region geometry is minimal or maximal, its separators are blocked
    QSize size = toRect(r->getGeometry()).size();
    bool isExtremeSize =
        (orientation == Region::horizontal)
            ? size.width() == r->getMinimumSize(Region::horizontal) ||
                  size.width() == r->getMaximumSize(Region::horizontal)
            : size.height() == r->getMinimumSize(Region::vertical) ||
                  size.height() == r->getMaximumSize(Region::vertical);
    if (isExtremeSize) {
      for (j = 0; j < static_cast<unsigned int>(separatorCount); ++j)
        r->separator(j)->setCursor(Qt::ArrowCursor);
      continue;
    }

    // Arrowize all separators as long as the preceding region has equal
    // maximum and minimum sizes
    for (j = 0; j < static_cast<unsigned int>(separatorCount); ++j) {
      child = r->childRegion(j);

      if (child->getMaximumSize(orientation) ==
          child->getMinimumSize(orientation))
        r->separator(j)->setCursor(Qt::ArrowCursor);
      else
        break;
    }

    jInt = static_cast<int>(j);
    // The same as above in reverse order
    k = std::min(childCount - 1, separatorCount);
    for (; k > jInt; --k) {
      child = r->childRegion(k);

      if (child->getMaximumSize(orientation) ==
          child->getMinimumSize(orientation))
        r->separator(k - 1)->setCursor(Qt::ArrowCursor);
      else
        break;
    }

    // Middle separators have a split cursor
    Qt::CursorShape shape = (orientation == Region::horizontal)
                                ? Qt::SplitHCursor
                                : Qt::SplitVCursor;
    for (; jInt < k; ++jInt) r->separator(jInt)->setCursor(shape);
  }
}

//--------------------------------------------------------------------------

//! Applies Regions geometry to dock widgets and separators.
void DockLayout::applyGeometry() {
  unsigned int i, j;
  const int stripHeight = tabStripHeight();
  for (i = 0; i < m_regions.size(); ++i) {
    Region *r                             = m_regions[i];
    const std::deque<Region *> &childList = r->getChildList();
    std::deque<DockSeparator *> &sepList  = r->m_separators;

    if (r->hasTabGroup()) {
      QRect regionRect = toRect(r->getGeometry());
      if (r->tabStripContainer()) {
        r->tabStripContainer()->setGeometry(
            QRect(regionRect.left(), regionRect.top(), regionRect.width(),
                  stripHeight));
        r->tabStripContainer()->show();
        r->tabStripContainer()->raise();
      }

      QRect contentRect = regionRect;
      contentRect.setTop(regionRect.top() + stripHeight);

      const std::vector<DockWidget *> &tabs = r->tabItems();
      DockWidget *active                    = r->activeTab();
      for (j = 0; j < tabs.size(); ++j) {
        if (tabs[j] == active) {
          tabs[j]->setGeometry(contentRect);
          tabs[j]->show();
        } else {
          // Keep hidden tabs aligned so switching tabs after restore does not
          // jump, and saveState sees a consistent geometry next time.
          tabs[j]->setGeometry(contentRect);
          tabs[j]->hide();
        }
      }
    } else if (m_regions[i]->getItem()) {
      m_regions[i]->getItem()->setGeometry(toRect(m_regions[i]->getGeometry()));
    } else {
      for (j = 0; j < sepList.size(); ++j) {
        QRect leftAdjRect = toRect(childList[j]->getGeometry());
        if (r->getOrientation() == Region::horizontal) {
          leftAdjRect.adjust(0, 0, 1, 0);  // Take adjacent-to topRight pixel
          sepList[j]->setGeometry(QRect(
              leftAdjRect.topRight(), QSize(spacing(), leftAdjRect.height())));
          sepList[j]->m_index = j;
        } else {
          leftAdjRect.adjust(0, 0, 0, 1);
          sepList[j]->setGeometry(QRect(leftAdjRect.bottomLeft(),
                                        QSize(leftAdjRect.width(), spacing())));
          sepList[j]->m_index = j;
        }
      }
    }
  }

  // If there is a maximized widget, reset its geometry to that of the main
  // region
  if (m_maximizedDock) {
    m_maximizedDock->setGeometry(toRect(m_regions[0]->getGeometry()));
    m_maximizedDock->raise();
  }

  // Finally, update separator cursors.
  updateSeparatorCursors();

  if (m_tabMergePreview && m_tabMergeTargetRegion)
    updateTabMergePreviewGeometry();
}

//------------------------------------------------------

//! Single commit point for dock operations: helpers mutate the region tree,
//! then the public operation asks for the geometry work to be done once.
void DockLayout::finishLayoutChange(LayoutUpdate update) {
  if (update == LayoutUpdate::Full)
    redistribute();  // applies geometry itself
  else
    applyGeometry();

  if (update != LayoutUpdate::Geometry && parentWidget())
    parentWidget()->repaint();
}

//------------------------------------------------------

void DockLayout::applyTransform(const QTransform &transform) {
  unsigned int i;
  for (i = 0; i < m_regions.size(); ++i)
    m_regions[i]->setGeometry(transform.mapRect(m_regions[i]->getGeometry()));
}

//------------------------------------------------------
// check if the region will be with fixed width
bool Region::checkWidgetsToBeFixedWidth(std::vector<QWidget *> &widgets,
                                        bool &fromDocking) {
  if (m_item) {
    if (m_item->wasFloating()) {
      fromDocking = true;
      m_item->clearWasFloating();
      return false;
    }
    /*
    if ((m_item->objectName() == "FilmStrip" && m_item->getCanFixWidth()) ||
        m_item->objectName() == "StyleEditor") {
      widgets.push_back(m_item);
      return true;
    } else if (m_item->objectName() == "ToolBar") {
      return true;
    } else
      return false;
    */
    switch (m_item->getFixWidthMode()) {
    case 2:
      widgets.push_back(m_item);
      // fallthrough
    case 1:
      return true;
    default:
      return false;
    }
  }
  if (m_childList.empty()) return false;
  // for horizontal orientation, return true if all items are to be fixed
  if (m_orientation == horizontal) {
    bool ret = true;
    for (Region *childRegion : m_childList) {
      if (!childRegion->checkWidgetsToBeFixedWidth(widgets, fromDocking))
        ret = false;
    }
    return ret;
  }
  // for vertical orientation, return true if at least one item is to be fixed
  else {
    bool ret = false;
    for (Region *childRegion : m_childList) {
      if (childRegion->checkWidgetsToBeFixedWidth(widgets, fromDocking))
        ret = true;
    }
    return ret;
  }
}

//------------------------------------------------------

void DockLayout::redistribute() {
  if (!m_regions.empty()) {
    std::vector<QWidget *> widgets;
    std::vector<QSize> minSizes;
    std::vector<QSize> maxSizes;

    // Recompute extremal region sizes
    // NOTE: This should only be done if a certain flag requires it;
    // otherwise, for things like resize events, it is useless...

    // let's force the width of the film strip / style editor not to change
    // check recursively from the root region, if the widgets can be fixed.
    // it avoids all widgets in horizontal alignment to be fixed, or UI becomes
    // glitchy.
    bool fromDocking = false;
    bool widgetsCanBeFixedWidth =
        !m_regions.front()->checkWidgetsToBeFixedWidth(widgets, fromDocking);
    if (!fromDocking && widgetsCanBeFixedWidth) {
      for (QWidget *widget : widgets) {
        minSizes.push_back(widget->minimumSize());
        maxSizes.push_back(widget->maximumSize());
        widget->setFixedWidth(widget->width());
      }
    }

    m_regions.front()->calculateExtremalSizes();

    int parentWidth  = contentsRect().width();
    int parentHeight = contentsRect().height();

    // Always check main window consistency before effective redistribution. DO
    // NOT ERASE or crashes may occur...
    if (m_regions.front()->getMinimumSize(Region::horizontal) > parentWidth ||
        m_regions.front()->getMinimumSize(Region::vertical) > parentHeight ||
        m_regions.front()->getMaximumSize(Region::horizontal) < parentWidth ||
        m_regions.front()->getMaximumSize(Region::vertical) < parentHeight) {
      // Restore original sizes before returning
      if (!fromDocking && widgetsCanBeFixedWidth) {
        for (int i = 0; i < widgets.size(); ++i) {
          widgets[i]->setMinimumSize(minSizes[i]);
          widgets[i]->setMaximumSize(maxSizes[i]);
        }
      }
      return;
    }

    // Recompute Layout geometry
    m_regions.front()->setGeometry(contentsRect());
    m_regions.front()->redistribute();

    if (!fromDocking && widgetsCanBeFixedWidth) {
      /*
      for (QWidget *widget : widgets) {
        widget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        widget->setMinimumSize(0, 0);
      }
      */
      for (int i = 0; i < widgets.size(); ++i) {
        widgets[i]->setMinimumSize(minSizes[i]);
        widgets[i]->setMaximumSize(maxSizes[i]);
      }
    }
  }

  // Finally, apply Region geometries found
  applyGeometry();
}

//=============================
//    Region implementation
//=============================

Region::~Region() {
  if (m_owner) m_owner->destroyTabStrip(this, StripDeletion::Immediate);

  // Delete separators
  unsigned int i;
  for (i = 0; i < m_separators.size(); ++i) delete m_separators[i];
}

//------------------------------------------------------

Region::Content Region::content() const {
  if (hasTabGroup()) return Content::TabGroup;
  if (hasChildren()) return Content::Split;
  if (m_item) return Content::SinglePanel;
  return Content::Empty;
}

//------------------------------------------------------

DockTabStrip *Region::tabStrip() const { return m_tabStrip; }

//------------------------------------------------------

TabBarContainter *Region::tabStripContainer() const {
  return m_tabStripContainer;
}

//------------------------------------------------------

bool Region::containsPanel(const DockWidget *panel) const {
  if (!panel) return false;
  if (m_item == panel) return true;
  return std::find(m_tabItems.begin(), m_tabItems.end(), panel) !=
         m_tabItems.end();
}

//------------------------------------------------------

DockWidget *Region::activeTab() const {
  if (hasTabGroup()) {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_tabItems.size())
      return m_tabItems[m_activeTabIndex];
    return m_tabItems.front();
  }
  return m_item;
}

//------------------------------------------------------

void Region::setSinglePanel(DockWidget *panel) {
  m_tabItems.clear();
  m_activeTabIndex = 0;
  m_item           = panel;
}

//------------------------------------------------------

void Region::setTabGroup(const std::vector<DockWidget *> &panels,
                         int activeIndex) {
  if (panels.size() < 2) {
    setSinglePanel(panels.empty() ? 0 : panels.front());
    return;
  }

  m_tabItems = panels;
  m_activeTabIndex =
      (activeIndex >= 0 && activeIndex < (int)panels.size()) ? activeIndex : 0;
  m_item = activeTab();
}

//------------------------------------------------------

void Region::appendTab(DockWidget *panel) {
  if (!panel) return;

  std::vector<DockWidget *> panels = m_tabItems;
  if (panels.empty() && m_item) panels.push_back(m_item);
  panels.push_back(panel);

  setTabGroup(panels, (int)panels.size() - 1);
}

//------------------------------------------------------

int Region::removeTab(DockWidget *panel) {
  auto it = std::find(m_tabItems.begin(), m_tabItems.end(), panel);
  if (it == m_tabItems.end()) return -1;

  const int removedIndex = (int)(it - m_tabItems.begin());

  std::vector<DockWidget *> panels = m_tabItems;
  panels.erase(panels.begin() + removedIndex);

  int activeIndex = m_activeTabIndex;
  if (activeIndex > removedIndex) --activeIndex;

  setTabGroup(panels, activeIndex);
  return removedIndex;
}

//------------------------------------------------------

void Region::moveTab(int fromIndex, int toIndex) {
  const int tabCount = (int)m_tabItems.size();
  if (fromIndex < 0 || toIndex < 0 || fromIndex >= tabCount ||
      toIndex >= tabCount || fromIndex == toIndex)
    return;

  DockWidget *active = activeTab();

  DockWidget *moved = m_tabItems[fromIndex];
  m_tabItems.erase(m_tabItems.begin() + fromIndex);
  m_tabItems.insert(m_tabItems.begin() + toIndex, moved);

  auto it = std::find(m_tabItems.begin(), m_tabItems.end(), active);
  m_activeTabIndex =
      (it == m_tabItems.end()) ? 0 : (int)(it - m_tabItems.begin());
  m_item = activeTab();
}

//------------------------------------------------------

void Region::setActiveTabIndex(int index) {
  if (index < 0 || index >= (int)m_tabItems.size()) return;
  m_activeTabIndex = index;
  m_item           = activeTab();
}

//------------------------------------------------------

void Region::adoptTabGroupFrom(Region *source) {
  if (!source || source == this) return;

  const std::vector<DockWidget *> panels = source->m_tabItems;
  const int activeIndex                  = source->m_activeTabIndex;
  TabBarContainter *container            = source->m_tabStripContainer;
  DockTabStrip *strip                    = source->m_tabStrip;

  source->detachTabStrip();
  source->setSinglePanel(0);

  setTabGroup(panels, activeIndex);
  attachTabStrip(container, strip);
}

//------------------------------------------------------

void Region::attachTabStrip(TabBarContainter *container, DockTabStrip *strip) {
  m_tabStripContainer = container;
  m_tabStrip          = strip;
  if (m_tabStrip) m_tabStrip->rebindRegion(this);
}

//------------------------------------------------------

void Region::detachTabStrip() {
  m_tabStripContainer = 0;
  m_tabStrip          = 0;
}

//------------------------------------------------------

//! Inserts DockSeparator \b sep in \b this Region
void Region::insertSeparator(DockSeparator *sep) {
  m_separators.push_back(sep);
}

//------------------------------------------------------

//! Removes a DockSeparator from \b this Region
void Region::removeSeparator() {
  delete m_separators.back();
  m_separators.pop_back();
}

//------------------------------------------------------

void Region::insertSubRegion(Region *subRegion, int idx) {
  m_childList.insert(m_childList.begin() + idx, subRegion);
  subRegion->m_parent      = this;
  subRegion->m_orientation = !m_orientation;
}

//------------------------------------------------------

//! Inserts input \b item before position \b idx. Returns associated new region.
Region *Region::insertItem(DockWidget *item, int idx) {
  Region *newRegion = new Region(m_owner, item);

  insertSubRegion(newRegion, idx);

  return newRegion;
}

//=============================================================

unsigned int Region::find(const Region *subRegion) const {
  unsigned int i;

  for (i = 0; i < m_childList.size(); ++i)
    if (m_childList[i] == subRegion) return i;

  return -1;
}

//------------------------------------------------------

Region *DockLayout::find(DockWidget *item) const {
  unsigned int i, j;

  for (i = 0; i < m_regions.size(); ++i) {
    Region *r = m_regions[i];
    if (r->getItem() == item) return r;

    if (r->hasTabGroup()) {
      const std::vector<DockWidget *> &tabs = r->tabItems();
      for (j = 0; j < tabs.size(); ++j)
        if (tabs[j] == item) return r;
    }
  }

  return 0;
}

//------------------------------------------------------

int DockLayout::tabStripHeight() const { return DockTabStrip::kHeight; }

//------------------------------------------------------

bool DockLayout::supportsTabGrouping(const DockWidget *widget) {
  if (!widget) return false;

  const QVariant optOut = widget->property("canJoinDockTabs");
  if (optOut.isValid()) return optOut.toBool();

  if (widget->getFixWidthMode() == DockWidget::fixed) return false;

  // Fallback for panels that predate the property: bars too thin to hold a
  // tab strip are excluded by their own fixed extent.
  const int maxJoinableBarWidth  = 60;
  const int maxJoinableBarHeight = 48;

  if (widget->minimumWidth() == widget->maximumWidth() &&
      widget->maximumWidth() <= maxJoinableBarWidth)
    return false;

  if (widget->minimumHeight() == widget->maximumHeight() &&
      widget->maximumHeight() <= maxJoinableBarHeight)
    return false;

  return true;
}

//------------------------------------------------------

void DockLayout::ensureTabStrip(Region *region) {
  if (!region || !parentWidget()) return;

  if (!region->m_tabStripContainer) {
    TabBarContainter *container = new TabBarContainter(parentWidget());
    // Reuse the Style Editor / Palette tab styling from the active theme.
    container->setObjectName("TabBarContainer");

    QHBoxLayout *tabLayout = new QHBoxLayout(container);
    tabLayout->setContentsMargins(6, 0, 0, 0);
    tabLayout->setSpacing(0);

    DockTabStrip *strip = new DockTabStrip(this, region, container);
    // Let the strip fill the container so expanding tabs share width equally.
    tabLayout->addWidget(strip, 1);

    container->setFixedHeight(tabStripHeight());
    container->hide();

    region->attachTabStrip(container, strip);
  }
  region->m_tabStrip->syncFromRegion();
}

//------------------------------------------------------

//! Deletes the tab strip of \b region, if any. Deferred deletion is required
//! whenever the strip may be the widget currently dispatching the event that
//! led here (a tab dragged out of its own group).
void DockLayout::destroyTabStrip(Region *region, StripDeletion deletion) {
  if (!region) return;

  TabBarContainter *container = region->m_tabStripContainer;
  region->detachTabStrip();
  if (!container) return;

  if (deletion == StripDeletion::Deferred) {
    container->hide();
    container->deleteLater();
  } else {
    delete container;
  }
}

//------------------------------------------------------

void DockLayout::updateTabVisibility(Region *region) {
  if (!region) return;

  const bool tabbed                     = region->hasTabGroup();
  const std::vector<DockWidget *> &tabs = region->tabItems();
  DockWidget *active                    = region->activeTab();

  for (unsigned int i = 0; i < tabs.size(); ++i) {
    DockWidget *tab = tabs[i];
    tab->setDockedAppearance();

    TDockWidget *tdw = qobject_cast<TDockWidget *>(tab);
    if (tdw && tdw->titleBarWidget())
      tdw->titleBarWidget()->setVisible(!tabbed);

    if (!tabbed) continue;

    if (tab == active) {
      tab->show();
      tab->raise();
    } else {
      tab->hide();
    }
  }

  if (tabbed)
    ensureTabStrip(region);
  else
    destroyTabStrip(region, StripDeletion::Immediate);
}

//------------------------------------------------------

void DockLayout::setActiveTab(Region *region, int index) {
  if (!region || !region->hasTabGroup()) return;

  region->setActiveTabIndex(index);
  updateTabVisibility(region);
  finishLayoutChange(LayoutUpdate::GeometryAndRepaint);
}

//------------------------------------------------------

void DockLayout::reorderTab(Region *region, int fromIndex, int toIndex) {
  if (!region || !region->hasTabGroup()) return;

  region->moveTab(fromIndex, toIndex);
  if (region->m_tabStrip) region->m_tabStrip->syncFromRegion();
  updateTabVisibility(region);
  finishLayoutChange(LayoutUpdate::Geometry);
}

//------------------------------------------------------

void DockLayout::showTabMergePreview(Region *region) {
  if (!parentWidget() || !region) return;

  m_tabMergeTargetRegion = region;
  if (!m_tabMergePreview) {
    // Tool overlay so the frame paints above SubWindow dock panels.
    m_tabMergePreview = new DockTabMergePreview(parentWidget());
  }

  updateTabMergePreviewGeometry();
  m_tabMergePreview->show();
  m_tabMergePreview->raise();
  m_tabMergePreview->update();
}

//------------------------------------------------------

void DockLayout::hideTabMergePreview() {
  m_tabMergeTargetRegion = 0;
  if (m_tabMergePreview) m_tabMergePreview->hide();
}

//------------------------------------------------------

//! The preview outlines the whole region the panel would join, whereas the
//! placeholder it is triggered by only covers the title / tab strip band so
//! that it never overlaps the classic split drop zones.
void DockLayout::updateTabMergePreviewGeometry() {
  if (!m_tabMergePreview || !m_tabMergeTargetRegion || !parentWidget()) return;

  const QRect local = toRect(m_tabMergeTargetRegion->getGeometry());
  m_tabMergePreview->setGeometry(
      QRect(parentWidget()->mapToGlobal(local.topLeft()), local.size()));
}

//------------------------------------------------------

void DockLayout::restorePanelTitleBar(DockWidget *item) {
  if (!item) return;
  TDockWidget *tdw = qobject_cast<TDockWidget *>(item);
  if (tdw && tdw->titleBarWidget()) {
    tdw->titleBarWidget()->setVisible(true);
    tdw->titleBarWidget()->raise();
  }
}

//------------------------------------------------------

void DockLayout::restoreDetachedPanelAppearance(DockWidget *item) {
  if (!item) return;

  restorePanelTitleBar(item);
  item->setDockedAppearance();
  item->show();
}

//------------------------------------------------------

void DockLayout::normalizeSingleDockedPanel(DockWidget *item) {
  if (!item) return;

  item->setWindowFlags(Qt::SubWindow);
  item->setDockedAppearance();
  item->m_floating = false;
  item->onDock(true);

  // setWindowFlags can re-hide children on Windows, so restore the title bar
  // last and show the panel explicitly.
  restorePanelTitleBar(item);
  if (QLayout *l = item->layout()) l->activate();
  item->show();
  item->raise();
  item->update();
}

//------------------------------------------------------

void DockLayout::normalizeTabGroupAppearance(Region *region) {
  if (!region) return;

  const std::vector<DockWidget *> &tabs = region->tabItems();
  for (unsigned int t = 0; t < tabs.size(); ++t) {
    tabs[t]->setDockedAppearance();
    TDockWidget *tdw = qobject_cast<TDockWidget *>(tabs[t]);
    if (tdw && tdw->titleBarWidget()) tdw->titleBarWidget()->setVisible(false);
  }
}

//------------------------------------------------------

void DockLayout::restoreDockedWidgetVisibility(DockWidget *keptVisible) {
  for (int i = 0; i < count(); ++i) {
    DockWidget *currWidget = (DockWidget *)itemAt(i)->widget();
    if (currWidget != keptVisible && !currWidget->isFloating())
      currWidget->show();
  }

  // Inactive members of a tab group must go back to being hidden.
  for (unsigned int i = 0; i < m_regions.size(); ++i)
    if (m_regions[i]->hasTabGroup()) updateTabVisibility(m_regions[i]);

  applyGeometry();
}

//------------------------------------------------------

bool DockLayout::removeFromTabGroup(DockWidget *item, Region *region,
                                    StripDeletion deletion) {
  if (!region || !region->hasTabGroup()) return false;
  if (region->removeTab(item) < 0) return false;

  restoreDetachedPanelAppearance(item);

  if (!region->hasTabGroup()) {
    destroyTabStrip(region, deletion);
    if (DockWidget *remaining = region->getItem())
      normalizeSingleDockedPanel(remaining);
  } else {
    if (region->m_tabStrip) region->m_tabStrip->syncFromRegion();
    updateTabVisibility(region);
  }

  return true;
}

//------------------------------------------------------

bool DockLayout::undockFromTabGroup(DockWidget *item, Region *region,
                                    StripDeletion deletion) {
  if (!removeFromTabGroup(item, region, deletion)) return false;

  item->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  item->setFloatingAppearance();
  item->m_floating = true;
  item->onDock(false);
  // Title bar can be reset by setWindowFlags on Windows.
  restorePanelTitleBar(item);

  setMaximized(item, false);
  finishLayoutChange(LayoutUpdate::Full);
  return true;
}

//------------------------------------------------------

bool DockLayout::detachTabForDrag(DockWidget *item, Region *region,
                                  const QPoint &globalPos,
                                  const QPoint &grabOffsetInTab) {
  if (!item || !region || !parentWidget()) return false;
  if (!region->hasTabGroup()) return false;

  for (int i = 0; i < (int)region->tabItems().size(); ++i) {
    if (region->tabItems()[i] == item && region->activeTabIndex() != i) {
      setActiveTab(region, i);
      break;
    }
  }

  // The tab strip is the widget dispatching the mouse event that led here,
  // so it may only be deleted once that handler has returned.
  if (!undockFromTabGroup(item, region, StripDeletion::Deferred)) return false;

  hideTabMergePreview();

  item->show();
  item->raise();

  // Hand off to DockWidget's native floating drag (same as title-bar undock),
  // keeping the point grabbed on the tab under the cursor.
  item->m_undocking           = false;
  item->m_dragging            = true;
  item->m_dragMouseInitialPos = globalPos;
  item->m_dragInitialPos =
      globalPos - grabOffsetInTab - item->settledDragGripOffset();
  item->move(item->m_dragInitialPos);
  item->grabMouse();

  if (!getMaximized() && !DockingCheck::instance()->isEnabled())
    calculateDockPlaceholders(item);

  return true;
}

//------------------------------------------------------

void DockLayout::addPanelToTabGroup(DockWidget *item, Region *region) {
  if (!item || !region) return;
  if (!supportsTabGrouping(item)) return;

  DockWidget *existing = region->hasTabGroup() ? 0 : region->getItem();

  // Validate before mutating window flags / appearance so a failed join
  // cannot leave the panel without a title bar or docked margins.
  if (!region->hasTabGroup()) {
    if (!existing || existing == item) return;
    if (!supportsTabGrouping(existing)) return;
  }

  item->onDock(true);
  item->setDockedAppearance();
  item->m_floating    = false;
  item->m_wasFloating = true;
  item->setWindowFlags(Qt::SubWindow);

  if (existing) {
    existing->setDockedAppearance();
    existing->setWindowFlags(Qt::SubWindow);
    existing->m_floating = false;
  }

  region->appendTab(item);
  normalizeTabGroupAppearance(region);

  ensureTabStrip(region);
  updateTabVisibility(region);
  item->show();
}

//------------------------------------------------------

bool DockLayout::canMergeAsTabs(DockWidget *item, DockWidget *target) const {
  if (!item || !target || item == target) return false;
  return supportsTabGrouping(item) && supportsTabGrouping(target);
}

//------------------------------------------------------

//! Takes \b item out of whatever holds it now, so that it can be appended to
//! \b destination. Returns false when the panel already belongs there.
bool DockLayout::detachPanelFromCurrentRegion(DockWidget *item,
                                              Region *destination) {
  Region *itemRegion = find(item);
  if (!itemRegion) return true;
  if (itemRegion == destination) return false;

  if (itemRegion->hasTabGroup())
    removeFromTabGroup(item, itemRegion, StripDeletion::Immediate);
  else
    undockItem(item);

  return true;
}

//------------------------------------------------------

void DockLayout::mergePanelsAsTabs(DockWidget *item, DockWidget *target) {
  if (!canMergeAsTabs(item, target)) return;

  Region *targetRegion = find(target);
  if (!targetRegion || targetRegion->containsPanel(item)) return;

  if (!detachPanelFromCurrentRegion(item, targetRegion)) return;

  // Undocking the panel may have collapsed and rebuilt part of the region
  // tree, so the target's region has to be looked up again.
  targetRegion = find(target);
  if (!targetRegion) return;

  addPanelToTabGroup(item, targetRegion);
  hideTabMergePreview();
  finishLayoutChange(LayoutUpdate::Full);
}

//------------------------------------------------------

//! Returns the docked panel whose drag grip lies under \b globalPos.
//! Floating panels are not merge targets.
DockWidget *DockLayout::dockWidgetTitleBarAt(const QPoint &globalPos) const {
  if (!parentWidget()) return 0;

  for (int i = count() - 1; i >= 0; --i) {
    DockWidget *dw = static_cast<DockWidget *>(itemAt(i)->widget());
    if (!dw || dw->isFloating() || !supportsTabGrouping(dw)) continue;

    QPoint localPos = dw->mapFromGlobal(globalPos);
    if (dw->rect().contains(localPos) && dw->isDragGrip(localPos)) return dw;
  }

  return 0;
}

//------------------------------------------------------

//! Calculates possible docking solutions for \b this
//! dock widget. They are stored into the dock widget.

//!\b NOTE: Placeholders are here calculated by decreasing importance;
//! in other words, if two rects are part of the layout, and the first
//! contains the second, placeholders of the first are found before
//! those of the second. This fact may be exploited when selecting an
//! appropriate placeholder for docking.
void DockLayout::calculateDockPlaceholders(DockWidget *item) {
  assert(item);

  // If the DockLayout's owner widget is hidden, avoid
  if (!parentWidget()->isVisible()) return;

  clearRegionPlaceholderReferences();
  item->clearDockPlaceholders();

  if (!m_regions.size()) {
    if (isPossibleInsertion(item, 0, 0)) {
      // Then insert a root placeholder only
      item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
          item, 0, 0, DockPlaceholder::root));
      return;
    }
  }

  // For all regions (and for all insertion index), check if
  // insertion may succeed.
  // NOTE: Insertion chance is just the same for all indexes in a given
  // parent region...

  // First check parentRegion=0 (under a new Root - External cases)
  if (isPossibleInsertion(item, 0, 0)) {
    QRect contRect = contentsRect();
    if (m_regions.front()->getOrientation() == Region::horizontal) {
      item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
          item, 0, 0, DockPlaceholder::top));
      item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
          item, 0, 1, DockPlaceholder::bottom));
    } else {
      item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
          item, 0, 0, DockPlaceholder::left));
      item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
          item, 0, 1, DockPlaceholder::right));
    }
  }

  unsigned int i;
  for (i = 0; i < m_regions.size(); ++i) {
    Region *r = m_regions[i];
    r->m_placeholders.clear();

    if (isPossibleInsertion(item, r, 0)) {
      unsigned int j;
      QRect cellRect;

      // For all indices, insert a placeholder
      if (r->getOrientation() == Region::horizontal) {
        // Left side
        item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
            item, r, 0, DockPlaceholder::left));
        r->m_placeholders.push_back(item->m_placeholders.back());

        // Separators
        for (j = 1; j < r->getChildList().size(); ++j) {
          item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
              item, r, j, DockPlaceholder::sepVert));
          r->m_placeholders.push_back(item->m_placeholders.back());
        }

        // Right side
        item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
            item, r, j, DockPlaceholder::right));
        r->m_placeholders.push_back(item->m_placeholders.back());
      } else {
        // Top side
        item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
            item, r, 0, DockPlaceholder::top));
        r->m_placeholders.push_back(item->m_placeholders.back());

        for (j = 1; j < r->getChildList().size(); ++j) {
          item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
              item, r, j, DockPlaceholder::sepHor));
          r->m_placeholders.push_back(item->m_placeholders.back());
        }

        // Bottom side
        item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
            item, r, j, DockPlaceholder::bottom));
        r->m_placeholders.push_back(item->m_placeholders.back());
      }
    }
  }

  addTabJoinTargets(item);
}

//------------------------------------------------------

void DockLayout::addTabJoinTargets(DockWidget *item) {
  if (!supportsTabGrouping(item)) return;

  for (unsigned int i = 0; i < m_regions.size(); ++i) {
    Region *r = m_regions[i];
    if (r->hasChildren() || r->content() == Region::Content::Empty) continue;
    if (r->containsPanel(item)) continue;
    if (!supportsTabGrouping(r->activeTab())) continue;

    item->m_placeholders.push_back(item->m_decoAllocator->newPlaceBuilt(
        item, r, 0, DockPlaceholder::tabJoinTarget));
  }
}

//------------------------------------------------------

//! Docks input \b item before position \b idx of region \b r. Deals with
//! overall region hierarchy.

//!\b NOTE: Docked items are forcedly shown.
void DockLayout::dockItem(DockWidget *item, DockPlaceholder *place) {
  place->hide();
  item->hide();

  if (place->getAttribute() == DockPlaceholder::tabJoinTarget) {
    Region *region     = place->getParentRegion();
    DockWidget *target = region ? region->activeTab() : 0;
    hideTabMergePreview();
    if (target) mergePanelsAsTabs(item, target);
  } else {
    dockItemPrivate(item, place->m_region, place->m_idx);
    redistribute();
    hideTabMergePreview();
    item->setWindowFlags(Qt::SubWindow);
    item->show();
  }

  parentWidget()->repaint();
}

//------------------------------------------------------

//! Docks input \b item at side \b regionside of \b target dock widget.
//! RegionSide can be Region::left, right, top or bottom.
void DockLayout::dockItem(DockWidget *item, DockWidget *target,
                          int regionSide) {
  Region *targetRegion = find(target);

  short var = regionSide >> 2 * (int)targetRegion->getOrientation();
  bool pos  = regionSide & 0xa;

  item->setWindowFlags(Qt::SubWindow);
  item->show();

  if (var & 0x3) {
    // Side is coherent with orientation => Direct insertion at position 0 or 1
    dockItemPrivate(item, targetRegion, pos);
  } else {
    // Side is not coherent - have to find target's index in parent region
    Region *parentRegion = targetRegion->getParent();
    unsigned int idx =
        parentRegion ? parentRegion->find(targetRegion) + pos : pos;
    dockItemPrivate(item, parentRegion, idx);
  }
}

//------------------------------------------------------

//! Docks input \b item into Region \b r, at position \b idx; returns region
//! corresponding to newly inserted item.

//!\b NOTE: Unlike dockItem(DockWidget*,DockPlaceholder*) and undockItem, this
//! method is supposedly called directly into application code; therefore, no \b
//! redistribution is done after a single dock you are supposed to manually call
//! redistribute() after all widgets have been docked.
Region *DockLayout::dockItem(DockWidget *item, Region *r, int idx) {
  item->setWindowFlags(Qt::SubWindow);
  item->show();
  return dockItemPrivate(item, r, idx);
}

//------------------------------------------------------

void DockLayout::clearRegionPlaceholderReferences() {
  for (unsigned int i = 0; i < m_regions.size(); ++i)
    m_regions[i]->m_placeholders.clear();
}

//------------------------------------------------------

//! Moves the whole tab group of \b region into a new child region, so that
//! the group can be split-docked against as a single unit.
Region *DockLayout::detachTabGroupAsSubRegion(Region *region) {
  if (!region || !region->hasTabGroup()) return 0;

  Region *child = new Region(this);
  child->adoptTabGroupFrom(region);

  return child;
}

//------------------------------------------------------

// Internal docking function. Contains raw docking code, excluded reparenting
// (setWindowFlags)  which may slow down a bit should be done only
// after a redistribute() and a repaint() on real-time docking.
Region *DockLayout::dockItemPrivate(DockWidget *item, Region *r, int idx) {
  // hide minimize button in FlipboolPanel
  item->onDock(true);

  item->setDockedAppearance();
  item->m_floating    = false;
  item->m_wasFloating = true;

  if (!r) {
    // Insert new root region
    Region *newRoot = new Region(this);

    m_regions.push_front(newRoot);

    newRoot->setSize(item->size());

    if (m_regions.size() == 1) {
      newRoot->setItem(item);
      return newRoot;
    }

    newRoot->setOrientation(!m_regions[1]->getOrientation());
    newRoot->insertSubRegion(m_regions[1], 0);

    r = newRoot;
  } else if (r->hasTabGroup()) {
    Region *regionForTabs = detachTabGroupAsSubRegion(r);
    regionForTabs->setSize(toRect(r->getGeometry()).size());
    r->insertSubRegion(regionForTabs, 0);
    m_regions.push_back(regionForTabs);
  } else if (r->getItem()) {
    // Then the Layout gets further subdived - r's item has to be moved
    Region *regionForOldItem = r->insertItem(r->getItem(), 0);
    regionForOldItem->setSize(r->getItem()->size());
    // regionForOldItem->setSize(r->getItem()->frameSize());
    r->setItem(0);
    m_regions.push_back(regionForOldItem);
  }

  Region *newRegion = r->insertItem(item, idx);
  m_regions.push_back(newRegion);
  // Temporarily setting suggested size for newly inserted region
  newRegion->setSize(item->size());

  // Finally, insert a new DockSeparator in parent region r.
  r->insertSeparator(
      m_decoAllocator->newSeparator(this, r->getOrientation(), r));

  return newRegion;
}

//------------------------------------------------------

//! A region is empty, if contains no item and no children.
static bool isEmptyRegion(Region *r) {
  if (r->hasTabGroup()) return false;
  if ((!r->getItem()) && (r->getChildList().size() == 0)) {
    delete r;  // Well, it's a bit improper, but it works...
    return true;
  }
  return false;
}

//------------------------------------------------------

//! Removes input item from region
void Region::removeItem(DockWidget *item) {
  if (item == 0) return;

  unsigned int i;
  for (i = 0; i < m_childList.size(); ++i)
    if (item == m_childList[i]->getItem()) {
      m_childList.erase(m_childList.begin() + i);

      removeSeparator();

      // parent Region may collapse; then move item back to parent and update
      // its parent
      if (m_childList.size() == 1) {
        Region *parent = getParent();
        if (parent) {
          Region *remainingSon = m_childList[0];
          if (!remainingSon->m_childList.size()) {
            if (remainingSon->hasTabGroup()) {
              adoptTabGroupFrom(remainingSon);
            } else {
              // remainingSon is a plain leaf: better keep this and move
              // son's item and childList
              setItem(remainingSon->getItem());
              remainingSon->setItem(0);
            }
          } else {
            // remainingSon is a branch: append remainingSon childList to parent
            // one and sign this and remainingSon nodes for destruction.
            // First find this position in parent
            unsigned int j = parent->find(this);

            parent->m_childList.erase(parent->m_childList.begin() + j);
            parent->m_childList.insert(parent->m_childList.begin() + j,
                                       remainingSon->m_childList.begin(),
                                       remainingSon->m_childList.end());
            parent->m_separators.insert(parent->m_separators.begin() + j,
                                        remainingSon->m_separators.begin(),
                                        remainingSon->m_separators.end());

            // Update remainingSon children's and DockSeparator's parent
            for (j = 0; j < remainingSon->m_childList.size(); ++j)
              remainingSon->m_childList[j]->m_parent = parent;

            for (j = 0; j < remainingSon->m_separators.size(); ++j)
              remainingSon->m_separators[j]->m_parentRegion = parent;

            remainingSon->m_childList.clear();
            remainingSon->m_separators.clear();
          }
        } else {
          // Root case; better keep the remaining child
          m_childList[0]->setParent(0);
        }

        m_childList.clear();
      }

      break;
    }
}

//------------------------------------------------------

//! Undocks \b item and updates geometry.

//!\b NOTE: Window flags are reset to floating appearance (thus hiding the
//! widget). Since the geometry reference changes a geometry() update
//! may be needed - so item's show() is not forced here. You should
//! eventually remember to call it manually after this.
bool DockLayout::undockItem(DockWidget *item) {
  // Find item's region index in m_regions
  Region *itemCarrier = find(item);
  if (!itemCarrier) return false;

  if (itemCarrier->hasTabGroup())
    return undockFromTabGroup(item, itemCarrier, StripDeletion::Immediate);

  Region *parent = itemCarrier->getParent();
  if (parent) {
    int removalIdx = 0;

    // Find removal index in parent's childList
    unsigned int j;
    for (j = 0; j < parent->getChildList().size(); ++j)
      if (parent->getChildList()[j]->getItem() == item) break;

    if (isPossibleRemoval(item, parent, removalIdx))
      parent->removeItem(item);
    else
      return false;
  }

  // Remove region in regions list m_regions.erase(i);
  // Don't - m_regions is cleaned before the end by remove_if
  itemCarrier->setItem(0);

  std::deque<Region *>::iterator j;
  j = std::remove_if(m_regions.begin(), m_regions.end(), isEmptyRegion);
  m_regions.resize(j - m_regions.begin());

  // Update status
  // qDebug("Undock");
  item->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  // NOTE: Using the Qt::Window flag, focus is automatically reassigned, with a
  // slight delay. Using Tool this does not happen.
  // In this case, placeholders must be disabled...

  item->setFloatingAppearance();
  item->m_floating = true;

  // show minimize button in FlipbookPanel
  item->onDock(false);

  setMaximized(item, false);

  redistribute();

  return true;
}

//=============================================================

//! Search for the \b nearest n-ple from a \b target one, under conditions:
//!\b 1) nearest elements belong to \b fixed \b intervals; \b 2) their \b sum is
//!\b fixed too.
static void calculateNearest(std::vector<double> target,
                             std::vector<double> &nearest,
                             std::vector<std::pair<int, int>> intervals,
                             double sum) {
  // Solve Lagrange for constraint (2)
  assert(target.size() == intervals.size());

  unsigned int i;

  double targetSum = 0;
  for (i = 0; i < target.size(); ++i) targetSum += target[i];

  double multiplier = (sum - targetSum) / (double)target.size();

  nearest.resize(target.size());
  for (i = 0; i < target.size(); ++i) nearest[i] = target[i] + multiplier;

  // Now, constraint (1) is not met; however, satisfying also (2) yields a
  // hyperRect on which we must find the nearest point to our above
  // partial solution. In particular, it can be demonstrated that at
  // least one coordinate of the current partial solution is related
  // to the final one (...). This mean that we may have to solve sub-problems of
  // this same kind, with less variable coordinates.

  unsigned int max;
  double distance, maxDistance = 0;

  for (i = 0; i < target.size(); ++i) {
    if (nearest[i] < intervals[i].first || nearest[i] > intervals[i].second) {
      distance   = nearest[i] < intervals[i].first
                       ? intervals[i].first - nearest[i]
                       : nearest[i] - intervals[i].second;
      nearest[i] = nearest[i] < intervals[i].first ? intervals[i].first
                                                   : intervals[i].second;
      if (maxDistance < distance) {
        maxDistance = distance;
        max         = i;
      }
    }
  }

  std::vector<double> newTarget = target;
  std::vector<double> newNearest;
  std::vector<std::pair<int, int>> newIntervals = intervals;

  if (maxDistance) {
    newTarget.erase(newTarget.begin() + max);
    newIntervals.erase(newIntervals.begin() + max);
    sum -= nearest[max];
    calculateNearest(newTarget, newNearest, newIntervals, sum);
    for (i = 0; i < max; ++i) nearest[i] = newNearest[i];
    for (i = max + 1; i < nearest.size(); ++i) nearest[i] = newNearest[i - 1];
  } else
    return;
}

//------------------------------------------------------

//! Equally redistribute separators and children regions' internal geometry
//! according to current subregion sizes.
void Region::redistribute() {
  if (!m_childList.size()) return;

  bool expansion = m_minimumSize[Region::horizontal] > m_rect.width() ||
                   m_minimumSize[Region::vertical] > m_rect.height();

  double regionSize[2];

  // If there is no need to expand this region, maintain current geometry;
  // otherwise, expand at minimum.
  regionSize[Region::horizontal] =
      expansion ? m_minimumSize[Region::horizontal] : m_rect.width();
  regionSize[Region::vertical] =
      expansion ? m_minimumSize[Region::vertical] : m_rect.height();

  // However, expansion in the oriented direction has to take care of parent's
  // consense.
  if (m_parent != 0)
    regionSize[m_orientation] =
        std::min((double)m_parent->m_maximumSize[m_orientation],
                 regionSize[m_orientation]);

  // Now find nearest-to-preferred window sizes, according to size constraints.
  unsigned int i;

  // First build target sizes vector
  std::vector<double> targetSizes(m_childList.size());
  for (i = 0; i < m_childList.size(); ++i) {
    // Assuming preferred sizes are those already present before redistribution.
    targetSizes[i] = (m_orientation == Region::horizontal)
                         ? m_childList[i]->m_rect.width()
                         : m_childList[i]->m_rect.height();
  }

  // Build minimum and maximum size constraints
  std::vector<std::pair<int, int>> sizeIntervals(m_childList.size());
  for (i = 0; i < m_childList.size(); ++i) {
    sizeIntervals[i].first  = m_childList[i]->m_minimumSize[m_orientation];
    sizeIntervals[i].second = m_childList[i]->m_maximumSize[m_orientation];
  }

  // Build width sum
  int separatorWidth = m_owner->spacing();
  double sum =
      regionSize[m_orientation] - (m_childList.size() - 1) * separatorWidth;

  std::vector<double> nearestSizes;
  calculateNearest(targetSizes, nearestSizes, sizeIntervals, sum);

  // NearestSizes stores optimal subregion sizes; calculate their geometries and
  // assign them.
  QPointF topLeftCorner = m_rect.topLeft();
  if (m_orientation == horizontal) {
    for (i = 0; i < m_childList.size(); ++i) {
      QSizeF currSize = QSizeF(nearestSizes[i], regionSize[vertical]);
      m_childList[i]->setGeometry(QRectF(topLeftCorner, currSize));
      topLeftCorner =
          m_childList[i]->getGeometry().topRight() + QPointF(separatorWidth, 0);
    }
  } else {
    for (i = 0; i < m_childList.size(); ++i) {
      QSizeF currSize = QSizeF(regionSize[horizontal], nearestSizes[i]);
      m_childList[i]->setGeometry(QRectF(topLeftCorner, currSize));
      topLeftCorner = m_childList[i]->getGeometry().bottomLeft() +
                      QPointF(0, separatorWidth);
    }
  }

  // Finally, redistribute region's children
  for (i = 0; i < m_childList.size(); ++i) m_childList[i]->redistribute();
}

//------------------------------------------------------

//! Calculates maximum and minimum sizes for each sub-region.
void Region::calculateExtremalSizes() {
  calculateMinimumSize(horizontal, true);
  calculateMinimumSize(vertical, true);
  calculateMaximumSize(horizontal, true);
  calculateMaximumSize(vertical, true);
}

//------------------------------------------------------

//! Calculates minimum occupiable space in \b this region on given \b direction.
//! Also stores cache for it.
int Region::calculateMinimumSize(bool direction, bool recalcChildren) {
  int sumMinSizes = 0, maxMinSizes = 0;

  if (hasTabGroup()) {
    unsigned int t;
    int tabBarExtra = (direction == vertical) ? m_owner->tabStripHeight() : 0;
    for (t = 0; t < m_tabItems.size(); ++t) {
      int w = m_tabItems[t]->minimumSize().width();
      int h = m_tabItems[t]->minimumSize().height();
      if (maxMinSizes < (direction == horizontal ? w : h))
        maxMinSizes = (direction == horizontal ? w : h);
    }
    sumMinSizes = maxMinSizes + tabBarExtra;
  } else if (m_item) {
    sumMinSizes = maxMinSizes = (direction == horizontal)
                                    ? m_item->minimumSize().width()
                                    : m_item->minimumSize().height();
  } else {
    unsigned int i;
    int currMinSize;

    // If required, recalculate children sizes along our direction.
    if (recalcChildren) {
      for (i = 0; i < m_childList.size(); ++i)
        m_childList[i]->calculateMinimumSize(direction, true);
    }

    for (i = 0; i < m_childList.size(); ++i) {
      sumMinSizes += currMinSize = m_childList[i]->getMinimumSize(direction);
      if (maxMinSizes < currMinSize) maxMinSizes = currMinSize;
    }

    // Add separators width
    sumMinSizes += m_separators.size() * m_owner->spacing();
  }

  // If m_orientation is coherent with input direction, minimum occupied space
  // is the sum of childs' minimumSizes. Otherwise, the maximum is taken.
  if (m_orientation == direction) {
    return m_minimumSize[direction] = sumMinSizes;
  } else {
    return m_minimumSize[direction] = maxMinSizes;
  }
}

//------------------------------------------------------

//! Calculates maximum occupiable space in \b this region on given \b direction.
//! Also stores cache for it.
// NOTE: Effectively the dual of calculateMinimumSize(int).
int Region::calculateMaximumSize(bool direction, bool recalcChildren) {
  const int inf = 1000000;

  int sumMaxSizes = 0, minMaxSizes = inf;

  if (hasTabGroup()) {
    unsigned int t;
    int tabBarExtra = (direction == vertical) ? m_owner->tabStripHeight() : 0;
    for (t = 0; t < m_tabItems.size(); ++t) {
      int w = m_tabItems[t]->maximumSize().width();
      int h = m_tabItems[t]->maximumSize().height();
      if (minMaxSizes > (direction == horizontal ? w : h))
        minMaxSizes = (direction == horizontal ? w : h);
    }
    sumMaxSizes = minMaxSizes + tabBarExtra;
  } else if (m_item) {
    sumMaxSizes = minMaxSizes = (direction == horizontal)
                                    ? m_item->maximumSize().width()
                                    : m_item->maximumSize().height();
  } else {
    unsigned int i;
    int currMaxSize;

    // If required, recalculate children sizes along our direction.
    if (recalcChildren) {
      for (i = 0; i < m_childList.size(); ++i)
        m_childList[i]->calculateMaximumSize(direction, true);
    }

    for (i = 0; i < m_childList.size(); ++i) {
      sumMaxSizes += currMaxSize = m_childList[i]->getMaximumSize(direction);
      if (minMaxSizes > currMaxSize) minMaxSizes = currMaxSize;
    }

    // Add separators width
    sumMaxSizes += m_separators.size() * m_owner->spacing();
  }

  // If m_orientation is coherent with input direction, maximum occupied space
  // is the sum of childs' maximumSizes. Otherwise, the minimum is taken.
  if (m_orientation == direction) {
    return m_maximumSize[direction] = sumMaxSizes;
  } else {
    return m_maximumSize[direction] = minMaxSizes;
  }
}

//------------------------------------------------------

bool Region::addItemSize(DockWidget *item) {
  int sepWidth = m_owner->spacing();

  if (m_orientation == horizontal) {
    // Add minimum and maximum horizontal sizes
    m_minimumSize[horizontal] +=
        item->getDockedMinimumSize().width() + sepWidth;
    m_maximumSize[horizontal] +=
        item->getDockedMaximumSize().width() + sepWidth;

    // Make max and min with vertical extremal sizes
    m_minimumSize[vertical] = std::max(m_minimumSize[vertical],
                                       item->getDockedMinimumSize().height());
    m_maximumSize[vertical] = std::min(m_maximumSize[vertical],
                                       item->getDockedMaximumSize().height());
  } else {
    // Vice versa
    m_minimumSize[vertical] += item->getDockedMinimumSize().height() + sepWidth;
    m_maximumSize[vertical] += item->getDockedMaximumSize().height() + sepWidth;

    m_minimumSize[horizontal] = std::max(m_minimumSize[horizontal],
                                         item->getDockedMinimumSize().width());
    m_maximumSize[horizontal] = std::min(m_maximumSize[horizontal],
                                         item->getDockedMaximumSize().width());
  }

  if (m_minimumSize[horizontal] > m_maximumSize[horizontal] ||
      m_minimumSize[vertical] > m_maximumSize[vertical])
    return false;

  // Now, climb parent hierarchy and update extremal sizes. If minSizes get >
  // maxSizes, return failed insertion
  Region *r = m_parent;
  while (r) {
    r->calculateMinimumSize(horizontal, false);
    r->calculateMinimumSize(vertical, false);
    r->calculateMaximumSize(horizontal, false);
    r->calculateMaximumSize(vertical, false);

    if (r->getMinimumSize(horizontal) > r->getMaximumSize(horizontal) ||
        r->getMinimumSize(vertical) > r->getMaximumSize(vertical))
      return false;

    r = r->m_parent;
  }

  return true;
}

//------------------------------------------------------

bool Region::subItemSize(DockWidget *item) {
  int sepWidth = m_owner->spacing();

  if (m_orientation == horizontal) {
    // Subtract minimum and maximum horizontal sizes
    m_minimumSize[horizontal] -= item->minimumSize().width() + sepWidth;
    m_maximumSize[horizontal] -= item->maximumSize().width() + sepWidth;

    // Recalculate opposite extremal sizes (without considering item)
    unsigned int i;
    for (i = 0; i < m_childList.size(); ++i)
      if (m_childList[i]->getItem() != item) {
        m_minimumSize[vertical] = std::max(
            m_minimumSize[vertical], m_childList[i]->getMinimumSize(vertical));
        m_maximumSize[vertical] = std::min(
            m_maximumSize[vertical], m_childList[i]->getMaximumSize(vertical));
      }
  } else {
    // Vice versa
    m_minimumSize[vertical] -= item->minimumSize().height() + sepWidth;
    m_maximumSize[vertical] -= item->maximumSize().height() + sepWidth;

    // Recalculate opposite extremal sizes (without considering item)
    unsigned int i;
    for (i = 0; i < m_childList.size(); ++i)
      if (m_childList[i]->getItem() != item) {
        m_minimumSize[horizontal] =
            std::max(m_minimumSize[horizontal],
                     m_childList[i]->getMinimumSize(horizontal));
        m_maximumSize[horizontal] =
            std::min(m_maximumSize[horizontal],
                     m_childList[i]->getMaximumSize(horizontal));
      }
  }

  if (m_minimumSize[horizontal] > m_maximumSize[horizontal] ||
      m_minimumSize[vertical] > m_maximumSize[vertical])
    return false;

  // Now, climb parent hierarchy and update extremal sizes. If minSizes get >
  // maxSizes, return failed insertion
  Region *r = m_parent;
  while (r) {
    r->calculateMinimumSize(horizontal, false);
    r->calculateMinimumSize(vertical, false);
    r->calculateMaximumSize(horizontal, false);
    r->calculateMaximumSize(vertical, false);

    if (r->getMinimumSize(horizontal) > r->getMaximumSize(horizontal) ||
        r->getMinimumSize(vertical) > r->getMaximumSize(vertical))
      return false;

    r = r->m_parent;
  }

  return true;
}

//=============================================================

//! Checks insertion validity of \b item inside \b parentRegion at position \b
//! insertionIdx.
bool DockLayout::isPossibleInsertion(DockWidget *item, Region *parentRegion,
                                     int insertionIdx) {
  const int inf = 1000000;

  int mainWindowWidth  = contentsRect().width();
  int mainWindowHeight = contentsRect().height();
  std::deque<Region *>::iterator i;
  bool result = true;

  if (m_regions.size()) {
    // Calculate original extremal sizes
    m_regions.front()->calculateExtremalSizes();

    if (parentRegion)  // Common case
    {
      // And update parent region extremal size after hypothetic insertion took
      // place
      result &= parentRegion->addItemSize(item);
    } else  // With root insertion
    {
      // Insertion under new root: simulated by adding with m_regions.front() on
      // its opposite direction;
      bool frontOrientation = m_regions.front()->getOrientation();
      m_regions.front()->setOrientation(!frontOrientation);
      result &= m_regions.front()->addItemSize(item);
      m_regions.front()->setOrientation(frontOrientation);
    }
  }

  QSize rootMinSize;
  QSize rootMaxSize;
  if (m_regions.size()) {
    rootMinSize = QSize(m_regions[0]->getMinimumSize(Region::horizontal),
                        m_regions[0]->getMinimumSize(Region::vertical));
    rootMaxSize = QSize(m_regions[0]->getMaximumSize(Region::horizontal),
                        m_regions[0]->getMaximumSize(Region::vertical));
  } else {
    // New Root
    rootMinSize = item->minimumSize();
    rootMaxSize = item->maximumSize();
  }

  // Finally, check updated root against main window sizes
  if (rootMinSize.width() > mainWindowWidth ||
      rootMinSize.height() > mainWindowHeight ||
      rootMaxSize.width() < mainWindowWidth ||
      rootMaxSize.height() < mainWindowHeight) {
    result = false;
  }

  return result;
}

//------------------------------------------------------

//! Checks insertion validity of \b item inside \b parentRegion at position \b
//! insertionIdx.
bool DockLayout::isPossibleRemoval(DockWidget *item, Region *parentRegion,
                                   int removalIdx) {
  // NOTE: parentRegion is necessarily !=0 or there's no need to check anything
  if (!parentRegion) return true;

  const int inf = 1000000;

  int mainWindowWidth  = contentsRect().width();
  int mainWindowHeight = contentsRect().height();
  std::deque<Region *>::iterator i;
  bool result = true;

  // Calculate original extremal sizes
  m_regions.front()->calculateExtremalSizes();

  // And update parent region extremal size after hypothetic insertion took
  // place
  result &= parentRegion->subItemSize(item);

  QSize rootMinSize;
  QSize rootMaxSize;

  rootMinSize = QSize(m_regions[0]->getMinimumSize(Region::horizontal),
                      m_regions[0]->getMinimumSize(Region::vertical));
  rootMaxSize = QSize(m_regions[0]->getMaximumSize(Region::horizontal),
                      m_regions[0]->getMaximumSize(Region::vertical));

  // Finally, check updated root against main window sizes
  if (rootMinSize.width() > mainWindowWidth ||
      rootMinSize.height() > mainWindowHeight ||
      rootMaxSize.width() < mainWindowWidth ||
      rootMaxSize.height() < mainWindowHeight) {
    result = false;
  }

  return result;
}

//===================
//    Save & Load
//===================

//! Returns the current \b State of the layout. A State is a typedef
//! for a pair containing the (normal) geometries of all layout items,
//! and a string indicating their hierarchycal structure.
DockLayout::State DockLayout::saveState() {
  QString hierarchy;

  // Set save indices so we don't need to find anything
  unsigned int i;
  for (i = 0; i < m_regions.size(); ++i) m_regions[i]->m_saveIndex = i;

  DockWidget *item;
  for (i = 0; i < m_items.size(); ++i) {
    item              = static_cast<DockWidget *>(m_items[i]->widget());
    item->m_saveIndex = i;
  }

  // Write item geometries
  std::vector<QRect> geometries;
  for (i = 0; i < m_items.size(); ++i)
    geometries.push_back(m_items[i]->geometry());

  QTextStream stream(&hierarchy, QIODevice::WriteOnly);

  // Save maximimized Dock index and geometry
  stream << QString::number(m_maximizedDock ? m_maximizedDock->m_saveIndex : -1)
         << " ";
  if (m_maximizedDock) {
    Region *r                                = find(m_maximizedDock);
    geometries[m_maximizedDock->m_saveIndex] = toRect(r->getGeometry());
  }

  // Save regions
  Region *r = rootRegion();
  if (r) {
    stream << QString::number(r->getOrientation()) << " ";
    writeRegion(r, hierarchy);
  }

  return std::pair<std::vector<QRect>, QString>(geometries, hierarchy);
}

//------------------------------------------------------

//! Reads the body of a tab group - the tokens after '{' up to '}' - into
//! \b region, starting at \b pos. Input that does not match the grammar is
//! rejected, not repaired.
bool DockLayout::parseTabGroup(const QStringList &tokens, int &pos,
                               Region *region,
                               std::vector<bool> &alreadyRestored) const {
  std::vector<DockWidget *> panels;
  int activeIndex = -1;
  bool closed     = false;

  for (; pos < tokens.size(); ++pos) {
    const QString &token = tokens[pos];
    if (token == "}") {
      closed = true;
      break;
    }

    bool tokenIsOk = false;
    if (token.startsWith(QLatin1Char('@'))) {
      if (activeIndex >= 0) return false;
      activeIndex = token.mid(1).toInt(&tokenIsOk);
      if (!tokenIsOk || activeIndex < 0) return false;
      continue;
    }

    const int panelIndex = token.toInt(&tokenIsOk);
    if (!tokenIsOk || panelIndex < 0 || panelIndex >= (int)m_items.size() ||
        alreadyRestored[panelIndex])
      return false;

    alreadyRestored[panelIndex] = true;
    panels.push_back(static_cast<DockWidget *>(m_items[panelIndex]->widget()));
  }

  if (!closed || panels.size() < 2) return false;
  if (activeIndex < 0 || activeIndex >= (int)panels.size()) return false;

  region->setTabGroup(panels, activeIndex);
  return true;
}

//------------------------------------------------------

void DockLayout::writeRegion(Region *r, QString &hierarchy) {
  if (r->hasTabGroup()) {
    hierarchy.append("{ ");
    const std::vector<DockWidget *> &tabs = r->tabItems();
    for (unsigned int t = 0; t < tabs.size(); ++t)
      hierarchy.append(QString::number(tabs[t]->m_saveIndex) + " ");
    hierarchy.append("@" + QString::number(r->activeTabIndex()) + " ");
    hierarchy.append("} ");
    return;
  }

  DockWidget *item = static_cast<DockWidget *>(r->getItem());

  // If Region has item, write it.
  if (item) {
    hierarchy.append(QString::number(item->m_saveIndex) + " ");
  } else {
    hierarchy.append("[ ");

    // Scan childList
    unsigned int i, size = r->getChildList().size();
    for (i = 0; i < size; ++i) {
      writeRegion(r->childRegion(i), hierarchy);
    }

    hierarchy.append("] ");
  }
}

//------------------------------------------------------

//! Restores the given internal structure of the layout.

//! This method is intended to restore the
//! geometry of a set of items that was handled by the layout
//! at the time the state was saved. Input are the geometries of
//! the items involved and the dock hierarchy in form of a string.

//!\b IMPORTANT \b NOTE: No check is performed on the item themselves,
//! except for the consistency of their geometrical constraints
//! inside the layout. Furthermore, this method does not ensure the
//! identity of the items involved, assuming that the set of dock
//! widget has ever been left unchanged or completely restored
//! as it were when saved. In particular, their ordering must be preserved.

//! Hierarchy string grammar:
//!   state    := maximizedIndex rootOrientation region
//!   region   := panelIndex | '[' region+ ']' | tabGroup
//!   tabGroup := '{' panelIndex panelIndex+ '@'activeIndex '}'
//! Panel indices refer to m_items and may each appear only once. Anything
//! that does not parse leaves the current layout untouched.
bool DockLayout::restoreState(const State &state) {
  const QStringList tokens = state.second.split(" ", Qt::SkipEmptyParts);
  if (tokens.isEmpty()) return false;

  // Check number of items
  if (m_items.size() != state.first.size()) return false;

  const int itemCount     = (int)m_items.size();
  bool maximizedIndexIsOk = false;
  const int maximizedItem = tokens[0].toInt(&maximizedIndexIsOk);
  if (!maximizedIndexIsOk || maximizedItem < -1 || maximizedItem >= itemCount)
    return false;

  // A panel may only be restored into one region.
  std::vector<bool> alreadyRestored(itemCount, false);

  // Initialize new Regions hierarchy
  std::deque<Region *> newHierarchy;
  const bool expectsHierarchy = tokens.size() > 1;
  bool malformed              = false;

  if (expectsHierarchy) {
    // Scan hierarchy
    Region *r                  = 0;
    bool orientationIsOk       = false;
    const int savedOrientation = tokens[1].toInt(&orientationIsOk);
    if (!orientationIsOk || (savedOrientation != 0 && savedOrientation != 1))
      return false;
    const int orientation = !savedOrientation;

    for (int i = 2; i < tokens.size(); ++i) {
      const QString &token = tokens[i];

      if (token == "]") {
        // End region and get parent
        if (!r) {
          malformed = true;
          break;
        }
        r = r->getParent();
        continue;
      }

      // The grammar allows a single root region.
      if (!r && !newHierarchy.empty()) {
        malformed = true;
        break;
      }

      Region *newRegion = new Region(this);
      newHierarchy.push_back(newRegion);
      newRegion->m_orientation = !orientation;
      if (r) r->insertSubRegion(newRegion, r->getChildList().size());

      if (token == "{") {
        ++i;
        if (!parseTabGroup(tokens, i, newRegion, alreadyRestored)) {
          malformed = true;
          break;
        }
      } else if (token == "[") {
        // Current region has children
        r = newRegion;
      } else {
        bool indexIsOk       = false;
        const int panelIndex = token.toInt(&indexIsOk);
        if (!indexIsOk || panelIndex < 0 || panelIndex >= itemCount ||
            alreadyRestored[panelIndex]) {
          malformed = true;
          break;
        }
        alreadyRestored[panelIndex] = true;
        newRegion->setSinglePanel(
            static_cast<DockWidget *>(m_items[panelIndex]->widget()));
      }
    }

    if (r) malformed = true;  // unterminated '['
  }

  if (malformed || (expectsHierarchy && newHierarchy.empty())) {
    for (unsigned int j = 0; j < newHierarchy.size(); ++j)
      delete newHierarchy[j];
    return false;
  }

  // Check if size constraints are satisfied
  if (!newHierarchy.empty()) newHierarchy[0]->calculateExtremalSizes();

  unsigned int j;
  for (j = 0; j < newHierarchy.size(); ++j) {
    // Check if their extremal sizes are valid
    Region *r = newHierarchy[j];
    if (r->getMinimumSize(Region::horizontal) >
            r->getMaximumSize(Region::horizontal) ||
        r->getMinimumSize(Region::vertical) >
            r->getMaximumSize(Region::vertical)) {
      // If not, deallocate attempted hierarchy and quit
      for (j = 0; j < newHierarchy.size(); ++j) delete newHierarchy[j];
      return false;
    }
  }

  // Else, deallocate old regions and substitute with new ones
  for (j = 0; j < m_regions.size(); ++j) delete m_regions[j];
  m_regions = newHierarchy;

  // Now re-initialize dock widgets' infos.
  const std::vector<QRect> &geoms = state.first;
  DockWidget *item;
  for (j = 0; j < m_items.size(); ++j) {
    item = static_cast<DockWidget *>(m_items[j]->widget());
    item->setGeometry(geoms[j]);
    item->m_maximized = false;
    item->m_saveIndex = j;
  }

  // Allocate region separators before showing docked widgets. Showing a widget
  // can synchronously activate the layout and reach updateSeparatorCursors().
  unsigned int k;
  for (j = 0; j < m_regions.size(); ++j) {
    Region *r = m_regions[j];
    for (k = 1; k < r->m_childList.size(); ++k) {
      r->insertSeparator(
          m_decoAllocator->newSeparator(this, r->getOrientation(), r));
    }
  }

  // Docked widgets are found in hierarchy
  for (j = 0; j < m_regions.size(); ++j) {
    Region *region = m_regions[j];
    if (region->hasTabGroup()) {
      const std::vector<DockWidget *> &tabs = region->tabItems();
      for (unsigned int t = 0; t < tabs.size(); ++t) {
        item = tabs[t];
        item->setWindowFlags(Qt::SubWindow);
        item->setDockedAppearance();
        item->m_floating  = false;
        item->m_saveIndex = -1;
        item->show();
      }
      ensureTabStrip(region);
      updateTabVisibility(region);
    } else if ((item = region->m_item)) {
      item->setWindowFlags(Qt::SubWindow);
      item->setDockedAppearance();
      item->m_floating  = false;
      item->m_saveIndex = -1;
      restorePanelTitleBar(item);
      item->show();
    }
  }

  // Recover available geometry infos
  // QRect availableRect= QApplication::desktop()->availableGeometry();
  int recoverX = 0, recoverY = 0;

  // Deal with floating panels
  for (j = 0; j < m_items.size(); ++j) {
    item = static_cast<DockWidget *>(m_items[j]->widget());

    if (item->m_saveIndex >= 0) {
      // Ensure that floating panels are not placed in
      // unavailable positions
      if ((geoms[j] & QApplication::desktop()->availableGeometry(item))
              .isEmpty())
        item->move(recoverX += 50, recoverY += 50);

      // Set floating appearances
      item->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
      item->setFloatingAppearance();
      item->m_floating = true;
      restorePanelTitleBar(item);
    }
  }

  normalizeRestoredTabGeometries();

  // Calculate regions' geometry starting from leaves (items)
  if (m_regions.size()) m_regions[0]->restoreGeometry();

  // Then, ensure the result is correctly fitting the contents rect
  // redistribute();

  // NOTE: The previous might be tempting to ensure all is right -
  // unfortunately, it may be that the main window's content rect
  // is not yet defined before it is shown the first
  // time (like on MAC), and that is needed to redistribute. So we force the
  // saved values (assuming they are right)...
  applyGeometry();

  // Finally, set maximized dock widget
  if (maximizedItem != -1) {
    item = static_cast<DockWidget *>(m_items[maximizedItem]->widget());

    // Problem: During data loading, the contentsRect of the layout
    // may be off! (see TMainWindow ctor) So, we skip the check performed
    // in setMaximized, and assume it is correct...
    // setMaximized(item, true);

    m_maximizedDock   = item;
    item->m_maximized = true;
    item->raise();

    // Hide all other widgets
    QWidget *currWidget;
    for (int i = 0; i < this->count(); ++i)
      if ((currWidget = itemAt(i)->widget()) != item) currWidget->hide();
  }

  return true;
}

//------------------------------------------------------

//! Hidden tabs may have been saved with geometries from before they were
//! merged, so align every member of a group on its active tab.
void DockLayout::normalizeRestoredTabGeometries() {
  for (unsigned int i = 0; i < m_regions.size(); ++i) {
    Region *region = m_regions[i];
    if (!region->hasTabGroup()) continue;

    DockWidget *active = region->activeTab();
    if (!active) continue;

    const QRect reference                 = active->geometry();
    const std::vector<DockWidget *> &tabs = region->tabItems();
    for (unsigned int t = 0; t < tabs.size(); ++t)
      tabs[t]->setGeometry(reference);
  }
}

//------------------------------------------------------

//! Recalculates the geometry of \b this Region and of its branches,
//! assuming those of 'leaf items' are correct.

//! Regions always tend to keep their geometry by default. However,
//! it may be useful (for example, when restoring the state of a DockLayout)
//! the possibility of recalculating its current geometry directly from the
//! items that are contained in the branches.
void Region::restoreGeometry() {
  // Applying a head-recursive algorithm to update the geometry of a Region
  // after those of its children have been updated
  if (hasTabGroup()) {
    DockWidget *active = activeTab();
    if (!active) return;

    // Panels sit below the tab strip, so a saved panel rect describes the
    // content area rather than the region. Expand upward to get the region.
    QRect g = active->geometry();
    g.setTop(g.top() - m_owner->tabStripHeight());
    setGeometry(g);
    return;
  }

  if (m_item) {
    // Place item's geometry
    setGeometry(m_item->geometry());
    return;
  }

  // First do children
  unsigned int i;
  for (i = 0; i < m_childList.size(); ++i) m_childList[i]->restoreGeometry();

  // Then, update this one: just take the edges of its children.
  unsigned int last = m_childList.size() - 1;
  QPoint topLeft(m_childList[0]->getGeometry().left(),
                 m_childList[0]->getGeometry().top());
  QPoint bottomRight(m_childList[last]->getGeometry().right(),
                     m_childList[last]->getGeometry().bottom());
  setGeometry(QRect(topLeft, bottomRight));

  return;
}

//---------------------------
//    Dock Deco Allocator
//---------------------------

//! Allocates a new DockSeparator with input parameters. This function can be
//! re-implemented
//! to allocate derived DockSeparator classes.
DockSeparator *DockDecoAllocator::newSeparator(DockLayout *owner,
                                               bool orientation,
                                               Region *parentRegion) {
  return new DockSeparator(owner, orientation, parentRegion);
}

//------------------------------------------------------

//! When inheriting a DockLayout class, new custom placeholders gets allocated
//! by this method.
DockPlaceholder *DockDecoAllocator::newPlaceholder(DockWidget *owner, Region *r,
                                                   int idx, int attributes) {
  return new DockPlaceholder(owner, r, idx, attributes);
}

//------------------------------------------------------

// BuildGeometry() method should not be called inside the base constructor -
// because it's a virtual method.
// So we provide this little inline...
DockPlaceholder *DockDecoAllocator::newPlaceBuilt(DockWidget *owner, Region *r,
                                                  int idx, int attributes) {
  DockPlaceholder *res = newPlaceholder(owner, r, idx, attributes);
  res->buildGeometry();
  return res;
}

//------------------------------------------------------

//! Sets current deco allocator to decoAllocator. A default deco allocator is
//! already provided at construction.

//!\b NOTE: DockLayout takes ownership of the allocator.
void DockLayout::setDecoAllocator(DockDecoAllocator *decoAllocator) {
  // Delete old one
  if (m_decoAllocator) delete m_decoAllocator;

  // Place new one
  m_decoAllocator = decoAllocator;
}

//------------------------------------------------------

//! Sets current deco allocator to decoAllocator. A default deco allocator is
//! already provided at construction.

//!\b NOTE: DockWidget takes ownership of the allocator.
void DockWidget::setDecoAllocator(DockDecoAllocator *decoAllocator) {
  // Delete old one
  if (m_decoAllocator) delete m_decoAllocator;

  // Place a copy of new one
  m_decoAllocator = decoAllocator;
}