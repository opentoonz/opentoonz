
#include "toonz/scriptengine.h"

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

#include "toonz/scriptbinding.h"
#include "toonz/scriptbinding_files.h"
#include "trenderer.h"
#include "toonz/toonzfolders.h"
#include <QScriptEngine>
#include <QScriptProgram>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QtGlobal>
#include <QApplication>

//=========================================================

namespace {

void sleep(unsigned long msec) {
  QMutex mutex;
  mutex.lock();
  QWaitCondition waitCondition;
  waitCondition.wait(&mutex, msec);
  mutex.unlock();
}

QString print(QScriptValue arg, bool addQuotes) {
  if (arg.isArray()) {
    QString s   = "[";
    quint32 len = arg.property(QLatin1String("length")).toUInt32();
    for (quint32 i = 0; i < len; ++i) {
      QScriptValue item = arg.property(i);
      if (i > 0) s += ",";
      s += print(item, addQuotes);
    }
    s += "]";
    return s;
  } else if (arg.isBool()) {
    return arg.toBool() ? "true" : "false";
  } else if (arg.isString()) {
    if (addQuotes)
      return "\"" + arg.toString() + "\"";
    else
      return arg.toString();
  } else
    return arg.toString();
}

QScriptValue printFunction(QScriptContext *context, QScriptEngine *engine) {
  QString result;
  for (int i = 0; i < context->argumentCount(); ++i) {
    if (i > 0) result.append(" ");
    result.append(print(context->argument(i), false));
  }
  QScriptValue calleeData = context->callee().data();
  ScriptEngine *se = qobject_cast<ScriptEngine *>(calleeData.toQObject());
  se->emitOutput(ScriptEngine::SimpleText, result);
  sleep(50);
  return se->voidValue();
}

QScriptValue warningFunction(QScriptContext *context, QScriptEngine *engine) {
  QString result;
  for (int i = 0; i < context->argumentCount(); ++i) {
    if (i > 0) result.append(" ");
    result.append(print(context->argument(i), false));
  }
  QScriptValue calleeData = context->callee().data();
  ScriptEngine *se = qobject_cast<ScriptEngine *>(calleeData.toQObject());
  se->emitOutput(ScriptEngine::Warning, result);
  sleep(50);
  return se->voidValue();
}

QScriptValue runFunction(QScriptContext *context, QScriptEngine *engine) {
  if (context->argumentCount() != 1) {
    return context->throwError("expected one parameter");
  }
  TFilePath fp;
  QScriptValue err =
      TScriptBinding::checkFilePath(context, context->argument(0), fp);
  if (err.isError()) return err;
  if (!fp.isAbsolute()) {
    fp = ToonzFolder::getLibraryFolder() + "scripts" + fp;
  }

  QString fpStr = QString::fromStdWString(fp.getWideString());
  QFile file(fpStr);
  if (!file.open(QIODevice::ReadOnly)) {
    return context->throwError("can't read file " + fpStr);
  }
  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  QScriptProgram program(content, fpStr);

  QScriptContext *parent = context->parentContext();
  if (parent != 0) {
    context->setActivationObject(context->parentContext()->activationObject());
    context->setThisObject(context->parentContext()->thisObject());
  }

  QScriptValue ret = engine->evaluate(program);

  if (engine->hasUncaughtException()) {
    int line = engine->uncaughtExceptionLineNumber();
    return context->throwError(QString("%1, at line %2 of %3")
                                   .arg(ret.toString())
                                   .arg(line)
                                   .arg(fpStr));
  }

  return ret;
}

/*
  QScriptValue glub(QScriptContext *context, QScriptEngine *engine)
  {
    QScriptValue global = engine->globalObject();
    QScriptValue te = global.property("_engine");
    ScriptEngine *se = qscriptvalue_cast<ScriptEngine*>(te);
    se->postCommand(context->argument(0));
    return 0;
  }
  */

}  // namespace

//=========================================================

class ScriptEngine::Executor final : public QThread {
  ScriptEngine *m_engine;
  QString m_cmd;

public:
  Executor(ScriptEngine *engine, const QString &cmd)
      : m_engine(engine), m_cmd(cmd) {}

  void run() override {
    m_engine->m_engine->collectGarbage();
    QScriptValue result = m_engine->m_engine->evaluate(m_cmd);
    if (result.isError()) {
      m_engine->emitOutput(ScriptEngine::SyntaxError, result.toString());
    } else if (result.isUndefined()) {
      m_engine->emitOutput(ScriptEngine::UndefinedEvaluationResult,
                           "undefined");
    } else {
      if (qscriptvalue_cast<TScriptBinding::Void *>(result)) {
      } else {
        m_engine->emitOutput(ScriptEngine::EvaluationResult,
                             print(result, true));
      }
    }
  }
};

class ScriptEngine::MainThreadEvaluationData {
public:
  QMutex m_mutex;
  QWaitCondition m_cond;
  QScriptValue m_fun, m_args, m_result;
};

//=========================================================

inline void defineFunction(ScriptEngine *se, const QString &name,
                           QScriptEngine::FunctionSignature f) {
  QScriptEngine *engine = se->getQScriptEngine();
  QScriptValue fObj     = engine->newFunction(f);
  fObj.setData(engine->newQObject(se));
  engine->globalObject().setProperty(name, fObj);
}

//=========================================================

ScriptEngine::ScriptEngine() : m_executor(0), m_engine(new QScriptEngine()) {
  // I must call TRenderer::initialize(), because a script could cause a
  // rendering driven by a working thread
  TRenderer::initialize();

  m_mainThreadEvaluationData = new MainThreadEvaluationData();
  QScriptValue global        = m_engine->globalObject();
  QScriptValue ctor;

  QScriptEngine &engine = *m_engine;

  defineFunction(this, "print", printFunction);
  defineFunction(this, "warning", warningFunction);
  defineFunction(this, "run", runFunction);

  /*
QScriptValue print = engine.newFunction(printFunction);
print.setData(engine.newQObject(this));
engine.globalObject().setProperty("print", print);

QScriptValue print = engine.newFunction(printFunction);
print.setData(engine.newQObject(this));
engine.globalObject().setProperty("print", print);

QScriptValue run = engine.newFunction(runFunction);
run.setData(engine.newQObject(this));
engine.globalObject().setProperty("run", run);

*/
  // QScriptValue g = engine.newFunction(glub);
  // g.setData(engine.newQObject(this));
  // engine.globalObject().setProperty("glub", g);

  // engine.globalObject().setProperty("_engine", engine.newQObject(this));

  m_voidValue  = new QScriptValue();
  *m_voidValue = engine.newQObject(new TScriptBinding::Void(),
                                   QScriptEngine::AutoOwnership);

  engine.globalObject().setProperty("void", *m_voidValue);

  TScriptBinding::bindAll(engine);
  bool ret = connect(this, SIGNAL(mainThreadEvaluationPosted()), this,
                     SLOT(onMainThreadEvaluationPosted()));
  assert(ret);
}

ScriptEngine::~ScriptEngine() {
  delete m_mainThreadEvaluationData;
  delete m_voidValue;
}

const QScriptValue &ScriptEngine::evaluateOnMainThread(
    const QScriptValue &fun, const QScriptValue &arguments) {
  MainThreadEvaluationData *d = m_mainThreadEvaluationData;
  QMutexLocker locker(&d->m_mutex);
  d->m_fun    = fun;
  d->m_args   = arguments;
  d->m_result = QScriptValue();
  emit mainThreadEvaluationPosted();
  d->m_cond.wait(&d->m_mutex);
  return d->m_result;
}

void ScriptEngine::onMainThreadEvaluationPosted() {
  Q_ASSERT(qApp && qApp->thread() == QThread::currentThread());
  MainThreadEvaluationData *d = m_mainThreadEvaluationData;
  QMutexLocker locker(&d->m_mutex);
  d->m_result = d->m_fun.call(d->m_fun, d->m_args);
  d->m_cond.wakeOne();
}

void ScriptEngine::evaluate(const QString &cmd) {
  if (m_executor) return;
  m_executor = new Executor(this, cmd);
  connect(m_executor, SIGNAL(finished()), this, SLOT(onTerminated()));
  m_executor->start();
}

bool ScriptEngine::wait(unsigned long time) {
  if (!m_executor) return true;
  const bool finished = m_executor->wait(time);
  if (finished) onTerminated();
  return finished;
}

bool ScriptEngine::isEvaluating() const { return m_engine->isEvaluating(); }

void ScriptEngine::interrupt() { m_engine->abortEvaluation(); }

void ScriptEngine::onTerminated() {
  if (!m_executor) return;
  emit evaluationDone();
  delete m_executor;
  m_executor = 0;
}

#else

#include "toonz/toonzfolders.h"
#include "trenderer.h"

#include <QApplication>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include <QMutex>
#include <QTextStream>
#include <QThread>
#include <QWaitCondition>

//=========================================================

namespace {

void sleep(unsigned long msec) {
  QMutex mutex;
  mutex.lock();
  QWaitCondition waitCondition;
  waitCondition.wait(&mutex, msec);
  mutex.unlock();
}

QString print(const QJSValue &arg, bool addQuotes) {
  if (arg.isArray()) {
    QString s = "[";
    const int len = arg.property(QLatin1String("length")).toInt();
    for (int i = 0; i < len; ++i) {
      QJSValue item = arg.property(i);
      if (i > 0) s += ",";
      s += print(item, addQuotes);
    }
    s += "]";
    return s;
  } else if (arg.isBool()) {
    return arg.toBool() ? "true" : "false";
  } else if (arg.isString()) {
    if (addQuotes)
      return "\"" + arg.toString() + "\"";
    else
      return arg.toString();
  } else {
    return arg.toString();
  }
}

QString formatError(const QJSValue &value, const QString &fallbackFileName) {
  QString message = value.toString();
  const int line = value.property(QStringLiteral("lineNumber")).toInt();
  const QString fileName =
      value.property(QStringLiteral("fileName")).toString();
  if (line > 0) {
    const QString source = fileName.isEmpty() ? fallbackFileName : fileName;
    if (!source.isEmpty())
      message = QStringLiteral("%1, at line %2 of %3")
                    .arg(message)
                    .arg(line)
                    .arg(source);
    else
      message = QStringLiteral("%1, at line %2").arg(message).arg(line);
  }
  return message;
}

const char kBootstrapScript[] = R"JS(
(function(global) {
  function format(value, addQuotes) {
    if (Array.isArray(value)) {
      var items = [];
      for (var i = 0; i < value.length; ++i)
        items.push(format(value[i], addQuotes));
      return "[" + items.join(",") + "]";
    }
    if (typeof value === "boolean")
      return value ? "true" : "false";
    if (typeof value === "string")
      return addQuotes ? "\"" + value + "\"" : value;
    if (value === undefined)
      return "undefined";
    if (value === null)
      return "null";
    return String(value);
  }

  global.print = function() {
    var items = [];
    for (var i = 0; i < arguments.length; ++i)
      items.push(format(arguments[i], false));
    __opentoonzScriptEngine.emitScriptOutput(0, items.join(" "));
  };

  global.warning = function() {
    var items = [];
    for (var i = 0; i < arguments.length; ++i)
      items.push(format(arguments[i], false));
    __opentoonzScriptEngine.emitScriptOutput(1, items.join(" "));
  };

  global.run = function(path) {
    return __opentoonzScriptEngine.runScriptFile(String(path));
  };
})(typeof globalThis !== "undefined" ? globalThis : this);
)JS";

void installBootstrap(QJSEngine *engine, ScriptEngine *scriptEngine) {
  QJSEngine::setObjectOwnership(scriptEngine, QJSEngine::CppOwnership);
  engine->globalObject().setProperty(
      QStringLiteral("__opentoonzScriptEngine"),
      engine->newQObject(scriptEngine));
  engine->evaluate(QString::fromLatin1(kBootstrapScript),
                   QStringLiteral("opentoonz-script-bootstrap.js"));
}

}  // namespace

//=========================================================

class ScriptEngine::Executor final : public QThread {
  ScriptEngine *m_engine;
  QString m_cmd;

public:
  Executor(ScriptEngine *engine, const QString &cmd)
      : m_engine(engine), m_cmd(cmd) {}

  void run() override {
    m_engine->m_engine->setInterrupted(false);
    m_engine->m_engine->collectGarbage();
    QJSValue result = m_engine->m_engine->evaluate(m_cmd);
    if (result.isError()) {
      m_engine->emitOutput(ScriptEngine::SyntaxError, formatError(result, ""));
    } else if (result.isUndefined()) {
      m_engine->emitOutput(ScriptEngine::UndefinedEvaluationResult,
                           "undefined");
    } else {
      m_engine->emitOutput(ScriptEngine::EvaluationResult,
                           print(result, true));
    }
  }
};

class ScriptEngine::MainThreadEvaluationData {};

//=========================================================

ScriptEngine::ScriptEngine() : m_executor(0), m_engine(new QJSEngine()) {
  // I must call TRenderer::initialize(), because a script could cause a
  // rendering driven by a working thread
  TRenderer::initialize();

  m_mainThreadEvaluationData = new MainThreadEvaluationData();
  installBootstrap(m_engine, this);
}

ScriptEngine::~ScriptEngine() {
  delete m_mainThreadEvaluationData;
  delete m_engine;
}

void ScriptEngine::emitScriptOutput(int type, const QString &value) {
  const OutputType outputType = type == Warning ? Warning : SimpleText;
  emitOutput(outputType, value);
  sleep(50);
}

QString ScriptEngine::runScriptFile(const QString &path) {
  TFilePath fp(path.toStdWString());
  if (!fp.isAbsolute()) {
    fp = ToonzFolder::getLibraryFolder() + "scripts" + fp;
  }

  const QString fpStr = QString::fromStdWString(fp.getWideString());
  QFile file(fpStr);
  if (!file.open(QIODevice::ReadOnly)) {
    emitOutput(ExecutionError, "can't read file " + fpStr);
    return QString();
  }

  QTextStream in(&file);
  QString content = in.readAll();
  file.close();

  QJSValue result = m_engine->evaluate(content, fpStr);
  if (result.isError()) {
    emitOutput(SyntaxError, formatError(result, fpStr));
    return QString();
  }
  if (result.isUndefined()) return QString();
  return print(result, true);
}

void ScriptEngine::evaluate(const QString &cmd) {
  if (m_executor) return;
  m_executor = new Executor(this, cmd);
  connect(m_executor, SIGNAL(finished()), this, SLOT(onTerminated()));
  m_executor->start();
}

bool ScriptEngine::wait(unsigned long time) {
  if (!m_executor) return true;
  const bool finished = m_executor->wait(time);
  if (finished) onTerminated();
  return finished;
}

bool ScriptEngine::isEvaluating() const { return m_executor != 0; }

void ScriptEngine::interrupt() { m_engine->setInterrupted(true); }

void ScriptEngine::onMainThreadEvaluationPosted() {}

void ScriptEngine::onTerminated() {
  if (!m_executor) return;
  emit evaluationDone();
  delete m_executor;
  m_executor = 0;
}

#endif
