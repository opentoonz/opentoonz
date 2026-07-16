

#include "docktabstrip.h"

#include "docklayout.h"
#include "toonzqt/gutil.h"

#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionTab>
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

// QTabBar::initStyleOption() is protected; this tiny subclass exposes it
// so the probe functions below can build a QStyleOptionTab that exactly
// matches what the real tab bar would hand to the style/stylesheet.
class ProbeTabBar final : public QTabBar {
public:
  using QTabBar::QTabBar;
  void initOption(QStyleOptionTab *opt, int index) const {
    initStyleOption(opt, index);
  }
};

// Renders a single tab's text label through the live style/stylesheet onto
// a transparent surface (CE_TabBarTabLabel paints only the label, not the
// tab background), so the returned color is whatever the active theme
// actually resolves for that state - no theme is ever assumed or hardcoded.
QColor probeTabTextColor(ProbeTabBar &probe, int index) {
  QStyleOptionTab opt;
  probe.initOption(&opt, index);

  QImage img(opt.rect.size(), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter painter(&img);
  painter.translate(-opt.rect.topLeft());
  probe.style()->drawControl(QStyle::CE_TabBarTabLabel, &opt, &painter,
                             &probe);
  painter.end();

  long long r = 0, g = 0, b = 0, n = 0;
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      const QColor c = img.pixelColor(x, y);
      if (c.alpha() < 40) continue;  // skip anti-aliased/background pixels.
      r += c.red();
      g += c.green();
      b += c.blue();
      ++n;
    }
  }
  if (n == 0) return QColor();
  return QColor(int(r / n), int(g / n), int(b / n));
}

// Renders just a tab's background/border (CE_TabBarTabShape, no label)
// through the live style, then samples a corner pixel far from any text -
// this is the true background tone that theme paints for that tab, used as
// the fallback wash color instead of any assumed/hardcoded value.
QColor probeTabBackgroundColor(ProbeTabBar &probe, int index) {
  QStyleOptionTab opt;
  probe.initOption(&opt, index);

  QImage img(opt.rect.size(), QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  QPainter painter(&img);
  painter.translate(-opt.rect.topLeft());
  probe.style()->drawControl(QStyle::CE_TabBarTabShape, &opt, &painter,
                             &probe);
  painter.end();

  const int x = std::min(4, img.width() - 1);
  const int y = std::max(img.height() - 4, 0);
  const QColor sampled = img.pixelColor(x, y);
  return sampled.alpha() > 0 ? sampled : QColor(Qt::transparent);
}

// Measures - by actually rendering through the live stylesheet, not by
// assuming anything about any particular theme - whether the current theme
// already gives a selected tab's text a visibly different color from an
// unselected one. Bundled themes mostly do (dimmed alpha vs. opaque), but a
// couple leave both states identical; this only kicks in for those, using a
// wash color sampled from that same theme rather than a fixed hardcoded hue.
struct TabTextContrast {
  bool needsWash = false;
  QColor washColor;
};

const TabTextContrast &dockTabTextContrast() {
  static QString cachedStyleSheet;
  static TabTextContrast cached;

  const QString liveStyleSheet = qApp ? qApp->styleSheet() : QString();
  if (!cachedStyleSheet.isNull() && liveStyleSheet == cachedStyleSheet)
    return cached;
  cachedStyleSheet = liveStyleSheet;

  TabBarContainter container;
  container.setAttribute(Qt::WA_DontShowOnScreen);
  container.resize(160, 28);

  ProbeTabBar probe(&container);
  probe.setDrawBase(false);
  probe.setDocumentMode(true);
  probe.addTab(QStringLiteral("Ag"));
  probe.addTab(QStringLiteral("Ag"));
  probe.resize(160, 28);
  probe.setCurrentIndex(0);  // Tab 0 selected, tab 1 is the plain state.
  container.ensurePolished();
  probe.ensurePolished();

  const QColor selectedColor   = probeTabTextColor(probe, 0);
  const QColor unselectedColor = probeTabTextColor(probe, 1);

  if (!selectedColor.isValid() || !unselectedColor.isValid()) {
    cached.needsWash = false;
  } else {
    const int distance = std::abs(selectedColor.red() - unselectedColor.red()) +
                         std::abs(selectedColor.green() - unselectedColor.green()) +
                         std::abs(selectedColor.blue() - unselectedColor.blue());
    cached.needsWash = distance < 24;  // near-identical: theme doesn't dim.
  }
  // Wash toward the tab's own (unselected) background tone, sampled live,
  // so the fallback still adapts to whatever theme triggered it.
  const QColor bg = probeTabBackgroundColor(probe, 1);
  cached.washColor = bg.alpha() > 0 ? bg : container.palette().color(QPalette::Window);

  return cached;
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
    , m_reordering(false)
    , m_dimInactiveTabs(false) {
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

// Dim inactive tab titles with an opaque blend so Light/Neutral themes
// (whose QSS forces solid black) still show a clear contrast. Alpha alone
// is often ignored or invisible against light tab backgrounds.
void DockTabStrip::updateTabTextColors() {
  // The active theme's stylesheet always wins over setTabTextColor() for a
  // styled #TabBarContainer QTabBar::tab, so this never fights the theme:
  // it only measures (see dockTabTextContrast()) whether that theme's own
  // :selected rule is actually visible, and if not, remembers a wash color
  // - sampled from that same theme - for paintEvent() to apply as a subtle
  // fallback. Themes that already differentiate are left untouched.
  const TabTextContrast &contrast = dockTabTextContrast();
  m_dimInactiveTabs                = contrast.needsWash;
  m_dimWashColor                   = contrast.washColor;

  for (int i = 0; i < count(); ++i) setTabTextColor(i, QColor());
  update();
}

//-------------------------------------

void DockTabStrip::onCurrentChanged(int index) {
  updateTabTextColors();
  if (!m_layout || !m_region || index < 0 || m_dragOutStarted) return;
  m_layout->setActiveTab(m_region, index);
}

//-------------------------------------

// Force every tab to share the strip's width equally (Qt's setExpanding
// only scales sizeHint-based widths, which keeps long titles wide and short
// ones squeezed). A floor keeps many tabs readable and lets the strip's
// scroll buttons take over instead of shrinking tabs further.
QSize DockTabStrip::tabSizeHint(int index) const {
  QSize hint          = QTabBar::tabSizeHint(index);
  const int tabCount  = count();
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

// Fallback for themes whose stylesheet does not itself distinguish a
// selected tab's text from an unselected one (see dockTabTextContrast());
// washes inactive tabs toward their own sampled background tone so their
// text loses some contrast without any theme-specific color being assumed.
void DockTabStrip::paintEvent(QPaintEvent *event) {
  QTabBar::paintEvent(event);
  if (!m_dimInactiveTabs || !m_dimWashColor.isValid()) return;

  QPainter painter(this);
  QColor wash = m_dimWashColor;
  wash.setAlphaF(0.45);
  const int current = currentIndex();
  for (int i = 0; i < count(); ++i) {
    if (i == current) continue;
    QRect r = tabRect(i);
    if (!r.isValid()) continue;
    r.adjust(1, 1, -1, -1);
    painter.fillRect(r, wash);
  }
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
