

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "docklayout.h"

// Qt includes
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QDesktopWidget>

// STD includes
#include <assert.h>
#include <math.h>
#include <algorithm>

//========================================================

//-----------------------
//    Dock Lock Check
//-----------------------

DockingCheck *DockingCheck::instance() {
  static DockingCheck _instance;
  return &_instance;
}

//-------------------------------------

void DockingCheck::setToggle(QAction *toggle) { m_toggle = toggle; }

//-------------------------------------

void DockingCheck::setIsEnabled(bool on) {
  m_enabled = on;
  if (m_toggle) m_toggle->setChecked(on);
}

//========================================================

class DockingToggleCommand final : public MenuItemHandler {
public:
  DockingToggleCommand() : MenuItemHandler("MI_DockingCheck") {}

  void execute() override {
    DockingCheck *dc = DockingCheck::instance();
    dc->setIsEnabled(!dc->isEnabled());
  }

} dockingToggleCommand;

//========================================================

//------------------------
//    Geometry inlines
//------------------------

//! Calculates infinite-norm distance between points \b a and \b b.
inline int absoluteDistance(const QPoint &a, const QPoint &b) {
  return std::max(abs(a.x() - b.x()), abs(a.y() - b.y()));
}

//-------------------------------------

// Il metodo QRectF::toRect pare che faccia la cosa commentata. Ovviamente non
// e' la cosa piu' simpatica - considera
// che in questo modo i bordi del rect di input *non vengono approssimati alle
// piu' vicine coordinate intere*!
//  es:   topLeft= (1/3, 1/3); width= 4/3, height= 4/3 => left= top= right=
//  bottom= 0.
inline QRect toRect(const QRectF &rect) {
  // return QRect(qRound(rect.left()), qRound(rect.top()), qRound(rect.width()),
  // qRound(rect.height()));
  return QRect(rect.topLeft().toPoint(),
               rect.bottomRight().toPoint() -= QPoint(1, 1));
}

//========================================================

// Forward declaration
namespace {
QDesktopWidget *desktop;
void getClosestAvailableMousePosition(QPoint &globalPos);
}

//========================================================

//----------------------
//    Layout inlines
//----------------------

inline void DockLayout::update() {
  // E' necessario?
  applyGeometry();
}

//========================================================

//------------------------------
//    My Dock Widget Methods
//------------------------------

//! Constructs a dock Widget; every newly constructed dock widget is floating
//! (i.e. not docked
//! into the layout).
DockWidget::DockWidget(QWidget *parent, Qt::WindowFlags flags)
    : QFrame(parent, flags)
    , m_dragging(false)
    , m_resizing(false)
    , m_floating(true)
    , m_undocking(false)
    , m_parentLayout(0)
    , m_selectedPlace(0)
    , m_maximized(0) {
  // Don't let this widget inherit the parent's backround color
  // setAutoFillBackground(true);
  // setBackgroundRole(QPalette::Background);

  // setAttribute(Qt::WA_DeleteOnClose);   //Since Toonz just hides panels...
  setAttribute(Qt::WA_Hover);

  // Set default minimum and maximum sizes.
  setMinimumSize(QSize(50, 50));
  setMaximumSize(QSize(10000, 10000));

  m_decoAllocator = new DockDecoAllocator;

  // Make sure the desktop is initialized and known
  desktop = qApp->desktop();
}

//-------------------------------------

DockWidget::~DockWidget() {
  // Since close button lies on the title bar, make sure mouse is released if
  // that is pressed.
  if (QWidget::mouseGrabber() == this) releaseMouse();
  clearDockPlaceholders();

  // Delete deco allocator
  delete m_decoAllocator;
}

//-------------------------------------

//! Clears dock placeholders for this dockwidget. This is automatically called
//! at drag ends.
void DockWidget::clearDockPlaceholders() {
  unsigned int i;
  for (i = 0; i < m_placeholders.size(); ++i) delete m_placeholders[i];
  m_placeholders.clear();
}

//-------------------------------------

//! Shows dock widget in docked mode.

//! Returns true or false whether input \b QPoint is inside the dragging grip
//! for \b this DockWidget, or not.
//! Typically (and by default), true is returned whenever p lies inside the
//! widget's title bar when floating;
//! however, you are free to place the drag grip anywhere reimplementing this
//! method in custom dockwidgets.
bool DockWidget::isDragGrip(QPoint p) {
  if (isFloating()) {
    QRect frame        = frameGeometry();
    QRect conts        = geometry();
    int margin         = conts.left() - frame.left();
    int titleBarHeight = conts.top() - frame.top();

    QRect titleArea(QPoint(0, margin - titleBarHeight),
                    QPoint(width() - 1, -1));

    return titleArea.contains(p);
  }

  return 0;
}

//-------------------------------------

bool DockWidget::event(QEvent *e) {
  // qDebug("Dock Widget - Event type: %d", e->type());

  // Little deviation or type specifications for received events
  switch (e->type()) {
  // Dock widgets are hover widgets - since their cursor may change on resize
  // grips.
  case QEvent::HoverMove:
    hoverMoveEvent(static_cast<QHoverEvent *>(e));
    return true;

  // Native titlebar press events must be redirected to common ones
  // - and, in this case mouse has to grabbed
  case QEvent::NonClientAreaMouseButtonPress:
    // grabMouse();  //Cannot not go here or resizes cannot be natively handled
    mousePressEvent(static_cast<QMouseEvent *>(e));
    return true;

  // Titlebars are not natively handled in this raw class - their responsibility
  // falls
  // to user implementation of DockWidget class
  case QEvent::WindowTitleChange:
    windowTitleEvent(e);
    return true;

  default:
    return QWidget::event(e);
  }
}

//-------------------------------------

//! Adjusts widget cursor depending on \b isResizeGrip()
// NOTA: Da migliorare: ricordare il cursor settato o se era unset e riapplicare
// quando fuori dal resize grip.
void DockWidget::hoverMoveEvent(QHoverEvent *he) {
  if (m_floating && !m_resizing && !m_undocking) {
    QCursor newCursor = Qt::ArrowCursor;

    if ((m_marginType = isResizeGrip(he->pos()))) {
      // Hovering a margin - update cursor shape
      if (m_marginType & leftMargin) {
        if (m_marginType & topMargin)
          newCursor = Qt::SizeFDiagCursor;
        else if (m_marginType & bottomMargin)
          newCursor = Qt::SizeBDiagCursor;
        else
          newCursor = Qt::SizeHorCursor;
      } else if (m_marginType & rightMargin) {
        if (m_marginType & topMargin)
          newCursor = Qt::SizeBDiagCursor;
        else if (m_marginType & bottomMargin)
          newCursor = Qt::SizeFDiagCursor;
        else
          newCursor = Qt::SizeHorCursor;
      } else
        newCursor = Qt::SizeVerCursor;
    }

    if (newCursor.shape() != cursor().shape()) setCursor(newCursor);
  }
}

//-------------------------------------

//! Dispatches mouse presses for particular purposes...

//! a)  Trigger a window resize (or none with native deco)
//!    if press is over a resize grip

//! b)  Trigger a window drag if press is over a drag grip
void DockWidget::mousePressEvent(QMouseEvent *me) {
  if ((m_marginType = m_floating ? isResizeGrip(me->pos()) : 0)) {
    // Resize begins

    // NOTE: It is better to assume that resize grips dominate over drag grips:
    // this ensures
    // that mouse cursor changes are always consistent with resize events.

    m_resizing            = true;
    m_dragMouseInitialPos = me->globalPos();  // Re-used as old position
  } else if (isDragGrip(me->pos())) {
    // Dragging begins
    DockingCheck *lock = DockingCheck::instance();  // Docking system lock

    m_dragMouseInitialPos = me->globalPos();
    m_dragInitialPos      = pos();
    // Grab mouse inputs - useful if widget gets under placeholders
    if (me->type() == QEvent::NonClientAreaMouseButtonPress)
      // If can receive double-clicks, avoid grabbing mouse
      grabMouse();

    if (m_floating) {
      m_dragging = true;
      // Do not allow docking if there is a maximized widget or the layout is
      // locked
      if (m_parentLayout && !m_parentLayout->getMaximized() &&
          !lock->isEnabled())
        m_parentLayout->calculateDockPlaceholders(this);
    } else {
      if (!lock->isEnabled()) m_undocking = true;
      m_dragInitialPos = parentWidget()->mapToGlobal(m_dragInitialPos);
    }
  }
}

//-------------------------------------

void DockWidget::mouseMoveEvent(QMouseEvent *me) {
  QPoint correctedGlobalPos(me->globalPos());
  getClosestAvailableMousePosition(correctedGlobalPos);

  if (m_resizing) {
    // m_dragMouseInitialPos re-used as old position
    int dx = correctedGlobalPos.x() - m_dragMouseInitialPos.x();
    int dy = correctedGlobalPos.y() - m_dragMouseInitialPos.y();

    QRect geom = geometry();

    if (m_marginType & leftMargin) {
      int newWidth = geom.width() - dx;
      if (newWidth >= minimumWidth() && newWidth <= maximumWidth())
        geom.setLeft(geom.left() + dx);
    } else if (m_marginType & rightMargin)
      geom.setRight(geom.right() + dx);

    if (m_marginType & topMargin) {
      int newHeight = geom.height() - dy;
      if (newHeight >= minimumHeight() && newHeight <= maximumHeight())
        geom.setTop(geom.top() + dy);
    } else if (m_marginType & bottomMargin)
      geom.setBottom(geom.bottom() + dy);

    setGeometry(geom);

    m_dragMouseInitialPos = correctedGlobalPos;
  } else if (m_dragging) {
    move(m_dragInitialPos + correctedGlobalPos - m_dragMouseInitialPos);
    selectDockPlaceholder(me);
  } else if (m_undocking) {
    int distance = absoluteDistance(me->globalPos(), m_dragMouseInitialPos);
    if (distance > 8) {
      m_undocking = false;

      // Attempt undocking
      if (m_parentLayout->undockItem(this)) {
        m_dragging = true;

        // Then, move dock widget under cursor, as if drag actually begun at
        // button press.
        move(m_dragInitialPos + correctedGlobalPos - m_dragMouseInitialPos);
        show();  // Dock widget is not automatically shown after undock.

        // Re-grab mouse inputs. Seems that making the window float (i.e.:
        // reparenting) breaks old grab.
        // NOTE: mouse *must* be grabbed only when visible - see Qt manual.
        grabMouse();

        // After undocking takes place, docking possibilities have to be
        // recalculated
        m_parentLayout->calculateDockPlaceholders(this);
        selectDockPlaceholder(me);
      }
    }
  }
}

//-------------------------------------

void DockWidget::mouseReleaseEvent(QMouseEvent *me) {
  // Ensure mouse is released
  releaseMouse();

  if (m_dragging) {
    m_dragging = false;

    if (m_floating && m_selectedPlace) {
      m_parentLayout->dockItem(this, m_selectedPlace);
    } else {
      // qDebug("Dock failed");
    }

    // Clear dock placeholders
    clearDockPlaceholders();
    m_selectedPlace = 0;
  } else if (m_undocking) {
    // qDebug("Undock failed");
    m_undocking = false;
  } else if (m_resizing) {
    m_resizing = false;
  }
}

//-------------------------------------

//! DockWidgets respond to title bar double clicks maximizing the widget in
//! layout's contents rect.
void DockWidget::mouseDoubleClickEvent(QMouseEvent *me) {
  if (!m_floating && isDragGrip(me->pos())) {
    parentLayout()->setMaximized(this, !m_maximized);
  }
}

//-------------------------------------

void DockWidget::maximizeDock() {
  if (!m_floating) {
    parentLayout()->setMaximized(this, !m_maximized);
  }
}

//-------------------------------------
//! Switch in selected dock placeholder's hierarchy.
void DockWidget::wheelEvent(QWheelEvent *we) {
  if (m_dragging) {
    if (m_selectedPlace) {
      DockPlaceholder *newSelected =
          (we->delta() > 0)
              ? m_selectedPlace->parentPlaceholder()
              : m_selectedPlace->childPlaceholder(
                    parentWidget()->mapFromGlobal(we->globalPos()));

      if (newSelected != m_selectedPlace) {
        m_selectedPlace->hide();
        newSelected->show();
        m_selectedPlace = newSelected;
      }
    }
  }
}

//-------------------------------------

//! Returns widget (separator or docked widget) containing input \point, or 0 if
//! none.
//! Convenience function combining parentLayout() and containerOf(\b point).

//!\b NOTE: Observe that, in any case, the use of QEnterEvents is discouraged
//! for this purpose:
//! remember that we forcedly remain within its boundaries when dragging a dock
//! widget;
//! instead, we are rather interested about entering the dock widgets *below*.
QWidget *DockWidget::hoveredWidget(QMouseEvent *me) {
  if (!m_parentLayout) return 0;

  QPoint point = parentWidget()->mapFromGlobal(me->globalPos());
  return m_parentLayout->containerOf(point);
}

//-------------------------------------

//! Returns adjacent placeholders to hovered widget. If hovered is a separator,
//! just return associated placeholder.
DockPlaceholder *DockWidget::placeAdjacentTo(DockWidget *dockWidget,
                                             int boundary) {
  Region *r = parentLayout()->find(dockWidget);

  if (((boundary == DockPlaceholder::left ||
        boundary == DockPlaceholder::right) &&
       r->getOrientation() == Region::horizontal) ||
      ((boundary == DockPlaceholder::top ||
        boundary == DockPlaceholder::bottom) &&
       r->getOrientation() == Region::vertical)) {
    // Placeholder is coherent with region orientation
    return r->placeholders().size() ? r->placeholder(boundary % 2) : 0;
  } else {
    // Search in parent region
    Region *parent = r->getParent();
    if (parent) {
      unsigned int i = parent->find(r);
      return parent->placeholders().size()
                 ? parent->placeholder(i + (boundary % 2))
                 : 0;
    } else {
      // No parent region - dockWidget is the only widget of the whole layout;
      // Then check the first 2 elements of placeholders vector.
      if (!m_placeholders[boundary % 2]->getParentRegion()) {
        return m_placeholders.size() ? m_placeholders[boundary % 2] : 0;
      }
    }
  }

  return 0;
}

//-------------------------------------

//! Returns placeholder associated with input separator.
DockPlaceholder *DockWidget::placeOfSeparator(DockSeparator *sep) {
  Region *r = sep->getParentRegion();
  int idx   = sep->getIndex();

  return r->placeholders().size() ? r->placeholder(idx + 1) : 0;
}

//-------------------------------------

//! Processes an input mouse event to select the active placeholder among the
//! possible ones
//! for \b this dock widget. The selected placeholder is also evidenced with
//! respect to the
//! other (or these are kept hidden) according to the body of this function.
void DockWidget::selectDockPlaceholder(QMouseEvent *me) {
  // const int inf= 1000000;
  DockPlaceholder *selected = 0;

  // Search placeholders cotaining muose position
  unsigned int i;
  for (i = 0; i < m_placeholders.size(); ++i) {
    if (m_placeholders[i]->geometry().contains(me->globalPos())) {
      selected = m_placeholders[i];
    }
  }

  // In order to avoid flickering
  if (m_selectedPlace != selected) {
    if (m_selectedPlace) m_selectedPlace->hide();
    if (selected) selected->show();
  }

  m_selectedPlace = selected;
}

//========================================================

//-------------------------
//    Dock Placeholders
//-------------------------

inline QRect DockPlaceholder::parentGeometry() const {
  return m_region ? toRect(m_region->getGeometry())
                  : m_owner->parentLayout()->contentsRect();
}

//------------------------------------------------------

//! Assigns the geometry of \b this placeholder.

//! Once a placeholder is created, in response to a user window drag,
//! dock placeholders are calculated. After a placeholder is created,
//! its geometry is built according to this function. It is possible to
//! reimplement it in order to build custom placeholder styles.
inline void DockPlaceholder::buildGeometry() {
  QRect relativeToMainRect;

  if (m_separator)
    relativeToMainRect = m_separator->geometry();
  else {
    QRect parentRect   = parentGeometry();
    DockLayout *layout = m_owner->parentLayout();
    QRect mainRect     = layout->contentsRect();
    int sepWidth       = layout->spacing();
    int margin = 6;  // layout->margin();   //Purtroppo questa info e' assegnata
                     // prima delle Room...

    if (isRoot()) {
      // Set the whole contents rect
      relativeToMainRect = parentRect;

      // Set a square at middle of parent geometry
      // QPoint center= parentRect.center();
      // relativeToMainRect= QRect(center - QPoint(50,50), center +
      // QPoint(50,50));
    } else if (getParentRegion() == 0 ||
               getParentRegion() == layout->rootRegion()) {
      // Outer insertion case
      switch (getAttribute()) {
        int leftBound, upperBound;

      case left:
        leftBound = parentRect.left() - margin;
        relativeToMainRect =
            QRect(leftBound, parentRect.top(), margin, parentRect.height());
        break;
      case right:
        leftBound = parentRect.right() + 1;
        relativeToMainRect =
            QRect(leftBound, parentRect.top(), margin, parentRect.height());
        break;
      case top:
        upperBound = parentRect.top() - margin;
        relativeToMainRect =
            QRect(parentRect.left(), upperBound, parentRect.width(), margin);
        break;
      default:
        upperBound = parentRect.bottom() + 1;
        relativeToMainRect =
            QRect(parentRect.left(), upperBound, parentRect.width(), margin);
        break;
      }
    } else
      switch (getAttribute()) {
        int leftBound, upperBound;

      case left:
        leftBound = parentRect.left();
        relativeToMainRect =
            QRect(leftBound, parentRect.top(), sepWidth, parentRect.height());
        break;
      case right:
        leftBound = parentRect.right() - sepWidth + 1;
        relativeToMainRect =
            QRect(leftBound, parentRect.top(), sepWidth, parentRect.height());
        break;
      case top:
        upperBound = parentRect.top();
        relativeToMainRect =
            QRect(parentRect.left(), upperBound, parentRect.width(), sepWidth);
        break;
      default:
        upperBound = parentRect.bottom() - sepWidth + 1;
        relativeToMainRect =
            QRect(parentRect.left(), upperBound, parentRect.width(), sepWidth);
        break;
      }
  }

  QPoint topLeft =
      m_owner->parentWidget()->mapToGlobal(relativeToMainRect.topLeft());
  QPoint bottomRight =
      m_owner->parentWidget()->mapToGlobal(relativeToMainRect.bottomRight());
  setGeometry(QRect(topLeft, bottomRight));
}

//------------------------------------------------------

DockPlaceholder::DockPlaceholder(DockWidget *owner, Region *r, int idx,
                                 int attributes)
    : QWidget(owner)
    , m_owner(owner)
    , m_separator(0)
    , m_region(r)
    , m_idx(idx)
    , m_attributes(attributes) {
  setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);

  // Set separators
  if (r && idx && idx < (int)r->getChildList().size()) {
    m_separator = r->separators()[idx - 1];
  }
}

//------------------------------------------------------

inline DockSeparator *DockPlaceholder::getSeparator() const {
  return m_separator;
}

//------------------------------------------------------

//! Find the 'parent' of this placeholder (same side/orientation, on grandParent
//! region)

//! NOTE: If no grandParent exists \b this is the root, which is then returned.
DockPlaceholder *DockPlaceholder::parentPlaceholder() {
  // Placeholders covering a whole separator or roots have no parent (return
  // itself)
  if (m_attributes == sepHor || m_attributes == sepVert || isRoot())
    return this;

  // Now, check if owner Region has a parent.
  Region *grandParent;
  if (!m_region || !m_region->getParent()) return this;

  if ((grandParent = m_region->getParent()->getParent())) {
    // Good, we finally have to search in grandParent's region our direct
    // parent.
    unsigned int idx = grandParent->find(m_region->getParent());

    // Since placeholders are built ordered, just use the found idx.
    if (m_attributes == left || m_attributes == top)
      return grandParent->placeholders().size() ? grandParent->placeholder(idx)
                                                : this;
    else
      return grandParent->placeholders().size()
                 ? grandParent->placeholder(idx + 1)
                 : this;
  } else {
    // GrandParent would be a new root. Then, take the first two possible
    // placeholders
    // of the entire dockWidget.
    if (m_owner->m_placeholders.size()) {
      DockPlaceholder *result = m_owner->m_placeholders[m_attributes % 2];
      if (!result->m_region) return result;
    }

    return this;
  }
}

//------------------------------------------------------

//! The opposite of parentPlaceholder() - but a point belonging to the child
//! region to be selected is requested as univoque key among all children.
DockPlaceholder *DockPlaceholder::childPlaceholder(QPoint p) {
  // Ensure this is not a root placeholder
  if (m_attributes == root) return this;

  // Define children list and ensure it is not void.
  Region *r;
  unsigned int i;
  bool lastExtremity;

  if (m_region) {
    if (!m_region->getChildList().size()) return this;

    // Search the subregion containing p
    for (i = 0; i < m_region->getChildList().size(); ++i)
      if (m_region->childRegion(i)->getGeometry().contains(p)) break;

    if (i == m_region->getChildList().size())
      return this;  // No subregion found...

    lastExtremity = (m_idx > (int)i);

    // Ensure it has subRegions.
    r = m_region->childRegion(i);
    if (!r->getChildList().size()) return this;
  } else {
    r             = m_owner->parentLayout()->rootRegion();
    lastExtremity = m_attributes % 2;
  }

  // Now, take the 'grandson' region as above.
  for (i = 0; i < r->getChildList().size(); ++i)
    if (r->childRegion(i)->getGeometry().contains(p)) break;

  if (i == r->getChildList().size()) return this;  // No subregion found...

  r = r->childRegion(i);

  // Finally, return child placeholder found.
  return r->placeholders().size()
             ? lastExtremity ? r->placeholders().back()
                             : r->placeholders().front()
             : this;
}

//------------------------------------------------------

//! Returns greatest placeholder which contains this one.
inline DockPlaceholder *DockPlaceholder::greatestPlaceholder() {
  DockPlaceholder *old = this, *current = parentPlaceholder();

  while (current != old) {
    old     = current;
    current = old->parentPlaceholder();
  }

  return current;
}

//------------------------------------------------------

//! Returns smallest placeholder contained by this one - a point belonging to
//! the child
//! region to be selected is requested as univoque key among all children.
inline DockPlaceholder *DockPlaceholder::smallestPlaceholder(QPoint p) {
  DockPlaceholder *old = this, *current = childPlaceholder(p);

  while (current != old) {
    old     = current;
    current = old->childPlaceholder(p);
  }

  return current;
}

//========================================================================

//------------------
//    Separators
//------------------

DockSeparator::DockSeparator(DockLayout *owner, bool orientation,
                             Region *parentRegion)
    : QWidget(owner->parentWidget())
    , m_owner(owner)
    , m_orientation(orientation)
    , m_parentRegion(parentRegion)
    , m_pressed(false) {
  setObjectName("DockSeparator");
  setWindowFlags(Qt::SubWindow);
  setAutoFillBackground(false);

  // Set appropriate widget cursor
  if (m_orientation == Region::horizontal)
    setCursor(Qt::SplitHCursor);
  else
    setCursor(Qt::SplitVCursor);

  show();
}

//-------------------------------------

void DockSeparator::mousePressEvent(QMouseEvent *me) {
  m_pressed = true;

  m_oldPos = me->globalPos();

  const std::deque<DockSeparator *> &sepList = m_parentRegion->separators();
  const std::deque<Region *> &childList      = m_parentRegion->getChildList();

  // Find separated Regions (separator index)
  unsigned int i;

  for (i = 0; i < sepList.size(); ++i)
    if (sepList[i] == this) break;

  // Calculate bounds for separator shift
  // First ensure extremal sizes are recalculated
  m_parentRegion->calculateExtremalSizes();

  int sepWidth = m_owner->spacing();
  Region *r    = getParentRegion();
  double parentLeft, parentRight;
  int leftSepCount = m_index;
  int rightSepCount =
      r->separators().size() - leftSepCount;  // This sep included

  if (m_orientation == Region::horizontal) {
    parentLeft  = r->getGeometry().left();
    parentRight = r->getGeometry().right();
  } else {
    parentLeft  = r->getGeometry().top();
    parentRight = r->getGeometry().bottom();
  }

  // Calculate left and right extremal sizes
  int j, leftMinSize = 0, rightMinSize = 0, leftMaxSize = 0, rightMaxSize = 0;

  for (j = 0; j <= m_index; ++j) {
    leftMinSize += r->childRegion(j)->getMinimumSize(m_orientation);
    leftMaxSize += r->childRegion(j)->getMaximumSize(m_orientation);
  }

  int size = r->getChildList().size();
  for (j = m_index + 1; j < size; ++j) {
    rightMinSize += r->childRegion(j)->getMinimumSize(m_orientation);
    rightMaxSize += r->childRegion(j)->getMaximumSize(m_orientation);
  }

  // Sull'intera regione padre
  m_leftBound = std::max(parentLeft + leftMinSize + leftSepCount * sepWidth,
                         parentRight - rightMaxSize - rightSepCount * sepWidth);

  m_rightBound =
      std::min(parentLeft + leftMaxSize + leftSepCount * sepWidth,
               parentRight - rightMinSize - rightSepCount * sepWidth);
}

//-------------------------------------

inline void DockSeparator::mouseReleaseEvent(QMouseEvent *me) {
  m_pressed = false;
}

//-------------------------------------

void DockSeparator::mouseMoveEvent(QMouseEvent *me) {
  if (m_pressed) {
    double movedPosition, newPosition;

    if (m_orientation == Region::horizontal) {
      double dx = me->globalX() - m_oldPos.x();
      movedPosition =
          getParentRegion()->childRegion(m_index)->getGeometry().right() + dx;
      newPosition =
          std::min(std::max(movedPosition, m_leftBound), m_rightBound);
      dx += newPosition - movedPosition;

      if (dx) {
        Region *r;
        QRectF newGeometry;
        double newWidth, dxTemp = dx;
        int i;

        // Propagate dx in left-adjacent regions
        for (i = m_index; dx != 0 && i >= 0; --i) {
          r           = getParentRegion()->childRegion(i);
          newGeometry = r->getGeometry();

          // Right margin is shifted by dx
          newGeometry.adjust(0, 0, dx, 0);
          newWidth = newGeometry.width();
          // New width absorbs part of dx according to constraints
          newWidth = std::min(
              std::max(newWidth, (double)r->getMinimumSize(Region::horizontal)),
              (double)r->getMaximumSize(Region::horizontal));
          newGeometry.adjust(dx = newGeometry.width() - newWidth, 0, 0, 0);
          r->setGeometry(newGeometry);
          r->redistribute();
        }

        dx       = dxTemp;
        int size = getParentRegion()->getChildList().size();
        // Propagate dx in right-adjacent regions
        for (i = m_index + 1; dx != 0 && i < size; ++i) {
          r           = getParentRegion()->childRegion(i);
          newGeometry = r->getGeometry();

          newGeometry.adjust(dx, 0, 0, 0);
          newWidth = newGeometry.width();
          newWidth = std::min(
              std::max(newWidth, (double)r->getMinimumSize(Region::horizontal)),
              (double)r->getMaximumSize(Region::horizontal));
          newGeometry.adjust(0, 0, dx = newWidth - newGeometry.width(), 0);
          r->setGeometry(newGeometry);
          r->redistribute();
        }

        m_owner->update();
      }
    } else {
      double dy = me->globalY() - m_oldPos.y();
      movedPosition =
          getParentRegion()->childRegion(m_index)->getGeometry().bottom() + dy;
      newPosition =
          std::min(std::max(movedPosition, m_leftBound), m_rightBound);
      dy += newPosition - movedPosition;

      if (dy) {
        Region *r;
        QRectF newGeometry;
        double newHeight, dyTemp = dy;
        int i;

        for (i = m_index; dy != 0 && i >= 0; --i) {
          r                  = getParentRegion()->childRegion(i);
          newGeometry        = r->getGeometry();
          QRectF oldGeometry = newGeometry;

          newGeometry.adjust(0, 0, 0, dy);
          newHeight = newGeometry.height();
          newHeight = std::min(
              std::max(newHeight, (double)r->getMinimumSize(Region::vertical)),
              (double)r->getMaximumSize(Region::vertical));
          newGeometry.adjust(0, dy = newGeometry.height() - newHeight, 0, 0);
          r->setGeometry(newGeometry);
          r->redistribute();
        }

        dy       = dyTemp;
        int size = getParentRegion()->getChildList().size();
        for (i = m_index + 1; dy != 0 && i < size; ++i) {
          r           = getParentRegion()->childRegion(i);
          newGeometry = r->getGeometry();

          newGeometry.adjust(0, dy, 0, 0);
          newHeight = newGeometry.height();
          newHeight = std::min(
              std::max(newHeight, (double)r->getMinimumSize(Region::vertical)),
              (double)r->getMaximumSize(Region::vertical));
          newGeometry.adjust(0, 0, 0, dy = newHeight - newGeometry.height());
          r->setGeometry(newGeometry);
          r->redistribute();
        }

        m_owner->update();
      }
    }

    m_oldPos = QPoint(me->globalX(), me->globalY());
  }
}

//========================================================

//----------------------------------
//    Available geometries code
//----------------------------------

namespace {

//-------------------------------------

// Finds the closest mouse point belonging to some available geometry.
void getClosestAvailableMousePosition(QPoint &globalPos) {
  // Search the screen rect containing the mouse position
  int i, screens = desktop->numScreens();
  for (i = 0; i < screens; ++i)
    if (desktop->screenGeometry(i).contains(globalPos)) break;

  // Find the closest point to the corresponding available region
  QRect rect(desktop->availableGeometry(i));
  if (rect.contains(globalPos)) return;

  // Return the closest point to the available geometry
  QPoint result;
  if (globalPos.x() < rect.left())
    globalPos.setX(rect.left());
  else if (globalPos.x() > rect.right())
    globalPos.setX(rect.right());

  if (globalPos.y() < rect.top())
    globalPos.setY(rect.top());
  else if (globalPos.y() > rect.bottom())
    globalPos.setY(rect.bottom());
}

}  // Local namespace
