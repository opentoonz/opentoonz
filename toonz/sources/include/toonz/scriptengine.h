#pragma once

#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

// #define SCRIPTENGINE_TEST

#include "tcommon.h"
#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QObject>
#include <QMetaType>
#include <QtGlobal>

#ifndef OPENTOONZ_QT_MAJOR
#if defined(QT_VERSION) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define OPENTOONZ_QT_MAJOR 6
#else
#define OPENTOONZ_QT_MAJOR 5
#endif
#endif

class Foo;
#if OPENTOONZ_QT_MAJOR < 6
class QScriptValue;
class QScriptEngine;
#else
class QJSEngine;
#endif

class DVAPI ScriptCommand {
public:
  ScriptCommand() {}
  virtual ~ScriptCommand() {}

  virtual void execute() = 0;
};

class DVAPI ScriptEngine final : public QObject {
  Q_OBJECT
#if OPENTOONZ_QT_MAJOR < 6
  QScriptEngine *m_engine;
#else
  QJSEngine *m_engine;
#endif
  class Executor;
  friend class Executor;
  Executor *m_executor;
  class MainThreadEvaluationData;
  MainThreadEvaluationData *m_mainThreadEvaluationData;
#if OPENTOONZ_QT_MAJOR < 6
  QScriptValue *m_voidValue;
#endif

public:
  ScriptEngine();
  ~ScriptEngine();

  void evaluate(const QString &cmd);
  void interrupt();
  bool isEvaluating() const;
  bool wait(unsigned long time = ULONG_MAX);

  enum OutputType {
    SimpleText,
    Warning,
    ExecutionError,
    SyntaxError,
    EvaluationResult,
    UndefinedEvaluationResult
  };
  void emitOutput(OutputType type, const QString &value) {
    emit output((int)type, value);
  }

#if OPENTOONZ_QT_MAJOR < 6
  const QScriptValue &evaluateOnMainThread(const QScriptValue &fun,
                                           const QScriptValue &arguments);

  QScriptEngine *getQScriptEngine() const { return m_engine; }
  const QScriptValue &voidValue() const { return *m_voidValue; }
#else
  QJSEngine *getQJSEngine() const { return m_engine; }

  Q_INVOKABLE void emitScriptOutput(int type, const QString &value);
  Q_INVOKABLE QString runScriptFile(const QString &path);
#endif

protected slots:
  void onTerminated();
  void onMainThreadEvaluationPosted();

signals:
  void evaluationDone();
  void output(int type, const QString &);

  void mainThreadEvaluationPosted();
};

Q_DECLARE_METATYPE(ScriptEngine *)

#endif
