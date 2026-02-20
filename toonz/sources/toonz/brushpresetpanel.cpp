#include "brushpresetpanel.h"

// Standard includes
#include <cmath>

// ToonzQt includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toolpresetcommandmanager.h"

// ToonzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/imagestyles.h"

// TnzCore includes for style detection
#include "tcolorstyles.h"
#include "tvectorbrushstyle.h"
#include "tundo.h"
#include "historytypes.h"
#include "tsimplecolorstyles.h"
#include "tpalette.h"
#include "toonz/palettecontroller.h"
#include "toonz/tpalettehandle.h"

// ToonzCore includes
#include "tenv.h"
#include "tfilepath.h"
#include "tsystem.h"
#include "tstream.h"

// Tools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tproperty.h"

// Command-based add/remove (avoids LNK2019 - handlers are in tnztools)
#include "toonzqt/brushpresetbridge.h"
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGridLayout>
#include <QPushButton>
#include <QSettings>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QToolTip>
#include <QMenu>
#include <QActionGroup>
#include <QStyleOptionButton>
#include <QFontMetrics>
#include <QEvent>
#include <QContextMenuEvent>
#include <QButtonGroup>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QSet>
#include <QFrame>

//=============================================================================
// PresetNamePopup - Popup to enter a preset name
//=============================================================================

class PresetNamePopup : public QDialog {
  QLineEdit *m_nameFld;

public:
  PresetNamePopup(QWidget *parent = nullptr)
      : QDialog(parent) {
    setWindowTitle(tr("Enter Preset Name"));
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_nameFld = new QLineEdit(this);
    mainLayout->addWidget(m_nameFld);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okBtn     = new QPushButton(tr("OK"), this);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);
    
    connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  }

  QString getName() const { return m_nameFld->text(); }
  void removeName() { m_nameFld->setText(""); }
};

//=============================================================================
// Helper function to load preset render data from disk
//=============================================================================

namespace {
PresetRenderData loadPresetRenderData(const QString &presetName, const QString &toolType) {
  PresetRenderData data;
  data.hasData = false;
  
  // Determine preset file path based on tool type
  TFilePath presetFilePath;
  if (toolType == "vector") {
    presetFilePath = TEnv::getConfigDir() + "brush_vector.txt";
  } else if (toolType == "toonzraster") {
    presetFilePath = TEnv::getConfigDir() + "brush_toonzraster.txt";
  } else if (toolType == "raster") {
    presetFilePath = TEnv::getConfigDir() + "brush_raster.txt";
  } else {
    return data;  // Unknown tool type
  }
  
  // Try to load preset file
  try {
    TIStream is(presetFilePath);
    std::string tagName;
    std::wstring wPresetName = presetName.toStdWString();
    
    while (is.matchTag(tagName)) {
      if (tagName == "version") {
        int major, minor;
        is >> major >> minor;
        is.matchEndTag();
      } else if (tagName == "brushes") {
        while (is.matchTag(tagName)) {
          if (tagName == "brush") {
            // Parse brush entry
            std::wstring name;
            double minThick = 1.0, maxThick = 5.0, hardness = 100.0;
            double minOp = 100.0, maxOp = 100.0;
            int pencil = 0;
            
            while (is.matchTag(tagName)) {
              if (tagName == "Name") {
                is >> name;
                is.matchEndTag();
              } else if (tagName == "Thickness") {
                is >> minThick >> maxThick;
                is.matchEndTag();
              } else if (tagName == "Hardness") {
                is >> hardness;
                is.matchEndTag();
              } else if (tagName == "Opacity") {
                is >> minOp >> maxOp;
                is.matchEndTag();
              } else if (tagName == "Pencil") {
                is >> pencil;
                is.matchEndTag();
              } else {
                is.skipCurrentTag();
              }
            }
            
            // Check if this is the preset we're looking for
            if (name == wPresetName) {
              data.minThickness = minThick;
              data.maxThickness = maxThick;
              data.hardness = hardness;
              data.minOpacity = minOp;
              data.maxOpacity = maxOp;
              data.isPencil = (pencil != 0);
              data.hasData = true;
              is.matchEndTag();
              return data;  // Found it!
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
  } catch (...) {
    // Failed to load - keep hasData = false
  }
  
  return data;
}
}  // namespace

//=============================================================================
// BrushPresetItem implementation
//=============================================================================

BrushPresetItem::BrushPresetItem(const QString &presetName, const QString &toolType, bool isListMode, bool useSampleStrokes, QWidget *parent)
    : QToolButton(parent)
    , m_presetName(presetName)
    , m_toolType(toolType)
    , m_hasIcon(false)
    , m_isCustomIcon(false)
    , m_isListMode(isListMode)
    , m_isSmallMode(false)
    , m_showBorders(true)
    , m_showBackgrounds(true)
    , m_checkboxVisible(false)
    , m_isMultiSelected(false)
    , m_useSampleStrokes(useSampleStrokes) {
  
  setText(presetName);
  setCheckable(true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMouseTracking(true);  // To detect hover
  setAcceptDrops(true);    // Enable drag & drop
  
  // Load icon for this preset
  QString customIconPath = findCustomPresetIcon(presetName);
  if (!customIconPath.isEmpty()) {
    setIconFromPath(customIconPath);
    m_isCustomIcon = true;  // Mark as custom icon
  } else {
    setDefaultIcon(toolType);
    m_isCustomIcon = false;  // Generated icon
  }
  
  // Load preset render data for dynamic fallback graphics
  // (only needed if no custom icon is available)
  if (customIconPath.isEmpty()) {
    // Check if this is a Universal Size preset (format: "[Size] NNN")
    if (presetName.startsWith("[Size] ") || presetName.startsWith("[Size ")) {
      // Create synthetic render data for size preset
      QString sizeStr = presetName;
      sizeStr.remove("[Size] ").remove("[Size ").remove("]");
      bool ok;
      double size = sizeStr.toDouble(&ok);
      if (ok) {
        m_renderData.hasData = true;
        m_renderData.maxThickness = size;
        m_renderData.minThickness = size;
        m_renderData.hardness = 100.0;
        m_renderData.minOpacity = 100.0;
        m_renderData.maxOpacity = 100.0;
        m_renderData.isPencil = false;
      }
    } else {
      // Regular brush preset - load from file
      m_renderData = loadPresetRenderData(presetName, toolType);
    }
  }
  
  // Create scaled version of icon (FIXED)
  updateScaledIcon();
  
  connect(this, &QPushButton::clicked, [this]() {
    emit presetSelected(m_presetName, m_toolType);
  });
  
  // Force repaint when checked state changes
  connect(this, &QPushButton::toggled, [this](bool) {
    update();
  });
}

void BrushPresetItem::setListMode(bool isListMode) {
  if (m_isListMode == isListMode) return;
  m_isListMode = isListMode;
  updateScaledIcon();  // Recalculate icon for new mode
  update();  // Repaint
}

void BrushPresetItem::setSmallMode(bool isSmallMode) {
  if (m_isSmallMode == isSmallMode) return;
  m_isSmallMode = isSmallMode;
  updateScaledIcon();  // Recalculate icon for new mode
  update();  // Repaint
}

QString BrushPresetItem::findCustomPresetIcon(const QString &presetName) {
  // Same logic as in ToolPresetCommandManager
  TFilePath iconFolder = TEnv::getStuffDir() + "library" + "brushpreseticons";
  
  // Try .svg and .png extensions
  QStringList extensions;
  extensions << ".svg" << ".png";
  
  for (const QString &ext : extensions) {
    QString iconFileName = presetName + ext;
    TFilePath iconPath = iconFolder + TFilePath(iconFileName.toStdString());
    
    if (TFileStatus(iconPath).doesExist()) {
      return iconPath.getQString();
    }
  }
  
  return QString();
}

void BrushPresetItem::setIconFromPath(const QString &iconPath) {
  if (iconPath.isEmpty()) {
    m_hasIcon = false;
    return;
  }
  
  m_isCustomIcon = true;
  
  // Load icon directly (no theme adaptation)
  QPixmap pixmap(iconPath);
  if (!pixmap.isNull()) {
    m_iconPixmap = pixmap;
    m_hasIcon = true;
  } else {
    m_hasIcon = false;
  }
}

void BrushPresetItem::setDefaultIcon(const QString &toolType) {
  // Use default SVG icons according to tool type
  QString iconName;
  if (toolType == "vector") {
    iconName = "brush";      // Vector brush
  } else if (toolType == "toonzraster") {
    iconName = "palette";    // Toonz Raster brush
  } else if (toolType == "raster") {
    iconName = "paintbrush"; // Raster brush
  }
  
  m_isCustomIcon = false;
  
  if (!iconName.isEmpty()) {
    QIcon icon = createQIcon(iconName.toStdString().c_str());
    if (!icon.isNull()) {
      m_iconPixmap = icon.pixmap(32, 32);
      m_hasIcon = true;
    } else {
      m_hasIcon = false;
    }
  } else {
    m_hasIcon = false;
  }
}

void BrushPresetItem::updateScaledIcon() {
  if (!m_hasIcon || m_iconPixmap.isNull()) {
    m_scaledIconCache = QPixmap();
    return;
  }
  
  // Calculate scaled icon size ONCE
  // based on MINIMUM widget height
  const int margin = 3;  // Reduced margins for more space
  
  // Use minimumHeight() to have a fixed size
  int cellHeight = minimumHeight();
  if (cellHeight <= 0) cellHeight = 60;  // Fallback
  
  // Available area for icon (depends on mode)
  int availableHeight, availableWidth;
  
  if (m_isListMode) {
    // In list mode: icon occupies full height (except reduced margins)
    availableHeight = cellHeight - (2 * margin);
    availableWidth = availableHeight;  // Square icon in list mode
  } else {
    // In grid mode: leave space for text below (larger text)
    const int textHeightSmall = 16;  // Larger text (was 14)
    const int textMargin = 2;
    availableHeight = cellHeight - (2 * margin) - textHeightSmall - textMargin;
    availableWidth = minimumWidth() - (2 * margin);
    if (availableWidth <= 0) availableWidth = 120;  // Fallback
  }
  
  // Calculate original icon ratio
  qreal iconRatio = (qreal)m_iconPixmap.width() / (qreal)m_iconPixmap.height();
  
  int scaledWidth, scaledHeight;
  
  if (availableHeight > 0 && availableWidth > 0) {
    qreal cellRatio = (qreal)availableWidth / (qreal)availableHeight;
    
    if (iconRatio > cellRatio) {
      // Icon is wider: limit by width
      scaledWidth = availableWidth;
      scaledHeight = (int)(availableWidth / iconRatio);
    } else {
      // Icon is taller: limit by height
      scaledHeight = availableHeight;
      scaledWidth = (int)(availableHeight * iconRatio);
    }
    
    // Safety check
    if (scaledHeight > availableHeight) {
      scaledHeight = availableHeight;
      scaledWidth = (int)(availableHeight * iconRatio);
    }
    if (scaledWidth > availableWidth) {
      scaledWidth = availableWidth;
      scaledHeight = (int)(availableWidth / iconRatio);
    }
  } else {
    scaledWidth = availableWidth;
    scaledHeight = availableHeight;
  }
  
  // For GENERATED icons (not custom) in GridSmall mode: reduce by 3px
  if (!m_isCustomIcon && m_isSmallMode && !m_isListMode) {
    const int smallModeReduction = 3;
    scaledWidth = qMax(1, scaledWidth - smallModeReduction);
    scaledHeight = qMax(1, scaledHeight - smallModeReduction);
  }
  
  // For GENERATED icons (not custom) in ListView mode: reduce by 55%
  // This optimization improves visual balance on screens < 4K (2.5K, 1080p, 1050p)
  if (!m_isCustomIcon && m_isListMode) {
    scaledWidth = qMax(1, (int)(scaledWidth * 0.45));
    scaledHeight = qMax(1, (int)(scaledHeight * 0.45));
  }
  
  // Create scaled icon and CACHE it
  m_scaledIconCache = m_iconPixmap.scaled(scaledWidth, scaledHeight, 
                                          Qt::KeepAspectRatio, 
                                          Qt::SmoothTransformation);
}

//-----------------------------------------------------------------------------

void BrushPresetItem::drawDynamicFallback(QPainter &painter, const QRect &iconRect) const {
  if (!m_renderData.hasData) return;
  
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  
  // Check if this is a size preset (pattern: [Size] NNN or [Size NNN])
  bool isSizePreset = m_presetName.startsWith("[Size] ") || m_presetName.startsWith("[Size ");
  
  if (isSizePreset) {
    // ==== SIZE PRESET: Progressive top-cropping circle + number ====
    // Small sizes: full circle (small).
    // As size grows, circle grows and gets progressively cropped from the TOP.
    // Around size 200+, only the bottom half remains (half-circle).
    QString sizeStr = m_presetName;
    sizeStr.remove("[Size] ").remove("[Size ").remove("]");
    double sizeVal = sizeStr.toDouble();
    
    // Font for the number
    QFont numFont = font();
    int fontSize = qMax(8, qMin(14, iconRect.height() / 4));
    numFont.setPixelSize(fontSize);
    numFont.setBold(true);
    QFontMetrics fm(numFont);
    int textH = fm.height();
    
    // Available space above text
    int availH = iconRect.height() - textH - 3;
    int maxDiam = qMin(iconRect.width() - 2, (int)(availH * 1.8));
    maxDiam = qMax(8, maxDiam);
    
    // Variable circle diameter (log scale from 0.5..1000)
    double minSz = 0.5, maxSz = 1000.0;
    double logMin = std::log(minSz + 1.0);
    double logMax = std::log(maxSz + 1.0);
    double logVal = std::log(qMax(sizeVal, minSz) + 1.0);
    double ratio = (logVal - logMin) / (logMax - logMin);
    ratio = qMax(0.0, qMin(1.0, ratio));
    
    int circleDiam = (int)(maxDiam * (0.15 + 0.85 * ratio));
    circleDiam = qMax(6, circleDiam);
    
    // Bottom of the circle sits at the text baseline
    int cx = iconRect.center().x();
    int circleBottom = iconRect.top() + availH;
    int circleTop = circleBottom - circleDiam;
    
    // Clip top: if circle extends above available area, crop from top
    int clipTop = iconRect.top();
    
    // Draw the circle. If it overflows the top, use a horizontal clip rect
    // so only the top is hidden — sides remain fully intact (like a horizontal mask).
    QRect circleRect(cx - circleDiam / 2, circleTop, circleDiam, circleDiam);
    
    if (circleTop < clipTop) {
      // Circle extends above the available area: clip horizontally from the top.
      // The clip rect spans the full width but starts at clipTop.
      painter.setClipRect(QRect(iconRect.left(), clipTop,
                                iconRect.width(), circleBottom - clipTop));
    }
    
    painter.setPen(QPen(QColor(180, 180, 180), 1));
    painter.setBrush(QColor(255, 255, 255));
    painter.drawEllipse(circleRect);
    
    // Remove clip region so it does not affect subsequent drawing
    if (circleTop < clipTop) {
      painter.setClipping(false);
    }
    
    // Size number below the shape
    painter.setPen(palette().color(QPalette::Text));
    painter.setFont(numFont);
    QRect textRect(iconRect.left(), circleBottom + 1,
                   iconRect.width(), textH + 2);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, sizeStr);
    
  } else {
    // ==== BRUSH PRESET: Horizontal undulation sample stroke ====
    // Smooth double-wave: first half rises (hump), second half dips (valley).
    // Fills full iconRect width with tapered pointed tips.
    // Taper shape adapts to brush type (native vs overlay).
    // Texture varies: raster=grainy, toonzraster=light grain, vector=smooth.
    
    double minThick = m_renderData.minThickness;
    double maxThick = m_renderData.maxThickness;
    double avgThick = (minThick + maxThick) / 2.0;
    double avgOpacity = (m_renderData.minOpacity + m_renderData.maxOpacity) / 2.0;
    
    // Stroke max height: proportional to thickness, capped at 50% of icon height
    double maxH = iconRect.height() * 0.50;
    double strokeH = qMax(3.0, qMin(maxH, avgThick * iconRect.height() / 12.0));
    strokeH = qMax(strokeH, 5.0);
    
    // If opacity is 0 (tool doesn't support opacity, e.g. Toonz Raster),
    // treat as fully opaque since the brush always paints at full intensity.
    double alpha;
    if (avgOpacity < 0.01)
      alpha = 1.0;
    else
      alpha = qMax(0.3, qMin(1.0, avgOpacity / 100.0));
    
    // Hardness influences stroke brightness and crispness.
    // High hardness (100) -> pure white, crisp edges.
    // Low hardness (0) -> darker gray, soft/diffuse edges.
    double hardness = m_renderData.hardness;  // 0..100
    double hardFrac = qMax(0.0, qMin(1.0, hardness / 100.0));
    
    // Stroke color: interpolate from a soft gray to pure white based on hardness.
    // Low hardness (0): gray ~140,140,140 (clearly visible as soft/diffuse)
    // High hardness (100): pure white 255,255,255 (crisp, bright)
    int lowVal = 140;   // Gray value at hardness=0
    int highVal = 255;  // White at hardness=100
    int val = lowVal + (int)((highVal - lowVal) * hardFrac);
    QColor strokeColor(val, val, val);
    strokeColor.setAlphaF(alpha);
    
    // Full width of iconRect with tiny margin for tips
    double x0 = iconRect.left() + 1.0;
    double x1 = iconRect.right() - 1.0;
    double totalW = x1 - x0;
    double cy = iconRect.center().y();
    
    // Wave amplitude: how much the centerline deviates up/down
    double waveAmp = iconRect.height() * 0.14;
    
    // Taper style depends on whether this is a native brush (no overlay).
    // Native brushes: taper driven by min/max thickness difference.
    // Other (MyPaint, texture, generated): default pointed taper.
    // Thickness ratio: how much thinner the tips are vs the body
    double thickRatio = (maxThick > 0.01) ? (minThick / maxThick) : 0.0;
    // Clamp to [0..1]: 0 = very tapered (big difference), 1 = uniform
    thickRatio = qMax(0.0, qMin(1.0, thickRatio));
    
    // Build the stroke outline as polygon (top edge + reversed bottom edge).
    // Centerline follows a sine wave (one full period = double undulation).
    const int N = 80;
    QPolygonF topEdge, botEdge;
    
    for (int i = 0; i <= N; ++i) {
      double t = (double)i / N;  // 0..1 along the stroke
      double x = x0 + totalW * t;
      
      // Centerline: smooth sine wave (one full period).
      // First half: hump up. Second half: dip down.
      double angle = t * 2.0 * 3.14159265358979;
      double centerY = cy - std::sin(angle) * waveAmp;
      
      // Taper envelope: pointed tips at both ends, full thickness in middle.
      // For native brushes, the min/max ratio controls how pointed the tips are.
      // thickRatio=0 => very pointed; thickRatio=1 => blunt (uniform thickness).
      double taper;
      if (t < 0.5) {
        double u = t * 2.0;       // 0 -> 1
        taper = u * u;            // Quadratic ease-in
      } else {
        double u = (1.0 - t) * 2.0; // 1 -> 0
        taper = u * u;            // Quadratic ease-out
      }
      
      // Blend taper with thickness ratio: native brushes use min/max difference
      // to set a floor on the taper (blunt tips when min ~ max).
      taper = thickRatio + (1.0 - thickRatio) * taper;
      
      // Hardness affects taper sharpness
      if (m_renderData.hardness < 50)
        taper = std::pow(taper, 0.7);
      if (m_renderData.hardness > 80)
        taper = std::pow(taper, 1.3);
      
      double halfH = strokeH * taper * 0.5;
      
      topEdge << QPointF(x, centerY - halfH);
      botEdge.prepend(QPointF(x, centerY + halfH));
    }
    
    // Build closed polygon path from the two edges
    QPolygonF outline = topEdge + botEdge;
    QPainterPath path;
    path.addPolygon(outline);
    path.closeSubpath();
    
    // === Texture rendering based on brush tool type ===
    // "raster"      : grainy/pixelated look (stipple dots along the stroke)
    // "toonzraster" : lighter grain (fewer dots)
    // "vector"      : perfectly smooth, no grain
    // Default       : smooth with soft glow for soft brushes
    
    bool isRaster = (m_toolType == "raster");
    bool isToonzRaster = (m_toolType == "toonzraster");
    bool isVector = (m_toolType == "vector");
    
    // Main fill
    painter.setPen(Qt::NoPen);
    painter.setBrush(strokeColor);
    painter.drawPath(path);
    
    // Grain overlay for raster types (drawn on top of the fill).
    // Grain diminishes with higher hardness (hard brushes are clean/crisp).
    if ((isRaster || isToonzRaster) && hardFrac < 0.85) {
      // Draw scattered dots inside the stroke for a grainy effect.
      // Use a deterministic seed based on preset name for consistency.
      unsigned int seed = 0;
      for (int k = 0; k < m_presetName.length(); ++k)
        seed = seed * 31 + m_presetName.at(k).unicode();
      
      // Scale grain intensity down as hardness increases
      double grainScale = 1.0 - hardFrac;  // 1.0 at hard=0, 0.0 at hard=100
      
      QColor grainColor = strokeColor;
      double grainAlpha = (isRaster ? 0.35 : 0.20) * grainScale;
      grainColor.setAlphaF(alpha * grainAlpha);
      painter.setPen(Qt::NoPen);
      
      // Number of grain dots: proportional to stroke area, scaled by hardness
      int dotCount = (int)((isRaster ? 60 : 30) * grainScale);
      double dotSize = isRaster ? 1.5 : 1.2;
      
      for (int d = 0; d < dotCount; ++d) {
        // Simple pseudo-random using seed (no external lib needed)
        seed = seed * 1103515245 + 12345;
        double rx = ((seed >> 16) & 0x7FFF) / 32768.0;  // 0..1
        seed = seed * 1103515245 + 12345;
        double ry = ((seed >> 16) & 0x7FFF) / 32768.0;  // 0..1
        
        double px = x0 + totalW * rx;
        double py = iconRect.top() + iconRect.height() * (0.2 + 0.6 * ry);
        
        // Only draw dot if it falls inside the stroke path
        if (path.contains(QPointF(px, py))) {
          // Alternate between dark and light grains
          if (d % 3 == 0) {
            QColor darkGrain = strokeColor.darker(140);
            darkGrain.setAlphaF(alpha * grainAlpha * 0.8);
            painter.setBrush(darkGrain);
          } else {
            painter.setBrush(grainColor);
          }
          painter.drawEllipse(QPointF(px, py), dotSize, dotSize);
        }
      }
    }
    
    // Soft glow for soft brushes (not pencil, not vector).
    // The softer the brush, the wider and more visible the diffuse glow.
    if (hardFrac < 0.5 && !m_renderData.isPencil && !isVector) {
      // Glow intensity increases as hardness decreases
      double glowIntensity = (0.5 - hardFrac) * 2.0;  // 0..1 (0 at hard=50, 1 at hard=0)
      double glowWidth = 1.5 + glowIntensity * 2.5;   // 1.5..4.0 px
      double glowAlpha = alpha * (0.08 + glowIntensity * 0.15);
      
      QColor glow = strokeColor;
      glow.setAlphaF(glowAlpha);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(glow, glowWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.drawPath(path);
    }
  }
  
  painter.restore();
}

//-----------------------------------------------------------------------------

void BrushPresetItem::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  
  // Draw background and border manually according to state
  QRect r = rect();
  
  // Colors according to state
  QColor bgColor;
  QColor borderColor = palette().color(QPalette::Mid);
  int borderRadius = 4;  // Always rounded for modern look
  
  // Get parent panel background color for seamless integration
  QColor panelBgColor = palette().color(QPalette::Window);
  if (parentWidget()) {
    panelBgColor = parentWidget()->palette().color(QPalette::Window);
  }
  
  // Use QToolButton theme colors for hover/checked/pressed states
  QStyleOptionToolButton opt;
  initStyleOption(&opt);
  const bool useThemeState =
      opt.state.testFlag(QStyle::State_MouseOver) ||
      opt.state.testFlag(QStyle::State_On) ||
      opt.state.testFlag(QStyle::State_Sunken);
  
  if (useThemeState) {
    QStyleOptionToolButton optBg = opt;
    optBg.text = QString();
    optBg.icon = QIcon();
    style()->drawComplexControl(QStyle::CC_ToolButton, &optBg, &painter, this);
  } else {
    // Normal state background matches Brush Preset logic
    QColor windowColor = panelBgColor;
    if (m_showBackgrounds) {
      if (windowColor.lightness() > 128) {
        bgColor = windowColor.darker(105);
      } else {
        bgColor = windowColor.lighter(108);
      }
    } else {
      bgColor = panelBgColor;
    }
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(r, borderRadius, borderRadius);
  }
  
  // Draw border (if enabled AND not checked)
  if (m_showBorders && !isChecked()) {
    QPen borderPen(borderColor);
    borderPen.setWidthF(0.5);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    QRect borderRect = r.adjusted(0, 0, -1, -1);
    painter.drawRoundedRect(borderRect, borderRadius, borderRadius);
  }
  
  // Draw icon if available
  if (m_hasIcon && !m_iconPixmap.isNull()) {
    // Margins and spacing (synchronized with updateScaledIcon)
    const int margin = 3;
    
    if (m_isListMode) {
      // === LIST MODE: Icon on left (FIXED, positioned by type), text on right ===
      
      // Use CACHED scaled icon (fixed size)
      if (!m_scaledIconCache.isNull()) {
        int iconX = margin;
        int iconY = (height() - m_scaledIconCache.height()) / 2;
        
        // Move custom icons up by 6px in list mode to avoid 4K clipping
        if (m_isCustomIcon) {
          iconY -= 6;
        }
        
        // For dynamic fallback in list mode, use a wider rect:
        // Stroke can span the full icon area width (original icon height used as width basis)
        int dynIconW = m_scaledIconCache.width();
        int dynIconH = m_scaledIconCache.height();
        
        // Check if we should use dynamic fallback instead of generic icon
        if (m_useSampleStrokes && !m_isCustomIcon && m_renderData.hasData) {
          // Give the stroke more horizontal room in list mode (~2x wider)
          int wideW = qMin(width() / 2, dynIconW * 3);
          QRect iconRect(iconX, iconY, wideW, dynIconH);
          drawDynamicFallback(painter, iconRect);
          // Text goes after the wider rect
          QRect textRect(iconX + wideW + margin, margin,
                         width() - iconX - wideW - (2 * margin),
                         height() - (2 * margin));
          painter.setPen(palette().color(QPalette::ButtonText));
          painter.setFont(font());
          QString displayText = m_presetName;
          QFontMetrics fm(font());
          if (fm.horizontalAdvance(displayText) > textRect.width())
            displayText = fm.elidedText(displayText, Qt::ElideMiddle, textRect.width());
          painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
        } else {
          painter.drawPixmap(iconX, iconY, m_scaledIconCache);
          // Draw text to the right of icon
          QRect textRect(iconX + m_scaledIconCache.width() + margin, margin,
                         width() - iconX - m_scaledIconCache.width() - (2 * margin),
                         height() - (2 * margin));
          painter.setPen(palette().color(QPalette::ButtonText));
          painter.setFont(font());
          QString displayText = m_presetName;
          QFontMetrics fm(font());
          if (fm.horizontalAdvance(displayText) > textRect.width())
            displayText = fm.elidedText(displayText, Qt::ElideMiddle, textRect.width());
          painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
        }
      }
      
    } else {
      // === GRID MODE: Icon on top (FIXED), Text below (larger) ===
      
      const int textHeightSmall = 16;  // Height reserved for text below (larger)
      const int textMargin = 2;        // Spacing between icon and text
      
      // Use CACHED scaled icon (fixed size)
      if (!m_scaledIconCache.isNull()) {
        // Available area to center icon
        int availableWidth = width() - (2 * margin);
        int availableHeight = height() - (2 * margin) - textHeightSmall - textMargin;
        
        // Check if we should use dynamic fallback instead of generic icon
        if (m_useSampleStrokes && !m_isCustomIcon && m_renderData.hasData) {
          // Use the full available width for the stroke
          int iconY = margin + (availableHeight - m_scaledIconCache.height()) / 2;
          QRect iconRect(margin, iconY, availableWidth, m_scaledIconCache.height());
          drawDynamicFallback(painter, iconRect);
        } else {
          // Center icon (which has a FIXED size) in available space
          int iconX = margin + (availableWidth - m_scaledIconCache.width()) / 2;
          int iconY = margin + (availableHeight - m_scaledIconCache.height()) / 2;
          painter.drawPixmap(iconX, iconY, m_scaledIconCache);
        }
      }
      
      // Draw text at BOTTOM (below icon) - LARGER
      int textStartY = height() - margin - textHeightSmall;
      QRect textRect(margin, textStartY, width() - (2 * margin), textHeightSmall);
      
      painter.setPen(palette().color(QPalette::ButtonText));
      QFont gridFont = font();
      // Use same font size as list mode (no reduction) for better readability
      gridFont.setPointSize(qMax(8, font().pointSize()));
      painter.setFont(gridFont);
      
      QString displayText = m_presetName;
      QFontMetrics fm(gridFont);
      if (fm.horizontalAdvance(displayText) > textRect.width()) {
        displayText = fm.elidedText(displayText, Qt::ElideMiddle, textRect.width());
      }
      
      painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, displayText);
    }
  } else {
    // No icon: just display centered text
    QRect textRect = rect().adjusted(4, 4, -4, -4);
    painter.setPen(palette().color(QPalette::ButtonText));
    painter.setFont(font());
    
    QString displayText = m_presetName;
    QFontMetrics fm(font());
    if (fm.horizontalAdvance(displayText) > textRect.width()) {
      displayText = fm.elidedText(displayText, Qt::ElideMiddle, textRect.width());
    }
    
    painter.drawText(textRect, Qt::AlignCenter, displayText);
  }
  
  // Draw checkbox overlay (top-right corner) when in checkbox mode
  if (m_checkboxVisible) {
    const int cbSize = 14;
    const int cbMargin = 4;
    QRect cbRect(width() - cbSize - cbMargin, cbMargin, cbSize, cbSize);
    
    // Draw checkbox background
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    if (m_isMultiSelected) {
      painter.setBrush(palette().color(QPalette::Highlight));
    } else {
      painter.setBrush(palette().color(QPalette::Base));
    }
    painter.drawRoundedRect(cbRect, 2, 2);
    
    // Draw checkmark if selected
    if (m_isMultiSelected) {
      painter.setPen(QPen(palette().color(QPalette::HighlightedText), 2));
      painter.drawLine(cbRect.left() + 3, cbRect.center().y(),
                       cbRect.center().x(), cbRect.bottom() - 3);
      painter.drawLine(cbRect.center().x(), cbRect.bottom() - 3,
                       cbRect.right() - 3, cbRect.top() + 3);
    }
  }
}

QSize BrushPresetItem::sizeHint() const {
  // Recommended default size (Large mode)
  return QSize(120, 60);
}

QSize BrushPresetItem::minimumSizeHint() const {
  // Minimum size to display icon + text
  return QSize(80, 50);
}

//-----------------------------------------------------------------------------
// Drag & Drop implementation for BrushPresetItem
//-----------------------------------------------------------------------------

void BrushPresetItem::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_dragStartPosition = event->pos();
  } else if (event->button() == Qt::RightButton) {
    // Emit right-click signal for context menu
    // Find parent BrushPresetPanel and show context menu
    BrushPresetPanel *panel = nullptr;
    QWidget *p = parentWidget();
    while (p) {
      panel = qobject_cast<BrushPresetPanel*>(p);
      if (panel) break;
      p = p->parentWidget();
    }
    if (panel) {
      panel->showPresetContextMenu(event->globalPos(), m_presetName);
      event->accept();
      return;
    }
  }
  QToolButton::mousePressEvent(event);
}

void BrushPresetItem::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton)) {
    QToolButton::mouseMoveEvent(event);
    return;
  }
  
  if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
    QToolButton::mouseMoveEvent(event);
    return;
  }
  
  // Start drag operation
  QDrag *drag = new QDrag(this);
  QMimeData *mimeData = new QMimeData;
  
  // Store preset name and tool type in MIME data
  mimeData->setText(m_presetName);
  mimeData->setData("application/x-brushpreset-tooltype", m_toolType.toUtf8());
  drag->setMimeData(mimeData);
  
  // Create drag pixmap (visual feedback)
  QPixmap dragPixmap = grab();
  drag->setPixmap(dragPixmap);
  drag->setHotSpot(event->pos());
  
  // Execute drag
  drag->exec(Qt::MoveAction);
}

void BrushPresetItem::dragEnterEvent(QDragEnterEvent *event) {
  // Accept only if dragging a brush preset of the same tool type
  if (event->mimeData()->hasText() && 
      event->mimeData()->hasFormat("application/x-brushpreset-tooltype")) {
    QString draggedToolType = QString::fromUtf8(
        event->mimeData()->data("application/x-brushpreset-tooltype"));
    
    if (draggedToolType == m_toolType) {
      event->acceptProposedAction();
      return;
    }
  }
  event->ignore();
}

void BrushPresetItem::dragMoveEvent(QDragMoveEvent *event) {
  // Same logic as dragEnterEvent
  if (event->mimeData()->hasText() && 
      event->mimeData()->hasFormat("application/x-brushpreset-tooltype")) {
    QString draggedToolType = QString::fromUtf8(
        event->mimeData()->data("application/x-brushpreset-tooltype"));
    
    if (draggedToolType == m_toolType) {
      event->acceptProposedAction();
      return;
    }
  }
  event->ignore();
}

void BrushPresetItem::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasText()) {
    event->ignore();
    return;
  }
  
  // Only accept drops if they have the correct MIME type
  if (!event->mimeData()->hasFormat("application/x-brushpreset-tooltype")) {
    event->ignore();
    return;
  }
  
  QString fromPreset = event->mimeData()->text();
  QString toPreset = m_presetName;
  
  // Don't reorder if dropping on itself
  if (fromPreset == toPreset) {
    event->ignore();
    return;
  }
  
  // Only accept drops from the same tool type
  QString draggedToolType = QString::fromUtf8(
      event->mimeData()->data("application/x-brushpreset-tooltype"));
  
  if (draggedToolType != m_toolType) {
    event->ignore();
    return;
  }
  
  // Emit signal to parent panel to handle reordering
  emit presetReordered(fromPreset, toPreset);
  
  event->acceptProposedAction();
}

//=============================================================================
// BrushPresetTabBar implementation (modeled after PaletteTabBar)
//=============================================================================

BrushPresetTabBar::BrushPresetTabBar(QWidget *parent)
    : QTabBar(parent)
    , m_renameTextField(new DVGui::LineEdit(this))
    , m_renameTabIndex(-1)
    , m_panel(nullptr) {
  setObjectName("BrushPresetTabBar");
  
  // Remove elevation effect - flat rendering like Level Palette
  setDrawBase(false);
  setDocumentMode(true); // CRITICAL: This removes the frame around tabs
  setMovable(false);  // We handle movement ourselves via Ctrl+Drag
  setExpanding(false); // Don't expand tabs to fill width
  setIconSize(QSize(16, 16)); // Standard icon size for palette_tab icon
  setUsesScrollButtons(true); // Show scroll buttons when many tabs
  setElideMode(Qt::ElideNone); // Don't elide tab text
  
  m_renameTextField->hide();
  
  connect(m_renameTextField, SIGNAL(editingFinished()), this, SLOT(updateTabName()));
}

//-----------------------------------------------------------------------------
/*! Hide rename text field and handle tab selection
 */
void BrushPresetTabBar::mousePressEvent(QMouseEvent *event) {
  m_renameTextField->hide();
  QTabBar::mousePressEvent(event);
}

//-----------------------------------------------------------------------------
/*! If Ctrl+Left button is pressed, allow tab reordering by emitting movePage signal
 */
void BrushPresetTabBar::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() == Qt::LeftButton &&
      event->modifiers() == Qt::ControlModifier) {
    int srcIndex = currentIndex();
    int dstIndex = tabAt(event->pos());
    if (dstIndex >= 0 && dstIndex < count() && dstIndex != srcIndex) {
      QRect rect = tabRect(srcIndex);
      int x = event->pos().x();
      if (x < rect.left() || x > rect.right()) {
        emit movePage(srcIndex, dstIndex);
      }
    }
  }
  QTabBar::mouseMoveEvent(event);
}

//-----------------------------------------------------------------------------
/*! Set a text field with focus in event position to edit tab name
 */
void BrushPresetTabBar::mouseDoubleClickEvent(QMouseEvent *event) {
  int index = tabAt(event->pos());
  if (index < 0) return;
  
  m_renameTabIndex = index;
  DVGui::LineEdit *fld = m_renameTextField;
  fld->setText(tabText(index));
  fld->setGeometry(tabRect(index));
  fld->setAlignment(Qt::AlignCenter);
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------
/*! Update tab name from rename text field
 */
void BrushPresetTabBar::updateTabName() {
  int index = m_renameTabIndex;
  if (index < 0) return;
  
  m_renameTabIndex = -1;
  if (m_renameTextField->text() != "" && m_renameTextField->text() != tabText(index)) {
    setTabText(index, m_renameTextField->text());
    emit tabTextChanged(index);
  }
  m_renameTextField->hide();
}

//-----------------------------------------------------------------------------
/*! Show context menu for page operations (like Level Palette)
 */
void BrushPresetTabBar::contextMenuEvent(QContextMenuEvent *event) {
  if (!m_panel) {
    QTabBar::contextMenuEvent(event);
    return;
  }
  
  int tabIndex = tabAt(event->pos());
  
  QMenu menu(this);
  
  // Add New Page action (always available)
  QAction *addPageAction = menu.addAction(tr("New Page"));
  connect(addPageAction, &QAction::triggered, [this]() {
    if (m_panel) m_panel->addNewPage();
  });
  
  // Delete Page action (only if clicking on a tab and not the last page)
  if (tabIndex >= 0 && count() > 1) {
    menu.addSeparator();
    QAction *deletePageAction = menu.addAction(tr("Delete Page"));
    connect(deletePageAction, &QAction::triggered, [this, tabIndex]() {
      if (m_panel) m_panel->deletePage(tabIndex);
    });
  }
  
  menu.exec(event->globalPos());
}

//=============================================================================
// BrushPresetPanel implementation
//=============================================================================

BrushPresetPanel::BrushPresetPanel(QWidget *parent)
    : TPanel(parent)
    , m_tabBarContainer(nullptr)
    , m_tabBar(nullptr)
    , m_scrollArea(nullptr)
    , m_presetContainer(nullptr)
    , m_presetLayout(nullptr)
    , m_addPresetButton(nullptr)
    , m_removePresetButton(nullptr)
    , m_refreshButton(nullptr)
    , m_nonDestructiveToggle(nullptr)
    , m_selectivePresetToggle(nullptr)
    , m_plainColorButton(nullptr)
    , m_toolLabel(nullptr)
    , m_viewModeButton(nullptr)
    , m_viewModeMenu(nullptr)
    , m_presetButtonGroup(nullptr)
    , m_toolHandle(nullptr)
    , m_currentToolType("")
    , m_currentPreset("")
    , m_presetNamePopup(nullptr)
    , m_viewMode(GridLarge)
    , m_currentColumns(2)
    , m_showBorders(false)
    , m_showBackgrounds(true)
    , m_useSampleStrokes(false)
    , m_currentPageIndex(0)
    , m_checkboxMode(false)
    , m_clipboardIsCut(false)
    , m_suppressRefresh(false) {
  
  // Load preferences from settings FIRST (before creating UI)
  QSettings settings;
  m_showBorders = settings.value("BrushPresetPanel/showBorders", false).toBool();
  m_showBackgrounds = settings.value("BrushPresetPanel/showBackgrounds", true).toBool();
  m_useSampleStrokes = settings.value("BrushPresetPanel/useSampleStrokes", false).toBool();
  
  // Load view mode (default: GridLarge)
  int savedViewMode = settings.value("BrushPresetPanel/viewMode", static_cast<int>(GridLarge)).toInt();
  m_viewMode = static_cast<ViewMode>(savedViewMode);
  
  // NOW initialize UI (menu will be created with correct m_viewMode)
  initializeUI();
  
  // Apply restored view mode (updates the layout)
  setViewMode(m_viewMode);
}

BrushPresetPanel::~BrushPresetPanel() {
  disconnectSignals();
  if (m_presetNamePopup) {
    delete m_presetNamePopup;
    m_presetNamePopup = nullptr;
  }
  
  // Clean up all pages
  clearPages();
}

void BrushPresetPanel::initializeUI() {
  // Main widget
  QWidget *mainWidget = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);
  
  // Label for displaying active tool (theme-aware colors)
  m_toolLabel = new QLabel(tr("No brush tool selected"), mainWidget);
  m_toolLabel->setAlignment(Qt::AlignCenter);
  m_toolLabel->setAutoFillBackground(true);
  // Apply same visual logic as brush cells: inherit theme palette colors
  // The label will follow the panel's theme automatically
  m_toolLabel->setMinimumHeight(28);
  mainLayout->addWidget(m_toolLabel);
  
  // Vertical spacing between header and button bar
  mainLayout->addSpacing(8);
  
  // Button bar at top (matching Level Palette + Tool Options Bar logic)
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  
  // New Page button (like Level Palette - separate from presets)
  QPushButton *newPageButton = new QPushButton(mainWidget);
  newPageButton->setIcon(createQIcon("newpage"));
  newPageButton->setIconSize(QSize(20, 20));
  newPageButton->setFixedSize(24, 24);
  newPageButton->setToolTip(tr("New Page"));
  connect(newPageButton, SIGNAL(clicked()), this, SLOT(onAddPageClicked()));
  
  // Separator after page button
  QFrame *separator1 = new QFrame(mainWidget);
  separator1->setFrameShape(QFrame::VLine);
  separator1->setFrameShadow(QFrame::Sunken);
  
  // PRESET BUTTONS (mirror of Tool Options Bar)
  // Remove Preset button (minus icon like Tool Options Bar)
  m_removePresetButton = new QPushButton(mainWidget);
  m_removePresetButton->setIcon(createQIcon("minus"));
  m_removePresetButton->setIconSize(QSize(16, 16));
  m_removePresetButton->setFixedSize(24, 24);
  m_removePresetButton->setToolTip(tr("Remove Preset"));
  m_removePresetButton->setEnabled(false);
  
  // Add Preset button (plus icon like Tool Options Bar)
  m_addPresetButton = new QPushButton(mainWidget);
  m_addPresetButton->setIcon(createQIcon("plus"));
  m_addPresetButton->setIconSize(QSize(16, 16));
  m_addPresetButton->setFixedSize(24, 24);
  m_addPresetButton->setToolTip(tr("Add Preset"));
  m_addPresetButton->setEnabled(false);
  
  // Separator after preset buttons
  QFrame *separator2 = new QFrame(mainWidget);
  separator2->setFrameShape(QFrame::VLine);
  separator2->setFrameShadow(QFrame::Sunken);
  
  // Refresh button
  m_refreshButton = new QPushButton(mainWidget);
  m_refreshButton->setIcon(createQIcon("repeat"));
  m_refreshButton->setIconSize(QSize(16, 16));
  m_refreshButton->setFixedSize(24, 24);
  m_refreshButton->setToolTip(tr("Refresh preset list"));
  m_refreshButton->setEnabled(true);
  
  // Separator before smart preset toggles
  QFrame *separator3 = new QFrame(mainWidget);
  separator3->setFrameShape(QFrame::VLine);
  separator3->setFrameShadow(QFrame::Sunken);
  separator3->setStyleSheet("QFrame { color: rgba(128,128,128,80); }");
  
  // Non-Destructive toggle (protects existing strokes by not overwriting styles)
  m_nonDestructiveToggle = new QToolButton(mainWidget);
  m_nonDestructiveToggle->setIcon(createQIcon("lock"));
  m_nonDestructiveToggle->setIconSize(QSize(14, 14));
  m_nonDestructiveToggle->setFixedSize(20, 20);
  m_nonDestructiveToggle->setCheckable(true);
  m_nonDestructiveToggle->setToolTip(tr("Non-Destructive"));
  
  // Selective Preset toggle (applies only tool parameters, ignores style snapshot)
  m_selectivePresetToggle = new QToolButton(mainWidget);
  m_selectivePresetToggle->setIcon(createQIcon("preset"));
  m_selectivePresetToggle->setIconSize(QSize(14, 14));
  m_selectivePresetToggle->setFixedSize(20, 20);
  m_selectivePresetToggle->setCheckable(true);
  m_selectivePresetToggle->setToolTip(tr("Selective Preset"));
  
  // Plain Color button (removes style overlay, restores solid color)
  m_plainColorButton = new QToolButton(mainWidget);
  m_plainColorButton->setIcon(createQIcon("plaincolor"));
  m_plainColorButton->setIconSize(QSize(14, 14));
  m_plainColorButton->setFixedSize(20, 20);
  m_plainColorButton->setToolTip(tr("Plain Color"));
  
  // Restore toggle states from QSettings
  {
    QSettings settings;
    bool nd = settings.value("BrushPresetPanel/NonDestructive", false).toBool();
    bool sp = settings.value("BrushPresetPanel/SelectivePreset", false).toBool();
    m_nonDestructiveToggle->setChecked(nd);
    m_selectivePresetToggle->setChecked(sp);
    BrushPresetBridge::setNonDestructiveMode(nd);
    BrushPresetBridge::setSelectivePresetMode(sp);
  }
  
  // Hamburger menu for display modes
  m_viewModeButton = new QPushButton(mainWidget);
  m_viewModeButton->setIcon(createQIcon("menu"));
  m_viewModeButton->setFixedSize(24, 24);
  m_viewModeButton->setToolTip(tr("View mode"));
  
  // Layout: [NewPage] | [-] [+] | [Refresh] | [Lock] [Selective] [PlainColor] ... [Menu]
  buttonLayout->addWidget(newPageButton);
  buttonLayout->addWidget(separator1);
  buttonLayout->addWidget(m_removePresetButton);
  buttonLayout->addWidget(m_addPresetButton);
  buttonLayout->addWidget(separator2);
  buttonLayout->addWidget(m_refreshButton);
  buttonLayout->addWidget(separator3);
  buttonLayout->addWidget(m_nonDestructiveToggle);
  buttonLayout->addWidget(m_selectivePresetToggle);
  buttonLayout->addWidget(m_plainColorButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(m_viewModeButton);
  
  mainLayout->addLayout(buttonLayout);
  
  // Tab bar container with top+bottom separator (like PaletteViewer).
  // TabBarContainter draws only the BOTTOM line via its paintEvent.
  // We wrap it in a container QFrame with a top border drawn via stylesheet
  // to ensure the top line is always visible regardless of theme.
  QFrame *tabContainerFrame = new QFrame(mainWidget);
  tabContainerFrame->setObjectName("BrushPresetTabContainerFrame");
  tabContainerFrame->setStyleSheet(
    "QFrame#BrushPresetTabContainerFrame {"
    "  border-top: 1px solid palette(dark);"
    "  border-left: none;"
    "  border-right: none;"
    "  border-bottom: none;"
    "  padding: 0px;"
    "  margin: 0px;"
    "}"
  );
  QVBoxLayout *tabContainerLayout = new QVBoxLayout(tabContainerFrame);
  tabContainerLayout->setContentsMargins(0, 0, 0, 0);
  tabContainerLayout->setSpacing(0);
  
  m_tabBarContainer = new TabBarContainter(tabContainerFrame);
  m_tabBar = new BrushPresetTabBar(m_tabBarContainer);
  m_tabBar->setBrushPresetPanel(this);
  
  // Layout for tab bar inside container (same as PaletteViewer::m_hLayout)
  QHBoxLayout *tabLayout = new QHBoxLayout;
  tabLayout->setContentsMargins(0, 0, 0, 0);
  tabLayout->setSpacing(0);
  tabLayout->addWidget(m_tabBar, 0);
  tabLayout->addStretch(1);
  m_tabBarContainer->setLayout(tabLayout);
  
  // Add TabBarContainter into the outer frame (which draws the top border)
  tabContainerLayout->addWidget(m_tabBarContainer);
  mainLayout->addWidget(tabContainerFrame, 0);
  
  // Scrollable container for presets
  m_scrollArea = new QScrollArea(mainWidget);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  
  m_presetContainer = new QWidget();
  m_presetLayout = new QGridLayout(m_presetContainer);
  m_presetLayout->setMargin(5);
  m_presetLayout->setSpacing(8);
  m_presetLayout->setAlignment(Qt::AlignTop);
  
  m_presetContainer->setLayout(m_presetLayout);
  m_scrollArea->setWidget(m_presetContainer);
  
  mainLayout->addWidget(m_scrollArea, 1);
  
  setWidget(mainWidget);
  setWindowTitle(tr("Brush Presets"));
  
  // Enable context menu
  setContextMenuPolicy(Qt::DefaultContextMenu);
  
  // Create display modes menu
  createViewModeMenu();
  
  // Connect buttons
  connect(m_addPresetButton, SIGNAL(clicked()), this, SLOT(onAddPresetClicked()));
  connect(m_removePresetButton, SIGNAL(clicked()), this, SLOT(onRemovePresetClicked()));
  connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(onRefreshClicked()));
  connect(m_nonDestructiveToggle, SIGNAL(toggled(bool)),
          this, SLOT(onNonDestructiveToggled(bool)));
  connect(m_selectivePresetToggle, SIGNAL(toggled(bool)),
          this, SLOT(onSelectivePresetToggled(bool)));
  connect(m_plainColorButton, SIGNAL(clicked()), this, SLOT(onPlainColorClicked()));
}

void BrushPresetPanel::connectSignals() {
  if (!m_toolHandle) {
    TApplication *app = TApp::instance();
    m_toolHandle = app->getCurrentTool();
  }
  
  if (m_toolHandle) {
    connect(m_toolHandle, SIGNAL(toolSwitched()), this, SLOT(onToolSwitched()));
    connect(m_toolHandle, SIGNAL(toolChanged()), this, SLOT(onToolChanged()));
    connect(m_toolHandle, SIGNAL(toolComboBoxListChanged(std::string)), 
            this, SLOT(onToolComboBoxListChanged(std::string)));
  }
  
  // Tab bar signals
  if (m_tabBar) {
    connect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));
    connect(m_tabBar, SIGNAL(tabTextChanged(int)), this, SLOT(onTabTextChanged(int)));
    connect(m_tabBar, SIGNAL(movePage(int, int)), this, SLOT(onTabMoved(int, int)));
  }
}

void BrushPresetPanel::disconnectSignals() {
  if (m_toolHandle) {
    disconnect(m_toolHandle, SIGNAL(toolSwitched()), this, SLOT(onToolSwitched()));
    disconnect(m_toolHandle, SIGNAL(toolChanged()), this, SLOT(onToolChanged()));
    disconnect(m_toolHandle, SIGNAL(toolComboBoxListChanged(std::string)), 
               this, SLOT(onToolComboBoxListChanged(std::string)));
  }
  
  // Tab bar signals
  if (m_tabBar) {
    disconnect(m_tabBar, SIGNAL(currentChanged(int)), this, SLOT(onTabChanged(int)));
    disconnect(m_tabBar, SIGNAL(tabTextChanged(int)), this, SLOT(onTabTextChanged(int)));
    disconnect(m_tabBar, SIGNAL(movePage(int, int)), this, SLOT(onTabMoved(int, int)));
  }
}

void BrushPresetPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  
  // Refresh brush preset commands to ensure they're up-to-date
  ToolPresetCommandManager::instance()->refreshPresetCommands();
  
  connectSignals();
  
  // Initialize display with correct tool type detection
  onToolSwitched();
}

void BrushPresetPanel::hideEvent(QHideEvent *e) {
  TPanel::hideEvent(e);
  disconnectSignals();
}

void BrushPresetPanel::enterEvent(QEvent *e) {
  TPanel::enterEvent(e);
  
  // Check if tool type changed (e.g., level switched while panel wasn't focused)
  QString currentType = detectCurrentToolType();
  if (!currentType.isEmpty() && currentType != m_currentToolType) {
    // Tool type changed externally - full refresh
    m_currentToolType = currentType;
    initializeTabs();
    refreshPresetList();
  }
  // No full refresh on every enter - avoids resetting page state
}

void BrushPresetPanel::reset() {
  clearPresetList();
  m_currentToolType = "";
  m_currentPreset = "";
  m_toolLabel->setText(tr("No brush tool selected"));
  m_addPresetButton->setEnabled(false);
  m_removePresetButton->setEnabled(false);
}

//-----------------------------------------------------------------------------
// Tool detection
//-----------------------------------------------------------------------------

QString BrushPresetPanel::detectCurrentToolType() {
  TTool *tool = getCurrentBrushTool();
  if (!tool) return "";
  
  std::string toolName = tool->getName();
  int targetType = tool->getTargetType();
  
  // CRITICAL: Return ONLY the LEVEL type, NOT the style/engine type.
  // Pages are organized per level type (vector, toonzraster, raster).
  // The active style (MyPaint, Texture, Generated, etc.) only affects
  // the display label, NOT the page key. This ensures pages remain
  // stable when the user changes styles on the same level.
  if (toolName == T_Brush || toolName == T_PaintBrush || toolName == T_Eraser) {
    if (targetType & TTool::VectorImage) {
      return "vector";
    } else if (targetType & TTool::ToonzImage) {
      return "toonzraster";
    } else if (targetType & TTool::RasterImage) {
      return "raster";
    }
  }
  
  return "";
}

TTool* BrushPresetPanel::getCurrentBrushTool() {
  if (!m_toolHandle) return nullptr;
  
  TTool *tool = m_toolHandle->getTool();
  if (!tool) return nullptr;
  
  // Check if it's a brush tool
  std::string toolName = tool->getName();
  if (toolName == T_Brush || toolName == T_PaintBrush || toolName == T_Eraser) {
    return tool;
  }
  
  return nullptr;
}

//-----------------------------------------------------------------------------
// Preset loading
//-----------------------------------------------------------------------------

QList<QString> BrushPresetPanel::loadPresetsForTool(const QString &toolType) {
  QList<QString> presets;
  
  // Get presets from tool's TEnumProperty
  // which has already loaded all presets from files
  TTool *tool = getCurrentBrushTool();
  if (!tool) return presets;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return presets;
  
  // Find "Preset:" property
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (prop->getName() == "Preset:") {
      TEnumProperty *presetProp = dynamic_cast<TEnumProperty*>(prop);
      if (presetProp) {
        // Get all enumeration values (unordered)
        const TEnumProperty::Range &range = presetProp->getRange();
        QList<QString> allPresets;
        for (const auto &item : range) {
          QString presetName = QString::fromStdWString(item);
          // Exclude the "<custom>" preset
          if (presetName != QString::fromStdWString(L"<custom>")) {
            allPresets.append(presetName);
          }
        }
        
        // Try to load custom order from file
        QString orderFileName;
        if (toolType == "vector") {
          orderFileName = "brush_vector_order.txt";
        } else if (toolType == "toonzraster") {
          orderFileName = "brush_toonzraster_order.txt";
        } else if (toolType == "raster") {
          orderFileName = "brush_raster_order.txt";
        }
        
        if (!orderFileName.isEmpty()) {
          TFilePath orderFilePath = TEnv::getConfigDir() + orderFileName.toStdString();
          QFile orderFile(orderFilePath.getQString());
          
          if (orderFile.exists() && orderFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&orderFile);
            QSet<QString> addedPresets;  // Track already added presets
            
            while (!in.atEnd()) {
              QString presetName = in.readLine().trimmed();
              if (!presetName.isEmpty() && allPresets.contains(presetName) && !addedPresets.contains(presetName)) {
                presets.append(presetName);
                addedPresets.insert(presetName);
              }
            }
            orderFile.close();
            
            // Add any missing presets (new presets not in order file yet)
            for (const QString &preset : allPresets) {
              if (!addedPresets.contains(preset)) {
                presets.append(preset);
              }
            }
            
            return presets;
          }
        }
        
        // No order file found, return alphabetical order
        presets = allPresets;
      }
      break;
    }
  }
  
  return presets;
}

void BrushPresetPanel::refreshPresetList() {
  clearPresetList();
  
  // Delete old button group and create a new one
  if (m_presetButtonGroup) {
    delete m_presetButtonGroup;
  }
  m_presetButtonGroup = new QButtonGroup(this);
  m_presetButtonGroup->setExclusive(true);  // Only one button checked at a time
  
  // Use m_currentToolType which is set by onToolSwitched/onToolChanged
  // before calling this method. Fall back to detection if not set yet.
  QString toolType = m_currentToolType;
  if (toolType.isEmpty()) {
    toolType = detectCurrentToolType();
  }
  
  if (toolType.isEmpty()) {
    m_toolLabel->setText(tr("No brush tool selected"));
    m_addPresetButton->setEnabled(false);
    m_removePresetButton->setEnabled(false);
    return;
  }
  
  m_currentToolType = toolType;
  
  // Update label: detect the active STYLE sub-type for display only.
  // This does NOT affect page storage (pages are keyed by level type only).
  QString toolDisplayName;
  TApplication *labelApp = TApp::instance();
  TColorStyle *activeStyle = labelApp ? labelApp->getCurrentLevelStyle() : nullptr;
  
  if (toolType == "vector") {
    if (dynamic_cast<TVectorBrushStyle*>(activeStyle)) {
      toolDisplayName = tr("Custom Vector Brush");
    } else if (dynamic_cast<TTextureStyle*>(activeStyle)) {
      toolDisplayName = tr("Vector Texture");
    } else if (activeStyle && !dynamic_cast<TSolidColorStyle*>(activeStyle)) {
      std::string brushId = activeStyle->getBrushIdName();
      if (brushId.find("VectorImagePatternStrokeStyle:") == 0 ||
          brushId.find("RasterImagePatternStrokeStyle:") == 0) {
        toolDisplayName = tr("Vector Trail");
      } else {
        toolDisplayName = tr("Vector Generated");
      }
    } else {
      toolDisplayName = tr("Vector Brush");
    }
  } else if (toolType == "toonzraster") {
    if (dynamic_cast<TMyPaintBrushStyle*>(activeStyle)) {
      toolDisplayName = tr("MyPaint Brush Tnz");
    } else if (dynamic_cast<TTextureStyle*>(activeStyle)) {
      toolDisplayName = tr("Toonz Raster Texture");
    } else {
      toolDisplayName = tr("Toonz Raster Brush");
    }
  } else if (toolType == "raster") {
    if (dynamic_cast<TMyPaintBrushStyle*>(activeStyle)) {
      toolDisplayName = tr("MyPaint Brush");
    } else if (dynamic_cast<TTextureStyle*>(activeStyle)) {
      toolDisplayName = tr("Raster Texture");
    } else {
      toolDisplayName = tr("Raster Brush");
    }
  }
  m_toolLabel->setText(toolDisplayName);
  
  // Only T_Brush tool has add/remove preset; T_PaintBrush and T_Eraser do not
  TTool *tool = getCurrentBrushTool();
  m_addPresetButton->setEnabled(tool && tool->getName() == T_Brush);
  
  // Get current page
  BrushPresetPage *currentPage = getCurrentPage();
  QList<QString> presets;
  
  if (!currentPage) {
    // No page available - shouldn't happen, but handle gracefully
    return;
  }
  
  // Load all physically existing presets
  QList<QString> allPresets = loadPresetsForTool(toolType);
  
  // Show presets that belong to the current page.
  // Take a COPY of the names list since removePreset() modifies the
  // underlying QStringList and would invalidate the range-based iterator.
  bool hasDeletedPresets = false;
  if (currentPage->getPresetCount() > 0) {
    QStringList pageNames = currentPage->getPresetNames();
    for (const QString &presetName : pageNames) {
      if (allPresets.contains(presetName)) {
        if (!presets.contains(presetName))
          presets.append(presetName);
      } else {
        currentPage->removePreset(presetName);
        hasDeletedPresets = true;
      }
    }
  }
  
  if (hasDeletedPresets) {
    savePageConfiguration();
  }
  
  // ALWAYS check for orphan presets (exist on disk but not in any page).
  // This handles two cases:
  // 1. First launch: Page 1 is empty, all presets are orphans
  // 2. Preset added from ToolOptionBar: new preset not in any page yet
  // Orphans are ALWAYS added to the CURRENT page (not just Page 1).
  QList<BrushPresetPage*> &allPages = m_pages[m_currentToolType];
  bool hasNewOrphans = false;
  for (const QString &presetName : allPresets) {
    bool foundInAnyPage = false;
    for (BrushPresetPage *page : allPages) {
      if (page->hasPreset(presetName)) {
        foundInAnyPage = true;
        break;
      }
    }
    
    if (!foundInAnyPage) {
      currentPage->addPreset(presetName);
      if (!presets.contains(presetName))
        presets.append(presetName);
      hasNewOrphans = true;
    }
  }
  
  if (hasNewOrphans) {
    savePageConfiguration();
  }
  
  // Calculate optimal number of columns based on panel width
  int panelWidth = m_scrollArea->viewport()->width();
  QSize itemSize = getItemSizeForViewMode();
  int spacing = (m_viewMode == ListView) ? 3 : 8;
  
  // Calculate optimal number of columns
  int optimalColumns = qMax(1, (panelWidth + spacing) / (itemSize.width() + spacing));
  
  // Limit according to view mode
  int maxColumns = 0;
  switch (m_viewMode) {
    case ListView: maxColumns = 1; break;
    case GridSmall: maxColumns = 6; break;
    case GridMedium: maxColumns = 4; break;
    case GridLarge: maxColumns = 3; break;
    default: maxColumns = optimalColumns; break;
  }
  
  optimalColumns = qMin(optimalColumns, maxColumns);
  m_currentColumns = optimalColumns;
  
  // Create preset buttons in a grid
  int row = 0, col = 0;
  const int numColumns = m_currentColumns;
  
  // Get active preset (tool already obtained above)
  TPropertyGroup *props = tool ? tool->getProperties(0) : nullptr;
  QString currentPresetName;
  
  if (props) {
    for (int i = 0; i < props->getPropertyCount(); ++i) {
      TProperty *prop = props->getProperty(i);
      if (prop->getName() == "Preset:") {
        TEnumProperty *presetProp = dynamic_cast<TEnumProperty*>(prop);
        if (presetProp) {
          currentPresetName = QString::fromStdWString(presetProp->getValue());
        }
        break;
      }
    }
  }
  
  // Determine if we are in list mode
  bool isListMode = (m_viewMode == ListView);
  
  // Determine if we are in GridSmall mode (for icon size adjustment)
  bool isSmallMode = (m_viewMode == GridSmall);
  
  for (const QString &presetName : presets) {
    BrushPresetItem *item = new BrushPresetItem(presetName, toolType, isListMode, m_useSampleStrokes, m_presetContainer);
    
    // Apply size according to mode (fixed height, flexible width)
    item->setMinimumSize(itemSize);
    item->setMaximumSize(QWIDGETSIZE_MAX, itemSize.height());
    
    // Apply current border, background, small mode, and checkbox states
    item->setShowBorders(m_showBorders);
    item->setShowBackgrounds(m_showBackgrounds);
    item->setSmallMode(isSmallMode);
    item->setCheckboxVisible(m_checkboxMode);
    item->setMultiSelected(m_selectedPresets.contains(presetName));
    
    // Add to button group to manage exclusivity
    m_presetButtonGroup->addButton(item);
    
    // Mark the active preset
    if (presetName == currentPresetName) {
      item->setChecked(true);
      m_currentPreset = presetName;
      // Enable remove button for valid presets (not <custom>)
      if (presetName != QString::fromStdWString(L"<custom>")) {
        m_removePresetButton->setEnabled(true);
      }
    }
    
    connect(item, &BrushPresetItem::presetSelected, 
            this, &BrushPresetPanel::onPresetItemClicked);
    connect(item, &BrushPresetItem::presetReordered,
            this, &BrushPresetPanel::reorderPreset);
    
    m_presetLayout->addWidget(item, row, col);
    
    col++;
    if (col >= numColumns) {
      col = 0;
      row++;
    }
  }
  
  // Adjust spacing according to view mode
  m_presetLayout->setSpacing(spacing);
  
  // Apply borders state
  updateItemBorders();
  
  // If no preset is selected, disable remove button
  if (currentPresetName.isEmpty() || currentPresetName == QString::fromStdWString(L"<custom>")) {
    m_removePresetButton->setEnabled(false);
  }
}

void BrushPresetPanel::clearPresetList() {
  // Remove all preset widgets
  while (QLayoutItem *item = m_presetLayout->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }
}

//-----------------------------------------------------------------------------
// Preset actions
//-----------------------------------------------------------------------------

void BrushPresetPanel::applyPreset(const QString &presetName) {
  TTool *tool = getCurrentBrushTool();
  if (!tool) return;
  
  // Get preset property
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (prop->getName() == "Preset:") {
      TEnumProperty *presetProp = dynamic_cast<TEnumProperty*>(prop);
      if (presetProp) {
        // Change preset value
        presetProp->setValue(presetName.toStdWString());
        m_currentPreset = presetName;
        
        // CRUCIAL: Trigger onPropertyChanged to actually load the preset
        tool->onPropertyChanged("Preset:");
        
        // Notify the system of the change
        if (m_toolHandle) {
          m_toolHandle->notifyToolChanged();
        }
        
        // Update checked states of all Custom Panels
        ToolPresetCommandManager::instance()->updateCheckedStates();
      }
      break;
    }
  }
  
  // Update visual selection (QButtonGroup handles exclusivity automatically)
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (BrushPresetItem *presetItem = dynamic_cast<BrushPresetItem*>(item->widget())) {
      if (presetItem->getPresetName() == presetName) {
        presetItem->setChecked(true);  // QButtonGroup will uncheck others automatically
        break;
      }
    }
  }
  
  // Enable remove button except for <custom>
  m_removePresetButton->setEnabled(!presetName.isEmpty() && 
                                   presetName != QString::fromStdWString(L"<custom>"));
}

void BrushPresetPanel::addNewPreset() {
  TTool *tool = getCurrentBrushTool();
  if (!tool) return;
  
  // Initialize the popup
  if (!m_presetNamePopup) {
    m_presetNamePopup = new PresetNamePopup();
  }
  
  if (!m_presetNamePopup->getName().isEmpty()) {
    m_presetNamePopup->removeName();
  }
  
  // Get the name
  bool ret = m_presetNamePopup->exec();
  if (!ret) return;
  
  QString name = m_presetNamePopup->getName();
  m_presetNamePopup->removeName();
  
  if (name.isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Name"), 
                        tr("Please enter a valid preset name."));
    return;
  }
  
  // Use command (handler in tnztools) to avoid LNK2019 when linking from toonz
  if (tool->getName() != T_Brush) return;
  BrushPresetBridge::setPendingPresetName(name);
  CommandManager::instance()->execute(MI_AddBrushPreset);
  
  // Add new preset to current page
  BrushPresetPage *currentPage = getCurrentPage();
  if (currentPage && !currentPage->hasPreset(name)) {
    currentPage->addPreset(name);
    savePageConfiguration();
  }
  
  refreshPresetList();
  ToolPresetCommandManager::instance()->refreshPresetCommands();
}

void BrushPresetPanel::removeCurrentPreset() {
  if (m_currentPreset.isEmpty()) return;
  
  TTool *tool = getCurrentBrushTool();
  if (!tool) return;
  
  // Don't remove <custom> preset
  if (m_currentPreset == QString::fromStdWString(L"<custom>")) return;
  
  // Ask for confirmation
  int ret = QMessageBox::question(
      this, tr("Remove Preset"),
      tr("Are you sure you want to remove the preset '%1'?").arg(m_currentPreset),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  
  if (ret != QMessageBox::Yes) return;
  
  // Remove from current page first
  BrushPresetPage *currentPage = getCurrentPage();
  if (currentPage) {
    currentPage->removePreset(m_currentPreset);
    savePageConfiguration();
  }
  
  // Use command (handler in tnztools) to avoid LNK2019 when linking from toonz
  if (tool->getName() != T_Brush) return;
  CommandManager::instance()->execute(MI_RemoveBrushPreset);
  
  m_currentPreset = QString::fromStdWString(L"<custom>");
  m_removePresetButton->setEnabled(false);
  refreshPresetList();
  ToolPresetCommandManager::instance()->refreshPresetCommands();
}

//-----------------------------------------------------------------------------
// Drag & Drop reordering
//-----------------------------------------------------------------------------

void BrushPresetPanel::reorderPreset(const QString &fromPreset, const QString &toPreset) {
  if (m_currentToolType.isEmpty()) return;
  
  // Get order file path based on tool type
  QString orderFileName;
  if (m_currentToolType == "vector") {
    orderFileName = "brush_vector_order.txt";
  } else if (m_currentToolType == "toonzraster") {
    orderFileName = "brush_toonzraster_order.txt";
  } else if (m_currentToolType == "raster") {
    orderFileName = "brush_raster_order.txt";
  } else {
    return;  // Unknown tool type
  }
  
  TFilePath orderFilePath = TEnv::getConfigDir() + orderFileName.toStdString();
  
  // Get current page
  BrushPresetPage *currentPage = getCurrentPage();
  if (!currentPage) return;
  
  // Get preset list from current page
  QStringList presets = currentPage->getPresetNames();
  if (presets.isEmpty()) return;
  
  // Find indices
  int fromIndex = presets.indexOf(fromPreset);
  int toIndex = presets.indexOf(toPreset);
  
  if (fromIndex == -1 || toIndex == -1) return;
  if (fromIndex == toIndex) return;
  
  // Reorder list
  presets.move(fromIndex, toIndex);
  
  // Update page
  currentPage->setPresetNames(presets);
  
  // Save configuration
  savePageConfiguration();
  
  // Refresh UI
  refreshPresetList();
}

//-----------------------------------------------------------------------------
// Slots
//-----------------------------------------------------------------------------

void BrushPresetPanel::onToolSwitched() {
  // CRITICAL: Detect tool type FIRST, before initializing tabs
  // Otherwise initializeTabs() loads pages for the OLD tool type
  QString newToolType = detectCurrentToolType();
  
  // Save previous page's selection before switching tool type
  if (!m_currentToolType.isEmpty() && !m_currentPreset.isEmpty()) {
    BrushPresetPage *prevPage = getCurrentPage();
    if (prevPage) {
      prevPage->setLastSelectedPreset(m_currentPreset);
      savePageConfiguration();
    }
  }
  
  m_currentToolType = newToolType;
  
  // Now initialize tabs with the CORRECT tool type
  initializeTabs();
  
  // Refresh preset list for current page
  refreshPresetList();
}

void BrushPresetPanel::onToolChanged() {
  // CRITICAL: Check if tool type has changed (e.g., switching between
  // vector and raster levels while keeping the same tool active).
  // onToolSwitched() only fires when the tool itself changes,
  // but onToolChanged() fires when the level type changes too.
  QString newToolType = detectCurrentToolType();
  if (!newToolType.isEmpty() && newToolType != m_currentToolType) {
    // Tool type changed (e.g., switched from vector level to raster level)
    // Save previous page's selection
    if (!m_currentToolType.isEmpty() && !m_currentPreset.isEmpty()) {
      BrushPresetPage *prevPage = getCurrentPage();
      if (prevPage) {
        prevPage->setLastSelectedPreset(m_currentPreset);
        savePageConfiguration();
      }
    }
    
    m_currentToolType = newToolType;
    initializeTabs();
    refreshPresetList();
    return;
  }
  
  // Same tool type - update checked states and refresh display label
  // (the style sub-type may have changed, e.g., switched from solid to MyPaint)
  updateCheckedStates();
  
  // Update the display label to reflect the current style sub-type
  // without touching the pages or presets
  if (!m_currentToolType.isEmpty()) {
    TApplication *labelApp = TApp::instance();
    TColorStyle *activeStyle = labelApp ? labelApp->getCurrentLevelStyle() : nullptr;
    QString toolDisplayName;
    
    if (m_currentToolType == "vector") {
      if (dynamic_cast<TVectorBrushStyle*>(activeStyle)) {
        toolDisplayName = tr("Custom Vector Brush");
      } else if (dynamic_cast<TTextureStyle*>(activeStyle)) {
        toolDisplayName = tr("Vector Texture");
      } else if (activeStyle && !dynamic_cast<TSolidColorStyle*>(activeStyle)) {
        std::string brushId = activeStyle->getBrushIdName();
        if (brushId.find("VectorImagePatternStrokeStyle:") == 0 ||
            brushId.find("RasterImagePatternStrokeStyle:") == 0) {
          toolDisplayName = tr("Vector Trail");
        } else {
          toolDisplayName = tr("Vector Generated");
        }
      } else {
        toolDisplayName = tr("Vector Brush");
      }
    } else if (m_currentToolType == "toonzraster") {
      if (dynamic_cast<TMyPaintBrushStyle*>(activeStyle)) {
        toolDisplayName = tr("MyPaint Brush Tnz");
      } else if (dynamic_cast<TTextureStyle*>(activeStyle)) {
        toolDisplayName = tr("Toonz Raster Texture");
      } else {
        toolDisplayName = tr("Toonz Raster Brush");
      }
    } else if (m_currentToolType == "raster") {
      if (dynamic_cast<TMyPaintBrushStyle*>(activeStyle)) {
        toolDisplayName = tr("MyPaint Brush");
      } else if (dynamic_cast<TTextureStyle*>(activeStyle)) {
        toolDisplayName = tr("Raster Texture");
      } else {
        toolDisplayName = tr("Raster Brush");
      }
    }
    
    if (!toolDisplayName.isEmpty()) {
      m_toolLabel->setText(toolDisplayName);
    }
  }
}

void BrushPresetPanel::updateCheckedStates() {
  // Update selection if preset has changed
  QString toolType = detectCurrentToolType();
  if (toolType.isEmpty() || toolType != m_currentToolType) {
    return;
  }
  
  TTool *tool = getCurrentBrushTool();
  if (!tool) return;
  
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return;
  
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (prop->getName() == "Preset:") {
      TEnumProperty *presetProp = dynamic_cast<TEnumProperty*>(prop);
      if (presetProp) {
        QString newPreset = QString::fromStdWString(presetProp->getValue());
        if (newPreset != m_currentPreset) {
          m_currentPreset = newPreset;
          // Update remove button state
          if (newPreset == QString::fromStdWString(L"<custom>")) {
            m_removePresetButton->setEnabled(false);
          } else {
            m_removePresetButton->setEnabled(true);
          }
        }
        
        // Always update visual selection (even if m_currentPreset hasn't changed)
        // because another panel may have changed the preset
        for (int j = 0; j < m_presetLayout->count(); ++j) {
          QLayoutItem *item = m_presetLayout->itemAt(j);
          if (BrushPresetItem *presetItem = dynamic_cast<BrushPresetItem*>(item->widget())) {
            presetItem->setChecked(presetItem->getPresetName() == newPreset);
          }
        }
        
        // Enable/disable remove button
        if (newPreset.isEmpty() || newPreset == QString::fromStdWString(L"<custom>")) {
          m_removePresetButton->setEnabled(false);
        } else {
          m_removePresetButton->setEnabled(true);
        }
      }
      break;
    }
  }
}

void BrushPresetPanel::onPresetItemClicked(const QString &presetName, const QString &toolType) {
  // In checkbox mode: toggle multi-selection instead of applying
  if (m_checkboxMode) {
    togglePresetSelection(presetName);
    return;
  }
  
  applyPreset(presetName);
  
  // Save this selection as the last selected preset for the current page
  BrushPresetPage *currentPage = getCurrentPage();
  if (currentPage) {
    currentPage->setLastSelectedPreset(presetName);
    savePageConfiguration();
  }
}

void BrushPresetPanel::onAddPresetClicked() {
  // Simple: just add new preset (no Shift modifier check)
  addNewPreset();
}

void BrushPresetPanel::onRemovePresetClicked() {
  // Mirror of Tool Options Bar - Remove current preset
  removeCurrentPreset();
}

void BrushPresetPanel::onRefreshClicked() {
  refreshPresetList();
}

void BrushPresetPanel::onNonDestructiveToggled(bool checked) {
  BrushPresetBridge::setNonDestructiveMode(checked);
  QSettings settings;
  settings.setValue("BrushPresetPanel/NonDestructive", checked);
}

void BrushPresetPanel::onSelectivePresetToggled(bool checked) {
  BrushPresetBridge::setSelectivePresetMode(checked);
  QSettings settings;
  settings.setValue("BrushPresetPanel/SelectivePreset", checked);
}

void BrushPresetPanel::onPlainColorClicked() {
  TApplication *app = TApp::instance();
  if (!app) return;
  TPaletteHandle *ph =
      app->getPaletteController()->getCurrentLevelPalette();
  if (!ph) return;
  TPalette *palette = ph->getPalette();
  if (!palette) return;
  int styleIndex = app->getCurrentLevelStyleIndex();
  TColorStyle *currentStyle = palette->getStyle(styleIndex);
  if (!currentStyle) return;
  if (dynamic_cast<TSolidColorStyle *>(currentStyle)) return;

  // Read user-controlled color (TTextureStyle::getMainColor returns the
  // texture's average pixel color, not the user's drawing color).
  TPixel32 color = TPixel32::Black;
  if (dynamic_cast<TTextureStyle *>(currentStyle) &&
      currentStyle->getColorParamCount() > 0)
    color = currentStyle->getColorParamValue(0);
  else
    color = currentStyle->getMainColor();

  TSolidColorStyle *newStyle = new TSolidColorStyle(color);

  TUndo *undo = BrushPresetBridge::createStyleOverwriteUndo(
      palette, styleIndex, currentStyle, newStyle, ph);

  palette->setStyle(styleIndex, newStyle);
  palette->setDirtyFlag(true);
  ph->notifyColorStyleChanged(false);
  TUndoManager::manager()->add(undo);
}

void BrushPresetPanel::onToolComboBoxListChanged(std::string id) {
  // Refresh list if preset combo has changed
  // This captures additions/deletions from the ToolOptionsBar
  if (id == "Preset:") {
    // Skip if a compound operation (paste/rename) is in progress
    if (m_suppressRefresh) return;
    // Force refresh even if we are on the same tool type
    refreshPresetList();
  }
}

//-----------------------------------------------------------------------------
// View mode management (Phase 2 - To be fully implemented later)
//-----------------------------------------------------------------------------

void BrushPresetPanel::resizeEvent(QResizeEvent *event) {
  TPanel::resizeEvent(event);
  
  // Smart adjustment: adapt number of columns based on width
  // Only in grid mode (not in list mode)
  if (m_presetLayout->count() > 0) {
    int panelWidth = m_scrollArea->viewport()->width();
    QSize itemSize = getItemSizeForViewMode();
    int spacing = m_presetLayout->spacing();
    
    // Calculate optimal number of columns
    int optimalColumns = qMax(1, (panelWidth + spacing) / (itemSize.width() + spacing));
    
    // Limit according to view mode
    int maxColumns = 0;
    switch (m_viewMode) {
      case ListView: maxColumns = 1; break;
      case GridSmall: maxColumns = 6; break;
      case GridMedium: maxColumns = 4; break;
      case GridLarge: maxColumns = 3; break;
      default: maxColumns = optimalColumns; break;
    }
    
    optimalColumns = qMin(optimalColumns, maxColumns);
    optimalColumns = qMax(1, optimalColumns);  // Au moins 1 colonne
    
    // If number of columns has changed, reorganize
    if (optimalColumns != m_currentColumns) {
      m_currentColumns = optimalColumns;
      reorganizeLayout(m_currentColumns);
    }
  }
}

void BrushPresetPanel::createViewModeMenu() {
  m_viewModeMenu = new QMenu(this);
  
  QActionGroup *viewModeGroup = new QActionGroup(this);
  viewModeGroup->setExclusive(true);
  
  // Option List View
  QAction *listAction = new QAction(tr("List"), this);
  listAction->setCheckable(true);
  listAction->setData(static_cast<int>(ListView));
  listAction->setChecked(m_viewMode == ListView);  // Check if current mode
  viewModeGroup->addAction(listAction);
  m_viewModeMenu->addAction(listAction);
  
  // Option Small
  QAction *smallAction = new QAction(tr("Small"), this);
  smallAction->setCheckable(true);
  smallAction->setData(static_cast<int>(GridSmall));
  smallAction->setChecked(m_viewMode == GridSmall);  // Check if current mode
  viewModeGroup->addAction(smallAction);
  m_viewModeMenu->addAction(smallAction);
  
  // Option Medium
  QAction *mediumAction = new QAction(tr("Medium"), this);
  mediumAction->setCheckable(true);
  mediumAction->setData(static_cast<int>(GridMedium));
  mediumAction->setChecked(m_viewMode == GridMedium);  // Check if current mode
  viewModeGroup->addAction(mediumAction);
  m_viewModeMenu->addAction(mediumAction);
  
  // Option Large (default)
  QAction *largeAction = new QAction(tr("Large"), this);
  largeAction->setCheckable(true);
  largeAction->setChecked(m_viewMode == GridLarge);  // Check if current mode
  largeAction->setData(static_cast<int>(GridLarge));
  viewModeGroup->addAction(largeAction);
  m_viewModeMenu->addAction(largeAction);
  
  // Connect view mode actions
  connect(viewModeGroup, &QActionGroup::triggered, [this](QAction *action) {
    ViewMode mode = static_cast<ViewMode>(action->data().toInt());
    onViewModeChanged(mode);
  });
  
  // Separator before multi-select option
  m_viewModeMenu->addSeparator();
  
  // Show Checkboxes option for multi-selection
  QAction *checkboxAction = new QAction(tr("Show Checkboxes"), this);
  checkboxAction->setCheckable(true);
  checkboxAction->setChecked(m_checkboxMode);
  m_viewModeMenu->addAction(checkboxAction);
  connect(checkboxAction, &QAction::triggered, [this, checkboxAction]() {
    m_checkboxMode = checkboxAction->isChecked();
    if (!m_checkboxMode) {
      // Clear multi-selection when disabling checkbox mode
      clearMultiSelection();
    }
    updateCheckboxVisibility();
  });
  
  // Separator before dynamic rendering options
  m_viewModeMenu->addSeparator();
  
  // Use Sample Strokes option (for dynamic fallback graphics)
  QAction *sampleStrokesAction = new QAction(tr("Use Sample Strokes"), this);
  sampleStrokesAction->setCheckable(true);
  sampleStrokesAction->setChecked(m_useSampleStrokes);
  m_viewModeMenu->addAction(sampleStrokesAction);
  connect(sampleStrokesAction, &QAction::triggered, [this, sampleStrokesAction]() {
    m_useSampleStrokes = sampleStrokesAction->isChecked();
    // Save preference
    QSettings settings;
    settings.setValue("BrushPresetPanel/useSampleStrokes", m_useSampleStrokes);
    // Refresh display in Brush Preset Panel
    refreshPresetList();
    // Refresh Universal Size icons in Custom Panels
    ToolPresetCommandManager::instance()->refreshSizeCommands();
  });
  
  // Add Show/Hide submenu
  addShowHideContextMenu(m_viewModeMenu);
  
  // Connect button to show menu manually (no triangle indicator)
  connect(m_viewModeButton, &QPushButton::clicked, [this]() {
    QPoint pos = m_viewModeButton->mapToGlobal(QPoint(0, m_viewModeButton->height()));
    m_viewModeMenu->exec(pos);
  });
}

void BrushPresetPanel::setViewMode(ViewMode mode) {
  if (m_viewMode == mode) return;
  m_viewMode = mode;
  
  // Update display with new mode
  updatePresetItemsSizes();
  
  // Reorganize grid
  refreshPresetList();
}

int BrushPresetPanel::getColumnsForViewMode() const {
  // For now, default mode: 2 columns
  switch (m_viewMode) {
    case ListView: return 1;
    case GridSmall: return 4;
    case GridMedium: return 3;
    case GridLarge: 
    default: return 2;
  }
}

QSize BrushPresetPanel::getItemSizeForViewMode() const {
  // Original cell sizes (icons adapt inside)
  switch (m_viewMode) {
    case ListView: return QSize(200, 40);
    case GridSmall: return QSize(80, 50);
    case GridMedium: return QSize(100, 55);
    case GridLarge:
    default: return QSize(120, 60);
  }
}

void BrushPresetPanel::updatePresetItemsSizes() {
  QSize itemSize = getItemSizeForViewMode();
  bool isListMode = (m_viewMode == ListView);
  bool isSmallMode = (m_viewMode == GridSmall);
  
  // Update size of all existing items (fixed height, flexible width)
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (BrushPresetItem *presetItem = dynamic_cast<BrushPresetItem*>(item->widget())) {
      presetItem->setMinimumSize(itemSize);
      presetItem->setMaximumSize(QWIDGETSIZE_MAX, itemSize.height());
      presetItem->setListMode(isListMode);  // Update mode
      presetItem->setSmallMode(isSmallMode);  // Update small mode for icon size
      // Recalculate scaled icon with new size
      presetItem->updateScaledIcon();
    }
  }
  
  // Adjust spacing according to view mode
  int spacing = (m_viewMode == ListView) ? 3 : 8;
  m_presetLayout->setSpacing(spacing);
}

void BrushPresetPanel::onViewModeChanged(ViewMode mode) {
  setViewMode(mode);
  refreshPresetList();
  
  // Save view preference in settings
  QSettings settings;
  settings.setValue("BrushPresetPanel/viewMode", static_cast<int>(mode));
}

//-----------------------------------------------------------------------------
// Context menu (right-click) - GUI Show/Hide
//-----------------------------------------------------------------------------

void BrushPresetPanel::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = new QMenu(this);
  
  // Paste action (available when clipboard has content, even on empty page)
  if (!m_clipboardPresets.isEmpty()) {
    QAction *pasteAction = menu->addAction(
        tr("Paste (%1 preset(s))").arg(m_clipboardPresets.size()));
    connect(pasteAction, &QAction::triggered, [this]() {
      pastePresetsToCurrentPage();
    });
    menu->addSeparator();
  }
  
  // Smart Preset toggles
  QAction *ndAction = menu->addAction(tr("Non-Destructive"));
  ndAction->setCheckable(true);
  ndAction->setChecked(m_nonDestructiveToggle->isChecked());
  connect(ndAction, &QAction::toggled, m_nonDestructiveToggle, &QToolButton::setChecked);
  
  QAction *spAction = menu->addAction(tr("Selective Preset"));
  spAction->setCheckable(true);
  spAction->setChecked(m_selectivePresetToggle->isChecked());
  connect(spAction, &QAction::toggled, m_selectivePresetToggle, &QToolButton::setChecked);
  
  QAction *pcAction = menu->addAction(tr("Plain Color"));
  connect(pcAction, &QAction::triggered, this, &BrushPresetPanel::onPlainColorClicked);
  
  menu->addSeparator();
  
  addShowHideContextMenu(menu);
  menu->exec(event->globalPos());
  delete menu;
}

void BrushPresetPanel::addShowHideContextMenu(QMenu *menu) {
  QMenu *showHideMenu = menu->addMenu(tr("GUI Show / Hide"));
  
  // Action to show/hide cell borders
  QAction *bordersAction = showHideMenu->addAction(tr("Cell Borders"));
  bordersAction->setCheckable(true);
  bordersAction->setChecked(m_showBorders);
  bordersAction->setData("borders");  // Identifier for slot handler
  connect(bordersAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
  
  // Action to show/hide cell backgrounds (for total design control)
  QAction *backgroundsAction = showHideMenu->addAction(tr("Cell Backgrounds"));
  backgroundsAction->setCheckable(true);
  backgroundsAction->setChecked(m_showBackgrounds);
  backgroundsAction->setData("backgrounds");  // Identifier for slot handler
  connect(backgroundsAction, SIGNAL(triggered()), this, SLOT(onShowHideActionTriggered()));
}

void BrushPresetPanel::onShowHideActionTriggered() {
  QAction *action = qobject_cast<QAction*>(sender());
  if (!action) return;
  
  QString actionType = action->data().toString();
  QSettings settings;
  
  if (actionType == "borders") {
    // Toggle borders state
    m_showBorders = action->isChecked();
    updateItemBorders();
    settings.setValue("BrushPresetPanel/showBorders", m_showBorders);
  } else if (actionType == "backgrounds") {
    // Toggle backgrounds state
    m_showBackgrounds = action->isChecked();
    updateItemBackgrounds();
    settings.setValue("BrushPresetPanel/showBackgrounds", m_showBackgrounds);
  }
}

void BrushPresetPanel::updateItemBorders() {
  // Update borders state of all items
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (BrushPresetItem *presetItem = dynamic_cast<BrushPresetItem*>(item->widget())) {
      presetItem->setShowBorders(m_showBorders);  // Synchronize and redraw
    }
  }
}

void BrushPresetPanel::updateItemBackgrounds() {
  // Update backgrounds state of all items
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (BrushPresetItem *presetItem = dynamic_cast<BrushPresetItem*>(item->widget())) {
      presetItem->setShowBackgrounds(m_showBackgrounds);  // Synchronize and redraw
    }
  }
}

void BrushPresetPanel::reorganizeLayout(int newColumns) {
  if (newColumns < 1 || m_presetLayout->count() == 0) return;
  
  // Collect all existing widgets
  QList<QWidget*> widgets;
  while (m_presetLayout->count() > 0) {
    QLayoutItem *item = m_presetLayout->takeAt(0);
    if (QWidget *widget = item->widget()) {
      widgets.append(widget);
    }
    delete item;
  }
  
  // Reorganize in new grid
  int row = 0, col = 0;
  for (QWidget *widget : widgets) {
    m_presetLayout->addWidget(widget, row, col);
    col++;
    if (col >= newColumns) {
      col = 0;
      row++;
    }
  }
}

//-----------------------------------------------------------------------------
// Page access methods (modeled after TPalette)
//-----------------------------------------------------------------------------

BrushPresetPage* BrushPresetPanel::getCurrentPage() {
  return getPage(m_currentPageIndex);
}

BrushPresetPage* BrushPresetPanel::getPage(int pageIndex) {
  if (m_currentToolType.isEmpty()) return nullptr;
  
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  if (pageIndex >= 0 && pageIndex < pages.size()) {
    return pages[pageIndex];
  }
  return nullptr;
}

int BrushPresetPanel::getPageCount() const {
  if (m_currentToolType.isEmpty()) return 0;
  return m_pages.value(m_currentToolType).size();
}

void BrushPresetPanel::clearPages() {
  // Delete all pages for all tool types
  for (auto it = m_pages.begin(); it != m_pages.end(); ++it) {
    qDeleteAll(it.value());
    it.value().clear();
  }
  m_pages.clear();
}

//-----------------------------------------------------------------------------
// Multi-selection management
//-----------------------------------------------------------------------------

void BrushPresetPanel::togglePresetSelection(const QString &presetName) {
  if (m_selectedPresets.contains(presetName)) {
    m_selectedPresets.remove(presetName);
  } else {
    m_selectedPresets.insert(presetName);
  }
  
  // Update checkbox state on the corresponding item widget
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (!item || !item->widget()) continue;
    BrushPresetItem *presetItem = qobject_cast<BrushPresetItem*>(item->widget());
    if (presetItem && presetItem->getPresetName() == presetName) {
      presetItem->setMultiSelected(m_selectedPresets.contains(presetName));
      break;
    }
  }
}

void BrushPresetPanel::clearMultiSelection() {
  m_selectedPresets.clear();
  
  // Clear all checkbox states
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (!item || !item->widget()) continue;
    BrushPresetItem *presetItem = qobject_cast<BrushPresetItem*>(item->widget());
    if (presetItem) {
      presetItem->setMultiSelected(false);
    }
  }
}

void BrushPresetPanel::updateCheckboxVisibility() {
  for (int i = 0; i < m_presetLayout->count(); ++i) {
    QLayoutItem *item = m_presetLayout->itemAt(i);
    if (!item || !item->widget()) continue;
    BrushPresetItem *presetItem = qobject_cast<BrushPresetItem*>(item->widget());
    if (presetItem) {
      presetItem->setCheckboxVisible(m_checkboxMode);
    }
  }
}

//-----------------------------------------------------------------------------
// Context menu for presets (copy/cut/paste/delete)
//-----------------------------------------------------------------------------

void BrushPresetPanel::showPresetContextMenu(const QPoint &globalPos, const QString &presetName) {
  QMenu menu(this);
  
  // Determine the effective selection (if multi-select or single)
  QStringList effectiveSelection;
  if (m_checkboxMode && !m_selectedPresets.isEmpty()) {
    // Use multi-selection
    effectiveSelection = m_selectedPresets.values();
  } else {
    // Use the right-clicked preset
    effectiveSelection.append(presetName);
  }
  
  int selCount = effectiveSelection.size();
  
  // Copy
  QAction *copyAction = menu.addAction(
      selCount > 1 ? tr("Copy %1 Presets").arg(selCount) : tr("Copy"));
  connect(copyAction, &QAction::triggered, [this, effectiveSelection]() {
    m_clipboardPresets = effectiveSelection;
    m_clipboardIsCut = false;
  });
  
  // Cut
  QAction *cutAction = menu.addAction(
      selCount > 1 ? tr("Cut %1 Presets").arg(selCount) : tr("Cut"));
  connect(cutAction, &QAction::triggered, [this, effectiveSelection]() {
    m_clipboardPresets = effectiveSelection;
    m_clipboardIsCut = true;
  });
  
  // Paste (only if clipboard has content)
  if (!m_clipboardPresets.isEmpty()) {
    QAction *pasteAction = menu.addAction(
        tr("Paste (%1)").arg(m_clipboardPresets.size()));
    connect(pasteAction, &QAction::triggered, [this]() {
      pastePresetsToCurrentPage();
    });
  }
  
  menu.addSeparator();
  
  // Delete
  QAction *deleteAction = menu.addAction(
      selCount > 1 ? tr("Delete %1 Presets").arg(selCount) : tr("Delete"));
  connect(deleteAction, &QAction::triggered, [this, effectiveSelection]() {
    // Confirm
    int ret = QMessageBox::question(
        this, tr("Delete Presets"),
        tr("Are you sure you want to permanently delete %1 preset(s)?")
            .arg(effectiveSelection.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
    
    // Remove from current page
    BrushPresetPage *currentPage = getCurrentPage();
    if (currentPage) {
      for (const QString &name : effectiveSelection) {
        currentPage->removePreset(name);
      }
      savePageConfiguration();
    }
    
    // Remove from disk via batch command
    TTool *tool = getCurrentBrushTool();
    if (tool && tool->getName() == T_Brush) {
      for (const QString &name : effectiveSelection) {
        BrushPresetBridge::addPendingRemoveName(name);
      }
      CommandManager::instance()->execute(MI_RemoveBrushPresetByName);
    }
    
    m_currentPreset = QString::fromStdWString(L"<custom>");
    clearMultiSelection();
    refreshPresetList();
    ToolPresetCommandManager::instance()->refreshPresetCommands();
  });
  
  // Rename (only for single selection)
  if (selCount == 1) {
    QAction *renameAction = menu.addAction(tr("Rename"));
    connect(renameAction, &QAction::triggered, [this, presetName]() {
      PresetNamePopup popup(this);
      popup.setWindowTitle(tr("Rename Preset"));
      if (popup.exec() == QDialog::Accepted) {
        QString newName = popup.getName().trimmed();
        if (!newName.isEmpty() && newName != presetName) {
          renamePreset(presetName, newName);
        }
      }
    });
  }
  
  // Separator before page-specific actions
  if (getPageCount() > 1) {
    menu.addSeparator();
    
    // Move to page submenu
    QMenu *moveToMenu = menu.addMenu(tr("Move to Page"));
    QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
    for (int i = 0; i < pages.size(); ++i) {
      if (i == m_currentPageIndex) continue; // Skip current page
      QAction *pageAction = moveToMenu->addAction(pages[i]->getName());
      connect(pageAction, &QAction::triggered, [this, effectiveSelection, i]() {
        BrushPresetPage *srcPage = getCurrentPage();
        BrushPresetPage *dstPage = getPage(i);
        if (!srcPage || !dstPage) return;
        
        for (const QString &name : effectiveSelection) {
          srcPage->removePreset(name);
          if (!dstPage->hasPreset(name)) {
            dstPage->addPreset(name);
          }
        }
        savePageConfiguration();
        clearMultiSelection();
        refreshPresetList();
      });
    }
  }
  
  // Smart Preset section
  menu.addSeparator();
  
  QAction *ndAction2 = menu.addAction(tr("Non-Destructive"));
  ndAction2->setCheckable(true);
  ndAction2->setChecked(m_nonDestructiveToggle->isChecked());
  connect(ndAction2, &QAction::toggled, m_nonDestructiveToggle, &QToolButton::setChecked);
  
  QAction *spAction2 = menu.addAction(tr("Selective Preset"));
  spAction2->setCheckable(true);
  spAction2->setChecked(m_selectivePresetToggle->isChecked());
  connect(spAction2, &QAction::toggled, m_selectivePresetToggle, &QToolButton::setChecked);
  
  QAction *pcAction2 = menu.addAction(tr("Plain Color"));
  connect(pcAction2, &QAction::triggered, this, &BrushPresetPanel::onPlainColorClicked);
  
  menu.exec(globalPos);
}

void BrushPresetPanel::copySelectedPresets() {
  if (m_checkboxMode && !m_selectedPresets.isEmpty()) {
    m_clipboardPresets = m_selectedPresets.values();
  } else if (!m_currentPreset.isEmpty()) {
    m_clipboardPresets.clear();
    m_clipboardPresets.append(m_currentPreset);
  }
  m_clipboardIsCut = false;
}

void BrushPresetPanel::cutSelectedPresets() {
  copySelectedPresets();
  m_clipboardIsCut = true;
}

void BrushPresetPanel::pastePresetsToCurrentPage() {
  if (m_clipboardPresets.isEmpty()) return;
  
  BrushPresetPage *currentPage = getCurrentPage();
  if (!currentPage) return;
  
  // Suppress intermediate refreshes triggered by MI_AddBrushPreset notifications
  m_suppressRefresh = true;
  
  if (m_clipboardIsCut) {
    // Cut mode: move presets (same name, remove from source pages)
    for (const QString &presetName : m_clipboardPresets) {
      if (!currentPage->hasPreset(presetName)) {
        currentPage->addPreset(presetName);
      }
    }
    // Remove from all other pages
    QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
    for (BrushPresetPage *page : pages) {
      if (page == currentPage) continue;
      for (const QString &presetName : m_clipboardPresets) {
        page->removePreset(presetName);
      }
    }
    m_clipboardIsCut = false;
  } else {
    // Copy mode: create true duplicates with unique names
    // This ensures each pasted preset is independent from the original
    for (const QString &presetName : m_clipboardPresets) {
      QString newName = generateUniqueCopyName(presetName);
      if (duplicatePresetInFile(presetName, newName)) {
        currentPage->addPreset(newName);
      }
    }
  }
  
  m_suppressRefresh = false;
  
  m_clipboardPresets.clear();
  savePageConfiguration();
  refreshPresetList();
  ToolPresetCommandManager::instance()->refreshPresetCommands();
}

//-----------------------------------------------------------------------------
// Generate a unique copy name (e.g., "MyBrush (Copy)", "MyBrush (Copy 2)")
//-----------------------------------------------------------------------------

QString BrushPresetPanel::generateUniqueCopyName(const QString &baseName) {
  // Collect all known preset names from the tool's Preset: property
  QSet<QString> existingNames;
  TTool *tool = getCurrentBrushTool();
  if (tool) {
    TPropertyGroup *props = tool->getProperties(0);
    if (props) {
      for (int i = 0; i < props->getPropertyCount(); ++i) {
        TProperty *prop = props->getProperty(i);
        if (prop->getName() == "Preset:") {
          TEnumProperty *presetProp = dynamic_cast<TEnumProperty*>(prop);
          if (presetProp) {
            const std::vector<std::wstring> &range = presetProp->getRange();
            for (const std::wstring &w : range) {
              existingNames.insert(QString::fromStdWString(w));
            }
          }
          break;
        }
      }
    }
  }
  
  // Also check all page references (some presets may be in pages but not yet synced)
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  for (BrushPresetPage *page : pages) {
    const QStringList &names = page->getPresetNames();
    for (const QString &n : names) {
      existingNames.insert(n);
    }
  }
  
  // Generate unique name
  QString candidate = baseName + " (Copy)";
  if (!existingNames.contains(candidate)) return candidate;
  
  for (int i = 2; i < 1000; ++i) {
    candidate = baseName + QString(" (Copy %1)").arg(i);
    if (!existingNames.contains(candidate)) return candidate;
  }
  
  return baseName + " (Copy)";  // Fallback
}

//-----------------------------------------------------------------------------
// Duplicate a preset: apply the source, then add as new with the given name
//-----------------------------------------------------------------------------

bool BrushPresetPanel::duplicatePresetInFile(const QString &srcName, const QString &newName) {
  TTool *tool = getCurrentBrushTool();
  if (!tool || tool->getName() != T_Brush) return false;
  
  // Save current preset selection to restore later
  QString previousPreset = m_currentPreset;
  
  // Step 1: Apply the source preset (loads all its settings into the tool)
  TPropertyGroup *props = tool->getProperties(0);
  if (!props) return false;
  
  TEnumProperty *presetProp = nullptr;
  for (int i = 0; i < props->getPropertyCount(); ++i) {
    TProperty *prop = props->getProperty(i);
    if (prop->getName() == "Preset:") {
      presetProp = dynamic_cast<TEnumProperty*>(prop);
      break;
    }
  }
  if (!presetProp) return false;
  
  // Load source preset into tool
  presetProp->setValue(srcName.toStdWString());
  tool->onPropertyChanged("Preset:");
  
  // Step 2: Add the preset with the new name (saves tool's current state)
  BrushPresetBridge::setPendingPresetName(newName);
  CommandManager::instance()->execute(MI_AddBrushPreset);
  
  // Step 3: Restore original preset
  if (!previousPreset.isEmpty() && 
      previousPreset != QString::fromStdWString(L"<custom>")) {
    presetProp->setValue(previousPreset.toStdWString());
    tool->onPropertyChanged("Preset:");
  }
  
  return true;
}

//-----------------------------------------------------------------------------
// Rename a preset: duplicate with new name, then delete the old one
//-----------------------------------------------------------------------------

void BrushPresetPanel::renamePreset(const QString &oldName, const QString &newName) {
  if (oldName.isEmpty() || newName.isEmpty() || oldName == newName) return;
  
  TTool *tool = getCurrentBrushTool();
  if (!tool || tool->getName() != T_Brush) return;
  
  // Suppress intermediate refreshes during the compound operation
  m_suppressRefresh = true;
  
  // Step 1: Duplicate the preset with the new name (old still exists on disk)
  if (!duplicatePresetInFile(oldName, newName)) {
    m_suppressRefresh = false;
    return;
  }
  
  // Step 2: Remove the old preset from disk
  BrushPresetBridge::addPendingRemoveName(oldName);
  CommandManager::instance()->execute(MI_RemoveBrushPresetByName);
  
  m_suppressRefresh = false;
  
  // Step 3: Update all page references in-place (preserves position)
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  for (BrushPresetPage *page : pages) {
    if (page->hasPreset(oldName)) {
      page->replacePreset(oldName, newName);
    }
    if (page->getLastSelectedPreset() == oldName) {
      page->setLastSelectedPreset(newName);
    }
  }
  
  // Step 4: Save, refresh, and select the renamed preset
  m_currentPreset = newName;
  savePageConfiguration();
  refreshPresetList();
  ToolPresetCommandManager::instance()->refreshPresetCommands();
  
  // Apply the renamed preset
  applyPreset(newName);
}

void BrushPresetPanel::deleteSelectedPresets() {
  QStringList toDelete;
  if (m_checkboxMode && !m_selectedPresets.isEmpty()) {
    toDelete = m_selectedPresets.values();
  } else if (!m_currentPreset.isEmpty() && 
             m_currentPreset != QString::fromStdWString(L"<custom>")) {
    toDelete.append(m_currentPreset);
  }
  
  if (toDelete.isEmpty()) return;
  
  int ret = QMessageBox::question(
      this, tr("Delete Presets"),
      tr("Are you sure you want to permanently delete %1 preset(s)?")
          .arg(toDelete.size()),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (ret != QMessageBox::Yes) return;
  
  BrushPresetPage *currentPage = getCurrentPage();
  if (currentPage) {
    for (const QString &name : toDelete) {
      currentPage->removePreset(name);
    }
    savePageConfiguration();
  }
  
  TTool *tool = getCurrentBrushTool();
  if (tool && tool->getName() == T_Brush) {
    for (const QString &name : toDelete) {
      BrushPresetBridge::addPendingRemoveName(name);
    }
    CommandManager::instance()->execute(MI_RemoveBrushPresetByName);
  }
  
  m_currentPreset = QString::fromStdWString(L"<custom>");
  clearMultiSelection();
  refreshPresetList();
  ToolPresetCommandManager::instance()->refreshPresetCommands();
}

//-----------------------------------------------------------------------------
// Page/Tab management (modeled after Level Palette Editor)
//-----------------------------------------------------------------------------

void BrushPresetPanel::initializeTabs() {
  if (!m_tabBar) return;
  if (m_currentToolType.isEmpty()) return;
  
  // Block signals to avoid triggering onTabChanged during rebuild
  m_tabBar->blockSignals(true);
  
  // Remove all existing tabs
  int tabCount = m_tabBar->count();
  for (int i = tabCount - 1; i >= 0; i--) {
    m_tabBar->removeTab(i);
  }
  
  // Load page configuration for current tool type from QSettings
  // This populates m_pages[m_currentToolType]
  loadPageConfiguration();
  
  // Get pages for current tool (create default page if none exist)
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  if (pages.isEmpty()) {
    BrushPresetPage *defaultPage = new BrushPresetPage(tr("Page 1"), 0);
    pages.append(defaultPage);
  }
  
  // Create tabs with palette icon (same as PaletteViewer::updateTabBar)
  QIcon tabIcon = createQIcon("palette_tab");
  m_tabBar->setIconSize(QSize(16, 16));
  for (int i = 0; i < pages.size(); ++i) {
    m_tabBar->addTab(tabIcon, pages[i]->getName());
  }
  
  // Set current page (bounded to valid range)
  if (m_currentPageIndex < 0 || m_currentPageIndex >= m_tabBar->count()) {
    m_currentPageIndex = 0;
  }
  m_tabBar->setCurrentIndex(m_currentPageIndex);
  
  // Unblock signals
  m_tabBar->blockSignals(false);
}

void BrushPresetPanel::updateTabBar() {
  initializeTabs();
}

void BrushPresetPanel::switchToPage(int pageIndex) {
  if (pageIndex < 0 || pageIndex >= m_tabBar->count()) return;
  
  // Avoid re-entering when setCurrentIndex triggers currentChanged
  if (pageIndex == m_currentPageIndex && m_tabBar->currentIndex() == pageIndex) {
    return;
  }
  
  // Save last selected preset for previous page BEFORE changing index
  BrushPresetPage *prevPage = getCurrentPage();
  if (prevPage && !m_currentPreset.isEmpty()) {
    prevPage->setLastSelectedPreset(m_currentPreset);
    savePageConfiguration();
  }
  
  // Change to new page index
  m_currentPageIndex = pageIndex;
  
  // Block signals temporarily to avoid recursive calls via onTabChanged
  m_tabBar->blockSignals(true);
  m_tabBar->setCurrentIndex(pageIndex);
  m_tabBar->blockSignals(false);
  
  // Restore last selected preset for new page
  BrushPresetPage *newPage = getCurrentPage();
  QString restoredPreset;
  if (newPage) {
    restoredPreset = newPage->getLastSelectedPreset();
    if (restoredPreset.isEmpty() || !newPage->hasPreset(restoredPreset)) {
      restoredPreset = QString();
    }
  }
  
  // CRITICAL: Force refresh preset list for current page
  // This ensures we show only presets belonging to this page
  refreshPresetList();
  
  // After refreshing, restore the selection for this page
  // This applies the preset to the tool AND updates the visual selection
  if (!restoredPreset.isEmpty()) {
    applyPreset(restoredPreset);
  } else {
    // No saved selection for this page - deselect all
    m_currentPreset = QString::fromStdWString(L"<custom>");
    m_removePresetButton->setEnabled(false);
  }
}

void BrushPresetPanel::addNewPage() {
  if (m_currentToolType.isEmpty()) return;
  
  // Generate new page name
  int pageNum = m_tabBar->count() + 1;
  QString newPageName = tr("Page %1").arg(pageNum);
  
  // Create new page object
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  BrushPresetPage *newPage = new BrushPresetPage(newPageName, pages.size());
  pages.append(newPage);
  
  // Add tab with palette icon
  QIcon tabIcon = createQIcon("palette_tab");
  int newIndex = m_tabBar->addTab(tabIcon, newPageName);
  
  // Save configuration
  savePageConfiguration();
  
  // Switch to new page
  switchToPage(newIndex);
}

void BrushPresetPanel::deletePage(int pageIndex) {
  if (pageIndex < 0 || pageIndex >= m_tabBar->count()) return;
  if (m_currentToolType.isEmpty()) return;
  
  // Don't allow deleting the last page
  if (m_tabBar->count() <= 1) {
    QMessageBox::information(this, tr("Cannot Delete Page"),
                            tr("Cannot delete the last page. At least one page must exist."));
    return;
  }
  
  // Get the page and its presets
  BrushPresetPage *page = getPage(pageIndex);
  if (!page) return;
  
  QStringList pagePresets = page->getPresetNames();
  QString pageName = m_tabBar->tabText(pageIndex);
  
  // Confirm deletion with accurate information
  QString message;
  if (pagePresets.isEmpty()) {
    message = tr("Are you sure you want to delete the page '%1'?").arg(pageName);
  } else {
    message = tr("Are you sure you want to delete the page '%1'?\n"
                 "This will permanently delete %2 preset(s) contained in this page.")
              .arg(pageName).arg(pagePresets.size());
  }
  
  int ret = QMessageBox::question(this, tr("Delete Page"), message,
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (ret != QMessageBox::Yes) return;
  
  // ACTUALLY delete the presets from disk via the batch remove command.
  // Queue all preset names and execute the batch remove command.
  TTool *tool = getCurrentBrushTool();
  if (tool && tool->getName() == T_Brush && !pagePresets.isEmpty()) {
    for (const QString &presetName : pagePresets) {
      BrushPresetBridge::addPendingRemoveName(presetName);
    }
    CommandManager::instance()->execute(MI_RemoveBrushPresetByName);
  }
  
  // Delete page object
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  if (pageIndex < pages.size()) {
    delete pages[pageIndex];
    pages.removeAt(pageIndex);
    for (int i = 0; i < pages.size(); ++i) {
      pages[i]->setIndex(i);
    }
  }
  
  // Remove tab
  m_tabBar->removeTab(pageIndex);
  savePageConfiguration();
  
  // Switch to appropriate page
  if (m_currentPageIndex >= m_tabBar->count()) {
    m_currentPageIndex = m_tabBar->count() - 1;
  }
  switchToPage(m_currentPageIndex);
  
  // Refresh commands
  ToolPresetCommandManager::instance()->refreshPresetCommands();
}

void BrushPresetPanel::renamePage(int pageIndex, const QString &newName) {
  if (pageIndex < 0 || pageIndex >= m_tabBar->count()) return;
  if (m_currentToolType.isEmpty()) return;
  if (newName.isEmpty()) return;
  
  // Update page object
  BrushPresetPage *page = getPage(pageIndex);
  if (page) {
    page->setName(newName);
  }
  
  // Update tab text (already done by tab bar, but ensure consistency)
  m_tabBar->setTabText(pageIndex, newName);
  
  // Save configuration
  savePageConfiguration();
}

void BrushPresetPanel::movePageTab(int srcIndex, int dstIndex) {
  if (srcIndex < 0 || srcIndex >= m_tabBar->count()) return;
  if (dstIndex < 0 || dstIndex >= m_tabBar->count()) return;
  if (srcIndex == dstIndex) return;
  if (m_currentToolType.isEmpty()) return;
  
  // Update pages list
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  if (srcIndex < pages.size() && dstIndex < pages.size()) {
    pages.move(srcIndex, dstIndex);
    
    // Update indices of all pages
    for (int i = 0; i < pages.size(); ++i) {
      pages[i]->setIndex(i);
    }
  }
  
  // Block signals during tab move to prevent stale refreshes from currentChanged
  m_tabBar->blockSignals(true);
  m_tabBar->moveTab(srcIndex, dstIndex);
  
  // Update current page index if it was moved
  if (m_currentPageIndex == srcIndex) {
    m_currentPageIndex = dstIndex;
  } else if (srcIndex < m_currentPageIndex && dstIndex >= m_currentPageIndex) {
    m_currentPageIndex--;
  } else if (srcIndex > m_currentPageIndex && dstIndex <= m_currentPageIndex) {
    m_currentPageIndex++;
  }
  
  // Sync tab bar to the updated current page index
  m_tabBar->setCurrentIndex(m_currentPageIndex);
  m_tabBar->blockSignals(false);
  
  // Save configuration
  savePageConfiguration();
  
  // Force refresh to show correct content for the current page after swap
  refreshPresetList();
}

void BrushPresetPanel::savePageConfiguration() {
  if (m_currentToolType.isEmpty()) return;
  
  QSettings settings;
  QList<BrushPresetPage*> pages = m_pages.value(m_currentToolType);
  
  // Clear old page data first to avoid stale data
  int oldPageCount = settings.value(QString("BrushPresetPanel/PageCount_%1").arg(m_currentToolType), 0).toInt();
  for (int i = 0; i < oldPageCount; ++i) {
    QString prefix = QString("BrushPresetPanel/%1_Page%2").arg(m_currentToolType).arg(i);
    settings.remove(prefix + "_Name");
    settings.remove(prefix + "_Presets");
    settings.remove(prefix + "_LastPreset");
  }
  
  // Save number of pages
  settings.setValue(QString("BrushPresetPanel/PageCount_%1").arg(m_currentToolType), pages.size());
  
  // Save each page configuration with explicit index
  for (int i = 0; i < pages.size(); ++i) {
    BrushPresetPage *page = pages[i];
    // Ensure page index is synchronized
    page->setIndex(i);
    
    QString prefix = QString("BrushPresetPanel/%1_Page%2").arg(m_currentToolType).arg(i);
    
    // Save page name
    settings.setValue(prefix + "_Name", page->getName());
    
    // Save presets in this page (join with ||| to avoid comma conflicts)
    settings.setValue(prefix + "_Presets", page->getPresetNames().join("|||"));
    
    // Save last selected preset
    settings.setValue(prefix + "_LastPreset", page->getLastSelectedPreset());
  }
  
  // Save current page index
  settings.setValue(QString("BrushPresetPanel/CurrentPage_%1").arg(m_currentToolType),
                   m_currentPageIndex);
  
  // Force sync to disk
  settings.sync();
}

void BrushPresetPanel::loadPageConfiguration() {
  if (m_currentToolType.isEmpty()) return;
  
  QSettings settings;
  
  // MIGRATION: Clean up obsolete engine-specific keys from previous versions.
  // Pages are now stored per level type (vector/toonzraster/raster) only.
  // Remove any data stored under old "mypainttnz" or "mypaint" keys
  // and merge their presets into the correct level-type pages.
  static bool migrationDone = false;
  if (!migrationDone) {
    QStringList obsoleteKeys = {"mypainttnz", "mypaint"};
    for (const QString &oldKey : obsoleteKeys) {
      int oldPageCount = settings.value(
          QString("BrushPresetPanel/PageCount_%1").arg(oldKey), 0).toInt();
      if (oldPageCount > 0) {
        // Remove all old page data
        for (int i = 0; i < oldPageCount; ++i) {
          QString prefix = QString("BrushPresetPanel/%1_Page%2").arg(oldKey).arg(i);
          settings.remove(prefix + "_Name");
          settings.remove(prefix + "_Presets");
          settings.remove(prefix + "_LastPreset");
        }
        settings.remove(QString("BrushPresetPanel/PageCount_%1").arg(oldKey));
        settings.remove(QString("BrushPresetPanel/CurrentPage_%1").arg(oldKey));
      }
    }
    migrationDone = true;
    settings.sync();
  }
  
  // Clear existing pages for this tool type
  QList<BrushPresetPage*> &pages = m_pages[m_currentToolType];
  qDeleteAll(pages);
  pages.clear();
  
  // Load number of pages (default 0 means no user data yet - will auto-create Page 1)
  int pageCount = settings.value(QString("BrushPresetPanel/PageCount_%1").arg(m_currentToolType), 0).toInt();
  
  if (pageCount <= 0) pageCount = 1;
  
  // Load each page
  for (int i = 0; i < pageCount; ++i) {
    QString prefix = QString("BrushPresetPanel/%1_Page%2").arg(m_currentToolType).arg(i);
    
    // Load page name
    QString pageName = settings.value(prefix + "_Name", tr("Page %1").arg(i + 1)).toString();
    
    // Create page
    BrushPresetPage *page = new BrushPresetPage(pageName, i);
    
    // Load presets (using ||| separator to avoid comma conflicts)
    QString presetsStr = settings.value(prefix + "_Presets", "").toString();
    if (!presetsStr.isEmpty()) {
      QStringList presetNames = presetsStr.split("|||", Qt::SkipEmptyParts);
      page->setPresetNames(presetNames);
    }
    
    // Load last selected preset
    QString lastPreset = settings.value(prefix + "_LastPreset", "").toString();
    page->setLastSelectedPreset(lastPreset);
    
    pages.append(page);
  }
  
  // Ensure at least one page exists
  if (pages.isEmpty()) {
    BrushPresetPage *defaultPage = new BrushPresetPage(tr("Page 1"), 0);
    pages.append(defaultPage);
  }
  
  // Load current page index
  m_currentPageIndex = settings.value(QString("BrushPresetPanel/CurrentPage_%1").arg(m_currentToolType), 0).toInt();
  
  // Bound to valid range
  if (m_currentPageIndex < 0 || m_currentPageIndex >= pages.size()) {
    m_currentPageIndex = 0;
  }
}


//-----------------------------------------------------------------------------
// Tab/Page slots
//-----------------------------------------------------------------------------

void BrushPresetPanel::onTabChanged(int index) {
  // Only process if index actually changed (avoid recursion)
  if (index < 0 || index == m_currentPageIndex) return;
  switchToPage(index);
}

void BrushPresetPanel::onTabTextChanged(int index) {
  if (index < 0 || index >= m_tabBar->count()) return;
  QString newName = m_tabBar->tabText(index);
  renamePage(index, newName);
}

void BrushPresetPanel::onTabMoved(int srcIndex, int dstIndex) {
  movePageTab(srcIndex, dstIndex);
}

void BrushPresetPanel::onAddPageClicked() {
  addNewPage();
}

void BrushPresetPanel::onDeletePageClicked() {
  if (m_currentPageIndex >= 0 && m_currentPageIndex < m_tabBar->count()) {
    deletePage(m_currentPageIndex);
  }
}

