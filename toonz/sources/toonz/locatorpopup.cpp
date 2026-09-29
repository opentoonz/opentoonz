#include "locatorpopup.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"
#include "toonz/stage2.h"

// Tnz6 includes
#include "tapp.h"
#include "pane.h"
#include "sceneviewer.h"
#include "viewerpane.h"
#include "toonzqt/gutil.h"

#include "tgeometry.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>
#include <QSize>
#include <algorithm>

namespace {

QImage tintNonTransparent(const QImage &src, const QColor &c) {
  QImage out = src.convertToFormat(QImage::Format_ARGB32);
  if (!c.isValid()) return out;
  const QRgb rgb = c.rgb() & 0x00FFFFFF;
  for (int y = 0; y < out.height(); ++y) {
    QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
    for (int x = 0; x < out.width(); ++x) {
      const int a = qAlpha(line[x]);
      if (a != 0)
        line[x] = rgb | (static_cast<QRgb>(qBound(0, a, 255)) << 24);
    }
  }
  return out;
}

QIcon themedPanArrowIcon(qreal degrees, const QColor &baseColor) {
  QImage img(QStringLiteral(":/Resources/arrow_up.png"));
  if (img.isNull()) return QIcon();
  const QColor c =
      baseColor.isValid() ? baseColor : QColor(0xd8, 0xd8, 0xd8);
  img = tintNonTransparent(img, c);
  QTransform t;
  t.rotate(degrees);
  QPixmap pm =
      QPixmap::fromImage(img.transformed(t, Qt::SmoothTransformation));
  return QIcon(pm);
}

QIcon themedVerticalArrowIcon(bool up, const QColor &baseColor) {
  return themedPanArrowIcon(up ? 0.0 : 180.0, baseColor);
}

//! Icon suggesting multiple stroke-direction cues (matching / guided workflow).
QIcon matchingStrokeArrowsIcon(const QColor &baseColor) {
  constexpr int S = 20;
  QPixmap px(S, S);
  px.fill(Qt::transparent);
  QPainter painter(&px);
  painter.setRenderHint(QPainter::Antialiasing);
  const QColor c =
      baseColor.isValid() ? baseColor : QColor(0xd8, 0xd8, 0xd8);
  painter.setPen(QPen(c, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(c);
  const QPointF tips[3] = {{3, 15}, {7, 11}, {11, 7}};
  for (const QPointF &tip : tips) {
    QPolygonF arrow;
    arrow << tip << QPointF(tip.x() + 5, tip.y() - 2) << QPointF(tip.x() + 4, tip.y() + 2.5);
    painter.drawPolygon(arrow);
  }
  return QIcon(px);
}

}  // namespace

//-----------------------------------------------------------------------------

LocatorPopup::~LocatorPopup() {
  persistTabIndex();
  if (m_matchingStrokeReferenceMode) setMatchingStrokeReferenceMode(false);
}

//-----------------------------------------------------------------------------

LocatorPopup::LocatorPopup(QWidget *parent, Qt::WindowFlags flags)
    : QFrame(parent)
    , m_initialZoom(true)
    , m_tabBarHost(nullptr)
    , m_tabBar(nullptr)
    , m_stack(nullptr)
    , m_navTopLayout(nullptr)
    , m_navBottomLayout(nullptr) {
  m_viewer = new SceneViewer(NULL);
  m_viewer->setParent(parent);
  m_viewer->setIsLocator();

  m_tabBarHost = new TabBarContainter(this);
  m_tabBar     = new QTabBar(m_tabBarHost);
  m_tabBar->setDrawBase(false);
  m_stack = new QStackedWidget(this);
  m_stack->setContentsMargins(0, 0, 0, 0);

  auto *tabRow = new QHBoxLayout(m_tabBarHost);
  tabRow->setContentsMargins(0, 0, 0, 0);
  tabRow->setSpacing(0);
  tabRow->addWidget(m_tabBar, 0);
  tabRow->addStretch(1);

  m_locatorPage = new QWidget(this);
  m_locatorPageLayout = new QVBoxLayout(m_locatorPage);
  m_locatorPageLayout->setContentsMargins(0, 0, 0, 0);
  m_locatorPageLayout->setSpacing(0);

  auto makeChromeSpacer = [&](QWidget *page) {
    QWidget *w = new QWidget(page);
    w->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    w->setAttribute(Qt::WA_TransparentForMouseEvents);
    w->setAutoFillBackground(false);
    return w;
  };
  m_locatorTopSpacer    = makeChromeSpacer(m_locatorPage);
  m_locatorBottomSpacer = makeChromeSpacer(m_locatorPage);

  m_navPage = new QWidget(this);
  m_navPageLayout = new QVBoxLayout(m_navPage);
  m_navPageLayout->setContentsMargins(0, 0, 0, 0);
  m_navPageLayout->setSpacing(4);

  buildNavigatorToolbar();
  buildNavigatorBottomBar();

  m_tabBar->addTab(tr("Locator"));
  m_stack->addWidget(m_locatorPage);
  if (Preferences::instance()->isLocatorNavigatorTabEnabled()) {
    m_tabBar->addTab(tr("Navigator"));
    m_stack->addWidget(m_navPage);
  } else
    m_navPage->hide();

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);
  outer->addWidget(m_tabBarHost, 0);
  outer->addWidget(m_stack, 1);
  setLayout(outer);

  m_locatorPageLayout->addWidget(m_locatorTopSpacer, 0);
  m_locatorPageLayout->addWidget(m_viewer, 1);
  m_locatorPageLayout->addWidget(m_locatorBottomSpacer, 0);

  connect(m_tabBar, &QTabBar::currentChanged, this,
          &LocatorPopup::onTabIndexChanged);

  connect(m_guidedCombo, QOverload<int>::of(&QComboBox::activated), this,
          &LocatorPopup::onGuidedComboChanged);
  connect(m_soloColumnTb, &QToolButton::toggled, this,
          &LocatorPopup::onSoloColumnToggled);
  connect(m_hideCurrentTb, &QToolButton::toggled, this,
          &LocatorPopup::onHideCurrentToggled);

  connect(m_followMainPanAct, &QAction::toggled, this, [this]() {
    m_viewer->update();
  });

  connect(m_matchingStrokeTb, &QToolButton::toggled, this,
          &LocatorPopup::onMatchingStrokeToggled);

  connect(Preferences::instance(),
          &Preferences::locatorNavigatorTabEnabledChanged, this,
          &LocatorPopup::onLocatorNavigatorPreferenceChanged);

  connect(ThemePropertiesNotifier::instance(),
          &ThemePropertiesNotifier::propertiesChanged, this,
          &LocatorPopup::refreshNavigatorThemedIcons);

  {
    QSettings settings;
    settings.beginGroup(QStringLiteral("LocatorNavigatorPanel"));
    int tab = settings.value(QStringLiteral("lastTabIndex"), 0).toInt();
    settings.endGroup();
    const int maxTab = m_tabBar->count() - 1;
    if (tab < 0 || tab > maxTab) tab = TabLocator;
    m_tabBar->blockSignals(true);
    m_tabBar->setCurrentIndex(tab);
    if (m_stack) m_stack->setCurrentIndex(tab);
    m_tabBar->blockSignals(false);
    onTabIndexChanged(tab);
  }

  updateTabPageSizeConsistency();
  updateLocatorTabBarChrome();

  bool ret = true;
  ret = connect(m_viewer, SIGNAL(onZoomChanged()), SLOT(changeWindowTitle()));
  ret = ret &&
        connect(m_viewer, SIGNAL(previewToggled()), SLOT(changeWindowTitle()));
  assert(ret);

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
  if (QGuiApplication *gApp =
          qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
    connect(gApp, &QGuiApplication::paletteChanged, this,
            [this](const QPalette &) { refreshNavigatorThemedIcons(); });
  }
#endif

  resize(400, 400);
}

//-----------------------------------------------------------------------------

void LocatorPopup::buildNavigatorToolbar() {
  m_guidedCombo = new QComboBox(m_navPage);
  m_guidedCombo->setToolTip(
      tr("Vector guided drawing for this Navigator panel only. Initial choice "
         "matches Preferences → \"Vector guided drawing\" (same as View menu); "
         "changing the list overrides for this panel until you pick another mode. "
         "This is not the same as Always \"Closest\": your global preference may "
         "be Off, Farthest, or All."));
  m_guidedCombo->addItem(tr("Off"), 0);
  m_guidedCombo->addItem(tr("Closest"), 1);
  m_guidedCombo->addItem(tr("Farthest"), 2);
  m_guidedCombo->addItem(tr("All"), 3);
  {
    int pref = Preferences::instance()->getGuidedDrawingType();
    if (pref < 0 || pref > 3) pref = 0;
    m_guidedCombo->setCurrentIndex(pref);
  }

  m_hideCurrentTb = new QToolButton(m_navPage);
  m_hideCurrentTb->setCheckable(true);
  m_hideCurrentTb->setIcon(createQIcon(QStringLiteral("preview"), false));
  m_hideCurrentTb->setToolTip(
      tr("When onion skin or Shift & Trace shows reference frames, hide the "
         "full-opacity current drawing in this panel only (not onion ghosts). "
         "Guided stroke arrows and ghosts stay visible for matching."));
  m_hideCurrentTb->setChecked(true);
  m_hideCurrentTb->setAutoRaise(true);

  m_soloColumnTb = new QToolButton(m_navPage);
  m_soloColumnTb->setCheckable(true);
  m_soloColumnTb->setIcon(createQIcon(QStringLiteral("fold_column"), false));
  m_soloColumnTb->setToolTip(
      tr("Show only the active xsheet column here. Other columns (e.g. a dimmed "
         "raster level used as reference while drawing on vector) are omitted "
         "so onion skin and guided strokes stay readable."));
  m_soloColumnTb->setChecked(true);
  m_soloColumnTb->setAutoRaise(true);

  m_matchingStrokeTb = new QToolButton(m_navPage);
  m_matchingStrokeTb->setCheckable(true);
  m_matchingStrokeTb->setIcon(matchingStrokeArrowsIcon(themeIconBaseColor()));
  m_matchingStrokeTb->setToolTip(
      tr("Matching stroke: hide onion skin ghosts on main workspace viewers "
         "(Combo / Scene). Global onion still applies in this Navigator / "
         "Locator panel. Turn off to restore each main viewer's previous state "
         "captured when this was turned on."));
  m_matchingStrokeTb->setAutoRaise(true);
  m_matchingStrokeTb->setFixedSize(24, 24);
  m_matchingStrokeTb->setIconSize(QSize(18, 18));

  m_gearBtn = new QToolButton(m_navPage);
  m_gearBtn->setIcon(createQIcon(QStringLiteral("gear"), false));
  m_gearBtn->setToolTip(tr("Follow main viewer pan and zoom."));
  m_gearBtn->setAutoRaise(true);
  m_gearBtn->setPopupMode(QToolButton::InstantPopup);
  auto *gearMenu = new QMenu(m_gearBtn);

  m_followMainPanAct =
      gearMenu->addAction(tr("Follow main viewer pan and zoom"));
  m_followMainPanAct->setCheckable(true);
  m_followMainPanAct->setToolTip(
      tr("When enabled, pan and zoom tracking from the main viewer affects this "
         "panel, similar to the classic locator. When disabled, the view stays "
         "independent unless you pan or zoom inside this panel."));
  m_gearBtn->setMenu(gearMenu);

  m_navTopLayout = new QHBoxLayout();
  m_navTopLayout->setSpacing(6);
  m_navTopLayout->addWidget(new QLabel(tr("Guided:"), m_navPage), 0);
  m_navTopLayout->addWidget(m_guidedCombo, 1);
  m_navTopLayout->addStretch(1);
  m_navTopLayout->addWidget(m_hideCurrentTb, 0);
  m_navTopLayout->addWidget(m_soloColumnTb, 0);
  m_navTopLayout->addWidget(m_matchingStrokeTb, 0);
  m_navTopLayout->addWidget(m_gearBtn, 0);

  m_navPageLayout->addLayout(m_navTopLayout);
}

//-----------------------------------------------------------------------------

void LocatorPopup::buildNavigatorBottomBar() {
  m_navBottomLayout = new QHBoxLayout();
  m_navBottomLayout->setSpacing(2);
  m_navBottomLayout->setContentsMargins(0, 0, 0, 0);

  auto mkBtnThemedSvg = [this](const QString &svgIconName, const QString &tip) {
    QToolButton *tb = new QToolButton(m_navPage);
    tb->setIcon(createQIcon(svgIconName, false));
    tb->setToolTip(tip);
    tb->setAutoRaise(true);
    tb->setFixedSize(24, 24);
    tb->setIconSize(QSize(18, 18));
    return tb;
  };
  auto mkBtnIcon = [this](const QIcon &icon, const QString &tip) {
    QToolButton *tb = new QToolButton(m_navPage);
    tb->setIcon(icon);
    tb->setToolTip(tip);
    tb->setAutoRaise(true);
    tb->setFixedSize(24, 24);
    tb->setIconSize(QSize(18, 18));
    return tb;
  };

  QToolButton *zoomIn =
      mkBtnThemedSvg(QStringLiteral("zoomin"), tr("Zoom in"));
  m_navBottomButtons[NBB_ZoomIn] = zoomIn;
  connect(zoomIn, &QToolButton::clicked, m_viewer, &SceneViewer::zoomIn);
  QToolButton *zoomOut =
      mkBtnThemedSvg(QStringLiteral("zoomout"), tr("Zoom out"));
  m_navBottomButtons[NBB_ZoomOut] = zoomOut;
  connect(zoomOut, &QToolButton::clicked, m_viewer, &SceneViewer::zoomOut);
  QToolButton *zoomReset =
      mkBtnThemedSvg(QStringLiteral("zoom_reset"),
                     tr("Reset zoom (scale) to default for this camera."));
  m_navBottomButtons[NBB_ZoomReset] = zoomReset;
  connect(zoomReset, &QToolButton::clicked, m_viewer, &SceneViewer::resetZoom);

  QToolButton *rotL =
      mkBtnThemedSvg(QStringLiteral("rotateleft"), tr("Rotate left"));
  m_navBottomButtons[NBB_RotL] = rotL;
  connect(rotL, &QToolButton::clicked, m_viewer, &SceneViewer::rotateLeft);
  QToolButton *rotR =
      mkBtnThemedSvg(QStringLiteral("rotateright"), tr("Rotate right"));
  m_navBottomButtons[NBB_RotR] = rotR;
  connect(rotR, &QToolButton::clicked, m_viewer, &SceneViewer::rotateRight);

  static const int kPanStep = 40;
  QToolButton *panL =
      mkBtnIcon(themedPanArrowIcon(-90.0, themeIconBaseColor()), tr("Pan view left"));
  m_navBottomButtons[NBB_PanL] = panL;
  connect(panL, &QToolButton::clicked, this, [this]() {
    m_viewer->navigatorPan(QPoint(-kPanStep, 0));
  });
  QToolButton *panR =
      mkBtnIcon(themedPanArrowIcon(90.0, themeIconBaseColor()), tr("Pan view right"));
  m_navBottomButtons[NBB_PanR] = panR;
  connect(panR, &QToolButton::clicked, this, [this]() {
    m_viewer->navigatorPan(QPoint(kPanStep, 0));
  });
  QToolButton *panU = mkBtnIcon(themedVerticalArrowIcon(true, themeIconBaseColor()),
                                tr("Pan view up"));
  m_navBottomButtons[NBB_PanU] = panU;
  connect(panU, &QToolButton::clicked, this, [this]() {
    m_viewer->navigatorPan(QPoint(0, -kPanStep));
  });
  QToolButton *panD = mkBtnIcon(themedVerticalArrowIcon(false, themeIconBaseColor()),
                                tr("Pan view down"));
  m_navBottomButtons[NBB_PanD] = panD;
  connect(panD, &QToolButton::clicked, this, [this]() {
    m_viewer->navigatorPan(QPoint(0, kPanStep));
  });

  QToolButton *flipH =
      mkBtnThemedSvg(QStringLiteral("fliphoriz"), tr("Flip viewer horizontally"));
  m_navBottomButtons[NBB_FlipH] = flipH;
  connect(flipH, &QToolButton::clicked, m_viewer, &SceneViewer::flipX);
  QToolButton *flipV =
      mkBtnThemedSvg(QStringLiteral("flipvert"), tr("Flip viewer vertically"));
  m_navBottomButtons[NBB_FlipV] = flipV;
  connect(flipV, &QToolButton::clicked, m_viewer, &SceneViewer::flipY);

  m_navBottomLayout->addWidget(zoomIn);
  m_navBottomLayout->addWidget(zoomOut);
  m_navBottomLayout->addWidget(zoomReset);
  m_navBottomLayout->addSpacing(6);
  m_navBottomLayout->addWidget(rotL);
  m_navBottomLayout->addWidget(rotR);
  m_navBottomLayout->addSpacing(6);
  m_navBottomLayout->addWidget(panL);
  m_navBottomLayout->addWidget(panR);
  m_navBottomLayout->addWidget(panU);
  m_navBottomLayout->addWidget(panD);
  m_navBottomLayout->addSpacing(6);
  m_navBottomLayout->addWidget(flipH);
  m_navBottomLayout->addWidget(flipV);
  m_navBottomLayout->addStretch(1);

  m_navPageLayout->addLayout(m_navBottomLayout);
}

//-----------------------------------------------------------------------------

QColor LocatorPopup::themeIconBaseColor() const {
  ThemeManager &tm = ThemeManager::getInstance();
  QColor c = tm.getIconBaseColor();
  if (c.isValid()) return c;
  c = tm.getCustomPropertyColor(QStringLiteral("icon-base-color"));
  if (c.isValid()) return c;

  const QWidget *ref = m_navPage ? m_navPage : static_cast<const QWidget *>(this);
  if (ref) {
    c = ref->palette().color(QPalette::WindowText);
    if (c.isValid()) return c;
  }
  c = QApplication::palette().color(QPalette::WindowText);
  return c.isValid() ? c : QColor(0xd8, 0xd8, 0xd8);
}

//-----------------------------------------------------------------------------

void LocatorPopup::refreshNavigatorThemedIcons() {
  if (!m_hideCurrentTb) return;
  m_hideCurrentTb->setIcon(createQIcon(QStringLiteral("preview"), false));
  m_soloColumnTb->setIcon(createQIcon(QStringLiteral("fold_column"), false));
  m_matchingStrokeTb->setIcon(matchingStrokeArrowsIcon(themeIconBaseColor()));
  m_gearBtn->setIcon(createQIcon(QStringLiteral("gear"), false));
  if (m_navBottomButtons[NBB_ZoomIn])
    m_navBottomButtons[NBB_ZoomIn]->setIcon(
        createQIcon(QStringLiteral("zoomin"), false));
  if (m_navBottomButtons[NBB_ZoomOut])
    m_navBottomButtons[NBB_ZoomOut]->setIcon(
        createQIcon(QStringLiteral("zoomout"), false));
  if (m_navBottomButtons[NBB_ZoomReset])
    m_navBottomButtons[NBB_ZoomReset]->setIcon(
        createQIcon(QStringLiteral("zoom_reset"), false));
  if (m_navBottomButtons[NBB_RotL])
    m_navBottomButtons[NBB_RotL]->setIcon(
        createQIcon(QStringLiteral("rotateleft"), false));
  if (m_navBottomButtons[NBB_RotR])
    m_navBottomButtons[NBB_RotR]->setIcon(
        createQIcon(QStringLiteral("rotateright"), false));
  if (m_navBottomButtons[NBB_PanL])
    m_navBottomButtons[NBB_PanL]->setIcon(
        themedPanArrowIcon(-90.0, themeIconBaseColor()));
  if (m_navBottomButtons[NBB_PanR])
    m_navBottomButtons[NBB_PanR]->setIcon(
        themedPanArrowIcon(90.0, themeIconBaseColor()));
  if (m_navBottomButtons[NBB_PanU])
    m_navBottomButtons[NBB_PanU]->setIcon(
        themedVerticalArrowIcon(true, themeIconBaseColor()));
  if (m_navBottomButtons[NBB_PanD])
    m_navBottomButtons[NBB_PanD]->setIcon(
        themedVerticalArrowIcon(false, themeIconBaseColor()));
  if (m_navBottomButtons[NBB_FlipH])
    m_navBottomButtons[NBB_FlipH]->setIcon(
        createQIcon(QStringLiteral("fliphoriz"), false));
  if (m_navBottomButtons[NBB_FlipV])
    m_navBottomButtons[NBB_FlipV]->setIcon(
        createQIcon(QStringLiteral("flipvert"), false));
}

//-----------------------------------------------------------------------------

void LocatorPopup::reparentViewerToTab(int tabIndex) {
  if (tabIndex == TabNavigator && m_tabBar->count() < 2) tabIndex = TabLocator;
  m_locatorPageLayout->removeWidget(m_viewer);
  m_navPageLayout->removeWidget(m_viewer);
  // Always keep viewer between top and bottom rows (index 1) on both pages.
  if (tabIndex == TabLocator)
    m_locatorPageLayout->insertWidget(1, m_viewer, 1);
  else
    m_navPageLayout->insertWidget(1, m_viewer, 1);
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyLocatorTabToViewer() {
  m_viewer->setGuidedDrawingModeOverride(-1);
  m_viewer->setSuppressOnionSkinInViewer(false);
  m_viewer->setHideCurrentDrawingInViewer(false);
  m_viewer->setShowOnlyCurrentColumnInViewer(false);
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyNavigatorTabToViewer() {
  onGuidedComboChanged(m_guidedCombo->currentIndex());
  m_viewer->setSuppressOnionSkinInViewer(false);
  m_viewer->setHideCurrentDrawingInViewer(m_hideCurrentTb->isChecked());
  m_viewer->setShowOnlyCurrentColumnInViewer(m_soloColumnTb->isChecked());
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavigatorControlsEnabled() {
  const bool nav = (m_tabBar->count() > 1 &&
                    m_tabBar->currentIndex() == TabNavigator);
  m_guidedCombo->setEnabled(nav);
  m_hideCurrentTb->setEnabled(nav);
  m_soloColumnTb->setEnabled(nav);
  m_matchingStrokeTb->setEnabled(nav);
  m_gearBtn->setEnabled(nav);
}

//-----------------------------------------------------------------------------

void LocatorPopup::persistTabIndex() {
  if (!m_tabBar) return;
  const int index = m_tabBar->currentIndex();
  QSettings settings;
  settings.beginGroup(QStringLiteral("LocatorNavigatorPanel"));
  settings.setValue(QStringLiteral("lastTabIndex"), index);
  settings.endGroup();
  settings.sync();
}

//-----------------------------------------------------------------------------

void LocatorPopup::setMatchingStrokeReferenceMode(bool on) {
  if (m_matchingStrokeReferenceMode == on) return;

  if (on) {
    m_matchingSuppressRestore.clear();
    TApp *app     = TApp::instance();
    QMainWindow *mainWin = app ? app->getMainWindow() : nullptr;
    QWidget *mw          = mainWin;
    if (mw) {
      const QList<BaseViewerPanel *> panels =
          mw->findChildren<BaseViewerPanel *>();
      for (BaseViewerPanel *p : panels) {
        if (!p) continue;
        SceneViewer *sv = p->getSceneViewer();
        if (!sv) continue;
        if (!sv->getIsLocator()) {
          m_matchingSuppressRestore.append(
              qMakePair(QPointer<SceneViewer>(sv),
                        sv->getSuppressOnionSkinInViewer()));
          sv->setSuppressOnionSkinInViewer(true);
        }
        // Full GL refresh: PartialUpdate can leave stale onion/reference pixels
        // after toggling suppress until hover or focus change.
        sv->GLInvalidateAll();
      }
    }
    m_matchingStrokeReferenceMode = true;
  } else {
    TApp *app            = TApp::instance();
    QMainWindow *mainWin = app ? app->getMainWindow() : nullptr;
    for (const auto &pr : m_matchingSuppressRestore) {
      SceneViewer *sv = pr.first.data();
      if (sv) {
        sv->setSuppressOnionSkinInViewer(pr.second);
        sv->GLInvalidateAll();
      }
    }
    m_matchingSuppressRestore.clear();
    m_matchingStrokeReferenceMode = false;
    if (mainWin) {
      const QList<BaseViewerPanel *> panels =
          mainWin->findChildren<BaseViewerPanel *>();
      for (BaseViewerPanel *p : panels) {
        if (!p) continue;
        if (SceneViewer *sv = p->getSceneViewer()) sv->GLInvalidateAll();
      }
    }
  }
  if (m_matchingStrokeTb && m_matchingStrokeTb->isChecked() != on) {
    m_matchingStrokeTb->blockSignals(true);
    m_matchingStrokeTb->setChecked(on);
    m_matchingStrokeTb->blockSignals(false);
  }
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateTabPageSizeConsistency() {
  if (!m_locatorTopSpacer || !m_locatorBottomSpacer || !m_navTopLayout ||
      !m_navBottomLayout || !m_viewer || !m_locatorPage || !m_navPage ||
      !m_tabBar)
    return;

  // Locator page stays classic: full-height canvas (Navigator keeps real toolbars).
  m_locatorTopSpacer->setFixedHeight(0);
  m_locatorBottomSpacer->setFixedHeight(0);
  m_locatorPageLayout->setSpacing(0);

  if (m_tabBar->count() > 1) {
    const QSize navMin = m_navPage->minimumSizeHint();
    const int minW =
        std::max(m_locatorPage->minimumSizeHint().width(), navMin.width());
    if (minW > 0) {
      m_locatorPage->setMinimumWidth(minW);
      m_navPage->setMinimumWidth(minW);
    }
  } else
    m_locatorPage->setMinimumWidth(0);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onLocatorNavigatorPreferenceChanged(bool enabled) {
  applyNavigatorTabVisibilityFromPreferences(enabled);
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyNavigatorTabVisibilityFromPreferences(bool navigatorEnabled) {
  if (!m_tabBar || !m_stack) return;

  if (navigatorEnabled) {
    if (m_tabBar->count() == 1) {
      m_navPage->show();
      m_tabBar->addTab(tr("Navigator"));
      m_stack->addWidget(m_navPage);
      updateNavigatorControlsEnabled();
      updateTabPageSizeConsistency();
      updateLocatorTabBarChrome();
      changeWindowTitle();
    }
    return;
  }

  if (m_tabBar->count() < 2) return;

  if (m_matchingStrokeReferenceMode) setMatchingStrokeReferenceMode(false);

  if (m_tabBar->currentIndex() == TabNavigator) {
    m_tabBar->blockSignals(true);
    m_tabBar->setCurrentIndex(TabLocator);
    m_stack->setCurrentIndex(TabLocator);
    m_tabBar->blockSignals(false);
    reparentViewerToTab(TabLocator);
    applyLocatorTabToViewer();
  }

  m_tabBar->blockSignals(true);
  m_tabBar->removeTab(TabNavigator);
  m_stack->removeWidget(m_navPage);
  m_navPage->setParent(this);
  m_navPage->hide();
  m_tabBar->blockSignals(false);
  updateNavigatorControlsEnabled();
  m_viewer->update();
  changeWindowTitle();
  persistTabIndex();
  updateTabPageSizeConsistency();
  updateLocatorTabBarChrome();
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateLocatorTabBarChrome() {
  if (!m_tabBarHost || !m_tabBar || !m_stack) return;

  const bool showTabs = (m_tabBar->count() > 1);
  m_tabBarHost->setVisible(showTabs);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onTabIndexChanged(int index) {
  if (index < 0) return;
  if (m_stack && index < m_stack->count()) m_stack->setCurrentIndex(index);
  reparentViewerToTab(index);
  updateNavigatorControlsEnabled();
  if (index == TabLocator)
    applyLocatorTabToViewer();
  else
    applyNavigatorTabToViewer();
  m_viewer->update();
  changeWindowTitle();

  updateTabPageSizeConsistency();
  persistTabIndex();
}

//-----------------------------------------------------------------------------

void LocatorPopup::onGuidedComboChanged(int index) {
  if (index < 0) return;
  const int mode = m_guidedCombo->itemData(index, Qt::UserRole).toInt();
  m_viewer->setGuidedDrawingModeOverride(mode);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onHideCurrentToggled(bool checked) {
  m_viewer->setHideCurrentDrawingInViewer(checked);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onSoloColumnToggled(bool checked) {
  m_viewer->setShowOnlyCurrentColumnInViewer(checked);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onMatchingStrokeToggled(bool checked) {
  setMatchingStrokeReferenceMode(checked);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onChangeViewAff(const TPointD &pos) {
  const int tab = m_tabBar->currentIndex();
  if (m_tabBar->count() > 1 && tab == TabNavigator &&
      !m_followMainPanAct->isChecked())
    return;

  TAffine curAff = m_viewer->getSceneMatrix();
  TAffine newAff(curAff.a11, 0, -pos.x * curAff.a11, 0, curAff.a22,
                 -pos.y * curAff.a22);
  m_viewer->setViewMatrix(newAff, 0);
  m_viewer->setViewMatrix(newAff, 1);
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void LocatorPopup::changeEvent(QEvent *e) {
  QFrame::changeEvent(e);
  switch (e->type()) {
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
  case QEvent::StyleChange:
#ifdef Q_OS_MACOS
  case QEvent::ThemeChange:
#endif
    refreshNavigatorThemedIcons();
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------

void LocatorPopup::showEvent(QShowEvent *) {
  if (m_initialZoom) {
    for (int z = 0; z < 4; z++) m_viewer->zoomQt(true, false);
    m_initialZoom = false;
  }

  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();

  bool ret = true;
  ret      = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                       SLOT(changeWindowTitle()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
                       SLOT(changeWindowTitle()));
  assert(ret);

  app->setActiveLocator(this);

  changeWindowTitle();

  updateTabPageSizeConsistency();
  refreshNavigatorThemedIcons();
}

//-----------------------------------------------------------------------------

void LocatorPopup::hideEvent(QHideEvent *) {
  persistTabIndex();
  TApp *app = TApp::instance();
  disconnect(app->getCurrentLevel());
  disconnect(app->getCurrentFrame());
  if (app->getActiveLocator() == this) app->setActiveLocator(0);
}

//-----------------------------------------------------------------------------

void LocatorPopup::changeWindowTitle() {
  TApp *app = TApp::instance();
  QString name = (m_tabBar->count() > 1 &&
                    m_tabBar->currentIndex() == TabNavigator)
                     ? tr("Navigator")
                     : tr("Locator");

  bool showZoomFactor = false;

  if (app->getCurrentFrame()->isEditingScene()) {
    if (m_viewer->isPreviewEnabled()) showZoomFactor = true;
    else if (Preferences::instance()
                 ->isActualPixelViewOnSceneEditingModeEnabled() &&
             app->getCurrentLevel()->getSimpleLevel() &&
             !CleanupPreviewCheck::instance()->isEnabled() &&
             !CameraTestCheck::instance()->isEnabled())
      showZoomFactor = true;
  } else {
    TXshLevel *level = app->getCurrentLevel()->getLevel();
    if (level) showZoomFactor = true;
  }

  if (showZoomFactor) {
    name = name + "  Zoom : " +
           QString::number((int)(100.0 * sqrt(m_viewer->getViewMatrix().det()) *
                                 m_viewer->getDpiFactor())) +
           "%";
  }
  if (TPanel *panel = qobject_cast<TPanel *>(parentWidget()))
    panel->setWindowTitle(name);
  else
    setWindowTitle(name);
}
