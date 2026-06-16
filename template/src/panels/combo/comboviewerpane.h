#pragma once

#include "../../framework/pane.h"
#include "ruler.h"
#include <QFrame>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QMenu>
#include <QToolButton>
#include <QPushButton>
#include <QHBoxLayout>
#include <QSpinBox>

class QActionGroup;

//=============================================================================
// PlaceholderViewer — replaces SceneViewer (OpenGL)
//=============================================================================

class PlaceholderViewer : public QWidget {
    Q_OBJECT
public:
    explicit PlaceholderViewer(QWidget* parent = nullptr);
    void setRulers(Ruler* vRuler, Ruler* hRuler);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    Ruler* m_vRuler = nullptr;
    Ruler* m_hRuler = nullptr;
    QPoint m_lastPos;
    bool m_panning = false;
    double m_zoom = 1.0;
    double m_panX = 0.0, m_panY = 0.0;
    void updateRulers();
};

//=============================================================================
// ViewerFlipConsole — playback bar matching OpenToonz FlipConsole layout
//=============================================================================

class ViewerFlipConsole : public QWidget {
    Q_OBJECT
public:
    explicit ViewerFlipConsole(QWidget* parent = nullptr);
    void setVisibilityMenu(QMenu* menu);

signals:
    void frameChanged(int frame);
    void played();
    void paused();
    void stopped();

private:
    QToolButton* m_menuToggleBtn;
    QToolButton* m_firstBtn;
    QToolButton* m_prevBtn;
    QToolButton* m_playBtn;
    QToolButton* m_pauseBtn;
    QToolButton* m_nextBtn;
    QToolButton* m_lastBtn;
    QToolButton* m_loopBtn;
    QSlider* m_frameSlider;
    QLabel* m_frameLabel;
    QSpinBox* m_fpsSpin;
    bool m_playing = false;
    int m_frame = 0;
    int m_frameCount = 120;

private slots:
    void onPlayPause();
    void onStop();
    void onSliderChanged(int v);
};

//=============================================================================
// ComboViewerPanel — TPanel with toonz-matching toolbar + viewer + rulers
//=============================================================================

class ComboViewerPanel : public TPanel {
    Q_OBJECT
public:
    ComboViewerPanel(QWidget* parent = nullptr,
                     Qt::WindowFlags flags = Qt::WindowFlags());
    ~ComboViewerPanel() {}

    void updateShowHide();
    void addShowHideContextMenu(QMenu* menu);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    // Top toolbar (matches OpenToonz buttonLayout)
    QToolBar* m_toolbar;
    QToolButton* m_expandButton;
    bool m_isExpanded = true;

    // Tool options bar
    QWidget* m_toolOptions;

    // Rulers + viewer
    Ruler* m_vRuler;
    Ruler* m_hRuler;
    PlaceholderViewer* m_viewer;

    // Flip console
    ViewerFlipConsole* m_flipConsole;

    int m_visiblePartsFlag;
    enum VP_Parts {
        VPPARTS_None = 0,
        VPPARTS_PLAYBAR = 0x1,
        VPPARTS_FRAMESLIDER = 0x2,
        VPPARTS_TOOLBAR = 0x4,
        VPPARTS_TOOLOPTIONS = 0x8,
        VPPARTS_ALL = VPPARTS_PLAYBAR | VPPARTS_FRAMESLIDER,
        VPPARTS_COMBO_ALL = VPPARTS_ALL | VPPARTS_TOOLBAR | VPPARTS_TOOLOPTIONS
    };

    void buildToolbar();
    void buildToolOptions();
    QToolButton* addToolbarButton(const QString& iconName, const QString& tooltip,
                                   bool collapsible = false);
    QAction* addToolbarSeparator(bool collapsible = false);
    void setIsExpanded(bool expand);

private slots:
    void onShowHideActionTriggered(QAction* action);
    void onToolButtonToggled(bool checked);
};
