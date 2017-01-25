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

  QPushButton *m_newVectorLevelButton;
  QPushButton *m_newToonzRasterLevelButton;
  QPushButton *m_newRasterLevelButton;
  QPushButton *m_reframe1sButton;
  QPushButton *m_reframe2sButton;
  QPushButton *m_reframe3sButton;
  QPushButton *m_repeatButton;
  ViewerKeyframeNavigator *m_keyFrameButton;

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
