

#include "docktabstrip.h"

#include "docklayout.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QToolButton>
#include <algorithm>
#include <cmath>

namespace {

// Samples the actual color the active theme paints for a checked/selected
// tool button (e.g. the selected tool in the toolbar, or a selected preset
// in the Brush Presets panel) so the hover-join frame always matches the
// current theme's accent, whatever it is.
QColor dockThemeAccentColor() {
  static const QColor kFallback(0x7f, 0xdb, 0xfc);

  static QToolButton *probe = 0;
  if (!probe) {
    probe = new QToolButton();
    probe->setAttribute(Qt::WA_DontShowOnScreen);
    probe->setCheckable(true);
    probe->setChecked(true);
    probe->setAutoRaise(false);
    probe->resize(20, 20);
  }

  probe->style()->unpolish(probe);
  probe->style()->polish(probe);
  probe->ensurePolished();

  QImage img(probe->size(), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter painter(&img);
  QStyleOptionToolButton opt;
  opt.initFrom(probe);
  opt.state |= QStyle::State_On | QStyle::State_Raised | QStyle::State_Enabled;
  opt.rect = probe->rect();
  probe->style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter,
                                     probe);
  painter.end();

  const QColor sampled = img.pixelColor(img.width() / 2, img.height() / 2);
  if (sampled.alpha() > 0) return sampled;

  return kFallback;
}

}  // namespace

const int DockTabStrip::kHeight              = 26;
const int DockTabStrip::kUndockDragThreshold = 8;

//========================================================

DockJoinHighlight::DockJoinHighlight(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint |
                          Qt::WindowTransparentForInput |
                          Qt::WindowDoesNotAcceptFocus) {
  setObjectName("DockJoinHighlight");
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setAutoFillBackground(false);
}

//-------------------------------------

void DockJoinHighlight::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  QPen pen(dockThemeAccentColor(), 2);
  painter.setPen(pen);
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
    , m_reordering(false) {
  setObjectName("DockTabStrip");
  setDrawBase(false);
  setDocumentMode(true);
  setMovable(false);
  setExpanding(false);
  setUsesScrollButtons(true);
  setElideMode(Qt::ElideRight);

  QFont tabFont = font();
  tabFont.setBold(true);
  setFont(tabFont);

  connect(this, &QTabBar::currentChanged, this,
          &DockTabStrip::onCurrentChanged);
}

//-------------------------------------

void DockTabStrip::syncFromRegion() {
  blockSignals(true);
  while (count()) removeTab(0);

  if (!m_region || !m_region->isTabbed()) {
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

  updateTabTextColors();
}

//-------------------------------------

// Dims inactive tab titles so the active tab (left at full theme opacity)
// stands out clearly. Only the inactive color is overridden here, so the
// active tab keeps whatever accent color the current theme's QSS assigns
// to QTabBar::tab:selected (which may vary per theme).
void DockTabStrip::updateTabTextColors() {
  const int current = currentIndex();
  for (int i = 0; i < count(); ++i) {
    if (i == current) {
      setTabTextColor(i, QColor());  // Reset override: use theme's :selected color.
      continue;
    }
    QColor dimmed = palette().color(QPalette::WindowText);
    dimmed.setAlphaF(0.6);
    setTabTextColor(i, dimmed);
  }
}

//-------------------------------------

void DockTabStrip::onCurrentChanged(int index) {
  updateTabTextColors();
  if (!m_layout || !m_region || index < 0 || m_dragOutStarted) return;
  m_layout->setActiveTab(m_region, index);
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

  m_dragOutStarted = true;
  releaseMouse();

  if (m_layout->beginTabDragOut(item, region, globalPos, grabOffsetInTab))
    return;

  m_dragOutStarted = false;
}

//-------------------------------------

void DockTabStrip::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_pressIndex      = tabAt(event->pos());
    m_pressPos        = event->pos();
    m_globalPressPos  = event->globalPos();
    m_dragOutStarted  = false;
    m_reordering      = false;

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
  const int distance =
      std::max(std::abs(delta.x()), std::abs(delta.y()));

  if (distance >= kUndockDragThreshold) {
    const bool outsideStrip = isOutsideTabStrip(event->globalPos());
    const bool verticalIntent =
        std::abs(delta.y()) > std::abs(delta.x());

    // Same spirit as docked title-bar undock: any significant move can detach.
    // Horizontal moves inside the strip reorder tabs instead.
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

    int srcIndex = m_pressIndex;
    if (srcIndex < 0 || srcIndex >= count()) {
      QTabBar::mouseMoveEvent(event);
      return;
    }

    int dstIndex = tabAt(event->pos());
    if (dstIndex >= 0 && dstIndex < count() && dstIndex != srcIndex) {
      QRect srcRect = tabRect(srcIndex);
      int x         = event->pos().x();
      if (x < srcRect.left() || x > srcRect.right())
        m_layout->moveTab(m_region, srcIndex, dstIndex);
    }
  }

  event->accept();
}

//-------------------------------------

void DockTabStrip::mouseReleaseEvent(QMouseEvent *event) {
  const bool wasClick =
      !m_dragOutStarted && m_pressIndex >= 0 && m_pressIndex < count();

  releaseMouse();

  if (wasClick) setCurrentIndex(m_pressIndex);

  m_pressIndex     = -1;
  m_dragOutStarted = false;
  m_reordering     = false;

  QTabBar::mouseReleaseEvent(event);
}
