#pragma once

#ifndef SCENERESOURCES_INCLUDED
#define SCENERESOURCES_INCLUDED

#include "tfilepath.h"
#include <map>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <QString>
#include <QSet>

//=============================================================================
// forward declarations
class TProject;
class ToonzScene;
class TXshSimpleLevel;
class TXshPaletteLevel;
class TXsheet;

//=============================================================================
/*!ResourceImportStrategy allows to control the resource import behaviour during
   load operation. It can be statically defined (i.e. DONT_IMPORT).
   Subclass it to provide dialog boxes.
   All the actual file copies are recorded (ImportLog).
   \sa ResourceImportMap, ResourceImportDialog (defined in toonz)
*/
//=============================================================================

class DVAPI ResourceImportStrategy {
public:
  typedef std::map<TFilePath, TFilePath> ImportLog;
  enum { DONT_IMPORT, IMPORT_AND_OVERWRITE, IMPORT_AND_RENAME };

  ResourceImportStrategy(int strategy);
  virtual bool aborted() { return false; }

  // note: this attribute is automatically initialized (from the preferences)
  bool isChildFolderEnabled() const { return m_childFolderEnabled; }
  void setChildFolderEnabled(bool enabled) { m_childFolderEnabled = enabled; }

  // accepts and returns coded file (e.g. +drawings/folder/a.pli)
  virtual TFilePath process(ToonzScene *dstScene, ToonzScene *srcScene,
                            TFilePath srcFile) = 0;

  const ImportLog &getImportLog() const { return m_importLog; }

private:
  bool m_childFolderEnabled;
  int m_strategy;
  ImportLog m_importLog;
};

//=============================================================================
//! ResourceProcessor is the base class for resource processing.
/*! It is a visitor accepted by SceneResources */
//=============================================================================

class TXshSoundLevel;

class DVAPI ResourceProcessor {
public:
  virtual void process(TXshSimpleLevel *sl) {}
  virtual void process(TXshPaletteLevel *sl) {}
  virtual void process(TXshSoundLevel *sl) {}
  virtual bool aborted() const { return false; }
};

//=============================================================================
//! The SceneResource class is the base class of scene resource.
/*!The class contains a pointer to scene, a boolean to know if scene is
   untiltled and a path to know old save path.
*/
//=============================================================================

class DVAPI SceneResource {
protected:
  ToonzScene *m_scene;
  bool m_untitledScene;
  TFilePath m_oldSavePath;

public:
  /*!
Constructs SceneResource with default value and \b scene.
*/
  SceneResource(ToonzScene *scene);
  /*!
Destroys the SceneResource object.
*/
  virtual ~SceneResource();

  //! save the resource to the disk using the updated path. Note: it calls
  //! SceneResource::updatePath(fp) before
  virtual void save() = 0;
  //! change the resource internal path, according to the scene
  virtual void updatePath() = 0;
  //! change back the resource internal path to its original value
  virtual void rollbackPath() = 0;

  //! visitor pattern
  virtual void accept(ResourceProcessor *processor) = 0;
  /*!
If scene is untitled update save path.
*/
  void updatePath(TFilePath &fp);

  virtual bool isDirty()                = 0;
  virtual QStringList getResourceName() = 0;
};

//=============================================================================
//! The ScenePalette class provides a container for scene palette resource and
//! allows its management.
/*!Inherits \b SceneResource.
\n The class, more than \b SceneResources, contains a pointer to a palette level
   \b TXshPaletteLevel, two path for old save path, one decoded and one no,
   two path for old scanned path, one decoded and one no. The class allows to
save
   simple level save() and to update or to restore old path (save and scan)
updatePath()
   rollbackPath().
*/
//=============================================================================

class DVAPI ScenePalette final : public SceneResource {
  TXshPaletteLevel *m_pl;
  TFilePath m_oldPath, m_oldActualPath;

public:
  /*!
Constructs SceneLevel with \b ToonzScene \b scene and \b TXshPaletteLevel \b pl.
*/
  ScenePalette(ToonzScene *scene, TXshPaletteLevel *pl);
  /*!
Save simple level in right path.
*/
  void save() override;
  /*!
Update simple level path.
*/
  void updatePath() override;
  /*!
Set simple level path to old path.
*/
  void rollbackPath() override;

  void accept(ResourceProcessor *processor) override {
    processor->process(m_pl);
  }

  bool isDirty() override;
  QStringList getResourceName() override;
};

//=============================================================================
//! The SceneLevel class provides a container for scene level resource and
//! allows its management.
/*!Inherits \b SceneResource.
\n The class, more than \b SceneResources, contains a pointer to a simple level
   \b TXshSimpleLevel, two path for old save path, one decoded and one no,
   two path for old scanned path, one decoded and one no. The class allows to
save
   simple level save() and to update or to restore old path (save and scan)
updatePath()
   rollbackPath().
*/
//=============================================================================

class DVAPI SceneLevel final : public SceneResource {
  TXshSimpleLevel *m_sl;
  TFilePath m_oldPath, m_oldActualPath;
  TFilePath m_oldScannedPath, m_oldActualScannedPath;
  TFilePath m_oldRefImgPath, m_oldActualRefImgPath;

public:
  /*!
Constructs SceneLevel with \b ToonzScene \b scene and \b TXshSimpleLevel \b sl.
*/
  SceneLevel(ToonzScene *scene, TXshSimpleLevel *sl);
  /*!
Save simple level in right path.
*/
  void save() override;
  /*!
Update simple level path.
*/
  void updatePath() override;
  /*!
Set simple level path to old path.
*/
  void rollbackPath() override;

  void accept(ResourceProcessor *processor) override {
    processor->process(m_sl);
  }

  bool isDirty() override;
  QStringList getResourceName() override;
};

//=============================================================================
//! The SceneSoundtrack class provides a container for scene sound track
//! resource and allows its management.
/*!Inherits \b SceneResource.
\n The class, more than \b SceneResources, contains a pointer to a sound column
   \b TXshSoundColumn, two path for old save path, one decoded and one no.
   The class allows to save sound track save() and to update or to restore old
path
   updatePath(), rollbackPath().
*/
//=============================================================================

class DVAPI SceneSound final : public SceneResource {
  TXshSoundLevel *m_sl;
  TFilePath m_oldPath, m_oldActualPath;

public:
  /*!
Constructs SceneSoundtrack with \b ToonzScene \b scene and \b TXshSoundLevel \b
sl.
*/
  SceneSound(ToonzScene *scene, TXshSoundLevel *sl);

  /*!
Save sound column in right path.
*/
  void save() override;
  /*!
Update sound track path.
*/
  void updatePath() override;
  /*!
Set sound track path to old path.
*/
  void rollbackPath() override;

  void accept(ResourceProcessor *processor) override {
    processor->process(m_sl);
  }

  bool isDirty() override { return false; }
  QStringList getResourceName() override { return QStringList(); }
};

//=============================================================================
//! The SceneResources class provides scene resources and allows their
//! management.
/*!The class contains a vector of pointer to \b SceneResource, a pointer to
   scene,
   a pointer to xsheet, a boolean to know if commit is done and a boolean to
   know
   if scene is untitled.
   The class allows to save scene resources save(), and to update or to restore
   old
   scene resources paths updatePaths(), rollbackPaths().
*/
//=============================================================================

class DVAPI SceneResources {
  std::vector<SceneResource *> m_resources;
  ToonzScene *m_scene;
  TXsheet *m_subXsheet;
  bool m_commitDone;
  bool m_wasUntitled;

  /*!
Set the vector of pointer to \b SceneResource to scene resources.
*/
  void getResources();

public:
  // n.b. se subXsheet != 0 salva solo le risorse utilizzate nel subxsheet
  /*!
Constructs SceneResources with \b ToonzScene \b scene and
\b TXsheet \b subXsheet.
*/
  SceneResources(ToonzScene *scene, TXsheet *subXsheet);
  /*!
Destroys the SceneResources object.
*/
  ~SceneResources();
  /*
!Save all scene resources in according to \b newScenePath.
\n
Set scene save path to \b newScenePath, call \b save()
for all scene resources and reset scene save path to
old save path.
If pointer to subXsheet is different from zero save only resources
used in subXsheet.
*/
  void save(const TFilePath newScenePath);

  /*!
Update all resources paths.
*/
  void updatePaths();
  /*!
Set all resources paths to old paths.
*/
  void rollbackPaths();
  /*!
Set boolean \b m_commitDone to true.
\n
If doesn't make \b commit() destroyer calls \b rollbackPaths().
*/
  void commit() { m_commitDone = true; }

  void accept(ResourceProcessor *processor, bool autoCommit = true);

  // return the name list of dirty resources
  void getDirtyResources(QStringList &dirtyResources);

private:
  // not implemented
  SceneResources(const SceneResources &);
  SceneResources &operator=(const SceneResources &);
};

//=============================================================================

class DVAPI ResourceImporter final : public ResourceProcessor {
public:
  ResourceImporter(ToonzScene *scene, TProject *dstProject,
                   ResourceImportStrategy &strategy);
  ~ResourceImporter();

  // se serve modifica path. path non deve esistere su disco
  // ritorna true se ha modificato path;
  // n.b. path puo' essere della forma +drawings/xxx.pli
  bool makeUnique(TFilePath &path);

  TFilePath getImportedScenePath() const;

  TFilePath codePath(const TFilePath &oldCodedPath,
                     const TFilePath &newActualPath);

  void process(TXshSimpleLevel *sl) override;
  void process(TXshPaletteLevel *sl) override;
  void process(TXshSoundLevel *sl) override;

  bool aborted() const override { return m_importStrategy.aborted(); }

  static std::string extractPsdSuffix(TFilePath &path);
  static TFilePath buildPsd(const TFilePath &path, const std::string &suffix);

private:
  ToonzScene *m_scene;
  TProject *m_dstProject;
  ToonzScene *m_dstScene;
  ResourceImportStrategy &m_importStrategy;
};

//=============================================================================

// Rende tutte le risorse locali: tutte quelle con path assoluto
// vengono copiate dentro il progetto

class DVAPI ResourceCollector final : public ResourceProcessor {
  ToonzScene *m_scene;
  int m_count;
  std::map<TFilePath, TFilePath> m_collectedFiles;

public:
  ResourceCollector(ToonzScene *scene);
  ~ResourceCollector();

  // se serve modifica path. path non deve esistere su disco
  // ritorna true se ha modificato path;
  // n.b. path e' della forma +drawings/xxx.pli
  bool makeUnique(TFilePath &path);

  int getCollectedResourceCount() const { return m_count; }

  void process(TXshSimpleLevel *sl) override;
  void process(TXshSoundLevel *sl) override;
  void process(TXshPaletteLevel *pl) override;
};

#endif
