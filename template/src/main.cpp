#include <QApplication>
#include <QStyleFactory>
#include <QPalette>

#include "framework/appcontext.h"
#include "framework/mainwindow.h"
#include "framework/menubarcommand.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("YZ UI Template");
    app.setOrganizationName("YZ");
    app.setStyle(QStyleFactory::create("Fusion"));

    // Set dark palette
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(45, 45, 48));
    dark.setColor(QPalette::WindowText, QColor(208, 208, 208));
    dark.setColor(QPalette::Base, QColor(30, 30, 30));
    dark.setColor(QPalette::AlternateBase, QColor(45, 45, 48));
    dark.setColor(QPalette::ToolTipBase, QColor(208, 208, 208));
    dark.setColor(QPalette::ToolTipText, QColor(208, 208, 208));
    dark.setColor(QPalette::Text, QColor(208, 208, 208));
    dark.setColor(QPalette::Button, QColor(45, 45, 48));
    dark.setColor(QPalette::ButtonText, QColor(208, 208, 208));
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Highlight, QColor(0, 122, 204));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::Link, QColor(0, 122, 204));
    qApp->setPalette(dark);

    // Load shortcuts (must happen before MainWindow constructs to have actions available)
    CommandManager::instance()->loadShortcuts();

    // Create and show main window (registerDemoPanels() is called in MainWindow constructor)
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
