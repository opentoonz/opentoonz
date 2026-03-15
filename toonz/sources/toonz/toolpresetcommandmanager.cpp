#include "toolpresetcommandmanager.h"

#include "menubarcommandids.h"
#include "mainwindow.h"
#include "shortcutpopup.h"
#include "custompaneleditorpopup.h"
#include "tapp.h"

// ToonzLib
#include "toonz/toonzfolders.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"
#include "tpalette.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/mypaint.h"
// ToonzQt
#include "toonzqt/gutil.h"
// ToonzCore
#include "tenv.h"
#include "tsystem.h"
#include "tfilepath.h"
#include "tcolorstyles.h"
// Tools
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tproperty.h"

#include "tstream.h"

#include <QSettings>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <algorithm>  // For std::min, std::max
#include <cmath>      // For sin, pow, log, exp

namespace {

//-----------------------------------------------------------------------------
// Helper function to check if Pressure sensitivity is enabled for the current tool
// When Pressure is DISABLED, we want fixed size behavior (set both Min and Max)
// When Pressure is ENABLED, we want dynamic range behavior (update Max only)
//-----------------------------------------------------------------------------
bool isPressureDisabledForTool(TTool* tool) {
  if (!tool) return true;  // Default to fixed size if no tool
  
  TPropertyGroup* props = tool->getProperties(0);
  if (!props) return true;
  
  // Look for the "Pressure" property (native OpenToonz pressure sensitivity)
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty* prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    if (propName == "Pressure" || propName == "PressureSensitivity") {
      TBoolProperty* pressureProp = dynamic_cast<TBoolProperty*>(prop);
      if (pressureProp) {
        // Return true if Pressure is DISABLED (we want fixed size)
        return !pressureProp->getValue();
      }
    }
  }
  
  // Default: if no Pressure property found, assume fixed size mode
  return true;
}

//-----------------------------------------------------------------------------
// Helper function to detect if current tool is using MyPaint brush
//-----------------------------------------------------------------------------
TMyPaintBrushStyle* getMyPaintBrushStyle() {
  TApp* app = TApp::instance();
  if (!app) return nullptr;
  
  TPaletteHandle* paletteHandle = app->getPaletteController()->getCurrentPalette();
  if (!paletteHandle) return nullptr;
  
  TColorStyle* style = paletteHandle->getStyle();
  if (!style) return nullptr;
  
  return dynamic_cast<TMyPaintBrushStyle*>(style);
}

//-----------------------------------------------------------------------------
// Convert Universal Size (0.5-1000 pixels) to MyPaint ModifierSize (-3 to +3)
// Formula: modifierSize = (log(universalSize/2) - baseRadiusLog) / log(2)
// Where: universalSize/2 converts diameter to radius
//-----------------------------------------------------------------------------
double universalSizeToMyPaintModifier(double universalSize, double baseRadiusLog) {
  if (universalSize <= 0.0) return -3.0;
  
  double targetRadius = universalSize / 2.0;  // Diameter → Radius
  double targetRadiusLog = std::log(targetRadius);
  double modifierSize = (targetRadiusLog - baseRadiusLog) / std::log(2.0);
  
  // Clamp to MyPaint's valid range [-3, 3]
  return std::max(-3.0, std::min(3.0, modifierSize));
}

// Structure to define a brush tool configuration
struct BrushToolConfig {
  QString toolId;           // Tool command ID (e.g., T_Brush)
  TTool::ToolTargetType targetType;  // Target type for the tool
  QString commandPrefix;    // Prefix for command IDs
  QString displayPrefix;    // Prefix for display names
  const char* iconName;     // Default SVG icon name (fallback)
};

// List of supported brush tools
const BrushToolConfig BRUSH_TOOLS[] = {
  { T_Brush, TTool::ToonzImage,   "MI_BrushPreset_TnzRasterBrush_", "[Tnz Raster Brush] ", "palette" },
  { T_Brush, TTool::VectorImage,  "MI_BrushPreset_TnzVectorBrush_", "[Vector Brush] ", "brush" },
  { T_Brush, TTool::RasterImage,  "MI_BrushPreset_RasterBrush_",    "[Raster Brush] ",     "paintbrush" }
};

// List of fixed sizes for universal size commands
const double FIXED_SIZES[] = {
  0.5, 1, 2, 3, 4, 7, 10, 12, 15, 20, 25, 30, 40, 50, 70, 100,
  150, 200, 300, 400, 500, 700, 1000
};

// Helper to find custom icon for a preset or size command
// Looks in: <stuff>/library/brushpreseticons/
// Returns: Full path to icon if found, empty QString otherwise
QString findCustomPresetIcon(const QString& presetName) {
  // Get the brushpreseticons folder path from stuff directory
  TFilePath iconFolder = TEnv::getStuffDir() + "library" + "brushpreseticons";
  
  // Try both .svg and .png extensions
  QStringList extensions;
  extensions << ".svg" << ".png";
  
  for (const QString& ext : extensions) {
    QString iconFileName = presetName + ext;
    TFilePath iconPath = iconFolder + TFilePath(iconFileName.toStdString());
    
    if (TFileStatus(iconPath).doesExist()) {
      return toQString(iconPath);
    }
  }
  
  // No custom icon found
  return QString();
}

// Helper to generate dynamic size icon for Custom Panels.
// Progressive top-cropping: small sizes show a full circle, larger sizes
// have the circle progressively cropped from the TOP until only the bottom
// half remains for the biggest values. Number always below the shape.
QPixmap generateDynamicSizeIcon(double size, int iconSize = 32) {
  QPixmap pixmap(iconSize, iconSize);
  pixmap.fill(Qt::transparent);
  
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  
  // Format the size string (show decimal for 0.5)
  QString sizeStr;
  if (size == (int)size)
    sizeStr = QString::number((int)size);
  else
    sizeStr = QString::number(size, 'g', 3);
  
  QFont numFont;
  int fontSize = qMax(7, iconSize / 4);
  numFont.setPixelSize(fontSize);
  numFont.setBold(true);
  QFontMetrics fm(numFont);
  int textH = fm.height();
  
  // Available height above text
  int availH = iconSize - textH - 2;
  // Allow circle diameter to exceed availH (it will be cropped from top)
  int maxDiam = qMin(iconSize - 2, (int)(availH * 1.8));
  maxDiam = qMax(6, maxDiam);
  
  // Variable diameter (log scale from 0.5..1000)
  double minSz = 0.5, maxSz = 1000.0;
  double logMin = std::log(minSz + 1.0);
  double logMax = std::log(maxSz + 1.0);
  double logVal = std::log(qMax(size, minSz) + 1.0);
  double ratio = (logVal - logMin) / (logMax - logMin);
  ratio = qMax(0.0, qMin(1.0, ratio));
  int circleDiam = (int)(maxDiam * (0.15 + 0.85 * ratio));
  circleDiam = qMax(4, circleDiam);
  
  int cx = iconSize / 2;
  // Circle bottom sits at the text baseline
  int circleBottom = availH;
  int circleTop = circleBottom - circleDiam;
  int clipTop = 0;  // Top of the icon area
  
  // Draw the circle. If it overflows the top, apply a horizontal clip rect
  // so only the top is hidden — sides stay fully intact (horizontal mask effect).
  QRect circleRect(cx - circleDiam / 2, circleTop, circleDiam, circleDiam);
  
  if (circleTop < clipTop) {
    painter.setClipRect(QRect(0, clipTop, iconSize, circleBottom - clipTop));
  }
  
  painter.setPen(QPen(QColor(160, 160, 160), 1));
  painter.setBrush(QColor(255, 255, 255));
  painter.drawEllipse(circleRect);
  
  if (circleTop < clipTop) {
    painter.setClipping(false);
  }
  
  // Number below the shape
  painter.setPen(QColor(220, 220, 220));
  painter.setFont(numFont);
  QRect textRect(0, circleBottom + 1, iconSize, textH + 2);
  painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, sizeStr);
  
  return pixmap;
}

// Check if user preference is set to use sample strokes
bool useSampleStrokesEnabled() {
  QSettings settings;
  return settings.value("BrushPresetPanel/useSampleStrokes", false).toBool();
}

// Helper structure to hold minimal render data for a brush preset
struct BrushRenderInfo {
  double minThickness = 1.0;
  double maxThickness = 5.0;
  double hardness = 100.0;
  double minOpacity = 100.0;
  double maxOpacity = 100.0;
  bool isPencil = false;
  bool found = false;
};

// Load render info from the preset .txt file for a given preset and tool type.
// toolType is one of: "vector", "toonzraster", "raster"
BrushRenderInfo loadBrushRenderInfo(const QString &presetName, const QString &toolType) {
  BrushRenderInfo info;
  TFilePath presetFilePath;
  if (toolType == "vector")
    presetFilePath = TEnv::getConfigDir() + "brush_vector.txt";
  else if (toolType == "toonzraster")
    presetFilePath = TEnv::getConfigDir() + "brush_toonzraster.txt";
  else if (toolType == "raster")
    presetFilePath = TEnv::getConfigDir() + "brush_raster.txt";
  else
    return info;
  
  try {
    TIStream is(presetFilePath);
    std::string tagName;
    std::wstring wPresetName = presetName.toStdWString();
    while (is.matchTag(tagName)) {
      if (tagName == "brushes") {
        while (is.matchTag(tagName)) {
          if (tagName == "brush") {
            std::wstring name;
            double minT = 1.0, maxT = 5.0, h = 100.0, minO = 100.0, maxO = 100.0;
            int pencil = 0;
            while (is.matchTag(tagName)) {
              if (tagName == "Name") { is >> name; is.matchEndTag(); }
              else if (tagName == "Thickness") { is >> minT >> maxT; is.matchEndTag(); }
              else if (tagName == "Hardness") { is >> h; is.matchEndTag(); }
              else if (tagName == "Opacity") { is >> minO >> maxO; is.matchEndTag(); }
              else if (tagName == "Pencil") { is >> pencil; is.matchEndTag(); }
              else is.skipCurrentTag();
            }
            if (name == wPresetName) {
              info.minThickness = minT;
              info.maxThickness = maxT;
              info.hardness = h;
              info.minOpacity = minO;
              info.maxOpacity = maxO;
              info.isPencil = (pencil != 0);
              info.found = true;
              is.matchEndTag();
              return info;
            }
            is.matchEndTag();
          } else {
            is.skipCurrentTag();
          }
        }
        is.matchEndTag();
      } else {
        is.skipCurrentTag();
      }
    }
  } catch (...) {}
  return info;
}

// Map a command prefix to the corresponding brush tool type string
// for loading preset render info from the correct .txt file.
QString toolTypeFromPrefix(const QString &prefix) {
  if (prefix.contains("TnzRasterBrush"))  return "toonzraster";
  if (prefix.contains("TnzVectorBrush"))  return "vector";
  if (prefix.contains("RasterBrush"))     return "raster";
  return "";
}

// Generate a dynamic sample-stroke icon for a brush preset (Custom Panels).
// Smooth sine-wave double undulation with quadratic-tapered pointed tips.
// Taper adapts to min/max thickness ratio of the brush.
QPixmap generateDynamicBrushIcon(const BrushRenderInfo &info, int iconSize = 32) {
  QPixmap pixmap(iconSize, iconSize);
  pixmap.fill(Qt::transparent);
  
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  
  double avgThick = (info.minThickness + info.maxThickness) / 2.0;
  double avgOpacity = (info.minOpacity + info.maxOpacity) / 2.0;
  
  double maxH = iconSize * 0.50;
  double strokeH = qMax(3.0, qMin(maxH, avgThick * iconSize / 12.0));
  strokeH = qMax(strokeH, 4.0);
  
  // Thickness ratio controls tip bluntness (0 = very pointed, 1 = uniform)
  double thickRatio = (info.maxThickness > 0.01)
      ? (info.minThickness / info.maxThickness) : 0.0;
  thickRatio = qMax(0.0, qMin(1.0, thickRatio));
  
  // If opacity is 0 (tool doesn't support opacity, e.g. Toonz Raster),
  // treat as fully opaque since the brush always paints at full intensity.
  double alpha;
  if (avgOpacity < 0.01)
    alpha = 1.0;
  else
    alpha = qMax(0.35, qMin(1.0, avgOpacity / 100.0));
  // Hardness influences stroke color: low=gray, high=white (matching brushpresetpanel)
  double hardFrac = qMax(0.0, qMin(1.0, info.hardness / 100.0));
  int lowVal = 140;
  int highVal = 255;
  int val = lowVal + (int)((highVal - lowVal) * hardFrac);
  QColor strokeColor(val, val, val);
  strokeColor.setAlphaF(alpha);
  
  double x0 = 1.0, x1 = iconSize - 1.0;
  double totalW = x1 - x0;
  double cy = iconSize / 2.0;
  double waveAmp = iconSize * 0.12;
  
  const int N = 60;
  QPolygonF topEdge, botEdge;
  for (int i = 0; i <= N; ++i) {
    double t = (double)i / N;
    double x = x0 + totalW * t;
    
    // Sine wave centerline (one full period: hump up then dip down)
    double angle = t * 2.0 * 3.14159265358979;
    double centerY = cy - std::sin(angle) * waveAmp;
    
    // Quadratic taper blended with thickness ratio
    double taper;
    if (t < 0.5) { double u = t * 2.0; taper = u * u; }
    else { double u = (1.0 - t) * 2.0; taper = u * u; }
    taper = thickRatio + (1.0 - thickRatio) * taper;
    if (info.hardness < 50) taper = std::pow(taper, 0.7);
    if (info.hardness > 80) taper = std::pow(taper, 1.3);
    
    double halfH = strokeH * taper * 0.5;
    topEdge << QPointF(x, centerY - halfH);
    botEdge.prepend(QPointF(x, centerY + halfH));
  }
  
  QPolygonF outline = topEdge + botEdge;
  QPainterPath path;
  path.addPolygon(outline);
  path.closeSubpath();
  
  painter.setPen(Qt::NoPen);
  painter.setBrush(strokeColor);
  painter.drawPath(path);
  
  // Soft glow for soft brushes
  if (info.hardness < 50 && !info.isPencil) {
    QColor glow = strokeColor;
    glow.setAlphaF(alpha * 0.12);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(glow, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
  }
  
  return pixmap;
}

// Helper to get presets from a tool's properties
QList<QString> getPresetsFromTool(const QString& toolId, TTool::ToolTargetType targetType) {
  QList<QString> presetNames;
  
  // Get the tool by name
  TTool* tool = TTool::getTool(toolId.toStdString(), targetType);
  if (!tool) return presetNames;
  
  // Get the tool's properties
  TPropertyGroup* props = tool->getProperties(0);
  if (!props) return presetNames;
  
  // Find the "Preset:" property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty* prop = props->getProperty(i);
    if (prop && prop->getName() == "Preset:") {
      TEnumProperty* enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        const TEnumProperty::Range& range = enumProp->getRange();
        for (const std::wstring& presetName : range) {
          // Skip the "<custom>" entry
          if (presetName == L"<custom>") continue;
          presetNames.append(QString::fromStdWString(presetName));
        }
      }
      break;
    }
  }
  
  return presetNames;
}

// Command handler for brush preset activation
class BrushPresetHandler : public CommandHandlerInterface {
  QString m_toolId;
  QString m_presetName;
  
public:
  BrushPresetHandler(const QString& toolId, const QString& presetName)
    : m_toolId(toolId), m_presetName(presetName) {}
    
  void execute() override {
    ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
    if (!toolHandle) return;
    
    // Activate the tool if not already active
    if (toolHandle->getRequestedToolName() != m_toolId) {
      toolHandle->setTool(m_toolId);
    }
    
    // Get the current tool
    TTool* tool = toolHandle->getTool();
    if (!tool) return;
    
    // Access tool properties to set the preset
    TPropertyGroup* props = tool->getProperties(0);
    if (!props) return;
    
    // Find the "Preset:" property
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty* prop = props->getProperty(i);
      if (prop && prop->getName() == "Preset:") {
        // Cast to TEnumProperty and set the value
        TEnumProperty* enumProp = dynamic_cast<TEnumProperty*>(prop);
        if (enumProp) {
          std::wstring presetValue = m_presetName.toStdWString();
          // Check if this preset value exists in the enum
          if (enumProp->isValue(presetValue)) {
            enumProp->setValue(presetValue);
            // Notify the tool of property change
            tool->onPropertyChanged("Preset:");
            
            // Notify the tool handle that the tool has changed
            // This will emit toolChanged() signal and update all connected panels
            toolHandle->notifyToolChanged();
            
            // Update checked states of all preset/size actions
            ToolPresetCommandManager::instance()->updateCheckedStates();
          }
        }
        break;
      }
    }
  }
};

// Command handler for universal size change
// Secured and UI-synchronized implementation
class ToolSizeHandler : public CommandHandlerInterface {
  double m_size;
  
public:
  ToolSizeHandler(double size) : m_size(size) {}
  
  void execute() override {
    try {
      // === STEP 1: Security checks ===
      TApp* app = TApp::instance();
      if (!app) return;
      
      ToolHandle* toolHandle = app->getCurrentTool();
      if (!toolHandle) return;
      
      TTool* tool = toolHandle->getTool();
      if (!tool) return;
      
      TPropertyGroup* props = tool->getProperties(0);
      if (!props) return;
      
      // === STEP 1.5: Check for MyPaint brush and handle ModifierSize ===
      TMyPaintBrushStyle* mypaintStyle = getMyPaintBrushStyle();
      if (mypaintStyle) {
        // Look for ModifierSize property (MyPaint's logarithmic size control)
        for (int i = 0; i < props->getPropertyCount(); ++i) {
          TProperty* prop = props->getProperty(i);
          if (!prop) continue;
          
          std::string propName = prop->getName();
          if (propName == "ModifierSize") {
            TDoubleProperty* modifierProp = dynamic_cast<TDoubleProperty*>(prop);
            if (modifierProp) {
              // Get MyPaint brush base radius (logarithmic scale)
              double baseRadiusLog = mypaintStyle->getBrush().getBaseValue(
                  MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
              
              // Convert Universal Size (0.5-1000) to MyPaint ModifierSize (-3 to +3)
              double newModifierSize = universalSizeToMyPaintModifier(m_size, baseRadiusLog);
              
              // Clamp to property range
              double minVal = modifierProp->getRange().first;
              double maxVal = modifierProp->getRange().second;
              newModifierSize = std::max(minVal, std::min(newModifierSize, maxVal));
              
              // Apply the new modifier size
              modifierProp->setValue(newModifierSize);
              tool->onPropertyChanged(propName);
              
              // Notify tool change
              if (toolHandle) {
                toolHandle->notifyToolChanged();
              }
              
              // Update checked states
              ToolPresetCommandManager::instance()->updateCheckedStates();
              return;  // MyPaint handled, exit
            }
          }
        }
      }
      
      // === STEP 2: Prioritized property search (for non-MyPaint brushes) ===
      // Search with priority order to avoid targeting wrong properties (e.g., Hardness instead of Size)
      bool propertyModified = false;
      
      // Priority 1: Exact matches for common size property names
      const std::vector<std::string> priorityNames = {
        "Size:", "Thickness:", "Size", "Thickness", 
        "Brush Size:", "Min:", "Max:"
      };
      
      for (const std::string& targetName : priorityNames) {
        for (int i = 0; i < props->getPropertyCount(); ++i) {
          TProperty* prop = props->getProperty(i);
          if (!prop) continue;
          
          std::string propName = prop->getName();
          if (propName != targetName) continue;
          
          // Try TIntProperty (integer value - used by Paint Brush, Eraser)
          TIntProperty* intProp = dynamic_cast<TIntProperty*>(prop);
          if (intProp) {
            int minVal = intProp->getRange().first;
            int maxVal = intProp->getRange().second;
            int clampedSize = std::max(minVal, std::min(static_cast<int>(m_size), maxVal));
            
            intProp->setValue(clampedSize);
            tool->onPropertyChanged(targetName);
            propertyModified = true;
            break;
          }
          
          // Try TDoubleProperty (double value)
          TDoubleProperty* doubleProp = dynamic_cast<TDoubleProperty*>(prop);
          if (doubleProp) {
            double minVal = doubleProp->getRange().first;
            double maxVal = doubleProp->getRange().second;
            double clampedSize = std::max(minVal, std::min(m_size, maxVal));
            
            doubleProp->setValue(clampedSize);
            tool->onPropertyChanged(targetName);
            propertyModified = true;
            break;
          }
          
          // Try TIntPairProperty (integer min/max range - used by FullColor Brush)
          TIntPairProperty* intPairProp = dynamic_cast<TIntPairProperty*>(prop);
          if (intPairProp) {
            int minVal = intPairProp->getRange().first;
            int maxVal = intPairProp->getRange().second;
            int clampedSize = std::max(minVal, std::min(static_cast<int>(m_size), maxVal));
            
            // Use native Pressure property to determine behavior
            // Pressure OFF → Fixed size (set both min and max)
            // Pressure ON  → Dynamic range (update max only, with guard)
            if (isPressureDisabledForTool(tool)) {
              intPairProp->setValue(TIntPairProperty::Value(clampedSize, clampedSize));
            } else {
              // Dynamic range: Update Max, preserve Min
              // Guard: if new Max < current Min, push Min down (same logic as IntPairField)
              int currentMin = intPairProp->getValue().first;
              int newMax = clampedSize;
              if (newMax < currentMin) {
                currentMin = newMax;  // Push Min down to match Max
              }
              intPairProp->setValue(TIntPairProperty::Value(currentMin, newMax));
            }
            tool->onPropertyChanged(targetName);
            propertyModified = true;
            break;
          }
          
          // Try TDoublePairProperty (double min/max range)
          TDoublePairProperty* pairProp = dynamic_cast<TDoublePairProperty*>(prop);
          if (pairProp) {
            double minVal = pairProp->getRange().first;
            double maxVal = pairProp->getRange().second;
            double clampedSize = std::max(minVal, std::min(m_size, maxVal));
            
            // Use native Pressure property to determine behavior
            // Pressure OFF → Fixed size (set both min and max)
            // Pressure ON  → Dynamic range (update max only, with guard)
            if (isPressureDisabledForTool(tool)) {
              pairProp->setValue(TDoublePairProperty::Value(clampedSize, clampedSize));
            } else {
              // Dynamic range: Update Max, preserve Min
              // Guard: if new Max < current Min, push Min down (same logic as DoublePairField)
              double currentMin = pairProp->getValue().first;
              double newMax = clampedSize;
              if (newMax < currentMin) {
                currentMin = newMax;  // Push Min down to match Max
              }
              pairProp->setValue(TDoublePairProperty::Value(currentMin, newMax));
            }
            tool->onPropertyChanged(targetName);
            propertyModified = true;
            break;
          }
        }
        if (propertyModified) break;
      }
      
      // Priority 2: If no exact match, search by keywords (but EXCLUDE Hardness)
      if (!propertyModified) {
        for (int i = 0; i < props->getPropertyCount(); ++i) {
          TProperty* prop = props->getProperty(i);
          if (!prop) continue;
          
          std::string propName = prop->getName();
          QString propNameLower = QString::fromStdString(propName).toLower();
          
          // EXCLUDE Hardness explicitly (it's not a size property)
          if (propNameLower.contains("hardness")) continue;
          
          // Check for size-related keywords
          bool isSizeProperty = propNameLower.contains("size") ||
                               propNameLower.contains("thickness") ||
                               propNameLower.contains("radius") ||
                               propNameLower.contains("width");
          
          if (!isSizeProperty) continue;
          
          // Try TIntProperty (integer value)
          TIntProperty* intProp = dynamic_cast<TIntProperty*>(prop);
          if (intProp) {
            int minVal = intProp->getRange().first;
            int maxVal = intProp->getRange().second;
            int clampedSize = std::max(minVal, std::min(static_cast<int>(m_size), maxVal));
            
            intProp->setValue(clampedSize);
            tool->onPropertyChanged(propName);
            propertyModified = true;
            break;
          }
          
          // Try TDoubleProperty (double value)
          TDoubleProperty* doubleProp = dynamic_cast<TDoubleProperty*>(prop);
          if (doubleProp) {
            double minVal = doubleProp->getRange().first;
            double maxVal = doubleProp->getRange().second;
            double clampedSize = std::max(minVal, std::min(m_size, maxVal));
            
            doubleProp->setValue(clampedSize);
            tool->onPropertyChanged(propName);
            propertyModified = true;
            break;
          }
          
          // Try TIntPairProperty (integer min/max range)
          TIntPairProperty* intPairProp = dynamic_cast<TIntPairProperty*>(prop);
          if (intPairProp) {
            int minVal = intPairProp->getRange().first;
            int maxVal = intPairProp->getRange().second;
            int clampedSize = std::max(minVal, std::min(static_cast<int>(m_size), maxVal));
            
            // Use native Pressure property to determine behavior
            // Pressure OFF → Fixed size (set both min and max)
            // Pressure ON  → Dynamic range (update max only, with guard)
            if (isPressureDisabledForTool(tool)) {
              intPairProp->setValue(TIntPairProperty::Value(clampedSize, clampedSize));
            } else {
              // Dynamic range: Update Max, preserve Min
              // Guard: if new Max < current Min, push Min down (same logic as IntPairField)
              int currentMin = intPairProp->getValue().first;
              int newMax = clampedSize;
              if (newMax < currentMin) {
                currentMin = newMax;  // Push Min down to match Max
              }
              intPairProp->setValue(TIntPairProperty::Value(currentMin, newMax));
            }
            tool->onPropertyChanged(propName);
            propertyModified = true;
            break;
          }
          
          // Try TDoublePairProperty (double min/max range)
          TDoublePairProperty* pairProp = dynamic_cast<TDoublePairProperty*>(prop);
          if (pairProp) {
            double minVal = pairProp->getRange().first;
            double maxVal = pairProp->getRange().second;
            double clampedSize = std::max(minVal, std::min(m_size, maxVal));
            
            // Use native Pressure property to determine behavior
            // Pressure OFF → Fixed size (set both min and max)
            // Pressure ON  → Dynamic range (update max only, with guard)
            if (isPressureDisabledForTool(tool)) {
              pairProp->setValue(TDoublePairProperty::Value(clampedSize, clampedSize));
            } else {
              // Dynamic range: Update Max, preserve Min
              // Guard: if new Max < current Min, push Min down (same logic as DoublePairField)
              double currentMin = pairProp->getValue().first;
              double newMax = clampedSize;
              if (newMax < currentMin) {
                currentMin = newMax;  // Push Min down to match Max
              }
              pairProp->setValue(TDoublePairProperty::Value(currentMin, newMax));
            }
            tool->onPropertyChanged(propName);
            propertyModified = true;
            break;
          }
        }
      }
      
      // === STEP 3: UI Synchronization ===
      if (propertyModified) {
        // Notify the tool system that the tool has changed
        // This will update the cursor and the tool options box
        toolHandle->notifyToolChanged();
        
        // Update checked states of all preset/size actions
        ToolPresetCommandManager::instance()->updateCheckedStates();
      }
      
    } catch (...) {
      // Fail silently to prevent crashes
    }
  }
};

}  // namespace

//-----------------------------------------------------------------------------

ToolPresetCommandManager::ToolPresetCommandManager()
    : QObject(), m_initialized(false), m_presetActionGroup(nullptr), m_sizeActionGroup(nullptr) {
  // Create action groups for mutual exclusivity
  m_presetActionGroup = new QActionGroup(this);
  m_presetActionGroup->setExclusive(true);
  
  m_sizeActionGroup = new QActionGroup(this);
  m_sizeActionGroup->setExclusive(true);
}

//-----------------------------------------------------------------------------

ToolPresetCommandManager* ToolPresetCommandManager::instance() {
  static ToolPresetCommandManager _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void ToolPresetCommandManager::initialize() {
  if (m_initialized) return;
  m_initialized = true;
  
  // Connect to ToolHandle signals to update checked states
  ToolHandle* toolHandle = TApp::instance()->getCurrentTool();
  if (toolHandle) {
    connect(toolHandle, SIGNAL(toolSwitched()), this, SLOT(updateCheckedStates()));
    connect(toolHandle, SIGNAL(toolChanged()), this, SLOT(updateCheckedStates()));
  }
}

//-----------------------------------------------------------------------------

void ToolPresetCommandManager::registerToolPresetCommands() {
  MainWindow* mainWindow = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (!mainWindow) return;
  
  // === Auto-create icons directory if it doesn't exist ===
  TFilePath iconFolder = TEnv::getStuffDir() + "library" + "brushpreseticons";
  if (!TFileStatus(iconFolder).doesExist()) {
    try {
      TSystem::mkDir(iconFolder);
    } catch (...) {
      // If directory creation fails, continue anyway (icons will use defaults)
    }
  }
  
  QSet<QString> currentPresets;
  
  // Process all supported brush tools
  for (const BrushToolConfig& config : BRUSH_TOOLS) {
    QList<QString> presets = getPresetsFromTool(config.toolId, config.targetType);
    
    // Register new presets for this tool
    for (const QString& presetName : presets) {
      QString commandId = config.commandPrefix + presetName;
      currentPresets.insert(commandId);
      
      // Check if already registered
      QAction* existingAction = CommandManager::instance()->getAction(
          commandId.toStdString().c_str(), false);
      if (existingAction) {
        // Update icon - check for custom icon first, then dynamic, then default
        QString customIconPath = findCustomPresetIcon(presetName);
        
        if (!customIconPath.isEmpty()) {
          existingAction->setIcon(QIcon(customIconPath));
        } else if (useSampleStrokesEnabled()) {
          // Dynamic sample stroke icon
          QString toolType = toolTypeFromPrefix(config.commandPrefix);
          BrushRenderInfo rInfo = loadBrushRenderInfo(presetName, toolType);
          if (rInfo.found) {
            QPixmap px = generateDynamicBrushIcon(rInfo, 32);
            existingAction->setIcon(QIcon(px));
          } else if (config.iconName && *config.iconName) {
            existingAction->setIcon(createQIcon(config.iconName, true));
          }
        } else if (config.iconName && *config.iconName) {
          existingAction->setIcon(createQIcon(config.iconName, true));
        }
        continue;
      }
      
      QString displayName = config.displayPrefix + presetName;
      QAction* action = new DVAction(displayName, mainWindow);
      
      // Make the action checkable for visual feedback in Custom Panels
      action->setCheckable(true);
      action->setChecked(false);  // Default unchecked
      
      // Priority 1: Custom icon  ->  2: Dynamic stroke  ->  3: Default icon
      QString customIconPath = findCustomPresetIcon(presetName);
      const char* iconNameToRegister = config.iconName;
      
      if (!customIconPath.isEmpty()) {
        action->setIcon(QIcon(customIconPath));
      } else if (useSampleStrokesEnabled()) {
        // Generate a sample-stroke icon dynamically
        QString toolType = toolTypeFromPrefix(config.commandPrefix);
        BrushRenderInfo rInfo = loadBrushRenderInfo(presetName, toolType);
        if (rInfo.found) {
          QPixmap px = generateDynamicBrushIcon(rInfo, 32);
          action->setIcon(QIcon(px));
        } else if (config.iconName && *config.iconName) {
          action->setIcon(createQIcon(config.iconName));
        }
      } else if (config.iconName && *config.iconName) {
        action->setIcon(createQIcon(config.iconName));
      }
      
      mainWindow->addAction(action);
      
      // Add action to preset action group for mutual exclusivity
      if (m_presetActionGroup) {
        m_presetActionGroup->addAction(action);
      }
      
      CommandManager::instance()->define(
          commandId.toStdString().c_str(),
          BrushPresetCommandType,  // Tool Modifiers/Brush Presets category
          "",
          action,
          iconNameToRegister);
      
      CommandManager::instance()->setHandler(
          commandId.toStdString().c_str(),
          new BrushPresetHandler(config.toolId, presetName));
    }
  }
  
  // Mark obsolete presets as deleted (they'll be removed on restart)
  QSet<QString> obsoletePresets = m_registeredPresetIds - currentPresets;
  for (const QString& commandId : obsoletePresets) {
    QAction* action = CommandManager::instance()->getAction(
        commandId.toStdString().c_str(), false);
    if (action) {
      // Disable the command so it can't be triggered
      CommandManager::instance()->enable(commandId.toStdString().c_str(), false);
      
      // Hide the action so it disappears from ShortcutPopup and CustomPanelEditor trees
      action->setVisible(false);
      
      // Rename to indicate it's deleted
      QString oldText = action->iconText();
      if (!oldText.startsWith("(Deleted) ")) {
        action->setIconText("(Deleted) " + oldText);
      }
      
      // Remove from main window to prevent keyboard shortcuts
      mainWindow->removeAction(action);
    }
  }
  
  // Update the registered set
  m_registeredPresetIds = currentPresets;
  
  // Notify open popups to refresh their command trees
  ShortcutPopup::refreshIfOpen();
  CustomPanelEditorPopup::refreshCommandTreeIfOpen();
  
  // Load shortcuts if any were previously saved
  TFilePath shortcutsFile = ToonzFolder::getMyModuleDir() + TFilePath("shortcuts.ini");
  if (!TFileStatus(shortcutsFile).doesExist()) return;
  
  QSettings settings(toQString(shortcutsFile), QSettings::IniFormat);
  settings.beginGroup("shortcuts");
  
  for (const QString& commandId : m_registeredPresetIds) {
    QString savedShortcut = settings.value(commandId, "").toString();
    if (!savedShortcut.isEmpty()) {
      QAction* action = CommandManager::instance()->getAction(
          commandId.toStdString().c_str(), false);
      if (action) {
        action->setShortcut(QKeySequence(savedShortcut));
      }
    }
  }
  
  settings.endGroup();
}

//-----------------------------------------------------------------------------

void ToolPresetCommandManager::refreshPresetCommands() {
  // Simply re-register all commands (new ones will be added, existing ones skipped)
  registerToolPresetCommands();
}

//-----------------------------------------------------------------------------

void ToolPresetCommandManager::registerSizeCommands() {
  MainWindow* mainWindow = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (!mainWindow) return;
  
  // === Auto-create icons directory if it doesn't exist ===
  TFilePath iconFolder = TEnv::getStuffDir() + "library" + "brushpreseticons";
  if (!TFileStatus(iconFolder).doesExist()) {
    try {
      TSystem::mkDir(iconFolder);
    } catch (...) {
      // If directory creation fails, continue anyway (icons will use defaults)
    }
  }
  
  QSet<QString> currentSizes;
  
  // Register commands for each fixed size
  const int numSizes = sizeof(FIXED_SIZES) / sizeof(FIXED_SIZES[0]);
  for (int i = 0; i < numSizes; ++i) {
    double size = FIXED_SIZES[i];
    
    // Create command ID and display name
    QString sizeStr = QString::number(size);
    if (sizeStr.contains('.')) {
      sizeStr.replace('.', '_');  // Replace decimal point with underscore for ID
    }
    QString commandId = "MI_ToolSize_" + sizeStr;
    currentSizes.insert(commandId);
    
    // Check if already registered
    QAction* existingAction = CommandManager::instance()->getAction(
        commandId.toStdString().c_str(), false);
    if (existingAction) {
      // Update icon if custom one is available, or use dynamic rendering
      QString iconName = "size_" + QString::number(size);
      QString customIconPath = findCustomPresetIcon(iconName);
      
      if (!customIconPath.isEmpty()) {
        existingAction->setIcon(QIcon(customIconPath));
      } else if (useSampleStrokesEnabled()) {
        // Use dynamic rendering
        QPixmap pixmap = generateDynamicSizeIcon(size, 32);
        existingAction->setIcon(QIcon(pixmap));
      }
      continue;
    }
    
    // Create display name
    QString displayName = "[Size] " + QString::number(size);
    QAction* action = new DVAction(displayName, mainWindow);
    
    // Make the action checkable for visual feedback in Custom Panels
    action->setCheckable(true);
    action->setChecked(false);  // Default unchecked
    
    // Try to find custom icon (e.g., size_50.svg)
    QString iconName = "size_" + QString::number(size);
    QString customIconPath = findCustomPresetIcon(iconName);
    
    if (!customIconPath.isEmpty()) {
      // Custom icon found - use it
      action->setIcon(QIcon(customIconPath));
    } else if (useSampleStrokesEnabled()) {
      // Use dynamic rendering (circle with number)
      QPixmap pixmap = generateDynamicSizeIcon(size, 32);
      action->setIcon(QIcon(pixmap));
    } else {
      // Use generic thickness icon
      action->setIcon(createQIcon("thickness"));
    }
    
    mainWindow->addAction(action);
    
    // Add action to size action group for mutual exclusivity
    if (m_sizeActionGroup) {
      m_sizeActionGroup->addAction(action);
    }
    
    CommandManager::instance()->define(
        commandId.toStdString().c_str(),
        BrushSizeCommandType,  // Tool Modifiers/Brush Sizes category
        "",
        action,
        "thickness");  // Default icon name
    
    CommandManager::instance()->setHandler(
        commandId.toStdString().c_str(),
        new ToolSizeHandler(size));
  }
  
  m_registeredSizeIds = currentSizes;
}

//-----------------------------------------------------------------------------

void ToolPresetCommandManager::refreshSizeCommands() {
  // Re-register size commands (updates icons for dynamic rendering mode)
  registerSizeCommands();
  // Also refresh brush preset icons to match the current rendering mode
  registerToolPresetCommands();
}

//-----------------------------------------------------------------------------

void ToolPresetCommandManager::updateCheckedStates() {
  // Get current tool
  TTool* tool = TApp::instance()->getCurrentTool()->getTool();
  if (!tool) {
    // No tool active, uncheck all
    for (const QString& presetId : m_registeredPresetIds) {
      QAction* action = CommandManager::instance()->getAction(presetId.toStdString().c_str(), false);
      if (action && action->isCheckable()) action->setChecked(false);
    }
    for (const QString& sizeId : m_registeredSizeIds) {
      QAction* action = CommandManager::instance()->getAction(sizeId.toStdString().c_str(), false);
      if (action && action->isCheckable()) action->setChecked(false);
    }
    return;
  }
  
  TPropertyGroup* props = tool->getProperties(0);
  if (!props) return;
  
  // === Update Preset Actions ===
  QString currentPreset;
  
  // Find the "Preset:" property to get the currently selected preset
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty* prop = props->getProperty(i);
    if (!prop) continue;
    
    std::string propName = prop->getName();
    if (propName == "Preset:") {
      TEnumProperty* enumProp = dynamic_cast<TEnumProperty*>(prop);
      if (enumProp) {
        currentPreset = QString::fromStdWString(enumProp->getValue());
        break;
      }
    }
  }
  
  // Update all preset actions
  for (const QString& presetId : m_registeredPresetIds) {
    QAction* action = CommandManager::instance()->getAction(presetId.toStdString().c_str(), false);
    if (!action || !action->isCheckable()) continue;
    
    // Extract preset name from command ID
    // Format: "MI_BrushPreset_TnzVectorBrush_PresetName" -> "PresetName"
    QString presetName;
    if (presetId.startsWith("MI_BrushPreset_TnzVectorBrush_")) {
      presetName = presetId.mid(QString("MI_BrushPreset_TnzVectorBrush_").length());
    } else if (presetId.startsWith("MI_BrushPreset_TnzRasterBrush_")) {
      presetName = presetId.mid(QString("MI_BrushPreset_TnzRasterBrush_").length());
    } else if (presetId.startsWith("MI_BrushPreset_RasterBrush_")) {
      presetName = presetId.mid(QString("MI_BrushPreset_RasterBrush_").length());
    }
    
    // Check if this preset matches the current one
    bool shouldBeChecked = (!presetName.isEmpty() && presetName == currentPreset);
    action->setChecked(shouldBeChecked);
  }
  
  // === Update Size Actions ===
  double currentSize = -1.0;
  
  // Check if using MyPaint brush - need to convert ModifierSize to actual size
  TMyPaintBrushStyle* mypaintStyle = getMyPaintBrushStyle();
  if (mypaintStyle) {
    // Look for ModifierSize property
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty* prop = props->getProperty(i);
      if (!prop) continue;
      
      std::string propName = prop->getName();
      if (propName == "ModifierSize") {
        TDoubleProperty* modifierProp = dynamic_cast<TDoubleProperty*>(prop);
        if (modifierProp) {
          // Get current modifier value
          double modifierSize = modifierProp->getValue();
          
          // Get MyPaint brush base radius (logarithmic scale)
          double baseRadiusLog = mypaintStyle->getBrush().getBaseValue(
              MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
          
          // Calculate actual radius: radius = exp(baseRadiusLog + modifierSize * log(2))
          double radiusLog = baseRadiusLog + modifierSize * std::log(2.0);
          double radius = std::exp(radiusLog);
          
          // Convert radius to diameter (Universal Size format)
          currentSize = radius * 2.0;
          break;
        }
      }
    }
  } else {
    // Find size-related properties (for non-MyPaint brushes)
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty* prop = props->getProperty(i);
      if (!prop) continue;
      
      std::string propName = prop->getName();
      QString propNameLower = QString::fromStdString(propName).toLower();
      
      // Check for size-related properties (prioritize exact matches)
      bool isSizeProperty = (propName == "Size" || propName == "Size:" ||
                            propName == "Thickness" || propName == "Thickness:" ||
                            propNameLower.contains("size") ||
                            propNameLower.contains("thickness"));
      
      if (!isSizeProperty) continue;
      
      // Try to get the size value
      TDoubleProperty* doubleProp = dynamic_cast<TDoubleProperty*>(prop);
      if (doubleProp) {
        currentSize = doubleProp->getValue();
        break;
      }
      
      TIntProperty* intProp = dynamic_cast<TIntProperty*>(prop);
      if (intProp) {
        currentSize = static_cast<double>(intProp->getValue());
        break;
      }
      
      TDoublePairProperty* pairProp = dynamic_cast<TDoublePairProperty*>(prop);
      if (pairProp) {
        // CRITICAL FIX: Use MAX value for size button checked state
        // In Range Mode (Sync disabled), buttons represent the Max value, not Min
        // In Fixed Mode (Sync enabled), Min == Max anyway, so this works for both cases
        currentSize = pairProp->getValue().second;  // Use max value
        break;
      }
      
      TIntPairProperty* intPairProp = dynamic_cast<TIntPairProperty*>(prop);
      if (intPairProp) {
        // CRITICAL FIX: Use MAX value for size button checked state
        currentSize = static_cast<double>(intPairProp->getValue().second);  // Use max value
        break;
      }
    }
  }
  
  // Update all size actions
  for (const QString& sizeId : m_registeredSizeIds) {
    QAction* action = CommandManager::instance()->getAction(sizeId.toStdString().c_str(), false);
    if (!action || !action->isCheckable()) continue;
    
    // Extract size value from command ID
    // Format: "MI_ToolSize_50" or "MI_ToolSize_0_5" -> 50.0 or 0.5
    QString sizeStr = sizeId.mid(QString("MI_ToolSize_").length());
    sizeStr.replace('_', '.');  // Replace underscore with decimal point
    bool ok = false;
    double actionSize = sizeStr.toDouble(&ok);
    
    if (!ok) continue;
    
    // Check if this size matches the current one (with small tolerance for floating point)
    bool shouldBeChecked = (currentSize >= 0.0 && std::abs(currentSize - actionSize) < 0.01);
    action->setChecked(shouldBeChecked);
  }
}

