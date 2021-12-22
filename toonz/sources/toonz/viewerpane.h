#pragma once

#ifndef VIEWER_PANE_INCLUDED
#define VIEWER_PANE_INCLUDED

#include "sceneviewer.h"
#include "toonzqt/intfield.h"
#include "toonzqt/keyframenavigator.h"
#include "toonzqt/flipconsoleowner.h"
#include "saveloadqsettings.h"

#include <QFrame>

class SceneViewer;
class QPoint;
class QToolBar;
class QLabel;
class QSlider;
class QActionGroup;
class QButtonGroup;
class QToolBar;
class Ruler;

//=============================================================================
// ViewerPanel
//-----------------------------------------------------------------------------

class FlipConsole;
class TXshLevel;
class SceneViewerPanel final : public QFrame,
                               public FlipConsoleOwner,
                               public SaveLoadQSettings {
  Q_OBJECT

  friend class SceneViewer;
  SceneViewer *m_sceneViewer;
  FlipConsole *m_flipConsole;
  ViewerKeyframeNavigator *m_keyFrameButton;

  TPanelTitleBarButtonSet *m_referenceModeBs;
  TPanelTitleBarButton *m_previewButton;
  TPanelTitleBarButton *m_subcameraPreviewButton;
  bool m_onionSkinActive = false;
  UINT m_visiblePartsFlag;
  bool m_playSound       = true;
  bool m_hasSoundtrack   = false;
  bool m_playing         = false;
  double m_fps;
  int m_viewerFps;
  double m_samplesPerFrame;
  bool m_first         = true;
  TSoundTrack *m_sound = NULL;

public:
#if QT_VERSION >= 0x050500
  SceneViewerPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  SceneViewerPanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~SceneViewerPanel();

  // toggle show/hide of the widgets according to m_visiblePartsFlag
  void setVisiblePartsFlag(UINT flag);
  void updateShowHide();
  void addShowHideContextMenu(QMenu*);

  void onDrawFrame(int frame,
                   const ImagePainter::VisualSettings &settings) override;

  void onEnterPanel() {
      m_sceneViewer->setFocus(Qt::OtherFocusReason);
      // activate shortcut key for this flipconsole
      m_flipConsole->makeCurrent();
  }
  void onLeavePanel() { m_sceneViewer->clearFocus(); }

  // SaveLoadQSettings
  virtual void save(QSettings& settings) const override;
  virtual void load(QSettings& settings) override;

  void initializeTitleBar(TPanelTitleBar* titleBar);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void createFrameToolBar();
  void createPlayToolBar();
  void addColorMaskButton(QWidget *parent, const char *iconSVGName, int id);
  // reimplementation of TPanel::widgetFocusOnEnter

  void enableFlipConsoleForCamerastand(bool on);
  void playAudioFrame(int frame);
  bool hasSoundtrack();
  void contextMenuEvent(QContextMenuEvent* event) override;

public slots:

  void changeWindowTitle();
  void onSceneChanged();
  void onXshLevelSwitched(TXshLevel *);
  void updateFrameRange();
  void updateFrameMarkers();
  void onButtonPressed(FlipConsole::EGadget button);
  void setFlipHButtonChecked(bool checked);
  void setFlipVButtonChecked(bool checked);

protected slots:

  void onFrameSwitched();
  void onSceneSwitched();
  void onFrameTypeChanged();
  void onPlayingStatusChanged(bool playing);
  // for showing/hiding the parts
  void onShowHideActionTriggered(QAction*);
  void enableFullPreview(bool enabled);
  void enableSubCameraPreview(bool enabled);
};

#endif
