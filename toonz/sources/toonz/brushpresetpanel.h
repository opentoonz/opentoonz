#pragma once

#ifndef BRUSHPRESETPANEL_H
#define BRUSHPRESETPANEL_H

#include "pane.h"
#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <set>

// Forward declarations
class TTool;
class ToolHandle;
class TApplication;
class PresetNamePopup;
class BrushPresetPanel;
class TabBarContainter;

namespace DVGui {
class LineEdit;
}

//=============================================================================
// BrushPresetItem - A widget representing an individual preset
//=============================================================================

class BrushPresetItem : public QToolButton {
  Q_OBJECT
  
  QString m_presetName;
  QString m_toolType;  // "vector", "toonzraster", or "raster"
  QPixmap m_iconPixmap;
  QPixmap m_scaledIconCache;  // Scaled icon (fixed)
  bool m_hasIcon;
  bool m_isCustomIcon;  // true = custom icon, false = generated icon
  bool m_isListMode;  // true = list mode (text on right), false = grid mode (text below)
  bool m_isSmallMode; // true = GridSmall mode (reduce generated icon sizes by 3px)
  bool m_showBorders;  // true = with borders, false = without borders (clean)
  bool m_showBackgrounds; // true = show backgrounds, false = transparent backgrounds
  bool m_checkboxVisible; // true = show checkbox for multi-select
  bool m_isMultiSelected; // true = checked in multi-select mode
  
  // Drag & Drop state
  QPoint m_dragStartPosition;
  
public:
  BrushPresetItem(const QString &presetName, const QString &toolType, bool isListMode, QWidget *parent = nullptr);
  
  QString getPresetName() const { return m_presetName; }
  QString getToolType() const { return m_toolType; }
  
  void setIconFromPath(const QString &iconPath);
  void setDefaultIcon(const QString &toolType);
  void setListMode(bool isListMode);
  void setSmallMode(bool isSmallMode);
  void setShowBorders(bool showBorders) { m_showBorders = showBorders; update(); }
  void setShowBackgrounds(bool showBackgrounds) { m_showBackgrounds = showBackgrounds; update(); }
  void setCheckboxVisible(bool visible) { m_checkboxVisible = visible; update(); }
  bool isCheckboxVisible() const { return m_checkboxVisible; }
  void setMultiSelected(bool selected) { m_isMultiSelected = selected; update(); }
  bool isMultiSelected() const { return m_isMultiSelected; }
  
  // Update scaled icon (cache) according to current size
  void updateScaledIcon();
  
protected:
  void paintEvent(QPaintEvent *event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  
  // Drag & Drop support
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  
private:
  QString findCustomPresetIcon(const QString &presetName);
  
signals:
  void presetSelected(const QString &presetName, const QString &toolType);
  void presetReordered(const QString &fromPreset, const QString &toPreset);
};

//=============================================================================
// BrushPresetPage - Page structure for organizing presets (like TPalette::Page)
//=============================================================================

/*! \brief A preset page contains a list of preset names and remembers the last selected preset
 *
 *  Modeled after TPalette::Page structure for consistency.
 */
class BrushPresetPage {
private:
  QString m_name;                  // Page name displayed in tab
  int m_index;                     // Page index
  QStringList m_presetNames;       // List of preset names in this page
  QString m_lastSelectedPreset;    // Last selected preset on this page
  
public:
  BrushPresetPage(const QString &name, int index = 0)
    : m_name(name), m_index(index) {}
  
  QString getName() const { return m_name; }
  void setName(const QString &name) { m_name = name; }
  
  int getIndex() const { return m_index; }
  void setIndex(int index) { m_index = index; }
  
  const QStringList& getPresetNames() const { return m_presetNames; }
  void setPresetNames(const QStringList &names) { m_presetNames = names; }
  void addPreset(const QString &presetName) { m_presetNames.append(presetName); }
  void removePreset(const QString &presetName) { m_presetNames.removeAll(presetName); }
  bool hasPreset(const QString &presetName) const { return m_presetNames.contains(presetName); }
  int getPresetCount() const { return m_presetNames.size(); }
  
  QString getLastSelectedPreset() const { return m_lastSelectedPreset; }
  void setLastSelectedPreset(const QString &preset) { m_lastSelectedPreset = preset; }
};

//=============================================================================
// BrushPresetTabBar - Tab bar for organizing presets into pages
//=============================================================================

/*! \brief Custom tab bar with rename and drag & drop support (based on PaletteTabBar)
 *
 *  This tab bar allows:
 *  - Double-click to rename a tab
 *  - Ctrl+Drag to reorder tabs
 *  - Right-click context menu for page operations
 *  Features are modeled after the Level Palette Editor's tab bar.
 */
class BrushPresetTabBar : public QTabBar {
  Q_OBJECT

public:
  BrushPresetTabBar(QWidget *parent = nullptr);

  void setBrushPresetPanel(BrushPresetPanel *panel) { m_panel = panel; }

public slots:
  void updateTabName();

signals:
  void tabTextChanged(int index);
  void movePage(int srcIndex, int dstIndex);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  DVGui::LineEdit *m_renameTextField;
  int m_renameTabIndex;
  BrushPresetPanel *m_panel;
};

//=============================================================================
// BrushPresetPanel - Dockable panel for brush preset management
//=============================================================================

class BrushPresetPanel final : public TPanel {
  Q_OBJECT
  
public:
  enum ViewMode {
    ListView,      // Vertical list view
    GridSmall,     // Small grid frames
    GridMedium,    // Medium grid frames
    GridLarge      // Large grid frames (default)
  };
  
private:
  // UI Components
  TabBarContainter *m_tabBarContainer;
  BrushPresetTabBar *m_tabBar;
  QScrollArea *m_scrollArea;
  QWidget *m_presetContainer;
  QGridLayout *m_presetLayout;
  QPushButton *m_addPresetButton;
  QPushButton *m_removePresetButton;
  QPushButton *m_refreshButton;
  QLabel *m_toolLabel;
  QPushButton *m_viewModeButton;
  QMenu *m_viewModeMenu;
  QButtonGroup *m_presetButtonGroup;
  
  // Application handles
  ToolHandle *m_toolHandle;
  
  // State
  QString m_currentToolType;  // Level type only: "vector", "toonzraster", "raster", or ""
  QString m_currentPreset;
  PresetNamePopup *m_presetNamePopup;
  ViewMode m_viewMode;
  int m_currentColumns;  // Current number of columns (dynamic based on width)
  bool m_showBorders;     // Show or hide cell borders
  bool m_showBackgrounds; // Show or hide cell backgrounds (for cleaner appearance)
  int m_currentPageIndex; // Current selected page/tab index
  
  // Page/Tab management (modeled after TPalette::Page structure)
  // Key format: "toolType" -> List of BrushPresetPage pointers
  QMap<QString, QList<BrushPresetPage*>> m_pages;
  
  // Multi-selection state
  bool m_checkboxMode;          // true = show checkboxes on each preset
  QSet<QString> m_selectedPresets; // Set of selected preset names (multi-select)
  
  // Clipboard for copy/cut/paste between pages
  QStringList m_clipboardPresets;  // Preset names in clipboard
  bool m_clipboardIsCut;           // true = cut (remove from source), false = copy
  
public:
  BrushPresetPanel(QWidget *parent = nullptr);
  ~BrushPresetPanel();
  
  void reset() override;
  
  // Page/Tab management (public for TabBar access)
  void addNewPage();
  void deletePage(int pageIndex);
  
  // Context menu for presets (public for BrushPresetItem access)
  void showPresetContextMenu(const QPoint &globalPos, const QString &presetName);
  
protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void enterEvent(QEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  
private:
  void initializeUI();
  void connectSignals();
  void disconnectSignals();
  void createViewModeMenu();
  
  // Preset loading and management
  QList<QString> loadPresetsForTool(const QString &toolType);
  void refreshPresetList();
  void clearPresetList();
  
  // Tool detection
  QString detectCurrentToolType();
  TTool* getCurrentBrushTool();
  
  // Preset actions
  void applyPreset(const QString &presetName);
  void addNewPreset();
  void removeCurrentPreset();
  
  // Drag & Drop reordering
  void reorderPreset(const QString &fromPreset, const QString &toPreset);
  
  // View mode management
  void setViewMode(ViewMode mode);
  int getColumnsForViewMode() const;
  QSize getItemSizeForViewMode() const;
  void updatePresetItemsSizes();
  void reorganizeLayout(int newColumns);
  
  // Border and background management
  void updateItemBorders();
  void updateItemBackgrounds();
  
  // Checked state synchronization
  void updateCheckedStates();
  
  // Multi-selection
  void togglePresetSelection(const QString &presetName);
  void clearMultiSelection();
  void updateCheckboxVisibility();
  
  // Context menu actions (showPresetContextMenu is public, above)
  void copySelectedPresets();
  void cutSelectedPresets();
  void pastePresetsToCurrentPage();
  void deleteSelectedPresets();
  
  // Page/Tab management (internal methods)
  void initializeTabs();
  void updateTabBar();
  void switchToPage(int pageIndex);
  void renamePage(int pageIndex, const QString &newName);
  void movePageTab(int srcIndex, int dstIndex);
  void savePageConfiguration();
  void loadPageConfiguration();
  
  // Page access methods (like TPalette)
  BrushPresetPage* getCurrentPage();
  BrushPresetPage* getPage(int pageIndex);
  int getPageCount() const;
  void clearPages();
  
protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void addShowHideContextMenu(QMenu *menu);
  
private slots:
  void onToolSwitched();
  void onToolChanged();
  void onPresetItemClicked(const QString &presetName, const QString &toolType);
  void onAddPresetClicked();
  void onRemovePresetClicked();
  void onRefreshClicked();
  void onToolComboBoxListChanged(std::string id);
  void onViewModeChanged(ViewMode mode);
  void onShowHideActionTriggered();
  
  // Tab/Page management slots
  void onTabChanged(int index);
  void onTabTextChanged(int index);
  void onTabMoved(int srcIndex, int dstIndex);
  void onAddPageClicked();
  void onDeletePageClicked();
};

#endif  // BRUSHPRESETPANEL_H

