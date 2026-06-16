#include "register.h"
#include "../framework/pane.h"

#include "logpanel.h"
#include "propertypanel.h"
#include "canvaspanel.h"
#include "commandpalette.h"
#include "welcomepanel.h"

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

void registerDemoPanels() {
    // Static factories auto-register via their constructors.
    // This function exists as an explicit init hook.
}
