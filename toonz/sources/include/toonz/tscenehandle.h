#pragma once

#ifndef TSCENEHANDLE_H
#define TSCENEHANDLE_H

#include <QObject>

#include "tcommon.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class ToonzScene;
class TFilePath;

//=============================================================================
// TSceneHandle
//-----------------------------------------------------------------------------

class DVAPI TSceneHandle final : public QObject {
  Q_OBJECT

  ToonzScene *m_scene;
  bool m_dirtyFlag;

public:
  TSceneHandle();
  ~TSceneHandle();

  ToonzScene *getScene() const;

  void setScene(ToonzScene *scene);
  void notifySceneChanged(bool setDirty = true) {
    emit sceneChanged();
    if (setDirty) setDirtyFlag(true);
  }
  void notifySceneSwitched() {
    emit sceneSwitched();
    setDirtyFlag(false);
  }
  void notifyCastChange() { emit castChanged(); }
  void notifyCastFolderAdded(const TFilePath &path) {
    emit castFolderAdded(path);
  }
  void notifyNameSceneChange() { emit nameSceneChanged(); }

  void notifyPreferenceChanged(const QString &prefName) {
    emit preferenceChanged(prefName);
  }

  void notifyPixelUnitSelected(bool on) { emit pixelUnitSelected(on); }
  void notifyImportPolicyChanged(int policy) {
    emit importPolicyChanged(policy);
  }

  void setDirtyFlag(bool dirtyFlag) {
    if (m_dirtyFlag == dirtyFlag) return;
    m_dirtyFlag = dirtyFlag;
    emit nameSceneChanged();
  }
  bool getDirtyFlag() const { return m_dirtyFlag; }

public slots:
  void setDirtyFlag() {
    if (m_dirtyFlag == true) return;
    m_dirtyFlag = true;
    emit nameSceneChanged();
  }

signals:
  void sceneSwitched();
  void sceneChanged();
  void castChanged();
  void castFolderAdded(const TFilePath &path);
  void nameSceneChanged();
  void preferenceChanged(const QString &prefName);
  void pixelUnitSelected(bool on);
  void importPolicyChanged(int policy);
};

#endif  // TSCENEHANDLE_H
