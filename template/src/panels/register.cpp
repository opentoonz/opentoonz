#include "register.h"
#include "../framework/pane.h"
#include "../framework/floatingpanelcommand.h"

#include "logpanel.h"
#include "propertypanel.h"
#include "canvaspanel.h"
#include "commandpalette.h"
#include "welcomepanel.h"

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

void registerDemoPanels() {
    // Static factories and floating panel commands auto-register.
}
