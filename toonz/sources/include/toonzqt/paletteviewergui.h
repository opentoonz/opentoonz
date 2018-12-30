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
{ LEVEL_PALETTE,      //!< Content palette is from a level.
  CLEANUP_PALETTE,    //!< Content palette is from cleanup settings.
  STUDIO_PALETTE      //!< Content palette is from a Studio Palette panel.
};

//****************************************************************************
//    PageViewer  declaration
//****************************************************************************

class DVAPI PageViewer final : public QFrame, public TSelection::View {
  Q_OBJECT

  QColor m_textColor;  // text color used for list view
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)

public:
  enum ViewMode         //! Possible view modes for a Palette Viewer.
  { SmallChips,         //!< Small icons.
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
#if QT_VERSION >= 0x050500
  PaletteIconWidget(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  PaletteIconWidget(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
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
