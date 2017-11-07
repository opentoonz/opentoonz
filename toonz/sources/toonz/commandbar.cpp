

#include "commandbar.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "tsystem.h"
#include "commandbarpopup.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/toonzfolders.h"
// Qt includes
#include <QWidgetAction>
#include <QXmlStreamReader>
#include <QtDebug>
#include <QMenuBar>
#include <QContextMenuEvent>

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                       bool isCollapsible, bool isXsheetToolbar)
#else
CommandBar::CommandBar(QWidget *parent, Qt::WFlags flags)
#endif
    : QToolBar(parent), m_isCollapsible(isCollapsible) {
  setObjectName("cornerWidget");
  setObjectName("CommandBar");
  fillToolbar(this, isXsheetToolbar);
}

//-----------------------------------------------------------------------------

void CommandBar::fillToolbar(CommandBar *toolbar, bool isXsheetToolbar) {
  toolbar->clear();
  TFilePath personalPath;
  if (isXsheetToolbar) {
    personalPath =
        ToonzFolder::getMyModuleDir() + TFilePath("xsheettoolbar.xml");
  } else {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
  }
  if (!TSystem::doesExistFileOrLevel(personalPath)) {
    if (isXsheetToolbar) {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("xsheettoolbar.xml");
    } else {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("commandbar.xml");
    }
  }
  QFile file(toQString(personalPath));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    buildDefaultToolbar(toolbar);
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "commandbar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "command") {
          QString cmdName    = reader.readElementText();
          std::string cmdStr = cmdName.toStdString();
          QAction *action =
              CommandManager::instance()->getAction(cmdStr.c_str());
          if (action) toolbar->addAction(action);
        } else if (reader.name() == "separator") {
          toolbar->addSeparator();
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  } else {
    reader.raiseError(QObject::tr("Cannot Read XML File"));
  }

  if (reader.hasError()) {
    buildDefaultToolbar(toolbar);
    return;
  }
}

//-----------------------------------------------------------------------------

void CommandBar::buildDefaultToolbar(CommandBar *toolbar) {
  toolbar->clear();
  TApp *app = TApp::instance();
  {
    QAction *newVectorLevel =
        CommandManager::instance()->getAction("MI_NewVectorLevel");
    toolbar->addAction(newVectorLevel);
    QAction *newToonzRasterLevel =
        CommandManager::instance()->getAction("MI_NewToonzRasterLevel");
    toolbar->addAction(newToonzRasterLevel);
    QAction *newRasterLevel =
        CommandManager::instance()->getAction("MI_NewRasterLevel");
    toolbar->addAction(newRasterLevel);
    toolbar->addSeparator();
    QAction *reframeOnes = CommandManager::instance()->getAction("MI_Reframe1");
    toolbar->addAction(reframeOnes);
    QAction *reframeTwos = CommandManager::instance()->getAction("MI_Reframe2");
    toolbar->addAction(reframeTwos);
    QAction *reframeThrees =
        CommandManager::instance()->getAction("MI_Reframe3");
    toolbar->addAction(reframeThrees);

    toolbar->addSeparator();

    QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
    toolbar->addAction(repeat);

    toolbar->addSeparator();

    QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
    toolbar->addAction(collapse);
    QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
    toolbar->addAction(open);
    QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
    toolbar->addAction(leave);
    QAction *editInPlace =
        CommandManager::instance()->getAction("MI_ToggleEditInPlace");
    toolbar->addAction(editInPlace);
  }
}

//-----------------------------------------------------------------------------

void CommandBar::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu                  = new QMenu(this);
  QAction *customizeCommandBar = menu->addAction(tr("Customize Command Bar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void CommandBar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup();

  if (cbPopup->exec()) {
    fillToolbar(this);
  }
  delete cbPopup;
}