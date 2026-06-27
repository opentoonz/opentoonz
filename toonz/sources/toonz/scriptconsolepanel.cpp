

#include "scriptconsolepanel.h"
#include "toonzqt/scriptconsole.h"
#include "toonz/scriptengine.h"

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "toonz/scriptbinding.h"
#include "toonz/scriptbinding_level.h"
#endif

#include "iocommand.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/selection.h"
#include "toonzqt/tselectionhandle.h"

#include "flipbook.h"
#include "tvectorimage.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QScriptEngine>
#endif
#include <QFile>
#include <QTextStream>

#include "timage.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

static QScriptValue loadSceneFun(QScriptContext *context,
                                 QScriptEngine *engine) {
  if (context->argumentCount() > 0) {
    QString fpArg = context->argument(0).toString();
    TFilePath fp(fpArg.toStdWString());
    IoCmd::loadScene(fp);
  }
  return QScriptValue();
}

static QScriptValue saveSceneFun(QScriptContext *context,
                                 QScriptEngine *engine) {
  if (context->argumentCount() > 0) {
    QString fpArg = context->argument(0).toString();
    TFilePath fp(fpArg.toStdWString());
    IoCmd::saveScene(fp, IoCmd::SILENTLY_OVERWRITE);
  }
  return QScriptValue();
}

static QScriptValue loadLevelFun(QScriptContext *context,
                                 QScriptEngine *engine) {
  if (context->argumentCount() > 0) {
    QString fpArg = context->argument(0).toString();
    TFilePath fp(fpArg.toStdWString());
    int row = 0, col = 0;
    if (context->argumentCount() == 3) {
      row = context->argument(1).toInteger();
      col = context->argument(1).toInteger();
    }

    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh      = scene->getXsheet();

    TFilePath actualPath = scene->decodeFilePath(fp);
    TXshLevel *xl        = scene->loadLevel(actualPath);
    if (xl) {
      scene->getXsheet()->exposeLevel(row, col, xl);
    }
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentScene()->notifySceneChanged();
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
  return QScriptValue();
}

static QScriptValue dummyFun(QScriptContext *context, QScriptEngine *engine) {
  return QScriptValue(engine, 0);
}

static QScriptValue viewFun(QScriptContext *context, QScriptEngine *engine) {
  TScriptBinding::Image *image = 0;
  TScriptBinding::Level *level = 0;

  if (context->argumentCount() == 1) {
    image = qscriptvalue_cast<TScriptBinding::Image *>(context->argument(0));
    level = qscriptvalue_cast<TScriptBinding::Level *>(context->argument(0));
  }
  if (image) {
    if (!image->getImg())
      return context->throwError("Can't view an empty image");
  } else if (level) {
    if (!level->getSimpleLevel())
      return context->throwError("Can't view an empty level");
  } else {
    return context->throwError("expected one argument : an image or a level");
  }

  FlipBook *flipBook;
  flipBook = FlipBookPool::instance()->pop();
  if (image) {
    ImageViewer *imageViewer = flipBook->getImageViewer();
    imageViewer->setImage(image->getImg());
  } else {
    flipBook->setLevel(level->getSimpleLevel());
  }
  return engine->globalObject().property("void");
}

static QScriptValue evaluateOnMainThread(QScriptContext *context,
                                         QScriptEngine *engine) {
  QScriptValue fun = context->callee().data();
  QObject *obj     = fun.data().toQObject();
  QString s        = fun.toString();
  ScriptEngine *se = qobject_cast<ScriptEngine *>(obj);
  return se->evaluateOnMainThread(fun, context->argumentsObject());
}

static void def(ScriptEngine *teng, const QString &name,
                QScriptEngine::FunctionSignature fun) {
  QScriptEngine *eng  = teng->getQScriptEngine();
  QScriptValue funVal = eng->newFunction(fun);
  funVal.setData(eng->newQObject(teng));

  QScriptValue evalFun = eng->newFunction(evaluateOnMainThread);
  evalFun.setData(funVal);
  eng->globalObject().setProperty(name, evalFun);
}

#endif

ScriptConsolePanel::ScriptConsolePanel(QWidget *parent, Qt::WindowFlags flags)
    : TPanel(parent) {
  setPanelType("ScriptConsole");
  setIsMaximizable(false);
  setWindowTitle(QObject::tr("Script Console"));

  m_scriptConsole = new ScriptConsole(this);

  ScriptEngine *teng = m_scriptConsole->getEngine();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  /*
def(teng, "saveScene", saveSceneFun);
def(teng, "loadScene", loadSceneFun);
def(teng, "loadLevel", loadLevelFun);
*/

  def(teng, "view", viewFun);
  def(teng, "dummy", dummyFun);

  // teng->getQScriptEngine()->evaluate("console={version:'1.0'};function
  // version() {print('Toonz '+toonz.version+'\nscript '+script.version);};");

  /*
QFile initFile(":/Resources/init.js");
if (initFile.open(QIODevice::ReadOnly))
{
QTextStream stream(&initFile);
QString contents = stream.readAll();
initFile.close();
teng->getQScriptEngine()->evaluate(contents, "init.js");
}
*/
#else
  QString helperError = teng->installConsoleBridge(this);
  if (!helperError.isEmpty())
    teng->emitOutput(ScriptEngine::Warning, helperError);

  teng->emitOutput(ScriptEngine::Warning,
                   tr("Qt 6 scripting is running on QJSEngine with partial "
                      "OpenToonz object bindings. The legacy view() helper is "
                      "available for Image and Level values."));
#endif

  setWidget(m_scriptConsole);
  setMinimumHeight(80);
  allowMultipleInstances(false);

  resize(800, 300);
  connect(m_scriptConsole, SIGNAL(selectionChanged()), this,
          SLOT(selectNone()));
}

//-----------------------------------------------------------------------------

ScriptConsolePanel::~ScriptConsolePanel() {}

//-----------------------------------------------------------------------------

#if OPENTOONZ_QT_MAJOR >= 6

QString ScriptConsolePanel::viewScriptImage(int imageId) {
  ScriptEngine *engine = m_scriptConsole ? m_scriptConsole->getEngine() : 0;
  TImage *image        = engine ? engine->scriptImageForView(imageId) : 0;
  if (!image) return tr("Can't view an empty image");

  FlipBook *flipBook = FlipBookPool::instance()->pop();
  if (!flipBook || !flipBook->getImageViewer())
    return tr("Could not open FlipBook");

  flipBook->getImageViewer()->setImage(TImageP(image));
  return QString();
}

QString ScriptConsolePanel::viewScriptLevel(int levelId) {
  ScriptEngine *engine   = m_scriptConsole ? m_scriptConsole->getEngine() : 0;
  TXshSimpleLevel *level = engine ? engine->scriptLevelForView(levelId) : 0;
  if (!level) return tr("Can't view an empty level");

  FlipBook *flipBook = FlipBookPool::instance()->pop();
  if (!flipBook) return tr("Could not open FlipBook");

  flipBook->setLevel(level);
  return QString();
}

#endif

//-----------------------------------------------------------------------------

void ScriptConsolePanel::executeCommand(const QString &cmd) {
  m_scriptConsole->executeCommand(cmd);
}

//-----------------------------------------------------------------------------

void ScriptConsolePanel::selectNone() {
  TApp::instance()->getCurrentSelection()->setSelection(0);
}
