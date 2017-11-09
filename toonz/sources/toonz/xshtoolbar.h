#pragma once

#ifndef XSHTOOLBAR_H
#define XSHTOOLBAR_H

#include <memory>

#include "toonz/txsheet.h"
#include "commandbar.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QAction;

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// XSheet Toolbar
//-----------------------------------------------------------------------------

class XSheetToolbar final : public CommandBar {
  Q_OBJECT

  XsheetViewer *m_viewer;
  bool m_isCollapsible;

public:
#if QT_VERSION >= 0x050500
  XSheetToolbar(XsheetViewer *parent = 0, Qt::WindowFlags flags = 0,
                bool isCollapsible = false);
#else
  XSheetToolbar(XsheetViewer *parent = 0, Qt::WFlags flags = 0);
#endif
  static void toggleXSheetToolbar();
  void showToolbar(bool show);
signals:
  void updateVisibility();

protected:
  void showEvent(QShowEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void doCustomizeCommandBar();
};

}  // namespace XsheetGUI;

#endif  // XSHTOOLBAR_H
