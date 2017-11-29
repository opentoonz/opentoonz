#pragma once

#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <memory>

#include "toonz/txsheet.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class QAction;

//=============================================================================
// CommandBar
//-----------------------------------------------------------------------------

class CommandBar : public QToolBar {
  Q_OBJECT
  bool m_isCollapsible;

public:
#if QT_VERSION >= 0x050500
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = 0,
             bool isCollapsible = false, bool isXsheetToolbar = false);
#else
  CommandBar(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif

signals:
  void updateVisibility();

protected:
  static void fillToolbar(CommandBar *toolbar, bool isXsheetToolbar = false);
  static void buildDefaultToolbar(CommandBar *toolbar);
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void doCustomizeCommandBar();
};

#endif  // COMMANDBAR_H
