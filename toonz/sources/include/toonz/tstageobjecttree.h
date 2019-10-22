#pragma once

#ifndef T_STAGE_OBJECT_TREE_INCLUDED
#define T_STAGE_OBJECT_TREE_INCLUDED

#include <memory>

#include "toonz/tstageobject.h"
#include "toonz/txsheet.h"

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
// forward declarations
class TStageObjectSpline;
class TCamera;

class TXsheet;

//=============================================================================
// HandleManager

class HandleManager {
public:
  virtual TPointD getHandlePos(const TStageObjectId &id,
                               const std::string &handle, int row) const = 0;
  virtual ~HandleManager() {}
};

//=============================================================================
//! The class TStageObjectTree permits to collect a set of a stage object in a
//! hierarchical tree structure.
/*!Inherits \b TPersist.
                \n In each tree each index for each type of TStageObjectId is
   unique.
                \n The class provides a collection of functions that returns
   stage object tree element,
                defined in struct \b TStageObjectTreeImp, and enables
   manipulation of these.

            Same class methods allows to menage generic stage objects through
   specialization,
            \b getStageObject(const TStageObjectId &id, bool create= true), or
   through them tree
                index, \b getStageObject(); it's possible to remove or insert
   stage object in the
                tree, removeStageObject() and insertStageObject() respectively.

                Others methods concerns specific stage object; it's possible to
   insert or remove
                directly TStageObjectId::ColumnId using insertColumn() or
   removeColumn() and swap
                columns through swapColumns().
                \n The tree cameras, stage object type:
   TStageObjectId::CameraId, can be manages using
                specialization, getCamera(const TStageObjectId &cameraId), or
   tree index,
                getCamera(int cameraIndex); it's possible to know current
   camera,
                getCurrentCameraId() or getCurrentCamera(), and set the tree
   current camera
                setCurrentCameraId().

                Tree can contains also TStageObjectSpline getSpline(). Spline
   can be added in
                tree by using insertSpline(), insert a given spline, or
   createSpline(),
                insert a new spline,and  remove it  by using removeSpline().
   It's
                possible to know if spline is in tree using containsSpline().

            The class allows to manage tree stage object handle
   setHandleManager(), and to
            know stage object handle position getHandlePos().
        */
//=============================================================================

class DVAPI TStageObjectTree final : public TPersist {
  PERSIST_DECLARATION(TStageObjectTree)
  struct TStageObjectTreeImp;
  std::unique_ptr<TStageObjectTreeImp> m_imp;

  //  usato solo sotto #ifndef NDEBUG
  void checkIntegrity();

public:
  TStageObjectTree();
  ~TStageObjectTree();

  /*!
          The method retrieves a stage object with the passed TStageObjectId \b
     \e id, if the
          second parameter is true it creates the stage object if it doesn't
     exist in
          the TStageObjectTree and returns a pointer to it.
  */
  TStageObject *getStageObject(const TStageObjectId &id, bool create = true);

  /*!
          Inserts in current stage object tree a column object
(TStageObjectId::ColumnId()) with index \b \e index.
\sa removeColumn() and insertStageObject()
*/
  void insertColumn(int index);
  /*!
          Removes from the current stage object tree the stage object column
(TStageObjectId::ColumnId),
          of index \b \e index. Children of the removed column are linked to the
parent of the removed column.
\sa insertColumn() and removeStageObject()
*/
  void removeColumn(int index);

  /*!
          Remove from current stage objetc tree the stage objetc with \b
TStageObjectId \b \e id.
   \n It doesn't delete the stage object.
\sa insertStageObject() and removeColumn()
*/
  void removeStageObject(const TStageObjectId &id);
  /*!
          Inserts in current stage object tree the object \b \e object.
          The \b \e object must exist.
\sa removeStageObject() and insertColumn()
*/
  void insertStageObject(TStageObject *object);
  /*!
          This method swaps colum \b \e i with column \b \e j.
          Columns \b \e i and \b \e j must exist.
  */
  void swapColumns(int i, int j);

  /*!
          Retrieves  object's data from a tagged data stream \b \e is.
  \sa saveData()
  */
  void loadData(TIStream &is, TXsheet *xsh);
  void loadData(TIStream &is) override{};  // not used
                                           /*!
                                                   Saves object's data (name, center coords, etc ) to a tagged data
                                              stream \b \e os saved on a file.
                                                   This method call iteratively all stage objects in the tree and save
                                              their data.
                                           */
  void saveData(TOStream &os, int occupiedColumnCount, TXsheet *xsh);
  void saveData(TOStream &os) override{};  // not used
  /*!
Returns the numbers of the objects in the tree.
*/
  int getStageObjectCount() const;
  /*!
          Returns a pointer to a generic stage object \b TStageObject of index
     \b \e index in the stage object tree.
          Index must be positive and less then the number of the objects.
  */
  TStageObject *getStageObject(int index) const;

  /*!
          Returns \b TStageObjectId of stage object tree current camera.
\sa setCurrentCameraId() and getCurrentCamera()
*/
  TStageObjectId getCurrentCameraId() const;
  TStageObjectId getCurrentPreviewCameraId() const;

  /*!
          Sets stage object tree current camera to \b TStageObjectId \b id.
\sa getCurrentCameraId()
  */
  void setCurrentCameraId(const TStageObjectId &id);
  void setCurrentPreviewCameraId(const TStageObjectId &id);
  /*!
          Calls TStageObject::invalidate() for all objects in the table.
  */

  void invalidateAll();

  /*!
          Sets the handle manager to be \b \e hm.
          An Handle Manager is an object that implements a method to retrieve
     the point position
          of the handle given its id and its name.
          See TStageObject::getHandle() for an explanation of what an hanlde is.
  */
  void setHandleManager(HandleManager *hm);
  /*!
          Returns the point position of the handle \b \e handle.
          If an Handle Manager has not been implemented this method returns a
     point of coordinates (0,0).
          \sa setHandleManager().
  */
  TPointD getHandlePos(const TStageObjectId &id, std::string handle,
                       int row) const;
  /*!
          Returns the number of the spline's objects in the tree.
  */
  int getSplineCount() const;
  /*!
Returns stage object tree spline \b TStageObjectSpline identified by tree index
\b \e index.
\sa insertSpline(), createSpline() and removeSpline()
*/
  TStageObjectSpline *getSpline(int index) const;
  /*!
Returns stage object tree spline \b TStageObjectSpline identified \b \e
splineId.
Returns 0 if StageObjectTree doesn't contains the spline.
\sa insertSpline(), createSpline() and removeSpline()
*/
  TStageObjectSpline *getSplineById(int splineId) const;
  /*!
Create a new spline and add it in stage object tree; returns a pointer to the
new spline.
\sa insertSpline() and removeSpline()
*/
  TStageObjectSpline *createSpline();

  /*! Assign a unique id to the passed \b spline
*/
  void assignUniqueSplineId(TStageObjectSpline *spline);
  /*!
Removes from stage object tree the spline \b spline.
  It deletes the object.
\sa insertSpline() and createSpline()
*/
  void removeSpline(TStageObjectSpline *spline);
  /*!
          Returns	\e true if \b \e spline is present in the tree,
     otherwise
     returns \e false.
  */
  bool containsSpline(TStageObjectSpline *) const;
  /*!
Inserts in the stage object tree the spline \b \e spline.
  If \b \e spline is already present it does nothing,
  otherwise it gets ownership of the spline object.
\sa removeSpline() and createSpline()
*/
  void insertSpline(TStageObjectSpline *);  // get ownership

  // void setToonzBuilder(const TDoubleParamP &param);
  void createGrammar(TXsheet *xsh);
  void setGrammar(const TDoubleParamP &param);
  TSyntax::Grammar *getGrammar() const;

  /*!
          Returns the number of camera objects in the tree.
  */
  int getCameraCount() const;
  int getNewGroupId();

  /*!
          Returns a pointer to stage object tree camera \b TCamera identified by
\b TStageObjectId
\b \e cameraId.
  */
  TCamera *getCamera(const TStageObjectId &cameraId);
  /*!
          Return a pointer to stage object tree camera \b TCamera identified by
index \b \e cameraIndex.
\sa getCurrentCamera()
  */
  inline TCamera *getCamera(int cameraIndex) {
    return getCamera(TStageObjectId::CameraId(cameraIndex));
  }
  /*!
Return a pointer to current stage objetc tree camera \b TCamera.
\sa getCamera() and getCurrentCameraId()
*/
  inline TCamera *getCurrentCamera() { return getCamera(getCurrentCameraId()); }
  inline TCamera *getCurrentPreviewCamera() {
    return getCamera(getCurrentPreviewCameraId());
  }

  void setDagGridDimension(int dim);
  int getDagGridDimension() const;

private:
  // not implemented
  TStageObjectTree(const TStageObjectTree &);
  TStageObjectTree &operator=(const TStageObjectTree &);
};

#endif
