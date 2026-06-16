#include "register.h"
#include "../framework/pane.h"
#include "../framework/floatingpanelcommand.h"

#include "logpanel.h"
#include "propertypanel.h"
#include "canvaspanel.h"
#include "commandpalette.h"
#include "welcomepanel.h"
#include "combo/comboviewerpane.h"
#include "design/layerspanel.h"
#include "design/propertiespanel.h"
#include "design/designtoolbar.h"

// ---- Panel Factories ----

class LogPanelFactory : public TPanelFactory {
public:
    LogPanelFactory() : TPanelFactory("LogPanel") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("Log");
        panel->setWidget(new LogPanel(panel));
    }
};
static LogPanelFactory logPanelFactoryInstance;

class PropertyInspectorFactory : public TPanelFactory {
public:
    PropertyInspectorFactory() : TPanelFactory("PropertyInspector") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("Property Inspector");
        panel->setWidget(new PropertyInspector(panel));
    }
};
static PropertyInspectorFactory propertyInspectorFactoryInstance;

class CanvasFactory : public TPanelFactory {
public:
    CanvasFactory() : TPanelFactory("Canvas") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("Canvas");
        panel->setWidget(new CanvasPanel(panel));
        panel->setIsMaximizable(true);
    }
};
static CanvasFactory canvasFactoryInstance;

class CommandPaletteFactory : public TPanelFactory {
public:
    CommandPaletteFactory() : TPanelFactory("CommandPalette") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("Command Palette");
        panel->setWidget(new CommandPalette(panel));
    }
};
static CommandPaletteFactory commandPaletteFactoryInstance;

class WelcomeFactory : public TPanelFactory {
public:
    WelcomeFactory() : TPanelFactory("Welcome") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle("Welcome");
        panel->setWidget(new WelcomePanel(panel));
        panel->setIsMaximizable(false);
    }
};
static WelcomeFactory welcomeFactoryInstance;

class ComboViewerFactory : public TPanelFactory {
public:
    ComboViewerFactory() : TPanelFactory("ComboViewer") {}
    TPanel* createPanel(QWidget* parent) override {
        auto* panel = new ComboViewerPanel(parent);
        panel->setObjectName("ComboViewer");
        panel->setWindowTitle(QObject::tr("Combo Viewer"));
        panel->setIsMaximizable(true);
        panel->setMinimumSize(300, 200);
        return panel;
    }
    void initialize(TPanel*) override {}
};
static ComboViewerFactory comboViewerFactoryInstance;

class LayersFactory : public TPanelFactory {
public:
    LayersFactory() : TPanelFactory("Layers") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle(QObject::tr("Layers"));
        panel->setWidget(new LayersPanel(panel));
    }
};
static LayersFactory layersFactoryInstance;

class PropertiesFactory : public TPanelFactory {
public:
    PropertiesFactory() : TPanelFactory("Properties") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle(QObject::tr("Properties"));
        panel->setWidget(new PropertiesPanel(panel));
    }
};
static PropertiesFactory propertiesFactoryInstance;

class DesignToolbarFactory : public TPanelFactory {
public:
    DesignToolbarFactory() : TPanelFactory("DesignToolbar") {}
    void initialize(TPanel* panel) override {
        panel->setWindowTitle(QObject::tr("Design Toolbar"));
        panel->setWidget(new DesignToolbar(panel));
        panel->setFixWidthMode(DockWidget::fixed);
    }
};
static DesignToolbarFactory dtFactoryInstance;

// ---- OpenFloatingPanel Commands (auto-register with CommandManager) ----
// Pattern: OpenFloatingPanel(cmdId, panelType, title)
//   where panelType matches TPanelFactory key

static OpenFloatingPanel openLogPanelCommand(
    "MI_OpenLogPanel", "LogPanel", QObject::tr("Log"));
static OpenFloatingPanel openPropertyPanelCommand(
    "MI_OpenPropertyPanel", "PropertyInspector", QObject::tr("Property Inspector"));
static OpenFloatingPanel openCanvasPanelCommand(
    "MI_OpenCanvasPanel", "Canvas", QObject::tr("Canvas"));
static OpenFloatingPanel openCommandPaletteCommand(
    "MI_OpenCommandPalette", "CommandPalette", QObject::tr("Command Palette"));
static OpenFloatingPanel openWelcomePanelCommand(
    "MI_OpenWelcomePanel", "Welcome", QObject::tr("Welcome"));
static OpenFloatingPanel openComboViewerCommand(
    "MI_OpenComboViewer", "ComboViewer", QObject::tr("Combo Viewer"));

void registerDemoPanels() {
    // Static factories and floating panel commands auto-register.
}
