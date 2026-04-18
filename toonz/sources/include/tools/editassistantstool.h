#pragma once

#include <QSettings>

// Helper for Edit Assistants tool "Auto-Switch & Keep" setting (QSettings)
inline bool isEditAssistantsAutoSwitchAndKeepEnabled() {
  return QSettings().value("EditAssistantsTool/AutoSwitchAndKeep", false)
      .toBool();
}
