#pragma once

#ifndef TPANELS_INCLUDED
#define TPANELS_INCLUDED

#include "pane.h"
#include "styleshortcutswitchablepanel.h"

#include "tpalette.h"
#include "trenderer.h"

#include "tfx.h"

#include "toonz/tstageobjectid.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/functionviewer.h"
class PaletteViewer;
class TPaletteHandle;
class StyleEditor;
class StopMotionController;
class TLevel;
class StudioPaletteViewer;
class TPanelTitleBarButton;
class SchematicViewer;
class FunctionViewer;
class FlipBook;
class ToolOptions;
class ComboViewerPanel;
class SceneViewerPanel;
class FxSettings;
class VectorGuidedDrawingPane;
class FxSelection;
class StageObjectSelection;

//=========================================================
// PaletteViewerPanel
//---------------------------------------------------------

class PaletteViewerPanel final : public StyleShortcutSwitchablePanel {
  Q_OBJECT

  TPaletteHandle *m_paletteHandle;
  PaletteViewer *m_paletteViewer;

  TPanelTitleBarButton *m_freezeButton;
  bool m_isFrozen;
  TPaletteP m_frozenPalette;

public:
  PaletteViewerPanel(QWidget *parent);

  void setViewType(int viewType) override;
  int getViewType() override;

  void reset() override;

  bool isFrozen() { return m_isFrozen; }
  void setFrozen(bool frozen) { m_isFrozen = frozen; }

protected:
  void initializeTitleBar();
  bool isActivatableOnEnter() override { return true; }
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void onColorStyleSwitched();
  void onPaletteSwitched();
  void onFreezeButtonToggled(bool isFrozen);
  void onSceneSwitched();
  void onPreferenceChanged(const QString &prefName);
};

//=========================================================
// StudioPaletteViewerPanel
//---------------------------------------------------------

class StudioPaletteViewerPanel final : public TPanel {
  Q_OBJECT

  TPaletteHandle *m_studioPaletteHandle;
  StudioPaletteViewer *m_studioPaletteViewer;

public:
  StudioPaletteViewerPanel(QWidget *parent);

  void setViewType(int viewType) override;
  int getViewType() override;

protected:
  bool isActivatableOnEnter() override { return true; }
protected slots:
  void onColorStyleSwitched();
  void onPaletteSwitched();
};

//=========================================================
// StyleEditorPanel
//---------------------------------------------------------

class StyleEditorPanel final : public TPanel {
  Q_OBJECT
  StyleEditor *m_styleEditor;

public:
  StyleEditorPanel(QWidget *parent);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
protected slots:
  void onPreferenceChanged(const QString &prefName);
};

//=========================================================
// ColorFieldEditorController
//---------------------------------------------------------

class ColorFieldEditorController final
    : public QObject,
      public DVGui::ColorField::ColorFieldEditorController {
  Q_OBJECT

  TPaletteP m_palette;
  TPaletteHandle *m_colorFieldHandle;
  DVGui::ColorField *m_currentColorField;

public:
  ColorFieldEditorController();
  ~ColorFieldEditorController() {}

  // Indice dello stile corrente == 1
  void edit(DVGui::ColorField *colorField) override;
  void hide() override;

protected slots:
  void onColorStyleChanged(bool);
  void onColorChanged(const TPixel32 &color, bool);
};

//=========================================================
// CleanupColorFieldEditorController
//---------------------------------------------------------

class CleanupColorFieldEditorController final
    : public QObject,
      public DVGui::CleanupColorField::CleanupColorFieldEditorController {
  Q_OBJECT

  TPaletteP m_palette;
  TPaletteHandle *m_colorFieldHandle;
  DVGui::CleanupColorField *m_currentColorField;
  bool m_blackColor;

public:
  CleanupColorFieldEditorController();
  ~CleanupColorFieldEditorController() {}

  // Indice dello stile corrente == 1
  void edit(DVGui::CleanupColorField *colorField) override;
  void hide() override;

protected slots:
  void onColorStyleChanged();
};

//=========================================================
// SchematicScenePanel
//---------------------------------------------------------

class SchematicScenePanel final : public TPanel {
  Q_OBJECT

  SchematicViewer *m_schematicViewer;

public:
  SchematicScenePanel(QWidget *parent);

  void setViewType(int viewType) override;
  int getViewType() override;

protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

protected slots:
  void onShowPreview(TFxP fx);
  void onCollapse(const QList<TFxP> &);
  void onCollapse(QList<TStageObjectId>);
  void onExplodeChild(const QList<TFxP> &);
  void onExplodeChild(QList<TStageObjectId>);
  void onEditObject();
  void onDeleteFxs(const FxSelection *);
  void onDeleteStageObjects(const StageObjectSelection *);
  void onColumnPaste(const QList<TXshColumnP> &);
};

//=========================================================
// FunctionViewerPanel
//---------------------------------------------------------

class FunctionViewerPanel final : public TPanel {
  Q_OBJECT

  FunctionViewer *m_functionViewer;

public:
  FunctionViewerPanel(QWidget *parent = 0);

  void reset() override;

  void attachHandles();
  void detachHandles();

  bool widgetInThisPanelIsFocused() override;

protected:
  void widgetFocusOnEnter() override;
  void widgetClearFocusOnLeave() override;

public slots:

  void onIoCurve(int type, TDoubleParam *curve, const std::string &name);
  void onEditObject();
};

//=========================================================
// ToolOptionPanel
//---------------------------------------------------------

class ToolOptionPanel final : public TPanel {
  Q_OBJECT

  ToolOptions *m_toolOption;

public:
  ToolOptionPanel(QWidget *parent);
};

class CommandBarPanel final : public TPanel {
  Q_OBJECT

  // ToolOptions *m_toolOption;

public:
  CommandBarPanel(QWidget *parent);
};

//=========================================================
// FlipbookPanel
//---------------------------------------------------------
// share the base class between Flipbook and Color Model panels
class FlipbookBasePanel : public TPanel {
  Q_OBJECT
protected:
  FlipBook *m_flipbook;

public:
  FlipbookBasePanel(QWidget *parent) : TPanel(parent) {}
  void zoomContentsAndFitGeometry(bool forward) override;
  void setFlipbook(FlipBook *fb) { m_flipbook = fb; }
};

class FlipbookPanel final : public FlipbookBasePanel {
  Q_OBJECT

  QSize m_floatingSize;
  TPanelTitleBarButton *m_button;

protected:
  void initializeTitleBar(TPanelTitleBar *titleBar);

public:
  FlipbookPanel(QWidget *parent);

  void reset() override;
  // disable minimize button when docked
  void onDock(bool docked) override;

protected slots:
  void onMinimizeButtonToggled(bool);
};

//=========================================================
// ComboViewerPanel
//---------------------------------------------------------

class ComboViewerPanelContainer final : public StyleShortcutSwitchablePanel {
  Q_OBJECT
  ComboViewerPanel *m_comboViewer;

public:
  ComboViewerPanelContainer(QWidget *parent);
  // reimplementation of TPanel::widgetInThisPanelIsFocused
  bool widgetInThisPanelIsFocused() override;

protected:
  // reimplementation of TPanel::widgetFocusOnEnter
  void widgetFocusOnEnter() override;
  void widgetClearFocusOnLeave() override;
};

//=========================================================
// SceneViewerPanel
//---------------------------------------------------------

class SceneViewerPanelContainer final : public StyleShortcutSwitchablePanel {
  Q_OBJECT
  SceneViewerPanel *m_sceneViewer;

public:
  SceneViewerPanelContainer(QWidget *parent);
  // reimplementation of TPanel::widgetInThisPanelIsFocused
  bool widgetInThisPanelIsFocused() override;

protected:
  // reimplementation of TPanel::widgetFocusOnEnter
  void widgetFocusOnEnter() override;
  void widgetClearFocusOnLeave() override;
};

//=========================================================
// FxSettingsPanel
//---------------------------------------------------------

class FxSettingsPanel final : public TPanel {
  Q_OBJECT

  FxSettings *m_fxSettings;

public:
  FxSettingsPanel(QWidget *parent);
  // FxSettings will adjust its size according to the current fx
  // so we only restore position of the panel.
  void restoreFloatingPanelState() override;
};

//=========================================================
// VectorGuidedDrawingPanel
//---------------------------------------------------------

class VectorGuidedDrawingPanel final : public TPanel {
  Q_OBJECT

public:
  VectorGuidedDrawingPanel(QWidget *parent);
};

#endif
