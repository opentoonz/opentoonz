#pragma once

#ifndef SCRIPTCONSOLEPANEL_H
#define SCRIPTCONSOLEPANEL_H

#include "pane.h"

class ScriptConsole;

class ScriptConsolePanel final : public TPanel {
  Q_OBJECT
  ScriptConsole *m_scriptConsole;

public:
  ScriptConsolePanel(QWidget *parent       = 0,
                     Qt::WindowFlags flags = Qt::WindowFlags());
  ~ScriptConsolePanel();

  void executeCommand(const QString &cmd);

public slots:
  void selectNone();
};

#endif  // TESTPANEL_H
