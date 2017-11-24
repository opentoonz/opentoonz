#pragma once

#ifndef IOCOMMAND_H
#define IOCOMMAND_H

#include <memory>

// TnzLib includes
#include "toonz/preferences.h"

// TnzCore includes
#include "tfilepath.h"
#include "tundo.h"

// Qt includes
#include <QDialog>

// boost includes
#include <boost/optional.hpp>

// STD includes
#include <set>

//====================================================

//  Forward declarations

class TXshLevel;
class TXshSimpleLevel;
class TXshSoundLevel;
class ToonzScene;
class TCamera;
class TPropertyGroup;

namespace DVGui {
class ProgressDialog;
}

//====================================================

/*! \file       iocommand.h
    \brief      Contains command functions related to Toonz I/O.          */

/*! \brief      Contains command functions related to Toonz I/O.          */

namespace IoCmd {

/*!
  \brief    Composite data used describing a group of Toonz resources
            to be loaded.

  \details  A Toonz resource can refer to either a level, audio
            file, or Toonz scene to be loaded as sub-xsheet.

  \sa       The loadResources() function.
*/

struct LoadResourceArguments {
  /*! \details    A loading ScopedBlock represents a monolithic loading
            procedure, with integrated progress dialog and undo block.        */

  struct ScopedBlock
      : private TUndoScopedBlock  //!  Scope variable hosting a loading process.
  {
    struct Data;

  public:
    ScopedBlock();
    ~ScopedBlock();  //!< Invokes all necessary Toonz updates before control is
                     //! returned,
    //!  included posting of the embedded undo block.
    DVGui::ProgressDialog &progressDialog()
        const;  //!< Progress dialog shown on multiple resource loads.

    Data &data() const {
      return *m_data;
    }  //!< Internal data used by the loading procedure.

  private:
    std::unique_ptr<Data> m_data;
  };

  struct ResourceData  //!  Data about a single resource to be loaded.
  {
    TFilePath m_path;  //!< Path of the resource to be loaded.

    boost::optional<LevelOptions>
        m_options;  //!< User-defined properties to be applied as a level.

  public:
    ResourceData() {}
    ResourceData(const TFilePath &path) : m_path(path) {}
  };

  enum ImportPolicy  //!  Policy adopted for resources external to current
                     //!  scene.
  { ASK_USER,        //!< User is prompted for a resolution.
    IMPORT,          //!< Resources are copied to scene folders (\a overwrites).
    LOAD,            //!< Resources are loaded from their original paths.
  };

public:
  std::vector<ResourceData>
      resourceDatas;  //!< [\p In/Out]  Data identifying a single resource.

  // reuse TFrameIds retrieved by FileBrowser
  std::vector<std::vector<TFrameId>> frameIdsSet;

  TFilePath castFolder;  //!< [\p In]      Cast panel folder where the resources
                         //! will be inserted.

  int row0,  //!< [\p In/Out]  Starting xsheet row where resources have been
             //! inserted.
      col0,  //!< [\p In/Out]  Starting xsheet column where resources have been
             //! inserted.
      row1,  //!< [\p Out]     Ending xsheet row where resources have been
             //! inserted (included).
      col1;  //!< [\p Out]     Ending xsheet column where resources have been
             //! inserted (included).

  ImportPolicy importPolicy;  //!< [\p In]      Policy adopted for resources
                              //! external to current scene.
  bool expose;  //!< [\p In]      Whether resources must be exposed in the
                //! xsheet.

  std::vector<TXshLevel *> loadedLevels;  //!< [\p Out]     Levels loaded by
                                          //! resource loading procedures.
  //! Multiple
  //!               levels \a may be loaded for a single resource data.
public:
  /*-
   * Resourceは常にLoadするように変更（ファイルをプロジェクトフォルダにコピー（=Import）したい場合は、ユーザが手動で行う）
   * -*/
  LoadResourceArguments()
      : row0(-1)
      , col0(-1)
      , row1(-1)
      , col1(-1)
      , importPolicy(static_cast<ImportPolicy>(
            Preferences::instance()->getDefaultImportPolicy()))
      , expose(Preferences::instance()->isAutoExposeEnabled()) {}

  LoadResourceArguments(const TFilePath &fp,
                        const TFilePath &cast = TFilePath())
      : castFolder(cast)
      , row0(-1)
      , col0(-1)
      , row1(-1)
      , col1(-1)
      , importPolicy(static_cast<ImportPolicy>(
            Preferences::instance()->getDefaultImportPolicy()))
      , expose(Preferences::instance()->isAutoExposeEnabled()) {
    resourceDatas.push_back(fp);
  }
};

//------------------------------------------------------------------------

class ConvertingPopup final : public QDialog {
public:
  ConvertingPopup(QWidget *parent, QString fileName);
  ~ConvertingPopup();
};

//------------------------------------------------------------------------

void newScene();

bool loadScene(ToonzScene &scene, const TFilePath &scenePath, bool import);
bool loadScene(const TFilePath &scenePath, bool updateRecentFile = true,
               bool checkSaveOldScene = true);
bool loadScene();

bool loadSubScene();
bool loadSubScene(const TFilePath &scenePath);

enum SaveSceneFlags {
  SILENTLY_OVERWRITE = 0x1,
  SAVE_SUBXSHEET     = 0x2,
};

// ritorna true sse la scena e' stata salvata
// se fp esiste gia': se (flags&SILENTLY_OVERWRITE) != 0 sovrascrive
// silenziosamente, altrimenti
// chiede il permesso all'utente
// se l'xsheet corrente non e' l'xsheet principale: se (flags&SAVE_SUBXSHEET) ==
// 0 salva comunque
// tutta la scena, altrimenti solo il sottoxsheet
bool saveScene(const TFilePath &fp, int flags);
bool saveScene();

bool saveLevel(const TFilePath &fp);
bool saveLevel();

bool saveLevel(const TFilePath &fp, TXshSimpleLevel *sl, bool overwrite);
bool saveLevel(TXshSimpleLevel *sl);

bool saveAll();

void saveNonSceneFiles();

bool saveSound(const TFilePath &fp, TXshSoundLevel *sc, bool overwrite);
bool saveSound(TXshSoundLevel *sc);

/*! \note     Will fallback to loadResourceFolders() in case all
              argument paths are folders.                                 */

enum CacheTlvBehavior {
  ON_DEMAND = 0,  // image data will be loaded when needed
  ALL_ICONS,      // icon data of all frames will be cached at the begininng
  ALL_ICONS_AND_IMAGES  // both icon and image data of all frames will be cached
                        // at the begininng
};

int loadResources(
    LoadResourceArguments &args,  //!< Resources to be loaded.
    bool updateRecentFiles =
        true,  //!< Whether Toonz's <I>Recent Files</I> list must be updated.
    LoadResourceArguments::ScopedBlock *sb =
        0,  //!< Load block. May be nonzero in order to extend block data
            //!  access and finalization.
            //!< Loads a group of resources by path.
            //!  \return  The actually loaded levels count.
    int xFrom = -1,
    int xTo = -1, std::wstring levelName = L"", int step = -1, int inc = -1,
    int frameCount = -1, bool doesFileActuallyExist = true,
    CacheTlvBehavior cachingBehavior = ON_DEMAND);

int loadResourceFolders(
    LoadResourceArguments &args,  //!< Resource folders to be loaded.
    LoadResourceArguments::ScopedBlock *sb =
        0  //!< Load block. May be nonzero in order to extend block data
           //!  access and finalization.
    );     //!< Loads the specified folders in current xsheet.
           //!  \return  The actually loaded levels count.
bool exposeLevel(TXshSimpleLevel *sl, int row, int col, bool insert = false,
                 bool overWrite = false);
bool exposeLevel(TXshSimpleLevel *sl, int row, int col,
                 const std::vector<TFrameId> &fids, bool insert = false,
                 bool overWrite = false);

// se e' necessario salvare la scena chiede il permesso all'utente
// (save,discard,cancel).
// Ritorna false se l'utente ha risposto Cancel o se il salvataggio della scena
// e' fallito per qualche motivo
bool saveSceneIfNeeded(QString msg);

//! Create and expose column with comment in \b commentList.
bool exposeComment(int row, int &col, QList<QString> commentList,
                   QString fileName);

bool importLipSync(TFilePath levelPath, QList<TFrameId> frameList,
                   QList<QString> commentList, QString fileName);

// If the scene will be saved in the different folder, check out the scene
// cast.
// if the cast contains the level specified with $scenefolder alias,
// open a warning popup notifying that such level will lose link.
bool takeCareSceneFolderItemsOnSaveSceneAs(ToonzScene *scene,
                                           const TFilePath &newPath);

}  // namespace IoCmd

#endif  // IOCOMMAND_H
