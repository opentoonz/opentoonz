#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>

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

    // Load saved theme via AppContext (which reads QSettings)
    QString themeName = AppContext::instance()->currentTheme();
    QString stylesheet = AppContext::instance()->loadStylesheet(themeName);
    if (!stylesheet.isEmpty()) {
        app.setStyleSheet(stylesheet);
        ThemeManager::getInstance().parseCustomPropertiesFromStylesheet(stylesheet);
    }

    // Load language translator
    QString language = AppContext::instance()->currentLanguage();
    QTranslator* appTranslator = nullptr;
    if (language != "English") {
        // Map display name to directory name
        QString langDir;
        if (language == "中文") langDir = "chinese";
        else if (language == "日本語") langDir = "japanese";

        if (!langDir.isEmpty()) {
            QString qmPath = QString(":/translations/%1/template.qm").arg(langDir);
            QFile qmFile(qmPath);
            if (qmFile.open(QIODevice::ReadOnly)) {
                QByteArray data = qmFile.readAll();
                appTranslator = new QTranslator();
                appTranslator->load(reinterpret_cast<const uchar*>(data.constData()),
                                    data.size());
                app.installTranslator(appTranslator);
            }
        }
    }

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
