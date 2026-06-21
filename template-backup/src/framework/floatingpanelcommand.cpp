#include "floatingpanelcommand.h"
#include "mainwindow.h"
#include "appcontext.h"
#include "pane.h"

#include <QApplication>
#include <QScreen>

// Center widget on screen and make it active
static void activateWidget(QWidget* w) {
    QScreen* screen = w->screen();
    if (!screen) screen = QApplication::primaryScreen();
    if (!screen) return;
    QRect screenRect = screen->availableGeometry();

    QPoint p((screenRect.width() - w->width()) / 2,
             (screenRect.height() - w->height()) / 2);
    w->setGeometry(QRect(p, w->size()));
    w->activateWindow();
}

//=============================================================================
// OpenFloatingPanel
//-----------------------------------------------------------------------------

OpenFloatingPanel::OpenFloatingPanel(CommandId id, const std::string& panelType,
                                     QString title)
    : MenuItemHandler(id), m_title(title), m_panelType(panelType) {}

//-----------------------------------------------------------------------------

void OpenFloatingPanel::execute() {
    getOrOpenFloatingPanel(m_panelType);
}

TPanel* OpenFloatingPanel::getOrOpenFloatingPanel(
    const std::string& panelType) {
    MainWindow* mw = AppContext::instance()->mainWindow();
    if (!mw) return nullptr;
    TMainWindow* currentRoom = mw->getCurrentRoom();
    if (!currentRoom) return nullptr;

    QList<TPanel*> list    = currentRoom->findChildren<TPanel*>();
    QPoint lastFloatingPos = QPoint(0, 0);

    for (int i = 0; i < list.size(); i++) {
        TPanel* panel = list.at(i);

        // we want floating panel (possibly hidden) with the correct name
        if (panel->getPanelType() == panelType &&
            panel->isFloating()) {
            // if there is already a floating panel and MultipleInstances are
            // not allowed we must use it
            if (!panel->areMultipleInstancesAllowed() && !panel->isHidden()) {
                if (panel->isFloating()) activateWidget(panel);
                return panel;
            }

            // If there is a hidden panel we can show and use it
            if (panel->isHidden()) {
                panel->reset();
                currentRoom->addDockWidget(panel);
                panel->show();
                panel->raise();
                return panel;
            } else
                lastFloatingPos = panel->pos();
        }
    }

    // No panel found. We must create a new pane
    TPanel* panel = TPanelFactory::createPanel(
        currentRoom, QString::fromStdString(panelType));
    if (!panel) return nullptr;
    panel->restoreFloatingPanelState();
    panel->setFloating(true);
    panel->show();
    panel->raise();
    if (!lastFloatingPos.isNull())
        panel->move(QPoint(lastFloatingPos.x() + 30, lastFloatingPos.y() + 30));

    return panel;
}
