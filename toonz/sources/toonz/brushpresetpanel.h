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
#include <QString>
#include <set>

// Forward declarations
class TTool;
class ToolHandle;
class TApplication;
class PresetNamePopup;

//=============================================================================
// BrushPresetItem - A widget representing an individual preset
//=============================================================================

class BrushPresetItem : public QToolButton {
  Q_OBJECT
  
  QString m_presetName;
  QString m_toolType;  // "vector", "toonzraster", "mypaint", "mypainttnz", or "raster"
  QPixmap m_iconPixmap;
  QPixmap m_scaledIconCache;  // Scaled icon (fixed)
  bool m_hasIcon;
  bool m_isCustomIcon;  // true = custom icon, false = generated icon
  bool m_isListMode;  // true = list mode (text on right), false = grid mode (text below)
  bool m_isSmallMode; // true = GridSmall mode (reduce generated icon sizes by 3px)
  bool m_showBorders;  // true = with borders, false = without borders (clean)
  bool m_showBackgrounds; // true = show backgrounds, false = transparent backgrounds
  
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
  QString m_currentToolType;  // "vector", "toonzraster", "raster", or ""
  QString m_currentPreset;
  PresetNamePopup *m_presetNamePopup;
  ViewMode m_viewMode;
  int m_currentColumns;  // Current number of columns (dynamic based on width)
  bool m_showBorders;     // Show or hide cell borders
  bool m_showBackgrounds; // Show or hide cell backgrounds (for cleaner appearance)
  
  // Preset storage (to avoid multiple reads)
  QMap<QString, QList<QString>> m_presetCache;
  
public:
  BrushPresetPanel(QWidget *parent = nullptr);
  ~BrushPresetPanel();
  
  void reset() override;
  
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
};

#endif  // BRUSHPRESETPANEL_H

