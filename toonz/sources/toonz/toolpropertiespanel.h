#pragma once

#ifndef TOOLPROPERTIESPANEL_H
#define TOOLPROPERTIESPANEL_H

#include "pane.h"
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QToolButton>
#include <QMap>
#include <QString>

//=============================================================================
// Custom button that respects Cells Borders/Backgrounds while preserving theme colors
//=============================================================================

class ToolPropertyButton : public QToolButton {
  Q_OBJECT
  
  bool m_showBorders;
  bool m_showBackgrounds;
  
public:
  ToolPropertyButton(const QString &text, QWidget *parent = nullptr);
  
  void setShowBorders(bool show) { m_showBorders = show; update(); }
  void setShowBackgrounds(bool show) { m_showBackgrounds = show; update(); }
  
protected:
  void paintEvent(QPaintEvent *event) override;
};

// Forward declarations
class TTool;
class ToolHandle;
class TApplication;
class QSpinBox;
class QDoubleSpinBox;

//=============================================================================
// Tool Properties Panel - Tool properties panel
// Generic architecture, brushes implementation only for now
//=============================================================================

class ToolPropertiesPanel : public TPanel {
  Q_OBJECT

  // UI Components
  QScrollArea *m_scrollArea;
  QWidget *m_propertiesContainer;
  QVBoxLayout *m_propertiesLayout;
  QLabel *m_toolNameLabel;
  
  // Tool management
  ToolHandle *m_toolHandle;
  QString m_currentToolId;
  QString m_currentToolType;  // "brush", "fill", "eraser", etc.
  bool m_useSingleMaxSlider;   // false = DoublePairField (double curseur natif), true = single slider (max uniquement)
  bool m_showLabels;           // Show/hide property labels
  bool m_showNumericFields;    // Show/hide numeric fields
  bool m_showBorders;          // Show/hide option borders in collapsible menus
  bool m_showBackgrounds;      // Show/hide option backgrounds in collapsible menus
  bool m_showIcons;            // Show/hide icons in Cap/Join options
  
public:
  ToolPropertiesPanel(QWidget *parent = nullptr);
  ~ToolPropertiesPanel();
  
  void reset() override;
  
protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  
private:
  void initializeUI();
  void connectSignals();
  void disconnectSignals();
  
  // Tool detection
  QString detectCurrentToolId();
  QString detectCurrentToolType();
  TTool* getCurrentTool();
  
  // Properties display
  void refreshProperties();
  void clearProperties();
  void updatePropertyValues();
  void updateWidgetFromProperty(QWidget *widget);
  
  // Brush-specific properties (current implementation)
  void createBrushProperties();
  void createMyPaintBrushProperties();
  void createSizeProperty();
  void createAccuracyProperty();
  void createHardnessProperty();
  void createOpacityProperty();
  void createSmoothProperty();
  void createBreakAnglesProperty();
  void createDrawOrderProperty();
  void createFrameRangeProperty();
  void createSnapProperty();
  void createSnapSensitivityProperty();
  void createCapProperty();
  void createJoinProperty();
  void createMiterProperty();
  void createLockAlphaProperty();
  void createPencilModeProperty();
  void createModifierSizeProperty();
  void createModifierOpacityProperty();
  void createModifierEraserProperty();
  void createModifierLockAlphaProperty();
  void createAssistantsProperty();
  void createPressureProperty();
  
  // MyPaint-specific properties (special sliders)
  void createMyPaintSizeProperty();
  void createMyPaintOpacityProperty();
  
  // Helper methods
  void createDoublePairSlider(const QString &label, void *prop, 
                              const std::string &propName);
  void createIntPairSlider(const QString &label, void *prop, 
                           const std::string &propName);
  QWidget* createSliderWithLabel(const QString &label, int min, int max, int value, 
                                 const std::string &propName);
  QWidget* createDoubleSliderWithLabel(const QString &label, double min, double max, 
                                       double value, const std::string &propName);
  QWidget* createCheckBox(const QString &label, bool checked, const std::string &propName);
  QWidget* createCollapsibleEnum(const QString &label, const QStringList &items, 
                                 int currentIndex, const std::string &propName,
                                 const QString &iconName = QString());
  QWidget* createCollapsibleEnumWithIcons(const QString &label, const QStringList &items,
                                          int currentIndex, const std::string &propName,
                                          const QStringList &iconNames);
  void addShowHideContextMenu(QMenu *menu);
  
  // Container stylesheet management for Cells Borders/Backgrounds
  void updateContainerStylesheet();
  
  // Theme-aware style helpers (no hardcoded colors)
  QString getButtonStyleChecked() const;
  QString getButtonStyleNormal(bool showBorders, bool showBackgrounds) const;
  
private slots:
  void onToolSwitched();
  void onToolChanged();
  
  // Brush property slots
  void onSizeChanged(int value);
  void onHardnessChanged(int value);
  void onOpacityChanged(int value);
  void onLockAlphaChanged(bool checked);
  void onPencilModeChanged(bool checked);
  void onDrawOrderChanged(int index);
  void onCapChanged(int index);
  void onJoinChanged(int index);
  void onSmoothChanged(int value);
  void onAssistantsChanged(bool checked);
  void onPressureChanged(bool checked);
  void onShowHideActionTriggered();
};

#endif // TOOLPROPERTIESPANEL_H

