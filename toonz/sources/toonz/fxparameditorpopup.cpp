

#include "fxparameditorpopup.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "tapp.h"
#include "toonz/tfxhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"

#include "tfxattributes.h"

#include "toonzqt/fxsettings.h"

#include "toonz/tcolumnfx.h"
#include "toonz/scenefx.h"
#include "toonz/preferences.h"

#include <QMainWindow>

using namespace DVGui;

//=============================================================================
/*! \class FxParamEditorPopup

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

FxParamEditorPopup::FxParamEditorPopup()
    : QDialog(TApp::instance()->getMainWindow()) {
  setWindowTitle(tr("Fx Settings"));
  setMinimumSize(20, 20);

  TApp *app            = TApp::instance();
  TSceneHandle *hScene = app->getCurrentScene();
  TPixel32 col1, col2;
  Preferences::instance()->getChessboardColors(col1, col2);

  FxSettings *fxSettings = new FxSettings(this, col1, col2);
  fxSettings->setSceneHandle(hScene);
  fxSettings->setFxHandle(app->getCurrentFx());
  fxSettings->setFrameHandle(app->getCurrentFrame());
  fxSettings->setXsheetHandle(app->getCurrentXsheet());
  fxSettings->setLevelHandle(app->getCurrentLevel());
  fxSettings->setObjectHandle(app->getCurrentObject());

  fxSettings->setCurrentFx();

  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setMargin(0);
  mainLayout->setSpacing(10);
  { mainLayout->addWidget(fxSettings); }
  setLayout(mainLayout);

  move(parentWidget()->geometry().center() - rect().bottomRight() / 2.0);
}

//=============================================================================
/*
OpenPopupCommandHandler<FxParamEditorPopup> openFxParamEditorPopup(
    MI_FxParamEditor);
*/