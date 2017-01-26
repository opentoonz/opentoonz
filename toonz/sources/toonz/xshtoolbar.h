#pragma once

#ifndef XSHTOOLBAR_H
#define XSHTOOLBAR_H

#include <memory>

#include "toonz/txsheet.h"
#include "toonz/txshleveltypes.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/dvtextedit.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/keyframenavigator.h"
#include "toonzqt/functiontoolbar.h"

#include <QFrame>
#include <QScrollArea>

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QTextEdit;
class TColorStyle;
class QToolButton;
class QPushButton;
class QComboBox;

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// XSheet Toolbar
//-----------------------------------------------------------------------------

class Toolbar final : public QFrame {
  Q_OBJECT

  XsheetViewer *m_viewer;

  QToolButton *m_newVectorLevelButton;
  QToolButton *m_newToonzRasterLevelButton;
  QToolButton *m_newRasterLevelButton;
  QPushButton *m_reframe1sButton;
  QPushButton *m_reframe2sButton;
  QPushButton *m_reframe3sButton;
  QToolButton *m_repeatButton;
  ViewerKeyframeNavigator *m_keyFrameButton;
  DVGui::ToolBar *m_toolbar;

public:
#if QT_VERSION >= 0x050500
  Toolbar(XsheetViewer *parent = 0, Qt::WindowFlags flags = 0);
#else
  Toolbar(XsheetViewer *parent = 0, Qt::WFlags flags = 0);
#endif

protected slots:
  void onNewVectorLevelButtonPressed();
  void onNewToonzRasterLevelButtonPressed();
  void onNewRasterLevelButtonPressed();
  void onReframe1sButtonPressed();
  void onReframe2sButtonPressed();
  void onReframe3sButtonPressed();
  void onRepeatButtonPressed();
};

}  // namespace XsheetGUI;

#endif  // XSHTOOLBAR_H
