

#include "commandbar.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"

// Qt includes
#include <QWidgetAction>

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                       bool isCollapsible)
#else
CommandBar::CommandBar(QWidget *parent, Qt::WFlags flags)
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
    m_editInPlace =
        CommandManager::instance()->getAction("MI_ToggleEditInPlace");
    m_editInPlace->setCheckable(true);
    addAction(m_editInPlace);

    addSeparator();
    addAction(keyFrameAction);
  }
  bool ret = true;
  ret = ret && connect(app->getCurrentXsheet(), SIGNAL(editInPlaceChanged()),
                       this, SLOT(updateEditInPlaceStatus()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void CommandBar::updateEditInPlaceStatus() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  int ancestorCount = scene->getChildStack()->getAncestorCount();
  if (ancestorCount == 0) {
    m_editInPlace->setChecked(false);
    return;
  }
  m_editInPlace->setChecked(scene->getChildStack()->getEditInPlace());
}
