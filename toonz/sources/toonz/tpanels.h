#pragma once

#ifndef TPANELS_INCLUDED
#define TPANELS_INCLUDED

#include "pane.h"

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
//=========================================================
// PaletteViewerPanel
//---------------------------------------------------------

class PaletteViewerPanel : public TPanel {
  Q_OBJECT

  TPaletteHandle *m_paletteHandle;
  PaletteViewer *m_paletteViewer;

  TPanelTitleBarButton *m_isCurrentButton;
  bool m_isCurrent;

public:
  PaletteViewerPanel(QWidget *parent);

  void setViewType(int viewType);
  int getViewType();

  void reset();

protected:
  void initializeTitleBar();
  bool isActivatableOnEnter() { return true; }

protected slots:
  void onColorStyleSwitched();
  void onPaletteSwitched();
  void onCurrentButtonToggled(bool isCurrent);
  void onSceneSwitched();
};

//=========================================================
// StudioPaletteViewerPanel
//---------------------------------------------------------

class StudioPaletteViewerPanel : public TPanel {
  Q_OBJECT

  TPaletteHandle *m_studioPaletteHandle;
  StudioPaletteViewer *m_studioPaletteViewer;

public:
  StudioPaletteViewerPanel(QWidget *parent);

protected:
  bool isActivatableOnEnter() { return true; }
protected slots:
  void onColorStyleSwitched();
  void onPaletteSwitched();
};

//=========================================================
// StyleEditorPanel
//---------------------------------------------------------

class StyleEditorPanel : public TPanel {
  Q_OBJECT
  StyleEditor *m_styleEditor;

public:
  StyleEditorPanel(QWidget *parent);
};

//=========================================================
// ColorFieldEditorController
//---------------------------------------------------------

class ColorFieldEditorController
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
  void edit(DVGui::ColorField *colorField);
  void hide();

protected slots:
  void onColorStyleChanged();
  void onColorChanged(const TPixel32 &color, bool);
};

//=========================================================
// CleanupColorFieldEditorController
//---------------------------------------------------------

class CleanupColorFieldEditorController
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
  void edit(DVGui::CleanupColorField *colorField);
  void hide();

protected slots:
  void onColorStyleChanged();
};

//=========================================================
// SchematicScenePanel
//---------------------------------------------------------

class SchematicScenePanel : public TPanel {
  Q_OBJECT

  SchematicViewer *m_schematicViewer;

public:
  SchematicScenePanel(QWidget *parent);

  void setViewType(int viewType);
  int getViewType();

protected:
  void showEvent(QShowEvent *e);
  void hideEvent(QHideEvent *e);

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

class FunctionViewerPanel : public TPanel {
  Q_OBJECT

  FunctionViewer *m_functionViewer;

public:
  FunctionViewerPanel(QWidget *parent = 0);

  void reset();

  void attachHandles();
  void detachHandles();

  bool widgetInThisPanelIsFocused();

protected:
  void widgetFocusOnEnter();
  void widgetClearFocusOnLeave();

public slots:

  void onIoCurve(int type, TDoubleParam *curve, const std::string &name);
  void onEditObject();
};

//=========================================================
// ToolOptionPanel
//---------------------------------------------------------

class ToolOptionPanel : public TPanel {
  Q_OBJECT

  ToolOptions *m_toolOption;

public:
  ToolOptionPanel(QWidget *parent);
};

//=========================================================
// FlipbookPanel
//---------------------------------------------------------

class FlipbookPanel : public TPanel {
  Q_OBJECT
  FlipBook *m_flipbook;

  QSize m_floatingSize;
  TPanelTitleBarButton *m_button;

protected:
  void initializeTitleBar(TPanelTitleBar *titleBar);

public:
  FlipbookPanel(QWidget *parent);

  void reset();
  // disable minimize button when docked
  void onDock(bool docked);

protected slots:
  void onMinimizeButtonToggled(bool);
};

#endif
