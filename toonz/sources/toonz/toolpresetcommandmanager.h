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
  // True when presets were added/removed and commands need rebuilding.
  // Prevents redundant full re-registration on every room switch.
  bool m_commandsDirty;
  
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
  
  // Marks commands as dirty so the next refreshPresetCommands() call
  // rebuilds them.  Call this when presets are actually added or removed.
  void markCommandsDirty();
  
  // Refreshes preset commands only when the preset list has changed.
  // Safe to call frequently (e.g. on panel show) without performance cost.
  void refreshPresetCommands();
  
  // Refreshes size commands (called when shortcut popup is opened)
  void refreshSizeCommands();
  
public slots:
  // Updates checked state of preset and size actions based on current tool
  void updateCheckedStates();
};

#endif

