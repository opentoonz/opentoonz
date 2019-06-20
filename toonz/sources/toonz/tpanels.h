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
class TLevel;
class StudioPaletteViewer;
class TPanelTitleBarButton;
class SchematicViewer;
class FunctionViewer;
class FlipBook;
class ToolOptions;
class ComboViewerPanel;
//=========================================================
// PaletteViewerPanel
//---------------------------------------------------------

class PaletteViewerPanel final : public StyleShortcutSwitchablePanel {
  Q_OBJECT

  TPaletteHandle *m_paletteHandle;
  PaletteViewer *m_paletteViewer;

  TPanelTitleBarButton *m_isCurrentButton;
  bool m_isCurrent;

public:
  PaletteViewerPanel(QWidget *parent);

  void setViewType(int viewType) override;
  int getViewType() override;

  void reset() override;

protected:
  void initializeTitleBar();
  bool isActivatableOnEnter() override { return true; }

protected slots:
  void onColorStyleSwitched();
  void onPaletteSwitched();
  void onCurrentButtonToggled(bool isCurrent);
  void onSceneSwitched();
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
  void onColorStyleChanged();
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

class FlipbookPanel final : public TPanel {
  Q_OBJECT
  FlipBook *m_flipbook;

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

#endif
