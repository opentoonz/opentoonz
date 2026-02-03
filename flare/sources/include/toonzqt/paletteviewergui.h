#pragma once

#ifndef PALETTEVIEWERGUI_H
#define PALETTEVIEWERGUI_H

// TnzQt includes
#include "toonzqt/selection.h"
#include "toonzqt/lineedit.h"

// TnzCore includes
#include "tpalette.h"

// Qt includes
#include <QFrame>
#include <QTabBar>
#include <QShortcut>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//==============================================================

//    Forward declarations

class TXsheetHandle;
class TFrameHandle;
class TPaletteHandle;
class TXshLevelHandle;
class TStyleSelection;
class TabBarContainter;
class ChangeStyleCommand;
class QMimeData;
class StyleNameEditor;
//==============================================================

//****************************************************************************
//    PaletteViewerGUI  namespace
//****************************************************************************

/*!
  \brief    Contains classes pertaining the GUI of a Toonz Palette Viewer.
  */

namespace PaletteViewerGUI {

enum PaletteViewType  //! Possible palette contents of a Palette Viewer.
{
  LEVEL_PALETTE,    //!< Content palette is from a level.
  CLEANUP_PALETTE,  //!< Content palette is from cleanup settings.
  STUDIO_PALETTE    //!< Content palette is from a Studio Palette panel.
};

//****************************************************************************
//    PageViewer  declaration
//****************************************************************************

class DVAPI PageViewer final : public QFrame, public TSelection::View {
  Q_OBJECT

  QColor m_textColor;  // text color used for list view
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)

  // list view chip separator color
  QColor m_separatorColor;
  Q_PROPERTY(
      QColor SeparatorColor READ getSeparatorColor WRITE setSeparatorColor)

  // selected chip border color
  QColor m_selectedBorderColor;
  Q_PROPERTY(QColor SelectedBorderColor READ getSelectedBorderColor WRITE
                 setSelectedBorderColor)

  // numpad shortcut key style
  QColor m_numpadShortcutBgColor;  // bg
  Q_PROPERTY(QColor NumpadShortcutBgColor READ getNumpadShortcutBgColor WRITE
                 setNumpadShortcutBgColor)

  QColor m_numpadShortcutBorderColor;  // border
  Q_PROPERTY(QColor NumpadShortcutBorderColor READ getNumpadShortcutBorderColor
                 WRITE setNumpadShortcutBorderColor)

  // list view current cell color
  QColor m_currentCellColor;
  Q_PROPERTY(QColor CurrentCellColor READ getCurrentCellColor WRITE
                 setCurrentCellColor)

  // list view selected cell color
  QColor m_selectedCellColor;
  Q_PROPERTY(QColor SelectedCellColor READ getSelectedCellColor WRITE
                 setSelectedCellColor)

  // list view numpad shortcut border color
  QColor m_listNumpadShortcutBorderColor;
  Q_PROPERTY(
      QColor ListNumpadShortcutBorderColor READ getListNumpadShortcutBorderColor
          WRITE setListNumpadShortcutBorderColor)

public:
  enum ViewMode  //! Possible view modes for a Palette Viewer.
  {
    SmallChips,         //!< Small icons.
    MediumChips,        //!< Medium icons.
    LargeChips,         //!< Large icons with style names.
    List,               //!< Top-down list of all icons.
    SmallChipsWithName  //!< Small icons with overlayed style names (if
                        //! user-defined).
  };

  // for displaying the linked style name from studio palette
  enum NameDisplayMode { Style, Original, StyleAndOriginal };

public:
  PageViewer(QWidget *parent = 0, PaletteViewType viewType = LEVEL_PALETTE,
             bool hasPasteColors = true);
  ~PageViewer();

  void setPaletteHandle(TPaletteHandle *paletteHandle);
  TPaletteHandle *getPaletteHandle() const;

  void setXsheetHandle(TXsheetHandle *xsheetHandle);
  TXsheetHandle *getXsheetHandle() const;

  void setFrameHandle(TFrameHandle *xsheetHandle);
  TFrameHandle *getFrameHandle() const;

  // for clearing the cache when execute paste style command on styleSelection
  void setLevelHandle(TXshLevelHandle *levelHandle);

  void setCurrentStyleIndex(int index);
  int getCurrentStyleIndex() const;

  void setPage(TPalette::Page *page);
  TPalette::Page *getPage() const { return m_page; }

  void setChangeStyleCommand(ChangeStyleCommand *changeStyleCommand) {
    m_changeStyleCommand = changeStyleCommand;
  };
  ChangeStyleCommand *getChangeStyleCommand() const {
    return m_changeStyleCommand;
  }

  int getChipCount() const;

  ViewMode getViewMode() const { return m_viewMode; }
  void setViewMode(ViewMode mode);
  NameDisplayMode getNameDisplayMode() const { return m_nameDisplayMode; }
  void setNameDisplayMode(NameDisplayMode mode);

  PaletteViewerGUI::PaletteViewType getViewType() const { return m_viewType; }

  int posToIndex(const QPoint &pos) const;

  QRect getItemRect(int index) const;
  QRect getColorChipRect(int index) const;
  QRect getColorNameRect(int index) const;

  void drop(int indexInPage, const QMimeData *mimeData);
  void createDropPage();
  void onSelectionChanged() override { update(); }

  TStyleSelection *getSelection() const { return m_styleSelection; }
  void clearSelection();

  // update the "lock"s for commands when the StyleSelection becomes current and
  // when the current palettte changed
  void updateCommandLocks();

  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }

  // list view chip separator color
  void setSeparatorColor(const QColor &color) { m_separatorColor = color; }
  QColor getSeparatorColor() const { return m_separatorColor; }

  void setSelectedBorderColor(const QColor &color) {
    m_selectedBorderColor = color;
  }
  QColor getSelectedBorderColor() const { return m_selectedBorderColor; }

  // numpad shortcut key style
  void setNumpadShortcutBgColor(const QColor &color) {
    m_numpadShortcutBgColor = color;
  }
  QColor getNumpadShortcutBgColor() const { return m_numpadShortcutBgColor; }

  void setNumpadShortcutBorderColor(const QColor &color) {
    m_numpadShortcutBorderColor = color;
  }
  QColor getNumpadShortcutBorderColor() const {
    return m_numpadShortcutBorderColor;
  }

  // list view current cell color
  void setCurrentCellColor(const QColor &color) { m_currentCellColor = color; }
  QColor getCurrentCellColor() const { return m_currentCellColor; }

  // list view selected cell color
  void setSelectedCellColor(const QColor &color) {
    m_selectedCellColor = color;
  }
  QColor getSelectedCellColor() const { return m_selectedCellColor; }

  // list view numpad shortcut border color
  void setListNumpadShortcutBorderColor(const QColor &color) {
    m_listNumpadShortcutBorderColor = color;
  }
  QColor getListNumpadShortcutBorderColor() const {
    return m_listNumpadShortcutBorderColor;
  }

public slots:

  void computeSize();
  void onFrameChanged();
  void onStyleRenamed();

  void addNewColor();
  void addNewPage();

protected:
  QSize getChipSize() const;
  void drawColorChip(QPainter &p, QRect &chipRect, TColorStyle *style);
  void drawColorName(QPainter &p, QRect &nameRect, TColorStyle *style,
                     int styleIndex);
  void drawToggleLink(QPainter &p, QRect &chipRect, TColorStyle *style);

  // event handlers
  void paintEvent(QPaintEvent *) override;

  void resizeEvent(QResizeEvent *) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

  void keyPressEvent(QKeyEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void startDragDrop();
  void createMenuAction(QMenu &menu, const char *id, QString name,
                        const char *slot);
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  bool event(QEvent *e) override;

  void select(int indexInPage, QMouseEvent *event);

  void zoomInChip();
  void zoomOutChip();

  bool hasShortcut(int indexInPage);

private:
  DVGui::LineEdit *m_renameTextField;
  QPoint m_dragStartPosition;

  TPalette::Page *m_page;
  QPoint m_chipsOrigin;
  int m_chipPerRow;
  ViewMode m_viewMode;
  NameDisplayMode m_nameDisplayMode;
  int m_dropPositionIndex;
  bool m_dropPageCreated;
  bool m_startDrag;

  TStyleSelection *m_styleSelection;
  TFrameHandle *m_frameHandle;
  bool m_hasPasteColors;
  PaletteViewType m_viewType;

  ChangeStyleCommand *m_changeStyleCommand;

  QShortcut *m_zoomInShortCut;
  QShortcut *m_zoomOutShortCut;
  StyleNameEditor *m_styleNameEditor;

signals:
  void changeWindowTitleSignal();
  void switchToPage(int);
};

//****************************************************************************
//    PaletteTabBar  declaration
//****************************************************************************

class DVAPI PaletteTabBar final : public QTabBar {
  Q_OBJECT

public:
  PaletteTabBar(QWidget *parent, bool hasPageCommand);

  void setPageViewer(PageViewer *pageViewer) { m_pageViewer = pageViewer; }

public slots:

  void updateTabName();

signals:

  void tabTextChanged(int index);
  void movePage(int srcIndex, int dstIndex);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  DVGui::LineEdit *m_renameTextField;
  int m_renameTabIndex;
  PageViewer *m_pageViewer;

  bool m_hasPageCommand;
};

//****************************************************************************
//    PaletteIconWidget  declaration
//****************************************************************************

/*!
        \brief    Special placeholder toolbar icon for \a starting a palette
   move
        through drag & drop.

        \details  This widget is currently employed as a mere mouse event filter
        to propagate drag & drop starts to a PaletteViewer ancestor
        in the widgets hierarchy.
        */

class DVAPI PaletteIconWidget final : public QWidget {
  Q_OBJECT

public:
  PaletteIconWidget(QWidget *parent       = 0,
                    Qt::WindowFlags flags = Qt::WindowFlags());
  ~PaletteIconWidget();

signals:

  void startDrag();  //!< Emitted \a once whenever the icon is sensibly dragged
                     //!  by the user.
protected:
  void paintEvent(QPaintEvent *) override;

  void enterEvent(QEvent *event) override;
  void leaveEvent(QEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  QPoint m_mousePressPos;  //!< Mouse position at mouse press.
  bool m_isOver,           //!< Whether mouse is hovering on this widget.
      m_dragged;  //!< Whether user has started a drag operation on the icon.
};

}  // namespace PaletteViewerGUI

#endif  // PALETTEVIEWERGUI_H
