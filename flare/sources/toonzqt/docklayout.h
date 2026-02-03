#pragma once

#ifndef DOCKLAYOUT_H
#define DOCKLAYOUT_H

#include "tcommon.h"

#include <QWidget>
#include <QAction>

#include <deque>
#include <vector>
#include <QLayout>
#include <QFrame>
#include "docklayout.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------

// Forward Declarations
class DockLayout;
class DockWidget;

class DockPlaceholder;
class DockSeparator;
class DockDecoAllocator;

class Region;

//========================================================================

//--------------------------
//    Docking Lock Check
//--------------------------

//! Singleton for docking system lock.
class DVAPI DockingCheck {
  bool m_enabled;
  QAction *m_toggle;
  DockingCheck() : m_enabled(false), m_toggle(0) {}

public:
  static DockingCheck *instance();
  void setToggle(QAction *toggle);
  bool isEnabled() const { return m_enabled; }
  void setIsEnabled(bool on);
};

//========================================================================

//-------------------
//    Dock Layout
//-------------------

//! DockLayout inherits the abstract QLayout to provide a docking system for
//! widgets.
/*!
  \b IMPORTANT NOTE: Observe that the addWidget() method expects only widgets of
  type DockWidget, so any other
  widget type added to this kind of Layout will cause a run-time error.

  \sa DockWidget and DockSeparator classes.
*/
class DVAPI DockLayout final : public QLayout {
  std::vector<QLayoutItem *> m_items;
  std::deque<Region *> m_regions;

  DockWidget
      *m_maximizedDock;  // Let the layout know if there is a maximized widget

  // Decoration-related allocator (separators)
  DockDecoAllocator *m_decoAllocator;

public:
  DockLayout();
  virtual ~DockLayout();

  // QLayout item handling (see Qt reference)
  int count(void) const override;
  void addItem(QLayoutItem *item) override;
  QSize sizeHint() const override;
  QSize minimumSize() const override;
  QSize maximumSize() const override;
  QLayoutItem *itemAt(int) const override;
  QWidget *widgetAt(int) const;
  QLayoutItem *takeAt(int) override;
  void setGeometry(const QRect &rect) override;

  void update();        // Re-applies partition found
  void redistribute();  // Calculates partition
  void applyTransform(const QTransform &transform);  // Applies transformation to
                                                     // known partition - Da
                                                     // rimuovere, non serve...

  DockWidget *getMaximized() { return m_maximizedDock; }
  void setMaximized(
      DockWidget *item,
      bool state = true);  // Let DockLayout handle maximization requests

  // Docking and undocking methods.
  Region *dockItem(DockWidget *item, Region *r = 0, int idx = 0);
  void dockItem(DockWidget *item, DockPlaceholder *place);
  void dockItem(DockWidget *item, DockWidget *target, int regionSide);
  bool undockItem(DockWidget *item);
  void calculateDockPlaceholders(DockWidget *item);

  // Query methods
  Region *rootRegion() const {
    return m_regions.size() ? m_regions.front() : 0;
  }
  const std::deque<Region *> &regions() const { return m_regions; }
  Region *region(int i) const { return m_regions[i]; }
  Region *find(DockWidget *item) const;
  QWidget *containerOf(QPoint point) const;

  // Save and load DockLayout states
  typedef std::pair<std::vector<QRect>, QString> State;

  State saveState();
  bool restoreState(const State &state);

  // Decorations allocator
  void setDecoAllocator(DockDecoAllocator *decoAllocator);

private:
  void applyGeometry();
  inline void updateSeparatorCursors();
  Region *dockItemPrivate(DockWidget *item, Region *r, int idx);

  // Insertion and removal check - called internally by dock/undockItem
  bool isPossibleInsertion(DockWidget *item, Region *parentRegion,
                           int insertionIdx);
  bool isPossibleRemoval(DockWidget *item, Region *parentRegion,
                         int removalIdx);

  // Internal save function
  void writeRegion(Region *r, QString &hierarchy);
};

//========================================================================

//-----------------------
//    Dock Widget
//-----------------------

//! Dockable widgets are widget containers handled by DockLayout class.
/*!
  DockLayouts accept only dock widgets of this class; so if you want to
  place a given widget under this kind of layout, a DockWidget shell for
  it must be allocated first. The following class implements base dock
  widgets, with native floating decorations and no docked title bar.
  It is encouraged to reimplement all the essential functions for custom aspect
  and behaviour.

  \sa DockLayout and DockPlaceholder classes.
*/
class DVAPI DockWidget : public QFrame {
  friend class DockLayout;  // DockLayout is granted access to placeholders'
                            // privates
  friend class DockPlaceholder;  // As above.
  // friend Region;          //Regions need access to m_saveIndex field.
public:
  void maximizeDock();

  enum {
      variable = 0,  // default, docked panel may auto resize with window
      fixed = 1,     // to be used with setFixedWidth()
      sizeable = 2   // allow panel to be sizeable but doesn't auto resize
  };
  int getFixWidthMode() { return m_modeFixWidth; }
  void setFixWidthMode(int fixedmode) { m_modeFixWidth = fixedmode; }

protected:
  // Private attributes for dragging purposes
  bool m_floating;  // Whether this window is floating or docked
  bool m_wasFloating;
  bool m_dragging;   // Whether this window is being dragged
  bool m_undocking;  // Still docked, but after a mouse button press on a title
                     // bar.
  // NOTE: m_dragging ==> !m_undocking; m_dragging=>m_floating.

  // Private infos for resizing purposes
  bool m_resizing;   // Whether this window is being resized
  int m_marginType;  // Type of resize to consider

  // Maximization
  bool m_maximized;

  // Level Strip and Style Editor use a fixed width on 
  // window resize to minimize user frustration
  // This variable is only used by Level Strip right now.
  // This is only true if the level strip is vertical.
  //
  // Edit: Implementation changed to avoid hardcoded checks with panels names
  int m_modeFixWidth = 0;

private:
  QPoint m_dragInitialPos;
  QPoint m_dragMouseInitialPos;

  // Widget and Layout links
  DockLayout *m_parentLayout;

  // Decoration-related allocator (placeholders)
  DockDecoAllocator *m_decoAllocator;

  int m_saveIndex;

protected:
  // Protected infos for docking purposes
  std::vector<DockPlaceholder *> m_placeholders;
  DockPlaceholder *m_selectedPlace;

public:
  DockWidget(QWidget *parent = 0, Qt::WindowFlags flags = Qt::Tool);
  virtual ~DockWidget();

  // Returns the DockLayout owning \b this dock widget
  DockLayout *parentLayout() const { return m_parentLayout; }

  bool isFloating() const { return m_floating; }
  bool wasFloating() const { return m_wasFloating; }
  void clearWasFloating() { m_wasFloating = false; }
  bool isMaximized() const { return m_maximized; }

  // Query functions
  QWidget *hoveredWidget(QMouseEvent *me);
  DockPlaceholder *placeAdjacentTo(DockWidget *dockWidget, int boundary);
  DockPlaceholder *placeOfSeparator(DockSeparator *);
  const std::vector<DockPlaceholder *> &placeholders() const {
    return m_placeholders;
  }

  // Re-implementable functions for custom dock widgets.

  //! Returns the minimum size of the dock widget when docked. This function
  //! should be
  //! reimlemented whenever minimum size changes between floating and docked
  //! appearance.
  virtual QSize getDockedMinimumSize() { return minimumSize(); }

  //! Returns the maximum size of the dock widget when docked. This function
  //! should be
  //! reimlemented whenever minimum size changes between floating and docked
  //! appearance.
  virtual QSize getDockedMaximumSize() { return maximumSize(); }

  //! This function is called in order to show the dock widget in its floating
  //! status. No show() or update()
  //! is needed in its body.
  //! It can be reimplemented to build custom-styled dock widgets.
  virtual void setFloatingAppearance() { setWindowFlags(Qt::Tool); }

  //! This function is called in order to show the dock widget in its docked
  //! status. No show() or update()
  //! is needed in its body.
  //! It can be reimplemented to build custom-styled dock widgets.
  virtual void setDockedAppearance() {}

  virtual bool isDragGrip(QPoint p);
  virtual int isResizeGrip(QPoint) {
    return 0;  // Native deco widgets handle margins and resizes on their own
  }

  enum {
    leftMargin   = 0x1,
    rightMargin  = 0x2,
    topMargin    = 0x4,
    bottomMargin = 0x8
  };

  // Placeholders-related methods
  virtual void selectDockPlaceholder(QMouseEvent *me);
  void clearDockPlaceholders();

  // Decorations allocator
  void setDecoAllocator(DockDecoAllocator *decoAllocator);

  // reimplemented in FlipbookPanel
  virtual void onDock(bool docked) {}

private:
  // Event handling
  // Basic events
  bool event(QEvent *e) override;
  void mousePressEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
  void hoverMoveEvent(QHoverEvent *he);

protected:
  // Customizable events
  void wheelEvent(QWheelEvent *we) override;
  void mouseDoubleClickEvent(QMouseEvent *me) override;
  virtual void windowTitleEvent(QEvent *) {}
};

//========================================================================

//---------------------
//    DockSeparator
//---------------------

//! Separators are interposition widgets among docked DockWidgets of a
//! DockLayout.
/*!
  A DockSeparator has the role of separating sister Regions; it is always owned
  by a parent
  Region and inherits its subdivision (here separation) direction. It also
  provides basical
  interaction with the user, allowing itself to be shifted along separation
  direction until
  geometric constraints are met.
  DockSeparator class can be inherited to build custom separators - in that
  case, a
  heir of DockDecoAllocator class allocating the new separator class must be
  assigned to
  the DockLayout.
  It is also possible to specify the Separators' thickness used in a DockLayout
  through the
  QLayout::setSpacing(int) method.

  \b NOTE: Observe that Separators' geometry is owned by the DockLayout to which
  it belongs; it
  is discouraged (but not forbidden) to manually modify it. You may, for
  example, modify the
  geometry of a DockSeparator when dragging a dock widget over it.
  In any case, the layout will automatically regenerate Separators' geometry at
  each update of
  the layout.

  \sa DockLayout and DockWidget classes.
*/

class DockSeparator : public QWidget {
  friend class DockLayout;  // Layout updates each DockSeparator during
                            // DockLayout::applyGeometry()
  friend class Region;      // Region may update a DockSeparator's parent during
                            // removeItem()

  DockLayout *m_owner;

  // Event-related infos
  bool m_pressed;
  QPoint m_oldOrigin;
  QPoint m_oldPos;

  // Structural infos
  Region *m_parentRegion;
  int m_index;
  bool m_orientation;

  // Constraint infos
  double m_leftBound;
  double m_rightBound;

public:
  DockSeparator(DockLayout *owner, bool orientation, Region *parentRegion);
  virtual ~DockSeparator() {}

  // Structural getters
  bool getOrientation() const { return m_orientation; }
  Region *getParentRegion() const { return m_parentRegion; }
  int getIndex() const { return m_index; }

  // Public setters
  void shift(int dx);

private:
  void calculateBounds();

  void mousePressEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
};

//========================================================================

//---------------------------
//    Dock Placeholder
//---------------------------

//! A dock placeholder contains the necessary information about a possible
//! docking solution.

/*!
  Dock placeholders are top-level widgets used by a DockLayout to draw docking
  solutions for a dragged DockWidget. They are actually generated when
  dragging of a DockWidget begins: if it belongs to a parent DockLayout, docking
  possibilities are calculated and stored into the layout.
  Placeholders selection and activation depend on the dragged dock window and
  therefore
  are of its own responsibility.
  You may, however, inherit this class to provide custom placeholders; in this
  case,
  a DockDecoAllocator class reimplementing allocation of placeholders must be
  assigned to
  the owner dock widget.

  \sa DockLayout and DockWidget classes
*/
class DockPlaceholder : public QWidget {
  friend class DockLayout;  // DockLayout is granted access to placeholders'
                            // privates

  // Docking information - private
  Region *m_region;
  int m_idx;

  // Docking information - public
  int m_attributes;

  // Owner
  DockSeparator *m_separator;
  DockWidget *m_owner;

public:
  DockPlaceholder(DockWidget *owner, Region *r, int idx, int attributes = 0);
  virtual ~DockPlaceholder() {}

  // Member access methods

  //! Returns DockSeparator on which docking takes place (if any)
  DockSeparator *getSeparator() const;
  //! Returns dockWidget owner
  DockWidget *getDockWidget() const { return m_owner; }
  //! Returns Region owner
  Region *getParentRegion() const { return m_region; }
  //! Returns insertion index into parent region
  int getInsertionIdx() const { return m_idx; }

  enum {
    left    = 0,
    right   = 1,
    top     = 2,
    bottom  = 3,
    sepHor  = 4,
    sepVert = 5,
    root    = 6
  };
  int getAttribute() const { return m_attributes; }
  void setAttribute(int attribute) { m_attributes = attribute; }

  // Geometry functions
  QRect parentGeometry() const;
  virtual void buildGeometry();

  // Query functions
  //! A root placeholder is passed only if no item is currently docked (special
  //! case)
  bool isRoot() const { return m_attributes == root; }
  DockPlaceholder *parentPlaceholder();
  DockPlaceholder *greatestPlaceholder();
  DockPlaceholder *childPlaceholder(QPoint p);
  DockPlaceholder *smallestPlaceholder(QPoint p);

private:
  //! Let wheel events also be propagated to owner dock widget
  void wheelEvent(QWheelEvent *we) override { m_owner->wheelEvent(we); }
};

//===========================================

//--------------------
//    Class Region
//--------------------

//! Regions represent any rectangular space inside a DockLayout.
//! Normally there is no reason to deal with Regions, unless you want to
//! build complex docking systems or manually dock widgets into your code.

/*!
  Regions are rectangular areas of the DockLayout's contentsRect() which
  can be either subdiveded into subRegions (all in a given \b subdivision
  \b direction) or contain a DockWidget.
  Every subRegion, if present, has opposite subdivision direction with
  respect to parent one. In addition, regions possess lists of separators
  and placeholders found along its subdivision direction.
  Region information is owned by the DockLayout who allocates it; however,
  they are read-only accessible by the user.

  \sa DockLayout, DockWidget, DockSeparator and DockPlaceholder classes.
*/

class Region {
  friend class DockLayout;     // Layout is the main operating class over
                               // rectangular regions - need full access
  friend class DockSeparator;  // Separators need access to extremal sizes
                               // methods when moving themselves

  DockLayout *m_owner;
  DockWidget *m_item;
  Region *m_parent;
  std::deque<Region *> m_childList;
  std::deque<DockSeparator *> m_separators;

  std::vector<DockPlaceholder *> m_placeholders;

  QRectF m_rect;
  bool m_orientation;

  int m_minimumSize[2];
  int m_maximumSize[2];

  int m_saveIndex;

public:
  Region(DockLayout *owner, DockWidget *item = 0)
      : m_owner(owner), m_item(item), m_parent(0), m_orientation(0) {}
  ~Region();

  enum { inf = 1000000 };
  enum { horizontal = 0, vertical = 1 };
  enum { left = 0x1, right = 0x2, top = 0x4, bottom = 0x8 };

  // Getters - public
  bool getOrientation() const { return m_orientation; }
  QRectF getGeometry() const { return m_rect; }
  QSizeF getSize() const { return QSizeF(m_rect.width(), m_rect.height()); }
  Region *getParent() const { return m_parent; }
  DockWidget *getItem() const { return m_item; }

  const std::deque<Region *> &getChildList() const { return m_childList; }
  Region *childRegion(int i) const { return m_childList[i]; }

  const std::deque<DockSeparator *> &separators() const { return m_separators; }
  DockSeparator *separator(int i) const { return m_separators[i]; }

  std::vector<DockPlaceholder *> &placeholders() { return m_placeholders; }
  DockPlaceholder *placeholder(int i) const { return m_placeholders[i]; }

  unsigned int find(const Region *subRegion) const;

  bool checkWidgetsToBeFixedWidth(std::vector<QWidget *> &widgets,
                                  bool &fromDocking);

private:
  // Setters - private
  void setOrientation(bool orientation) { m_orientation = orientation; }
  void setGeometry(const QRectF &rect) { m_rect = rect; }
  void setSize(const QSizeF &size) { m_rect.setSize(size); }
  void setParent(Region *parent) { m_parent = parent; }
  void setItem(DockWidget *item) { m_item = item; }

  // Insertion and removal methods
  void insertSubRegion(Region *subregion, int idx);
  Region *insertItem(DockWidget *item, int idx);
  void removeItem(DockWidget *item);
  void insertSeparator(DockSeparator *sep);
  void removeSeparator();

  // Extremal region sizes
  //! Returns cached occupied space in \b this region along given \b direction.
  inline int getMaximumSize(bool direction) const {
    return m_maximumSize[direction];
  }
  //! Returns cached occupied space in \b this region along given \b direction.
  inline int getMinimumSize(bool direction) const {
    return m_minimumSize[direction];
  }

  bool addItemSize(DockWidget *item);
  bool subItemSize(DockWidget *item);
  void calculateExtremalSizes();
  int calculateMinimumSize(bool direction, bool recalcChildren);
  int calculateMaximumSize(bool direction, bool recalcChildren);

  // Redistribution function.
  // The main feature of a Region consists of the redistribute() method, which
  // extracts the optimal layout among the branching regions with \b this
  // root. However, only the full redistribute() method in DockLayout is made
  // public.
  void redistribute();
  void restoreGeometry();
  // void updateSeparators();
};

//========================================================================

//----------------------
//    Dock Allocator
//----------------------

//! DockDecoAllocator class handles the allocation of decoration elements used
//! by our dock manager.
/*!
  In order to implement custom appearances for the docking system, it
  is possible to customize both DockSeparator and DockPlaceholder classes. Since
  allocation of such objects is handled internally by the docking system,
  it is necessary to reimplement allocator functions whenever decoration
  classes change.
  In order to assign a DockDecoAllocator to a DockLayout or DockWidget, the
  respective 'setDecoAllocator' methods are provided.

  \sa DockLayout, DockWidget, DockSeparator and DockPlaceholder classes.
*/
class DockDecoAllocator {
  friend class DockLayout;

public:
  DockDecoAllocator() {}
  virtual ~DockDecoAllocator() {}

  // Customizabile allocators
  virtual DockSeparator *newSeparator(DockLayout *owner, bool orientation,
                                      Region *parentRegion);
  virtual DockPlaceholder *newPlaceholder(DockWidget *owner, Region *r, int idx,
                                          int attributes);

private:
  DockPlaceholder *newPlaceBuilt(DockWidget *owner, Region *r, int idx,
                                 int attributes);
};

#endif  // SIMPLEQTTEST_H
