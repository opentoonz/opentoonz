

#include "commandbar.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QWidgetAction>


//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                             bool isCollapsible)
#else
CommandBar::CommandBar(XsheetViewer *parent, Qt::WFlags flags)
#endif
    : QToolBar(parent), m_isCollapsible(isCollapsible) {
  setObjectName("cornerWidget");
  setObjectName("CommandBar");

  TApp *app        = TApp::instance();
  m_keyFrameButton = new ViewerKeyframeNavigator(this, app->getCurrentFrame());
  m_keyFrameButton->setObjectHandle(app->getCurrentObject());
  m_keyFrameButton->setXsheetHandle(app->getCurrentXsheet());

  QWidgetAction *keyFrameAction = new QWidgetAction(this);
  keyFrameAction->setDefaultWidget(m_keyFrameButton);

  {
    QAction *newVectorLevel =
        CommandManager::instance()->getAction("MI_NewVectorLevel");
    addAction(newVectorLevel);
    QAction *newToonzRasterLevel =
        CommandManager::instance()->getAction("MI_NewToonzRasterLevel");
    addAction(newToonzRasterLevel);
    QAction *newRasterLevel =
        CommandManager::instance()->getAction("MI_NewRasterLevel");
    addAction(newRasterLevel);
    addSeparator();
    QAction *reframeOnes = CommandManager::instance()->getAction("MI_Reframe1");
    addAction(reframeOnes);
    QAction *reframeTwos = CommandManager::instance()->getAction("MI_Reframe2");
    addAction(reframeTwos);
    QAction *reframeThrees =
        CommandManager::instance()->getAction("MI_Reframe3");
    addAction(reframeThrees);

    addSeparator();

    QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
    addAction(repeat);

    addSeparator();

    QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
    addAction(collapse);
    QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
    addAction(open);
    QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
    addAction(leave);

    addSeparator();
    addAction(keyFrameAction);
  }
}






