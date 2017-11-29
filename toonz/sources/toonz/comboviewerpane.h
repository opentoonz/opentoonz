#pragma once

#ifndef COMBOVIEWER_PANE_INCLUDED
#define COMBOVIEWER_PANE_INCLUDED

#include "styleshortcutswitchablepanel.h"
#include "sceneviewer.h"
#include "toonzqt/intfield.h"
#include "toonzqt/keyframenavigator.h"

#include "toonzqt/flipconsoleowner.h"

class QPoint;
class QToolBar;
class QLabel;
class QSlider;
class QActionGroup;
class QButtonGroup;
class QToolBar;
class Ruler;

class Toolbar;
class TPanel;
class Ruler;
class FlipConsole;
class TXshLevel;
class ToolOptions;

//=============================================================================
// ComboViewerPanel
//-----------------------------------------------------------------------------
enum CV_Parts {
  CVPARTS_TOOLBAR = 0,
  CVPARTS_TOOLOPTIONS,
  CVPARTS_FLIPCONSOLE,
  CVPARTS_COUNT
};
//-----------------------------------------------------------------------------

class ComboViewerPanel final : public StyleShortcutSwitchablePanel,
                               public FlipConsoleOwner {
  Q_OBJECT

  SceneViewer *m_sceneViewer;
  FlipConsole *m_flipConsole;
  ViewerKeyframeNavigator *m_keyFrameButton;
  TPanelTitleBarButtonSet *m_referenceModeBs;

  Toolbar *m_toolbar;
  ToolOptions *m_toolOptions;
  Ruler *m_vRuler;
  Ruler *m_hRuler;
  bool m_visibleFlag[CVPARTS_COUNT];
  bool m_onionSkinActive = false;
  bool m_playSound       = true;
  bool m_hasSoundtrack   = false;
  bool m_playing         = false;
  double m_fps;
  int m_viewerFps;
  double m_samplesPerFrame;
  bool m_first         = true;
  TSoundTrack *m_sound = NULL;

  TPanelTitleBarButton *m_previewButton;
  TPanelTitleBarButton *m_subcameraPreviewButton;

public:
#if QT_VERSION >= 0x050500
  ComboViewerPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ComboViewerPanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ComboViewerPanel();

  SceneViewer *getSceneViewer() { return m_sceneViewer; }

  // toggle show/hide of the widgets according to m_visibleFlag
  void updateShowHide();
  void addShowHideContextMenu(QMenu *);
  void setShowHideFlag(CV_Parts parts, bool visible) {
    m_visibleFlag[parts] = visible;
  }
  bool getShowHideFlag(CV_Parts parts) { return m_visibleFlag[parts]; }
  // reimplementation of TPanel::widgetInThisPanelIsFocused
  bool widgetInThisPanelIsFocused() override {
    return m_sceneViewer->hasFocus();
  }

  void onDrawFrame(int frame,
                   const ImagePainter::VisualSettings &settings) override;

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void initializeTitleBar(TPanelTitleBar *titleBar);
  void createFrameToolBar();
  void createPlayToolBar();
  void addColorMaskButton(QWidget *parent, const char *iconSVGName, int id);
  void contextMenuEvent(QContextMenuEvent *event) override;
  // reimplementation of TPanel::widgetFocusOnEnter
  void widgetFocusOnEnter() override {
    m_sceneViewer->setFocus(Qt::OtherFocusReason);
    // activate shortcut key for this flipconsole
    m_flipConsole->makeCurrent();
  };
  void widgetClearFocusOnLeave() override { m_sceneViewer->clearFocus(); };
  void playAudioFrame(int frame);
  bool hasSoundtrack();

public slots:
  void onSceneChanged();
  void changeWindowTitle();
  void updateFrameRange();
  void onXshLevelSwitched(TXshLevel *);
  void onPlayingStatusChanged(bool playing);
  // for showing/hiding the parts
  void onShowHideActionTriggered(QAction *);
  void enableFlipConsoleForCamerastand(bool on);
  void onButtonPressed(FlipConsole::EGadget button);

protected slots:
  void onFrameChanged();

  // need to update the preview marker as well as the frame range in flipconsole
  void onFrameTypeChanged();

  void onSceneSwitched();
  void enableFullPreview(bool enabled);
  void enableSubCameraPreview(bool enabled);

  void onPreferenceChanged(const QString &prefName) override;
};

#endif
