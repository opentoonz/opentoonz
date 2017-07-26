#pragma once

#ifndef XSHTOOLBAR_H
#define XSHTOOLBAR_H

#include <memory>

#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonzqt/keyframenavigator.h"

#include <QFrame>
#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QPushButton;

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// XSheet Toolbar
//-----------------------------------------------------------------------------

class Toolbar final : public QFrame {
  Q_OBJECT

  XsheetViewer *m_viewer;

  QPushButton *m_newVectorLevelButton;
  QPushButton *m_newToonzRasterLevelButton;
  QPushButton *m_newRasterLevelButton;
  ViewerKeyframeNavigator *m_keyFrameButton;
  QToolBar *m_toolbar;

public:
#if QT_VERSION >= 0x050500
  Toolbar(XsheetViewer *parent = 0, Qt::WindowFlags flags = 0);
#else
  Toolbar(XsheetViewer *parent = 0, Qt::WFlags flags = 0);
#endif
  static void toggleXSheetToolbar();
  void showToolbar(bool show);

protected slots:
  void onNewVectorLevelButtonPressed();
  void onNewToonzRasterLevelButtonPressed();
  void onNewRasterLevelButtonPressed();
};

}  // namespace XsheetGUI;

#endif  // XSHTOOLBAR_H
