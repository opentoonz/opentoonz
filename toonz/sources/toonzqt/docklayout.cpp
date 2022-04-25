

#include "docklayout.h"

#include <assert.h>
#include <math.h>
#include <algorithm>

#include <QTextStream>
#include <QApplication>
#include <QDesktopWidget>

//========================================================

// TO DO:
//  * Usa la macro QWIDGETSIZE_MAX per max grandezza settabile per una widget
//      => Dopodiche', basterebbe troncare tutte le somme che eccedono quel
//      valore... Comunque...
//  * Il ricalcolo delle extremal sizes e' inutile nei resize events... Cerca di
//  tagliare cose di questo tipo - ottimizza!
//      => Pero' e' sensato in generale che redistribute possa ricalcolarle -
//      forse si potrebbe fare una redistribute
//         con un bool di ingresso. Se l'operazione contenitrice non puo'
//         cambiare le extremal sizes, allora metti false..
//  * Implementa gli stretch factors

//  * Nascondere le finestre dockate...? Molto rognoso...

//  * Una dock widget potrebbe non avere i soli stati docked e floating... Che
//  succede se e' una subwindow
//    non dockata in un dockLayout??
//  * Spezzare tdockwindows.h in tmainwindow.h e tdockwidget.h, come in Qt?
//  * Muovere o fare operazioni su una dockWidget allo stato attuale non e'
//  sicuro se quella non e' assegnata ad
//    un DockLayout!! Comunque, si puo' assumere che il parente di una
//    DockWidget non sia nemmeno una QWidget
//    che implementa un DockLayout. Questa cosa puo' venire meno in
//    un'implementazione specifica come TDockWidget?
//      Esempio: vedi calculateDockPlaceholders ad un drag, viene lanciato
//      comunque...
//  * Ha senso mettere DockLayout e DockWidget nella DVAPI? Forse se definissi
//  delle inline opportune in TDockWidget...
//  * Quanto contenuto in DockSeparator::mousePress e mouseMove dovrebbe essere
//  reso pubblico. Anche la geometria
//    delle regioni potrebbe essere editabile dall'utente... ma prima si
//    dovrebbero fare gli stretch factors!!
//      > Ossia: se si vuole esplicitamente settare la geometria di una regione,
//      si potrebbe fare che quella
//        si prende stretch factor infinito (o quasi), e gli altri 1...
//  * Dovrebbe esistere un modo per specificare la posizione dei separatori da
//  codice?
//    Rognoso. Comunque, ora basta specificare la geometria delle widget prima
//    del dock;
//    la redistribute cerca di adattarsi.
//  * Capita spesso di considerare l'ipotetica nuova radice della struttura.
//  Perche' non metterla direttamente??

//  X Non e' possibile coprire tutte le possibilita' di docking con il sistema
//  attuale, anche se e' comunque piu'
//    esteso di quello di Qt.
//      Esempio:           |
//                    -----|
//                      |  |        => !!
//                      |-----
//                      |

//========================================================

//------------------------
//    Geometry inlines
//------------------------

// QRectF::toRect seems to work in the commented way inside the following
// function.
// Of course that way the rect borders are *not* approximated to the nearest
// integer
// coordinates:
//  e.g.:   topLeft= (1/3, 1/3); width= 4/3, height= 4/3 => left= top= right=
//  bottom= 0.
inline QRect toRect(const QRectF &rect) {
  // return QRect(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()),
  // qRound(rect.height()));
  return QRect(rect.topLeft().toPoint(),
               rect.bottomRight().toPoint() -= QPoint(1, 1));
}

//========================================================

//-----------------
//    Dock Layout
//-----------------

DockLayout::DockLayout()
    : m_maximizedDock(0), m_decoAllocator(new DockDecoAllocator()) {}

//-------------------------------------

DockLayout::~DockLayout() {
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

  // Force reparentation. This is required in order to ensure that all items
  // possess
  // the same geometry() reference. Also store parentLayout for convenience.
  addedItem->m_parentLayout = this;
  addedItem->setParent(parentWidget());

  // Remember that reparenting a widget produces a window flags reset if the new
  // parent is not the current one (see Qt's manual). So, first reassign
  // standard
  // floating flags, then call for custom appearance (which may eventually
  // reassign the flags).
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

//-------------------------------------

//----------------------
//    Custom methods
//----------------------

QWidget *DockLayout::containerOf(QPoint point) const {
  // Search among regions, from leaf regions to root.
  int i;
  unsigned int j;
  for (i = m_regions.size() - 1; i >= 0; --i) {
    Region *currRegion = m_regions[i];
    DockWidget *item   = currRegion->getItem();

    // First check if item contains it
    if (item && item->geometry().contains(point)) return currRegion->getItem();

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

      // Show all other docked widgets
      DockWidget *currWidget;
      for (int i = 0; i < count(); ++i) {
        currWidget = (DockWidget *)itemAt(i)->widget();
        if (currWidget != item && !currWidget->isFloating()) currWidget->show();
      }
    }
  }
}

//======================================================================

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
  int k, jInt;
  for (i = 0; i < m_regions.size(); ++i) {
    r                = m_regions[i];
    bool orientation = r->getOrientation();

    // If region geometry is minimal or maximal, its separators are blocked
    // NOTE: If this update follows only from a dock/undock, this should be
    // disabled 'til 'Otherwise'
    QSize size = toRect(r->getGeometry()).size();
    bool isExtremeSize =
        (orientation == Region::horizontal)
            ? size.width() == r->getMinimumSize(Region::horizontal) ||
                  size.width() == r->getMaximumSize(Region::horizontal)
            : size.height() == r->getMinimumSize(Region::vertical) ||
                  size.height() == r->getMaximumSize(Region::vertical);
    if (isExtremeSize) {
      for (j = 0; j < r->separators().size(); ++j)
        r->separator(j)->setCursor(Qt::ArrowCursor);
      continue;
    }

    // Otherwise...

    // Arrowize all separators as long as the preceding region has equal
    // maximum and minimum sizes
    for (j = 0; j < r->getChildList().size(); ++j) {
      child = r->childRegion(j);

      if (child->getMaximumSize(orientation) ==
          child->getMinimumSize(orientation))
        r->separator(j)->setCursor(Qt::ArrowCursor);
      else
        break;
    }

    jInt = j;
    // The same as above in reverse order
    for (k = r->getChildList().size() - 1; k > jInt; --k) {
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
  // Update docked window's geometries
  unsigned int i, j;
  for (i = 0; i < m_regions.size(); ++i) {
    Region *r                             = m_regions[i];
    const std::deque<Region *> &childList = r->getChildList();
    std::deque<DockSeparator *> &sepList  = r->m_separators;

    if (m_regions[i]->getItem()) {
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
    if ((m_item->objectName() == "FilmStrip" && m_item->getCanFixWidth()) ||
        m_item->objectName() == "StyleEditor") {
      widgets.push_back(m_item);
      return true;
    } else if (m_item->objectName() == "ToolBar") {
      return true;
    } else
      return false;
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

    // Recompute extremal region sizes
    // NOTA: Sarebbe da fare solo se un certo flag lo richiede; altrimenti tipo
    // per resize events e' inutile...

    // let's force the width of the film strip / style editor not to change
    // check recursively from the root region, if the widgets can be fixed.
    // it avoids all widgets in horizontal alignment to be fixed, or UI becomes
    // glitchy.
    bool fromDocking = false;
    bool widgetsCanBeFixedWidth =
        !m_regions.front()->checkWidgetsToBeFixedWidth(widgets, fromDocking);
    if (!fromDocking && widgetsCanBeFixedWidth) {
      for (QWidget *widget : widgets) widget->setFixedWidth(widget->width());
    }

    m_regions.front()->calculateExtremalSizes();

    int parentWidth  = contentsRect().width();
    int parentHeight = contentsRect().height();

    // Always check main window consistency before effective redistribution. DO
    // NOT ERASE or crashes may occur...
    if (m_regions.front()->getMinimumSize(Region::horizontal) > parentWidth ||
        m_regions.front()->getMinimumSize(Region::vertical) > parentHeight ||
        m_regions.front()->getMaximumSize(Region::horizontal) < parentWidth ||
        m_regions.front()->getMaximumSize(Region::vertical) < parentHeight)
      return;

    // Recompute Layout geometry
    m_regions.front()->setGeometry(contentsRect());
    m_regions.front()->redistribute();

    if (!fromDocking && widgetsCanBeFixedWidth) {
      for (QWidget *widget : widgets) {
        widget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        widget->setMinimumSize(0, 0);
      }
    }
  }

  // Finally, apply Region geometries found
  applyGeometry();
}

//======================================================================

//=============================
//    Region implementation
//=============================

Region::~Region() {
  // Delete separators
  unsigned int i;
  for (i = 0; i < m_separators.size(); ++i) delete m_separators[i];
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
  unsigned int i;

  for (i = 0; i < m_regions.size(); ++i)
    if (m_regions[i]->getItem() == item) return m_regions[i];

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

  // Disable all placeholders
  // for(i=0; i<item->m_placeholders.size(); ++i)
  //  item->m_placeholders[i]->setDisabled(true);
}

//------------------------------------------------------

//! Docks input \b item before position \b idx of region \b r. Deals with
//! overall region hierarchy.

//!\b NOTE: Docked items are forcedly shown.
void DockLayout::dockItem(DockWidget *item, DockPlaceholder *place) {
  place->hide();
  item->hide();
  dockItemPrivate(item, place->m_region, place->m_idx);
  redistribute();
  parentWidget()->repaint();
  item->setWindowFlags(Qt::SubWindow);
  item->show();
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
//! corresponding to
//! newly inserted item.

//!\b NOTE: Unlike dockItem(DockWidget*,DockPlaceholder*) and undockItem, this
//! method is supposedly called directly into application code; therefore, no \b
//! redistribution
//! is done after a single dock - you are supposed to manually call
//! redistribute() after
//! all widgets have been docked.
Region *DockLayout::dockItem(DockWidget *item, Region *r, int idx) {
  item->setWindowFlags(Qt::SubWindow);
  item->show();
  return dockItemPrivate(item, r, idx);
}

//------------------------------------------------------

// Internal docking function. Contains raw docking code, excluded reparenting
// (setWindowFlags)
// which may slow down a bit - should be done only after a redistribute() and a
// repaint() on
// real-time docking.
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
  if ((!r->getItem()) && (r->getChildList().size() == 0)) {
    delete r;  // Be', e' un po' improprio, ma funziona...
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
            // remainingSon is a leaf: better keep this and move son's item and
            // childList
            setItem(remainingSon->getItem());
            remainingSon->setItem(0);
          } else {
            // remainingSon is a branch: append remainingSon childList to parent
            // one and
            // sign this and remainingSon nodes for destruction.

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
//! widget). Since the geometry
//! reference changes a geometry() update may be needed - so item's show() is
//! not forced here. You should
//! eventually remember to call it manually after this.
bool DockLayout::undockItem(DockWidget *item) {
  // Find item's region index in m_regions
  Region *itemCarrier = find(item);

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

  // Remove region in regions list
  // m_regions.erase(i);   //Don't - m_regions is cleaned before the end by
  // remove_if
  itemCarrier->setItem(0);

  std::deque<Region *>::iterator j;
  j = std::remove_if(m_regions.begin(), m_regions.end(), isEmptyRegion);
  m_regions.resize(j - m_regions.begin());

  // Update status
  // qDebug("Undock");
  item->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
  // NOTA: Usando la flag Qt::Window il focus viene automaticamente riassegnato,
  // con un po' di ritardo.
  // Usando Tool questo non accade. In questo caso, i placeholder devono essere
  // disattivati...

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
  // Solving a small Lagrange multipliers problem to find solution on constraint
  // (2)
  assert(target.size() == intervals.size());

  unsigned int i;

  double targetSum = 0;
  for (i = 0; i < target.size(); ++i) targetSum += target[i];

  double multiplier = (sum - targetSum) / (double)target.size();

  nearest.resize(target.size());
  for (i = 0; i < target.size(); ++i) nearest[i] = target[i] + multiplier;

  // Now, constraint (1) is not met; however, satisfying also (2) yields a
  // hyperRect
  // on which we must find the nearest point to our above partial solution. In
  // particular,
  // it can be demonstrated that at least one coordinate of the current partial
  // solution is related
  // to the final one (...). This mean that we may have to solve sub-problems of
  // this same kind,
  // with less variable coordinates.

  unsigned int max;
  double distance, maxDistance = 0;

  for (i = 0; i < target.size(); ++i) {
    if (nearest[i] < intervals[i].first || nearest[i] > intervals[i].second) {
      distance = nearest[i] < intervals[i].first
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

  if (m_item) {
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
  // is the sum
  // of childs' minimumSizes. Otherwise, the maximum is taken.
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

  if (m_item) {
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
  // is the sum
  // of childs' maximumSizes. Otherwise, the minimum is taken.
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

//========================================================

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

void DockLayout::writeRegion(Region *r, QString &hierarchy) {
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
bool DockLayout::restoreState(const State &state) {
  QStringList vars = state.second.split(" ", QString::SkipEmptyParts);
  if (vars.size() < 1) return 0;

  // Check number of items
  unsigned int count = state.first.size();

  if (m_items.size() != count) return false;  // Items list is not coherent

  // Initialize new Regions hierarchy
  std::deque<Region *> newHierarchy;

  // Load it
  int maximizedItem = vars[0].toInt();

  if (vars.size() > 1) {
    // Scan hierarchy
    Region *r       = 0, *newRegion;
    int orientation = !vars[1].toInt();

    int i;
    for (i = 2; i < vars.size(); ++i) {
      if (vars[i] == "]") {
        // End region and get parent
        r = r->getParent();
      } else {
        // Allocate new Region
        newRegion = new Region(this);
        newHierarchy.push_back(newRegion);
        newRegion->m_orientation = !orientation;

        if (r) r->insertSubRegion(newRegion, r->getChildList().size());

        if (vars[i] == "[") {
          // Current region has children
          r = newRegion;
        } else {
          // newRegion has item
          newRegion->m_item =
              static_cast<DockWidget *>(m_items[vars[i].toInt()]->widget());
        }
      }
    }

    // Check if size constraints are satisfied
    newHierarchy[0]->calculateExtremalSizes();
  }

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

  // Docked widgets are found in hierarchy
  for (j = 0; j < m_regions.size(); ++j)
    if ((item = m_regions[j]->m_item)) {
      item->setWindowFlags(Qt::SubWindow);
      item->setDockedAppearance();
      item->m_floating  = false;
      item->m_saveIndex = -1;
      item->show();
    }

  // Recover available geometry infos
  // QRect availableRect= QApplication::desktop()->availableGeometry();
  int recoverX = 0, recoverY = 0;

  // Deal with floating panels
  for (j = 0; j < m_items.size(); ++j) {
    item = static_cast<DockWidget *>(m_items[j]->widget());

    if (item->m_saveIndex > 0) {
      // Ensure that floating panels are not placed in
      // unavailable positions
      if ((geoms[j] & QApplication::desktop()->availableGeometry(item))
              .isEmpty())
        item->move(recoverX += 50, recoverY += 50);

      // Set floating appearances
      item->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
      item->setFloatingAppearance();
      item->m_floating = true;
    }
  }

  // Allocate region separators
  unsigned int k;
  for (j = 0; j < m_regions.size(); ++j) {
    Region *r = m_regions[j];
    for (k = 1; k < r->m_childList.size(); ++k) {
      r->insertSeparator(
          m_decoAllocator->newSeparator(this, r->getOrientation(), r));
    }
  }

  // Calculate regions' geometry starting from leaves (items)
  if (m_regions.size()) m_regions[0]->restoreGeometry();

  // Then, ensure the result is correctly fitting the contents rect
  // redistribute();

  // NOTE: The previous might be tempting to ensure all is right -
  // unfortunately, it may
  // be that the main window's content rect is not yet defined before it is
  // shown the first
  // time (like on MAC), and that is needed to redistribute. Se we force the
  // saved values
  //(assuming they are right)...
  applyGeometry();

  // Finally, set maximized dock widget
  if (maximizedItem != -1) {
    item = static_cast<DockWidget *>(m_items[maximizedItem]->widget());

    // Problema: Puo' essere, in fase di caricamento dati, che la contentsRect
    // del layout
    // venga sballata! (vv. lo ctor di TMainWindow) Allora, evitiamo il
    // controllo fatto
    // in setMazimized, e assumiamo sia per forza corretto...
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

//! Recalculates the geometry of \b this Region and of its branches,
//! assuming those of 'leaf items' are correct.

//! Regions always tend to keep their geometry by default. However,
//! it may be useful (for example, when restoring the state of a DockLayout)
//! the possibility of recalculating its current geometry directly from the
//! items that are contained in the branches.
void Region::restoreGeometry() {
  // Applying a head-recursive algorithm to update the geometry of a Region
  // after those of its children have been updated
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

//========================================================================

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

//! Sets current deco allocator to decoAllocator. A default deco allocator
//! is already provided at construction.

//!\b NOTE: DockLayout takes ownership of the allocator.
void DockLayout::setDecoAllocator(DockDecoAllocator *decoAllocator) {
  // Delete old one
  if (m_decoAllocator) delete m_decoAllocator;

  // Place new one
  m_decoAllocator = decoAllocator;
}

//------------------------------------------------------

//! Sets current deco allocator to decoAllocator. A default deco allocator
//! is already provided at construction.

//!\b NOTE: DockWidget takes ownership of the allocator.
void DockWidget::setDecoAllocator(DockDecoAllocator *decoAllocator) {
  // Delete old one
  if (m_decoAllocator) delete m_decoAllocator;

  // Place a copy of new one
  m_decoAllocator = decoAllocator;
}
