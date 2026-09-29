#pragma once

#ifndef LOCATORPOPUP_H
#define LOCATORPOPUP_H

#include "tgeometry.h"
#include "toonzqt/dvdialog.h"

#include <QList>
#include <QPair>
#include <QPointer>

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

//=============================================================================
// LocatorPopup — Locator tab (minimal) and Navigator tab (display overrides).
//=============================================================================

class LocatorPopup : public QFrame {
  Q_OBJECT
  SceneViewer *m_viewer;
  bool m_initialZoom;

  TabBarContainter *m_tabBarHost;
  QTabBar *m_tabBar;
  QStackedWidget *m_stack;
  QWidget *m_locatorPage;
  //! Kept for layout index stability; height 0 on Locator (Navigator keeps real toolbars).
  QWidget *m_locatorTopSpacer;
  QWidget *m_locatorBottomSpacer;
  QWidget *m_navPage;
  QVBoxLayout *m_locatorPageLayout;
  QVBoxLayout *m_navPageLayout;
  QHBoxLayout *m_navTopLayout;
  QHBoxLayout *m_navBottomLayout;

  QComboBox *m_guidedCombo;
  QToolButton *m_hideCurrentTb;
  QToolButton *m_soloColumnTb;
  QToolButton *m_matchingStrokeTb;
  QToolButton *m_gearBtn;
  QAction *m_followMainPanAct;
  bool m_matchingStrokeReferenceMode = false;
  QList<QPair<QPointer<SceneViewer>, bool>> m_matchingSuppressRestore;

  enum TabIndex { TabLocator = 0, TabNavigator = 1 };

  enum NavBottomButtonIndex {
    NBB_ZoomIn = 0,
    NBB_ZoomOut,
    NBB_ZoomReset,
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

  void buildNavigatorToolbar();
  void buildNavigatorBottomBar();
  void refreshNavigatorThemedIcons();
  QColor themeIconBaseColor() const;
  void reparentViewerToTab(int tabIndex);
  void applyLocatorTabToViewer();
  void applyNavigatorTabToViewer();
  void updateNavigatorControlsEnabled();
  void persistTabIndex();
  void updateTabPageSizeConsistency();
  void setMatchingStrokeReferenceMode(bool on);
  void applyNavigatorTabVisibilityFromPreferences(bool navigatorEnabled);
  void updateLocatorTabBarChrome();

public:
  LocatorPopup(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~LocatorPopup() override;

  SceneViewer *viewer() { return m_viewer; }

  void onChangeViewAff(const TPointD &curPos);

protected:
  void changeEvent(QEvent *e) override;
  void showEvent(QShowEvent *);
  void hideEvent(QHideEvent *);

protected slots:
  void changeWindowTitle();
  void onLocatorNavigatorPreferenceChanged(bool enabled);
  void onTabIndexChanged(int index);
  void onGuidedComboChanged(int index);
  void onHideCurrentToggled(bool checked);
  void onSoloColumnToggled(bool checked);
  void onMatchingStrokeToggled(bool checked);
};

#endif
