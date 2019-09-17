#pragma once

#ifndef PALETTEVIEWER_H
#define PALETTEVIEWER_H

#include "paletteviewergui.h"
#include "toonz/tpalettehandle.h"

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

class DVAPI PaletteViewer final : public QFrame {
  Q_OBJECT

public:
  PaletteViewer(QWidget *parent = 0, PaletteViewType viewType = LEVEL_PALETTE,
                bool hasSaveToolBar = true, bool hasPageCommand = true,
                bool hasPasteColors = true);
  ~PaletteViewer();

  const TPaletteHandle *getPaletteHandle() const { return m_paletteHandle; }
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

protected:
  TPaletteHandle *m_paletteHandle;
  TFrameHandle *m_frameHandle;
  TXsheetHandle *m_xsheetHandle;

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

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

  void clearStyleSelection();

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
};

#endif  // PALETTEVIEWER_H
