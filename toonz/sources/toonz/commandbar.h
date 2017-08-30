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
// XSheet Toolbar
//-----------------------------------------------------------------------------

class CommandBar final : public QToolBar {
  Q_OBJECT

  ViewerKeyframeNavigator *m_keyFrameButton;
  bool m_isCollapsible;
  QAction *m_editInPlace;

public:
#if QT_VERSION >= 0x050500
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = 0,
             bool isCollapsible = false);
#else
  CommandBar(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif

signals:
  void updateVisibility();

protected slots:
  void updateEditInPlaceStatus();
};

#endif  // COMMANDBAR_H
