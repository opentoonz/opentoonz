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
#include <QPushButton>
#include <QList>
#include <QMap>
#include <QString>
#include <functional>

//=============================================================================
// Custom button that respects Cells Borders/Backgrounds while preserving theme colors
//=============================================================================

class ToolPropertyButton : public QToolButton {
  Q_OBJECT
  
  bool m_showBorders;
  bool m_showBackgrounds;
  int m_compactIconSize = 0;  // >0: highlight/hover only around icon (px)
  
public:
  ToolPropertyButton(const QString &text, QWidget *parent = nullptr);
  
  void setShowBorders(bool show) { m_showBorders = show; update(); }
  void setShowBackgrounds(bool show) { m_showBackgrounds = show; update(); }
  // Compact highlight: keeps hit area, draws theme state on a smaller centered rect.
  void setCompactIconHighlight(int iconSize) {
    m_compactIconSize = iconSize;
    update();
  }
  
protected:
  void paintEvent(QPaintEvent *event) override;
};

// Forward declarations
class TTool;
class ToolHandle;
class TApplication;
class TStringProperty;
class TEnumProperty;
class TXsheetHandle;
class TObjectHandle;
class QSpinBox;
class QDoubleSpinBox;

// tnztools – Selection tool specialized fields (forward declarations only)
class SelectionScaleField;
class SelectionRotationField;
class SelectionMoveField;
class ThickChangeField;
class MeasuredValueField;
class ClickableLabel;
class SelectionTool;
class PlasticTool;
class RulerToolOptionsBox;
class ToolOptionTextField;
namespace DVGui { class CheckBox; }

//=============================================================================
// Tool Properties Panel - Tool properties panel
// Generic architecture, brushes implementation only for now
//=============================================================================

class ToolPropertiesPanel : public TPanel {
  Q_OBJECT

  class PropertyWidgetSync;
  friend class PropertyWidgetSync;

  // UI Components
  QScrollArea *m_scrollArea;
  QWidget *m_propertiesContainer;
  QVBoxLayout *m_propertiesLayout;
  QLabel *m_toolNameLabel;
  
  // Tool management
  ToolHandle *m_toolHandle;
  QString m_currentToolId;
  QString m_currentToolType;  // "brush", "fill", "eraser", etc.

  // Selection tool transform fields (valid only while selection tool is active)
  SelectionScaleField    *m_selScaleX    = nullptr;
  SelectionScaleField    *m_selScaleY    = nullptr;
  SelectionRotationField *m_selRotation  = nullptr;
  SelectionMoveField     *m_selMoveX     = nullptr;
  SelectionMoveField     *m_selMoveY     = nullptr;
  ThickChangeField       *m_selThick     = nullptr;
  DVGui::CheckBox        *m_selScaleLink = nullptr;
  QPushButton            *m_selFlipH     = nullptr;
  QPushButton            *m_selFlipV     = nullptr;
  QPushButton            *m_selRotL      = nullptr;
  QPushButton            *m_selRotR      = nullptr;
  ClickableLabel         *m_selHLabel    = nullptr;
  ClickableLabel         *m_selVLabel    = nullptr;
  ClickableLabel         *m_selXLabel    = nullptr;
  ClickableLabel         *m_selYLabel    = nullptr;
  QLabel                 *m_selScaleLinkIcon = nullptr;

  // Type tool — Style capsule rebuilt when font changes (toolComboBoxListChanged).
  QWidget *m_typeStyleWidget = nullptr;

  // Ruler tool: persistent read-only measurement strip (no TPropertyGroup).
  RulerToolOptionsBox *m_rulerOptionsBox           = nullptr;
  bool                 m_rulerOptionsBoxRegistered = false;

  // Plastic tool persistent widgets (synced with tool options bar).
  QWidget      *m_plasticModeContainer = nullptr;
  QVBoxLayout  *m_plasticModeLayout    = nullptr;
  QWidget      *m_plasticSkelPicker    = nullptr;
  QWidget      *m_plasticVertexWidget  = nullptr;
  int           m_plasticVisibleMode   = -1;

  // Animate (Edit) tool persistent widgets.
  QWidget *m_animateColumnWidget   = nullptr;
  QList<QWidget *> m_animateSplineRowWidgets;
  QList<QWidget *> m_animateXYRowWidgets;
  QList<MeasuredValueField *> m_animateMeasuredFields;

  bool m_useSingleMaxSlider;   // false = DoublePairField (native double cursor), true = single slider (max only)
  bool m_showLabels;           // Show/hide property labels
  bool m_showNumericFields;    // Show/hide numeric fields
  bool m_showBorders;          // Show/hide option borders in collapsible menus
  bool m_showBackgrounds;      // Show/hide option backgrounds in collapsible menus
  bool m_showIcons;            // Icon grid replaces collapsible enums when possible
  
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
  void updatePropertyWidgetsIn(QWidget *root);
  void attachAllPropertySyncListeners();
  
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

  // Generic property helpers (name-based lookup, no-op if property absent)
  void createEnumProperty(const QString &label, const std::string &propName,
                          int propGroup = 0,
                          const QString &iconName = QString());
  void createBoolProperty(const QString &label, const std::string &propName,
                          int propGroup = 0);
  void createDoublePairByName(const QString &label, const std::string &propName,
                              int propGroup = 0);
  void createIntSliderByName(const QString &label, const std::string &propName,
                             int propGroup = 0);
  void createDoubleSliderByName(const QString &label,
                                const std::string &propName,
                                int propGroup = 0);

  // Generic property builder — iterates all TProperty in a group dynamically.
  // Works for any tool without hardcoding property names.
  // Returns true if at least one widget was added.
  // When targetLayout is set, widgets are added there instead of the main panel.
  bool createGenericProperties(int propGroup = 0,
                               QVBoxLayout *targetLayout = nullptr,
                               bool plasticAlignedFields = false);

  // Ruler / Plastic need bespoke layouts (no or multi-mode property groups).
  void createRulerProperties();
  void createPlasticProperties();
  void rebuildPlasticModeSection();
  void syncPlasticModeFromTool();
  void createPlasticAnimateModeProperties(PlasticTool *plastic);
  void createPlasticRigidityModeProperties(PlasticTool *plastic);
  void updatePlasticRelayFields();
  void createAnimateProperties();
  void updateAnimateColumnPicker();
  QWidget *createEnumIconGridPanel(
      const QString &label, TEnumProperty *enumProp, const std::string &propName,
      int propGroup = 0, QWidget *parentWidget = nullptr,
      const std::function<void(int)> &onChanged = nullptr);

  // Eraser properties (3 variants: vector / toonz-raster / fullcolor-raster)
  void createEraserProperties();

  // Fill properties (3 variants: vector / toonz-raster / fullcolor-raster)
  void createFillProperties();

  // Geometric properties (vector / toonz-raster / fullcolor-raster)
  void createGeometricProperties();

  // Selection properties
  void createSelectionProperties();

  // Type (text) tool — Font/Size as dropdown menus (same as TOB)
  void createTypeProperties();
  void refreshTypeStyleWidget();

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
  QWidget* createTextProperty(const QString &label, TStringProperty *prop,
                              const std::string &propName, int propGroup = 0);
  QWidget* createCollapsibleEnum(const QString &label, const QStringList &items, 
                                 int currentIndex, const std::string &propName,
                                 const QString &iconName = QString(),
                                 const std::function<void(int)> &onChanged = nullptr,
                                 bool reserveHeaderRightSlot = false,
                                 QWidget *parentWidget = nullptr);
  QWidget* createCollapsiblePicker(
      const QString &label, const QStringList &items, int currentIndex,
      const QString &storageKey, const std::function<void(int)> &onChanged);
  QWidget* createAnimateColumnPicker(TXsheetHandle *xshHandle,
                                     TObjectHandle *objHandle);
  void syncCollapsiblePicker(QWidget *container, int index);
  QWidget *createCollapsibleTextField(
      const QString &label, const QString &textValue,
      const std::string &storageKey, const std::string &propName,
      int propGroup, const std::function<void(const QString &)> &onChanged,
      bool reserveHeaderRightSlot = false);
  QWidget *createCollapsibleSection(
      const QString &label, const std::string &storageKey, QWidget *contentWidget,
      const QString &headerValue = QString(),
      const QString &iconName = QString());
  QWidget *createCollapsibleIntSlider(
      const QString &label, int min, int max, int value,
      const std::string &propName, int propGroup,
      const std::string &storageKey, bool reserveHeaderRightSlot = false);
  QWidget *buildPlasticSkeletonPicker(PlasticTool *plastic);
  void updatePlasticVertexField();
  QWidget* createCollapsibleEnumWithIcons(const QString &label, const QStringList &items,
                                          int currentIndex, const std::string &propName,
                                          const QStringList &iconNames);
  QWidget *createCollapsibleEnumForProperty(
      const QString &label, TEnumProperty *enumProp, const std::string &propName,
      int propGroup = 0, const QString &headerIconName = QString(),
      const std::function<void(int)> &onChanged = nullptr,
      bool reserveHeaderRightSlot = false, QWidget *parentWidget = nullptr);
  void addShowHideContextMenu(QMenu *menu);
  
  // Container stylesheet management for Cells Borders/Backgrounds
  void updateContainerStylesheet();
  
  // Theme-aware style helpers (no hardcoded colors)
  QString getButtonStyleChecked() const;
  QString getButtonStyleNormal(bool showBorders, bool showBackgrounds) const;
  
private slots:
  void onToolSwitched();
  void onToolChanged();
  void onToolComboBoxListChanged(const std::string &id);
  
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
  void rebuildPlasticSkeletonPicker();
  void updatePlasticSkeletonPicker();
};

#endif // TOOLPROPERTIESPANEL_H

