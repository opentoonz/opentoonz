

#include "scriptengine.h"

#include <QScriptEngine>
#include <QScriptEngineDebugger>
#include <QTextStream>
#include <QFile>

#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "filebrowserpopup.h"
#include "tapp.h"

#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzfolders.h"

namespace ScriptWrapper {

Level::Level() {}
Level::Level(TXshSimpleLevel *level)
    : m_name(QString::fromStdWString(level->getName())) {}
Level::~Level() {}

TXshSimpleLevel *Level::getLevel() const {
  ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();
  TXshLevel *level    = scene->getLevelSet()->getLevel(m_name.toStdWString());
  TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);
  return sl;
}

}  // ScriptWrapper

QScriptValue getLevel(QScriptContext *ctx, QScriptEngine *eng) {
  QString levelName = ctx->argument(0).toString();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXshLevel *level  = scene->getLevelSet()->getLevel(levelName.toStdWString());
  if (!level) {
    level     = scene->createNewLevel(PLI_XSHLEVEL, levelName.toStdWString());
    levelName = QString::fromStdWString(level->getName());
  }
  if (level->getSimpleLevel())
    return eng->newQObject(new ScriptWrapper::Level(level->getSimpleLevel()),
                           QScriptEngine::ScriptOwnership);
  else
    return QScriptValue();
}

QScriptValue foo(QScriptContext *ctx, QScriptEngine *eng) {
  int r                 = ctx->argument(0).toInteger();
  int c                 = ctx->argument(1).toInteger();
  QScriptValue levelArg = ctx->argument(2);

  ScriptWrapper::Level *level =
      dynamic_cast<ScriptWrapper::Level *>(levelArg.toQObject());
  if (level) {
    TXshSimpleLevel *sl = level->getLevel();
    if (!sl) return QScriptValue(0);

    TFrameId fid(1);
    if (!sl->isFid(fid)) sl->setFrame(fid, sl->createEmptyFrame());

    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    scene->getXsheet()->setCell(r, c, TXshCell(sl, fid));
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  return QScriptValue(1);
}

class LoadScriptPopup : public FileBrowserPopup {
public:
  LoadScriptPopup() : FileBrowserPopup(tr("Load script")) {
    QStringList sl;
    sl << "qs";
    setFilterTypes(sl);
  }
  ~LoadScriptPopup() {}
  void initFolder() {
    setFolder(ToonzFolder::getLibraryFolder());
    // setFolder("");
  }
  bool execute() {
    TFilePath fp;
    QString fileName = "helloworld.qs";
    QFile scriptFile(QString::fromStdWString(fp.getWideString()));
    if (!scriptFile.open(QIODevice::ReadOnly)) {
      DVGui::MsgBox(DVGui::WARNING, QObject::tr("File not found"));
      return false;
    } else {
      QTextStream stream(&scriptFile);
      QString contents = stream.readAll();
      scriptFile.close();
      QScriptEngine myEngine;
      QScriptEngineDebugger debugger;
      debugger.attachTo(&myEngine);

      QScriptValue fFoo      = myEngine.newFunction(foo);
      QScriptValue fGetLevel = myEngine.newFunction(getLevel);

      myEngine.globalObject().setProperty("foo", fFoo);
      myEngine.globalObject().setProperty("getLevel", fGetLevel);
      debugger.action(QScriptEngineDebugger::InterruptAction)->trigger();
      myEngine.evaluate(contents, fileName);
    }
    return true;
  }
  void go() {
    exec();
    /*
show();
raise();
activateWindow();
*/
  }
};

OpenPopupCommandHandler<LoadScriptPopup> loadScriptPopupHandler("MI_RunScript");
