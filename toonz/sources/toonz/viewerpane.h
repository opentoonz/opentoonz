#pragma once

#ifndef VIEWER_PANE_INCLUDED
#define VIEWER_PANE_INCLUDED

#include "pane.h"
#include "sceneviewer.h"
#include "toonzqt/intfield.h"
#include "toonzqt/keyframenavigator.h"
#include "toonzqt/flipconsoleowner.h"

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
class SceneViewerPanel final : public TPanel, public FlipConsoleOwner {
  Q_OBJECT

  friend class SceneViewer;
  SceneViewer *m_sceneViewer;
  FlipConsole *m_flipConsole;
  ViewerKeyframeNavigator *m_keyFrameButton;

  TPanelTitleBarButtonSet *m_referenceModeBs;
  TPanelTitleBarButton *m_previewButton;
  TPanelTitleBarButton *m_subcameraPreviewButton;
  bool m_onionSkinActive = false;

public:
#if QT_VERSION >= 0x050500
  SceneViewerPanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  SceneViewerPanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~SceneViewerPanel();

  void onDrawFrame(int frame,
                   const ImagePainter::VisualSettings &settings) override;

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void initializeTitleBar(TPanelTitleBar *titleBar);
  void createFrameToolBar();
  void createPlayToolBar();
  void addColorMaskButton(QWidget *parent, const char *iconSVGName, int id);
  void enableFlipConsoleForCamerastand(bool on);

public slots:

  void changeWindowTitle();
  void onSceneChanged();
  void onXshLevelSwitched(TXshLevel *);
  void updateFrameRange();
  void updateFrameMarkers();

protected slots:

  void onFrameSwitched();
  void onSceneSwitched();
  void onFrameTypeChanged();
  void onPlayingStatusChanged(bool playing);
  void enableFullPreview(bool enabled);
  void enableSubCameraPreview(bool enabled);
};

#endif
