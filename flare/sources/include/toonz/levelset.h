#pragma once

#ifndef LEVELSET_INCLUDED
#define LEVELSET_INCLUDED

// TnzCore includes
#include "tfilepath.h"

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//    Forward declarations

class ToonzScene;
class TXshLevel;
class TXshSimpleLevel;
class TLevel;
class TIStream;
class TOStream;

//=============================================================================

//*********************************************************************************************
//    TLevelSet  declaration
//*********************************************************************************************

//! The TLevelSet is a typical ToonzScene component that is used to trace all
//! levels in a
//! Toonz scene.
/*!
  The main purpose of the TLevelSet is that of acting as a container of <B>
  externally
  owned <\B> TXshLevel* instances. The container is internally implemented as a
  vector,
  and shares its access model.
  \n\n
  Additionally, the set is used to map the stored levels by their names
  (case-insensitive
  comparisons), and associates a \a folder to each of the level instances.
  The level folder is not physical (ie it does not refer to a directory in the
  file system),
  but can be used to list the levels pool in a hierarchycal view.

  \warning This class distinguishes between \b paths and \b folders as described
  above, but
           the associated data type (TFilePath) is the same. Please, pay
  attention to the
           function argument names, in this respect.
*/

class DVAPI TLevelSet {
  std::vector<TXshLevel *> m_levels;                 //!< The levels container
  std::map<std::wstring, TXshLevel *> m_levelTable;  //!< Name to level map

  std::map<TXshLevel *, TFilePath>
      m_folderTable;                 //!< Level to its containing folder map
  std::vector<TFilePath> m_folders;  //!< Folders container
  TFilePath m_defaultFolder;         //!< The root folder

  std::set<TXshLevel *>
      m_saveSet;  //!< If m_saveSet is not empty, only its levels
                  //!< will be saved
public:
  TLevelSet();
  ~TLevelSet();

  bool insertLevel(TXshLevel *level);
  bool renameLevel(TXshLevel *level, const std::wstring &newName);
  void removeLevel(TXshLevel *level, bool deleteIt = true);
  void clear();

  int getLevelCount() const;

  TXshLevel *getLevel(int index) const;
  TXshLevel *getLevel(const std::wstring &levelName) const;
  TXshLevel *getLevel(const ToonzScene &scene,
                      const TFilePath &levelPath) const;

  bool hasLevel(const std::wstring &levelName) const;
  bool hasLevel(const ToonzScene &scene, const TFilePath &levelPath) const;

  void listLevels(std::vector<TXshLevel *> &levels) const;
  void listLevels(std::vector<TXshLevel *> &levels,
                  const TFilePath &parentFolder) const;

  TFilePath getDefaultFolder() const { return m_defaultFolder; }
  void setDefaultFolder(TFilePath folder);

  TFilePath createFolder(const TFilePath &parent, const std::wstring &newName);
  TFilePath renameFolder(const TFilePath &folder, const std::wstring &newName);
  void removeFolder(const TFilePath &folder);

  TFilePath getFolder(TXshLevel *xl) const;
  void listFolders(std::vector<TFilePath> &folders,
                   const TFilePath &parentFolder) const;

  void moveLevelToFolder(const TFilePath &folder, TXshLevel *level);

  //! Defines the set of levels that will be saved in output streams (in case
  //! it's empty, all
  //! levels will be saved). \sa The saveData() method inherited from TPersist.
  void setSaveSet(const std::set<TXshLevel *> &saveSet) { m_saveSet = saveSet; }

  void loadData(TIStream &is);
  void saveData(TOStream &os);

private:
  // Not copiable
  TLevelSet(const TLevelSet &);
  TLevelSet &operator=(const TLevelSet &);

  void saveFolder(TOStream &os, TFilePath folder);
  void loadFolder(TIStream &is, TFilePath folder);
};

#endif  // LEVELSET_INCLUDED
