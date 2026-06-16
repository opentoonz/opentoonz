#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QTranslator>
#include <QLibraryInfo>

#include "framework/appcontext.h"
#include "framework/mainwindow.h"
#include "framework/menubarcommand.h"
#include "framework/thememanager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("YZ UI Template");
    app.setOrganizationName("YZ");
    app.setStyle(QStyleFactory::create("Fusion"));

    // Initialize ThemeManager and preload icon metadata
    ThemeManager::getInstance().initialize();

    // Load shortcuts
    CommandManager::instance()->loadShortcuts();

    // AppContext constructor loads saved theme + language
    QString themeName = AppContext::instance()->currentTheme();
    AppContext::instance()->applyTheme(themeName);

    // Qt system translator for standard dialogs
    QTranslator qtTranslator;
    if (qtTranslator.load("qt_" + QLocale::system().name(),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
