#pragma once
#ifndef TOOL_PRESET_COMMAND_MANAGER_H
#define TOOL_PRESET_COMMAND_MANAGER_H

#include "toonzqt/menubarcommand.h"

#include <QString>
#include <QList>
#include <QSet>
#include <QActionGroup>

//=============================================================================
// Tool Preset Command Manager
// Dynamically registers brush presets as commands for Custom Panels
//=============================================================================

class ToolPresetCommandManager : public QObject {  // singleton
  Q_OBJECT
  
  ToolPresetCommandManager();
  
  QSet<QString> m_registeredPresetIds;
  QSet<QString> m_registeredSizeIds;
  bool m_initialized;
  
  // Action groups for mutual exclusivity
  QActionGroup* m_presetActionGroup;
  QActionGroup* m_sizeActionGroup;

public:
  static ToolPresetCommandManager* instance();
  
  // Initialize signal connections (call once at startup)
  void initialize();
  
  // Registers all available tool presets as commands
  void registerToolPresetCommands();
  
  // Registers universal size commands
  void registerSizeCommands();
  
  // Refreshes preset commands (called when presets are added/removed)
  void refreshPresetCommands();
  
  // Refreshes size commands (called when shortcut popup is opened)
  void refreshSizeCommands();
  
public slots:
  // Updates checked state of preset and size actions based on current tool
  void updateCheckedStates();
};

#endif

