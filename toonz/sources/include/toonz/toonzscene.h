#pragma once

#ifndef TOONZSCENE_H
#define TOONZSCENE_H

// TnzCore includes
#include "tfilepath.h"
#include "traster.h"
#include "tstream.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//========================================================================

//    Forward declarations

class ChildStack;
class TSceneProperties;
class TLevelSet;
class TProject;
class TCamera;
class TXsheet;
class TXshLevel;
class TXshSoundColumn;
class TContentHistory;
class LevelOptions;

//========================================================================

//**********************************************************************************
//    ToonzScene  definition
//**********************************************************************************

class DVAPI ToonzScene {
public:
  ToonzScene();
  ~ToonzScene();

  /*! \details    The project associated with a scene could be different
            from the \a current project.

\internal   Seems that said project is <I>sharedly owned</I> by every
            scene referring to the project. I would expect a
            \a project to own its scenes, not the other way around...         */

  TProject *getProject()
      const;  //!< Returns a pointer to the project holding a scene instance.
  void setProject(TProject *);  //!< Associates a scene to a project.

  void setScenePath(const TFilePath &p);  //!< Sets the scene's file path.
  TFilePath getScenePath() const {
    return m_scenePath;
  }  //!< Returns the scene's file path.

  void setSceneName(std::wstring name);  //!< Sets the scene's name.
  std::wstring getSceneName() const      //!  Returns the scene's name.
  {
    return m_scenePath.getWideName();
  }

  /*! \details    An untitled scene generates unique temporary paths hosting
            any newly created scene content. Said content is then moved
            once a proper path is specified by the user.                      */

  void setUntitled();  //!< Marks the scene as explicitly untitled.
  bool isUntitled()
      const;  //!< Returns whether the scene path is empty, or the scene
              //!  is explicitly untitled.  \sa  Member function setUntitled().

  TXsheet *getXsheet()
      const;  //!< Returns a pointer to the scene's \a current xsheet.
  TXsheet *getTopXsheet() const;  //!< Returns the \a topmost xsheet in the
                                  //! scene's xsheet hierarchy.

  ChildStack *getChildStack() const {
    return m_childStack;
  }  //!< Returns a pointer to the xsheets hierarchy.

  TSceneProperties *getProperties()
      const  //!< Returns a pointer to the scene's properties.
  {
    return m_properties;
  }

  TLevelSet *getLevelSet() const {
    return m_levelSet;
  }  //!< Returns a pointer to the scene's <I>Level Set</I>.

  void clear();  //!< Clears the scene.

  void save(
      const TFilePath &path,
      TXsheet *subxsheet = 0);  //!< Saves the scene (or a sub-xsheet) at the
                                //!  specified path.
  void loadTnzFile(
      const TFilePath &path);  //!< Loads scene data from file, \a excluding the
                               //!  associated project and the scene resources.
  void loadNoResources(const TFilePath &path);  //!< Loads a scene \a without
                                                //! loading its resources.
  void loadResources(
      bool withProgressDialog = false);  //!< Loads the scene resources.
  void load(const TFilePath &path,
            bool withProgressDialog = false);  //!  Loads a scene from file.

  /*! \return   The \a coded path to be used for import. */

  TFilePath getImportedLevelPath(const TFilePath path)
      const;  //!< Builds the path to be used during a level import
              //!< operation.

  /*! \details  If convertion is required, a new level file will be created
          and \p levelPath will be substituted with its new path.

\return   Whether the level doesn't need to be converted or
          the convertion succeeded.                                           */

  bool convertLevelIfNeeded(TFilePath &levelPath);  //!< Checks if the given
                                                    //! levelPath is a file path
  //! of an
  //!  old Toonz level, and converts it if needed.

  /*! \details  The loadLevel() function accepts an optional pointer to the
          LevelOptions to be used by the level. If no option is explicitly
          specified, a level format match will be looked up in the
          application preferences; if none is found the default ones will
          be used.

\return   A Pointer to the loaded level.                                      */

  TXshLevel *loadLevel(
      const TFilePath &actualPath,
      const LevelOptions *options =
          0,  //!< Loads a level from the specified \a decoded path.
      std::wstring levelName            = L"",
      const std::vector<TFrameId> &fIds = std::vector<TFrameId>());

  /*!
Performs a camera-stand render of the specified xsheet (or the main one in case
none was
specified), on its currently selected camera, and returns the result on top of
the specified
32-bit raster.
*/
  void renderFrame(const TRaster32P &ras, int row, const TXsheet *xsh = 0,
                   bool checkFlags = true) const;

  /*!
Performs a camera-stand render of the specified xsheet in the specified
placedRect,
with known world/placed reference change - and returns the result on top of the
specified
32-bit raster.
*/
  void renderFrame(const TRaster32P &ras, int row, const TXsheet *xsh,
                   const TRectD &placedRect,
                   const TAffine &worldToPlacedAff) const;

  /*!
Return scene frame count.
*/
  int getFrameCount() const;

  /*!
Returs frame count stored in the scene header.
Quicker than load(fp); getFrameCount()
*/
  int loadFrameCount(const TFilePath &fp);

  /*!
Return a pointer to \b TXshLevel, a new level.
\n
Create a new level with \b type defined in \b txshleveltypes.h and \b levelName.
\b wstring \b levelName force an anambiguos name.
*/
  TXshLevel *createNewLevel(int type, std::wstring levelName = L"",
                            const TDimension &dim = TDimension(),
                            double dpi = 0, TFilePath fp = TFilePath());

  /*!
Code file path \b TFilePath.
\sa decodeFilePath()
*/
  TFilePath codeFilePath(const TFilePath &) const;
  /*!
Decode file path \b TFilePath.
\sa codeFilePath()
*/
  TFilePath decodeFilePath(const TFilePath &) const;
  /*!
Return true if \b TFilePath \b fp is not contained in a project folder.
*/
  bool isExternPath(const TFilePath &fp) const;

  /*!
Return the default \b TFilePath of a \b levelType level
*/

  TFilePath getDefaultLevelPath(int levelType,
                                std::wstring levelName = L"a") const;

  /*!
Code save path. Es. +drawings/seq1/scena1/a.pli -> +drawings/$savepath/a.pli
\sa decodeSavePath()
*/
  TFilePath codeSavePath(TFilePath) const;

  /*!
Decode save path. es. +drawings/$savepath/a.pli -> +drawings/seq1/scena1/a.pli
\sa codeSavePath()
*/
  TFilePath decodeSavePath(TFilePath) const;

  /*!
Return a pointer to current camera \b TCamera.
*/
  TCamera *getCurrentCamera();

  TCamera *getCurrentPreviewCamera();

  /*!
Insert in \b vector \b columns all sound column of scene \b scene.
*/
  void getSoundColumns(std::vector<TXshSoundColumn *> &columns);
  /*!
          Set output properties frameRate to all sound column of scene.
  */
  void updateSoundColumnFrameRate();

  /*!
Returns the path of the icon related to the scene
*/
  static TFilePath getIconPath(const TFilePath &scenePath);
  inline TFilePath getIconPath() const { return getIconPath(getScenePath()); }

  /*!Return save path of \b scene.
If \b scene is in +scenes/name.tnz return name,
\n if \b scene is in +scenes/folder/name.tnz return folder/name
\n if \b scene is untitledxxx.tnz return untitledxxx.
*/
  TFilePath getSavePath() const;

  //! gets the contentHistory. possibly creates a new one (if createIfNeeded)
  TContentHistory *getContentHistory(bool createIfNeeded = false);

  // shifts camera offet, returning old value; usend for steroscophy
  double shiftCameraX(double val);

  void setVersionNumber(VersionNumber version) { m_versionNumber = version; }
  VersionNumber getVersionNumber() { return m_versionNumber; }

  // if the path is codable with $scenefolder alias, replace it and return true
  bool codeFilePathWithSceneFolder(TFilePath &path) const;

private:
  TFilePath m_scenePath;  //!< Full path to the scene file (.tnz).

  ChildStack *m_childStack;  //!< Xsheet hierachy.
  TSceneProperties *m_properties;
  TLevelSet *m_levelSet;
  TProject *m_project;
  TContentHistory *m_contentHistory;
  bool m_isUntitled;  //!< Whether the scene is untitled.
                      //!  \sa  The setUntitled() member function.
  VersionNumber m_versionNumber;

private:
  // noncopyable
  ToonzScene(const ToonzScene &);
  ToonzScene &operator=(const ToonzScene &);

  // if the option is set in the preferences,
  // remove the scene numbers("c####_") from the file name
  std::wstring getLevelNameWithoutSceneNumber(std::wstring orgName);
};

#endif  // TOONZSCENE_H
