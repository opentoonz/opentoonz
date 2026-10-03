#pragma once

#ifndef LOCATORPOPUP_H
#define LOCATORPOPUP_H

#include "saveloadqsettings.h"
#include "tgeometry.h"
#include "toonzqt/dvdialog.h"

#include <QFrame>
#include <QString>
#include <QPointF>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QSettings>

#include <array>

class QMenu;

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class SceneViewer;
class QComboBox;
class QTabBar;
class QStackedWidget;
class QToolButton;
class TabBarContainter;
class QEvent;
class QHBoxLayout;
class QVBoxLayout;
class QAction;
class QContextMenuEvent;
class QLabel;
class QResizeEvent;

//=============================================================================
// LocatorPopup — Locator tab (minimal) and Navigator tab (Xsheet display
// options).
//=============================================================================

class LocatorPopup : public QFrame, public SaveLoadQSettings {
  Q_OBJECT
  SceneViewer *m_viewer;

  TabBarContainter *m_tabBarHost;
  QTabBar *m_tabBar;
  QStackedWidget *m_stack;
  QWidget *m_locatorPage;
  //! Kept for layout index stability; height 0 on Locator (Navigator keeps real
  //! toolbars).
  QWidget *m_locatorTopSpacer;
  QWidget *m_locatorBottomSpacer;
  QWidget *m_navPage;
  QVBoxLayout *m_locatorPageLayout;
  QVBoxLayout *m_navPageLayout;
  QHBoxLayout *m_navTopLayout;
  QHBoxLayout *m_navBottomLayout;
  QWidget *m_navTopBarHost;
  QWidget *m_navBottomBarHost;

  QComboBox *m_guidedCombo;
  QLabel *m_guidedLabel;
  QToolButton *m_hideCurrentTb;
  QToolButton *m_soloColumnTb;
  QToolButton *m_matchingStrokeTb;
  QToolButton *m_gearBtn        = nullptr;
  QToolButton *m_overviewTb     = nullptr;
  QToolButton *m_navToolsOnlyTb = nullptr;
  QAction *m_overviewAct        = nullptr;
  QAction *m_navToolsOnlyAct    = nullptr;
  QAction *m_syncZoomAct        = nullptr;
  QAction *m_syncPanAct         = nullptr;
  QPointer<SceneViewer> m_navFrameSource;
  bool m_navSyncing           = false;
  bool m_draggingNavFrame     = false;
  bool m_overviewHandDragging = false;
  TPointD m_overviewWorldPos;
  QPointF m_navHandScreenPos;
  QPointF m_naviRectPos;
  QPointF m_icon2ViewerRatio;
  bool m_haveLastNavAffs                 = false;
  std::array<TAffine, 2> m_lastNavAffs   = {TAffine(), TAffine()};
  std::array<TAffine, 2> m_locatorAffs   = {TAffine(), TAffine()};
  std::array<TAffine, 2> m_navigatorAffs = {TAffine(), TAffine()};
  bool m_haveLocatorAffs                 = false;
  bool m_haveNavigatorAffs               = false;
  int m_viewTabIndex                     = -1;
  bool m_matchingStrokeReferenceMode     = false;
  bool m_showDisplayToolbar              = true;
  bool m_showNavGuided                   = false;
  bool m_showNavOverview                 = false;
  bool m_showNavToolsOnly                = false;
  bool m_showNavTools                    = true;
  bool m_showNavZoom                     = true;
  bool m_showNavView                     = true;
  bool m_showNavRotate                   = true;
  bool m_showNavPan                      = false;
  bool m_showNavFlip                     = true;
  QWidget *m_navTopSpacerAfterGuided;
  QWidget *m_navTopStretchWhenGuidedHidden;
  QWidget *m_navBottomSpacerAfterTools;
  QWidget *m_navBottomSpacerAfterZoom;
  QWidget *m_navBottomSpacerAfterView;
  QWidget *m_navBottomSpacerAfterRotate;
  QWidget *m_navBottomSpacerAfterPan;
  QList<QPair<QPointer<SceneViewer>, bool>> m_matchingSuppressRestore;
  bool m_viewRestorePending     = false;
  bool m_didInitialViewFit      = false;
  bool m_overviewFitScheduled   = false;
  bool m_navFrameScheduled      = false;
  bool m_locatorFollowScheduled = false;
  TPointD m_pendingLocatorPos;
  std::array<TAffine, 2> m_pendingViewAffs = {TAffine(), TAffine()};
  QString m_toolBeforeNav;
  bool m_haveToolBeforeNav = false;
  bool m_navPickHover      = false;
  bool m_navPickDragging   = false;

  enum TabIndex { TabLocator = 0, TabNavigator = 1 };

  enum NavBottomButtonIndex {
    NBB_ToolZoom = 0,
    NBB_ToolHand,
    NBB_ToolRotate,
    NBB_ZoomIn,
    NBB_ZoomOut,
    NBB_ZoomReset,
    NBB_Fit,
    NBB_ResetView,
    NBB_RotL,
    NBB_RotR,
    NBB_PanL,
    NBB_PanR,
    NBB_PanU,
    NBB_PanD,
    NBB_FlipH,
    NBB_FlipV,
    NBB_COUNT
  };

  QToolButton *m_navBottomButtons[NBB_COUNT]{};
  int m_navTopIconPx = 14;

  void buildNavigatorToolbar();
  void buildNavigatorBottomBar();
  void refreshNavigatorThemedIcons();
  void updateNavigatorVisibilityIcons();
  void updateNavigatorToolbarTooltips();
  QColor themeIconBaseColor() const;
  void reparentViewerToTab(int tabIndex);
  void applyLocatorTabToViewer();
  void applyNavigatorTabToViewer();
  void captureTabView(int tab);
  void applyTabView(int tab);
  void updateNavigatorControlsEnabled();
  void writePanelStateTo(QSettings &settings) const;
  void readPanelStateFrom(QSettings &settings);
  bool readViewAffsFrom(QSettings &settings);
  bool readAffsFrom(QSettings &settings, const QString &prefix,
                    std::array<TAffine, 2> &out);
  void applyLoadedTabIndex(int tab);
  SceneViewer *resolveMainViewer() const;
  SceneViewer *viewToolTarget() const;
  bool isOverview() const;
  bool isNavToolsOnly() const;
  bool isViewNavTool() const;
  bool isNavPickTool() const;
  bool canNavPickAt(const QPointF &winPos) const;
  void rememberToolBeforeNav();
  void restoreToolAfterNav();
  bool overviewUsesNavHand() const;
  bool usesNavHand() const;
  bool usesNavOnlyCursor() const;
  void updateOverviewCursor();
  void updateNavToolButtonChecks();
  void applyOverviewMode();
  void captureLastNavAffs();
  void applyNavigatorSyncToMain();
  void hookNavFrameSource();
  void scheduleNavFrameUpdate();
  void updateNavViewFrame();
  void execOverviewPan(const QPointF &pos);
  void panOverviewByWorld(const TPointD &worldDelta);
  void persistPanelState();
  void fitOverviewMap();
  void scheduleOverviewFit();
  void restoreOrFitView();
  void updateNavigatorBarsVisibility();
  void updateNavigatorIconScale();
  void fillNavigatorContextMenu(QMenu *menu);
  void addShowHideContextMenu(QMenu *menu);
  void updateTabPageSizeConsistency();
  void setMatchingStrokeReferenceMode(bool on);
  void applyNavigatorTabVisibilityFromPreferences(bool navigatorEnabled);
  void applyMultiInstanceAllowed(bool allowed);
  void syncActiveLocatorRole();
  bool isLocatorRole() const;
  void updateLocatorTabBarChrome();

public:
  LocatorPopup(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~LocatorPopup() override;

  // SaveLoadQSettings — persisted in room layout and floating popups.ini
  void save(QSettings &settings) const override;
  void load(QSettings &settings) override;

  SceneViewer *viewer() { return m_viewer; }

  void onChangeViewAff(const TPointD &curPos);

protected:
  void changeEvent(QEvent *e) override;
  void resizeEvent(QResizeEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void showEvent(QShowEvent *);
  void hideEvent(QHideEvent *);
  void leaveEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

protected slots:
  void changeWindowTitle();
  void onLocatorNavigatorPreferenceChanged(bool enabled);
  void onTabIndexChanged(int index);
  void onGuidedComboChanged(int index);
  void onHideCurrentToggled(bool checked);
  void onSoloColumnToggled(bool checked);
  void onMatchingStrokeToggled(bool checked);
  void onShowHideActionTriggered();
  void onNavigatorViewChanged();
  void onFrameOrLevelForNav();
};

#endif
