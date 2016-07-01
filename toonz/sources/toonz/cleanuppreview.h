#pragma once

#ifndef CLEANUPPREVIEW_H
#define CLEANUPPREVIEW_H

// STL includes
#include <vector>

// ToonzLib includes
#include "toonz/txshsimplelevel.h"

// ToonzQt includes
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QTimer>

//========================================================

//  Forward declarations

class TXshSimpleLevel;
class TTool;

//========================================================

//**********************************************************************************
//    PreviewToggleCommand declaration
//**********************************************************************************

class PreviewToggleCommand final : public MenuItemHandler {
  Q_OBJECT

  TXshSimpleLevelP m_sl;         //!< Level under cleanup focus
  std::vector<TFrameId> m_fids;  //!< Previewed frames of said level

  QTimer m_timer;  //!< Processing timer - allows processing only
                   //!< after half a sec. without parameter changes
public:
  PreviewToggleCommand();

  void execute() override;

protected:
  friend class CameraTestToggleCommand;

  void enable();
  void disable();

  void clean();

protected slots:

  void onPreviewDataChanged();
  void onModelChanged(bool needsPostProcess);
  void postProcess();
};

//**********************************************************************************
//    CameraTestToggleCommand declaration
//**********************************************************************************

class CameraTestToggleCommand final : public MenuItemHandler {
  Q_OBJECT

  TTool *m_oldTool;

  TXshSimpleLevelP m_sl;
  std::vector<TFrameId> m_fids;

  QTimer m_timer;

public:
  CameraTestToggleCommand();

  void execute() override;

protected:
  friend class PreviewToggleCommand;

  void enable();
  void disable();

  void clean();

protected slots:

  void onPreviewDataChanged();
  void postProcess();
};

#endif  // CLEANUPPREVIEW_H
