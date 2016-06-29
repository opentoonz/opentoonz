#pragma once

#ifndef SCRIPTCONSOLE_H
#define SCRIPTCONSOLE_H

#include "tcommon.h"
#include <QTextEdit>

class ScriptEngine;

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class DVAPI ScriptConsole final : public QTextEdit {
  Q_OBJECT
  ScriptEngine *m_engine;

  QList<QString> m_commands;
  int m_commandIndex;
  QString m_currentCommand;

  QString m_prompt;

public:
  ScriptConsole(QWidget *parent = 0);
  ~ScriptConsole();

  void executeCommand(const QString &cmd);
  ScriptEngine *getEngine() const { return m_engine; }

protected:
  void keyPressEvent(QKeyEvent *e) override;
  void onReturnKeyPress();

  bool canInsertFromMimeData(const QMimeData *source) const override;
  void insertFromMimeData(const QMimeData *source) override;

public slots:
  void onEvaluationDone();
  void output(int, const QString &msg);
  void onCursorPositionChanged();
};

#endif  // SCRIPTCONSOLE_H
