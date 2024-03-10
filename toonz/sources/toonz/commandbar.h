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
protected:
  bool m_isCollapsible;

public:
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags(),
             bool isCollapsible = false, bool isXsheetToolbar = false);

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
