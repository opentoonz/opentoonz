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
  QPushButton *m_reframe1sButton;
  QPushButton *m_reframe2sButton;
  QPushButton *m_reframe3sButton;
  QPushButton *m_repeatButton;
  QPushButton *m_collapseSubButton;
  QPushButton *m_enterSubButton;
  QPushButton *m_leaveSubButton;
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
  void onReframe1sButtonPressed();
  void onReframe2sButtonPressed();
  void onReframe3sButtonPressed();
  void onRepeatButtonPressed();
  void onCollapseSubButtonPressed();
  void onEnterSubButtonPressed();
  void onLeaveSubButtonPressed();
};

}  // namespace XsheetGUI;

#endif  // XSHTOOLBAR_H
