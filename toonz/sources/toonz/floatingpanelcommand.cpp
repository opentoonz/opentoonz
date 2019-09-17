

#include "floatingpanelcommand.h"
#include "mainwindow.h"
#include "flipbook.h"
#include "tapp.h"

#include "pane.h"

#include "toonzqt/styleeditor.h"

#include <QMainWindow>
#include <QDesktopWidget>
#include <QApplication>
#include <../toonzqt/tdockwindows.h>

//=============================================================================

// mette il widget al centro dello schermo e lo fa diventare la finestra
// corrente
static void activateWidget(QWidget *w) {
  QDesktopWidget *desktop = qApp->desktop();
  QRect screenRect        = desktop->screenGeometry(w);

  QPoint p((screenRect.width() - w->width()) / 2,
           (screenRect.height() - w->height()) / 2);
  w->setGeometry(QRect(p, w->size()));
  w->activateWindow();
}

//=============================================================================
// OpenFloatingPanel
//-----------------------------------------------------------------------------

OpenFloatingPanel::OpenFloatingPanel(CommandId id, const std::string &panelType,
                                     QString title)
    : MenuItemHandler(id), m_title(title), m_panelType(panelType) {}

//-----------------------------------------------------------------------------

void OpenFloatingPanel::execute() {
  // Let flipbooks handle the problem on their own
  if (m_panelType == "FlipBook") {
    FlipBookPool::instance()->pop();
    return;
  }

  getOrOpenFloatingPanel(m_panelType);

  /*

// Search for pane that are floating and have the same panelName
TMainWindow *currentRoom = TApp::instance()->getCurrentRoom();
QList<TPanel *> list = currentRoom->findChildren<TPanel *>();
  QPoint lastFloatingPos = QPoint(0,0);
for(int i=0;i<list.size();i++)
{
TPanel *panel = list.at(i);

// we want floating panel (possibly hidden) with the correct name
if(panel->getPanelType() == m_panelType && panel->isFloating())
{

// if there is already a floating panel and MultipleInstances are
// not allowed we must use it
if(!panel->areMultipleInstancesAllowed() && !panel->isHidden())
{
  activateWidget(panel);
  return;
}

// If there is a hidden panel we can show and use it
if(panel->isHidden())
{
                          //Alcuni pannelli devono essere resettati (Es.: il
paletteViewerPanel)
  panel->reset();
  //Devo porre il pannello sotto il controllo del layout della stanza
  currentRoom->addDockWidget(panel);
  panel->show();
  panel->raise();
  return;
}
                  else
                          lastFloatingPos = panel->pos();
}
}

// No panel found. We must create a new pane

TPanel *panel = TPanelFactory::createPanel(
currentRoom,
QString::fromStdString(m_panelType));
if(!panel) return; // it should never happen
panel->setWindowTitle(QObject::tr(m_title.toStdString().c_str()));
panel->setFloating(true);
panel->show();
panel->raise();
  if(!lastFloatingPos.isNull())
          panel->move(QPoint(lastFloatingPos.x()+30,lastFloatingPos.y()+30));
*/
}

TPanel *OpenFloatingPanel::getOrOpenFloatingPanel(
    const std::string &panelType) {
  TMainWindow *currentRoom = TApp::instance()->getCurrentRoom();
  QList<TPanel *> list     = currentRoom->findChildren<TPanel *>();
  QPoint lastFloatingPos   = QPoint(0, 0);
  for (int i = 0; i < list.size(); i++) {
    TPanel *panel = list.at(i);

    // we want floating panel (possibly hidden) with the correct name
    if (panel->getPanelType() == panelType && panel->isFloating()) {
      // if there is already a floating panel and MultipleInstances are
      // not allowed we must use it
      if (!panel->areMultipleInstancesAllowed() && !panel->isHidden()) {
        activateWidget(panel);
        return panel;
      }

      // If there is a hidden panel we can show and use it
      if (panel->isHidden()) {
        // Alcuni pannelli devono essere resettati (Es.: il paletteViewerPanel)
        panel->reset();
        // Devo porre il pannello sotto il controllo del layout della stanza
        currentRoom->addDockWidget(panel);
        panel->show();
        panel->raise();
        return panel;
      } else
        lastFloatingPos = panel->pos();
    }
  }

  // No panel found. We must create a new pane
  TPanel *panel = TPanelFactory::createPanel(currentRoom,
                                             QString::fromStdString(panelType));
  if (!panel) return 0;  // it should never happen
  // panel->setWindowTitle(QObject::tr(m_title.toStdString().c_str()));
  panel->restoreFloatingPanelState();
  panel->setFloating(true);
  panel->show();
  panel->raise();
  if (!lastFloatingPos.isNull())
    panel->move(QPoint(lastFloatingPos.x() + 30, lastFloatingPos.y() + 30));

  return panel;
}
