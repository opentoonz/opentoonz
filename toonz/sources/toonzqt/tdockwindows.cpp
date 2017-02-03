

#include "tdockwindows.h"
#include <QBoxLayout>
#include <QVBoxLayout>

#include <QRegion>

#include <QPainter>
#include <QStyleOption>

#include <QMouseEvent>

//----------------------------------------

//-------------------
//    Decorations
//-------------------

class TDockDecoAllocator final : public DockDecoAllocator {
  DockSeparator *newSeparator(DockLayout *owner, bool orientation,
                              Region *parentRegion) override;
  DockPlaceholder *newPlaceholder(DockWidget *owner, Region *r, int idx,
                                  int attributes) override;
};

//========================================================================

//-----------------------
//    TMainWindow
//-----------------------

TMainWindow::TMainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags) {
  // Delete on close
  setAttribute(Qt::WidgetAttribute(Qt::WA_DeleteOnClose));

  // Set a vertical layout to include menu bars
  QVBoxLayout *vlayout = new QVBoxLayout;
  vlayout->setMargin(0);
  vlayout->setSpacing(4);
  setLayout(vlayout);

  // Allocate the dock layout
  m_layout = new DockLayout;
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(4);
  m_layout->setDecoAllocator(new TDockDecoAllocator);
  vlayout->addLayout(m_layout);
  vlayout->setAlignment(m_layout, Qt::AlignTop);

  show();  // NOTA: E' NECESSARIO MOSTRARE LA FINESTRA, prima di dockare
           // qualcosa (altrimenti non viene fatto
  // l'update della geometria della main window, e il contentsRect del layout
  // viene sballato!!).
}

//----------------------------------------

TMainWindow::~TMainWindow() {}

//----------------------------------------

//! Adds input \b item to this TMainWindow. If item was already
//! assigned to \b this TMainWindow, nothing happens.
void TMainWindow::addDockWidget(TDockWidget *item) {
  if (!m_layout->find(item)) m_layout->addWidget(item);
}

//----------------------------------------

void TMainWindow::removeDockWidget(TDockWidget *item) {
  m_layout->removeWidget(item);
}

//----------------------------------------

// NOTE: Unlike QMainWindow::addToolBar, we only allow one
// fixed-size undockable menu bar at top of the dock layout.
void TMainWindow::setMenuWidget(QWidget *menubar) {
  if (menubar) {
    QVBoxLayout *vlayout = static_cast<QVBoxLayout *>(layout());

    // If necessary, remove current menu bar
    if (m_menu && m_menu != menubar) vlayout->removeWidget(m_menu);

    vlayout->insertWidget(0, menubar);
  }
}

//----------------------------------------

void TMainWindow::setDecoAllocator(DockDecoAllocator *allocator) {
  m_layout->setDecoAllocator(allocator);
}

//----------------------------------------

//! Sets global thickness of separators between dock widget.
void TMainWindow::setSeparatorsThickness(int thick) {
  if (thick > 0) {
    m_layout->setSpacing(thick);
    m_layout->redistribute();
  }
}

//----------------------------------------

void TMainWindow::resizeEvent(QResizeEvent *event) { m_layout->redistribute(); }

//========================================================================

//-------------------
//    TDockWidget
//-------------------

//! Constructs a TDockWidget with given parent and window flags. If parent is
//! a TMainWindow, then the constructed dock widget is assigned to it
//! (addDockWidget'd).
//! TDockWidgets are always floating at construction.
TDockWidget::TDockWidget(QWidget *parent, Qt::WindowFlags flags)
    : DockWidget(parent, flags), m_widget(0), m_titlebar(0), m_margin(5) {
  setAutoFillBackground(false);
  // setFrameStyle(QFrame::StyledPanel);

  QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
  layout->setSpacing(0);
  setLayout(layout);

  // Check if parent is a TMainWindow class
  TMainWindow *parentMain = qobject_cast<TMainWindow *>(parent);
  if (parentMain) parentMain->addDockWidget(this);

  setDecoAllocator(new TDockDecoAllocator);
}

//----------------------------------------

TDockWidget::TDockWidget(const QString &title, QWidget *parent,
                         Qt::WindowFlags flags)
    : DockWidget(parent, flags), m_widget(0), m_titlebar(0), m_margin(5) {
  setWindowTitle(title);

  QBoxLayout *layout = new QBoxLayout(QBoxLayout::TopToBottom);
  layout->setSpacing(0);
  setLayout(layout);
}

//----------------------------------------

void TDockWidget::setTitleBarWidget(QWidget *titlebar) {
  if (titlebar) {
    QBoxLayout *boxLayout = static_cast<QBoxLayout *>(layout());

    if (m_titlebar && m_titlebar != titlebar)
      boxLayout->removeWidget(m_titlebar);

    boxLayout->insertWidget(0, titlebar);
    // Set top/left-aligned
    boxLayout->setAlignment(
        titlebar, getOrientation() == vertical ? Qt::AlignTop : Qt::AlignLeft);

    m_titlebar = titlebar;
    if (m_floating) setFloatingAppearance();
  }
}

//----------------------------------------

void TDockWidget::windowTitleEvent(QEvent *e) {
  if (m_titlebar) m_titlebar->update();
}

//----------------------------------------

void TDockWidget::setWidget(QWidget *widget) {
  if (widget) {
    QBoxLayout *boxLayout = static_cast<QBoxLayout *>(layout());

    if (m_widget && m_widget != widget) boxLayout->removeWidget(m_widget);

    boxLayout->insertWidget(1, widget);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_widget = widget;
  }
}

//----------------------------------------

void TDockWidget::setFloatingAppearance() {
  if (m_titlebar) {
    // If has a custom title bar, impose a margin to the layout
    // to provide a frame.
    layout()->setMargin(m_margin);

    if (!m_floating)  // was docked
    {
      // Adding margin to extremal sizes
      int addition = 2 * m_margin;
      setMinimumSize(
          QSize(minimumWidth() + addition, minimumHeight() + addition));
      setMaximumSize(
          QSize(maximumWidth() + addition, maximumHeight() + addition));
    }
  }
  // else
  //  setWindowFlags(Qt::Tool);
}

//----------------------------------------

void TDockWidget::setDockedAppearance() {
  // No layout margin is visible when docked
  layout()->setMargin(0);

  if (m_floating)  // was floating
  {
    // Removing margin from extremal sizes
    int addition = 2 * m_margin;
    setMinimumSize(
        QSize(minimumWidth() - addition, minimumHeight() - addition));
    setMaximumSize(
        QSize(maximumWidth() - addition, maximumHeight() - addition));
  }
}

//----------------------------------------

bool TDockWidget::isDragGrip(QPoint p) {
  if (!m_titlebar) return DockWidget::isDragGrip(p);

  return m_titlebar->geometry().contains(p);
}

//----------------------------------------

int TDockWidget::isResizeGrip(QPoint p) {
  if (m_dragging || (!m_titlebar && m_floating)) return 0;

  int marginType = 0;
  QRect geom(QPoint(0, 0), QPoint(width(), height()));
  int margin = layout()->margin();
  QRect contGeom(geom.adjusted(margin, margin, -margin, -margin));

  if (geom.contains(p) && !contGeom.contains(p)) {
    if (p.x() < 15) marginType |= leftMargin;
    if (p.y() < 15) marginType |= topMargin;
    if (p.x() > width() - 15) marginType |= rightMargin;
    if (p.y() > height() - 15) marginType |= bottomMargin;
  }

  return marginType;
}

//----------------------------------------

//! Currently working only for \b status = true. If you need to
//! dock a TDockWidget, you \b must specify a dock location by either
//! choosing a placeholder or identifying the Region and insertion index,
//! and then running 'parentLayout()->dockItem(..)'.
void TDockWidget::setFloating(bool status) {
  if (status) {
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    if (!m_floating) parentLayout()->undockItem(this);
  }
}

//----------------------------------------

//! Specifies the orientation of the dock widget. It can
//! be either \b vertical (default) or \b horizontal, meaning that
//! the titlebar is laid respectively at the top or left side of
//! content widget. Directly speaking, it is equivalent to setting the
//! Qt's QDockWidget::DockWidgetVerticalTitleBar feature.
void TDockWidget::setOrientation(bool direction) {
  QBoxLayout *boxLayout = static_cast<QBoxLayout *>(layout());
  QBoxLayout::Direction boxDirection;

  if (direction == vertical) {
    boxLayout->setAlignment(m_titlebar, Qt::AlignTop);
    boxDirection = QBoxLayout::TopToBottom;
  } else {
    boxLayout->setAlignment(m_titlebar, Qt::AlignLeft);
    boxDirection = QBoxLayout::LeftToRight;
  }

  boxLayout->setDirection(boxDirection);
}

//----------------------------------------

bool TDockWidget::getOrientation() const {
  QBoxLayout *boxLayout = static_cast<QBoxLayout *>(layout());

  return (boxLayout->direction() == QBoxLayout::TopToBottom) ? vertical
                                                             : horizontal;
}

//----------------------------------------

//! Maximizes \b this TDockWidget, if docked.
void TDockWidget::setMaximized(bool status) {
  parentLayout()->setMaximized(this, status);
}

//----------------------------------------

QSize TDockWidget::getDockedMinimumSize() {
  int addedSize = 2 * m_margin;
  return m_floating ? minimumSize() -= QSize(addedSize, addedSize)
                    : minimumSize();
}

//----------------------------------------

QSize TDockWidget::getDockedMaximumSize() {
  int addedSize = 2 * m_margin;
  return m_floating ? maximumSize() -= QSize(addedSize, addedSize)
                    : maximumSize();
}

//========================================================================

//--------------------------
//    Custom Decorations
//--------------------------

class TDockSeparator final : public DockSeparator {
public:
  TDockSeparator(DockLayout *owner, bool orientation, Region *parentRegion)
      : DockSeparator(owner, orientation, parentRegion) {}

  void paintEvent(QPaintEvent *pe) override;
};

//----------------------------------------

class TDockPlaceholder final : public DockPlaceholder {
  QWidget *m_associated[3];

public:
  TDockPlaceholder(DockWidget *owner, Region *r, int idx, int attributes);
  ~TDockPlaceholder();

  void buildGeometry() override;

  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *he) override;
};

//----------------------------------------

TDockPlaceholder::TDockPlaceholder(DockWidget *owner, Region *r, int idx,
                                   int attributes)
    : DockPlaceholder(owner, r, idx, attributes) {
  setAutoFillBackground(true);

  setObjectName("TDockPlaceholder");

  setWindowOpacity(0.8);
}

//----------------------------------------

TDockPlaceholder::~TDockPlaceholder() {
  if (isRoot()) {
    delete m_associated[0];
    delete m_associated[1];
    delete m_associated[2];
  }
}

//----------------------------------------

inline void TDockPlaceholder::buildGeometry() {
  DockPlaceholder::buildGeometry();

  if (isRoot()) {
    // Solution 2: Set associated widgets
    QRect geom(geometry());
    QSize horSize(geom.width(), 6);
    QSize vertSize(6, geom.height() + 12);

    setGeometry(QRect(geom.topLeft() - QPoint(6, 6), vertSize));

    m_associated[0] = new TDockPlaceholder(0, 0, 0, 0);
    m_associated[0]->setGeometry(QRect(geom.topLeft() - QPoint(0, 6), horSize));

    m_associated[1] = new TDockPlaceholder(0, 0, 0, 0);
    m_associated[1]->setGeometry(
        QRect(geom.topRight() + QPoint(1, -6), vertSize));

    m_associated[2] = new TDockPlaceholder(0, 0, 0, 0);
    m_associated[2]->setGeometry(
        QRect(geom.bottomLeft() + QPoint(0, 1), horSize));
  }
}

//-------------------------------------

void TDockPlaceholder::showEvent(QShowEvent *se) {
  if (isRoot()) {
    // Show associated widgets
    m_associated[0]->show();
    m_associated[1]->show();
    m_associated[2]->show();
  }
}

//-------------------------------------

void TDockPlaceholder::hideEvent(QHideEvent *he) {
  if (isRoot()) {
    // Show associated widgets
    m_associated[0]->hide();
    m_associated[1]->hide();
    m_associated[2]->hide();
  }
}

//-------------------------------------

void TDockSeparator::paintEvent(QPaintEvent *pe) {
  QPainter p(this);
  QStyleOption opt(0);
  opt.state = (getOrientation() == Region::horizontal)
                  ? QStyle::State_None
                  : QStyle::State_Horizontal;

  /*if (w->isEnabled())
opt.state |= QStyle::State_Enabled;
if (o != Qt::Horizontal)
opt.state |= QStyle::State_Horizontal;
if (mouse_over)
opt.state |= QStyle::State_MouseOver;*/

  opt.rect    = QRect(QPoint(0, 0), QSize(geometry().size()));
  opt.palette = palette();

  style()->drawPrimitive(QStyle::PE_IndicatorDockWidgetResizeHandle, &opt, &p,
                         this);

  p.end();
}

//-------------------------------------

DockSeparator *TDockDecoAllocator::newSeparator(DockLayout *owner,
                                                bool orientation,
                                                Region *parentRegion) {
  return new TDockSeparator(owner, orientation, parentRegion);
}

//-------------------------------------

DockPlaceholder *TDockDecoAllocator::newPlaceholder(DockWidget *owner,
                                                    Region *r, int idx,
                                                    int attributes) {
  return new TDockPlaceholder(owner, r, idx, attributes);
}

//-------------------------------------

void TDockWidget::selectDockPlaceholder(QMouseEvent *me) {
  if (m_placeholders.size() && m_placeholders[0]->isRoot()) {
    DockPlaceholder *selected = 0;

    QPoint pos = parentWidget()->mapFromGlobal(me->globalPos());
    if (parentLayout()->contentsRect().contains(pos))
      selected = m_placeholders[0];

    if (m_selectedPlace != selected) {
      if (m_selectedPlace) m_selectedPlace->hide();
      if (selected) selected->show();
    }

    m_selectedPlace = selected;
  } else
    DockWidget::selectDockPlaceholder(me);
}
