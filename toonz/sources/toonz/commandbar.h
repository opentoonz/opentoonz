#pragma once

#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <memory>

#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QPushButton;

//=============================================================================
// XSheet Toolbar
//-----------------------------------------------------------------------------

class CommandBar final : public QToolBar {
  Q_OBJECT

  //XsheetViewer *m_viewer;
  ViewerKeyframeNavigator *m_keyFrameButton;
  bool m_isCollapsible;

public:
#if QT_VERSION >= 0x050500
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = 0,
                bool isCollapsible = false);
#else
  CommandBar(XsheetViewer *parent = 0, Qt::WFlags flags = 0);
#endif
  
signals:
  void updateVisibility();
 
};

#endif  // COMMANDBAR_H
