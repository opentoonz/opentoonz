#include "brushpresetpanel.h"

// ToonzQt includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvdialog.h"
#include "toolpresetcommandmanager.h"

// ToonzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"
#include "toonz/mypaintbrushstyle.h"

// ToonzCore includes
#include "tenv.h"
#include "tfilepath.h"
#include "tsystem.h"

// Tools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tproperty.h"

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
// BrushPresetItem implementation
//=============================================================================

BrushPresetItem::BrushPresetItem(const QString &presetName, const QString &toolType, bool isListMode, QWidget *parent)
    : QToolButton(parent)
    , m_presetName(presetName)
    , m_toolType(toolType)
    , m_hasIcon(false)
    , m_isCustomIcon(false)
    , m_isListMode(isListMode)
    , m_isSmallMode(false)
    , m_showBorders(true)
    , m_showBackgrounds(true) {
  
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
    iconName = "palette";    // Toonz Raster brush (palette icon)
  } else if (toolType == "mypaint") {
    iconName = "paintbrush"; // MyPaint brush (raster)
  } else if (toolType == "mypainttnz") {
    iconName = "palette";    // MyPaint brush on Toonz Raster
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
        
        painter.drawPixmap(iconX, iconY, m_scaledIconCache);
        
        // Draw text to the right of icon
        QRect textRect(iconX + m_scaledIconCache.width() + margin, margin, 
                       width() - iconX - m_scaledIconCache.width() - (2 * margin), 
                       height() - (2 * margin));
        painter.setPen(palette().color(QPalette::ButtonText));
        painter.setFont(font());
        
        QString displayText = m_presetName;
        QFontMetrics fm(font());
        if (fm.horizontalAdvance(displayText) > textRect.width()) {
          displayText = fm.elidedText(displayText, Qt::ElideMiddle, textRect.width());
        }
        
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, displayText);
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
        
        // Center icon (which has a FIXED size) in available space
        int iconX = margin + (availableWidth - m_scaledIconCache.width()) / 2;
        int iconY = margin + (availableHeight - m_scaledIconCache.height()) / 2;
        
        painter.drawPixmap(iconX, iconY, m_scaledIconCache);
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
  
  QString fromPreset = event->mimeData()->text();
  QString toPreset = m_presetName;
  
  // Don't reorder if dropping on itself
  if (fromPreset == toPreset) {
    event->ignore();
    return;
  }
  
  // Emit signal to parent panel to handle reordering
  emit presetReordered(fromPreset, toPreset);
  
  event->acceptProposedAction();
}

//=============================================================================
// BrushPresetPanel implementation
//=============================================================================

BrushPresetPanel::BrushPresetPanel(QWidget *parent)
    : TPanel(parent)
    , m_scrollArea(nullptr)
    , m_presetContainer(nullptr)
    , m_presetLayout(nullptr)
    , m_addPresetButton(nullptr)
    , m_removePresetButton(nullptr)
    , m_refreshButton(nullptr)
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
    , m_showBorders(false)  // Borders disabled by default
    , m_showBackgrounds(true) {  // Backgrounds enabled by default
  
  // Load preferences from settings FIRST (before creating UI)
  QSettings settings;
  m_showBorders = settings.value("BrushPresetPanel/showBorders", false).toBool();
  m_showBackgrounds = settings.value("BrushPresetPanel/showBackgrounds", true).toBool();
  
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
  
  // Button bar at top
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  
  // Use native OpenToonz icons matching ToolOptionBar design
  m_removePresetButton = new QPushButton(mainWidget);
  m_removePresetButton->setIcon(createQIcon("minus"));
  m_removePresetButton->setIconSize(QSize(16, 16));
  m_removePresetButton->setFixedSize(24, 24);
  m_removePresetButton->setToolTip(tr("Remove selected preset"));
  m_removePresetButton->setEnabled(false);
  
  m_addPresetButton = new QPushButton(mainWidget);
  m_addPresetButton->setIcon(createQIcon("plus"));
  m_addPresetButton->setIconSize(QSize(16, 16));
  m_addPresetButton->setFixedSize(24, 24);
  m_addPresetButton->setToolTip(tr("Add current settings as new preset"));
  m_addPresetButton->setEnabled(false);
  
  // Refresh button with circular arrow icon (repeat)
  m_refreshButton = new QPushButton(mainWidget);
  m_refreshButton->setIcon(createQIcon("repeat"));
  m_refreshButton->setIconSize(QSize(16, 16));
  m_refreshButton->setFixedSize(24, 24);
  m_refreshButton->setToolTip(tr("Refresh preset list"));
  m_refreshButton->setEnabled(true);
  
  // Hamburger menu for display modes - use standard OpenToonz menu icon
  m_viewModeButton = new QPushButton(mainWidget);
  m_viewModeButton->setIcon(createQIcon("menu"));
  m_viewModeButton->setFixedSize(24, 24);  // Match Level Palette button size
  m_viewModeButton->setToolTip(tr("View mode"));
  
  buttonLayout->addWidget(m_removePresetButton);
  buttonLayout->addWidget(m_addPresetButton);
  buttonLayout->addWidget(m_refreshButton);
  buttonLayout->addStretch();
  buttonLayout->addWidget(m_viewModeButton);
  
  mainLayout->addLayout(buttonLayout);
  
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
}

void BrushPresetPanel::disconnectSignals() {
  if (m_toolHandle) {
    disconnect(m_toolHandle, SIGNAL(toolSwitched()), this, SLOT(onToolSwitched()));
    disconnect(m_toolHandle, SIGNAL(toolChanged()), this, SLOT(onToolChanged()));
    disconnect(m_toolHandle, SIGNAL(toolComboBoxListChanged(std::string)), 
               this, SLOT(onToolComboBoxListChanged(std::string)));
  }
}

void BrushPresetPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  
  // Refresh brush preset commands to ensure they're up-to-date
  // (same approach as ShortcutPopup and CustomPanelEditorPopup)
  ToolPresetCommandManager::instance()->refreshPresetCommands();
  
  connectSignals();
  onToolSwitched();  // Initialize display
}

void BrushPresetPanel::hideEvent(QHideEvent *e) {
  TPanel::hideEvent(e);
  disconnectSignals();
}

void BrushPresetPanel::enterEvent(QEvent *e) {
  TPanel::enterEvent(e);
  // Refresh automatically when mouse enters panel
  refreshPresetList();
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
  
  // Determine brush type based on name and target type
  if (toolName == T_Brush || toolName == T_PaintBrush || toolName == T_Eraser) {
    if (targetType & TTool::VectorImage) {
      return "vector";
    } else if (targetType & TTool::ToonzImage) {
      // Check if it's a MyPaint brush on Toonz Raster
      TApplication *app = TApp::instance();
      if (app) {
        TColorStyle *style = app->getCurrentLevelStyle();
        if (dynamic_cast<TMyPaintBrushStyle*>(style)) {
          return "mypainttnz";
        }
      }
      return "toonzraster";
    } else if (targetType & TTool::RasterImage) {
      // Check if it's a MyPaint brush on Raster
      TApplication *app = TApp::instance();
      if (app) {
        TColorStyle *style = app->getCurrentLevelStyle();
        if (dynamic_cast<TMyPaintBrushStyle*>(style)) {
          return "mypaint";
        }
      }
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
        } else if (toolType == "raster" || toolType == "mypaint" || toolType == "mypainttnz") {
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
  
  QString toolType = detectCurrentToolType();
  
  if (toolType.isEmpty()) {
    m_toolLabel->setText(tr("No brush tool selected"));
    m_addPresetButton->setEnabled(false);
    m_removePresetButton->setEnabled(false);
    return;
  }
  
  m_currentToolType = toolType;
  
  // Update label
  QString toolDisplayName;
  if (toolType == "vector") {
    toolDisplayName = tr("Vector Brush");
  } else if (toolType == "toonzraster") {
    toolDisplayName = tr("Toonz Raster Brush");
  } else if (toolType == "mypaint") {
    toolDisplayName = tr("MyPaint Brush");
  } else if (toolType == "mypainttnz") {
    toolDisplayName = tr("MyPaint Brush Tnz");
  } else if (toolType == "raster") {
    toolDisplayName = tr("Raster Brush");
  }
  m_toolLabel->setText(toolDisplayName);
  
  m_addPresetButton->setEnabled(true);
  
  // Load all presets for current tool
  QList<QString> presets = loadPresetsForTool(toolType);
  m_presetCache[toolType] = presets;
  
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
  
  // Get active preset
  TTool *tool = getCurrentBrushTool();
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
    BrushPresetItem *item = new BrushPresetItem(presetName, toolType, isListMode, m_presetContainer);
    
    // Apply size according to mode (fixed height, flexible width)
    item->setMinimumSize(itemSize);
    item->setMaximumSize(QWIDGETSIZE_MAX, itemSize.height());
    
    // Apply current border, background, and small mode states
    item->setShowBorders(m_showBorders);
    item->setShowBackgrounds(m_showBackgrounds);
    item->setSmallMode(isSmallMode);
    
    // Add to button group to manage exclusivity
    m_presetButtonGroup->addButton(item);
    
    // Mark the active preset
    if (presetName == currentPresetName) {
      item->setChecked(true);
      m_currentPreset = presetName;
      m_removePresetButton->setEnabled(true);
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
  
  // Note: Direct call to tool's addPreset method causes linking issues
  // because the methods are not exported from tnztools library.
  // For now, users can add presets via the Tool Options Bar.
  QMessageBox::information(this, tr("Add Preset"), 
                           tr("Please use the [+] button in the Tool Options Bar to add new presets.\n\n"
                              "The new preset will automatically appear in this panel."));
  return;
}

void BrushPresetPanel::removeCurrentPreset() {
  if (m_currentPreset.isEmpty()) return;
  
  TTool *tool = getCurrentBrushTool();
  if (!tool) return;
  
  // Ask for confirmation
  int ret = QMessageBox::question(
      this, tr("Remove Preset"),
      tr("Are you sure you want to remove the preset '%1'?").arg(m_currentPreset),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  
  if (ret != QMessageBox::Yes) return;
  
  // Note: Direct call to tool's removePreset method causes linking issues
  // because the methods are not exported from tnztools library.
  // For now, users can remove presets via the Tool Options Bar.
  QMessageBox::information(this, tr("Remove Preset"), 
                           tr("Please use the [-] button in the Tool Options Bar to remove presets.\n\n"
                              "The change will automatically be reflected in this panel."));
  return;
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
  } else if (m_currentToolType == "raster" || 
             m_currentToolType == "mypaint" || 
             m_currentToolType == "mypainttnz") {
    orderFileName = "brush_raster_order.txt";
  } else {
    return;  // Unknown tool type
  }
  
  TFilePath orderFilePath = TEnv::getConfigDir() + orderFileName.toStdString();
  
  // Load current preset list
  QList<QString> presets = m_presetCache.value(m_currentToolType);
  if (presets.isEmpty()) return;
  
  // Find indices
  int fromIndex = presets.indexOf(fromPreset);
  int toIndex = presets.indexOf(toPreset);
  
  if (fromIndex == -1 || toIndex == -1) return;
  if (fromIndex == toIndex) return;
  
  // Reorder list
  presets.move(fromIndex, toIndex);
  
  // Save new order to file
  QFile file(orderFilePath.getQString());
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    for (const QString &preset : presets) {
      out << preset << "\n";
    }
    file.close();
  }
  
  // Update cache
  m_presetCache[m_currentToolType] = presets;
  
  // Refresh UI
  refreshPresetList();
}

//-----------------------------------------------------------------------------
// Slots
//-----------------------------------------------------------------------------

void BrushPresetPanel::onToolSwitched() {
  refreshPresetList();
}

void BrushPresetPanel::onToolChanged() {
  // Update checked states (called when tool changes)
  updateCheckedStates();
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
  applyPreset(presetName);
}

void BrushPresetPanel::onAddPresetClicked() {
  addNewPreset();
}

void BrushPresetPanel::onRemovePresetClicked() {
  removeCurrentPreset();
}

void BrushPresetPanel::onRefreshClicked() {
  refreshPresetList();
}

void BrushPresetPanel::onToolComboBoxListChanged(std::string id) {
  // Refresh list if preset combo has changed
  // This captures additions/deletions from the ToolOptionsBar
  if (id == "Preset:") {
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
  
  // Connecter les actions
  connect(viewModeGroup, &QActionGroup::triggered, [this](QAction *action) {
    ViewMode mode = static_cast<ViewMode>(action->data().toInt());
    onViewModeChanged(mode);
  });
  
  // Connect button to show menu manually (no triangle indicator)
  connect(m_viewModeButton, &QPushButton::clicked, [this]() {
    // Show menu below the button
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

