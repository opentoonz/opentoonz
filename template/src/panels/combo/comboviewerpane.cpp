#include "comboviewerpane.h"
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QStyle>
#include <QApplication>

//=============================================================================
// Simple icon generator — colored geometric shapes
//=============================================================================

static QIcon makeToolIcon(const QColor& color, char shape) {
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    switch (shape) {
    case 'r': p.drawRoundedRect(2, 2, 16, 16, 3, 3); break;       // rectangle: Edit, Geo, Type
    case 's': p.drawRect(3, 3, 14, 14); break;                     // selection: Select
    case 'c': p.drawEllipse(3, 3, 14, 14); break;                  // circle: Brush, Fill
    case 'e': p.drawEllipse(7, 1, 6, 18); break;                   // ellipse: Paint
    case 'x': p.setPen(QPen(color.darker(200), 2));                // X: Eraser
              p.drawLine(4, 4, 16, 16); p.drawLine(16, 4, 4, 16); break;
    case 't': { QPointF tri[] = {{10,3},{17,17},{3,17}};           // triangle: Tape, Assist
               p.drawPolygon(tri, 3); } break;
    case 'h': { QPointF pts[] = {{3,10},{6,5},{14,5},{17,10},{14,15},{6,15}}; // hex: Finger, ControlPoint
               p.drawPolygon(pts, 6); } break;
    case 'd': p.drawEllipse(5, 5, 10, 10);                         // dot: Picker, RGB
              p.setBrush(color.lighter(150));
              p.drawEllipse(7, 7, 6, 6); break;
    case 'l': p.drawLine(4, 10, 16, 10); p.drawLine(4, 16, 16, 16); // lines: Ruler
              break;
    case 'p': p.drawEllipse(4, 4, 12, 12);                         // pinch: Pinch
              p.setBrush(color.darker(150));
              p.drawEllipse(6, 6, 8, 8); break;
    case 'z': p.drawEllipse(10, 3, 4, 14);                         // zoom: Zoom
              p.drawEllipse(6, 7, 10, 10); break;
    case 'a': p.drawLine(4, 10, 10, 4); p.drawLine(10, 4, 16, 10); // arrow: Hand
              p.drawLine(10, 4, 10, 17); break;
    default:  p.fillRect(2, 2, 16, 16, color); break;
    }
    p.end();
    return QIcon(pm);
}

// Color map for tool categories
static QColor toolColor(const QString& name) {
    if (name == "Edit" || name == "Select") return QColor(180, 180, 180);
    if (name == "Brush" || name == "Geo" || name == "Fill" || name == "Paint")
        return QColor(100, 180, 255);
    if (name == "Eraser" || name == "Tape") return QColor(255, 130, 100);
    if (name == "Picker" || name == "RGB") return QColor(180, 255, 130);
    if (name == "CPEdit") return QColor(255, 200, 80);
    if (name == "Zoom" || name == "Hand") return QColor(200, 170, 255);
    return QColor(150, 150, 220);
}

static char toolShape(const QString& name) {
    if (name == "Edit" || name == "Geo") return 'r';
    if (name == "Select") return 's';
    if (name == "Brush" || name == "Fill") return 'c';
    if (name == "Paint") return 'e';
    if (name == "Eraser") return 'x';
    if (name == "Tape" || name == "Assist") return 't';
    if (name == "Finger" || name == "CPEdit") return 'h';
    if (name == "Picker" || name == "RGB") return 'd';
    if (name == "Ruler") return 'l';
    if (name == "Zoom") return 'z';
    if (name == "Hand") return 'a';
    return 'r';
}

//=============================================================================
// PlaceholderViewer
//=============================================================================

PlaceholderViewer::PlaceholderViewer(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setObjectName("PlaceholderViewer");
}

void PlaceholderViewer::setRulers(Ruler* vRuler, Ruler* hRuler) {
    m_vRuler = vRuler; m_hRuler = hRuler; updateRulers();
}

void PlaceholderViewer::updateRulers() {
    if (m_vRuler) { m_vRuler->setZoomScale(m_zoom); m_vRuler->setPan(m_panY); m_vRuler->update(); }
    if (m_hRuler) { m_hRuler->setZoomScale(m_zoom); m_hRuler->setPan(m_panX); m_hRuler->update(); }
}

void PlaceholderViewer::paintEvent(QPaintEvent*) {
    QPainter p(this);
    int s = 32;
    for (int y = 0; y < height(); y += s)
        for (int x = 0; x < width(); x += s)
            p.fillRect(x, y, s, s, ((x/s)+(y/s))%2==0 ? QColor(60,60,60) : QColor(50,50,50));
    p.setPen(QColor(100,100,100));
    int cx = width()/2 + static_cast<int>(m_panX);
    int cy = height()/2 + static_cast<int>(m_panY);
    p.drawLine(cx-30, cy, cx+30, cy);
    p.drawLine(cx, cy-30, cx, cy+30);
    p.setPen(QColor(140,140,140));
    p.setFont(QFont("sans-serif", 12));
    p.drawText(rect(), Qt::AlignCenter, tr("Viewer\nPan: drag | Zoom: wheel"));
}

void PlaceholderViewer::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton) {
        m_panning = true; m_lastPos = e->pos(); setCursor(Qt::ClosedHandCursor);
    }
}
void PlaceholderViewer::mouseMoveEvent(QMouseEvent* e) {
    if (m_panning) {
        QPoint d = e->pos() - m_lastPos; m_panX += d.x(); m_panY += d.y();
        m_lastPos = e->pos(); updateRulers(); update();
    }
}
void PlaceholderViewer::mouseReleaseEvent(QMouseEvent*) {
    m_panning = false; setCursor(Qt::ArrowCursor);
}
void PlaceholderViewer::wheelEvent(QWheelEvent* e) {
    double f = e->angleDelta().y() > 0 ? 1.15 : 1.0/1.15;
    m_zoom = qBound(0.1, m_zoom*f, 50.0);
    updateRulers(); update();
}

//=============================================================================
// ViewerFlipConsole
//=============================================================================

ViewerFlipConsole::ViewerFlipConsole(QWidget* parent) : QWidget(parent) {
    setFixedHeight(36);
    setObjectName("FlipConsole");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    auto btnStyle = QStringLiteral(
        "QToolButton { background: #3a3a3a; border: 1px solid #555; "
        "border-radius: 2px; color: #ccc; font-weight: bold; } "
        "QToolButton:hover { background: #4a4a4a; border-color: #888; } "
        "QToolButton:checked { background: #007acc; color: white; } "
        "QToolButton:pressed { background: #005a9e; }");

    auto makeBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setText(text); b->setToolTip(tip); b->setFixedSize(26, 26);
        b->setStyleSheet(btnStyle);
        return b;
    };

    // First button: visibility/menu toggle (popup menu set externally)
    m_menuToggleBtn = new QToolButton(this);
    m_menuToggleBtn->setText("V");
    m_menuToggleBtn->setToolTip(tr("Toggle Toolbar Visibility"));
    m_menuToggleBtn->setFixedSize(26, 26);
    m_menuToggleBtn->setPopupMode(QToolButton::InstantPopup);
    m_menuToggleBtn->setStyleSheet(btnStyle);
    layout->addWidget(m_menuToggleBtn);
    layout->addSpacing(4);

    // Transport
    m_firstBtn = makeBtn("|<", tr("First Frame"));
    m_prevBtn  = makeBtn("<",  tr("Previous Frame"));
    m_playBtn  = makeBtn(">",  tr("Play"));
    m_pauseBtn = makeBtn("||", tr("Pause")); m_pauseBtn->hide();
    m_nextBtn  = makeBtn(">",  tr("Next Frame"));
    m_lastBtn  = makeBtn(">|", tr("Last Frame"));

    layout->addWidget(m_firstBtn);
    layout->addWidget(m_prevBtn);
    layout->addWidget(m_playBtn);
    layout->addWidget(m_pauseBtn);
    layout->addWidget(m_nextBtn);
    layout->addWidget(m_lastBtn);
    layout->addSpacing(6);

    m_loopBtn = new QToolButton(this);
    m_loopBtn->setCheckable(true);
    m_loopBtn->setText("L");
    m_loopBtn->setToolTip(tr("Loop"));
    m_loopBtn->setFixedSize(26, 26);
    m_loopBtn->setStyleSheet(btnStyle);

    // Connect all just-built buttons
    connect(m_playBtn, &QToolButton::clicked, this, &ViewerFlipConsole::onPlayPause);
    connect(m_pauseBtn, &QToolButton::clicked, this, &ViewerFlipConsole::onPlayPause);
    connect(m_firstBtn, &QToolButton::clicked, this, [this]{ onSliderChanged(0); });
    connect(m_lastBtn, &QToolButton::clicked, this, [this]{ onSliderChanged(m_frameCount); });
    connect(m_prevBtn, &QToolButton::clicked, this, [this]{ if(m_frame>0) onSliderChanged(m_frame-1); });
    connect(m_nextBtn, &QToolButton::clicked, this, [this]{ if(m_frame<m_frameCount) onSliderChanged(m_frame+1); });

    layout->addWidget(m_loopBtn);
    layout->addSpacing(4);

    // Frame slider
    m_frameSlider = new QSlider(Qt::Horizontal, this);
    m_frameSlider->setRange(0, m_frameCount);
    layout->addWidget(m_frameSlider, 1);

    m_frameLabel = new QLabel("0 / 120", this);
    m_frameLabel->setFixedWidth(70);
    m_frameLabel->setAlignment(Qt::AlignCenter);
    m_frameLabel->setStyleSheet("color: #ccc; background: transparent;");
    layout->addWidget(m_frameLabel);

    m_fpsSpin = new QSpinBox(this);
    m_fpsSpin->setRange(1, 60);
    m_fpsSpin->setValue(24);
    m_fpsSpin->setFixedWidth(50);
    m_fpsSpin->setToolTip(tr("FPS"));
    layout->addWidget(m_fpsSpin);

    connect(m_frameSlider, &QSlider::valueChanged, this, &ViewerFlipConsole::onSliderChanged);
}

void ViewerFlipConsole::setVisibilityMenu(QMenu* menu) {
    m_menuToggleBtn->setMenu(menu);
}

void ViewerFlipConsole::onPlayPause() {
    m_playing = !m_playing;
    m_playBtn->setVisible(!m_playing);
    m_pauseBtn->setVisible(m_playing);
    if (m_playing) emit played(); else emit paused();
}
void ViewerFlipConsole::onStop() { m_playing=false; m_playBtn->show(); m_pauseBtn->hide(); emit stopped(); }
void ViewerFlipConsole::onSliderChanged(int v) {
    m_frame = v;
    m_frameLabel->setText(QString("%1 / %2").arg(v).arg(m_frameCount));
    emit frameChanged(v);
}

//=============================================================================
// ComboViewerPanel
//=============================================================================

struct ToolLayoutEntry { const char* name; const char* tip; bool collapsible; bool separator; };
static const ToolLayoutEntry s_toolLayout[] = {
    {"Edit", "Edit Tool", false, false}, {"Select", "Selection Tool", false, false},
    {"", "", false, true},
    {"Brush", "Brush Tool", false, false}, {"Geo", "Geometric Tool", false, false},
    {"Type", "Type Tool", true, false}, {"Fill", "Fill Tool", false, false},
    {"Paint", "Paint Brush", false, false},
    {"", "", false, true},
    {"Eraser", "Eraser Tool", false, false}, {"Tape", "Tape Tool", false, false},
    {"Finger", "Finger Tool", false, false},
    {"", "", false, true},
    {"Picker", "Style Picker", false, false}, {"RGB", "RGB Picker", false, false},
    {"Ruler", "Ruler Tool", false, false}, {"Assist", "Edit Assistants", false, false},
    {"", "", false, true},
    {"CPEdit", "Control Point Editor", false, false},
    {"Pinch", "Pinch Tool", true, false}, {"Pump", "Pump Tool", true, false},
    {"Magnet", "Magnet Tool", true, false}, {"Bender", "Bender Tool", true, false},
    {"Iron", "Iron Tool", true, false}, {"Cutter", "Cutter Tool", true, false},
    {"", "", true, true},
    {"Skeleton", "Skeleton Tool", true, false}, {"Tracker", "Tracker Tool", true, false},
    {"Hook", "Hook Tool", true, false}, {"Plastic", "Plastic Tool", true, false},
    {"", "", false, true},
    {"Zoom", "Zoom Tool", false, false}, {"Rotate", "Rotate Tool", true, false},
    {"Hand", "Hand Tool", false, false},
    {nullptr, nullptr, false, false}
};

QToolButton* ComboViewerPanel::addToolbarButton(const QString& iconName,
    const QString& tooltip, bool collapsible) {
    auto* btn = new QToolButton(m_toolbar);
    btn->setCheckable(true);
    btn->setIcon(makeToolIcon(toolColor(iconName), toolShape(iconName)));
    btn->setToolTip(tooltip);
    btn->setFixedSize(32, 32);
    btn->setIconSize(QSize(22, 22));
    btn->setAutoRaise(true);
    btn->setStyleSheet(
        "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 2px; } "
        "QToolButton:hover { background: #4a4a4a; border-color: #666; } "
        "QToolButton:checked { background: #007acc; border-color: #0098ff; }");
    connect(btn, &QToolButton::toggled, this, &ComboViewerPanel::onToolButtonToggled);
    m_toolbar->addWidget(btn);
    if (collapsible) btn->setVisible(m_isExpanded);
    return btn;
}

void ComboViewerPanel::buildToolbar() {
    m_toolbar->setObjectName("ComboViewerToolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(24, 24));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setOrientation(Qt::Horizontal);

    for (int i = 0; s_toolLayout[i].name != nullptr; ++i) {
        const auto& e = s_toolLayout[i];
        if (e.separator) m_toolbar->addSeparator();
        else addToolbarButton(e.name, tr(e.tip), e.collapsible);
    }

    // Expand/collapse button
    m_toolbar->addSeparator();
    m_expandButton = new QToolButton(m_toolbar);
    m_expandButton->setText(m_isExpanded ? "<<" : ">>");
    m_expandButton->setToolTip(tr("Show/Hide Advanced Tools"));
    m_expandButton->setFixedSize(32, 32);
    m_expandButton->setAutoRaise(true);
    m_expandButton->setStyleSheet(
        "QToolButton { color: #aaa; background: transparent; border: 1px solid transparent; "
        "border-radius: 2px; font-weight: bold; } "
        "QToolButton:hover { color: white; background: #4a4a4a; }");
    connect(m_expandButton, &QToolButton::clicked, this, [this]() { setIsExpanded(!m_isExpanded); });
    m_toolbar->addWidget(m_expandButton);
}

void ComboViewerPanel::buildToolOptions() {
    m_toolOptions = new QWidget(this);
    m_toolOptions->setObjectName("ComboViewerToolOptions");
    m_toolOptions->setFixedHeight(28);
    auto* layout = new QHBoxLayout(m_toolOptions);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);
    auto* toolLabel = new QLabel(tr("Tool:"), m_toolOptions);
    toolLabel->setStyleSheet("color: #ccc;");
    layout->addWidget(toolLabel);
    auto* toolName = new QLabel("Brush", m_toolOptions);
    toolName->setStyleSheet("color: #fff; font-weight: bold;");
    layout->addWidget(toolName);
    layout->addSpacing(12);
    auto* szLabel = new QLabel(tr("Size:"), m_toolOptions);
    szLabel->setStyleSheet("color: #ccc;");
    layout->addWidget(szLabel);
    auto* sizeSpin = new QSpinBox(m_toolOptions);
    sizeSpin->setRange(1, 500);
    sizeSpin->setValue(10);
    layout->addWidget(sizeSpin);
    layout->addStretch();
}

void ComboViewerPanel::setIsExpanded(bool expand) {
    m_isExpanded = expand;
    m_expandButton->setText(expand ? "<<" : ">>");
    for (auto* w : m_toolbar->findChildren<QToolButton*>()) {
        for (int i = 0; s_toolLayout[i].name != nullptr; ++i) {
            if (s_toolLayout[i].collapsible && s_toolLayout[i].name == w->toolTip())
                w->setVisible(expand);
        }
    }
}

void ComboViewerPanel::onToolButtonToggled(bool checked) {
    if (!checked) return;
    auto* senderBtn = qobject_cast<QToolButton*>(sender());
    if (!senderBtn) return;
    for (auto* btn : m_toolbar->findChildren<QToolButton*>()) {
        if (btn != senderBtn && btn->isCheckable())
            btn->setChecked(false);
    }
}

//=============================================================================

ComboViewerPanel::ComboViewerPanel(QWidget* parent, Qt::WindowFlags flags)
    : TPanel(parent, flags) {
    setObjectName("ComboViewerPanel");
    setWindowTitle(tr("Combo Viewer"));
    setIsMaximizable(true);
    setMinimumSize(400, 300);

    m_toolbar = new QToolBar(tr("Tools"), this);
    buildToolbar();
    buildToolOptions();

    m_viewer = new PlaceholderViewer(this);
    m_vRuler = new Ruler(this, true);
    m_hRuler = new Ruler(this, false);
    m_viewer->setRulers(m_vRuler, m_hRuler);
    m_flipConsole = new ViewerFlipConsole(this);

    // Build visibility submenu for the flip console [V] button
    auto* visMenu = new QMenu(this);
    auto addVis = [&](const QString& text, uint flag) {
        auto* a = visMenu->addAction(text);
        a->setCheckable(true);
        a->setChecked(m_visiblePartsFlag & flag);
        connect(a, &QAction::toggled, this, [this, flag](bool on) {
            if (on) m_visiblePartsFlag |= flag;
            else    m_visiblePartsFlag &= ~flag;
            updateShowHide();
        });
    };
    addVis(tr("Toolbar"), VPPARTS_TOOLBAR);
    addVis(tr("Tool Options"), VPPARTS_TOOLOPTIONS);
    addVis(tr("Playback Bar"), VPPARTS_PLAYBAR);
    addVis(tr("Frame Slider"), VPPARTS_FRAMESLIDER);
    m_flipConsole->setVisibilityMenu(visMenu);

    auto* outerLayout = new QVBoxLayout();
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_toolbar);
    outerLayout->addWidget(m_toolOptions);

    auto* viewerLayout = new QGridLayout();
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);
    viewerLayout->addWidget(m_vRuler, 1, 0);
    viewerLayout->addWidget(m_hRuler, 0, 1);
    viewerLayout->addWidget(m_viewer, 1, 1);
    viewerLayout->setRowStretch(1, 1);
    viewerLayout->setColumnStretch(1, 1);
    outerLayout->addLayout(viewerLayout, 1);
    outerLayout->addWidget(m_flipConsole);

    auto* wrapper = new QWidget(this);
    wrapper->setLayout(outerLayout);
    setWidget(wrapper);

    m_visiblePartsFlag = VPPARTS_COMBO_ALL;
    updateShowHide();
}

void ComboViewerPanel::updateShowHide() {
    m_toolbar->setVisible(m_visiblePartsFlag & VPPARTS_TOOLBAR);
    m_toolOptions->setVisible(m_visiblePartsFlag & VPPARTS_TOOLOPTIONS);
    m_flipConsole->setVisible(m_visiblePartsFlag & VPPARTS_PLAYBAR);
}

void ComboViewerPanel::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    addShowHideContextMenu(&menu);
    menu.exec(event->globalPos());
}

void ComboViewerPanel::addShowHideContextMenu(QMenu* menu) {
    QMenu* showHideMenu = menu->addMenu(tr("GUI Show / Hide"));
    auto addAction = [&](const QString& text, uint flag) {
        QAction* a = showHideMenu->addAction(text);
        a->setCheckable(true);
        a->setChecked(m_visiblePartsFlag & flag);
        a->setData(flag);
        return a;
    };
    auto* group = new QActionGroup(this);
    group->setExclusive(false);
    group->addAction(addAction(tr("Toolbar"), VPPARTS_TOOLBAR));
    group->addAction(addAction(tr("Tool Options Bar"), VPPARTS_TOOLOPTIONS));
    group->addAction(addAction(tr("Playback Bar"), VPPARTS_PLAYBAR));
    group->addAction(addAction(tr("Frame Slider"), VPPARTS_FRAMESLIDER));
    connect(group, &QActionGroup::triggered, this, &ComboViewerPanel::onShowHideActionTriggered);
}

void ComboViewerPanel::onShowHideActionTriggered(QAction* action) {
    if (!action) return;
    uint flag = action->data().toUInt();
    if (action->isChecked()) m_visiblePartsFlag |= flag;
    else m_visiblePartsFlag &= ~flag;
    updateShowHide();
}
