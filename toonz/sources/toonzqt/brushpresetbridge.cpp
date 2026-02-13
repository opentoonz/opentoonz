#include "toonzqt/brushpresetbridge.h"
#include <QMutex>
#include <QMutexLocker>

namespace BrushPresetBridge {
static QString s_pendingPresetName;
static QStringList s_pendingRemoveNames;
static QMutex s_mutex;

void setPendingPresetName(const QString &name) {
  QMutexLocker lock(&s_mutex);
  s_pendingPresetName = name;
}

QString takePendingPresetName() {
  QMutexLocker lock(&s_mutex);
  QString name = s_pendingPresetName;
  s_pendingPresetName.clear();
  return name;
}

void addPendingRemoveName(const QString &name) {
  QMutexLocker lock(&s_mutex);
  s_pendingRemoveNames.append(name);
}

QStringList takePendingRemoveNames() {
  QMutexLocker lock(&s_mutex);
  QStringList names = s_pendingRemoveNames;
  s_pendingRemoveNames.clear();
  return names;
}
}  // namespace BrushPresetBridge
