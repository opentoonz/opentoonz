#include <QApplication>
#include <QStyleFactory>
#include <QStyle>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <QFile>
#include <QDebug>

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

    // Load language translator - try filesystem first, then resource
    QString language = AppContext::instance()->currentLanguage();
    QTranslator* appTranslator = nullptr;
    if (language != "English") {
        QString langDir;
        if (language == "中文") langDir = "chinese";
        else if (language == "日本語") langDir = "japanese";

        if (!langDir.isEmpty()) {
            appTranslator = new QTranslator();
            // Try filesystem first (runtime directory)
            QString fsPath = QApplication::applicationDirPath()
                           + "/translations/" + langDir + "/template.qm";
            bool ok = appTranslator->load(fsPath);
            // Fallback to resource system
            if (!ok) {
                QString qmPath = QString(":/translations/%1/template.qm").arg(langDir);
                QFile qmFile(qmPath);
                if (qmFile.open(QIODevice::ReadOnly)) {
                    QByteArray data = qmFile.readAll();
                    ok = appTranslator->load(
                        reinterpret_cast<const uchar*>(data.constData()), data.size());
                }
            }
            if (ok) app.installTranslator(appTranslator);
            else delete appTranslator;
        }
    }

    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
