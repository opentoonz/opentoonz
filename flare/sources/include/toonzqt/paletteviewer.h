#pragma once

#ifndef PALETTEVIEWER_H
#define PALETTEVIEWER_H

#include "saveloadqsettings.h"
#include "paletteviewergui.h"
#include "toonz/tpalettehandle.h"
#include "toonz/preferences.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QScrollArea;
class DvScrollWidget;
class QToolBar;
class PaletteKeyframeNavigator;
class TFrameHandle;

using namespace PaletteViewerGUI;

//-----------------------------------------------------------------------------

class DVAPI ChangeStyleCommand {
public:
  ChangeStyleCommand() {}
  virtual ~ChangeStyleCommand() {}
  virtual bool onStyleChanged() = 0;
};

// DAFARE: non mi piace, forse e' meglio un comando esterno!!
class TXsheetHandle;

//=============================================================================
// PaletteViewer
//-----------------------------------------------------------------------------

class DVAPI PaletteViewer final : public QFrame, public SaveLoadQSettings {
  Q_OBJECT

public:
  PaletteViewer(QWidget *parent = 0, PaletteViewType viewType = LEVEL_PALETTE,
                bool hasSaveToolBar = true, bool hasPageCommand = true,
                bool hasPasteColors = true);
  ~PaletteViewer();

  enum ToolbarButtons : int  //! Toolbar buttons to display
  {
    TBVisKeyframe,
    TBVisNewStylePage,
    TBVisPaletteGizmo,
    TBVisNameEditor,
    TBVisTotal
  };

  TPaletteHandle *getPaletteHandle() const { return m_paletteHandle; }
  void setPaletteHandle(TPaletteHandle *paletteHandle);

  const TFrameHandle *getFrameHandle() const { return m_frameHandle; }
  void setFrameHandle(TFrameHandle *frameHandle);

  const TXsheetHandle *getXsheetHandle() const { return m_xsheetHandle; }
  void setXsheetHandle(TXsheetHandle *xsheetHandle);

  // for clearing level cache after "paste style" command called from style
  // selection
  void setLevelHandle(TXshLevelHandle *levelHandle);

  TPalette *getPalette();

  void setChangeStyleCommand(ChangeStyleCommand *);  // gets ownership
  ChangeStyleCommand *getChangeStyleCommand() const {
    return m_changeStyleCommand;
  }

  int getViewMode() const { return m_pageViewer->getViewMode(); }
  void setViewMode(int mode) {
    m_pageViewer->setViewMode((PaletteViewerGUI::PageViewer::ViewMode)mode);
  }

  void updateView();

  void enableSaveAction(bool enable);

  // SaveLoadQSettings
  virtual void save(QSettings &settings) const override;
  virtual void load(QSettings &settings) override;

protected:
  TPaletteHandle *m_paletteHandle;
  TFrameHandle *m_frameHandle;
  TXsheetHandle *m_xsheetHandle;
  TXshLevelHandle *m_levelHandle;

  QScrollArea *m_pageViewerScrollArea;
  PaletteViewerGUI::PageViewer *m_pageViewer;
  TabBarContainter *m_tabBarContainer;
  PaletteTabBar *m_pagesBar;
  QToolBar *m_paletteToolBar;
  QToolBar *m_savePaletteToolBar;

  int m_indexPageToDelete;

  PaletteViewType m_viewType;

  PaletteKeyframeNavigator *m_keyFrameButton;

  ChangeStyleCommand *m_changeStyleCommand;

  bool m_hasSavePaletteToolbar;
  bool m_hasPageCommand;

  bool m_isSaveActionEnabled;

  QAction *m_lockPaletteAction;
  QToolButton *m_lockPaletteToolButton;

  bool m_toolbarOnTop;
  QAction *m_showToolbarOnTopAct;
  DvScrollWidget *m_toolbarContainer;
  QHBoxLayout *m_hLayout;

  StyleNameEditor *m_styleNameEditor;
  QAction *m_sharedGizmoAction;

  int m_toolbarVisibleOtherParts;
  QMultiMap<int, QAction *> m_toolbarParts;
  QAction *m_visibleKeysAction;
  QAction *m_visibleNewAction;
  QAction *m_visibleGizmoAction;
  QAction *m_visibleNameAction;
  QAction *m_variableWidthAction;

protected:
  void createTabBar();

  void createToolBar() {
    createPaletteToolBar();
    createSavePaletteToolBar();
  }
  void createPaletteToolBar();
  void createSavePaletteToolBar();

  void updateTabBar();

  void updateToolBar() {
    updatePaletteToolBar();
    updateSavePaletteToolBar();
  }
  void updatePaletteToolBar();
  void updateSavePaletteToolBar();

  void resizeEvent(QResizeEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

  void clearStyleSelection();

  void applyToolbarPartVisibility(int part, bool visible);

protected slots:

  void setPageView(int currentIndexPage);

  void addNewPage();
  void addNewColor();
  void deletePage();

  void saveStudioPalette();

  void onColorStyleSwitched();
  void onPaletteChanged();
  void onPaletteSwitched();
  void onFrameSwitched();
  void onTabTextChanged(int tabIndex);
  void onViewMode(QAction *);

  void changeWindowTitle();

  void movePage(int srcIndex, int dstIndex);

  void startDragDrop();

  void onNameDisplayMode(QAction *);
  void setIsLocked(bool lock);

  void onSwitchToPage(int pageIndex);
  void onShowNewStyleButtonToggled();

  void toggleToolbarOnTop();

  void toggleKeyframeVisibility(bool);
  void toggleNewStylePageVisibility(bool);
  void togglePaletteGizmoVisibility(bool);
  void toggleNameEditorVisibility(bool);

  void toggleVariableWidth(bool);
};

#endif  // PALETTEVIEWER_H
