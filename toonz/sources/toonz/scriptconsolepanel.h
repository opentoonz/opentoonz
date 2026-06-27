#pragma once

#ifndef SCRIPTCONSOLEPANEL_H
#define SCRIPTCONSOLEPANEL_H

#include "pane.h"

#include <QString>
#include <QtGlobal>

class ScriptConsole;

class ScriptConsolePanel final : public TPanel {
  Q_OBJECT
  ScriptConsole *m_scriptConsole;

public:
  ScriptConsolePanel(QWidget *parent       = 0,
                     Qt::WindowFlags flags = Qt::WindowFlags());
  ~ScriptConsolePanel();

  void executeCommand(const QString &cmd);

#if OPENTOONZ_QT_MAJOR >= 6
  Q_INVOKABLE QString viewScriptImage(int imageId);
  Q_INVOKABLE QString viewScriptLevel(int levelId);
#endif

public slots:
  void selectNone();
};

#endif  // TESTPANEL_H
