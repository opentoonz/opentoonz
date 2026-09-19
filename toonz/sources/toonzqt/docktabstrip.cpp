

#include "docktabstrip.h"

#include "docklayout.h"
#include "toonzqt/gutil.h"

#include <QMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>

const int DockTabStrip::kHeight              = 26;
const int DockTabStrip::kUndockDragThreshold = 8;

namespace {

//! Accent color published by the active theme in its :TOONZCOLORS block,
//! like the icon colors. The fallback covers themes that predate it.
QColor themeAccentColor() {
  static const QColor fallback(0x4f, 0x7b, 0xc7);
  return ThemeManager::getInstance().getCustomPropertyColor("hl-color",
                                                            fallback);
}

}  // namespace

//========================================================

DockTabMergePreview::DockTabMergePreview(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint |
                          Qt::WindowTransparentForInput |
                          Qt::WindowDoesNotAcceptFocus) {
  setObjectName("DockTabMergePreview");
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setAutoFillBackground(false);
}

//-------------------------------------

QColor DockTabMergePreview::frameColor() const {
  return m_frameColor.isValid() ? m_frameColor : themeAccentColor();
}

//-------------------------------------

void DockTabMergePreview::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(frameColor(), 2));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(rect().adjusted(1, 1, -2, -2));
}

//========================================================

DockTabStrip::DockTabStrip(DockLayout *layout, Region *region, QWidget *parent)
    : QTabBar(parent)
    , m_layout(layout)
    , m_region(region)
    , m_pressIndex(-1)
    , m_dragOutStarted(false)
    , m_reordering(false)
    , m_insertionGapIndex(-1) {
  setObjectName("DockTabStrip");
  setDrawBase(false);
  setDocumentMode(true);
  setMovable(false);
  setExpanding(true);
  setUsesScrollButtons(true);
  setElideMode(Qt::ElideRight);

  QFont tabFont = font();
  tabFont.setBold(true);
  setFont(tabFont);

  connect(this, &QTabBar::currentChanged, this,
          &DockTabStrip::onCurrentChanged);
}

//-------------------------------------

QColor DockTabStrip::insertionMarkerColor() const {
  return m_insertionMarkerColor.isValid() ? m_insertionMarkerColor
                                          : themeAccentColor();
}

//-------------------------------------

void DockTabStrip::syncFromRegion() {
  blockSignals(true);
  while (count()) removeTab(0);

  if (!m_region || !m_region->hasTabGroup()) {
    blockSignals(false);
    return;
  }

  const std::vector<DockWidget *> &tabs = m_region->tabItems();
  for (unsigned int i = 0; i < tabs.size(); ++i) {
    QString title = tabs[i]->windowTitle();
    if (title.isEmpty()) title = tabs[i]->objectName();
    addTab(title);
  }

  setCurrentIndex(m_region->activeTabIndex());
  blockSignals(false);
}

//-------------------------------------

void DockTabStrip::onCurrentChanged(int index) {
  if (!m_layout || !m_region || index < 0 || m_dragOutStarted) return;
  m_layout->setActiveTab(m_region, index);
}

//-------------------------------------

// Equal widths, which setExpanding() does not give: it scales the sizeHint,
// so long titles stay wide. Below the floor the strip scrolls instead.
QSize DockTabStrip::tabSizeHint(int index) const {
  QSize hint           = QTabBar::tabSizeHint(index);
  const int tabCount   = count();
  const int stripWidth = width();
  if (tabCount > 0 && stripWidth > 0) {
    const int minTabWidth = 60;
    hint.setWidth(std::max(minTabWidth, stripWidth / tabCount));
  }
  return hint;
}

//-------------------------------------

// Mirrors the standalone-panel title-bar double-click-to-maximize behavior
// (see DockWidget::mouseDoubleClickEvent), since a tabbed panel's own title
// bar is hidden and replaced by this strip.
void DockTabStrip::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_layout && m_region) {
    if (DockWidget *active = m_region->activeTab())
      m_layout->setMaximized(active, !active->isMaximized());
  }
  event->accept();
}

//-------------------------------------

void DockTabStrip::paintEvent(QPaintEvent *event) {
  QTabBar::paintEvent(event);

  if (!m_reordering) return;

  QPainter painter(this);
  paintReorderInsertionMarker(painter);
}

//-------------------------------------

void DockTabStrip::paintReorderInsertionMarker(QPainter &painter) const {
  if (m_insertionGapIndex < 0 || m_insertionGapIndex > count() || !count())
    return;

  const int x = m_insertionGapIndex < count()
                    ? tabRect(m_insertionGapIndex).left()
                    : tabRect(count() - 1).right();

  const int markerWidth = 3;
  painter.fillRect(QRect(x - markerWidth / 2, 2, markerWidth, height() - 4),
                   insertionMarkerColor());
}

//-------------------------------------

bool DockTabStrip::isOutsideTabStrip(const QPoint &globalPos) const {
  QWidget *stripHost =
      parentWidget() ? parentWidget() : const_cast<DockTabStrip *>(this);
  const QRect hostRect(stripHost->mapToGlobal(QPoint(0, 0)), stripHost->size());
  return !hostRect.contains(globalPos);
}

//-------------------------------------

void DockTabStrip::tryBeginDragOut(const QPoint &globalPos) {
  if (m_dragOutStarted || !m_layout || !m_region || m_pressIndex < 0 ||
      m_pressIndex >= (int)m_region->tabItems().size())
    return;

  DockWidget *item = m_region->tabItems()[m_pressIndex];
  Region *region   = m_region;

  // Offset of the original click relative to the pressed tab's own
  // top-left; reused after undock to keep the same relative grab point
  // once the panel shows its own title bar instead of the tab.
  const QPoint grabOffsetInTab = m_pressPos - tabRect(m_pressIndex).topLeft();

  clearInsertionMarker();
  m_reordering     = false;
  m_dragOutStarted = true;
  releaseMouse();

  if (m_layout->detachTabForDrag(item, region, globalPos, grabOffsetInTab))
    return;

  m_dragOutStarted = false;
}

//-------------------------------------

int DockTabStrip::insertionGapAt(const QPoint &pos) const {
  const int n = count();
  if (n <= 0) return -1;

  for (int i = 0; i < n; ++i) {
    const QRect r = tabRect(i);
    if (!r.isValid()) continue;
    if (pos.x() < r.center().x()) return i;
  }
  return n;
}

//-------------------------------------

void DockTabStrip::setInsertionGapIndex(int gap) {
  if (gap == m_insertionGapIndex) return;
  m_insertionGapIndex = gap;
  update();
}

//-------------------------------------

void DockTabStrip::clearInsertionMarker() { setInsertionGapIndex(-1); }

//-------------------------------------

void DockTabStrip::commitTabReorder() {
  if (!m_layout || !m_region || !m_reordering) return;
  if (m_pressIndex < 0 || m_pressIndex >= count()) return;
  if (m_insertionGapIndex < 0 || m_insertionGapIndex > count()) return;

  // Dropping in the gaps immediately before/after the source is a no-op.
  if (m_insertionGapIndex == m_pressIndex ||
      m_insertionGapIndex == m_pressIndex + 1)
    return;

  // Convert insertion gap into a destination index (erase, then insert).
  int toIndex = m_insertionGapIndex;
  if (toIndex > m_pressIndex) --toIndex;
  m_layout->reorderTab(m_region, m_pressIndex, toIndex);
}

//-------------------------------------

void DockTabStrip::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_pressIndex     = tabAt(event->pos());
    m_pressPos       = event->pos();
    m_globalPressPos = event->globalPos();
    m_dragOutStarted = false;
    m_reordering     = false;
    clearInsertionMarker();

    if (m_pressIndex >= 0) {
      grabMouse();
      event->accept();
      return;
    }
  }

  QTabBar::mousePressEvent(event);
}

//-------------------------------------

void DockTabStrip::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton) || m_pressIndex < 0 ||
      m_dragOutStarted) {
    QTabBar::mouseMoveEvent(event);
    return;
  }

  const QPoint delta = event->globalPos() - m_globalPressPos;
  const int distance = std::max(std::abs(delta.x()), std::abs(delta.y()));

  if (distance >= kUndockDragThreshold) {
    const bool outsideStrip   = isOutsideTabStrip(event->globalPos());
    const bool verticalIntent = std::abs(delta.y()) > std::abs(delta.x());

    // Matches docked title-bar undock: any significant move detaches.
    // Horizontal moves inside the strip preview a reorder instead.
    if (outsideStrip || verticalIntent) {
      tryBeginDragOut(event->globalPos());
      event->accept();
      return;
    }

    if (!m_reordering) m_reordering = true;
  }

  if (m_reordering && !m_dragOutStarted) {
    if (isOutsideTabStrip(event->globalPos())) {
      tryBeginDragOut(event->globalPos());
      event->accept();
      return;
    }

    if (m_pressIndex >= count()) {
      QTabBar::mouseMoveEvent(event);
      return;
    }

    const int gap = insertionGapAt(event->pos());
    if (gap == m_pressIndex || gap == m_pressIndex + 1)
      clearInsertionMarker();
    else
      setInsertionGapIndex(gap);
  }

  event->accept();
}

//-------------------------------------

void DockTabStrip::mouseReleaseEvent(QMouseEvent *event) {
  const bool wasReorder =
      m_reordering && !m_dragOutStarted && m_pressIndex >= 0;
  const bool wasClick = !m_dragOutStarted && !m_reordering &&
                        m_pressIndex >= 0 && m_pressIndex < count();

  if (wasReorder) commitTabReorder();

  releaseMouse();

  if (wasClick) setCurrentIndex(m_pressIndex);

  clearInsertionMarker();
  m_pressIndex     = -1;
  m_dragOutStarted = false;
  m_reordering     = false;

  QTabBar::mouseReleaseEvent(event);
}
