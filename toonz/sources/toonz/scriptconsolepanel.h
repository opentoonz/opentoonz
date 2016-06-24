#pragma once

#ifndef SCRIPTCONSOLEPANEL_H
#define SCRIPTCONSOLEPANEL_H

#include "pane.h"

class ScriptConsole;

class ScriptConsolePanel final : public TPanel {
  Q_OBJECT
  ScriptConsole *m_scriptConsole;

public:
#if QT_VERSION >= 0x050500
  ScriptConsolePanel(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
  ScriptConsolePanel(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
  ~ScriptConsolePanel();

  void executeCommand(const QString &cmd);

public slots:
  void selectNone();
};

#endif  // TESTPANEL_H
