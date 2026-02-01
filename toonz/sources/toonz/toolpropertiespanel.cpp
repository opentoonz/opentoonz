#include "toolpropertiespanel.h"

// ToonzQt includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "toonzqt/doublepairfield.h"
#include "toonzqt/intpairfield.h"

// ToonzLib includes
#include "toonz/preferences.h"
#include "toonz/mypaintbrushstyle.h"

// Tools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tproperty.h"

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QToolButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>
#include <QGroupBox>

// Standard library
#include <algorithm>  // For std::min
#include <QSettings>
#include <QMenu>
#include <QContextMenuEvent>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QStyle>

//=============================================================================
// ToolPropertyButton - Custom button with theme-aware painting
//=============================================================================

ToolPropertyButton::ToolPropertyButton(const QString &text, QWidget *parent)
    : QToolButton(parent)
    , m_showBorders(true)
    , m_showBackgrounds(true) {
  setText(text);
  setMouseTracking(true);  // To detect hover
}

void ToolPropertyButton::paintEvent(QPaintEvent *event) {
  QStyleOptionToolButton opt;
  initStyleOption(&opt);

  const bool isHovered = opt.state.testFlag(QStyle::State_MouseOver);
  const bool isChecked = opt.state.testFlag(QStyle::State_On);
  const bool isPressed = opt.state.testFlag(QStyle::State_Sunken);
  const bool useThemeState = isHovered || isChecked || isPressed;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  if (useThemeState) {
    // Hover/checked/pressed: let QSS draw full button to match theme colors
    style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter, this);
    return;
  }

  // Normal state: draw background based on Cells Backgrounds option
  if (m_showBackgrounds) {
    QColor panelBg = palette().color(QPalette::Window);
    if (QWidget *parent = parentWidget()) {
      panelBg = parent->palette().color(QPalette::Window);
    }

    QColor bgColor = panelBg;
    if (panelBg.lightness() > 128) {
      bgColor = panelBg.darker(105);
    } else {
      bgColor = panelBg.lighter(108);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRect(rect());
  }

  // Draw label (text/icon)
  style()->drawControl(QStyle::CE_ToolButtonLabel, &opt, &painter, this);

  // Draw thin border if enabled (Brush Preset style)
  if (m_showBorders) {
    QColor borderColor = palette().color(QPalette::Mid);
    QPen borderPen(borderColor);
    borderPen.setWidthF(0.5);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

//=============================================================================
// ToolPropertiesPanel implementation
//=============================================================================

ToolPropertiesPanel::ToolPropertiesPanel(QWidget *parent)
    : TPanel(parent)
    , m_scrollArea(nullptr)
    , m_propertiesContainer(nullptr)
    , m_propertiesLayout(nullptr)
    , m_toolNameLabel(nullptr)
    , m_toolHandle(nullptr)
    , m_currentToolId("")
    , m_currentToolType("")
    , m_useSingleMaxSlider(false)  // DoublePairField by default (native double cursor)
    , m_showLabels(true)           // Show labels by default
    , m_showNumericFields(true)    // Show numeric fields by default
    , m_showBorders(true)          // Show option borders by default
    , m_showBackgrounds(true)      // Show option backgrounds by default
    , m_showIcons(true) {          // Show Cap/Join icons by default
  
  // Load preferences from settings
  QSettings settings;
  m_useSingleMaxSlider = settings.value("ToolPropertiesPanel/useSingleMaxSlider", false).toBool();
  m_showLabels = settings.value("ToolPropertiesPanel/showLabels", true).toBool();
  m_showNumericFields = settings.value("ToolPropertiesPanel/showNumericFields", true).toBool();
  m_showBorders = settings.value("ToolPropertiesPanel/showBorders", true).toBool();
  m_showBackgrounds = settings.value("ToolPropertiesPanel/showBackgrounds", true).toBool();
  m_showIcons = settings.value("ToolPropertiesPanel/showIcons", true).toBool();

  initializeUI();
  connectSignals();
}

ToolPropertiesPanel::~ToolPropertiesPanel() {
  disconnectSignals();
}

void ToolPropertiesPanel::reset() {
  connectSignals();
  refreshProperties();
}

void ToolPropertiesPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  connectSignals();
  refreshProperties();
}

void ToolPropertiesPanel::hideEvent(QHideEvent *e) {
  TPanel::hideEvent(e);
  disconnectSignals();
}

//-----------------------------------------------------------------------------
// UI Initialization
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::initializeUI() {
  QWidget *mainWidget = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);
  
  // Tool name label (header) - Normal style like "Size" property
  m_toolNameLabel = new QLabel(tr("Tool Properties"), this);
  m_toolNameLabel->setAlignment(Qt::AlignCenter);
  mainLayout->addWidget(m_toolNameLabel);
  
  // Separator
  QFrame *separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  mainLayout->addWidget(separator);
  
  // Scroll area for properties
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  
  m_propertiesContainer = new QWidget();
  // CRITICAL: Set objectName to "toolOptionsPanel" to inherit theme styles from .qss
  // All themes define #toolOptionsPanel QPushButton:checked, :hover, etc.
  m_propertiesContainer->setObjectName("toolOptionsPanel");
  
  m_propertiesLayout = new QVBoxLayout(m_propertiesContainer);
  m_propertiesLayout->setMargin(8);
  m_propertiesLayout->setSpacing(20);  // Significant spacing between properties
  m_propertiesLayout->setAlignment(Qt::AlignTop);
  
  m_propertiesContainer->setLayout(m_propertiesLayout);
  m_scrollArea->setWidget(m_propertiesContainer);
  
  // Apply initial container stylesheet for Cells Borders/Backgrounds control
  updateContainerStylesheet();
  
  mainLayout->addWidget(m_scrollArea, 1);
  
  setWidget(mainWidget);
  setWindowTitle(tr("Tool Properties"));
  
  // Enable context menu
  setContextMenuPolicy(Qt::DefaultContextMenu);
}

void ToolPropertiesPanel::connectSignals() {
  if (!m_toolHandle) {
    TApplication *app = TApp::instance();
    m_toolHandle = app->getCurrentTool();
  }
  
  if (m_toolHandle) {
    connect(m_toolHandle, SIGNAL(toolSwitched()), this, SLOT(onToolSwitched()));
    connect(m_toolHandle, SIGNAL(toolChanged()), this, SLOT(onToolChanged()));
  }
}

void ToolPropertiesPanel::disconnectSignals() {
  if (m_toolHandle) {
    disconnect(m_toolHandle, SIGNAL(toolSwitched()), this, SLOT(onToolSwitched()));
    disconnect(m_toolHandle, SIGNAL(toolChanged()), this, SLOT(onToolChanged()));
  }
}

//-----------------------------------------------------------------------------
// Tool Detection
//-----------------------------------------------------------------------------

QString ToolPropertiesPanel::detectCurrentToolId() {
  if (!m_toolHandle) return "";
  return m_toolHandle->getRequestedToolName();
}

QString ToolPropertiesPanel::detectCurrentToolType() {
  QString toolId = detectCurrentToolId();
  
  // Identify tool types
  if (toolId == T_Brush) {
    // Check if it's a MyPaint brush (similar to BrushPresetPanel logic)
    TTool *tool = getCurrentTool();
    if (tool) {
      int targetType = tool->getTargetType();
      if (targetType & TTool::ToonzImage) {
        // Check if it's a MyPaint brush on Toonz Raster
        TApplication *app = TApp::instance();
        if (app) {
          TColorStyle *style = app->getCurrentLevelStyle();
          if (dynamic_cast<TMyPaintBrushStyle*>(style)) {
            return "mypainttnz";
          }
        }
      } else if (targetType & TTool::RasterImage) {
        // Check if it's a MyPaint brush on Raster
        TApplication *app = TApp::instance();
        if (app) {
          TColorStyle *style = app->getCurrentLevelStyle();
          if (dynamic_cast<TMyPaintBrushStyle*>(style)) {
            return "mypaint";
          }
        }
      }
    }
    return "brush";
  }
  if (toolId == T_Fill) return "fill";
  if (toolId == T_Eraser) return "eraser";
  if (toolId == T_Geometric) return "geometric";
  if (toolId == T_Selection) return "selection";
  if (toolId == T_Edit) return "edit";
  if (toolId == T_Ruler) return "ruler";
  if (toolId == T_Cutter) return "cutter";
  if (toolId == T_Tape) return "tape";
  if (toolId == T_StylePicker) return "stylepicker";
  if (toolId == T_RGBPicker) return "rgbpicker";
  if (toolId == T_ControlPointEditor) return "controlpoint";
  if (toolId == T_Pinch) return "pinch";
  if (toolId == T_Pump) return "pump";
  if (toolId == T_Magnet) return "magnet";
  if (toolId == T_Bender) return "bender";
  if (toolId == T_Iron) return "iron";
  if (toolId == T_Cutter) return "cutter";
  if (toolId == T_Skeleton) return "skeleton";
  if (toolId == T_Tracker) return "tracker";
  if (toolId == T_Hook) return "hook";
  if (toolId == T_Plastic) return "plastic";
  if (toolId == T_Zoom) return "zoom";
  if (toolId == T_Rotate) return "rotate";
  if (toolId == T_Hand) return "hand";
  
  return "unknown";
}

TTool* ToolPropertiesPanel::getCurrentTool() {
  if (!m_toolHandle) return nullptr;
  return m_toolHandle->getTool();
}

//-----------------------------------------------------------------------------
// Properties Display
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::refreshProperties() {
  QString toolId = detectCurrentToolId();
  QString toolType = detectCurrentToolType();
  
  // Clear existing properties
  clearProperties();
  
  m_currentToolId = toolId;
  m_currentToolType = toolType;
  
  // Update header
  TTool *tool = getCurrentTool();
  if (tool) {
    QString toolName = QString::fromStdString(tool->getName());
    m_toolNameLabel->setText(tr("Tool Properties - %1").arg(toolName));
  } else {
    m_toolNameLabel->setText(tr("Tool Properties"));
    // No active tool, display nothing
    m_propertiesLayout->addStretch(1);
    return;
  }
  
  // Create properties based on tool type
  if (toolType == "brush") {
    createBrushProperties();
  } else if (toolType == "mypaint" || toolType == "mypainttnz") {
    createMyPaintBrushProperties();
  } else if (toolType == "fill") {
    // TODO: Implement later
    QLabel *comingSoon = new QLabel(tr("Properties for Fill tool - Coming soon!"), this);
    comingSoon->setAlignment(Qt::AlignCenter);
    comingSoon->setStyleSheet("color: gray; padding: 20px;");
    m_propertiesLayout->addWidget(comingSoon);
  } else if (toolType == "eraser") {
    // TODO: Implement later
    QLabel *comingSoon = new QLabel(tr("Properties for Eraser tool - Coming soon!"), this);
    comingSoon->setAlignment(Qt::AlignCenter);
    comingSoon->setStyleSheet("color: gray; padding: 20px;");
    m_propertiesLayout->addWidget(comingSoon);
  } else {
    // Other tools or unrecognized tool
    QLabel *comingSoon = new QLabel(tr("Properties for this tool - Coming soon!"), this);
    comingSoon->setAlignment(Qt::AlignCenter);
    comingSoon->setStyleSheet("color: gray; padding: 20px;");
    m_propertiesLayout->addWidget(comingSoon);
  }
  
  // Add stretch at the end
  m_propertiesLayout->addStretch(1);
}

void ToolPropertiesPanel::clearProperties() {
  // Remove all widgets from layout
  QLayoutItem *item;
  while ((item = m_propertiesLayout->takeAt(0)) != nullptr) {
    if (item->widget()) {
      delete item->widget();
    }
    delete item;
  }
}

//-----------------------------------------------------------------------------
// Brush Properties Creation
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createBrushProperties() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Create brush properties in STRICT order per level type:
  // Vector: Size/Accuracy/Smooth/DrawOrder/BreakAngles/Pressure/Range/Snap/Assistants/Cap/Join/Miter
  // Toonz Raster: Size/Hardness/Smooth/DrawOrder/LockAlpha/PencilMode/Assistants/Pressure
  // Raster: Size/Hardness/Opacity/LockAlpha/Pressure/Assistants
  
  // === SIZE (all levels) ===
  createSizeProperty();
  
  // === VECTOR: Accuracy ===
  createAccuracyProperty();
  
  // === RASTER/TOONZ RASTER: Hardness ===
  createHardnessProperty();
  
  // === RASTER ONLY: Opacity (double slider) ===
  createOpacityProperty();
  
  // === VECTOR & TOONZ RASTER: Smooth ===
  createSmoothProperty();
  
  // === VECTOR & TOONZ RASTER: Draw Order ===
  createDrawOrderProperty();
  
  // === RASTER & TOONZ RASTER: Lock Alpha ===
  createLockAlphaProperty();
  
  // === TOONZ RASTER: Pencil Mode ===
  createPencilModeProperty();
  
  // === VECTOR: Break Angles ===
  createBreakAnglesProperty();
  
  // === PRESSURE (all levels, but different positions) ===
  createPressureProperty();
  
  // === VECTOR: Frame Range (Off/Linear/In/Out/In&Out) ===
  createFrameRangeProperty();
  
  // === VECTOR: Snap ===
  createSnapProperty();
  createSnapSensitivityProperty();
  
  // === ASSISTANTS (all levels) ===
  createAssistantsProperty();
  
  // === VECTOR: Cap/Join/Miter ===
  createCapProperty();
  createJoinProperty();
  createMiterProperty();
}

//-----------------------------------------------------------------------------
// MyPaint Brush Properties Creation
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createMyPaintBrushProperties() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // MyPaint brushes use Modifier* properties to control the brush
  // These are single-value properties (not min/max pairs)
  //
  // Raster MyPaint (mypaint):
  //   Size (ModifierSize) / Opacity (ModifierOpacity) / Eraser (ModifierEraser) / 
  //   Lock Alpha (ModifierLockAlpha) / Pressure / Assistants
  //
  // Toonz Raster MyPaint (mypainttnz):
  //   Size (ModifierSize) / Smooth / Lock Alpha (ModifierLockAlpha) / 
  //   Assistants / Pressure
  
  QString toolType = m_currentToolType;
  
  if (toolType == "mypaint") {
    // === RASTER MYPAINT (in order as specified) ===
    createMyPaintSizeProperty();           // ModifierSize slider
    createMyPaintOpacityProperty();        // ModifierOpacity slider  
    createModifierEraserProperty();        // Eraser checkbox
    createModifierLockAlphaProperty();     // Lock Alpha checkbox
    createPressureProperty();              // Pressure checkbox
    createAssistantsProperty();            // Assistants checkbox
    // Note: Preset is excluded as per requirements
  } else if (toolType == "mypainttnz") {
    // === TOONZ RASTER MYPAINT (in order as specified) ===
    createMyPaintSizeProperty();           // ModifierSize slider
    createSmoothProperty();                // Smooth slider
    createModifierLockAlphaProperty();     // Lock Alpha checkbox
    createAssistantsProperty();            // Assistants checkbox
    createPressureProperty();              // Pressure checkbox
    // Note: Preset is excluded as per requirements
    // Note: NO Draw Order, NO Pencil Mode for MyPaint
  }
}

void ToolPropertiesPanel::createSizeProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Size property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Size:" || propName == "Size" || propName == "Thickness:" || propName == "Thickness") {
      // Try TDoublePairProperty first (min/max)
      TDoublePairProperty *pairProp = dynamic_cast<TDoublePairProperty*>(prop);
      if (pairProp) {
        createDoublePairSlider(tr("Size"), pairProp, propName);
        return;
      }
      
      // Try TIntPairProperty (min/max)
      TIntPairProperty *intPairProp = dynamic_cast<TIntPairProperty*>(prop);
      if (intPairProp) {
        createIntPairSlider(tr("Size"), intPairProp, propName);
        return;
      }
      
      // Try TDoubleProperty
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;
        double max = doubleProp->getRange().second;
        double value = doubleProp->getValue();
        
        // Create widget
        QWidget *container = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->setMargin(0);
        layout->setSpacing(3);
        
        // Label + value display
        QHBoxLayout *headerLayout = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(tr("Size"), container);
        QLabel *valueLabel = new QLabel(QString::number(value, 'f', 1), container);
        valueLabel->setAlignment(Qt::AlignRight);
        valueLabel->setMinimumWidth(50);
        headerLayout->addWidget(nameLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(valueLabel);
        layout->addLayout(headerLayout);
        
        // Slider
        QSlider *slider = new QSlider(Qt::Horizontal, container);
        slider->setMinimum(static_cast<int>(min * 10));
        slider->setMaximum(static_cast<int>(max * 10));
        slider->setValue(static_cast<int>(value * 10));
        layout->addWidget(slider);
        
        // Connect slider to value label
        connect(slider, &QSlider::valueChanged, [valueLabel](int val) {
          valueLabel->setText(QString::number(val / 10.0, 'f', 1));
        });
        
        // Connect slider to tool property
        connect(slider, &QSlider::valueChanged, [this, doubleProp, propName](int val) {
          double newValue = val / 10.0;
          doubleProp->setValue(newValue);
          
          TTool *tool = getCurrentTool();
          if (tool) {
            tool->onPropertyChanged(propName);
            if (m_toolHandle) {
              m_toolHandle->notifyToolChanged();
            }
          }
        });
        
        m_propertiesLayout->addWidget(container);
        return;
      }
      
      // Try TIntProperty
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        // Create widget
        QWidget *container = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->setMargin(0);
        layout->setSpacing(3);
        
        // Label + value display
        QHBoxLayout *headerLayout = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(tr("Size"), container);
        QLabel *valueLabel = new QLabel(QString::number(value), container);
        valueLabel->setAlignment(Qt::AlignRight);
        valueLabel->setMinimumWidth(50);
        headerLayout->addWidget(nameLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(valueLabel);
        layout->addLayout(headerLayout);
        
        // Slider
        QSlider *slider = new QSlider(Qt::Horizontal, container);
        slider->setMinimum(min);
        slider->setMaximum(max);
        slider->setValue(value);
        layout->addWidget(slider);
        
        // Connect slider to value label
        connect(slider, &QSlider::valueChanged, [valueLabel](int val) {
          valueLabel->setText(QString::number(val));
        });
        
        // Connect slider to tool property
        connect(slider, &QSlider::valueChanged, [this, intProp, propName](int val) {
          intProp->setValue(val);
          
          TTool *tool = getCurrentTool();
          if (tool) {
            tool->onPropertyChanged(propName);
            if (m_toolHandle) {
              m_toolHandle->notifyToolChanged();
            }
          }
        });
        
        m_propertiesLayout->addWidget(container);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createHardnessProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Hardness property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Hardness:" || propName == "Hardness") {
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Hardness"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
      
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;
        double max = doubleProp->getRange().second;
        double value = doubleProp->getValue();
        
        QWidget *widget = createDoubleSliderWithLabel(tr("Hardness"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createOpacityProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Opacity property (Raster brush uses TDoublePairProperty)
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Opacity:" || propName == "Opacity") {
      // Try TDoublePairProperty first (Raster brush uses this for min/max opacity)
      TDoublePairProperty *doublePairProp = dynamic_cast<TDoublePairProperty*>(prop);
      if (doublePairProp) {
        createDoublePairSlider(tr("Opacity"), doublePairProp, propName);
        // Note: createDoublePairSlider already adds spacing
        return;
      }
      
      // Try IntPairProperty (min/max)
      TIntPairProperty *intPairProp = dynamic_cast<TIntPairProperty*>(prop);
      if (intPairProp) {
        createIntPairSlider(tr("Opacity"), intPairProp, propName);
        return;
      }
      
      // Try single IntProperty
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Opacity"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        m_propertiesLayout->addSpacing(10);
        return;
      }
      
      // Try single DoubleProperty
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;
        double max = doubleProp->getRange().second;
        double value = doubleProp->getValue();
        
        QWidget *widget = createDoubleSliderWithLabel(tr("Opacity"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        m_propertiesLayout->addSpacing(10);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createLockAlphaProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Lock Alpha property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Lock Alpha" || propName == "LockAlpha") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Lock Alpha"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createPencilModeProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Pencil Mode property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Pencil" || propName == "PencilMode" || propName == "Pencil Mode") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Pencil Mode"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createDrawOrderProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Draw Order property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Draw Order:" || propName == "DrawOrder" || propName == "Draw Order") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        QStringList items;
        const TEnumProperty::Items &enumItems = enumProp->getItems();
        for (int j = 0; j < enumItems.size(); ++j) {
          items << enumItems[j].UIName;
        }
        int currentIndex = enumProp->getIndex();
        
        QWidget *widget = createCollapsibleEnum(tr("Draw Order"), items, currentIndex, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createCapProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(1);  // prop[1] for Cap/Join
  if (!props) {
    // Try getProperties(0) if group 1 doesn't exist
    props = tool->getProperties(0);
    if (!props) return;
  }
  
  // Cap icon names corresponding to the enum items
  QStringList capIcons;
  capIcons << "butt_cap" << "round_cap" << "projecting_cap";
  
  // Search for Cap Style property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Cap:" || propName == "Cap" || propName == "CapStyle" || propName == "Cap Style:" || propName == "Cap Style") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        QStringList items;
        const TEnumProperty::Items &enumItems = enumProp->getItems();
        for (int j = 0; j < enumItems.size(); ++j) {
          items << enumItems[j].UIName;
        }
        int currentIndex = enumProp->getIndex();
        
        // Only create widget if we have items
        if (items.size() > 0) {
          // Use standard collapsible enum but add icons if m_showIcons is true
          QWidget *widget = createCollapsibleEnumWithIcons(tr("Cap"), items, currentIndex, propName, capIcons);
          widget->setProperty("propGroup", 1);  // Cap is in group 1
          m_propertiesLayout->addWidget(widget);
        }
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createJoinProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(1);  // prop[1] for Cap/Join
  if (!props) {
    // Try getProperties(0) if group 1 doesn't exist
    props = tool->getProperties(0);
    if (!props) return;
  }
  
  // Join icon names corresponding to the enum items
  QStringList joinIcons;
  joinIcons << "miter_join" << "round_join" << "bevel_join";
  
  // Search for Join Style property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Join:" || propName == "Join" || propName == "JoinStyle" || propName == "Join Style:" || propName == "Join Style") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        QStringList items;
        const TEnumProperty::Items &enumItems = enumProp->getItems();
        for (int j = 0; j < enumItems.size(); ++j) {
          items << enumItems[j].UIName;
        }
        int currentIndex = enumProp->getIndex();
        
        // Only create widget if we have items
        if (items.size() > 0) {
          // Use collapsible enum with icons if m_showIcons is true
          QWidget *widget = createCollapsibleEnumWithIcons(tr("Join"), items, currentIndex, propName, joinIcons);
          widget->setProperty("propGroup", 1);  // Join is in group 1
          m_propertiesLayout->addWidget(widget);
        }
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createSmoothProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Smooth property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Smooth:" || propName == "Smooth") {
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Smooth"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
      
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;
        double max = doubleProp->getRange().second;
        double value = doubleProp->getValue();
        
        QWidget *widget = createDoubleSliderWithLabel(tr("Smooth"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createAssistantsProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Assistants property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Assistants" || propName == "Assistant") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Assistants"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createPressureProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Search for Pressure property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Pressure" || propName == "PressureSensitivity" || propName == "Pressure Sensitivity") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Pressure"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createAccuracyProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Accuracy:" || propName == "Accuracy") {
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Accuracy"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
      
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;
        double max = doubleProp->getRange().second;
        double value = doubleProp->getValue();
        
        QWidget *widget = createDoubleSliderWithLabel(tr("Accuracy"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createBreakAnglesProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Break" || propName == "Break Angles" || propName == "Break:" || propName == "BreakAngles") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Break Angles"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createFrameRangeProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Frame Range:" || propName == "Frame Range" || propName == "FrameRange" ||
        propName == "Range:" || propName == "Range") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        QStringList items;
        const TEnumProperty::Items &enumItems = enumProp->getItems();
        for (int j = 0; j < enumItems.size(); ++j) {
          items << enumItems[j].UIName;
        }
        int currentIndex = enumProp->getIndex();
        
        QWidget *widget = createCollapsibleEnum(tr("Frame Range"), items, currentIndex, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createSnapProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Snap" || propName == "Snap:" || propName == "AutoFill") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Snap"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createSnapSensitivityProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    // Property name is "Sensitivity:" (not "Snap Sensitivity")
    if (propName == "Sensitivity:" || propName == "Sensitivity" || propName == "SnapSensitivity") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        // Use collapsible enum like Cap/Join for reliable synchronization
        QStringList items;
        const TEnumProperty::Items &enumItems = enumProp->getItems();
        for (int j = 0; j < enumItems.size(); ++j) {
          items << enumItems[j].UIName;
        }
        int currentIndex = enumProp->getIndex();
        
        if (items.size() > 0) {
          QWidget *widget = createCollapsibleEnum(tr("Sensitivity"), items, currentIndex, propName);
          widget->setProperty("propGroup", 0);
          m_propertiesLayout->addWidget(widget);
        }
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createMiterProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(1);  // prop[1] for Cap/Join/Miter
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Miter:" || propName == "Miter" || propName == "MiterJoinLimit") {
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Miter"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
      
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;
        double max = doubleProp->getRange().second;
        double value = doubleProp->getValue();
        
        QWidget *widget = createDoubleSliderWithLabel(tr("Miter"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createModifierSizeProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "ModifierSize" || propName == "Modifier Size") {
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Modifier Size"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createModifierOpacityProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "ModifierOpacity" || propName == "Modifier Opacity") {
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int min = intProp->getRange().first;
        int max = intProp->getRange().second;
        int value = intProp->getValue();
        
        QWidget *widget = createSliderWithLabel(tr("Modifier Opacity"), min, max, value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createModifierEraserProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "ModifierEraser") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Eraser"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createModifierLockAlphaProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "ModifierLockAlpha" || propName == "Lock Alpha") {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        bool value = boolProp->getValue();
        QWidget *widget = createCheckBox(tr("Lock Alpha"), value, propName);
        m_propertiesLayout->addWidget(widget);
        return;
      }
    }
  }
}

//-----------------------------------------------------------------------------
// MyPaint-specific Properties (Special sliders)
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createMyPaintSizeProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Find ModifierSize property (MyPaint uses this for size)
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "ModifierSize") {
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;    // -3.0
        double max = doubleProp->getRange().second;   // 3.0
        double value = doubleProp->getValue();
        
        // Create widget
        QWidget *container = new QWidget(this);
        container->setProperty("propName", QString::fromStdString(propName));
        container->setProperty("propGroup", 0);
        container->setProperty("valueFactor", 100);  // MyPaint Size uses factor 100
        container->setProperty("valueDecimals", 2);  // Display 2 decimal places
        
        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->setMargin(0);
        layout->setSpacing(3);
        
        // Label (respect m_showLabels) + value display
        QHBoxLayout *headerLayout = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(tr("Size"), container);
        nameLabel->setVisible(m_showLabels);
        QLabel *valueLabel = new QLabel(QString::number(value, 'f', 2), container);
        valueLabel->setAlignment(Qt::AlignRight);
        valueLabel->setMinimumWidth(50);
        valueLabel->setVisible(m_showLabels);
        headerLayout->addWidget(nameLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(valueLabel);
        layout->addLayout(headerLayout);
        
        // Slider + numeric field layout
        QHBoxLayout *sliderLayout = new QHBoxLayout();
        sliderLayout->setMargin(0);
        sliderLayout->setSpacing(5);
        
        // Numeric field (respect m_showNumericFields)
        QLineEdit *lineEdit = new QLineEdit(container);
        lineEdit->setText(QString::number(value, 'f', 2));
        lineEdit->setAlignment(Qt::AlignRight);
        lineEdit->setMaximumWidth(60);
        lineEdit->setValidator(new QDoubleValidator(min, max, 2, lineEdit));
        lineEdit->setVisible(m_showNumericFields);
        
        // Slider (range -3.0 to 3.0, multiply by 100 for integer slider)
        QSlider *slider = new QSlider(Qt::Horizontal, container);
        slider->setMinimum(static_cast<int>(min * 100));
        slider->setMaximum(static_cast<int>(max * 100));
        slider->setValue(static_cast<int>(value * 100));
        
        sliderLayout->addWidget(lineEdit);
        sliderLayout->addWidget(slider);
        layout->addLayout(sliderLayout);
        
        // Connect slider to value label and line edit
        connect(slider, &QSlider::valueChanged, [valueLabel, lineEdit](int val) {
          double dVal = val / 100.0;
          valueLabel->setText(QString::number(dVal, 'f', 2));
          lineEdit->blockSignals(true);
          lineEdit->setText(QString::number(dVal, 'f', 2));
          lineEdit->blockSignals(false);
        });
        
        // Connect line edit to slider
        connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
          double val = lineEdit->text().toDouble();
          slider->blockSignals(true);
          slider->setValue(static_cast<int>(val * 100));
          slider->blockSignals(false);
        });
        
        // Connect slider to tool property
        connect(slider, &QSlider::valueChanged, [this, doubleProp, propName](int val) {
          double newValue = val / 100.0;
          doubleProp->setValue(newValue);
          
          TTool *tool = getCurrentTool();
          if (tool) {
            tool->onPropertyChanged(propName);
            if (m_toolHandle) {
              m_toolHandle->notifyToolChanged();
            }
          }
        });
        
        // Connect line edit to tool property
        connect(lineEdit, &QLineEdit::editingFinished, [this, doubleProp, lineEdit, propName]() {
          double newValue = lineEdit->text().toDouble();
          doubleProp->setValue(newValue);
          
          TTool *tool = getCurrentTool();
          if (tool) {
            tool->onPropertyChanged(propName);
            if (m_toolHandle) {
              m_toolHandle->notifyToolChanged();
            }
          }
        });
        
        m_propertiesLayout->addWidget(container);
        return;
      }
    }
  }
}

void ToolPropertiesPanel::createMyPaintOpacityProperty() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  // Find ModifierOpacity property (MyPaint uses this for opacity)
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "ModifierOpacity") {
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double min = doubleProp->getRange().first;    // 0
        double max = doubleProp->getRange().second;   // 100
        double value = doubleProp->getValue();
        
        // Create widget
        QWidget *container = new QWidget(this);
        container->setProperty("propName", QString::fromStdString(propName));
        container->setProperty("propGroup", 0);
        container->setProperty("valueFactor", 1);  // MyPaint Opacity uses factor 1 (direct value)
        container->setProperty("valueDecimals", 0);  // Display as integer
        
        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->setMargin(0);
        layout->setSpacing(3);
        
        // Label (respect m_showLabels) + value display
        QHBoxLayout *headerLayout = new QHBoxLayout();
        QLabel *nameLabel = new QLabel(tr("Opacity"), container);
        nameLabel->setVisible(m_showLabels);
        QLabel *valueLabel = new QLabel(QString::number(static_cast<int>(value)), container);
        valueLabel->setAlignment(Qt::AlignRight);
        valueLabel->setMinimumWidth(50);
        valueLabel->setVisible(m_showLabels);
        headerLayout->addWidget(nameLabel);
        headerLayout->addStretch();
        headerLayout->addWidget(valueLabel);
        layout->addLayout(headerLayout);
        
        // Slider + numeric field layout
        QHBoxLayout *sliderLayout = new QHBoxLayout();
        sliderLayout->setMargin(0);
        sliderLayout->setSpacing(5);
        
        // Numeric field (respect m_showNumericFields)
        QLineEdit *lineEdit = new QLineEdit(container);
        lineEdit->setText(QString::number(static_cast<int>(value)));
        lineEdit->setAlignment(Qt::AlignRight);
        lineEdit->setMaximumWidth(60);
        lineEdit->setValidator(new QIntValidator(static_cast<int>(min), static_cast<int>(max), lineEdit));
        lineEdit->setVisible(m_showNumericFields);
        
        // Slider (range 0 to 100)
        QSlider *slider = new QSlider(Qt::Horizontal, container);
        slider->setMinimum(static_cast<int>(min));
        slider->setMaximum(static_cast<int>(max));
        slider->setValue(static_cast<int>(value));
        
        sliderLayout->addWidget(lineEdit);
        sliderLayout->addWidget(slider);
        layout->addLayout(sliderLayout);
        
        // Connect slider to value label and line edit
        connect(slider, &QSlider::valueChanged, [valueLabel, lineEdit](int val) {
          valueLabel->setText(QString::number(val));
          lineEdit->blockSignals(true);
          lineEdit->setText(QString::number(val));
          lineEdit->blockSignals(false);
        });
        
        // Connect line edit to slider
        connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
          int val = lineEdit->text().toInt();
          slider->blockSignals(true);
          slider->setValue(val);
          slider->blockSignals(false);
        });
        
        // Connect slider to tool property
        connect(slider, &QSlider::valueChanged, [this, doubleProp, propName](int val) {
          double newValue = static_cast<double>(val);
          doubleProp->setValue(newValue);
          
          TTool *tool = getCurrentTool();
          if (tool) {
            tool->onPropertyChanged(propName);
            if (m_toolHandle) {
              m_toolHandle->notifyToolChanged();
            }
          }
        });
        
        // Connect line edit to tool property
        connect(lineEdit, &QLineEdit::editingFinished, [this, doubleProp, lineEdit, propName]() {
          double newValue = static_cast<double>(lineEdit->text().toInt());
          doubleProp->setValue(newValue);
          
          TTool *tool = getCurrentTool();
          if (tool) {
            tool->onPropertyChanged(propName);
            if (m_toolHandle) {
              m_toolHandle->notifyToolChanged();
            }
          }
        });
        
        m_propertiesLayout->addWidget(container);
        return;
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Helper Methods for UI Creation
//-----------------------------------------------------------------------------

QWidget* ToolPropertiesPanel::createSliderWithLabel(const QString &label, int min, int max, 
                                                     int value, const std::string &propName) {
  QWidget *container = new QWidget(this);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);
  
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setMargin(0);
  layout->setSpacing(3);
  
  // Label (respect m_showLabels)
  QLabel *nameLabel = new QLabel(label, container);
  nameLabel->setVisible(m_showLabels);
  layout->addWidget(nameLabel);
  
  // Slider with numeric fields
  QHBoxLayout *sliderLayout = new QHBoxLayout();
  sliderLayout->setMargin(0);
  sliderLayout->setSpacing(5);
  
  // Numeric field (QLineEdit to avoid arrows, respect m_showNumericFields)
  QLineEdit *lineEdit = new QLineEdit(container);
  lineEdit->setText(QString::number(value));
  lineEdit->setFixedWidth(45);  // Reduce size
  lineEdit->setAlignment(Qt::AlignRight);
  lineEdit->setVisible(m_showNumericFields);
  QIntValidator *validator = new QIntValidator(min, max, lineEdit);
  lineEdit->setValidator(validator);
  sliderLayout->addWidget(lineEdit);
  
  // Slider
  QSlider *slider = new QSlider(Qt::Horizontal, container);
  slider->setMinimum(min);
  slider->setMaximum(max);
  slider->setValue(value);
  sliderLayout->addWidget(slider, 1);  // Stretch to take available space
  
  layout->addLayout(sliderLayout);
  
  // Connect slider and lineEdit together
  connect(slider, &QSlider::valueChanged, [lineEdit](int val) {
    lineEdit->setText(QString::number(val));
  });
  connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
    slider->setValue(lineEdit->text().toInt());
  });
  
  // Connect to tool property
  connect(slider, &QSlider::valueChanged, [this, propName](int val) {
    TTool *tool = getCurrentTool();
    if (!tool) return;
    
    TPropertyGroup *props = tool->getProperties(0);
    if (!props) return;
    
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty *prop = props->getProperty(i);
      if (prop && prop->getName() == propName) {
        TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
        if (intProp) {
          intProp->setValue(val);
          tool->onPropertyChanged(propName);
          if (m_toolHandle) m_toolHandle->notifyToolChanged();
        }
        break;
      }
    }
  });
  
  return container;
}

QWidget* ToolPropertiesPanel::createDoubleSliderWithLabel(const QString &label, double min, 
                                                           double max, double value, 
                                                           const std::string &propName) {
  QWidget *container = new QWidget(this);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);
  
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setMargin(0);
  layout->setSpacing(3);
  
  // Label (respect m_showLabels)
  QLabel *nameLabel = new QLabel(label, container);
  nameLabel->setVisible(m_showLabels);
  layout->addWidget(nameLabel);
  
  // Slider with numeric field
  QHBoxLayout *sliderLayout = new QHBoxLayout();
  sliderLayout->setMargin(0);
  sliderLayout->setSpacing(5);
  
  // Numeric field (QLineEdit to avoid arrows, respect m_showNumericFields)
  QLineEdit *lineEdit = new QLineEdit(container);
  lineEdit->setText(QString::number(value, 'f', 1));
  lineEdit->setFixedWidth(50);  // Reduce size
  lineEdit->setAlignment(Qt::AlignRight);
  lineEdit->setVisible(m_showNumericFields);
  QDoubleValidator *validator = new QDoubleValidator(min, max, 1, lineEdit);
  lineEdit->setValidator(validator);
  sliderLayout->addWidget(lineEdit);
  
  // Slider (scale to int)
  QSlider *slider = new QSlider(Qt::Horizontal, container);
  slider->setMinimum(static_cast<int>(min * 10));
  slider->setMaximum(static_cast<int>(max * 10));
  slider->setValue(static_cast<int>(value * 10));
  sliderLayout->addWidget(slider, 1);  // Stretch to take available space
  
  layout->addLayout(sliderLayout);
  
  // Connect slider and lineEdit together
  connect(slider, &QSlider::valueChanged, [lineEdit](int val) {
    lineEdit->setText(QString::number(val / 10.0, 'f', 1));
  });
  connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
    slider->setValue(static_cast<int>(lineEdit->text().toDouble() * 10));
  });
  
  // Connect to tool property
  connect(slider, &QSlider::valueChanged, [this, propName](int val) {
    double newValue = val / 10.0;
    
    TTool *tool = getCurrentTool();
    if (!tool) return;
    
    TPropertyGroup *props = tool->getProperties(0);
    if (!props) return;
    
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty *prop = props->getProperty(i);
      if (prop && prop->getName() == propName) {
        TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
        if (doubleProp) {
          doubleProp->setValue(newValue);
          tool->onPropertyChanged(propName);
          if (m_toolHandle) m_toolHandle->notifyToolChanged();
        }
        break;
      }
    }
  });
  
  return container;
}

QWidget* ToolPropertiesPanel::createCheckBox(const QString &label, bool checked, 
                                             const std::string &propName) {
  QCheckBox *checkBox = new QCheckBox(label, this);
  checkBox->setProperty("propName", QString::fromStdString(propName));
  checkBox->setProperty("propGroup", 0);
  checkBox->setChecked(checked);
  
  // Connect checkbox to tool property
  connect(checkBox, &QCheckBox::toggled, [this, propName](bool checked) {
    TTool *tool = getCurrentTool();
    if (!tool) return;
    
    TPropertyGroup *props = tool->getProperties(0);
    if (!props) return;
    
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty *prop = props->getProperty(i);
      if (prop && prop->getName() == propName) {
        TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
        if (boolProp) {
          boolProp->setValue(checked);
          tool->onPropertyChanged(propName);
          if (m_toolHandle) m_toolHandle->notifyToolChanged();
        }
        break;
      }
    }
  });
  
  return checkBox;
}

QWidget* ToolPropertiesPanel::createCollapsibleEnum(const QString &label, 
                                                     const QStringList &items, 
                                                     int currentIndex, 
                                                     const std::string &propName,
                                                     const QString &iconName) {
  QWidget *container = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(2);
  
  // Header with toggle button
  QWidget *header = new QWidget(container);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  headerLayout->setMargin(2);
  headerLayout->setSpacing(5);
  
  // Toggle button (triangle)
  QToolButton *toggleButton = new QToolButton(header);
  toggleButton->setArrowType(Qt::RightArrow);
  toggleButton->setCheckable(true);
  toggleButton->setStyleSheet("QToolButton { border: none; }");
  toggleButton->setFixedSize(16, 16);
  headerLayout->addWidget(toggleButton);
  
  // Icon (if provided)
  if (!iconName.isEmpty()) {
    QLabel *iconLabel = new QLabel(header);
    QPixmap icon = QIcon(":Resources/" + iconName + ".svg").pixmap(16, 16);
    if (!icon.isNull()) {
      iconLabel->setPixmap(icon);
      headerLayout->addWidget(iconLabel);
    }
  }
  
  // Label (respect m_showLabels)
  QLabel *nameLabel = new QLabel(label, header);
  nameLabel->setVisible(m_showLabels);
  headerLayout->addWidget(nameLabel);
  
  // Current value indicator (attenuated: italic + disabled color from palette)
  QLabel *valueLabel = new QLabel(items.value(currentIndex, ""), header);
  valueLabel->setObjectName("valueLabel");
  QFont italicFont = valueLabel->font();
  italicFont.setItalic(true);
  valueLabel->setFont(italicFont);
  // Use palette disabled text color for attenuation (theme-aware)
  QPalette pal = valueLabel->palette();
  pal.setColor(QPalette::WindowText, palette().color(QPalette::Disabled, QPalette::WindowText));
  valueLabel->setPalette(pal);
  headerLayout->addStretch();
  headerLayout->addWidget(valueLabel);
  
  mainLayout->addWidget(header);
  
  // Content (checkable buttons in a group)
  QWidget *content = new QWidget(container);
  
  // Load saved collapse state from QSettings (persists between sessions)
  QSettings settings;
  QString settingsKey = QString("ToolPropertiesPanel/CollapsedState/%1").arg(QString::fromStdString(propName));
  bool isExpanded = settings.value(settingsKey, false).toBool();  // Default: collapsed
  content->setVisible(isExpanded);
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);
  
  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  contentLayout->setMargin(0);
  contentLayout->setContentsMargins(25, 2, 0, 2);  // Indent
  contentLayout->setSpacing(2);
  
  // Create button group for exclusive selection
  QButtonGroup *buttonGroup = new QButtonGroup(content);
  buttonGroup->setExclusive(true);
  
  // Create checkable buttons using custom ToolPropertyButton class
  // This class has custom paintEvent that respects theme colors AND Cells options
  for (int i = 0; i < items.size(); ++i) {
    ToolPropertyButton *optionButton = new ToolPropertyButton(items[i], content);
    optionButton->setCursor(Qt::PointingHandCursor);
    optionButton->setCheckable(true);
    optionButton->setAutoExclusive(true);
    optionButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    optionButton->setMinimumHeight(24);
    optionButton->setMinimumWidth(100);
    
    // Set Cells Borders/Backgrounds state
    optionButton->setShowBorders(m_showBorders);
    optionButton->setShowBackgrounds(m_showBackgrounds);
    
    // Set initial checked state
    if (i == currentIndex) {
      optionButton->setChecked(true);
    }
    
    buttonGroup->addButton(optionButton, i);
    contentLayout->addWidget(optionButton);
  }
  
  mainLayout->addWidget(content);
  
  // Toggle behavior - Save state on change
  connect(toggleButton, &QToolButton::toggled, [content, toggleButton, propName](bool checked) {
    content->setVisible(checked);
    toggleButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    
    // Save collapsed state to QSettings for persistence
    QSettings settings;
    QString settingsKey = QString("ToolPropertiesPanel/CollapsedState/%1").arg(QString::fromStdString(propName));
    settings.setValue(settingsKey, checked);
  });
  
  // Store propName in container for later use
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);  // Default to group 0
  
  // Connect button group to property update
  connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), 
          [this, valueLabel, items, container](int id) {
    // Update value label
    valueLabel->setText(items.value(id, ""));
    
    // Update tool property
    std::string propName = container->property("propName").toString().toStdString();
    int propGroup = container->property("propGroup").toInt();
    
    TTool *tool = getCurrentTool();
    if (!tool) return;
    
    TPropertyGroup *props = tool->getProperties(propGroup);
    if (!props) return;
    
    for (int k = 0; k < props->getPropertyCount(); ++k) {
      TProperty *prop = props->getProperty(k);
      if (prop && prop->getName() == propName) {
        TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
        if (enumProp) {
          enumProp->setIndex(id);
          tool->onPropertyChanged(propName);
          if (m_toolHandle) m_toolHandle->notifyToolChanged();
        }
        break;
      }
    }
  });
  
  return container;
}

//-----------------------------------------------------------------------------
// createCollapsibleEnumWithIcons - Collapsible enum with icons for each option
//-----------------------------------------------------------------------------

QWidget* ToolPropertiesPanel::createCollapsibleEnumWithIcons(
    const QString &label, const QStringList &items, 
    int currentIndex, const std::string &propName,
    const QStringList &iconNames) {
  
  QWidget *container = new QWidget(this);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);
  
  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(2);
  
  // Header row with toggle button, label, and current value
  QHBoxLayout *headerLayout = new QHBoxLayout();
  headerLayout->setMargin(0);
  headerLayout->setSpacing(5);
  
  // Collapsible toggle button (triangle)
  QToolButton *toggleButton = new QToolButton(container);
  toggleButton->setArrowType(Qt::DownArrow);
  toggleButton->setCheckable(true);
  toggleButton->setAutoRaise(true);
  toggleButton->setFixedSize(16, 16);
  
  // Load collapsed state from QSettings
  QSettings settings;
  QString settingsKey = QString("ToolPropertiesPanel/CollapsedState/%1").arg(QString::fromStdString(propName));
  bool isExpanded = settings.value(settingsKey, true).toBool();
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);
  
  headerLayout->addWidget(toggleButton);
  
  // Label
  QLabel *nameLabel = new QLabel(label, container);
  nameLabel->setVisible(m_showLabels);
  headerLayout->addWidget(nameLabel);
  
  headerLayout->addStretch();
  
  // Value label showing current selection (attenuated: italic + disabled color)
  QLabel *valueLabel = new QLabel(items.value(currentIndex, ""), container);
  valueLabel->setObjectName("valueLabel");
  QFont italicFont = valueLabel->font();
  italicFont.setItalic(true);
  valueLabel->setFont(italicFont);
  // Use palette disabled text color for attenuation (theme-aware)
  QPalette pal = valueLabel->palette();
  pal.setColor(QPalette::WindowText, palette().color(QPalette::Disabled, QPalette::WindowText));
  valueLabel->setPalette(pal);
  headerLayout->addWidget(valueLabel);
  
  mainLayout->addLayout(headerLayout);
  
  // Content widget (collapsible options)
  QWidget *content = new QWidget(container);
  content->setVisible(isExpanded);
  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  contentLayout->setMargin(0);
  contentLayout->setContentsMargins(20, 0, 0, 0);  // Indent content
  contentLayout->setSpacing(2);
  
  // Create button group for exclusive selection
  QButtonGroup *buttonGroup = new QButtonGroup(content);
  buttonGroup->setExclusive(true);
  
  bool showIcons = m_showIcons;
  
  // Create checkable buttons with icons using custom ToolPropertyButton
  for (int i = 0; i < items.size(); ++i) {
    ToolPropertyButton *optionButton = new ToolPropertyButton("", container);  // Empty text, we'll add via layout
    optionButton->setCursor(Qt::PointingHandCursor);
    optionButton->setCheckable(true);
    optionButton->setAutoExclusive(true);
    optionButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    optionButton->setMinimumHeight(24);
    optionButton->setMinimumWidth(100);
    
    // Set Cells Borders/Backgrounds state
    optionButton->setShowBorders(m_showBorders);
    optionButton->setShowBackgrounds(m_showBackgrounds);
    
    // For buttons with custom layout, we need to override the paintEvent text drawing
    // Create layout for button content: text on left, icon on right
    QHBoxLayout *btnLayout = new QHBoxLayout(optionButton);
    btnLayout->setMargin(4);
    btnLayout->setSpacing(5);
    
    QLabel *textLabel = new QLabel(items[i], optionButton);
    textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(textLabel);
    
    btnLayout->addStretch();
    
    // Add icon on the right if showIcons is true and icon exists
    // Use QToolButton with setIcon() so it redraws on theme change via SvgIconEngine
    if (showIcons && i < iconNames.size() && !iconNames[i].isEmpty()) {
      QToolButton *iconBtn = new QToolButton(optionButton);
      iconBtn->setIcon(createQIcon(iconNames[i].toStdString().c_str()));
      iconBtn->setIconSize(QSize(16, 16));
      iconBtn->setAutoRaise(true);  // Transparent background
      iconBtn->setFocusPolicy(Qt::NoFocus);  // Non-focusable
      iconBtn->setAttribute(Qt::WA_TransparentForMouseEvents);  // Non-interactive
      btnLayout->addWidget(iconBtn);
    }
    
    // Set initial checked state
    if (i == currentIndex) {
      optionButton->setChecked(true);
    }
    
    buttonGroup->addButton(optionButton, i);
    contentLayout->addWidget(optionButton);
  }
  
  mainLayout->addWidget(content);
  
  // Toggle behavior - Save state on change
  connect(toggleButton, &QToolButton::toggled, [content, toggleButton, propName](bool checked) {
    content->setVisible(checked);
    toggleButton->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    
    QSettings settings;
    QString settingsKey = QString("ToolPropertiesPanel/CollapsedState/%1").arg(QString::fromStdString(propName));
    settings.setValue(settingsKey, checked);
  });
  
  // Connect button group to property update
  connect(buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), 
          [this, valueLabel, items, container](int id) {
    valueLabel->setText(items.value(id, ""));
    
    std::string propName = container->property("propName").toString().toStdString();
    int propGroup = container->property("propGroup").toInt();
    
    TTool *tool = getCurrentTool();
    if (!tool) return;
    
    TPropertyGroup *props = tool->getProperties(propGroup);
    if (!props) return;
    
    for (int k = 0; k < props->getPropertyCount(); ++k) {
      TProperty *prop = props->getProperty(k);
      if (prop && prop->getName() == propName) {
        TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
        if (enumProp) {
          enumProp->setIndex(id);
          tool->onPropertyChanged(propName);
          if (m_toolHandle) m_toolHandle->notifyToolChanged();
        }
        break;
      }
    }
  });
  
  return container;
}

//-----------------------------------------------------------------------------
// Slots
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::onToolSwitched() {
  refreshProperties();
}

void ToolPropertiesPanel::onToolChanged() {
  // Check if tool type has changed (e.g., from "brush" to "mypaint")
  // This can happen when user changes brush style from standard to MyPaint
  QString currentType = detectCurrentToolType();
  if (currentType != m_currentToolType) {
    // Tool type changed, need to refresh entire properties display
    refreshProperties();
  } else {
    // Same tool type, just update property values (synchronization from ToolOptionsBar)
    updatePropertyValues();
  }
}

void ToolPropertiesPanel::updatePropertyValues() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  // Update all widgets in the properties layout
  for (int i = 0; i < m_propertiesLayout->count(); ++i) {
    QLayoutItem *item = m_propertiesLayout->itemAt(i);
    if (!item || !item->widget()) continue;
    
    QWidget *widget = item->widget();
    updateWidgetFromProperty(widget);
  }
}

void ToolPropertiesPanel::updateWidgetFromProperty(QWidget *widget) {
  if (!widget) return;
  
  // Check if widget has a property name stored
  QVariant propNameVariant = widget->property("propName");
  if (!propNameVariant.isValid()) return;
  
  std::string propName = propNameVariant.toString().toStdString();
  int propGroup = widget->property("propGroup").toInt();
  
  TTool *tool = getCurrentTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props && propGroup > 0) {
    // Fallback to group 0 if specified group doesn't exist
    props = tool->getProperties(0);
  }
  if (!props) return;
  
  // Find the property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop || prop->getName() != propName) continue;
    
    // Update widget based on property type
    
    // Check for DoublePairField (Size with min/max)
    DVGui::DoublePairField *doublePairField = widget->findChild<DVGui::DoublePairField*>();
    if (doublePairField) {
      TDoublePairProperty *doublePairProp = dynamic_cast<TDoublePairProperty*>(prop);
      if (doublePairProp) {
        std::pair<double, double> values = doublePairProp->getValue();
        doublePairField->blockSignals(true);
        doublePairField->setValues(values);
        doublePairField->blockSignals(false);
        return;
      }
    }
    
    // Check for IntPairField (Size with min/max)
    DVGui::IntPairField *intPairField = widget->findChild<DVGui::IntPairField*>();
    if (intPairField) {
      TIntPairProperty *intPairProp = dynamic_cast<TIntPairProperty*>(prop);
      if (intPairProp) {
        std::pair<int, int> values = intPairProp->getValue();
        intPairField->blockSignals(true);
        intPairField->setValues(values);
        intPairField->blockSignals(false);
        return;
      }
    }
    
    // Check for slider + line edit combo
    QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();
    QList<QSlider*> sliders = widget->findChildren<QSlider*>();
    
    if (!lineEdits.isEmpty() && !sliders.isEmpty()) {
      QLineEdit *lineEdit = lineEdits.first();
      QSlider *slider = sliders.first();
      
      // CRITICAL FIX: Check for TIntPairProperty first (Single Slider Max Only mode)
      // When m_useSingleMaxSlider is enabled, we create a simple slider for pair properties
      // that only displays/controls the MAX value
      TIntPairProperty *intPairProp = dynamic_cast<TIntPairProperty*>(prop);
      if (intPairProp) {
        std::pair<int, int> values = intPairProp->getValue();
        int maxValue = values.second;  // Use MAX value for single slider
        slider->blockSignals(true);
        lineEdit->blockSignals(true);
        slider->setValue(maxValue);
        lineEdit->setText(QString::number(maxValue));
        slider->blockSignals(false);
        lineEdit->blockSignals(false);
        return;
      }
      
      // CRITICAL FIX: Check for TDoublePairProperty (Single Slider Max Only mode)
      TDoublePairProperty *doublePairProp = dynamic_cast<TDoublePairProperty*>(prop);
      if (doublePairProp) {
        std::pair<double, double> values = doublePairProp->getValue();
        double maxValue = values.second;  // Use MAX value for single slider
        slider->blockSignals(true);
        lineEdit->blockSignals(true);
        slider->setValue(static_cast<int>(maxValue * 10));
        lineEdit->setText(QString::number(maxValue, 'f', 1));
        slider->blockSignals(false);
        lineEdit->blockSignals(false);
        return;
      }
      
      // Update int property (simple, not pair)
      TIntProperty *intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        int value = intProp->getValue();
        slider->blockSignals(true);
        lineEdit->blockSignals(true);
        slider->setValue(value);
        lineEdit->setText(QString::number(value));
        slider->blockSignals(false);
        lineEdit->blockSignals(false);
        return;
      }
      
      // Update double property (simple, not pair)
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double value = doubleProp->getValue();
        
        // Check if widget has custom value factor (for MyPaint sliders)
        int valueFactor = widget->property("valueFactor").isValid() ? 
                          widget->property("valueFactor").toInt() : 10;
        int valueDecimals = widget->property("valueDecimals").isValid() ? 
                           widget->property("valueDecimals").toInt() : 1;
        
        // Also update the value label if present
        QList<QLabel*> labels = widget->findChildren<QLabel*>();
        for (QLabel *label : labels) {
          if (label->alignment() & Qt::AlignRight) {
            label->setText(QString::number(value, 'f', valueDecimals));
            break;
          }
        }
        
        slider->blockSignals(true);
        lineEdit->blockSignals(true);
        slider->setValue(static_cast<int>(value * valueFactor));
        lineEdit->setText(QString::number(value, 'f', valueDecimals));
        slider->blockSignals(false);
        lineEdit->blockSignals(false);
        return;
      }
    }
    
    // Check if widget itself is a checkbox
    QCheckBox *checkBox = qobject_cast<QCheckBox*>(widget);
    if (checkBox) {
      TBoolProperty *boolProp = dynamic_cast<TBoolProperty*>(prop);
      if (boolProp) {
        checkBox->blockSignals(true);
        checkBox->setChecked(boolProp->getValue());
        checkBox->blockSignals(false);
        return;
      }
    }
    
    // Check for QComboBox (Snap Sensitivity, etc.)
    QComboBox *comboBox = widget->findChild<QComboBox*>();
    if (comboBox) {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        int index = enumProp->getIndex();
        comboBox->blockSignals(true);
        comboBox->setCurrentIndex(index);
        comboBox->blockSignals(false);
        return;
      }
    }
    
    // Check for collapsible enum with QButtonGroup
    QButtonGroup *buttonGroup = widget->findChild<QButtonGroup*>();
    if (buttonGroup) {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        int newIndex = enumProp->getIndex();
        
        // Update button checked state via button group
        QAbstractButton *button = buttonGroup->button(newIndex);
        if (button) {
          button->blockSignals(true);
          button->setChecked(true);
          button->blockSignals(false);
        }
        
        // Update value label
        QList<QLabel*> labels = widget->findChildren<QLabel*>();
        for (QLabel *label : labels) {
          if (label->objectName() == "valueLabel" && enumProp->getItems().size() > newIndex) {
            label->setText(enumProp->getItems()[newIndex].UIName);
            break;
          }
        }
        return;
      }
    }
    
    break;
  }
}

void ToolPropertiesPanel::onSizeChanged(int value) {
  // TODO: Implement
}

void ToolPropertiesPanel::onHardnessChanged(int value) {
  // TODO: Implement
}

void ToolPropertiesPanel::onOpacityChanged(int value) {
  // TODO: Implement
}

void ToolPropertiesPanel::onLockAlphaChanged(bool checked) {
  // TODO: Implement
}

void ToolPropertiesPanel::onPencilModeChanged(bool checked) {
  // TODO: Implement
}

void ToolPropertiesPanel::onDrawOrderChanged(int index) {
  // TODO: Implement
}

void ToolPropertiesPanel::onCapChanged(int index) {
  // TODO: Implement
}

void ToolPropertiesPanel::onJoinChanged(int index) {
  // TODO: Implement
}

void ToolPropertiesPanel::onSmoothChanged(int value) {
  // TODO: Implement
}

void ToolPropertiesPanel::onAssistantsChanged(bool checked) {
  // TODO: Implement
}

void ToolPropertiesPanel::onPressureChanged(bool checked) {
  // TODO: Implement
}

//-----------------------------------------------------------------------------
// Double/Single Slider Creation
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createDoublePairSlider(const QString &label, 
                                                  void *propPtr, 
                                                  const std::string &propName) {
  TDoublePairProperty *prop = static_cast<TDoublePairProperty*>(propPtr);
  double rangeMin = prop->getRange().first;
  double rangeMax = prop->getRange().second;
  double valueMin = prop->getValue().first;
  double valueMax = prop->getValue().second;
  
  QWidget *container = new QWidget(this);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);
  
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setMargin(0);
  layout->setSpacing(3);
  
  // Label (respect m_showLabels)
  QLabel *nameLabel = new QLabel(label, container);
  nameLabel->setVisible(m_showLabels);
  layout->addWidget(nameLabel);
  
  if (!m_useSingleMaxSlider) {
    // === NATIVE DOUBLE SLIDER (DoublePairField) - Simple original layout ===
    DVGui::DoublePairField *pairField = new DVGui::DoublePairField(container);
    pairField->setRange(rangeMin, rangeMax);
    pairField->setValues(std::make_pair(valueMin, valueMax));
    pairField->setLeftText(tr("Min"));
    pairField->setRightText(tr("Max"));
    pairField->setMinimumWidth(200);
    
    // Hide numeric fields if m_showNumericFields is false
    if (!m_showNumericFields) {
      pairField->setLabelsEnabled(false);
      QList<QLineEdit*> lineEdits = pairField->findChildren<QLineEdit*>();
      for (QLineEdit *le : lineEdits) {
        le->hide();
      }
    }
    
    layout->addWidget(pairField);
    
    // Connect to property - update property value when slider changes
    connect(pairField, &DVGui::DoublePairField::valuesChanged, 
            [this, prop, pairField, propName](bool isDragging) {
      // Get new values from the field and update property
      std::pair<double, double> newValues = pairField->getValues();
      prop->setValue(TDoublePairProperty::Value(newValues.first, newValues.second));
      
      TTool *tool = getCurrentTool();
      if (tool) {
        tool->onPropertyChanged(propName);
        if (m_toolHandle) {
          m_toolHandle->notifyToolChanged();
        }
      }
    });
    
  } else {
    // === SINGLE SLIDER (single value) - With numeric field ===
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->setMargin(0);
    sliderLayout->setSpacing(5);
    
    // Numeric field (respect m_showNumericFields)
    QLineEdit *lineEdit = new QLineEdit(container);
    lineEdit->setText(QString::number(valueMax, 'f', 1));
    lineEdit->setFixedWidth(45);
    lineEdit->setAlignment(Qt::AlignRight);
    lineEdit->setVisible(m_showNumericFields);
    QDoubleValidator *validator = new QDoubleValidator(rangeMin, rangeMax, 1, lineEdit);
    lineEdit->setValidator(validator);
    sliderLayout->addWidget(lineEdit);
    
    // Single Slider for Max
    QSlider *maxSlider = new QSlider(Qt::Horizontal, container);
    maxSlider->setMinimum(static_cast<int>(rangeMin * 10));
    maxSlider->setMaximum(static_cast<int>(rangeMax * 10));
    maxSlider->setValue(static_cast<int>(valueMax * 10));
    sliderLayout->addWidget(maxSlider, 1);
    
    layout->addLayout(sliderLayout);
    
    // Connect slider to lineEdit
    connect(maxSlider, &QSlider::valueChanged, [lineEdit](int val) {
      lineEdit->setText(QString::number(val / 10.0, 'f', 1));
    });
    
    // Connect lineEdit to slider
    connect(lineEdit, &QLineEdit::editingFinished, [maxSlider, lineEdit]() {
      maxSlider->setValue(static_cast<int>(lineEdit->text().toDouble() * 10));
    });
    
    // Connect slider to property
    connect(maxSlider, &QSlider::valueChanged, [this, prop, propName](int val) {
      double newMaxValue = val / 10.0;
      
      // Use native Pressure property to determine behavior
      // Pressure OFF → Fixed size (set both min and max)
      // Pressure ON  → Dynamic range (update max only, preserve min)
      TTool *tool = getCurrentTool();
      bool pressureDisabled = true;  // Default to fixed size
      
      if (tool) {
        TPropertyGroup *props = tool->getProperties(0);
        if (props) {
          for (int i = 0; i < props->getPropertyCount(); ++i) {
            TProperty *p = props->getProperty(i);
            if (p && (p->getName() == "Pressure" || p->getName() == "PressureSensitivity")) {
              TBoolProperty *pressureProp = dynamic_cast<TBoolProperty*>(p);
              if (pressureProp) {
                pressureDisabled = !pressureProp->getValue();
                break;
              }
            }
          }
        }
      }
      
      if (pressureDisabled) {
        // Fixed Size Mode: Set both Min and Max to the same value
        prop->setValue(TDoublePairProperty::Value(newMaxValue, newMaxValue));
      } else {
        // Dynamic Range Mode: Only update Max, preserve Min
        // Guard: if new Max < current Min, push Min down (same logic as DoublePairField)
        double currentMin = prop->getValue().first;
        double newMin = currentMin;
        if (newMaxValue < currentMin) {
          newMin = newMaxValue;  // Push Min down to match Max
        }
        prop->setValue(TDoublePairProperty::Value(newMin, newMaxValue));
      }
      
      if (tool) {
        tool->onPropertyChanged(propName);
        if (m_toolHandle) {
          m_toolHandle->notifyToolChanged();
        }
      }
    });
  }
  
  m_propertiesLayout->addWidget(container);
  m_propertiesLayout->addSpacing(10);
}

void ToolPropertiesPanel::createIntPairSlider(const QString &label, 
                                               void *propPtr, 
                                               const std::string &propName) {
  TIntPairProperty *prop = static_cast<TIntPairProperty*>(propPtr);
  int rangeMin = prop->getRange().first;
  int rangeMax = prop->getRange().second;
  int valueMin = prop->getValue().first;
  int valueMax = prop->getValue().second;
  
  QWidget *container = new QWidget(this);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);
  
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setMargin(0);
  layout->setSpacing(3);
  
  // Label (respect m_showLabels)
  QLabel *nameLabel = new QLabel(label, container);
  nameLabel->setVisible(m_showLabels);
  layout->addWidget(nameLabel);
  
  if (!m_useSingleMaxSlider) {
    // === DOUBLE CURSEUR NATIF (IntPairField) ===
    DVGui::IntPairField *pairField = new DVGui::IntPairField(container);
    pairField->setRange(rangeMin, rangeMax);
    pairField->setValues(std::make_pair(valueMin, valueMax));
    pairField->setLeftText(tr("Min:"));
    pairField->setRightText(tr("Max:"));
    pairField->setMinimumWidth(200);  // Minimum width to avoid cramping
    
    // Hide numeric fields if m_showNumericFields is false
    if (!m_showNumericFields) {
      pairField->setLabelsEnabled(false);  // Hide labels (Min:/Max:)
      // Also hide the line edit fields (find children and hide)
      QList<QLineEdit*> lineEdits = pairField->findChildren<QLineEdit*>();
      for (QLineEdit *le : lineEdits) {
        le->hide();
      }
    }
    
    layout->addWidget(pairField);
    
    // Connect to property
    connect(pairField, &DVGui::IntPairField::valuesChanged, [this, pairField, prop, propName](bool isDragging) {
      std::pair<int, int> values = pairField->getValues();
      prop->setValue(TIntPairProperty::Value(values.first, values.second));
      
      TTool *tool = getCurrentTool();
      if (tool) {
        tool->onPropertyChanged(propName);
        if (m_toolHandle) {
          m_toolHandle->notifyToolChanged();
        }
      }
    });
    
  } else {
    // === SINGLE SLIDER (single value) - With numeric field ===
    
    // Slider layout with numeric field on the left
    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->setMargin(0);
    sliderLayout->setSpacing(5);
    
    // Numeric field (QLineEdit to avoid arrows, respect m_showNumericFields)
    QLineEdit *lineEdit = new QLineEdit(container);
    lineEdit->setText(QString::number(valueMax));
    lineEdit->setFixedWidth(45);  // Match Hardness field size
    lineEdit->setAlignment(Qt::AlignRight);
    lineEdit->setVisible(m_showNumericFields);
    QIntValidator *validator = new QIntValidator(rangeMin, rangeMax, lineEdit);
    lineEdit->setValidator(validator);
    sliderLayout->addWidget(lineEdit);
    
    // Single Slider for Max (set both min and max to same value)
    QSlider *maxSlider = new QSlider(Qt::Horizontal, container);
    maxSlider->setMinimum(rangeMin);
    maxSlider->setMaximum(rangeMax);
    maxSlider->setValue(valueMax);
    sliderLayout->addWidget(maxSlider, 1);  // Stretch to take available space
    
    layout->addLayout(sliderLayout);
    
    // Connect slider to lineEdit
    connect(maxSlider, &QSlider::valueChanged, [lineEdit](int val) {
      lineEdit->setText(QString::number(val));
    });
    
    // Connect lineEdit to slider
    connect(lineEdit, &QLineEdit::editingFinished, [maxSlider, lineEdit]() {
      maxSlider->setValue(lineEdit->text().toInt());
    });
    
    // Connect slider to property
    connect(maxSlider, &QSlider::valueChanged, [this, prop, propName](int val) {
      int newMaxValue = val;
      
      // Use native Pressure property to determine behavior
      // Pressure OFF → Fixed size (set both min and max)
      // Pressure ON  → Dynamic range (update max only, preserve min)
      TTool *tool = getCurrentTool();
      bool pressureDisabled = true;  // Default to fixed size
      
      if (tool) {
        TPropertyGroup *props = tool->getProperties(0);
        if (props) {
          for (int i = 0; i < props->getPropertyCount(); ++i) {
            TProperty *p = props->getProperty(i);
            if (p && (p->getName() == "Pressure" || p->getName() == "PressureSensitivity")) {
              TBoolProperty *pressureProp = dynamic_cast<TBoolProperty*>(p);
              if (pressureProp) {
                pressureDisabled = !pressureProp->getValue();
                break;
              }
            }
          }
        }
      }
      
      if (pressureDisabled) {
        // Fixed Size Mode: Set both Min and Max to the same value
        prop->setValue(TIntPairProperty::Value(newMaxValue, newMaxValue));
      } else {
        // Dynamic Range Mode: Only update Max, preserve Min
        // Guard: if new Max < current Min, push Min down (same logic as IntPairField)
        int currentMin = prop->getValue().first;
        int newMin = currentMin;
        if (newMaxValue < currentMin) {
          newMin = newMaxValue;  // Push Min down to match Max
        }
        prop->setValue(TIntPairProperty::Value(newMin, newMaxValue));
      }
      
      if (tool) {
        tool->onPropertyChanged(propName);
        if (m_toolHandle) {
          m_toolHandle->notifyToolChanged();
        }
      }
    });
  }
  
  m_propertiesLayout->addWidget(container);
  
  // Additional spacing after Size block
  m_propertiesLayout->addSpacing(10);
}

//-----------------------------------------------------------------------------
// Context Menu (GUI Show/Hide)
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = new QMenu(this);
  
  // Add "GUI Show / Hide" menu specific to this panel
  addShowHideContextMenu(menu);
  
  // NOTE: The "Bind to Room" menu is handled by the parent DockWidget, not by the panel itself.
  // It appears when right-clicking on the panel title bar, not on its content.
  
  menu->exec(event->globalPos());
  delete menu;
}

void ToolPropertiesPanel::addShowHideContextMenu(QMenu *menu) {
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));
  
  // Action to toggle to single slider (Max only)
  QAction *singleMaxSliderAction = showHideMenu->addAction(tr("Single Slider (Max Only)"));
  singleMaxSliderAction->setCheckable(true);
  singleMaxSliderAction->setChecked(m_useSingleMaxSlider);
  singleMaxSliderAction->setObjectName("singleSlider");
  
  // Action to show/hide labels
  QAction *showLabelsAction = showHideMenu->addAction(tr("Show Labels"));
  showLabelsAction->setCheckable(true);
  showLabelsAction->setChecked(m_showLabels);
  showLabelsAction->setObjectName("showLabels");
  
  // Action to show/hide numeric fields
  QAction *showNumericAction = showHideMenu->addAction(tr("Show Numeric Fields"));
  showNumericAction->setCheckable(true);
  showNumericAction->setChecked(m_showNumericFields);
  showNumericAction->setObjectName("showNumeric");
  
  showHideMenu->addSeparator();
  
  // Action to show/hide cell borders (for collapsible menu options)
  QAction *showBordersAction = showHideMenu->addAction(tr("Cell Borders"));
  showBordersAction->setCheckable(true);
  showBordersAction->setChecked(m_showBorders);
  showBordersAction->setObjectName("showBorders");
  
  // Action to show/hide cell backgrounds (for collapsible menu options)
  QAction *showBackgroundsAction = showHideMenu->addAction(tr("Cell Backgrounds"));
  showBackgroundsAction->setCheckable(true);
  showBackgroundsAction->setChecked(m_showBackgrounds);
  showBackgroundsAction->setObjectName("showBackgrounds");
  
  // Action to show/hide Cap/Join icons
  QAction *showIconsAction = showHideMenu->addAction(tr("Show Icons"));
  showIconsAction->setCheckable(true);
  showIconsAction->setChecked(m_showIcons);
  showIconsAction->setObjectName("showIcons");
  
  connect(singleMaxSliderAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
  connect(showLabelsAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
  connect(showNumericAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
  connect(showBordersAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
  connect(showBackgroundsAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
  connect(showIconsAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
}

//-----------------------------------------------------------------------------
// Theme-aware style helpers (no hardcoded colors)
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::updateContainerStylesheet() {
  if (!m_propertiesContainer) return;
  
  // No stylesheet needed - we use custom ToolPropertyButton with paintEvent
  // Instead, update all existing ToolPropertyButton instances
  QList<ToolPropertyButton*> buttons = m_propertiesContainer->findChildren<ToolPropertyButton*>();
  for (ToolPropertyButton *btn : buttons) {
    btn->setShowBorders(m_showBorders);
    btn->setShowBackgrounds(m_showBackgrounds);
  }
}

QString ToolPropertiesPanel::getButtonStyleChecked() const {
  // Not used anymore - kept for API compatibility
  return QString();
}

QString ToolPropertiesPanel::getButtonStyleNormal(bool showBorders, bool showBackgrounds) const {
  // Not used anymore - kept for API compatibility
  return QString();
}

void ToolPropertiesPanel::onShowHideActionTriggered() {
  QAction *action = qobject_cast<QAction*>(sender());
  if (!action) return;
  
  QString actionName = action->objectName();
  bool needsRefresh = false;
  
  // Save preferences according to the action
  QSettings settings;
  
  if (actionName == "singleSlider") {
    m_useSingleMaxSlider = action->isChecked();
    settings.setValue("ToolPropertiesPanel/useSingleMaxSlider", m_useSingleMaxSlider);
    needsRefresh = true;
  } 
  else if (actionName == "showLabels") {
    m_showLabels = action->isChecked();
    settings.setValue("ToolPropertiesPanel/showLabels", m_showLabels);
    needsRefresh = true;
  } 
  else if (actionName == "showNumeric") {
    m_showNumericFields = action->isChecked();
    settings.setValue("ToolPropertiesPanel/showNumericFields", m_showNumericFields);
    needsRefresh = true;
  }
  else if (actionName == "showBorders") {
    m_showBorders = action->isChecked();
    settings.setValue("ToolPropertiesPanel/showBorders", m_showBorders);
    updateContainerStylesheet();  // Update container stylesheet immediately
    needsRefresh = true;
  }
  else if (actionName == "showBackgrounds") {
    m_showBackgrounds = action->isChecked();
    settings.setValue("ToolPropertiesPanel/showBackgrounds", m_showBackgrounds);
    updateContainerStylesheet();  // Update container stylesheet immediately
    needsRefresh = true;
  }
  else if (actionName == "showIcons") {
    m_showIcons = action->isChecked();
    settings.setValue("ToolPropertiesPanel/showIcons", m_showIcons);
    needsRefresh = true;
  }
  
  // Refresh properties to apply changes
  if (needsRefresh) {
    refreshProperties();
  }
}

