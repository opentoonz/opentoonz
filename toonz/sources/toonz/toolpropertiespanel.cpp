#include "toolpropertiespanel.h"

#include <cmath>

#include <QPointer>

#include <array>
#include <limits>

// ToonzQt includes
#include "tapp.h"
#include "toonzqt/doublepairfield.h"
#include "toonzqt/intpairfield.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"

// ToonzLib includes
#include "toonz/preferences.h"
#include "toonz/mypaintbrushstyle.h"

// TnzCore includes
#include "tenv.h"
#include "tundo.h"

// TnzBase includes
#include "tdoubleparamrelayproperty.h"

// ToonzLib includes
#include "toonz/doubleparamcmd.h"

// Tools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tproperty.h"

// tnztools — Selection tool specialized fields (requires tnztools in include path)
#include "selectiontool.h"
#include "vectorselectiontool.h"
#include "rasterselectiontool.h"
#include "tooloptionscontrols.h"
#include "rulertool.h"
#include "plastictool.h"
#include "ext/plasticskeleton.h"
#include "tools/tooloptions.h"
#include "shifttracetool.h"

#include "menubarcommandids.h"
#include "toonz/onionskinmask.h"
#include "toonz/tonionskinmaskhandle.h"

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QAction>
#include <QList>
#include <QGridLayout>
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
#include <map>
#include <set>
#include <functional>
#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QIcon>
#include <QObject>
#include <QPixmap>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QStyle>

namespace {

// Tool Properties Panel UI preferences (stored in user .env via TEnv).
TEnv::IntVar ToolPropertiesUseSingleMaxSlider("ToolPropertiesUseSingleMaxSlider",
                                              0);
TEnv::IntVar ToolPropertiesShowLabels("ToolPropertiesShowLabels", 1);
TEnv::IntVar ToolPropertiesShowNumericFields("ToolPropertiesShowNumericFields",
                                             1);
TEnv::IntVar ToolPropertiesShowBorders("ToolPropertiesShowBorders", 1);
TEnv::IntVar ToolPropertiesShowBackgrounds("ToolPropertiesShowBackgrounds", 1);
TEnv::IntVar ToolPropertiesShowIcons("ToolPropertiesShowIcons", 1);
// Serialized map: "propName=0|1" entries separated by ';' (1 = expanded).
TEnv::StringVar ToolPropertiesCollapsedStates("ToolPropertiesCollapsedStates",
                                              "");

std::map<std::string, bool> parseCollapsedStates() {
  std::map<std::string, bool> result;
  const QString stored =
      QString::fromStdString((std::string)ToolPropertiesCollapsedStates);
  const QStringList entries =
      stored.split(';', Qt::SkipEmptyParts);
  for (const QString &entry : entries) {
    const int eq = entry.indexOf('=');
    if (eq <= 0) continue;
    const std::string name = entry.left(eq).toStdString();
    result[name]           = entry.mid(eq + 1).toInt() != 0;
  }
  return result;
}

void writeCollapsedStates(const std::map<std::string, bool> &states) {
  QStringList entries;
  for (const auto &it : states) {
    entries << QString::fromStdString(it.first) + "=" +
                 QString::number(it.second ? 1 : 0);
  }
  ToolPropertiesCollapsedStates = entries.join(";").toStdString();
}

bool collapsedStateFromEnv(const std::string &propName, bool defaultExpanded) {
  const std::map<std::string, bool> states = parseCollapsedStates();
  const auto it                            = states.find(propName);
  if (it == states.end()) return defaultExpanded;
  return it->second;
}

void setCollapsedStateInEnv(const std::string &propName, bool expanded) {
  std::map<std::string, bool> states = parseCollapsedStates();
  states[propName]                   = expanded;
  writeCollapsedStates(states);
}

}  // namespace

namespace {

// Same cell fill as ToolPropertyButton in collapsible enum menus.
QColor tppCollapsibleCellBackground(const QColor &panelBg) {
  if (!panelBg.isValid()) return panelBg;
  if (panelBg.lightness() > 128) return panelBg.darker(105);
  return panelBg.lighter(108);
}

QColor tppCollapsibleCellPanelBackground(const QWidget *widget) {
  if (!widget) return QColor();
  // Use the same panel surface as collapsible pickers (toolOptionsPanel), not a
  // styled QFrame parent whose palette can read too dark under stylesheets.
  for (const QWidget *w = widget; w; w = w->parentWidget()) {
    if (w->objectName() == QLatin1String("toolOptionsPanel"))
      return w->palette().color(QPalette::Window);
  }
  QColor panelBg = widget->palette().color(QPalette::Window);
  if (widget->parentWidget())
    panelBg = widget->parentWidget()->palette().color(QPalette::Window);
  return panelBg;
}

}  // namespace

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

  const bool isHovered =
      m_hoverEnabled && opt.state.testFlag(QStyle::State_MouseOver);
  const bool isChecked = opt.state.testFlag(QStyle::State_On);
  const bool isPressed = opt.state.testFlag(QStyle::State_Sunken);
  const bool useThemeState = isHovered || isChecked || isPressed;

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Compact highlight: theme colors on a small centered pill, icon size unchanged.
  if (m_compactIconSize > 0) {
    constexpr int kCompactHighlightPad = 4;
    const int hlSize =
        m_compactIconSize + kCompactHighlightPad * 2;
    const QRect hlRect((width() - hlSize) / 2, (height() - hlSize) / 2, hlSize,
                       hlSize);

    // Full cell fill first — same layer as collapsible enum rows (Column, etc.).
    if (m_showBackgrounds) {
      painter.setPen(Qt::NoPen);
      painter.setBrush(
          tppCollapsibleCellBackground(tppCollapsibleCellPanelBackground(this)));
      painter.drawRect(rect());
    }

    if (useThemeState) {
      QStyleOptionToolButton hlOpt = opt;
      hlOpt.rect = QRect(0, 0, hlRect.width(), hlRect.height());
      painter.save();
      painter.translate(hlRect.topLeft());
      style()->drawComplexControl(QStyle::CC_ToolButton, &hlOpt, &painter, this);
      painter.restore();
    }

    style()->drawControl(QStyle::CE_ToolButtonLabel, &opt, &painter, this);

    if (m_showBorders) {
      QColor borderColor = palette().color(QPalette::Mid);
      QPen borderPen(borderColor);
      borderPen.setWidthF(0.5);
      painter.setPen(borderPen);
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    return;
  }

  if (useThemeState) {
    // Hover/checked/pressed: let QSS draw full button to match theme colors
    style()->drawComplexControl(QStyle::CC_ToolButton, &opt, &painter, this);
    return;
  }

  // Normal state: draw background based on Cells Backgrounds option
  if (m_showBackgrounds) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(
        tppCollapsibleCellBackground(tppCollapsibleCellPanelBackground(this)));
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
    , m_showIcons(true) {          // Icon grid replaces collapsible enums when possible
  
  // Load preferences from TEnv
  m_useSingleMaxSlider = ToolPropertiesUseSingleMaxSlider != 0;
  m_showLabels         = ToolPropertiesShowLabels != 0;
  m_showNumericFields  = ToolPropertiesShowNumericFields != 0;
  m_showBorders        = ToolPropertiesShowBorders != 0;
  m_showBackgrounds    = ToolPropertiesShowBackgrounds != 0;
  m_showIcons          = ToolPropertiesShowIcons != 0;

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
  m_propertiesContainer->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Preferred);
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
    connect(m_toolHandle, SIGNAL(toolComboBoxListChanged(std::string)), this,
            SLOT(onToolComboBoxListChanged(std::string)));
    connect(m_toolHandle, SIGNAL(toolOptionsBoxChanged()), this,
            SLOT(onSceneContextChanged()));
  }

  TApplication *app = TApp::instance();
  if (app) {
    connect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
            SLOT(onSceneContextChanged()));
    connect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel *)), this,
            SLOT(onSceneContextChanged()));
    connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
            SLOT(onSceneContextChanged()));
    connect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
            SLOT(onSceneContextChanged()));
  }
}

void ToolPropertiesPanel::disconnectSignals() {
  if (m_toolHandle) {
    disconnect(m_toolHandle, SIGNAL(toolSwitched()), this, SLOT(onToolSwitched()));
    disconnect(m_toolHandle, SIGNAL(toolChanged()), this, SLOT(onToolChanged()));
    disconnect(m_toolHandle, SIGNAL(toolComboBoxListChanged(std::string)), this,
               SLOT(onToolComboBoxListChanged(std::string)));
    disconnect(m_toolHandle, SIGNAL(toolOptionsBoxChanged()), this,
               SLOT(onSceneContextChanged()));
  }

  TApplication *app = TApp::instance();
  if (app) {
    disconnect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
               SLOT(onSceneContextChanged()));
    disconnect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel *)), this,
               SLOT(onSceneContextChanged()));
    disconnect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
               SLOT(onSceneContextChanged()));
    disconnect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
               SLOT(onSceneContextChanged()));
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

  if (toolId == T_ShiftTrace) return QStringLiteral("shifttrace");

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
  if (toolId == T_PaintBrush) return "paintbrush";
  if (toolId == T_Finger) return "finger";
  if (toolId == T_Type) return "type";
  if (toolId == T_EditAssistants) return "assistant";
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

QString ToolPropertiesPanel::displayNameForToolId(const QString &toolId) const {
  if (toolId == T_Brush) return tr("Brush tool");
  if (toolId == T_Fill) return tr("Fill tool");
  if (toolId == T_PaintBrush) return tr("Paint Brush tool");
  if (toolId == T_Finger) return tr("Finger tool");
  if (toolId == T_Type) return tr("Type tool");
  if (toolId == T_EditAssistants) return tr("Edit Assistants tool");
  if (toolId == T_Eraser) return tr("Eraser tool");
  if (toolId == T_Selection) return tr("Selection tool");
  if (toolId == T_Geometric) return tr("Geometric tool");
  if (toolId == T_Edit) return tr("Animate tool");
  if (toolId == T_Tape) return tr("Tape tool");
  if (toolId == T_Cutter) return tr("Cutter tool");
  if (toolId == T_StylePicker) return tr("Style Picker tool");
  if (toolId == T_RGBPicker) return tr("RGB Picker tool");
  if (toolId == T_ControlPointEditor) return tr("Control Point Editor tool");
  if (toolId == T_Pinch) return tr("Pinch tool");
  if (toolId == T_Pump) return tr("Pump tool");
  if (toolId == T_Magnet) return tr("Magnet tool");
  if (toolId == T_Bender) return tr("Bender tool");
  if (toolId == T_Iron) return tr("Iron tool");
  if (toolId == T_Skeleton) return tr("Skeleton tool");
  if (toolId == T_Tracker) return tr("Tracker tool");
  if (toolId == T_Hook) return tr("Hook tool");
  if (toolId == T_Plastic) return tr("Plastic tool");
  if (toolId == T_Zoom) return tr("Zoom tool");
  if (toolId == T_Rotate) return tr("Rotate tool");
  if (toolId == T_Hand) return tr("Hand tool");
  if (toolId == T_Ruler) return tr("Ruler tool");
  if (toolId == T_HideLine) return tr("Hide Line tool");
  if (toolId == T_ShiftTrace) return tr("Shift and Trace");
  return toolId;
}

//-----------------------------------------------------------------------------
// Properties Display
//-----------------------------------------------------------------------------

namespace {

struct LevelTypeDesc {
  int levelType;
  TTool::ToolTargetType targetType;
  const char *name;
};

const LevelTypeDesc kLevelTypes[] = {
    {PLI_XSHLEVEL, TTool::VectorImage, QT_TR_NOOP("Toonz Vector Level")},
    {TZP_XSHLEVEL, TTool::ToonzImage, QT_TR_NOOP("Toonz Raster Level")},
    {OVL_XSHLEVEL, TTool::RasterImage, QT_TR_NOOP("Raster Level")},
    {MESH_XSHLEVEL, TTool::MeshImage, QT_TR_NOOP("Mesh Level")},
    {META_XSHLEVEL, TTool::MetaImage, QT_TR_NOOP("Assistants Level")},
};

bool isPlaceholderTool(TTool *tool) {
  return tool && tool->getName() == "T_Dummy";
}

bool propertyGroupHasAdjustableProps(TPropertyGroup *props) {
  if (!props || props->getPropertyCount() == 0) return false;

  std::set<std::string> seenNames;
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;

    const std::string name = prop->getName();
    if (seenNames.count(name)) continue;
    seenNames.insert(name);

    if (auto *ep = dynamic_cast<TEnumProperty *>(prop)) {
      if (!ep->getItems().empty()) return true;
    } else if (dynamic_cast<TBoolProperty *>(prop) ||
               dynamic_cast<TIntProperty *>(prop) ||
               dynamic_cast<TDoubleProperty *>(prop) ||
               dynamic_cast<TStringProperty *>(prop) ||
               dynamic_cast<TDoublePairProperty *>(prop) ||
               dynamic_cast<TIntPairProperty *>(prop)) {
      return true;
    }
  }
  return false;
}

bool toolInstanceHasAdjustableProperties(TTool *tool) {
  if (!tool || isPlaceholderTool(tool)) return false;
  return propertyGroupHasAdjustableProps(tool->getProperties(0)) ||
         propertyGroupHasAdjustableProps(tool->getProperties(1));
}

int detectCurrentLevelType(TApplication *app) {
  if (!app) return NO_XSHLEVEL;

  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  const int columnIndex = app->getCurrentColumn()->getColumnIndex();
  const int rowIndex    = app->getCurrentFrame()->getFrame();

  TXshColumn *column =
      (columnIndex >= 0) ? xsh->getColumn(columnIndex) : nullptr;

  TXshLevel *xl       = app->getCurrentLevel()->getLevel();
  TXshSimpleLevel *sl = xl ? xl->getSimpleLevel() : nullptr;
  int levelType       = sl ? sl->getType() : NO_XSHLEVEL;

  if (levelType == NO_XSHLEVEL &&
      !app->getCurrentFrame()->isEditingLevel()) {
    if (!column || !column->getSoundColumn()) {
      TXshCell cell = xsh->getCell(rowIndex, columnIndex);
      sl            = cell.isEmpty() ? nullptr : cell.getSimpleLevel();
      levelType     = cell.isEmpty() ? NO_XSHLEVEL : cell.m_level->getType();
    }
  }

  return levelType;
}

bool isToolSupportedOnLevelType(const std::string &toolName, int levelType) {
  for (const LevelTypeDesc &desc : kLevelTypes) {
    if (levelType != desc.levelType) continue;
    TTool *tool = TTool::getTool(toolName, desc.targetType);
    return tool && !isPlaceholderTool(tool) &&
           (tool->getTargetType() & desc.targetType);
  }
  return false;
}

QStringList compatibleLevelNamesWithProperties(const std::string &toolName) {
  QStringList names;
  for (const LevelTypeDesc &desc : kLevelTypes) {
    TTool *tool = TTool::getTool(toolName, desc.targetType);
    if (!tool || isPlaceholderTool(tool)) continue;
    if (!(tool->getTargetType() & desc.targetType)) continue;
    if (!toolInstanceHasAdjustableProperties(tool)) continue;
    names << QObject::tr(desc.name);
  }
  return names;
}

QStringList compatibleLevelNamesForToolUse(const std::string &toolName) {
  QStringList names;
  for (const LevelTypeDesc &desc : kLevelTypes) {
    TTool *tool = TTool::getTool(toolName, desc.targetType);
    if (!tool || isPlaceholderTool(tool)) continue;
    if (!(tool->getTargetType() & desc.targetType)) continue;
    names << QObject::tr(desc.name);
  }
  return names;
}

QString formatLevelHintMessage(const QString &singleLineTemplate,
                               const QString &listTemplate,
                               const QStringList &levels) {
  if (levels.isEmpty()) return QString();
  if (levels.size() == 1) return singleLineTemplate.arg(levels.first());
  return listTemplate.arg(levels.join(QObject::tr(", ")));
}

bool hasNoActiveLevelContext(TApplication *app) {
  if (!app) return true;
  if (app->getCurrentFrame()->isEditingLevel()) return false;
  return detectCurrentLevelType(app) == NO_XSHLEVEL;
}

constexpr const char *kTppPropPtrKey = "tppPropPtr";

void storeTppPropertyPtr(QWidget *w, TProperty *prop) {
  if (w && prop) {
    w->setProperty(kTppPropPtrKey,
                   QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(prop)));
  }
}

TProperty *tppPropertyFromWidget(const QWidget *w) {
  if (!w) return nullptr;
  const QVariant v = w->property(kTppPropPtrKey);
  if (!v.isValid()) return nullptr;
  return reinterpret_cast<TProperty *>(v.value<quintptr>());
}

TProperty *resolveTppProperty(TTool *tool, const QWidget *w, int propGroup,
                              const std::string &propName) {
  if (TProperty *direct = tppPropertyFromWidget(w)) return direct;
  if (!tool) return nullptr;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props) return nullptr;
  return props->getProperty(propName);
}

}  // namespace

int ToolPropertiesPanel::propertyWidgetCount() const {
  if (!m_propertiesLayout) return 0;

  int count = 0;
  for (int i = 0; i < m_propertiesLayout->count(); ++i) {
    QLayoutItem *item = m_propertiesLayout->itemAt(i);
    if (item && item->widget()) ++count;
  }
  return count;
}

void ToolPropertiesPanel::showEmptyPropertiesHint(
    const QString &requestedToolId) {
  const std::string toolName = requestedToolId.toStdString();
  if (toolName.empty()) return;

  TApplication *app = TApp::instance();
  if (hasNoActiveLevelContext(app)) return;

  const QStringList propertyLevels =
      compatibleLevelNamesWithProperties(toolName);
  const QStringList usableLevels = compatibleLevelNamesForToolUse(toolName);
  const int levelType           = detectCurrentLevelType(app);
  const bool supportedOnCurrent = isToolSupportedOnLevelType(toolName, levelType);

  QString message;
  if (!usableLevels.isEmpty() && !supportedOnCurrent) {
    if (!propertyLevels.isEmpty()) {
      message = formatLevelHintMessage(
          tr("This tool's properties are only available on %1."),
          tr("This tool's properties are only available on: %1"), propertyLevels);
    } else {
      message = formatLevelHintMessage(
          tr("This tool can only be used on %1."),
          tr("This tool can only be used on: %1"), usableLevels);
    }
  } else {
    message = tr("No adjustable properties for this tool.");
  }

  QLabel *hint = new QLabel(message, m_propertiesContainer);
  hint->setAlignment(Qt::AlignCenter);
  hint->setWordWrap(true);
  QPalette pal = hint->palette();
  pal.setColor(QPalette::WindowText,
               palette().color(QPalette::Disabled, QPalette::WindowText));
  hint->setPalette(pal);
  hint->setStyleSheet("padding: 20px;");
  m_propertiesLayout->addWidget(hint);
}

void ToolPropertiesPanel::refreshProperties() {
  QString toolId = detectCurrentToolId();
  if (toolId.isEmpty()) {
    clearProperties();
    m_currentToolId   = QString();
    m_currentToolType = QString();
    m_toolNameLabel->setText(tr("Tool Properties"));
    m_propertiesLayout->addStretch(1);
    return;
  }

  QString toolType = detectCurrentToolType();

  clearProperties();

  m_currentToolId   = toolId;
  m_currentToolType = toolType;

  m_toolNameLabel->setText(displayNameForToolId(toolId));

  // Create properties based on tool type
  if (toolType == "brush") {
    createBrushProperties();
  } else if (toolType == "mypaint" || toolType == "mypainttnz") {
    createMyPaintBrushProperties();
  } else if (toolType == "fill") {
    createFillProperties();
  } else if (toolType == "eraser") {
    createEraserProperties();
  } else if (toolType == "geometric") {
    createGeometricProperties();
  } else if (toolType == "selection") {
    createSelectionProperties();

  } else if (toolType == "type") {
    createTypeProperties();

  } else if (toolType == "ruler") {
    createRulerProperties();

  } else if (toolType == "plastic") {
    createPlasticProperties();

  } else if (toolType == "edit") {
    createAnimateProperties();

  } else if (toolType == "assistant") {
    createEditAssistantsProperties();

  } else if (toolType == "shifttrace") {
    createShiftTraceProperties();

  } else {
    TTool *t = getCurrentTool();
    TPropertyGroup *g0 = t ? t->getProperties(0) : nullptr;
    TPropertyGroup *g1 = t ? t->getProperties(1) : nullptr;
    const bool sameGroup = (g0 && g1 && g0 == g1);

    createGenericProperties(0);

    if (!sameGroup && g1 && g1->getPropertyCount() > 0) {
      QFrame *sep = new QFrame(this);
      sep->setFrameShape(QFrame::HLine);
      sep->setFrameShadow(QFrame::Sunken);
      sep->setStyleSheet("color: #555; margin: 4px 0;");
      m_propertiesLayout->addWidget(sep);
      createGenericProperties(1);
    }
  }

  if (propertyWidgetCount() == 0) showEmptyPropertiesHint(toolId);

  attachAllPropertySyncListeners();

  if (toolType == QStringLiteral("shifttrace")) updateShiftTraceWidgets();

  m_propertiesLayout->addStretch(1);
}

void ToolPropertiesPanel::clearProperties() {
  // Remove all widgets from layout
  QLayoutItem *item;
  while ((item = m_propertiesLayout->takeAt(0)) != nullptr) {
    if (item->widget()) {
      QWidget *w = item->widget();
      // Ruler measurements are owned by the panel, not rebuilt each refresh.
      if (w == m_rulerOptionsBox) {
        m_rulerOptionsBox->hide();
        m_rulerOptionsBox->setParent(this);
        delete item;
        continue;
      }
      delete w;
    }
    delete item;
  }

  m_plasticModeContainer = nullptr;
  m_plasticModeLayout    = nullptr;
  m_plasticSkelPicker    = nullptr;
  m_plasticVertexWidget  = nullptr;
  m_plasticVisibleMode   = -1;

  m_animateColumnWidget   = nullptr;
  m_animateSplineRowWidgets.clear();
  m_animateXYRowWidgets.clear();
  m_animateMeasuredFields.clear();

  m_selScaleX = m_selScaleY = nullptr;
  m_selRotation             = nullptr;
  m_selMoveX = m_selMoveY  = nullptr;
  m_selThick                = nullptr;
  m_selScaleLink            = nullptr;
  m_selFlipH = m_selFlipV  = nullptr;
  m_selRotL = m_selRotR    = nullptr;
  m_selHLabel = m_selVLabel = nullptr;
  m_selXLabel = m_selYLabel = nullptr;
  m_selScaleLinkIcon        = nullptr;
  m_typeStyleWidget         = nullptr;

  m_shiftTraceGhostPicker       = nullptr;
  m_shiftTraceBBoxPicker        = nullptr;
  m_shiftTraceNoShiftChk        = nullptr;
  m_shiftTraceNoShiftIconBtn    = nullptr;
  m_shiftTraceResetShiftIconBtn = nullptr;
  m_shiftTraceResetPrevBtn      = nullptr;
  m_shiftTraceResetFollowingBtn = nullptr;

  if (TTool *tool = getCurrentTool()) {
    if (auto *plastic = dynamic_cast<PlasticTool *>(tool))
      disconnect(plastic, nullptr, this, nullptr);
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
  
  // Cap icon names are stored on TEnumProperty items (or resolved from values).
  // Search for Cap Style property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Cap:" || propName == "Cap" || propName == "CapStyle" || propName == "Cap Style:" || propName == "Cap Style") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp && enumProp->getItems().size() > 0) {
        QWidget *widget =
            createCollapsibleEnumForProperty(tr("Cap"), enumProp, propName, 1);
        if (widget) m_propertiesLayout->addWidget(widget);
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
  
  // Join icon names are stored on TEnumProperty items (or resolved from values).
  // Search for Join Style property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    
    if (propName == "Join:" || propName == "Join" || propName == "JoinStyle" || propName == "Join Style:" || propName == "Join Style") {
      TEnumProperty *enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp && enumProp->getItems().size() > 0) {
        QWidget *widget =
            createCollapsibleEnumForProperty(tr("Join"), enumProp, propName, 1);
        if (widget) m_propertiesLayout->addWidget(widget);
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
// Collapsible field metrics — shared by Plastic TPP capsules and mode options.
//-----------------------------------------------------------------------------

namespace CollapsibleStyle {
constexpr int kToggleSize         = 16;
constexpr int kHeaderMarginH      = 4;
constexpr int kHeaderMarginV      = 2;
constexpr int kHeaderSpacing      = 3;
constexpr int kHeaderRowHeight    = 24;
constexpr int kContentLeftIndent  = 25;
constexpr int kContentRightMargin = 44;  // aligns with skeleton [-][+] column
constexpr int kHeaderRightSlot    = 44;
constexpr int kFieldHeight        = 23;
constexpr int kItemSpacing        = 2;
constexpr int kBlockGap           = 6;  // vertical gap between collapsible capsules
constexpr int kSectionGap         = 20; // gap between major plastic mode blocks
constexpr int kAnimateRelayLabelW = 56; // compact aligned relay labels
constexpr int kAnimateRelayFieldW = 58; // compact aligned relay fields
}  // namespace CollapsibleStyle

namespace {

// Match ToolOptionSlider / DoubleLineEdit slider scale and TOB field layout.
constexpr int kTppDoubleSliderDecimals = 2;
constexpr int kTppDoubleSliderFactor   = 100000;

int tppNumericLineEditWidth(const QFont &font, int minIntDigits, int maxIntDigits,
                            int decimalPlaces) {
  const int textMaxLength =
      std::max(minIntDigits, maxIntDigits) + decimalPlaces + 1;
  return QFontMetrics(font).horizontalAdvance(QString(textMaxLength, QChar('0'))) +
         5;
}

void styleTppIntLineEdit(QLineEdit *lineEdit, int minValue, int maxValue) {
  lineEdit->setFixedWidth(tppNumericLineEditWidth(
      lineEdit->font(), QString::number(minValue).length(),
      QString::number(maxValue).length(), 0));
  lineEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void styleTppDoubleLineEdit(QLineEdit *lineEdit, double minValue, double maxValue,
                            int decimals = kTppDoubleSliderDecimals) {
  lineEdit->setFixedWidth(tppNumericLineEditWidth(
      lineEdit->font(), QString::number(static_cast<int>(minValue)).length(),
      QString::number(static_cast<int>(maxValue)).length(), decimals));
  lineEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

int tppDoubleValueToSliderPos(double value, int sliderMin, int sliderMax,
                              int factor, bool isLinear) {
  const int sliderValue = static_cast<int>(std::lround(value * factor));
  if (isLinear) return sliderValue;

  const double rangeSize = static_cast<double>(sliderMax - sliderMin);
  if (std::abs(rangeSize) < std::numeric_limits<double>::epsilon())
    return sliderValue;

  const double valueRatio =
      (static_cast<double>(sliderValue) - static_cast<double>(sliderMin)) /
      rangeSize;
  double t = 0.0;
  if (valueRatio <= 0.02)
    t = valueRatio / 0.04;
  else if (valueRatio <= 0.04)
    t = (valueRatio + 0.02) / 0.08;
  else if (valueRatio <= 0.1)
    t = (valueRatio + 0.26) / 0.4;
  else
    t = (valueRatio + 8.0) / 9.0;
  return sliderMin + static_cast<int>(t * rangeSize);
}

double tppSliderPosToDoubleValue(int pos, int sliderMin, int sliderMax,
                                 int factor, bool isLinear) {
  if (isLinear) return static_cast<double>(pos) / factor;

  static constexpr std::array<double, 4> thresholds{0.5, 0.75, 0.9, 1.0};
  const double rangeSize = static_cast<double>(sliderMax - sliderMin);
  if (std::abs(rangeSize) < std::numeric_limits<double>::epsilon())
    return static_cast<double>(pos) / factor;

  const double posRatio =
      static_cast<double>(pos - sliderMin) / rangeSize;
  double t = 0.0;
  if (posRatio <= thresholds[0])
    t = 0.04 * posRatio;
  else if (posRatio <= thresholds[1])
    t = -0.02 + 0.08 * posRatio;
  else if (posRatio <= thresholds[2])
    t = -0.26 + 0.4 * posRatio;
  else
    t = -8.0 + 9.0 * posRatio;

  const double sliderValue =
      std::round(static_cast<double>(sliderMin) + rangeSize * t);
  return sliderValue / factor;
}

constexpr int kTppPairSliderTrackMin = 72;
constexpr int kTppPairFieldGutter    = 8;

class TppCompactDoublePairField final : public DVGui::DoublePairField {
public:
  using DoublePairField::DoublePairField;

  int configureTppLayout(double rangeMin, double rangeMax, bool isLinear,
                           bool showNumericFields) {
    setLinearSlider(isLinear);
    setLabelsEnabled(false);

    const int widgetWidth = tppNumericLineEditWidth(
        font(), QString::number(static_cast<int>(rangeMin)).length(),
        QString::number(static_cast<int>(rangeMax)).length(),
        kTppDoubleSliderDecimals);

    if (showNumericFields) {
      m_leftLineEdit->setFixedWidth(widgetWidth);
      m_rightLineEdit->setFixedWidth(widgetWidth);
      m_leftLineEdit->show();
      m_rightLineEdit->show();
      m_leftMargin  = widgetWidth + kTppPairFieldGutter;
      m_rightMargin = widgetWidth + kTppPairFieldGutter;
    } else {
      m_leftLineEdit->hide();
      m_rightLineEdit->hide();
      m_leftMargin  = kTppPairFieldGutter + 2;
      m_rightMargin = kTppPairFieldGutter + 2;
    }

    setMinimumWidth(kTppPairSliderTrackMin + m_leftMargin + m_rightMargin);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    update();
    return widgetWidth;
  }
};

void configureTppIntPairField(DVGui::IntPairField *pairField, int rangeMin,
                              int rangeMax, bool showNumericFields) {
  if (!pairField) return;

  pairField->setLabelsEnabled(false);

  const int widgetWidth =
      tppNumericLineEditWidth(pairField->font(), QString::number(rangeMin).length(),
                              QString::number(rangeMax).length(), 0);

  for (DVGui::IntLineEdit *edit :
       pairField->findChildren<DVGui::IntLineEdit *>()) {
    edit->setFixedWidth(widgetWidth);
    edit->setVisible(showNumericFields);
  }

  const int sideReserve =
      showNumericFields ? widgetWidth + kTppPairFieldGutter : kTppPairFieldGutter + 2;
  pairField->setMinimumWidth(kTppPairSliderTrackMin + 2 * sideReserve);
  pairField->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

}  // namespace

class ToolPropertiesPanel::PropertyWidgetSync final
    : public QObject, public TProperty::Listener {
  ToolPropertiesPanel *m_panel;
  QPointer<QWidget> m_widget;
  TProperty *m_property;

public:
  PropertyWidgetSync(ToolPropertiesPanel *panel, QWidget *widget,
                     TProperty *property, QObject *parent = nullptr)
      : QObject(parent), m_panel(panel), m_widget(widget), m_property(property) {
    m_property->addListener(this);
  }

  ~PropertyWidgetSync() override {
    if (m_property) m_property->removeListener(this);
  }

  void onPropertyChanged() override {
    if (m_panel && m_widget) m_panel->updateWidgetFromProperty(m_widget);
  }
};

namespace {

void applyCollapsibleContentLayout(QVBoxLayout *contentLayout) {
  contentLayout->setMargin(0);
  contentLayout->setContentsMargins(CollapsibleStyle::kContentLeftIndent,
                                    CollapsibleStyle::kHeaderMarginV,
                                    CollapsibleStyle::kContentRightMargin,
                                    CollapsibleStyle::kHeaderMarginV);
  contentLayout->setSpacing(CollapsibleStyle::kItemSpacing);
}

void addCollapsibleHeaderRightReserve(QHBoxLayout *headerLayout, QWidget *header) {
  QWidget *reserve = new QWidget(header);
  reserve->setFixedWidth(CollapsibleStyle::kHeaderRightSlot);
  reserve->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  headerLayout->addWidget(reserve);
}

void styleCollapsibleField(QWidget *field) {
  field->setFixedHeight(CollapsibleStyle::kFieldHeight);
  field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QWidget *replacePlasticSkeletonEmptyLineEdit(QWidget *picker, bool showBorders,
                                             bool showBackgrounds) {
  if (!picker) return nullptr;
  QLineEdit *emptyField = picker->findChild<QLineEdit *>("pickerEmptyField");
  if (!emptyField) return nullptr;

  QWidget *content = emptyField->parentWidget();
  if (!content) return nullptr;
  auto *contentLayout = qobject_cast<QVBoxLayout *>(content->layout());
  if (!contentLayout) return nullptr;

  contentLayout->removeWidget(emptyField);
  delete emptyField;

  ToolPropertyButton *emptySlot = new ToolPropertyButton(QString(), content);
  emptySlot->setObjectName("plasticSkelEmptySlot");
  emptySlot->setCursor(Qt::ArrowCursor);
  emptySlot->setCheckable(false);
  emptySlot->setHoverEnabled(false);
  emptySlot->setFocusPolicy(Qt::NoFocus);
  styleCollapsibleField(emptySlot);
  emptySlot->setShowBorders(showBorders);
  emptySlot->setShowBackgrounds(showBackgrounds);
  contentLayout->insertWidget(0, emptySlot);
  return emptySlot;
}

void uniformPlasticFieldWidget(QWidget *widget) {
  if (!widget) return;
  for (QLineEdit *lineEdit : widget->findChildren<QLineEdit *>())
    styleCollapsibleField(lineEdit);
  for (ToolPropertyButton *button :
       widget->findChildren<ToolPropertyButton *>())
    styleCollapsibleField(button);
  for (QSlider *slider : widget->findChildren<QSlider *>())
    slider->setFixedHeight(CollapsibleStyle::kFieldHeight);
  for (QCheckBox *checkBox : widget->findChildren<QCheckBox *>())
    checkBox->setMinimumHeight(CollapsibleStyle::kFieldHeight);
}

void syncPlasticRelayField(MeasuredValueField *field,
                           TDoubleParamRelayProperty *relay) {
  if (!field || !relay) return;

  TDoubleParamP param = relay->getParam();
  if (!param) {
    field->setEnabled(false);
    field->blockSignals(true);
    field->setText("");
    field->blockSignals(false);
    return;
  }

  field->setEnabled(true);
  if (TMeasure *measure = param->getMeasure())
    field->setMeasure(measure->getName());

  field->blockSignals(true);
  field->setValue(relay->getValue());
  field->blockSignals(false);
}

void commitPlasticRelayField(MeasuredValueField *field, PlasticTool *plastic,
                             TDoubleParamRelayProperty *relay) {
  if (!field || !plastic || !relay) return;

  TDoubleParamP param = relay->getParam();
  if (!param) return;

  const double oldVal = relay->getValue();
  const double newVal = field->getValue();
  const double frame  = relay->frame();
  if (oldVal == newVal) return;

  struct SetValueUndo final : public TUndo {
    TDoubleParamP m_param;
    double m_oldVal, m_newVal, m_frame;

    SetValueUndo(const TDoubleParamP &param, double oldVal, double newVal,
                 double frame)
        : m_param(param)
        , m_oldVal(oldVal)
        , m_newVal(newVal)
        , m_frame(frame) {}

    int getSize() const override {
      return sizeof(SetValueUndo) + sizeof(TDoubleParam);
    }
    void undo() const override { m_param->setValue(m_frame, m_oldVal); }
    void redo() const override { m_param->setValue(m_frame, m_newVal); }
  };

  auto setKeyframe = [](TDoubleParamRelayProperty *prop) {
    if (!prop) return;
    TDoubleParam *p = prop->getParam().getPointer();
    if (!p) return;
    const double f = prop->frame();
    if (!p->isKeyframe(f)) {
      KeyframeSetter setter(p, -1, true);
      setter.createKeyframe(f);
    }
  };

  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();

  plastic->keyframePlasticRelays(relay);

  relay->setValue(newVal);
  plastic->onPropertyChanged(relay->getName());

  manager->add(new SetValueUndo(param, oldVal, newVal, frame));
  manager->endBlock();
}

MeasuredValueField *createPlasticRelayField(QWidget *parent, PlasticTool *plastic,
                                            TDoubleParamRelayProperty *relay,
                                            const QString &objectName) {
  auto *field = new MeasuredValueField(parent);
  field->setObjectName(objectName);
  field->setProperty("plasticRelay", true);
  syncPlasticRelayField(field, relay);
  QObject::connect(field, &QLineEdit::editingFinished, [field, plastic, relay]() {
    commitPlasticRelayField(field, plastic, relay);
  });
  return field;
}

QLineEdit *createPlasticStringField(QWidget *parent, TTool *tool,
                                    TStringProperty *prop, int propGroup) {
  auto *lineEdit = new QLineEdit(parent);
  lineEdit->setText(QString::fromStdWString(prop->getValue()));
  lineEdit->setProperty("propName", QString::fromStdString(prop->getName()));
  lineEdit->setProperty("propGroup", propGroup);
  styleCollapsibleField(lineEdit);

  QObject::connect(lineEdit, &QLineEdit::editingFinished, [tool, lineEdit]() {
    if (!tool) return;
    const std::string name =
        lineEdit->property("propName").toString().toStdString();
    const int group = lineEdit->property("propGroup").toInt();
    TPropertyGroup *props = tool->getProperties(group);
    if (!props) return;
    if (auto *sp = dynamic_cast<TStringProperty *>(props->getProperty(name))) {
      sp->setValue(lineEdit->text().toStdWString());
      tool->onPropertyChanged(name);
      sp->notifyListeners();
    }
  });
  return lineEdit;
}

void initCollapsibleHeader(QHBoxLayout *headerLayout) {
  headerLayout->setContentsMargins(CollapsibleStyle::kHeaderMarginH,
                                   CollapsibleStyle::kHeaderMarginV,
                                   CollapsibleStyle::kHeaderMarginH,
                                   CollapsibleStyle::kHeaderMarginV);
  headerLayout->setSpacing(CollapsibleStyle::kHeaderSpacing);
}

void connectClickableLabel(ClickableLabel *label, MeasuredValueField *field) {
  QObject::connect(label, SIGNAL(onMousePress(QMouseEvent *)), field,
                   SLOT(receiveMousePress(QMouseEvent *)));
  QObject::connect(label, SIGNAL(onMouseMove(QMouseEvent *)), field,
                   SLOT(receiveMouseMove(QMouseEvent *)));
  QObject::connect(label, SIGNAL(onMouseRelease(QMouseEvent *)), field,
                   SLOT(receiveMouseRelease(QMouseEvent *)));
}

}  // namespace

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
  styleTppIntLineEdit(lineEdit, min, max);
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
  connect(slider, &QSlider::valueChanged, [this, container](int val) {
    TTool *tool = getCurrentTool();
    if (!tool) return;

    const std::string propName =
        container->property("propName").toString().toStdString();
    const int propGroup = container->property("propGroup").toInt();

    TProperty *prop = resolveTppProperty(tool, container, propGroup, propName);
    if (auto *intProp = dynamic_cast<TIntProperty *>(prop)) {
      intProp->setValue(val);
      tool->onPropertyChanged(propName, true);
      intProp->notifyListeners();
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
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
  container->setProperty("valueFactor", kTppDoubleSliderFactor);
  container->setProperty("valueDecimals", kTppDoubleSliderDecimals);
  
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
  
  const int sliderMin = static_cast<int>(std::lround(min * kTppDoubleSliderFactor));
  const int sliderMax = static_cast<int>(std::lround(max * kTppDoubleSliderFactor));
  const int sliderVal = static_cast<int>(std::lround(value * kTppDoubleSliderFactor));

  // Numeric field (QLineEdit to avoid arrows, respect m_showNumericFields)
  QLineEdit *lineEdit = new QLineEdit(container);
  lineEdit->setText(QString::number(value, 'f', kTppDoubleSliderDecimals));
  styleTppDoubleLineEdit(lineEdit, min, max);
  lineEdit->setVisible(m_showNumericFields);
  QDoubleValidator *validator =
      new QDoubleValidator(min, max, kTppDoubleSliderDecimals, lineEdit);
  lineEdit->setValidator(validator);
  sliderLayout->addWidget(lineEdit);
  
  // Slider (scale matches ToolOptionSlider / DoubleLineEdit)
  QSlider *slider = new QSlider(Qt::Horizontal, container);
  slider->setMinimum(sliderMin);
  slider->setMaximum(sliderMax);
  slider->setValue(sliderVal);
  sliderLayout->addWidget(slider, 1);  // Stretch to take available space
  
  layout->addLayout(sliderLayout);
  
  // Connect slider and lineEdit together
  connect(slider, &QSlider::valueChanged, [lineEdit](int val) {
    lineEdit->setText(QString::number(
        static_cast<double>(val) / kTppDoubleSliderFactor, 'f',
        kTppDoubleSliderDecimals));
  });
  connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
    slider->setValue(static_cast<int>(std::lround(
        lineEdit->text().toDouble() * kTppDoubleSliderFactor)));
  });
  
  // Connect to tool property
  connect(slider, &QSlider::valueChanged, [this, container](int val) {
    const double newValue =
        static_cast<double>(val) / kTppDoubleSliderFactor;

    TTool *tool = getCurrentTool();
    if (!tool) return;

    const std::string propName =
        container->property("propName").toString().toStdString();
    const int propGroup = container->property("propGroup").toInt();

    TProperty *prop = resolveTppProperty(tool, container, propGroup, propName);
    if (auto *doubleProp = dynamic_cast<TDoubleProperty *>(prop)) {
      doubleProp->setValue(newValue);
      tool->onPropertyChanged(propName, true);
      doubleProp->notifyListeners();
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
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
  connect(checkBox, &QCheckBox::toggled, [this, checkBox](bool checked) {
    TTool *tool = getCurrentTool();
    if (!tool) return;

    const std::string propName =
        checkBox->property("propName").toString().toStdString();
    const int propGroup = checkBox->property("propGroup").toInt();

    TProperty *prop = resolveTppProperty(tool, checkBox, propGroup, propName);
    if (auto *boolProp = dynamic_cast<TBoolProperty *>(prop)) {
      boolProp->setValue(checked);
      tool->onPropertyChanged(propName, true);
      boolProp->notifyListeners();
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
    }
  });
  
  return checkBox;
}

QWidget *ToolPropertiesPanel::createTextProperty(const QString &label,
                                                 TStringProperty *prop,
                                                 const std::string &propName,
                                                 int propGroup) {
  QWidget *container = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  QLabel *nameLabel = new QLabel(label, container);
  layout->addWidget(nameLabel);

  QLineEdit *lineEdit = new QLineEdit(container);
  lineEdit->setText(QString::fromStdWString(prop->getValue()));
  lineEdit->setProperty("propName", QString::fromStdString(propName));
  lineEdit->setProperty("propGroup", propGroup);
  storeTppPropertyPtr(container, prop);
  layout->addWidget(lineEdit);

  connect(lineEdit, &QLineEdit::editingFinished, [this, container, lineEdit]() {
    TTool *tool = getCurrentTool();
    if (!tool) return;

    const std::string name =
        lineEdit->property("propName").toString().toStdString();
    const int group = lineEdit->property("propGroup").toInt();
    TProperty *p = resolveTppProperty(tool, container, group, name);
    if (auto *sp = dynamic_cast<TStringProperty *>(p)) {
      sp->setValue(lineEdit->text().toStdWString());
      tool->onPropertyChanged(name, true);
      sp->notifyListeners();
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
    }
  });

  return container;
}

QWidget* ToolPropertiesPanel::createCollapsibleEnum(const QString &label,
                                                     const QStringList &items, 
                                                     int currentIndex, 
                                                     const std::string &propName,
                                                     const QString &iconName,
                                                     const std::function<void(int)> &onChanged,
                                                     bool reserveHeaderRightSlot,
                                                     QWidget *parentWidget) {
  QWidget *container = new QWidget(parentWidget ? parentWidget
                                                : static_cast<QWidget *>(this));
  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(CollapsibleStyle::kItemSpacing);
  
  // Header with toggle button
  QWidget *header = new QWidget(container);
  header->setFixedHeight(CollapsibleStyle::kHeaderRowHeight);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  initCollapsibleHeader(headerLayout);
  
  // Toggle button (triangle)
  QToolButton *toggleButton = new QToolButton(header);
  toggleButton->setArrowType(Qt::RightArrow);
  toggleButton->setCheckable(true);
  toggleButton->setStyleSheet("QToolButton { border: none; }");
  toggleButton->setFixedSize(CollapsibleStyle::kToggleSize,
                             CollapsibleStyle::kToggleSize);
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
  if (reserveHeaderRightSlot) addCollapsibleHeaderRightReserve(headerLayout, header);
  
  mainLayout->addWidget(header);
  
  // Content (checkable buttons in a group)
  QWidget *content = new QWidget(container);
  
  // Load saved collapse state from TEnv (persists between sessions)
  bool isExpanded =
      collapsedStateFromEnv(propName, false);  // Default: collapsed
  content->setVisible(isExpanded);
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);
  
  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  applyCollapsibleContentLayout(contentLayout);
  
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
    styleCollapsibleField(optionButton);
    
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
    
    // Save collapsed state to TEnv for persistence
    setCollapsedStateInEnv(propName, checked);
  });
  
  // Store propName in container for later use
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", 0);  // Default to group 0
  
  // Connect button group to property update
  connect(buttonGroup, &QButtonGroup::idClicked,
          [this, valueLabel, items, container, onChanged](int id) {
    // Update value label
    valueLabel->setText(items.value(id, ""));

    // Update tool property
    std::string propName = container->property("propName").toString().toStdString();
    int propGroup = container->property("propGroup").toInt();

    TTool *tool = getCurrentTool();
    if (!tool) return;

    TProperty *prop = resolveTppProperty(tool, container, propGroup, propName);
    if (auto *enumProp = dynamic_cast<TEnumProperty *>(prop)) {
      enumProp->setIndex(id);
      tool->onPropertyChanged(propName, true);
      enumProp->notifyListeners();
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
      if (tool->getName() == T_EditAssistants && propName == "AssistantType")
        refreshProperties();
    }

    if (onChanged) onChanged(id);
  });
  
  return container;
}

QWidget *ToolPropertiesPanel::createCollapsibleIntSlider(
    const QString &label, int min, int max, int value,
    const std::string &propName, int propGroup,
    const std::string &storageKey, bool reserveHeaderRightSlot) {
  QWidget *container = new QWidget(m_propertiesContainer);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", propGroup);
  container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(CollapsibleStyle::kItemSpacing);

  QWidget *header = new QWidget(container);
  header->setFixedHeight(CollapsibleStyle::kHeaderRowHeight);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  initCollapsibleHeader(headerLayout);

  QToolButton *toggleButton = new QToolButton(header);
  toggleButton->setArrowType(Qt::RightArrow);
  toggleButton->setCheckable(true);
  toggleButton->setStyleSheet("QToolButton { border: none; }");
  toggleButton->setFixedSize(CollapsibleStyle::kToggleSize,
                             CollapsibleStyle::kToggleSize);
  headerLayout->addWidget(toggleButton);

  QLabel *nameLabel = new QLabel(label, header);
  nameLabel->setVisible(m_showLabels);
  headerLayout->addWidget(nameLabel);

  QLabel *valueLabel = new QLabel(QString::number(value), header);
  valueLabel->setObjectName("intSliderValueLabel");
  QFont italicFont = valueLabel->font();
  italicFont.setItalic(true);
  valueLabel->setFont(italicFont);
  QPalette pal = valueLabel->palette();
  pal.setColor(QPalette::WindowText,
               palette().color(QPalette::Disabled, QPalette::WindowText));
  valueLabel->setPalette(pal);
  headerLayout->addStretch();
  headerLayout->addWidget(valueLabel);
  if (reserveHeaderRightSlot) addCollapsibleHeaderRightReserve(headerLayout, header);
  mainLayout->addWidget(header);

  QWidget *content = new QWidget(container);
  const bool isExpanded = collapsedStateFromEnv(storageKey, false);
  content->setVisible(isExpanded);
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);

  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  applyCollapsibleContentLayout(contentLayout);

  QHBoxLayout *sliderLayout = new QHBoxLayout();
  sliderLayout->setMargin(0);
  sliderLayout->setSpacing(CollapsibleStyle::kHeaderSpacing);

  QLineEdit *lineEdit = new QLineEdit(content);
  lineEdit->setText(QString::number(value));
  styleTppIntLineEdit(lineEdit, min, max);
  lineEdit->setVisible(m_showNumericFields);
  lineEdit->setValidator(new QIntValidator(min, max, lineEdit));
  styleCollapsibleField(lineEdit);

  QSlider *slider = new QSlider(Qt::Horizontal, content);
  slider->setMinimum(min);
  slider->setMaximum(max);
  slider->setValue(value);
  slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  slider->setFixedHeight(CollapsibleStyle::kFieldHeight);

  sliderLayout->addWidget(lineEdit);
  sliderLayout->addWidget(slider, 1);
  contentLayout->addLayout(sliderLayout);
  mainLayout->addWidget(content);

  QObject::connect(toggleButton, &QToolButton::toggled,
                   [content, toggleButton, storageKey](bool checked) {
                     content->setVisible(checked);
                     toggleButton->setArrowType(checked ? Qt::DownArrow
                                                        : Qt::RightArrow);
                     setCollapsedStateInEnv(storageKey, checked);
                   });

  QObject::connect(slider, &QSlider::valueChanged, [lineEdit](int val) {
    lineEdit->setText(QString::number(val));
  });
  QObject::connect(lineEdit, &QLineEdit::editingFinished, [slider, lineEdit]() {
    slider->setValue(lineEdit->text().toInt());
  });

  auto applyValue = [this, container, valueLabel](int val) {
    valueLabel->setText(QString::number(val));

    TTool *tool = getCurrentTool();
    if (!tool) return;

    const std::string name =
        container->property("propName").toString().toStdString();
    const int group = container->property("propGroup").toInt();
    TProperty *prop = resolveTppProperty(tool, container, group, name);
    if (auto *intProp = dynamic_cast<TIntProperty *>(prop)) {
      intProp->setValue(val);
      tool->onPropertyChanged(name, true);
      intProp->notifyListeners();
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
    }
  };

  QObject::connect(slider, &QSlider::valueChanged, applyValue);
  QObject::connect(lineEdit, &QLineEdit::editingFinished,
                   [slider, lineEdit, applyValue]() {
                     applyValue(slider->value());
                   });

  return container;
}

QWidget *ToolPropertiesPanel::createCollapsiblePicker(
    const QString &label, const QStringList &items, int currentIndex,
    const QString &storageKey, const std::function<void(int)> &onChanged) {
  QWidget *container = new QWidget(this);
  container->setProperty("pickerStorageKey", storageKey);

  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(CollapsibleStyle::kItemSpacing);

  QWidget *header = new QWidget(container);
  header->setFixedHeight(CollapsibleStyle::kHeaderRowHeight);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  initCollapsibleHeader(headerLayout);

  QToolButton *toggleButton = new QToolButton(header);
  toggleButton->setArrowType(Qt::RightArrow);
  toggleButton->setCheckable(true);
  toggleButton->setStyleSheet("QToolButton { border: none; }");
  toggleButton->setFixedSize(CollapsibleStyle::kToggleSize,
                             CollapsibleStyle::kToggleSize);
  headerLayout->addWidget(toggleButton);

  QLabel *nameLabel = new QLabel(label, header);
  nameLabel->setVisible(m_showLabels);
  headerLayout->addWidget(nameLabel);

  QLabel *valueLabel = new QLabel(items.value(currentIndex, ""), header);
  valueLabel->setObjectName("pickerValueLabel");
  QFont italicFont = valueLabel->font();
  italicFont.setItalic(true);
  valueLabel->setFont(italicFont);
  QPalette pal = valueLabel->palette();
  pal.setColor(QPalette::WindowText,
               palette().color(QPalette::Disabled, QPalette::WindowText));
  valueLabel->setPalette(pal);
  headerLayout->addStretch();
  headerLayout->addWidget(valueLabel);
  mainLayout->addWidget(header);

  QWidget *content = new QWidget(container);
  const bool isExpanded =
      collapsedStateFromEnv(storageKey.toStdString(), false);
  content->setVisible(isExpanded);
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);

  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  applyCollapsibleContentLayout(contentLayout);

  QButtonGroup *buttonGroup = new QButtonGroup(content);
  buttonGroup->setExclusive(true);
  buttonGroup->setObjectName("pickerButtonGroup");

  if (items.isEmpty()) {
    QLineEdit *emptyField = new QLineEdit(content);
    emptyField->setObjectName("pickerEmptyField");
    emptyField->setReadOnly(true);
    styleCollapsibleField(emptyField);
    contentLayout->addWidget(emptyField);
  } else {
    for (int i = 0; i < items.size(); ++i) {
      ToolPropertyButton *optionButton =
          new ToolPropertyButton(items[i], content);
      optionButton->setCursor(Qt::PointingHandCursor);
      optionButton->setCheckable(true);
      optionButton->setAutoExclusive(true);
      styleCollapsibleField(optionButton);
      optionButton->setShowBorders(m_showBorders);
      optionButton->setShowBackgrounds(m_showBackgrounds);
      if (i == currentIndex) optionButton->setChecked(true);
      buttonGroup->addButton(optionButton, i);
      contentLayout->addWidget(optionButton);
    }
  }

  mainLayout->addWidget(content);

  QObject::connect(toggleButton, &QToolButton::toggled,
                   [content, toggleButton, storageKey](bool checked) {
                     content->setVisible(checked);
                     toggleButton->setArrowType(checked ? Qt::DownArrow
                                                        : Qt::RightArrow);
                     setCollapsedStateInEnv(storageKey.toStdString(), checked);
                   });

  QObject::connect(buttonGroup, &QButtonGroup::idClicked,
                   [valueLabel, items, onChanged](int id) {
                     valueLabel->setText(items.value(id, ""));
                     if (onChanged) onChanged(id);
                   });

  container->setProperty("pickerCurrentIndex", currentIndex);
  return container;
}

QWidget *ToolPropertiesPanel::createCollapsibleTextField(
    const QString &label, const QString &textValue,
    const std::string &storageKey, const std::string &propName, int propGroup,
    const std::function<void(const QString &)> &onChanged,
    bool reserveHeaderRightSlot) {
  QWidget *container = new QWidget(m_propertiesContainer);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", propGroup);

  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(CollapsibleStyle::kItemSpacing);

  QWidget *header = new QWidget(container);
  header->setFixedHeight(CollapsibleStyle::kHeaderRowHeight);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  initCollapsibleHeader(headerLayout);

  QToolButton *toggleButton = new QToolButton(header);
  toggleButton->setArrowType(Qt::RightArrow);
  toggleButton->setCheckable(true);
  toggleButton->setStyleSheet("QToolButton { border: none; }");
  toggleButton->setFixedSize(CollapsibleStyle::kToggleSize,
                             CollapsibleStyle::kToggleSize);
  headerLayout->addWidget(toggleButton);

  QLabel *nameLabel = new QLabel(label, header);
  nameLabel->setVisible(m_showLabels);
  headerLayout->addWidget(nameLabel);

  QLabel *valueLabel = new QLabel(textValue, header);
  valueLabel->setObjectName("textValueLabel");
  QFont italicFont = valueLabel->font();
  italicFont.setItalic(true);
  valueLabel->setFont(italicFont);
  QPalette pal = valueLabel->palette();
  pal.setColor(QPalette::WindowText,
               palette().color(QPalette::Disabled, QPalette::WindowText));
  valueLabel->setPalette(pal);
  headerLayout->addStretch();
  headerLayout->addWidget(valueLabel);
  if (reserveHeaderRightSlot) addCollapsibleHeaderRightReserve(headerLayout, header);
  mainLayout->addWidget(header);

  QWidget *content = new QWidget(container);
  const bool isExpanded = collapsedStateFromEnv(storageKey, false);
  content->setVisible(isExpanded);
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);

  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  applyCollapsibleContentLayout(contentLayout);

  QLineEdit *lineEdit = new QLineEdit(content);
  lineEdit->setObjectName("collapsibleTextEdit");
  lineEdit->setText(textValue);
  styleCollapsibleField(lineEdit);
  contentLayout->addWidget(lineEdit);
  mainLayout->addWidget(content);

  QObject::connect(toggleButton, &QToolButton::toggled,
                   [content, toggleButton, storageKey](bool checked) {
                     content->setVisible(checked);
                     toggleButton->setArrowType(checked ? Qt::DownArrow
                                                        : Qt::RightArrow);
                     setCollapsedStateInEnv(storageKey, checked);
                   });

  QObject::connect(lineEdit, &QLineEdit::editingFinished, [lineEdit, valueLabel, onChanged]() {
    valueLabel->setText(lineEdit->text());
    if (onChanged) onChanged(lineEdit->text());
  });

  return container;
}

QWidget *ToolPropertiesPanel::createCollapsibleSection(
    const QString &label, const std::string &storageKey, QWidget *contentWidget,
    const QString &headerValue, const QString &iconName) {
  QWidget *container = new QWidget(m_propertiesContainer);
  container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  QVBoxLayout *mainLayout = new QVBoxLayout(container);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(CollapsibleStyle::kItemSpacing);

  QWidget *header = new QWidget(container);
  header->setFixedHeight(CollapsibleStyle::kHeaderRowHeight);
  QHBoxLayout *headerLayout = new QHBoxLayout(header);
  initCollapsibleHeader(headerLayout);

  QToolButton *toggleButton = new QToolButton(header);
  toggleButton->setArrowType(Qt::RightArrow);
  toggleButton->setCheckable(true);
  toggleButton->setStyleSheet("QToolButton { border: none; }");
  toggleButton->setFixedSize(CollapsibleStyle::kToggleSize,
                             CollapsibleStyle::kToggleSize);
  headerLayout->addWidget(toggleButton);

  if (!iconName.isEmpty()) {
    QLabel *iconLabel = new QLabel(header);
    QIcon qi = createQIcon(iconName);
    if (!qi.isNull()) iconLabel->setPixmap(qi.pixmap(16, 16));
    headerLayout->addWidget(iconLabel);
  }

  QLabel *nameLabel = new QLabel(label, header);
  nameLabel->setVisible(m_showLabels);
  headerLayout->addWidget(nameLabel);

  if (!headerValue.isEmpty()) {
    QLabel *valueLabel = new QLabel(headerValue, header);
    QFont italicFont = valueLabel->font();
    italicFont.setItalic(true);
    valueLabel->setFont(italicFont);
    QPalette pal = valueLabel->palette();
    pal.setColor(QPalette::WindowText,
                 palette().color(QPalette::Disabled, QPalette::WindowText));
    valueLabel->setPalette(pal);
    headerLayout->addStretch();
    headerLayout->addWidget(valueLabel);
  } else {
    headerLayout->addStretch();
  }

  mainLayout->addWidget(header);

  QWidget *content = new QWidget(container);
  const bool isExpanded = collapsedStateFromEnv(storageKey, false);
  content->setVisible(isExpanded);
  toggleButton->setChecked(isExpanded);
  toggleButton->setArrowType(isExpanded ? Qt::DownArrow : Qt::RightArrow);

  QVBoxLayout *contentLayout = new QVBoxLayout(content);
  applyCollapsibleContentLayout(contentLayout);
  if (contentWidget) {
    contentWidget->setParent(content);
    const QSizePolicy::Policy hPolicy =
        contentWidget->sizePolicy().horizontalPolicy();
    if (hPolicy == QSizePolicy::Expanding ||
        hPolicy == QSizePolicy::MinimumExpanding)
      contentLayout->addWidget(contentWidget, 1);
    else
      contentLayout->addWidget(contentWidget, 0, Qt::AlignLeft);
  }
  mainLayout->addWidget(content);

  QObject::connect(toggleButton, &QToolButton::toggled,
                   [content, toggleButton, storageKey](bool checked) {
                     content->setVisible(checked);
                     toggleButton->setArrowType(checked ? Qt::DownArrow
                                                        : Qt::RightArrow);
                     setCollapsedStateInEnv(storageKey, checked);
                   });

  return container;
}

void ToolPropertiesPanel::syncCollapsiblePicker(QWidget *container, int index) {
  if (!container || index < 0) return;

  if (QLabel *valueLabel = container->findChild<QLabel *>("pickerValueLabel")) {
    if (auto *group = container->findChild<QButtonGroup *>("pickerButtonGroup")) {
      if (QAbstractButton *btn = group->button(index)) {
        valueLabel->setText(btn->text());
        btn->setChecked(true);
        container->setProperty("pickerCurrentIndex", index);
      }
    }
  }
}

QWidget *ToolPropertiesPanel::createAnimateColumnPicker(TXsheetHandle *xshHandle,
                                                        TObjectHandle *objHandle) {
  TXsheet *xsh = xshHandle->getXsheet();
  QStringList items;
  QVariantList codes;

  for (int i = 0; i < xsh->getStageObjectTree()->getStageObjectCount(); ++i) {
    TStageObjectId id = xsh->getStageObjectTree()->getStageObject(i)->getId();
    if (id.isColumn()) {
      int columnIndex = id.getIndex();
      if (xsh->isColumnEmpty(columnIndex)) continue;
    }

    TStageObject *pegbar = xsh->getStageObject(id);
    QString itemName     = id.isTable()
                               ? tr("Table")
                               : QString::fromStdString(pegbar->getName());
    items << itemName;
    codes << QVariant((int)id.getCode());
  }

  TStageObjectId curObjId = objHandle->getObjectId();
  int currentIndex        = -1;
  for (int i = 0; i < codes.size(); ++i) {
    if (codes[i].toInt() == (int)curObjId.getCode()) {
      currentIndex = i;
      break;
    }
  }
  if (currentIndex < 0) {
    TStageObject *pegbar = xsh->getStageObject(curObjId);
    items << QString::fromStdString(pegbar->getName());
    codes << QVariant((int)curObjId.getCode());
    currentIndex = items.size() - 1;
  }

  QWidget *picker = createCollapsiblePicker(
      tr("Column"), items, currentIndex, "animate_column",
      [objHandle, xshHandle, codes](int index) {
        if (index < 0 || index >= codes.size()) return;
        TStageObjectId id;
        id.setCode(codes[index].toInt());
        if (id == TStageObjectId::NoneId) return;
        if (id.isCamera()) {
          TXsheet *xsh = xshHandle->getXsheet();
          if (xsh->getCameraColumnIndex() != id.getIndex())
            xshHandle->changeXsheetCamera(id.getIndex());
        }
        objHandle->setObjectId(id);
      });
  picker->setProperty("pickerCodes", codes);
  return picker;
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
  
  // Load collapsed state from TEnv
  bool isExpanded = collapsedStateFromEnv(propName, true);
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
    
    setCollapsedStateInEnv(propName, checked);
  });
  
  // Connect button group to property update
  connect(buttonGroup, &QButtonGroup::idClicked,
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

namespace {

// Enum icon grid — 3-column framed panel (replaces collapsible enum when Show Icons is on).
constexpr int kEnumIconGridCols        = 3;
constexpr int kEnumIconGridBtnSize     = 40;
constexpr int kEnumIconGridIconSize    = 20;
constexpr int kEnumIconGridFrameRadius = 6;
constexpr int kEnumIconGridFramePad    = 8;
constexpr int kEnumIconGridSpacingH    = 10;
constexpr int kEnumIconGridSpacingV    = 8;
constexpr int kEnumIconGridLabelGap    = 3;  // air below label descenders (g, p, q)

void applyEnumIconGridFrameStyle(QFrame *frame, bool showBorder,
                                 bool showBackground, const QPalette &pal) {
  Q_UNUSED(showBackground);
  frame->setFrameShape(QFrame::NoFrame);
  QStringList rules;
  // Cell backgrounds are drawn per icon button (same as collapsible enum cells).
  rules << QStringLiteral("background-color: transparent");
  if (showBorder) {
    rules << QStringLiteral("border: 1px solid %1")
                 .arg(pal.color(QPalette::Mid).name());
  } else {
    rules << QStringLiteral("border: none");
  }
  rules << QStringLiteral("border-radius: %1px").arg(kEnumIconGridFrameRadius);
  frame->setStyleSheet(QStringLiteral("QFrame#tppEnumIconGridFrame { %1 }")
                           .arg(rules.join("; ")));
}

// Resolve SVG icon name for enum items that do not carry iconName in the tool source.
QString resolveEnumItemIcon(TTool *tool, const std::string &propName,
                            const std::wstring &value) {
  const QString v  = QString::fromStdWString(value);
  const QString pn = QString::fromStdString(propName);

  if (v.endsWith(QStringLiteral("_cap"), Qt::CaseInsensitive) ||
      v.endsWith(QStringLiteral("_join"), Qt::CaseInsensitive))
    return v;

  if (!tool) return QString();
  const QString tid = QString::fromStdString(tool->getName());

  if (pn == QLatin1String("Shape:") || pn == QLatin1String("Shape")) {
    if (v == QLatin1String("Rectangle"))
      return QStringLiteral("geometric_rectangle");
    if (v == QLatin1String("Circle")) return QStringLiteral("geometric_circle");
    if (v == QLatin1String("Ellipse"))
      return QStringLiteral("geometric_ellipse");
    if (v == QLatin1String("Line")) return QStringLiteral("geometric_line");
    if (v == QLatin1String("Polyline"))
      return QStringLiteral("geometric_polyline");
    if (v == QLatin1String("Arc")) return QStringLiteral("geometric_arc");
    if (v == QLatin1String("MultiArc"))
      return QStringLiteral("geometric_multiarc");
    if (v == QLatin1String("Polygon"))
      return QStringLiteral("geometric_polygon");
    return QString();
  }

  if (pn == QLatin1String("Type:") || pn == QLatin1String("Type")) {
    QString prefix;
    if (tid == QLatin1String(T_Fill))
      prefix = QStringLiteral("fill_");
    else if (tid == QLatin1String(T_Eraser))
      prefix = QStringLiteral("eraser_");
    else if (tid == QLatin1String(T_Tape))
      prefix = QStringLiteral("tape_");
    else if (tid == QLatin1String(T_Selection))
      prefix = QStringLiteral("selection_");

    if (prefix.isEmpty()) return QString();

    if (v == QLatin1String("Normal")) return prefix + QStringLiteral("normal");
    if (v == QLatin1String("Rectangular"))
      return prefix + QStringLiteral("rectangular");
    if (v == QLatin1String("Freehand"))
      return prefix + QStringLiteral("freehand");
    if (v == QLatin1String("Polyline"))
      return prefix + QStringLiteral("polyline");
    if (v == QLatin1String("Freepick"))
      return prefix + QStringLiteral("freepick");
    if (v == QLatin1String("Segment"))
      return prefix + QStringLiteral("segment");
    return QString();
  }

  if (pn == QLatin1String("Mode:") || pn == QLatin1String("Mode")) {
    if (tid == QLatin1String(T_Fill) || tid == QLatin1String(T_Eraser) ||
        tid == QLatin1String(T_PaintBrush) || tid == QLatin1String(T_Finger) ||
        tid == QLatin1String(T_StylePicker)) {
      if (v == QLatin1String("Lines"))
        return QStringLiteral("fill_mode_lines");
      if (v == QLatin1String("Areas"))
        return QStringLiteral("fill_mode_areas");
      if (v == QLatin1String("Lines & Areas"))
        return QStringLiteral("fill_mode_lines_areas");
    }
    if (tid == QLatin1String(T_Tape)) {
      if (v == QLatin1String("Endpoint to Endpoint"))
        return QStringLiteral("tape_end_to_end");
      if (v == QLatin1String("Endpoint to Line"))
        return QStringLiteral("tape_end_to_line");
      if (v == QLatin1String("Line to Line"))
        return QStringLiteral("tape_line_to_line");
    }
    if (tid == QLatin1String(T_Selection) && v == QLatin1String("Standard"))
      return QStringLiteral("selection_normal");
  }

  return QString();
}

QStringList collectEnumItemIcons(TTool *tool, TEnumProperty *enumProp) {
  QStringList icons;
  if (!enumProp) return icons;

  const TEnumProperty::Items &items = enumProp->getItems();
  const TEnumProperty::Range &range = enumProp->getRange();
  const int count = std::min((int)items.size(), (int)range.size());
  icons.reserve(count);
  for (int i = 0; i < count; ++i) {
    if (!items[i].iconName.isEmpty())
      icons << items[i].iconName;
    else
      icons << resolveEnumItemIcon(tool, enumProp->getName(), range[i]);
  }
  return icons;
}

bool enumIconsComplete(const QStringList &icons, int itemCount) {
  if ((int)icons.size() < itemCount || itemCount <= 0) return false;
  for (int i = 0; i < itemCount; ++i) {
    if (icons[i].isEmpty()) return false;
  }
  return true;
}

}  // namespace

QWidget *ToolPropertiesPanel::createEnumIconGridPanel(
    const QString &label, TEnumProperty *enumProp, const std::string &propName,
    int propGroup, QWidget *parentWidget,
    const std::function<void(int)> &onChanged) {
  if (!enumProp) return nullptr;

  TTool *tool = getCurrentTool();
  const QStringList iconNames = collectEnumItemIcons(tool, enumProp);
  const TEnumProperty::Items &items = enumProp->getItems();
  if (!enumIconsComplete(iconNames, (int)items.size())) return nullptr;

  QWidget *host = parentWidget ? parentWidget : m_propertiesContainer;
  QWidget *container = new QWidget(host);
  container->setProperty("propName", QString::fromStdString(propName));
  container->setProperty("propGroup", propGroup);
  storeTppPropertyPtr(container, enumProp);
  container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  QVBoxLayout *outerLayout = new QVBoxLayout(container);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(CollapsibleStyle::kItemSpacing);

  const QString sectionLabel =
      label.isEmpty() ? enumProp->getQStringName() : label;
  if (m_showLabels && !sectionLabel.isEmpty()) {
    auto *nameLabel = new QLabel(sectionLabel, container);
    nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outerLayout->addWidget(nameLabel);
    outerLayout->addSpacing(kEnumIconGridLabelGap);
  }

  QFrame *frame = new QFrame(container);
  frame->setObjectName(QStringLiteral("tppEnumIconGridFrame"));
  applyEnumIconGridFrameStyle(frame, m_showBorders, m_showBackgrounds,
                              palette());

  // Host grid on a plain widget so icon cells inherit toolOptionsPanel palette
  // (styled QFrame parents can make cell backgrounds read too dark).
  QWidget *gridHost = new QWidget(frame);
  gridHost->setAttribute(Qt::WA_TranslucentBackground);
  gridHost->setAutoFillBackground(false);

  QVBoxLayout *frameLayout = new QVBoxLayout(frame);
  frameLayout->setContentsMargins(0, 0, 0, 0);
  frameLayout->setSpacing(0);
  frameLayout->addWidget(gridHost);

  QGridLayout *gridLayout = new QGridLayout(gridHost);
  gridLayout->setContentsMargins(kEnumIconGridFramePad, kEnumIconGridFramePad,
                                 kEnumIconGridFramePad, kEnumIconGridFramePad);
  gridLayout->setHorizontalSpacing(kEnumIconGridSpacingH);
  gridLayout->setVerticalSpacing(kEnumIconGridSpacingV);
  for (int col = 0; col < kEnumIconGridCols; ++col)
    gridLayout->setColumnStretch(col, 1);

  QButtonGroup *buttonGroup = new QButtonGroup(container);
  buttonGroup->setExclusive(true);

  const int currentIndex = enumProp->getIndex();

  for (int i = 0; i < (int)items.size(); ++i) {
    const int row = i / kEnumIconGridCols;
    const int col = i % kEnumIconGridCols;

    ToolPropertyButton *btn = new ToolPropertyButton(QString(), gridHost);
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setAutoFillBackground(false);
    btn->setFixedSize(kEnumIconGridBtnSize, kEnumIconGridBtnSize);
    btn->setIconSize(QSize(kEnumIconGridIconSize, kEnumIconGridIconSize));
    btn->setIcon(createQIcon(iconNames[i].toStdString().c_str()));
    btn->setToolTip(items[i].UIName);
    btn->setCompactIconHighlight(kEnumIconGridIconSize);
    btn->setShowBorders(m_showBorders);
    btn->setShowBackgrounds(m_showBackgrounds);
    if (i == currentIndex) btn->setChecked(true);

    buttonGroup->addButton(btn, i);
    gridLayout->addWidget(btn, row, col, Qt::AlignCenter);
  }

  outerLayout->addWidget(frame);

  connect(buttonGroup, &QButtonGroup::idClicked,
          [this, container, propName, onChanged](int id) {
            TTool *tool = getCurrentTool();
            if (!tool) return;

            const int group = container->property("propGroup").toInt();
            TProperty *prop =
                resolveTppProperty(tool, container, group, propName);
            if (auto *ep = dynamic_cast<TEnumProperty *>(prop)) {
              ep->setIndex(id);
              tool->onPropertyChanged(propName, true);
              ep->notifyListeners();
              if (m_toolHandle) m_toolHandle->notifyToolChanged();
              if (tool->getName() == T_EditAssistants &&
                  propName == "AssistantType")
                refreshProperties();
            }

            if (onChanged) onChanged(id);
          });

  return container;
}

QWidget *ToolPropertiesPanel::createCollapsibleEnumForProperty(
    const QString &label, TEnumProperty *enumProp, const std::string &propName,
    int propGroup, const QString &headerIconName,
    const std::function<void(int)> &onChanged, bool reserveHeaderRightSlot,
    QWidget *parentWidget) {
  if (!enumProp) return nullptr;

  QStringList items;
  for (const auto &item : enumProp->getItems()) items << item.UIName;
  if (items.isEmpty()) return nullptr;

  const int index = enumProp->getIndex();
  QWidget *widget = nullptr;

  if (m_showIcons) {
    widget = createEnumIconGridPanel(label, enumProp, propName, propGroup,
                                     parentWidget, onChanged);
  }

  if (!widget) {
    widget = createCollapsibleEnum(label, items, index, propName, headerIconName,
                                   onChanged, reserveHeaderRightSlot,
                                   parentWidget);
  }

  if (widget) {
    widget->setProperty("propGroup", propGroup);
    storeTppPropertyPtr(widget, enumProp);
  }
  return widget;
}

//=============================================================================
// Generic Property Helpers
// Name-based property lookup: each helper is a no-op when the property is
// absent from the current tool's group — identical to the brush pattern.
//=============================================================================

void ToolPropertiesPanel::createEnumProperty(const QString &label,
                                             const std::string &propName,
                                             int propGroup,
                                             const QString &iconName) {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props) return;

  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop || prop->getName() != propName) continue;

    TEnumProperty *enumProp = dynamic_cast<TEnumProperty *>(prop);
    if (!enumProp) return;

    if (enumProp->getItems().empty()) return;

    QWidget *w = createCollapsibleEnumForProperty(label, enumProp, propName,
                                                  propGroup, iconName);
    if (!w) return;
    m_propertiesLayout->addWidget(w);
    return;
  }
}

void ToolPropertiesPanel::createBoolProperty(const QString &label,
                                             const std::string &propName,
                                             int propGroup) {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props) return;

  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop || prop->getName() != propName) continue;

    TBoolProperty *boolProp = dynamic_cast<TBoolProperty *>(prop);
    if (!boolProp) return;

    QWidget *w = createCheckBox(label, boolProp->getValue(), propName);
    w->setProperty("propGroup", propGroup);
    m_propertiesLayout->addWidget(w);
    return;
  }
}

void ToolPropertiesPanel::createDoublePairByName(const QString &label,
                                                 const std::string &propName,
                                                 int propGroup) {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props) return;

  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop || prop->getName() != propName) continue;

    TDoublePairProperty *dp = dynamic_cast<TDoublePairProperty *>(prop);
    if (dp) {
      createDoublePairSlider(label, dp, propName);
      return;
    }
    TIntPairProperty *ip = dynamic_cast<TIntPairProperty *>(prop);
    if (ip) {
      createIntPairSlider(label, ip, propName);
      return;
    }
    return;
  }
}

void ToolPropertiesPanel::createIntSliderByName(const QString &label,
                                                const std::string &propName,
                                                int propGroup) {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props) return;

  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop || prop->getName() != propName) continue;

    TIntProperty *intProp = dynamic_cast<TIntProperty *>(prop);
    if (!intProp) return;

    int min = intProp->getRange().first;
    int max = intProp->getRange().second;
    int val = intProp->getValue();

    QWidget *w = createSliderWithLabel(label, min, max, val, propName);
    w->setProperty("propGroup", propGroup);
    m_propertiesLayout->addWidget(w);
    return;
  }
}

void ToolPropertiesPanel::createDoubleSliderByName(const QString &label,
                                                   const std::string &propName,
                                                   int propGroup) {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props) return;

  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop || prop->getName() != propName) continue;

    TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty *>(prop);
    if (!doubleProp) return;

    double min = doubleProp->getRange().first;
    double max = doubleProp->getRange().second;
    double val = doubleProp->getValue();

    QWidget *w = createDoubleSliderWithLabel(label, min, max, val, propName);
    w->setProperty("propGroup", propGroup);
    m_propertiesLayout->addWidget(w);
    return;
  }
}

//=============================================================================
// Eraser Properties Creation
//
// Exact Tool Options Bar order (left→right = top→bottom):
//
//   Vector:       Size | Type | Selective | Invert | Frame Range | Interpolation
//   Toonz Raster: Size | Hardness | Type | Mode | Selective | Invert |
//                 Frame Range | Pencil Mode
//   FullColor:    Size | Hardness | Opacity | Type | Invert | Frame Range
//
// Calls that do not match the current tool type are no-ops.
//=============================================================================

//-----------------------------------------------------------------------------
// Generic property builder — works for any tool without hardcoding prop names.
// Iterates all TProperty in the given group and creates appropriate widgets.
//-----------------------------------------------------------------------------

bool ToolPropertiesPanel::createGenericProperties(int propGroup,
                                                  QVBoxLayout *targetLayout,
                                                  bool plasticAlignedFields) {
  TTool *tool = getCurrentTool();
  if (!tool) return false;
  TPropertyGroup *props = tool->getProperties(propGroup);
  if (!props || props->getPropertyCount() == 0) return false;

  QVBoxLayout *layout = targetLayout ? targetLayout : m_propertiesLayout;
  bool addedAny         = false;
  // Guard against tools that bind the same property twice in one group.
  std::set<std::string> seenNames;

  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (!prop) continue;

    const std::string name = prop->getName();
    if (seenNames.count(name)) continue;  // skip duplicate property
    seenNames.insert(name);

    // Use translated UI name; fall back to internal name
    QString label = prop->getQStringName();
    if (label.isEmpty()) label = QString::fromStdString(name);
    QWidget *w = nullptr;

    if (auto *ep = dynamic_cast<TEnumProperty *>(prop)) {
      if (ep->getItems().empty()) continue;
      w = createCollapsibleEnumForProperty(label, ep, name, propGroup, QString(),
                                           nullptr, plasticAlignedFields);

    } else if (auto *bp = dynamic_cast<TBoolProperty *>(prop)) {
      w = createCheckBox(label, bp->getValue(), name);

    } else if (auto *ip = dynamic_cast<TIntProperty *>(prop)) {
      auto range = ip->getRange();
      w = createSliderWithLabel(label, static_cast<int>(range.first),
                                static_cast<int>(range.second),
                                ip->getValue(), name);

    } else if (auto *dp = dynamic_cast<TDoubleProperty *>(prop)) {
      auto range = dp->getRange();
      w = createDoubleSliderWithLabel(label, range.first, range.second,
                                      dp->getValue(), name);

    } else if (auto *sp = dynamic_cast<TStringProperty *>(prop)) {
      w = createTextProperty(label, sp, name, propGroup);

    } else if (dynamic_cast<TDoublePairProperty *>(prop)) {
      // Reuse existing helper (does its own lookup, no widget returned)
      createDoublePairByName(label, name, propGroup);
      addedAny = true;
      continue;

    } else if (dynamic_cast<TIntPairProperty *>(prop)) {
      createIntPairSlider(label, prop, name);
      addedAny = true;
      continue;
    }
    // TPointerProperty, etc. — skip silently

    if (w) {
      w->setProperty("propGroup", propGroup);
      storeTppPropertyPtr(w, prop);
      layout->addWidget(w);
      if (plasticAlignedFields) uniformPlasticFieldWidget(w);
      addedAny = true;
    }
  }

  return addedAny;
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createRulerProperties() {
  TTool *tool = getCurrentTool();
  auto *ruler = dynamic_cast<RulerTool *>(tool);
  if (!ruler) return;

  // RulerTool has no TPropertyGroup — it pushes live measurements to registered
  // RulerToolOptionsBox instances (same mechanism as the horizontal options bar).
  if (!m_rulerOptionsBox) {
    // Vertical layout for the side properties panel (toolbar keeps horizontal).
    m_rulerOptionsBox = new RulerToolOptionsBox(this, tool, true);
    m_rulerOptionsBox->setSizePolicy(QSizePolicy::Preferred,
                                     QSizePolicy::Minimum);
  } else {
    m_rulerOptionsBox->setParent(m_propertiesContainer);
  }

  if (!m_rulerOptionsBoxRegistered) {
    ruler->setToolOptionsBox(m_rulerOptionsBox);
    m_rulerOptionsBoxRegistered = true;
  }

  m_rulerOptionsBox->resetValues();
  m_rulerOptionsBox->hide();
  // Keep registered for live measurement updates, but do not show in the panel:
  // the ruler has no adjustable properties (same message as Iron / Zoom).
}

void ToolPropertiesPanel::createEditAssistantsProperties() {
  // EditAssistantsTool::getProperties() rebuilds its group on every call and
  // must not be invoked from change handlers — store direct TProperty* on widgets.
  createGenericProperties(0);
}

namespace {

void resetShiftTraceGhost(int index) {
  TApplication *app = TApp::instance();
  if (!app) return;
  OnionSkinMask osm = app->getCurrentOnionSkin()->getOnionSkinMask();
  osm.setShiftTraceGhostCenter(index, TPointD());
  osm.setShiftTraceGhostAff(index, TAffine());
  app->getCurrentOnionSkin()->setOnionSkinMask(osm);
  app->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
  if (TTool *tool = app->getCurrentTool()->getTool()) tool->reset();
}

bool shiftTraceGhostHasShift(int index) {
  TApplication *app = TApp::instance();
  if (!app) return false;
  const OnionSkinMask osm =
      app->getCurrentOnionSkin()->getOnionSkinMask();
  return !(osm.getShiftTraceGhostAff(index).isIdentity() &&
           osm.getShiftTraceGhostCenter(index) == TPointD());
}

ShiftTraceTool *activeShiftTraceTool() {
  if (TTool *tool = TApp::instance()->getCurrentTool()->getTool())
    return dynamic_cast<ShiftTraceTool *>(tool);
  return dynamic_cast<ShiftTraceTool *>(
      TTool::getTool(T_ShiftTrace, TTool::ToonzImage));
}

void setShiftTraceGhostIndex(int index) {
  if (ShiftTraceTool *st = activeShiftTraceTool()) {
    st->setCurrentGhostIndex(index);
    return;
  }
  CommandManager *cm = CommandManager::instance();
  if (!cm) return;
  if (index == 0)
    cm->execute(MI_ShiftTraceSelectPrevGhost);
  else
    cm->execute(MI_ShiftTraceSelectNextGhost);
}

QCheckBox *addShiftTraceToggleRow(QVBoxLayout *layout, QWidget *parent,
                                  QAction *action,
                                  QCheckBox **storedPtr) {
  if (!layout || !parent || !action) return nullptr;
  QCheckBox *checkBox = new QCheckBox(action->text(), parent);
  checkBox->setChecked(action->isChecked());
  checkBox->setEnabled(action->isEnabled());
  layout->addWidget(checkBox);
  QObject::connect(checkBox, &QCheckBox::clicked, action, &QAction::trigger);
  QObject::connect(action, &QAction::toggled, checkBox, &QCheckBox::setChecked);
  QObject::connect(action, &QAction::changed, checkBox, [checkBox, action]() {
    checkBox->setEnabled(action->isEnabled());
  });
  if (storedPtr) *storedPtr = checkBox;
  return checkBox;
}

}  // namespace

void ToolPropertiesPanel::createShiftTraceProperties() {
  CommandManager *cm = CommandManager::instance();
  if (!cm) return;

  QAction *noShiftAct    = cm->getAction(MI_NoShift);
  QAction *resetShiftAct = cm->getAction(MI_ResetShift);
  if (!noShiftAct || !resetShiftAct) return;

  ShiftTraceTool *stTool = activeShiftTraceTool();
  const int ghostIndex   = stTool ? stTool->getCurrentGhostIndex() : 0;

  const QStringList ghostItems{tr("Previous Drawing"), tr("Following Drawing")};
  m_shiftTraceGhostPicker =
      createCollapsibleEnum(tr("Drawing"), ghostItems, ghostIndex,
                            "shift_trace_ghost", QString(),
                            [](int index) { setShiftTraceGhostIndex(index); });
  if (m_shiftTraceGhostPicker) {
    m_shiftTraceGhostPicker->setObjectName("shiftTraceGhostPicker");
    m_propertiesLayout->addWidget(m_shiftTraceGhostPicker);
  }

  QWidget *resetRow = new QWidget(m_propertiesContainer);
  QHBoxLayout *resetLayout = new QHBoxLayout(resetRow);
  resetLayout->setContentsMargins(0, 0, 0, 0);
  resetLayout->setSpacing(CollapsibleStyle::kHeaderSpacing);

  m_shiftTraceResetPrevBtn = new QPushButton(tr("Reset Previous"), resetRow);
  m_shiftTraceResetFollowingBtn =
      new QPushButton(tr("Reset Following"), resetRow);
  m_shiftTraceResetPrevBtn->setSizePolicy(QSizePolicy::Expanding,
                                          QSizePolicy::Fixed);
  m_shiftTraceResetFollowingBtn->setSizePolicy(QSizePolicy::Expanding,
                                               QSizePolicy::Fixed);
  resetLayout->addWidget(m_shiftTraceResetPrevBtn);
  resetLayout->addWidget(m_shiftTraceResetFollowingBtn);
  m_propertiesLayout->addWidget(resetRow);

  QObject::connect(m_shiftTraceResetPrevBtn, &QPushButton::clicked, []() {
    resetShiftTraceGhost(0);
  });
  QObject::connect(m_shiftTraceResetFollowingBtn, &QPushButton::clicked, []() {
    resetShiftTraceGhost(1);
  });

  TApplication *app = TApp::instance();
  const int levelType = detectCurrentLevelType(app);
  if (levelType == TZP_XSHLEVEL || levelType == OVL_XSHLEVEL ||
      levelType == TZI_XSHLEVEL) {
    const QStringList bboxItems{tr("Full raster bounding box"), tr("Savebox"),
                                tr("Content (alpha)")};
    const int bboxIdx = static_cast<int>(ShiftTraceTool::getGhostBBoxMode());
    m_shiftTraceBBoxPicker = createCollapsibleEnum(
        tr("Ghost Reference"), bboxItems, bboxIdx, "shift_trace_bbox", QString(),
        [](int index) {
          ShiftTraceTool::setGhostBBoxMode(
              static_cast<ShiftTraceTool::GhostBBoxMode>(index));
        });
    if (m_shiftTraceBBoxPicker) {
      m_shiftTraceBBoxPicker->setObjectName("shiftTraceBBoxPicker");
      m_propertiesLayout->addWidget(m_shiftTraceBBoxPicker);
    }
  }

  m_propertiesLayout->addSpacing(CollapsibleStyle::kBlockGap);

  if (m_showIcons) {
    QWidget *iconRow = new QWidget(m_propertiesContainer);
    QHBoxLayout *iconLayout = new QHBoxLayout(iconRow);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setSpacing(CollapsibleStyle::kHeaderSpacing);

    m_shiftTraceNoShiftIconBtn = new ToolPropertyButton(QString(), iconRow);
    m_shiftTraceNoShiftIconBtn->setCheckable(true);
    m_shiftTraceNoShiftIconBtn->setCursor(Qt::PointingHandCursor);
    m_shiftTraceNoShiftIconBtn->setToolTip(noShiftAct->text());
    m_shiftTraceNoShiftIconBtn->setIcon(createQIcon("shift_and_trace_no_shift"));
    m_shiftTraceNoShiftIconBtn->setIconSize(QSize(20, 20));
    m_shiftTraceNoShiftIconBtn->setFixedSize(32, 32);
    m_shiftTraceNoShiftIconBtn->setShowBorders(m_showBorders);
    m_shiftTraceNoShiftIconBtn->setShowBackgrounds(m_showBackgrounds);
    m_shiftTraceNoShiftIconBtn->setChecked(noShiftAct->isChecked());
    m_shiftTraceNoShiftIconBtn->setEnabled(noShiftAct->isEnabled());
    iconLayout->addWidget(m_shiftTraceNoShiftIconBtn);

    m_shiftTraceResetShiftIconBtn =
        new ToolPropertyButton(QString(), iconRow);
    m_shiftTraceResetShiftIconBtn->setCursor(Qt::PointingHandCursor);
    m_shiftTraceResetShiftIconBtn->setToolTip(resetShiftAct->text());
    m_shiftTraceResetShiftIconBtn->setIcon(createQIcon("shift_and_trace_reset"));
    m_shiftTraceResetShiftIconBtn->setIconSize(QSize(20, 20));
    m_shiftTraceResetShiftIconBtn->setFixedSize(32, 32);
    m_shiftTraceResetShiftIconBtn->setShowBorders(m_showBorders);
    m_shiftTraceResetShiftIconBtn->setShowBackgrounds(m_showBackgrounds);
    m_shiftTraceResetShiftIconBtn->setEnabled(resetShiftAct->isEnabled());
    iconLayout->addWidget(m_shiftTraceResetShiftIconBtn);
    iconLayout->addStretch(1);
    m_propertiesLayout->addWidget(iconRow);

    QObject::connect(m_shiftTraceNoShiftIconBtn, &QToolButton::clicked,
                     noShiftAct, &QAction::trigger);
    QObject::connect(noShiftAct, &QAction::toggled, m_shiftTraceNoShiftIconBtn,
                     &QToolButton::setChecked);
    QObject::connect(m_shiftTraceResetShiftIconBtn, &QToolButton::clicked,
                     resetShiftAct, &QAction::trigger);
  } else {
    m_shiftTraceNoShiftChk =
        addShiftTraceToggleRow(m_propertiesLayout, m_propertiesContainer,
                               noShiftAct, &m_shiftTraceNoShiftChk);

    QPushButton *resetShiftBtn = new QPushButton(resetShiftAct->text(),
                                                 m_propertiesContainer);
    resetShiftBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    resetShiftBtn->setEnabled(resetShiftAct->isEnabled());
    QObject::connect(resetShiftBtn, &QPushButton::clicked, resetShiftAct,
                     &QAction::trigger);
    QObject::connect(resetShiftAct, &QAction::changed, resetShiftBtn,
                     [resetShiftBtn, resetShiftAct]() {
                       resetShiftBtn->setEnabled(resetShiftAct->isEnabled());
                     });
    m_propertiesLayout->addWidget(resetShiftBtn);
  }

  connect(noShiftAct, &QAction::toggled, this,
          &ToolPropertiesPanel::onShiftTraceCommandChanged);
  connect(noShiftAct, &QAction::changed, this,
          &ToolPropertiesPanel::onShiftTraceCommandChanged);
  connect(resetShiftAct, &QAction::toggled, this,
          &ToolPropertiesPanel::onShiftTraceCommandChanged);
  connect(resetShiftAct, &QAction::changed, this,
          &ToolPropertiesPanel::onShiftTraceCommandChanged);

  updateShiftTraceWidgets();
}

void ToolPropertiesPanel::updateShiftTraceWidgets() {
  if (m_currentToolType != QStringLiteral("shifttrace")) return;

  CommandManager *cm = CommandManager::instance();
  if (!cm) return;

  auto syncCheckableAction = [](QAction *act, QCheckBox *chk,
                                ToolPropertyButton *iconBtn) {
    if (!act) return;
    if (chk) {
      if (chk->isChecked() != act->isChecked()) {
        chk->blockSignals(true);
        chk->setChecked(act->isChecked());
        chk->blockSignals(false);
      }
      chk->setEnabled(act->isEnabled());
    }
    if (iconBtn) {
      if (iconBtn->isChecked() != act->isChecked()) {
        iconBtn->blockSignals(true);
        iconBtn->setChecked(act->isChecked());
        iconBtn->blockSignals(false);
      }
      iconBtn->setEnabled(act->isEnabled());
    }
  };

  syncCheckableAction(cm->getAction(MI_NoShift), m_shiftTraceNoShiftChk,
                      m_shiftTraceNoShiftIconBtn);

  auto syncCollapsibleEnum = [](QWidget *container, int index,
                                const QStringList &items) {
    if (!container || index < 0 || index >= items.size()) return;
    if (QLabel *valueLabel = container->findChild<QLabel *>("valueLabel"))
      valueLabel->setText(items.value(index));
    if (auto *group = container->findChild<QButtonGroup *>()) {
      if (QAbstractButton *btn = group->button(index)) {
        group->blockSignals(true);
        btn->setChecked(true);
        group->blockSignals(false);
      }
    }
  };

  ShiftTraceTool *stTool = activeShiftTraceTool();
  const int ghostIndex   = stTool ? stTool->getCurrentGhostIndex() : 0;

  if (m_shiftTraceGhostPicker) {
    syncCollapsibleEnum(m_shiftTraceGhostPicker, ghostIndex,
                        {tr("Previous Drawing"), tr("Following Drawing")});
  }

  if (m_shiftTraceBBoxPicker) {
    syncCollapsibleEnum(
        m_shiftTraceBBoxPicker,
        static_cast<int>(ShiftTraceTool::getGhostBBoxMode()),
        {tr("Full raster bounding box"), tr("Savebox"), tr("Content (alpha)")});
  }

  if (m_shiftTraceResetPrevBtn)
    m_shiftTraceResetPrevBtn->setEnabled(shiftTraceGhostHasShift(0));
  if (m_shiftTraceResetFollowingBtn)
    m_shiftTraceResetFollowingBtn->setEnabled(shiftTraceGhostHasShift(1));

  if (QAction *act = cm->getAction(MI_ResetShift)) {
    if (m_shiftTraceResetShiftIconBtn)
      m_shiftTraceResetShiftIconBtn->setEnabled(act->isEnabled());
  }
}

void ToolPropertiesPanel::onShiftTraceCommandChanged() {
  updateShiftTraceWidgets();
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::updatePlasticSkeletonPicker() {
  if (!m_plasticSkelPicker) return;

  auto *plastic = dynamic_cast<PlasticTool *>(getCurrentTool());
  if (!plastic) return;

  QStringList items;
  int currentIndex = -1;
  const SkDP &sd = plastic->deformation();
  if (sd) {
    const int curId = plastic->currentSkeletonId();
    SkD::skelId_iterator st, sEnd;
    sd->skeletonIds(st, sEnd);
    for (int i = 0; st != sEnd; ++st, ++i) {
      items << QString::number(*st);
      if (*st == curId) currentIndex = i;
    }
  }

  // After removal the skelIds curve can still reference a deleted id — realign.
  if (currentIndex < 0 && !items.isEmpty()) {
    plastic->editSkelId_undo(items[0].toInt());
    return;
  }

  const int storedCount = m_plasticSkelPicker->property("pickerItemCount").toInt();
  if (storedCount != items.size()) {
    rebuildPlasticSkeletonPicker();
    return;
  }

  if (items.isEmpty()) {
    if (QLabel *valueLabel =
            m_plasticSkelPicker->findChild<QLabel *>("pickerValueLabel"))
      valueLabel->clear();
    if (auto *group =
            m_plasticSkelPicker->findChild<QButtonGroup *>("pickerButtonGroup")) {
      if (QAbstractButton *checked = group->checkedButton()) {
        group->setExclusive(false);
        checked->setChecked(false);
        group->setExclusive(true);
      }
    }
    m_plasticSkelPicker->setProperty("pickerCurrentIndex", -1);
    m_plasticSkelPicker->setProperty("pickerCurrentSkelId", -1);
    return;
  }

  if (m_plasticSkelPicker->property("pickerCurrentSkelId").toInt() ==
          plastic->currentSkeletonId() &&
      m_plasticSkelPicker->property("pickerCurrentIndex").toInt() == currentIndex)
    return;

  syncCollapsiblePicker(m_plasticSkelPicker, currentIndex);
  m_plasticSkelPicker->setProperty("pickerCurrentSkelId",
                                    plastic->currentSkeletonId());
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::updatePlasticVertexField() {
  if (!m_plasticVertexWidget) return;

  TTool *tool = getCurrentTool();
  if (!tool) return;

  TPropertyGroup *pg = tool->getProperties(PlasticTool::MODES_COUNT);
  if (!pg) return;

  auto *sp = dynamic_cast<TStringProperty *>(pg->getProperty("vertexName"));
  if (!sp) return;

  const QString newText = QString::fromStdWString(sp->getValue());
  if (QLineEdit *textEdit =
          m_plasticVertexWidget->findChild<QLineEdit *>("collapsibleTextEdit")) {
    if (textEdit->text() != newText) {
      textEdit->blockSignals(true);
      textEdit->setText(newText);
      textEdit->blockSignals(false);
    }
  }
  if (QLabel *valueLabel =
          m_plasticVertexWidget->findChild<QLabel *>("textValueLabel")) {
    valueLabel->setText(newText);
  }
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::rebuildPlasticSkeletonPicker() {
  auto *plastic = dynamic_cast<PlasticTool *>(getCurrentTool());
  if (!plastic || !m_propertiesLayout) return;

  if (const SkDP &sd = plastic->deformation()) {
    QStringList items;
    int currentIndex = -1;
    const int curId = plastic->currentSkeletonId();
    SkD::skelId_iterator st, sEnd;
    sd->skeletonIds(st, sEnd);
    for (int i = 0; st != sEnd; ++st, ++i) {
      items << QString::number(*st);
      if (*st == curId) currentIndex = i;
    }
    if (currentIndex < 0 && !items.isEmpty()) {
      plastic->editSkelId_undo(items[0].toInt());
      return;
    }
  }

  if (m_plasticSkelPicker) {
    m_propertiesLayout->removeWidget(m_plasticSkelPicker);
    delete m_plasticSkelPicker;
    m_plasticSkelPicker = nullptr;
  }

  m_plasticSkelPicker = buildPlasticSkeletonPicker(plastic);
  if (!m_plasticSkelPicker) return;

  int insertAt = 1;
  for (int i = 0; i < m_propertiesLayout->count(); ++i) {
    QLayoutItem *item = m_propertiesLayout->itemAt(i);
    if (!item || !item->widget()) continue;
    if (auto *btn = qobject_cast<QPushButton *>(item->widget())) {
      if (btn->text() == tr("Create Mesh")) {
        insertAt = i + 1;
        break;
      }
    }
  }
  m_propertiesLayout->insertWidget(insertAt, m_plasticSkelPicker);
}

//-----------------------------------------------------------------------------

QWidget *ToolPropertiesPanel::buildPlasticSkeletonPicker(PlasticTool *plastic) {
  QStringList items;
  int currentIndex = -1;

  const SkDP &sd = plastic->deformation();
  if (sd) {
    const int curId = plastic->currentSkeletonId();
    SkD::skelId_iterator st, sEnd;
    sd->skeletonIds(st, sEnd);
    for (int i = 0; st != sEnd; ++st, ++i) {
      items << QString::number(*st);
      if (*st == curId) currentIndex = i;
    }
  }

  const int pickerIndex = currentIndex >= 0 ? currentIndex : 0;
  const QString headerValue =
      currentIndex >= 0 ? items.value(currentIndex) : QString();

  QWidget *picker = createCollapsiblePicker(
      tr("Skeleton:"), items, pickerIndex, "plastic_skeleton",
      [plastic](int index) {
        if (!plastic->deformation()) return;
        QStringList skelItems;
        SkD::skelId_iterator st, sEnd;
        plastic->deformation()->skeletonIds(st, sEnd);
        for (; st != sEnd; ++st) skelItems << QString::number(*st);
        if (index < 0 || index >= skelItems.size()) return;
        const int skelId = skelItems[index].toInt();
        if (skelId == plastic->currentSkeletonId()) return;
        plastic->editSkelId_undo(skelId);
      });
  picker->setObjectName("plasticSkelPicker");
  picker->setProperty("pickerItemCount", items.size());
  picker->setProperty("pickerCurrentSkelId",
                       currentIndex >= 0 ? plastic->currentSkeletonId() : -1);
  picker->setProperty("pickerCurrentIndex", currentIndex);

  if (QLabel *valueLabel = picker->findChild<QLabel *>("pickerValueLabel"))
    valueLabel->setText(headerValue);

  if (items.isEmpty())
    replacePlasticSkeletonEmptyLineEdit(picker, m_showBorders, m_showBackgrounds);

  // Keep [-][+] on the header row (outside expanded list), fixed on the right.
  if (auto *mainLayout = qobject_cast<QVBoxLayout *>(picker->layout())) {
    if (QLayoutItem *headerItem = mainLayout->itemAt(0)) {
      if (auto *header = headerItem->widget()) {
        if (auto *headerLayout = qobject_cast<QHBoxLayout *>(header->layout())) {
          QWidget *btnHost = new QWidget(header);
          QHBoxLayout *btnLayout = new QHBoxLayout(btnHost);
          btnLayout->setContentsMargins(0, 0, 0, 0);
          btnLayout->setSpacing(4);

          QPushButton *removeSkelBtn = new QPushButton("-", btnHost);
          QPushButton *addSkelBtn    = new QPushButton("+", btnHost);
          removeSkelBtn->setFixedSize(20, 20);
          addSkelBtn->setFixedSize(20, 20);

          // TPP header order [-][+] (reversed vs TOB [+][-]).
          btnLayout->addWidget(removeSkelBtn);
          btnLayout->addWidget(addSkelBtn);
          btnHost->setFixedWidth(CollapsibleStyle::kHeaderRightSlot);
          headerLayout->addWidget(btnHost);

          QObject::connect(removeSkelBtn, &QPushButton::released, [plastic]() {
            if (plastic->isEnabled() && plastic->deformation())
              plastic->removeSkeleton_withKeyframes_undo(
                  plastic->currentSkeletonId());
          });
          QObject::connect(addSkelBtn, &QPushButton::released, [plastic]() {
            if (plastic->isEnabled())
              plastic->addSkeleton_undo(PlasticSkeletonP(new PlasticSkeleton));
          });
        }
      }
    }
  }

  return picker;
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::syncPlasticModeFromTool() {
  TTool *tool = getCurrentTool();
  auto *plastic = dynamic_cast<PlasticTool *>(tool);
  if (!plastic || !m_plasticModeLayout) return;

  TPropertyGroup *modeGroup = tool->getProperties(PlasticTool::MODES_COUNT);
  if (!modeGroup) return;

  TEnumProperty *modeProp =
      dynamic_cast<TEnumProperty *>(modeGroup->getProperty("mode"));
  if (!modeProp) return;

  const int mode = modeProp->getIndex();
  if (mode == m_plasticVisibleMode) return;

  m_plasticVisibleMode = mode;
  rebuildPlasticModeSection();
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::rebuildPlasticModeSection() {
  if (!m_plasticModeLayout) return;

  TTool *tool = getCurrentTool();
  auto *plastic = dynamic_cast<PlasticTool *>(tool);
  if (!plastic) return;

  QLayoutItem *item;
  while ((item = m_plasticModeLayout->takeAt(0)) != nullptr) {
    if (item->widget()) delete item->widget();
    delete item;
  }

  TPropertyGroup *modeGroup = tool->getProperties(PlasticTool::MODES_COUNT);
  if (!modeGroup) return;

  TEnumProperty *modeProp =
      dynamic_cast<TEnumProperty *>(modeGroup->getProperty("mode"));
  if (!modeProp) return;

  const int mode = modeProp->getIndex();
  m_plasticVisibleMode = mode;

  // Match toolbar: only the active mode sub-toolbar is visible.
  if (mode == PlasticTool::MESH_IDX) return;

  if (mode == PlasticTool::RIGIDITY_IDX) {
    m_plasticModeLayout->setContentsMargins(0, 0, 0, 0);
    createPlasticRigidityModeProperties(plastic);
    return;
  }

  m_plasticModeLayout->setContentsMargins(CollapsibleStyle::kContentLeftIndent, 0,
                                          CollapsibleStyle::kContentRightMargin,
                                          0);

  if (mode == PlasticTool::ANIMATE_IDX) {
    createPlasticAnimateModeProperties(plastic);
    return;
  }

  createGenericProperties(mode, m_plasticModeLayout, true);
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createPlasticRigidityModeProperties(
    PlasticTool *plastic) {
  if (!plastic || !m_plasticModeLayout) return;

  TTool *tool = getCurrentTool();
  TPropertyGroup *rigidityGroup = tool->getProperties(PlasticTool::RIGIDITY_IDX);
  if (!rigidityGroup) return;

  for (int i = 0; i < rigidityGroup->getPropertyCount(); ++i) {
    TProperty *prop = rigidityGroup->getProperty(i);
    if (!prop) continue;

    const std::string name = prop->getName();
    QString label          = prop->getQStringName();
    if (label.isEmpty()) label = QString::fromStdString(name);

    if (auto *thickness = dynamic_cast<TIntProperty *>(prop)) {
      auto range = thickness->getRange();
      QWidget *thicknessWidget = createCollapsibleIntSlider(
          label, static_cast<int>(range.first),
          static_cast<int>(range.second), thickness->getValue(), name,
          PlasticTool::RIGIDITY_IDX, name, true);
      m_plasticModeLayout->addWidget(thicknessWidget);
      continue;
    }

    if (i > 0) m_plasticModeLayout->addSpacing(CollapsibleStyle::kSectionGap);

    if (auto *rigidValue = dynamic_cast<TEnumProperty *>(prop)) {
      QStringList items;
      for (const auto &item : rigidValue->getItems()) items << item.UIName;
      if (items.isEmpty()) continue;

      const QString rigidLabel = rigidValue->getQStringName();
      QWidget *rigidWidget = createCollapsibleEnumForProperty(
          rigidLabel, rigidValue, name, PlasticTool::RIGIDITY_IDX, QString(),
          nullptr, true, m_propertiesContainer);
      if (!rigidWidget) continue;
      rigidWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      m_plasticModeLayout->addWidget(rigidWidget);
    }
  }

  m_plasticModeLayout->addSpacing(CollapsibleStyle::kSectionGap);
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createPlasticAnimateModeProperties(
    PlasticTool *plastic) {
  if (!plastic || !m_plasticModeLayout) return;

  QWidget *relayBlock = new QWidget(m_plasticModeContainer);
  relayBlock->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

  QGridLayout *relayGrid = new QGridLayout(relayBlock);
  relayGrid->setContentsMargins(0, 0, 0, 0);
  relayGrid->setHorizontalSpacing(CollapsibleStyle::kHeaderSpacing);
  relayGrid->setVerticalSpacing(CollapsibleStyle::kBlockGap);
  relayGrid->setColumnMinimumWidth(0, CollapsibleStyle::kAnimateRelayLabelW);
  relayGrid->setColumnMinimumWidth(1, CollapsibleStyle::kAnimateRelayFieldW);
  relayGrid->setColumnStretch(0, 0);
  relayGrid->setColumnStretch(1, 0);

  auto addRelayRow = [&](int row, const QString &labelText,
                         TDoubleParamRelayProperty *relay,
                         const QString &objectName) {
    auto *label = new ClickableLabel(labelText, relayBlock);
    label->setFixedSize(CollapsibleStyle::kAnimateRelayLabelW,
                        CollapsibleStyle::kFieldHeight);

    MeasuredValueField *field =
        createPlasticRelayField(relayBlock, plastic, relay, objectName);
    field->setFixedSize(CollapsibleStyle::kAnimateRelayFieldW,
                        CollapsibleStyle::kFieldHeight);

    relayGrid->addWidget(label, row, 0, Qt::AlignRight | Qt::AlignVCenter);
    relayGrid->addWidget(field, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
    connectClickableLabel(label, field);

    QObject::connect(field, &QLineEdit::editingFinished, [this]() {
      if (m_toolHandle) m_toolHandle->notifyToolChanged();
    });
  };

  addRelayRow(0, tr("Distance"), &plastic->distanceRelayProperty(),
              "plasticDistanceRelay");
  addRelayRow(1, tr("Angle"), &plastic->angleRelayProperty(),
              "plasticAngleRelay");
  addRelayRow(2, tr("SO"), &plastic->soRelayProperty(), "plasticSORelay");

  m_plasticModeLayout->addWidget(relayBlock, 0, Qt::AlignLeft);

  createGenericProperties(PlasticTool::ANIMATE_IDX, m_plasticModeLayout, true);
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::updatePlasticRelayFields() {
  if (!m_plasticModeContainer) return;

  auto *plastic = dynamic_cast<PlasticTool *>(getCurrentTool());
  if (!plastic) return;

  if (MeasuredValueField *distanceField =
          m_plasticModeContainer->findChild<MeasuredValueField *>(
              "plasticDistanceRelay"))
    syncPlasticRelayField(distanceField, &plastic->distanceRelayProperty());
  if (MeasuredValueField *angleField =
          m_plasticModeContainer->findChild<MeasuredValueField *>(
              "plasticAngleRelay"))
    syncPlasticRelayField(angleField, &plastic->angleRelayProperty());
  if (MeasuredValueField *soField =
          m_plasticModeContainer->findChild<MeasuredValueField *>(
              "plasticSORelay"))
    syncPlasticRelayField(soField, &plastic->soRelayProperty());
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createPlasticProperties() {
  TTool *tool = getCurrentTool();
  auto *plastic = dynamic_cast<PlasticTool *>(tool);
  if (!plastic) return;

  tool->updateTranslation();
  m_plasticVisibleMode = -1;

  // 1. Create Mesh — CommandManager only (no shared QAction with toolbar).
  QPushButton *meshBtn =
      new QPushButton(tr("Create Mesh"), m_propertiesContainer);
  meshBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QObject::connect(meshBtn, &QPushButton::clicked, []() {
    CommandManager::instance()->execute("A_ToolOption_Meshify");
  });
  m_propertiesLayout->addWidget(meshBtn);

  TPropertyGroup *modeGroup = tool->getProperties(PlasticTool::MODES_COUNT);
  if (!modeGroup) return;

  // 2. Skeleton — collapsible picker (TPP layout, button list).
  m_plasticSkelPicker = buildPlasticSkeletonPicker(plastic);
  m_propertiesLayout->addWidget(m_plasticSkelPicker);

  QObject::connect(plastic, SIGNAL(skelIdsListChanged()), this,
                   SLOT(rebuildPlasticSkeletonPicker()));
  QObject::connect(plastic, SIGNAL(skelIdChanged()), this,
                   SLOT(updatePlasticSkeletonPicker()));

  m_propertiesLayout->addSpacing(CollapsibleStyle::kBlockGap);

  // 3. Mode selector — matches toolbar order (before Vertex Name).
  TEnumProperty *modeProp =
      dynamic_cast<TEnumProperty *>(modeGroup->getProperty("mode"));
  if (!modeProp) return;

  QWidget *modeWidget = createCollapsibleEnumForProperty(
      modeProp->getQStringName(), modeProp, "mode", PlasticTool::MODES_COUNT,
      QString(), [this](int) { rebuildPlasticModeSection(); }, true);
  if (modeWidget) m_propertiesLayout->addWidget(modeWidget);

  m_propertiesLayout->addSpacing(CollapsibleStyle::kBlockGap);

  // 4. Vertex name — after Mode, as in the tool options bar.
  if (TProperty *vn = modeGroup->getProperty("vertexName")) {
    if (auto *sp = dynamic_cast<TStringProperty *>(vn)) {
      const QString vertexLabel =
          sp->getQStringName().isEmpty() ? tr("Vertex Name")
                                         : sp->getQStringName();
      m_plasticVertexWidget = createCollapsibleTextField(
          vertexLabel, QString::fromStdWString(sp->getValue()),
          "plastic_vertex", "vertexName", PlasticTool::MODES_COUNT,
          [this](const QString &text) {
            TTool *t = getCurrentTool();
            if (!t) return;
            TPropertyGroup *pg = t->getProperties(PlasticTool::MODES_COUNT);
            if (!pg) return;
            if (auto *vp =
                    dynamic_cast<TStringProperty *>(pg->getProperty("vertexName"))) {
              vp->setValue(text.toStdWString());
              t->onPropertyChanged("vertexName");
              if (m_toolHandle) m_toolHandle->notifyToolChanged();
            }
          },
          true);
      m_propertiesLayout->addWidget(m_plasticVertexWidget);
    }
  }

  // 5. Active mode options only (rebuilt when mode changes).
  m_plasticModeContainer = new QWidget(m_propertiesContainer);
  m_plasticModeLayout    = new QVBoxLayout(m_plasticModeContainer);
  m_plasticModeLayout->setContentsMargins(CollapsibleStyle::kContentLeftIndent, 0,
                                          CollapsibleStyle::kContentRightMargin,
                                          0);
  m_plasticModeLayout->setSpacing(CollapsibleStyle::kBlockGap);
  m_plasticModeLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  m_propertiesLayout->addWidget(m_plasticModeContainer);

  rebuildPlasticModeSection();
}

//-----------------------------------------------------------------------------

namespace {

void updateToolOptionControlsIn(QWidget *root) {
  if (!root) return;
  const QList<QWidget *> widgets = root->findChildren<QWidget *>();
  for (QWidget *w : widgets) {
    if (auto *c = dynamic_cast<ToolOptionControl *>(w)) c->updateStatus();
  }
}

QWidget *addMeasuredFieldRow(QWidget *parent, QVBoxLayout *layout,
                             const QString &labelText,
                             MeasuredValueField *field) {
  QWidget *row = new QWidget(parent);
  QHBoxLayout *rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(3);

  auto *label = new ClickableLabel(labelText, row);
  label->setFixedHeight(20);
  field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  rowLayout->addWidget(label);
  rowLayout->addWidget(field, 1);
  layout->addWidget(row);
  connectClickableLabel(label, field);
  return row;
}

constexpr int kGridLabelWidth = 22;
constexpr int kGridFieldWidth = 68;
constexpr int kGridColGutter  = 16;
constexpr int kGridLabelGap   = 2;
constexpr int kGridRowVSpacing = 11;

QWidget *createCenteredFieldGrid(QWidget *parent, QGridLayout *&outGrid,
                                 int columnCount) {
  QWidget *block = new QWidget(parent);
  QHBoxLayout *outer = new QHBoxLayout(block);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addStretch();

  QWidget *gridHost = new QWidget(block);
  outGrid = new QGridLayout(gridHost);
  outGrid->setContentsMargins(0, 4, 0, 4);
  outGrid->setHorizontalSpacing(kGridLabelGap);
  outGrid->setVerticalSpacing(kGridRowVSpacing);

  outer->addWidget(gridHost);
  outer->addStretch();
  return block;
}

// Selection transform grid — shared column widths across all rows.
constexpr int kSelIconWidth  = 16;
constexpr int kSelLabelWidth = kGridLabelWidth;
constexpr int kSelFieldWidth = kGridFieldWidth;
constexpr int kSelAuxWidth   = 20;

constexpr int kSelUniformFieldW = 52;

// Animate field grid — same compact base as Selection.
constexpr int kAnimateUniformFieldW = kSelUniformFieldW;
constexpr int kAnimateColGutter     = 4;

constexpr int kSelCaptionHeight = 16;
// Optical bias: shift caption block slightly left (~2–3 px on 52 px field).
constexpr int kSelCaptionCenterBiasL = 3;
constexpr int kSelCaptionCenterBiasR = 4;

void applyTppExpandingMeasuredField(MeasuredValueField *field, int minWidth) {
  if (!field) return;
  // Override TOB constructor caps (getMaximumWidthFor*ToolField) for TPP grids.
  field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  field->setMinimumWidth(minWidth);
  field->setMaximumWidth(QWIDGETSIZE_MAX);
  field->setFixedHeight(CollapsibleStyle::kFieldHeight);
}

void styleSelectionTransformField(MeasuredValueField *field) {
  applyTppExpandingMeasuredField(field, kSelUniformFieldW);
}

QWidget *selectionTransformAuxSpacer(QWidget *parent) {
  QWidget *sp = new QWidget(parent);
  sp->setFixedWidth(kSelAuxWidth);
  sp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  return sp;
}

QWidget *selectionTransformCaptionCell(QWidget *parent, QWidget *caption) {
  QWidget *cell = new QWidget(parent);
  cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  cell->setFixedHeight(kSelCaptionHeight);
  QHBoxLayout *layout = new QHBoxLayout(cell);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addStretch(kSelCaptionCenterBiasL);
  layout->addWidget(caption, 0, Qt::AlignVCenter);
  layout->addStretch(kSelCaptionCenterBiasR);
  return cell;
}

QWidget *selectionTransformCenteredAuxCell(QWidget *parent, QWidget *content,
                                           int height, int stretchLeft = 1,
                                           int stretchRight = 1) {
  QWidget *cell = new QWidget(parent);
  cell->setFixedSize(kSelAuxWidth, height);
  QHBoxLayout *layout = new QHBoxLayout(cell);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addStretch(stretchLeft);
  layout->addWidget(content, 0, Qt::AlignCenter);
  layout->addStretch(stretchRight);
  return cell;
}

QWidget *selectionTransformLockIconCell(QWidget *parent, QWidget *icon) {
  QWidget *cell = new QWidget(parent);
  cell->setFixedSize(kSelAuxWidth, kSelCaptionHeight);
  QHBoxLayout *layout = new QHBoxLayout(cell);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  // Toward Scale H: left-align in the link column (symmetric margins had no effect).
  layout->addWidget(icon, 0, Qt::AlignLeft | Qt::AlignVCenter);
  layout->addStretch(1);
  return cell;
}

// Scale row: flip H | link | V field | flip V (5-column grid)

void initSelectionTransformGrid(QGridLayout *grid) {
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(kGridLabelGap);
  grid->setVerticalSpacing(2);
  for (int col = 0; col < 5; ++col) grid->setColumnStretch(col, 0);
  grid->setColumnStretch(0, 1);
  grid->setColumnStretch(3, 1);
  grid->setColumnMinimumWidth(0, kSelUniformFieldW);
  grid->setColumnMinimumWidth(1, kSelAuxWidth);
  grid->setColumnMinimumWidth(2, kSelAuxWidth);
  grid->setColumnMinimumWidth(3, kSelUniformFieldW);
  grid->setColumnMinimumWidth(4, kSelAuxWidth);
}

QWidget *selectionTransformSectionGap(QWidget *parent) {
  QWidget *gap = new QWidget(parent);
  gap->setFixedHeight(kGridRowVSpacing);
  gap->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  return gap;
}

void selectionTransformAddSectionGap(QGridLayout *grid, int row,
                                     QWidget *parent) {
  QWidget *gap = selectionTransformSectionGap(parent);
  grid->addWidget(gap, row, 0, 1, 5);
}

ClickableLabel *selectionStackedAddCaption(const QString &text,
                                         QWidget *parent) {
  auto *label = new ClickableLabel(text, parent);
  label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  label->setFixedHeight(kSelCaptionHeight);
  const int textW =
      label->fontMetrics().horizontalAdvance(text) + 2;
  label->setFixedWidth(textW);
  label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  return label;
}

QWidget *selectionStackedAddCaptionGroup(QWidget *parent,
                                         const QString &iconName,
                                         ClickableLabel *&outLabel,
                                         const QString &text,
                                         const QString &iconToolTip = QString()) {
  QWidget *group = new QWidget(parent);
  group->setFixedHeight(kSelCaptionHeight);
  group->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  QHBoxLayout *layout = new QHBoxLayout(group);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  if (!iconName.isEmpty()) {
    auto *icon = new QLabel(group);
    QIcon qi   = createQIcon(iconName);
    if (!qi.isNull()) icon->setPixmap(qi.pixmap(16, 16));
    icon->setFixedSize(16, 16);
    icon->setAlignment(Qt::AlignCenter);
    if (!iconToolTip.isEmpty()) {
      icon->setToolTip(iconToolTip);
      group->setToolTip(iconToolTip);
    }
    layout->addWidget(icon, 0, Qt::AlignVCenter);
  }

  outLabel = selectionStackedAddCaption(text, group);
  layout->addWidget(outLabel, 0, Qt::AlignVCenter);
  return group;
}

QLabel *selectionStackedAddSectionIcon(QWidget *parent,
                                       const QString &iconName,
                                       const QString &toolTip = QString()) {
  auto *icon = new QLabel(parent);
  QIcon qi   = createQIcon(iconName);
  if (!qi.isNull()) icon->setPixmap(qi.pixmap(16, 16));
  icon->setFixedSize(16, 16);
  icon->setAlignment(Qt::AlignCenter);
  if (!toolTip.isEmpty()) icon->setToolTip(toolTip);
  return icon;
}

void updateSelectionScaleLinkLockIcon(QLabel *icon, bool linked) {
  if (!icon) return;
  const QString iconName =
      linked ? QStringLiteral("lock_on") : QStringLiteral("lock");
  QIcon qi = createQIcon(iconName);
  if (qi.isNull()) return;

  QPixmap pm = qi.pixmap(16, 16);
  if (linked) {
    // lock_on glyph reads right of lock; redraw 2 px left inside the label (toward H).
    QPixmap shifted(16, 16);
    shifted.fill(Qt::transparent);
    QPainter painter(&shifted);
    painter.drawPixmap(-2, 0, pm);
    icon->setPixmap(shifted);
  } else {
    icon->setPixmap(pm);
  }

  icon->setStyleSheet(QString());
  if (QWidget *cell = icon->parentWidget()) {
    if (QHBoxLayout *layout = qobject_cast<QHBoxLayout *>(cell->layout())) {
      layout->setContentsMargins(0, 0, 0, 0);
    }
  }
}

void selectionStackAddBlank(QGridLayout *grid, int row, int col,
                            QWidget *parent, int width,
                            int height = CollapsibleStyle::kFieldHeight) {
  auto *blank = new QWidget(parent);
  blank->setFixedSize(width, height);
  grid->addWidget(blank, row, col);
}

void initSelectionFieldGrid(QGridLayout *grid) {
  grid->setColumnMinimumWidth(0, kSelIconWidth);
  grid->setColumnMinimumWidth(1, kSelLabelWidth);
  grid->setColumnMinimumWidth(2, kSelFieldWidth);
  grid->setColumnMinimumWidth(3, kSelAuxWidth);
  grid->setColumnMinimumWidth(4, kSelAuxWidth);
  grid->setColumnMinimumWidth(5, kGridColGutter);
  grid->setColumnMinimumWidth(6, kSelLabelWidth);
  grid->setColumnMinimumWidth(7, kSelFieldWidth);
  grid->setColumnMinimumWidth(8, kSelAuxWidth);
  grid->setColumnMinimumWidth(9, 40);
  for (int col = 0; col < 10; ++col) grid->setColumnStretch(col, 0);
}

void selectionGridAddSeparator(QGridLayout *grid, int row, QWidget *parent) {
  auto *sep = new QFrame(parent);
  sep->setFrameShape(QFrame::HLine);
  sep->setFrameShadow(QFrame::Sunken);
  sep->setFixedHeight(2);
  grid->addWidget(sep, row, 0, 1, 10);
}

void selectionGridAddIcon(QGridLayout *grid, int row, QWidget *parent,
                        const QString &iconName) {
  auto *icon = new QLabel(parent);
  QIcon qi   = createQIcon(iconName);
  if (!qi.isNull()) icon->setPixmap(qi.pixmap(16, 16));
  icon->setFixedSize(kSelIconWidth, 16);
  grid->addWidget(icon, row, 0, Qt::AlignHCenter | Qt::AlignVCenter);
}

void selectionGridAddBlank(QGridLayout *grid, int row, int col, QWidget *parent,
                           int width, int height = 20) {
  auto *blank = new QWidget(parent);
  blank->setFixedSize(width, height);
  grid->addWidget(blank, row, col);
}

void selectionGridAddLabel(QGridLayout *grid, int row, int col, QWidget *parent,
                           ClickableLabel *label) {
  label->setFixedSize(kSelLabelWidth, CollapsibleStyle::kFieldHeight);
  grid->addWidget(label, row, col, Qt::AlignRight | Qt::AlignVCenter);
}

void selectionGridAddField(QGridLayout *grid, int row, int col,
                           MeasuredValueField *field) {
  styleSelectionTransformField(field);
  grid->addWidget(field, row, col);
}

QPushButton *selectionGridAddAuxButton(QWidget *parent,
                                       const QString &iconName,
                                       const QString &fallback,
                                       const QString &tip) {
  auto *btn = new QPushButton(parent);
  QIcon qi  = createQIcon(iconName);
  if (!qi.isNull()) {
    btn->setIcon(qi);
    btn->setIconSize(QSize(16, 16));
  } else {
    btn->setText(fallback);
  }
  btn->setFixedSize(kSelAuxWidth, CollapsibleStyle::kFieldHeight);
  btn->setToolTip(tip);
  return btn;
}

QPushButton *selectionGridAddAuxButton(QGridLayout *grid, int row, int col,
                                       QWidget *parent,
                                       const QString &iconName,
                                       const QString &fallback,
                                       const QString &tip) {
  QPushButton *btn =
      selectionGridAddAuxButton(parent, iconName, fallback, tip);
  grid->addWidget(btn, row, col, Qt::AlignLeft | Qt::AlignVCenter);
  return btn;
}

void styleAnimateGridField(MeasuredValueField *field) {
  applyTppExpandingMeasuredField(field, kAnimateUniformFieldW);
}

void reapplySelectionTransformFieldSizing(SelectionScaleField *scaleX,
                                          SelectionScaleField *scaleY,
                                          SelectionRotationField *rotation,
                                          SelectionMoveField *moveX,
                                          SelectionMoveField *moveY,
                                          ThickChangeField *thick) {
  styleSelectionTransformField(scaleX);
  styleSelectionTransformField(scaleY);
  styleSelectionTransformField(rotation);
  styleSelectionTransformField(moveX);
  styleSelectionTransformField(moveY);
  styleSelectionTransformField(thick);
}

void gridAddLabeledField(QGridLayout *grid, int row, int labelCol, int fieldCol,
                         QWidget *parent, const QString &labelText,
                         MeasuredValueField *field,
                         QList<QWidget *> *rowWidgets = nullptr) {
  if (!labelText.isEmpty()) {
    auto *label = new ClickableLabel(labelText, parent);
    label->setFixedSize(kGridLabelWidth, 20);
    grid->addWidget(label, row, labelCol, Qt::AlignRight | Qt::AlignVCenter);
    connectClickableLabel(label, field);
    if (rowWidgets) rowWidgets->append(label);
  } else {
    auto *spacer = new QWidget(parent);
    spacer->setFixedSize(kGridLabelWidth, 20);
    grid->addWidget(spacer, row, labelCol);
    if (rowWidgets) rowWidgets->append(spacer);
  }

  styleAnimateGridField(field);
  grid->addWidget(field, row, fieldCol);
  if (rowWidgets) rowWidgets->append(field);
}

void gridAddColumnSpacer(QGridLayout *grid, int row, QWidget *parent,
                         int rightLabelCol, int rightFieldCol,
                         QList<QWidget *> *rowWidgets = nullptr) {
  auto *spacer = new QWidget(parent);
  spacer->setMinimumWidth(kGridLabelWidth);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  grid->addWidget(spacer, row, rightLabelCol);
  auto *fieldSpacer = new QWidget(parent);
  fieldSpacer->setMinimumWidth(kAnimateUniformFieldW);
  fieldSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  grid->addWidget(fieldSpacer, row, rightFieldCol);
  if (rowWidgets) {
    rowWidgets->append(spacer);
    rowWidgets->append(fieldSpacer);
  }
}

void setGridRowVisible(const QList<QWidget *> &rowWidgets, bool visible) {
  for (QWidget *w : rowWidgets) if (w) w->setVisible(visible);
}

void initAnimateFieldGrid(QGridLayout *grid) {
  grid->setContentsMargins(0, 4, 0, 4);
  grid->setHorizontalSpacing(kGridLabelGap);
  grid->setVerticalSpacing(kGridRowVSpacing);
  for (int col = 0; col < 5; ++col) grid->setColumnStretch(col, 0);
  grid->setColumnStretch(1, 1);
  grid->setColumnStretch(4, 1);
  grid->setColumnMinimumWidth(0, kGridLabelWidth);
  grid->setColumnMinimumWidth(1, kAnimateUniformFieldW);
  grid->setColumnMinimumWidth(2, kAnimateColGutter);
  grid->setColumnMinimumWidth(3, kGridLabelWidth);
  grid->setColumnMinimumWidth(4, kAnimateUniformFieldW);
}

void styleTypeToolCombo(QWidget *combo) {
  combo->setMinimumWidth(kSelUniformFieldW);
  combo->setMaximumWidth(QWIDGETSIZE_MAX);
  combo->setFixedHeight(CollapsibleStyle::kFieldHeight);
  combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

constexpr int kTypeLabelWidth = 32;

void addTypeToolComboRow(QWidget *parent, QVBoxLayout *layout,
                         const QString &labelText, QWidget *combo,
                         bool showLabel) {
  QWidget *row = new QWidget(parent);
  row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout *h = new QHBoxLayout(row);
  h->setContentsMargins(CollapsibleStyle::kHeaderMarginH, 0, 0, 0);
  h->setSpacing(kGridLabelGap);
  if (showLabel && !labelText.isEmpty()) {
    auto *label = new QLabel(labelText, row);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setFixedWidth(kTypeLabelWidth);
    h->addWidget(label);
  }
  styleTypeToolCombo(combo);
  h->addWidget(combo, 1);
  layout->addWidget(row);
}

}  // namespace

void ToolPropertiesPanel::createAnimateProperties() {
  TTool *tool = getCurrentTool();
  if (!tool || detectCurrentToolId() != T_Edit) return;

  TPropertyGroup *pg = tool->getProperties(0);
  if (!pg) return;

  TApplication *app = TApp::instance();
  if (!app) return;

  TFrameHandle *frameHandle = app->getCurrentFrame();
  TObjectHandle *objHandle  = app->getCurrentObject();
  TXsheetHandle *xshHandle  = app->getCurrentXsheet();

  tool->updateTranslation();

  // 1–2. Collapsible capsules — full panel width so triangles align
  m_animateColumnWidget = createAnimateColumnPicker(xshHandle, objHandle);
  m_animateColumnWidget->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Fixed);
  m_propertiesLayout->addWidget(m_animateColumnWidget);

  if (TEnumProperty *activeAxisProp =
          dynamic_cast<TEnumProperty *>(pg->getProperty("Active Axis"))) {
    QWidget *axisWidget = createCollapsibleEnumForProperty(
        activeAxisProp->getQStringName(), activeAxisProp,
        activeAxisProp->getName());
    if (axisWidget) {
      axisWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      m_propertiesLayout->addWidget(axisWidget);
    }
  }

  // Position / X-Y / Z / SO — full-width grid, adaptive fields
  QWidget *fieldBlock = new QWidget(m_propertiesContainer);
  fieldBlock->setMinimumWidth(0);
  fieldBlock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  QGridLayout *fieldGrid = new QGridLayout(fieldBlock);
  initAnimateFieldGrid(fieldGrid);
  QWidget *gridHost = fieldBlock;

  QList<QWidget *> splineRowWidgets;
  QList<QWidget *> xyRowWidgets;

  int row = 0;

  auto *motionPathField = new PegbarChannelField(
      tool, TStageObject::T_Path, "field", frameHandle, objHandle, xshHandle,
      gridHost);
  gridAddLabeledField(fieldGrid, row, 0, 1, gridHost, tr("Position:"),
                      motionPathField, &splineRowWidgets);
  gridAddColumnSpacer(fieldGrid, row, gridHost, 3, 4, &splineRowWidgets);

  ++row;
  auto *ewPosField = new PegbarChannelField(tool, TStageObject::T_X, "field",
                                            frameHandle, objHandle, xshHandle,
                                            gridHost);
  auto *nsPosField = new PegbarChannelField(tool, TStageObject::T_Y, "field",
                                            frameHandle, objHandle, xshHandle,
                                            gridHost);
  gridAddLabeledField(fieldGrid, row, 0, 1, gridHost, tr("X:"), ewPosField,
                      &xyRowWidgets);
  gridAddLabeledField(fieldGrid, row, 3, 4, gridHost, tr("Y:"), nsPosField,
                      &xyRowWidgets);

  ++row;
  auto *zField = new PegbarChannelField(tool, TStageObject::T_Z, "field",
                                        frameHandle, objHandle, xshHandle,
                                        gridHost);
  zField->setPrecision(4);
  auto *noScaleZField = new NoScaleField(tool, "field");
  noScaleZField->setParent(gridHost);
  noScaleZField->setPrecision(4);
  gridAddLabeledField(fieldGrid, row, 0, 1, gridHost, tr("Z:"), zField);
  gridAddLabeledField(fieldGrid, row, 3, 4, gridHost, QString(), noScaleZField);

  ++row;
  auto *soField = new PegbarChannelField(tool, TStageObject::T_SO, "field",
                                         frameHandle, objHandle, xshHandle,
                                         gridHost);
  gridAddLabeledField(fieldGrid, row, 0, 1, gridHost, tr("SO:"), soField);
  gridAddColumnSpacer(fieldGrid, row, gridHost, 3, 4);

  m_propertiesLayout->addWidget(fieldBlock);

  m_animateSplineRowWidgets = splineRowWidgets;
  m_animateXYRowWidgets     = xyRowWidgets;
  m_animateMeasuredFields     = fieldBlock->findChildren<MeasuredValueField *>();
  for (MeasuredValueField *field : m_animateMeasuredFields)
    styleAnimateGridField(field);

  TStageObjectId objId = objHandle->getObjectId();
  bool splined =
      xshHandle->getXsheet()->getStageObject(objId)->getSpline() != 0;
  setGridRowVisible(splineRowWidgets, splined);
  setGridRowVisible(xyRowWidgets, !splined);
}

void ToolPropertiesPanel::updateAnimateColumnPicker() {
  if (!m_animateColumnWidget) return;

  TApplication *app = TApp::instance();
  if (!app) return;

  const int code = (int)app->getCurrentObject()->getObjectId().getCode();
  const QVariantList codes =
      m_animateColumnWidget->property("pickerCodes").toList();
  for (int i = 0; i < codes.size(); ++i) {
    if (codes[i].toInt() == code) {
      syncCollapsiblePicker(m_animateColumnWidget, i);
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void ToolPropertiesPanel::createEraserProperties() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  if (!tool->getProperties(0)) return;

  // 1. Size — all variants
  createSizeProperty();

  // 2. Hardness — Toonz Raster + FullColor
  createHardnessProperty();

  // 3. Opacity — FullColor only
  createOpacityProperty();

  // 4. Type: Normal / Rectangular / Freehand / Polyline [/ Segment for vector]
  createEnumProperty(tr("Type"), "Type:");

  // 5. Mode: Lines / Areas / Lines & Areas — Toonz Raster only
  createEnumProperty(tr("Mode"), "Mode:");

  // 6. Selective — Vector (propName "Selective")
  //    Current Style — Toonz Raster (same UI label, different propName)
  createBoolProperty(tr("Selective"), "Selective");
  createBoolProperty(tr("Selective"), "Current Style");

  // 7. Invert — Vector + FullColor
  createBoolProperty(tr("Invert"), "Invert");

  // 8. Frame Range — all variants (TBoolProperty, not the brush enum)
  createBoolProperty(tr("Frame Range"), "Frame Range");

  // 9. Pencil Mode — Toonz Raster only
  createPencilModeProperty();

  // 10. Interpolation — Vector only (enabled when Frame Range is checked)
  createEnumProperty(tr("Interpolation"), "interpolation:");
}

//=============================================================================
// Fill Properties Creation
//
// Exact Tool Options Bar order (left→right = top→bottom):
//
//   Vector:       Type | Mode | Empty Only | Onion Skin | Frame Range |
//                 Maximum Gap
//   Toonz Raster: Type | Mode | Empty Only | Fill Depth | Segment |
//                 Close Gap | Refer Fill | Onion Skin | Frame Range |
//                 Autopaint Lines | Extend Fill | Gap Close Distance
//   FullColor:    Fill Depth
//
// Calls that do not match the current tool type are no-ops.
//=============================================================================

void ToolPropertiesPanel::createFillProperties() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  if (!tool->getProperties(0)) return;

  // 1. Type: Normal / Rectangular / Freehand / Polyline / FreePick
  //    (absent for FullColor → no-op)
  createEnumProperty(tr("Type"), "Type:");

  // 2. Mode: Lines / Areas / Lines & Areas
  //    (absent for FullColor → no-op)
  createEnumProperty(tr("Mode"), "Mode:");

  // 3. Empty Only
  createBoolProperty(tr("Empty Only"), "Empty Only");

  // 4. Fill Depth (DoublePair) — Toonz Raster + FullColor
  createDoublePairByName(tr("Fill Depth"), "Fill Depth");

  // 5–7. Toonz Raster properties only
  createBoolProperty(tr("Segment"), "Segment");
  createBoolProperty(tr("Close Gap"), "Close Gap");
  createBoolProperty(tr("Refer Fill"), "Refer Fill");

  // 8. Onion Skin — Vector + Toonz Raster
  createBoolProperty(tr("Onion Skin"), "Onion Skin");

  // 9. Frame Range — Vector + Toonz Raster (TBoolProperty)
  createBoolProperty(tr("Frame Range"), "Frame Range");

  // 10. Maximum Gap — Vector only
  createDoubleSliderByName(tr("Maximum Gap"), "Maximum Gap");

  // 11–13. Toonz Raster only
  createBoolProperty(tr("Autopaint Lines"), "Autopaint Lines");
  createBoolProperty(tr("Extend Fill"), "Extend Fill");
  createIntSliderByName(tr("Gap Close Distance"), "Gap Close Distance:");
}

//=============================================================================
// Geometric Properties Creation
//
// Exact Tool Options Bar order (left→right = top→bottom):
//
//   Vector:       Size | Shape | Polygon Sides | Rotate | Auto Group |
//                 Auto Fill | Snap | Sensitivity | Smooth |
//                 Cap | Join | Miter
//   Toonz Raster: Size | Hardness | Shape | Polygon Sides | Rotate |
//                 Empty Only | Pencil Mode | Smooth |
//                 Cap | Join | Miter
//   FullColor:    Size | Hardness | Opacity | Shape | Polygon Sides |
//                 Rotate | Smooth | Cap | Join | Miter
//
// Calls that do not match the current tool type are no-ops.
//=============================================================================

void ToolPropertiesPanel::createGeometricProperties() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  if (!tool->getProperties(0)) return;

  // 1. Size — all variants
  createSizeProperty();

  // 2. Hardness — Toonz Raster + FullColor
  createHardnessProperty();

  // 3. Opacity — FullColor only
  createOpacityProperty();

  // 4. Shape — all variants
  createEnumProperty(tr("Shape"), "Shape:");

  // 5. Polygon Sides — all variants (enabled when Shape == Polygon)
  createIntSliderByName(tr("Polygon Sides"), "Polygon Sides:");

  // 6. Rotate — all variants (lowercase propName in source code)
  createBoolProperty(tr("Rotate"), "rotate");

  // 7–8. Auto Group / Auto Fill — Vector only
  createBoolProperty(tr("Auto Group"), "Auto Group");
  createBoolProperty(tr("Auto Fill"), "Auto Fill");

  // 9–10. Snap + Sensitivity — Vector only
  createSnapProperty();
  createSnapSensitivityProperty();

  // 11. Empty Only — Toonz Raster only
  createBoolProperty(tr("Empty Only"), "Empty Only");

  // 12. Pencil Mode — Toonz Raster only
  createPencilModeProperty();

  // 13. Smooth — all variants (TBoolProperty for Geometric,
  //     so use createBoolProperty instead of createSmoothProperty)
  createBoolProperty(tr("Smooth"), "Smooth");

  // 14–16. Cap / Join / Miter — group 1, Vector only
  createCapProperty();
  createJoinProperty();
  createMiterProperty();
}

//=============================================================================
// Selection Properties Creation
//
// Exact Tool Options Bar order (left→right = top→bottom):
//
//   All variants: Type (Rect / Freehand / Polyline)
//   Vector:  + Mode | Include Intersection | Preserve Thickness |
//              Cap | Join | Miter (group outlineProps)
//   Raster:  + No Antialiasing
//   Toonz Raster: + Modify Savebox
//=============================================================================

void ToolPropertiesPanel::createSelectionProperties() {
  // Reset all selection field pointers (they'll be set below or remain nullptr)
  m_selScaleX = m_selScaleY = nullptr;
  m_selRotation             = nullptr;
  m_selMoveX = m_selMoveY  = nullptr;
  m_selThick                = nullptr;
  m_selScaleLink            = nullptr;
  m_selFlipH = m_selFlipV  = nullptr;
  m_selRotL = m_selRotR    = nullptr;
  m_selHLabel = m_selVLabel = nullptr;
  m_selXLabel = m_selYLabel = nullptr;
  m_selScaleLinkIcon        = nullptr;

  TTool *tool = getCurrentTool();
  if (!tool || !tool->getProperties(0)) return;

  SelectionTool       *selTool = dynamic_cast<SelectionTool *>(tool);
  VectorSelectionTool *vecTool = dynamic_cast<VectorSelectionTool *>(tool);
  RasterSelectionTool *rasTool = dynamic_cast<RasterSelectionTool *>(tool);
  if (!selTool) return;

  // ── 1. Type: Rect / Freehand / Polyline (all variants) ──────────────────
  createEnumProperty(tr("Type"), "Type:");

  // ── 2. Mode (vector only — no-op for raster via missing prop) ───────────
  createEnumProperty(tr("Mode"), "Mode:");

  // ── 3–4. Vector only ────────────────────────────────────────────────────
  createBoolProperty(tr("Include Intersection"), "Include Intersection");
  createBoolProperty(tr("Preserve Thickness"), "Preserve Thickness");

  // ── 5. No Antialiasing (raster only) ────────────────────────────────────
  createBoolProperty(tr("No Antialiasing"), "No Antialiasing");

  // ── Transform — single 5-column grid (mockup: aligned fields)
  {
    QWidget *transformBlock = new QWidget(m_propertiesContainer);
    transformBlock->setMinimumWidth(0);
    transformBlock->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
    QGridLayout *grid = new QGridLayout(transformBlock);
    initSelectionTransformGrid(grid);

    int row = 0;

    m_selScaleX = new SelectionScaleField(selTool, 0, "Scale X");
    m_selFlipH  = selectionGridAddAuxButton(transformBlock, "fliphoriz", "H",
                                            tr("Flip Selection Horizontally"));
    m_selScaleY = new SelectionScaleField(selTool, 1, "Scale Y");
    m_selFlipV  = selectionGridAddAuxButton(transformBlock, "flipvert", "V",
                                            tr("Flip Selection Vertically"));
    m_selScaleLink = new DVGui::CheckBox(QString(), transformBlock);
    m_selScaleLink->setToolTip(tr("Link Scale H and V"));
    m_selScaleLink->setFixedSize(kSelAuxWidth, CollapsibleStyle::kFieldHeight);
    m_selScaleLink->setChecked(selectionScaleLinkIsEnabled());
    styleSelectionTransformField(m_selScaleX);
    styleSelectionTransformField(m_selScaleY);

    m_selScaleLinkIcon = selectionStackedAddSectionIcon(
        transformBlock, QStringLiteral("lock"), tr("Link Scale H and V"));
    updateSelectionScaleLinkLockIcon(m_selScaleLinkIcon,
                                     selectionScaleLinkIsEnabled());
    QWidget *linkIconCell = selectionTransformLockIconCell(transformBlock,
                                                         m_selScaleLinkIcon);
    QWidget *linkCheckCell = selectionTransformCenteredAuxCell(
        transformBlock, m_selScaleLink, CollapsibleStyle::kFieldHeight, 0, 1);

    grid->setRowMinimumHeight(row, kSelCaptionHeight);
    grid->addWidget(selectionTransformCaptionCell(
                        transformBlock,
                        selectionStackedAddCaptionGroup(transformBlock, "edit_scale",
                                                        m_selHLabel, tr("H"),
                                                        tr("Scale"))),
                    row, 0);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 1);
    grid->addWidget(linkIconCell, row, 2, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(selectionTransformCaptionCell(
                        transformBlock,
                        selectionStackedAddCaptionGroup(transformBlock, QString(),
                                                        m_selVLabel, tr("V"))),
                    row, 3);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
    ++row;

    grid->addWidget(m_selScaleX, row, 0);
    grid->addWidget(m_selFlipH, row, 1, Qt::AlignCenter);
    grid->addWidget(linkCheckCell, row, 2, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(m_selScaleY, row, 3);
    grid->addWidget(m_selFlipV, row, 4, Qt::AlignCenter);
    ++row;

    selectionTransformAddSectionGap(grid, row++, transformBlock);

    m_selRotation = new SelectionRotationField(selTool, tr("Rotation"));
    styleSelectionTransformField(m_selRotation);
    m_selRotL = selectionGridAddAuxButton(transformBlock, "rotateleft", "L",
                                          tr("Rotate Selection Left 90°"));
    m_selRotR = selectionGridAddAuxButton(transformBlock, "rotateright", "R",
                                          tr("Rotate Selection Right 90°"));
    QWidget *rotIcon = selectionStackedAddSectionIcon(transformBlock,
                                                      "edit_rotation",
                                                      tr("Rotation"));

    grid->setRowMinimumHeight(row, kSelCaptionHeight);
    grid->addWidget(selectionTransformCaptionCell(transformBlock, rotIcon), row,
                    0);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 1);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 2);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 3);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
    ++row;

    grid->addWidget(m_selRotation, row, 0);
    grid->addWidget(m_selRotL, row, 1, Qt::AlignCenter);
    grid->addWidget(m_selRotR, row, 2, Qt::AlignCenter);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 3);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
    ++row;

    selectionTransformAddSectionGap(grid, row++, transformBlock);

    m_selMoveX = new SelectionMoveField(selTool, 0, "Move X");
    m_selMoveY = new SelectionMoveField(selTool, 1, "Move Y");
    styleSelectionTransformField(m_selMoveX);
    styleSelectionTransformField(m_selMoveY);

    grid->setRowMinimumHeight(row, kSelCaptionHeight);
    grid->addWidget(selectionTransformCaptionCell(
                        transformBlock,
                        selectionStackedAddCaptionGroup(transformBlock,
                                                        "edit_position",
                                                        m_selXLabel, tr("X"),
                                                        tr("Position"))),
                    row, 0);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 1);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 2);
    grid->addWidget(selectionTransformCaptionCell(
                        transformBlock,
                        selectionStackedAddCaptionGroup(transformBlock, QString(),
                                                        m_selYLabel, tr("Y"))),
                    row, 3);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
    ++row;

    grid->addWidget(m_selMoveX, row, 0);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 1);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 2);
    grid->addWidget(m_selMoveY, row, 3);
    grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
    ++row;

    if (vecTool) {
      selectionTransformAddSectionGap(grid, row++, transformBlock);

      m_selThick = new ThickChangeField(selTool, tr("Thickness"));
      styleSelectionTransformField(m_selThick);
      QWidget *thickIcon = selectionStackedAddSectionIcon(transformBlock,
                                                          "thickness",
                                                          tr("Thickness"));

      grid->setRowMinimumHeight(row, kSelCaptionHeight);
      grid->addWidget(selectionTransformCaptionCell(transformBlock, thickIcon),
                      row, 0);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 1);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 2);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 3);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
      ++row;

      grid->addWidget(m_selThick, row, 0);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 1);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 2);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 3);
      grid->addWidget(selectionTransformAuxSpacer(transformBlock), row, 4);
    }

    m_propertiesLayout->addWidget(transformBlock);
    reapplySelectionTransformFieldSizing(
        m_selScaleX, m_selScaleY, m_selRotation, m_selMoveX, m_selMoveY,
        m_selThick);
  }

  connect(m_selScaleX, &SelectionScaleField::valueChange,
          [this](bool addToUndo) {
            if (!m_selScaleLink || !m_selScaleLink->isChecked()) return;
            if (!m_selScaleY) return;
            if (m_selScaleX->getValue() == m_selScaleY->getValue()) return;
            m_selScaleY->setValue(m_selScaleX->getValue());
            m_selScaleY->applyChange(addToUndo);
          });
  connect(m_selScaleY, &SelectionScaleField::valueChange,
          [this](bool addToUndo) {
            if (!m_selScaleLink || !m_selScaleLink->isChecked()) return;
            if (!m_selScaleX) return;
            if (m_selScaleX->getValue() == m_selScaleY->getValue()) return;
            m_selScaleX->setValue(m_selScaleY->getValue());
            m_selScaleX->applyChange(addToUndo);
          });

  connect(m_selScaleLink, &QCheckBox::toggled, [this](bool linked) {
    updateSelectionScaleLinkLockIcon(m_selScaleLinkIcon, linked);
    setSelectionScaleLinkEnabled(linked);
  });

  connect(m_selFlipH, &QPushButton::clicked, [this]() {
    if (!m_selScaleX) return;
    m_selScaleX->setValue(m_selScaleX->getValue() * -1.0);
    m_selScaleX->applyChange(true);
    if (m_selScaleLink && m_selScaleLink->isChecked() && m_selScaleY) {
      m_selScaleY->setValue(m_selScaleX->getValue());
      m_selScaleY->applyChange(true);
    }
  });
  connect(m_selFlipV, &QPushButton::clicked, [this]() {
    if (!m_selScaleY) return;
    m_selScaleY->setValue(m_selScaleY->getValue() * -1.0);
    m_selScaleY->applyChange(true);
    if (m_selScaleLink && m_selScaleLink->isChecked() && m_selScaleX) {
      m_selScaleX->setValue(m_selScaleY->getValue());
      m_selScaleX->applyChange(true);
    }
  });

  connectClickableLabel(m_selHLabel, m_selScaleX);
  connectClickableLabel(m_selVLabel, m_selScaleY);

  connect(m_selRotL, &QPushButton::clicked, [this]() {
    if (!m_selRotation) return;
    m_selRotation->setValue(m_selRotation->getValue() + 90.0);
    m_selRotation->applyChange(true);
  });
  connect(m_selRotR, &QPushButton::clicked, [this]() {
    if (!m_selRotation) return;
    m_selRotation->setValue(m_selRotation->getValue() - 90.0);
    m_selRotation->applyChange(true);
  });

  connectClickableLabel(m_selXLabel, m_selMoveX);
  connectClickableLabel(m_selYLabel, m_selMoveY);

  // ── Modify Savebox (ToonzRaster only) ───────────────────────────────────
  if (rasTool) {
    TBoolProperty *modSaveProp = rasTool->getModifySaveboxProperty();
    if (modSaveProp) {
      QWidget    *w    = createCheckBox(tr("Modify Savebox"),
                                       modSaveProp->getValue(), "ModifySavebox");
      QCheckBox  *cb   = w ? w->findChild<QCheckBox *>() : nullptr;
      if (!cb) cb      = qobject_cast<QCheckBox *>(w);
      if (cb) {
        connect(cb, &QCheckBox::toggled, [rasTool](bool checked) {
          TBoolProperty *p = rasTool->getModifySaveboxProperty();
          if (p) {
            p->setValue(checked);
            // Call via TTool* (DVAPI base) — virtual dispatch reaches
            // RasterSelectionTool::onPropertyChanged at runtime
            static_cast<TTool *>(rasTool)->onPropertyChanged("ModifySavebox");
          }
        });
      }
      m_propertiesLayout->addWidget(w);
    }
  }

  // ── Vector-only: Cap / Join / Miter ─────────────────────────────────────
  if (vecTool) {
    createCapProperty();
    createJoinProperty();
    createMiterProperty();
  }

  // Initial disabled state — updateStatus() will enable them when selection exists
  m_selScaleX->setEnabled(false);
  m_selScaleY->setEnabled(false);
  m_selHLabel->setEnabled(false);
  m_selVLabel->setEnabled(false);
  m_selFlipH->setEnabled(false);
  m_selFlipV->setEnabled(false);
  m_selRotation->setEnabled(false);
  m_selRotL->setEnabled(false);
  m_selRotR->setEnabled(false);
  m_selMoveX->setEnabled(false);
  m_selMoveY->setEnabled(false);
  m_selXLabel->setEnabled(false);
  m_selYLabel->setEnabled(false);
  if (m_selThick) m_selThick->setEnabled(false);
}

//=============================================================================
// Type (Text) Properties — Font / Size as dropdown menus (same as TOB)
//=============================================================================

void ToolPropertiesPanel::createTypeProperties() {
  TTool *tool = getCurrentTool();
  if (!tool) return;
  tool->updateTranslation();

  auto processGroup = [&](int propGroup) {
    TPropertyGroup *props = tool->getProperties(propGroup);
    if (!props) return;

    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty *prop = props->getProperty(i);
      if (!prop) continue;

      const std::string name = prop->getName();
      QString label          = prop->getQStringName();
      if (label.isEmpty()) label = QString::fromStdString(name);

      if (name == "Font:") {
        auto *ep = dynamic_cast<TEnumProperty *>(prop);
        if (!ep) continue;
        auto *fontCombo = new ToolOptionFontCombo(tool, ep, m_toolHandle);
        addTypeToolComboRow(m_propertiesContainer, m_propertiesLayout, label,
                            fontCombo, m_showLabels);
        continue;
      }

      if (name == "Size:") {
        auto *ep = dynamic_cast<TEnumProperty *>(prop);
        if (!ep) continue;
        auto *sizeCombo = new ToolOptionCombo(tool, ep, m_toolHandle);
        addTypeToolComboRow(m_propertiesContainer, m_propertiesLayout, label,
                            sizeCombo, m_showLabels);
        continue;
      }

      if (name == "Style:") {
        auto *ep = dynamic_cast<TEnumProperty *>(prop);
        if (!ep || ep->getItems().empty()) continue;
        m_typeStyleWidget = createCollapsibleEnumForProperty(
            label, ep, name, propGroup);
        if (!m_typeStyleWidget) continue;
        m_typeStyleWidget->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Fixed);
        m_propertiesLayout->addWidget(m_typeStyleWidget);
        continue;
      }

      if (auto *ep = dynamic_cast<TEnumProperty *>(prop)) {
        if (ep->getItems().empty()) continue;
        QWidget *w = createCollapsibleEnumForProperty(label, ep, name, propGroup);
        if (!w) continue;
        w->setProperty("propGroup", propGroup);
        w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_propertiesLayout->addWidget(w);
        continue;
      }

      if (auto *bp = dynamic_cast<TBoolProperty *>(prop)) {
        QWidget *w = createCheckBox(label, bp->getValue(), name);
        if (!w) continue;
        w->setProperty("propGroup", propGroup);
        m_propertiesLayout->addWidget(w);
        continue;
      }
    }
  };

  processGroup(0);
  processGroup(1);
}

void ToolPropertiesPanel::refreshTypeStyleWidget() {
  if (!m_typeStyleWidget || m_currentToolType != "type") return;

  TTool *tool = getCurrentTool();
  if (!tool) return;

  TPropertyGroup *props = tool->getProperties(1);
  if (!props) return;

  auto *ep = dynamic_cast<TEnumProperty *>(props->getProperty("Style:"));
  if (!ep) return;

  const int index = m_propertiesLayout->indexOf(m_typeStyleWidget);
  m_propertiesLayout->removeWidget(m_typeStyleWidget);
  m_typeStyleWidget->deleteLater();
  m_typeStyleWidget = nullptr;

  m_typeStyleWidget = createCollapsibleEnumForProperty(
      ep->getQStringName(), ep, "Style:", 1);
  if (!m_typeStyleWidget) return;
  m_typeStyleWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  if (index >= 0)
    m_propertiesLayout->insertWidget(index, m_typeStyleWidget);
  else
    m_propertiesLayout->addWidget(m_typeStyleWidget);
}

//-----------------------------------------------------------------------------
// Slots
//-----------------------------------------------------------------------------

void ToolPropertiesPanel::onToolSwitched() {
  refreshProperties();
}

void ToolPropertiesPanel::onSceneContextChanged() {
  TApplication *app = TApp::instance();
  if (app && app->getCurrentFrame()->isPlaying()) {
    updatePropertyValues();
    return;
  }
  refreshProperties();
}

void ToolPropertiesPanel::onToolChanged() {
  const QString currentId   = detectCurrentToolId();
  const QString currentType = detectCurrentToolType();
  if (currentId != m_currentToolId || currentType != m_currentToolType) {
    refreshProperties();
  } else {
    updatePropertyValues();
  }
}

void ToolPropertiesPanel::onToolComboBoxListChanged(const std::string &id) {
  if (m_currentToolType != "type") return;
  if (id != "Style:") return;
  refreshTypeStyleWidget();
  updateToolOptionControlsIn(m_propertiesContainer);
}

void ToolPropertiesPanel::updatePropertyValues() {
  TTool *tool = getCurrentTool();
  if (!tool) return;

  // Update all TProperty-backed widgets (including nested mode sections).
  updatePropertyWidgetsIn(m_propertiesContainer);

  // Update Selection tool live transform fields (updateStatus reads DeformValues)
  if (m_currentToolType == "selection") {
    if (m_selScaleX) {
      m_selScaleX->updateStatus();
      bool scaleEnabled = m_selScaleX->isEnabled();
      if (m_selHLabel) m_selHLabel->setEnabled(scaleEnabled);
      if (m_selFlipH)  m_selFlipH->setEnabled(scaleEnabled);
    }
    if (m_selScaleY) {
      m_selScaleY->updateStatus();
      bool scaleEnabled = m_selScaleY->isEnabled();
      if (m_selVLabel) m_selVLabel->setEnabled(scaleEnabled);
      if (m_selFlipV)  m_selFlipV->setEnabled(scaleEnabled);
    }
    if (m_selRotation) {
      m_selRotation->updateStatus();
      bool rotEnabled = m_selRotation->isEnabled();
      if (m_selRotL) m_selRotL->setEnabled(rotEnabled);
      if (m_selRotR) m_selRotR->setEnabled(rotEnabled);
    }
    if (m_selMoveX) {
      m_selMoveX->updateStatus();
      if (m_selXLabel) m_selXLabel->setEnabled(m_selMoveX->isEnabled());
    }
    if (m_selMoveY) {
      m_selMoveY->updateStatus();
      if (m_selYLabel) m_selYLabel->setEnabled(m_selMoveY->isEnabled());
    }
    if (m_selThick) m_selThick->updateStatus();
    reapplySelectionTransformFieldSizing(
        m_selScaleX, m_selScaleY, m_selRotation, m_selMoveX, m_selMoveY,
        m_selThick);
    if (m_selScaleLink) {
      const bool linked = selectionScaleLinkIsEnabled();
      if (m_selScaleLink->isChecked() != linked) {
        m_selScaleLink->blockSignals(true);
        m_selScaleLink->setChecked(linked);
        m_selScaleLink->blockSignals(false);
      }
      updateSelectionScaleLinkLockIcon(m_selScaleLinkIcon, linked);
    }
  }

  if (m_currentToolType == "plastic") {
    syncPlasticModeFromTool();
    updatePlasticSkeletonPicker();
    updatePlasticVertexField();
    updatePlasticRelayFields();
  }

  if (m_currentToolType == "shifttrace") updateShiftTraceWidgets();

  if (m_currentToolType == "type") {
    updateToolOptionControlsIn(m_propertiesContainer);
  }

  if (m_currentToolType == "edit") {
    updateAnimateColumnPicker();
    updateToolOptionControlsIn(m_propertiesContainer);
    for (MeasuredValueField *field : m_animateMeasuredFields)
      styleAnimateGridField(field);

    TApplication *app = TApp::instance();
    if (app && !m_animateSplineRowWidgets.isEmpty()) {
      TStageObjectId objId = app->getCurrentObject()->getObjectId();
      bool splined =
          app->getCurrentXsheet()->getXsheet()->getStageObject(objId)->getSpline() != 0;
      setGridRowVisible(m_animateSplineRowWidgets, splined);
      setGridRowVisible(m_animateXYRowWidgets, !splined);
    }
  }
}

void ToolPropertiesPanel::updatePropertyWidgetsIn(QWidget *root) {
  if (!root) return;

  if (root->property("propName").isValid()) updateWidgetFromProperty(root);

  const QList<QWidget *> widgets = root->findChildren<QWidget *>();
  for (QWidget *widget : widgets) {
    if (widget->property("propName").isValid())
      updateWidgetFromProperty(widget);
  }
}

void ToolPropertiesPanel::attachAllPropertySyncListeners() {
  TTool *tool = getCurrentTool();
  if (!tool) return;

  const QList<QWidget *> widgets =
      m_propertiesContainer->findChildren<QWidget *>();
  for (QWidget *widget : widgets) {
    if (!widget->property("propName").isValid()) continue;
    if (widget->property("propSyncAttached").toBool()) continue;

    const std::string propName =
        widget->property("propName").toString().toStdString();
    const int propGroup = widget->property("propGroup").toInt();

    TProperty *prop = tppPropertyFromWidget(widget);
    if (!prop) {
      TPropertyGroup *props = tool->getProperties(propGroup);
      if (!props && propGroup > 0) props = tool->getProperties(0);
      if (props) prop = props->getProperty(propName);
    }
    if (!prop) continue;

    new PropertyWidgetSync(this, widget, prop, widget);
    widget->setProperty("propSyncAttached", true);
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

  TProperty *prop = resolveTppProperty(tool, widget, propGroup, propName);
  if (!prop) return;

  // Collapsible text field (Plastic vertex name) — before generic line edits.
  if (auto *strProp = dynamic_cast<TStringProperty *>(prop)) {
      if (QLineEdit *directEdit = qobject_cast<QLineEdit *>(widget)) {
        const QString newText = QString::fromStdWString(strProp->getValue());
        directEdit->blockSignals(true);
        directEdit->setText(newText);
        directEdit->blockSignals(false);
        return;
      }
      if (QLineEdit *textEdit =
              widget->findChild<QLineEdit *>("collapsibleTextEdit")) {
        const QString newText = QString::fromStdWString(strProp->getValue());
        textEdit->blockSignals(true);
        textEdit->setText(newText);
        textEdit->blockSignals(false);
        if (QLabel *valueLabel =
                widget->findChild<QLabel *>("textValueLabel")) {
          valueLabel->setText(newText);
        }
        return;
      }
    }
    
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
        double maxValue = values.second;
        const int valueFactor =
            widget->property("valueFactor").isValid()
                ? widget->property("valueFactor").toInt()
                : kTppDoubleSliderFactor;
        const int valueDecimals =
            widget->property("valueDecimals").isValid()
                ? widget->property("valueDecimals").toInt()
                : kTppDoubleSliderDecimals;
        const bool isLinear =
            widget->property("isLinearSlider").isValid()
                ? widget->property("isLinearSlider").toBool()
                : doublePairProp->isLinearSlider();
        const int sliderPos = tppDoubleValueToSliderPos(
            maxValue, slider->minimum(), slider->maximum(), valueFactor,
            isLinear);
        slider->blockSignals(true);
        lineEdit->blockSignals(true);
        slider->setValue(sliderPos);
        lineEdit->setText(QString::number(maxValue, 'f', valueDecimals));
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
        if (QLabel *valueLabel =
                widget->findChild<QLabel *>("intSliderValueLabel")) {
          valueLabel->setText(QString::number(value));
        }
        return;
      }
      
      // Update double property (simple, not pair)
      TDoubleProperty *doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        double value = doubleProp->getValue();
        
        // Check if widget has custom value factor (for MyPaint sliders)
        int valueFactor = widget->property("valueFactor").isValid()
                              ? widget->property("valueFactor").toInt()
                              : kTppDoubleSliderFactor;
        int valueDecimals = widget->property("valueDecimals").isValid()
                                ? widget->property("valueDecimals").toInt()
                                : kTppDoubleSliderDecimals;
        
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
    // === NATIVE DOUBLE SLIDER (DoublePairField) — compact, no Min/Max labels ===
    auto *pairField =
        new TppCompactDoublePairField(container, prop->isMaxRangeLimited());
    pairField->setRange(rangeMin, rangeMax);
    pairField->setValues(std::make_pair(valueMin, valueMax));
    pairField->configureTppLayout(rangeMin, rangeMax, prop->isLinearSlider(),
                                  m_showNumericFields);

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
    const int valueFactor   = kTppDoubleSliderFactor;
    const int valueDecimals = kTppDoubleSliderDecimals;
    const bool isLinear     = prop->isLinearSlider();
    container->setProperty("valueFactor", valueFactor);
    container->setProperty("valueDecimals", valueDecimals);
    container->setProperty("isLinearSlider", isLinear);

    QHBoxLayout *sliderLayout = new QHBoxLayout();
    sliderLayout->setMargin(0);
    sliderLayout->setSpacing(5);

    QLineEdit *lineEdit = new QLineEdit(container);
    styleTppDoubleLineEdit(lineEdit, rangeMin, rangeMax, valueDecimals);
    lineEdit->setText(QString::number(valueMax, 'f', valueDecimals));
    lineEdit->setVisible(m_showNumericFields);
    QDoubleValidator *validator =
        new QDoubleValidator(rangeMin, rangeMax, valueDecimals, lineEdit);
    lineEdit->setValidator(validator);
    sliderLayout->addWidget(lineEdit);

    const int sliderMin = static_cast<int>(std::lround(rangeMin * valueFactor));
    const int sliderMax = static_cast<int>(std::lround(rangeMax * valueFactor));
    QSlider *maxSlider = new QSlider(Qt::Horizontal, container);
    maxSlider->setMinimum(sliderMin);
    maxSlider->setMaximum(sliderMax);
    maxSlider->setValue(tppDoubleValueToSliderPos(valueMax, sliderMin, sliderMax,
                                                  valueFactor, isLinear));
    sliderLayout->addWidget(maxSlider, 1);

    layout->addLayout(sliderLayout);

    connect(maxSlider, &QSlider::valueChanged,
            [lineEdit, maxSlider, valueFactor, valueDecimals, isLinear](int val) {
              const double v = tppSliderPosToDoubleValue(
                  val, maxSlider->minimum(), maxSlider->maximum(), valueFactor,
                  isLinear);
              lineEdit->setText(QString::number(v, 'f', valueDecimals));
            });

    connect(lineEdit, &QLineEdit::editingFinished,
            [maxSlider, lineEdit, valueFactor, isLinear]() {
              const double v = lineEdit->text().toDouble();
              maxSlider->setValue(tppDoubleValueToSliderPos(
                  v, maxSlider->minimum(), maxSlider->maximum(), valueFactor,
                  isLinear));
            });

    connect(maxSlider, &QSlider::valueChanged,
            [this, prop, propName, maxSlider, valueFactor, isLinear](int val) {
              const double newMaxValue = tppSliderPosToDoubleValue(
                  val, maxSlider->minimum(), maxSlider->maximum(), valueFactor,
                  isLinear);
      
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
    // === NATIVE DOUBLE SLIDER (IntPairField) — compact, no Min/Max labels ===
    DVGui::IntPairField *pairField =
        new DVGui::IntPairField(container, prop->isMaxRangeLimited());
    pairField->setRange(rangeMin, rangeMax);
    pairField->setValues(std::make_pair(valueMin, valueMax));
    configureTppIntPairField(pairField, rangeMin, rangeMax, m_showNumericFields);

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
    styleTppIntLineEdit(lineEdit, rangeMin, rangeMax);
    lineEdit->setText(QString::number(valueMax));
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
  
  // Show Icons: icon grid replaces collapsible enum menus (when every option has an icon)
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
  
  // Save preferences according to the action (TEnv)
  if (actionName == "singleSlider") {
    m_useSingleMaxSlider = action->isChecked();
    ToolPropertiesUseSingleMaxSlider = m_useSingleMaxSlider ? 1 : 0;
    needsRefresh = true;
  } 
  else if (actionName == "showLabels") {
    m_showLabels = action->isChecked();
    ToolPropertiesShowLabels = m_showLabels ? 1 : 0;
    needsRefresh = true;
  } 
  else if (actionName == "showNumeric") {
    m_showNumericFields = action->isChecked();
    ToolPropertiesShowNumericFields = m_showNumericFields ? 1 : 0;
    needsRefresh = true;
  }
  else if (actionName == "showBorders") {
    m_showBorders = action->isChecked();
    ToolPropertiesShowBorders = m_showBorders ? 1 : 0;
    updateContainerStylesheet();  // Update container stylesheet immediately
    needsRefresh = true;
  }
  else if (actionName == "showBackgrounds") {
    m_showBackgrounds = action->isChecked();
    ToolPropertiesShowBackgrounds = m_showBackgrounds ? 1 : 0;
    updateContainerStylesheet();  // Update container stylesheet immediately
    needsRefresh = true;
  }
  else if (actionName == "showIcons") {
    m_showIcons = action->isChecked();
    ToolPropertiesShowIcons = m_showIcons ? 1 : 0;
    needsRefresh = true;
  }
  
  // Refresh properties to apply changes
  if (needsRefresh) {
    refreshProperties();
  }
}

