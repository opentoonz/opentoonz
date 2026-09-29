#include "locatorpopup.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"
#include "toonz/stage2.h"

// Tnz6 includes
#include "pane.h"
#include "viewerpane.h"
#include "tapp.h"
#include "sceneviewer.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"
#include "tools/cursors.h"
#include "tools/cursormanager.h"

#include "tgeometry.h"
#include "tvectorimage.h"

#include <QContextMenuEvent>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QResizeEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QTabBar>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>
#include <QSize>
#include <algorithm>
#include <cmath>

namespace {

constexpr int kNavTopBtnSize     = 18;
constexpr int kNavTopIconSize    = 14;
constexpr int kNavBottomBtnSize  = 18;
constexpr int kNavBottomIconSize = 13;
constexpr int kNavGroupSpacer    = 6;
constexpr int kNavMinBtnSize     = 16;
constexpr int kNavMinIconSize    = 11;
constexpr double kNavMinScale    = 16.0 / 18.0;
constexpr double kNavScaleDamp   = 0.32;

QImage tintNonTransparent(const QImage &src, const QColor &c) {
  QImage out = src.convertToFormat(QImage::Format_ARGB32);
  if (!c.isValid()) return out;
  const QRgb rgb = c.rgb() & 0x00FFFFFF;
  for (int y = 0; y < out.height(); ++y) {
    QRgb *line = reinterpret_cast<QRgb *>(out.scanLine(y));
    for (int x = 0; x < out.width(); ++x) {
      const int a = qAlpha(line[x]);
      if (a != 0) line[x] = rgb | (static_cast<QRgb>(qBound(0, a, 255)) << 24);
    }
  }
  return out;
}

QIcon themedPanArrowIcon(qreal degrees, const QColor &baseColor) {
  QImage img(QStringLiteral(":/Resources/arrow_up.png"));
  if (img.isNull()) return QIcon();
  const QColor c = baseColor.isValid() ? baseColor : QColor(0xd8, 0xd8, 0xd8);
  img            = tintNonTransparent(img, c);
  QTransform t;
  t.rotate(degrees);
  QPixmap pm = QPixmap::fromImage(img.transformed(t, Qt::SmoothTransformation));
  return QIcon(pm);
}

QIcon themedVerticalArrowIcon(bool up, const QColor &baseColor) {
  return themedPanArrowIcon(up ? 0.0 : 180.0, baseColor);
}

void drawNavigatorDisableSlash(QPainter &painter, int size, const QColor &c) {
  const QColor slash = c.isValid() ? c : QColor(0xd8, 0xd8, 0xd8);
  painter.setPen(QPen(slash, 1.5, Qt::SolidLine, Qt::RoundCap));
  // Same length as the full diagonal; shifted slightly downward.
  painter.drawLine(QPointF(3, size - 3.5), QPointF(size - 3, 3.5));
}

QPixmap navigatorThemedIconPixmap(const QString &iconName, int size) {
  const QPixmap pm = createQIcon(iconName, false).pixmap(size, size);
  return pm.isNull() ? QPixmap(size, size) : pm;
}

QPixmap navigatorSlashedPixmap(const QPixmap &base, const QColor &ink,
                               int size) {
  if (base.isNull()) return base;
  QPixmap pm = base;
  QPainter painter(&pm);
  painter.setRenderHint(QPainter::Antialiasing, true);
  drawNavigatorDisableSlash(painter, size, ink);
  return pm;
}

QIcon navigatorToggleIcon(const QPixmap &base, bool checked, const QColor &ink,
                          int size) {
  return QIcon(checked ? navigatorSlashedPixmap(base, ink, size) : base);
}

void styleNavigatorToggleButton(QToolButton *tb) {
  if (!tb) return;
  tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
  tb->setStyleSheet(QStringLiteral(
      "QToolButton { border: none; background: transparent; padding: 0; "
      "margin: 0; min-width: 0; min-height: 0; }"
      "QToolButton:hover { background: rgba(128, 128, 128, 0.12); }"
      "QToolButton:checked { background: transparent; border: none; }"
      "QToolButton:checked:hover { background: rgba(128, 128, 128, 0.12); }"));
}

bool isPointerPress(QEvent::Type t) {
  return t == QEvent::MouseButtonPress || t == QEvent::TabletPress;
}

bool isPointerMove(QEvent::Type t) {
  return t == QEvent::MouseMove || t == QEvent::TabletMove;
}

bool isPointerRelease(QEvent::Type t) {
  return t == QEvent::MouseButtonRelease || t == QEvent::TabletRelease;
}

bool isPointerEvent(QEvent::Type t) {
  return isPointerPress(t) || isPointerMove(t) || isPointerRelease(t);
}

QPointF pointerLocalPos(QEvent *event) {
  const QEvent::Type type = event->type();
  if (type == QEvent::TabletPress || type == QEvent::TabletMove ||
      type == QEvent::TabletRelease) {
    auto *te = static_cast<QTabletEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return te->position();
#else
    return te->posF();
#endif
  }
  auto *me = static_cast<QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  return me->position();
#else
  return me->localPos();
#endif
}

Qt::MouseButton pointerButton(QEvent *event) {
  const QEvent::Type type = event->type();
  if (type == QEvent::TabletPress || type == QEvent::TabletMove ||
      type == QEvent::TabletRelease)
    return static_cast<QTabletEvent *>(event)->button();
  return static_cast<QMouseEvent *>(event)->button();
}

Qt::MouseButtons pointerButtons(QEvent *event) {
  const QEvent::Type type = event->type();
  if (type == QEvent::TabletPress || type == QEvent::TabletMove ||
      type == QEvent::TabletRelease)
    return static_cast<QTabletEvent *>(event)->buttons();
  return static_cast<QMouseEvent *>(event)->buttons();
}

}  // namespace

//-----------------------------------------------------------------------------

LocatorPopup::~LocatorPopup() {
  restoreToolAfterNav();
  persistPanelState();
  if (m_matchingStrokeReferenceMode) setMatchingStrokeReferenceMode(false);
}

//-----------------------------------------------------------------------------

LocatorPopup::LocatorPopup(QWidget *parent, Qt::WindowFlags flags)
    : QFrame(parent)
    , m_tabBarHost(nullptr)
    , m_tabBar(nullptr)
    , m_stack(nullptr)
    , m_navTopLayout(nullptr)
    , m_navBottomLayout(nullptr)
    , m_navTopBarHost(nullptr)
    , m_navBottomBarHost(nullptr)
    , m_navTopSpacerAfterGuided(nullptr)
    , m_navTopStretchWhenGuidedHidden(nullptr)
    , m_navBottomSpacerAfterZoom(nullptr)
    , m_navBottomSpacerAfterView(nullptr)
    , m_navBottomSpacerAfterRotate(nullptr)
    , m_navBottomSpacerAfterPan(nullptr) {
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

  m_locatorPage       = new QWidget(this);
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

  m_navPage       = new QWidget(this);
  m_navPageLayout = new QVBoxLayout(m_navPage);
  m_navPageLayout->setContentsMargins(0, 0, 0, 0);
  m_navPageLayout->setSpacing(2);

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
  connect(m_soloColumnTb, &QToolButton::toggled, this,
          &LocatorPopup::updateNavigatorVisibilityIcons);
  connect(m_hideCurrentTb, &QToolButton::toggled, this,
          &LocatorPopup::onHideCurrentToggled);
  connect(m_hideCurrentTb, &QToolButton::toggled, this,
          &LocatorPopup::updateNavigatorVisibilityIcons);

  connect(m_matchingStrokeTb, &QToolButton::toggled, this,
          &LocatorPopup::onMatchingStrokeToggled);
  connect(m_matchingStrokeTb, &QToolButton::toggled, this,
          &LocatorPopup::updateNavigatorVisibilityIcons);

  connect(m_overviewAct, &QAction::toggled, this, [this](bool on) {
    if (m_overviewTb && m_overviewTb->isChecked() != on) {
      QSignalBlocker blocker(m_overviewTb);
      m_overviewTb->setChecked(on);
    }
    persistPanelState();
    applyOverviewMode();
    if (!isNavToolsOnly()) restoreToolAfterNav();
    if (isOverview()) scheduleOverviewFit();
    updateNavigatorToolbarTooltips();
  });
  if (m_overviewTb) {
    connect(m_overviewTb, &QToolButton::toggled, this, [this](bool on) {
      if (m_overviewAct) m_overviewAct->setChecked(on);
    });
  }
  connect(m_navToolsOnlyAct, &QAction::toggled, this, [this](bool on) {
    if (m_navToolsOnlyTb && m_navToolsOnlyTb->isChecked() != on) {
      QSignalBlocker blocker(m_navToolsOnlyTb);
      m_navToolsOnlyTb->setChecked(on);
    }
    persistPanelState();
    applyOverviewMode();
    if (!isNavToolsOnly()) restoreToolAfterNav();
    updateOverviewCursor();
    updateNavigatorToolbarTooltips();
  });
  if (m_navToolsOnlyTb) {
    connect(m_navToolsOnlyTb, &QToolButton::toggled, this, [this](bool on) {
      if (m_navToolsOnlyAct) m_navToolsOnlyAct->setChecked(on);
    });
  }
  connect(m_syncZoomAct, &QAction::toggled, this,
          [this]() { persistPanelState(); });
  connect(m_syncPanAct, &QAction::toggled, this,
          [this]() { persistPanelState(); });
  if (TApp *app = TApp::instance()) {
    connect(app, &TApp::activeViewerChanged, this, [this]() {
      hookNavFrameSource();
      if (TApp *a = TApp::instance()) {
        if (SceneViewer *sv = a->getActiveViewer()) {
          if (sv != m_viewer) restoreToolAfterNav();
        }
      }
    });
    if (ToolHandle *th = app->getCurrentTool()) {
      connect(th, &ToolHandle::toolSwitched, this,
              &LocatorPopup::updateNavToolButtonChecks);
      updateNavToolButtonChecks();
    }
  }
  hookNavFrameSource();
  applyOverviewMode();
  m_viewer->installEventFilter(this);

  connect(Preferences::instance(),
          &Preferences::locatorNavigatorTabEnabledChanged, this,
          &LocatorPopup::onLocatorNavigatorPreferenceChanged);

  connect(ThemePropertiesNotifier::instance(),
          &ThemePropertiesNotifier::propertiesChanged, this,
          &LocatorPopup::refreshNavigatorThemedIcons);

  updateNavigatorBarsVisibility();

  updateTabPageSizeConsistency();
  updateLocatorTabBarChrome();

  bool ret = true;
  ret = connect(m_viewer, SIGNAL(onZoomChanged()), SLOT(changeWindowTitle()));
  ret = ret &&
        connect(m_viewer, SIGNAL(previewToggled()), SLOT(changeWindowTitle()));
  ret = ret && connect(m_viewer, &SceneViewer::onZoomChanged, this,
                       &LocatorPopup::onNavigatorViewChanged);
  ret = ret && connect(m_viewer, &SceneViewer::refreshNavi, this,
                       &LocatorPopup::onNavigatorViewChanged);
  assert(ret);
  captureLastNavAffs();

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
  m_guidedCombo->setToolTip(tr("Guided drawing"));
  m_guidedCombo->addItem(tr("Off"), 0);
  m_guidedCombo->addItem(tr("Closest"), 1);
  m_guidedCombo->addItem(tr("Farthest"), 2);
  m_guidedCombo->addItem(tr("All"), 3);
  {
    int pref = Preferences::instance()->getGuidedDrawingType();
    if (pref < 0 || pref > 3) pref = 0;
    m_guidedCombo->setCurrentIndex(pref);
  }
  m_guidedCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_guidedCombo->setMinimumWidth(0);
  m_guidedCombo->setFixedHeight(kNavTopBtnSize);

  m_hideCurrentTb = new QToolButton(m_navPage);
  m_hideCurrentTb->setCheckable(true);
  m_hideCurrentTb->setToolTip(tr("Show current drawing"));
  m_hideCurrentTb->setChecked(false);
  m_hideCurrentTb->setAutoRaise(true);
  styleNavigatorToggleButton(m_hideCurrentTb);
  m_hideCurrentTb->setFixedSize(kNavTopBtnSize, kNavTopBtnSize);
  m_hideCurrentTb->setIconSize(QSize(kNavTopIconSize, kNavTopIconSize));

  m_soloColumnTb = new QToolButton(m_navPage);
  m_soloColumnTb->setCheckable(true);
  m_soloColumnTb->setToolTip(tr("Show all columns"));
  m_soloColumnTb->setChecked(false);
  m_soloColumnTb->setAutoRaise(true);
  styleNavigatorToggleButton(m_soloColumnTb);
  m_soloColumnTb->setFixedSize(kNavTopBtnSize, kNavTopBtnSize);
  m_soloColumnTb->setIconSize(QSize(kNavTopIconSize, kNavTopIconSize));

  m_matchingStrokeTb = new QToolButton(m_navPage);
  m_matchingStrokeTb->setCheckable(true);
  m_matchingStrokeTb->setToolTip(tr("Show onion skin in other viewers"));
  m_matchingStrokeTb->setChecked(false);
  m_matchingStrokeTb->setAutoRaise(true);
  styleNavigatorToggleButton(m_matchingStrokeTb);
  m_matchingStrokeTb->setFixedSize(kNavTopBtnSize, kNavTopBtnSize);
  m_matchingStrokeTb->setIconSize(QSize(kNavTopIconSize, kNavTopIconSize));

  m_overviewTb = new QToolButton(m_navPage);
  m_overviewTb->setCheckable(true);
  m_overviewTb->setChecked(true);
  m_overviewTb->setToolTip(tr("Use as Overview"));
  m_overviewTb->setAutoRaise(true);
  m_overviewTb->setIcon(
      createQIcon(QStringLiteral("navigator_overview"), false));
  m_overviewTb->setFixedSize(kNavTopBtnSize, kNavTopBtnSize);
  m_overviewTb->setIconSize(QSize(kNavTopIconSize, kNavTopIconSize));

  m_navToolsOnlyTb = new QToolButton(m_navPage);
  m_navToolsOnlyTb->setCheckable(true);
  m_navToolsOnlyTb->setChecked(true);
  m_navToolsOnlyTb->setToolTip(tr("Navigation tools only"));
  m_navToolsOnlyTb->setAutoRaise(true);
  m_navToolsOnlyTb->setIcon(
      createQIcon(QStringLiteral("navigator_nav_only"), false));
  m_navToolsOnlyTb->setFixedSize(kNavTopBtnSize, kNavTopBtnSize);
  m_navToolsOnlyTb->setIconSize(QSize(kNavTopIconSize, kNavTopIconSize));

  m_gearBtn = new QToolButton(m_navPage);
  m_gearBtn->setIcon(createQIcon(QStringLiteral("menu"), false));
  m_gearBtn->setAutoRaise(true);
  m_gearBtn->setPopupMode(QToolButton::InstantPopup);
  auto *gearMenu = new QMenu(m_gearBtn);
  gearMenu->setToolTipsVisible(true);
  m_overviewAct = gearMenu->addAction(tr("Use as Overview"));
  m_overviewAct->setCheckable(true);
  m_overviewAct->setChecked(true);
  m_overviewAct->setToolTip(tr("Map of the main viewer."));
  gearMenu->addSeparator();
  m_navToolsOnlyAct = gearMenu->addAction(tr("Navigation tools only"));
  m_navToolsOnlyAct->setCheckable(true);
  m_navToolsOnlyAct->setChecked(true);
  m_navToolsOnlyAct->setToolTip(tr("Zoom, Hand, and Rotate only."));
  m_syncZoomAct = gearMenu->addAction(tr("Synchronize Zoom"));
  m_syncZoomAct->setCheckable(true);
  m_syncZoomAct->setToolTip(tr("Also zooms the main viewer."));
  m_syncPanAct = gearMenu->addAction(tr("Synchronize Pan"));
  m_syncPanAct->setCheckable(true);
  m_syncPanAct->setToolTip(tr("Also pans the main viewer."));
  m_gearBtn->setMenu(gearMenu);
  m_gearBtn->setFixedSize(kNavTopBtnSize, kNavTopBtnSize);
  m_gearBtn->setIconSize(QSize(kNavTopIconSize, kNavTopIconSize));

  m_navTopLayout = new QHBoxLayout();
  m_navTopLayout->setContentsMargins(2, 1, 2, 1);
  m_navTopLayout->setSpacing(0);
  m_guidedLabel = new QLabel(tr("Guided:"), m_navPage);
  m_guidedLabel->setFixedHeight(kNavTopBtnSize);
  m_navTopSpacerAfterGuided = new QWidget(m_navPage);
  m_navTopSpacerAfterGuided->setFixedWidth(6);
  m_navTopStretchWhenGuidedHidden = new QWidget(m_navPage);
  m_navTopLayout->addWidget(m_guidedLabel, 0);
  m_navTopLayout->addWidget(m_guidedCombo, 1);
  m_navTopLayout->addWidget(m_navTopSpacerAfterGuided, 0);
  m_navTopLayout->addWidget(m_navTopStretchWhenGuidedHidden, 1);
  m_navTopLayout->addWidget(m_hideCurrentTb, 0);
  m_navTopLayout->addWidget(m_soloColumnTb, 0);
  m_navTopLayout->addWidget(m_matchingStrokeTb, 0);
  m_navTopLayout->addWidget(m_navToolsOnlyTb, 0);
  m_navTopLayout->addWidget(m_overviewTb, 0);
  m_navTopLayout->addWidget(m_gearBtn, 0);

  m_navTopBarHost = new QWidget(m_navPage);
  m_navTopBarHost->setLayout(m_navTopLayout);
  m_navTopBarHost->setFixedHeight(kNavTopBtnSize + 2);
  m_navPageLayout->addWidget(m_navTopBarHost);
  updateNavigatorVisibilityIcons();
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
    tb->setFixedSize(kNavBottomBtnSize, kNavBottomBtnSize);
    tb->setIconSize(QSize(kNavBottomIconSize, kNavBottomIconSize));
    return tb;
  };
  auto mkBtnIcon = [this](const QIcon &icon, const QString &tip) {
    QToolButton *tb = new QToolButton(m_navPage);
    tb->setIcon(icon);
    tb->setToolTip(tip);
    tb->setAutoRaise(true);
    tb->setFixedSize(kNavBottomBtnSize, kNavBottomBtnSize);
    tb->setIconSize(QSize(kNavBottomIconSize, kNavBottomIconSize));
    return tb;
  };

  auto bindViewTool = [this](QToolButton *tb, void (SceneViewer::*fn)()) {
    connect(tb, &QToolButton::clicked, this, [this, fn]() {
      if (SceneViewer *sv = viewToolTarget()) (sv->*fn)();
    });
  };

  auto mkToolBtn = [this](const char *commandId, const QString &iconName) {
    QToolButton *tb = new QToolButton(m_navPage);
    tb->setCheckable(true);
    if (QAction *act = CommandManager::instance()->getAction(commandId)) {
      tb->setToolTip(act->toolTip());
      connect(tb, &QToolButton::clicked, this, [this, act]() {
        if (isNavToolsOnly()) rememberToolBeforeNav();
        act->trigger();
      });
    }
    tb->setIcon(createQIcon(iconName, false));
    tb->setAutoRaise(true);
    tb->setFixedSize(kNavBottomBtnSize, kNavBottomBtnSize);
    tb->setIconSize(QSize(kNavBottomIconSize, kNavBottomIconSize));
    return tb;
  };

  QToolButton *toolZoom = mkToolBtn(T_Zoom, QStringLiteral("navigator_zoom"));
  m_navBottomButtons[NBB_ToolZoom] = toolZoom;
  QToolButton *toolHand = mkToolBtn(T_Hand, QStringLiteral("navigator_hand"));
  m_navBottomButtons[NBB_ToolHand] = toolHand;
  QToolButton *toolRotate =
      mkToolBtn(T_Rotate, QStringLiteral("navigator_rotate"));
  m_navBottomButtons[NBB_ToolRotate] = toolRotate;

  QToolButton *zoomIn =
      mkBtnThemedSvg(QStringLiteral("navigator_zoomin"), tr("Zoom in"));
  m_navBottomButtons[NBB_ZoomIn] = zoomIn;
  bindViewTool(zoomIn, &SceneViewer::zoomIn);
  QToolButton *zoomOut =
      mkBtnThemedSvg(QStringLiteral("navigator_zoomout"), tr("Zoom out"));
  m_navBottomButtons[NBB_ZoomOut] = zoomOut;
  bindViewTool(zoomOut, &SceneViewer::zoomOut);
  QToolButton *zoomReset =
      mkBtnThemedSvg(QStringLiteral("navigator_zoom_reset"),
                     tr("Reset zoom (scale) to default for this camera."));
  m_navBottomButtons[NBB_ZoomReset] = zoomReset;
  bindViewTool(zoomReset, &SceneViewer::resetZoom);
  QToolButton *fitCam = mkBtnThemedSvg(
      QStringLiteral("navigator_fit_to_window"), tr("Fit to Camera"));
  m_navBottomButtons[NBB_Fit] = fitCam;
  bindViewTool(fitCam, &SceneViewer::fitToCamera);
  QToolButton *resetView =
      mkBtnThemedSvg(QStringLiteral("navigator_reset"), tr("Reset View"));
  m_navBottomButtons[NBB_ResetView] = resetView;
  bindViewTool(resetView, &SceneViewer::resetSceneViewer);

  QToolButton *rotL =
      mkBtnThemedSvg(QStringLiteral("navigator_rotateleft"), tr("Rotate left"));
  m_navBottomButtons[NBB_RotL] = rotL;
  bindViewTool(rotL, &SceneViewer::rotateLeft);
  QToolButton *rotR = mkBtnThemedSvg(QStringLiteral("navigator_rotateright"),
                                     tr("Rotate right"));
  m_navBottomButtons[NBB_RotR] = rotR;
  bindViewTool(rotR, &SceneViewer::rotateRight);

  static const int kPanStep = 40;
  QToolButton *panL = mkBtnIcon(themedPanArrowIcon(-90.0, themeIconBaseColor()),
                                tr("Pan view left"));
  m_navBottomButtons[NBB_PanL] = panL;
  connect(panL, &QToolButton::clicked, this, [this]() {
    if (SceneViewer *sv = viewToolTarget())
      sv->navigatorPan(QPoint(-kPanStep, 0));
  });
  QToolButton *panR = mkBtnIcon(themedPanArrowIcon(90.0, themeIconBaseColor()),
                                tr("Pan view right"));
  m_navBottomButtons[NBB_PanR] = panR;
  connect(panR, &QToolButton::clicked, this, [this]() {
    if (SceneViewer *sv = viewToolTarget())
      sv->navigatorPan(QPoint(kPanStep, 0));
  });
  QToolButton *panU = mkBtnIcon(
      themedVerticalArrowIcon(true, themeIconBaseColor()), tr("Pan view up"));
  m_navBottomButtons[NBB_PanU] = panU;
  connect(panU, &QToolButton::clicked, this, [this]() {
    if (SceneViewer *sv = viewToolTarget())
      sv->navigatorPan(QPoint(0, -kPanStep));
  });
  QToolButton *panD =
      mkBtnIcon(themedVerticalArrowIcon(false, themeIconBaseColor()),
                tr("Pan view down"));
  m_navBottomButtons[NBB_PanD] = panD;
  connect(panD, &QToolButton::clicked, this, [this]() {
    if (SceneViewer *sv = viewToolTarget())
      sv->navigatorPan(QPoint(0, kPanStep));
  });

  QToolButton *flipH = mkBtnThemedSvg(QStringLiteral("navigator_fliphoriz"),
                                      tr("Flip viewer horizontally"));
  m_navBottomButtons[NBB_FlipH] = flipH;
  bindViewTool(flipH, &SceneViewer::flipX);
  QToolButton *flipV = mkBtnThemedSvg(QStringLiteral("navigator_flipvert"),
                                      tr("Flip viewer vertically"));
  m_navBottomButtons[NBB_FlipV] = flipV;
  bindViewTool(flipV, &SceneViewer::flipY);

  m_navBottomLayout->addStretch(1);
  m_navBottomLayout->addWidget(toolZoom);
  m_navBottomLayout->addWidget(toolHand);
  m_navBottomLayout->addWidget(toolRotate);
  m_navBottomSpacerAfterTools = new QWidget(m_navPage);
  m_navBottomSpacerAfterTools->setFixedWidth(6);
  m_navBottomLayout->addWidget(m_navBottomSpacerAfterTools);
  m_navBottomLayout->addWidget(zoomIn);
  m_navBottomLayout->addWidget(zoomOut);
  m_navBottomLayout->addWidget(zoomReset);
  m_navBottomSpacerAfterZoom = new QWidget(m_navPage);
  m_navBottomSpacerAfterZoom->setFixedWidth(6);
  m_navBottomLayout->addWidget(m_navBottomSpacerAfterZoom);
  m_navBottomLayout->addWidget(fitCam);
  m_navBottomLayout->addWidget(resetView);
  m_navBottomSpacerAfterView = new QWidget(m_navPage);
  m_navBottomSpacerAfterView->setFixedWidth(6);
  m_navBottomLayout->addWidget(m_navBottomSpacerAfterView);
  m_navBottomLayout->addWidget(rotL);
  m_navBottomLayout->addWidget(rotR);
  m_navBottomSpacerAfterRotate = new QWidget(m_navPage);
  m_navBottomSpacerAfterRotate->setFixedWidth(6);
  m_navBottomLayout->addWidget(m_navBottomSpacerAfterRotate);
  m_navBottomLayout->addWidget(panL);
  m_navBottomLayout->addWidget(panR);
  m_navBottomLayout->addWidget(panU);
  m_navBottomLayout->addWidget(panD);
  m_navBottomSpacerAfterPan = new QWidget(m_navPage);
  m_navBottomSpacerAfterPan->setFixedWidth(6);
  m_navBottomLayout->addWidget(m_navBottomSpacerAfterPan);
  m_navBottomLayout->addWidget(flipH);
  m_navBottomLayout->addWidget(flipV);
  m_navBottomLayout->addStretch(1);

  m_navBottomBarHost = new QWidget(m_navPage);
  m_navBottomBarHost->setLayout(m_navBottomLayout);
  m_navBottomBarHost->setFixedHeight(kNavBottomBtnSize);
  m_navPageLayout->addWidget(m_navBottomBarHost);
}

//-----------------------------------------------------------------------------

QColor LocatorPopup::themeIconBaseColor() const {
  ThemeManager &tm = ThemeManager::getInstance();
  QColor c         = tm.getIconBaseColor();
  if (c.isValid()) return c;
  c = tm.getCustomPropertyColor(QStringLiteral("icon-base-color"));
  if (c.isValid()) return c;

  const QWidget *ref =
      m_navPage ? m_navPage : static_cast<const QWidget *>(this);
  if (ref) {
    c = ref->palette().color(QPalette::WindowText);
    if (c.isValid()) return c;
  }
  c = QApplication::palette().color(QPalette::WindowText);
  return c.isValid() ? c : QColor(0xd8, 0xd8, 0xd8);
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavigatorVisibilityIcons() {
  if (!m_hideCurrentTb || !m_soloColumnTb || !m_matchingStrokeTb) return;
  const int iconPx = m_navTopIconPx > 0 ? m_navTopIconPx : kNavTopIconSize;
  const QColor ink = themeIconBaseColor();
  m_hideCurrentTb->setIcon(navigatorToggleIcon(
      navigatorThemedIconPixmap(QStringLiteral("preview"), iconPx),
      m_hideCurrentTb->isChecked(), ink, iconPx));
  m_soloColumnTb->setIcon(navigatorToggleIcon(
      navigatorThemedIconPixmap(QStringLiteral("navigator_column"), iconPx),
      m_soloColumnTb->isChecked(), ink, iconPx));
  m_matchingStrokeTb->setIcon(navigatorToggleIcon(
      navigatorThemedIconPixmap(QStringLiteral("navigator_onionskin"), iconPx),
      m_matchingStrokeTb->isChecked(), ink, iconPx));
  updateNavigatorToolbarTooltips();
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavigatorToolbarTooltips() {
  if (m_guidedCombo) m_guidedCombo->setToolTip(tr("Guided drawing"));
  if (m_hideCurrentTb)
    m_hideCurrentTb->setToolTip(m_hideCurrentTb->isChecked()
                                    ? tr("Hide current drawing")
                                    : tr("Show current drawing"));
  if (m_soloColumnTb)
    m_soloColumnTb->setToolTip(m_soloColumnTb->isChecked()
                                   ? tr("Show active column only")
                                   : tr("Show all columns"));
  if (m_matchingStrokeTb)
    m_matchingStrokeTb->setToolTip(
        m_matchingStrokeTb->isChecked()
            ? tr("Hide onion skin in other viewers")
            : tr("Show onion skin in other viewers"));
  if (m_overviewTb)
    m_overviewTb->setToolTip(m_overviewTb->isChecked()
                                 ? tr("Use as Overview")
                                 : tr("Use as second canvas"));
  if (m_navToolsOnlyTb)
    m_navToolsOnlyTb->setToolTip(tr("Navigation tools only"));
  if (m_gearBtn) m_gearBtn->setToolTip(tr("Settings"));
}

//-----------------------------------------------------------------------------

void LocatorPopup::refreshNavigatorThemedIcons() {
  if (!m_hideCurrentTb) return;
  updateNavigatorVisibilityIcons();
  if (m_overviewTb)
    m_overviewTb->setIcon(
        createQIcon(QStringLiteral("navigator_overview"), false));
  if (m_navToolsOnlyTb)
    m_navToolsOnlyTb->setIcon(
        createQIcon(QStringLiteral("navigator_nav_only"), false));
  if (m_gearBtn) m_gearBtn->setIcon(createQIcon(QStringLiteral("menu"), false));
  if (m_navBottomButtons[NBB_ToolZoom])
    m_navBottomButtons[NBB_ToolZoom]->setIcon(
        createQIcon(QStringLiteral("navigator_zoom"), false));
  if (m_navBottomButtons[NBB_ToolHand])
    m_navBottomButtons[NBB_ToolHand]->setIcon(
        createQIcon(QStringLiteral("navigator_hand"), false));
  if (m_navBottomButtons[NBB_ToolRotate])
    m_navBottomButtons[NBB_ToolRotate]->setIcon(
        createQIcon(QStringLiteral("navigator_rotate"), false));
  if (m_navBottomButtons[NBB_ZoomIn])
    m_navBottomButtons[NBB_ZoomIn]->setIcon(
        createQIcon(QStringLiteral("navigator_zoomin"), false));
  if (m_navBottomButtons[NBB_ZoomOut])
    m_navBottomButtons[NBB_ZoomOut]->setIcon(
        createQIcon(QStringLiteral("navigator_zoomout"), false));
  if (m_navBottomButtons[NBB_ZoomReset])
    m_navBottomButtons[NBB_ZoomReset]->setIcon(
        createQIcon(QStringLiteral("navigator_zoom_reset"), false));
  if (m_navBottomButtons[NBB_Fit])
    m_navBottomButtons[NBB_Fit]->setIcon(
        createQIcon(QStringLiteral("navigator_fit_to_window"), false));
  if (m_navBottomButtons[NBB_ResetView])
    m_navBottomButtons[NBB_ResetView]->setIcon(
        createQIcon(QStringLiteral("navigator_reset"), false));
  if (m_navBottomButtons[NBB_RotL])
    m_navBottomButtons[NBB_RotL]->setIcon(
        createQIcon(QStringLiteral("navigator_rotateleft"), false));
  if (m_navBottomButtons[NBB_RotR])
    m_navBottomButtons[NBB_RotR]->setIcon(
        createQIcon(QStringLiteral("navigator_rotateright"), false));
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
        createQIcon(QStringLiteral("navigator_fliphoriz"), false));
  if (m_navBottomButtons[NBB_FlipV])
    m_navBottomButtons[NBB_FlipV]->setIcon(
        createQIcon(QStringLiteral("navigator_flipvert"), false));
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

void LocatorPopup::captureTabView(int tab) {
  if (!m_viewer || (tab != TabLocator && tab != TabNavigator)) return;
  auto &dst  = (tab == TabLocator) ? m_locatorAffs : m_navigatorAffs;
  bool &have = (tab == TabLocator) ? m_haveLocatorAffs : m_haveNavigatorAffs;
  for (int mode = 0; mode < 2; ++mode)
    dst[mode] = m_viewer->getViewAffine(mode);
  have = true;
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyTabView(int tab) {
  if (!m_viewer) return;
  if (tab == TabNavigator && isOverview()) return;
  const bool have =
      (tab == TabLocator) ? m_haveLocatorAffs : m_haveNavigatorAffs;
  if (!have) return;
  const auto &src = (tab == TabLocator) ? m_locatorAffs : m_navigatorAffs;
  for (int mode = 0; mode < 2; ++mode) m_viewer->setViewMatrix(src[mode], mode);
  m_viewer->update();
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavigatorControlsEnabled() {
  const bool nav =
      (m_tabBar->count() > 1 && m_tabBar->currentIndex() == TabNavigator);
  m_guidedCombo->setEnabled(nav);
  m_hideCurrentTb->setEnabled(nav);
  m_soloColumnTb->setEnabled(nav);
  m_matchingStrokeTb->setEnabled(nav);
  if (m_overviewTb) m_overviewTb->setEnabled(nav);
  if (m_navToolsOnlyTb) m_navToolsOnlyTb->setEnabled(nav);
  if (m_gearBtn) m_gearBtn->setEnabled(nav);
}

//-----------------------------------------------------------------------------

void LocatorPopup::writePanelStateTo(QSettings &settings) const {
  if (!m_tabBar || !m_viewer) return;

  settings.setValue(QStringLiteral("lastTabIndex"), m_tabBar->currentIndex());
  settings.setValue(QStringLiteral("showDisplayToolbar"), m_showDisplayToolbar);
  settings.setValue(QStringLiteral("showNavGuided"), m_showNavGuided);
  settings.setValue(QStringLiteral("showNavOverview"), m_showNavOverview);
  settings.setValue(QStringLiteral("showNavToolsOnly"), m_showNavToolsOnly);
  settings.setValue(QStringLiteral("showNavTools"), m_showNavTools);
  settings.setValue(QStringLiteral("showNavZoom"), m_showNavZoom);
  settings.setValue(QStringLiteral("showNavView"), m_showNavView);
  settings.setValue(QStringLiteral("showNavRotate"), m_showNavRotate);
  settings.setValue(QStringLiteral("showNavPan"), m_showNavPan);
  settings.setValue(QStringLiteral("showNavFlip"), m_showNavFlip);
  if (m_overviewAct)
    settings.setValue(QStringLiteral("useAsOverview"),
                      m_overviewAct->isChecked());
  if (m_navToolsOnlyAct)
    settings.setValue(QStringLiteral("navigationToolsOnly"),
                      m_navToolsOnlyAct->isChecked());
  if (m_syncZoomAct)
    settings.setValue(QStringLiteral("syncNavZoom"),
                      m_syncZoomAct->isChecked());
  if (m_syncPanAct)
    settings.setValue(QStringLiteral("syncNavPan"), m_syncPanAct->isChecked());

  const auto writeAffs = [&settings](const QString &prefix,
                                     const std::array<TAffine, 2> &affs) {
    for (int mode = 0; mode < 2; ++mode) {
      const TAffine &aff = affs[mode];
      settings.beginGroup(prefix + QString::number(mode));
      settings.setValue(QStringLiteral("a11"), aff.a11);
      settings.setValue(QStringLiteral("a12"), aff.a12);
      settings.setValue(QStringLiteral("a13"), aff.a13);
      settings.setValue(QStringLiteral("a21"), aff.a21);
      settings.setValue(QStringLiteral("a22"), aff.a22);
      settings.setValue(QStringLiteral("a23"), aff.a23);
      settings.endGroup();
    }
  };

  std::array<TAffine, 2> loc = m_locatorAffs;
  std::array<TAffine, 2> nav = m_navigatorAffs;
  bool haveLoc               = m_haveLocatorAffs;
  bool haveNav               = m_haveNavigatorAffs;
  if (m_viewer) {
    std::array<TAffine, 2> cur = {m_viewer->getViewAffine(0),
                                  m_viewer->getViewAffine(1)};
    if (isLocatorRole()) {
      loc     = cur;
      haveLoc = true;
    } else {
      nav     = cur;
      haveNav = true;
    }
  }
  if (haveLoc) writeAffs(QStringLiteral("locatorViewAff"), loc);
  if (haveNav)
    writeAffs(QStringLiteral("viewAff"), nav);
  else if (m_viewer)
    writeAffs(QStringLiteral("viewAff"),
              {m_viewer->getViewAffine(0), m_viewer->getViewAffine(1)});
}

//-----------------------------------------------------------------------------

void LocatorPopup::readPanelStateFrom(QSettings &settings) {
  const bool legacyNavigationBar =
      settings.value(QStringLiteral("showNavigationBar"), true).toBool();
  m_showDisplayToolbar =
      settings.value(QStringLiteral("showDisplayToolbar"), true).toBool();
  m_showNavGuided =
      settings.value(QStringLiteral("showNavGuided"), false).toBool();
  m_showNavOverview =
      settings.value(QStringLiteral("showNavOverview"), false).toBool();
  m_showNavToolsOnly =
      settings.value(QStringLiteral("showNavToolsOnly"), false).toBool();
  m_showNavTools =
      settings.value(QStringLiteral("showNavTools"), true).toBool();
  m_showNavZoom =
      settings.value(QStringLiteral("showNavZoom"), legacyNavigationBar)
          .toBool();
  m_showNavView = settings.value(QStringLiteral("showNavView"), true).toBool();
  m_showNavRotate =
      settings.value(QStringLiteral("showNavRotate"), legacyNavigationBar)
          .toBool();
  m_showNavPan = settings.value(QStringLiteral("showNavPan"), false).toBool();
  m_showNavFlip =
      settings.value(QStringLiteral("showNavFlip"), legacyNavigationBar)
          .toBool();
  const bool legacyFollow =
      settings.value(QStringLiteral("followMainPan"), false).toBool();
  if (m_overviewAct) {
    QSignalBlocker blocker(m_overviewAct);
    m_overviewAct->setChecked(
        settings.value(QStringLiteral("useAsOverview"), true).toBool());
  }
  if (m_overviewTb && m_overviewAct) {
    QSignalBlocker blocker(m_overviewTb);
    m_overviewTb->setChecked(m_overviewAct->isChecked());
  }
  if (m_navToolsOnlyAct) {
    QSignalBlocker blocker(m_navToolsOnlyAct);
    m_navToolsOnlyAct->setChecked(
        settings.value(QStringLiteral("navigationToolsOnly"), true).toBool());
  }
  if (m_navToolsOnlyTb && m_navToolsOnlyAct) {
    QSignalBlocker blocker(m_navToolsOnlyTb);
    m_navToolsOnlyTb->setChecked(m_navToolsOnlyAct->isChecked());
  }
  if (m_syncZoomAct) {
    QSignalBlocker blocker(m_syncZoomAct);
    m_syncZoomAct->setChecked(
        settings.value(QStringLiteral("syncNavZoom"), legacyFollow).toBool());
  }
  if (m_syncPanAct) {
    QSignalBlocker blocker(m_syncPanAct);
    m_syncPanAct->setChecked(
        settings.value(QStringLiteral("syncNavPan"), legacyFollow).toBool());
  }

  const int tab =
      settings.value(QStringLiteral("lastTabIndex"), TabLocator).toInt();
  applyLoadedTabIndex(tab);
  readViewAffsFrom(settings);
  if (readAffsFrom(settings, QStringLiteral("locatorViewAff"), m_locatorAffs))
    m_haveLocatorAffs = true;
  if (m_viewRestorePending) {
    if (tab == TabNavigator) {
      m_navigatorAffs     = m_pendingViewAffs;
      m_haveNavigatorAffs = true;
    } else if (!m_haveLocatorAffs) {
      m_locatorAffs     = m_pendingViewAffs;
      m_haveLocatorAffs = true;
    }
  }
}

//-----------------------------------------------------------------------------

bool LocatorPopup::readAffsFrom(QSettings &settings, const QString &prefix,
                                std::array<TAffine, 2> &out) {
  bool found = false;
  for (int mode = 0; mode < 2; ++mode) {
    settings.beginGroup(prefix + QString::number(mode));
    if (!settings.contains(QStringLiteral("a11"))) {
      settings.endGroup();
      continue;
    }
    TAffine &aff = out[mode];
    aff.a11      = settings.value(QStringLiteral("a11"), 1.0).toDouble();
    aff.a12      = settings.value(QStringLiteral("a12"), 0.0).toDouble();
    aff.a13      = settings.value(QStringLiteral("a13"), 0.0).toDouble();
    aff.a21      = settings.value(QStringLiteral("a21"), 0.0).toDouble();
    aff.a22      = settings.value(QStringLiteral("a22"), 1.0).toDouble();
    aff.a23      = settings.value(QStringLiteral("a23"), 0.0).toDouble();
    settings.endGroup();
    found = true;
  }
  return found;
}

//-----------------------------------------------------------------------------

bool LocatorPopup::readViewAffsFrom(QSettings &settings) {
  if (!readAffsFrom(settings, QStringLiteral("viewAff"), m_pendingViewAffs))
    return false;
  m_viewRestorePending = true;
  return true;
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyLoadedTabIndex(int tab) {
  if (!m_tabBar) return;
  const int maxTab = m_tabBar->count() - 1;
  if (tab < 0 || tab > maxTab) tab = TabLocator;
  m_tabBar->blockSignals(true);
  m_tabBar->setCurrentIndex(tab);
  if (m_stack) m_stack->setCurrentIndex(tab);
  m_tabBar->blockSignals(false);
  onTabIndexChanged(tab);
}

//-----------------------------------------------------------------------------

void LocatorPopup::save(QSettings &settings) const {
  writePanelStateTo(settings);
}

//-----------------------------------------------------------------------------

void LocatorPopup::load(QSettings &settings) {
  readPanelStateFrom(settings);

  // Backward compatibility: older builds only wrote global QSettings.
  if (!m_viewRestorePending) {
    QSettings global;
    global.beginGroup(QStringLiteral("LocatorNavigatorPanel"));
    readViewAffsFrom(global);
    global.endGroup();
  }

  updateNavigatorBarsVisibility();
  updateTabPageSizeConsistency();
  updateLocatorTabBarChrome();
  applyOverviewMode();
  updateNavigatorToolbarTooltips();
}

//-----------------------------------------------------------------------------

SceneViewer *LocatorPopup::resolveMainViewer() const {
  if (TApp *app = TApp::instance()) {
    SceneViewer *src = app->getActiveViewer();
    if (src && src != m_viewer && !src->getIsLocator()) return src;
    QWidget *mw = app->getMainWindow();
    if (!mw) return nullptr;
    const QList<BaseViewerPanel *> panels =
        mw->findChildren<BaseViewerPanel *>();
    for (BaseViewerPanel *panel : panels) {
      if (!panel) continue;
      SceneViewer *sv = panel->getSceneViewer();
      if (sv && sv != m_viewer && !sv->getIsLocator()) return sv;
    }
  }
  return nullptr;
}

//-----------------------------------------------------------------------------

SceneViewer *LocatorPopup::viewToolTarget() const {
  if (isOverview()) {
    if (SceneViewer *main = resolveMainViewer()) return main;
  }
  return m_viewer;
}

//-----------------------------------------------------------------------------

bool LocatorPopup::isOverview() const {
  return m_overviewAct && m_overviewAct->isChecked() && !isLocatorRole();
}

//-----------------------------------------------------------------------------

bool LocatorPopup::isNavToolsOnly() const {
  return !isLocatorRole() && m_navToolsOnlyAct &&
         m_navToolsOnlyAct->isChecked();
}

//-----------------------------------------------------------------------------

bool LocatorPopup::isViewNavTool() const {
  TTool *tool            = TApp::instance()->getCurrentTool()->getTool();
  const std::string name = tool ? tool->getName() : std::string();
  return name == T_Hand || name == T_HandView || name == T_Zoom ||
         name == T_ZoomView || name == T_Rotate || name == T_RotateView;
}

//-----------------------------------------------------------------------------

bool LocatorPopup::isNavPickTool() const {
  TTool *tool            = TApp::instance()->getCurrentTool()->getTool();
  const std::string name = tool ? tool->getName() : std::string();
  return name == T_Selection || name == T_ControlPointEditor;
}

//-----------------------------------------------------------------------------

bool LocatorPopup::canNavPickAt(const QPointF &winPos) const {
  if (!m_viewer || isOverview() || !isNavToolsOnly() || !isNavPickTool())
    return false;
  TTool *tool = TApp::instance()->getCurrentTool()->getTool();
  TVectorImageP vi(TTool::getImage(false));
  if (!tool || !vi || vi->getStrokeCount() == 0) return false;

  const TPointD pos = tool->getMatrix().inv() * m_viewer->winToWorld(winPos);
  double t = 0, dist2 = 0;
  UINT index = 0;
  if (!vi->getNearestStroke(pos, t, index, dist2)) return false;
  const double pixelSize = m_viewer->getPixelSize();
  const double maxDist   = 5.0 * pixelSize;
  return dist2 < (maxDist * maxDist) * 4.0;
}

//-----------------------------------------------------------------------------

void LocatorPopup::rememberToolBeforeNav() {
  if (m_haveToolBeforeNav) return;
  ToolHandle *th = TApp::instance()->getCurrentTool();
  if (!th) return;
  TTool *tool = th->getTool();
  if (!tool) return;
  m_toolBeforeNav     = QString::fromStdString(tool->getName());
  m_haveToolBeforeNav = true;
}

//-----------------------------------------------------------------------------

void LocatorPopup::restoreToolAfterNav() {
  if (!m_haveToolBeforeNav) return;
  m_haveToolBeforeNav = false;
  ToolHandle *th      = TApp::instance()->getCurrentTool();
  if (!th) {
    m_toolBeforeNav.clear();
    return;
  }
  TTool *cur = th->getTool();
  const QString curName =
      cur ? QString::fromStdString(cur->getName()) : QString();
  if (curName != m_toolBeforeNav) th->setTool(m_toolBeforeNav);
  m_toolBeforeNav.clear();
}

//-----------------------------------------------------------------------------

bool LocatorPopup::overviewUsesNavHand() const {
  if (!isOverview()) return false;
  TTool *tool            = TApp::instance()->getCurrentTool()->getTool();
  const std::string name = tool ? tool->getName() : std::string();
  if (name == T_Hand || name == T_HandView) return true;
  if (!isNavToolsOnly()) return false;
  if (name == T_Zoom || name == T_ZoomView || name == T_Rotate ||
      name == T_RotateView)
    return false;
  return true;
}

//-----------------------------------------------------------------------------

bool LocatorPopup::usesNavHand() const {
  if (isOverview()) return overviewUsesNavHand();
  if (!isNavToolsOnly() || isViewNavTool()) return false;
  if (isNavPickTool() && (m_navPickHover || m_navPickDragging)) return false;
  return true;
}

//-----------------------------------------------------------------------------

bool LocatorPopup::usesNavOnlyCursor() const { return usesNavHand(); }

//-----------------------------------------------------------------------------

void LocatorPopup::updateOverviewCursor() {
  if (!m_viewer || !usesNavOnlyCursor()) return;
  setToolCursor(m_viewer, ToolCursor::PanCursor);
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavToolButtonChecks() {
  std::string name;
  if (TTool *tool = TApp::instance()->getCurrentTool()->getTool())
    name = tool->getName();
  auto setOn = [&](int id, bool on) {
    if (QToolButton *tb = m_navBottomButtons[id]) {
      QSignalBlocker blocker(tb);
      tb->setChecked(on);
    }
  };
  setOn(NBB_ToolZoom, name == T_Zoom || name == T_ZoomView);
  setOn(NBB_ToolHand, name == T_Hand || name == T_HandView);
  setOn(NBB_ToolRotate, name == T_Rotate || name == T_RotateView);
  updateOverviewCursor();
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyOverviewMode() {
  const bool overviewOn = m_overviewAct && m_overviewAct->isChecked();
  if (m_syncZoomAct) m_syncZoomAct->setEnabled(!overviewOn);
  if (m_syncPanAct) m_syncPanAct->setEnabled(!overviewOn);
  if (m_viewer) m_viewer->setNavPickOnly(!isOverview() && isNavToolsOnly());
  hookNavFrameSource();
}

//-----------------------------------------------------------------------------

void LocatorPopup::hookNavFrameSource() {
  SceneViewer *src = isOverview() ? resolveMainViewer() : nullptr;
  if (m_navFrameSource && m_navFrameSource != src) {
    disconnect(m_navFrameSource, &SceneViewer::onZoomChanged, this,
               &LocatorPopup::scheduleNavFrameUpdate);
    disconnect(m_navFrameSource, &SceneViewer::refreshNavi, this,
               &LocatorPopup::scheduleNavFrameUpdate);
  }
  if (src && m_navFrameSource != src) {
    connect(src, &SceneViewer::onZoomChanged, this,
            &LocatorPopup::scheduleNavFrameUpdate);
    connect(src, &SceneViewer::refreshNavi, this,
            &LocatorPopup::scheduleNavFrameUpdate);
  }
  m_navFrameSource = src;
  if (m_viewer) m_viewer->setViewForwardTarget(src);
  updateNavViewFrame();
}

//-----------------------------------------------------------------------------

void LocatorPopup::scheduleNavFrameUpdate() {
  if (m_navFrameScheduled) return;
  m_navFrameScheduled = true;
  QTimer::singleShot(0, this, [this]() {
    m_navFrameScheduled = false;
    updateNavViewFrame();
  });
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavViewFrame() {
  if (!m_viewer) return;
  if (!isOverview()) {
    m_viewer->setNavViewFrame(false, nullptr);
    return;
  }
  SceneViewer *main = resolveMainViewer();
  if (!main || main->width() < 2 || main->height() < 2 ||
      m_viewer->width() < 2 || m_viewer->height() < 2) {
    m_viewer->setNavViewFrame(false, nullptr);
    return;
  }

  const TAffine mainInv    = main->getViewMatrix().inv();
  const TAffine navAff     = m_viewer->getViewMatrix();
  const double hw          = 0.5 * (double)main->width();
  const double hh          = 0.5 * (double)main->height();
  const TPointD corners[4] = {TPointD(-hw, -hh), TPointD(hw, -hh),
                              TPointD(hw, hh), TPointD(-hw, hh)};
  TPointD gl[4];
  TPointD acc(0, 0);
  for (int i = 0; i < 4; ++i) {
    gl[i] = navAff * (mainInv * corners[i]);
    acc   = acc + gl[i];
  }
  m_viewer->setNavViewFrame(true, gl);

  const TPointD c    = acc * 0.25;
  const double w2    = m_viewer->width() * 0.5;
  const double h2    = m_viewer->height() * 0.5;
  m_naviRectPos      = QPointF(w2 + c.x, h2 - c.y);
  const double navS  = std::sqrt(std::abs(navAff.det()));
  const double mainS = std::sqrt(std::abs(main->getViewMatrix().det()));
  const double r     = (navS > 1e-12) ? (mainS / navS) : 1.0;
  m_icon2ViewerRatio = QPointF(r, r);
}

//-----------------------------------------------------------------------------

void LocatorPopup::execOverviewPan(const QPointF &pos) {
  SceneViewer *main = resolveMainViewer();
  if (!main || !isOverview()) return;
  QPointF delta = m_naviRectPos - pos;
  delta.setX(delta.x() * m_icon2ViewerRatio.x());
  delta.setY(delta.y() * m_icon2ViewerRatio.y());
  if (std::abs(delta.x()) < 1e-6 && std::abs(delta.y()) < 1e-6) return;
  main->navigatorPan(delta);
}

//-----------------------------------------------------------------------------

void LocatorPopup::panOverviewByWorld(const TPointD &worldDelta) {
  SceneViewer *main = resolveMainViewer();
  if (!main || !isOverview()) return;
  if (std::abs(worldDelta.x) < 1e-12 && std::abs(worldDelta.y) < 1e-12) return;
  const TAffine aff       = main->getViewMatrix();
  const TPointD viewDelta = aff * worldDelta - aff * TPointD(0, 0);
  main->navigatorPan(QPointF(-viewDelta.x, viewDelta.y));
}

//-----------------------------------------------------------------------------

void LocatorPopup::captureLastNavAffs() {
  if (!m_viewer) return;
  for (int mode = 0; mode < 2; ++mode)
    m_lastNavAffs[mode] = m_viewer->getViewAffine(mode);
  m_haveLastNavAffs = true;
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyNavigatorSyncToMain() {
  if (m_navSyncing || !m_viewer || isLocatorRole() || isOverview()) {
    captureLastNavAffs();
    return;
  }
  const bool syncZoom = m_syncZoomAct && m_syncZoomAct->isChecked();
  const bool syncPan  = m_syncPanAct && m_syncPanAct->isChecked();
  if (!m_haveLastNavAffs) {
    captureLastNavAffs();
    return;
  }

  std::array<TAffine, 2> curr = {m_viewer->getViewAffine(0),
                                 m_viewer->getViewAffine(1)};
  SceneViewer *mainViewer     = nullptr;
  if (syncZoom || syncPan) mainViewer = resolveMainViewer();
  if (mainViewer) {
    m_navSyncing = true;
    for (int mode = 0; mode < 2; ++mode) {
      const TAffine &prev  = m_lastNavAffs[mode];
      const TAffine &now   = curr[mode];
      const double prevDet = std::abs(prev.det());
      const double nowDet  = std::abs(now.det());
      const double ratio =
          (prevDet > 1e-12) ? std::sqrt(nowDet / prevDet) : 1.0;
      const TPointD panDelta(now.a13 - prev.a13, now.a23 - prev.a23);
      const bool zoomed = std::abs(ratio - 1.0) > 1e-6;
      const bool panned =
          std::abs(panDelta.x) > 1e-6 || std::abs(panDelta.y) > 1e-6;

      TAffine d = mainViewer->getViewAffine(mode);
      if (syncZoom && zoomed) {
        const TPointD worldCenter = d.inv() * TPointD(0, 0);
        d                         = TAffine::scale(worldCenter, ratio) * d;
      }
      if (syncPan && panned && (!zoomed || syncZoom)) {
        d.a13 += panDelta.x;
        d.a23 += panDelta.y;
      }
      mainViewer->setViewMatrix(d, mode);
    }
    mainViewer->invalidateAll();
    m_navSyncing = false;
  }
  m_lastNavAffs     = curr;
  m_haveLastNavAffs = true;
}

//-----------------------------------------------------------------------------

void LocatorPopup::onNavigatorViewChanged() {
  applyNavigatorSyncToMain();
  updateNavViewFrame();
}

//-----------------------------------------------------------------------------

void LocatorPopup::persistPanelState() {
  QSettings settings;
  settings.beginGroup(QStringLiteral("LocatorNavigatorPanel"));
  writePanelStateTo(settings);
  settings.endGroup();
  settings.sync();
}

//-----------------------------------------------------------------------------

void LocatorPopup::fitOverviewMap() {
  if (!m_viewer || !isOverview() || m_navSyncing) return;
  if (m_viewer->width() < 8 || m_viewer->height() < 8) return;

  // Fit this viewer; the red frame still follows the main one.
  m_navSyncing = true;
  m_viewer->setViewForwardTarget(nullptr);
  m_viewer->fitToCamera();
  if (SceneViewer *main = resolveMainViewer())
    m_viewer->setViewForwardTarget(main);
  m_navSyncing = false;
  captureLastNavAffs();
  updateNavViewFrame();
}

//-----------------------------------------------------------------------------

void LocatorPopup::scheduleOverviewFit() {
  if (m_overviewFitScheduled || !isOverview()) return;
  m_overviewFitScheduled = true;
  QTimer::singleShot(0, this, [this]() {
    m_overviewFitScheduled = false;
    fitOverviewMap();
  });
}

//-----------------------------------------------------------------------------

void LocatorPopup::restoreOrFitView() {
  if (!m_viewer) return;

  if (isOverview()) {
    m_viewRestorePending = false;
    m_didInitialViewFit  = true;
    fitOverviewMap();
    return;
  }

  if (m_viewRestorePending) {
    m_viewRestorePending              = false;
    SceneViewer *viewer               = m_viewer;
    const std::array<TAffine, 2> affs = m_pendingViewAffs;
    QTimer::singleShot(0, this, [this, viewer, affs]() {
      for (int mode = 0; mode < 2; ++mode)
        viewer->setViewMatrix(affs[mode], mode);
      viewer->update();
      captureLastNavAffs();
    });
    m_didInitialViewFit = true;
    return;
  }

  if (!m_didInitialViewFit) {
    m_didInitialViewFit = true;
    m_viewer->fitToCamera();
  }
  captureLastNavAffs();
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavigatorBarsVisibility() {
  if (m_guidedLabel) m_guidedLabel->setVisible(m_showNavGuided);
  if (m_guidedCombo) m_guidedCombo->setVisible(m_showNavGuided);
  if (m_hideCurrentTb) m_hideCurrentTb->setVisible(m_showDisplayToolbar);
  if (m_soloColumnTb) m_soloColumnTb->setVisible(m_showDisplayToolbar);
  if (m_matchingStrokeTb) m_matchingStrokeTb->setVisible(m_showDisplayToolbar);
  if (m_overviewTb) m_overviewTb->setVisible(m_showNavOverview);
  if (m_navToolsOnlyTb) m_navToolsOnlyTb->setVisible(m_showNavToolsOnly);
  if (m_gearBtn) m_gearBtn->setVisible(m_showDisplayToolbar);
  const bool topRight =
      m_showDisplayToolbar || m_showNavOverview || m_showNavToolsOnly;
  if (m_navTopSpacerAfterGuided)
    m_navTopSpacerAfterGuided->setVisible(m_showNavGuided && topRight);
  if (m_navTopStretchWhenGuidedHidden)
    m_navTopStretchWhenGuidedHidden->setVisible(!m_showNavGuided && topRight);
  if (m_navTopBarHost) m_navTopBarHost->setVisible(m_showNavGuided || topRight);

  const auto setGroupVisible = [&](bool visible,
                                   std::initializer_list<int> ids) {
    for (int id : ids) {
      if (m_navBottomButtons[id]) m_navBottomButtons[id]->setVisible(visible);
    }
  };

  setGroupVisible(m_showNavTools, {NBB_ToolZoom, NBB_ToolHand, NBB_ToolRotate});
  setGroupVisible(m_showNavZoom, {NBB_ZoomIn, NBB_ZoomOut, NBB_ZoomReset});
  setGroupVisible(m_showNavView, {NBB_Fit, NBB_ResetView});
  setGroupVisible(m_showNavRotate, {NBB_RotL, NBB_RotR});
  setGroupVisible(m_showNavPan, {NBB_PanL, NBB_PanR, NBB_PanU, NBB_PanD});
  setGroupVisible(m_showNavFlip, {NBB_FlipH, NBB_FlipV});

  const auto updateSpacer = [&](QWidget *spacer, bool left, bool right) {
    if (spacer) spacer->setVisible(left && right);
  };
  const bool afterTools = m_showNavZoom || m_showNavView || m_showNavRotate ||
                          m_showNavPan || m_showNavFlip;
  updateSpacer(m_navBottomSpacerAfterTools, m_showNavTools, afterTools);
  updateSpacer(
      m_navBottomSpacerAfterZoom, m_showNavZoom,
      m_showNavView || m_showNavRotate || m_showNavPan || m_showNavFlip);
  updateSpacer(m_navBottomSpacerAfterView, m_showNavView,
               m_showNavRotate || m_showNavPan || m_showNavFlip);
  updateSpacer(m_navBottomSpacerAfterRotate, m_showNavRotate, m_showNavPan);
  updateSpacer(m_navBottomSpacerAfterPan, m_showNavPan, m_showNavFlip);

  if (m_navBottomBarHost) {
    const bool anyNavControl = m_showNavTools || m_showNavZoom ||
                               m_showNavView || m_showNavRotate ||
                               m_showNavPan || m_showNavFlip;
    m_navBottomBarHost->setVisible(anyNavControl);
  }

  updateNavigatorIconScale();
  updateTabPageSizeConsistency();
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateNavigatorIconScale() {
  int n = 0;
  for (int i = 0; i < NBB_COUNT; ++i) {
    if (m_navBottomButtons[i] && m_navBottomButtons[i]->isVisibleTo(this)) ++n;
  }

  int spacerCount        = 0;
  const auto countSpacer = [&](QWidget *w) {
    if (w && w->isVisibleTo(this)) ++spacerCount;
  };
  countSpacer(m_navBottomSpacerAfterTools);
  countSpacer(m_navBottomSpacerAfterZoom);
  countSpacer(m_navBottomSpacerAfterView);
  countSpacer(m_navBottomSpacerAfterRotate);
  countSpacer(m_navBottomSpacerAfterPan);

  int avail = width();
  if (m_navBottomBarHost && m_navBottomBarHost->isVisible() &&
      m_navBottomBarHost->width() > 8)
    avail = m_navBottomBarHost->width();

  const int rowGaps  = std::max(0, n + spacerCount - 1);
  const auto rowNeed = [&](int btn, int sp, int gap) {
    return n * btn + spacerCount * sp + rowGaps * gap;
  };

  double s       = 1.0;
  int spacer     = kNavGroupSpacer;
  int botSpacing = 2;
  if (n > 0 && avail > 0 &&
      rowNeed(kNavBottomBtnSize, kNavGroupSpacer, 2) > avail) {
    spacer = 0;
    for (int sp = kNavGroupSpacer - 1; sp >= 0; --sp) {
      if (rowNeed(kNavBottomBtnSize, sp, 2) <= avail) {
        spacer = sp;
        break;
      }
    }
    if (rowNeed(kNavBottomBtnSize, spacer, 2) > avail) botSpacing = 1;
    if (rowNeed(kNavBottomBtnSize, spacer, botSpacing) > avail) {
      const double ratio = (double)avail / (double)rowNeed(kNavBottomBtnSize,
                                                           spacer, botSpacing);
      s = std::max(kNavMinScale, 1.0 - kNavScaleDamp * (1.0 - ratio));
    }
  }

  const int botBtn =
      std::max(kNavMinBtnSize, (int)std::lround(kNavBottomBtnSize * s));
  const int botIcon =
      std::max(kNavMinIconSize, (int)std::lround(kNavBottomIconSize * s));
  const int topBtn =
      std::max(kNavMinBtnSize, (int)std::lround(kNavTopBtnSize * s));
  const int topIcon =
      std::max(kNavMinIconSize, (int)std::lround(kNavTopIconSize * s));

  const auto applyBtn = [](QToolButton *tb, int btn, int icon) {
    if (!tb) return;
    tb->setFixedSize(btn, btn);
    tb->setIconSize(QSize(icon, icon));
  };
  for (int i = 0; i < NBB_COUNT; ++i)
    applyBtn(m_navBottomButtons[i], botBtn, botIcon);
  applyBtn(m_hideCurrentTb, topBtn, topIcon);
  applyBtn(m_soloColumnTb, topBtn, topIcon);
  applyBtn(m_matchingStrokeTb, topBtn, topIcon);
  applyBtn(m_overviewTb, topBtn, topIcon);
  applyBtn(m_navToolsOnlyTb, topBtn, topIcon);
  applyBtn(m_gearBtn, topBtn, topIcon);

  if (m_guidedCombo) m_guidedCombo->setFixedHeight(topBtn);
  if (m_guidedLabel) m_guidedLabel->setFixedHeight(topBtn);
  if (m_navTopSpacerAfterGuided)
    m_navTopSpacerAfterGuided->setFixedWidth(
        std::max(2, (int)std::lround(kNavGroupSpacer * s)));
  if (m_navTopBarHost) m_navTopBarHost->setFixedHeight(topBtn + 2);

  if (m_navBottomLayout) m_navBottomLayout->setSpacing(botSpacing);
  const auto setSp = [&](QWidget *w) {
    if (w) w->setFixedWidth(spacer);
  };
  setSp(m_navBottomSpacerAfterTools);
  setSp(m_navBottomSpacerAfterZoom);
  setSp(m_navBottomSpacerAfterView);
  setSp(m_navBottomSpacerAfterRotate);
  setSp(m_navBottomSpacerAfterPan);
  if (m_navBottomBarHost && m_navBottomBarHost->isVisible())
    m_navBottomBarHost->setFixedHeight(botBtn);

  if (topIcon != m_navTopIconPx) {
    m_navTopIconPx = topIcon;
    updateNavigatorVisibilityIcons();
  }
}

//-----------------------------------------------------------------------------

void LocatorPopup::fillNavigatorContextMenu(QMenu *menu) {
  if (!isLocatorRole()) {
    QAction *fitAct = menu->addAction(tr("Fit to Camera"));
    connect(fitAct, &QAction::triggered, this, [this]() {
      if (SceneViewer *sv = viewToolTarget()) sv->fitToCamera();
    });
    QAction *resetAct = menu->addAction(tr("Reset View"));
    connect(resetAct, &QAction::triggered, this, [this]() {
      if (SceneViewer *sv = viewToolTarget()) sv->resetSceneViewer();
    });
    menu->addSeparator();
  }
  addShowHideContextMenu(menu);
}

//-----------------------------------------------------------------------------

void LocatorPopup::addShowHideContextMenu(QMenu *menu) {
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));

  QAction *guidedAct = showHideMenu->addAction(tr("Guided Drawing"));
  guidedAct->setCheckable(true);
  guidedAct->setChecked(m_showNavGuided);
  guidedAct->setObjectName(QStringLiteral("navGuided"));

  QAction *overviewAct = showHideMenu->addAction(tr("Use as Overview"));
  overviewAct->setCheckable(true);
  overviewAct->setChecked(m_showNavOverview);
  overviewAct->setObjectName(QStringLiteral("navOverview"));

  QAction *navToolsOnlyAct =
      showHideMenu->addAction(tr("Navigation tools only"));
  navToolsOnlyAct->setCheckable(true);
  navToolsOnlyAct->setChecked(m_showNavToolsOnly);
  navToolsOnlyAct->setObjectName(QStringLiteral("navToolsOnly"));

  QAction *displayToolbarAct = showHideMenu->addAction(tr("Display Toolbar"));
  displayToolbarAct->setCheckable(true);
  displayToolbarAct->setChecked(m_showDisplayToolbar);
  displayToolbarAct->setObjectName(QStringLiteral("displayToolbar"));

  showHideMenu->addSeparator();

  QAction *navBarAct = showHideMenu->addAction(tr("Navigation Bar"));
  navBarAct->setCheckable(true);
  navBarAct->setChecked(m_showNavTools);
  navBarAct->setObjectName(QStringLiteral("navTools"));

  QAction *zoomAct = showHideMenu->addAction(tr("Zoom"));
  zoomAct->setCheckable(true);
  zoomAct->setChecked(m_showNavZoom);
  zoomAct->setObjectName(QStringLiteral("navZoom"));

  QAction *viewAct = showHideMenu->addAction(tr("View"));
  viewAct->setCheckable(true);
  viewAct->setChecked(m_showNavView);
  viewAct->setObjectName(QStringLiteral("navView"));

  QAction *rotateAct = showHideMenu->addAction(tr("Rotate"));
  rotateAct->setCheckable(true);
  rotateAct->setChecked(m_showNavRotate);
  rotateAct->setObjectName(QStringLiteral("navRotate"));

  QAction *panAct = showHideMenu->addAction(tr("Pan"));
  panAct->setCheckable(true);
  panAct->setChecked(m_showNavPan);
  panAct->setObjectName(QStringLiteral("navPan"));

  QAction *flipAct = showHideMenu->addAction(tr("Flip"));
  flipAct->setCheckable(true);
  flipAct->setChecked(m_showNavFlip);
  flipAct->setObjectName(QStringLiteral("navFlip"));

  QActionGroup *group = new QActionGroup(menu);
  group->setExclusive(false);
  group->addAction(guidedAct);
  group->addAction(overviewAct);
  group->addAction(navToolsOnlyAct);
  group->addAction(displayToolbarAct);
  group->addAction(navBarAct);
  group->addAction(zoomAct);
  group->addAction(viewAct);
  group->addAction(rotateAct);
  group->addAction(panAct);
  group->addAction(flipAct);

  connect(guidedAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(overviewAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(navToolsOnlyAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(displayToolbarAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(navBarAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(zoomAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(viewAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(rotateAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(panAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
  connect(flipAct, &QAction::triggered, this,
          &LocatorPopup::onShowHideActionTriggered);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onShowHideActionTriggered() {
  QAction *action = qobject_cast<QAction *>(sender());
  if (!action) return;

  if (action->objectName() == QStringLiteral("navGuided"))
    m_showNavGuided = action->isChecked();
  else if (action->objectName() == QStringLiteral("navOverview"))
    m_showNavOverview = action->isChecked();
  else if (action->objectName() == QStringLiteral("navToolsOnly"))
    m_showNavToolsOnly = action->isChecked();
  else if (action->objectName() == QStringLiteral("displayToolbar"))
    m_showDisplayToolbar = action->isChecked();
  else if (action->objectName() == QStringLiteral("navTools"))
    m_showNavTools = action->isChecked();
  else if (action->objectName() == QStringLiteral("navZoom"))
    m_showNavZoom = action->isChecked();
  else if (action->objectName() == QStringLiteral("navView"))
    m_showNavView = action->isChecked();
  else if (action->objectName() == QStringLiteral("navRotate"))
    m_showNavRotate = action->isChecked();
  else if (action->objectName() == QStringLiteral("navPan"))
    m_showNavPan = action->isChecked();
  else if (action->objectName() == QStringLiteral("navFlip"))
    m_showNavFlip = action->isChecked();
  else
    return;

  updateNavigatorBarsVisibility();
  persistPanelState();
}

//-----------------------------------------------------------------------------

void LocatorPopup::setMatchingStrokeReferenceMode(bool on) {
  if (m_matchingStrokeReferenceMode == on) return;

  if (on) {
    m_matchingSuppressRestore.clear();
    TApp *app            = TApp::instance();
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
          m_matchingSuppressRestore.append(qMakePair(
              QPointer<SceneViewer>(sv), sv->getSuppressOnionSkinInViewer()));
          sv->setSuppressOnionSkinInViewer(true);
        }
        // Full GL refresh: PartialUpdate can leave stale onion-skin pixels
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
    updateNavigatorVisibilityIcons();
  }
}

//-----------------------------------------------------------------------------

void LocatorPopup::updateTabPageSizeConsistency() {
  if (!m_locatorTopSpacer || !m_locatorBottomSpacer || !m_navTopLayout ||
      !m_navBottomLayout || !m_viewer || !m_locatorPage || !m_navPage ||
      !m_tabBar)
    return;

  // Locator page stays classic: full-height canvas (Navigator keeps real
  // toolbars).
  m_locatorTopSpacer->setFixedHeight(0);
  m_locatorBottomSpacer->setFixedHeight(0);
  m_locatorPageLayout->setSpacing(0);

  m_locatorPage->setMinimumWidth(0);
  m_navPage->setMinimumWidth(0);
}

//-----------------------------------------------------------------------------

void LocatorPopup::onLocatorNavigatorPreferenceChanged(bool enabled) {
  applyNavigatorTabVisibilityFromPreferences(enabled);
  applyMultiInstanceAllowed(enabled);
  syncActiveLocatorRole();
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyMultiInstanceAllowed(bool allowed) {
  for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
    if (auto *panel = dynamic_cast<TPanel *>(w)) {
      panel->allowMultipleInstances(allowed);
      return;
    }
  }
}

//-----------------------------------------------------------------------------

bool LocatorPopup::isLocatorRole() const {
  if (!m_tabBar || m_tabBar->count() < 2) return true;
  return m_tabBar->currentIndex() == TabLocator;
}

//-----------------------------------------------------------------------------

void LocatorPopup::syncActiveLocatorRole() {
  TApp *app = TApp::instance();
  if (!app) return;
  if (isVisible() && isLocatorRole()) {
    app->setActiveLocator(this);
    return;
  }
  if (app->getActiveLocator() != this) return;
  LocatorPopup *other = nullptr;
  if (QWidget *mw = app->getMainWindow()) {
    const QList<LocatorPopup *> list = mw->findChildren<LocatorPopup *>();
    for (LocatorPopup *p : list) {
      if (p && p != this && p->isVisible() && p->isLocatorRole()) {
        other = p;
        break;
      }
    }
  }
  app->setActiveLocator(other);
}

//-----------------------------------------------------------------------------

void LocatorPopup::applyNavigatorTabVisibilityFromPreferences(
    bool navigatorEnabled) {
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
    if (isLocatorRole()) applyLocatorTabToViewer();
    applyOverviewMode();
    restoreToolAfterNav();
    if (m_viewer) m_viewer->update();
    return;
  }

  if (m_tabBar->count() < 2) return;

  if (m_matchingStrokeReferenceMode) setMatchingStrokeReferenceMode(false);

  if (m_tabBar->currentIndex() == TabNavigator) {
    captureTabView(TabNavigator);
    m_tabBar->blockSignals(true);
    m_tabBar->setCurrentIndex(TabLocator);
    m_stack->setCurrentIndex(TabLocator);
    m_tabBar->blockSignals(false);
    reparentViewerToTab(TabLocator);
    applyTabView(TabLocator);
    m_viewTabIndex = TabLocator;
  }

  m_tabBar->blockSignals(true);
  m_tabBar->removeTab(TabNavigator);
  m_stack->removeWidget(m_navPage);
  m_navPage->setParent(this);
  m_navPage->hide();
  m_tabBar->blockSignals(false);
  applyLocatorTabToViewer();
  applyOverviewMode();
  restoreToolAfterNav();
  updateNavigatorControlsEnabled();
  m_viewer->update();
  changeWindowTitle();
  persistPanelState();
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
  if (m_viewTabIndex >= 0 && m_viewTabIndex != index)
    captureTabView(m_viewTabIndex);
  if (m_stack && index < m_stack->count()) m_stack->setCurrentIndex(index);
  reparentViewerToTab(index);
  updateNavigatorControlsEnabled();
  if (index == TabLocator)
    applyLocatorTabToViewer();
  else {
    applyNavigatorTabToViewer();
    if (isOverview()) scheduleOverviewFit();
  }
  applyTabView(index);
  m_viewTabIndex = index;
  captureLastNavAffs();
  applyOverviewMode();
  if (index == TabLocator) restoreToolAfterNav();
  m_viewer->update();
  changeWindowTitle();

  updateNavigatorIconScale();
  updateTabPageSizeConsistency();
  persistPanelState();
  syncActiveLocatorRole();
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
  if (m_tabBar->count() > 1 && tab == TabNavigator) return;

  m_pendingLocatorPos = pos;
  if (m_locatorFollowScheduled) return;
  m_locatorFollowScheduled = true;
  QTimer::singleShot(0, this, [this]() {
    m_locatorFollowScheduled = false;
    if (!m_viewer) return;
    if (m_tabBar && m_tabBar->count() > 1 &&
        m_tabBar->currentIndex() == TabNavigator)
      return;
    const TAffine curAff = m_viewer->getSceneMatrix();
    const TAffine newAff(curAff.a11, 0, -m_pendingLocatorPos.x * curAff.a11, 0,
                         curAff.a22, -m_pendingLocatorPos.y * curAff.a22);
    m_viewer->setViewMatrix(newAff, 0);
    m_viewer->setViewMatrix(newAff, 1);
    m_viewer->update();
  });
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

void LocatorPopup::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  fillNavigatorContextMenu(&menu);
  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void LocatorPopup::resizeEvent(QResizeEvent *event) {
  QFrame::resizeEvent(event);
  updateNavigatorIconScale();
  if (isOverview()) scheduleOverviewFit();
}

//-----------------------------------------------------------------------------

void LocatorPopup::showEvent(QShowEvent *) {
  QTimer::singleShot(0, this, [this]() {
    restoreOrFitView();
    updateNavigatorIconScale();
  });

  TApp *app                    = TApp::instance();
  TFrameHandle *frameHandle    = app->getCurrentFrame();
  TXshLevelHandle *levelHandle = app->getCurrentLevel();

  bool ret = true;
  ret      = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
                            SLOT(onFrameOrLevelForNav()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
                       SLOT(onFrameOrLevelForNav()));
  assert(ret);

  syncActiveLocatorRole();
  captureLastNavAffs();
  hookNavFrameSource();

  changeWindowTitle();

  updateTabPageSizeConsistency();
  refreshNavigatorThemedIcons();
}

//-----------------------------------------------------------------------------

bool LocatorPopup::eventFilter(QObject *watched, QEvent *event) {
  if (watched != m_viewer) return QFrame::eventFilter(watched, event);

  if (event->type() == QEvent::ContextMenu && !isLocatorRole()) {
    auto *ce = static_cast<QContextMenuEvent *>(event);
    QMenu menu(this);
    fillNavigatorContextMenu(&menu);
    menu.exec(ce->globalPos());
    return true;
  }

  if (isLocatorRole()) return QFrame::eventFilter(watched, event);

  const bool overview = isOverview();
  const bool navOnly  = isNavToolsOnly();

  if (!overview && !navOnly) return QFrame::eventFilter(watched, event);

  const QEvent::Type type = event->type();
  if (overview && type == QEvent::MouseButtonDblClick) {
    if (SceneViewer *main = resolveMainViewer()) main->fitToCamera();
    return true;
  }
  if (type == QEvent::Enter) {
    m_navPickHover = false;
    if (usesNavHand()) {
      updateOverviewCursor();
      return true;
    }
    return QFrame::eventFilter(watched, event);
  }
  if (!isPointerEvent(type)) return QFrame::eventFilter(watched, event);

  const QPointF pos =
      pointerLocalPos(event) * (qreal)m_viewer->getDevPixRatio();
  const Qt::MouseButton button   = pointerButton(event);
  const Qt::MouseButtons buttons = pointerButtons(event);

  if (!m_overviewHandDragging && !m_navPickDragging)
    m_navPickHover = !overview && navOnly && canNavPickAt(pos);

  const bool treatAsHand = usesNavHand();
  const bool pickPass    = !overview && navOnly && isNavPickTool() &&
                        (m_navPickHover || m_navPickDragging);
  const bool blockDraw =
      navOnly && !treatAsHand && !isViewNavTool() && !pickPass;

  if (treatAsHand || blockDraw) updateOverviewCursor();

  if (pickPass && isPointerPress(type) && button == Qt::LeftButton)
    m_navPickDragging = true;
  if (isPointerRelease(type) && m_navPickDragging) m_navPickDragging = false;

  if (treatAsHand && isPointerPress(type) && button == Qt::LeftButton) {
    m_overviewHandDragging = true;
    m_overviewWorldPos     = m_viewer->winToWorld(pos);
    m_navHandScreenPos     = pos;
    return true;
  }
  if (treatAsHand && isPointerMove(type) && m_overviewHandDragging &&
      (buttons & Qt::LeftButton)) {
    if (overview) {
      const TPointD world = m_viewer->winToWorld(pos);
      panOverviewByWorld(world - m_overviewWorldPos);
      m_overviewWorldPos = world;
    } else {
      m_viewer->navigatorPan(pos - m_navHandScreenPos);
      m_navHandScreenPos = pos;
    }
    return true;
  }
  if (treatAsHand && isPointerRelease(type) && m_overviewHandDragging) {
    m_overviewHandDragging = false;
    return true;
  }

  if (overview && isPointerPress(type) && button == Qt::MiddleButton) {
    m_draggingNavFrame = true;
    execOverviewPan(pos);
    return true;
  }
  if (overview && isPointerMove(type) && m_draggingNavFrame &&
      (buttons & Qt::MiddleButton)) {
    execOverviewPan(pos);
    return true;
  }
  if (overview && isPointerRelease(type) && m_draggingNavFrame) {
    m_draggingNavFrame = false;
    return true;
  }
  if (treatAsHand || blockDraw) return true;
  return QFrame::eventFilter(watched, event);
}

//-----------------------------------------------------------------------------

void LocatorPopup::leaveEvent(QEvent *event) {
  QFrame::leaveEvent(event);
  m_navPickHover    = false;
  m_navPickDragging = false;
  if (!rect().contains(mapFromGlobal(QCursor::pos()))) restoreToolAfterNav();
}

//-----------------------------------------------------------------------------

void LocatorPopup::hideEvent(QHideEvent *) {
  m_draggingNavFrame     = false;
  m_overviewHandDragging = false;
  m_navPickHover         = false;
  m_navPickDragging      = false;
  restoreToolAfterNav();
  persistPanelState();
  TApp *app = TApp::instance();
  disconnect(app->getCurrentLevel());
  disconnect(app->getCurrentFrame());
  syncActiveLocatorRole();
}

//-----------------------------------------------------------------------------

void LocatorPopup::onFrameOrLevelForNav() {
  changeWindowTitle();
  updateNavViewFrame();
}

void LocatorPopup::changeWindowTitle() {
  TApp *app = TApp::instance();
  QString name =
      (m_tabBar->count() > 1 && m_tabBar->currentIndex() == TabNavigator)
          ? tr("Navigator")
          : tr("Locator");

  bool showZoomFactor = false;

  if (app->getCurrentFrame()->isEditingScene()) {
    if (m_viewer->isPreviewEnabled())
      showZoomFactor = true;
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
