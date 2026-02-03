

#include "toonzqt/dvscrollwidget.h"

// TnzQt includes
#include "toonzqt/freelayout.h"

// Qt includes
#include <QLayout>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QTimer>

// STD includes
#include <numeric>

//****************************************************************************
//    Local namespace stuff
//****************************************************************************

namespace {

class ScrollLayout final : public DummyLayout {
  DvScrollWidget *m_scrollWidget;

public:
  ScrollLayout(DvScrollWidget *scrollWidget) : m_scrollWidget(scrollWidget) {
    assert(m_scrollWidget);
  }

  QSize minimumSize() const override {
    struct locals {
      inline static QSize expand(const QSize &size, const QLayoutItem *item) {
        return size.expandedTo(item->minimumSize());
      }
    };

    QSize minSize = std::accumulate(m_items.begin(), m_items.end(), QSize(),
                                    locals::expand);

    return (m_scrollWidget->getOrientation() == Qt::Horizontal)
               ? QSize(0, minSize.height())
               : QSize(minSize.width(), 0);
  }

  QSize maximumSize() const override {
    struct locals {
      inline static QSize bound(const QSize &size, const QLayoutItem *item) {
        return size.boundedTo(item->minimumSize());
      }
    };

    QSize maxSize =
        std::accumulate(m_items.begin(), m_items.end(),
                        QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX), locals::bound);

    return (m_scrollWidget->getOrientation() == Qt::Horizontal)
               ? QSize(QWIDGETSIZE_MAX, maxSize.height())
               : QSize(maxSize.width(), QWIDGETSIZE_MAX);
  }

  void setGeometry(const QRect &r) override {
    const Qt::Orientation orientation = m_scrollWidget->getOrientation();

    QList<QLayoutItem *>::const_iterator it, iEnd = m_items.end();
    for (it = m_items.begin(); it != iEnd; ++it) {
      QLayoutItem *item = *it;

      QSize targetSize = item->sizeHint();

      if (orientation & item->expandingDirections()) {
        if (orientation & Qt::Horizontal)
          targetSize.setWidth(r.width());
        else
          targetSize.setHeight(r.height());
      }

      const QSize &minSize = item->minimumSize(),
                  &maxSize = item->maximumSize();

      targetSize.setWidth(
          tcrop(targetSize.width(), minSize.width(), maxSize.width()));
      targetSize.setHeight(
          tcrop(targetSize.height(), minSize.height(), maxSize.height()));

      const QRect &geom = item->geometry();

      if (geom.size() != targetSize)
        item->setGeometry(QRect(geom.topLeft(), targetSize));
    }

    m_scrollWidget->scroll(0);  // Refresh scroll buttons visibility
  }
};

//==============================================================================

qreal heldScrollEasing(qreal progress) {
  // Equilibrate sum of Linear and InQuad
  return 0.5 * progress * (1.0 + progress);
}

}  // namespace

//****************************************************************************
//    DvScrollWidget implementation
//****************************************************************************

DvScrollWidget::DvScrollWidget(QWidget *parent, Qt::Orientation orientation)
    : QFrame(parent)
    , m_content(0)
    , m_animation(0)
    , m_clickEase(QEasingCurve::OutCubic)
    , m_holdEase(QEasingCurve::Linear)
    , m_backwardTimer(new QTimer(this))
    , m_forwardTimer(new QTimer(this))
    , m_pressed(false)
    , m_heldRelease(false)
    , m_heldClick(false) {
  ScrollLayout *scrollLayout = new ScrollLayout(this);
  setLayout(scrollLayout);

  // At the toolbar sides, add scroll buttons
  m_scrollBackward =
      new QPushButton(this);  // NOTE: Not being managed by the scroll layout.
  m_scrollBackward->setFixedSize(
      24, 24);  //       It's not necessary. They are not part
  m_scrollBackward->setFocusPolicy(Qt::NoFocus);  //       of the content.

  m_scrollForward =
      new QPushButton(this);  //       Observe that the parent widget must
  m_scrollForward->setFixedSize(24,
                                24);  //       therefore be explicitly supplied.
  m_scrollForward->setFocusPolicy(Qt::NoFocus);

  setOrientation(orientation);

  m_scrollBackward->move(0, 0);

  m_backwardTimer->setInterval(450);
  m_forwardTimer->setInterval(450);
  m_backwardTimer->setSingleShot(true);
  m_forwardTimer->setSingleShot(true);

  connect(m_scrollBackward, SIGNAL(clicked(bool)), this,
          SLOT(scrollBackward()));
  connect(m_scrollForward, SIGNAL(clicked(bool)), this, SLOT(scrollForward()));
  connect(m_backwardTimer, SIGNAL(timeout()), this, SLOT(holdBackward()));
  connect(m_forwardTimer, SIGNAL(timeout()), this, SLOT(holdForward()));

  connect(m_scrollBackward, SIGNAL(pressed()), m_backwardTimer, SLOT(start()));
  connect(m_scrollForward, SIGNAL(pressed()), m_forwardTimer, SLOT(start()));
  connect(m_scrollBackward, SIGNAL(released()), this, SLOT(releaseBackward()));
  connect(m_scrollForward, SIGNAL(released()), this, SLOT(releaseForward()));
}

//------------------------------------------------------------------------------

void DvScrollWidget::setWidget(QWidget *widget) {
  // Delete currently set widget, if any
  QLayout *lay = layout();

  while (QLayoutItem *item = lay->takeAt(
             0))  // Should be 1 item only - while is just to be pedant
  {
    assert(item->widget());

    delete item->widget();  // The item DOES NOT own the widget.
    delete item;            // The parent widget (this) does - this
  }                         // is by Qt's manual.

  // Add widget
  lay->addWidget(widget);
  m_content = widget;
  m_content->lower();  // Needs to be below the scroll buttons.
                       // Seemingly not working on Mac (see showEvent).

  assert(widget->parent() == this);

  // Use animations to make scrolling 'smooth'
  delete m_animation;
  m_animation = new QPropertyAnimation(m_content, "pos");
  connect(m_animation, SIGNAL(stateChanged(QAbstractAnimation::State,
                                           QAbstractAnimation::State)),
          this, SLOT(updateButtonsVisibility()));
}

//------------------------------------------------------------------------------

void DvScrollWidget::setOrientation(Qt::Orientation orientation) {
  if ((m_horizontal = (orientation == Qt::Horizontal))) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_scrollBackward->setObjectName("ScrollLeftButton");
    m_scrollForward->setObjectName("ScrollRightButton");
  } else {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    m_scrollBackward->setObjectName("ScrollUpButton");
    m_scrollForward->setObjectName("ScrollDownButton");
  }
}

//------------------------------------------------------------------------------

Qt::Orientation DvScrollWidget::getOrientation() const {
  return m_horizontal ? Qt::Horizontal : Qt::Vertical;
}

//------------------------------------------------------------------------------

void DvScrollWidget::setEasing(QEasingCurve clickEase,
                               QEasingCurve holdPressEase) {
  m_clickEase = clickEase;
  m_holdEase  = holdPressEase;
}

//------------------------------------------------------------------------------

void DvScrollWidget::scroll(int dx, int duration, QEasingCurve ease) {
  if (m_content)
    scrollTo(m_horizontal ? m_content->x() + dx : m_content->y() + dx, duration,
             ease);
}

//------------------------------------------------------------------------------

void DvScrollWidget::scrollTo(int pos, int duration, QEasingCurve ease) {
  if (!m_content) return;

  // Retrieve the contents' bounds
  QRect bounds(m_content->pos(), m_content->size());

  QPoint newPos;
  if (m_horizontal) {
    newPos     = QPoint(pos, 0);
    int minPos = width() - bounds.width();

    if (newPos.x() <= minPos) newPos.setX(minPos);
    if (newPos.x() >= 0) newPos.setX(0);
  } else {
    newPos     = QPoint(0, pos);
    int minPos = height() - bounds.height();

    if (newPos.y() <= minPos) newPos.setY(minPos);
    if (newPos.y() >= 0) newPos.setY(0);
  }

  if (duration > 0) {
    m_animation->stop();
    m_animation->setEasingCurve(ease);
    m_animation->setStartValue(bounds.topLeft());
    m_animation->setEndValue(newPos);
    m_animation->setDuration(duration);
    m_animation->start();
  } else {
    m_content->move(newPos);
    updateButtonsVisibility();
  }
}

//------------------------------------------------------------------------------

void DvScrollWidget::updateButtonsVisibility() {
  if ((!m_content) || (m_animation->state() == QPropertyAnimation::Running))
    return;

  QRect bounds(m_content->pos(), m_content->size());
  if (m_horizontal) {
    if (bounds.right() <= width()) {
      m_scrollForward->setDown(false);
      m_scrollForward->hide();
      m_heldRelease = m_heldClick = false;
    } else
      m_scrollForward->show();

    if (bounds.left() >= 0) {
      m_scrollBackward->setDown(false);
      m_scrollBackward->hide();
      m_heldRelease = m_heldClick = false;
    } else
      m_scrollBackward->show();
  } else {
    if (bounds.bottom() <= height()) {
      m_scrollForward->setDown(false);
      m_scrollForward->hide();
      m_heldRelease = m_heldClick = false;
    } else
      m_scrollForward->show();

    if (bounds.top() >= 0) {
      m_scrollBackward->setDown(false);
      m_scrollBackward->hide();
      m_heldRelease = m_heldClick = false;
    } else
      m_scrollBackward->show();
  }
}

//------------------------------------------------------------------------------

void DvScrollWidget::showEvent(QShowEvent *se) {
  // These are necessary on Mac.
  m_scrollBackward->raise();
  m_scrollForward->raise();
}

//------------------------------------------------------------------------------

void DvScrollWidget::resizeEvent(QResizeEvent *re) {
  QWidget::resizeEvent(re);
  scroll(0);
  if (m_horizontal) {
    m_scrollBackward->setFixedSize(m_scrollBackward->width(), height());
    m_scrollForward->setFixedSize(m_scrollForward->width(), height());
    m_scrollForward->move(re->size().width() - m_scrollForward->width(), 0);
  } else {
    m_scrollBackward->setFixedSize(width(), m_scrollBackward->height());
    m_scrollForward->setFixedSize(width(), m_scrollForward->height());
    m_scrollForward->move(0, re->size().height() - m_scrollForward->height());
  }
}

//------------------------------------------------------------------------------

void DvScrollWidget::mousePressEvent(QMouseEvent *me) {
  m_pressed  = true;
  m_mousePos = m_horizontal ? me->x() : me->y();
  me->accept();
}

//------------------------------------------------------------------------------

void DvScrollWidget::mouseMoveEvent(QMouseEvent *me) {
  if (!m_pressed) return;

  if (m_horizontal) {
    scroll(me->x() - m_mousePos);
    m_mousePos = me->x();
  } else {
    scroll(me->y() - m_mousePos);
    m_mousePos = me->y();
  }
  me->accept();
}

//------------------------------------------------------------------------------

void DvScrollWidget::mouseReleaseEvent(QMouseEvent *me) {
  m_pressed = false;
  me->accept();
}

//------------------------------------------------------------------------------

void DvScrollWidget::holdBackward() {
  if (!m_content) return;

  m_heldRelease = m_heldClick = true;

  QRect bounds(m_content->pos(), m_content->size());
  int spaceLeft = -(m_horizontal ? bounds.left() : bounds.top());

  QEasingCurve ease;
  ease.setCustomType(&heldScrollEasing);
  scrollTo(0, spaceLeft * 10, ease);  // 100 pix per second
}

//------------------------------------------------------------------------------

void DvScrollWidget::holdForward() {
  if (!m_content) return;

  m_heldRelease = m_heldClick = true;

  QRect bounds(m_content->pos(), m_content->size());
  int pos =
      m_horizontal ? width() - bounds.width() : height() - bounds.height();
  int spaceLeft = (m_horizontal ? bounds.left() : bounds.top()) - pos;

  QEasingCurve ease;
  ease.setCustomType(&heldScrollEasing);
  scrollTo(pos, spaceLeft * 10, ease);  // 100 pix per second
}

//------------------------------------------------------------------------------

void DvScrollWidget::releaseBackward() {
  m_backwardTimer->stop();

  if (m_heldRelease) m_animation->stop();

  m_heldRelease = false;
}

//------------------------------------------------------------------------------

void DvScrollWidget::releaseForward() {
  m_forwardTimer->stop();

  if (m_heldRelease) m_animation->stop();

  m_heldRelease = false;
}

//------------------------------------------------------------------------------

void DvScrollWidget::scrollBackward() {
  if (!m_heldClick) scroll(0.5 * (m_horizontal ? width() : height()), 300);

  m_heldClick = false;
}

//------------------------------------------------------------------------------

void DvScrollWidget::scrollForward() {
  if (!m_heldClick) scroll(-0.5 * (m_horizontal ? width() : height()), 300);

  m_heldClick = false;
}
