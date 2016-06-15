

#include "toonz/tscenehandle.h"

#include "toonz/toonzscene.h"
#include "toonz/tproject.h"

//=============================================================================
// TSceneHandle
//-----------------------------------------------------------------------------

TSceneHandle::TSceneHandle() : m_scene(0), m_dirtyFlag(false) {}

//-----------------------------------------------------------------------------

TSceneHandle::~TSceneHandle() {}

//-----------------------------------------------------------------------------

ToonzScene *TSceneHandle::getScene() const { return m_scene; }

//-----------------------------------------------------------------------------

void TSceneHandle::setScene(ToonzScene *scene) {
  if (m_scene == scene) return;
  delete m_scene;
  m_scene = scene;
  if (m_scene) emit sceneSwitched();
}
